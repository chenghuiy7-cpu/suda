#include "lwe_decrypt.hpp"

enum TokenKind { MASK_WORD = 0, LWE_BODY = 1, STREAM_END = 2 };

struct WordToken {
    ap_uint<64> word;
    ap_uint<8> keep;
    ap_uint<1> end;
};

struct MaskToken {
    ap_uint<64> word;
    ap_uint<2> kind;
    ap_uint<4> error;
};

struct BlockToken {
    ap_uint<64> body;
    ap_uint<64> dot;
    ap_uint<2> kind;
    ap_uint<4> error;
};

struct ClearToken {
    ap_uint<8> value;
    ap_uint<1> end;
    ap_uint<4> error;
};

struct FinishToken {
    ap_uint<512> data;
    ap_uint<64> keep;
    ap_uint<64> strb;
    ap_uint<8> user;
    ap_uint<1> last;
    ap_uint<8> id;
    ap_uint<8> dest;
};

static ap_uint<11> reverse_psi64_mask_index(ap_uint<11> index)
{
#pragma HLS INLINE
    ap_uint<11> reversed = 0;
    for (int bit = 0; bit < 11; bit++) {
#pragma HLS UNROLL
        reversed[10 - bit] = index[bit];
    }
    return reversed;
}

static ap_uint<11> natural_index_from_pc_offset(ap_uint<1> pc, ap_uint<10> offset)
{
#pragma HLS INLINE
    ap_uint<11> hpu_index = (ap_uint<11>(offset >> 4) << 5) |
                            (ap_uint<11>(pc) << 4) | offset.range(3, 0);
    return reverse_psi64_mask_index(hpu_index);
}

static void write_error_packet(Acc_Data &output, ap_uint<32> error)
{
#pragma HLS INLINE
    Acc_Data_Pkt packet;
    packet.data = 0;
    packet.data.range(63, 0) = 0x4c57454445435252ULL;
    packet.data.range(95, 64) = error;
    packet.keep = ~ap_uint<64>(0);
    packet.strb = ~ap_uint<64>(0);
    packet.user = 0xff;
    packet.last = 1;
    packet.id = 0;
    packet.dest = 0;
    output.write(packet);
}

static void flush_clear_packet(Acc_Data &output, ap_uint<512> &data,
                               ap_uint<7> &count)
{
#pragma HLS INLINE
    if (count == 0) {
        return;
    }
    Acc_Data_Pkt packet;
    packet.data = data;
    packet.keep = 0;
    for (int lane = 0; lane < 64; lane++) {
#pragma HLS UNROLL
        packet.keep[lane] = lane < count;
    }
    packet.strb = packet.keep;
    packet.user = 0;
    packet.last = 0;
    packet.id = 0;
    packet.dest = 0;
    output.write(packet);
    data = 0;
    count = 0;
}

static void load_secret_key(ap_uint<512> context[256], ap_uint<1> key[2048])
{
#pragma HLS INLINE off
key_context_loop:
    for (int page = 0; page < 4; page++) {
        ap_uint<512> packed = context[LWE_DECRYPT_KEY_CONTEXT_BASE + page];
    key_bit_loop:
        for (int bit = 0; bit < 512; bit++) {
#pragma HLS PIPELINE II=1
            key[page * 512 + bit] = packed[bit];
        }
    }
}

static void serialize_input_stream(Acc_Data &input, hls::stream<WordToken> &words,
                                   hls::stream<FinishToken> &finish_packets)
{
#pragma HLS INLINE off
    ap_uint<512> data = 0;
    ap_uint<64> keep = 0;
    ap_uint<3> lane = 0;
    bool finished = false;
serialize_word_loop:
    while (!finished) {
#pragma HLS PIPELINE II=1
        if (lane == 0) {
            Acc_Data_Pkt packet = input.read();
            data = packet.data;
            keep = packet.keep;
            finished = packet.user.range(7, 4) != 0;
            if (finished) {
                FinishToken finish;
                finish.data = packet.data;
                finish.keep = packet.keep;
                finish.strb = packet.strb;
                finish.user = packet.user;
                finish.last = packet.last;
                finish.id = packet.id;
                finish.dest = packet.dest;
                finish_packets.write(finish);
            }
        }
        WordToken token;
        token.word = data.range(63, 0);
        token.keep = keep.range(7, 0);
        token.end = finished;
        words.write(token);
        data >>= 64;
        keep >>= 8;
        lane++;
    }
}

// Errors are latched and input is drained to the task marker so DMA can finish.
static void parse_mask_stream(hls::stream<WordToken> &words, ap_uint<1> key[2048],
                              ap_uint<32> input_count, ap_uint<2> layout,
                              hls::stream<MaskToken> &terms)
{
#pragma HLS INLINE off
    ap_uint<12> position = 0;
    ap_uint<2> radix_index = 0;
    ap_uint<32> processed = 0;
    ap_uint<64> native_body = 0;
    ap_uint<4> error = 0;
    bool finished = false;
parse_word_loop:
    while (!finished) {
#pragma HLS PIPELINE II=1
        bool emit = false;
        MaskToken token;
        token.word = 0;
        token.kind = MASK_WORD;
        token.error = 0;
        WordToken incoming = words.read();
        finished = incoming.end;
        if (finished) {
            if (error == 0 && (position != 0 || radix_index != 0 ||
                (input_count != 0 && processed != input_count))) {
                error = 5;
            }
            token.kind = STREAM_END;
            token.error = error;
            emit = true;
        } else {
            ap_uint<8> keep = incoming.keep;
            ap_uint<64> word = incoming.word;
            if (error == 0 && (keep != 0 || layout == LWE_DECRYPT_INPUT_HPU_NATIVE)) {
                if (keep != 0xff) {
                    error = 7;
                } else {
                    bool complete_lwe = false;
                    if (layout == LWE_DECRYPT_INPUT_CPU_PADDED && position > 2048) {
                        if (word != 0) {
                            error = 6;
                        }
                        position++;
                        if (position == LWE_DECRYPT_PADDED_LWE_WORDS) {
                            position = 0;
                            complete_lwe = true;
                        }
                    } else if (input_count != 0 && processed >= input_count) {
                        if (layout == LWE_DECRYPT_INPUT_HPU_NATIVE || word != 0) {
                            error = 6;
                        }
                    } else if (layout != LWE_DECRYPT_INPUT_HPU_NATIVE) {
                        if (position < 2048) {
                            token.word = key[position] ? word : ap_uint<64>(0);
                            emit = true;
                            position++;
                        } else {
                            token.word = word;
                            token.kind = LWE_BODY;
                            emit = true;
                            if (layout == LWE_DECRYPT_INPUT_CPU_PADDED) {
                                position = LWE_DECRYPT_LOGICAL_LWE_WORDS;
                            } else {
                                position = 0;
                                complete_lwe = true;
                            }
                        }
                    } else {
                        ap_uint<1> pc = position >= LWE_DECRYPT_HPU_PC_SLOT_WORDS;
                        ap_uint<12> offset = pc
                            ? ap_uint<12>(position - LWE_DECRYPT_HPU_PC_SLOT_WORDS) : position;
                        if (offset < LWE_DECRYPT_HPU_PC_DATA_WORDS) {
                            ap_uint<11> index = natural_index_from_pc_offset(pc, ap_uint<10>(offset));
                            token.word = key[index] ? word : ap_uint<64>(0);
                            emit = true;
                        } else if (pc == 0 && offset == LWE_DECRYPT_HPU_PC_DATA_WORDS) {
                            native_body = word;
                        }
                        position++;
                        if (position == LWE_DECRYPT_HPU_PC_COUNT * LWE_DECRYPT_HPU_PC_SLOT_WORDS) {
                            token.word = native_body;
                            token.kind = LWE_BODY;
                            emit = true;
                            position = 0;
                            complete_lwe = true;
                        }
                    }
                    if (complete_lwe) {
                        if (radix_index == 3) {
                            processed++;
                        }
                        radix_index++;
                    }
                }
            }
        }
        if (emit) {
            terms.write(token);
        }
    }
}

static void accumulate_mask_stream(hls::stream<MaskToken> &terms,
                                   hls::stream<BlockToken> &blocks)
{
#pragma HLS INLINE off
    ap_uint<64> dot = 0;
    bool finished = false;
accumulate_word_loop:
    while (!finished) {
#pragma HLS PIPELINE II=1
        MaskToken term = terms.read();
        if (term.kind == MASK_WORD) {
            dot += term.word;
        } else {
            BlockToken block;
            block.body = term.word;
            block.dot = dot;
            block.kind = term.kind;
            block.error = term.error;
            blocks.write(block);
            dot = 0;
            finished = term.kind == STREAM_END;
        }
    }
}

static void decode_block_stream(hls::stream<BlockToken> &blocks,
                                hls::stream<ClearToken> &clear)
{
#pragma HLS INLINE off
    ap_uint<2> radix_index = 0;
    ap_uint<8> value = 0;
decode_block_loop:
    while (true) {
        BlockToken block = blocks.read();
        ClearToken token;
        token.value = 0;
        token.end = block.kind == STREAM_END;
        token.error = block.error;
        if (token.end) {
            clear.write(token);
            return;
        }
        ap_uint<64> phase = block.body - block.dot;
        ap_uint<64> rounded = phase + ap_uint<64>(LWE_DECRYPT_HPU_DELTA >> 1);
        ap_uint<2> digit = rounded.range(60, 59);
        value.range(radix_index * 2 + 1, radix_index * 2) = digit;
        if (radix_index == 3) {
            token.value = value;
            clear.write(token);
            value = 0;
        }
        radix_index++;
    }
}

static void pack_clear_stream(hls::stream<ClearToken> &clear,
                              hls::stream<FinishToken> &finish_packets, Acc_Data &output)
{
#pragma HLS INLINE off
    ap_uint<512> data = 0;
    ap_uint<7> count = 0;
pack_clear_loop:
    while (true) {
        ClearToken token = clear.read();
        if (token.end) {
            FinishToken saved = finish_packets.read();
            Acc_Data_Pkt finish;
            finish.data = saved.data;
            finish.keep = saved.keep;
            finish.strb = saved.strb;
            finish.user = saved.user;
            finish.last = saved.last;
            finish.id = saved.id;
            finish.dest = saved.dest;
            if (token.error != 0) {
                write_error_packet(output, token.error);
            } else {
                flush_clear_packet(output, data, count);
                output.write(finish);
            }
            return;
        }
        data.range(count * 8 + 7, count * 8) = token.value;
        count++;
        if (count == 64) {
            flush_clear_packet(output, data, count);
        }
    }
}

static void decrypt_dataflow(Acc_Data &input, Acc_Data &output,
                             ap_uint<1> key[2048], ap_uint<32> count,
                             ap_uint<2> layout)
{
#pragma HLS INLINE off
#pragma HLS DATAFLOW
    hls::stream<WordToken> words;
    hls::stream<MaskToken> terms;
    hls::stream<BlockToken> blocks;
    hls::stream<ClearToken> clear;
    hls::stream<FinishToken> finish_packets;
#pragma HLS STREAM variable=words depth=16
#pragma HLS STREAM variable=terms depth=32
#pragma HLS STREAM variable=blocks depth=4
#pragma HLS STREAM variable=clear depth=4
#pragma HLS STREAM variable=finish_packets depth=4
#pragma HLS BIND_STORAGE variable=finish_packets type=fifo impl=srl
    serialize_input_stream(input, words, finish_packets);
    parse_mask_stream(words, key, count, layout, terms);
    accumulate_mask_stream(terms, blocks);
    decode_block_stream(blocks, clear);
    pack_clear_stream(clear, finish_packets, output);
}

void lwe_decrypt(Acc_Data &data_in, Acc_Data &data_out, ap_uint<512> context[256])
{
#pragma HLS INTERFACE axis register_mode=off port=data_in
#pragma HLS INTERFACE axis register_mode=off port=data_out
#pragma HLS INTERFACE bram port=context
    ap_uint<512> cfg = context[LWE_DECRYPT_STATIC_CONTEXT_BASE];
    ap_uint<32> count = cfg.range(31, 0);
    ap_uint<32> dimension = cfg.range(63, 32);
    ap_uint<64> delta = cfg.range(127, 64);
    ap_uint<32> message_width = cfg.range(159, 128);
    ap_uint<32> radix_blocks = cfg.range(191, 160);
    ap_uint<32> layout = cfg.range(223, 192);
    ap_uint<4> error = 0;
    if (dimension != LWE_DECRYPT_HPU_BIG_LWE_DIMENSION) {
        error = 1;
    } else if (delta != 0 && delta != LWE_DECRYPT_HPU_DELTA) {
        error = 2;
    } else if ((message_width != 0 && message_width != LWE_DECRYPT_HPU_MESSAGE_WIDTH) ||
               (radix_blocks != 0 && radix_blocks != LWE_DECRYPT_U8_RADIX_BLOCK_COUNT)) {
        error = 3;
    } else if (layout > LWE_DECRYPT_INPUT_CPU_PADDED) {
        error = 4;
    }
    if (error != 0) {
        write_error_packet(data_out, error);
        return;
    }
    ap_uint<1> key[2048];
#pragma HLS BIND_STORAGE variable=key type=ram_1p impl=lutram
    load_secret_key(context, key);
    decrypt_dataflow(data_in, data_out, key, count, ap_uint<2>(layout));
}

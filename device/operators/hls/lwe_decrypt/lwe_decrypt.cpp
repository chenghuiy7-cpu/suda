#include "lwe_decrypt.hpp"

enum TokenKind { MASK_WORD = 0, LWE_BODY = 1, STREAM_END = 2 };

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

// Eight banks store consecutive 256-bit key regions. Natural-order
// packing reads one byte; bit-reversed HPU packing reads one bit per bank.
// Loading and permutation take 256 iterations each instead of 2048 each.
static void load_secret_key(ap_uint<512> context[256], ap_uint<8> key[8][32])
{
#pragma HLS INLINE off
#pragma HLS ARRAY_PARTITION variable=key complete dim=1
key_context_loop:
    for (int page = 0; page < 4; ++page) {
        ap_uint<512> packed = context[LWE_DECRYPT_KEY_CONTEXT_BASE + page];
    key_byte_loop:
        for (int byte = 0; byte < 64; ++byte) {
#pragma HLS PIPELINE II=1
            key[page * 2 + (byte >> 5)][byte & 31] = packed.range(7, 0);
            packed >>= 8;
        }
    }
}

// Two byte reads cover every input beat, even across compact LWE bodies.
// Precompute the HPU permutation once, keeping bit reversal out of hot RAM
// access. Eight distinct banks supply the eight bits of each HPU key byte.
static void prepare_key_groups(ap_uint<8> key[8][32], ap_uint<8> groups[256],
                               ap_uint<2> layout)
{
#pragma HLS INLINE off
#pragma HLS ARRAY_PARTITION variable=key complete dim=1
prepare_key_loop:
    for (int group = 0; group < 256; ++group) {
#pragma HLS PIPELINE II=1
        ap_uint<8> packed = 0;
        if (layout != LWE_DECRYPT_INPUT_HPU_NATIVE) {
            packed = key[group >> 5][group & 31];
        } else {
            ap_uint<11> source = natural_index_from_pc_offset(group >= 128,
                ap_uint<10>(group * 8));
            for (int lane = 0; lane < 8; ++lane) {
#pragma HLS UNROLL
                const int bank = ((lane & 1) << 2) | (lane & 2) | ((lane & 4) >> 2);
                ap_uint<8> byte = key[bank][source.range(7, 3)];
                packed[lane] = byte[source.range(2, 0)];
            }
        }
        groups[group] = packed;
    }
}

struct MaskBeatToken {
    ap_uint<64> words[8];
    ap_uint<64> body;
    ap_uint<4> body_lane;
    ap_uint<1> has_body;
    ap_uint<1> end;
    ap_uint<4> error;
};

struct SumBeatToken {
    ap_uint<64> before;
    ap_uint<64> after;
    ap_uint<64> body;
    ap_uint<1> has_body;
    ap_uint<1> end;
    ap_uint<4> error;
};

struct InputBeatToken {
    ap_uint<512> data;
    ap_uint<4> prefix[8];
    ap_uint<8> valid;
    ap_uint<4> count;
    ap_uint<1> bad_keep;
    ap_uint<1> end;
};

struct FrameBeatToken {
    InputBeatToken beat;
    ap_uint<12> position;
    ap_uint<12> span;
    ap_uint<1> complete;
    ap_uint<1> past_count;
    ap_uint<1> last_item;
    ap_uint<1> count_error;
};

struct AddressBeatToken {
    ap_uint<512> data;
    ap_uint<3> key_bits[8];
    ap_uint<8> use_first;
    ap_uint<8> first_group;
    ap_uint<8> last_group;
    ap_uint<8> masks;
    ap_uint<8> body_mask;
    ap_uint<8> zero_mask;
    ap_uint<4> body_lane;
    ap_uint<1> extra_native;
    ap_uint<1> native_complete;
    ap_uint<1> bad_keep;
    ap_uint<1> count_error;
    ap_uint<1> end;
};

struct KeyBeatToken {
    AddressBeatToken beat;
    ap_uint<8> first_bits;
    ap_uint<8> last_bits;
};

// Keep popcounts and TKEEP validation off the position recurrence.
static void read_input_beats(Acc_Data &input, ap_uint<2> layout,
                              hls::stream<InputBeatToken> &beats,
                              hls::stream<FinishToken> &finish_packets)
{
#pragma HLS INLINE off
    bool finished = false;
read_beat_loop:
    while (!finished) {
#pragma HLS PIPELINE II=1
        Acc_Data_Pkt incoming = input.read();
        InputBeatToken token;
        token.data = incoming.data;
        token.valid = 0;
        token.bad_keep = 0;
        token.end = incoming.user.range(7, 4) != 0;
        for (int lane = 0; lane < 8; ++lane) {
#pragma HLS UNROLL
            ap_uint<8> keep = incoming.keep.range(lane * 8 + 7, lane * 8);
            token.valid[lane] = layout == LWE_DECRYPT_INPUT_HPU_NATIVE || keep != 0;
            token.bad_keep |= token.valid[lane] && keep != 0xff;
        }
        token.count = 0;
        for (int lane = 0; lane < 8; ++lane) {
#pragma HLS UNROLL
            ap_uint<4> prefix = 0;
            for (int earlier = 0; earlier < lane; ++earlier) {
#pragma HLS UNROLL
                prefix += token.valid[earlier];
            }
            token.prefix[lane] = prefix;
            token.count += token.valid[lane];
        }
        if (token.end) {
            FinishToken finish;
            finish.data = incoming.data;
            finish.keep = incoming.keep;
            finish.strb = incoming.strb;
            finish.user = incoming.user;
            finish.last = incoming.last;
            finish.id = incoming.id;
            finish.dest = incoming.dest;
            finish_packets.write(finish);
        }
        beats.write(token);
        finished = token.end;
    }
}

// Only a narrow position/count update feeds back here. Key lookup, zero
// checking and the adder tree are downstream and cannot lengthen its II.
static void frame_input_beats(hls::stream<InputBeatToken> &beats,
                               ap_uint<32> input_count, ap_uint<2> layout,
                               hls::stream<FrameBeatToken> &frames)
{
#pragma HLS INLINE off
    ap_uint<12> position = 0;
    ap_uint<2> radix_index = 0;
    ap_uint<32> processed = 0;
    const ap_uint<12> span = layout == LWE_DECRYPT_INPUT_HPU_NATIVE ? 3072
        : layout == LWE_DECRYPT_INPUT_CPU_PADDED ? 2056 : 2049;
    bool finished = false;
frame_beat_loop:
    while (!finished) {
#pragma HLS PIPELINE II=1
        InputBeatToken incoming = beats.read();
        FrameBeatToken token;
        token.beat = incoming;
        token.position = position;
        token.span = span;
        token.past_count = input_count != 0 && processed >= input_count;
        token.last_item = input_count != 0 && processed + 1 == input_count && radix_index == 3;
        ap_uint<13> total = ap_uint<13>(position) + incoming.count;
        token.complete = total >= span;
        // Known-count tail words are checked as zero padding downstream.
        // Count comparison must not feed back into the position recurrence.
        token.count_error = input_count != 0 ? processed != input_count
            : (position != 0 || radix_index != 0);
        if (!incoming.end) {
            position = token.complete ? ap_uint<12>(total - span) : ap_uint<12>(total);
            if (token.complete) {
                if (radix_index == 3 && !token.past_count) processed++;
                radix_index++;
            }
        }
        frames.write(token);
        finished = incoming.end;
    }
}

static void address_mask_beats(hls::stream<FrameBeatToken> &frames,
                                ap_uint<2> layout,
                                hls::stream<AddressBeatToken> &addresses)
{
#pragma HLS INLINE off
    bool finished = false;
address_beat_loop:
    while (!finished) {
#pragma HLS PIPELINE II=1
        FrameBeatToken incoming = frames.read();
        AddressBeatToken token;
        token.data = incoming.beat.data;
        token.first_group = 0;
        token.last_group = 0;
        token.masks = 0;
        token.body_mask = 0;
        token.zero_mask = 0;
        token.body_lane = 8;
        token.extra_native = 0;
        token.native_complete = incoming.complete && !incoming.past_count && layout == LWE_DECRYPT_INPUT_HPU_NATIVE;
        token.bad_keep = incoming.beat.bad_keep;
        token.count_error = incoming.count_error;
        token.end = incoming.beat.end;
        ap_uint<8> groups[8];
#pragma HLS ARRAY_PARTITION variable=groups complete
        bool found_mask = false;
        for (int lane = 0; lane < 8; ++lane) {
#pragma HLS UNROLL
            ap_uint<13> absolute = ap_uint<13>(incoming.position) + incoming.beat.prefix[lane];
            bool wrapped = absolute >= incoming.span;
            ap_uint<12> p = wrapped ? ap_uint<12>(absolute - incoming.span) : ap_uint<12>(absolute);
            bool valid = incoming.beat.valid[lane];
            bool tail = incoming.past_count || (incoming.last_item && wrapped);
            bool mask = layout == LWE_DECRYPT_INPUT_HPU_NATIVE
                ? (p < 1024 || (p >= 1536 && p < 2560)) : p < 2048;
            ap_uint<11> index = layout == LWE_DECRYPT_INPUT_HPU_NATIVE && p >= 1536
                ? ap_uint<11>(p - 512) : ap_uint<11>(p);
            groups[lane] = index >> 3;
            token.key_bits[lane] = index.range(2, 0);
            token.masks[lane] = valid && !tail && mask;
            token.body_mask[lane] = valid && !tail &&
                (layout == LWE_DECRYPT_INPUT_HPU_NATIVE ? p == 1024 : p == 2048);
            token.zero_mask[lane] = valid && (tail ||
                (layout == LWE_DECRYPT_INPUT_CPU_PADDED && p > 2048));
            token.extra_native |= valid && tail && layout == LWE_DECRYPT_INPUT_HPU_NATIVE;
            if (token.masks[lane]) {
                if (!found_mask) {
                    token.first_group = groups[lane];
                    found_mask = true;
                }
                token.last_group = groups[lane];
            }
            if (token.body_mask[lane]) token.body_lane = lane;
        }
        token.use_first = 0;
        for (int lane = 0; lane < 8; ++lane) {
#pragma HLS UNROLL
            token.use_first[lane] = groups[lane] == token.first_group;
        }
        addresses.write(token);
        finished = token.end;
    }
}

// Separate stage enforces exactly TWO RAM reads per beat. If the RAM loads
// are fused into lane selection HLS may speculate eight reads and increase II.
static void fetch_key_beats(hls::stream<AddressBeatToken> &addresses,
                             ap_uint<8> groups[256],
                             hls::stream<KeyBeatToken> &keyed)
{
#pragma HLS INLINE off
    bool finished = false;
fetch_key_beat_loop:
    while (!finished) {
#pragma HLS PIPELINE II=1
        AddressBeatToken incoming = addresses.read();
        KeyBeatToken token;
        token.beat = incoming;
        token.first_bits = groups[incoming.first_group];
        token.last_bits = groups[incoming.last_group];
        keyed.write(token);
        finished = incoming.end;
    }
}

static void select_mask_beats(hls::stream<KeyBeatToken> &keyed, ap_uint<2> layout,
                               hls::stream<MaskBeatToken> &terms)
{
#pragma HLS INLINE off
    ap_uint<64> native_body = 0;
    ap_uint<4> error = 0;
    bool finished = false;
select_mask_beat_loop:
    while (!finished) {
#pragma HLS PIPELINE II=1
        KeyBeatToken incoming = keyed.read();
        const AddressBeatToken &beat = incoming.beat;
        MaskBeatToken token;
        token.body = 0;
        token.body_lane = layout == LWE_DECRYPT_INPUT_HPU_NATIVE ? ap_uint<4>(8) : beat.body_lane;
        token.has_body = beat.native_complete || (layout != LWE_DECRYPT_INPUT_HPU_NATIVE && beat.body_mask != 0);
        token.end = beat.end;
        bool bad_padding = beat.extra_native;
        ap_uint<64> body = 0;
        for (int lane = 0; lane < 8; ++lane) {
#pragma HLS UNROLL
            ap_uint<64> word = beat.data.range(lane * 64 + 63, lane * 64);
            ap_uint<8> bits = beat.use_first[lane] ? incoming.first_bits : incoming.last_bits;
            token.words[lane] = beat.masks[lane] && bits[beat.key_bits[lane]] ? word : ap_uint<64>(0);
            if (beat.body_mask[lane]) body |= word;
            bad_padding |= beat.zero_mask[lane] && word != 0;
        }
        if (layout == LWE_DECRYPT_INPUT_HPU_NATIVE) {
            if (beat.body_mask != 0 && !beat.end) native_body = body;
            token.body = native_body;
        } else {
            token.body = body;
        }
        if (error == 0) {
            if (beat.end) {
                if (beat.count_error) error = 5;
            } else if (beat.bad_keep) {
                error = 7;
            } else if (bad_padding) {
                error = 6;
            }
        }
        if (error != 0 || beat.end) {
            for (int lane = 0; lane < 8; ++lane) {
#pragma HLS UNROLL
                token.words[lane] = 0;
            }
            token.has_body = 0;
        }
        token.error = error;
        terms.write(token);
        finished = beat.end;
    }
}

static ap_uint<64> sum_eight(ap_uint<64> values[8])
{
#pragma HLS INLINE
#pragma HLS ARRAY_PARTITION variable=values complete
    ap_uint<64> a = values[0] + values[1];
    ap_uint<64> b = values[2] + values[3];
    ap_uint<64> c = values[4] + values[5];
    ap_uint<64> d = values[6] + values[7];
    return (a + b) + (c + d);
}

// The adder tree is a separate pipeline, keeping key lookup and state update
// off the single-cycle dot accumulator's recurrence path.
static void reduce_mask_beats(hls::stream<MaskBeatToken> &terms,
                              hls::stream<SumBeatToken> &sums)
{
#pragma HLS INLINE off
    bool finished = false;
reduce_beat_loop:
    while (!finished) {
#pragma HLS PIPELINE II=1
        MaskBeatToken term = terms.read();
        ap_uint<64> before[8];
        ap_uint<64> after[8];
#pragma HLS ARRAY_PARTITION variable=before complete
#pragma HLS ARRAY_PARTITION variable=after complete
        for (int lane = 0; lane < 8; ++lane) {
#pragma HLS UNROLL
            before[lane] = !term.has_body || lane < term.body_lane
                ? term.words[lane] : ap_uint<64>(0);
            after[lane] = term.has_body && lane > term.body_lane
                ? term.words[lane] : ap_uint<64>(0);
        }
        SumBeatToken token;
        token.before = sum_eight(before);
        token.after = sum_eight(after);
        token.body = term.body;
        token.has_body = term.has_body;
        token.end = term.end;
        token.error = term.error;
        sums.write(token);
        finished = term.end;
    }
}

static void accumulate_mask_beats(hls::stream<SumBeatToken> &sums,
                                  hls::stream<BlockToken> &blocks)
{
#pragma HLS INLINE off
    ap_uint<64> dot = 0;
    bool finished = false;
accumulate_beat_loop:
    while (!finished) {
#pragma HLS PIPELINE II=1
        SumBeatToken term = sums.read();
        if (term.has_body || term.end) {
            BlockToken block;
            block.body = term.body;
            block.dot = dot + term.before;
            block.kind = term.end ? STREAM_END : LWE_BODY;
            block.error = term.error;
            blocks.write(block);
            dot = term.after;
        } else {
            dot += term.before;
        }
        finished = term.end;
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
                             ap_uint<8> groups[256], ap_uint<32> count,
                             ap_uint<2> layout)
{
#pragma HLS INLINE off
#pragma HLS DATAFLOW
    hls::stream<InputBeatToken> beats;
    hls::stream<FrameBeatToken> frames;
    hls::stream<AddressBeatToken> addresses;
    hls::stream<KeyBeatToken> keyed;
    hls::stream<MaskBeatToken> terms;
    hls::stream<SumBeatToken> sums;
    hls::stream<BlockToken> blocks;
    hls::stream<ClearToken> clear;
    hls::stream<FinishToken> finish_packets;
#pragma HLS STREAM variable=beats depth=4
#pragma HLS AGGREGATE variable=beats compact=bit
#pragma HLS BIND_STORAGE variable=beats type=fifo impl=srl
#pragma HLS STREAM variable=frames depth=4
#pragma HLS AGGREGATE variable=frames compact=bit
#pragma HLS BIND_STORAGE variable=frames type=fifo impl=srl
#pragma HLS STREAM variable=addresses depth=4
#pragma HLS AGGREGATE variable=addresses compact=bit
#pragma HLS BIND_STORAGE variable=addresses type=fifo impl=srl
#pragma HLS STREAM variable=keyed depth=4
#pragma HLS AGGREGATE variable=keyed compact=bit
#pragma HLS BIND_STORAGE variable=keyed type=fifo impl=srl
#pragma HLS STREAM variable=terms depth=4
#pragma HLS AGGREGATE variable=terms compact=bit
#pragma HLS BIND_STORAGE variable=terms type=fifo impl=srl
#pragma HLS STREAM variable=sums depth=16
#pragma HLS AGGREGATE variable=sums compact=bit
#pragma HLS BIND_STORAGE variable=sums type=fifo impl=srl
#pragma HLS STREAM variable=blocks depth=4
#pragma HLS AGGREGATE variable=blocks compact=bit
#pragma HLS BIND_STORAGE variable=blocks type=fifo impl=srl
#pragma HLS STREAM variable=clear depth=4
#pragma HLS AGGREGATE variable=clear compact=bit
#pragma HLS BIND_STORAGE variable=clear type=fifo impl=srl
#pragma HLS STREAM variable=finish_packets depth=9
#pragma HLS AGGREGATE variable=finish_packets compact=bit
#pragma HLS BIND_STORAGE variable=finish_packets type=fifo impl=srl
    read_input_beats(input, layout, beats, finish_packets);
    frame_input_beats(beats, count, layout, frames);
    address_mask_beats(frames, layout, addresses);
    fetch_key_beats(addresses, groups, keyed);
    select_mask_beats(keyed, layout, terms);
    reduce_mask_beats(terms, sums);
    accumulate_mask_beats(sums, blocks);
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
    ap_uint<8> key[8][32];
#pragma HLS ARRAY_PARTITION variable=key complete dim=1
#pragma HLS BIND_STORAGE variable=key type=ram_2p impl=lutram
    ap_uint<8> groups[256];
#pragma HLS BIND_STORAGE variable=groups type=ram_2p impl=lutram
    load_secret_key(context, key);
    prepare_key_groups(key, groups, ap_uint<2>(layout));
    decrypt_dataflow(data_in, data_out, groups, count, ap_uint<2>(layout));
}

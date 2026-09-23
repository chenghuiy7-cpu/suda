#include "lwe_decrypt.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#ifndef LWE_DECRYPT_TEST_U8_COUNT
#define LWE_DECRYPT_TEST_U8_COUNT 3
#endif

static const uint32_t TEST_U8_COUNT = LWE_DECRYPT_TEST_U8_COUNT;
static uint32_t key_pattern = 0;
static bool boundary_plaintexts = false;

static bool test_key_bit(uint32_t index)
{
    if (key_pattern != 0) {
        return key_pattern == 2;
    }
    uint32_t mixed = index * 0x9e3779b9U + 0x7f4a7c15U;
    return ((mixed ^ (mixed >> 11) ^ (index >> 3)) & 1U) != 0;
}

static uint64_t next_random(uint64_t &state)
{
    if (state == 0) {
        state = 0x9e3779b97f4a7c15ULL;
    }
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
}

static uint32_t reverse_11(uint32_t index)
{
    uint32_t reversed = 0;
    for (uint32_t bit = 0; bit < 11; bit++) {
        reversed |= ((index >> bit) & 1U) << (10 - bit);
    }
    return reversed;
}

static size_t native_word_index(uint32_t natural_index)
{
    uint32_t hpu_index = reverse_11(natural_index);
    uint32_t group = hpu_index >> 4;
    uint32_t lane = hpu_index & 15U;
    uint32_t pc = group & 1U;
    uint32_t pc_offset = ((group >> 1) << 4) | lane;
    return size_t(pc) * LWE_DECRYPT_HPU_PC_SLOT_WORDS + pc_offset;
}

static uint8_t expected_clear(uint32_t index)
{
    if (boundary_plaintexts) {
        const uint8_t values[] = {0, 255, 1, 127, 128, 254};
        return values[index % 6];
    }
    return uint8_t(index * 37U + 11U);
}

static void append_native_lwe(
    Acc_Data &input,
    uint8_t clear_block,
    uint32_t ciphertext_index,
    uint32_t layout,
    std::vector<uint64_t> &logical_words)
{
    const size_t words_per_lwe =
        LWE_DECRYPT_HPU_PC_COUNT * LWE_DECRYPT_HPU_PC_SLOT_WORDS;
    std::vector<uint64_t> words(words_per_lwe, 0);
    uint64_t state = 0xd1b54a32d192ed03ULL ^
                     (uint64_t(ciphertext_index + 1) * 0x94d049bb133111ebULL);
    uint64_t dot = 0;

    for (uint32_t natural_index = 0;
         natural_index < LWE_DECRYPT_HPU_BIG_LWE_DIMENSION;
         natural_index++) {
        uint64_t mask = next_random(state);
        words[native_word_index(natural_index)] = mask;
        if (layout != LWE_DECRYPT_INPUT_HPU_NATIVE) {
            logical_words.push_back(mask);
        }
        if (test_key_bit(natural_index)) {
            dot += mask;
        }
    }

    int64_t signed_noise = int64_t(ciphertext_index % 17U) - 8;
    uint64_t body = dot +
                    uint64_t(clear_block) * LWE_DECRYPT_HPU_DELTA +
                    uint64_t(signed_noise);
    words[LWE_DECRYPT_HPU_PC_DATA_WORDS] = body;
    if (layout != LWE_DECRYPT_INPUT_HPU_NATIVE) {
        logical_words.push_back(body);
        if (layout == LWE_DECRYPT_INPUT_CPU_PADDED) {
            logical_words.resize(logical_words.size() + 7, 0);
        }
        return;
    }

    for (size_t packet_index = 0;
         packet_index < words_per_lwe / (TDATA_WIDTH / 64);
         packet_index++) {
        Acc_Data_Pkt pkt;
        pkt.data = 0;
        for (int lane = 0; lane < TDATA_WIDTH / 64; lane++) {
            pkt.data.range(lane * 64 + 63, lane * 64) =
                words[packet_index * (TDATA_WIDTH / 64) + lane];
        }
        pkt.keep = ~ap_uint<64>(0);
        pkt.strb = ~ap_uint<64>(0);
        pkt.user = 0;
        pkt.last = 0;
        pkt.id = 0;
        pkt.dest = 0;
        input.write(pkt);
    }
}

static void run_test(uint32_t layout, uint32_t count, bool until_finish,
                     bool lba_padding, bool truncate = false,
                     bool corrupt_padding = false, bool partial_word = false,
                     bool corrupt_internal_padding = false)
{
    Acc_Data input;
    Acc_Data output;
    ap_uint<512> context[256];

    for (int i = 0; i < 256; i++) {
        context[i] = 0;
    }

    ap_uint<512> cfg = 0;
    cfg.range(31, 0) = until_finish ? 0U : count;
    cfg.range(63, 32) = LWE_DECRYPT_HPU_BIG_LWE_DIMENSION;
    cfg.range(127, 64) = LWE_DECRYPT_HPU_DELTA;
    cfg.range(159, 128) = LWE_DECRYPT_HPU_MESSAGE_WIDTH;
    cfg.range(191, 160) = LWE_DECRYPT_U8_RADIX_BLOCK_COUNT;
    cfg.range(223, 192) = layout;
    context[LWE_DECRYPT_STATIC_CONTEXT_BASE] = cfg;

    for (uint32_t index = 0;
         index < LWE_DECRYPT_HPU_BIG_LWE_DIMENSION;
         index++) {
        if (test_key_bit(index)) {
            uint32_t word = LWE_DECRYPT_KEY_CONTEXT_BASE + (index >> 9);
            context[word][index & 511U] = 1;
        }
    }

    uint32_t ciphertext_index = 0;
    std::vector<uint64_t> logical_words;
    for (uint32_t clear_index = 0; clear_index < count; clear_index++) {
        uint8_t clear_u8 = expected_clear(clear_index);
        for (uint32_t block_index = 0;
             block_index < LWE_DECRYPT_U8_RADIX_BLOCK_COUNT;
             block_index++) {
            uint8_t clear_block =
                (clear_u8 >> (block_index * LWE_DECRYPT_HPU_MESSAGE_WIDTH)) & 3U;
            append_native_lwe(input, clear_block, ciphertext_index++, layout,
                              logical_words);
        }
    }

    if (layout != LWE_DECRYPT_INPUT_HPU_NATIVE) {
        if (corrupt_internal_padding) {
            logical_words[LWE_DECRYPT_LOGICAL_LWE_WORDS] = 1;
        }
        if (truncate) {
            logical_words.pop_back();
        }
        size_t payload_words = logical_words.size();
        if (lba_padding) {
            logical_words.resize(((payload_words + 511) / 512) * 512, 0);
        }
        if (corrupt_padding) {
            assert(logical_words.size() > payload_words);
            logical_words[payload_words] = 1;
        }
        for (size_t base = 0; base < logical_words.size(); base += 8) {
            Acc_Data_Pkt pkt;
            pkt.data = 0;
            pkt.keep = 0;
            pkt.strb = 0;
            for (int lane = 0; lane < 8; ++lane) {
                if (base + lane < logical_words.size()) {
                    pkt.data.range(lane * 64 + 63, lane * 64) = logical_words[base + lane];
                    pkt.keep.range(lane * 8 + 7, lane * 8) = 0xff;
                    pkt.strb.range(lane * 8 + 7, lane * 8) = 0xff;
                }
            }
            pkt.user = 0;
            if (partial_word && base == 0) {
                pkt.keep.range(7, 0) = 0x0f;
                pkt.strb.range(7, 0) = 0x0f;
            }
            pkt.last = (base % 512 == 0); // TLAST is not the SUDA task delimiter.
            pkt.id = 0;
            pkt.dest = 0;
            input.write(pkt);
        }
    }

    Acc_Data_Pkt done;
    done.data = 0xfeedbeef01234567ULL;
    done.keep = ~ap_uint<64>(0);
    done.strb = ~ap_uint<64>(0);
    done.user = 0xff;
    done.last = 1;
    done.id = 7;
    done.dest = 0x35;
    input.write(done);

    lwe_decrypt(input, output, context);

    uint32_t clear_index = 0;
    bool saw_done = false;
    while (!output.empty()) {
        Acc_Data_Pkt pkt = output.read();
        if (pkt.user.range(7, 4) != 0) {
            if (truncate || corrupt_padding || partial_word || corrupt_internal_padding) {
                assert(pkt.data.range(63, 0) == 0x4c57454445435252ULL);
                assert(pkt.data.range(95, 64) == (truncate ? 5 : partial_word ? 7 : 6));
                saw_done = true;
                continue;
            }
            assert(clear_index == count);
            assert(pkt.data == done.data);
            assert(pkt.keep == done.keep && pkt.strb == done.strb);
            assert(pkt.user == done.user && pkt.last == done.last);
            assert(pkt.id == done.id && pkt.dest == done.dest);
            saw_done = true;
            continue;
        }

        for (int lane = 0; lane < TDATA_WIDTH / 8; lane++) {
            bool valid = pkt.keep[lane] != 0;
            if (!valid) {
                continue;
            }
            assert(clear_index < count);
            uint8_t actual = uint8_t(pkt.data.range(lane * 8 + 7, lane * 8));
            assert(actual == expected_clear(clear_index));
            clear_index++;
        }
    }

    assert(saw_done);
    assert(truncate || corrupt_padding || partial_word || corrupt_internal_padding || clear_index == count);
    assert(input.empty());
    std::cout << "lwe_decrypt HLS test passed. input_layout=" << layout
              << " mask_dimension=" << LWE_DECRYPT_HPU_BIG_LWE_DIMENSION
              << " decrypted_u8=" << clear_index
              << " output_packing=64-u8-per-beat"
              << std::endl;
}

int main()
{
#ifdef LWE_DECRYPT_COSIM_SMOKE
    run_test(LWE_DECRYPT_INPUT_HPU_NATIVE, 1, false, false);
    run_test(LWE_DECRYPT_INPUT_CPU_LWE, 65, false, true);
    run_test(LWE_DECRYPT_INPUT_CPU_PADDED, 3, true, false);
    run_test(LWE_DECRYPT_INPUT_CPU_PADDED, 1, false, false, true);
    run_test(LWE_DECRYPT_INPUT_CPU_PADDED, 1, false, false, false, false, false, true);
    run_test(LWE_DECRYPT_INPUT_CPU_LWE, 1, false, false, false, false, true);
#else
    run_test(LWE_DECRYPT_INPUT_HPU_NATIVE, TEST_U8_COUNT, false, false);
    run_test(LWE_DECRYPT_INPUT_HPU_NATIVE, 65, false, false);
    run_test(LWE_DECRYPT_INPUT_CPU_LWE, 1, false, true);
    run_test(LWE_DECRYPT_INPUT_CPU_LWE, TEST_U8_COUNT, false, true);
    run_test(LWE_DECRYPT_INPUT_CPU_LWE, 65, false, true);
    run_test(LWE_DECRYPT_INPUT_CPU_LWE, 3, true, false);
    run_test(LWE_DECRYPT_INPUT_CPU_LWE, 128, false, true);
    run_test(LWE_DECRYPT_INPUT_CPU_PADDED, 1, false, true);
    run_test(LWE_DECRYPT_INPUT_CPU_PADDED, 3, true, false);
    run_test(LWE_DECRYPT_INPUT_CPU_PADDED, 65, false, true);
    run_test(LWE_DECRYPT_INPUT_CPU_PADDED, 128, false, true);
    run_test(LWE_DECRYPT_INPUT_CPU_PADDED, 1, false, false, true);
    run_test(LWE_DECRYPT_INPUT_CPU_PADDED, 1, false, true, false, true);
    run_test(LWE_DECRYPT_INPUT_CPU_PADDED, 1, false, false, false, false, false, true);
    run_test(LWE_DECRYPT_INPUT_CPU_LWE, 1, false, false, true);
    run_test(LWE_DECRYPT_INPUT_CPU_LWE, 1, false, true, false, true);
    run_test(LWE_DECRYPT_INPUT_CPU_LWE, 1, false, false, false, false, true);
    run_test(LWE_DECRYPT_INPUT_HPU_NATIVE, 128, false, false);
    run_test(LWE_DECRYPT_INPUT_CPU_LWE, 63, false, true);
    run_test(LWE_DECRYPT_INPUT_CPU_PADDED, 64, false, true);
    boundary_plaintexts = true;
    for (key_pattern = 1; key_pattern <= 2; key_pattern++) {
        run_test(LWE_DECRYPT_INPUT_CPU_LWE, 6, false, true);
        run_test(LWE_DECRYPT_INPUT_CPU_PADDED, 6, false, true);
        run_test(LWE_DECRYPT_INPUT_HPU_NATIVE, 6, false, false);
    }
    key_pattern = 0;
    boundary_plaintexts = false;
#endif
    return 0;
}

#include "selective_filter.hpp"
#include "../lwe_encrypt/lwe_encrypt.hpp"

#include <stdint.h>
#include <stdio.h>
#include <vector>

namespace {

struct RecordSpec {
    uint32_t id;
    uint8_t quantity;
    uint64_t price;
    int32_t delta;
};

bool verify_status(
    const Acc_Data_Pkt &pkt,
    uint32_t total_count,
    uint32_t selected_count);

void configure_filter(
    ap_uint<512> context[256],
    uint32_t record_count,
    uint32_t predicate,
    uint8_t threshold,
    uint32_t output_mode)
{
    for (int i = 0; i < 256; ++i) {
        context[i] = 0;
    }
    ap_uint<512> &cfg = context[SELECTIVE_FILTER_STATIC_CONTEXT_BASE];
    cfg.range(31, 0) = record_count;
    cfg.range(63, 32) = SELECTIVE_FILTER_RECORD_BYTES;
    cfg.range(95, 64) = SELECTIVE_FILTER_QUANTITY_OFFSET;
    cfg.range(127, 96) = predicate;
    cfg.range(159, 128) = threshold;
    cfg.range(191, 160) = output_mode;
}

void set_v2_predicate(
    ap_uint<512> context[256], int index, uint32_t offset,
    uint8_t type, uint8_t comparison, uint64_t literal)
{
    ap_uint<512> &word = context[
        SELECTIVE_FILTER_STATIC_CONTEXT_BASE + 1 + index / 4];
    int base = (index % 4) * 128;
    word.range(base + 31, base) = offset;
    word.range(base + 39, base + 32) = type;
    word.range(base + 47, base + 40) = comparison;
    word.range(base + 127, base + 64) = literal;
}

void configure_filter_v2(ap_uint<512> context[256], uint32_t record_count)
{
    for (int i = 0; i < 256; ++i) context[i] = 0;
    ap_uint<512> &cfg = context[SELECTIVE_FILTER_STATIC_CONTEXT_BASE];
    cfg.range(31, 0) = record_count;
    cfg.range(63, 32) = SELECTIVE_FILTER_RECORD_BYTES;
    cfg.range(191, 160) = SELECTIVE_FILTER_OUTPUT_QUANTITY;
    cfg.range(223, 192) = SELECTIVE_FILTER_V2_MAGIC;
    cfg.range(255, 224) = SELECTIVE_FILTER_V2_VERSION;
    cfg.range(287, 256) = 4;
    cfg.range(319, 288) = 7;
    cfg.range(351, 320) = 4;
    cfg.range(359, 352) = SELECTIVE_FILTER_TYPE_U8;
    const uint8_t tokens[7] = {
        0, 1, SELECTIVE_FILTER_TOKEN_AND,
        2, 3, SELECTIVE_FILTER_TOKEN_AND,
        SELECTIVE_FILTER_TOKEN_OR};
    for (int i = 0; i < 7; ++i)
        cfg.range(391 + i * 8, 384 + i * 8) = tokens[i];
    set_v2_predicate(context, 0, 4, SELECTIVE_FILTER_TYPE_U8,
        SELECTIVE_FILTER_CMP_GT, 10);
    set_v2_predicate(context, 1, 8, SELECTIVE_FILTER_TYPE_U64,
        SELECTIVE_FILTER_CMP_LE, 500);
    set_v2_predicate(context, 2, 0, SELECTIVE_FILTER_TYPE_U32,
        SELECTIVE_FILTER_CMP_EQ, 103);
    set_v2_predicate(context, 3, 24, SELECTIVE_FILTER_TYPE_I32,
        SELECTIVE_FILTER_CMP_LT, uint64_t(int64_t(-1)));
}

void configure_lwe(ap_uint<512> context[256])
{
    for (int i = 0; i < 256; ++i) {
        context[i] = 0;
    }
    ap_uint<512> &cfg = context[LWE_ENCRYPT_STATIC_CONTEXT_BASE];
    cfg.range(31, 0) = LWE_ENCRYPT_HPU_BIG_LWE_DIMENSION;
    cfg.range(63, 32) = 0;
    cfg.range(95, 64) = LWE_ENCRYPT_INPUT_U8_RADIX_SCALAR_STREAM;
    cfg.range(127, 96) = LWE_ENCRYPT_NOISE_ZERO;
    cfg.range(159, 128) = LWE_ENCRYPT_HPU_GLWE_NOISE_BOUND_LOG2;
    cfg.range(191, 160) = LWE_ENCRYPT_OUTPUT_CPU_LWE;
    cfg.range(319, 256) = LWE_ENCRYPT_HPU_DELTA;
    cfg.range(383, 320) = 0x123456789abcdef0ULL;
    cfg.range(447, 384) = 0x0fedcba987654321ULL;
}

void append_records(Acc_Data &input, const std::vector<RecordSpec> &records)
{
    for (size_t record_index = 0; record_index < records.size(); ++record_index) {
        for (int beat = 0; beat < SELECTIVE_FILTER_RECORD_BEATS; ++beat) {
            Acc_Data_Pkt pkt;
            pkt.data = 0;
            pkt.keep = -1;
            pkt.strb = -1;
            pkt.user = 0;
            pkt.id = 0;
            pkt.dest = 0;
            pkt.last = 0;
            for (int byte_lane = 0; byte_lane < SELECTIVE_FILTER_AXIS_BYTES; ++byte_lane) {
                uint8_t value = static_cast<uint8_t>(
                    (record_index * SELECTIVE_FILTER_RECORD_BYTES +
                     beat * SELECTIVE_FILTER_AXIS_BYTES + byte_lane) & 0xff);
                pkt.data.range(byte_lane * 8 + 7, byte_lane * 8) = value;
            }
            if (beat == 0) {
                pkt.data.range(31, 0) = records[record_index].id;
                pkt.data.range(39, 32) = records[record_index].quantity;
                pkt.data.range(127, 64) = records[record_index].price;
                pkt.data.range(223, 192) =
                    static_cast<uint32_t>(records[record_index].delta);
            }
            input.write(pkt);
        }
    }

    Acc_Data_Pkt finish;
    finish.data = 0;
    finish.keep = -1;
    finish.strb = -1;
    finish.user = 0xff;
    finish.id = 0;
    finish.dest = 0;
    finish.last = 1;
    input.write(finish);
}

bool test_runtime_sql_metadata()
{
    // (quantity > 10 AND price <= 500) OR (id = 103 AND delta < -1)
    std::vector<RecordSpec> records = {
        {100, 3, 100, -9}, {101, 17, 400, 1},
        {102, 9, 200, -4}, {103, 24, 900, -2}
    };
    Acc_Data input, output;
    ap_uint<512> context[256];
    configure_filter_v2(context, records.size());
    append_records(input, records);
    selective_filter(input, output, context);
    const uint8_t expected[2] = {17, 24};
    for (int i = 0; i < 2; ++i) {
        if (output.empty()) return false;
        Acc_Data_Pkt pkt = output.read();
        if (pkt.user != 0 || pkt.data.range(7, 0) != expected[i] ||
            pkt.keep != 1 || pkt.last) return false;
    }
    if (output.empty()) return false;
    Acc_Data_Pkt status = output.read();
    return verify_status(status, records.size(), 2) && output.empty();
}

bool verify_status(
    const Acc_Data_Pkt &pkt,
    uint32_t total_count,
    uint32_t selected_count)
{
    return pkt.user == 0xff &&
        pkt.data.range(63, 0) == SELECTIVE_FILTER_STATUS_MAGIC &&
        pkt.data.range(95, 64) == total_count &&
        pkt.data.range(127, 96) == selected_count &&
        pkt.data.range(159, 128) == 0;
}

bool test_full_record_output()
{
    std::vector<RecordSpec> records = {
        {100, 3}, {101, 17}, {102, 9}, {103, 24}
    };
    Acc_Data input;
    Acc_Data output;
    ap_uint<512> context[256];
    configure_filter(
        context,
        records.size(),
        SELECTIVE_FILTER_PREDICATE_GT,
        10,
        SELECTIVE_FILTER_OUTPUT_FULL_RECORD);
    append_records(input, records);
    selective_filter(input, output, context);

    const uint32_t expected_ids[2] = {101, 103};
    for (int selected_index = 0; selected_index < 2; ++selected_index) {
        for (int beat = 0; beat < SELECTIVE_FILTER_RECORD_BEATS; ++beat) {
            Acc_Data_Pkt pkt = output.read();
            if (beat == 0 && pkt.data.range(31, 0) != expected_ids[selected_index]) {
                fprintf(
                    stderr,
                    "full-record id mismatch selected=%d expected=%u actual=%u\n",
                    selected_index,
                    expected_ids[selected_index],
                    static_cast<unsigned>(pkt.data.range(31, 0)));
                return false;
            }
            if (pkt.user != 0) {
                fprintf(
                    stderr,
                    "unexpected TUSER in full-record payload selected=%d beat=%d user=%u data0=%llx\n",
                    selected_index,
                    beat,
                    static_cast<unsigned>(pkt.user),
                    static_cast<unsigned long long>(pkt.data.range(63, 0)));
                return false;
            }
        }
    }
    Acc_Data_Pkt status = output.read();
    if (!verify_status(status, records.size(), 2)) {
        fprintf(
            stderr,
            "status mismatch magic=%llx total=%u selected=%u error=%u user=%u\n",
            static_cast<unsigned long long>(status.data.range(63, 0)),
            static_cast<unsigned>(status.data.range(95, 64)),
            static_cast<unsigned>(status.data.range(127, 96)),
            static_cast<unsigned>(status.data.range(159, 128)),
            static_cast<unsigned>(status.user));
        return false;
    }
    if (!output.empty()) {
        fprintf(stderr, "unexpected trailing full-record output\n");
        return false;
    }
    return true;
}

bool test_filter_lwe_pipeline()
{
    std::vector<RecordSpec> records = {
        {200, 7}, {201, 12}, {202, 7}, {203, 99}
    };
    Acc_Data source;
    Acc_Data projected;
    Acc_Data graph_link;
    Acc_Data encrypted;
    ap_uint<512> filter_context[256];
    ap_uint<512> lwe_context[256];

    configure_filter(
        filter_context,
        records.size(),
        SELECTIVE_FILTER_PREDICATE_EQ,
        7,
        SELECTIVE_FILTER_OUTPUT_QUANTITY);
    configure_lwe(lwe_context);
    append_records(source, records);

    selective_filter(source, projected, filter_context);

    // 模拟板上互联未可靠保留稀疏 TKEEP 的情况。标量流模式必须把每个
    // 非结束 beat 解释为一个 quantity，而不能把全 1 TKEEP 展开成 64B。
    while (!projected.empty()) {
        Acc_Data_Pkt pkt = projected.read();
        if (pkt.user.range(7, 4) == 0) {
            pkt.keep = -1;
            pkt.strb = -1;
        }
        graph_link.write(pkt);
    }
    lwe_encrypt(graph_link, encrypted, lwe_context);

    const int selected_count = 2;
    const int words_per_lwe = LWE_ENCRYPT_HPU_BIG_LWE_DIMENSION + 1;
    const int packets_per_lwe = (words_per_lwe + 7) / 8;
    const int payload_packets =
        selected_count * LWE_ENCRYPT_U8_RADIX_BLOCK_COUNT * packets_per_lwe;
    uint8_t decrypted[selected_count] = {0};

    for (int packet = 0; packet < payload_packets; ++packet) {
        Acc_Data_Pkt pkt = encrypted.read();
        if (pkt.user != 0) {
            return false;
        }
        int packet_in_lwe = packet % packets_per_lwe;
        if (packet_in_lwe == packets_per_lwe - 1) {
            int lwe_index = packet / packets_per_lwe;
            int selected_index = lwe_index / LWE_ENCRYPT_U8_RADIX_BLOCK_COUNT;
            int radix_block = lwe_index % LWE_ENCRYPT_U8_RADIX_BLOCK_COUNT;
            uint64_t body = static_cast<uint64_t>(pkt.data.range(63, 0));
            uint8_t clear_block = static_cast<uint8_t>(
                (body / LWE_ENCRYPT_HPU_DELTA) &
                ((1U << LWE_ENCRYPT_HPU_MESSAGE_WIDTH) - 1));
            decrypted[selected_index] |=
                static_cast<uint8_t>(clear_block <<
                                     (radix_block * LWE_ENCRYPT_HPU_MESSAGE_WIDTH));
        }
    }
    if (decrypted[0] != 7 || decrypted[1] != 7) {
        fprintf(
            stderr,
            "filter->LWE plaintext mismatch actual=[%u,%u] expected=[7,7]\n",
            decrypted[0],
            decrypted[1]);
        return false;
    }
    Acc_Data_Pkt status = encrypted.read();
    return verify_status(status, records.size(), selected_count) && encrypted.empty();
}

} // namespace

int main()
{
    if (!test_runtime_sql_metadata()) {
        fprintf(stderr, "selective_filter runtime SQL metadata test failed\n");
        return 1;
    }
    if (!test_full_record_output()) {
        fprintf(stderr, "selective_filter full-record test failed\n");
        return 1;
    }
    if (!test_filter_lwe_pipeline()) {
        fprintf(stderr, "selective_filter -> lwe_encrypt test failed\n");
        return 1;
    }
    printf("selective_filter unit and filter->LWE pipeline tests passed\n");
    return 0;
}

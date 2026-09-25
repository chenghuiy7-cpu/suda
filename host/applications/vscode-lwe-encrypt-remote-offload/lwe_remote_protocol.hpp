#ifndef LWE_REMOTE_PROTOCOL_HPP
#define LWE_REMOTE_PROTOCOL_HPP

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace lwe_remote {

constexpr uint64_t kFrameRequest = 1;
constexpr uint64_t kFrameResponse = 2;
constexpr uint64_t kFrameError = 3;
constexpr uint64_t kOperationEchoU8 = 0;
constexpr uint64_t kOperationAddScalarU8 = 1;
constexpr uint64_t kOperationAddScalarU8HpuNative = 2;
constexpr uint64_t kOperationAddScalarU8HpuNativeRoundTrip = 3;

constexpr uint64_t kOperationAddU8 = 0x100;
constexpr uint64_t kOperationSubU8 = 0x101;
constexpr uint64_t kOperationMulU8 = 0x102;
constexpr uint64_t kOperationDivU8 = 0x103;
constexpr uint64_t kOperationRemU8 = 0x104;
constexpr uint64_t kOperationBitAndU8 = 0x110;
constexpr uint64_t kOperationBitOrU8 = 0x111;
constexpr uint64_t kOperationBitXorU8 = 0x112;
constexpr uint64_t kOperationBitNotU8 = 0x113;
constexpr uint64_t kOperationShlU8 = 0x120;
constexpr uint64_t kOperationShrU8 = 0x121;
constexpr uint64_t kOperationRotlU8 = 0x122;
constexpr uint64_t kOperationRotrU8 = 0x123;
constexpr uint64_t kOperationEqU8 = 0x130;
constexpr uint64_t kOperationNeU8 = 0x131;
constexpr uint64_t kOperationLtU8 = 0x132;
constexpr uint64_t kOperationLeU8 = 0x133;
constexpr uint64_t kOperationGtU8 = 0x134;
constexpr uint64_t kOperationGeU8 = 0x135;

constexpr uint64_t kOperationSubScalarU8 = 0x200;
constexpr uint64_t kOperationRsubScalarU8 = 0x201;
constexpr uint64_t kOperationMulScalarU8 = 0x202;
constexpr uint64_t kOperationDivScalarU8 = 0x203;
constexpr uint64_t kOperationRemScalarU8 = 0x204;
constexpr uint64_t kOperationShlScalarU8 = 0x210;
constexpr uint64_t kOperationShrScalarU8 = 0x211;
constexpr uint64_t kOperationRotlScalarU8 = 0x212;
constexpr uint64_t kOperationRotrScalarU8 = 0x213;

constexpr uint64_t kOperationInputHpuNative = UINT64_C(1) << 62;
constexpr uint64_t kOperationOutputHpuNative = UINT64_C(1) << 61;
constexpr uint64_t kOperationLayoutFlags =
    kOperationInputHpuNative | kOperationOutputHpuNative;

uint64_t base_operation(uint64_t operation);
bool input_is_hpu_native(uint64_t operation);
bool output_is_hpu_native(uint64_t operation);
size_t operation_operand_count(uint64_t operation);
bool operation_returns_bool(uint64_t operation);
bool operation_is_supported(uint64_t operation);

struct BatchMetadata {
    uint64_t mask_dimension;
    uint64_t item_count;
    uint64_t radix_blocks_per_item;
    uint64_t message_width;
    uint64_t carry_width;
    uint64_t padding_bit_width;
    uint64_t delta_log2;
    uint64_t ciphertext_word_count;

    bool operator==(const BatchMetadata& other) const;
};

struct RpcResult {
    BatchMetadata metadata = {};
    std::vector<uint64_t> ciphertext_words;
    uint64_t request_id = 0;
    uint64_t operation = 0;
    uint64_t scalar = 0;
    double connect_ms = 0.0;
    double request_send_ms = 0.0;
    double wait_response_header_ms = 0.0;
    double response_receive_ms = 0.0;
    double telemetry_receive_ms = 0.0;
    double round_trip_ms = 0.0;
    double server_request_receive_ms = 0.0;
    double server_request_validate_ms = 0.0;
    double server_request_decode_ms = 0.0;
    double server_hpu_prepare_ms = 0.0;
    double server_hpu_enqueue_ms = 0.0;
    double server_hpu_wait_sync_ms = 0.0;
    double server_hpu_output_convert_ms = 0.0;
    double server_result_encode_ms = 0.0;
    double server_mem_sanitizer_ms = 0.0;
    double server_process_ms = 0.0;
    double server_response_send_ms = 0.0;
    double server_total_ms = 0.0;
    uint64_t server_response_payload_bytes = 0;
};

// For binary operations, ciphertext_words must be lhs_batch followed by rhs_batch.
// metadata.ciphertext_word_count describes exactly one operand batch.
bool compute_u8(
    const std::string& host,
    uint16_t port,
    uint64_t request_id,
    uint64_t operation,
    uint8_t scalar,
    const BatchMetadata& metadata,
    const std::vector<uint64_t>& ciphertext_words,
    uint32_t connect_timeout_ms,
    uint32_t io_timeout_secs,
    size_t max_response_bytes,
    RpcResult* result,
    std::string* error);

}  // namespace lwe_remote

#endif

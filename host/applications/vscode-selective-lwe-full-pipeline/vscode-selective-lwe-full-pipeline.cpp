#include <libnvme.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "sql_filter_compiler.hpp"
#include "lwe_remote_protocol.hpp"

namespace {

constexpr size_t kLbaSize = 4096;
constexpr size_t kAxisBytes = 64;
constexpr uint32_t kComputeNsid = 2;
constexpr uint32_t kMaskDimension = 2048;
constexpr uint32_t kRadixBlockCount = 4;
constexpr uint32_t kMessageWidth = 2;
constexpr uint32_t kCarryWidth = 2;
constexpr uint32_t kPaddingWidth = 1;
constexpr uint32_t kDeltaLog2 = 59;
constexpr uint64_t kDelta = uint64_t{1} << kDeltaLog2;
constexpr uint32_t kNoiseBoundLog2 = 17;
// filter 每个输出 beat 只携带一个 quantity，不能按 TKEEP 展开成最多 64B。
constexpr uint32_t kInputModeU8RadixScalarStream = 3;
constexpr uint32_t kNoiseModeInternal = 0;
constexpr uint32_t kNoiseModeZero = 2;
constexpr uint32_t kOutputLayoutCpuLwe = 0;
constexpr uint32_t kOutputLayoutHpuNative = 1;
constexpr uint32_t kRecordBytes = 512;
constexpr uint32_t kQuantityOffset = 4;
constexpr uint32_t kPredicateGt = 0;
constexpr uint32_t kPredicateEq = 1;
constexpr uint32_t kFilterOutputQuantity = 1;
constexpr uint32_t kMaxCopyLbasPerRange = 32;
constexpr size_t kDefaultSlmReadChunkBytes = 128 * 1024;
constexpr size_t kMaxSlmReadChunkBytes = 128 * 1024;
constexpr size_t kDefaultSlmWriteChunkBytes = kLbaSize;
constexpr size_t kMaxSlmWriteChunkBytes = 128 * 1024;
constexpr int kSlmReadEintrMaxRetries = 16;
constexpr size_t kLogicalWordsPerCiphertext = kMaskDimension + 1;
constexpr size_t kPacketsPerCiphertext = (kMaskDimension / 8) + 1;
constexpr size_t kPhysicalWordsPerCiphertext = kPacketsPerCiphertext * 8;
constexpr size_t kOutputDonePacketBytes = kAxisBytes;
constexpr size_t kLogicalOutputBytes =
    kRadixBlockCount * kLogicalWordsPerCiphertext * sizeof(uint64_t);
constexpr size_t kPhysicalOutputBytes =
    kRadixBlockCount * kPacketsPerCiphertext * kAxisBytes;
constexpr size_t kHpuPcCount = 2;
constexpr size_t kHpuPcGroupWords = 16;
constexpr size_t kHpuPcDataWords = kMaskDimension / kHpuPcCount;
constexpr size_t kHpuPc0DataWords = kHpuPcDataWords + 1;
constexpr size_t kHpuPcSlotBytes = 3 * kLbaSize;
constexpr size_t kHpuPcSlotWords = kHpuPcSlotBytes / sizeof(uint64_t);
constexpr size_t kHpuNativeLweBytes = kHpuPcCount * kHpuPcSlotBytes;
constexpr size_t kHpuNativeOutputBytes = kRadixBlockCount * kHpuNativeLweBytes;
constexpr size_t kDefaultMaxResponseBytes = 512ULL * 1024 * 1024;

const char* kDefaultKeyPath =
    "../../../device/operators/hls/lwe_encrypt/testdata/"
    "psi64_big_lwe_secret_key.bin";

struct Options {
    const char* admin_device = "nvmq0";
    const char* io_device = "nvmq0n1";
    const char* key_path = kDefaultKeyPath;
    const char* output_path = "selective_lwe_fpga_ciphertexts.bin";
    const char* reference_path = "testdata/tpch_like_512b.bin";
    const char* slm_read_trace_path = nullptr;
    const char* remote_host = "10.16.0.129";
    const char* plaintext_output_path = nullptr;
    const char* remote_native_output_path = nullptr;
    const char* remote_expected_output_path = nullptr;
    uint32_t storage_nsid = 1;
    uint32_t input_lbas = 0;
    uint32_t plaintext_bytes = 0;
    uint32_t record_count = 16;
    uint32_t record_bytes = kRecordBytes;
    uint32_t threshold = 32;
    uint32_t predicate = kPredicateGt;
    size_t slm_read_chunk_bytes = kDefaultSlmReadChunkBytes;
    size_t slm_write_chunk_bytes = kDefaultSlmWriteChunkBytes;
    uint32_t slm_read_queue_depth = 1;
    uint64_t ssd_lba = 0;
    bool ssd_lba_set = false;
    bool input_lbas_set = false;
    uint32_t output_storage_nsid = 1;
    uint64_t output_ssd_lba = 0;
    bool output_storage_nsid_set = false;
    bool output_ssd_lba_set = false;
    bool in_place = false;
    uint32_t filter_operator_type_id = 0;
    uint32_t lwe_operator_type_id = 2;
    uint32_t program_id = 13;
    uint32_t decrypt_operator_type_id = 3;
    uint32_t decrypt_program_id = 12;
    uint64_t seed = 0;
    uint64_t nonce = 0;
    bool seed_set = false;
    bool nonce_set = false;
    bool zero_noise = false;
    bool benchmark = false;
    bool skip_dump = true;
    bool explain_filter = false;
    bool self_test = false;
    bool verify_decrypt_slm_write = false;
    uint16_t remote_port = 19090;
    uint8_t scalar = 1;
    uint32_t connect_timeout_ms = 10000;
    uint32_t io_timeout_secs = 300;
    size_t max_response_bytes = kDefaultMaxResponseBytes;
    uint64_t remote_operation =
        lwe_remote::kOperationAddScalarU8HpuNativeRoundTrip;
    uint32_t output_layout = kOutputLayoutHpuNative;
    std::string schema = "id:u32@0,quantity:u8@4,order_key:u64@8,price:u64@16";
    std::string query;
    bool schema_set = false;
    bool sql_mode = false;
    SqlFilterProgram sql_program;
};

void print_usage(const char* argv0)
{
    fprintf(
        stderr,
        "Usage: %s [options]\n"
        "  --ssd-nsid N       source SSD namespace id (default: 1)\n"
        "  --ssd-lba N        source SSD logical block (required)\n"
        "  --output-ssd-nsid N destination namespace (default: source nsid)\n"
        "  --output-ssd-lba N destination start LBA for the patched record image\n"
        "  --in-place         patch the source SSD range in place\n"
        "  --records N        number of fixed-size records (default: 16)\n"
        "  --record-bytes N   fixed row size; SQL mode supports 64..4096, multiple of 64\n"
        "  --schema TEXT      comma-separated name:type@offset metadata\n"
        "                     types: u8,u16,u32,u64,i8,i16,i32,i64\n"
        "  --query SQL        SELECT one u8 field FROM name WHERE expression\n"
        "                     comparisons: =,!=,<,<=,>,>=; logic: AND,OR,NOT,( )\n"
        "  --explain-filter   compile and print filter metadata without device I/O\n"
        "  --predicate gt|eq  quantity comparison (default: gt)\n"
        "  --threshold N      uint8 predicate threshold (default: 32)\n"
        "  --reference PATH   local binary image used only for correctness checking\n"
        "  --input-lbas N      copy N 4KB SSD blocks to input SLM\n"
        "                      (default: minimum needed for all records)\n"
        "  --slm-read-chunk-bytes N\n"
        "                      output SLM read size: 4KB..128KB, 4KB aligned\n"
        "                      (default: 131072; use 4096 for legacy mode)\n"
        "  --slm-read-queue-depth N\n"
        "                      concurrent SLM reads: 1, 2, or 4 (default: 1)\n"
        "  --slm-read-trace PATH\n"
        "                      append per-request SLM read latency samples to CSV\n"
        "  --key PATH         2048-byte Big-LWE key file\n"
        "  --server HOST      remote HPU server (default: 10.16.0.129)\n"
        "  --server-port N    remote HPU TCP port (default: 19090)\n"
        "  --scalar N         remote u8 ADDS scalar (default: 1)\n"
        "  --remote-operation adds|echo (default: adds)\n"
        "  --plaintext-output PATH optional decrypted selected-byte dump\n"
        "  --remote-native-output PATH save the verified remote HPU-native response\n"
        "  --remote-expected-output PATH save its Host-decoded expected u8 bytes\n"
        "  --slm-write-chunk-bytes N 4096..131072, 4KB aligned\n"
        "  --connect-timeout-ms N TCP connect timeout (default: 10000)\n"
        "  --io-timeout-secs N TCP I/O timeout (default: 300)\n"
        "  --max-response-bytes N response limit (default: 512MiB)\n"
        "  --filter-operator-type N selective_filter type id (default: 0)\n"
        "  --lwe-operator-type N    lwe_encrypt type id (default: 2)\n"
        "  --program-id N     compute program slot (default: 13)\n"
        "  --decrypt-operator-type N lwe_decrypt type id (default: 3)\n"
        "  --decrypt-program-id N decrypt program slot (default: 12)\n"
        "  --admin DEV        NVMe admin device (default: nvmq0)\n"
        "  --io DEV           NVMe I/O device (default: nvmq0n1)\n"
        "  --seed N           mask/noise PRNG seed\n"
        "  --nonce N          per-run PRNG nonce\n"
        "  --zero-noise       generate ciphertexts with exactly zero noise\n"
        "  --benchmark        print machine-readable stage timings and reduce log noise\n"
        "  --skip-dump        keep the verified result in memory without writing a dump\n"
        "  --verify-decrypt-slm-write verify remote ciphertext after Host->SLM\n"
        "  --self-test        test record patching without device I/O\n"
        "  --help             show this message\n",
        argv0);
}

bool parse_u64(const char* text, uint64_t* value)
{
    if (text == nullptr || *text == '\0') {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    unsigned long long parsed = strtoull(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0') {
        return false;
    }
    *value = static_cast<uint64_t>(parsed);
    return true;
}

bool parse_options(int argc, char** argv, Options* options)
{
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (strcmp(arg, "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        }
        if (strcmp(arg, "--zero-noise") == 0) {
            options->zero_noise = true;
            continue;
        }
        if (strcmp(arg, "--benchmark") == 0) {
            options->benchmark = true;
            continue;
        }
        if (strcmp(arg, "--skip-dump") == 0) {
            options->skip_dump = true;
            continue;
        }
        if (strcmp(arg, "--explain-filter") == 0) {
            options->explain_filter = true;
            continue;
        }
        if (strcmp(arg, "--self-test") == 0) {
            options->self_test = true;
            continue;
        }
        if (strcmp(arg, "--in-place") == 0) {
            options->in_place = true;
            continue;
        }
        if (strcmp(arg, "--verify-decrypt-slm-write") == 0) {
            options->verify_decrypt_slm_write = true;
            continue;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "Missing value for %s\n", arg);
            return false;
        }

        const char* text = argv[++i];
        uint64_t value = 0;
        if (strcmp(arg, "--key") == 0) {
            options->key_path = text;
        } else if (strcmp(arg, "--server") == 0) {
            options->remote_host = text;
        } else if (strcmp(arg, "--plaintext-output") == 0) {
            options->plaintext_output_path = text;
        } else if (strcmp(arg, "--remote-native-output") == 0) {
            options->remote_native_output_path = text;
        } else if (strcmp(arg, "--remote-expected-output") == 0) {
            options->remote_expected_output_path = text;
        } else if (strcmp(arg, "--remote-operation") == 0) {
            if (strcmp(text, "adds") == 0) {
                options->remote_operation =
                    lwe_remote::kOperationAddScalarU8HpuNativeRoundTrip;
            } else if (strcmp(text, "echo") == 0) {
                options->remote_operation = lwe_remote::kOperationEchoU8;
            } else {
                fprintf(stderr, "Invalid remote operation: %s\n", text);
                return false;
            }
        } else if (strcmp(arg, "--output") == 0) {
            options->output_path = text;
        } else if (strcmp(arg, "--reference") == 0) {
            options->reference_path = text;
        } else if (strcmp(arg, "--schema") == 0) {
            options->schema = text;
            options->schema_set = true;
        } else if (strcmp(arg, "--query") == 0) {
            options->query = text;
            options->sql_mode = true;
        } else if (strcmp(arg, "--slm-read-trace") == 0) {
            options->slm_read_trace_path = text;
        } else if (strcmp(arg, "--output-layout") == 0) {
            if (strcmp(text, "hpu-native") == 0) {
                options->output_layout = kOutputLayoutHpuNative;
            } else if (strcmp(text, "cpu") == 0) {
                options->output_layout = kOutputLayoutCpuLwe;
            } else {
                fprintf(stderr, "Invalid output layout: %s\n", text);
                return false;
            }
        } else if (strcmp(arg, "--admin") == 0) {
            options->admin_device = text;
        } else if (strcmp(arg, "--io") == 0) {
            options->io_device = text;
        } else if (strcmp(arg, "--ssd-nsid") == 0) {
            if (!parse_u64(text, &value) || value == 0 || value > UINT32_MAX) {
                fprintf(stderr, "Invalid SSD namespace id: %s\n", text);
                return false;
            }
            options->storage_nsid = static_cast<uint32_t>(value);
        } else if (strcmp(arg, "--output-ssd-nsid") == 0) {
            if (!parse_u64(text, &value) || value == 0 || value > UINT32_MAX) {
                fprintf(stderr, "Invalid output SSD namespace id: %s\n", text);
                return false;
            }
            options->output_storage_nsid = static_cast<uint32_t>(value);
            options->output_storage_nsid_set = true;
        } else if (strcmp(arg, "--ssd-lba") == 0) {
            if (!parse_u64(text, &options->ssd_lba)) {
                fprintf(stderr, "Invalid SSD LBA: %s\n", text);
                return false;
            }
            options->ssd_lba_set = true;
        } else if (strcmp(arg, "--output-ssd-lba") == 0) {
            if (!parse_u64(text, &options->output_ssd_lba)) {
                fprintf(stderr, "Invalid output SSD LBA: %s\n", text);
                return false;
            }
            options->output_ssd_lba_set = true;
        } else if (strcmp(arg, "--input-lbas") == 0) {
            if (!parse_u64(text, &value) || value == 0 || value > UINT32_MAX) {
                fprintf(stderr, "Invalid input LBA count: %s\n", text);
                return false;
            }
            options->input_lbas = static_cast<uint32_t>(value);
            options->input_lbas_set = true;
        } else if (strcmp(arg, "--records") == 0) {
            if (!parse_u64(text, &value) || value == 0 || value > UINT32_MAX) {
                fprintf(stderr, "Invalid record count: %s\n", text);
                return false;
            }
            options->record_count = static_cast<uint32_t>(value);
        } else if (strcmp(arg, "--record-bytes") == 0) {
            if (!parse_u64(text, &value) || value < kAxisBytes ||
                value > SELECTIVE_FILTER_V2_MAX_RECORD_BYTES || value % kAxisBytes != 0) {
                fprintf(stderr, "Invalid record size: %s; expected 64..4096, multiple of 64\n", text);
                return false;
            }
            options->record_bytes = static_cast<uint32_t>(value);
        } else if (strcmp(arg, "--threshold") == 0) {
            if (!parse_u64(text, &value) || value > UINT8_MAX) {
                fprintf(stderr, "Invalid uint8 threshold: %s\n", text);
                return false;
            }
            options->threshold = static_cast<uint32_t>(value);
        } else if (strcmp(arg, "--predicate") == 0) {
            if (strcmp(text, "gt") == 0) {
                options->predicate = kPredicateGt;
            } else if (strcmp(text, "eq") == 0) {
                options->predicate = kPredicateEq;
            } else {
                fprintf(stderr, "Invalid predicate: %s; use gt or eq\n", text);
                return false;
            }
        } else if (strcmp(arg, "--slm-read-chunk-bytes") == 0) {
            if (!parse_u64(text, &value) || value < kLbaSize ||
                value > kMaxSlmReadChunkBytes || value % kLbaSize != 0) {
                fprintf(
                    stderr,
                    "Invalid SLM read chunk: %s; expected a 4KB-aligned value from 4096 to %zu\n",
                    text,
                    kMaxSlmReadChunkBytes);
                return false;
            }
            options->slm_read_chunk_bytes = static_cast<size_t>(value);
        } else if (strcmp(arg, "--slm-write-chunk-bytes") == 0) {
            if (!parse_u64(text, &value) || value < kLbaSize ||
                value > kMaxSlmWriteChunkBytes || value % kLbaSize != 0) {
                fprintf(stderr, "Invalid SLM write chunk size: %s\n", text);
                return false;
            }
            options->slm_write_chunk_bytes = static_cast<size_t>(value);
        } else if (strcmp(arg, "--slm-read-queue-depth") == 0) {
            if (!parse_u64(text, &value) ||
                (value != 1 && value != 2 && value != 4)) {
                fprintf(
                    stderr,
                    "Invalid SLM read queue depth: %s; expected 1, 2, or 4\n",
                    text);
                return false;
            }
            options->slm_read_queue_depth = static_cast<uint32_t>(value);
        } else if (strcmp(arg, "--filter-operator-type") == 0) {
            if (!parse_u64(text, &value) || value > UINT8_MAX) {
                fprintf(stderr, "Invalid filter operator type: %s\n", text);
                return false;
            }
            options->filter_operator_type_id = static_cast<uint32_t>(value);
        } else if (strcmp(arg, "--lwe-operator-type") == 0) {
            if (!parse_u64(text, &value) || value > UINT8_MAX) {
                fprintf(stderr, "Invalid LWE operator type: %s\n", text);
                return false;
            }
            options->lwe_operator_type_id = static_cast<uint32_t>(value);
        } else if (strcmp(arg, "--program-id") == 0) {
            if (!parse_u64(text, &value) || value >= 64) {
                fprintf(stderr, "Invalid program id: %s\n", text);
                return false;
            }
            options->program_id = static_cast<uint32_t>(value);
        } else if (strcmp(arg, "--decrypt-operator-type") == 0) {
            if (!parse_u64(text, &value) || value > UINT8_MAX) {
                fprintf(stderr, "Invalid decrypt operator type: %s\n", text);
                return false;
            }
            options->decrypt_operator_type_id = static_cast<uint32_t>(value);
        } else if (strcmp(arg, "--decrypt-program-id") == 0) {
            if (!parse_u64(text, &value) || value >= 64) {
                fprintf(stderr, "Invalid decrypt program id: %s\n", text);
                return false;
            }
            options->decrypt_program_id = static_cast<uint32_t>(value);
        } else if (strcmp(arg, "--server-port") == 0) {
            if (!parse_u64(text, &value) || value == 0 || value > UINT16_MAX) {
                fprintf(stderr, "Invalid remote server port: %s\n", text);
                return false;
            }
            options->remote_port = static_cast<uint16_t>(value);
        } else if (strcmp(arg, "--scalar") == 0) {
            if (!parse_u64(text, &value) || value > UINT8_MAX) {
                fprintf(stderr, "Invalid u8 scalar: %s\n", text);
                return false;
            }
            options->scalar = static_cast<uint8_t>(value);
        } else if (strcmp(arg, "--connect-timeout-ms") == 0) {
            if (!parse_u64(text, &value) || value == 0 || value > UINT32_MAX) {
                fprintf(stderr, "Invalid connect timeout: %s\n", text);
                return false;
            }
            options->connect_timeout_ms = static_cast<uint32_t>(value);
        } else if (strcmp(arg, "--io-timeout-secs") == 0) {
            if (!parse_u64(text, &value) || value == 0 || value > UINT32_MAX) {
                fprintf(stderr, "Invalid I/O timeout: %s\n", text);
                return false;
            }
            options->io_timeout_secs = static_cast<uint32_t>(value);
        } else if (strcmp(arg, "--max-response-bytes") == 0) {
            if (!parse_u64(text, &value) || value == 0 || value > SIZE_MAX) {
                fprintf(stderr, "Invalid response size limit: %s\n", text);
                return false;
            }
            options->max_response_bytes = static_cast<size_t>(value);
        } else if (strcmp(arg, "--seed") == 0) {
            if (!parse_u64(text, &options->seed)) {
                fprintf(stderr, "Invalid seed: %s\n", text);
                return false;
            }
            options->seed_set = true;
        } else if (strcmp(arg, "--nonce") == 0) {
            if (!parse_u64(text, &options->nonce)) {
                fprintf(stderr, "Invalid nonce: %s\n", text);
                return false;
            }
            options->nonce_set = true;
        } else {
            fprintf(stderr, "Unknown option: %s\n", arg);
            return false;
        }
    }
    if (options->self_test) {
        return true;
    }
    if (!options->ssd_lba_set && !options->explain_filter) {
        fprintf(stderr, "--ssd-lba is required to select the source SSD block\n");
        return false;
    }
    if (options->schema_set && !options->sql_mode) {
        fprintf(stderr, "--schema requires --query\n");
        return false;
    }
    if (options->sql_mode) {
        std::vector<SqlField> fields;
        std::string error;
        if (!parse_sql_schema(options->schema, options->record_bytes, &fields, &error) ||
            !compile_filter_sql(options->query, options->record_bytes, fields,
                                &options->sql_program, &error)) {
            fprintf(stderr, "Invalid SQL filter metadata: %s\n", error.c_str());
            return false;
        }
        if (options->sql_program.projection_type != SELECTIVE_FILTER_TYPE_U8) {
            fprintf(stderr, "SELECT field must be u8 when connected to lwe_encrypt scalar mode\n");
            return false;
        }
    } else if (options->record_bytes != kRecordBytes) {
        fprintf(stderr, "--record-bytes other than 512 requires --query\n");
        return false;
    }
    if (options->explain_filter) {
        return true;
    }
    if (options->output_layout != kOutputLayoutHpuNative) {
        fprintf(stderr, "The remote HPU round trip requires --output-layout hpu-native\n");
        return false;
    }
    if (options->remote_host == nullptr || options->remote_host[0] == '\0') {
        fprintf(stderr, "--server must not be empty\n");
        return false;
    }
    if (options->in_place && options->output_ssd_lba_set) {
        fprintf(stderr, "Use either --in-place or --output-ssd-lba, not both\n");
        return false;
    }
    if (!options->in_place && !options->output_ssd_lba_set) {
        fprintf(stderr, "--output-ssd-lba is required unless --in-place is used\n");
        return false;
    }
    if (!options->output_storage_nsid_set) {
        options->output_storage_nsid = options->storage_nsid;
    }
    if (options->in_place) {
        options->output_storage_nsid = options->storage_nsid;
        options->output_ssd_lba = options->ssd_lba;
        options->output_ssd_lba_set = true;
    }
    if (options->program_id == options->decrypt_program_id) {
        fprintf(stderr, "Filter/encrypt and decrypt program ids must differ\n");
        return false;
    }
    uint64_t record_bytes =
        static_cast<uint64_t>(options->record_count) * options->record_bytes;
    uint64_t required_lbas = (record_bytes + kLbaSize - 1) / kLbaSize;
    if (!options->input_lbas_set) {
        options->input_lbas = static_cast<uint32_t>(required_lbas);
    }
    uint64_t input_bytes = static_cast<uint64_t>(options->input_lbas) * kLbaSize;
    if (input_bytes > INT_MAX) {
        fprintf(stderr, "Input SLM exceeds the runtime's signed 32-bit size limit\n");
        return false;
    }
    if (record_bytes > input_bytes) {
        fprintf(
            stderr,
            "%u records need %llu bytes, exceeding --input-lbas %u\n",
            options->record_count,
            static_cast<unsigned long long>(record_bytes),
            options->input_lbas);
        return false;
    }
    uint64_t physical_output =
        static_cast<uint64_t>(options->record_count) *
        (options->output_layout == kOutputLayoutHpuNative
             ? kHpuNativeOutputBytes
             : kPhysicalOutputBytes);
    uint64_t output_with_done = physical_output + kOutputDonePacketBytes;
    uint64_t output_aligned =
        ((output_with_done + kLbaSize - 1) / kLbaSize) * kLbaSize;
    if (output_aligned > INT_MAX) {
        fprintf(
            stderr,
            "Requested batch needs %llu output bytes, exceeding the runtime's signed 32-bit SLM limit\n",
            static_cast<unsigned long long>(output_aligned));
        return false;
    }
    if (physical_output > options->max_response_bytes) {
        fprintf(stderr,
                "Worst-case remote response is %llu bytes, exceeding --max-response-bytes %zu\n",
                static_cast<unsigned long long>(physical_output),
                options->max_response_bytes);
        return false;
    }
    if (options->ssd_lba > UINT64_MAX - options->input_lbas ||
        options->output_ssd_lba > UINT64_MAX - options->input_lbas) {
        fprintf(stderr, "Source or destination SSD LBA range overflows\n");
        return false;
    }
    if (!options->in_place &&
        options->storage_nsid == options->output_storage_nsid &&
        options->ssd_lba < options->output_ssd_lba + options->input_lbas &&
        options->output_ssd_lba < options->ssd_lba + options->input_lbas) {
        fprintf(stderr, "Source and destination SSD ranges overlap; use --in-place for intentional overwrite\n");
        return false;
    }
    return true;
}

bool read_reference_selection(
    const Options& options,
    std::vector<uint8_t>* selected_quantities,
    std::vector<uint8_t>* selected_records,
    std::vector<uint32_t>* selected_indices,
    std::vector<uint8_t>* reference_records)
{
    std::ifstream file(options.reference_path, std::ios::binary);
    if (!file) {
        fprintf(stderr, "Unable to open reference dataset: %s\n", options.reference_path);
        return false;
    }
    std::vector<uint8_t> bytes{
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()};
    const size_t required = static_cast<size_t>(options.record_count) * options.record_bytes;
    if (bytes.size() < required) {
        fprintf(
            stderr,
            "Reference dataset has %zu bytes; %u records require %zu bytes\n",
            bytes.size(),
            options.record_count,
            required);
        return false;
    }

    selected_quantities->clear();
    selected_records->clear();
    selected_indices->clear();
    reference_records->assign(bytes.begin(), bytes.begin() + required);
    for (uint32_t record = 0; record < options.record_count; ++record) {
        size_t record_offset = static_cast<size_t>(record) * options.record_bytes;
        uint8_t quantity = 0;
        bool selected = false;
        if (options.sql_mode) {
            uint64_t projection = 0;
            std::string error;
            if (!evaluate_filter_sql(options.sql_program, bytes.data() + record_offset,
                                     options.record_bytes, &selected, &projection, &error)) {
                fprintf(stderr, "Reference SQL evaluation failed at record %u: %s\n",
                        record, error.c_str());
                return false;
            }
            quantity = static_cast<uint8_t>(projection);
        } else {
            quantity = bytes[record_offset + kQuantityOffset];
            selected = options.predicate == kPredicateGt
                ? quantity > options.threshold : quantity == options.threshold;
        }
        if (selected) {
            selected_quantities->push_back(quantity);
            selected_indices->push_back(record);
            selected_records->insert(
                selected_records->end(),
                bytes.begin() + record_offset,
                bytes.begin() + record_offset + options.record_bytes);
        }
    }
    if (selected_quantities->empty()) {
        fprintf(
            stderr,
            "Predicate selected zero records; choose a threshold with at least one match for the first demo\n");
        return false;
    }
    return true;
}

uint64_t random_u64()
{
    uint64_t value = 0;
    ssize_t bytes = getrandom(&value, sizeof(value), 0);
    if (bytes == static_cast<ssize_t>(sizeof(value))) {
        return value;
    }

    struct timeval now;
    gettimeofday(&now, nullptr);
    return (static_cast<uint64_t>(now.tv_sec) << 32) ^
           static_cast<uint64_t>(now.tv_usec) ^
           static_cast<uint64_t>(getpid());
}

bool read_binary_key(const char* path, std::vector<uint8_t>* key)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        fprintf(stderr, "Unable to open key file: %s\n", path);
        return false;
    }

    key->assign(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>());
    if (key->size() != kMaskDimension) {
        fprintf(
            stderr,
            "Key file must contain exactly %u binary coefficients; got %zu bytes\n",
            kMaskDimension,
            key->size());
        return false;
    }

    for (size_t i = 0; i < key->size(); ++i) {
        if ((*key)[i] > 1) {
            fprintf(stderr, "Key coefficient %zu is not binary\n", i);
            return false;
        }
    }
    return true;
}

void store_u32(uint8_t* data, size_t offset, uint32_t value)
{
    memcpy(data + offset, &value, sizeof(value));
}

void store_u64(uint8_t* data, size_t offset, uint64_t value)
{
    memcpy(data + offset, &value, sizeof(value));
}

void build_contexts(
    uint8_t* contexts,
    const std::vector<uint8_t>& key,
    const Options& options)
{
    memset(contexts, 0, 2 * kLbaSize);
    uint8_t* filter_context = contexts;
    uint8_t* lwe_context = contexts + kLbaSize;

    // Context page 0 belongs to selective_filter (logical operator 0).
    store_u32(filter_context, 0, options.record_count);
    store_u32(filter_context, 4, options.record_bytes);
    store_u32(filter_context, 20, kFilterOutputQuantity);
    if (options.sql_mode) {
        store_u32(filter_context, SELECTIVE_FILTER_V2_HDR_MAGIC, SELECTIVE_FILTER_V2_MAGIC);
        store_u32(filter_context, SELECTIVE_FILTER_V2_HDR_VERSION, SELECTIVE_FILTER_V2_VERSION);
        store_u32(filter_context, SELECTIVE_FILTER_V2_HDR_PREDICATE_COUNT,
                  options.sql_program.predicates.size());
        store_u32(filter_context, SELECTIVE_FILTER_V2_HDR_TOKEN_COUNT,
                  options.sql_program.tokens.size());
        store_u32(filter_context, SELECTIVE_FILTER_V2_HDR_PROJECTION_OFFSET,
                  options.sql_program.projection_offset);
        store_u32(filter_context, SELECTIVE_FILTER_V2_HDR_PROJECTION_TYPE,
                  options.sql_program.projection_type);
        memcpy(filter_context + SELECTIVE_FILTER_V2_HDR_TOKENS,
               options.sql_program.tokens.data(), options.sql_program.tokens.size());
        for (size_t i = 0; i < options.sql_program.predicates.size(); ++i) {
            const SqlPredicate &predicate = options.sql_program.predicates[i];
            uint8_t *descriptor = filter_context +
                SELECTIVE_FILTER_V2_PREDICATES_OFFSET +
                i * SELECTIVE_FILTER_V2_PREDICATE_BYTES;
            store_u32(descriptor, SELECTIVE_FILTER_V2_PRED_OFFSET, predicate.offset);
            descriptor[SELECTIVE_FILTER_V2_PRED_TYPE] = predicate.type;
            descriptor[SELECTIVE_FILTER_V2_PRED_COMPARISON] = predicate.comparison;
            store_u64(descriptor, SELECTIVE_FILTER_V2_PRED_LITERAL, predicate.literal);
        }
    } else {
        store_u32(filter_context, 8, kQuantityOffset);
        store_u32(filter_context, 12, options.predicate);
        store_u32(filter_context, 16, options.threshold);
    }

    // Context page 1 belongs to lwe_encrypt (logical operator 1). input_count
    // is zero because the filter determines the number of selected bytes.
    store_u32(lwe_context, 0, kMaskDimension);
    store_u32(lwe_context, 4, 0);
    store_u32(lwe_context, 8, kInputModeU8RadixScalarStream);
    store_u32(
        lwe_context,
        12,
        options.zero_noise ? kNoiseModeZero : kNoiseModeInternal);
    store_u32(lwe_context, 16, kNoiseBoundLog2);
    store_u32(lwe_context, 20, options.output_layout);
    store_u64(lwe_context, 32, kDelta);
    store_u64(lwe_context, 40, options.seed);
    store_u64(lwe_context, 48, options.nonce);

    // Host offset 64 becomes LWE HLS context word 4.
    for (size_t i = 0; i < key.size(); ++i) {
        if (key[i] != 0) {
            lwe_context[kAxisBytes + (i / 8)] |= uint8_t{1} << (i % 8);
        }
    }
}

void build_program(
    hlsacccompute_program* program,
    const Options& options)
{
    memset(program, 0, sizeof(*program));
    program->input_channum = 1;
    program->output_channum = 1;
    program->program_id = options.program_id;
    program->apply_operators_id_map[0] =
        static_cast<uint8_t>(options.filter_operator_type_id);
    program->apply_operators_id_map[1] =
        static_cast<uint8_t>(options.lwe_operator_type_id);

    program->applyops[0].header.cid = 0;
    program->applyops[0].header.opc = APPLY_OPS;
    program->applyops[0].header.ops_num = 2;
    program->applyops[2].apply_ops_payload2.connections_num = 1;
    program->applyops[2].apply_ops_payload2.connections[0].from = 0x00;
    program->applyops[2].apply_ops_payload2.connections[0].to = 0x10;
    program->applyops[4].apply_ops_payload2.connections_num = 1;
    program->applyops[4].apply_ops_payload2.connections[0].from = 0x10;
    program->applyops[4].apply_ops_payload2.connections[0].to = 0xf0;

    program->pauseops[0].header.cid = 0;
    program->pauseops[0].header.opc = SUSPEND_OPS;
    program->pauseops[0].header.ops_num = 2;
    program->pauseops[1].generic_ops_payload.op_lists[0] = 0;
    program->pauseops[1].generic_ops_payload.op_lists[1] = 1;

    program->freeops[0].header.cid = 0;
    program->freeops[0].header.opc = FORCE_FREE_OPS;
    program->freeops[0].header.ops_num = 2;
    program->freeops[1].generic_ops_payload.op_lists[0] = 0;
    program->freeops[1].generic_ops_payload.op_lists[1] = 1;

    program->input_channel_destination[0] = 0;
    program->apply_ops_size = 5;
    program->apply_operators_num = 2;
    uint64_t estimated_us = static_cast<uint64_t>(options.record_count) * 200;
    uint64_t maximum_us = static_cast<uint64_t>(options.record_count) * 1000;
    program->esti_executed_time =
        estimated_us > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(estimated_us);
    program->max_responded_time =
        maximum_us > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(maximum_us);
}

size_t round_up_to_lba(size_t value)
{
    return ((value + kLbaSize - 1) / kLbaSize) * kLbaSize;
}

size_t input_buffer_bytes(const Options& options)
{
    return static_cast<size_t>(options.input_lbas) * kLbaSize;
}

size_t stream_input_bytes(const Options& options)
{
    return static_cast<size_t>(options.record_count) * options.record_bytes;
}

size_t input_range_bytes(const Options& options)
{
    size_t stream_bytes = stream_input_bytes(options);
    return ((stream_bytes + kLbaSize - 1) / kLbaSize) * kLbaSize;
}

size_t physical_output_bytes(const Options& options)
{
    const size_t bytes_per_u8 = options.output_layout == kOutputLayoutHpuNative
        ? kHpuNativeOutputBytes
        : kPhysicalOutputBytes;
    return static_cast<size_t>(options.plaintext_bytes) * bytes_per_u8;
}

size_t output_buffer_bytes(const Options& options)
{
    const size_t bytes_per_u8 = options.output_layout == kOutputLayoutHpuNative
        ? kHpuNativeOutputBytes
        : kPhysicalOutputBytes;
    return round_up_to_lba(
        static_cast<size_t>(options.record_count) * bytes_per_u8 +
        kOutputDonePacketBytes);
}

struct SlmReadSample {
    size_t request_index;
    size_t worker_index;
    size_t offset_bytes;
    size_t length_bytes;
    double elapsed_ms;
    int result;
    int error_number;
    int eintr_retries;
    bool completed;
};

int read_slm_in_chunks(
    int io_fd,
    unsigned int mem_id,
    size_t offset,
    size_t bytes,
    size_t chunk_bytes,
    uint32_t queue_depth,
    void* output,
    size_t* failed_offset,
    size_t* failed_length,
    int* failed_errno,
    bool verbose,
    std::vector<SlmReadSample>* samples)
{
    uint8_t* out = static_cast<uint8_t*>(output);
    const size_t request_count = (bytes + chunk_bytes - 1) / chunk_bytes;
    const uint32_t active_queue_depth =
        std::min<uint32_t>(queue_depth, static_cast<uint32_t>(request_count));
    std::vector<SlmReadSample> local_samples;
    std::vector<SlmReadSample>* records =
        samples != nullptr ? samples : &local_samples;
    records->assign(
        request_count,
        SlmReadSample{0, 0, 0, 0, 0.0, INT_MIN, 0, 0, false});
    std::atomic<size_t> next_request{0};
    std::atomic<bool> stop{false};

    auto worker = [&](size_t worker_index) {
        while (!stop.load(std::memory_order_relaxed)) {
            const size_t request_index =
                next_request.fetch_add(1, std::memory_order_relaxed);
            if (request_index >= request_count) {
                return;
            }
            const size_t request_done = request_index * chunk_bytes;
            const size_t request_offset = offset + request_done;
            const size_t chunk = std::min(chunk_bytes, bytes - request_done);
            int ret = 0;
            int eintr_retries = 0;
            int request_errno = 0;
            if (verbose) {
                fprintf(
                    stderr,
                    "[lwe_encrypt] reading output SLM request=%zu worker=%zu "
                    "offset=%zu length=%zu\n",
                    request_index,
                    worker_index,
                    request_offset,
                    chunk);
                fflush(stderr);
            }
            const auto request_start = std::chrono::steady_clock::now();
            while (true) {
                errno = 0;
                ret = nvme_slm_read(
                    io_fd,
                    mem_id,
                    static_cast<int>(request_offset),
                    static_cast<int>(chunk),
                    out + request_done);
                request_errno = errno;
                if (ret == 0) {
                    break;
                }
                if (errno != EINTR ||
                    eintr_retries >= kSlmReadEintrMaxRetries) {
                    break;
                }
                ++eintr_retries;
                if (verbose) {
                    fprintf(
                        stderr,
                        "[lwe_encrypt] nvme_slm_read EINTR retry %d/%d "
                        "at request=%zu offset=%zu\n",
                        eintr_retries,
                        kSlmReadEintrMaxRetries,
                        request_index,
                        request_offset);
                    fflush(stderr);
                }
            }
            const auto request_end = std::chrono::steady_clock::now();
            (*records)[request_index] = SlmReadSample{
                request_index,
                worker_index,
                request_offset,
                chunk,
                std::chrono::duration<double, std::milli>(
                    request_end - request_start)
                    .count(),
                ret,
                request_errno,
                eintr_retries,
                true};
            if (ret != 0) {
                stop.store(true, std::memory_order_relaxed);
                return;
            }
        }
    };

    if (active_queue_depth == 1) {
        worker(0);
    } else {
        std::vector<std::thread> workers;
        workers.reserve(active_queue_depth);
        for (uint32_t worker_index = 0; worker_index < active_queue_depth;
             ++worker_index) {
            workers.emplace_back(worker, worker_index);
        }
        for (std::thread& thread : workers) {
            thread.join();
        }
    }

    for (const SlmReadSample& sample : *records) {
        if (sample.completed && sample.result != 0) {
            if (failed_offset != nullptr) {
                *failed_offset = sample.offset_bytes;
            }
            if (failed_length != nullptr) {
                *failed_length = sample.length_bytes;
            }
            if (failed_errno != nullptr) {
                *failed_errno = sample.error_number;
            }
            return sample.result;
        }
    }
    return 0;
}

bool append_slm_read_trace(
    const char* path,
    const Options& options,
    size_t expected_request_count,
    const std::vector<SlmReadSample>& samples)
{
    static const char kTraceHeader[] =
        "run_nonce,seed,plaintext_bytes,chunk_bytes,queue_depth,"
        "expected_request_count,request_index,worker_index,offset_bytes,"
        "length_bytes,elapsed_ms,ret,errno,eintr_retries\n";
    FILE* output = fopen(path, "a+");
    if (output == nullptr) {
        fprintf(stderr, "Unable to open SLM read trace: %s\n", path);
        return false;
    }
    if (fseek(output, 0, SEEK_END) != 0) {
        fprintf(stderr, "Unable to seek SLM read trace: %s\n", path);
        fclose(output);
        return false;
    }
    long output_size = ftell(output);
    if (output_size < 0) {
        fprintf(stderr, "Unable to query SLM read trace size: %s\n", path);
        fclose(output);
        return false;
    }
    if (output_size == 0) {
        fputs(kTraceHeader, output);
    } else {
        rewind(output);
        char existing_header[512] = {};
        if (fgets(existing_header, sizeof(existing_header), output) == nullptr ||
            strcmp(existing_header, kTraceHeader) != 0) {
            fprintf(
                stderr,
                "SLM read trace header does not match the current format: %s\n"
                "Use a new trace file for queue-depth measurements.\n",
                path);
            fclose(output);
            return false;
        }
        if (fseek(output, 0, SEEK_END) != 0) {
            fprintf(stderr, "Unable to seek SLM read trace: %s\n", path);
            fclose(output);
            return false;
        }
    }
    for (const SlmReadSample& sample : samples) {
        if (!sample.completed) {
            continue;
        }
        fprintf(
            output,
            "0x%016llx,0x%016llx,%u,%zu,%u,%zu,%zu,%zu,%zu,%zu,"
            "%.6f,%d,%d,%d\n",
            static_cast<unsigned long long>(options.nonce),
            static_cast<unsigned long long>(options.seed),
            options.plaintext_bytes,
            options.slm_read_chunk_bytes,
            options.slm_read_queue_depth,
            expected_request_count,
            sample.request_index,
            sample.worker_index,
            sample.offset_bytes,
            sample.length_bytes,
            sample.elapsed_ms,
            sample.result,
            sample.error_number,
            sample.eintr_retries);
    }
    if (fclose(output) != 0) {
        fprintf(stderr, "Unable to close SLM read trace: %s\n", path);
        return false;
    }
    return true;
}

void print_slm_read_request_stats(const std::vector<SlmReadSample>& samples)
{
    std::vector<double> successful;
    successful.reserve(samples.size());
    size_t failed = 0;
    const SlmReadSample* slowest = nullptr;
    double sum_ms = 0.0;
    for (const SlmReadSample& sample : samples) {
        if (!sample.completed) {
            continue;
        }
        if (sample.result != 0) {
            ++failed;
            continue;
        }
        successful.push_back(sample.elapsed_ms);
        sum_ms += sample.elapsed_ms;
        if (slowest == nullptr || sample.elapsed_ms > slowest->elapsed_ms) {
            slowest = &sample;
        }
    }
    if (successful.empty()) {
        printf("slm_read_request_stats count=0 failed=%zu\n", failed);
        return;
    }
    std::sort(successful.begin(), successful.end());
    const size_t count = successful.size();
    const double p50 = count % 2 == 0
        ? (successful[count / 2 - 1] + successful[count / 2]) / 2.0
        : successful[count / 2];
    const size_t p95_index = (count * 95 + 99) / 100 - 1;
    printf(
        "slm_read_request_stats count=%zu failed=%zu mean_ms=%.3f "
        "p50_ms=%.3f p95_ms=%.3f max_ms=%.3f max_index=%zu "
        "max_offset=%zu max_length=%zu\n",
        count,
        failed,
        sum_ms / static_cast<double>(count),
        p50,
        successful[p95_index],
        slowest->elapsed_ms,
        slowest->request_index,
        slowest->offset_bytes,
        slowest->length_bytes);
}

size_t source_range_count(const Options& options)
{
    return (static_cast<size_t>(options.input_lbas) + kMaxCopyLbasPerRange - 1) /
           kMaxCopyLbasPerRange;
}

uint64_t load_word(const uint8_t* data, size_t word_index)
{
    uint64_t value = 0;
    memcpy(&value, data + word_index * sizeof(uint64_t), sizeof(value));
    return value;
}

size_t reverse_psi64_mask_index(size_t index)
{
    size_t reversed = 0;
    for (size_t bit = 0; bit < 11; ++bit) {
        reversed = (reversed << 1) | ((index >> bit) & 1);
    }
    return reversed;
}

size_t hpu_native_word_index(size_t lwe_index, size_t pc, size_t pc_offset)
{
    return lwe_index * kHpuPcCount * kHpuPcSlotWords +
           pc * kHpuPcSlotWords + pc_offset;
}

bool extract_hpu_native_and_verify(
    const uint8_t* raw,
    size_t raw_bytes,
    size_t clear_count,
    const std::vector<uint8_t>& key,
    bool expected_value_set,
    uint8_t expected_value,
    bool zero_noise,
    std::vector<uint8_t>* clears,
    std::vector<uint64_t>* logical_words)
{
    const size_t lwe_count = clear_count * kRadixBlockCount;
    const size_t required_words =
        lwe_count * kHpuPcCount * kHpuPcSlotWords;
    if (raw_bytes / sizeof(uint64_t) < required_words) {
        return false;
    }

    clears->clear();
    clears->reserve(clear_count);
    logical_words->clear();
    logical_words->reserve(lwe_count * kLogicalWordsPerCiphertext);

    for (size_t clear_index = 0; clear_index < clear_count; ++clear_index) {
        uint8_t reconstructed = 0;

        for (size_t block = 0; block < kRadixBlockCount; ++block) {
            const size_t lwe_index = clear_index * kRadixBlockCount + block;
            uint64_t dot = 0;

            for (size_t natural_index = 0;
                 natural_index < kMaskDimension;
                 ++natural_index) {
                const size_t hpu_index =
                    reverse_psi64_mask_index(natural_index);
                const size_t group = hpu_index / kHpuPcGroupWords;
                const size_t lane = hpu_index % kHpuPcGroupWords;
                const size_t pc = group % kHpuPcCount;
                const size_t pc_offset =
                    (group / kHpuPcCount) * kHpuPcGroupWords + lane;
                const uint64_t mask = load_word(
                    raw,
                    hpu_native_word_index(lwe_index, pc, pc_offset));
                logical_words->push_back(mask);
                if (key[natural_index] != 0) {
                    dot += mask;
                }
            }

            const uint64_t body = load_word(
                raw,
                hpu_native_word_index(lwe_index, 0, kHpuPcDataWords));
            logical_words->push_back(body);

            for (size_t i = kHpuPc0DataWords; i < kHpuPcSlotWords; ++i) {
                if (load_word(raw, hpu_native_word_index(lwe_index, 0, i)) != 0) {
                    return false;
                }
            }
            for (size_t i = kHpuPcDataWords; i < kHpuPcSlotWords; ++i) {
                if (load_word(raw, hpu_native_word_index(lwe_index, 1, i)) != 0) {
                    return false;
                }
            }

            const uint64_t phase = body - dot;
            const uint64_t decoded =
                ((phase + (kDelta / 2)) >> kDeltaLog2) &
                ((uint64_t{1} << kMessageWidth) - 1);
            const uint64_t encoded = decoded * kDelta;
            const int64_t error = static_cast<int64_t>(phase - encoded);
            const int64_t max_error =
                zero_noise ? 0 : ((int64_t{1} << kNoiseBoundLog2) - 1);
            if (error < -max_error || error > max_error) {
                return false;
            }

            if (expected_value_set && clear_index == 0) {
                const uint64_t expected_block =
                    (expected_value >> (block * kMessageWidth)) &
                    ((uint64_t{1} << kMessageWidth) - 1);
                if (decoded != expected_block) {
                    return false;
                }
            }
            reconstructed |=
                static_cast<uint8_t>(decoded << (block * kMessageWidth));
        }

        if (expected_value_set && clear_index == 0 &&
            reconstructed != expected_value) {
            return false;
        }
        clears->push_back(reconstructed);
    }
    return true;
}

bool extract_and_verify(
    const uint8_t* raw,
    size_t raw_bytes,
    size_t clear_count,
    size_t stride_words,
    const std::vector<uint8_t>& key,
    bool expected_value_set,
    uint8_t expected_value,
    bool zero_noise,
    std::vector<uint8_t>* clears,
    std::vector<uint64_t>* logical_words)
{
    const size_t required_words =
        (clear_count * kRadixBlockCount - 1) * stride_words +
        kLogicalWordsPerCiphertext;
    if (raw_bytes / sizeof(uint64_t) < required_words) {
        return false;
    }

    clears->clear();
    clears->reserve(clear_count);
    logical_words->clear();
    logical_words->reserve(
        clear_count * kRadixBlockCount * kLogicalWordsPerCiphertext);

    for (size_t clear_index = 0; clear_index < clear_count; ++clear_index) {
        uint8_t reconstructed = 0;

        for (size_t block = 0; block < kRadixBlockCount; ++block) {
            const size_t base =
                (clear_index * kRadixBlockCount + block) * stride_words;
            uint64_t dot = 0;

            for (size_t i = 0; i < kMaskDimension; ++i) {
                uint64_t mask = load_word(raw, base + i);
                logical_words->push_back(mask);
                if (key[i] != 0) {
                    dot += mask;
                }
            }

            uint64_t body = load_word(raw, base + kMaskDimension);
            logical_words->push_back(body);
            uint64_t phase = body - dot;
            uint64_t decoded =
                ((phase + (kDelta / 2)) >> kDeltaLog2) &
                ((uint64_t{1} << kMessageWidth) - 1);
            uint64_t encoded = decoded * kDelta;
            int64_t error = static_cast<int64_t>(phase - encoded);
            int64_t max_error =
                zero_noise ? 0 : ((int64_t{1} << kNoiseBoundLog2) - 1);
            if (error < -max_error || error > max_error) {
                return false;
            }

            if (expected_value_set && clear_index == 0) {
                uint64_t expected_block =
                    (expected_value >> (block * kMessageWidth)) &
                    ((uint64_t{1} << kMessageWidth) - 1);
                if (decoded != expected_block) {
                    return false;
                }
            }
            reconstructed |=
                static_cast<uint8_t>(decoded << (block * kMessageWidth));
        }

        if (expected_value_set && clear_index == 0 &&
            reconstructed != expected_value) {
            return false;
        }
        clears->push_back(reconstructed);
    }
    return true;
}

void write_le64(std::ofstream* file, uint64_t value)
{
    uint8_t bytes[8];
    for (size_t i = 0; i < sizeof(bytes); ++i) {
        bytes[i] = static_cast<uint8_t>(value >> (i * 8));
    }
    file->write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
}

bool write_dump(
    const char* path,
    const std::vector<uint8_t>& clears,
    const std::vector<uint64_t>& ciphertext_words)
{
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        fprintf(stderr, "Unable to open output file: %s\n", path);
        return false;
    }

    file.write("LWEHLS01", 8);
    write_le64(&file, 1);
    write_le64(&file, kMaskDimension);
    write_le64(&file, clears.size());
    write_le64(&file, kRadixBlockCount);
    write_le64(&file, kMessageWidth);
    write_le64(&file, kCarryWidth);
    write_le64(&file, kPaddingWidth);
    write_le64(&file, kDeltaLog2);
    write_le64(&file, ciphertext_words.size());
    for (uint8_t clear : clears) {
        write_le64(&file, clear);
    }
    for (uint64_t word : ciphertext_words) {
        write_le64(&file, word);
    }
    return file.good();
}

using Clock = std::chrono::steady_clock;

double elapsed_clock_ms(const Clock::time_point& start, const Clock::time_point& end)
{
    return std::chrono::duration<double, std::milli>(end - start).count();
}

const char* operation_name(uint64_t operation)
{
    return operation == lwe_remote::kOperationEchoU8
        ? "echo" : "adds-hpu-native-roundtrip";
}

bool write_plaintext_file(const char* path, const std::vector<uint8_t>& bytes)
{
    if (path == nullptr) {
        return true;
    }
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    if (!file) {
        fprintf(stderr, "Unable to write plaintext output: %s\n", path);
        return false;
    }
    return true;
}

bool write_remote_native_file(
    const char* path,
    const std::vector<uint64_t>& words)
{
    if (path == nullptr) {
        return true;
    }
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        fprintf(stderr, "Unable to open remote HPU-native output: %s\n", path);
        return false;
    }
    file.write(
        reinterpret_cast<const char*>(words.data()),
        static_cast<std::streamsize>(words.size() * sizeof(uint64_t)));
    if (!file) {
        fprintf(stderr, "Unable to write remote HPU-native output: %s\n", path);
        return false;
    }
    return true;
}

int write_slm_in_chunks(
    int io_fd,
    unsigned int mem_id,
    const void* input,
    size_t bytes,
    size_t chunk_bytes)
{
    const auto* data = static_cast<const uint8_t*>(input);
    for (size_t offset = 0; offset < bytes;) {
        const size_t chunk = std::min(chunk_bytes, bytes - offset);
        int retries = 0;
        while (true) {
            errno = 0;
            int ret = nvme_slm_write(
                io_fd, mem_id, static_cast<int>(offset),
                static_cast<int>(chunk), const_cast<uint8_t*>(data + offset));
            if (ret == 0) {
                break;
            }
            if (ret == -1 && errno == EINTR && retries < kSlmReadEintrMaxRetries) {
                ++retries;
                continue;
            }
            fprintf(stderr,
                    "nvme_slm_write failed: ret=%d mem_id=%u offset=%zu length=%zu errno=%d (%s)\n",
                    ret, mem_id, offset, chunk, errno, strerror(errno));
            return ret == 0 ? -EIO : ret;
        }
        offset += chunk;
    }
    return 0;
}

void build_decrypt_context(
    uint8_t* context,
    const std::vector<uint8_t>& key,
    uint32_t plaintext_bytes)
{
    memset(context, 0, kLbaSize);
    store_u32(context, 0, plaintext_bytes);
    store_u32(context, 4, kMaskDimension);
    store_u64(context, 8, kDelta);
    store_u32(context, 16, kMessageWidth);
    store_u32(context, 20, kRadixBlockCount);
    store_u32(context, 24, kOutputLayoutHpuNative);
    for (size_t i = 0; i < key.size(); ++i) {
        if (key[i] != 0) {
            context[kAxisBytes + i / 8] |= uint8_t{1} << (i % 8);
        }
    }
}

void build_single_operator_program(
    hlsacccompute_program* program,
    uint32_t operator_type_id,
    uint32_t program_id,
    uint32_t item_count)
{
    memset(program, 0, sizeof(*program));
    program->input_channum = 1;
    program->output_channum = 1;
    program->program_id = program_id;
    program->apply_operators_id_map[0] = static_cast<uint8_t>(operator_type_id);
    program->applyops[0].header.cid = 0;
    program->applyops[0].header.opc = APPLY_OPS;
    program->applyops[0].header.ops_num = 1;
    program->applyops[2].apply_ops_payload2.connections_num = 1;
    program->applyops[2].apply_ops_payload2.connections[0].from = 0x00;
    program->applyops[2].apply_ops_payload2.connections[0].to = 0xf0;
    program->pauseops[0].header.cid = 0;
    program->pauseops[0].header.opc = SUSPEND_OPS;
    program->pauseops[0].header.ops_num = 1;
    program->pauseops[1].generic_ops_payload.op_lists[0] = 0;
    program->freeops[0].header.cid = 0;
    program->freeops[0].header.opc = FORCE_FREE_OPS;
    program->freeops[0].header.ops_num = 1;
    program->freeops[1].generic_ops_payload.op_lists[0] = 0;
    program->input_channel_destination[0] = 0;
    program->apply_ops_size = 3;
    program->apply_operators_num = 1;
    // Small decrypt batches still pay fixed controller/DMA setup latency.
    // Match the independently validated lwe-decrypt application instead of
    // shrinking a five-item job to the old 5 ms response window.
    const uint64_t estimated_us =
        std::max<uint64_t>(1000, static_cast<uint64_t>(item_count) * 100);
    const uint64_t maximum_us =
        std::max<uint64_t>(30000, static_cast<uint64_t>(item_count) * 2000);
    program->esti_executed_time = estimated_us > UINT32_MAX
        ? UINT32_MAX : static_cast<uint32_t>(estimated_us);
    program->max_responded_time = maximum_us > UINT32_MAX
        ? UINT32_MAX : static_cast<uint32_t>(maximum_us);
}

int transfer_ssd_blocks(
    int io_fd,
    uint32_t nsid,
    uint64_t first_lba,
    void* buffer,
    size_t bytes,
    bool write)
{
    auto* data = static_cast<uint8_t*>(buffer);
    const size_t total_lbas = bytes / kLbaSize;
    for (size_t lba_offset = 0; lba_offset < total_lbas;) {
        const size_t lbas = std::min<size_t>(
            kMaxCopyLbasPerRange, total_lbas - lba_offset);
        struct nvme_io_args args = {};
        args.args_size = sizeof(args);
        args.fd = io_fd;
        args.nsid = nsid;
        args.slba = first_lba + lba_offset;
        args.nlb = static_cast<uint16_t>(lbas - 1);
        args.data = data + lba_offset * kLbaSize;
        args.data_len = lbas * kLbaSize;
        const int ret = write ? nvme_write(&args) : nvme_read(&args);
        if (ret != 0) {
            fprintf(stderr, "nvme_%s failed: ret=%d nsid=%u lba=%llu lbas=%zu\n",
                    write ? "write" : "read", ret, nsid,
                    static_cast<unsigned long long>(args.slba), lbas);
            return ret;
        }
        lba_offset += lbas;
    }
    return 0;
}

bool patch_selected_u8(
    std::vector<uint8_t>* image,
    size_t record_bytes,
    size_t field_offset,
    const std::vector<uint32_t>& selected_indices,
    const std::vector<uint8_t>& values,
    std::string* error)
{
    if (selected_indices.size() != values.size()) {
        *error = "selected index/value counts differ";
        return false;
    }
    if (field_offset >= record_bytes || record_bytes == 0) {
        *error = "projected u8 field is outside the record";
        return false;
    }
    for (size_t i = 0; i < values.size(); ++i) {
        const uint64_t offset =
            static_cast<uint64_t>(selected_indices[i]) * record_bytes + field_offset;
        if (offset >= image->size()) {
            *error = "selected record is outside the SSD image";
            return false;
        }
        (*image)[static_cast<size_t>(offset)] = values[i];
    }
    return true;
}

bool decode_hpu_native_u8(
    const std::vector<uint64_t>& native_words,
    size_t clear_count,
    const std::vector<uint8_t>& key,
    std::vector<uint8_t>* clears)
{
    const size_t expected_words = clear_count * kRadixBlockCount *
        kHpuPcCount * kHpuPcSlotWords;
    if (native_words.size() != expected_words) {
        return false;
    }
    const auto* raw = reinterpret_cast<const uint8_t*>(native_words.data());
    clears->clear();
    clears->reserve(clear_count);
    for (size_t clear_index = 0; clear_index < clear_count; ++clear_index) {
        uint8_t reconstructed = 0;
        for (size_t block = 0; block < kRadixBlockCount; ++block) {
            const size_t lwe_index = clear_index * kRadixBlockCount + block;
            uint64_t dot = 0;
            for (size_t natural_index = 0; natural_index < kMaskDimension;
                 ++natural_index) {
                const size_t hpu_index = reverse_psi64_mask_index(natural_index);
                const size_t group = hpu_index / kHpuPcGroupWords;
                const size_t lane = hpu_index % kHpuPcGroupWords;
                const size_t pc = group % kHpuPcCount;
                const size_t pc_offset =
                    (group / kHpuPcCount) * kHpuPcGroupWords + lane;
                if (key[natural_index] != 0) {
                    dot += load_word(
                        raw, hpu_native_word_index(lwe_index, pc, pc_offset));
                }
            }
            const uint64_t body = load_word(
                raw, hpu_native_word_index(lwe_index, 0, kHpuPcDataWords));
            const uint64_t phase = body - dot;
            const uint64_t decoded =
                ((phase + kDelta / 2) >> kDeltaLog2) &
                ((uint64_t{1} << kMessageWidth) - 1);
            reconstructed |= static_cast<uint8_t>(
                decoded << (block * kMessageWidth));
        }
        clears->push_back(reconstructed);
    }
    return true;
}

bool run_self_test()
{
    std::vector<uint8_t> image(3 * 64, 0xa5);
    std::vector<uint8_t> original = image;
    std::vector<uint32_t> indices{0, 2};
    std::vector<uint8_t> values{11, 255};
    std::string error;
    if (!patch_selected_u8(&image, 64, 7, indices, values, &error) ||
        image[7] != 11 || image[2 * 64 + 7] != 255) {
        fprintf(stderr, "patch self-test failed: %s\n", error.c_str());
        return false;
    }
    for (size_t i = 0; i < image.size(); ++i) {
        if (i != 7 && i != 2 * 64 + 7 && image[i] != original[i]) {
            fprintf(stderr, "patch self-test changed byte %zu unexpectedly\n", i);
            return false;
        }
    }
    printf("selective full-pipeline self-test passed\n");
    return true;
}

struct DecryptTimings {
    double slm_create_ms = 0.0;
    double slm_zero_ms = 0.0;
    double host_to_slm_ms = 0.0;
    double slm_write_verify_ms = 0.0;
    double program_setup_ms = 0.0;
    double fpga_execute_ms = 0.0;
    double slm_to_host_ms = 0.0;
    double cleanup_ms = 0.0;
};

bool run_decrypt_to_host(
    const Options& options,
    const std::vector<uint8_t>& key,
    const std::vector<uint64_t>& native_words,
    std::vector<uint8_t>* plaintext,
    unsigned int* result_bytes_out,
    unsigned int* rsid_out,
    DecryptTimings* timings)
{
    const size_t native_bytes = native_words.size() * sizeof(uint64_t);
    const size_t expected_native_bytes =
        static_cast<size_t>(options.plaintext_bytes) * kHpuNativeOutputBytes;
    const size_t output_slm_bytes = round_up_to_lba(
        static_cast<size_t>(options.plaintext_bytes) + kOutputDonePacketBytes);
    if (native_bytes != expected_native_bytes || native_bytes > INT_MAX ||
        output_slm_bytes > INT_MAX) {
        fprintf(stderr, "Remote HPU-native response size mismatch: got=%zu expected=%zu\n",
                native_bytes, expected_native_bytes);
        return false;
    }

    void* input_buffer = nullptr;
    void* context_page = nullptr;
    void* output_buffer = nullptr;
    void* verify_buffer = nullptr;
    if (posix_memalign(&input_buffer, kLbaSize, native_bytes) != 0 ||
        posix_memalign(&context_page, kLbaSize, kLbaSize) != 0 ||
        posix_memalign(&output_buffer, kLbaSize, output_slm_bytes) != 0 ||
        (options.verify_decrypt_slm_write &&
         posix_memalign(&verify_buffer, kLbaSize, native_bytes) != 0)) {
        fprintf(stderr, "Unable to allocate aligned decrypt buffers\n");
        free(input_buffer); free(context_page); free(output_buffer); free(verify_buffer);
        return false;
    }
    memcpy(input_buffer, native_words.data(), native_bytes);
    memset(output_buffer, 0, output_slm_bytes);
    build_decrypt_context(static_cast<uint8_t*>(context_page), key,
                          options.plaintext_bytes);

    int admin_fd = nvme_open(options.admin_device);
    int io_fd = nvme_open(options.io_device);
    if (admin_fd < 0 || io_fd < 0) {
        fprintf(stderr, "Unable to open NVMe devices for decrypt stage\n");
        if (admin_fd >= 0) close(admin_fd);
        if (io_fd >= 0) close(io_fd);
        free(input_buffer); free(context_page); free(output_buffer); free(verify_buffer);
        return false;
    }

    unsigned int input_mem_id = 0, output_mem_id = 0, rsid = 0, result_bytes = 0;
    bool input_created = false, output_created = false, range_created = false;
    bool program_loaded = false, program_activated = false, success = false;
    union memory_range_set_decriptor ranges[2] = {};
    struct hlsacccompute_program program = {};
    std::vector<SlmReadSample> samples;
    auto stage_start = Clock::now();

    do {
        fprintf(stderr, "[selective_full] creating decrypt input/output SLM\n");
        int ret = nvme_create_slm_ns(admin_fd, &input_mem_id, native_bytes);
        if (ret != 0) {
            fprintf(stderr, "nvme_create_slm_ns(decrypt input) failed: %d\n", ret);
            break;
        }
        input_created = true;
        ret = nvme_create_slm_ns(admin_fd, &output_mem_id, output_slm_bytes);
        if (ret != 0) {
            fprintf(stderr, "nvme_create_slm_ns(decrypt output) failed: %d\n", ret);
            break;
        }
        output_created = true;
        timings->slm_create_ms = elapsed_clock_ms(stage_start, Clock::now());

        stage_start = Clock::now();
        ret = nvme_slm_fill(io_fd, output_mem_id, 0, output_slm_bytes);
        if (ret != 0) {
            fprintf(stderr, "nvme_slm_fill(decrypt output) failed: %d\n", ret);
            break;
        }
        timings->slm_zero_ms = elapsed_clock_ms(stage_start, Clock::now());

        stage_start = Clock::now();
        fprintf(stderr, "[selective_full] writing remote result to decrypt input SLM\n");
        ret = write_slm_in_chunks(io_fd, input_mem_id, input_buffer,
                                  native_bytes, options.slm_write_chunk_bytes);
        if (ret != 0) break;
        timings->host_to_slm_ms = elapsed_clock_ms(stage_start, Clock::now());

        if (options.verify_decrypt_slm_write) {
            size_t failed_offset = 0, failed_length = 0;
            int failed_errno = 0;
            stage_start = Clock::now();
            ret = read_slm_in_chunks(io_fd, input_mem_id, 0, native_bytes,
                                     options.slm_read_chunk_bytes, 1, verify_buffer,
                                     &failed_offset, &failed_length, &failed_errno,
                                     false, &samples);
            if (ret != 0 || memcmp(input_buffer, verify_buffer, native_bytes) != 0) {
                fprintf(stderr, "Decrypt input SLM verification failed\n");
                break;
            }
            timings->slm_write_verify_ms = elapsed_clock_ms(stage_start, Clock::now());
            fprintf(stderr,
                    "[selective_full] decrypt input SLM verification passed bytes=%zu\n",
                    native_bytes);
        }

        stage_start = Clock::now();
        ranges[0].payload.mnsid = input_mem_id;
        ranges[0].payload.length = native_bytes;
        ranges[0].payload.flag = memory_range_descriptor::mdes_flag::MEM_RANGE_DEVICE_MEM;
        ranges[1].payload.mnsid = output_mem_id;
        ranges[1].payload.length = output_slm_bytes;
        ranges[1].payload.flag = memory_range_descriptor::mdes_flag::MEM_RANGE_DEVICE_MEM;
        ret = nvme_create_memory_range_set(admin_fd, kComputeNsid, &rsid, 2, ranges);
        if (ret != 0) {
            fprintf(stderr, "nvme_create_memory_range_set(decrypt) failed: %d\n", ret);
            break;
        }
        range_created = true;
        build_single_operator_program(&program, options.decrypt_operator_type_id,
                                      options.decrypt_program_id, options.plaintext_bytes);
        ret = nvme_unload_hlsacc_program(admin_fd, options.decrypt_program_id, kComputeNsid);
        if (ret != 0) {
            fprintf(stderr, "nvme_unload_hlsacc_program(decrypt pre-load) failed: %d\n", ret);
            break;
        }
        ret = nvme_load_hlsacc_program(admin_fd, sizeof(program),
                                       options.decrypt_program_id, kComputeNsid, &program);
        if (ret != 0) {
            fprintf(stderr, "nvme_load_hlsacc_program(decrypt) failed: %d\n", ret);
            break;
        }
        program_loaded = true;
        ret = nvme_activate_program(admin_fd, options.decrypt_program_id, kComputeNsid);
        if (ret != 0) {
            fprintf(stderr, "nvme_activate_program(decrypt) failed: %d\n", ret);
            break;
        }
        program_activated = true;
        timings->program_setup_ms = elapsed_clock_ms(stage_start, Clock::now());

        stage_start = Clock::now();
        fprintf(stderr,
                "[selective_full] executing lwe_decrypt on FPGA response_budget_us=%llu\n",
                static_cast<unsigned long long>(std::max<uint64_t>(
                    30000, static_cast<uint64_t>(options.plaintext_bytes) * 2000)));
        errno = 0;
        ret = nvme_execute_hlsacc_program(
            io_fd, kComputeNsid, rsid, options.decrypt_program_id,
            reinterpret_cast<struct AccContext*>(context_page), 1, 0, 0,
            &result_bytes);
        const int execute_errno = errno;
        timings->fpga_execute_ms = elapsed_clock_ms(stage_start, Clock::now());
        if (ret != 0 || result_bytes < options.plaintext_bytes ||
            result_bytes > output_slm_bytes) {
            fprintf(stderr,
                    "Invalid decrypt result: ret=%d errno=%d (%s) bytes=%u "
                    "expected_at_least=%u waited_ms=%.3f response_budget_us=%llu\n",
                    ret, execute_errno, strerror(execute_errno), result_bytes,
                    options.plaintext_bytes, timings->fpga_execute_ms,
                    static_cast<unsigned long long>(std::max<uint64_t>(
                        30000, static_cast<uint64_t>(options.plaintext_bytes) * 2000)));
            break;
        }

        stage_start = Clock::now();
        size_t failed_offset = 0, failed_length = 0;
        int failed_errno = 0;
        ret = read_slm_in_chunks(io_fd, output_mem_id, 0, output_slm_bytes,
                                 options.slm_read_chunk_bytes, 1, output_buffer,
                                 &failed_offset, &failed_length, &failed_errno,
                                 false, &samples);
        if (ret != 0) {
            fprintf(stderr, "Decrypt output SLM read failed: offset=%zu length=%zu errno=%d\n",
                    failed_offset, failed_length, failed_errno);
            break;
        }
        timings->slm_to_host_ms = elapsed_clock_ms(stage_start, Clock::now());
        const auto* output = static_cast<const uint8_t*>(output_buffer);
        plaintext->assign(output, output + options.plaintext_bytes);
        success = true;
    } while (false);

    stage_start = Clock::now();
    if (program_activated)
        nvme_deactivate_program(admin_fd, options.decrypt_program_id, kComputeNsid);
    if (program_loaded)
        nvme_unload_hlsacc_program(admin_fd, options.decrypt_program_id, kComputeNsid);
    if (range_created) nvme_delete_memory_range_set(admin_fd, kComputeNsid, rsid);
    if (output_created) nvme_delete_slm_ns(admin_fd, output_mem_id);
    if (input_created) nvme_delete_slm_ns(admin_fd, input_mem_id);
    close(io_fd); close(admin_fd);
    timings->cleanup_ms = elapsed_clock_ms(stage_start, Clock::now());
    free(input_buffer); free(context_page); free(output_buffer); free(verify_buffer);
    if (success) {
        *result_bytes_out = result_bytes;
        *rsid_out = rsid;
    }
    return success;
}

double elapsed_ms(const timeval& start, const timeval& end)
{
    return (end.tv_sec - start.tv_sec) * 1000.0 +
           (end.tv_usec - start.tv_usec) / 1000.0;
}

void log_stage(const char* stage)
{
    fprintf(stderr, "[selective_lwe] %s\n", stage);
    fflush(stderr);
}

void print_filter_plan(const Options &options)
{
    if (!options.sql_mode) {
        printf("filter_protocol=legacy-v1 record_bytes=512 field=quantity:u8@4 "
               "predicate=%s threshold=%u projection=quantity\n",
               options.predicate == kPredicateGt ? "gt" : "eq", options.threshold);
        return;
    }
    printf("filter_protocol=sql-metadata-v2 magic=0x%08x version=%u\n",
           SELECTIVE_FILTER_V2_MAGIC, SELECTIVE_FILTER_V2_VERSION);
    printf("record_bytes=%u projection_offset=%u projection_type=%s\n",
           options.record_bytes, options.sql_program.projection_offset,
           sql_filter_type_name(options.sql_program.projection_type));
    printf("schema=%s\nquery=%s\n", options.schema.c_str(), options.query.c_str());
    for (size_t i = 0; i < options.sql_program.predicates.size(); ++i) {
        const SqlPredicate &p = options.sql_program.predicates[i];
        printf("predicate[%zu]=offset:%u type:%s comparison:%u literal:0x%016llx\n",
               i, p.offset, sql_filter_type_name(p.type), p.comparison,
               static_cast<unsigned long long>(p.literal));
    }
    printf("rpn_tokens=");
    for (size_t i = 0; i < options.sql_program.tokens.size(); ++i)
        printf("%s%02x", i ? "," : "", options.sql_program.tokens[i]);
    printf("\n");
}

}  // namespace

int main(int argc, char** argv)
{
    setvbuf(stdout, nullptr, _IOLBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    Options options;
    if (!parse_options(argc, argv, &options)) {
        print_usage(argv[0]);
        return 1;
    }
    if (options.self_test) {
        return run_self_test() ? 0 : 1;
    }
    if (options.explain_filter) {
        print_filter_plan(options);
        return 0;
    }
    struct timeval application_start = {};
    gettimeofday(&application_start, nullptr);
    if (!options.seed_set) {
        options.seed = random_u64();
    }
    if (!options.nonce_set) {
        options.nonce = random_u64();
    }

    std::vector<uint8_t> expected_quantities;
    std::vector<uint8_t> expected_records;
    std::vector<uint32_t> selected_indices;
    std::vector<uint8_t> reference_records;
    if (!read_reference_selection(options, &expected_quantities, &expected_records,
                                  &selected_indices, &reference_records)) {
        return 1;
    }
    options.plaintext_bytes = static_cast<uint32_t>(expected_quantities.size());

    std::vector<uint8_t> key;
    if (!read_binary_key(options.key_path, &key)) {
        return 1;
    }

    const size_t input_bytes = input_buffer_bytes(options);
    const size_t compute_input_bytes = stream_input_bytes(options);
    const size_t compute_input_range_bytes = input_range_bytes(options);
    const size_t cipher_physical_bytes = physical_output_bytes(options);
    const size_t cipher_bytes_per_selected =
        options.output_layout == kOutputLayoutHpuNative
            ? kHpuNativeOutputBytes
            : kPhysicalOutputBytes;
    const size_t output_bytes = output_buffer_bytes(options);
    const size_t output_read_bytes =
        round_up_to_lba(cipher_physical_bytes + kOutputDonePacketBytes);
    const size_t slm_read_request_count =
        (output_read_bytes + options.slm_read_chunk_bytes - 1) /
        options.slm_read_chunk_bytes;
    const size_t copy_range_count = source_range_count(options);
    if (copy_range_count * sizeof(union nvme_source_range) > kLbaSize) {
        fprintf(
            stderr,
            "Too many SSD source ranges for one source range page: %zu\n",
            copy_range_count);
        return 1;
    }
    if (copy_range_count > UINT8_MAX) {
        fprintf(
            stderr,
            "Too many SSD source ranges for nvme_slm_copy: %zu\n",
            copy_range_count);
        return 1;
    }

    void* output_buffer = nullptr;
    void* context_pages = nullptr;
    void* source_range_page = nullptr;
    if (posix_memalign(&output_buffer, kLbaSize, output_bytes) != 0 ||
        posix_memalign(&context_pages, kLbaSize, 2 * kLbaSize) != 0 ||
        posix_memalign(&source_range_page, kLbaSize, kLbaSize) != 0) {
        fprintf(stderr, "Unable to allocate aligned host buffers\n");
        free(output_buffer);
        free(context_pages);
        free(source_range_page);
        return 1;
    }
    memset(output_buffer, 0, output_bytes);
    memset(source_range_page, 0, kLbaSize);
    build_contexts(static_cast<uint8_t*>(context_pages), key, options);

    int admin_fd = nvme_open(options.admin_device);
    int io_fd = nvme_open(options.io_device);
    if (admin_fd < 0 || io_fd < 0) {
        fprintf(
            stderr,
            "Unable to open NVMe devices admin=%s io=%s\n",
            options.admin_device,
            options.io_device);
        free(output_buffer);
        free(context_pages);
        free(source_range_page);
        return 1;
    }

    unsigned int input_mem_id = 0;
    unsigned int output_mem_id = 0;
    unsigned int rsid = 0;
    bool input_created = false;
    bool output_created = false;
    bool range_created = false;
    bool program_loaded = false;
    bool program_activated = false;
    int status = 1;
    union memory_range_set_decriptor ranges[2];
    struct hlsacccompute_program program;
    struct timeval start = {};
    struct timeval end = {};
    struct timeval pipeline_start = {};
    struct timeval slm_create_start = {};
    struct timeval slm_create_end = {};
    struct timeval ssd_copy_start = {};
    struct timeval ssd_copy_end = {};
    struct timeval program_setup_start = {};
    struct timeval program_setup_end = {};
    struct timeval slm_read_start = {};
    struct timeval slm_read_end = {};
    struct timeval verify_start = {};
    struct timeval verify_end = {};
    struct timeval dump_start = {};
    struct timeval dump_end = {};
    struct timeval cleanup_start = {};
    struct timeval cleanup_end = {};
    unsigned int result_bytes = 0;
    size_t fpga_selected_count = 0;
    size_t failed_slm_offset = 0;
    size_t failed_slm_length = 0;
    int failed_slm_errno = 0;
    union nvme_source_range* source_range =
        static_cast<union nvme_source_range*>(source_range_page);
    std::vector<uint8_t> clears;
    std::vector<uint64_t> logical_words;
    std::vector<uint64_t> hpu_native_words;
    std::vector<SlmReadSample> slm_read_samples;
    const uint8_t* raw = static_cast<const uint8_t*>(output_buffer);
    const char* layout = nullptr;

    memset(ranges, 0, sizeof(ranges));
    memset(&program, 0, sizeof(program));

    fprintf(
        stderr,
        "[selective_lwe] sizing records=%u selected_reference=%u input_bytes=%zu "
        "compute_input_range_bytes=%zu "
        "cipher_physical_bytes=%zu output_slm_bytes=%zu output_read_bytes=%zu "
        "compute_output_range_bytes=%zu "
        "read_chunk_bytes=%zu "
        "read_requests=%zu read_queue_depth=%u\n",
        options.record_count,
        options.plaintext_bytes,
        input_bytes,
        compute_input_range_bytes,
        cipher_physical_bytes,
        output_bytes,
        output_read_bytes,
        output_read_bytes,
        options.slm_read_chunk_bytes,
        slm_read_request_count,
        options.slm_read_queue_depth);

    int ret = 0;
    gettimeofday(&pipeline_start, nullptr);
    gettimeofday(&slm_create_start, nullptr);
    log_stage("creating input SLM");
    ret = nvme_create_slm_ns(admin_fd, &input_mem_id, input_bytes);
    if (ret != 0) {
        fprintf(stderr, "nvme_create_slm_ns(input) failed: %d\n", ret);
        goto cleanup;
    }
    input_created = true;

    log_stage("creating output SLM");
    ret = nvme_create_slm_ns(admin_fd, &output_mem_id, output_bytes);
    if (ret != 0) {
        fprintf(stderr, "nvme_create_slm_ns(output) failed: %d\n", ret);
        goto cleanup;
    }
    output_created = true;
    gettimeofday(&slm_create_end, nullptr);

    for (size_t i = 0; i < copy_range_count; ++i) {
        uint32_t remaining_lbas =
            options.input_lbas - static_cast<uint32_t>(i * kMaxCopyLbasPerRange);
        uint32_t lbas_this_range =
            remaining_lbas > kMaxCopyLbasPerRange ? kMaxCopyLbasPerRange : remaining_lbas;

        source_range[i].scc.snsid = options.storage_nsid;
        source_range[i].scc.slba =
            options.ssd_lba + static_cast<uint64_t>(i * kMaxCopyLbasPerRange);
        source_range[i].scc.nlb = lbas_this_range - 1;
    }

    log_stage("copying SSD blocks to input SLM");
    gettimeofday(&ssd_copy_start, nullptr);
    ret = nvme_slm_copy(
        io_fd,
        source_range_page,
        sizeof(*source_range) * copy_range_count,
        0,
        0x3,
        static_cast<unsigned char>(copy_range_count),
        input_mem_id);
    if (ret != 0) {
        fprintf(
            stderr,
            "nvme_slm_copy SSD nsid=%u lba=%llu lbas=%u to input SLM failed: %d\n",
            options.storage_nsid,
            static_cast<unsigned long long>(options.ssd_lba),
            options.input_lbas,
            ret);
        goto cleanup;
    }
    gettimeofday(&ssd_copy_end, nullptr);

    gettimeofday(&program_setup_start, nullptr);
    ranges[0].payload.mnsid = input_mem_id;
    ranges[0].payload.length = compute_input_range_bytes;
    ranges[0].payload.starting_byte = 0;
    ranges[0].payload.flag =
        memory_range_descriptor::mdes_flag::MEM_RANGE_DEVICE_MEM;
    ranges[1].payload.mnsid = output_mem_id;
    // RX completes a multi-BD submission only at its final descriptor. The
    // allocation holds the worst-case result, but posting that entire range
    // leaves unused descriptors after a selective result's finish marker.
    // Bound the receive range to the reference payload plus its finish page
    // so the marker lands in the final descriptor of the final submission.
    ranges[1].payload.length = output_read_bytes;
    ranges[1].payload.starting_byte = 0;
    ranges[1].payload.flag =
        memory_range_descriptor::mdes_flag::MEM_RANGE_DEVICE_MEM;

    log_stage("creating memory range set");
    ret = nvme_create_memory_range_set(
        admin_fd,
        kComputeNsid,
        &rsid,
        2,
        ranges);
    if (ret != 0) {
        fprintf(stderr, "nvme_create_memory_range_set failed: %d\n", ret);
        goto cleanup;
    }
    range_created = true;

    build_program(&program, options);
    log_stage("clearing stale FPGA program slot");
    ret = nvme_unload_hlsacc_program(
        admin_fd,
        options.program_id,
        kComputeNsid);
    if (ret != 0) {
        fprintf(
            stderr,
            "nvme_unload_hlsacc_program(pre-load) failed: %d\n",
            ret);
        goto cleanup;
    }
    log_stage("loading FPGA program");
    ret = nvme_load_hlsacc_program(
        admin_fd,
        sizeof(program),
        options.program_id,
        kComputeNsid,
        &program);
    if (ret != 0) {
        fprintf(stderr, "nvme_load_hlsacc_program failed: %d\n", ret);
        goto cleanup;
    }
    program_loaded = true;

    log_stage("activating FPGA program");
    ret = nvme_activate_program(
        admin_fd,
        options.program_id,
        kComputeNsid);
    if (ret != 0) {
        fprintf(stderr, "nvme_activate_program failed: %d\n", ret);
        goto cleanup;
    }
    program_activated = true;
    gettimeofday(&program_setup_end, nullptr);

    gettimeofday(&start, nullptr);

    // The two 4KB pages initialize filter logical op 0 and LWE logical op 1.
    log_stage("executing selective_filter -> lwe_encrypt on FPGA");
    ret = nvme_execute_hlsacc_program(
        io_fd,
        kComputeNsid,
        rsid,
        options.program_id,
        reinterpret_cast<struct AccContext*>(context_pages),
        2,
        0,
        0,
        &result_bytes);
    gettimeofday(&end, nullptr);
    if (ret != 0) {
        fprintf(stderr, "nvme_execute_hlsacc_program failed: %d\n", ret);
        goto cleanup;
    }
    fprintf(
        stderr,
        "[selective_lwe] FPGA execution returned result_bytes=%u expected_payload_bytes=%zu output_slm_bytes=%zu\n",
        result_bytes,
        cipher_physical_bytes,
        output_bytes);
    if (result_bytes % cipher_bytes_per_selected != 0) {
        fprintf(
            stderr,
            "FPGA result size is not an integral selected-record count: got=%u bytes_per_selected=%zu\n",
            result_bytes,
            cipher_bytes_per_selected);

        size_t diagnostic_read_bytes = round_up_to_lba(
            std::min<size_t>(
                output_bytes,
                static_cast<size_t>(result_bytes) + kOutputDonePacketBytes));
        std::vector<SlmReadSample> diagnostic_samples;
        int diagnostic_ret = read_slm_in_chunks(
            io_fd,
            output_mem_id,
            0,
            diagnostic_read_bytes,
            options.slm_read_chunk_bytes,
            1,
            output_buffer,
            &failed_slm_offset,
            &failed_slm_length,
            &failed_slm_errno,
            false,
            &diagnostic_samples);
        if (diagnostic_ret == 0) {
            bool raw_selected_record_prefix =
                result_bytes <= expected_records.size() &&
                memcmp(output_buffer, expected_records.data(), result_bytes) == 0;
            const uint8_t* diagnostic_raw = static_cast<const uint8_t*>(output_buffer);
            fprintf(
                stderr,
                "[selective_lwe] 异常输出诊断 read_bytes=%zu raw_selected_record_prefix=%s first_16=",
                diagnostic_read_bytes,
                raw_selected_record_prefix ? "yes" : "no");
            for (size_t i = 0; i < std::min<size_t>(16, result_bytes); ++i) {
                fprintf(stderr, "%02x", diagnostic_raw[i]);
            }
            fprintf(stderr, " last_16=");
            size_t tail_start = result_bytes > 16 ? result_bytes - 16 : 0;
            for (size_t i = tail_start; i < result_bytes; ++i) {
                fprintf(stderr, "%02x", diagnostic_raw[i]);
            }
            fprintf(stderr, "\n");
        } else {
            fprintf(
                stderr,
                "[selective_lwe] 异常输出回读失败 ret=%d offset=%zu length=%zu errno=%d (%s)\n",
                diagnostic_ret,
                failed_slm_offset,
                failed_slm_length,
                failed_slm_errno,
                strerror(failed_slm_errno));
        }
        ret = 1;
        goto cleanup;
    }
    fpga_selected_count = result_bytes / cipher_bytes_per_selected;
    if (fpga_selected_count != expected_quantities.size()) {
        fprintf(
            stderr,
            "FPGA selected count does not match host predicate reference: got=%zu expected=%zu\n",
            fpga_selected_count,
            expected_quantities.size());
        ret = 1;
        goto cleanup;
    }
    log_stage("FPGA execution completed; reading output SLM");

    gettimeofday(&slm_read_start, nullptr);
    ret = read_slm_in_chunks(
        io_fd,
        output_mem_id,
        0,
        output_read_bytes,
        options.slm_read_chunk_bytes,
        options.slm_read_queue_depth,
        output_buffer,
        &failed_slm_offset,
        &failed_slm_length,
        &failed_slm_errno,
        !options.benchmark,
        &slm_read_samples);
    if (options.slm_read_trace_path != nullptr) {
        if (!append_slm_read_trace(
                options.slm_read_trace_path,
                options,
                slm_read_request_count,
                slm_read_samples)) {
            fprintf(
                stderr,
                "warning: failed to write SLM request trace; encryption result is unchanged\n");
        }
        print_slm_read_request_stats(slm_read_samples);
        printf("slm_read_trace=%s\n", options.slm_read_trace_path);
    }
    if (ret != 0) {
        fprintf(
            stderr,
            "nvme_slm_read failed: ret=%d mem_id=%u offset=%zu length=%zu errno=%d (%s)\n",
            ret,
            output_mem_id,
            failed_slm_offset,
            failed_slm_length,
            failed_slm_errno,
            strerror(failed_slm_errno));
        goto cleanup;
    }
    gettimeofday(&slm_read_end, nullptr);
    log_stage("output read completed; verifying ciphertext");

    gettimeofday(&verify_start, nullptr);
    if (options.output_layout == kOutputLayoutHpuNative) {
        if (!extract_hpu_native_and_verify(
                raw,
                output_read_bytes,
                options.plaintext_bytes,
                key,
                false,
                0,
                options.zero_noise,
                &clears,
                &logical_words)) {
            fprintf(stderr, "FPGA HPU-native ciphertext verification failed\n");
            goto cleanup;
        }
        layout = "hpu-native-psi64-v80";
    // The current OperatorController forces TKEEP to all ones, so each CPU
    // ciphertext body occupies a full final 64-byte beat. Keep support for
    // a future controller that preserves the HLS partial TKEEP as well.
    } else if (extract_and_verify(
            raw,
            output_read_bytes,
            options.plaintext_bytes,
            kPhysicalWordsPerCiphertext,
            key,
            false,
            0,
            options.zero_noise,
            &clears,
            &logical_words)) {
        layout = "64-byte-padded";
    } else if (extract_and_verify(
                   raw,
                   output_read_bytes,
                   options.plaintext_bytes,
                   kLogicalWordsPerCiphertext,
                   key,
                   false,
                   0,
                   options.zero_noise,
                   &clears,
                   &logical_words)) {
        layout = "compact";
    } else {
        fprintf(
            stderr,
            "FPGA CPU-LWE ciphertext verification failed for both stream layouts\n");
        goto cleanup;
    }
    if (clears != expected_quantities) {
        fprintf(stderr, "Decrypted selected quantities do not match the host reference predicate\n");
        goto cleanup;
    }
    gettimeofday(&verify_end, nullptr);

    gettimeofday(&dump_start, nullptr);
    if (!options.skip_dump) {
        if (!write_dump(options.output_path, clears, logical_words)) {
            goto cleanup;
        }
    }
    gettimeofday(&dump_end, nullptr);

    hpu_native_words.assign(
        reinterpret_cast<const uint64_t*>(raw),
        reinterpret_cast<const uint64_t*>(raw) +
            cipher_physical_bytes / sizeof(uint64_t));

    // The decrypt program reuses the same physical operator fabric. Release
    // every encryption-stage resource before the network wait and decrypt run.
    log_stage("releasing filter/encrypt CSD resources before remote HPU RPC");
    if (program_activated) {
        ret = nvme_deactivate_program(admin_fd, options.program_id, kComputeNsid);
        if (ret != 0) goto cleanup;
        program_activated = false;
    }
    if (program_loaded) {
        ret = nvme_unload_hlsacc_program(admin_fd, options.program_id, kComputeNsid);
        if (ret != 0) goto cleanup;
        program_loaded = false;
    }
    if (range_created) {
        nvme_delete_memory_range_set(admin_fd, kComputeNsid, rsid);
        range_created = false;
    }
    if (output_created) {
        nvme_delete_slm_ns(admin_fd, output_mem_id);
        output_created = false;
    }
    if (input_created) {
        nvme_delete_slm_ns(admin_fd, input_mem_id);
        input_created = false;
    }
    close(io_fd); close(admin_fd);
    io_fd = -1; admin_fd = -1;
    free(output_buffer); free(context_pages); free(source_range_page);
    output_buffer = nullptr; context_pages = nullptr; source_range_page = nullptr;

    {
        const Clock::time_point remote_start = Clock::now();
        lwe_remote::BatchMetadata metadata = {
            kMaskDimension,
            options.plaintext_bytes,
            kRadixBlockCount,
            kMessageWidth,
            kCarryWidth,
            kPaddingWidth,
            kDeltaLog2,
            hpu_native_words.size(),
        };
        lwe_remote::RpcResult rpc_result;
        std::string rpc_error;
        const uint64_t request_id = random_u64();
        fprintf(stderr, "[selective_full] sending %u selected ciphertexts to remote HPU\n",
                options.plaintext_bytes);
        if (!lwe_remote::compute_u8(
                options.remote_host, options.remote_port, request_id,
                options.remote_operation, options.scalar, metadata,
                hpu_native_words, options.connect_timeout_ms,
                options.io_timeout_secs, options.max_response_bytes,
                &rpc_result, &rpc_error)) {
            fprintf(stderr, "remote HPU RPC failed: %s\n", rpc_error.c_str());
            goto cleanup;
        }
        const double remote_round_trip_wall_ms =
            elapsed_clock_ms(remote_start, Clock::now());

        std::vector<uint8_t> expected_remote = expected_quantities;
        if (options.remote_operation != lwe_remote::kOperationEchoU8) {
            for (uint8_t& value : expected_remote) {
                value = static_cast<uint8_t>(value + options.scalar);
            }
        }
        std::vector<uint8_t> host_decoded_remote;
        if (!decode_hpu_native_u8(rpc_result.ciphertext_words,
                                  options.plaintext_bytes, key,
                                  &host_decoded_remote) ||
            host_decoded_remote != expected_remote) {
            fprintf(stderr,
                    "Remote HPU-native response failed Host reference decryption\n");
            goto cleanup;
        }
        fprintf(stderr,
                "[selective_full] remote HPU-native response Host verification passed\n");
        if (!write_remote_native_file(options.remote_native_output_path,
                                      rpc_result.ciphertext_words) ||
            !write_plaintext_file(options.remote_expected_output_path,
                                  expected_remote)) {
            goto cleanup;
        }
        if (options.remote_native_output_path != nullptr) {
            fprintf(stderr,
                    "[selective_full] saved decrypt diagnostic fixture native=%s "
                    "expected=%s\n",
                    options.remote_native_output_path,
                    options.remote_expected_output_path != nullptr
                        ? options.remote_expected_output_path : "(disabled)");
        }
        std::vector<uint8_t> decrypted;
        unsigned int decrypt_result_bytes = 0;
        unsigned int decrypt_rsid = 0;
        DecryptTimings decrypt_timings;
        if (!run_decrypt_to_host(options, key, rpc_result.ciphertext_words,
                                 &decrypted, &decrypt_result_bytes,
                                 &decrypt_rsid, &decrypt_timings)) {
            fprintf(stderr, "FPGA decrypt stage failed\n");
            goto cleanup;
        }
        if (decrypted != expected_remote) {
            size_t mismatch = 0;
            while (mismatch < expected_remote.size() &&
                   mismatch < decrypted.size() &&
                   decrypted[mismatch] == expected_remote[mismatch]) {
                ++mismatch;
            }
            fprintf(stderr,
                    "End-to-end result mismatch at selected item %zu: got=%u expected=%u\n",
                    mismatch,
                    mismatch < decrypted.size() ? decrypted[mismatch] : 0,
                    mismatch < expected_remote.size() ? expected_remote[mismatch] : 0);
            goto cleanup;
        }
        if (!write_plaintext_file(options.plaintext_output_path, decrypted)) {
            goto cleanup;
        }

        void* source_image_buffer = nullptr;
        void* readback_buffer = nullptr;
        if (posix_memalign(&source_image_buffer, kLbaSize, input_bytes) != 0 ||
            posix_memalign(&readback_buffer, kLbaSize, input_bytes) != 0) {
            fprintf(stderr, "Unable to allocate aligned SSD update buffers\n");
            free(source_image_buffer); free(readback_buffer);
            goto cleanup;
        }
        int update_fd = nvme_open(options.io_device);
        if (update_fd < 0) {
            fprintf(stderr, "Unable to open NVMe I/O device for SSD update\n");
            free(source_image_buffer); free(readback_buffer);
            goto cleanup;
        }
        const Clock::time_point ssd_update_start = Clock::now();
        ret = transfer_ssd_blocks(update_fd, options.storage_nsid,
                                  options.ssd_lba, source_image_buffer,
                                  input_bytes, false);
        if (ret != 0) {
            close(update_fd); free(source_image_buffer); free(readback_buffer);
            goto cleanup;
        }
        if (memcmp(source_image_buffer, reference_records.data(),
                   reference_records.size()) != 0) {
            fprintf(stderr,
                    "Source SSD records differ from --reference; refusing to patch possibly wrong rows\n");
            close(update_fd); free(source_image_buffer); free(readback_buffer);
            goto cleanup;
        }
        std::vector<uint8_t> patched(
            static_cast<uint8_t*>(source_image_buffer),
            static_cast<uint8_t*>(source_image_buffer) + input_bytes);
        const size_t projection_offset = options.sql_mode
            ? options.sql_program.projection_offset : kQuantityOffset;
        std::string patch_error;
        if (!patch_selected_u8(&patched, options.record_bytes, projection_offset,
                               selected_indices, decrypted, &patch_error)) {
            fprintf(stderr, "Unable to patch selected records: %s\n", patch_error.c_str());
            close(update_fd); free(source_image_buffer); free(readback_buffer);
            goto cleanup;
        }
        memcpy(source_image_buffer, patched.data(), patched.size());
        fprintf(stderr,
                "[selective_full] writing patched record image to SSD nsid=%u lba=%llu mode=%s\n",
                options.output_storage_nsid,
                static_cast<unsigned long long>(options.output_ssd_lba),
                options.in_place ? "in-place" : "copy-on-write");
        ret = transfer_ssd_blocks(update_fd, options.output_storage_nsid,
                                  options.output_ssd_lba, source_image_buffer,
                                  input_bytes, true);
        if (ret == 0) {
            ret = transfer_ssd_blocks(update_fd, options.output_storage_nsid,
                                      options.output_ssd_lba, readback_buffer,
                                      input_bytes, false);
        }
        close(update_fd);
        if (ret != 0 || memcmp(source_image_buffer, readback_buffer, input_bytes) != 0) {
            fprintf(stderr, "Destination SSD readback verification failed\n");
            free(source_image_buffer); free(readback_buffer);
            goto cleanup;
        }
        const double ssd_update_ms =
            elapsed_clock_ms(ssd_update_start, Clock::now());
        free(source_image_buffer); free(readback_buffer);

        printf("selective SSD-to-remote-HPU-to-SSD update pipeline passed\n");
        printf("total_count=%u selected_count=%zu updated_count=%zu\n",
               options.record_count, fpga_selected_count, decrypted.size());
        printf("selected_indices=");
        for (size_t i = 0; i < selected_indices.size(); ++i)
            printf("%s%u", i ? "," : "", selected_indices[i]);
        printf("\ninput_values=");
        for (uint8_t value : expected_quantities) printf("%02x", value);
        printf("\nresult_values=");
        for (uint8_t value : decrypted) printf("%02x", value);
        printf("\n");
        printf("query=%s\n", options.sql_mode ? options.query.c_str() : "legacy-v1");
        printf("updated_field=u8@%zu operation=%s scalar=%u\n",
               projection_offset, operation_name(options.remote_operation), options.scalar);
        printf(
            "data_path=SSD(nsid=%u,lba=%llu,lbas=%u)->SLM->FPGA_filter->FPGA_LWE_encrypt->Host->TCP->remote_HPU->TCP->Host->SLM->FPGA_LWE_decrypt->Host_RMW->SSD(nsid=%u,lba=%llu,lbas=%u)\n",
            options.storage_nsid, static_cast<unsigned long long>(options.ssd_lba),
            options.input_lbas, options.output_storage_nsid,
            static_cast<unsigned long long>(options.output_ssd_lba), options.input_lbas);
        printf("ssd_update_mode=%s source_reference_checked=yes destination_readback_checked=yes\n",
               options.in_place ? "in-place" : "copy-on-write");
        printf("filter_encrypt_result_bytes=%u remote_result_bytes=%zu decrypt_result_bytes=%u\n",
               result_bytes, rpc_result.ciphertext_words.size() * sizeof(uint64_t),
               decrypt_result_bytes);
        printf("record_stream_bytes=%zu ssd_image_bytes=%zu\n",
               compute_input_bytes, input_bytes);
        printf("remote_server=%s:%u request_id=%llu rpc_round_trip_ms=%.3f wall_ms=%.3f\n",
               options.remote_host, options.remote_port,
               static_cast<unsigned long long>(rpc_result.request_id),
               rpc_result.round_trip_ms, remote_round_trip_wall_ms);
        printf(
            "remote_stage_ms hpu_prepare=%.3f hpu_enqueue=%.3f hpu_wait_sync=%.3f hpu_output_convert=%.3f server_total=%.3f\n",
            rpc_result.server_hpu_prepare_ms,
            rpc_result.server_hpu_enqueue_ms,
            rpc_result.server_hpu_wait_sync_ms,
            rpc_result.server_hpu_output_convert_ms,
            rpc_result.server_total_ms);
        printf("remote_hpu_ciphertext_compute=%s\n",
               options.remote_operation == lwe_remote::kOperationEchoU8
                   ? "skipped-echo" : "passed");
        printf("decrypt_rsid=%u fpga_decrypt_checked=yes ssd_update_ms=%.3f\n",
               decrypt_rsid, ssd_update_ms);
        if (options.benchmark) {
            printf(
                "decrypt_stage_ms slm_create=%.3f slm_zero=%.3f host_to_slm=%.3f slm_write_verify=%.3f program_setup=%.3f fpga_execute=%.3f slm_to_host=%.3f cleanup=%.3f\n",
                decrypt_timings.slm_create_ms, decrypt_timings.slm_zero_ms,
                decrypt_timings.host_to_slm_ms,
                decrypt_timings.slm_write_verify_ms,
                decrypt_timings.program_setup_ms,
                decrypt_timings.fpga_execute_ms,
                decrypt_timings.slm_to_host_ms,
                decrypt_timings.cleanup_ms);
        }
        if (!options.zero_noise)
            printf("warning=internal PRNG/noise is a prototype, not bit-exact tfhe-rs randomness\n");
        status = 0;
    }

cleanup:
    gettimeofday(&cleanup_start, nullptr);
    if (program_activated) {
        int cleanup_ret =
            nvme_deactivate_program(admin_fd, options.program_id, kComputeNsid);
        if (cleanup_ret != 0) {
            fprintf(
                stderr,
                "warning: nvme_deactivate_program cleanup failed: %d\n",
                cleanup_ret);
        }
    }
    if (program_loaded) {
        int cleanup_ret =
            nvme_unload_hlsacc_program(admin_fd, options.program_id, kComputeNsid);
        if (cleanup_ret != 0) {
            fprintf(
                stderr,
                "warning: nvme_unload_hlsacc_program cleanup failed: %d\n",
                cleanup_ret);
        }
    }
    if (range_created) {
        nvme_delete_memory_range_set(admin_fd, kComputeNsid, rsid);
    }
    if (output_created) {
        nvme_delete_slm_ns(admin_fd, output_mem_id);
    }
    if (input_created) {
        nvme_delete_slm_ns(admin_fd, input_mem_id);
    }
    close(io_fd);
    close(admin_fd);
    free(output_buffer);
    free(context_pages);
    free(source_range_page);
    gettimeofday(&cleanup_end, nullptr);
    if (status == 0 && options.benchmark) {
        const double slm_create_ms = elapsed_ms(slm_create_start, slm_create_end);
        const double ssd_copy_ms = elapsed_ms(ssd_copy_start, ssd_copy_end);
        const double program_setup_ms =
            elapsed_ms(program_setup_start, program_setup_end);
        const double fpga_execute_ms = elapsed_ms(start, end);
        const double slm_read_ms = elapsed_ms(slm_read_start, slm_read_end);
        const double host_verify_ms = elapsed_ms(verify_start, verify_end);
        const double dump_write_ms = elapsed_ms(dump_start, dump_end);
        const double cleanup_ms = elapsed_ms(cleanup_start, cleanup_end);
        const double transport_ready_ms =
            ssd_copy_ms + fpga_execute_ms + slm_read_ms;
        const double data_path_ms =
            transport_ready_ms + host_verify_ms;
        const double one_shot_transport_ready_ms =
            elapsed_ms(pipeline_start, slm_read_end);
        const double one_shot_pipeline_ms =
            elapsed_ms(pipeline_start, verify_end);
        const double process_ms = elapsed_ms(application_start, cleanup_end);
        const double count = static_cast<double>(options.plaintext_bytes);

        printf(
            "benchmark_stage_ms slm_create=%.3f ssd_to_slm=%.3f "
            "program_setup=%.3f fpga_execute=%.3f slm_to_host=%.3f "
            "host_verify=%.3f dump_write=%.3f cleanup=%.3f "
            "transport_ready=%.3f data_path=%.3f "
            "one_shot_transport_ready=%.3f one_shot_pipeline=%.3f "
            "process=%.3f\n",
            slm_create_ms,
            ssd_copy_ms,
            program_setup_ms,
            fpga_execute_ms,
            slm_read_ms,
            host_verify_ms,
            dump_write_ms,
            cleanup_ms,
            transport_ready_ms,
            data_path_ms,
            one_shot_transport_ready_ms,
            one_shot_pipeline_ms,
            process_ms);
        printf(
            "BENCH_FPGA_CSV,%u,%u,%zu,%u,%s,%zu,"
            "%.6f,%.6f,%.6f,%.6f,%.6f,"
            "%.6f,%.6f,%.6f,%.6f,%.6f,"
            "%.6f,%.6f,%.6f,%.6f,%.6f,"
            "%.3f,%.3f\n",
            options.plaintext_bytes,
            options.input_lbas,
            options.slm_read_chunk_bytes,
            options.slm_read_queue_depth,
            layout,
            cipher_physical_bytes / options.plaintext_bytes,
            slm_create_ms,
            ssd_copy_ms,
            program_setup_ms,
            fpga_execute_ms,
            slm_read_ms,
            host_verify_ms,
            dump_write_ms,
            cleanup_ms,
            transport_ready_ms,
            data_path_ms,
            one_shot_transport_ready_ms,
            one_shot_pipeline_ms,
            process_ms,
            fpga_execute_ms * 1000.0 / count,
            data_path_ms * 1000.0 / count,
            count * 1000.0 / fpga_execute_ms,
            count * 1000.0 / data_path_ms);
    }
    return status;
}

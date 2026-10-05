#pragma once
#include "selective_filter_protocol.h"
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
namespace selection_manifest {
inline uint64_t load(const uint8_t* p, unsigned n) {
    uint64_t v = 0;
    for (unsigned i = 0; i < n; ++i) v |= uint64_t(p[i]) << (8 * i);
    return v;
}
inline size_t payload_bytes(uint32_t rows) { return (size_t(rows) + 1) * 64; }
inline bool parse(const uint8_t* data, size_t bytes, uint32_t rows,
                  uint32_t record_bytes, uint32_t projection_offset,
                  std::vector<uint32_t>* indices, std::vector<uint8_t>* values,
                  std::string* error) {
    auto fail = [&](const std::string& message) { *error = message; return false; };
    if (bytes != payload_bytes(rows)) return fail("manifest payload length mismatch");
    const uint8_t* s = data + size_t(rows) * 64;
    if (load(s, 8) != SELECTIVE_FILTER_MANIFEST_MAGIC ||
        load(s + 36, 4) != SELECTIVE_FILTER_MANIFEST_VERSION)
        return fail("unsupported manifest protocol; update selective_filter RTL/BOOT.bin");
    if (load(s + 20, 4)) return fail("FPGA filter error " + std::to_string(load(s + 20, 4)));
    if (load(s + 8, 4) != rows || load(s + 12, 4) != rows ||
        load(s + 24, 4) != record_bytes || load(s + 28, 4) != projection_offset ||
        load(s + 32, 4) != SELECTIVE_FILTER_TYPE_U8) return fail("manifest task metadata mismatch");
    for (unsigned j = 40; j < 64; ++j) if (s[j]) return fail("nonzero summary reserved bytes");
    std::vector<uint32_t> ids; std::vector<uint8_t> vals;
    for (uint32_t i = 0; i < rows; ++i) {
        const uint8_t* r = data + size_t(i) * 64;
        uint64_t flag = load(r + 4, 4), value = load(r + 8, 8);
        if (load(r, 4) != i || flag > 1 || value > 255 || (!flag && value))
            return fail("invalid row identity, flag or projection");
        for (unsigned j = 16; j < 64; ++j) if (r[j]) return fail("nonzero row reserved bytes");
        if (flag) { ids.push_back(i); vals.push_back(value); }
    }
    if (load(s + 16, 4) != ids.size()) return fail("manifest count mismatch");
    indices->swap(ids); values->swap(vals); return true;
}
} // namespace selection_manifest

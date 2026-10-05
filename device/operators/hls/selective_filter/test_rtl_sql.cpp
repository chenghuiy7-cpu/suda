#include "Vselective_filter.h"
#include "verilated.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "selective_filter_protocol.h"
#include "../../../../host/applications/vscode-selective-lwe-full-pipeline/selection_manifest.hpp"

static void require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

static void store32(uint8_t *p, uint32_t value) { std::memcpy(p, &value, 4); }
static void store64(uint8_t *p, uint64_t value) { std::memcpy(p, &value, 8); }

int main(int argc, char **argv)
{
    Verilated::commandArgs(argc, argv);
    const bool manifest = argc > 1;
    const bool empty = argc > 1 && std::strcmp(argv[1], "--empty") == 0;
    const bool all = argc > 1 && std::strcmp(argv[1], "--all") == 0;
    const bool bad_config = argc > 1 && std::strcmp(argv[1], "--error") == 0;
    const bool truncated = argc > 1 && std::strcmp(argv[1], "--truncated") == 0;
    Vselective_filter dut;
    std::array<std::array<uint32_t, 16>, 256> ram{};
    uint8_t *header = reinterpret_cast<uint8_t *>(ram[3].data());
    store32(header + 0, 4); store32(header + 4, 128); store32(header + 20, manifest ? SELECTIVE_FILTER_OUTPUT_MANIFEST : 1);
    store32(header + 24, SELECTIVE_FILTER_V2_MAGIC);
    store32(header + 28, bad_config ? 99 : SELECTIVE_FILTER_V2_VERSION);
    store32(header + 32, 4); store32(header + 36, 7);
    store32(header + 40, 4); store32(header + 44, SELECTIVE_FILTER_TYPE_U8);
    const uint8_t rpn[] = {0, 1, SELECTIVE_FILTER_TOKEN_AND,
                           2, 3, SELECTIVE_FILTER_TOKEN_AND,
                           SELECTIVE_FILTER_TOKEN_OR};
    std::memcpy(header + 48, rpn, sizeof(rpn));
    struct Pred { uint32_t offset; uint8_t type, op; uint64_t literal; };
    Pred predicates[] = {
        {4, SELECTIVE_FILTER_TYPE_U8, SELECTIVE_FILTER_CMP_GT, 10},
        {8, SELECTIVE_FILTER_TYPE_U64, SELECTIVE_FILTER_CMP_LE, 500},
        {0, SELECTIVE_FILTER_TYPE_U32, SELECTIVE_FILTER_CMP_EQ, 103},
        {24, SELECTIVE_FILTER_TYPE_I32, SELECTIVE_FILTER_CMP_LT, UINT64_MAX}};
    if (all) predicates[0].literal = 0;
    uint8_t *descriptors = reinterpret_cast<uint8_t *>(ram[4].data());
    for (unsigned i = 0; i < 4; ++i) {
        uint8_t *p = descriptors + i * 16;
        store32(p, predicates[i].offset); p[4] = predicates[i].type;
        p[5] = predicates[i].op; store64(p + 8, predicates[i].literal);
    }

    struct Row { uint32_t id; uint8_t quantity; uint64_t price; int32_t delta; };
    Row rows[] = {{100,3,100,-9}, {101,17,400,1},
                        {102,9,200,-4}, {103,24,900,-2}};
    if (empty) for (auto &r : rows) { r.quantity = 0; r.id = 0; }
    std::vector<std::array<uint32_t, 16>> beats;
    for (const Row &row : rows) {
        std::array<uint32_t, 16> first{}, second{};
        uint8_t *p = reinterpret_cast<uint8_t *>(first.data());
        store32(p, row.id); p[4] = row.quantity; store64(p + 8, row.price);
        store32(p + 24, static_cast<uint32_t>(row.delta));
        beats.push_back(first); beats.push_back(second);
    }
    if (truncated) beats.resize(7);
    beats.push_back({});

    unsigned sent = 0, received = 0;
    std::vector<uint8_t> manifest_bytes;
    const uint8_t expected[] = {17, 24};
    bool finished = false;
    dut.ap_clk = 0; dut.ap_rst_n = 0; dut.ap_start = 0;
    dut.data_in_TID = 0; dut.data_in_TDEST = 0;
    for (unsigned cycle = 0; cycle < 20000 && !finished; ++cycle) {
        const bool marker = sent + 1 == beats.size();
        dut.ap_clk = 0; dut.ap_rst_n = cycle >= 5; dut.ap_start = cycle >= 5;
        dut.data_out_TREADY = cycle % 11 >= 3;
        dut.data_in_TVALID = cycle >= 6 && sent < beats.size();
        for (int w = 0; w < 16; ++w)
            dut.data_in_TDATA[w] = sent < beats.size() ? beats[sent][w] : 0;
        dut.data_in_TKEEP = UINT64_MAX; dut.data_in_TSTRB = UINT64_MAX;
        dut.data_in_TUSER = marker ? 0xff : 0; dut.data_in_TLAST = marker;
        dut.eval();
        const bool accepted = dut.data_in_TVALID && dut.data_in_TREADY;
        const bool context_read = dut.context_EN_A;
        const unsigned context_word = dut.context_Addr_A / 64;
        if (dut.ap_rst_n && dut.data_out_TVALID && dut.data_out_TREADY) {
            if (dut.data_out_TUSER == 0 && manifest) {
                require(dut.data_out_TKEEP == UINT64_MAX && !dut.data_out_TLAST,
                        "bad manifest framing");
                for (unsigned lane = 0; lane < 64; ++lane)
                    manifest_bytes.push_back((dut.data_out_TDATA[lane / 4] >> ((lane % 4) * 8)) & 255);
                ++received;
            } else if (dut.data_out_TUSER == 0) {
                require(received < 2, "extra projected row");
                require(dut.data_out_TKEEP == 1 && !dut.data_out_TLAST,
                        "bad projection framing");
                require((dut.data_out_TDATA[0] & 0xff) == expected[received],
                        "wrong projected value");
                ++received;
            } else {
                require(dut.data_out_TUSER == 0xff && dut.data_out_TLAST,
                        "bad status framing");
                require(dut.data_out_TDATA[0] == 0x53544154 &&
                        dut.data_out_TDATA[1] == 0x53464c54,
                        "bad status magic");
                if (!manifest) require(dut.data_out_TDATA[2] == 4 && dut.data_out_TDATA[3] == 2 &&
                        dut.data_out_TDATA[4] == 0, "bad status counters");
                finished = true;
            }
        }
        dut.ap_clk = 1; dut.eval();
        if (context_read) {
            require(context_word < ram.size(), "context address outside RAM");
            for (int w = 0; w < 16; ++w) dut.context_Dout_A[w] = ram[context_word][w];
            dut.eval();
        }
        if (accepted) ++sent;
        if (finished && !manifest)
            printf("PASS SQL metadata RTL rows=4 selected=2 values=[17,24] cycles=%u backpressure=yes\n", cycle);
    }
    require(finished, "SQL metadata RTL timeout");
    if (manifest) {
        require(manifest_bytes.size() == selection_manifest::payload_bytes(4), "manifest receive size mismatch");
        std::vector<uint32_t> ids;
        std::vector<uint8_t> values;
        std::string error;
        const bool parsed = selection_manifest::parse(manifest_bytes.data(), manifest_bytes.size(),
                                                      4, 128, 4, &ids, &values, &error);
        if (bad_config || truncated) require(!parsed, "manifest should report device error");
        else {
            require(parsed, error.c_str());
            const std::vector<uint32_t> wanted = empty ? std::vector<uint32_t>{}
                : all ? std::vector<uint32_t>{0,1,2,3} : std::vector<uint32_t>{1,3};
            require(ids == wanted, "wrong device row mapping");
            for (unsigned i = 0; i < ids.size(); ++i)
                require(values[i] == rows[ids[i]].quantity, "wrong manifest projection");
        }
        printf("PASS manifest RTL scenario=%s bytes=%zu selected=%zu error=%s backpressure=yes\n",
               argv[1], manifest_bytes.size(), ids.size(), error.c_str());
    }
    dut.final();
}

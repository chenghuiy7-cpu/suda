#include "Vlwe_encrypt.h"
#include "verilated.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <vector>

static void require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

// Synchronous context RAM, zero secret key/noise, and periodic output stalls.
// Check ciphertext bodies and total DMA byte count, not just the finish flag.
static void run(bool scalar)
{
    Vlwe_encrypt dut;
    std::array<std::array<uint32_t, 16>, 256> ram{};
    auto &cfg = ram[3];
    cfg[0] = 2048;
    cfg[1] = scalar ? 0 : 1;
    cfg[2] = scalar ? 3 : 2;
    cfg[3] = 2; // LWE_ENCRYPT_NOISE_ZERO
    cfg[5] = 1;
    cfg[10] = 0x12345678;
    const std::vector<uint8_t> quantities = scalar
        ? std::vector<uint8_t>{58,44,45,41,54,39}
        : std::vector<uint8_t>{160};
    const uint64_t marker_keep = scalar ? UINT64_MAX : 0xffff;
    unsigned sent = 0, payload_beats = 0, bodies = 0;
    bool finished = false;
    dut.ap_clk = 0;
    dut.ap_rst_n = 0;
    dut.ap_start = 0;
    dut.data_in_TID = 0;
    dut.data_in_TDEST = 0;
    for (unsigned cycle = 0; cycle < 2000000 && !finished; ++cycle) {
        dut.ap_clk = 0;
        dut.ap_rst_n = cycle >= 5;
        dut.ap_start = cycle >= 5;
        dut.data_out_TREADY = cycle % 13 >= 4;
        dut.data_in_TVALID = cycle >= 6 && sent <= quantities.size();
        const bool marker = sent == quantities.size();
        for (int w = 0; w < 16; ++w) dut.data_in_TDATA[w] = 0;
        dut.data_in_TDATA[0] = marker ? 0x53544154 :
            (sent < quantities.size() ? quantities[sent] : 0);
        dut.data_in_TDATA[1] = marker ? 0x53464c54 : 0;
        dut.data_in_TKEEP = marker ? marker_keep : (scalar ? 1 : UINT64_MAX);
        dut.data_in_TSTRB = dut.data_in_TKEEP;
        dut.data_in_TUSER = marker ? 0xff : 0;
        dut.data_in_TLAST = marker;
        dut.eval();
        const bool accepted = dut.data_in_TVALID && dut.data_in_TREADY;
        const bool read_context = dut.context_EN_A;
        const unsigned address = dut.context_Addr_A / 64;
        if (dut.ap_rst_n && dut.data_out_TVALID && dut.data_out_TREADY) {
            require(dut.data_out_TKEEP == UINT64_MAX, "output is not 64 valid bytes");
            require(dut.data_out_TSTRB == UINT64_MAX, "output strobe mismatch");
            if (dut.data_out_TUSER != 0) {
                require(dut.data_out_TUSER == 0xff && dut.data_out_TLAST,
                        "bad finish framing");
                require(dut.data_out_TDATA[0] == 0x53544154 &&
                        dut.data_out_TDATA[1] == 0x53464c54, "status lost");
                require(payload_beats == quantities.size() * 1536,
                        "ciphertext byte count mismatch");
                require(bodies == quantities.size() * 4, "missing ciphertext bodies");
                finished = true;
            } else {
                require(!dut.data_out_TLAST, "early payload TLAST");
                require(payload_beats < quantities.size() * 1536, "extra ciphertext");
                if (payload_beats % 384 == 128) {
                    const unsigned lwe = payload_beats / 384;
                    const uint64_t expected = uint64_t((quantities[lwe / 4] >>
                        (2 * (lwe % 4))) & 3) << 59;
                    const uint64_t body = uint64_t(dut.data_out_TDATA[0]) |
                        (uint64_t(dut.data_out_TDATA[1]) << 32);
                    require(body == expected, "decrypted body mismatch");
                    ++bodies;
                }
                ++payload_beats;
            }
        }
        dut.ap_clk = 1;
        dut.eval();
        if (read_context) {
            require(address < ram.size(), "context address outside RAM");
            for (int w = 0; w < 16; ++w) dut.context_Dout_A[w] = ram[address][w];
            dut.eval();
        }
        if (accepted) ++sent;
        if (finished)
            printf("PASS RTL mode=%u selected=%zu payload_bytes=%u finish_bytes=64 "
                   "raw_rx_bytes=%u decrypted=yes cycles=%u\n",
                   cfg[2], quantities.size(), payload_beats * 64,
                   payload_beats * 64 + 64, cycle);
    }
    require(finished, "RTL timeout");
    dut.final();
}

int main(int argc, char **argv)
{
    Verilated::commandArgs(argc, argv);
    run(false);
    run(true);
}

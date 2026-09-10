#include <cassert>
#include <cstdio>
#include "lwe_encrypt.hpp"

// RX strips a 64-byte marker; external DMA supplies only 16 valid bytes.
int main()
{
    for (int marker_bytes = 16; marker_bytes <= 64; marker_bytes += 48) {
        ap_uint<512> context[256] = {};
        ap_uint<512> &cfg = context[LWE_ENCRYPT_STATIC_CONTEXT_BASE];
        cfg.range(31, 0) = LWE_ENCRYPT_HPU_BIG_LWE_DIMENSION;
        cfg.range(95, 64) = LWE_ENCRYPT_INPUT_U8_RADIX_SCALAR_STREAM;
        cfg.range(191, 160) = LWE_ENCRYPT_OUTPUT_HPU_NATIVE;
        Acc_Data input, output;
        Acc_Data_Pkt marker;
        marker.data = 0;
        marker.data.range(63, 0) = 0x53464c5453544154ULL;
        marker.data.range(95, 64) = 16;
        marker.keep = marker_bytes == 16 ? ap_uint<64>(0xffff) : ~ap_uint<64>(0);
        marker.strb = marker.keep;
        marker.user = 0xff;
        marker.last = 1;
        marker.id = 3;
        marker.dest = 5;
        input.write(marker);
        lwe_encrypt(input, output, context);
        assert(input.empty());
        assert(!output.empty());
        Acc_Data_Pkt result = output.read();
        assert(result.keep == ~ap_uint<64>(0));
        assert(result.strb == ~ap_uint<64>(0));
        assert(result.last == 1);
        assert(result.user == marker.user);
        assert(result.data == marker.data);
        assert(result.id == marker.id && result.dest == marker.dest);
        assert(output.empty());
        printf("PASS input marker=%d bytes, output marker=64 bytes, status preserved\n", marker_bytes);
    }
}

#pragma once
#include "../../../device/shared_components/lwe_hw_profile.h"
#include <stdio.h>

// The stream interval includes stalls. Do not subtract both stall counts:
// input starvation and output backpressure can overlap in a dataflow design.
inline void print_lwe_hw_profile(const uint8_t *output, size_t bytes,
                                 size_t physical_payload_bytes)
{
    struct lwe_hw_profile profile = {};
    const size_t offset = (physical_payload_bytes + 63) & ~size_t(63);
    if (offset > bytes || !lwe_profile_decode(output + offset, bytes - offset, &profile)) {
        printf("hw_profile available=no\n");
        return;
    }
    if (profile.output_beats != offset / 64) {
        printf("hw_profile available=no reason=output_beat_count_mismatch\n");
        return;
    }
    const double cycles_per_ms = LWE_HW_PROFILE_CLOCK_HZ / 1000.0;
    printf("hw_profile available=yes version=1 clock_hz=%llu "
           "boundary=ap_start_to_finish_valid cycles=%llu stream_ms=%.6f "
           "input_wait_cycles=%llu input_wait_ms=%.6f "
           "output_wait_cycles=%llu output_wait_ms=%.6f "
           "input_beats=%llu output_beats=%llu\n",
           (unsigned long long)LWE_HW_PROFILE_CLOCK_HZ,
           (unsigned long long)profile.cycles, profile.cycles / cycles_per_ms,
           (unsigned long long)profile.input_wait_cycles, profile.input_wait_cycles / cycles_per_ms,
           (unsigned long long)profile.output_wait_cycles, profile.output_wait_cycles / cycles_per_ms,
           (unsigned long long)profile.input_beats, (unsigned long long)profile.output_beats);
}

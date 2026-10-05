#ifndef SUDA_LWE_HW_PROFILE_H
#define SUDA_LWE_HW_PROFILE_H

#include <stdint.h>
#include <stddef.h>

/* Version 1 footer in the final 64-byte SUDA marker; excludes marker bytes
 * from the existing payload/result length. All fields are little endian.
 * Runtime clears bytes 0..15 and preserves this 48-byte diagnostic footer.
 * Old hardware/runtime yields available=no, never a fabricated duration.
 */
#define LWE_HW_PROFILE_MAGIC UINT64_C(0x313030465045574c)
#define LWE_HW_PROFILE_CLOCK_HZ UINT64_C(250000000)
#define LWE_HW_PROFILE_OFFSET 16
#define LWE_HW_PROFILE_BYTES 48
#define LWE_HW_FINISH_BYTES 64

struct lwe_hw_profile {
    uint64_t cycles;
    uint64_t input_wait_cycles;
    uint64_t output_wait_cycles;
    uint64_t input_beats;
    uint64_t output_beats;
};

static inline uint64_t lwe_profile_load_le64(const uint8_t *data)
{
    uint64_t value = 0;
    for (unsigned int i = 0; i < 8; ++i) value |= (uint64_t)data[i] << (i * 8);
    return value;
}

static inline int lwe_profile_decode(const uint8_t *finish, size_t bytes,
                                    struct lwe_hw_profile *profile)
{
    if (bytes < LWE_HW_FINISH_BYTES ||
        lwe_profile_load_le64(finish + LWE_HW_PROFILE_OFFSET) != LWE_HW_PROFILE_MAGIC) return 0;
    profile->cycles = lwe_profile_load_le64(finish + 24);
    profile->input_wait_cycles = lwe_profile_load_le64(finish + 32);
    profile->output_wait_cycles = lwe_profile_load_le64(finish + 40);
    profile->input_beats = lwe_profile_load_le64(finish + 48);
    profile->output_beats = lwe_profile_load_le64(finish + 56);
    return profile->cycles != 0 && profile->input_wait_cycles <= profile->cycles &&
        profile->output_wait_cycles <= profile->cycles;
}
#endif

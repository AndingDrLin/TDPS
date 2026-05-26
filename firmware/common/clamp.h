#ifndef TDPS_CLAMP_H
#define TDPS_CLAMP_H

#include <stdint.h>

static inline int16_t TDPS_ClampI16(int32_t v, int16_t lo, int16_t hi)
{
    if (v < (int32_t)lo) {
        return lo;
    }
    if (v > (int32_t)hi) {
        return hi;
    }
    return (int16_t)v;
}

static inline uint16_t TDPS_ClampU16(int32_t v, uint16_t lo, uint16_t hi)
{
    if (v < (int32_t)lo) {
        return lo;
    }
    if (v > (int32_t)hi) {
        return hi;
    }
    return (uint16_t)v;
}

static inline uint8_t TDPS_ClampU8(uint32_t v, uint8_t lo, uint8_t hi)
{
    if (v < (uint32_t)lo) {
        return lo;
    }
    if (v > (uint32_t)hi) {
        return hi;
    }
    return (uint8_t)v;
}

#endif /* TDPS_CLAMP_H */

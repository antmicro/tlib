#pragma once

#include <stdint.h>

/*
 * Note that vector data is stored in host-endian 64-bit chunks,
 * so addressing units smaller than that needs a host-endian fixup.
 *
 * The H<N> macros are used when indexing an array of elements of size N.
 *
 * The H1_<N> macros are used when performing byte arithmetic and then
 * casting the final pointer to a type of size N.
 */
#if HOST_WORDS_BIGENDIAN
#define H1(x)   ((x) ^ 7)
#define H1_2(x) ((x) ^ 6)
#define H1_4(x) ((x) ^ 4)
#define H2(x)   ((x) ^ 3)
#define H4(x)   ((x) ^ 1)
#else
#define H1(x)   (x)
#define H1_2(x) (x)
#define H1_4(x) (x)
#define H2(x)   (x)
#define H4(x)   (x)
#endif

/*
 * Expand active predicate bits to bytes, for byte elements.
 */
extern const uint64_t expand_pred_b_data[256];
static inline uint64_t expand_pred_b(uint8_t byte)
{
    return expand_pred_b_data[byte];
}

/* Similarly for half-word elements. */
extern const uint64_t expand_pred_h_data[0x55 + 1];
static inline uint64_t expand_pred_h(uint8_t byte)
{
    return expand_pred_h_data[byte & 0x55];
}

/* Similarly for single word elements.  */
static inline uint64_t expand_pred_s(uint8_t byte)
{
    static const uint64_t word[] = {
        [0x01] = 0x00000000ffffffffull,
        [0x10] = 0xffffffff00000000ull,
        [0x11] = 0xffffffffffffffffull,
    };
    return word[byte & 0x11];
}

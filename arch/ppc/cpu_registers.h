#include "cpu-defs.h"

typedef enum {
#ifdef TARGET_PPC64
    R_0_64 = 0,
    R_1_64 = 1,
    R_2_64 = 2,
    R_3_64 = 3,
    R_4_64 = 4,
    R_5_64 = 5,
    R_6_64 = 6,
    R_7_64 = 7,
    R_8_64 = 8,
    R_9_64 = 9,
    R_10_64 = 10,
    R_11_64 = 11,
    R_12_64 = 12,
    R_13_64 = 13,
    R_14_64 = 14,
    R_15_64 = 15,
    R_16_64 = 16,
    R_17_64 = 17,
    R_18_64 = 18,
    R_19_64 = 19,
    R_20_64 = 20,
    R_21_64 = 21,
    R_22_64 = 22,
    R_23_64 = 23,
    R_24_64 = 24,
    R_25_64 = 25,
    R_26_64 = 26,
    R_27_64 = 27,
    R_28_64 = 28,
    R_29_64 = 29,
    R_30_64 = 30,
    R_31_64 = 31,
    NIP_64 = 64,
    MSR_64 = 65,
    LR_64 = 67,
    CTR_64 = 68,
    XER_64 = 69,
#endif
#ifdef TARGET_PPC32
    R_0_32 = 0,
    R_1_32 = 1,
    R_2_32 = 2,
    R_3_32 = 3,
    R_4_32 = 4,
    R_5_32 = 5,
    R_6_32 = 6,
    R_7_32 = 7,
    R_8_32 = 8,
    R_9_32 = 9,
    R_10_32 = 10,
    R_11_32 = 11,
    R_12_32 = 12,
    R_13_32 = 13,
    R_14_32 = 14,
    R_15_32 = 15,
    R_16_32 = 16,
    R_17_32 = 17,
    R_18_32 = 18,
    R_19_32 = 19,
    R_20_32 = 20,
    R_21_32 = 21,
    R_22_32 = 22,
    R_23_32 = 23,
    R_24_32 = 24,
    R_25_32 = 25,
    R_26_32 = 26,
    R_27_32 = 27,
    R_28_32 = 28,
    R_29_32 = 29,
    R_30_32 = 30,
    R_31_32 = 31,
    NIP_32 = 64,
    MSR_32 = 65,
    LR_32 = 67,
    CTR_32 = 68,
    XER_32 = 69,
#endif
} Registers;

#pragma once

#include "cpu-defs.h"

//  this is a trick to make the registers parser simpler
#if defined(TARGET_ARM32) && defined(TARGET_PROTO_ARM_M)
#define TARGET_PROTO_ARM32_M
#endif

typedef enum {
#ifdef TARGET_ARM32
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
    SP_32 = 13,
    R_14_32 = 14,
    LR_32 = 14,
    R_15_32 = 15,
    PC_32 = 15,
    CPSR_32 = 25,
#endif
#ifdef TARGET_PROTO_ARM32_M
    Control_32 = 18,
    BasePri_32 = 19,
    VecBase_32 = 20,
    CurrentSP_32 = 21,
    OtherSP_32 = 22,
    FPCCR_32 = 23,
    FPCAR_32 = 24,
    FPDSCR_32 = 26,
    CPACR_32 = 27,
    PRIMASK_32 = 28,
    FAULTMASK_32 = 30,
#endif
#ifdef TARGET_ARM64
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
    SP_32 = 13,
    R_14_32 = 14,
    LR_32 = 14,
    R_15_32 = 15,
    PC_32 = 15,
    CPSR_32 = 25,
    PC_64 = 28,
    X_0_64 = 32,
    X_1_64 = 33,
    X_2_64 = 34,
    X_3_64 = 35,
    X_4_64 = 36,
    X_5_64 = 37,
    X_6_64 = 38,
    X_7_64 = 39,
    X_8_64 = 40,
    X_9_64 = 41,
    X_10_64 = 42,
    X_11_64 = 43,
    X_12_64 = 44,
    X_13_64 = 45,
    X_14_64 = 46,
    X_15_64 = 47,
    X_16_64 = 48,
    X_17_64 = 49,
    X_18_64 = 50,
    X_19_64 = 51,
    X_20_64 = 52,
    X_21_64 = 53,
    X_22_64 = 54,
    X_23_64 = 55,
    X_24_64 = 56,
    X_25_64 = 57,
    X_26_64 = 58,
    X_27_64 = 59,
    X_28_64 = 60,
    X_29_64 = 61,
    X_30_64 = 62,
    X_31_64 = 63
#endif
} Registers;

/* The return address is stored here */
#if TARGET_LONG_BITS == 64
#define RA X_30_64
#endif
#if TARGET_LONG_BITS == 32
#define RA R_14_32
#endif

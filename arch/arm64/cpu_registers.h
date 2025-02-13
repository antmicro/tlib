#pragma once

#include "cpu-defs.h"

//  Indexes for AArch64 registers are in line with GDB's 'arch/aarch64.h'.
typedef enum {
    X_0_64 = 0,
    X_1_64 = 1,
    X_2_64 = 2,
    X_3_64 = 3,
    X_4_64 = 4,
    X_5_64 = 5,
    X_6_64 = 6,
    X_7_64 = 7,
    X_8_64 = 8,
    X_9_64 = 9,
    X_10_64 = 10,
    X_11_64 = 11,
    X_12_64 = 12,
    X_13_64 = 13,
    X_14_64 = 14,
    X_15_64 = 15,
    X_16_64 = 16,
    X_17_64 = 17,
    X_18_64 = 18,
    X_19_64 = 19,
    X_20_64 = 20,
    X_21_64 = 21,
    X_22_64 = 22,
    X_23_64 = 23,
    X_24_64 = 24,
    X_25_64 = 25,
    X_26_64 = 26,
    X_27_64 = 27,
    X_28_64 = 28,
    X_29_64 = 29,
    X_30_64 = 30,
    //  There's no X31 register even though Stack Pointer is often represented with
    //  31 in the instruction encoding (but it can also mean Zero Register: XZR/WZR).
    SP_64 = 31,
    PC_64 = 32,
    PSTATE_32 = 33,
    FPSR_32 = 66,
    FPCR_32 = 67,

    R_0_32 = 100,
    R_1_32 = 101,
    R_2_32 = 102,
    R_3_32 = 103,
    R_4_32 = 104,
    R_5_32 = 105,
    R_6_32 = 106,
    R_7_32 = 107,
    R_8_32 = 108,
    R_9_32 = 109,
    R_10_32 = 110,
    R_11_32 = 111,
    R_12_32 = 112,
    //  If AArch32 it can be also accessed with SP_64.
    R_13_32 = 113,
    R_14_32 = 114,
    //  If AArch32 it can be also accessed with PC_64.
    R_15_32 = 115,
    CPSR_32 = 125,
} Registers;

/* The return address is stored here */
#define RA (is_a64(cpu) ? X_30_64 : R_14_32)

/*
 * Tiny Code Generator for QEMU
 *
 * Copyright (c) 2008 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#pragma once
#define TCG_TARGET_I386 1

#define TCG_TARGET_DEFAULT_MO (TCG_MO_ALL & ~TCG_MO_ST_LD)

//  #define TCG_TARGET_WORDS_BIGENDIAN

#if TCG_TARGET_REG_BITS == 64
#define TCG_TARGET_NB_REGS 16
#else
#define TCG_TARGET_NB_REGS 8
#endif

typedef enum {
    TCG_REG_EAX = 0,
    TCG_REG_ECX,
    TCG_REG_EDX,
    TCG_REG_EBX,
    TCG_REG_ESP,
    TCG_REG_EBP,
    TCG_REG_ESI,
    TCG_REG_EDI,

    /* 64-bit registers; always define the symbols to avoid
       too much if-deffing.  */
    TCG_REG_R8,
    TCG_REG_R9,
    TCG_REG_R10,
    TCG_REG_R11,
    TCG_REG_R12,
    TCG_REG_R13,
    TCG_REG_R14,
    TCG_REG_R15,
    TCG_REG_RAX = TCG_REG_EAX,
    TCG_REG_RCX = TCG_REG_ECX,
    TCG_REG_RDX = TCG_REG_EDX,
    TCG_REG_RBX = TCG_REG_EBX,
    TCG_REG_RSP = TCG_REG_ESP,
    TCG_REG_RBP = TCG_REG_EBP,
    TCG_REG_RSI = TCG_REG_ESI,
    TCG_REG_RDI = TCG_REG_EDI,
} TCGReg;

typedef struct TCGReg128 {
    TCGReg low;
    TCGReg high;
} TCGReg128;

#define TCG_CT_CONST_S32 0x100
#define TCG_CT_CONST_U32 0x200

/* used for function call generation */
#define TCG_REG_CALL_STACK     TCG_REG_ESP
#define TCG_TARGET_STACK_ALIGN 16
#if defined(_WIN64)
#define TCG_TARGET_CALL_STACK_OFFSET 32
#else
#define TCG_TARGET_CALL_STACK_OFFSET 0
#endif

/* optional instructions */
#define TCG_TARGET_HAS_andc_i32                              0
#define TCG_TARGET_HAS_bswap16_i32                           1
#define TCG_TARGET_HAS_bswap32_i32                           1
#define TCG_TARGET_HAS_deposit_i32                           1
#define TCG_TARGET_HAS_div2_i32                              1
#define TCG_TARGET_HAS_eqv_i32                               0
#define TCG_TARGET_HAS_ext16s_i32                            1
#define TCG_TARGET_HAS_ext16u_i32                            1
#define TCG_TARGET_HAS_ext8s_i32                             1
#define TCG_TARGET_HAS_ext8u_i32                             1
#define TCG_TARGET_HAS_extract_i32                           1
#define TCG_TARGET_HAS_movcond_i32                           1
#define TCG_TARGET_HAS_muls2_i32                             1
#define TCG_TARGET_HAS_mulu2_i32                             1
#define TCG_TARGET_HAS_nand_i32                              0
#define TCG_TARGET_HAS_neg_i32                               1
#define TCG_TARGET_HAS_nor_i32                               0
#define TCG_TARGET_HAS_not_i32                               1
#define TCG_TARGET_HAS_orc_i32                               0
#define TCG_TARGET_HAS_rot_i32                               1
#define TCG_TARGET_HAS_atomic_fetch_add_intrinsic_i32        1
#define TCG_TARGET_HAS_atomic_compare_and_swap_intrinsic_i32 1

#if TCG_TARGET_REG_BITS == 64
#define TCG_TARGET_HAS_andc_i64                               0
#define TCG_TARGET_HAS_bswap16_i64                            1
#define TCG_TARGET_HAS_bswap32_i64                            1
#define TCG_TARGET_HAS_bswap64_i64                            1
#define TCG_TARGET_HAS_deposit_i64                            1
#define TCG_TARGET_HAS_div2_i64                               1
#define TCG_TARGET_HAS_eqv_i64                                0
#define TCG_TARGET_HAS_ext16s_i64                             1
#define TCG_TARGET_HAS_ext16u_i64                             1
#define TCG_TARGET_HAS_ext32s_i64                             1
#define TCG_TARGET_HAS_ext32u_i64                             1
#define TCG_TARGET_HAS_ext8s_i64                              1
#define TCG_TARGET_HAS_ext8u_i64                              1
#define TCG_TARGET_HAS_movcond_i64                            1
#define TCG_TARGET_HAS_muls2_i64                              1
#define TCG_TARGET_HAS_mulu2_i64                              1
#define TCG_TARGET_HAS_nand_i64                               0
#define TCG_TARGET_HAS_neg_i64                                1
#define TCG_TARGET_HAS_nor_i64                                0
#define TCG_TARGET_HAS_not_i64                                1
#define TCG_TARGET_HAS_orc_i64                                0
#define TCG_TARGET_HAS_qemu_st8_i32                           0
#define TCG_TARGET_HAS_rot_i64                                1
#define TCG_TARGET_HAS_atomic_fetch_add_intrinsic_i64         1
#define TCG_TARGET_HAS_atomic_compare_and_swap_intrinsic_i64  1
#define TCG_TARGET_HAS_atomic_compare_and_swap_intrinsic_i128 1

#else
#define TCG_TARGET_HAS_qemu_st8_i32 1
#endif

/* Whether the host has any atomic intrinsics implemented at all. */
#define TCG_TARGET_HAS_INTRINSIC_ATOMICS                                                                             \
    ((TCG_TARGET_HAS_atomic_fetch_add_intrinsic_i32 == 1) || (TCG_TARGET_HAS_atomic_fetch_add_intrinsic_i64 == 1) || \
     (TCG_TARGET_HAS_atomic_compare_and_swap_intrinsic_i32 == 1) ||                                                  \
     (TCG_TARGET_HAS_atomic_compare_and_swap_intrinsic_i64 == 1) ||                                                  \
     (TCG_TARGET_HAS_atomic_compare_and_swap_intrinsic_i128 == 1))

//  MOVBE isn't very common in non-Atom CPUs and it isn't currently supported by TCG.
#define TCG_TARGET_HAS_MEMORY_BSWAP 0

#define TCG_TARGET_deposit_i32_valid(ofs, len) \
    (((ofs) == 0 && (len) == 8) || ((ofs) == 8 && (len) == 8) || ((ofs) == 0 && (len) == 16))
#define TCG_TARGET_deposit_i64_valid TCG_TARGET_deposit_i32_valid

/* Check for the possibility of high-byte extraction and, for 64-bit,
   zero-extending 32-bit right-shift.  */
#define TCG_TARGET_extract_i32_valid(ofs, len) ((ofs) == 8 && (len) == 8)
#define TCG_TARGET_extract_i64_valid(ofs, len) (((ofs) == 8 && (len) == 8) || ((ofs) + (len)) == 32)

#define TCG_TARGET_HAS_GUEST_BASE

/* Note: must be synced with cpu-defs.h */
#if TCG_TARGET_REG_BITS == 64
#define TCG_AREG0 TCG_REG_R14
#else
#if defined(__linux__)
#define TCG_AREG0 TCG_REG_EBX
#else
#define TCG_AREG0 TCG_REG_EBP
#endif
#endif

static inline void flush_icache_range(uintptr_t start, uintptr_t stop) { }

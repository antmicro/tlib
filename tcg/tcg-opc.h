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

/*
 * DEF(name, oargs, iargs, cargs, flags)
 */

/* predefined ops */
DEF(end, 0, 0, 0, 0) /* must be kept first */
DEF(nop, 0, 0, 0, 0)
DEF(nop1, 0, 0, 1, 0)
DEF(nop2, 0, 0, 2, 0)
DEF(nop3, 0, 0, 3, 0)
DEF(nopn, 0, 0, 1, 0) /* variable number of parameters */

DEF(discard, 1, 0, 0, 0)

DEF(set_label, 0, 0, 1, 0)
DEF(call, 0, 1, 2, TCG_OPF_SIDE_EFFECTS) /* variable number of parameters */
DEF(jmp, 0, 1, 0, TCG_OPF_BB_END | TCG_OPF_SIDE_EFFECTS)
DEF(br, 0, 0, 1, TCG_OPF_BB_END | TCG_OPF_SIDE_EFFECTS)

#define IMPL(X) (X ? 0 : TCG_OPF_NOT_PRESENT)
#if TCG_TARGET_REG_BITS == 32
#define IMPL64 TCG_OPF_64BIT | TCG_OPF_NOT_PRESENT
#else
#define IMPL64 TCG_OPF_64BIT
#endif

DEF(mb, 0, 0, 1, 0)

DEF(mov_i32, 1, 1, 0, 0)
DEF(movi_i32, 1, 0, 1, 0)
DEF(setcond_i32, 1, 2, 1, 0)
DEF(movcond_i32, 1, 4, 1, IMPL(TCG_TARGET_HAS_movcond_i32))
/* load/store */
DEF(ld8u_i32, 1, 1, 1, 0)
DEF(ld8s_i32, 1, 1, 1, 0)
DEF(ld16u_i32, 1, 1, 1, 0)
DEF(ld16s_i32, 1, 1, 1, 0)
DEF(ld_i32, 1, 1, 1, 0)
DEF(st8_i32, 0, 2, 1, TCG_OPF_SIDE_EFFECTS)
DEF(st16_i32, 0, 2, 1, TCG_OPF_SIDE_EFFECTS)
DEF(st_i32, 0, 2, 1, TCG_OPF_SIDE_EFFECTS)
/* arith */
DEF(add_i32, 1, 2, 0, 0)
DEF(sub_i32, 1, 2, 0, 0)
DEF(mul_i32, 1, 2, 0, 0)
DEF(div_i32, 1, 2, 0, IMPL(TCG_TARGET_HAS_div_i32))
DEF(divu_i32, 1, 2, 0, IMPL(TCG_TARGET_HAS_div_i32))
DEF(rem_i32, 1, 2, 0, IMPL(TCG_TARGET_HAS_div_i32))
DEF(remu_i32, 1, 2, 0, IMPL(TCG_TARGET_HAS_div_i32))
DEF(div2_i32, 2, 3, 0, IMPL(TCG_TARGET_HAS_div2_i32))
DEF(divu2_i32, 2, 3, 0, IMPL(TCG_TARGET_HAS_div2_i32))
DEF(and_i32, 1, 2, 0, 0)
DEF(or_i32, 1, 2, 0, 0)
DEF(xor_i32, 1, 2, 0, 0)
/* shifts/rotates */
DEF(shl_i32, 1, 2, 0, 0)
DEF(shr_i32, 1, 2, 0, 0)
DEF(sar_i32, 1, 2, 0, 0)
DEF(rotl_i32, 1, 2, 0, IMPL(TCG_TARGET_HAS_rot_i32))
DEF(rotr_i32, 1, 2, 0, IMPL(TCG_TARGET_HAS_rot_i32))
DEF(deposit_i32, 1, 2, 2, IMPL(TCG_TARGET_HAS_deposit_i32))
DEF(extract_i32, 1, 1, 2, IMPL(TCG_TARGET_HAS_extract_i32))
/* atomics */
/*
 * operation has 1 output arg, 2 input args and 0 constant args.
 * TCG_OPF_CALL_CLOBBER: The implementation makes function calls and thus clobbers call registers,
 *                       so inform TCG about it such that it may avoid allocating these registers.
 * TCG_OPF_SIDE_EFFECTS: The operation writes to memory, i.e. it has side-effects.
 *                       Therefore it shouldn't be optimized away even if the result of
 *                       the operation is unused.
 */
DEF(atomic_fetch_add_intrinsic_i32, 1, 2, 0, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
/*
 * operation has 1 output arg, 3 input args and 0 constant args.
 * TCG_OPF_SIDE_EFFECTS: The operation writes to memory, i.e. it has side-effects.
 *                       Therefore it shouldn't be optimized away even if the result of
 *                       the operation is unused.
 *            IMPL(...): This operation is only implemented when this definition is set to 1.
 */
DEF(atomic_compare_and_swap_intrinsic_i32, 1, 3, 0,
    TCG_OPF_SIDE_EFFECTS | IMPL(TCG_TARGET_HAS_atomic_compare_and_swap_intrinsic_i32))

DEF(brcond_i32, 0, 2, 2, TCG_OPF_BB_END | TCG_OPF_SIDE_EFFECTS)

DEF(add2_i32, 2, 4, 0, 0)
DEF(sub2_i32, 2, 4, 0, 0)
DEF(brcond2_i32, 0, 4, 2, TCG_OPF_BB_END | TCG_OPF_SIDE_EFFECTS | IMPL(TCG_TARGET_REG_BITS == 32))
DEF(mulu2_i32, 2, 2, 0, IMPL(TCG_TARGET_HAS_mulu2_i32))
DEF(muls2_i32, 2, 2, 0, IMPL(TCG_TARGET_HAS_muls2_i32))
DEF(setcond2_i32, 1, 4, 1, IMPL(TCG_TARGET_REG_BITS == 32))

DEF(ext8s_i32, 1, 1, 0, IMPL(TCG_TARGET_HAS_ext8s_i32))
DEF(ext16s_i32, 1, 1, 0, IMPL(TCG_TARGET_HAS_ext16s_i32))
DEF(ext8u_i32, 1, 1, 0, IMPL(TCG_TARGET_HAS_ext8u_i32))
DEF(ext16u_i32, 1, 1, 0, IMPL(TCG_TARGET_HAS_ext16u_i32))
DEF(bswap16_i32, 1, 1, 1, IMPL(TCG_TARGET_HAS_bswap16_i32))
DEF(bswap32_i32, 1, 1, 1, IMPL(TCG_TARGET_HAS_bswap32_i32))
DEF(not_i32, 1, 1, 0, IMPL(TCG_TARGET_HAS_not_i32))
DEF(neg_i32, 1, 1, 0, IMPL(TCG_TARGET_HAS_neg_i32))
DEF(andc_i32, 1, 2, 0, IMPL(TCG_TARGET_HAS_andc_i32))
DEF(orc_i32, 1, 2, 0, IMPL(TCG_TARGET_HAS_orc_i32))
DEF(eqv_i32, 1, 2, 0, IMPL(TCG_TARGET_HAS_eqv_i32))
DEF(nand_i32, 1, 2, 0, IMPL(TCG_TARGET_HAS_nand_i32))
DEF(nor_i32, 1, 2, 0, IMPL(TCG_TARGET_HAS_nor_i32))

DEF(mov_i64, 1, 1, 0, IMPL64)
DEF(movi_i64, 1, 0, 1, IMPL64)
DEF(setcond_i64, 1, 2, 1, IMPL64)
DEF(movcond_i64, 1, 4, 1, IMPL64 | IMPL(TCG_TARGET_HAS_movcond_i64))
/* load/store */
DEF(ld8u_i64, 1, 1, 1, IMPL64)
DEF(ld8s_i64, 1, 1, 1, IMPL64)
DEF(ld16u_i64, 1, 1, 1, IMPL64)
DEF(ld16s_i64, 1, 1, 1, IMPL64)
DEF(ld32u_i64, 1, 1, 1, IMPL64)
DEF(ld32s_i64, 1, 1, 1, IMPL64)
DEF(ld_i64, 1, 1, 1, IMPL64)
DEF(st8_i64, 0, 2, 1, TCG_OPF_SIDE_EFFECTS | IMPL64)
DEF(st16_i64, 0, 2, 1, TCG_OPF_SIDE_EFFECTS | IMPL64)
DEF(st32_i64, 0, 2, 1, TCG_OPF_SIDE_EFFECTS | IMPL64)
DEF(st_i64, 0, 2, 1, TCG_OPF_SIDE_EFFECTS | IMPL64)
/* arith */
DEF(add_i64, 1, 2, 0, IMPL64)
DEF(sub_i64, 1, 2, 0, IMPL64)
DEF(mul_i64, 1, 2, 0, IMPL64)
DEF(div_i64, 1, 2, 0, IMPL64 | IMPL(TCG_TARGET_HAS_div_i64))
DEF(divu_i64, 1, 2, 0, IMPL64 | IMPL(TCG_TARGET_HAS_div_i64))
DEF(rem_i64, 1, 2, 0, IMPL64 | IMPL(TCG_TARGET_HAS_div_i64))
DEF(remu_i64, 1, 2, 0, IMPL64 | IMPL(TCG_TARGET_HAS_div_i64))
DEF(div2_i64, 2, 3, 0, IMPL64 | IMPL(TCG_TARGET_HAS_div2_i64))
DEF(divu2_i64, 2, 3, 0, IMPL64 | IMPL(TCG_TARGET_HAS_div2_i64))
DEF(and_i64, 1, 2, 0, IMPL64)
DEF(or_i64, 1, 2, 0, IMPL64)
DEF(xor_i64, 1, 2, 0, IMPL64)
/* shifts/rotates */
DEF(shl_i64, 1, 2, 0, IMPL64)
DEF(shr_i64, 1, 2, 0, IMPL64)
DEF(sar_i64, 1, 2, 0, IMPL64)
DEF(rotl_i64, 1, 2, 0, IMPL64 | IMPL(TCG_TARGET_HAS_rot_i64))
DEF(rotr_i64, 1, 2, 0, IMPL64 | IMPL(TCG_TARGET_HAS_rot_i64))
DEF(deposit_i64, 1, 2, 2, IMPL64 | IMPL(TCG_TARGET_HAS_deposit_i64))
/* atomics */
/*
 * operation has 1 output arg, 2 input args and 0 constant args.
 * TCG_OPF_CALL_CLOBBER: The implementation makes function calls and thus clobbers call registers,
 *                       so inform TCG about it such that it may avoid allocating these registers.
 * TCG_OPF_SIDE_EFFECTS: The operation writes to memory, i.e. it has side-effects.
 *                       Therefore it shouldn't be optimized away even if the result of
 *                       the operation is unused.
 */
DEF(atomic_fetch_add_intrinsic_i64, 1, 2, 0, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
/*
 * operation has 1 output arg, 3 input args and 0 constant args.
 * TCG_OPF_SIDE_EFFECTS: The operation writes to memory, i.e. it has side-effects.
 *                       Therefore it shouldn't be optimized away even if the result of
 *                       the operation is unused.
 *            IMPL(...): This operation is only implemented when this definition is set to 1.
 *               IMPL64: This operation is only implemented on 64-bit hosts.
 */
DEF(atomic_compare_and_swap_intrinsic_i64, 1, 3, 0,
    TCG_OPF_SIDE_EFFECTS | IMPL64 | IMPL(TCG_TARGET_HAS_atomic_compare_and_swap_intrinsic_i64))
/*
 * operation has 2 output args, 5 input args and 0 constant args.
 * TCG_OPF_SIDE_EFFECTS: The operation writes to memory, i.e. it has side-effects.
 *                       Therefore it shouldn't be optimized away even if the result of
 *                       the operation is unused.
 *            IMPL(...): This operation is only implemented when this definition is set to 1.
 *               IMPL64: This operation is only implemented on 64-bit hosts.
 */
DEF(atomic_compare_and_swap_intrinsic_i128, 2, 5, 0,
    TCG_OPF_SIDE_EFFECTS | IMPL64 | IMPL(TCG_TARGET_HAS_atomic_compare_and_swap_intrinsic_i128))

DEF(brcond_i64, 0, 2, 2, TCG_OPF_BB_END | TCG_OPF_SIDE_EFFECTS | IMPL64)
DEF(ext8s_i64, 1, 1, 0, IMPL64 | IMPL(TCG_TARGET_HAS_ext8s_i64))
DEF(ext16s_i64, 1, 1, 0, IMPL64 | IMPL(TCG_TARGET_HAS_ext16s_i64))
DEF(ext32s_i64, 1, 1, 0, IMPL64 | IMPL(TCG_TARGET_HAS_ext32s_i64))
DEF(ext8u_i64, 1, 1, 0, IMPL64 | IMPL(TCG_TARGET_HAS_ext8u_i64))
DEF(ext16u_i64, 1, 1, 0, IMPL64 | IMPL(TCG_TARGET_HAS_ext16u_i64))
DEF(ext32u_i64, 1, 1, 0, IMPL64 | IMPL(TCG_TARGET_HAS_ext32u_i64))
DEF(bswap16_i64, 1, 1, 1, IMPL64 | IMPL(TCG_TARGET_HAS_bswap16_i64))
DEF(bswap32_i64, 1, 1, 1, IMPL64 | IMPL(TCG_TARGET_HAS_bswap32_i64))
DEF(bswap64_i64, 1, 1, 1, IMPL64 | IMPL(TCG_TARGET_HAS_bswap64_i64))
DEF(not_i64, 1, 1, 0, IMPL64 | IMPL(TCG_TARGET_HAS_not_i64))
DEF(neg_i64, 1, 1, 0, IMPL64 | IMPL(TCG_TARGET_HAS_neg_i64))
DEF(andc_i64, 1, 2, 0, IMPL64 | IMPL(TCG_TARGET_HAS_andc_i64))
DEF(orc_i64, 1, 2, 0, IMPL64 | IMPL(TCG_TARGET_HAS_orc_i64))
DEF(eqv_i64, 1, 2, 0, IMPL64 | IMPL(TCG_TARGET_HAS_eqv_i64))
DEF(nand_i64, 1, 2, 0, IMPL64 | IMPL(TCG_TARGET_HAS_nand_i64))
DEF(nor_i64, 1, 2, 0, IMPL64 | IMPL(TCG_TARGET_HAS_nor_i64))

DEF(mulu2_i64, 2, 2, 0, IMPL64 | IMPL(TCG_TARGET_HAS_mulu2_i64))
DEF(muls2_i64, 2, 2, 0, IMPL64 | IMPL(TCG_TARGET_HAS_muls2_i64))

#if TARGET_LONG_BITS > TCG_TARGET_REG_BITS
DEF(insn_start, 0, 0, 2 * TARGET_INSN_START_WORDS, TCG_OPF_NOT_PRESENT)
#else
DEF(insn_start, 0, 0, TARGET_INSN_START_WORDS, TCG_OPF_NOT_PRESENT)
#endif

DEF(exit_tb, 0, 0, 1, TCG_OPF_BB_END | TCG_OPF_SIDE_EFFECTS)
DEF(goto_tb, 0, 0, 1, TCG_OPF_BB_END | TCG_OPF_SIDE_EFFECTS)
/* Note: even if TARGET_LONG_BITS is not defined, the INDEX_op
   constants must be defined */
#if TCG_TARGET_REG_BITS == 32
#if TARGET_LONG_BITS == 32
DEF(qemu_ld8u, 1, 1, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
#else
DEF(qemu_ld8u, 1, 2, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
#endif
#if TARGET_LONG_BITS == 32
DEF(qemu_ld8s, 1, 1, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
#else
DEF(qemu_ld8s, 1, 2, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
#endif
#if TARGET_LONG_BITS == 32
DEF(qemu_ld16u, 1, 1, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
#else
DEF(qemu_ld16u, 1, 2, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
#endif
#if TARGET_LONG_BITS == 32
DEF(qemu_ld16s, 1, 1, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
#else
DEF(qemu_ld16s, 1, 2, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
#endif
#if TARGET_LONG_BITS == 32
DEF(qemu_ld32, 1, 1, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
#else
DEF(qemu_ld32, 1, 2, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
#endif
#if TARGET_LONG_BITS == 32
DEF(qemu_ld64, 2, 1, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
#else
DEF(qemu_ld64, 2, 2, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
#endif

#if TARGET_LONG_BITS == 32
DEF(qemu_st8, 0, 2, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
#else
DEF(qemu_st8, 0, 3, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
#endif
#if TARGET_LONG_BITS == 32
DEF(qemu_st16, 0, 2, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
#else
DEF(qemu_st16, 0, 3, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
#endif
#if TARGET_LONG_BITS == 32
DEF(qemu_st32, 0, 2, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
#else
DEF(qemu_st32, 0, 3, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
#endif
#if TARGET_LONG_BITS == 32
DEF(qemu_st64, 0, 3, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
#else
DEF(qemu_st64, 0, 4, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
#endif

#else /* TCG_TARGET_REG_BITS == 32 */

DEF(qemu_ld8u, 1, 1, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
DEF(qemu_ld8s, 1, 1, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
DEF(qemu_ld16u, 1, 1, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
DEF(qemu_ld16s, 1, 1, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
DEF(qemu_ld32, 1, 1, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
DEF(qemu_ld32u, 1, 1, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
DEF(qemu_ld32s, 1, 1, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
DEF(qemu_ld64, 1, 1, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)

DEF(qemu_st8, 0, 2, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
DEF(qemu_st16, 0, 2, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
DEF(qemu_st32, 0, 2, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)
DEF(qemu_st64, 0, 2, 1, TCG_OPF_CALL_CLOBBER | TCG_OPF_SIDE_EFFECTS)

#endif /* TCG_TARGET_REG_BITS != 32 */

/* Host vector support.  */

#define IMPLVEC TCG_OPF_VECTOR | IMPL(TCG_TARGET_MAYBE_vec)

DEF(mov_vec, 1, 1, 0, TCG_OPF_VECTOR | TCG_OPF_NOT_PRESENT)

DEF(dup_vec, 1, 1, 0, IMPLVEC)
DEF(dup2_vec, 1, 2, 0, IMPLVEC | IMPL(TCG_TARGET_REG_BITS == 32))

DEF(ld_vec, 1, 1, 1, IMPLVEC)
DEF(st_vec, 0, 2, 1, IMPLVEC)
DEF(dupm_vec, 1, 1, 1, IMPLVEC)

DEF(add_vec, 1, 2, 0, IMPLVEC)
DEF(sub_vec, 1, 2, 0, IMPLVEC)
DEF(mul_vec, 1, 2, 0, IMPLVEC | IMPL(TCG_TARGET_HAS_mul_vec))
DEF(neg_vec, 1, 1, 0, IMPLVEC | IMPL(TCG_TARGET_HAS_neg_vec))
DEF(abs_vec, 1, 1, 0, IMPLVEC | IMPL(TCG_TARGET_HAS_abs_vec))
DEF(ssadd_vec, 1, 2, 0, IMPLVEC | IMPL(TCG_TARGET_HAS_sat_vec))
DEF(usadd_vec, 1, 2, 0, IMPLVEC | IMPL(TCG_TARGET_HAS_sat_vec))
DEF(sssub_vec, 1, 2, 0, IMPLVEC | IMPL(TCG_TARGET_HAS_sat_vec))
DEF(ussub_vec, 1, 2, 0, IMPLVEC | IMPL(TCG_TARGET_HAS_sat_vec))
DEF(smin_vec, 1, 2, 0, IMPLVEC | IMPL(TCG_TARGET_HAS_minmax_vec))
DEF(umin_vec, 1, 2, 0, IMPLVEC | IMPL(TCG_TARGET_HAS_minmax_vec))
DEF(smax_vec, 1, 2, 0, IMPLVEC | IMPL(TCG_TARGET_HAS_minmax_vec))
DEF(umax_vec, 1, 2, 0, IMPLVEC | IMPL(TCG_TARGET_HAS_minmax_vec))

DEF(and_vec, 1, 2, 0, IMPLVEC)
DEF(or_vec, 1, 2, 0, IMPLVEC)
DEF(xor_vec, 1, 2, 0, IMPLVEC)
DEF(andc_vec, 1, 2, 0, IMPLVEC | IMPL(TCG_TARGET_HAS_andc_vec))
DEF(orc_vec, 1, 2, 0, IMPLVEC | IMPL(TCG_TARGET_HAS_orc_vec))
DEF(nand_vec, 1, 2, 0, IMPLVEC | IMPL(TCG_TARGET_HAS_nand_vec))
DEF(nor_vec, 1, 2, 0, IMPLVEC | IMPL(TCG_TARGET_HAS_nor_vec))
DEF(eqv_vec, 1, 2, 0, IMPLVEC | IMPL(TCG_TARGET_HAS_eqv_vec))
DEF(not_vec, 1, 1, 0, IMPLVEC | IMPL(TCG_TARGET_HAS_not_vec))

DEF(shli_vec, 1, 1, 1, IMPLVEC | IMPL(TCG_TARGET_HAS_shi_vec))
DEF(shri_vec, 1, 1, 1, IMPLVEC | IMPL(TCG_TARGET_HAS_shi_vec))
DEF(sari_vec, 1, 1, 1, IMPLVEC | IMPL(TCG_TARGET_HAS_shi_vec))
DEF(rotli_vec, 1, 1, 1, IMPLVEC | IMPL(TCG_TARGET_HAS_roti_vec))

DEF(shls_vec, 1, 2, 0, IMPLVEC | IMPL(TCG_TARGET_HAS_shs_vec))
DEF(shrs_vec, 1, 2, 0, IMPLVEC | IMPL(TCG_TARGET_HAS_shs_vec))
DEF(sars_vec, 1, 2, 0, IMPLVEC | IMPL(TCG_TARGET_HAS_shs_vec))
DEF(rotls_vec, 1, 2, 0, IMPLVEC | IMPL(TCG_TARGET_HAS_rots_vec))

DEF(shlv_vec, 1, 2, 0, IMPLVEC | IMPL(TCG_TARGET_HAS_shv_vec))
DEF(shrv_vec, 1, 2, 0, IMPLVEC | IMPL(TCG_TARGET_HAS_shv_vec))
DEF(sarv_vec, 1, 2, 0, IMPLVEC | IMPL(TCG_TARGET_HAS_shv_vec))
DEF(rotlv_vec, 1, 2, 0, IMPLVEC | IMPL(TCG_TARGET_HAS_rotv_vec))
DEF(rotrv_vec, 1, 2, 0, IMPLVEC | IMPL(TCG_TARGET_HAS_rotv_vec))

DEF(cmp_vec, 1, 2, 1, IMPLVEC)

DEF(bitsel_vec, 1, 3, 0, IMPLVEC | IMPL(TCG_TARGET_HAS_bitsel_vec))
DEF(cmpsel_vec, 1, 4, 1, IMPLVEC | IMPL(TCG_TARGET_HAS_cmpsel_vec))

#undef IMPLVEC

#undef IMPL
#undef IMPL64
#undef DEF

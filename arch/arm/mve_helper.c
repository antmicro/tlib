/*
 *  ARM helper functions for M-Profile Vector Extension (MVE)
 *
 *  Copyright (c) Antmicro
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifdef TARGET_PROTO_ARM_M

#include "cpu.h"
#include "helper.h"
#include "common.h"
#include "vec_common.h"
#include "softfloat-2.h"

#define DO_ADD(N, M) ((N) + (M))
#define DO_SUB(N, M) ((N) - (M))
#define DO_MUL(N, M) ((N) * (M))
#define DO_MAX(N, M) ((N) >= (M) ? (N) : (M))
#define DO_MIN(N, M) ((N) >= (M) ? (M) : (N))
#define DO_ABS(N)    ((N) < 0 ? -(N) : (N))
#define DO_NEG(N)    (-(N))

static uint16_t mve_eci_mask(CPUState *env)
{
    /*
     * Return the mask of which elements in the MVE vector correspond
     * to beats being executed. The mask has 1 bits for executed lanes
     * and 0 bits where ECI says this beat was already executed.
     */
    uint32_t eci = env->condexec_bits;

    if((eci & 0xf) != 0) {
        return 0xffff;
    }

    switch(eci >> 4) {
        case ECI_NONE:
            return 0xffff;
        case ECI_A0:
            return 0xfff0;
        case ECI_A0A1:
            return 0xff00;
        case ECI_A0A1A2:
        case ECI_A0A1A2B0:
            return 0xf000;
        default:
            g_assert_not_reached();
    }
}

static uint16_t mve_element_mask(CPUState *env)
{
    /*
     * Return the mask of which elements in the MVE vector should be
     * updated. This is a combination of multiple things:
     *  (1) by default, we update every lane in the vector
     *  (2) VPT predication stores its state in the VPR register;
     *  (3) low-overhead-branch tail predication will mask out part
     *      the vector on the final iteration of the loop
     *  (4) if EPSR.ECI is set then we must execute only some beats
     *      of the insn
     * We combine all these into a 16-bit result with the same semantics
     * as VPR.P0: 0 to mask the lane, 1 if it is active.
     * 8-bit vector ops will look at all bits of the result;
     * 16-bit ops will look at bits 0, 2, 4, ...;
     * 32-bit ops will look at bits 0, 4, 8 and 12.
     * Compare pseudocode GetCurInstrBeat(), though that only returns
     * the 4-bit slice of the mask corresponding to a single beat.
     */
    uint16_t mask = FIELD_EX32(env->v7m.vpr, V7M_VPR, P0);

    if(!(env->v7m.vpr & __REGISTER_V7M_VPR_MASK01_MASK)) {
        mask |= 0xff;
    }
    if(!(env->v7m.vpr & __REGISTER_V7M_VPR_MASK23_MASK)) {
        mask |= 0xff00;
    }

    if(env->v7m.ltpsize < 4 && env->regs[14] <= (1 << (4 - env->v7m.ltpsize))) {
        /*
         * Tail predication active, and this is the last loop iteration.
         * The element size is (1 << ltpsize), and we only want to process
         * loopcount elements, so we want to retain the least significant
         * (loopcount * esize) predicate bits and zero out bits above that.
         */
        int masklen = env->regs[14] << env->v7m.ltpsize;
        assert(masklen <= 16);
        mask &= masklen ? MAKE_64BIT_MASK(0, masklen) : 0;
    }

    /*
     * ECI bits indicate which beats are already executed;
     * we handle this by effectively predicating them out.
     */
    mask &= mve_eci_mask(env);
    return mask;
}

static void mve_advance_vpt(CPUState *env)
{
    /* Advance the VPT and ECI state if necessary */
    uint32_t vpr = env->v7m.vpr;
    unsigned mask01, mask23;
    uint16_t inv_mask;
    uint16_t eci_mask = mve_eci_mask(env);

    if((env->condexec_bits & 0xf) == 0) {
        env->condexec_bits = (env->condexec_bits == (ECI_A0A1A2B0 << 4)) ? (ECI_A0 << 4) : (ECI_NONE << 4);
    }

    if(!(vpr & (__REGISTER_V7M_VPR_MASK01_MASK | __REGISTER_V7M_VPR_MASK23_MASK))) {
        /* VPT not enabled, nothing to do */
        return;
    }

    /* Invert P0 bits if needed, but only for beats we actually executed */
    mask01 = FIELD_EX32(vpr, V7M_VPR, MASK01);
    mask23 = FIELD_EX32(vpr, V7M_VPR, MASK23);
    /* Start by assuming we invert all bits corresponding to executed beats */
    inv_mask = eci_mask;
    if(mask01 <= 8) {
        /* MASK01 says don't invert low half of P0 */
        inv_mask &= ~0xff;
    }
    if(mask23 <= 8) {
        /* MASK23 says don't invert high half of P0 */
        inv_mask &= ~0xff00;
    }
    vpr ^= inv_mask;
    /* Only update MASK01 if beat 1 executed */
    if(eci_mask & 0xf0) {
        vpr = FIELD_DP32(vpr, V7M_VPR, MASK01, mask01 << 1);
    }
    /* Beat 3 always executes, so update MASK23 */
    vpr = FIELD_DP32(vpr, V7M_VPR, MASK23, mask23 << 1);
    env->v7m.vpr = vpr;
}

/*
 * The mergemask(D, R, M) macro performs the operation "*D = R" but
 * storing only the bytes which correspond to 1 bits in M,
 * leaving other bytes in *D unchanged. We use _Generic
 * to select the correct implementation based on the type of D.
 */

static void mergemask_ub(uint8_t *d, uint8_t r, uint16_t mask)
{
    if(mask & 1) {
        *d = r;
    }
}

static void mergemask_sb(int8_t *d, int8_t r, uint16_t mask)
{
    mergemask_ub((uint8_t *)d, r, mask);
}

static void mergemask_uh(uint16_t *d, uint16_t r, uint16_t mask)
{
    uint16_t bmask = expand_pred_b(mask);
    *d = (*d & ~bmask) | (r & bmask);
}

static void mergemask_sh(int16_t *d, int16_t r, uint16_t mask)
{
    mergemask_uh((uint16_t *)d, r, mask);
}

static void mergemask_uw(uint32_t *d, uint32_t r, uint16_t mask)
{
    uint32_t bmask = expand_pred_b(mask);
    *d = (*d & ~bmask) | (r & bmask);
}

static void mergemask_sw(int32_t *d, int32_t r, uint16_t mask)
{
    mergemask_uw((uint32_t *)d, r, mask);
}

static void mergemask_uq(uint64_t *d, uint64_t r, uint16_t mask)
{
    uint64_t bmask = expand_pred_b(mask);
    *d = (*d & ~bmask) | (r & bmask);
}

static void mergemask_sq(int64_t *d, int64_t r, uint16_t mask)
{
    mergemask_uq((uint64_t *)d, r, mask);
}

#define mergemask(D, R, M)        \
    _Generic(D,                   \
        uint8_t *: mergemask_ub,  \
        int8_t *: mergemask_sb,   \
        uint16_t *: mergemask_uh, \
        int16_t *: mergemask_sh,  \
        uint32_t *: mergemask_uw, \
        int32_t *: mergemask_sw,  \
        uint64_t *: mergemask_uq, \
        int64_t *: mergemask_sq)(D, R, M)

#define DO_2OP(OP, ESIZE, TYPE, FN)                                               \
    void HELPER(glue(mve_, OP))(CPUState * env, void *vd, void *vn, void *vm)     \
    {                                                                             \
        TYPE *d = vd, *n = vn, *m = vm;                                           \
        uint16_t mask = mve_element_mask(env);                                    \
        unsigned e;                                                               \
        for(e = 0; e < 16 / ESIZE; e++, mask >>= ESIZE) {                         \
            mergemask(&d[H##ESIZE(e)], FN(n[H##ESIZE(e)], m[H##ESIZE(e)]), mask); \
        }                                                                         \
        mve_advance_vpt(env);                                                     \
    }

/* provide unsigned 2-op helpers for all sizes */
#define DO_2OP_U(OP, FN)           \
    DO_2OP(OP##b, 1, uint8_t, FN)  \
    DO_2OP(OP##h, 2, uint16_t, FN) \
    DO_2OP(OP##w, 4, uint32_t, FN)

DO_2OP_U(vadd, DO_ADD)
DO_2OP_U(vsub, DO_SUB)
DO_2OP_U(vmul, DO_MUL)

/* For loads, predicated lanes are zeroed instead of keeping their old values */
#define DO_VLDR(OP, TYPE, ESIZE, MSIZE, LD_TYPE)                                                 \
    void HELPER(glue(mve_, OP))(CPUState * env, void *vd, uint32_t addr)                         \
    {                                                                                            \
        TYPE *d = vd;                                                                            \
        uint16_t mask = mve_element_mask(env);                                                   \
        uint16_t eci_mask = mve_eci_mask(env);                                                   \
        unsigned e;                                                                              \
        /*                                                                                       \
         * R_SXTM allows the dest reg to become UNKNOWN for abandoned                            \
         * beats so we don't care if we update part of the dest and                              \
         * then take an exception.                                                               \
         */                                                                                      \
        for(e = 0; e < (16 / ESIZE); e++) {                                                      \
            if(eci_mask & 1) {                                                                   \
                if(mask & 1) {                                                                   \
                    d[e] = __inner_##LD_TYPE##_err_mmu(addr, cpu_mmu_index(env), NULL, GETPC()); \
                } else {                                                                         \
                    d[e] = 0;                                                                    \
                }                                                                                \
            }                                                                                    \
            addr += MSIZE;                                                                       \
            mask >>= ESIZE;                                                                      \
            eci_mask >>= ESIZE;                                                                  \
        }                                                                                        \
        mve_advance_vpt(env);                                                                    \
    }

DO_VLDR(vldrb, uint8_t, 1, 1, ldb)
DO_VLDR(vldrh, uint16_t, 2, 2, ldw)
DO_VLDR(vldrw, uint32_t, 4, 4, ldl)

//  TODO(MVE): Signed loads won't work. They need to have the sign bit retained.
/* Widening loads, interpret as: load a byte to signed half-word */
DO_VLDR(vldrb_sh, int16_t, 1, 2, ldb)
DO_VLDR(vldrb_sw, int32_t, 1, 4, ldb)
DO_VLDR(vldrb_uh, uint16_t, 1, 2, ldb)
DO_VLDR(vldrb_uw, uint32_t, 1, 4, ldb)
DO_VLDR(vldrh_sw, int32_t, 2, 4, ldw)
DO_VLDR(vldrh_uw, uint32_t, 2, 4, ldw)

#define DO_VSTR(OP, TYPE, MSIZE, ESIZE, ST_TYPE)                                  \
    void HELPER(glue(mve_, OP))(CPUState * env, void *vd, uint32_t addr)          \
    {                                                                             \
        TYPE *d = vd;                                                             \
        uint16_t mask = mve_element_mask(env);                                    \
        unsigned e;                                                               \
        for(e = 0; e < (16 / ESIZE); e++) {                                       \
            if(mask & 1) {                                                        \
                __inner_##ST_TYPE##_mmu(addr, d[e], cpu_mmu_index(env), GETPC()); \
            }                                                                     \
            addr += MSIZE;                                                        \
            mask >>= ESIZE;                                                       \
        }                                                                         \
        mve_advance_vpt(env);                                                     \
    }

DO_VSTR(vstrb, uint8_t, 1, 1, stb)
DO_VSTR(vstrh, uint16_t, 2, 2, stw)
DO_VSTR(vstrw, uint32_t, 4, 4, stl)

/* Narrowing stores, interpret as: store half-word in a byte */
DO_VSTR(vstrb_h, int16_t, 1, 2, stb)
DO_VSTR(vstrb_w, int32_t, 1, 4, stb)
DO_VSTR(vstrh_w, int32_t, 2, 4, stw)

#undef DO_VSTR
#undef DO_VLDR

#define DO_VLD4B(OP, O1, O2, O3, O4)                                          \
    void glue(gen_mve_, OP)(DisasContext * s, uint32_t qnindx, TCGv_i32 base) \
    {                                                                         \
        TCGv_i32 data;                                                        \
        TCGv_i32 addr = tcg_temp_new_i32();                                   \
        uint16_t mask = mve_eci_mask(env);                                    \
        static const uint8_t off[4] = { O1, O2, O3, O4 };                     \
        uint32_t e, qn_offset, beat;                                          \
        for(beat = 0; beat < 4; beat++, mask >>= 4) {                         \
            if((mask & 1) == 0) {                                             \
                /* ECI says skip this beat */                                 \
                continue;                                                     \
            }                                                                 \
            tcg_gen_addi_i32(addr, base, off[beat] * 4);                      \
            data = gen_ld32(addr, context_to_mmu_index(s));                   \
            for(e = 0; e < 4; e++) {                                          \
                qn_offset = mve_qreg_offset(qnindx + e);                      \
                qn_offset += off[beat];                                       \
                tcg_gen_st8_i32(data, cpu_env, qn_offset);                    \
                tcg_gen_shri_i32(data, data, 8);                              \
            }                                                                 \
            tcg_temp_free_i32(data);                                          \
        }                                                                     \
        tcg_temp_free_i32(addr);                                              \
    }

DO_VLD4B(vld40b, 0, 1, 10, 11)
DO_VLD4B(vld41b, 2, 3, 12, 13)
DO_VLD4B(vld42b, 4, 5, 14, 15)
DO_VLD4B(vld43b, 6, 7, 8, 9)

#define DO_VLD4H(OP, O1, O2)                                                  \
    void glue(gen_mve_, OP)(DisasContext * s, uint32_t qnindx, TCGv_i32 base) \
    {                                                                         \
        TCGv_i32 data;                                                        \
        TCGv_i32 addr = tcg_temp_new_i32();                                   \
        uint16_t mask = mve_eci_mask(env);                                    \
        static const uint8_t off[4] = { O1, O1, O2, O2 };                     \
        uint32_t y, qn_offset, beat;                                          \
        /* y counts 0 2 0 2 */                                                \
        for(beat = 0, y = 0; beat < 4; beat++, mask >>= 4, y ^= 2) {          \
            if((mask & 1) == 0) {                                             \
                /* ECI says skip this beat */                                 \
                continue;                                                     \
            }                                                                 \
            tcg_gen_addi_i32(addr, base, off[beat] * 8 + (beat & 1) * 4);     \
            data = gen_ld32(addr, context_to_mmu_index(s));                   \
                                                                              \
            qn_offset = mve_qreg_offset(qnindx + y);                          \
            qn_offset += off[beat];                                           \
            tcg_gen_st16_i32(data, cpu_env, qn_offset);                       \
                                                                              \
            tcg_gen_shri_i32(data, data, 16);                                 \
                                                                              \
            qn_offset = mve_qreg_offset(qnindx + y + 1);                      \
            qn_offset += off[beat];                                           \
            tcg_gen_st16_i32(data, cpu_env, qn_offset);                       \
            tcg_temp_free_i32(data);                                          \
        }                                                                     \
        tcg_temp_free_i32(addr);                                              \
    }

DO_VLD4H(vld40h, 0, 5)
DO_VLD4H(vld41h, 1, 6)
DO_VLD4H(vld42h, 2, 7)
DO_VLD4H(vld43h, 3, 4)

#define DO_VLD4W(OP, O1, O2, O3, O4)                                          \
    void glue(gen_mve_, OP)(DisasContext * s, uint32_t qnindx, TCGv_i32 base) \
    {                                                                         \
        TCGv_i32 data;                                                        \
        TCGv_i32 addr = tcg_temp_new_i32();                                   \
        uint16_t mask = mve_eci_mask(env);                                    \
        static const uint8_t off[4] = { O1, O2, O3, O4 };                     \
        uint32_t y, qn_offset, beat;                                          \
        for(beat = 0; beat < 4; beat++, mask >>= 4) {                         \
            if((mask & 1) == 0) {                                             \
                /* ECI says skip this beat */                                 \
                continue;                                                     \
            }                                                                 \
            tcg_gen_addi_i32(addr, base, off[beat] * 4);                      \
            data = gen_ld32(addr, context_to_mmu_index(s));                   \
            y = (beat + (O1 & 2)) & 3;                                        \
            qn_offset = mve_qreg_offset(qnindx + y);                          \
            /* Align the offset to 4  */                                      \
            qn_offset += off[beat] & ~3;                                      \
            tcg_gen_st_i32(data, cpu_env, qn_offset);                         \
            tcg_temp_free_i32(data);                                          \
        }                                                                     \
        tcg_temp_free_i32(addr);                                              \
    }

DO_VLD4W(vld40w, 0, 1, 10, 11)
DO_VLD4W(vld41w, 2, 3, 12, 13)
DO_VLD4W(vld42w, 4, 5, 14, 15)
DO_VLD4W(vld43w, 6, 7, 8, 9)

#undef DO_VLD4W
#undef DO_VLD4H
#undef DO_VLD4B

#define DO_2OP_FP_SCALAR(OP, ESIZE, TYPE, FN)                                    \
    void HELPER(glue(mve_, OP))(CPUState * env, void *vd, void *vn, uint32_t rm) \
    {                                                                            \
        TYPE *d = vd, *n = vn;                                                   \
        TYPE r, m = rm;                                                          \
        uint16_t mask = mve_element_mask(env);                                   \
        unsigned e;                                                              \
        float_status *fpst;                                                      \
        float_status scratch_fpst;                                               \
        for(e = 0; e < 16 / ESIZE; e++, mask >>= ESIZE) {                        \
            if((mask & MAKE_64BIT_MASK(0, ESIZE)) == 0) {                        \
                continue;                                                        \
            }                                                                    \
            fpst = ESIZE == 2 ? &env->vfp.fp_status_f16 : &env->vfp.fp_status;   \
            if(!(mask & 1)) {                                                    \
                /* We need the result but without updating flags */              \
                scratch_fpst = *fpst;                                            \
                fpst = &scratch_fpst;                                            \
            }                                                                    \
            r = FN(n[e], m, fpst);                                               \
            mergemask(&d[e], r, mask);                                           \
        }                                                                        \
        mve_advance_vpt(env);                                                    \
    }

DO_2OP_FP_SCALAR(vfadd_scalars, 4, float32, float32_add)
DO_2OP_FP_SCALAR(vfsub_scalars, 4, float32, float32_sub)
DO_2OP_FP_SCALAR(vfmul_scalars, 4, float32, float32_mul)

#define DO_2OP_FP(OP, ESIZE, TYPE, FN)                                                           \
    void HELPER(glue(mve_, OP))(CPUState * env, void *vd, void *vn, void *vm)                    \
    {                                                                                            \
        TYPE *d = vd, *n = vn, *m = vm;                                                          \
        TYPE r;                                                                                  \
        uint16_t mask = mve_element_mask(env);                                                   \
        unsigned e;                                                                              \
        float_status *fpst;                                                                      \
        float_status scratch_fpst;                                                               \
        for(e = 0; e < 16 / ESIZE; e++, mask >>= ESIZE) {                                        \
            if((mask & MAKE_64BIT_MASK(0, ESIZE)) == 0) {                                        \
                continue;                                                                        \
            }                                                                                    \
            fpst = ESIZE == 2 ? &env->vfp.standard_fp_status_f16 : &env->vfp.standard_fp_status; \
            if(!(mask & 1)) {                                                                    \
                /* We need the result but without updating flags */                              \
                scratch_fpst = *fpst;                                                            \
                fpst = &scratch_fpst;                                                            \
            }                                                                                    \
            r = FN(n[e], m[e], fpst);                                                            \
            mergemask(&d[e], r, mask);                                                           \
        }                                                                                        \
        mve_advance_vpt(env);                                                                    \
    }

DO_2OP_FP(vfadds, 4, float32, float32_add)
DO_2OP_FP(vfsubs, 4, float32, float32_sub)
DO_2OP_FP(vfmuls, 4, float32, float32_mul)
DO_2OP_FP(vmaxnms, 4, float32, float32_maxnum)
DO_2OP_FP(vminnms, 4, float32, float32_minnum)

static inline float32 float32_maxnuma(float32 a, float32 b, float_status *s)
{
    return float32_maxnum(float32_abs(a), float32_abs(b), s);
}

static inline float32 float32_minnuma(float32 a, float32 b, float_status *s)
{
    return float32_minnum(float32_abs(a), float32_abs(b), s);
}

DO_2OP_FP(vmaxnmas, 4, float32, float32_maxnuma)
DO_2OP_FP(vminnmas, 4, float32, float32_minnuma)

#define DO_2OP_FP_ACC_SCALAR(OP, ESIZE, TYPE, FN)                                \
    void HELPER(glue(mve_, OP))(CPUState * env, void *vd, void *vn, uint32_t rm) \
    {                                                                            \
        TYPE *d = vd, *n = vn;                                                   \
        TYPE r, m = rm;                                                          \
        uint16_t mask = mve_element_mask(env);                                   \
        unsigned e;                                                              \
        float_status *fpst;                                                      \
        float_status scratch_fpst;                                               \
        for(e = 0; e < 16 / ESIZE; e++, mask >>= ESIZE) {                        \
            if((mask & MAKE_64BIT_MASK(0, ESIZE)) == 0) {                        \
                continue;                                                        \
            }                                                                    \
            fpst = ESIZE == 2 ? &env->vfp.fp_status_f16 : &env->vfp.fp_status;   \
            if(!(mask & 1)) {                                                    \
                /* We need the result but without updating flags */              \
                scratch_fpst = *fpst;                                            \
                fpst = &scratch_fpst;                                            \
            }                                                                    \
            r = FN(n[e], m, d[e], 0, fpst);                                      \
            mergemask(&d[e], r, mask);                                           \
        }                                                                        \
        mve_advance_vpt(env);                                                    \
    }

/* VFMAS is vector * vector + scalar, so swap op2 and op3 */
#define DO_VFMAS_SCALARS(N, M, D, F, S) float32_muladd(N, D, M, F, S)

DO_2OP_FP_ACC_SCALAR(vfma_scalars, 4, float32, float32_muladd)
DO_2OP_FP_ACC_SCALAR(vfmas_scalars, 4, float32, DO_VFMAS_SCALARS)

#undef DO_2OP_FP_ACC_SCALAR
#undef DO_VFMAS_SCALARS
#undef DO_2OP_FP

void HELPER(mve_vdup)(CPUState *env, void *vd, uint32_t val)
{
    /*
     * The generated code already replicated an 8 or 16 bit constant
     * into the 32-bit value, so we only need to write the 32-bit
     * value to all elements of the Qreg, allowing for predication.
     */
    uint32_t *d = vd;
    uint16_t mask = mve_element_mask(env);
    unsigned e;
    for(e = 0; e < 16 / 4; e++, mask >>= 4) {
        mergemask(&d[e], val, mask);
    }
    mve_advance_vpt(env);
}

/*
 * VCTP: P0 unexecuted bits unchanged, predicated bits zeroed,
 * otherwise set according to value of Rn. The calculation of
 * newmask here works in the same way as the calculation of the
 * ltpmask in mve_element_mask(), but we have pre-calculated
 * the masklen in the generated code.
 */
void HELPER(mve_vctp)(CPUState *env, uint32_t masklen)
{
    uint16_t mask = mve_element_mask(env);
    uint16_t eci_mask = mve_eci_mask(env);
    uint16_t newmask;

    assert(masklen <= 16);
    newmask = masklen ? MAKE_64BIT_MASK(0, masklen) : 0;
    newmask &= mask;
    env->v7m.vpr = (env->v7m.vpr & ~(uint32_t)eci_mask) | (newmask & eci_mask);
    mve_advance_vpt(env);
}

void gen_mve_vpst(DisasContext *s, uint32_t mask)
{
    /*
     * Set the VPR mask fields. We take advantage of MASK01 and MASK23
     * being adjacent fields in the register.
     *
     * Updating the masks is not predicated, but it is subject to beat-wise
     * execution, and the mask is updated on the odd-numbered beats.
     * So if PSR.ECI says we should skip beat 1, we mustn't update the
     * 01 mask field.
     */
    TCGv_i32 vpr = load_cpu_field(v7m.vpr);
    TCGv_i32 m = tcg_temp_new_i32();

    switch(s->eci) {
        case ECI_NONE:
        case ECI_A0:
            tcg_gen_movi_i32(m, mask | (mask << 4));
            tcg_gen_deposit_i32(vpr, vpr, m, __REGISTER_V7M_VPR_MASK01_START,
                                __REGISTER_V7M_VPR_MASK01_WIDTH + __REGISTER_V7M_VPR_MASK23_WIDTH);
            break;
        case ECI_A0A1:
        case ECI_A0A1A2:
        case ECI_A0A1A2B0:
            /* Update only the 23 mask field */
            tcg_gen_movi_i32(m, mask);
            tcg_gen_deposit_i32(vpr, vpr, m, __REGISTER_V7M_VPR_MASK23_START, __REGISTER_V7M_VPR_MASK23_WIDTH);
            break;
        default:
            g_assert_not_reached();
    }
    tcg_temp_free_i32(m);
    store_cpu_field(vpr, v7m.vpr);
}

/* FP compares; note that all comparisons signal InvalidOp for QNaNs */
#define DO_VCMP_FP(OP, ESIZE, TYPE, FN)                                              \
    void HELPER(glue(mve_, OP))(CPUState * env, void *vn, void *vm)                  \
    {                                                                                \
        TYPE *n = vn, *m = vm;                                                       \
        uint16_t mask = mve_element_mask(env);                                       \
        uint16_t eci_mask = mve_eci_mask(env);                                       \
        uint16_t beatpred = 0;                                                       \
        uint16_t emask = MAKE_64BIT_MASK(0, ESIZE);                                  \
        unsigned e;                                                                  \
        float_status *fpst;                                                          \
        float_status scratch_fpst;                                                   \
        bool r;                                                                      \
        for(e = 0; e < 16 / ESIZE; e++, emask <<= ESIZE) {                           \
            if((mask & emask) == 0) {                                                \
                continue;                                                            \
            }                                                                        \
            fpst = ESIZE == 2 ? &env->vfp.fp_status_f16 : &env->vfp.fp_status;       \
            if(!(mask & (1 << (e * ESIZE)))) {                                       \
                /* We need the result but without updating flags */                  \
                scratch_fpst = *fpst;                                                \
                fpst = &scratch_fpst;                                                \
            }                                                                        \
            r = FN(n[e], m[e], fpst);                                                \
            /* Comparison sets 0/1 bits for each byte in the element */              \
            beatpred |= r * emask;                                                   \
        }                                                                            \
        beatpred &= mask;                                                            \
        env->v7m.vpr = (env->v7m.vpr & ~(uint32_t)eci_mask) | (beatpred & eci_mask); \
        mve_advance_vpt(env);                                                        \
    }

#define DO_VCMP_FP_SCALAR(OP, ESIZE, TYPE, FN)                                       \
    void HELPER(glue(mve_, OP))(CPUState * env, void *vn, uint32_t rm)               \
    {                                                                                \
        TYPE *n = vn;                                                                \
        uint16_t mask = mve_element_mask(env);                                       \
        uint16_t eci_mask = mve_eci_mask(env);                                       \
        uint16_t beatpred = 0;                                                       \
        uint16_t emask = MAKE_64BIT_MASK(0, ESIZE);                                  \
        unsigned e;                                                                  \
        float_status *fpst;                                                          \
        float_status scratch_fpst;                                                   \
        bool r;                                                                      \
        for(e = 0; e < 16 / ESIZE; e++, emask <<= ESIZE) {                           \
            if((mask & emask) == 0) {                                                \
                continue;                                                            \
            }                                                                        \
            fpst = ESIZE == 2 ? &env->vfp.fp_status_f16 : &env->vfp.fp_status;       \
            if(!(mask & (1 << (e * ESIZE)))) {                                       \
                /* We need the result but without updating flags */                  \
                scratch_fpst = *fpst;                                                \
                fpst = &scratch_fpst;                                                \
            }                                                                        \
            r = FN(n[e], (float32)rm, fpst);                                         \
            /* Comparison sets 0/1 bits for each byte in the element */              \
            beatpred |= r * emask;                                                   \
        }                                                                            \
        beatpred &= mask;                                                            \
        env->v7m.vpr = (env->v7m.vpr & ~(uint32_t)eci_mask) | (beatpred & eci_mask); \
                                                                                     \
        mve_advance_vpt(env);                                                        \
    }

#define DO_VCMP_FP_BOTH(VOP, SOP, ESIZE, TYPE, FN) \
    DO_VCMP_FP(VOP, ESIZE, TYPE, FN)               \
    DO_VCMP_FP_SCALAR(SOP, ESIZE, TYPE, FN)

/*
 * Some care is needed here to get the correct result for the unordered case.
 * Architecturally EQ, GE and GT are defined to be false for unordered, but
 * the NE, LT and LE comparisons are defined as simple logical inverses of
 * EQ, GE and GT and so they must return true for unordered. The softfloat
 * comparison functions float*_{eq,le,lt} all return false for unordered.
 */
#define DO_GE16(X, Y, S) float16_le(Y, X, S)
#define DO_GE32(X, Y, S) float32_le(Y, X, S)
#define DO_GT16(X, Y, S) float16_lt(Y, X, S)
#define DO_GT32(X, Y, S) float32_lt(Y, X, S)

DO_VCMP_FP_BOTH(vfcmp_eqs, vfcmp_eq_scalars, 4, float32, float32_eq)
DO_VCMP_FP_BOTH(vfcmp_nes, vfcmp_ne_scalars, 4, float32, !float32_eq)
DO_VCMP_FP_BOTH(vfcmp_ges, vfcmp_ge_scalars, 4, float32, DO_GE32)
DO_VCMP_FP_BOTH(vfcmp_lts, vfcmp_lt_scalars, 4, float32, !DO_GE32)
DO_VCMP_FP_BOTH(vfcmp_gts, vfcmp_gt_scalars, 4, float32, DO_GT32)
DO_VCMP_FP_BOTH(vfcmp_les, vfcmp_le_scalars, 4, float32, !DO_GT32)

#undef DO_GT32
#undef DO_GT16
#undef DO_GE32
#undef DO_GE16
#undef DO_VCMP_FP_BOTH
#undef DO_VCMP_FP_SCALAR
#undef DO_VCMP_FP

#define DO_VIDUP(OP, ESIZE, TYPE, FN)                                                        \
    uint32_t HELPER(glue(mve_, OP))(CPUState * env, void *vd, uint32_t offset, uint32_t imm) \
    {                                                                                        \
        TYPE *d = vd;                                                                        \
        uint16_t mask = mve_element_mask(env);                                               \
        unsigned e;                                                                          \
        for(e = 0; e < 16 / ESIZE; e++, mask >>= ESIZE) {                                    \
            mergemask(&d[e], offset, mask);                                                  \
            offset = FN(offset, imm);                                                        \
        }                                                                                    \
        mve_advance_vpt(env);                                                                \
        return offset;                                                                       \
    }

#define DO_VIWDUP(OP, ESIZE, TYPE, FN)                                                                      \
    uint32_t HELPER(glue(mve_, OP))(CPUState * env, void *vd, uint32_t offset, uint32_t wrap, uint32_t imm) \
    {                                                                                                       \
        TYPE *d = vd;                                                                                       \
        uint16_t mask = mve_element_mask(env);                                                              \
        unsigned e;                                                                                         \
        for(e = 0; e < 16 / ESIZE; e++, mask >>= ESIZE) {                                                   \
            mergemask(&d[e], offset, mask);                                                                 \
            offset = FN(offset, wrap, imm);                                                                 \
        }                                                                                                   \
        mve_advance_vpt(env);                                                                               \
        return offset;                                                                                      \
    }

#define DO_VIDUP_ALL(OP, FN)        \
    DO_VIDUP(OP##b, 1, int8_t, FN)  \
    DO_VIDUP(OP##h, 2, int16_t, FN) \
    DO_VIDUP(OP##w, 4, int32_t, FN)

#define DO_VIWDUP_ALL(OP, FN)        \
    DO_VIWDUP(OP##b, 1, int8_t, FN)  \
    DO_VIWDUP(OP##h, 2, int16_t, FN) \
    DO_VIWDUP(OP##w, 4, int32_t, FN)

static uint32_t do_add_wrap(uint32_t offset, uint32_t wrap, uint32_t imm)
{
    offset += imm;
    if(offset == wrap) {
        offset = 0;
    }
    return offset;
}

static uint32_t do_sub_wrap(uint32_t offset, uint32_t wrap, uint32_t imm)
{
    if(offset == 0) {
        offset = wrap;
    }
    offset -= imm;
    return offset;
}

DO_VIDUP_ALL(vidup, DO_ADD)
DO_VIWDUP_ALL(viwdup, do_add_wrap)
DO_VIWDUP_ALL(vdwdup, do_sub_wrap)

#define DO_VMAXMINV(OP, ESIZE, TYPE, RATYPE, FN)                              \
    uint32_t HELPER(glue(mve_, OP))(CPUState * env, void *vm, uint32_t ra_in) \
    {                                                                         \
        uint16_t mask = mve_element_mask(env);                                \
        unsigned e;                                                           \
        TYPE *m = vm;                                                         \
        int64_t ra = (RATYPE)ra_in;                                           \
        for(e = 0; e < (16 / ESIZE); e++) {                                   \
            if(mask & 1) {                                                    \
                ra = FN(ra, m[e]);                                            \
            }                                                                 \
            mask >>= ESIZE;                                                   \
        }                                                                     \
        mve_advance_vpt(env);                                                 \
        return ra;                                                            \
    }

#define DO_VMAXMINV_U(INSN, FN)                     \
    DO_VMAXMINV(INSN##b, 1, uint8_t, uint8_t, FN)   \
    DO_VMAXMINV(INSN##h, 2, uint16_t, uint16_t, FN) \
    DO_VMAXMINV(INSN##w, 4, uint32_t, uint32_t, FN)
#define DO_VMAXMINV_S(INSN, FN)                   \
    DO_VMAXMINV(INSN##b, 1, int8_t, int8_t, FN)   \
    DO_VMAXMINV(INSN##h, 2, int16_t, int16_t, FN) \
    DO_VMAXMINV(INSN##w, 4, int32_t, int32_t, FN)

/*
 * Helpers for max and min of absolute values across vector:
 * note that we only take the absolute value of 'm', not 'n'
 */
static int64_t do_maxa(int64_t n, int64_t m)
{
    if(m < 0) {
        m = -m;
    }
    return MAX(n, m);
}

static int64_t do_mina(int64_t n, int64_t m)
{
    if(m < 0) {
        m = -m;
    }
    return MIN(n, m);
}

DO_VMAXMINV_S(vmaxvs, DO_MAX)
DO_VMAXMINV_U(vmaxvu, DO_MAX)
DO_VMAXMINV_S(vminvs, DO_MIN)
DO_VMAXMINV_U(vminvu, DO_MIN)
/*
 * VMAXAV, VMINAV treat the general purpose input as unsigned
 * and the vector elements as signed.
 */
DO_VMAXMINV(vmaxavb, 1, int8_t, uint8_t, do_maxa)
DO_VMAXMINV(vmaxavh, 2, int16_t, uint16_t, do_maxa)
DO_VMAXMINV(vmaxavw, 4, int32_t, uint32_t, do_maxa)
DO_VMAXMINV(vminavb, 1, int8_t, uint8_t, do_mina)
DO_VMAXMINV(vminavh, 2, int16_t, uint16_t, do_mina)
DO_VMAXMINV(vminavw, 4, int32_t, uint32_t, do_mina)

#define float32_silence_nan(a, fpst) float32_maybe_silence_nan(a, fpst)

#define DO_FP_VMAXMINV(OP, ESIZE, TYPE, ABS, FN)                                         \
    uint32_t HELPER(glue(mve_, OP))(CPUState * env, void *vm, uint32_t ra_in)            \
    {                                                                                    \
        uint16_t mask = mve_element_mask(env);                                           \
        unsigned e;                                                                      \
        TYPE *m = vm;                                                                    \
        TYPE ra = (TYPE)ra_in;                                                           \
        float_status *fpst = ESIZE == 2 ? &env->vfp.fp_status_f16 : &env->vfp.fp_status; \
        for(e = 0; e < (16 / ESIZE); e++) {                                              \
            if(mask & 1) {                                                               \
                TYPE v = m[e];                                                           \
                if(glue(TYPE, _is_signaling_nan)(ra, fpst)) {                            \
                    ra = glue(TYPE, _silence_nan)(ra, fpst);                             \
                    float_raise(float_flag_invalid, fpst);                               \
                }                                                                        \
                if(glue(TYPE, _is_signaling_nan)(v, fpst)) {                             \
                    v = glue(TYPE, _silence_nan)(v, fpst);                               \
                    float_raise(float_flag_invalid, fpst);                               \
                }                                                                        \
                if(ABS) {                                                                \
                    v = glue(TYPE, _abs)(v);                                             \
                }                                                                        \
                ra = FN(ra, v, fpst);                                                    \
            }                                                                            \
            mask >>= ESIZE;                                                              \
        }                                                                                \
        mve_advance_vpt(env);                                                            \
        return ra;                                                                       \
    }

DO_FP_VMAXMINV(vmaxnmvs, 4, float32, false, float32_maxnum)
DO_FP_VMAXMINV(vminnmvs, 4, float32, false, float32_minnum)
DO_FP_VMAXMINV(vmaxnmavs, 4, float32, true, float32_maxnum)
DO_FP_VMAXMINV(vminnmavs, 4, float32, true, float32_minnum)

void HELPER(mve_vpsel)(CPUState *env, void *vd, void *vn, void *vm)
{
    /*
     * Qd[n] = VPR.P0[n] ? Qn[n] : Qm[n]
     * but note that whether bytes are written to Qd is still subject
     * to (all forms of) predication in the usual way.
     */
    uint64_t *d = vd, *n = vn, *m = vm;
    uint16_t mask = mve_element_mask(env);
    uint16_t p0 = FIELD_EX32(env->v7m.vpr, V7M_VPR, P0);
    unsigned e;
    for(e = 0; e < 16 / 8; e++, mask >>= 8, p0 >>= 8) {
        uint64_t r = m[e];
        mergemask(&r, n[e], p0);
        mergemask(&d[e], r, mask);
    }
    mve_advance_vpt(env);
}

#define DO_VCMULH(N, M, S) float16_mul((N), (M), (S))
#define DO_VCMULS(N, M, S) float32_mul((N), (M), (S))

#define DO_VCMLA(OP, ESIZE, TYPE, ROT, FN)                                      \
    void HELPER(glue(mve_, OP))(CPUState * env, void *vd, void *vn, void *vm)   \
    {                                                                           \
        TYPE *d = vd, *n = vn, *m = vm;                                         \
        TYPE r0, r1;                                                            \
        uint16_t mask = mve_element_mask(env);                                  \
        unsigned e;                                                             \
        float_status *fpst0, *fpst1;                                            \
        float_status scratch_fpst;                                              \
        /* We loop through pairs of elements at a time */                       \
        for(e = 0; e < 16 / ESIZE; e += 2, mask >>= ESIZE * 2) {                \
            if((mask & MAKE_64BIT_MASK(0, ESIZE * 2)) == 0) {                   \
                continue;                                                       \
            }                                                                   \
            fpst0 = ESIZE == 2 ? &env->vfp.fp_status_f16 : &env->vfp.fp_status; \
            fpst1 = fpst0;                                                      \
            if(!(mask & 1)) {                                                   \
                scratch_fpst = *fpst0;                                          \
                fpst0 = &scratch_fpst;                                          \
            }                                                                   \
            if(!(mask & (1 << ESIZE))) {                                        \
                scratch_fpst = *fpst1;                                          \
                fpst1 = &scratch_fpst;                                          \
            }                                                                   \
            switch(ROT) {                                                       \
                case 0:                                                         \
                    r0 = FN(n[e], m[e], fpst0);                                 \
                    r1 = FN(n[e], m[e + 1], fpst1);                             \
                    break;                                                      \
                case 1:                                                         \
                    r0 = FN(glue(TYPE, _chs)(n[e + 1]), m[e + 1], fpst0);       \
                    r1 = FN(n[e + 1], m[e], fpst1);                             \
                    break;                                                      \
                case 2:                                                         \
                    r0 = FN(glue(TYPE, _chs)(n[e]), m[e], fpst0);               \
                    r1 = FN(glue(TYPE, _chs)(n[e]), m[e + 1], fpst1);           \
                    break;                                                      \
                case 3:                                                         \
                    r0 = FN(n[e + 1], m[e + 1], fpst0);                         \
                    r1 = FN(glue(TYPE, _chs)(n[e + 1]), m[e], fpst1);           \
                    break;                                                      \
                default:                                                        \
                    g_assert_not_reached();                                     \
            }                                                                   \
            mergemask(&d[e], r0, mask);                                         \
            mergemask(&d[e + 1], r1, mask >> ESIZE);                            \
        }                                                                       \
        mve_advance_vpt(env);                                                   \
    }

DO_VCMLA(vcmul0s, 4, float32, 0, DO_VCMULS)
DO_VCMLA(vcmul90s, 4, float32, 1, DO_VCMULS)
DO_VCMLA(vcmul180s, 4, float32, 2, DO_VCMULS)
DO_VCMLA(vcmul270s, 4, float32, 3, DO_VCMULS)

#undef DO_VCMLA
#undef DO_VCMULH
#undef DO_VCMULS

#define DO_1OP(OP, ESIZE, TYPE, FN)                                 \
    void HELPER(glue(mve_, OP))(CPUState * env, void *vd, void *vm) \
    {                                                               \
        TYPE *d = vd, *m = vm;                                      \
        uint16_t mask = mve_element_mask(env);                      \
        unsigned e;                                                 \
        for(e = 0; e < 16 / ESIZE; e++, mask >>= ESIZE) {           \
            mergemask(&d[e], FN(m[e]), mask);                       \
        }                                                           \
        mve_advance_vpt(env);                                       \
    }

DO_1OP(vclzb, 1, uint8_t, clz_u8)
DO_1OP(vclzh, 2, uint16_t, clz_u16)
DO_1OP(vclzw, 4, uint32_t, __builtin_clz)

DO_1OP(vclsb, 1, int8_t, cls_s8)
DO_1OP(vclsh, 2, int16_t, cls_s16)
DO_1OP(vclsw, 4, int32_t, __builtin_clrsb)

DO_1OP(vabsb, 1, int8_t, DO_ABS)
DO_1OP(vabsh, 2, int16_t, DO_ABS)
DO_1OP(vabsw, 4, int32_t, DO_ABS)

#define DO_FABSH(N) ((N) & dup_const(MO_16, 0x7fff))
#define DO_FABSS(N) ((N) & dup_const(MO_32, 0x7fffffff))
DO_1OP(vfabsh, 8, uint64_t, DO_FABSH)
DO_1OP(vfabss, 8, uint64_t, DO_FABSS)
#undef DO_FABSS
#undef DO_FABSH

DO_1OP(vnegb, 1, int8_t, DO_NEG)
DO_1OP(vnegh, 2, int16_t, DO_NEG)
DO_1OP(vnegw, 4, int32_t, DO_NEG)

#define DO_FNEGH(N) ((N) ^ dup_const(MO_16, 0x8000))
#define DO_FNEGS(N) ((N) ^ dup_const(MO_32, 0x80000000))
DO_1OP(vfnegh, 8, uint64_t, DO_FNEGH)
DO_1OP(vfnegs, 8, uint64_t, DO_FNEGS)
#undef DO_FABSS
#undef DO_FABSH

#define DO_VMAXMINA(OP, ESIZE, STYPE, UTYPE, FN)                    \
    void HELPER(glue(mve_, OP))(CPUState * env, void *vd, void *vm) \
    {                                                               \
        UTYPE *d = vd;                                              \
        STYPE *m = vm;                                              \
        uint16_t mask = mve_element_mask(env);                      \
        unsigned e;                                                 \
        for(e = 0; e < 16 / ESIZE; e++, mask >>= ESIZE) {           \
            UTYPE r = DO_ABS(m[e]);                                 \
            r = FN(d[e], r);                                        \
            mergemask(&d[e], r, mask);                              \
        }                                                           \
        mve_advance_vpt(env);                                       \
    }

DO_VMAXMINA(vmaxab, 1, int8_t, uint8_t, DO_MAX)
DO_VMAXMINA(vmaxah, 2, int16_t, uint16_t, DO_MAX)
DO_VMAXMINA(vmaxaw, 4, int32_t, uint32_t, DO_MAX)
DO_VMAXMINA(vminab, 1, int8_t, uint8_t, DO_MIN)
DO_VMAXMINA(vminah, 2, int16_t, uint16_t, DO_MIN)
DO_VMAXMINA(vminaw, 4, int32_t, uint32_t, DO_MIN)
#undef DO_VMAXMINA

static inline uint32_t hswap32(uint32_t h)
{
    return rol32(h, 16);
}

static inline uint64_t hswap64(uint64_t h)
{
    uint64_t m = 0x0000ffff0000ffffull;
    h = rol64(h, 32);
    return ((h & m) << 16) | ((h >> 16) & m);
}

static inline uint64_t wswap64(uint64_t h)
{
    return rol64(h, 32);
}

DO_1OP(vrev16b, 2, uint16_t, bswap16)
DO_1OP(vrev32b, 4, uint32_t, bswap32)
DO_1OP(vrev32h, 4, uint32_t, hswap32)
DO_1OP(vrev64b, 8, uint64_t, bswap64)
DO_1OP(vrev64h, 8, uint64_t, hswap64)
DO_1OP(vrev64w, 8, uint64_t, wswap64)

#undef DO_1OP

#endif

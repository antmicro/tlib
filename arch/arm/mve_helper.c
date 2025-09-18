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

#endif

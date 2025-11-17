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

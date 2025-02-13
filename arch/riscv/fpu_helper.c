/*
 *  RISC-V FPU emulation helpers
 *
 *  Author: Sagar Karandikar, sagark@eecs.berkeley.edu
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

/*
 * Parts of this code are derived from Spike https://github.com/riscv-software-src/riscv-isa-sim,
 * under the following license:
 */

/*============================================================================

This C source file is part of the SoftFloat IEEE Floating-Point Arithmetic
Package, Release 3d, by John R. Hauser.

Copyright 2011, 2012, 2013, 2014, 2015, 2016 The Regents of the University of
California.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions, and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions, and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

 3. Neither the name of the University nor the names of its contributors may
    be used to endorse or promote products derived from this software without
    specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS", AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ARE
DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=============================================================================*/

#include "cpu.h"

/* convert RISC-V rounding mode to IEEE library numbers */
unsigned int ieee_rm[] = { float_round_nearest_even, float_round_to_zero, float_round_down, float_round_up,
                           float_round_ties_away };

typedef enum {
    riscv_float_round_nearest_even = 0,
    riscv_float_round_to_zero = 1,
    riscv_float_round_down = 2,
    riscv_float_round_up = 3,
    riscv_float_round_ties_away = 4
} riscv_float_round_mode;

/* convert RISC-V Vector Fixed-Point Rounding Mode to IEEE library numbers */
unsigned int ieee_vxrm[] = { float_round_up, float_round_nearest_even, float_round_down };

/* obtain rm value to use in computation
 * as the last step, convert rm codes to what the softfloat library expects
 * Adapted from Spike's decode.h:RM
 */
#define RM                                                             \
    ({                                                                 \
        if(rm == 7) {                                                  \
            rm = env->frm;                                             \
        }                                                              \
        if(rm > 4) {                                                   \
            raise_exception_and_sync_pc(env, RISCV_EXCP_ILLEGAL_INST); \
        }                                                              \
        ieee_rm[rm];                                                   \
    })

#define require_fp                                                 \
    if(!(env->mstatus & MSTATUS_FS)) {                             \
        raise_exception_and_sync_pc(env, RISCV_EXCP_ILLEGAL_INST); \
    }

/* adapted from spike */
#define isNaNF32UI(ui) (0xFF000000 < (uint32_t)((uint_fast32_t)ui << 1))
#define signF32UI(a)   ((bool)((uint32_t)a >> 31))
#define expF32UI(a)    ((int_fast16_t)(a >> 23) & 0xFF)
#define fracF32UI(a)   (a & 0x007FFFFF)

union ui32_f32 {
    uint32_t ui;
    uint32_t f;
};

#define isNaNF64UI(ui) (UINT64_C(0xFFE0000000000000) < (uint64_t)((uint_fast64_t)ui << 1))
#define signF64UI(a)   ((bool)((uint64_t)a >> 63))
#define expF64UI(a)    ((int_fast16_t)(a >> 52) & 0x7FF)
#define fracF64UI(a)   (a & UINT64_C(0x000FFFFFFFFFFFFF))

union ui64_f64 {
    uint64_t ui;
    uint64_t f;
};

/* convert softfloat library flag numbers to RISC-V */
unsigned int softfloat_flags_to_riscv(unsigned int flags)
{
    int rv_flags = 0;
    rv_flags |= (flags & float_flag_inexact) ? 1 : 0;
    rv_flags |= (flags & float_flag_underflow) ? 2 : 0;
    rv_flags |= (flags & float_flag_overflow) ? 4 : 0;
    rv_flags |= (flags & float_flag_divbyzero) ? 8 : 0;
    rv_flags |= (flags & float_flag_invalid) ? 16 : 0;
    return rv_flags;
}

/* adapted from Spike's decode.h:set_fp_exceptions */
#define set_fp_exceptions()                                                                  \
    do {                                                                                     \
        env->fflags |= softfloat_flags_to_riscv(get_float_exception_flags(&env->fp_status)); \
        set_float_exception_flags(0, &env->fp_status);                                       \
    } while(0)

uint64_t helper_fmadd_s(CPUState *env, uint64_t frs1, uint64_t frs2, uint64_t frs3, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float32_muladd(frs1, frs2, frs3, 0, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fmadd_d(CPUState *env, uint64_t frs1, uint64_t frs2, uint64_t frs3, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float64_muladd(frs1, frs2, frs3, 0, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fmadd_h(CPUState *env, uint64_t frs1, uint64_t frs2, uint64_t frs3, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);

    float16_t f1, f2, f3;
    f1.v = (uint16_t)frs1;
    f2.v = (uint16_t)frs2;
    f3.v = (uint16_t)frs3;
    frs1 = f16_mulAdd(f1, f2, f3).v;

    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fmsub_s(CPUState *env, uint64_t frs1, uint64_t frs2, uint64_t frs3, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float32_muladd(frs1, frs2, frs3 ^ (uint32_t)INT32_MIN, 0, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fmsub_d(CPUState *env, uint64_t frs1, uint64_t frs2, uint64_t frs3, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float64_muladd(frs1, frs2, frs3 ^ (uint64_t)INT64_MIN, 0, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fmsub_h(CPUState *env, uint64_t frs1, uint64_t frs2, uint64_t frs3, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);

    float16_t f1, f2, f3;
    f1.v = (uint16_t)frs1;
    f2.v = (uint16_t)frs2;
    f3.v = (uint16_t)(frs3 ^ (uint64_t)INT16_MIN);
    frs1 = f16_mulAdd(f1, f2, f3).v;

    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fnmsub_s(CPUState *env, uint64_t frs1, uint64_t frs2, uint64_t frs3, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float32_muladd(frs1 ^ (uint32_t)INT32_MIN, frs2, frs3, 0, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fnmsub_d(CPUState *env, uint64_t frs1, uint64_t frs2, uint64_t frs3, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float64_muladd(frs1 ^ (uint64_t)INT64_MIN, frs2, frs3, 0, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fnmsub_h(CPUState *env, uint64_t frs1, uint64_t frs2, uint64_t frs3, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);

    float16_t f1, f2, f3;
    f1.v = (uint16_t)(frs1 ^ (uint64_t)INT16_MIN);
    f2.v = (uint16_t)frs2;
    f3.v = (uint16_t)frs3;
    frs1 = f16_mulAdd(f1, f2, f3).v;

    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fnmadd_s(CPUState *env, uint64_t frs1, uint64_t frs2, uint64_t frs3, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float32_muladd(frs1 ^ (uint32_t)INT32_MIN, frs2, frs3 ^ (uint32_t)INT32_MIN, 0, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fnmadd_d(CPUState *env, uint64_t frs1, uint64_t frs2, uint64_t frs3, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float64_muladd(frs1 ^ (uint64_t)INT64_MIN, frs2, frs3 ^ (uint64_t)INT64_MIN, 0, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fnmadd_h(CPUState *env, uint64_t frs1, uint64_t frs2, uint64_t frs3, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);

    float16_t f1, f2, f3;
    f1.v = (uint16_t)(frs1 ^ (uint64_t)INT16_MIN);
    f2.v = (uint16_t)frs2;
    f3.v = (uint16_t)(frs3 ^ (uint64_t)INT16_MIN);
    frs1 = f16_mulAdd(f1, f2, f3).v;

    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fadd_s(CPUState *env, uint64_t frs1, uint64_t frs2, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float32_add(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fsub_s(CPUState *env, uint64_t frs1, uint64_t frs2, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float32_sub(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fmul_s(CPUState *env, uint64_t frs1, uint64_t frs2, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float32_mul(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fdiv_s(CPUState *env, uint64_t frs1, uint64_t frs2, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float32_div(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fmin_s(CPUState *env, uint64_t frs1, uint64_t frs2)
{
    require_fp;
    frs1 = float32_minnum(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fmax_s(CPUState *env, uint64_t frs1, uint64_t frs2)
{
    require_fp;
    frs1 = float32_maxnum(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fsqrt_s(CPUState *env, uint64_t frs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float32_sqrt(frs1, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

target_ulong helper_fle_s(CPUState *env, uint64_t frs1, uint64_t frs2)
{
    require_fp;
    frs1 = float32_le(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

target_ulong helper_flt_s(CPUState *env, uint64_t frs1, uint64_t frs2)
{
    require_fp;
    frs1 = float32_lt(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

target_ulong helper_fge_s(CPUState *env, uint64_t frs1, uint64_t frs2)
{
    return helper_fle_s(env, frs2, frs1);
}

target_ulong helper_fgt_s(CPUState *env, uint64_t frs1, uint64_t frs2)
{
    return helper_flt_s(env, frs2, frs1);
}

target_ulong helper_feq_s(CPUState *env, uint64_t frs1, uint64_t frs2)
{
    require_fp;
    frs1 = float32_eq_quiet(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

target_ulong helper_fcvt_hw_s(CPUState *env, uint64_t frs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float32_to_int16(frs1, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

target_ulong helper_fcvt_hwu_s(CPUState *env, uint64_t frs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = (int16_t)float32_to_uint16(frs1, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

target_ulong helper_fcvt_w_s(CPUState *env, uint64_t frs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float32_to_int32(frs1, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

target_ulong helper_fcvt_wu_s(CPUState *env, uint64_t frs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = (!isNaNF32UI(frs1) && signF32UI(frs1)) ? 0 : (int32_t)float32_to_uint32(frs1, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

uint64_t helper_fcvt_l_s(CPUState *env, uint64_t frs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float32_to_int64(frs1, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

uint64_t helper_fcvt_lu_s(CPUState *env, uint64_t frs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = (!isNaNF32UI(frs1) && signF32UI(frs1)) ? 0 : float32_to_uint64(frs1, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

uint64_t helper_fcvt_s_hw(CPUState *env, target_ulong rs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    rs1 = int32_to_float32((int16_t)rs1, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return rs1;
}

uint64_t helper_fcvt_s_hwu(CPUState *env, target_ulong rs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    rs1 = uint32_to_float32((uint16_t)rs1, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return rs1;
}

uint64_t helper_fcvt_s_w(CPUState *env, target_ulong rs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    rs1 = int32_to_float32((int32_t)rs1, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return rs1;
}

uint64_t helper_fcvt_s_wu(CPUState *env, target_ulong rs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    rs1 = uint32_to_float32((uint32_t)rs1, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return rs1;
}

uint64_t helper_fcvt_s_l(CPUState *env, uint64_t rs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    rs1 = int64_to_float32(rs1, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return rs1;
}

uint64_t helper_fcvt_s_lu(CPUState *env, uint64_t rs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    rs1 = uint64_to_float32(rs1, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return rs1;
}

uint32_t helper_fcvt_wu_s_rod(CPUState *env, uint32_t frs1)
{
    require_fp;
    frs1 = float32_to_uint32_rod(frs1, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

int32_t helper_fcvt_w_s_rod(CPUState *env, uint32_t frs1)
{
    require_fp;
    frs1 = float32_to_int32_rod(frs1, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

uint64_t helper_fcvt_lu_d_rod(CPUState *env, uint64_t frs1)
{
    require_fp;
    frs1 = float64_to_uint64_rod(frs1, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

int64_t helper_fcvt_l_d_rod(CPUState *env, uint64_t frs1)
{
    require_fp;
    frs1 = float64_to_int64_rod(frs1, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

uint64_t helper_fcvt_lu_s_rod(CPUState *env, uint32_t frs1)
{
    require_fp;
    frs1 = float32_to_uint64_rod(frs1, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

int64_t helper_fcvt_l_s_rod(CPUState *env, uint32_t frs1)
{
    require_fp;
    frs1 = float32_to_int64_rod(frs1, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

uint_fast16_t float32_classify(uint32_t a, float_status *status)
{
    union ui32_f32 uA;
    uint_fast32_t uiA;

    uA.f = a;
    uiA = uA.ui;

    uint_fast16_t infOrNaN = expF32UI(uiA) == 0xFF;
    uint_fast16_t subnormalOrZero = expF32UI(uiA) == 0;
    bool sign = signF32UI(uiA);

    // clang-format off
    return
        (sign && infOrNaN && fracF32UI(uiA) == 0)           << 0 |
        (sign && !infOrNaN && !subnormalOrZero)             << 1 |
        (sign && subnormalOrZero && fracF32UI(uiA))         << 2 |
        (sign && subnormalOrZero && fracF32UI(uiA) == 0)    << 3 |
        (!sign && infOrNaN && fracF32UI(uiA) == 0)          << 7 |
        (!sign && !infOrNaN && !subnormalOrZero)            << 6 |
        (!sign && subnormalOrZero && fracF32UI(uiA))        << 5 |
        (!sign && subnormalOrZero && fracF32UI(uiA) == 0)   << 4 |
        (isNaNF32UI(uiA) &&  float32_is_signaling_nan(uiA, status)) << 8 |
        (isNaNF32UI(uiA) && !float32_is_signaling_nan(uiA, status)) << 9;
    // clang-format on
}

target_ulong helper_fclass_s(CPUState *env, uint64_t frs1)
{
    require_fp;
    frs1 = float32_classify(frs1, &env->fp_status);
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fadd_d(CPUState *env, uint64_t frs1, uint64_t frs2, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float64_add(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fsub_d(CPUState *env, uint64_t frs1, uint64_t frs2, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float64_sub(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fmul_d(CPUState *env, uint64_t frs1, uint64_t frs2, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float64_mul(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fdiv_d(CPUState *env, uint64_t frs1, uint64_t frs2, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float64_div(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fmin_d(CPUState *env, uint64_t frs1, uint64_t frs2)
{
    require_fp;
    frs1 = float64_minnum(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fmax_d(CPUState *env, uint64_t frs1, uint64_t frs2)
{
    require_fp;
    frs1 = float64_maxnum(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fcvt_s_d(CPUState *env, uint64_t rs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    rs1 = float64_to_float32(rs1, &env->fp_status);
    set_fp_exceptions();
    return rs1;
}

uint64_t helper_fcvt_d_s(CPUState *env, uint64_t rs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    rs1 = float32_to_float64(rs1, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return rs1;
}

uint64_t helper_fsqrt_d(CPUState *env, uint64_t frs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float64_sqrt(frs1, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

target_ulong helper_fle_d(CPUState *env, uint64_t frs1, uint64_t frs2)
{
    require_fp;
    frs1 = float64_le(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

target_ulong helper_flt_d(CPUState *env, uint64_t frs1, uint64_t frs2)
{
    require_fp;
    frs1 = float64_lt(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

target_ulong helper_fge_d(CPUState *env, uint64_t frs1, uint64_t frs2)
{
    return helper_fle_d(env, frs2, frs1);
}

target_ulong helper_fgt_d(CPUState *env, uint64_t frs1, uint64_t frs2)
{
    return helper_flt_d(env, frs2, frs1);
}

target_ulong helper_feq_d(CPUState *env, uint64_t frs1, uint64_t frs2)
{
    require_fp;
    frs1 = float64_eq_quiet(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

target_ulong helper_fcvt_w_d(CPUState *env, uint64_t frs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = (int64_t)((int32_t)float64_to_int32(frs1, &env->fp_status));
    set_fp_exceptions();
    return frs1;
}

target_ulong helper_fcvt_wu_d(CPUState *env, uint64_t frs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = (!isNaNF64UI(frs1) && signF64UI(frs1)) ? 0 : (int64_t)((int32_t)float64_to_uint32(frs1, &env->fp_status));
    set_fp_exceptions();
    return frs1;
}

uint64_t helper_fcvt_l_d(CPUState *env, uint64_t frs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float64_to_int64(frs1, &env->fp_status);
    set_fp_exceptions();
    return frs1;
}

uint64_t helper_fcvt_lu_d(CPUState *env, uint64_t frs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = (!isNaNF64UI(frs1) && signF64UI(frs1)) ? 0 : float64_to_uint64(frs1, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fcvt_d_w(CPUState *env, target_ulong rs1, uint64_t rm)
{
    require_fp;
    uint64_t res;
    set_float_rounding_mode(RM, &env->fp_status);
    res = int32_to_float64((int32_t)rs1, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return res;
}

uint64_t helper_fcvt_d_wu(CPUState *env, target_ulong rs1, uint64_t rm)
{
    require_fp;
    uint64_t res;
    set_float_rounding_mode(RM, &env->fp_status);
    res = uint32_to_float64((uint32_t)rs1, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return res;
}

uint64_t helper_fcvt_d_l(CPUState *env, uint64_t rs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    rs1 = int64_to_float64(rs1, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return rs1;
}

uint64_t helper_fcvt_d_lu(CPUState *env, uint64_t rs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    rs1 = uint64_to_float64(rs1, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return rs1;
}

target_ulong helper_fcvt_wu_d_rod(CPUState *env, uint64_t frs1)
{
    require_fp;
    frs1 = (int64_t)((int32_t)float64_to_uint32_rod(frs1, &env->fp_status));
    set_fp_exceptions();
    return frs1;
}

target_ulong helper_fcvt_w_d_rod(CPUState *env, uint64_t frs1)
{
    require_fp;
    frs1 = (int64_t)((int32_t)float64_to_int32_rod(frs1, &env->fp_status));
    set_fp_exceptions();
    return frs1;
}

uint64_t helper_fcvt_s_d_rod(CPUState *env, uint64_t rs1)
{
    require_fp;
    rs1 = float64_to_float32_rod(rs1, &env->fp_status);
    set_fp_exceptions();
    return rs1;
}

uint64_t helper_fadd_h(CPUState *env, uint64_t frs1, uint64_t frs2, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);

    float16_t f1, f2;
    f1.v = (uint16_t)frs1;
    f2.v = (uint16_t)frs2;
    frs1 = f16_add(f1, f2).v;

    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fsub_h(CPUState *env, uint64_t frs1, uint64_t frs2, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);

    float16_t f1, f2;
    f1.v = (uint16_t)frs1;
    f2.v = (uint16_t)frs2;
    frs1 = f16_sub(f1, f2).v;

    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fmul_h(CPUState *env, uint64_t frs1, uint64_t frs2, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);

    float16_t f1, f2;
    f1.v = (uint16_t)frs1;
    f2.v = (uint16_t)frs2;
    frs1 = f16_mul(f1, f2).v;

    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fdiv_h(CPUState *env, uint64_t frs1, uint64_t frs2, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);

    float16_t f1, f2;
    f1.v = (uint16_t)frs1;
    f2.v = (uint16_t)frs2;
    frs1 = f16_div(f1, f2).v;

    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fmin_h(CPUState *env, uint64_t frs1, uint64_t frs2)
{
    require_fp;

    float16_t f1, f2;
    f1.v = (uint16_t)frs1;
    f2.v = (uint16_t)frs2;

    frs1 = f16_min(f1, f2).v;

    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fmax_h(CPUState *env, uint64_t frs1, uint64_t frs2)
{
    require_fp;

    float16_t f1, f2;
    f1.v = (uint16_t)frs1;
    f2.v = (uint16_t)frs2;

    frs1 = f16_max(f1, f2).v;

    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fcvt_s_h(CPUState *env, uint64_t rs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);

    float16_t f1;
    f1.v = (uint16_t)rs1;
    rs1 = f16_to_f32(f1).v;

    set_fp_exceptions();
    mark_fs_dirty();
    return rs1;
}

uint64_t helper_fcvt_h_s(CPUState *env, uint64_t rs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);

    float32_t f1;
    f1.v = (uint32_t)rs1;
    rs1 = f32_to_f16(f1).v;

    set_fp_exceptions();
    mark_fs_dirty();
    return rs1;
}

uint64_t helper_fcvt_d_h(CPUState *env, uint64_t rs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);

    float16_t f1;
    f1.v = (uint16_t)rs1;
    rs1 = f16_to_f64(f1).v;

    set_fp_exceptions();
    mark_fs_dirty();
    return rs1;
}

uint64_t helper_fcvt_h_d(CPUState *env, uint64_t rs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);

    float64_t f1;
    f1.v = rs1;
    rs1 = f64_to_f16(f1).v;

    set_fp_exceptions();
    mark_fs_dirty();
    return rs1;
}

uint64_t helper_fsqrt_h(CPUState *env, uint64_t frs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);

    float16_t f1;
    f1.v = (uint16_t)frs1;
    frs1 = f16_sqrt(f1).v;

    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

target_ulong helper_fle_h(CPUState *env, uint64_t frs1, uint64_t frs2)
{
    require_fp;

    float16_t f1, f2;
    f1.v = (uint16_t)frs1;
    f2.v = (uint16_t)frs2;
    frs1 = f16_le(f1, f2);

    set_fp_exceptions();
    return frs1;
}

target_ulong helper_flt_h(CPUState *env, uint64_t frs1, uint64_t frs2)
{
    require_fp;

    float16_t f1, f2;
    f1.v = (uint16_t)frs1;
    f2.v = (uint16_t)frs2;
    frs1 = f16_lt(f1, f2);

    set_fp_exceptions();
    return frs1;
}

target_ulong helper_fge_h(CPUState *env, uint64_t frs1, uint64_t frs2)
{
    return helper_fle_h(env, frs2, frs1);
}

target_ulong helper_fgt_h(CPUState *env, uint64_t frs1, uint64_t frs2)
{
    return helper_flt_h(env, frs2, frs1);
}

target_ulong helper_feq_h(CPUState *env, uint64_t frs1, uint64_t frs2)
{
    require_fp;

    float16_t f1, f2;
    f1.v = (uint16_t)frs1;
    f2.v = (uint16_t)frs2;
    frs1 = f16_eq(f1, f2);

    set_fp_exceptions();
    return frs1;
}

target_ulong helper_fcvt_w_h(CPUState *env, uint64_t frs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);

    float16_t f1;
    f1.v = (uint16_t)frs1;
    frs1 = (int64_t)((int32_t)f16_to_i32(f1, RM, true));

    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

target_ulong helper_fcvt_wu_h(CPUState *env, uint64_t frs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);

    float16_t f1;
    f1.v = (uint16_t)frs1;
    frs1 = (int64_t)((int32_t)f16_to_ui32(f1, RM, true));

    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fcvt_l_h(CPUState *env, uint64_t frs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);

    float16_t f1;
    f1.v = (uint16_t)frs1;
    frs1 = f16_to_i64(f1, RM, true);

    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fcvt_lu_h(CPUState *env, uint64_t frs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);

    float16_t f1;
    f1.v = (uint16_t)frs1;
    frs1 = f16_to_ui64(f1, RM, true);

    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fcvt_h_w(CPUState *env, target_ulong rs1, uint64_t rm)
{
    require_fp;
    uint64_t res;
    set_float_rounding_mode(RM, &env->fp_status);

    res = i32_to_f16((uint32_t)rs1).v;

    set_fp_exceptions();
    mark_fs_dirty();
    return res;
}

uint64_t helper_fcvt_h_wu(CPUState *env, target_ulong rs1, uint64_t rm)
{
    require_fp;
    uint64_t res;
    set_float_rounding_mode(RM, &env->fp_status);

    res = ui32_to_f16((uint32_t)rs1).v;

    set_fp_exceptions();
    mark_fs_dirty();
    return res;
}

uint64_t helper_fcvt_h_l(CPUState *env, uint64_t rs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);

    rs1 = i64_to_f16(rs1).v;

    set_fp_exceptions();
    mark_fs_dirty();
    return rs1;
}

uint64_t helper_fcvt_h_lu(CPUState *env, uint64_t rs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);

    rs1 = ui64_to_f16(rs1).v;

    set_fp_exceptions();
    mark_fs_dirty();
    return rs1;
}

uint64_t helper_fmv_x_h(CPUState *env, uint64_t rs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);

    rs1 = (int64_t)((int32_t)((int16_t)rs1));

    set_fp_exceptions();
    mark_fs_dirty();
    return rs1;
}

uint64_t helper_fmv_h_x(CPUState *env, uint64_t rs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);

    //  FMV.H.X moves the half-precision value encoded in IEEE 754-2008 standard encoding from the
    //  lower 16 bits of integer register rs1 to the floating-point register rd, NaN-boxing the result
    rs1 = box_float(RISCV_HALF_PRECISION, rs1);

    set_fp_exceptions();
    mark_fs_dirty();
    return rs1;
}

target_ulong helper_fclass_h(CPUState *env, uint64_t frs1)
{
    require_fp;

    float16_t f1;
    f1.v = (uint16_t)frs1;

    frs1 = f16_classify(f1);

    mark_fs_dirty();
    return frs1;
}

void helper_vfmv_vf(CPUState *env, uint32_t vd, uint64_t f1)
{
    const target_ulong eew = env->vsew;
    ensure_vector_embedded_extension_for_eew_or_raise_exception(env, eew);
    if(V_IDX_INVALID(vd)) {
        raise_exception_and_sync_pc(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch(eew) {
        case 16:
            f1 = unbox_float(RISCV_HALF_PRECISION, env, f1);
            break;
        case 32:
            f1 = unbox_float(RISCV_SINGLE_PRECISION, env, f1);
            break;
        default:
            raise_exception_and_sync_pc(env, RISCV_EXCP_ILLEGAL_INST);
            return;
    }
    for(int ei = 0; ei < env->vl; ++ei) {
        switch(eew) {
            case 16:
                ((uint16_t *)V(vd))[ei] = f1;
                break;
            case 32:
                ((uint32_t *)V(vd))[ei] = f1;
                break;
            case 64:
                ((uint64_t *)V(vd))[ei] = f1;
                break;
        }
    }
}

void helper_vfmv_fs(CPUState *env, int32_t vd, int32_t vs2)
{
    const target_ulong eew = env->vsew;
    ensure_vector_float_embedded_extension_or_raise_exception(env, eew);
    switch(eew) {
        case 16:
            env->fpr[vd] = box_float(RISCV_HALF_PRECISION, ((uint16_t *)V(vs2))[0]);
            break;
        case 32:
            env->fpr[vd] = box_float(RISCV_SINGLE_PRECISION, ((uint32_t *)V(vs2))[0]);
            break;
        case 64:
            env->fpr[vd] = ((uint64_t *)V(vs2))[0];
            break;
        default:
            raise_exception_and_sync_pc(env, RISCV_EXCP_ILLEGAL_INST);
            break;
    }
}

void helper_vfmv_sf(CPUState *env, uint32_t vd, float64 rs1)
{
    const target_ulong eew = env->vsew;
    ensure_vector_float_embedded_extension_or_raise_exception(env, eew);
    if(env->vstart >= env->vl) {
        return;
    }
    switch(eew) {
        case 16:
            ((int16_t *)V(vd))[0] = unbox_float(RISCV_HALF_PRECISION, env, rs1);
            break;
        case 32:
            ((int32_t *)V(vd))[0] = unbox_float(RISCV_SINGLE_PRECISION, env, rs1);
            break;
        case 64:
            ((int64_t *)V(vd))[0] = rs1;
            break;
        default:
            raise_exception_and_sync_pc(env, RISCV_EXCP_ILLEGAL_INST);
            break;
    }
}

uint_fast16_t float64_classify(uint64_t a, float_status *status)
{
    union ui64_f64 uA;
    uint_fast64_t uiA;

    uA.f = a;
    uiA = uA.ui;

    uint_fast16_t infOrNaN = expF64UI(uiA) == 0x7FF;
    uint_fast16_t subnormalOrZero = expF64UI(uiA) == 0;
    bool sign = signF64UI(uiA);

    return (sign && infOrNaN && fracF64UI(uiA) == 0) << 0 | (sign && !infOrNaN && !subnormalOrZero) << 1 |
           (sign && subnormalOrZero && fracF64UI(uiA)) << 2 | (sign && subnormalOrZero && fracF64UI(uiA) == 0) << 3 |
           (!sign && infOrNaN && fracF64UI(uiA) == 0) << 7 | (!sign && !infOrNaN && !subnormalOrZero) << 6 |
           (!sign && subnormalOrZero && fracF64UI(uiA)) << 5 | (!sign && subnormalOrZero && fracF64UI(uiA) == 0) << 4 |
           (isNaNF64UI(uiA) && float64_is_signaling_nan(uiA, status)) << 8 |
           (isNaNF64UI(uiA) && !float64_is_signaling_nan(uiA, status)) << 9;
}

target_ulong helper_fclass_d(CPUState *env, uint64_t frs1)
{
    require_fp;
    frs1 = float64_classify(frs1, &env->fp_status);
    mark_fs_dirty();
    return frs1;
}

static inline uint64_t make_mask64(int pos, int len)
{
    assert(pos >= 0 && len > 0 && pos < 64 && len <= 64);
    return (UINT64_MAX >> (64 - len)) << pos;
}

//  user needs to truncate output to required length
static inline uint64_t rsqrte7(uint64_t val, int e, int s, bool sub)
{
    uint64_t exp = extract64(val, s, e);
    uint64_t sig = extract64(val, 0, s);
    uint64_t sign = extract64(val, s + e, 1);
    const int p = 7;

    static const uint8_t table[] = { 52,  51,  50,  48,  47,  46, 44, 43,  42,  41,  40,  39,  38,  36,  35,  34,  33,  32,  31,
                                     30,  30,  29,  28,  27,  26, 25, 24,  23,  23,  22,  21,  20,  19,  19,  18,  17,  16,  16,
                                     15,  14,  14,  13,  12,  12, 11, 10,  10,  9,   9,   8,   7,   7,   6,   6,   5,   4,   4,
                                     3,   3,   2,   2,   1,   1,  0,  127, 125, 123, 121, 119, 118, 116, 114, 113, 111, 109, 108,
                                     106, 105, 103, 102, 100, 99, 97, 96,  95,  93,  92,  91,  90,  88,  87,  86,  85,  84,  83,
                                     82,  80,  79,  78,  77,  76, 75, 74,  73,  72,  71,  70,  70,  69,  68,  67,  66,  65,  64,
                                     63,  63,  62,  61,  60,  59, 59, 58,  57,  56,  56,  55,  54,  53 };

    if(sub) {
        while(extract64(sig, s - 1, 1) == 0) {
            exp--, sig <<= 1;
        }

        sig = (sig << 1) & make_mask64(0, s);
    }

    int idx = ((exp & 1) << (p - 1)) | (sig >> (s - p + 1));
    uint64_t out_sig = (uint64_t)(table[idx]) << (s - p);
    uint64_t out_exp = (3 * make_mask64(0, e - 1) + ~exp) / 2;

    return (sign << (s + e)) | (out_exp << s) | out_sig;
}

float32 f32_rsqrte7(CPUState *env, float32 in)
{
    union ui32_f32 uA;
    unsigned int flags = 0;

    uA.f = in;
    unsigned int ret = float32_classify(in, &env->fp_status);
    bool sub = false;
    switch(ret) {
        case 0x001:  //  -inf
        case 0x002:  //  -normal
        case 0x004:  //  -subnormal
        case 0x100:  //  sNaN
            flags |= float_flag_invalid;
            /* falls through */
        case 0x200:  //  qNaN
            uA.ui = float32_default_nan;
            break;
        case 0x008:  //  -0
            uA.ui = 0xff800000;
            flags |= float_flag_overflow;
            break;
        case 0x010:  //  +0
            uA.ui = 0x7f800000;
            flags |= float_flag_overflow;
            break;
        case 0x080:  //+inf
            uA.ui = 0x0;
            break;
        case 0x020:  //+ sub
            sub = true;
            /* falls through */
        default:  //  +num
            uA.ui = rsqrte7(uA.ui, 8, 23, sub);
            break;
    }

    env->fflags |= softfloat_flags_to_riscv(flags);
    set_float_exception_flags(0, &env->fp_status);
    return uA.f;
}

float64 f64_rsqrte7(CPUState *env, float64 in)
{
    union ui64_f64 uA;
    unsigned int flags = 0;

    uA.f = in;
    unsigned int ret = float64_classify(in, &env->fp_status);
    bool sub = false;
    switch(ret) {
        case 0x001:  //  -inf
        case 0x002:  //  -normal
        case 0x004:  //  -subnormal
        case 0x100:  //  sNaN
            flags |= float_flag_invalid;
            /* falls through */
        case 0x200:  //  qNaN
            uA.ui = float64_default_nan;
            break;
        case 0x008:  //  -0
            uA.ui = 0xfff0000000000000ul;
            flags |= float_flag_overflow;
            break;
        case 0x010:  //  +0
            uA.ui = 0x7ff0000000000000ul;
            flags |= float_flag_overflow;
            break;
        case 0x080:  //+inf
            uA.ui = 0x0;
            break;
        case 0x020:  //+ sub
            sub = true;
            /* falls through */
        default:  //  +num
            uA.ui = rsqrte7(uA.ui, 11, 52, sub);
            break;
    }

    env->fflags |= softfloat_flags_to_riscv(flags);
    set_float_exception_flags(0, &env->fp_status);
    return uA.f;
}

//  user needs to truncate output to required length
static inline uint64_t recip7(uint64_t val, int e, int s, int rm, bool sub, bool *round_abnormal)
{
    uint64_t exp = extract64(val, s, e);
    uint64_t sig = extract64(val, 0, s);
    uint64_t sign = extract64(val, s + e, 1);
    const int p = 7;

    static const uint8_t table[] = { 127, 125, 123, 121, 119, 117, 116, 114, 112, 110, 109, 107, 105, 104, 102, 100, 99, 97, 96,
                                     94,  93,  91,  90,  88,  87,  85,  84,  83,  81,  80,  79,  77,  76,  75,  74,  72, 71, 70,
                                     69,  68,  66,  65,  64,  63,  62,  61,  60,  59,  58,  57,  56,  55,  54,  53,  52, 51, 50,
                                     49,  48,  47,  46,  45,  44,  43,  42,  41,  40,  40,  39,  38,  37,  36,  35,  35, 34, 33,
                                     32,  31,  31,  30,  29,  28,  28,  27,  26,  25,  25,  24,  23,  23,  22,  21,  21, 20, 19,
                                     19,  18,  17,  17,  16,  15,  15,  14,  14,  13,  12,  12,  11,  11,  10,  9,   9,  8,  8,
                                     7,   7,   6,   5,   5,   4,   4,   3,   3,   2,   2,   1,   1,   0 };

    if(sub) {
        while(extract64(sig, s - 1, 1) == 0) {
            exp--, sig <<= 1;
        }

        sig = (sig << 1) & make_mask64(0, s);

        if(exp != 0 && exp != UINT64_MAX) {
            *round_abnormal = true;
            if(rm == 1 || (rm == 2 && !sign) || (rm == 3 && sign)) {
                return ((sign << (s + e)) | make_mask64(s, e)) - 1;
            } else {
                return (sign << (s + e)) | make_mask64(s, e);
            }
        }
    }

    int idx = sig >> (s - p);
    uint64_t out_sig = (uint64_t)(table[idx]) << (s - p);
    uint64_t out_exp = 2 * make_mask64(0, e - 1) + ~exp;
    if(out_exp == 0 || out_exp == UINT64_MAX) {
        out_sig = (out_sig >> 1) | make_mask64(s - 1, 1);
        if(out_exp == UINT64_MAX) {
            out_sig >>= 1;
            out_exp = 0;
        }
    }

    return (sign << (s + e)) | (out_exp << s) | out_sig;
}

float32 f32_recip7(CPUState *env, float32 in)
{
    union ui32_f32 uA;
    unsigned int flags = 0;

    uA.f = in;
    unsigned int ret = float32_classify(in, &env->fp_status);
    bool sub = false;
    bool round_abnormal = false;
    switch(ret) {
        case 0x001:  //  -inf
            uA.ui = 0x80000000;
            break;
        case 0x080:  //+inf
            uA.ui = 0x0;
            break;
        case 0x008:  //  -0
            uA.ui = 0xff800000;
            flags |= float_flag_overflow;
            break;
        case 0x010:  //  +0
            uA.ui = 0x7f800000;
            flags |= float_flag_overflow;
            break;
        case 0x100:  //  sNaN
            flags |= float_flag_invalid;
            /* falls through */
        case 0x200:  //  qNaN
            uA.ui = float32_default_nan;
            break;
        case 0x004:  //  -subnormal
        case 0x020:  //+ sub
            sub = true;
            /* falls through */
        default:  //  +- normal
            uA.ui = recip7(uA.ui, 8, 23, env->frm, sub, &round_abnormal);
            if(round_abnormal) {
                flags |= float_flag_inexact | float_flag_overflow;
            }
            break;
    }

    env->fflags |= softfloat_flags_to_riscv(flags);
    set_float_exception_flags(0, &env->fp_status);
    return uA.f;
}

float64 f64_recip7(CPUState *env, float64 in)
{
    union ui64_f64 uA;
    unsigned int flags = 0;

    uA.f = in;
    unsigned int ret = float64_classify(in, &env->fp_status);
    bool sub = false;
    bool round_abnormal = false;
    switch(ret) {
        case 0x001:  //  -inf
            uA.ui = 0x8000000000000000;
            break;
        case 0x080:  //+inf
            uA.ui = 0x0;
            break;
        case 0x008:  //  -0
            uA.ui = 0xfff0000000000000;
            flags |= float_flag_overflow;
            break;
        case 0x010:  //  +0
            uA.ui = 0x7ff0000000000000;
            flags |= float_flag_overflow;
            break;
        case 0x100:  //  sNaN
            flags |= float_flag_invalid;
            /* falls through */
        case 0x200:  //  qNaN
            uA.ui = float64_default_nan;
            break;
        case 0x004:  //  -subnormal
        case 0x020:  //+ sub
            sub = true;
            /* falls through */
        default:  //  +- normal
            uA.ui = recip7(uA.ui, 11, 52, env->frm, sub, &round_abnormal);
            if(round_abnormal) {
                flags |= float_flag_inexact | float_flag_overflow;
            }
            break;
    }

    env->fflags |= softfloat_flags_to_riscv(flags);
    set_float_exception_flags(0, &env->fp_status);
    return uA.f;
}

//  Note that MASKED is not defined for the 2nd include
#define MASKED
#include "fpu_vector_helper_template.h"
#include "fpu_vector_helper_template.h"

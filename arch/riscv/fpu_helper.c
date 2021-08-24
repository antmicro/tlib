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
#include "cpu.h"

/* convert RISC-V rounding mode to IEEE library numbers */
unsigned int ieee_rm[] = {
    float_round_nearest_even, float_round_to_zero, float_round_down, float_round_up, float_round_ties_away
};

/* convert RISC-V Vector Fixed-Point Rounding Mode to IEEE library numbers */
unsigned int ieee_vxrm[] = {
    float_round_up, float_round_nearest_even, float_round_down
};

/* obtain rm value to use in computation
 * as the last step, convert rm codes to what the softfloat library expects
 * Adapted from Spike's decode.h:RM
 */
#define RM         ({                                     \
if (rm == 7) {                                            \
    rm = env->frm;                                        \
}                                                         \
if (rm > 4) {                                             \
    helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST); \
}                                                         \
ieee_rm[rm]; })

#define require_fp if (!(env->mstatus & MSTATUS_FS)) { \
    helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST); \
}

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
#define set_fp_exceptions() do { \
    env->fflags |= softfloat_flags_to_riscv(get_float_exception_flags(\
                            &env->fp_status)); \
    set_float_exception_flags(0, &env->fp_status); \
} while (0)

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
    mark_fs_dirty();
    return frs1;
}

target_ulong helper_flt_s(CPUState *env, uint64_t frs1, uint64_t frs2)
{
    require_fp;
    frs1 = float32_lt(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

target_ulong helper_feq_s(CPUState *env, uint64_t frs1, uint64_t frs2)
{
    require_fp;
    frs1 = float32_eq_quiet(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

target_ulong helper_fcvt_w_s(CPUState *env, uint64_t frs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float32_to_int32(frs1, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

target_ulong helper_fcvt_wu_s(CPUState *env, uint64_t frs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = (int32_t)float32_to_uint32(frs1, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

#if defined(TARGET_RISCV64)
uint64_t helper_fcvt_l_s(CPUState *env, uint64_t frs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float32_to_int64(frs1, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fcvt_lu_s(CPUState *env, uint64_t frs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float32_to_uint64(frs1, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}
#endif

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

#if defined(TARGET_RISCV64)
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
#endif

uint32_t helper_fcvt_wu_s_rod(CPUState *env, uint32_t frs1)
{
    require_fp;
    frs1 = float32_to_uint32_rod(frs1, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

int32_t helper_fcvt_w_s_rod(CPUState *env, uint32_t frs1)
{
    require_fp;
    frs1 = float32_to_int32_rod(frs1, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fcvt_lu_d_rod(CPUState *env, uint64_t frs1)
{
    require_fp;
    frs1 = float64_to_uint64_rod(frs1, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

int64_t helper_fcvt_l_d_rod(CPUState *env, uint64_t frs1)
{
    require_fp;
    frs1 = float64_to_int64_rod(frs1, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

/* adapted from spike */
#define isNaNF32UI(ui) (0xFF000000 < (uint32_t)((uint_fast32_t)ui << 1))
#define signF32UI(a)   ((bool)((uint32_t)a >> 31))
#define expF32UI(a)    ((int_fast16_t)(a >> 23) & 0xFF)
#define fracF32UI(a)   (a & 0x007FFFFF)

union ui32_f32 { uint32_t ui; uint32_t f; };

uint_fast16_t float32_classify(uint32_t a, float_status *status)
{
    union ui32_f32 uA;
    uint_fast32_t uiA;

    uA.f = a;
    uiA = uA.ui;

    uint_fast16_t infOrNaN = expF32UI(uiA) == 0xFF;
    uint_fast16_t subnormalOrZero = expF32UI(uiA) == 0;
    bool sign = signF32UI(uiA);

/* *INDENT-OFF* */
    return
        (sign && infOrNaN && fracF32UI(uiA) == 0)           << 0 |
        (sign && !infOrNaN && !subnormalOrZero)             << 1 |
        (sign && subnormalOrZero && fracF32UI(uiA))         << 2 |
        (sign && subnormalOrZero && fracF32UI(uiA) == 0)    << 3 |
        (!sign && infOrNaN && fracF32UI(uiA) == 0)          << 7 |
        (!sign && !infOrNaN && !subnormalOrZero)            << 6 |
        (!sign && subnormalOrZero && fracF32UI(uiA))        << 5 |
        (!sign && subnormalOrZero && fracF32UI(uiA) == 0)   << 4 |
        (isNaNF32UI(uiA) &&  float32_is_signaling_nan(uiA)) << 8 |
        (isNaNF32UI(uiA) && !float32_is_signaling_nan(uiA)) << 9;
/* *INDENT-ON* */
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
    mark_fs_dirty();
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
    mark_fs_dirty();
    return frs1;
}

target_ulong helper_flt_d(CPUState *env, uint64_t frs1, uint64_t frs2)
{
    require_fp;
    frs1 = float64_lt(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

target_ulong helper_feq_d(CPUState *env, uint64_t frs1, uint64_t frs2)
{
    require_fp;
    frs1 = float64_eq_quiet(frs1, frs2, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

target_ulong helper_fcvt_w_d(CPUState *env, uint64_t frs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = (int64_t)((int32_t)float64_to_int32(frs1, &env->fp_status));
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

target_ulong helper_fcvt_wu_d(CPUState *env, uint64_t frs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = (int64_t)((int32_t)float64_to_uint32(frs1, &env->fp_status));
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fcvt_l_d(CPUState *env, uint64_t frs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float64_to_int64(frs1, &env->fp_status);
    set_fp_exceptions();
    mark_fs_dirty();
    return frs1;
}

uint64_t helper_fcvt_lu_d(CPUState *env, uint64_t frs1, uint64_t rm)
{
    require_fp;
    set_float_rounding_mode(RM, &env->fp_status);
    frs1 = float64_to_uint64(frs1, &env->fp_status);
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

/* adapted from spike */
#define isNaNF64UI(ui) (UINT64_C(0xFFE0000000000000) \
                       < (uint64_t)((uint_fast64_t)ui << 1))
#define signF64UI(a)   ((bool)((uint64_t) a >> 63))
#define expF64UI(a)    ((int_fast16_t)(a >> 52) & 0x7FF)
#define fracF64UI(a)   (a & UINT64_C(0x000FFFFFFFFFFFFF))

union ui64_f64 { uint64_t ui; uint64_t f; };

uint_fast16_t float64_classify(uint64_t a, float_status *status)
{
    union ui64_f64 uA;
    uint_fast64_t uiA;

    uA.f = a;
    uiA = uA.ui;

    uint_fast16_t infOrNaN = expF64UI(uiA) == 0x7FF;
    uint_fast16_t subnormalOrZero = expF64UI(uiA) == 0;
    bool sign = signF64UI(uiA);

    return
        (sign && infOrNaN &&
         fracF64UI(uiA) ==
         0) << 0 |
    (sign && !infOrNaN &&
     !subnormalOrZero)             << 1 |
    (sign && subnormalOrZero &&
     fracF64UI(uiA))         << 2 |
    (sign && subnormalOrZero &&
     fracF64UI(uiA) ==
     0)    << 3 |
    (!sign && infOrNaN &&
     fracF64UI(uiA) ==
     0)          << 7 |
    (!sign && !infOrNaN &&
     !subnormalOrZero)            << 6 |
    (!sign && subnormalOrZero &&
     fracF64UI(uiA))        << 5 |
    (!sign && subnormalOrZero &&
     fracF64UI(uiA) ==
     0)   << 4 |
    (isNaNF64UI(uiA) &&  float64_is_signaling_nan(uiA)) << 8 | (isNaNF64UI(uiA) && !float64_is_signaling_nan(uiA)) << 9;
}

target_ulong helper_fclass_d(CPUState *env, uint64_t frs1)
{
    require_fp;
    frs1 = float64_classify(frs1, &env->fp_status);
    mark_fs_dirty();
    return frs1;
}

#define MASKED
#include "fpu_vector_helper_template.h"
#include "fpu_vector_helper_template.h"

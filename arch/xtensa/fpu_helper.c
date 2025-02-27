/*
 * Copyright (c) 2011 - 2019, Max Filippov, Open Source and Linux Lab.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Open Source and Linux Lab nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "cpu.h"
#include "osdep.h"
#include "softfloat-2.h"
#include "tb-helper.h"

enum {
    XTENSA_FP_I = 0x1,
    XTENSA_FP_U = 0x2,
    XTENSA_FP_O = 0x4,
    XTENSA_FP_Z = 0x8,
    XTENSA_FP_V = 0x10,
};

enum {
    XTENSA_FCR_FLAGS_SHIFT = 2,
    XTENSA_FSR_FLAGS_SHIFT = 7,
};

static const struct {
    uint32_t xtensa_fp_flag;
    int softfloat_fp_flag;
} xtensa_fp_flag_map[] = {
    {
     XTENSA_FP_I, float_flag_inexact,
     },
    {
     XTENSA_FP_U,           float_flag_underflow,
     },
    {
     XTENSA_FP_O, float_flag_overflow,
     },
    {
     XTENSA_FP_Z,           float_flag_divbyzero,
     },
    {
     XTENSA_FP_V, float_flag_invalid,
     },
};

void HELPER(wur_fpu2k_fcr)(CPUState *env, uint32_t v)
{
    static const int rounding_mode[] = {
        float_round_nearest_even,
        float_round_to_zero,
        float_round_up,
        float_round_down,
    };

    env->uregs[FCR] = v & 0xfffff07f;
    set_float_rounding_mode(rounding_mode[v & 3], &env->fp_status);
}

void HELPER(wur_fpu_fcr)(CPUState *env, uint32_t v)
{
    static const int rounding_mode[] = {
        float_round_nearest_even,
        float_round_to_zero,
        float_round_up,
        float_round_down,
    };

    if(v & 0xfffff000) {
        tlib_printf(LOG_LEVEL_ERROR, "MBZ field of FCR is written non-zero: %08x\n", v);
    }
    env->uregs[FCR] = v & 0x0000007f;
    set_float_rounding_mode(rounding_mode[v & 3], &env->fp_status);
}

void HELPER(wur_fpu_fsr)(CPUState *env, uint32_t v)
{
    uint32_t flags = v >> XTENSA_FSR_FLAGS_SHIFT;
    int fef = 0;
    unsigned i;

    if(v & 0xfffff000) {
        tlib_printf(LOG_LEVEL_ERROR, "MBZ field of FSR is written non-zero: %08x\n", v);
    }
    env->uregs[FSR] = v & 0x00000f80;
    for(i = 0; i < ARRAY_SIZE(xtensa_fp_flag_map); ++i) {
        if(flags & xtensa_fp_flag_map[i].xtensa_fp_flag) {
            fef |= xtensa_fp_flag_map[i].softfloat_fp_flag;
        }
    }
    set_float_exception_flags(fef, &env->fp_status);
}

uint32_t HELPER(rur_fpu_fsr)(CPUState *env)
{
    uint32_t flags = 0;
    int fef = get_float_exception_flags(&env->fp_status);
    unsigned i;

    for(i = 0; i < ARRAY_SIZE(xtensa_fp_flag_map); ++i) {
        if(fef & xtensa_fp_flag_map[i].softfloat_fp_flag) {
            flags |= xtensa_fp_flag_map[i].xtensa_fp_flag;
        }
    }
    env->uregs[FSR] = flags << XTENSA_FSR_FLAGS_SHIFT;
    return flags << XTENSA_FSR_FLAGS_SHIFT;
}

float64 HELPER(abs_d)(float64 v)
{
    return float64_abs(v);
}

float32 HELPER(abs_s)(float32 v)
{
    return float32_abs(v);
}

float64 HELPER(neg_d)(float64 v)
{
    return float64_chs(v);
}

float32 HELPER(neg_s)(float32 v)
{
    return float32_chs(v);
}

float32 HELPER(fpu2k_add_s)(CPUState *env, float32 a, float32 b)
{
    return float32_add(a, b, &env->fp_status);
}

float32 HELPER(fpu2k_sub_s)(CPUState *env, float32 a, float32 b)
{
    return float32_sub(a, b, &env->fp_status);
}

float32 HELPER(fpu2k_mul_s)(CPUState *env, float32 a, float32 b)
{
    return float32_mul(a, b, &env->fp_status);
}

float32 HELPER(fpu2k_madd_s)(CPUState *env, float32 a, float32 b, float32 c)
{
    return float32_muladd(b, c, a, 0, &env->fp_status);
}

float32 HELPER(fpu2k_msub_s)(CPUState *env, float32 a, float32 b, float32 c)
{
    return float32_muladd(b, c, a, float_muladd_negate_product, &env->fp_status);
}

float64 HELPER(add_d)(CPUState *env, float64 a, float64 b)
{
    set_use_first_nan(true, &env->fp_status);
    return float64_add(a, b, &env->fp_status);
}

float32 HELPER(add_s)(CPUState *env, float32 a, float32 b)
{
    set_use_first_nan(env->config->use_first_nan, &env->fp_status);
    return float32_add(a, b, &env->fp_status);
}

float64 HELPER(sub_d)(CPUState *env, float64 a, float64 b)
{
    set_use_first_nan(true, &env->fp_status);
    return float64_sub(a, b, &env->fp_status);
}

float32 HELPER(sub_s)(CPUState *env, float32 a, float32 b)
{
    set_use_first_nan(env->config->use_first_nan, &env->fp_status);
    return float32_sub(a, b, &env->fp_status);
}

float64 HELPER(mul_d)(CPUState *env, float64 a, float64 b)
{
    set_use_first_nan(true, &env->fp_status);
    return float64_mul(a, b, &env->fp_status);
}

float32 HELPER(mul_s)(CPUState *env, float32 a, float32 b)
{
    set_use_first_nan(env->config->use_first_nan, &env->fp_status);
    return float32_mul(a, b, &env->fp_status);
}

float64 HELPER(madd_d)(CPUState *env, float64 a, float64 b, float64 c)
{
    set_use_first_nan(env->config->use_first_nan, &env->fp_status);
    return float64_muladd(b, c, a, 0, &env->fp_status);
}

float32 HELPER(madd_s)(CPUState *env, float32 a, float32 b, float32 c)
{
    set_use_first_nan(env->config->use_first_nan, &env->fp_status);
    return float32_muladd(b, c, a, 0, &env->fp_status);
}

float64 HELPER(msub_d)(CPUState *env, float64 a, float64 b, float64 c)
{
    set_use_first_nan(env->config->use_first_nan, &env->fp_status);
    return float64_muladd(b, c, a, float_muladd_negate_product, &env->fp_status);
}

float32 HELPER(msub_s)(CPUState *env, float32 a, float32 b, float32 c)
{
    set_use_first_nan(env->config->use_first_nan, &env->fp_status);
    return float32_muladd(b, c, a, float_muladd_negate_product, &env->fp_status);
}

float64 HELPER(mkdadj_d)(CPUState *env, float64 a, float64 b)
{
    set_use_first_nan(true, &env->fp_status);
    return float64_div(b, a, &env->fp_status);
}

float32 HELPER(mkdadj_s)(CPUState *env, float32 a, float32 b)
{
    set_use_first_nan(env->config->use_first_nan, &env->fp_status);
    return float32_div(b, a, &env->fp_status);
}

float64 HELPER(mksadj_d)(CPUState *env, float64 v)
{
    set_use_first_nan(true, &env->fp_status);
    return float64_sqrt(v, &env->fp_status);
}

float32 HELPER(mksadj_s)(CPUState *env, float32 v)
{
    set_use_first_nan(env->config->use_first_nan, &env->fp_status);
    return float32_sqrt(v, &env->fp_status);
}

uint32_t HELPER(ftoi_d)(CPUState *env, float64 v, uint32_t rounding_mode, uint32_t scale)
{
    float_status fp_status = env->fp_status;
    uint32_t res;

    set_float_rounding_mode(rounding_mode, &fp_status);
    res = float64_to_int32(float64_scalbn(v, scale, &fp_status), &fp_status);
    set_float_exception_flags(get_float_exception_flags(&fp_status), &env->fp_status);
    return res;
}

uint32_t HELPER(ftoi_s)(CPUState *env, float32 v, uint32_t rounding_mode, uint32_t scale)
{
    float_status fp_status = env->fp_status;
    uint32_t res;

    set_float_rounding_mode(rounding_mode, &fp_status);
    res = float32_to_int32(float32_scalbn(v, scale, &fp_status), &fp_status);
    set_float_exception_flags(get_float_exception_flags(&fp_status), &env->fp_status);
    return res;
}

uint32_t HELPER(ftoui_d)(CPUState *env, float64 v, uint32_t rounding_mode, uint32_t scale)
{
    float_status fp_status = env->fp_status;
    float64 res;
    uint32_t rv;

    set_float_rounding_mode(rounding_mode, &fp_status);

    res = float64_scalbn(v, scale, &fp_status);

    if(float64_is_neg(v) && !float64_is_any_nan(v)) {
        set_float_exception_flags(float_flag_invalid, &fp_status);
        rv = float64_to_int32(res, &fp_status);
    } else {
        rv = float64_to_uint32(res, &fp_status);
    }
    set_float_exception_flags(get_float_exception_flags(&fp_status), &env->fp_status);
    return rv;
}

uint32_t HELPER(ftoui_s)(CPUState *env, float32 v, uint32_t rounding_mode, uint32_t scale)
{
    float_status fp_status = env->fp_status;
    float32 res;
    uint32_t rv;

    set_float_rounding_mode(rounding_mode, &fp_status);

    res = float32_scalbn(v, scale, &fp_status);

    if(float32_is_neg(v) && !float32_is_any_nan(v)) {
        rv = float32_to_int32(res, &fp_status);
        if(rv) {
            set_float_exception_flags(float_flag_invalid, &fp_status);
        }
    } else {
        rv = float32_to_uint32(res, &fp_status);
    }
    set_float_exception_flags(get_float_exception_flags(&fp_status), &env->fp_status);
    return rv;
}

float64 HELPER(itof_d)(CPUState *env, uint32_t v, uint32_t scale)
{
    return float64_scalbn(int32_to_float64(v, &env->fp_status), (int32_t)scale, &env->fp_status);
}

float32 HELPER(itof_s)(CPUState *env, uint32_t v, uint32_t scale)
{
    return float32_scalbn(int32_to_float32(v, &env->fp_status), (int32_t)scale, &env->fp_status);
}

float64 HELPER(uitof_d)(CPUState *env, uint32_t v, uint32_t scale)
{
    return float64_scalbn(uint32_to_float64(v, &env->fp_status), (int32_t)scale, &env->fp_status);
}

float32 HELPER(uitof_s)(CPUState *env, uint32_t v, uint32_t scale)
{
    return float32_scalbn(uint32_to_float32(v, &env->fp_status), (int32_t)scale, &env->fp_status);
}

float64 HELPER(cvtd_s)(CPUState *env, float32 v)
{
    return float32_to_float64(v, &env->fp_status);
}

float32 HELPER(cvts_d)(CPUState *env, float64 v)
{
    return float64_to_float32(v, &env->fp_status);
}

uint32_t HELPER(un_d)(CPUState *env, float64 a, float64 b)
{
    return float64_unordered_quiet(a, b, &env->fp_status);
}

uint32_t HELPER(un_s)(CPUState *env, float32 a, float32 b)
{
    return float32_unordered_quiet(a, b, &env->fp_status);
}

uint32_t HELPER(oeq_d)(CPUState *env, float64 a, float64 b)
{
    return float64_eq_quiet(a, b, &env->fp_status);
}

uint32_t HELPER(oeq_s)(CPUState *env, float32 a, float32 b)
{
    return float32_eq_quiet(a, b, &env->fp_status);
}

uint32_t HELPER(ueq_d)(CPUState *env, float64 a, float64 b)
{
    int v = float64_compare_quiet(a, b, &env->fp_status);

    return v == float_relation_equal || v == float_relation_unordered;
}

uint32_t HELPER(ueq_s)(CPUState *env, float32 a, float32 b)
{
    int v = float32_compare_quiet(a, b, &env->fp_status);

    return v == float_relation_equal || v == float_relation_unordered;
}

uint32_t HELPER(olt_d)(CPUState *env, float64 a, float64 b)
{
    return float64_lt(a, b, &env->fp_status);
}

uint32_t HELPER(olt_s)(CPUState *env, float32 a, float32 b)
{
    return float32_lt(a, b, &env->fp_status);
}

uint32_t HELPER(ult_d)(CPUState *env, float64 a, float64 b)
{
    int v = float64_compare_quiet(a, b, &env->fp_status);

    return v == float_relation_less || v == float_relation_unordered;
}

uint32_t HELPER(ult_s)(CPUState *env, float32 a, float32 b)
{
    int v = float32_compare_quiet(a, b, &env->fp_status);

    return v == float_relation_less || v == float_relation_unordered;
}

uint32_t HELPER(ole_d)(CPUState *env, float64 a, float64 b)
{
    return float64_le(a, b, &env->fp_status);
}

uint32_t HELPER(ole_s)(CPUState *env, float32 a, float32 b)
{
    return float32_le(a, b, &env->fp_status);
}

uint32_t HELPER(ule_d)(CPUState *env, float64 a, float64 b)
{
    int v = float64_compare_quiet(a, b, &env->fp_status);

    return v != float_relation_greater;
}

uint32_t HELPER(ule_s)(CPUState *env, float32 a, float32 b)
{
    int v = float32_compare_quiet(a, b, &env->fp_status);

    return v != float_relation_greater;
}

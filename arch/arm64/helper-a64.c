/*
 *  AArch64 specific helpers
 *
 *  Copyright (c) 2013 Alexander Graf <agraf@suse.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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
#include "helper.h"
#include "host-utils.h"
#include "int128.h"
#include "osdep.h"
#include "softfloat-2.h"
#include "syndrome.h"

//  Some silly MMU adjustments. Memops (mostly MO_128, MO_ALIGN_16 and MO_BE) are ignored.
#define cpu_ldq_be_mmu(env, address, memidx, ra) __ldq_mmu(address, memidx)
#define cpu_ldq_le_mmu(env, address, memidx, ra) __ldq_mmu(address, memidx)
#define cpu_stq_be_mmu(env, address, value, memidx, ra) \
    {                                                   \
        (void)ra;                                       \
        __stq_mmu(address, value, memidx);              \
    }
#define cpu_stq_le_mmu(env, address, value, memidx, ra) \
    {                                                   \
        (void)ra;                                       \
        __stq_mmu(address, value, memidx);              \
    }
#define make_memop_idx(memop, memidx) (memidx)

/* C2.4.7 Multiply and divide */
/* special cases for 0 and LLONG_MIN are mandated by the standard */
uint64_t HELPER(udiv64)(uint64_t num, uint64_t den)
{
    if(den == 0) {
        return 0;
    }
    return num / den;
}

int64_t HELPER(sdiv64)(int64_t num, int64_t den)
{
    if(den == 0) {
        return 0;
    }
    if(num == LLONG_MIN && den == -1) {
        return LLONG_MIN;
    }
    return num / den;
}

uint64_t HELPER(rbit64)(uint64_t x)
{
    return revbit64(x);
}

void HELPER(msr_i_spsel)(CPUARMState *env, uint32_t imm)
{
    //  Save the current SP in the SP_EL[el] bank.
    //  'el' is 0 or the current EL depending on the current PSTATE_SP.
    aarch64_save_sp(env);

    //  Set PSTATE_SP.
    env->pstate = deposit32(env->pstate, 0, 1, imm);

    //  Restore banked SP_EL[el].
    //  'el' will be different than before if PSTATE_SP has changed.
    aarch64_restore_sp(env);
}

static void daif_check(CPUARMState *env, uint32_t op, uint32_t imm, uintptr_t ra)
{
    /* DAIF update to PSTATE. This is OK from EL0 only if UMA is set.  */
    if(arm_current_el(env) == 0 && !(arm_sctlr(env, 0) & SCTLR_UMA)) {
        raise_exception_ra(env, EXCP_UDEF, syn_aa64_sysregtrap(0, extract32(op, 0, 3), extract32(op, 3, 3), 4, imm, 0x1f, 0),
                           exception_target_el(env), ra);
    }
}

void HELPER(msr_i_daifset)(CPUARMState *env, uint32_t imm)
{
    daif_check(env, 0x1e, imm, ARM_GETPC());
    env->daif |= (imm << 6) & PSTATE_DAIF;
    arm_rebuild_hflags(env);
}

void HELPER(msr_i_daifclear)(CPUARMState *env, uint32_t imm)
{
    daif_check(env, 0x1f, imm, ARM_GETPC());
    env->daif &= ~((imm << 6) & PSTATE_DAIF);
    arm_rebuild_hflags(env);
}

/* Convert a softfloat float_relation_ (as returned by
 * the float*_compare functions) to the correct ARM
 * NZCV flag state.
 */
static inline uint32_t float_rel_to_flags(int res)
{
    uint64_t flags;
    switch(res) {
        case float_relation_equal:
            flags = PSTATE_Z | PSTATE_C;
            break;
        case float_relation_less:
            flags = PSTATE_N;
            break;
        case float_relation_greater:
            flags = PSTATE_C;
            break;
        case float_relation_unordered:
        default:
            flags = PSTATE_C | PSTATE_V;
            break;
    }
    return flags;
}

uint64_t HELPER(vfp_cmph_a64)(uint32_t x, uint32_t y, void *fp_status)
{
    return float_rel_to_flags(float16_compare_quiet(x, y, fp_status));
}

uint64_t HELPER(vfp_cmpeh_a64)(uint32_t x, uint32_t y, void *fp_status)
{
    return float_rel_to_flags(float16_compare(x, y, fp_status));
}

uint64_t HELPER(vfp_cmps_a64)(float32 x, float32 y, void *fp_status)
{
    return float_rel_to_flags(float32_compare_quiet(x, y, fp_status));
}

uint64_t HELPER(vfp_cmpes_a64)(float32 x, float32 y, void *fp_status)
{
    return float_rel_to_flags(float32_compare(x, y, fp_status));
}

uint64_t HELPER(vfp_cmpd_a64)(float64 x, float64 y, void *fp_status)
{
    return float_rel_to_flags(float64_compare_quiet(x, y, fp_status));
}

uint64_t HELPER(vfp_cmped_a64)(float64 x, float64 y, void *fp_status)
{
    return float_rel_to_flags(float64_compare(x, y, fp_status));
}

float32 HELPER(vfp_mulxs)(float32 a, float32 b, void *fpstp)
{
    float_status *fpst = fpstp;

    a = float32_squash_input_denormal(a, fpst);
    b = float32_squash_input_denormal(b, fpst);

    if((float32_is_zero(a) && float32_is_infinity(b)) || (float32_is_infinity(a) && float32_is_zero(b))) {
        /* 2.0 with the sign bit set to sign(A) XOR sign(B) */
        return make_float32((1U << 30) | ((float32_val(a) ^ float32_val(b)) & (1U << 31)));
    }
    return float32_mul(a, b, fpst);
}

float64 HELPER(vfp_mulxd)(float64 a, float64 b, void *fpstp)
{
    float_status *fpst = fpstp;

    a = float64_squash_input_denormal(a, fpst);
    b = float64_squash_input_denormal(b, fpst);

    if((float64_is_zero(a) && float64_is_infinity(b)) || (float64_is_infinity(a) && float64_is_zero(b))) {
        /* 2.0 with the sign bit set to sign(A) XOR sign(B) */
        return make_float64((1ULL << 62) | ((float64_val(a) ^ float64_val(b)) & (1ULL << 63)));
    }
    return float64_mul(a, b, fpst);
}

/* 64bit/double versions of the neon float compare functions */
uint64_t HELPER(neon_ceq_f64)(float64 a, float64 b, void *fpstp)
{
    float_status *fpst = fpstp;
    return -float64_eq_quiet(a, b, fpst);
}

uint64_t HELPER(neon_cge_f64)(float64 a, float64 b, void *fpstp)
{
    float_status *fpst = fpstp;
    return -float64_le(b, a, fpst);
}

uint64_t HELPER(neon_cgt_f64)(float64 a, float64 b, void *fpstp)
{
    float_status *fpst = fpstp;
    return -float64_lt(b, a, fpst);
}

/* Reciprocal step and sqrt step. Note that unlike the A32/T32
 * versions, these do a fully fused multiply-add or
 * multiply-add-and-halve.
 */

uint32_t HELPER(recpsf_f16)(uint32_t a, uint32_t b, void *fpstp)
{
    float_status *fpst = fpstp;

    a = float16_squash_input_denormal(a, fpst);
    b = float16_squash_input_denormal(b, fpst);

    a = float16_chs(a);
    if((float16_is_infinity(a) && float16_is_zero(b)) || (float16_is_infinity(b) && float16_is_zero(a))) {
        return float16_two;
    }
    return float16_muladd(a, b, float16_two, 0, fpst);
}

float32 HELPER(recpsf_f32)(float32 a, float32 b, void *fpstp)
{
    float_status *fpst = fpstp;

    a = float32_squash_input_denormal(a, fpst);
    b = float32_squash_input_denormal(b, fpst);

    a = float32_chs(a);
    if((float32_is_infinity(a) && float32_is_zero(b)) || (float32_is_infinity(b) && float32_is_zero(a))) {
        return float32_two;
    }
    return float32_muladd(a, b, float32_two, 0, fpst);
}

float64 HELPER(recpsf_f64)(float64 a, float64 b, void *fpstp)
{
    float_status *fpst = fpstp;

    a = float64_squash_input_denormal(a, fpst);
    b = float64_squash_input_denormal(b, fpst);

    a = float64_chs(a);
    if((float64_is_infinity(a) && float64_is_zero(b)) || (float64_is_infinity(b) && float64_is_zero(a))) {
        return float64_two;
    }
    return float64_muladd(a, b, float64_two, 0, fpst);
}

uint32_t HELPER(rsqrtsf_f16)(uint32_t a, uint32_t b, void *fpstp)
{
    float_status *fpst = fpstp;

    a = float16_squash_input_denormal(a, fpst);
    b = float16_squash_input_denormal(b, fpst);

    a = float16_chs(a);
    if((float16_is_infinity(a) && float16_is_zero(b)) || (float16_is_infinity(b) && float16_is_zero(a))) {
        return float16_one_point_five;
    }
    return float16_muladd(a, b, float16_three, float_muladd_halve_result, fpst);
}

float32 HELPER(rsqrtsf_f32)(float32 a, float32 b, void *fpstp)
{
    float_status *fpst = fpstp;

    a = float32_squash_input_denormal(a, fpst);
    b = float32_squash_input_denormal(b, fpst);

    a = float32_chs(a);
    if((float32_is_infinity(a) && float32_is_zero(b)) || (float32_is_infinity(b) && float32_is_zero(a))) {
        return float32_one_point_five;
    }
    return float32_muladd(a, b, float32_three, float_muladd_halve_result, fpst);
}

float64 HELPER(rsqrtsf_f64)(float64 a, float64 b, void *fpstp)
{
    float_status *fpst = fpstp;

    a = float64_squash_input_denormal(a, fpst);
    b = float64_squash_input_denormal(b, fpst);

    a = float64_chs(a);
    if((float64_is_infinity(a) && float64_is_zero(b)) || (float64_is_infinity(b) && float64_is_zero(a))) {
        return float64_one_point_five;
    }
    return float64_muladd(a, b, float64_three, float_muladd_halve_result, fpst);
}

/* Pairwise long add: add pairs of adjacent elements into
 * double-width elements in the result (eg _s8 is an 8x8->16 op)
 */
uint64_t HELPER(neon_addlp_s8)(uint64_t a)
{
    uint64_t nsignmask = 0x0080008000800080ULL;
    uint64_t wsignmask = 0x8000800080008000ULL;
    uint64_t elementmask = 0x00ff00ff00ff00ffULL;
    uint64_t tmp1, tmp2;
    uint64_t res, signres;

    /* Extract odd elements, sign extend each to a 16 bit field */
    tmp1 = a & elementmask;
    tmp1 ^= nsignmask;
    tmp1 |= wsignmask;
    tmp1 = (tmp1 - nsignmask) ^ wsignmask;
    /* Ditto for the even elements */
    tmp2 = (a >> 8) & elementmask;
    tmp2 ^= nsignmask;
    tmp2 |= wsignmask;
    tmp2 = (tmp2 - nsignmask) ^ wsignmask;

    /* calculate the result by summing bits 0..14, 16..22, etc,
     * and then adjusting the sign bits 15, 23, etc manually.
     * This ensures the addition can't overflow the 16 bit field.
     */
    signres = (tmp1 ^ tmp2) & wsignmask;
    res = (tmp1 & ~wsignmask) + (tmp2 & ~wsignmask);
    res ^= signres;

    return res;
}

uint64_t HELPER(neon_addlp_u8)(uint64_t a)
{
    uint64_t tmp;

    tmp = a & 0x00ff00ff00ff00ffULL;
    tmp += (a >> 8) & 0x00ff00ff00ff00ffULL;
    return tmp;
}

uint64_t HELPER(neon_addlp_s16)(uint64_t a)
{
    int32_t reslo, reshi;

    reslo = (int32_t)(int16_t)a + (int32_t)(int16_t)(a >> 16);
    reshi = (int32_t)(int16_t)(a >> 32) + (int32_t)(int16_t)(a >> 48);

    return (uint32_t)reslo | (((uint64_t)reshi) << 32);
}

uint64_t HELPER(neon_addlp_u16)(uint64_t a)
{
    uint64_t tmp;

    tmp = a & 0x0000ffff0000ffffULL;
    tmp += (a >> 16) & 0x0000ffff0000ffffULL;
    return tmp;
}

/* Floating-point reciprocal exponent - see FPRecpX in ARM ARM */
uint32_t HELPER(frecpx_f16)(uint32_t a, void *fpstp)
{
    float_status *fpst = fpstp;
    uint16_t val16, sbit;
    int16_t exp;

    if(float16_is_any_nan(a)) {
        float16 nan = a;
        if(float16_is_signaling_nan(a, fpst)) {
            float_raise(float_flag_invalid, fpst);
            if(!fpst->default_nan_mode) {
                nan = float16_silence_nan(a, fpst);
            }
        }
        if(fpst->default_nan_mode) {
            nan = float16_default_nan(fpst);
        }
        return nan;
    }

    a = float16_squash_input_denormal(a, fpst);

    val16 = float16_val(a);
    sbit = 0x8000 & val16;
    exp = extract32(val16, 10, 5);

    if(exp == 0) {
        return make_float16(deposit32(sbit, 10, 5, 0x1e));
    } else {
        return make_float16(deposit32(sbit, 10, 5, ~exp));
    }
}

float32 HELPER(frecpx_f32)(float32 a, void *fpstp)
{
    float_status *fpst = fpstp;
    uint32_t val32, sbit;
    int32_t exp;

    if(float32_is_any_nan(a)) {
        float32 nan = a;
        if(float32_is_signaling_nan(a, fpst)) {
            float_raise(float_flag_invalid, fpst);
            if(!fpst->default_nan_mode) {
                nan = float32_silence_nan(a, fpst);
            }
        }
        if(fpst->default_nan_mode) {
            nan = float32_default_nan(fpst);
        }
        return nan;
    }

    a = float32_squash_input_denormal(a, fpst);

    val32 = float32_val(a);
    sbit = 0x80000000ULL & val32;
    exp = extract32(val32, 23, 8);

    if(exp == 0) {
        return make_float32(sbit | (0xfe << 23));
    } else {
        return make_float32(sbit | (~exp & 0xff) << 23);
    }
}

float64 HELPER(frecpx_f64)(float64 a, void *fpstp)
{
    float_status *fpst = fpstp;
    uint64_t val64, sbit;
    int64_t exp;

    if(float64_is_any_nan(a)) {
        float64 nan = a;
        if(float64_is_signaling_nan(a, fpst)) {
            float_raise(float_flag_invalid, fpst);
            if(!fpst->default_nan_mode) {
                nan = float64_silence_nan(a, fpst);
            }
        }
        if(fpst->default_nan_mode) {
            nan = float64_default_nan(fpst);
        }
        return nan;
    }

    a = float64_squash_input_denormal(a, fpst);

    val64 = float64_val(a);
    sbit = 0x8000000000000000ULL & val64;
    exp = extract64(float64_val(a), 52, 11);

    if(exp == 0) {
        return make_float64(sbit | (0x7feULL << 52));
    } else {
        return make_float64(sbit | (~exp & 0x7ffULL) << 52);
    }
}

float32 HELPER(fcvtx_f64_to_f32)(float64 a, CPUARMState *env)
{
    /* Von Neumann rounding is implemented by using round-to-zero
     * and then setting the LSB of the result if Inexact was raised.
     */
    float32 r;
    float_status *fpst = &env->vfp.fp_status;
    float_status tstat = *fpst;
    int exflags;

    set_float_rounding_mode(float_round_to_zero, &tstat);
    set_float_exception_flags(0, &tstat);
    r = float64_to_float32(a, &tstat);
    exflags = get_float_exception_flags(&tstat);
    if(exflags & float_flag_inexact) {
        r = make_float32(float32_val(r) | 1);
    }
    exflags |= get_float_exception_flags(fpst);
    set_float_exception_flags(exflags, fpst);
    return r;
}

/* 64-bit versions of the CRC helpers. Note that although the operation
 * (and the prototypes of crc32c() and crc32() mean that only the bottom
 * 32 bits of the accumulator and result are used, we pass and return
 * uint64_t for convenience of the generated code. Unlike the 32-bit
 * instruction set versions, val may genuinely have 64 bits of data in it.
 * The upper bytes of val (above the number specified by 'bytes') must have
 * been zeroed out by the caller.
 */
uint64_t HELPER(crc32_64)(uint64_t acc, uint64_t val, uint32_t bytes)
{
    uint8_t buf[8];

    stq_le_p(buf, val);

    return tlib_crc32(acc, buf, bytes);
}

uint64_t HELPER(crc32c_64)(uint64_t acc, uint64_t val, uint32_t bytes)
{
    uint8_t buf[8];

    stq_le_p(buf, val);

    return calculate_crc32c(acc, buf, bytes);
}

uint64_t HELPER(paired_cmpxchg64_le)(CPUARMState *env, uint64_t addr, uint64_t new_lo, uint64_t new_hi)
{
    __int128_t cmpv = int128_make128(env->exclusive_val, env->exclusive_high);
    __int128_t newv = int128_make128(new_lo, new_hi);
    __int128_t oldv;
    uintptr_t ra = ARM_GETPC();
    uint64_t o0, o1;
    bool success;
    int mem_idx = cpu_mmu_index(env);
    MemOpIdx oi0 = make_memop_idx(MO_LEUQ | MO_ALIGN_16, mem_idx);
    MemOpIdx oi1 = make_memop_idx(MO_LEUQ, mem_idx);

    o0 = cpu_ldq_le_mmu(env, addr + 0, oi0, ra);
    o1 = cpu_ldq_le_mmu(env, addr + 8, oi1, ra);
    oldv = int128_make128(o0, o1);

    success = int128_eq(oldv, cmpv);
    if(success) {
        cpu_stq_le_mmu(env, addr + 0, int128_getlo(newv), oi1, ra);
        cpu_stq_le_mmu(env, addr + 8, int128_gethi(newv), oi1, ra);
    }

    return !success;
}

uint64_t HELPER(paired_cmpxchg64_le_parallel)(CPUARMState *env, uint64_t addr, uint64_t new_lo, uint64_t new_hi)
{
    __int128_t oldv, cmpv, newv;
    uintptr_t ra = ARM_GETPC();
    bool success;
    int mem_idx;
    MemOpIdx oi;

    tlib_assert(HAVE_CMPXCHG128);

    mem_idx = cpu_mmu_index(env);
    oi = make_memop_idx(MO_LE | MO_128 | MO_ALIGN, mem_idx);

    cmpv = int128_make128(env->exclusive_val, env->exclusive_high);
    newv = int128_make128(new_lo, new_hi);
    oldv = cpu_atomic_cmpxchgo_le_mmu(env, addr, cmpv, newv, oi, ra);

    success = int128_eq(oldv, cmpv);
    return !success;
}

uint64_t HELPER(paired_cmpxchg64_be)(CPUARMState *env, uint64_t addr, uint64_t new_lo, uint64_t new_hi)
{
    /*
     * High and low need to be switched here because this is not actually a
     * 128bit store but two doublewords stored consecutively
     */
    __int128_t cmpv = int128_make128(env->exclusive_high, env->exclusive_val);
    __int128_t newv = int128_make128(new_hi, new_lo);
    __int128_t oldv;
    uintptr_t ra = ARM_GETPC();
    uint64_t o0, o1;
    bool success;
    int mem_idx = cpu_mmu_index(env);
    MemOpIdx oi0 = make_memop_idx(MO_BEUQ | MO_ALIGN_16, mem_idx);
    MemOpIdx oi1 = make_memop_idx(MO_BEUQ, mem_idx);

    o1 = cpu_ldq_be_mmu(env, addr + 0, oi0, ra);
    o0 = cpu_ldq_be_mmu(env, addr + 8, oi1, ra);
    oldv = int128_make128(o0, o1);

    success = int128_eq(oldv, cmpv);
    if(success) {
        cpu_stq_be_mmu(env, addr + 0, int128_gethi(newv), oi1, ra);
        cpu_stq_be_mmu(env, addr + 8, int128_getlo(newv), oi1, ra);
    }

    return !success;
}

uint64_t HELPER(paired_cmpxchg64_be_parallel)(CPUARMState *env, uint64_t addr, uint64_t new_lo, uint64_t new_hi)
{
    __int128_t oldv, cmpv, newv;
    uintptr_t ra = ARM_GETPC();
    bool success;
    int mem_idx;
    MemOpIdx oi;

    tlib_assert(HAVE_CMPXCHG128);

    mem_idx = cpu_mmu_index(env);
    oi = make_memop_idx(MO_BE | MO_128 | MO_ALIGN, mem_idx);

    /*
     * High and low need to be switched here because this is not actually a
     * 128bit store but two doublewords stored consecutively
     */
    cmpv = int128_make128(env->exclusive_high, env->exclusive_val);
    newv = int128_make128(new_hi, new_lo);
    oldv = cpu_atomic_cmpxchgo_be_mmu(env, addr, cmpv, newv, oi, ra);

    success = int128_eq(oldv, cmpv);
    return !success;
}

/* Writes back the old data into Rs.  */
void HELPER(casp_le_parallel)(CPUARMState *env, uint32_t rs, uint64_t addr, uint64_t new_lo, uint64_t new_hi)
{
    __int128_t oldv, cmpv, newv;
    uintptr_t ra = ARM_GETPC();
    int mem_idx;
    MemOpIdx oi;

    tlib_assert(HAVE_CMPXCHG128);

    mem_idx = cpu_mmu_index(env);
    oi = make_memop_idx(MO_LE | MO_128 | MO_ALIGN, mem_idx);

    cmpv = int128_make128(env->xregs[rs], env->xregs[rs + 1]);
    newv = int128_make128(new_lo, new_hi);
    oldv = cpu_atomic_cmpxchgo_le_mmu(env, addr, cmpv, newv, oi, ra);

    env->xregs[rs] = int128_getlo(oldv);
    env->xregs[rs + 1] = int128_gethi(oldv);
}

void HELPER(casp_be_parallel)(CPUARMState *env, uint32_t rs, uint64_t addr, uint64_t new_lo, uint64_t new_hi)
{
    __int128_t oldv, cmpv, newv;
    uintptr_t ra = ARM_GETPC();
    int mem_idx;
    MemOpIdx oi;

    tlib_assert(HAVE_CMPXCHG128);

    mem_idx = cpu_mmu_index(env);
    oi = make_memop_idx(MO_LE | MO_128 | MO_ALIGN, mem_idx);

    /*
     * High and low need to be switched here because this is not actually a
     * 128bit store but two doublewords stored consecutively
     */
    cmpv = int128_make128(env->xregs[rs + 1], env->xregs[rs]);
    newv = int128_make128(new_hi, new_lo);
    oldv = cpu_atomic_cmpxchgo_be_mmu(env, addr, cmpv, newv, oi, ra);

    env->xregs[rs + 1] = int128_getlo(oldv);
    env->xregs[rs] = int128_gethi(oldv);
}

/*
 * AdvSIMD half-precision
 */

#define ADVSIMD_HELPER(name, suffix) HELPER(glue(glue(advsimd_, name), suffix))

#define ADVSIMD_HALFOP(name)                                              \
    uint32_t ADVSIMD_HELPER(name, h)(uint32_t a, uint32_t b, void *fpstp) \
    {                                                                     \
        float_status *fpst = fpstp;                                       \
        return float16_##name(a, b, fpst);                                \
    }

ADVSIMD_HALFOP(add)
ADVSIMD_HALFOP(sub)
ADVSIMD_HALFOP(mul)
ADVSIMD_HALFOP(div)
ADVSIMD_HALFOP(min)
ADVSIMD_HALFOP(max)
ADVSIMD_HALFOP(minnum)
ADVSIMD_HALFOP(maxnum)

#define ADVSIMD_TWOHALFOP(name)                                                    \
    uint32_t ADVSIMD_HELPER(name, 2h)(uint32_t two_a, uint32_t two_b, void *fpstp) \
    {                                                                              \
        float16 a1, a2, b1, b2;                                                    \
        uint32_t r1, r2;                                                           \
        float_status *fpst = fpstp;                                                \
        a1 = extract32(two_a, 0, 16);                                              \
        a2 = extract32(two_a, 16, 16);                                             \
        b1 = extract32(two_b, 0, 16);                                              \
        b2 = extract32(two_b, 16, 16);                                             \
        r1 = float16_##name(a1, b1, fpst);                                         \
        r2 = float16_##name(a2, b2, fpst);                                         \
        return deposit32(r1, 16, 16, r2);                                          \
    }

ADVSIMD_TWOHALFOP(add)
ADVSIMD_TWOHALFOP(sub)
ADVSIMD_TWOHALFOP(mul)
ADVSIMD_TWOHALFOP(div)
ADVSIMD_TWOHALFOP(min)
ADVSIMD_TWOHALFOP(max)
ADVSIMD_TWOHALFOP(minnum)
ADVSIMD_TWOHALFOP(maxnum)

/* Data processing - scalar floating-point and advanced SIMD */
static float16 float16_mulx(float16 a, float16 b, void *fpstp)
{
    float_status *fpst = fpstp;

    a = float16_squash_input_denormal(a, fpst);
    b = float16_squash_input_denormal(b, fpst);

    if((float16_is_zero(a) && float16_is_infinity(b)) || (float16_is_infinity(a) && float16_is_zero(b))) {
        /* 2.0 with the sign bit set to sign(A) XOR sign(B) */
        return make_float16((1U << 14) | ((float16_val(a) ^ float16_val(b)) & (1U << 15)));
    }
    return float16_mul(a, b, fpst);
}

ADVSIMD_HALFOP(mulx)
ADVSIMD_TWOHALFOP(mulx)

/* fused multiply-accumulate */
uint32_t HELPER(advsimd_muladdh)(uint32_t a, uint32_t b, uint32_t c, void *fpstp)
{
    float_status *fpst = fpstp;
    return float16_muladd(a, b, c, 0, fpst);
}

uint32_t HELPER(advsimd_muladd2h)(uint32_t two_a, uint32_t two_b, uint32_t two_c, void *fpstp)
{
    float_status *fpst = fpstp;
    float16 a1, a2, b1, b2, c1, c2;
    uint32_t r1, r2;
    a1 = extract32(two_a, 0, 16);
    a2 = extract32(two_a, 16, 16);
    b1 = extract32(two_b, 0, 16);
    b2 = extract32(two_b, 16, 16);
    c1 = extract32(two_c, 0, 16);
    c2 = extract32(two_c, 16, 16);
    r1 = float16_muladd(a1, b1, c1, 0, fpst);
    r2 = float16_muladd(a2, b2, c2, 0, fpst);
    return deposit32(r1, 16, 16, r2);
}

/*
 * Floating point comparisons produce an integer result. Softfloat
 * routines return float_relation types which we convert to the 0/-1
 * Neon requires.
 */

#define ADVSIMD_CMPRES(test) (test) ? 0xffff : 0

uint32_t HELPER(advsimd_ceq_f16)(uint32_t a, uint32_t b, void *fpstp)
{
    float_status *fpst = fpstp;
    int compare = float16_compare_quiet(a, b, fpst);
    return ADVSIMD_CMPRES(compare == float_relation_equal);
}

uint32_t HELPER(advsimd_cge_f16)(uint32_t a, uint32_t b, void *fpstp)
{
    float_status *fpst = fpstp;
    int compare = float16_compare(a, b, fpst);
    return ADVSIMD_CMPRES(compare == float_relation_greater || compare == float_relation_equal);
}

uint32_t HELPER(advsimd_cgt_f16)(uint32_t a, uint32_t b, void *fpstp)
{
    float_status *fpst = fpstp;
    int compare = float16_compare(a, b, fpst);
    return ADVSIMD_CMPRES(compare == float_relation_greater);
}

uint32_t HELPER(advsimd_acge_f16)(uint32_t a, uint32_t b, void *fpstp)
{
    float_status *fpst = fpstp;
    float16 f0 = float16_abs(a);
    float16 f1 = float16_abs(b);
    int compare = float16_compare(f0, f1, fpst);
    return ADVSIMD_CMPRES(compare == float_relation_greater || compare == float_relation_equal);
}

uint32_t HELPER(advsimd_acgt_f16)(uint32_t a, uint32_t b, void *fpstp)
{
    float_status *fpst = fpstp;
    float16 f0 = float16_abs(a);
    float16 f1 = float16_abs(b);
    int compare = float16_compare(f0, f1, fpst);
    return ADVSIMD_CMPRES(compare == float_relation_greater);
}

/* round to integral */
uint32_t HELPER(advsimd_rinth_exact)(uint32_t x, void *fp_status)
{
    return float16_round_to_int(x, fp_status);
}

uint32_t HELPER(advsimd_rinth)(uint32_t x, void *fp_status)
{
    int old_flags = get_float_exception_flags(fp_status), new_flags;
    float16 ret;

    ret = float16_round_to_int(x, fp_status);

    /* Suppress any inexact exceptions the conversion produced */
    if(!(old_flags & float_flag_inexact)) {
        new_flags = get_float_exception_flags(fp_status);
        set_float_exception_flags(new_flags & ~float_flag_inexact, fp_status);
    }

    return ret;
}

/*
 * Half-precision floating point conversion functions
 *
 * There are a multitude of conversion functions with various
 * different rounding modes. This is dealt with by the calling code
 * setting the mode appropriately before calling the helper.
 */

uint32_t HELPER(advsimd_f16tosinth)(uint32_t a, void *fpstp)
{
    float_status *fpst = fpstp;

    /* Invalid if we are passed a NaN */
    if(float16_is_any_nan(a)) {
        float_raise(float_flag_invalid, fpst);
        return 0;
    }
    return float16_to_int16(a, fpst);
}

uint32_t HELPER(advsimd_f16touinth)(uint32_t a, void *fpstp)
{
    float_status *fpst = fpstp;

    /* Invalid if we are passed a NaN */
    if(float16_is_any_nan(a)) {
        float_raise(float_flag_invalid, fpst);
        return 0;
    }
    return float16_to_uint16(a, fpst);
}

static int el_from_spsr(uint32_t spsr)
{
    /* Return the exception level that this SPSR is requesting a return to,
     * or -1 if it is invalid (an illegal return)
     */
    if(spsr & PSTATE_nRW) {
        switch(spsr & CPSR_M) {
            case ARM_CPU_MODE_USR:
                return 0;
            case ARM_CPU_MODE_HYP:
                return 2;
            case ARM_CPU_MODE_FIQ:
            case ARM_CPU_MODE_IRQ:
            case ARM_CPU_MODE_SVC:
            case ARM_CPU_MODE_ABT:
            case ARM_CPU_MODE_UND:
            case ARM_CPU_MODE_SYS:
                return 1;
            case ARM_CPU_MODE_MON:
                /* Returning to Mon from AArch64 is never possible,
                 * so this is an illegal return.
                 */
            default:
                return -1;
        }
    } else {
        if(extract32(spsr, 1, 1)) {
            /* Return with reserved M[1] bit set */
            return -1;
        }
        if(extract32(spsr, 0, 4) == 1) {
            /* return to EL0 with M[0] bit set */
            return -1;
        }
        return extract32(spsr, 2, 2);
    }
}

static void cpsr_write_from_spsr_elx(CPUARMState *env, uint32_t val)
{
    uint32_t mask;

    /* Save SPSR_ELx.SS into PSTATE. */
    env->pstate = (env->pstate & ~PSTATE_SS) | (val & PSTATE_SS);
    val &= ~PSTATE_SS;

    /* Move DIT to the correct location for CPSR */
    if(val & PSTATE_DIT) {
        val &= ~PSTATE_DIT;
        val |= CPSR_DIT;
    }

    mask = aarch32_cpsr_valid_mask(env->features, &env_archcpu(env)->isar);
    cpsr_write(env, val, mask, CPSRWriteRaw);
}

uint32_t cpsr_read_to_spsr_elx(CPUARMState *env)
{
    uint32_t spsr = cpsr_read(env);

    /* Load SS from PSTATE */
    spsr |= env->pstate & PSTATE_SS;

    /* Move DIT to the correct location for SPSR */
    if(spsr & CPSR_DIT) {
        spsr &= ~CPSR_DIT;
        spsr |= PSTATE_DIT;
    }

    return spsr;
}

//  This is just a dummy replacement based on what it causes. PSTATE_SS is supposed
//  to be unset if this function returns true so only abort if it's set.
bool arm_generate_debug_exceptions(CPUState *env)
{
    int cur_el = arm_current_el(env);
    unsigned int spsr_idx = aarch64_banked_spsr_index(cur_el);
    uint32_t spsr = env->banked_spsr[spsr_idx];

    if(spsr & PSTATE_SS) {
        tlib_abort("PSTATE_SS set with arm_generate_debug_exceptions unimplemented.");
    }
    return false;
}

//  TODO: Move to a separate file with our license header.
bool arm_singlestep_active(CPUState *env)
{
    return EX_TBFLAG_ANY(env->hflags, SS_ACTIVE);
}

uint32_t aarch64_pstate_valid_mask(ARMISARegisters *isar)
{
    //  TODO: Implement.
    tlib_printf(LOG_LEVEL_DEBUG, "Masking SPSR with aarch64_pstate_valid_mask skipped");
    return UINT32_MAX;
}

void HELPER(exception_return)(CPUARMState *env, uint64_t new_pc)
{
    int cur_el = arm_current_el(env);
    unsigned int spsr_idx = aarch64_banked_spsr_index(cur_el);
    uint32_t spsr = env->banked_spsr[spsr_idx];
    int new_el;
    bool return_to_aa64 = (spsr & PSTATE_nRW) == 0;

    arm_clear_exclusive(env);
    aarch64_save_sp(env);

    /* We must squash the PSTATE.SS bit to zero unless both of the
     * following hold:
     *  1. debug exceptions are currently disabled
     *  2. singlestep will be active in the EL we return to
     * We check 1 here and 2 after we've done the pstate/cpsr write() to
     * transition to the EL we're going to.
     */
    if(arm_generate_debug_exceptions(env)) {
        spsr &= ~PSTATE_SS;
    }

    new_el = el_from_spsr(spsr);
    if(new_el == -1) {
        goto illegal_return;
    }
    if(new_el > cur_el || (new_el == 2 && !arm_is_el2_enabled(env))) {
        /* Disallow return to an EL which is unimplemented or higher
         * than the current one.
         */
        goto illegal_return;
    }

    if(new_el != 0 && arm_el_is_aa64(env, new_el) != return_to_aa64) {
        /* Return to an EL which is configured for a different register width */
        goto illegal_return;
    }

    if(new_el == 1 && (arm_hcr_el2_eff(env) & HCR_TGE)) {
        goto illegal_return;
    }

    if(!return_to_aa64) {
        env->aarch64 = false;
        /* We do a raw CPSR write because aarch64_sync_64_to_32()
         * will sort the register banks out for us, and we've already
         * caught all the bad-mode cases in el_from_spsr().
         */
        cpsr_write_from_spsr_elx(env, spsr);
        if(!arm_singlestep_active(env)) {
            env->pstate &= ~PSTATE_SS;
        }
        aarch64_sync_64_to_32(env);

        if(spsr & CPSR_T) {
            env->regs[15] = new_pc & ~0x1;
        } else {
            env->regs[15] = new_pc & ~0x3;
        }
        helper_rebuild_hflags_a32(env, new_el);
        tlib_printf(LOG_LEVEL_NOISY,
                    "Exception return from AArch64 EL%d to "
                    "AArch32 EL%d PC 0x%" PRIx32,
                    cur_el, new_el, env->regs[15]);
    } else {
        int tbii;

        env->aarch64 = true;
        spsr &= aarch64_pstate_valid_mask(&env_archcpu(env)->isar);
        pstate_write(env, spsr);
        aarch64_restore_sp(env);
        if(!arm_singlestep_active(env)) {
            env->pstate &= ~PSTATE_SS;
        }
        helper_rebuild_hflags_a64(env, new_el);

        /*
         * Apply TBI to the exception return address.  We had to delay this
         * until after we selected the new EL, so that we could select the
         * correct TBI+TBID bits.  This is made easier by waiting until after
         * the hflags rebuild, since we can pull the composite TBII field
         * from there.
         */
        tbii = EX_TBFLAG_A64(env->hflags, TBII);
        if((tbii >> extract64(new_pc, 55, 1)) & 1) {
            /* TBI is enabled. */
            int core_mmu_idx = cpu_mmu_index(env);
            if(regime_has_2_ranges(core_to_aa64_mmu_idx(core_mmu_idx))) {
                new_pc = sextract64(new_pc, 0, 56);
            } else {
                new_pc = extract64(new_pc, 0, 56);
            }
        }
        env->pc = new_pc;

        tlib_printf(LOG_LEVEL_NOISY,
                    "Exception return from AArch64 EL%d to "
                    "AArch64 EL%d PC 0x%" PRIx64,
                    cur_el, new_el, env->pc);
    }

    /*
     * Note that cur_el can never be 0.  If new_el is 0, then
     * el0_a64 is return_to_aa64, else el0_a64 is ignored.
     */
    aarch64_sve_change_el(env, cur_el, new_el, return_to_aa64);

    return;

illegal_return:
    /* Illegal return events of various kinds have architecturally
     * mandated behaviour:
     * restore NZCV and DAIF from SPSR_ELx
     * set PSTATE.IL
     * restore PC from ELR_ELx
     * no change to exception level, execution state or stack pointer
     */
    env->pstate |= PSTATE_IL;
    env->pc = new_pc;
    spsr &= PSTATE_NZCV | PSTATE_DAIF;
    spsr |= pstate_read(env) & ~(PSTATE_NZCV | PSTATE_DAIF);
    pstate_write(env, spsr);
    if(!arm_singlestep_active(env)) {
        env->pstate &= ~PSTATE_SS;
    }
    helper_rebuild_hflags_a64(env, cur_el);
    tlib_printf(LOG_LEVEL_ERROR,
                "Illegal exception return at EL%d: "
                "resuming execution at 0x%" PRIx64,
                cur_el, env->pc);
}

/*
 * Square Root and Reciprocal square root
 */

uint32_t HELPER(sqrt_f16)(uint32_t a, void *fpstp)
{
    float_status *s = fpstp;

    return float16_sqrt(a, s);
}

void HELPER(dc_zva)(CPUARMState *env, uint64_t vaddr_in)
{
    /*
     * Implement DC ZVA, which zeroes a fixed-length block of memory.
     * Note that we do not implement the (architecturally mandated)
     * alignment fault for attempts to use this on Device memory
     * (which matches the usual QEMU behaviour of not implementing either
     * alignment faults or any memory attribute handling).
     */
    int blocklen = 4 << env_archcpu(env)->dcz_blocksize;
    uint64_t vaddr = vaddr_in & ~(blocklen - 1);
    int mmu_idx = cpu_mmu_index(env);
    target_ulong phys_addr = 0;
    int prot;
    target_ulong page_size;

    if(get_phys_addr(env, vaddr, ACCESS_DATA_STORE, mmu_idx, 0, false, &phys_addr, &prot, &page_size, blocklen) !=
       TRANSLATE_SUCCESS) {
        tlib_printf(LOG_LEVEL_DEBUG, "Incorrect virtual address in DC ZVA: 0x%" PRIx64, vaddr_in);
        return;
    }

    uint8_t *buf = calloc(blocklen, sizeof(uint8_t));
    cpu_physical_memory_rw(phys_addr, buf, blocklen, 1);
    free(buf);
}

//  TODO: Move to a separate file with our license header.
void HELPER(rebuild_hflags_a64)(CPUARMState *env, int el)
{
    const ARMISARegisters *isar = &env->arm_core_config.isar;
    uint64_t sctlr = arm_sctlr(env, el);
    uint64_t tcr = arm_tcr(env, el);

    //  we are rebuilding AAarch64 flags so always 1
    DP_TBFLAG_ANY(env->hflags, AARCH64_STATE, 1);

    //  SS_ACTIVE - software step active
    //  TODO: get correct value after implementation of 'MDSCR_EL1' system register
    //  for now disable
    DP_TBFLAG_ANY(env->hflags, SS_ACTIVE, 0);

    //  BE - big endian data
    DP_TBFLAG_ANY(env->hflags, BE_DATA, arm_cpu_data_is_big_endian(env));

    ARMMMUIdx mmuidx = el_to_arm_mmu_idx(env, el);
    DP_TBFLAG_ANY(env->hflags, MMUIDX, arm_to_core_mmu_idx(mmuidx));

    DP_TBFLAG_ANY(env->hflags, FPEXC_EL, get_fp_exc_el(env, el));

    //  TODO: we only checking SCTLR_ELx.A, but field comment also writes about CCR.UNALIGN_TRP
    DP_TBFLAG_ANY(env->hflags, ALIGN_MEM, sctlr & SCTLR_A);
    DP_TBFLAG_ANY(env->hflags, PSTATE__IL, env->pstate & PSTATE_IL);

    uint8_t tbii = 0;
    if(regime_has_2_ranges(mmuidx)) {
        tbii = extract64(tcr, 37, 2);
    } else {
        //  Two bits are expected from single-range regimes too.
        tbii = extract64(tcr, 20, 1) ? 0b11 : 0;
    }
    DP_TBFLAG_A64(env->hflags, TBII, tbii);

    //  TODO: get correct EL, for now always 3
    DP_TBFLAG_A64(env->hflags, SVEEXC_EL, 3);

    DP_TBFLAG_A64(env->hflags, VL, 0);
    //  TODO: assume not active
    DP_TBFLAG_A64(env->hflags, PAUTH_ACTIVE, 0);

    uint8_t bt = 0;
    switch(el) {
        case 3:
            bt = extract64(sctlr, 36, 1);
            break;
        case 2:
        case 1:
            bt = extract64(sctlr, 35, 2);
            break;
        case 0:
            break;
        default:
            tlib_abortf("Unreachable: %d", el);
    }
    DP_TBFLAG_A64(env->hflags, BT, bt);

    uint8_t tbid = 0;
    //  TBID requires ARMv8.3-PAuth feature.
    if(isar_feature_aa64_pauth(isar)) {
        if(regime_has_2_ranges(mmuidx)) {
            tbid = extract64(tcr, 50, 2);
        } else {
            //  Two bits are expected from single-range regimes too.
            tbid = extract64(tcr, 29, 1) ? 0b11 : 0;
        }
    }
    DP_TBFLAG_A64(env->hflags, TBID, tbid);

    //  D1.1
    if(el != 0) {
        DP_TBFLAG_A64(env->hflags, UNPRIV, 0);
    } else {
        DP_TBFLAG_A64(env->hflags, UNPRIV, 1);
    }
    //  ATA - allocation tag access
    uint8_t ata = 0;
    switch(el) {
        case 3:
            ata = extract64(sctlr, 43, 2);
            break;
        case 2:
        case 1:
            ata = extract64(sctlr, 42, 2);
            break;
        case 0:
            break;
        default:
            tlib_abortf("Unreachable: %d", el);
    }
    DP_TBFLAG_A64(env->hflags, ATA, ata);
    uint8_t tcma = 0;
    switch(el) {
        case 3:
        case 2:
            tcma = extract64(env->cp15.tcr_el[el], 30, 1);
            break;
        case 1:
            tcma = extract64(env->cp15.tcr_el[el], 57, 2);
            break;
        case 0:
            break;
        default:
            tlib_abortf("Unreachable: %d", el);
    }
    DP_TBFLAG_A64(env->hflags, TCMA, tcma);
    //  TODO: assume not active
    //  get correct value after implementation of 'ID_AA64PFR1_EL1' register
    DP_TBFLAG_A64(env->hflags, MTE_ACTIVE, 0);
    //  TODO: assume not active
    //  unprivileged access?
    DP_TBFLAG_A64(env->hflags, MTE0_ACTIVE, 0);
    //  TODO: get correct EL, for now always 3
    DP_TBFLAG_A64(env->hflags, SMEEXC_EL, 3);
    //  TODO: get correct value after implementation of 'SVCR' register
    //  for now always disabled
    DP_TBFLAG_A64(env->hflags, PSTATE_SM, 0);
    DP_TBFLAG_A64(env->hflags, PSTATE_ZA, 0);

    DP_TBFLAG_A64(env->hflags, SVL, 0);
    //  TODO: get correct value, for now disable
    DP_TBFLAG_A64(env->hflags, SME_TRAP_NONSTREAMING, 0);
}

void arm_rebuild_hflags(CPUARMState *env)
{
    //  TODO: Why is this function called by msr_i_daifset/daifclear after setting env->daif?
    if(is_a64(env)) {
        helper_rebuild_hflags_a64(env, arm_current_el(env));
    } else {
        helper_rebuild_hflags_a32(env, arm_current_el(env));
    }
}

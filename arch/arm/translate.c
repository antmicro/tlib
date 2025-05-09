/*
 *  ARM translation
 *
 *  Copyright (c) 2003 Fabrice Bellard
 *  Copyright (c) 2005-2007 CodeSourcery
 *  Copyright (c) 2007 OpenedHand, Ltd.
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

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "cpu.h"
#include "system_registers.h"
#include "ttable.h"
#include "bit_helper.h"

#include "tb-helper.h"

#include "debug.h"

#define abort()                                                   \
    do {                                                          \
        cpu_abort(cpu, "ABORT at %s : %d\n", __FILE__, __LINE__); \
    } while(0)

#define ENABLE_ARCH_4T arm_feature(env, ARM_FEATURE_V4T)
#define ENABLE_ARCH_5  arm_feature(env, ARM_FEATURE_V5)
/* currently all emulated v5 cores are also v5TE, so don't bother */
#define ENABLE_ARCH_5TE  arm_feature(env, ARM_FEATURE_V5)
#define ENABLE_ARCH_5J   0
#define ENABLE_ARCH_6    arm_feature(env, ARM_FEATURE_V6)
#define ENABLE_ARCH_6K   arm_feature(env, ARM_FEATURE_V6K)
#define ENABLE_ARCH_6T2  arm_feature(env, ARM_FEATURE_THUMB2)
#define ENABLE_ARCH_7    arm_feature(env, ARM_FEATURE_V7)
#define ENABLE_ARCH_8    arm_feature(env, ARM_FEATURE_V8)
#define ENABLE_ARCH_8_1M arm_feature(env, ARM_FEATURE_V8_1M)
#define ENABLE_ARCH_MVE  arm_feature(env, ARM_FEATURE_MVE)

//  Masks for coprocessor instruction
#define COPROCESSOR_INSTR_OP_OFFSET              (4)
#define COPROCESSOR_INSTR_OP_MASK                (1 << COPROCESSOR_INSTR_OP_OFFSET)
#define COPROCESSOR_INSTR_OP1_OFFSET             (20)
#define COPROCESSOR_INSTR_OP1_MASK               (0x3f << COPROCESSOR_INSTR_OP1_OFFSET)
#define COPROCESSOR_INSTR_OP1_PARTIAL_MASK(mask) (COPROCESSOR_INSTR_OP1_MASK & ((mask) << COPROCESSOR_INSTR_OP1_OFFSET))

#define ARCH(x)              \
    do {                     \
        if(!ENABLE_ARCH_##x) \
            goto illegal_op; \
    } while(0)

#define TRANS_STATUS_SUCCESS      0
#define TRANS_STATUS_ILLEGAL_INSN 1

/* We reuse the same 64-bit temporaries for efficiency.  */
static TCGv_i64 cpu_V0, cpu_V1, cpu_M0;
static TCGv_i32 cpu_R[16];
#ifdef TARGET_PROTO_ARM_M
static TCGv_i32 cpu_control_ns;
static TCGv_i32 cpu_fpccr_s;
static TCGv_i32 cpu_fpccr_ns;
#endif
static TCGv_i32 cpu_exclusive_val;
static TCGv_i32 cpu_exclusive_high;

/* FIXME:  These should be removed.  */
static TCGv cpu_F0s, cpu_F1s;
static TCGv_i64 cpu_F0d, cpu_F1d;

/* initialize TCG globals.  */
void translate_init(void)
{
    static const char *regnames[] = { "r0", "r1", "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
                                      "r8", "r9", "r10", "r11", "r12", "r13", "r14", "pc" };

    int i;
    for(i = 0; i < 16; i++) {
        cpu_R[i] = tcg_global_mem_new_i32(TCG_AREG0, offsetof(CPUState, regs[i]), regnames[i]);
    }
#ifdef TARGET_PROTO_ARM_M
    cpu_control_ns = tcg_global_mem_new_i32(TCG_AREG0, offsetof(CPUState, v7m.control[M_REG_NS]), "control_ns");
    cpu_fpccr_s = tcg_global_mem_new_i32(TCG_AREG0, offsetof(CPUState, v7m.fpccr[M_REG_S]), "fpccr_s");
    cpu_fpccr_ns = tcg_global_mem_new_i32(TCG_AREG0, offsetof(CPUState, v7m.fpccr[M_REG_NS]), "fpccr_ns");
#endif
#ifdef TARGET_ARM64
    cpu_pc = tcg_global_mem_new(TCG_AREG0, offsetof(CPUState, pc), "pc");
#endif
    cpu_exclusive_val = tcg_global_mem_new_i32(TCG_AREG0, offsetof(CPUState, exclusive_val), "exclusive_val");
    cpu_exclusive_high = tcg_global_mem_new_i32(TCG_AREG0, offsetof(CPUState, exclusive_high), "exclusive_high");
}

/* These instructions trap after executing, so defer them until after the
   conditional executions state has been updated.  */
#define DISAS_WFI 4
#define DISAS_SWI 5
#define DISAS_WFE 6

static inline void gen_set_pc(target_ulong value)
{
#if defined(TARGET_ARM32)
    tcg_gen_movi_i32(cpu_R[15], value);
#elif defined(TARGET_ARM64)
    tcg_gen_movi_tl(cpu_pc, value);
#endif
}

void gen_sync_pc(DisasContext *dc)
{
    gen_set_pc(dc->base.pc);
}

static inline TCGv load_cpu_offset(int offset)
{
    TCGv tmp = tcg_temp_new_i32();
    tcg_gen_ld_i32(tmp, cpu_env, offset);
    return tmp;
}

#define load_cpu_field(name) load_cpu_offset(offsetof(CPUState, name))

static inline void store_cpu_offset(TCGv var, int offset)
{
    tcg_gen_st_i32(var, cpu_env, offset);
    tcg_temp_free_i32(var);
}

#define store_cpu_field(var, name) store_cpu_offset(var, offsetof(CPUState, name))

/* Set a variable to the value of a CPU register.  */
static void load_reg_var(DisasContext *s, TCGv var, int reg)
{
    if(reg == 15) {
        uint32_t addr;
        /* normaly, since we updated PC, we need only to add one insn */
        if(s->thumb) {
            addr = (long)s->base.pc + 2;
        } else {
            addr = (long)s->base.pc + 4;
        }
        tcg_gen_movi_i32(var, addr);
    } else {
        tcg_gen_mov_i32(var, cpu_R[reg]);
    }
}

/* Create a new temporary and set it to the value of a CPU register.  */
static inline TCGv load_reg(DisasContext *s, int reg)
{
    TCGv tmp = tcg_temp_new_i32();
    load_reg_var(s, tmp, reg);
    return tmp;
}

/* Set a CPU register.  The source must be a temporary and will be
   marked as dead.  */
static void store_reg(DisasContext *s, int reg, TCGv var)
{
    if(reg == 15) {
        tcg_gen_andi_i32(var, var, ~1);
        s->base.is_jmp = DISAS_JUMP;
    }
    if(unlikely(s->base.guest_profile) && reg == SP_32) {
        //  Store old SP
        TCGv oldsp = tcg_temp_new_i32();
        tcg_gen_mov_i32(oldsp, cpu_R[SP_32]);

        //  Update SP
        tcg_gen_mov_i32(cpu_R[SP_32], var);

        //  Announce old and new SP values to the guest profiler
        gen_helper_announce_stack_pointer_change(cpu_R[PC_32], oldsp, cpu_R[SP_32]);
        tcg_temp_free_i32(oldsp);
    } else {
        tcg_gen_mov_i32(cpu_R[reg], var);
    }
    tcg_temp_free_i32(var);
}

/* Value extensions.  */
#define gen_uxtb(var) tcg_gen_ext8u_i32(var, var)
#define gen_uxth(var) tcg_gen_ext16u_i32(var, var)
#define gen_sxtb(var) tcg_gen_ext8s_i32(var, var)
#define gen_sxth(var) tcg_gen_ext16s_i32(var, var)

#define gen_sxtb16(var) gen_helper_sxtb16(var, var)
#define gen_uxtb16(var) gen_helper_uxtb16(var, var)

static inline void gen_set_cpsr(TCGv var, uint32_t mask)
{
    TCGv tmp_mask = tcg_const_i32(mask);
    gen_helper_cpsr_write(var, tmp_mask);
    tcg_temp_free_i32(tmp_mask);
}
/* Set NZCV flags from the high 4 bits of var.  */
#define gen_set_nzcv(var) gen_set_cpsr(var, CPSR_NZCV)

static void gen_exception(int excp)
{
    TCGv tmp = tcg_temp_new_i32();
    tcg_gen_movi_i32(tmp, excp);
    gen_helper_exception(tmp);
    tcg_temp_free_i32(tmp);
}

static void gen_smul_dual(TCGv a, TCGv b)
{
    TCGv tmp1 = tcg_temp_new_i32();
    TCGv tmp2 = tcg_temp_new_i32();
    tcg_gen_ext16s_i32(tmp1, a);
    tcg_gen_ext16s_i32(tmp2, b);
    tcg_gen_mul_i32(tmp1, tmp1, tmp2);
    tcg_temp_free_i32(tmp2);
    tcg_gen_sari_i32(a, a, 16);
    tcg_gen_sari_i32(b, b, 16);
    tcg_gen_mul_i32(b, b, a);
    tcg_gen_mov_i32(a, tmp1);
    tcg_temp_free_i32(tmp1);
}

/* Byteswap each halfword.  */
static void gen_rev16(TCGv var)
{
    TCGv tmp = tcg_temp_new_i32();
    tcg_gen_shri_i32(tmp, var, 8);
    tcg_gen_andi_i32(tmp, tmp, 0x00ff00ff);
    tcg_gen_shli_i32(var, var, 8);
    tcg_gen_andi_i32(var, var, 0xff00ff00);
    tcg_gen_or_i32(var, var, tmp);
    tcg_temp_free_i32(tmp);
}

/* Byteswap low halfword and sign extend.  */
static void gen_revsh(TCGv var)
{
    tcg_gen_ext16u_i32(var, var);
    tcg_gen_bswap16_i32(var, var, 0);
    tcg_gen_ext16s_i32(var, var);
}

/* Unsigned bitfield extract.  */
static void gen_ubfx(TCGv var, int shift, uint32_t mask)
{
    if(shift) {
        tcg_gen_shri_i32(var, var, shift);
    }
    tcg_gen_andi_i32(var, var, mask);
}

/* Signed bitfield extract.  */
static void gen_sbfx(TCGv var, int shift, int width)
{
    uint32_t signbit;

    if(shift) {
        tcg_gen_sari_i32(var, var, shift);
    }
    if(shift + width < 32) {
        signbit = 1u << (width - 1);
        tcg_gen_andi_i32(var, var, (1u << width) - 1);
        tcg_gen_xori_i32(var, var, signbit);
        tcg_gen_subi_i32(var, var, signbit);
    }
}

/* Bitfield insertion.  Insert val into base.  Clobbers base and val.  */
static void gen_bfi(TCGv dest, TCGv base, TCGv val, int shift, uint32_t mask)
{
    tcg_gen_andi_i32(val, val, mask);
    tcg_gen_shli_i32(val, val, shift);
    tcg_gen_andi_i32(base, base, ~(mask << shift));
    tcg_gen_or_i32(dest, base, val);
}

/* Return (b << 32) + a. Mark inputs as dead */
static TCGv_i64 gen_addq_msw(TCGv_i64 a, TCGv b)
{
    TCGv_i64 tmp64 = tcg_temp_new_i64();

    tcg_gen_extu_i32_i64(tmp64, b);
    tcg_temp_free_i32(b);
    tcg_gen_shli_i64(tmp64, tmp64, 32);
    tcg_gen_add_i64(a, tmp64, a);

    tcg_temp_free_i64(tmp64);
    return a;
}

/* Return (b << 32) - a. Mark inputs as dead. */
static TCGv_i64 gen_subq_msw(TCGv_i64 a, TCGv b)
{
    TCGv_i64 tmp64 = tcg_temp_new_i64();

    tcg_gen_extu_i32_i64(tmp64, b);
    tcg_temp_free_i32(b);
    tcg_gen_shli_i64(tmp64, tmp64, 32);
    tcg_gen_sub_i64(a, tmp64, a);

    tcg_temp_free_i64(tmp64);
    return a;
}

/* FIXME: Most targets have native widening multiplication.
   It would be good to use that instead of a full wide multiply.  */
/* 32x32->64 multiply.  Marks inputs as dead.  */
static TCGv_i64 gen_mulu_i64_i32(TCGv a, TCGv b)
{
    TCGv_i64 tmp1 = tcg_temp_new_i64();
    TCGv_i64 tmp2 = tcg_temp_new_i64();

    tcg_gen_extu_i32_i64(tmp1, a);
    tcg_temp_free_i32(a);
    tcg_gen_extu_i32_i64(tmp2, b);
    tcg_temp_free_i32(b);
    tcg_gen_mul_i64(tmp1, tmp1, tmp2);
    tcg_temp_free_i64(tmp2);
    return tmp1;
}

static TCGv_i64 gen_muls_i64_i32(TCGv a, TCGv b)
{
    TCGv_i64 tmp1 = tcg_temp_new_i64();
    TCGv_i64 tmp2 = tcg_temp_new_i64();

    tcg_gen_ext_i32_i64(tmp1, a);
    tcg_temp_free_i32(a);
    tcg_gen_ext_i32_i64(tmp2, b);
    tcg_temp_free_i32(b);
    tcg_gen_mul_i64(tmp1, tmp1, tmp2);
    tcg_temp_free_i64(tmp2);
    return tmp1;
}

/* Swap low and high halfwords.  */
static void gen_swap_half(TCGv var)
{
    TCGv tmp = tcg_temp_new_i32();
    tcg_gen_shri_i32(tmp, var, 16);
    tcg_gen_shli_i32(var, var, 16);
    tcg_gen_or_i32(var, var, tmp);
    tcg_temp_free_i32(tmp);
}

/* Dual 16-bit add.  Result placed in t0 and t1 is marked as dead.
    tmp = (t0 ^ t1) & 0x8000;
    t0 &= ~0x8000;
    t1 &= ~0x8000;
    t0 = (t0 + t1) ^ tmp;
 */

static void gen_add16(TCGv t0, TCGv t1)
{
    TCGv tmp = tcg_temp_new_i32();
    tcg_gen_xor_i32(tmp, t0, t1);
    tcg_gen_andi_i32(tmp, tmp, 0x8000);
    tcg_gen_andi_i32(t0, t0, ~0x8000);
    tcg_gen_andi_i32(t1, t1, ~0x8000);
    tcg_gen_add_i32(t0, t0, t1);
    tcg_gen_xor_i32(t0, t0, tmp);
    tcg_temp_free_i32(tmp);
    tcg_temp_free_i32(t1);
}

#define gen_set_CF(var) tcg_gen_st_i32(var, cpu_env, offsetof(CPUState, CF))

/* Set CF to the top bit of var.  */
static void gen_set_CF_bit31(TCGv var)
{
    TCGv tmp = tcg_temp_new_i32();
    tcg_gen_shri_i32(tmp, var, 31);
    gen_set_CF(tmp);
    tcg_temp_free_i32(tmp);
}

/* Set N and Z flags from var.  */
static inline void gen_logic_CC(TCGv var)
{
    tcg_gen_st_i32(var, cpu_env, offsetof(CPUState, NF));
    tcg_gen_st_i32(var, cpu_env, offsetof(CPUState, ZF));
}

/* T0 += T1 + CF.  */
static void gen_adc(TCGv t0, TCGv t1)
{
    TCGv tmp;
    tcg_gen_add_i32(t0, t0, t1);
    tmp = load_cpu_field(CF);
    tcg_gen_add_i32(t0, t0, tmp);
    tcg_temp_free_i32(tmp);
}

/* dest = T0 + T1 + CF. */
static void gen_add_carry(TCGv dest, TCGv t0, TCGv t1)
{
    TCGv tmp;
    tcg_gen_add_i32(dest, t0, t1);
    tmp = load_cpu_field(CF);
    tcg_gen_add_i32(dest, dest, tmp);
    tcg_temp_free_i32(tmp);
}

/* dest = T0 - T1 + CF - 1.  */
static void gen_sub_carry(TCGv dest, TCGv t0, TCGv t1)
{
    TCGv tmp;
    tcg_gen_sub_i32(dest, t0, t1);
    tmp = load_cpu_field(CF);
    tcg_gen_add_i32(dest, dest, tmp);
    tcg_gen_subi_i32(dest, dest, 1);
    tcg_temp_free_i32(tmp);
}

/* FIXME:  Implement this natively.  */
#define tcg_gen_abs_i32(t0, t1) gen_helper_abs(t0, t1)

static void shifter_out_im(TCGv var, int shift)
{
    TCGv tmp = tcg_temp_new_i32();
    if(shift == 0) {
        tcg_gen_andi_i32(tmp, var, 1);
    } else {
        tcg_gen_shri_i32(tmp, var, shift);
        if(shift != 31) {
            tcg_gen_andi_i32(tmp, tmp, 1);
        }
    }
    gen_set_CF(tmp);
    tcg_temp_free_i32(tmp);
}

/* Shift by immediate.  Includes special handling for shift == 0.  */
static inline void gen_arm_shift_im(TCGv var, int shiftop, int shift, int flags)
{
    switch(shiftop) {
        case 0: /* LSL */
            if(shift != 0) {
                if(flags) {
                    shifter_out_im(var, 32 - shift);
                }
                tcg_gen_shli_i32(var, var, shift);
            }
            break;
        case 1: /* LSR */
            if(shift == 0) {
                if(flags) {
                    tcg_gen_shri_i32(var, var, 31);
                    gen_set_CF(var);
                }
                tcg_gen_movi_i32(var, 0);
            } else {
                if(flags) {
                    shifter_out_im(var, shift - 1);
                }
                tcg_gen_shri_i32(var, var, shift);
            }
            break;
        case 2: /* ASR */
            if(shift == 0) {
                shift = 32;
            }
            if(flags) {
                shifter_out_im(var, shift - 1);
            }
            if(shift == 32) {
                shift = 31;
            }
            tcg_gen_sari_i32(var, var, shift);
            break;
        case 3: /* ROR/RRX */
            if(shift != 0) {
                if(flags) {
                    shifter_out_im(var, shift - 1);
                }
                tcg_gen_rotri_i32(var, var, shift);
                break;
            } else {
                TCGv tmp = load_cpu_field(CF);
                if(flags) {
                    shifter_out_im(var, 0);
                }
                tcg_gen_shri_i32(var, var, 1);
                tcg_gen_shli_i32(tmp, tmp, 31);
                tcg_gen_or_i32(var, var, tmp);
                tcg_temp_free_i32(tmp);
            }
    }
};

static inline void gen_arm_shift_reg(TCGv var, int shiftop, TCGv shift, int flags)
{
    if(flags) {
        switch(shiftop) {
            case 0:
                gen_helper_shl_cc(var, var, shift);
                break;
            case 1:
                gen_helper_shr_cc(var, var, shift);
                break;
            case 2:
                gen_helper_sar_cc(var, var, shift);
                break;
            case 3:
                gen_helper_ror_cc(var, var, shift);
                break;
        }
    } else {
        switch(shiftop) {
            case 0:
                gen_helper_shl(var, var, shift);
                break;
            case 1:
                gen_helper_shr(var, var, shift);
                break;
            case 2:
                gen_helper_sar(var, var, shift);
                break;
            case 3:
                tcg_gen_andi_i32(shift, shift, 0x1f);
                tcg_gen_rotr_i32(var, var, shift);
                break;
        }
    }
    tcg_temp_free_i32(shift);
}

#define PAS_OP(pfx)                             \
    switch(op2) {                               \
        case 0:                                 \
            gen_pas_helper(glue(pfx, add16));   \
            break;                              \
        case 1:                                 \
            gen_pas_helper(glue(pfx, addsubx)); \
            break;                              \
        case 2:                                 \
            gen_pas_helper(glue(pfx, subaddx)); \
            break;                              \
        case 3:                                 \
            gen_pas_helper(glue(pfx, sub16));   \
            break;                              \
        case 4:                                 \
            gen_pas_helper(glue(pfx, add8));    \
            break;                              \
        case 7:                                 \
            gen_pas_helper(glue(pfx, sub8));    \
            break;                              \
    }
static void gen_arm_parallel_addsub(int op1, int op2, TCGv a, TCGv b)
{
    TCGv_ptr tmp;

    switch(op1) {
#define gen_pas_helper(name) glue(gen_helper_, name)(a, a, b, tmp)
        case 1:
            tmp = tcg_temp_new_ptr();
            tcg_gen_addi_ptr(tmp, cpu_env, offsetof(CPUState, GE));
            PAS_OP(s)
            tcg_temp_free_ptr(tmp);
            break;
        case 5:
            tmp = tcg_temp_new_ptr();
            tcg_gen_addi_ptr(tmp, cpu_env, offsetof(CPUState, GE));
            PAS_OP(u)
            tcg_temp_free_ptr(tmp);
            break;
#undef gen_pas_helper
#define gen_pas_helper(name) glue(gen_helper_, name)(a, a, b)
        case 2:
            PAS_OP(q);
            break;
        case 3:
            PAS_OP(sh);
            break;
        case 6:
            PAS_OP(uq);
            break;
        case 7:
            PAS_OP(uh);
            break;
#undef gen_pas_helper
    }
}
#undef PAS_OP

/* For unknown reasons Arm and Thumb-2 use arbitrarily different encodings.  */
#define PAS_OP(pfx)                             \
    switch(op1) {                               \
        case 0:                                 \
            gen_pas_helper(glue(pfx, add8));    \
            break;                              \
        case 1:                                 \
            gen_pas_helper(glue(pfx, add16));   \
            break;                              \
        case 2:                                 \
            gen_pas_helper(glue(pfx, addsubx)); \
            break;                              \
        case 4:                                 \
            gen_pas_helper(glue(pfx, sub8));    \
            break;                              \
        case 5:                                 \
            gen_pas_helper(glue(pfx, sub16));   \
            break;                              \
        case 6:                                 \
            gen_pas_helper(glue(pfx, subaddx)); \
            break;                              \
    }
static void gen_thumb2_parallel_addsub(int op1, int op2, TCGv a, TCGv b)
{
    TCGv_ptr tmp;

    switch(op2) {
#define gen_pas_helper(name) glue(gen_helper_, name)(a, a, b, tmp)
        case 0:
            tmp = tcg_temp_new_ptr();
            tcg_gen_addi_ptr(tmp, cpu_env, offsetof(CPUState, GE));
            PAS_OP(s)
            tcg_temp_free_ptr(tmp);
            break;
        case 4:
            tmp = tcg_temp_new_ptr();
            tcg_gen_addi_ptr(tmp, cpu_env, offsetof(CPUState, GE));
            PAS_OP(u)
            tcg_temp_free_ptr(tmp);
            break;
#undef gen_pas_helper
#define gen_pas_helper(name) glue(gen_helper_, name)(a, a, b)
        case 1:
            PAS_OP(q);
            break;
        case 2:
            PAS_OP(sh);
            break;
        case 5:
            PAS_OP(uq);
            break;
        case 6:
            PAS_OP(uh);
            break;
#undef gen_pas_helper
    }
}
#undef PAS_OP

static void gen_test_cc(int cc, int label)
{
    TCGv tmp;
    TCGv tmp2;
    int inv;

    switch(cc) {
        case 0: /* eq: Z */
            tmp = load_cpu_field(ZF);
            tcg_gen_brcondi_i32(TCG_COND_EQ, tmp, 0, label);
            break;
        case 1: /* ne: !Z */
            tmp = load_cpu_field(ZF);
            tcg_gen_brcondi_i32(TCG_COND_NE, tmp, 0, label);
            break;
        case 2: /* cs: C */
            tmp = load_cpu_field(CF);
            tcg_gen_brcondi_i32(TCG_COND_NE, tmp, 0, label);
            break;
        case 3: /* cc: !C */
            tmp = load_cpu_field(CF);
            tcg_gen_brcondi_i32(TCG_COND_EQ, tmp, 0, label);
            break;
        case 4: /* mi: N */
            tmp = load_cpu_field(NF);
            tcg_gen_brcondi_i32(TCG_COND_LT, tmp, 0, label);
            break;
        case 5: /* pl: !N */
            tmp = load_cpu_field(NF);
            tcg_gen_brcondi_i32(TCG_COND_GE, tmp, 0, label);
            break;
        case 6: /* vs: V */
            tmp = load_cpu_field(VF);
            tcg_gen_brcondi_i32(TCG_COND_LT, tmp, 0, label);
            break;
        case 7: /* vc: !V */
            tmp = load_cpu_field(VF);
            tcg_gen_brcondi_i32(TCG_COND_GE, tmp, 0, label);
            break;
        case 8: /* hi: C && !Z */
            inv = gen_new_label();
            tmp = load_cpu_field(CF);
            tcg_gen_brcondi_i32(TCG_COND_EQ, tmp, 0, inv);
            tcg_temp_free_i32(tmp);
            tmp = load_cpu_field(ZF);
            tcg_gen_brcondi_i32(TCG_COND_NE, tmp, 0, label);
            gen_set_label(inv);
            break;
        case 9: /* ls: !C || Z */
            tmp = load_cpu_field(CF);
            tcg_gen_brcondi_i32(TCG_COND_EQ, tmp, 0, label);
            tcg_temp_free_i32(tmp);
            tmp = load_cpu_field(ZF);
            tcg_gen_brcondi_i32(TCG_COND_EQ, tmp, 0, label);
            break;
        case 10: /* ge: N == V -> N ^ V == 0 */
            tmp = load_cpu_field(VF);
            tmp2 = load_cpu_field(NF);
            tcg_gen_xor_i32(tmp, tmp, tmp2);
            tcg_temp_free_i32(tmp2);
            tcg_gen_brcondi_i32(TCG_COND_GE, tmp, 0, label);
            break;
        case 11: /* lt: N != V -> N ^ V != 0 */
            tmp = load_cpu_field(VF);
            tmp2 = load_cpu_field(NF);
            tcg_gen_xor_i32(tmp, tmp, tmp2);
            tcg_temp_free_i32(tmp2);
            tcg_gen_brcondi_i32(TCG_COND_LT, tmp, 0, label);
            break;
        case 12: /* gt: !Z && N == V */
            inv = gen_new_label();
            tmp = load_cpu_field(ZF);
            tcg_gen_brcondi_i32(TCG_COND_EQ, tmp, 0, inv);
            tcg_temp_free_i32(tmp);
            tmp = load_cpu_field(VF);
            tmp2 = load_cpu_field(NF);
            tcg_gen_xor_i32(tmp, tmp, tmp2);
            tcg_temp_free_i32(tmp2);
            tcg_gen_brcondi_i32(TCG_COND_GE, tmp, 0, label);
            gen_set_label(inv);
            break;
        case 13: /* le: Z || N != V */
            tmp = load_cpu_field(ZF);
            tcg_gen_brcondi_i32(TCG_COND_EQ, tmp, 0, label);
            tcg_temp_free_i32(tmp);
            tmp = load_cpu_field(VF);
            tmp2 = load_cpu_field(NF);
            tcg_gen_xor_i32(tmp, tmp, tmp2);
            tcg_temp_free_i32(tmp2);
            tcg_gen_brcondi_i32(TCG_COND_LT, tmp, 0, label);
            break;
        default:
            tlib_abortf("Bad condition code 0x%x\n", cc);
            goto exit_fun;
    }
    tcg_temp_free_i32(tmp);

exit_fun:
    return;
}

static const uint8_t table_logic_cc[16] = {
    1, /* and */
    1, /* xor */
    0, /* sub */
    0, /* rsb */
    0, /* add */
    0, /* adc */
    0, /* sbc */
    0, /* rsc */
    1, /* andl */
    1, /* xorl */
    0, /* cmp */
    0, /* cmn */
    1, /* orr */
    1, /* mov */
    1, /* bic */
    1, /* mvn */
};

/* Set PC and Thumb state from an immediate address.  */
static inline void gen_bx_im(DisasContext *s, uint32_t addr, int stack_announcement_type)
{
    TCGv tmp;
    if(unlikely(s->base.guest_profile)) {
        generate_stack_announcement_imm_i32(addr, stack_announcement_type, true);
    }
    s->base.is_jmp = DISAS_UPDATE;
    if(s->thumb != (addr & 1)) {
        tmp = tcg_temp_new_i32();
        tcg_gen_movi_i32(tmp, addr & 1);
        tcg_gen_st_i32(tmp, cpu_env, offsetof(CPUState, thumb));
        tcg_temp_free_i32(tmp);
    }
    tcg_gen_movi_i32(cpu_R[15], addr & ~1);
}

/* Set PC and Thumb state from var.  var is marked as dead.  */
static inline void gen_bx(DisasContext *s, TCGv var, int stack_announcement_type)
{
    if(unlikely(s->base.guest_profile)) {
        generate_stack_announcement(var, stack_announcement_type, true);
    }
    s->base.is_jmp = DISAS_UPDATE;
#ifdef TARGET_PROTO_ARM_M
    gen_helper_v8m_bx_update_pc(cpu_env, var);
#else
    tcg_gen_andi_i32(cpu_R[15], var, ~1);
    tcg_gen_andi_i32(var, var, 1);
    store_cpu_field(var, thumb);
#endif
}

/* Variant of store_reg which uses branch&exchange logic when storing
   to r15 in ARM architecture v7 and above. The source must be a temporary
   and will be marked as dead. */
static inline void store_reg_bx(CPUState *env, DisasContext *s, int reg, TCGv var)
{
    if(reg == 15 && ENABLE_ARCH_7) {
        /* Mostly arithmetic on the PC, so no stack changes can be detected */
        gen_bx(s, var, STACK_FRAME_NO_CHANGE);
    } else {
        store_reg(s, reg, var);
    }
}

/* Variant of store_reg which uses branch&exchange logic when storing
 * to r15 in ARM architecture v5T and above. This is used for storing
 * the results of a LDR/LDM/POP into r15, and corresponds to the cases
 * in the ARM ARM which use the LoadWritePC() pseudocode function. */
static inline void store_reg_from_load(CPUState *env, DisasContext *s, int reg, TCGv var, int stack_announcement_type)
{
    if(reg == 15 && ENABLE_ARCH_5) {
        gen_bx(s, var, stack_announcement_type);
    } else {
        store_reg(s, reg, var);
    }
}

static inline TCGv gen_ld8s(TCGv addr, int index)
{
    TCGv tmp = tcg_temp_new_i32();
    tcg_gen_qemu_ld8s(tmp, addr, index);
    return tmp;
}
static inline TCGv gen_ld8u(TCGv addr, int index)
{
    TCGv tmp = tcg_temp_new_i32();
    tcg_gen_qemu_ld8u(tmp, addr, index);
    return tmp;
}
static inline TCGv gen_ld16s(TCGv addr, int index)
{
    TCGv tmp = tcg_temp_new_i32();
    tcg_gen_qemu_ld16s(tmp, addr, index);
    return tmp;
}
static inline TCGv gen_ld16u(TCGv addr, int index)
{
    TCGv tmp = tcg_temp_new_i32();
    tcg_gen_qemu_ld16u(tmp, addr, index);
    return tmp;
}
static inline TCGv gen_ld32(TCGv addr, int index)
{
    TCGv tmp = tcg_temp_new_i32();
    tcg_gen_qemu_ld32u(tmp, addr, index);
    return tmp;
}
static inline TCGv_i64 gen_ld64(TCGv addr, int index)
{
    TCGv_i64 tmp = tcg_temp_new_i64();
    tcg_gen_qemu_ld64(tmp, addr, index);
    return tmp;
}
static inline void gen_st8(TCGv val, TCGv addr, int index)
{
    tcg_gen_qemu_st8(val, addr, index);
    tcg_temp_free_i32(val);
}
static inline void gen_st16(TCGv val, TCGv addr, int index)
{
    tcg_gen_qemu_st16(val, addr, index);
    tcg_temp_free_i32(val);
}
static inline void gen_st32(TCGv val, TCGv addr, int index)
{
    tcg_gen_qemu_st32(val, addr, index);
    tcg_temp_free_i32(val);
}
static inline void gen_st64(TCGv_i64 val, TCGv addr, int index)
{
    tcg_gen_qemu_st64(val, addr, index);
    tcg_temp_free_i64(val);
}

static inline void gen_set_pc_im(uint32_t val)
{
    tcg_gen_movi_i32(cpu_R[15], val);
}

/* Always force TB end in addition to generating host memory barrier or
   applying invalidations for dirty addresses from other CPUs. In a scenario
   of software interrupt happening just before the barrier, instructions
   following barrier have to see the changes caused by the interrupt handler.
   This was exposed by Zephyr zero-latency interrupt tests.
   Don't flush the TLB, though: page table update code in guest software
   will contain DSB/ISB, but this is not relevant in tlib as it does not
   emulate caches. */
static inline void gen_barrier(DisasContext *s, bool is_isb)
{
    if(is_isb) {
        gen_helper_invalidate_dirty_addresses_shared(cpu_env);
    } else {
        tcg_gen_mb(TCG_MO_ALL | TCG_BAR_SC);
    }

    gen_set_pc_im(s->base.pc);
    s->base.is_jmp = DISAS_UPDATE;
}

static inline void gen_dxb(DisasContext *s)
{
    gen_barrier(s, /* is_isb: */ false);
}

static inline void gen_isb(DisasContext *s)
{
    gen_barrier(s, /* is_isb: */ true);
}

/* Force a TB lookup after an instruction that changes the CPU state.  */
static inline void gen_lookup_tb(DisasContext *s)
{
    tcg_gen_movi_i32(cpu_R[15], s->base.pc & ~1);
    s->base.is_jmp = DISAS_UPDATE;
}

static inline void gen_add_data_offset(DisasContext *s, unsigned int insn, TCGv var)
{
    int val, rm, shift, shiftop;
    TCGv offset;

    if(!(insn & (1 << 25))) {
        /* immediate */
        val = insn & 0xfff;
        if(!(insn & (1 << 23))) {
            val = -val;
        }
        if(val != 0) {
            tcg_gen_addi_i32(var, var, val);
        }
    } else {
        /* shift/register */
        rm = (insn) & 0xf;
        shift = (insn >> 7) & 0x1f;
        shiftop = (insn >> 5) & 3;
        offset = load_reg(s, rm);
        gen_arm_shift_im(offset, shiftop, shift, 0);
        if(!(insn & (1 << 23))) {
            tcg_gen_sub_i32(var, var, offset);
        } else {
            tcg_gen_add_i32(var, var, offset);
        }
        tcg_temp_free_i32(offset);
    }
}

static inline void gen_add_datah_offset(DisasContext *s, unsigned int insn, int extra, TCGv var)
{
    int val, rm;
    TCGv offset;

    if(insn & (1 << 22)) {
        /* immediate */
        val = (insn & 0xf) | ((insn >> 4) & 0xf0);
        if(!(insn & (1 << 23))) {
            val = -val;
        }
        val += extra;
        if(val != 0) {
            tcg_gen_addi_i32(var, var, val);
        }
    } else {
        /* register */
        if(extra) {
            tcg_gen_addi_i32(var, var, extra);
        }
        rm = (insn) & 0xf;
        offset = load_reg(s, rm);
        if(!(insn & (1 << 23))) {
            tcg_gen_sub_i32(var, var, offset);
        } else {
            tcg_gen_add_i32(var, var, offset);
        }
        tcg_temp_free_i32(offset);
    }
}

static TCGv_ptr get_fpstatus_ptr(int neon)
{
    TCGv_ptr statusptr = tcg_temp_new_ptr();
    int offset;
    if(neon) {
        offset = offsetof(CPUState, vfp.standard_fp_status);
    } else {
        offset = offsetof(CPUState, vfp.fp_status);
    }
    tcg_gen_addi_ptr(statusptr, cpu_env, offset);
    return statusptr;
}

#define VFP_OP2(name)                                                  \
    static inline void gen_vfp_##name(int dp)                          \
    {                                                                  \
        TCGv_ptr fpst = get_fpstatus_ptr(0);                           \
        if(dp) {                                                       \
            gen_helper_vfp_##name##d(cpu_F0d, cpu_F0d, cpu_F1d, fpst); \
        } else {                                                       \
            gen_helper_vfp_##name##s(cpu_F0s, cpu_F0s, cpu_F1s, fpst); \
        }                                                              \
        tcg_temp_free_ptr(fpst);                                       \
    }

VFP_OP2(add)
VFP_OP2(sub)
VFP_OP2(mul)
VFP_OP2(div)

#undef VFP_OP2

static inline void gen_vfp_F1_mul(int dp)
{
    /* Like gen_vfp_mul() but put result in F1 */
    TCGv_ptr fpst = get_fpstatus_ptr(0);
    if(dp) {
        gen_helper_vfp_muld(cpu_F1d, cpu_F0d, cpu_F1d, fpst);
    } else {
        gen_helper_vfp_muls(cpu_F1s, cpu_F0s, cpu_F1s, fpst);
    }
    tcg_temp_free_ptr(fpst);
}

static inline void gen_vfp_F1_neg(int dp)
{
    /* Like gen_vfp_neg() but put result in F1 */
    if(dp) {
        gen_helper_vfp_negd(cpu_F1d, cpu_F0d);
    } else {
        gen_helper_vfp_negs(cpu_F1s, cpu_F0s);
    }
}

static inline void gen_vfp_abs(int dp)
{
    if(dp) {
        gen_helper_vfp_absd(cpu_F0d, cpu_F0d);
    } else {
        gen_helper_vfp_abss(cpu_F0s, cpu_F0s);
    }
}

static inline void gen_vfp_neg(int dp)
{
    if(dp) {
        gen_helper_vfp_negd(cpu_F0d, cpu_F0d);
    } else {
        gen_helper_vfp_negs(cpu_F0s, cpu_F0s);
    }
}

static inline void gen_vfp_sqrt(int dp)
{
    if(dp) {
        gen_helper_vfp_sqrtd(cpu_F0d, cpu_F0d, cpu_env);
    } else {
        gen_helper_vfp_sqrts(cpu_F0s, cpu_F0s, cpu_env);
    }
}

static inline void gen_vfp_cmp(int dp)
{
    if(dp) {
        gen_helper_vfp_cmpd(cpu_F0d, cpu_F1d, cpu_env);
    } else {
        gen_helper_vfp_cmps(cpu_F0s, cpu_F1s, cpu_env);
    }
}

static inline void gen_vfp_cmpe(int dp)
{
    if(dp) {
        gen_helper_vfp_cmped(cpu_F0d, cpu_F1d, cpu_env);
    } else {
        gen_helper_vfp_cmpes(cpu_F0s, cpu_F1s, cpu_env);
    }
}

static inline void gen_vfp_F1_ld0(int dp)
{
    if(dp) {
        tcg_gen_movi_i64(cpu_F1d, 0);
    } else {
        tcg_gen_movi_i32(cpu_F1s, 0);
    }
}

#define VFP_GEN_ITOF(name)                                         \
    static inline void gen_vfp_##name(int dp, int neon)            \
    {                                                              \
        TCGv_ptr statusptr = get_fpstatus_ptr(neon);               \
        if(dp) {                                                   \
            gen_helper_vfp_##name##d(cpu_F0d, cpu_F0s, statusptr); \
        } else {                                                   \
            gen_helper_vfp_##name##s(cpu_F0s, cpu_F0s, statusptr); \
        }                                                          \
        tcg_temp_free_ptr(statusptr);                              \
    }

VFP_GEN_ITOF(uito)
VFP_GEN_ITOF(sito)
#undef VFP_GEN_ITOF

#define VFP_GEN_FTOI(name)                                         \
    static inline void gen_vfp_##name(int dp, int neon)            \
    {                                                              \
        TCGv_ptr statusptr = get_fpstatus_ptr(neon);               \
        if(dp) {                                                   \
            gen_helper_vfp_##name##d(cpu_F0s, cpu_F0d, statusptr); \
        } else {                                                   \
            gen_helper_vfp_##name##s(cpu_F0s, cpu_F0s, statusptr); \
        }                                                          \
        tcg_temp_free_ptr(statusptr);                              \
    }

VFP_GEN_FTOI(toui)
VFP_GEN_FTOI(touiz)
VFP_GEN_FTOI(tosi)
VFP_GEN_FTOI(tosiz)
#undef VFP_GEN_FTOI

#define VFP_GEN_FIX(name)                                                     \
    static inline void gen_vfp_##name(int dp, int shift, int neon)            \
    {                                                                         \
        TCGv tmp_shift = tcg_const_i32(shift);                                \
        TCGv_ptr statusptr = get_fpstatus_ptr(neon);                          \
        if(dp) {                                                              \
            gen_helper_vfp_##name##d(cpu_F0d, cpu_F0d, tmp_shift, statusptr); \
        } else {                                                              \
            gen_helper_vfp_##name##s(cpu_F0s, cpu_F0s, tmp_shift, statusptr); \
        }                                                                     \
        tcg_temp_free_i32(tmp_shift);                                         \
        tcg_temp_free_ptr(statusptr);                                         \
    }
VFP_GEN_FIX(tosh)
VFP_GEN_FIX(tosl)
VFP_GEN_FIX(touh)
VFP_GEN_FIX(toul)
VFP_GEN_FIX(shto)
VFP_GEN_FIX(slto)
VFP_GEN_FIX(uhto)
VFP_GEN_FIX(ulto)
#undef VFP_GEN_FIX

static inline void gen_vfp_ld(DisasContext *s, int dp, TCGv addr)
{
    if(dp) {
        tcg_gen_qemu_ld64(cpu_F0d, addr, context_to_mmu_index(s));
    } else {
        tcg_gen_qemu_ld32u(cpu_F0s, addr, context_to_mmu_index(s));
    }
}

static inline void gen_vfp_st(DisasContext *s, int dp, TCGv addr)
{
    if(dp) {
        tcg_gen_qemu_st64(cpu_F0d, addr, context_to_mmu_index(s));
    } else {
        tcg_gen_qemu_st32(cpu_F0s, addr, context_to_mmu_index(s));
    }
}

static inline long vfp_reg_offset(int dp, int reg)
{
    if(dp) {
        return offsetof(CPUState, vfp.regs[reg]);
    } else if(reg & 1) {
        return offsetof(CPUState, vfp.regs[reg >> 1]) + offsetof(CPU_DoubleU, l.upper);
    } else {
        return offsetof(CPUState, vfp.regs[reg >> 1]) + offsetof(CPU_DoubleU, l.lower);
    }
}

/* Return the offset of a 32-bit piece of a NEON register.
   zero is the least significant end of the register.  */
static inline long neon_reg_offset(int reg, int n)
{
    int sreg;
    sreg = reg * 2 + n;
    return vfp_reg_offset(0, sreg);
}

static TCGv neon_load_reg(int reg, int pass)
{
    TCGv tmp = tcg_temp_new_i32();
    tcg_gen_ld_i32(tmp, cpu_env, neon_reg_offset(reg, pass));
    return tmp;
}

static void neon_store_reg(int reg, int pass, TCGv var)
{
    tcg_gen_st_i32(var, cpu_env, neon_reg_offset(reg, pass));
    tcg_temp_free_i32(var);
}

static inline void neon_load_reg64(TCGv_i64 var, int reg)
{
    tcg_gen_ld_i64(var, cpu_env, vfp_reg_offset(1, reg));
}

static inline void neon_store_reg64(TCGv_i64 var, int reg)
{
    tcg_gen_st_i64(var, cpu_env, vfp_reg_offset(1, reg));
}

#define tcg_gen_ld_f32 tcg_gen_ld_i32
#define tcg_gen_ld_f64 tcg_gen_ld_i64
#define tcg_gen_st_f32 tcg_gen_st_i32
#define tcg_gen_st_f64 tcg_gen_st_i64

static inline void gen_mov_F0_vreg(int dp, int reg)
{
    if(dp) {
        tcg_gen_ld_f64(cpu_F0d, cpu_env, vfp_reg_offset(dp, reg));
    } else {
        tcg_gen_ld_f32(cpu_F0s, cpu_env, vfp_reg_offset(dp, reg));
    }
}

static inline void gen_mov_F1_vreg(int dp, int reg)
{
    if(dp) {
        tcg_gen_ld_f64(cpu_F1d, cpu_env, vfp_reg_offset(dp, reg));
    } else {
        tcg_gen_ld_f32(cpu_F1s, cpu_env, vfp_reg_offset(dp, reg));
    }
}

static inline void gen_mov_vreg_F0(int dp, int reg)
{
    if(dp) {
        tcg_gen_st_f64(cpu_F0d, cpu_env, vfp_reg_offset(dp, reg));
    } else {
        tcg_gen_st_f32(cpu_F0s, cpu_env, vfp_reg_offset(dp, reg));
    }
}

#define ARM_CP_RW_BIT (1 << 20)

static inline void iwmmxt_load_reg(TCGv_i64 var, int reg)
{
    tcg_gen_ld_i64(var, cpu_env, offsetof(CPUState, iwmmxt.regs[reg]));
}

static inline void iwmmxt_store_reg(TCGv_i64 var, int reg)
{
    tcg_gen_st_i64(var, cpu_env, offsetof(CPUState, iwmmxt.regs[reg]));
}

static inline TCGv iwmmxt_load_creg(int reg)
{
    TCGv var = tcg_temp_new_i32();
    tcg_gen_ld_i32(var, cpu_env, offsetof(CPUState, iwmmxt.cregs[reg]));
    return var;
}

static inline void iwmmxt_store_creg(int reg, TCGv var)
{
    tcg_gen_st_i32(var, cpu_env, offsetof(CPUState, iwmmxt.cregs[reg]));
    tcg_temp_free_i32(var);
}

static inline void gen_op_iwmmxt_movq_wRn_M0(int rn)
{
    iwmmxt_store_reg(cpu_M0, rn);
}

static inline void gen_op_iwmmxt_movq_M0_wRn(int rn)
{
    iwmmxt_load_reg(cpu_M0, rn);
}

static inline void gen_op_iwmmxt_orq_M0_wRn(int rn)
{
    iwmmxt_load_reg(cpu_V1, rn);
    tcg_gen_or_i64(cpu_M0, cpu_M0, cpu_V1);
}

static inline void gen_op_iwmmxt_andq_M0_wRn(int rn)
{
    iwmmxt_load_reg(cpu_V1, rn);
    tcg_gen_and_i64(cpu_M0, cpu_M0, cpu_V1);
}

static inline void gen_op_iwmmxt_xorq_M0_wRn(int rn)
{
    iwmmxt_load_reg(cpu_V1, rn);
    tcg_gen_xor_i64(cpu_M0, cpu_M0, cpu_V1);
}

#define IWMMXT_OP(name)                                      \
    static inline void gen_op_iwmmxt_##name##_M0_wRn(int rn) \
    {                                                        \
        iwmmxt_load_reg(cpu_V1, rn);                         \
        gen_helper_iwmmxt_##name(cpu_M0, cpu_M0, cpu_V1);    \
    }

#define IWMMXT_OP_ENV(name)                                        \
    static inline void gen_op_iwmmxt_##name##_M0_wRn(int rn)       \
    {                                                              \
        iwmmxt_load_reg(cpu_V1, rn);                               \
        gen_helper_iwmmxt_##name(cpu_M0, cpu_env, cpu_M0, cpu_V1); \
    }

#define IWMMXT_OP_ENV_SIZE(name) \
    IWMMXT_OP_ENV(name##b)       \
    IWMMXT_OP_ENV(name##w)       \
    IWMMXT_OP_ENV(name##l)

#define IWMMXT_OP_ENV1(name)                               \
    static inline void gen_op_iwmmxt_##name##_M0(void)     \
    {                                                      \
        gen_helper_iwmmxt_##name(cpu_M0, cpu_env, cpu_M0); \
    }

IWMMXT_OP(maddsq)
IWMMXT_OP(madduq)
IWMMXT_OP(sadb)
IWMMXT_OP(sadw)
IWMMXT_OP(mulslw)
IWMMXT_OP(mulshw)
IWMMXT_OP(mululw)
IWMMXT_OP(muluhw)
IWMMXT_OP(macsw)
IWMMXT_OP(macuw)

IWMMXT_OP_ENV_SIZE(unpackl)
IWMMXT_OP_ENV_SIZE(unpackh)

IWMMXT_OP_ENV1(unpacklub)
IWMMXT_OP_ENV1(unpackluw)
IWMMXT_OP_ENV1(unpacklul)
IWMMXT_OP_ENV1(unpackhub)
IWMMXT_OP_ENV1(unpackhuw)
IWMMXT_OP_ENV1(unpackhul)
IWMMXT_OP_ENV1(unpacklsb)
IWMMXT_OP_ENV1(unpacklsw)
IWMMXT_OP_ENV1(unpacklsl)
IWMMXT_OP_ENV1(unpackhsb)
IWMMXT_OP_ENV1(unpackhsw)
IWMMXT_OP_ENV1(unpackhsl)

IWMMXT_OP_ENV_SIZE(cmpeq)
IWMMXT_OP_ENV_SIZE(cmpgtu)
IWMMXT_OP_ENV_SIZE(cmpgts)

IWMMXT_OP_ENV_SIZE(mins)
IWMMXT_OP_ENV_SIZE(minu)
IWMMXT_OP_ENV_SIZE(maxs)
IWMMXT_OP_ENV_SIZE(maxu)

IWMMXT_OP_ENV_SIZE(subn)
IWMMXT_OP_ENV_SIZE(addn)
IWMMXT_OP_ENV_SIZE(subu)
IWMMXT_OP_ENV_SIZE(addu)
IWMMXT_OP_ENV_SIZE(subs)
IWMMXT_OP_ENV_SIZE(adds)

IWMMXT_OP_ENV(avgb0)
IWMMXT_OP_ENV(avgb1)
IWMMXT_OP_ENV(avgw0)
IWMMXT_OP_ENV(avgw1)

IWMMXT_OP_ENV(packuw)
IWMMXT_OP_ENV(packul)
IWMMXT_OP_ENV(packuq)
IWMMXT_OP_ENV(packsw)
IWMMXT_OP_ENV(packsl)
IWMMXT_OP_ENV(packsq)

static void gen_op_iwmmxt_set_mup(void)
{
    TCGv tmp;
    tmp = load_cpu_field(iwmmxt.cregs[ARM_IWMMXT_wCon]);
    tcg_gen_ori_i32(tmp, tmp, 2);
    store_cpu_field(tmp, iwmmxt.cregs[ARM_IWMMXT_wCon]);
}

static void gen_op_iwmmxt_set_cup(void)
{
    TCGv tmp;
    tmp = load_cpu_field(iwmmxt.cregs[ARM_IWMMXT_wCon]);
    tcg_gen_ori_i32(tmp, tmp, 1);
    store_cpu_field(tmp, iwmmxt.cregs[ARM_IWMMXT_wCon]);
}

static void gen_op_iwmmxt_setpsr_nz(void)
{
    TCGv tmp = tcg_temp_new_i32();
    gen_helper_iwmmxt_setpsr_nz(tmp, cpu_M0);
    store_cpu_field(tmp, iwmmxt.cregs[ARM_IWMMXT_wCASF]);
}

static inline void gen_op_iwmmxt_addl_M0_wRn(int rn)
{
    iwmmxt_load_reg(cpu_V1, rn);
    tcg_gen_ext32u_i64(cpu_V1, cpu_V1);
    tcg_gen_add_i64(cpu_M0, cpu_M0, cpu_V1);
}

static inline int gen_iwmmxt_address(DisasContext *s, uint32_t insn, TCGv dest)
{
    int rd;
    uint32_t offset;
    TCGv tmp;

    rd = (insn >> 16) & 0xf;
    tmp = load_reg(s, rd);

    offset = (insn & 0xff) << ((insn >> 7) & 2);
    if(insn & (1 << 24)) {
        /* Pre indexed */
        if(insn & (1 << 23)) {
            tcg_gen_addi_i32(tmp, tmp, offset);
        } else {
            tcg_gen_addi_i32(tmp, tmp, -offset);
        }
        tcg_gen_mov_i32(dest, tmp);
        if(insn & (1 << 21)) {
            store_reg(s, rd, tmp);
        } else {
            tcg_temp_free_i32(tmp);
        }
    } else if(insn & (1 << 21)) {
        /* Post indexed */
        tcg_gen_mov_i32(dest, tmp);
        if(insn & (1 << 23)) {
            tcg_gen_addi_i32(tmp, tmp, offset);
        } else {
            tcg_gen_addi_i32(tmp, tmp, -offset);
        }
        store_reg(s, rd, tmp);
    } else if(!(insn & (1 << 23))) {
        return 1;
    }
    return 0;
}

static inline int gen_iwmmxt_shift(uint32_t insn, uint32_t mask, TCGv dest)
{
    int rd = (insn >> 0) & 0xf;
    TCGv tmp;

    if(insn & (1 << 8)) {
        if(rd < ARM_IWMMXT_wCGR0 || rd > ARM_IWMMXT_wCGR3) {
            return 1;
        } else {
            tmp = iwmmxt_load_creg(rd);
        }
    } else {
        tmp = tcg_temp_new_i32();
        iwmmxt_load_reg(cpu_V0, rd);
        tcg_gen_trunc_i64_i32(tmp, cpu_V0);
    }
    tcg_gen_andi_i32(tmp, tmp, mask);
    tcg_gen_mov_i32(dest, tmp);
    tcg_temp_free_i32(tmp);
    return 0;
}

/* Disassemble an iwMMXt instruction.  Returns nonzero if an error occurred
   (ie. an undefined instruction).  */
static int disas_iwmmxt_insn(CPUState *env, DisasContext *s, uint32_t insn)
{
    int rd, wrd;
    int rdhi, rdlo, rd0, rd1, i;
    TCGv addr;
    TCGv tmp, tmp2, tmp3;

    if((insn & 0x0e000e00) == 0x0c000000) {
        if((insn & 0x0fe00ff0) == 0x0c400000) {
            wrd = insn & 0xf;
            rdlo = (insn >> 12) & 0xf;
            rdhi = (insn >> 16) & 0xf;
            if(insn & ARM_CP_RW_BIT) { /* TMRRC */
                iwmmxt_load_reg(cpu_V0, wrd);
                tcg_gen_trunc_i64_i32(cpu_R[rdlo], cpu_V0);
                tcg_gen_shri_i64(cpu_V0, cpu_V0, 32);
                tcg_gen_trunc_i64_i32(cpu_R[rdhi], cpu_V0);
            } else { /* TMCRR */
                tcg_gen_concat_i32_i64(cpu_V0, cpu_R[rdlo], cpu_R[rdhi]);
                iwmmxt_store_reg(cpu_V0, wrd);
                gen_op_iwmmxt_set_mup();
            }
            return 0;
        }

        wrd = (insn >> 12) & 0xf;
        addr = tcg_temp_new_i32();
        if(gen_iwmmxt_address(s, insn, addr)) {
            tcg_temp_free_i32(addr);
            return 1;
        }
        if(insn & ARM_CP_RW_BIT) {
            if((insn >> 28) == 0xf) { /* WLDRW wCx */
                tmp = tcg_temp_new_i32();
                tcg_gen_qemu_ld32u(tmp, addr, context_to_mmu_index(s));
                iwmmxt_store_creg(wrd, tmp);
            } else {
                i = 1;
                if(insn & (1 << 8)) {
                    if(insn & (1 << 22)) { /* WLDRD */
                        tcg_gen_qemu_ld64(cpu_M0, addr, context_to_mmu_index(s));
                        i = 0;
                    } else { /* WLDRW wRd */
                        tmp = gen_ld32(addr, context_to_mmu_index(s));
                    }
                } else {
                    if(insn & (1 << 22)) { /* WLDRH */
                        tmp = gen_ld16u(addr, context_to_mmu_index(s));
                    } else { /* WLDRB */
                        tmp = gen_ld8u(addr, context_to_mmu_index(s));
                    }
                }
                if(i) {
                    tcg_gen_extu_i32_i64(cpu_M0, tmp);
                    tcg_temp_free_i32(tmp);
                }
                gen_op_iwmmxt_movq_wRn_M0(wrd);
            }
        } else {
            if((insn >> 28) == 0xf) { /* WSTRW wCx */
                tmp = iwmmxt_load_creg(wrd);
                gen_st32(tmp, addr, context_to_mmu_index(s));
            } else {
                gen_op_iwmmxt_movq_M0_wRn(wrd);
                tmp = tcg_temp_new_i32();
                if(insn & (1 << 8)) {
                    if(insn & (1 << 22)) { /* WSTRD */
                        tcg_temp_free_i32(tmp);
                        tcg_gen_qemu_st64(cpu_M0, addr, context_to_mmu_index(s));
                    } else { /* WSTRW wRd */
                        tcg_gen_trunc_i64_i32(tmp, cpu_M0);
                        gen_st32(tmp, addr, context_to_mmu_index(s));
                    }
                } else {
                    if(insn & (1 << 22)) { /* WSTRH */
                        tcg_gen_trunc_i64_i32(tmp, cpu_M0);
                        gen_st16(tmp, addr, context_to_mmu_index(s));
                    } else { /* WSTRB */
                        tcg_gen_trunc_i64_i32(tmp, cpu_M0);
                        gen_st8(tmp, addr, context_to_mmu_index(s));
                    }
                }
            }
        }
        tcg_temp_free_i32(addr);
        return 0;
    }

    if((insn & 0x0f000000) != 0x0e000000) {
        return 1;
    }

    switch(((insn >> 12) & 0xf00) | ((insn >> 4) & 0xff)) {
        case 0x000: /* WOR */
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 0) & 0xf;
            rd1 = (insn >> 16) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            gen_op_iwmmxt_orq_M0_wRn(rd1);
            gen_op_iwmmxt_setpsr_nz();
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            gen_op_iwmmxt_set_cup();
            break;
        case 0x011: /* TMCR */
            if(insn & 0xf) {
                return 1;
            }
            rd = (insn >> 12) & 0xf;
            wrd = (insn >> 16) & 0xf;
            switch(wrd) {
                case ARM_IWMMXT_wCID:
                case ARM_IWMMXT_wCASF:
                    break;
                case ARM_IWMMXT_wCon:
                    gen_op_iwmmxt_set_cup();
                /* Fall through.  */
                case ARM_IWMMXT_wCSSF:
                    tmp = iwmmxt_load_creg(wrd);
                    tmp2 = load_reg(s, rd);
                    tcg_gen_andc_i32(tmp, tmp, tmp2);
                    tcg_temp_free_i32(tmp2);
                    iwmmxt_store_creg(wrd, tmp);
                    break;
                case ARM_IWMMXT_wCGR0:
                case ARM_IWMMXT_wCGR1:
                case ARM_IWMMXT_wCGR2:
                case ARM_IWMMXT_wCGR3:
                    gen_op_iwmmxt_set_cup();
                    tmp = load_reg(s, rd);
                    iwmmxt_store_creg(wrd, tmp);
                    break;
                default:
                    return 1;
            }
            break;
        case 0x100: /* WXOR */
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 0) & 0xf;
            rd1 = (insn >> 16) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            gen_op_iwmmxt_xorq_M0_wRn(rd1);
            gen_op_iwmmxt_setpsr_nz();
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            gen_op_iwmmxt_set_cup();
            break;
        case 0x111: /* TMRC */
            if(insn & 0xf) {
                return 1;
            }
            rd = (insn >> 12) & 0xf;
            wrd = (insn >> 16) & 0xf;
            tmp = iwmmxt_load_creg(wrd);
            store_reg(s, rd, tmp);
            break;
        case 0x300: /* WANDN */
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 0) & 0xf;
            rd1 = (insn >> 16) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            tcg_gen_neg_i64(cpu_M0, cpu_M0);
            gen_op_iwmmxt_andq_M0_wRn(rd1);
            gen_op_iwmmxt_setpsr_nz();
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            gen_op_iwmmxt_set_cup();
            break;
        case 0x200: /* WAND */
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 0) & 0xf;
            rd1 = (insn >> 16) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            gen_op_iwmmxt_andq_M0_wRn(rd1);
            gen_op_iwmmxt_setpsr_nz();
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            gen_op_iwmmxt_set_cup();
            break;
        case 0x810:  //  WMADD
        case 0xa10:
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 0) & 0xf;
            rd1 = (insn >> 16) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            if(insn & (1 << 21)) {
                gen_op_iwmmxt_maddsq_M0_wRn(rd1);
            } else {
                gen_op_iwmmxt_madduq_M0_wRn(rd1);
            }
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            break;
        case 0x10e:  //  WUNPCKIL
        case 0x50e:
        case 0x90e:
        case 0xd0e:
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 16) & 0xf;
            rd1 = (insn >> 0) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            switch((insn >> 22) & 3) {
                case 0:
                    gen_op_iwmmxt_unpacklb_M0_wRn(rd1);
                    break;
                case 1:
                    gen_op_iwmmxt_unpacklw_M0_wRn(rd1);
                    break;
                case 2:
                    gen_op_iwmmxt_unpackll_M0_wRn(rd1);
                    break;
                case 3:
                    return 1;
            }
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            gen_op_iwmmxt_set_cup();
            break;
        case 0x10c:  //  WUNPCKIH
        case 0x50c:
        case 0x90c:
        case 0xd0c:
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 16) & 0xf;
            rd1 = (insn >> 0) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            switch((insn >> 22) & 3) {
                case 0:
                    gen_op_iwmmxt_unpackhb_M0_wRn(rd1);
                    break;
                case 1:
                    gen_op_iwmmxt_unpackhw_M0_wRn(rd1);
                    break;
                case 2:
                    gen_op_iwmmxt_unpackhl_M0_wRn(rd1);
                    break;
                case 3:
                    return 1;
            }
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            gen_op_iwmmxt_set_cup();
            break;
        case 0x012:  //  WSAD
        case 0x112:
        case 0x412:
        case 0x512:
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 16) & 0xf;
            rd1 = (insn >> 0) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            if(insn & (1 << 22)) {
                gen_op_iwmmxt_sadw_M0_wRn(rd1);
            } else {
                gen_op_iwmmxt_sadb_M0_wRn(rd1);
            }
            if(!(insn & (1 << 20))) {
                gen_op_iwmmxt_addl_M0_wRn(wrd);
            }
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            break;
        case 0x010:  //  WMUL
        case 0x110:
        case 0x210:
        case 0x310:
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 16) & 0xf;
            rd1 = (insn >> 0) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            if(insn & (1 << 21)) {
                if(insn & (1 << 20)) {
                    gen_op_iwmmxt_mulshw_M0_wRn(rd1);
                } else {
                    gen_op_iwmmxt_mulslw_M0_wRn(rd1);
                }
            } else {
                if(insn & (1 << 20)) {
                    gen_op_iwmmxt_muluhw_M0_wRn(rd1);
                } else {
                    gen_op_iwmmxt_mululw_M0_wRn(rd1);
                }
            }
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            break;
        case 0x410:  //  WMAC
        case 0x510:
        case 0x610:
        case 0x710:
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 16) & 0xf;
            rd1 = (insn >> 0) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            if(insn & (1 << 21)) {
                gen_op_iwmmxt_macsw_M0_wRn(rd1);
            } else {
                gen_op_iwmmxt_macuw_M0_wRn(rd1);
            }
            if(!(insn & (1 << 20))) {
                iwmmxt_load_reg(cpu_V1, wrd);
                tcg_gen_add_i64(cpu_M0, cpu_M0, cpu_V1);
            }
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            break;
        case 0x006:  //  WCMPEQ
        case 0x406:
        case 0x806:
        case 0xc06:
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 16) & 0xf;
            rd1 = (insn >> 0) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            switch((insn >> 22) & 3) {
                case 0:
                    gen_op_iwmmxt_cmpeqb_M0_wRn(rd1);
                    break;
                case 1:
                    gen_op_iwmmxt_cmpeqw_M0_wRn(rd1);
                    break;
                case 2:
                    gen_op_iwmmxt_cmpeql_M0_wRn(rd1);
                    break;
                case 3:
                    return 1;
            }
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            gen_op_iwmmxt_set_cup();
            break;
        case 0x800:  //  WAVG2
        case 0x900:
        case 0xc00:
        case 0xd00:
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 16) & 0xf;
            rd1 = (insn >> 0) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            if(insn & (1 << 22)) {
                if(insn & (1 << 20)) {
                    gen_op_iwmmxt_avgw1_M0_wRn(rd1);
                } else {
                    gen_op_iwmmxt_avgw0_M0_wRn(rd1);
                }
            } else {
                if(insn & (1 << 20)) {
                    gen_op_iwmmxt_avgb1_M0_wRn(rd1);
                } else {
                    gen_op_iwmmxt_avgb0_M0_wRn(rd1);
                }
            }
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            gen_op_iwmmxt_set_cup();
            break;
        case 0x802:  //  WALIGNR
        case 0x902:
        case 0xa02:
        case 0xb02:
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 16) & 0xf;
            rd1 = (insn >> 0) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            tmp = iwmmxt_load_creg(ARM_IWMMXT_wCGR0 + ((insn >> 20) & 3));
            tcg_gen_andi_i32(tmp, tmp, 7);
            iwmmxt_load_reg(cpu_V1, rd1);
            gen_helper_iwmmxt_align(cpu_M0, cpu_M0, cpu_V1, tmp);
            tcg_temp_free_i32(tmp);
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            break;
        case 0x601:  //  TINSR
        case 0x605:
        case 0x609:
        case 0x60d:
            if(((insn >> 6) & 3) == 3) {
                return 1;
            }
            rd = (insn >> 12) & 0xf;
            wrd = (insn >> 16) & 0xf;
            tmp = load_reg(s, rd);
            gen_op_iwmmxt_movq_M0_wRn(wrd);
            switch((insn >> 6) & 3) {
                case 0:
                    tmp2 = tcg_const_i32(0xff);
                    tmp3 = tcg_const_i32((insn & 7) << 3);
                    break;
                case 1:
                    tmp2 = tcg_const_i32(0xffff);
                    tmp3 = tcg_const_i32((insn & 3) << 4);
                    break;
                case 2:
                    tmp2 = tcg_const_i32(0xffffffff);
                    tmp3 = tcg_const_i32((insn & 1) << 5);
                    break;
                default:
                    TCGV_UNUSED(tmp2);
                    TCGV_UNUSED(tmp3);
            }
            gen_helper_iwmmxt_insr(cpu_M0, cpu_M0, tmp, tmp2, tmp3);
            tcg_temp_free(tmp3);
            tcg_temp_free(tmp2);
            tcg_temp_free_i32(tmp);
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            break;
        case 0x107:  //  TEXTRM
        case 0x507:
        case 0x907:
        case 0xd07:
            rd = (insn >> 12) & 0xf;
            wrd = (insn >> 16) & 0xf;
            if(rd == 15 || ((insn >> 22) & 3) == 3) {
                return 1;
            }
            gen_op_iwmmxt_movq_M0_wRn(wrd);
            tmp = tcg_temp_new_i32();
            switch((insn >> 22) & 3) {
                case 0:
                    tcg_gen_shri_i64(cpu_M0, cpu_M0, (insn & 7) << 3);
                    tcg_gen_trunc_i64_i32(tmp, cpu_M0);
                    if(insn & 8) {
                        tcg_gen_ext8s_i32(tmp, tmp);
                    } else {
                        tcg_gen_andi_i32(tmp, tmp, 0xff);
                    }
                    break;
                case 1:
                    tcg_gen_shri_i64(cpu_M0, cpu_M0, (insn & 3) << 4);
                    tcg_gen_trunc_i64_i32(tmp, cpu_M0);
                    if(insn & 8) {
                        tcg_gen_ext16s_i32(tmp, tmp);
                    } else {
                        tcg_gen_andi_i32(tmp, tmp, 0xffff);
                    }
                    break;
                case 2:
                    tcg_gen_shri_i64(cpu_M0, cpu_M0, (insn & 1) << 5);
                    tcg_gen_trunc_i64_i32(tmp, cpu_M0);
                    break;
            }
            store_reg(s, rd, tmp);
            break;
        case 0x117:  //  TEXTRC
        case 0x517:
        case 0x917:
        case 0xd17:
            if((insn & 0x000ff008) != 0x0003f000 || ((insn >> 22) & 3) == 3) {
                return 1;
            }
            tmp = iwmmxt_load_creg(ARM_IWMMXT_wCASF);
            switch((insn >> 22) & 3) {
                case 0:
                    tcg_gen_shri_i32(tmp, tmp, ((insn & 7) << 2) + 0);
                    break;
                case 1:
                    tcg_gen_shri_i32(tmp, tmp, ((insn & 3) << 3) + 4);
                    break;
                case 2:
                    tcg_gen_shri_i32(tmp, tmp, ((insn & 1) << 4) + 12);
                    break;
            }
            tcg_gen_shli_i32(tmp, tmp, 28);
            gen_set_nzcv(tmp);
            tcg_temp_free_i32(tmp);
            break;
        case 0x401:  //  TBCST
        case 0x405:
        case 0x409:
        case 0x40d:
            if(((insn >> 6) & 3) == 3) {
                return 1;
            }
            rd = (insn >> 12) & 0xf;
            wrd = (insn >> 16) & 0xf;
            tmp = load_reg(s, rd);
            switch((insn >> 6) & 3) {
                case 0:
                    gen_helper_iwmmxt_bcstb(cpu_M0, tmp);
                    break;
                case 1:
                    gen_helper_iwmmxt_bcstw(cpu_M0, tmp);
                    break;
                case 2:
                    gen_helper_iwmmxt_bcstl(cpu_M0, tmp);
                    break;
            }
            tcg_temp_free_i32(tmp);
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            break;
        case 0x113:  //  TANDC
        case 0x513:
        case 0x913:
        case 0xd13:
            if((insn & 0x000ff00f) != 0x0003f000 || ((insn >> 22) & 3) == 3) {
                return 1;
            }
            tmp = iwmmxt_load_creg(ARM_IWMMXT_wCASF);
            tmp2 = tcg_temp_new_i32();
            tcg_gen_mov_i32(tmp2, tmp);
            switch((insn >> 22) & 3) {
                case 0:
                    for(i = 0; i < 7; i++) {
                        tcg_gen_shli_i32(tmp2, tmp2, 4);
                        tcg_gen_and_i32(tmp, tmp, tmp2);
                    }
                    break;
                case 1:
                    for(i = 0; i < 3; i++) {
                        tcg_gen_shli_i32(tmp2, tmp2, 8);
                        tcg_gen_and_i32(tmp, tmp, tmp2);
                    }
                    break;
                case 2:
                    tcg_gen_shli_i32(tmp2, tmp2, 16);
                    tcg_gen_and_i32(tmp, tmp, tmp2);
                    break;
            }
            gen_set_nzcv(tmp);
            tcg_temp_free_i32(tmp2);
            tcg_temp_free_i32(tmp);
            break;
        case 0x01c:  //  WACC
        case 0x41c:
        case 0x81c:
        case 0xc1c:
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 16) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            switch((insn >> 22) & 3) {
                case 0:
                    gen_helper_iwmmxt_addcb(cpu_M0, cpu_M0);
                    break;
                case 1:
                    gen_helper_iwmmxt_addcw(cpu_M0, cpu_M0);
                    break;
                case 2:
                    gen_helper_iwmmxt_addcl(cpu_M0, cpu_M0);
                    break;
                case 3:
                    return 1;
            }
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            break;
        case 0x115:  //  TORC
        case 0x515:
        case 0x915:
        case 0xd15:
            if((insn & 0x000ff00f) != 0x0003f000 || ((insn >> 22) & 3) == 3) {
                return 1;
            }
            tmp = iwmmxt_load_creg(ARM_IWMMXT_wCASF);
            tmp2 = tcg_temp_new_i32();
            tcg_gen_mov_i32(tmp2, tmp);
            switch((insn >> 22) & 3) {
                case 0:
                    for(i = 0; i < 7; i++) {
                        tcg_gen_shli_i32(tmp2, tmp2, 4);
                        tcg_gen_or_i32(tmp, tmp, tmp2);
                    }
                    break;
                case 1:
                    for(i = 0; i < 3; i++) {
                        tcg_gen_shli_i32(tmp2, tmp2, 8);
                        tcg_gen_or_i32(tmp, tmp, tmp2);
                    }
                    break;
                case 2:
                    tcg_gen_shli_i32(tmp2, tmp2, 16);
                    tcg_gen_or_i32(tmp, tmp, tmp2);
                    break;
            }
            gen_set_nzcv(tmp);
            tcg_temp_free_i32(tmp2);
            tcg_temp_free_i32(tmp);
            break;
        case 0x103:  //  TMOVMSK
        case 0x503:
        case 0x903:
        case 0xd03:
            rd = (insn >> 12) & 0xf;
            rd0 = (insn >> 16) & 0xf;
            if((insn & 0xf) != 0 || ((insn >> 22) & 3) == 3) {
                return 1;
            }
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            tmp = tcg_temp_new_i32();
            switch((insn >> 22) & 3) {
                case 0:
                    gen_helper_iwmmxt_msbb(tmp, cpu_M0);
                    break;
                case 1:
                    gen_helper_iwmmxt_msbw(tmp, cpu_M0);
                    break;
                case 2:
                    gen_helper_iwmmxt_msbl(tmp, cpu_M0);
                    break;
            }
            store_reg(s, rd, tmp);
            break;
        case 0x106:  //  WCMPGT
        case 0x306:
        case 0x506:
        case 0x706:
        case 0x906:
        case 0xb06:
        case 0xd06:
        case 0xf06:
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 16) & 0xf;
            rd1 = (insn >> 0) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            switch((insn >> 22) & 3) {
                case 0:
                    if(insn & (1 << 21)) {
                        gen_op_iwmmxt_cmpgtsb_M0_wRn(rd1);
                    } else {
                        gen_op_iwmmxt_cmpgtub_M0_wRn(rd1);
                    }
                    break;
                case 1:
                    if(insn & (1 << 21)) {
                        gen_op_iwmmxt_cmpgtsw_M0_wRn(rd1);
                    } else {
                        gen_op_iwmmxt_cmpgtuw_M0_wRn(rd1);
                    }
                    break;
                case 2:
                    if(insn & (1 << 21)) {
                        gen_op_iwmmxt_cmpgtsl_M0_wRn(rd1);
                    } else {
                        gen_op_iwmmxt_cmpgtul_M0_wRn(rd1);
                    }
                    break;
                case 3:
                    return 1;
            }
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            gen_op_iwmmxt_set_cup();
            break;
        case 0x00e:  //  WUNPCKEL
        case 0x20e:
        case 0x40e:
        case 0x60e:
        case 0x80e:
        case 0xa0e:
        case 0xc0e:
        case 0xe0e:
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 16) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            switch((insn >> 22) & 3) {
                case 0:
                    if(insn & (1 << 21)) {
                        gen_op_iwmmxt_unpacklsb_M0();
                    } else {
                        gen_op_iwmmxt_unpacklub_M0();
                    }
                    break;
                case 1:
                    if(insn & (1 << 21)) {
                        gen_op_iwmmxt_unpacklsw_M0();
                    } else {
                        gen_op_iwmmxt_unpackluw_M0();
                    }
                    break;
                case 2:
                    if(insn & (1 << 21)) {
                        gen_op_iwmmxt_unpacklsl_M0();
                    } else {
                        gen_op_iwmmxt_unpacklul_M0();
                    }
                    break;
                case 3:
                    return 1;
            }
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            gen_op_iwmmxt_set_cup();
            break;
        case 0x00c:  //  WUNPCKEH
        case 0x20c:
        case 0x40c:
        case 0x60c:
        case 0x80c:
        case 0xa0c:
        case 0xc0c:
        case 0xe0c:
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 16) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            switch((insn >> 22) & 3) {
                case 0:
                    if(insn & (1 << 21)) {
                        gen_op_iwmmxt_unpackhsb_M0();
                    } else {
                        gen_op_iwmmxt_unpackhub_M0();
                    }
                    break;
                case 1:
                    if(insn & (1 << 21)) {
                        gen_op_iwmmxt_unpackhsw_M0();
                    } else {
                        gen_op_iwmmxt_unpackhuw_M0();
                    }
                    break;
                case 2:
                    if(insn & (1 << 21)) {
                        gen_op_iwmmxt_unpackhsl_M0();
                    } else {
                        gen_op_iwmmxt_unpackhul_M0();
                    }
                    break;
                case 3:
                    return 1;
            }
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            gen_op_iwmmxt_set_cup();
            break;
        case 0x204:  //  WSRL
        case 0x604:
        case 0xa04:
        case 0xe04:
        case 0x214:
        case 0x614:
        case 0xa14:
        case 0xe14:
            if(((insn >> 22) & 3) == 0) {
                return 1;
            }
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 16) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            tmp = tcg_temp_new_i32();
            if(gen_iwmmxt_shift(insn, 0xff, tmp)) {
                tcg_temp_free_i32(tmp);
                return 1;
            }
            switch((insn >> 22) & 3) {
                case 1:
                    gen_helper_iwmmxt_srlw(cpu_M0, cpu_env, cpu_M0, tmp);
                    break;
                case 2:
                    gen_helper_iwmmxt_srll(cpu_M0, cpu_env, cpu_M0, tmp);
                    break;
                case 3:
                    gen_helper_iwmmxt_srlq(cpu_M0, cpu_env, cpu_M0, tmp);
                    break;
            }
            tcg_temp_free_i32(tmp);
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            gen_op_iwmmxt_set_cup();
            break;
        case 0x004:  //  WSRA
        case 0x404:
        case 0x804:
        case 0xc04:
        case 0x014:
        case 0x414:
        case 0x814:
        case 0xc14:
            if(((insn >> 22) & 3) == 0) {
                return 1;
            }
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 16) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            tmp = tcg_temp_new_i32();
            if(gen_iwmmxt_shift(insn, 0xff, tmp)) {
                tcg_temp_free_i32(tmp);
                return 1;
            }
            switch((insn >> 22) & 3) {
                case 1:
                    gen_helper_iwmmxt_sraw(cpu_M0, cpu_env, cpu_M0, tmp);
                    break;
                case 2:
                    gen_helper_iwmmxt_sral(cpu_M0, cpu_env, cpu_M0, tmp);
                    break;
                case 3:
                    gen_helper_iwmmxt_sraq(cpu_M0, cpu_env, cpu_M0, tmp);
                    break;
            }
            tcg_temp_free_i32(tmp);
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            gen_op_iwmmxt_set_cup();
            break;
        case 0x104:  //  WSLL
        case 0x504:
        case 0x904:
        case 0xd04:
        case 0x114:
        case 0x514:
        case 0x914:
        case 0xd14:
            if(((insn >> 22) & 3) == 0) {
                return 1;
            }
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 16) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            tmp = tcg_temp_new_i32();
            if(gen_iwmmxt_shift(insn, 0xff, tmp)) {
                tcg_temp_free_i32(tmp);
                return 1;
            }
            switch((insn >> 22) & 3) {
                case 1:
                    gen_helper_iwmmxt_sllw(cpu_M0, cpu_env, cpu_M0, tmp);
                    break;
                case 2:
                    gen_helper_iwmmxt_slll(cpu_M0, cpu_env, cpu_M0, tmp);
                    break;
                case 3:
                    gen_helper_iwmmxt_sllq(cpu_M0, cpu_env, cpu_M0, tmp);
                    break;
            }
            tcg_temp_free_i32(tmp);
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            gen_op_iwmmxt_set_cup();
            break;
        case 0x304:  //  WROR
        case 0x704:
        case 0xb04:
        case 0xf04:
        case 0x314:
        case 0x714:
        case 0xb14:
        case 0xf14:
            if(((insn >> 22) & 3) == 0) {
                return 1;
            }
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 16) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            tmp = tcg_temp_new_i32();
            switch((insn >> 22) & 3) {
                case 1:
                    if(gen_iwmmxt_shift(insn, 0xf, tmp)) {
                        tcg_temp_free_i32(tmp);
                        return 1;
                    }
                    gen_helper_iwmmxt_rorw(cpu_M0, cpu_env, cpu_M0, tmp);
                    break;
                case 2:
                    if(gen_iwmmxt_shift(insn, 0x1f, tmp)) {
                        tcg_temp_free_i32(tmp);
                        return 1;
                    }
                    gen_helper_iwmmxt_rorl(cpu_M0, cpu_env, cpu_M0, tmp);
                    break;
                case 3:
                    if(gen_iwmmxt_shift(insn, 0x3f, tmp)) {
                        tcg_temp_free_i32(tmp);
                        return 1;
                    }
                    gen_helper_iwmmxt_rorq(cpu_M0, cpu_env, cpu_M0, tmp);
                    break;
            }
            tcg_temp_free_i32(tmp);
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            gen_op_iwmmxt_set_cup();
            break;
        case 0x116:  //  WMIN
        case 0x316:
        case 0x516:
        case 0x716:
        case 0x916:
        case 0xb16:
        case 0xd16:
        case 0xf16:
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 16) & 0xf;
            rd1 = (insn >> 0) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            switch((insn >> 22) & 3) {
                case 0:
                    if(insn & (1 << 21)) {
                        gen_op_iwmmxt_minsb_M0_wRn(rd1);
                    } else {
                        gen_op_iwmmxt_minub_M0_wRn(rd1);
                    }
                    break;
                case 1:
                    if(insn & (1 << 21)) {
                        gen_op_iwmmxt_minsw_M0_wRn(rd1);
                    } else {
                        gen_op_iwmmxt_minuw_M0_wRn(rd1);
                    }
                    break;
                case 2:
                    if(insn & (1 << 21)) {
                        gen_op_iwmmxt_minsl_M0_wRn(rd1);
                    } else {
                        gen_op_iwmmxt_minul_M0_wRn(rd1);
                    }
                    break;
                case 3:
                    return 1;
            }
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            break;
        case 0x016:  //  WMAX
        case 0x216:
        case 0x416:
        case 0x616:
        case 0x816:
        case 0xa16:
        case 0xc16:
        case 0xe16:
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 16) & 0xf;
            rd1 = (insn >> 0) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            switch((insn >> 22) & 3) {
                case 0:
                    if(insn & (1 << 21)) {
                        gen_op_iwmmxt_maxsb_M0_wRn(rd1);
                    } else {
                        gen_op_iwmmxt_maxub_M0_wRn(rd1);
                    }
                    break;
                case 1:
                    if(insn & (1 << 21)) {
                        gen_op_iwmmxt_maxsw_M0_wRn(rd1);
                    } else {
                        gen_op_iwmmxt_maxuw_M0_wRn(rd1);
                    }
                    break;
                case 2:
                    if(insn & (1 << 21)) {
                        gen_op_iwmmxt_maxsl_M0_wRn(rd1);
                    } else {
                        gen_op_iwmmxt_maxul_M0_wRn(rd1);
                    }
                    break;
                case 3:
                    return 1;
            }
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            break;
        case 0x002:  //  WALIGNI
        case 0x102:
        case 0x202:
        case 0x302:
        case 0x402:
        case 0x502:
        case 0x602:
        case 0x702:
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 16) & 0xf;
            rd1 = (insn >> 0) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            tmp = tcg_const_i32((insn >> 20) & 3);
            iwmmxt_load_reg(cpu_V1, rd1);
            gen_helper_iwmmxt_align(cpu_M0, cpu_M0, cpu_V1, tmp);
            tcg_temp_free(tmp);
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            break;
        case 0x01a:  //  WSUB
        case 0x11a:
        case 0x21a:
        case 0x31a:
        case 0x41a:
        case 0x51a:
        case 0x61a:
        case 0x71a:
        case 0x81a:
        case 0x91a:
        case 0xa1a:
        case 0xb1a:
        case 0xc1a:
        case 0xd1a:
        case 0xe1a:
        case 0xf1a:
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 16) & 0xf;
            rd1 = (insn >> 0) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            switch((insn >> 20) & 0xf) {
                case 0x0:
                    gen_op_iwmmxt_subnb_M0_wRn(rd1);
                    break;
                case 0x1:
                    gen_op_iwmmxt_subub_M0_wRn(rd1);
                    break;
                case 0x3:
                    gen_op_iwmmxt_subsb_M0_wRn(rd1);
                    break;
                case 0x4:
                    gen_op_iwmmxt_subnw_M0_wRn(rd1);
                    break;
                case 0x5:
                    gen_op_iwmmxt_subuw_M0_wRn(rd1);
                    break;
                case 0x7:
                    gen_op_iwmmxt_subsw_M0_wRn(rd1);
                    break;
                case 0x8:
                    gen_op_iwmmxt_subnl_M0_wRn(rd1);
                    break;
                case 0x9:
                    gen_op_iwmmxt_subul_M0_wRn(rd1);
                    break;
                case 0xb:
                    gen_op_iwmmxt_subsl_M0_wRn(rd1);
                    break;
                default:
                    return 1;
            }
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            gen_op_iwmmxt_set_cup();
            break;
        case 0x01e:  //  WSHUFH
        case 0x11e:
        case 0x21e:
        case 0x31e:
        case 0x41e:
        case 0x51e:
        case 0x61e:
        case 0x71e:
        case 0x81e:
        case 0x91e:
        case 0xa1e:
        case 0xb1e:
        case 0xc1e:
        case 0xd1e:
        case 0xe1e:
        case 0xf1e:
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 16) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            tmp = tcg_const_i32(((insn >> 16) & 0xf0) | (insn & 0x0f));
            gen_helper_iwmmxt_shufh(cpu_M0, cpu_env, cpu_M0, tmp);
            tcg_temp_free(tmp);
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            gen_op_iwmmxt_set_cup();
            break;
        case 0x018:  //  WADD
        case 0x118:
        case 0x218:
        case 0x318:
        case 0x418:
        case 0x518:
        case 0x618:
        case 0x718:
        case 0x818:
        case 0x918:
        case 0xa18:
        case 0xb18:
        case 0xc18:
        case 0xd18:
        case 0xe18:
        case 0xf18:
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 16) & 0xf;
            rd1 = (insn >> 0) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            switch((insn >> 20) & 0xf) {
                case 0x0:
                    gen_op_iwmmxt_addnb_M0_wRn(rd1);
                    break;
                case 0x1:
                    gen_op_iwmmxt_addub_M0_wRn(rd1);
                    break;
                case 0x3:
                    gen_op_iwmmxt_addsb_M0_wRn(rd1);
                    break;
                case 0x4:
                    gen_op_iwmmxt_addnw_M0_wRn(rd1);
                    break;
                case 0x5:
                    gen_op_iwmmxt_adduw_M0_wRn(rd1);
                    break;
                case 0x7:
                    gen_op_iwmmxt_addsw_M0_wRn(rd1);
                    break;
                case 0x8:
                    gen_op_iwmmxt_addnl_M0_wRn(rd1);
                    break;
                case 0x9:
                    gen_op_iwmmxt_addul_M0_wRn(rd1);
                    break;
                case 0xb:
                    gen_op_iwmmxt_addsl_M0_wRn(rd1);
                    break;
                default:
                    return 1;
            }
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            gen_op_iwmmxt_set_cup();
            break;
        case 0x008:  //  WPACK
        case 0x108:
        case 0x208:
        case 0x308:
        case 0x408:
        case 0x508:
        case 0x608:
        case 0x708:
        case 0x808:
        case 0x908:
        case 0xa08:
        case 0xb08:
        case 0xc08:
        case 0xd08:
        case 0xe08:
        case 0xf08:
            if(!(insn & (1 << 20)) || ((insn >> 22) & 3) == 0) {
                return 1;
            }
            wrd = (insn >> 12) & 0xf;
            rd0 = (insn >> 16) & 0xf;
            rd1 = (insn >> 0) & 0xf;
            gen_op_iwmmxt_movq_M0_wRn(rd0);
            switch((insn >> 22) & 3) {
                case 1:
                    if(insn & (1 << 21)) {
                        gen_op_iwmmxt_packsw_M0_wRn(rd1);
                    } else {
                        gen_op_iwmmxt_packuw_M0_wRn(rd1);
                    }
                    break;
                case 2:
                    if(insn & (1 << 21)) {
                        gen_op_iwmmxt_packsl_M0_wRn(rd1);
                    } else {
                        gen_op_iwmmxt_packul_M0_wRn(rd1);
                    }
                    break;
                case 3:
                    if(insn & (1 << 21)) {
                        gen_op_iwmmxt_packsq_M0_wRn(rd1);
                    } else {
                        gen_op_iwmmxt_packuq_M0_wRn(rd1);
                    }
                    break;
            }
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            gen_op_iwmmxt_set_cup();
            break;
        case 0x201:
        case 0x203:
        case 0x205:
        case 0x207:
        case 0x209:
        case 0x20b:
        case 0x20d:
        case 0x20f:
        case 0x211:
        case 0x213:
        case 0x215:
        case 0x217:
        case 0x219:
        case 0x21b:
        case 0x21d:
        case 0x21f:
            wrd = (insn >> 5) & 0xf;
            rd0 = (insn >> 12) & 0xf;
            rd1 = (insn >> 0) & 0xf;
            if(rd0 == 0xf || rd1 == 0xf) {
                return 1;
            }
            gen_op_iwmmxt_movq_M0_wRn(wrd);
            tmp = load_reg(s, rd0);
            tmp2 = load_reg(s, rd1);
            switch((insn >> 16) & 0xf) {
                case 0x0: /* TMIA */
                    gen_helper_iwmmxt_muladdsl(cpu_M0, cpu_M0, tmp, tmp2);
                    break;
                case 0x8: /* TMIAPH */
                    gen_helper_iwmmxt_muladdsw(cpu_M0, cpu_M0, tmp, tmp2);
                    break;
                case 0xc:  //  TMIAxy
                case 0xd:
                case 0xe:
                case 0xf:
                    if(insn & (1 << 16)) {
                        tcg_gen_shri_i32(tmp, tmp, 16);
                    }
                    if(insn & (1 << 17)) {
                        tcg_gen_shri_i32(tmp2, tmp2, 16);
                    }
                    gen_helper_iwmmxt_muladdswl(cpu_M0, cpu_M0, tmp, tmp2);
                    break;
                default:
                    tcg_temp_free_i32(tmp2);
                    tcg_temp_free_i32(tmp);
                    return 1;
            }
            tcg_temp_free_i32(tmp2);
            tcg_temp_free_i32(tmp);
            gen_op_iwmmxt_movq_wRn_M0(wrd);
            gen_op_iwmmxt_set_mup();
            break;
        default:
            return 1;
    }

    return 0;
}

/* Disassemble an XScale DSP instruction.  Returns nonzero if an error occurred
   (ie. an undefined instruction).  */
static int disas_dsp_insn(CPUState *env, DisasContext *s, uint32_t insn)
{
    int acc, rd0, rd1, rdhi, rdlo;
    TCGv tmp, tmp2;

    if((insn & 0x0ff00f10) == 0x0e200010) {
        /* Multiply with Internal Accumulate Format */
        rd0 = (insn >> 12) & 0xf;
        rd1 = insn & 0xf;
        acc = (insn >> 5) & 7;

        if(acc != 0) {
            return 1;
        }

        tmp = load_reg(s, rd0);
        tmp2 = load_reg(s, rd1);
        switch((insn >> 16) & 0xf) {
            case 0x0: /* MIA */
                gen_helper_iwmmxt_muladdsl(cpu_M0, cpu_M0, tmp, tmp2);
                break;
            case 0x8: /* MIAPH */
                gen_helper_iwmmxt_muladdsw(cpu_M0, cpu_M0, tmp, tmp2);
                break;
            case 0xc: /* MIABB */
            case 0xd: /* MIABT */
            case 0xe: /* MIATB */
            case 0xf: /* MIATT */
                if(insn & (1 << 16)) {
                    tcg_gen_shri_i32(tmp, tmp, 16);
                }
                if(insn & (1 << 17)) {
                    tcg_gen_shri_i32(tmp2, tmp2, 16);
                }
                gen_helper_iwmmxt_muladdswl(cpu_M0, cpu_M0, tmp, tmp2);
                break;
            default:
                return 1;
        }
        tcg_temp_free_i32(tmp2);
        tcg_temp_free_i32(tmp);

        gen_op_iwmmxt_movq_wRn_M0(acc);
        return 0;
    }

    if((insn & 0x0fe00ff8) == 0x0c400000) {
        /* Internal Accumulator Access Format */
        rdhi = (insn >> 16) & 0xf;
        rdlo = (insn >> 12) & 0xf;
        acc = insn & 7;

        if(acc != 0) {
            return 1;
        }

        if(insn & ARM_CP_RW_BIT) { /* MRA */
            iwmmxt_load_reg(cpu_V0, acc);
            tcg_gen_trunc_i64_i32(cpu_R[rdlo], cpu_V0);
            tcg_gen_shri_i64(cpu_V0, cpu_V0, 32);
            tcg_gen_trunc_i64_i32(cpu_R[rdhi], cpu_V0);
            tcg_gen_andi_i32(cpu_R[rdhi], cpu_R[rdhi], (1 << (40 - 32)) - 1);
        } else { /* MAR */
            tcg_gen_concat_i32_i64(cpu_V0, cpu_R[rdlo], cpu_R[rdhi]);
            iwmmxt_store_reg(cpu_V0, acc);
        }
        return 0;
    }

    return 1;
}

//  Registers that need special handling in userspace
//  Their EL in ttable is 0, but that is not always a case
static bool cp15_special_user_ok(CPUState *env, int user, int is64, int opc1, int crn, int crm, int opc2, bool isread)
{
    if(arm_feature(env, ARM_FEATURE_V7) && crn == 9) {
        /* Performance monitor registers fall into three categories:
         *  (a) always UNDEF in usermode
         *  (b) UNDEF only if PMUSERENR.EN is 0
         *  (c) always read OK and UNDEF on write (PMUSERENR only)
         */
        if((crm == 12 && opc2 < 7 && opc1 == 0) || (crm == 13 && opc2 < 3 && opc1 == 0)) {
            return !((env->cp15.c9_pmuserenr & 1) == 0);
        } else if(crm == 14 && opc2 == 0 && opc1 == 0 && !isread) {
            /* PMUSERENR, read only */
            return false;
        }
    }

    if(crn == 13 && crm == 0) {
        /* TLS register.
         * When TPIDRURO is written to at EL0 - deny access
         */
        if(opc2 == 3 && opc1 == 0 && !isread) {
            return false;
        }
    }

    if(opc1 == 6 && opc2 == 0 && crn == 1 && crm == 0) {
        /* TEEHBR */
        if(!(user && (env->teecr & 1))) {
            return false;
        }
    }

    //  For other normally-handled registers, use EL as defined in TTable
    return true;
}

#define VFP_REG_SHR(x, n)                (((n) > 0) ? (x) >> (n) : (x) << -(n))
#define VFP_SREG(insn, bigbit, smallbit) ((VFP_REG_SHR(insn, bigbit - 1) & 0x1e) | (((insn) >> (smallbit)) & 1))
#define VFP_DREG(reg, insn, bigbit, smallbit)                                            \
    do {                                                                                 \
        if(arm_feature(env, ARM_FEATURE_VFP3)) {                                         \
            reg = (((insn) >> (bigbit)) & 0x0f) | (((insn) >> ((smallbit) - 4)) & 0x10); \
        } else {                                                                         \
            if(insn & (1 << (smallbit)))                                                 \
                return 1;                                                                \
            reg = ((insn) >> (bigbit)) & 0x0f;                                           \
        }                                                                                \
    } while(0)

#define VFP_SREG_D(insn)      VFP_SREG(insn, 12, 22)
#define VFP_DREG_D(reg, insn) VFP_DREG(reg, insn, 12, 22)
#define VFP_SREG_N(insn)      VFP_SREG(insn, 16, 7)
#define VFP_DREG_N(reg, insn) VFP_DREG(reg, insn, 16, 7)
#define VFP_SREG_M(insn)      VFP_SREG(insn, 0, 5)
#define VFP_DREG_M(reg, insn) VFP_DREG(reg, insn, 0, 5)

/* Move between integer and VFP cores.  */
static TCGv gen_vfp_mrs(void)
{
    TCGv tmp = tcg_temp_new_i32();
    tcg_gen_mov_i32(tmp, cpu_F0s);
    return tmp;
}

static void gen_vfp_msr(TCGv tmp)
{
    tcg_gen_mov_i32(cpu_F0s, tmp);
    tcg_temp_free_i32(tmp);
}

static void gen_neon_dup_u8(TCGv var, int shift)
{
    TCGv tmp = tcg_temp_new_i32();
    if(shift) {
        tcg_gen_shri_i32(var, var, shift);
    }
    tcg_gen_ext8u_i32(var, var);
    tcg_gen_shli_i32(tmp, var, 8);
    tcg_gen_or_i32(var, var, tmp);
    tcg_gen_shli_i32(tmp, var, 16);
    tcg_gen_or_i32(var, var, tmp);
    tcg_temp_free_i32(tmp);
}

static void gen_neon_dup_low16(TCGv var)
{
    TCGv tmp = tcg_temp_new_i32();
    tcg_gen_ext16u_i32(var, var);
    tcg_gen_shli_i32(tmp, var, 16);
    tcg_gen_or_i32(var, var, tmp);
    tcg_temp_free_i32(tmp);
}

static void gen_neon_dup_high16(TCGv var)
{
    TCGv tmp = tcg_temp_new_i32();
    tcg_gen_andi_i32(var, var, 0xffff0000);
    tcg_gen_shri_i32(tmp, var, 16);
    tcg_gen_or_i32(var, var, tmp);
    tcg_temp_free_i32(tmp);
}

static TCGv gen_load_and_replicate(DisasContext *s, TCGv addr, int size)
{
    /* Load a single Neon element and replicate into a 32 bit TCG reg */
    TCGv tmp = 0;
    switch(size) {
        case 0:
            tmp = gen_ld8u(addr, context_to_mmu_index(s));
            gen_neon_dup_u8(tmp, 0);
            break;
        case 1:
            tmp = gen_ld16u(addr, context_to_mmu_index(s));
            gen_neon_dup_low16(tmp);
            break;
        case 2:
            tmp = gen_ld32(addr, context_to_mmu_index(s));
            break;
        default: /* Avoid compiler warnings.  */
            abort();
    }
    return tmp;
}

#define VSEL_INSN_MASK  0xff800c50
#define VSEL_INSN_VALUE 0xfe000800

static int generate_vsel_insn(CPUState *env, DisasContext *s, uint32_t insn)
{
    uint32_t rn, rm, rd;
    uint32_t cc = extract32(insn, 20, 2);
    uint32_t size = extract32(insn, 8, 2);

    switch(size) {
        case 1: /* 16-bit */
        case 2: /* 32-bit */
            rm = deposit32(extract32(insn, 5, 1), 1, 31, extract32(insn, 0, 4));
            rn = deposit32(extract32(insn, 7, 1), 1, 31, extract32(insn, 16, 4));
            rd = deposit32(extract32(insn, 22, 1), 1, 31, extract32(insn, 12, 4));
            break;
        case 3: /* 64-bit */
            rm = deposit32(extract32(insn, 0, 4), 4, 28, extract32(insn, 5, 1));
            rn = deposit32(extract32(insn, 16, 4), 4, 28, extract32(insn, 7, 1));
            rd = deposit32(extract32(insn, 12, 4), 4, 28, extract32(insn, 22, 1));
            break;
        default:
            /* Invalid size of the operation */
            return 1;
            break;
    }

    TCGv cpu_zf = load_cpu_field(ZF);
    TCGv cpu_vf = load_cpu_field(VF);
    TCGv cpu_nf = load_cpu_field(NF);

    if(size == 3) {
        TCGv_i64 frn, frm, dest;
        TCGv_i64 tmp, zero, zf, nf, vf;

        zero = tcg_const_i64(0);

        frn = tcg_temp_new_i64();
        frm = tcg_temp_new_i64();
        dest = tcg_temp_new_i64();

        zf = tcg_temp_new_i64();
        nf = tcg_temp_new_i64();
        vf = tcg_temp_new_i64();

        tcg_gen_extu_i32_i64(zf, cpu_zf);
        tcg_gen_extu_i32_i64(nf, cpu_nf);
        tcg_gen_extu_i32_i64(vf, cpu_vf);

        tcg_gen_ld_i64(frn, cpu_env, vfp_reg_offset(1, rn));
        tcg_gen_ld_i64(frm, cpu_env, vfp_reg_offset(1, rm));

        switch(cc) {
            case 0: /* Equal */
                tcg_gen_movcond_i64(TCG_COND_EQ, dest, cpu_zf, zero, frn, frm);
                break;
            case 1: /* Less than */
                tcg_gen_movcond_i64(TCG_COND_LT, dest, cpu_vf, zero, frn, frm);
                break;
            case 2: /* Greater than or equal */
                tmp = tcg_temp_new_i64();
                tcg_gen_xor_i64(tmp, cpu_vf, cpu_nf);
                tcg_gen_movcond_i64(TCG_COND_GE, dest, tmp, zero, frn, frm);
                tcg_temp_free_i64(tmp);
                break;
            case 3: /* Greater than */
                tcg_gen_movcond_i64(TCG_COND_NE, dest, cpu_zf, zero, frn, frm);
                tmp = tcg_temp_new_i64();
                tcg_gen_xor_i64(tmp, cpu_vf, cpu_nf);
                tcg_gen_movcond_i64(TCG_COND_GE, dest, tmp, zero, dest, frm);
                tcg_temp_free_i64(tmp);
                break;
        }

        tcg_gen_st_i64(dest, cpu_env, vfp_reg_offset(1, rd));

        tcg_temp_free_i64(frn);
        tcg_temp_free_i64(frm);
        tcg_temp_free_i64(dest);

        tcg_temp_free_i64(zf);
        tcg_temp_free_i64(nf);
        tcg_temp_free_i64(vf);
    } else {
        TCGv_i32 frn, frm, dest;
        TCGv_i32 tmp, zero;

        zero = tcg_const_i32(0);

        frn = tcg_temp_new_i32();
        frm = tcg_temp_new_i32();
        dest = tcg_temp_new_i32();

        tcg_gen_ld_i32(frn, cpu_env, vfp_reg_offset(0, rn));
        tcg_gen_ld_i32(frm, cpu_env, vfp_reg_offset(0, rm));

        switch(cc) {
            case 0: /* Equal */
                tcg_gen_movcond_i32(TCG_COND_EQ, dest, cpu_zf, zero, frn, frm);
                break;
            case 1: /* Less than */
                tcg_gen_movcond_i32(TCG_COND_LT, dest, cpu_vf, zero, frn, frm);
                break;
            case 2: /* Greater than or equal */
                tmp = tcg_temp_new_i32();
                tcg_gen_xor_i32(tmp, cpu_vf, cpu_nf);
                tcg_gen_movcond_i32(TCG_COND_GE, dest, tmp, zero, frn, frm);
                tcg_temp_free_i32(tmp);
                break;
            case 3: /* Greater than */
                tcg_gen_movcond_i32(TCG_COND_NE, dest, cpu_zf, zero, frn, frm);
                tmp = tcg_temp_new_i32();
                tcg_gen_xor_i32(tmp, cpu_vf, cpu_nf);
                tcg_gen_movcond_i32(TCG_COND_GE, dest, tmp, zero, dest, frm);
                tcg_temp_free_i32(tmp);
                break;
        }

        /* For fp16 the top half is always zeroes */
        if(size == 1) {
            tcg_gen_andi_i32(dest, dest, 0xffff);
        }

        tcg_gen_st_i32(dest, cpu_env, vfp_reg_offset(0, rd));
        tcg_temp_free_i32(dest);
        tcg_temp_free_i32(frn);
        tcg_temp_free_i32(frm);
    }

    tcg_temp_free(cpu_zf);
    tcg_temp_free(cpu_vf);
    tcg_temp_free(cpu_nf);
    return 0;
}

/* Disassemble a VFP instruction.  Returns nonzero if an error occurred
   (ie. an undefined instruction).  */
static int disas_vfp_insn(CPUState *env, DisasContext *s, uint32_t insn)
{
    uint32_t rd, rn, rm, op, i, n, offset, delta_d, delta_m, bank_mask;
    int dp, veclen;
    TCGv addr;
    TCGv tmp;
    TCGv tmp2;

    if(!arm_feature(env, ARM_FEATURE_VFP)) {
        return 1;
    }

    if(!s->vfp_enabled) {
        /* VFP disabled.  Only allow fmxr/fmrx to/from some control regs.  */
        if((insn & 0x0fe00fff) != 0x0ee00a10) {
            return 1;
        }
        rn = (insn >> 16) & 0xf;

        //  TODO: this is a hack for cortex-m. check if this is actually legal to issue fpscr if vfp is disabled.
#ifdef TARGET_PROTO_ARM_M
        if(!(rn == ARM_VFP_FPSCR))
#endif
        {
            if(rn != ARM_VFP_FPSID && rn != ARM_VFP_FPEXC && rn != ARM_VFP_MVFR1 && rn != ARM_VFP_MVFR0) {
                return 1;
            }
        }
    }
#ifdef TARGET_PROTO_ARM_M
    /* Lazy FP state preservation  */
    gen_helper_fp_lsp(cpu_env);
#endif
    if((insn & VSEL_INSN_MASK) == VSEL_INSN_VALUE) {
        return generate_vsel_insn(env, s, insn);
    }

    dp = ((insn & 0xf00) == 0xb00);
    switch((insn >> 24) & 0xf) {
        case 0xe:
            if(insn & (1 << 4)) {
                /* single register transfer */
                rd = (insn >> 12) & 0xf;
                if(dp) {
                    int size;
                    int pass;

                    VFP_DREG_N(rn, insn);
                    if(insn & 0xf) {
                        return 1;
                    }
                    if(insn & 0x00c00060 && !arm_feature(env, ARM_FEATURE_NEON)) {
                        return 1;
                    }

                    pass = (insn >> 21) & 1;
                    if(insn & (1 << 22)) {
                        size = 0;
                        offset = ((insn >> 5) & 3) * 8;
                    } else if(insn & (1 << 5)) {
                        size = 1;
                        offset = (insn & (1 << 6)) ? 16 : 0;
                    } else {
                        size = 2;
                        offset = 0;
                    }
                    if(insn & ARM_CP_RW_BIT) {
                        /* vfp->arm */
                        tmp = neon_load_reg(rn, pass);
                        switch(size) {
                            case 0:
                                if(offset) {
                                    tcg_gen_shri_i32(tmp, tmp, offset);
                                }
                                if(insn & (1 << 23)) {
                                    gen_uxtb(tmp);
                                } else {
                                    gen_sxtb(tmp);
                                }
                                break;
                            case 1:
                                if(insn & (1 << 23)) {
                                    if(offset) {
                                        tcg_gen_shri_i32(tmp, tmp, 16);
                                    } else {
                                        gen_uxth(tmp);
                                    }
                                } else {
                                    if(offset) {
                                        tcg_gen_sari_i32(tmp, tmp, 16);
                                    } else {
                                        gen_sxth(tmp);
                                    }
                                }
                                break;
                            case 2:
                                break;
                        }
                        store_reg(s, rd, tmp);
                    } else {
                        /* arm->vfp */
                        tmp = load_reg(s, rd);
                        if(insn & (1 << 23)) {
                            /* VDUP */
                            if(size == 0) {
                                gen_neon_dup_u8(tmp, 0);
                            } else if(size == 1) {
                                gen_neon_dup_low16(tmp);
                            }
                            for(n = 0; n <= pass * 2; n++) {
                                tmp2 = tcg_temp_new_i32();
                                tcg_gen_mov_i32(tmp2, tmp);
                                neon_store_reg(rn, n, tmp2);
                            }
                            neon_store_reg(rn, n, tmp);
                        } else {
                            /* VMOV */
                            switch(size) {
                                case 0:
                                    tmp2 = neon_load_reg(rn, pass);
                                    gen_bfi(tmp, tmp2, tmp, offset, 0xff);
                                    tcg_temp_free_i32(tmp2);
                                    break;
                                case 1:
                                    tmp2 = neon_load_reg(rn, pass);
                                    gen_bfi(tmp, tmp2, tmp, offset, 0xffff);
                                    tcg_temp_free_i32(tmp2);
                                    break;
                                case 2:
                                    break;
                            }
                            neon_store_reg(rn, pass, tmp);
                        }
                    }
                } else { /* !dp */
                    if((insn & 0x6f) != 0x00) {
                        return 1;
                    }
                    rn = VFP_SREG_N(insn);
                    if(insn & ARM_CP_RW_BIT) {
                        /* vfp->arm */
                        if(insn & (1 << 21)) {
                            /* system register */
                            rn >>= 1;

                            switch(rn) {
                                case ARM_VFP_FPSID:
                                    /* VFP2 allows access to FSID from userspace.
                                       VFP3 restricts all id registers to privileged
                                       accesses.  */
                                    if(s->user && arm_feature(env, ARM_FEATURE_VFP3)) {
                                        return 1;
                                    }
                                    tmp = load_cpu_field(vfp.xregs[rn]);
                                    break;
                                case ARM_VFP_FPEXC:
                                    if(s->user) {
                                        return 1;
                                    }
                                    tmp = load_cpu_field(vfp.xregs[rn]);
                                    break;
                                case ARM_VFP_FPINST:
                                case ARM_VFP_FPINST2:
                                    /* Not present in VFP3.  */
                                    if(s->user || arm_feature(env, ARM_FEATURE_VFP3)) {
                                        return 1;
                                    }
                                    tmp = load_cpu_field(vfp.xregs[rn]);
                                    break;
                                case ARM_VFP_FPSCR:
                                    if(rd == 15) {
                                        tmp = load_cpu_field(vfp.xregs[ARM_VFP_FPSCR]);
                                        tcg_gen_andi_i32(tmp, tmp, 0xf0000000);
                                    } else {
                                        tmp = tcg_temp_new_i32();
                                        gen_helper_vfp_get_fpscr(tmp, cpu_env);
                                    }
                                    break;
                                case ARM_VFP_MVFR0:
                                case ARM_VFP_MVFR1:
                                    if(s->user || !arm_feature(env, ARM_FEATURE_VFP3)) {
                                        return 1;
                                    }
                                    tmp = load_cpu_field(vfp.xregs[rn]);
                                    break;
                                default:
                                    return 1;
                            }
                        } else {
                            gen_mov_F0_vreg(0, rn);
                            tmp = gen_vfp_mrs();
                        }
                        if(rd == 15) {
                            /* Set the 4 flag bits in the CPSR.  */
                            gen_set_nzcv(tmp);
                            tcg_temp_free_i32(tmp);
                        } else {
                            store_reg(s, rd, tmp);
                        }
                    } else {
                        /* arm->vfp */
                        tmp = load_reg(s, rd);
                        if(insn & (1 << 21)) {
                            rn >>= 1;
                            /* system register */
                            switch(rn) {
                                case ARM_VFP_FPSID:
                                case ARM_VFP_MVFR0:
                                case ARM_VFP_MVFR1:
                                    /* Writes are ignored.  */
                                    break;
                                case ARM_VFP_FPSCR:
                                    gen_helper_vfp_set_fpscr(cpu_env, tmp);
                                    tcg_temp_free_i32(tmp);
                                    gen_lookup_tb(s);
                                    break;
                                case ARM_VFP_FPEXC:
                                    if(s->user) {
                                        return 1;
                                    }
                                    /* TODO: VFP subarchitecture support.
                                     * For now, keep the EN bit only */
                                    tcg_gen_andi_i32(tmp, tmp, 1 << 30);
                                    store_cpu_field(tmp, vfp.xregs[rn]);
                                    gen_lookup_tb(s);
                                    break;
                                case ARM_VFP_FPINST:
                                case ARM_VFP_FPINST2:
                                    store_cpu_field(tmp, vfp.xregs[rn]);
                                    break;
                                default:
                                    return 1;
                            }
                        } else {
                            gen_vfp_msr(tmp);
                            gen_mov_vreg_F0(0, rn);
                        }
                    }
                }
            } else {
                /* data processing */
                /* The opcode is in bits 23, 21, 20 and 6.  */
                op = ((insn >> 20) & 8) | ((insn >> 19) & 6) | ((insn >> 6) & 1);
                if(dp) {
                    if(op == 15) {
                        /* rn is opcode */
                        rn = ((insn >> 15) & 0x1e) | ((insn >> 7) & 1);
                    } else {
                        /* rn is register number */
                        VFP_DREG_N(rn, insn);
                    }

                    if(op == 15 && (rn == 15 || ((rn & 0x1c) == 0x18))) {
                        /* Integer or single precision destination.  */
                        rd = VFP_SREG_D(insn);
                    } else {
                        VFP_DREG_D(rd, insn);
                    }
                    if(op == 15 && (((rn & 0x1c) == 0x10) || ((rn & 0x14) == 0x14))) {
                        /* VCVT from int is always from S reg regardless of dp bit.
                         * VCVT with immediate frac_bits has same format as SREG_M
                         */
                        rm = VFP_SREG_M(insn);
                    } else {
                        VFP_DREG_M(rm, insn);
                    }
                } else {
                    rn = VFP_SREG_N(insn);
                    if(op == 15 && rn == 15) {
                        /* Double precision destination.  */
                        VFP_DREG_D(rd, insn);
                    } else {
                        rd = VFP_SREG_D(insn);
                    }
                    /* NB that we implicitly rely on the encoding for the frac_bits
                     * in VCVT of fixed to float being the same as that of an SREG_M
                     */
                    rm = VFP_SREG_M(insn);
                }

                veclen = s->vec_len;
                if(op == 15 && rn > 3) {
                    veclen = 0;
                }

                /* Shut up compiler warnings.  */
                delta_m = 0;
                delta_d = 0;
                bank_mask = 0;

                if(veclen > 0) {
                    if(dp) {
                        bank_mask = 0xc;
                    } else {
                        bank_mask = 0x18;
                    }

                    /* Figure out what type of vector operation this is.  */
                    if((rd & bank_mask) == 0) {
                        /* scalar */
                        veclen = 0;
                    } else {
                        if(dp) {
                            delta_d = (s->vec_stride >> 1) + 1;
                        } else {
                            delta_d = s->vec_stride + 1;
                        }

                        if((rm & bank_mask) == 0) {
                            /* mixed scalar/vector */
                            delta_m = 0;
                        } else {
                            /* vector */
                            delta_m = delta_d;
                        }
                    }
                }

                /* Load the initial operands.  */
                if(op == 15) {
                    switch(rn) {
                        case 16:
                        case 17:
                            /* Integer source */
                            gen_mov_F0_vreg(0, rm);
                            break;
                        case 8:
                        case 9:
                            /* Compare */
                            gen_mov_F0_vreg(dp, rd);
                            gen_mov_F1_vreg(dp, rm);
                            break;
                        case 10:
                        case 11:
                            /* Compare with zero */
                            gen_mov_F0_vreg(dp, rd);
                            gen_vfp_F1_ld0(dp);
                            break;
                        case 20:
                        case 21:
                        case 22:
                        case 23:
                        case 28:
                        case 29:
                        case 30:
                        case 31:
                            /* Source and destination the same.  */
                            gen_mov_F0_vreg(dp, rd);
                            break;
                        case 4:
                        case 5:
                        case 6:
                        case 7:
                            /* VCVTB, VCVTT: only present with the halfprec extension,
                             * UNPREDICTABLE if bit 8 is set (we choose to UNDEF)
                             */
                            if(dp || !arm_feature(env, ARM_FEATURE_VFP_FP16)) {
                                return 1;
                            }
                            /* Otherwise fall through */
                            goto case_default;
                        default:
                        case_default:
                            /* One source operand.  */
                            gen_mov_F0_vreg(dp, rm);
                            break;
                    }
                } else {
                    /* Two source operands.  */
                    gen_mov_F0_vreg(dp, rn);
                    gen_mov_F1_vreg(dp, rm);
                }

                for(;;) {
                    /* Perform the calculation.  */
                    switch(op) {
                        case 0: /* VMLA: fd + (fn * fm) */
                            /* Note that order of inputs to the add matters for NaNs */
                            gen_vfp_F1_mul(dp);
                            gen_mov_F0_vreg(dp, rd);
                            gen_vfp_add(dp);
                            break;
                        case 1: /* VMLS: fd + -(fn * fm) */
                            gen_vfp_mul(dp);
                            gen_vfp_F1_neg(dp);
                            gen_mov_F0_vreg(dp, rd);
                            gen_vfp_add(dp);
                            break;
                        case 2: /* VNMLS: -fd + (fn * fm) */
                            /* Note that it isn't valid to replace (-A + B) with (B - A)
                             * or similar plausible looking simplifications
                             * because this will give wrong results for NaNs.
                             */
                            gen_vfp_F1_mul(dp);
                            gen_mov_F0_vreg(dp, rd);
                            gen_vfp_neg(dp);
                            gen_vfp_add(dp);
                            break;
                        case 3: /* VNMLA: -fd + -(fn * fm) */
                            gen_vfp_mul(dp);
                            gen_vfp_F1_neg(dp);
                            gen_mov_F0_vreg(dp, rd);
                            gen_vfp_neg(dp);
                            gen_vfp_add(dp);
                            break;
                        case 4: /* mul: fn * fm */
                            gen_vfp_mul(dp);
                            break;
                        case 5: /* nmul: -(fn * fm) */
                            gen_vfp_mul(dp);
                            gen_vfp_neg(dp);
                            break;
                        case 6: /* add: fn + fm */
                            gen_vfp_add(dp);
                            break;
                        case 7: /* sub: fn - fm */
                            gen_vfp_sub(dp);
                            break;
                        case 8: /* div: fn / fm */
                            gen_vfp_div(dp);
                            break;
                        case 10: /* VFNMA : fd = muladd(-fd,  fn, fm) */
                        case 11: /* VFNMS : fd = muladd(-fd, -fn, fm) */
                        case 12: /* VFMA  : fd = muladd( fd,  fn, fm) */
                        case 13: /* VFMS  : fd = muladd( fd, -fn, fm) */
                            /* These are fused multiply-add, and must be done as one
                             * floating point operation with no rounding between the
                             * multiplication and addition steps.
                             * NB that doing the negations here as separate steps is
                             * correct : an input NaN should come out with its sign bit
                             * flipped if it is a negated-input.
                             */
                            if(!arm_feature(env, ARM_FEATURE_VFP4)) {
                                return 1;
                            }
                            if(dp) {
                                TCGv_ptr fpst;
                                TCGv_i64 frd;
                                if(op & 1) {
                                    /* VFNMS, VFMS */
                                    gen_helper_vfp_negd(cpu_F0d, cpu_F0d);
                                }
                                frd = tcg_temp_new_i64();
                                tcg_gen_ld_f64(frd, cpu_env, vfp_reg_offset(dp, rd));
                                if(op & 2) {
                                    /* VFNMA, VFNMS */
                                    gen_helper_vfp_negd(frd, frd);
                                }
                                fpst = get_fpstatus_ptr(0);
                                gen_helper_vfp_muladdd(cpu_F0d, cpu_F0d, cpu_F1d, frd, fpst);
                                tcg_temp_free_ptr(fpst);
                                tcg_temp_free_i64(frd);
                            } else {
                                TCGv_ptr fpst;
                                TCGv_i32 frd;
                                if(op & 1) {
                                    /* VFNMS, VFMS */
                                    gen_helper_vfp_negs(cpu_F0s, cpu_F0s);
                                }
                                frd = tcg_temp_new_i32();
                                tcg_gen_ld_f32(frd, cpu_env, vfp_reg_offset(dp, rd));
                                if(op & 2) {
                                    gen_helper_vfp_negs(frd, frd);
                                }
                                fpst = get_fpstatus_ptr(0);
                                gen_helper_vfp_muladds(cpu_F0s, cpu_F0s, cpu_F1s, frd, fpst);
                                tcg_temp_free_ptr(fpst);
                                tcg_temp_free_i32(frd);
                            }
                            break;
                        case 14: /* fconst */
                            if(!arm_feature(env, ARM_FEATURE_VFP3)) {
                                return 1;
                            }

                            n = (insn << 12) & 0x80000000;
                            i = ((insn >> 12) & 0x70) | (insn & 0xf);
                            if(dp) {
                                if(i & 0x40) {
                                    i |= 0x3f80;
                                } else {
                                    i |= 0x4000;
                                }
                                n |= i << 16;
                                tcg_gen_movi_i64(cpu_F0d, ((uint64_t)n) << 32);
                            } else {
                                if(i & 0x40) {
                                    i |= 0x780;
                                } else {
                                    i |= 0x800;
                                }
                                n |= i << 19;
                                tcg_gen_movi_i32(cpu_F0s, n);
                            }
                            break;
                        case 15: /* extension space */
                            switch(rn) {
                                case 0: /* cpy */
                                    /* no-op */
                                    break;
                                case 1: /* abs */
                                    gen_vfp_abs(dp);
                                    break;
                                case 2: /* neg */
                                    gen_vfp_neg(dp);
                                    break;
                                case 3: /* sqrt */
                                    gen_vfp_sqrt(dp);
                                    break;
                                case 4: /* vcvtb.f32.f16 */
                                    tmp = gen_vfp_mrs();
                                    tcg_gen_ext16u_i32(tmp, tmp);
                                    gen_helper_vfp_fcvt_f16_to_f32(cpu_F0s, tmp, cpu_env);
                                    tcg_temp_free_i32(tmp);
                                    break;
                                case 5: /* vcvtt.f32.f16 */
                                    tmp = gen_vfp_mrs();
                                    tcg_gen_shri_i32(tmp, tmp, 16);
                                    gen_helper_vfp_fcvt_f16_to_f32(cpu_F0s, tmp, cpu_env);
                                    tcg_temp_free_i32(tmp);
                                    break;
                                case 6: /* vcvtb.f16.f32 */
                                    tmp = tcg_temp_new_i32();
                                    gen_helper_vfp_fcvt_f32_to_f16(tmp, cpu_F0s, cpu_env);
                                    gen_mov_F0_vreg(0, rd);
                                    tmp2 = gen_vfp_mrs();
                                    tcg_gen_andi_i32(tmp2, tmp2, 0xffff0000);
                                    tcg_gen_or_i32(tmp, tmp, tmp2);
                                    tcg_temp_free_i32(tmp2);
                                    gen_vfp_msr(tmp);
                                    break;
                                case 7: /* vcvtt.f16.f32 */
                                    tmp = tcg_temp_new_i32();
                                    gen_helper_vfp_fcvt_f32_to_f16(tmp, cpu_F0s, cpu_env);
                                    tcg_gen_shli_i32(tmp, tmp, 16);
                                    gen_mov_F0_vreg(0, rd);
                                    tmp2 = gen_vfp_mrs();
                                    tcg_gen_ext16u_i32(tmp2, tmp2);
                                    tcg_gen_or_i32(tmp, tmp, tmp2);
                                    tcg_temp_free_i32(tmp2);
                                    gen_vfp_msr(tmp);
                                    break;
                                case 8: /* cmp */
                                    gen_vfp_cmp(dp);
                                    break;
                                case 9: /* cmpe */
                                    gen_vfp_cmpe(dp);
                                    break;
                                case 10: /* cmpz */
                                    gen_vfp_cmp(dp);
                                    break;
                                case 11: /* cmpez */
                                    gen_vfp_F1_ld0(dp);
                                    gen_vfp_cmpe(dp);
                                    break;
                                case 15: /* single<->double conversion */
                                    if(dp) {
                                        gen_helper_vfp_fcvtsd(cpu_F0s, cpu_F0d, cpu_env);
                                    } else {
                                        gen_helper_vfp_fcvtds(cpu_F0d, cpu_F0s, cpu_env);
                                    }
                                    break;
                                case 16: /* fuito */
                                    gen_vfp_uito(dp, 0);
                                    break;
                                case 17: /* fsito */
                                    gen_vfp_sito(dp, 0);
                                    break;
                                case 20: /* fshto */
                                    if(!arm_feature(env, ARM_FEATURE_VFP3)) {
                                        return 1;
                                    }
                                    gen_vfp_shto(dp, 16 - rm, 0);
                                    break;
                                case 21: /* fslto */
                                    if(!arm_feature(env, ARM_FEATURE_VFP3)) {
                                        return 1;
                                    }
                                    gen_vfp_slto(dp, 32 - rm, 0);
                                    break;
                                case 22: /* fuhto */
                                    if(!arm_feature(env, ARM_FEATURE_VFP3)) {
                                        return 1;
                                    }
                                    gen_vfp_uhto(dp, 16 - rm, 0);
                                    break;
                                case 23: /* fulto */
                                    if(!arm_feature(env, ARM_FEATURE_VFP3)) {
                                        return 1;
                                    }
                                    gen_vfp_ulto(dp, 32 - rm, 0);
                                    break;
                                case 24: /* ftoui */
                                    gen_vfp_toui(dp, 0);
                                    break;
                                case 25: /* ftouiz */
                                    gen_vfp_touiz(dp, 0);
                                    break;
                                case 26: /* ftosi */
                                    gen_vfp_tosi(dp, 0);
                                    break;
                                case 27: /* ftosiz */
                                    gen_vfp_tosiz(dp, 0);
                                    break;
                                case 28: /* ftosh */
                                    if(!arm_feature(env, ARM_FEATURE_VFP3)) {
                                        return 1;
                                    }
                                    gen_vfp_tosh(dp, 16 - rm, 0);
                                    break;
                                case 29: /* ftosl */
                                    if(!arm_feature(env, ARM_FEATURE_VFP3)) {
                                        return 1;
                                    }
                                    gen_vfp_tosl(dp, 32 - rm, 0);
                                    break;
                                case 30: /* ftouh */
                                    if(!arm_feature(env, ARM_FEATURE_VFP3)) {
                                        return 1;
                                    }
                                    gen_vfp_touh(dp, 16 - rm, 0);
                                    break;
                                case 31: /* ftoul */
                                    if(!arm_feature(env, ARM_FEATURE_VFP3)) {
                                        return 1;
                                    }
                                    gen_vfp_toul(dp, 32 - rm, 0);
                                    break;
                                default: /* undefined */
                                    return 1;
                            }
                            break;
                        default: /* undefined */
                            return 1;
                    }

                    /* Write back the result.  */
                    if(op == 15 && (rn >= 8 && rn <= 11)) {
                        ; /* Comparison, do nothing.  */
                    } else if(op == 15 && dp && ((rn & 0x1c) == 0x18)) {
                        /* VCVT double to int: always integer result. */
                        gen_mov_vreg_F0(0, rd);
                    } else if(op == 15 && rn == 15) {
                        /* conversion */
                        gen_mov_vreg_F0(!dp, rd);
                    } else {
                        gen_mov_vreg_F0(dp, rd);
                    }

                    /* break out of the loop if we have finished  */
                    if(veclen == 0) {
                        break;
                    }

                    if(op == 15 && delta_m == 0) {
                        /* single source one-many */
                        while(veclen--) {
                            rd = ((rd + delta_d) & (bank_mask - 1)) | (rd & bank_mask);
                            gen_mov_vreg_F0(dp, rd);
                        }
                        break;
                    }
                    /* Setup the next operands.  */
                    veclen--;
                    rd = ((rd + delta_d) & (bank_mask - 1)) | (rd & bank_mask);

                    if(op == 15) {
                        /* One source operand.  */
                        rm = ((rm + delta_m) & (bank_mask - 1)) | (rm & bank_mask);
                        gen_mov_F0_vreg(dp, rm);
                    } else {
                        /* Two source operands.  */
                        rn = ((rn + delta_d) & (bank_mask - 1)) | (rn & bank_mask);
                        gen_mov_F0_vreg(dp, rn);
                        if(delta_m) {
                            rm = ((rm + delta_m) & (bank_mask - 1)) | (rm & bank_mask);
                            gen_mov_F1_vreg(dp, rm);
                        }
                    }
                }
            }
            break;
        case 0xc:
        case 0xd:
            if((insn & 0x03e00000) == 0x00400000) {
                /* two-register transfer */
                rn = (insn >> 16) & 0xf;
                rd = (insn >> 12) & 0xf;
                if(dp) {
                    VFP_DREG_M(rm, insn);
                } else {
                    rm = VFP_SREG_M(insn);
                }

                if(insn & ARM_CP_RW_BIT) {
                    /* vfp->arm */
                    if(dp) {
                        gen_mov_F0_vreg(0, rm * 2);
                        tmp = gen_vfp_mrs();
                        store_reg(s, rd, tmp);
                        gen_mov_F0_vreg(0, rm * 2 + 1);
                        tmp = gen_vfp_mrs();
                        store_reg(s, rn, tmp);
                    } else {
                        gen_mov_F0_vreg(0, rm);
                        tmp = gen_vfp_mrs();
                        store_reg(s, rd, tmp);
                        gen_mov_F0_vreg(0, rm + 1);
                        tmp = gen_vfp_mrs();
                        store_reg(s, rn, tmp);
                    }
                } else {
                    /* arm->vfp */
                    if(dp) {
                        tmp = load_reg(s, rd);
                        gen_vfp_msr(tmp);
                        gen_mov_vreg_F0(0, rm * 2);
                        tmp = load_reg(s, rn);
                        gen_vfp_msr(tmp);
                        gen_mov_vreg_F0(0, rm * 2 + 1);
                    } else {
                        tmp = load_reg(s, rd);
                        gen_vfp_msr(tmp);
                        gen_mov_vreg_F0(0, rm);
                        tmp = load_reg(s, rn);
                        gen_vfp_msr(tmp);
                        gen_mov_vreg_F0(0, rm + 1);
                    }
                }
            } else {
                /* Load/store */
                rn = (insn >> 16) & 0xf;
                if(dp) {
                    VFP_DREG_D(rd, insn);
                } else {
                    rd = VFP_SREG_D(insn);
                }

                if(rn == 0xf && (insn & 0x1B00000) == 0x900000 && (insn & 0xE00) == 0xA00) {
#ifdef TARGET_PROTO_ARM_M
                    /* VSCCLRM T1/T2 encodings */
                    ARCH(8_1M);
                    if(s->ns) {
                        goto illegal_op;
                    }

                    /* T1 encoding */
                    bool double_precision = ((insn >> 8) & 1) > 0;
                    int reg_count = insn & 0xFF;
                    int first_reg = 0;

                    if(double_precision) {
                        reg_count >>= 1;
                        first_reg = (insn >> 18) | ((insn >> 12) & 0xf);
                    } else {
                        first_reg = ((insn >> 11) & 0x1e) | ((insn >> 22) & 1);
                    }

                    TCGv_i64 zero;
                    if(double_precision) {
                        zero = tcg_const_i64(0);
                    } else {
                        zero = tcg_const_i32(0);
                    }
                    for(int i = 0; i < reg_count; ++i) {
                        int currentReg = i + first_reg;
                        if(double_precision) {
                            tcg_gen_st_i64(zero, cpu_env, vfp_reg_offset(true, currentReg));
                        } else {
                            tcg_gen_st_i32(zero, cpu_env, vfp_reg_offset(false, currentReg));
                        }
                    }
                    if(double_precision) {
                        tcg_temp_free_i64(zero);
                    } else {
                        tcg_temp_free_i32(zero);
                    }

                    /* We should clear VPR here, but our MVE implementation doesn't have it yet */
#else
                    goto illegal_op;
#endif
                } else if((insn & 0x01200000) == 0x01000000) {
                    /* Single load/store */
                    offset = (insn & 0xff) << 2;
                    if((insn & (1 << 23)) == 0) {
                        offset = -offset;
                    }
                    if(s->thumb && rn == 15) {
                        /* This is actually UNPREDICTABLE */
                        addr = tcg_temp_new_i32();
                        tcg_gen_movi_i32(addr, s->base.pc & ~2);
                    } else {
                        addr = load_reg(s, rn);
                    }
                    tcg_gen_addi_i32(addr, addr, offset);
                    if(insn & (1 << 20)) {
                        gen_vfp_ld(s, dp, addr);
                        gen_mov_vreg_F0(dp, rd);
                    } else {
                        gen_mov_F0_vreg(dp, rd);
                        gen_vfp_st(s, dp, addr);
                    }
                    tcg_temp_free_i32(addr);
                } else {
                    /* load/store multiple */
                    int w = insn & (1 << 21);
                    if(dp) {
                        n = (insn >> 1) & 0x7f;
                    } else {
                        n = insn & 0xff;
                    }

                    if(w && !(((insn >> 23) ^ (insn >> 24)) & 1)) {
                        /* P == U , W == 1  => UNDEF */
                        return 1;
                    }
                    if(n == 0 || (rd + n) > 32 || (dp && n > 16)) {
                        /* UNPREDICTABLE cases for bad immediates: we choose to
                         * UNDEF to avoid generating huge numbers of TCG ops
                         */
                        return 1;
                    }
                    if(rn == 15 && w) {
                        /* writeback to PC is UNPREDICTABLE, we choose to UNDEF */
                        return 1;
                    }

                    if(s->thumb && rn == 15) {
                        /* This is actually UNPREDICTABLE */
                        addr = tcg_temp_new_i32();
                        tcg_gen_movi_i32(addr, s->base.pc & ~2);
                    } else {
                        addr = load_reg(s, rn);
                    }
                    if(insn & (1 << 24)) { /* pre-decrement */
                        tcg_gen_addi_i32(addr, addr, -((insn & 0xff) << 2));
                    }

                    if(dp) {
                        offset = 8;
                    } else {
                        offset = 4;
                    }
                    for(i = 0; i < n; i++) {
                        if(insn & ARM_CP_RW_BIT) {
                            /* load */
                            gen_vfp_ld(s, dp, addr);
                            gen_mov_vreg_F0(dp, rd + i);
                        } else {
                            /* store */
                            gen_mov_F0_vreg(dp, rd + i);
                            gen_vfp_st(s, dp, addr);
                        }
                        tcg_gen_addi_i32(addr, addr, offset);
                    }
                    if(w) {
                        /* writeback */
                        if(insn & (1 << 24)) {
                            offset = -offset * n;
                        } else if(dp && (insn & 1)) {
                            offset = 4;
                        } else {
                            offset = 0;
                        }

                        if(offset != 0) {
                            tcg_gen_addi_i32(addr, addr, offset);
                        }
                        store_reg(s, rn, addr);
                    } else {
                        tcg_temp_free_i32(addr);
                    }
                }
            }
            break;
        default:
            /* Should never happen.  */
        illegal_op:
            return 1;
    }
#ifdef TARGET_PROTO_ARM_M
    /* set CONTROL.FPCA if FPCCR.ASPEN is set
     * additionally set CONTROL.SFPA in Secure state */
    tmp = tcg_temp_new_i32();
    if(!s->ns) {
        tcg_gen_shri_i32(tmp, cpu_fpccr_s, ARM_FPCCR_ASPEN - ARM_CONTROL_SFPA);
        tcg_gen_andi_i32(tmp, tmp, ARM_CONTROL_SFPA_MASK);
        tcg_gen_or_i32(cpu_control_ns, cpu_control_ns, tmp);
    }
    if(s->ns) {
        /* Remember that FPCCR.ASPEN is banked */
        tcg_gen_shri_i32(tmp, cpu_fpccr_ns, ARM_FPCCR_ASPEN - ARM_CONTROL_FPCA);
    } else {
        tcg_gen_shri_i32(tmp, cpu_fpccr_s, ARM_FPCCR_ASPEN - ARM_CONTROL_FPCA);
    }

    tcg_gen_andi_i32(tmp, tmp, ARM_CONTROL_FPCA_MASK);
    tcg_gen_or_i32(cpu_control_ns, cpu_control_ns, tmp);

    /* Update the "S" flag*/
    tcg_gen_andi_i32(cpu_fpccr_s, cpu_fpccr_s, ~ARM_FPCCR_S_MASK);
    tcg_gen_ori_i32(cpu_fpccr_s, cpu_fpccr_s, (!s->ns) << ARM_FPCCR_S);

    tcg_temp_free_i32(tmp);
#endif
    return 0;
}

static inline void gen_goto_tb(DisasContext *s, int n, uint32_t dest)
{
    TranslationBlock *tb;

    tb = s->base.tb;
    if((tb->pc & TARGET_PAGE_MASK) == (dest & TARGET_PAGE_MASK)) {
        tcg_gen_goto_tb(n);
        gen_set_pc_im(dest);
        gen_exit_tb(tb, n);
    } else {
        gen_set_pc_im(dest);
        gen_exit_tb_no_chaining(tb);
    }
}

static inline void gen_jmp(DisasContext *s, uint32_t dest, int stack_announcement_type)
{
    if(unlikely(s->base.guest_profile)) {
        generate_stack_announcement_imm_i32(dest, stack_announcement_type, true);
    }
    gen_goto_tb(s, 0, dest);
    s->base.is_jmp = DISAS_TB_JUMP;
}

static inline void gen_mulxy(TCGv t0, TCGv t1, int x, int y)
{
    if(x) {
        tcg_gen_sari_i32(t0, t0, 16);
    } else {
        gen_sxth(t0);
    }
    if(y) {
        tcg_gen_sari_i32(t1, t1, 16);
    } else {
        gen_sxth(t1);
    }
    tcg_gen_mul_i32(t0, t0, t1);
}

/* Return the mask of PSR bits set by a MSR instruction.  */
static uint32_t msr_mask(CPUState *env, DisasContext *s, int flags, int spsr)
{
    uint32_t mask;

    mask = 0;
    if(flags & (1 << 0)) {
        mask |= 0xff;
    }
    if(flags & (1 << 1)) {
        mask |= 0xff00;
    }
    if(flags & (1 << 2)) {
        mask |= 0xff0000;
    }
    if(flags & (1 << 3)) {
        mask |= 0xff000000;
    }

    /* Mask out undefined bits.  */
    mask &= ~CPSR_RESERVED;
    if(!arm_feature(env, ARM_FEATURE_V4T)) {
        mask &= ~CPSR_T;
    }
    if(!arm_feature(env, ARM_FEATURE_V5)) {
        mask &= ~CPSR_Q; /* V5TE in reality*/
    }
    if(!arm_feature(env, ARM_FEATURE_V6)) {
        mask &= ~(CPSR_E | CPSR_GE);
    }
    if(!arm_feature(env, ARM_FEATURE_THUMB2)) {
        mask &= ~CPSR_IT;
    }
    /* Mask out execution state bits.  */
    if(!spsr) {
        mask &= ~CPSR_EXEC;
    }
    /* Mask out privileged bits.  */
    if(s->user) {
        mask &= CPSR_USER;
    }
    return mask;
}

/* Returns nonzero if access to the PSR is not permitted. Marks t0 as dead. */
static int gen_set_psr(DisasContext *s, uint32_t mask, int spsr, TCGv t0)
{
    TCGv tmp;
    if(spsr) {
        /* ??? This is also undefined in system mode.  */
        if(s->user) {
            return 1;
        }

        tmp = load_cpu_field(spsr);
        tcg_gen_andi_i32(tmp, tmp, ~mask);
        tcg_gen_andi_i32(t0, t0, mask);
        tcg_gen_or_i32(tmp, tmp, t0);
        store_cpu_field(tmp, spsr);
    } else {
        gen_set_cpsr(t0, mask);
    }
    tcg_temp_free_i32(t0);
    gen_lookup_tb(s);
    return 0;
}

/* Returns nonzero if access to the PSR is not permitted.  */
static int gen_set_psr_im(DisasContext *s, uint32_t mask, int spsr, uint32_t val)
{
    TCGv tmp;
    tmp = tcg_temp_new_i32();
    tcg_gen_movi_i32(tmp, val);
    return gen_set_psr(s, mask, spsr, tmp);
}

/* Generate an old-style exception return. Marks pc as dead. */
static void gen_exception_return(DisasContext *s, TCGv pc)
{
    TCGv tmp;
    //  Exception index is always -1 in exception returns for consistency with RISC-V
    if(env->interrupt_end_callback_enabled) {
        TCGv_i64 exception_index = tcg_const_i64(-1);
        gen_helper_on_interrupt_end_event(exception_index);
        tcg_temp_free_i64(exception_index);
    }
    store_reg(s, 15, pc);
    tmp = load_cpu_field(spsr);
    gen_set_cpsr(tmp, 0xffffffff);
    tcg_temp_free_i32(tmp);
    s->base.is_jmp = DISAS_UPDATE;
}

/* Generate a v6 exception return.  Marks both values as dead.  */
static void gen_rfe(DisasContext *s, TCGv pc, TCGv cpsr)
{
    gen_set_cpsr(cpsr, 0xffffffff);
    tcg_temp_free_i32(cpsr);
    store_reg(s, 15, pc);
    s->base.is_jmp = DISAS_UPDATE;
}

static inline uint32_t pack_condexec(DisasContext *s)
{
    return (s->condexec_cond << 4) | (s->condexec_mask >> 1);
}

static inline void gen_set_condexec(DisasContext *s)
{
    if(s->condexec_mask) {
        uint32_t val = pack_condexec(s);
        TCGv tmp = tcg_temp_new_i32();
        tcg_gen_movi_i32(tmp, val);
        store_cpu_field(tmp, condexec_bits);
    }
}

static void gen_exception_insn(DisasContext *s, int offset, int excp)
{
    gen_set_condexec(s);
    gen_set_pc_im(s->base.pc - offset);
    gen_exception(excp);
    s->base.is_jmp = DISAS_JUMP;
}

static void gen_nop_hint(DisasContext *s, int val)
{
    switch(val) {
        case 3: /* wfi */
            if(tlib_is_wfi_as_nop()) {
                break;
            }
            gen_set_pc_im(s->base.pc);
            s->base.is_jmp = DISAS_WFI;
            break;
        case 2: /* wfe */
            if(tlib_is_wfe_and_sev_as_nop()) {
                break;
            }
            gen_set_pc_im(s->base.pc);
            s->base.is_jmp = DISAS_WFE;
            break;

        case 4: /* sev */
            if(tlib_is_wfe_and_sev_as_nop()) {
                break;
            }
            gen_helper_set_system_event();
            break;
        default: /* nop */
            break;
    }
}

#define CPU_V001 cpu_V0, cpu_V0, cpu_V1

static inline void gen_neon_add(int size, TCGv t0, TCGv t1)
{
    switch(size) {
        case 0:
            gen_helper_neon_add_u8(t0, t0, t1);
            break;
        case 1:
            gen_helper_neon_add_u16(t0, t0, t1);
            break;
        case 2:
            tcg_gen_add_i32(t0, t0, t1);
            break;
        default:
            abort();
    }
}

static inline void gen_neon_rsb(int size, TCGv t0, TCGv t1)
{
    switch(size) {
        case 0:
            gen_helper_neon_sub_u8(t0, t1, t0);
            break;
        case 1:
            gen_helper_neon_sub_u16(t0, t1, t0);
            break;
        case 2:
            tcg_gen_sub_i32(t0, t1, t0);
            break;
        default:
            return;
    }
}

/* 32-bit pairwise ops end up the same as the elementwise versions.  */
#define gen_helper_neon_pmax_s32 gen_helper_neon_max_s32
#define gen_helper_neon_pmax_u32 gen_helper_neon_max_u32
#define gen_helper_neon_pmin_s32 gen_helper_neon_min_s32
#define gen_helper_neon_pmin_u32 gen_helper_neon_min_u32

#define GEN_NEON_INTEGER_OP_ENV(name)                                  \
    do {                                                               \
        switch((size << 1) | u) {                                      \
            case 0:                                                    \
                gen_helper_neon_##name##_s8(tmp, cpu_env, tmp, tmp2);  \
                break;                                                 \
            case 1:                                                    \
                gen_helper_neon_##name##_u8(tmp, cpu_env, tmp, tmp2);  \
                break;                                                 \
            case 2:                                                    \
                gen_helper_neon_##name##_s16(tmp, cpu_env, tmp, tmp2); \
                break;                                                 \
            case 3:                                                    \
                gen_helper_neon_##name##_u16(tmp, cpu_env, tmp, tmp2); \
                break;                                                 \
            case 4:                                                    \
                gen_helper_neon_##name##_s32(tmp, cpu_env, tmp, tmp2); \
                break;                                                 \
            case 5:                                                    \
                gen_helper_neon_##name##_u32(tmp, cpu_env, tmp, tmp2); \
                break;                                                 \
            default:                                                   \
                return 1;                                              \
        }                                                              \
    } while(0)

#define GEN_NEON_INTEGER_OP(name)                             \
    do {                                                      \
        switch((size << 1) | u) {                             \
            case 0:                                           \
                gen_helper_neon_##name##_s8(tmp, tmp, tmp2);  \
                break;                                        \
            case 1:                                           \
                gen_helper_neon_##name##_u8(tmp, tmp, tmp2);  \
                break;                                        \
            case 2:                                           \
                gen_helper_neon_##name##_s16(tmp, tmp, tmp2); \
                break;                                        \
            case 3:                                           \
                gen_helper_neon_##name##_u16(tmp, tmp, tmp2); \
                break;                                        \
            case 4:                                           \
                gen_helper_neon_##name##_s32(tmp, tmp, tmp2); \
                break;                                        \
            case 5:                                           \
                gen_helper_neon_##name##_u32(tmp, tmp, tmp2); \
                break;                                        \
            default:                                          \
                return 1;                                     \
        }                                                     \
    } while(0)

static TCGv neon_load_scratch(int scratch)
{
    TCGv tmp = tcg_temp_new_i32();
    tcg_gen_ld_i32(tmp, cpu_env, offsetof(CPUState, vfp.scratch[scratch]));
    return tmp;
}

static void neon_store_scratch(int scratch, TCGv var)
{
    tcg_gen_st_i32(var, cpu_env, offsetof(CPUState, vfp.scratch[scratch]));
    tcg_temp_free_i32(var);
}

static inline TCGv neon_get_scalar(int size, int reg)
{
    TCGv tmp;
    if(size == 1) {
        tmp = neon_load_reg(reg & 7, reg >> 4);
        if(reg & 8) {
            gen_neon_dup_high16(tmp);
        } else {
            gen_neon_dup_low16(tmp);
        }
    } else {
        tmp = neon_load_reg(reg & 15, reg >> 4);
    }
    return tmp;
}

static int gen_neon_unzip(int rd, int rm, int size, int q)
{
    TCGv tmp, tmp2;
    if(!q && size == 2) {
        return 1;
    }
    tmp = tcg_const_i32(rd);
    tmp2 = tcg_const_i32(rm);
    if(q) {
        switch(size) {
            case 0:
                gen_helper_neon_qunzip8(cpu_env, tmp, tmp2);
                break;
            case 1:
                gen_helper_neon_qunzip16(cpu_env, tmp, tmp2);
                break;
            case 2:
                gen_helper_neon_qunzip32(cpu_env, tmp, tmp2);
                break;
            default:
                abort();
        }
    } else {
        switch(size) {
            case 0:
                gen_helper_neon_unzip8(cpu_env, tmp, tmp2);
                break;
            case 1:
                gen_helper_neon_unzip16(cpu_env, tmp, tmp2);
                break;
            default:
                abort();
        }
    }
    tcg_temp_free_i32(tmp);
    tcg_temp_free_i32(tmp2);
    return 0;
}

static int gen_neon_zip(int rd, int rm, int size, int q)
{
    TCGv tmp, tmp2;
    if(!q && size == 2) {
        return 1;
    }
    tmp = tcg_const_i32(rd);
    tmp2 = tcg_const_i32(rm);
    if(q) {
        switch(size) {
            case 0:
                gen_helper_neon_qzip8(cpu_env, tmp, tmp2);
                break;
            case 1:
                gen_helper_neon_qzip16(cpu_env, tmp, tmp2);
                break;
            case 2:
                gen_helper_neon_qzip32(cpu_env, tmp, tmp2);
                break;
            default:
                abort();
        }
    } else {
        switch(size) {
            case 0:
                gen_helper_neon_zip8(cpu_env, tmp, tmp2);
                break;
            case 1:
                gen_helper_neon_zip16(cpu_env, tmp, tmp2);
                break;
            default:
                abort();
        }
    }
    tcg_temp_free_i32(tmp);
    tcg_temp_free_i32(tmp2);
    return 0;
}

static void gen_neon_trn_u8(TCGv t0, TCGv t1)
{
    TCGv rd, tmp;

    rd = tcg_temp_new_i32();
    tmp = tcg_temp_new_i32();

    tcg_gen_shli_i32(rd, t0, 8);
    tcg_gen_andi_i32(rd, rd, 0xff00ff00);
    tcg_gen_andi_i32(tmp, t1, 0x00ff00ff);
    tcg_gen_or_i32(rd, rd, tmp);

    tcg_gen_shri_i32(t1, t1, 8);
    tcg_gen_andi_i32(t1, t1, 0x00ff00ff);
    tcg_gen_andi_i32(tmp, t0, 0xff00ff00);
    tcg_gen_or_i32(t1, t1, tmp);
    tcg_gen_mov_i32(t0, rd);

    tcg_temp_free_i32(tmp);
    tcg_temp_free_i32(rd);
}

static void gen_neon_trn_u16(TCGv t0, TCGv t1)
{
    TCGv rd, tmp;

    rd = tcg_temp_new_i32();
    tmp = tcg_temp_new_i32();

    tcg_gen_shli_i32(rd, t0, 16);
    tcg_gen_andi_i32(tmp, t1, 0xffff);
    tcg_gen_or_i32(rd, rd, tmp);
    tcg_gen_shri_i32(t1, t1, 16);
    tcg_gen_andi_i32(tmp, t0, 0xffff0000);
    tcg_gen_or_i32(t1, t1, tmp);
    tcg_gen_mov_i32(t0, rd);

    tcg_temp_free_i32(tmp);
    tcg_temp_free_i32(rd);
}

static struct {
    int nregs;
    int interleave;
    int spacing;
} neon_ls_element_type[11] = {
    { 4, 4, 1 },
    { 4, 4, 2 },
    { 4, 1, 1 },
    { 4, 2, 1 },
    { 3, 3, 1 },
    { 3, 3, 2 },
    { 3, 1, 1 },
    { 1, 1, 1 },
    { 2, 2, 1 },
    { 2, 2, 2 },
    { 2, 1, 1 }
};

/* Translate a NEON load/store element instruction.  Return nonzero if the
   instruction is invalid.  */
static int disas_neon_ls_insn(CPUState *env, DisasContext *s, uint32_t insn)
{
    int rd, rn, rm;
    int op;
    int nregs;
    int interleave;
    int spacing;
    int stride;
    int size;
    int reg;
    int pass;
    int load;
    int shift;
    int n;
    TCGv addr;
    TCGv tmp;
    TCGv tmp2;
    TCGv_i64 tmp64;

    if(!s->vfp_enabled) {
        return 1;
    }
    VFP_DREG_D(rd, insn);
    rn = (insn >> 16) & 0xf;
    rm = insn & 0xf;
    load = (insn & (1 << 21)) != 0;
    if((insn & (1 << 23)) == 0) {
        /* Load store all elements.  */
        op = (insn >> 8) & 0xf;
        size = (insn >> 6) & 3;
        if(op > 10) {
            return 1;
        }
        /* Catch UNDEF cases for bad values of align field */
        switch(op & 0xc) {
            case 4:
                if(((insn >> 5) & 1) == 1) {
                    return 1;
                }
                break;
            case 8:
                if(((insn >> 4) & 3) == 3) {
                    return 1;
                }
                break;
            default:
                break;
        }
        nregs = neon_ls_element_type[op].nregs;
        interleave = neon_ls_element_type[op].interleave;
        spacing = neon_ls_element_type[op].spacing;
        if(size == 3 && (interleave | spacing) != 1) {
            return 1;
        }
        addr = tcg_temp_new_i32();
        load_reg_var(s, addr, rn);
        stride = (1 << size) * interleave;
        for(reg = 0; reg < nregs; reg++) {
            if(interleave > 2 || (interleave == 2 && nregs == 2)) {
                load_reg_var(s, addr, rn);
                tcg_gen_addi_i32(addr, addr, (1 << size) * reg);
            } else if(interleave == 2 && nregs == 4 && reg == 2) {
                load_reg_var(s, addr, rn);
                tcg_gen_addi_i32(addr, addr, 1 << size);
            }
            if(size == 3) {
                if(load) {
                    tmp64 = gen_ld64(addr, context_to_mmu_index(s));
                    neon_store_reg64(tmp64, rd);
                    tcg_temp_free_i64(tmp64);
                } else {
                    tmp64 = tcg_temp_new_i64();
                    neon_load_reg64(tmp64, rd);
                    gen_st64(tmp64, addr, context_to_mmu_index(s));
                }
                tcg_gen_addi_i32(addr, addr, stride);
            } else {
                for(pass = 0; pass < 2; pass++) {
                    if(size == 2) {
                        if(load) {
                            tmp = gen_ld32(addr, context_to_mmu_index(s));
                            neon_store_reg(rd, pass, tmp);
                        } else {
                            tmp = neon_load_reg(rd, pass);
                            gen_st32(tmp, addr, context_to_mmu_index(s));
                        }
                        tcg_gen_addi_i32(addr, addr, stride);
                    } else if(size == 1) {
                        if(load) {
                            tmp = gen_ld16u(addr, context_to_mmu_index(s));
                            tcg_gen_addi_i32(addr, addr, stride);
                            tmp2 = gen_ld16u(addr, context_to_mmu_index(s));
                            tcg_gen_addi_i32(addr, addr, stride);
                            tcg_gen_shli_i32(tmp2, tmp2, 16);
                            tcg_gen_or_i32(tmp, tmp, tmp2);
                            tcg_temp_free_i32(tmp2);
                            neon_store_reg(rd, pass, tmp);
                        } else {
                            tmp = neon_load_reg(rd, pass);
                            tmp2 = tcg_temp_new_i32();
                            tcg_gen_shri_i32(tmp2, tmp, 16);
                            gen_st16(tmp, addr, context_to_mmu_index(s));
                            tcg_gen_addi_i32(addr, addr, stride);
                            gen_st16(tmp2, addr, context_to_mmu_index(s));
                            tcg_gen_addi_i32(addr, addr, stride);
                        }
                    } else { /* size == 0 */
                        if(load) {
                            TCGV_UNUSED(tmp2);
                            for(n = 0; n < 4; n++) {
                                tmp = gen_ld8u(addr, context_to_mmu_index(s));
                                tcg_gen_addi_i32(addr, addr, stride);
                                if(n == 0) {
                                    tmp2 = tmp;
                                } else {
                                    tcg_gen_shli_i32(tmp, tmp, n * 8);
                                    tcg_gen_or_i32(tmp2, tmp2, tmp);
                                    tcg_temp_free_i32(tmp);
                                }
                            }
                            neon_store_reg(rd, pass, tmp2);
                        } else {
                            tmp2 = neon_load_reg(rd, pass);
                            for(n = 0; n < 4; n++) {
                                tmp = tcg_temp_new_i32();
                                if(n == 0) {
                                    tcg_gen_mov_i32(tmp, tmp2);
                                } else {
                                    tcg_gen_shri_i32(tmp, tmp2, n * 8);
                                }
                                gen_st8(tmp, addr, context_to_mmu_index(s));
                                tcg_gen_addi_i32(addr, addr, stride);
                            }
                            tcg_temp_free_i32(tmp2);
                        }
                    }
                }
            }
            rd += spacing;
        }
        tcg_temp_free_i32(addr);
        stride = nregs * 8;
    } else {
        size = (insn >> 10) & 3;
        if(size == 3) {
            /* Load single element to all lanes.  */
            int a = (insn >> 4) & 1;
            if(!load) {
                return 1;
            }
            size = (insn >> 6) & 3;
            nregs = ((insn >> 8) & 3) + 1;

            if(size == 3) {
                if(nregs != 4 || a == 0) {
                    return 1;
                }
                /* For VLD4 size==3 a == 1 means 32 bits at 16 byte alignment */
                size = 2;
            }
            if(nregs == 1 && a == 1 && size == 0) {
                return 1;
            }
            if(nregs == 3 && a == 1) {
                return 1;
            }
            addr = tcg_temp_new_i32();
            load_reg_var(s, addr, rn);
            if(nregs == 1) {
                /* VLD1 to all lanes: bit 5 indicates how many Dregs to write */
                tmp = gen_load_and_replicate(s, addr, size);
                tcg_gen_st_i32(tmp, cpu_env, neon_reg_offset(rd, 0));
                tcg_gen_st_i32(tmp, cpu_env, neon_reg_offset(rd, 1));
                if(insn & (1 << 5)) {
                    tcg_gen_st_i32(tmp, cpu_env, neon_reg_offset(rd + 1, 0));
                    tcg_gen_st_i32(tmp, cpu_env, neon_reg_offset(rd + 1, 1));
                }
                tcg_temp_free_i32(tmp);
            } else {
                /* VLD2/3/4 to all lanes: bit 5 indicates register stride */
                stride = (insn & (1 << 5)) ? 2 : 1;
                for(reg = 0; reg < nregs; reg++) {
                    tmp = gen_load_and_replicate(s, addr, size);
                    tcg_gen_st_i32(tmp, cpu_env, neon_reg_offset(rd, 0));
                    tcg_gen_st_i32(tmp, cpu_env, neon_reg_offset(rd, 1));
                    tcg_temp_free_i32(tmp);
                    tcg_gen_addi_i32(addr, addr, 1 << size);
                    rd += stride;
                }
            }
            tcg_temp_free_i32(addr);
            stride = (1 << size) * nregs;
        } else {
            /* Single element.  */
            int idx = (insn >> 4) & 0xf;
            pass = (insn >> 7) & 1;
            switch(size) {
                case 0:
                    shift = ((insn >> 5) & 3) * 8;
                    stride = 1;
                    break;
                case 1:
                    shift = ((insn >> 6) & 1) * 16;
                    stride = (insn & (1 << 5)) ? 2 : 1;
                    break;
                case 2:
                    shift = 0;
                    stride = (insn & (1 << 6)) ? 2 : 1;
                    break;
                default:
                    abort();
            }
            nregs = ((insn >> 8) & 3) + 1;
            /* Catch the UNDEF cases. This is unavoidably a bit messy. */
            switch(nregs) {
                case 1:
                    if(((idx & (1 << size)) != 0) || (size == 2 && ((idx & 3) == 1 || (idx & 3) == 2))) {
                        return 1;
                    }
                    break;
                case 3:
                    if((idx & 1) != 0) {
                        return 1;
                    }
                /* fall through */
                case 2:
                    if(size == 2 && (idx & 2) != 0) {
                        return 1;
                    }
                    break;
                case 4:
                    if((size == 2) && ((idx & 3) == 3)) {
                        return 1;
                    }
                    break;
                default:
                    abort();
            }
            if((rd + stride * (nregs - 1)) > 31) {
                /* Attempts to write off the end of the register file
                 * are UNPREDICTABLE; we choose to UNDEF because otherwise
                 * the neon_load_reg() would write off the end of the array.
                 */
                return 1;
            }
            addr = tcg_temp_new_i32();
            load_reg_var(s, addr, rn);
            for(reg = 0; reg < nregs; reg++) {
                if(load) {
                    switch(size) {
                        case 0:
                            tmp = gen_ld8u(addr, context_to_mmu_index(s));
                            break;
                        case 1:
                            tmp = gen_ld16u(addr, context_to_mmu_index(s));
                            break;
                        case 2:
                            tmp = gen_ld32(addr, context_to_mmu_index(s));
                            break;
                        default: /* Avoid compiler warnings.  */
                            abort();
                    }
                    if(size != 2) {
                        tmp2 = neon_load_reg(rd, pass);
                        gen_bfi(tmp, tmp2, tmp, shift, size ? 0xffff : 0xff);
                        tcg_temp_free_i32(tmp2);
                    }
                    neon_store_reg(rd, pass, tmp);
                } else { /* Store */
                    tmp = neon_load_reg(rd, pass);
                    if(shift) {
                        tcg_gen_shri_i32(tmp, tmp, shift);
                    }
                    switch(size) {
                        case 0:
                            gen_st8(tmp, addr, context_to_mmu_index(s));
                            break;
                        case 1:
                            gen_st16(tmp, addr, context_to_mmu_index(s));
                            break;
                        case 2:
                            gen_st32(tmp, addr, context_to_mmu_index(s));
                            break;
                    }
                }
                rd += stride;
                tcg_gen_addi_i32(addr, addr, 1 << size);
            }
            tcg_temp_free_i32(addr);
            stride = nregs * (1 << size);
        }
    }
    if(rm != 15) {
        TCGv base;

        base = load_reg(s, rn);
        if(rm == 13) {
            tcg_gen_addi_i32(base, base, stride);
        } else {
            TCGv index;
            index = load_reg(s, rm);
            tcg_gen_add_i32(base, base, index);
            tcg_temp_free_i32(index);
        }
        store_reg(s, rn, base);
    }
    return 0;
}

/* Bitwise select.  dest = c ? t : f.  Clobbers T and F.  */
static void gen_neon_bsl(TCGv dest, TCGv t, TCGv f, TCGv c)
{
    tcg_gen_and_i32(t, t, c);
    tcg_gen_andc_i32(f, f, c);
    tcg_gen_or_i32(dest, t, f);
}

static inline void gen_neon_narrow(int size, TCGv dest, TCGv_i64 src)
{
    switch(size) {
        case 0:
            gen_helper_neon_narrow_u8(dest, src);
            break;
        case 1:
            gen_helper_neon_narrow_u16(dest, src);
            break;
        case 2:
            tcg_gen_trunc_i64_i32(dest, src);
            break;
        default:
            abort();
    }
}

static inline void gen_neon_narrow_sats(int size, TCGv dest, TCGv_i64 src)
{
    switch(size) {
        case 0:
            gen_helper_neon_narrow_sat_s8(dest, cpu_env, src);
            break;
        case 1:
            gen_helper_neon_narrow_sat_s16(dest, cpu_env, src);
            break;
        case 2:
            gen_helper_neon_narrow_sat_s32(dest, cpu_env, src);
            break;
        default:
            abort();
    }
}

static inline void gen_neon_narrow_satu(int size, TCGv dest, TCGv_i64 src)
{
    switch(size) {
        case 0:
            gen_helper_neon_narrow_sat_u8(dest, cpu_env, src);
            break;
        case 1:
            gen_helper_neon_narrow_sat_u16(dest, cpu_env, src);
            break;
        case 2:
            gen_helper_neon_narrow_sat_u32(dest, cpu_env, src);
            break;
        default:
            abort();
    }
}

static inline void gen_neon_unarrow_sats(int size, TCGv dest, TCGv_i64 src)
{
    switch(size) {
        case 0:
            gen_helper_neon_unarrow_sat8(dest, cpu_env, src);
            break;
        case 1:
            gen_helper_neon_unarrow_sat16(dest, cpu_env, src);
            break;
        case 2:
            gen_helper_neon_unarrow_sat32(dest, cpu_env, src);
            break;
        default:
            abort();
    }
}

static inline void gen_neon_shift_narrow(int size, TCGv var, TCGv shift, int q, int u)
{
    if(q) {
        if(u) {
            switch(size) {
                case 1:
                    gen_helper_neon_rshl_u16(var, var, shift);
                    break;
                case 2:
                    gen_helper_neon_rshl_u32(var, var, shift);
                    break;
                default:
                    abort();
            }
        } else {
            switch(size) {
                case 1:
                    gen_helper_neon_rshl_s16(var, var, shift);
                    break;
                case 2:
                    gen_helper_neon_rshl_s32(var, var, shift);
                    break;
                default:
                    abort();
            }
        }
    } else {
        if(u) {
            switch(size) {
                case 1:
                    gen_helper_neon_shl_u16(var, var, shift);
                    break;
                case 2:
                    gen_helper_neon_shl_u32(var, var, shift);
                    break;
                default:
                    abort();
            }
        } else {
            switch(size) {
                case 1:
                    gen_helper_neon_shl_s16(var, var, shift);
                    break;
                case 2:
                    gen_helper_neon_shl_s32(var, var, shift);
                    break;
                default:
                    abort();
            }
        }
    }
}

static inline void gen_neon_widen(TCGv_i64 dest, TCGv src, int size, int u)
{
    if(u) {
        switch(size) {
            case 0:
                gen_helper_neon_widen_u8(dest, src);
                break;
            case 1:
                gen_helper_neon_widen_u16(dest, src);
                break;
            case 2:
                tcg_gen_extu_i32_i64(dest, src);
                break;
            default:
                abort();
        }
    } else {
        switch(size) {
            case 0:
                gen_helper_neon_widen_s8(dest, src);
                break;
            case 1:
                gen_helper_neon_widen_s16(dest, src);
                break;
            case 2:
                tcg_gen_ext_i32_i64(dest, src);
                break;
            default:
                abort();
        }
    }
    tcg_temp_free_i32(src);
}

static inline void gen_neon_addl(int size)
{
    switch(size) {
        case 0:
            gen_helper_neon_addl_u16(CPU_V001);
            break;
        case 1:
            gen_helper_neon_addl_u32(CPU_V001);
            break;
        case 2:
            tcg_gen_add_i64(CPU_V001);
            break;
        default:
            abort();
    }
}

static inline void gen_neon_subl(int size)
{
    switch(size) {
        case 0:
            gen_helper_neon_subl_u16(CPU_V001);
            break;
        case 1:
            gen_helper_neon_subl_u32(CPU_V001);
            break;
        case 2:
            tcg_gen_sub_i64(CPU_V001);
            break;
        default:
            abort();
    }
}

static inline void gen_neon_negl(TCGv_i64 var, int size)
{
    switch(size) {
        case 0:
            gen_helper_neon_negl_u16(var, var);
            break;
        case 1:
            gen_helper_neon_negl_u32(var, var);
            break;
        case 2:
            tcg_gen_neg_i64(var, var);
            break;
        default:
            abort();
    }
}

static inline void gen_neon_addl_saturate(TCGv_i64 op0, TCGv_i64 op1, int size)
{
    switch(size) {
        case 1:
            gen_helper_neon_addl_saturate_s32(op0, cpu_env, op0, op1);
            break;
        case 2:
            gen_helper_neon_addl_saturate_s64(op0, cpu_env, op0, op1);
            break;
        default:
            abort();
    }
}

static inline void gen_neon_mull(TCGv_i64 dest, TCGv a, TCGv b, int size, int u)
{
    TCGv_i64 tmp;

    switch((size << 1) | u) {
        case 0:
            gen_helper_neon_mull_s8(dest, a, b);
            break;
        case 1:
            gen_helper_neon_mull_u8(dest, a, b);
            break;
        case 2:
            gen_helper_neon_mull_s16(dest, a, b);
            break;
        case 3:
            gen_helper_neon_mull_u16(dest, a, b);
            break;
        case 4:
            tmp = gen_muls_i64_i32(a, b);
            tcg_gen_mov_i64(dest, tmp);
            tcg_temp_free_i64(tmp);
            break;
        case 5:
            tmp = gen_mulu_i64_i32(a, b);
            tcg_gen_mov_i64(dest, tmp);
            tcg_temp_free_i64(tmp);
            break;
        default:
            abort();
    }

    /* gen_helper_neon_mull_[su]{8|16} do not free their parameters.
       Don't forget to clean them now.  */
    if(size < 2) {
        tcg_temp_free_i32(a);
        tcg_temp_free_i32(b);
    }
}

static void gen_neon_narrow_op(int op, int u, int size, TCGv dest, TCGv_i64 src)
{
    if(op) {
        if(u) {
            gen_neon_unarrow_sats(size, dest, src);
        } else {
            gen_neon_narrow(size, dest, src);
        }
    } else {
        if(u) {
            gen_neon_narrow_satu(size, dest, src);
        } else {
            gen_neon_narrow_sats(size, dest, src);
        }
    }
}

/* Symbolic constants for op fields for Neon 3-register same-length.
 * The values correspond to bits [11:8,4]; see the ARM ARM DDI0406B
 * table A7-9.
 */
#define NEON_3R_VHADD            0
#define NEON_3R_VQADD            1
#define NEON_3R_VRHADD           2
#define NEON_3R_LOGIC            3 /* VAND,VBIC,VORR,VMOV,VORN,VEOR,VBIF,VBIT,VBSL */
#define NEON_3R_VHSUB            4
#define NEON_3R_VQSUB            5
#define NEON_3R_VCGT             6
#define NEON_3R_VCGE             7
#define NEON_3R_VSHL             8
#define NEON_3R_VQSHL            9
#define NEON_3R_VRSHL            10
#define NEON_3R_VQRSHL           11
#define NEON_3R_VMAX             12
#define NEON_3R_VMIN             13
#define NEON_3R_VABD             14
#define NEON_3R_VABA             15
#define NEON_3R_VADD_VSUB        16
#define NEON_3R_VTST_VCEQ        17
#define NEON_3R_VML              18 /* VMLA, VMLAL, VMLS, VMLSL */
#define NEON_3R_VMUL             19
#define NEON_3R_VPMAX            20
#define NEON_3R_VPMIN            21
#define NEON_3R_VQDMULH_VQRDMULH 22
#define NEON_3R_VPADD            23
#define NEON_3R_VFM              25 /* VFMA, VFMS : float fused multiply-add */
#define NEON_3R_FLOAT_ARITH      26 /* float VADD, VSUB, VPADD, VABD */
#define NEON_3R_FLOAT_MULTIPLY   27 /* float VMLA, VMLS, VMUL */
#define NEON_3R_FLOAT_CMP        28 /* float VCEQ, VCGE, VCGT */
#define NEON_3R_FLOAT_ACMP       29 /* float VACGE, VACGT, VACLE, VACLT */
#define NEON_3R_FLOAT_MINMAX     30 /* float VMIN, VMAX */
#define NEON_3R_VRECPS_VRSQRTS   31 /* float VRECPS, VRSQRTS */

static const uint8_t neon_3r_sizes[] = {
    [NEON_3R_VHADD] = 0x7,
    [NEON_3R_VQADD] = 0xf,
    [NEON_3R_VRHADD] = 0x7,
    [NEON_3R_LOGIC] = 0xf, /* size field encodes op type */
    [NEON_3R_VHSUB] = 0x7,
    [NEON_3R_VQSUB] = 0xf,
    [NEON_3R_VCGT] = 0x7,
    [NEON_3R_VCGE] = 0x7,
    [NEON_3R_VSHL] = 0xf,
    [NEON_3R_VQSHL] = 0xf,
    [NEON_3R_VRSHL] = 0xf,
    [NEON_3R_VQRSHL] = 0xf,
    [NEON_3R_VMAX] = 0x7,
    [NEON_3R_VMIN] = 0x7,
    [NEON_3R_VABD] = 0x7,
    [NEON_3R_VABA] = 0x7,
    [NEON_3R_VADD_VSUB] = 0xf,
    [NEON_3R_VTST_VCEQ] = 0x7,
    [NEON_3R_VML] = 0x7,
    [NEON_3R_VMUL] = 0x7,
    [NEON_3R_VPMAX] = 0x7,
    [NEON_3R_VPMIN] = 0x7,
    [NEON_3R_VQDMULH_VQRDMULH] = 0x6,
    [NEON_3R_VPADD] = 0x7,
    [NEON_3R_VFM] = 0x5,            /* size bit 1 encodes op */
    [NEON_3R_FLOAT_ARITH] = 0x5,    /* size bit 1 encodes op */
    [NEON_3R_FLOAT_MULTIPLY] = 0x5, /* size bit 1 encodes op */
    [NEON_3R_FLOAT_CMP] = 0x5,      /* size bit 1 encodes op */
    [NEON_3R_FLOAT_ACMP] = 0x5,     /* size bit 1 encodes op */
    [NEON_3R_FLOAT_MINMAX] = 0x5,   /* size bit 1 encodes op */
    [NEON_3R_VRECPS_VRSQRTS] = 0x5, /* size bit 1 encodes op */
};

/* Symbolic constants for op fields for Neon 2-register miscellaneous.
 * The values correspond to bits [17:16,10:7]; see the ARM ARM DDI0406B
 * table A7-13.
 */
#define NEON_2RM_VREV64       0
#define NEON_2RM_VREV32       1
#define NEON_2RM_VREV16       2
#define NEON_2RM_VPADDL       4
#define NEON_2RM_VPADDL_U     5
#define NEON_2RM_VCLS         8
#define NEON_2RM_VCLZ         9
#define NEON_2RM_VCNT         10
#define NEON_2RM_VMVN         11
#define NEON_2RM_VPADAL       12
#define NEON_2RM_VPADAL_U     13
#define NEON_2RM_VQABS        14
#define NEON_2RM_VQNEG        15
#define NEON_2RM_VCGT0        16
#define NEON_2RM_VCGE0        17
#define NEON_2RM_VCEQ0        18
#define NEON_2RM_VCLE0        19
#define NEON_2RM_VCLT0        20
#define NEON_2RM_VABS         22
#define NEON_2RM_VNEG         23
#define NEON_2RM_VCGT0_F      24
#define NEON_2RM_VCGE0_F      25
#define NEON_2RM_VCEQ0_F      26
#define NEON_2RM_VCLE0_F      27
#define NEON_2RM_VCLT0_F      28
#define NEON_2RM_VABS_F       30
#define NEON_2RM_VNEG_F       31
#define NEON_2RM_VSWP         32
#define NEON_2RM_VTRN         33
#define NEON_2RM_VUZP         34
#define NEON_2RM_VZIP         35
#define NEON_2RM_VMOVN        36 /* Includes VQMOVN, VQMOVUN */
#define NEON_2RM_VQMOVN       37 /* Includes VQMOVUN */
#define NEON_2RM_VSHLL        38
#define NEON_2RM_VCVT_F16_F32 44
#define NEON_2RM_VCVT_F32_F16 46
#define NEON_2RM_VRECPE       56
#define NEON_2RM_VRSQRTE      57
#define NEON_2RM_VRECPE_F     58
#define NEON_2RM_VRSQRTE_F    59
#define NEON_2RM_VCVT_FS      60
#define NEON_2RM_VCVT_FU      61
#define NEON_2RM_VCVT_SF      62
#define NEON_2RM_VCVT_UF      63

static int neon_2rm_is_float_op(int op)
{
    /* Return true if this neon 2reg-misc op is float-to-float */
    return (op == NEON_2RM_VABS_F || op == NEON_2RM_VNEG_F || op >= NEON_2RM_VRECPE_F);
}

/* Each entry in this array has bit n set if the insn allows
 * size value n (otherwise it will UNDEF). Since unallocated
 * op values will have no bits set they always UNDEF.
 */
static const uint8_t neon_2rm_sizes[] = {
    [NEON_2RM_VREV64] = 0x7,   [NEON_2RM_VREV32] = 0x3,  [NEON_2RM_VREV16] = 0x1,       [NEON_2RM_VPADDL] = 0x7,
    [NEON_2RM_VPADDL_U] = 0x7, [NEON_2RM_VCLS] = 0x7,    [NEON_2RM_VCLZ] = 0x7,         [NEON_2RM_VCNT] = 0x1,
    [NEON_2RM_VMVN] = 0x1,     [NEON_2RM_VPADAL] = 0x7,  [NEON_2RM_VPADAL_U] = 0x7,     [NEON_2RM_VQABS] = 0x7,
    [NEON_2RM_VQNEG] = 0x7,    [NEON_2RM_VCGT0] = 0x7,   [NEON_2RM_VCGE0] = 0x7,        [NEON_2RM_VCEQ0] = 0x7,
    [NEON_2RM_VCLE0] = 0x7,    [NEON_2RM_VCLT0] = 0x7,   [NEON_2RM_VABS] = 0x7,         [NEON_2RM_VNEG] = 0x7,
    [NEON_2RM_VCGT0_F] = 0x4,  [NEON_2RM_VCGE0_F] = 0x4, [NEON_2RM_VCEQ0_F] = 0x4,      [NEON_2RM_VCLE0_F] = 0x4,
    [NEON_2RM_VCLT0_F] = 0x4,  [NEON_2RM_VABS_F] = 0x4,  [NEON_2RM_VNEG_F] = 0x4,       [NEON_2RM_VSWP] = 0x1,
    [NEON_2RM_VTRN] = 0x7,     [NEON_2RM_VUZP] = 0x7,    [NEON_2RM_VZIP] = 0x7,         [NEON_2RM_VMOVN] = 0x7,
    [NEON_2RM_VQMOVN] = 0x7,   [NEON_2RM_VSHLL] = 0x7,   [NEON_2RM_VCVT_F16_F32] = 0x2, [NEON_2RM_VCVT_F32_F16] = 0x2,
    [NEON_2RM_VRECPE] = 0x4,   [NEON_2RM_VRSQRTE] = 0x4, [NEON_2RM_VRECPE_F] = 0x4,     [NEON_2RM_VRSQRTE_F] = 0x4,
    [NEON_2RM_VCVT_FS] = 0x4,  [NEON_2RM_VCVT_FU] = 0x4, [NEON_2RM_VCVT_SF] = 0x4,      [NEON_2RM_VCVT_UF] = 0x4,
};

/* Translate a NEON data processing instruction.  Return nonzero if the
   instruction is invalid.
   We process data in a mixture of 32-bit and 64-bit chunks.
   Mostly we use 32-bit chunks so we can use normal scalar instructions.  */

static int disas_neon_data_insn(CPUState *env, DisasContext *s, uint32_t insn)
{
    int op;
    int q;
    int rd, rn, rm;
    int size;
    int shift;
    int pass;
    int count;
    int pairwise;
    int u;
    uint32_t imm = 0;
    uint32_t mask = 0;
    TCGv tmp, tmp2, tmp3, tmp4, tmp5;
    TCGv_i64 tmp64;

    if(!s->vfp_enabled) {
        return 1;
    }
    q = (insn & (1 << 6)) != 0;
    u = (insn >> 24) & 1;
    VFP_DREG_D(rd, insn);
    VFP_DREG_N(rn, insn);
    VFP_DREG_M(rm, insn);
    size = (insn >> 20) & 3;
    if((insn & (1 << 23)) == 0) {
        /* Three register same length.  */
        op = ((insn >> 7) & 0x1e) | ((insn >> 4) & 1);
        /* Catch invalid op and bad size combinations: UNDEF */
        if((neon_3r_sizes[op] & (1 << size)) == 0) {
            return 1;
        }
        /* All insns of this form UNDEF for either this condition or the
         * superset of cases "Q==1"; we catch the latter later.
         */
        if(q && ((rd | rn | rm) & 1)) {
            return 1;
        }
        if(size == 3 && op != NEON_3R_LOGIC) {
            /* 64-bit element instructions. */
            for(pass = 0; pass < (q ? 2 : 1); pass++) {
                neon_load_reg64(cpu_V0, rn + pass);
                neon_load_reg64(cpu_V1, rm + pass);
                switch(op) {
                    case NEON_3R_VQADD:
                        if(u) {
                            gen_helper_neon_qadd_u64(cpu_V0, cpu_env, cpu_V0, cpu_V1);
                        } else {
                            gen_helper_neon_qadd_s64(cpu_V0, cpu_env, cpu_V0, cpu_V1);
                        }
                        break;
                    case NEON_3R_VQSUB:
                        if(u) {
                            gen_helper_neon_qsub_u64(cpu_V0, cpu_env, cpu_V0, cpu_V1);
                        } else {
                            gen_helper_neon_qsub_s64(cpu_V0, cpu_env, cpu_V0, cpu_V1);
                        }
                        break;
                    case NEON_3R_VSHL:
                        if(u) {
                            gen_helper_neon_shl_u64(cpu_V0, cpu_V1, cpu_V0);
                        } else {
                            gen_helper_neon_shl_s64(cpu_V0, cpu_V1, cpu_V0);
                        }
                        break;
                    case NEON_3R_VQSHL:
                        if(u) {
                            gen_helper_neon_qshl_u64(cpu_V0, cpu_env, cpu_V1, cpu_V0);
                        } else {
                            gen_helper_neon_qshl_s64(cpu_V0, cpu_env, cpu_V1, cpu_V0);
                        }
                        break;
                    case NEON_3R_VRSHL:
                        if(u) {
                            gen_helper_neon_rshl_u64(cpu_V0, cpu_V1, cpu_V0);
                        } else {
                            gen_helper_neon_rshl_s64(cpu_V0, cpu_V1, cpu_V0);
                        }
                        break;
                    case NEON_3R_VQRSHL:
                        if(u) {
                            gen_helper_neon_qrshl_u64(cpu_V0, cpu_env, cpu_V1, cpu_V0);
                        } else {
                            gen_helper_neon_qrshl_s64(cpu_V0, cpu_env, cpu_V1, cpu_V0);
                        }
                        break;
                    case NEON_3R_VADD_VSUB:
                        if(u) {
                            tcg_gen_sub_i64(CPU_V001);
                        } else {
                            tcg_gen_add_i64(CPU_V001);
                        }
                        break;
                    default:
                        abort();
                }
                neon_store_reg64(cpu_V0, rd + pass);
            }
            return 0;
        }
        pairwise = 0;
        switch(op) {
            case NEON_3R_VSHL:
            case NEON_3R_VQSHL:
            case NEON_3R_VRSHL:
            case NEON_3R_VQRSHL: {
                int rtmp;
                /* Shift instruction operands are reversed.  */
                rtmp = rn;
                rn = rm;
                rm = rtmp;
            } break;
            case NEON_3R_VPADD:
                if(u) {
                    return 1;
                }
            /* Fall through */
            case NEON_3R_VPMAX:
            case NEON_3R_VPMIN:
                pairwise = 1;
                break;
            case NEON_3R_FLOAT_ARITH:
                pairwise = (u && size < 2); /* if VPADD (float) */
                break;
            case NEON_3R_FLOAT_MINMAX:
                pairwise = u; /* if VPMIN/VPMAX (float) */
                break;
            case NEON_3R_FLOAT_CMP:
                if(!u && size) {
                    /* no encoding for U=0 C=1x */
                    return 1;
                }
                break;
            case NEON_3R_FLOAT_ACMP:
                if(!u) {
                    return 1;
                }
                break;
            case NEON_3R_VRECPS_VRSQRTS:
                if(u) {
                    return 1;
                }
                break;
            case NEON_3R_VMUL:
                if(u && (size != 0)) {
                    /* UNDEF on invalid size for polynomial subcase */
                    return 1;
                }
                break;
            case NEON_3R_VFM:
                if(!arm_feature(env, ARM_FEATURE_VFP4) || u) {
                    return 1;
                }
                break;
            default:
                break;
        }

        if(pairwise && q) {
            /* All the pairwise insns UNDEF if Q is set */
            return 1;
        }

        for(pass = 0; pass < (q ? 4 : 2); pass++) {

            if(pairwise) {
                /* Pairwise.  */
                if(pass < 1) {
                    tmp = neon_load_reg(rn, 0);
                    tmp2 = neon_load_reg(rn, 1);
                } else {
                    tmp = neon_load_reg(rm, 0);
                    tmp2 = neon_load_reg(rm, 1);
                }
            } else {
                /* Elementwise.  */
                tmp = neon_load_reg(rn, pass);
                tmp2 = neon_load_reg(rm, pass);
            }
            switch(op) {
                case NEON_3R_VHADD:
                    GEN_NEON_INTEGER_OP(hadd);
                    break;
                case NEON_3R_VQADD:
                    GEN_NEON_INTEGER_OP_ENV(qadd);
                    break;
                case NEON_3R_VRHADD:
                    GEN_NEON_INTEGER_OP(rhadd);
                    break;
                case NEON_3R_LOGIC: /* Logic ops.  */
                    switch((u << 2) | size) {
                        case 0: /* VAND */
                            tcg_gen_and_i32(tmp, tmp, tmp2);
                            break;
                        case 1: /* BIC */
                            tcg_gen_andc_i32(tmp, tmp, tmp2);
                            break;
                        case 2: /* VORR */
                            tcg_gen_or_i32(tmp, tmp, tmp2);
                            break;
                        case 3: /* VORN */
                            tcg_gen_orc_i32(tmp, tmp, tmp2);
                            break;
                        case 4: /* VEOR */
                            tcg_gen_xor_i32(tmp, tmp, tmp2);
                            break;
                        case 5: /* VBSL */
                            tmp3 = neon_load_reg(rd, pass);
                            gen_neon_bsl(tmp, tmp, tmp2, tmp3);
                            tcg_temp_free_i32(tmp3);
                            break;
                        case 6: /* VBIT */
                            tmp3 = neon_load_reg(rd, pass);
                            gen_neon_bsl(tmp, tmp, tmp3, tmp2);
                            tcg_temp_free_i32(tmp3);
                            break;
                        case 7: /* VBIF */
                            tmp3 = neon_load_reg(rd, pass);
                            gen_neon_bsl(tmp, tmp3, tmp, tmp2);
                            tcg_temp_free_i32(tmp3);
                            break;
                    }
                    break;
                case NEON_3R_VHSUB:
                    GEN_NEON_INTEGER_OP(hsub);
                    break;
                case NEON_3R_VQSUB:
                    GEN_NEON_INTEGER_OP_ENV(qsub);
                    break;
                case NEON_3R_VCGT:
                    GEN_NEON_INTEGER_OP(cgt);
                    break;
                case NEON_3R_VCGE:
                    GEN_NEON_INTEGER_OP(cge);
                    break;
                case NEON_3R_VSHL:
                    GEN_NEON_INTEGER_OP(shl);
                    break;
                case NEON_3R_VQSHL:
                    GEN_NEON_INTEGER_OP_ENV(qshl);
                    break;
                case NEON_3R_VRSHL:
                    GEN_NEON_INTEGER_OP(rshl);
                    break;
                case NEON_3R_VQRSHL:
                    GEN_NEON_INTEGER_OP_ENV(qrshl);
                    break;
                case NEON_3R_VMAX:
                    GEN_NEON_INTEGER_OP(max);
                    break;
                case NEON_3R_VMIN:
                    GEN_NEON_INTEGER_OP(min);
                    break;
                case NEON_3R_VABD:
                    GEN_NEON_INTEGER_OP(abd);
                    break;
                case NEON_3R_VABA:
                    GEN_NEON_INTEGER_OP(abd);
                    tcg_temp_free_i32(tmp2);
                    tmp2 = neon_load_reg(rd, pass);
                    gen_neon_add(size, tmp, tmp2);
                    break;
                case NEON_3R_VADD_VSUB:
                    if(!u) { /* VADD */
                        gen_neon_add(size, tmp, tmp2);
                    } else { /* VSUB */
                        switch(size) {
                            case 0:
                                gen_helper_neon_sub_u8(tmp, tmp, tmp2);
                                break;
                            case 1:
                                gen_helper_neon_sub_u16(tmp, tmp, tmp2);
                                break;
                            case 2:
                                tcg_gen_sub_i32(tmp, tmp, tmp2);
                                break;
                            default:
                                abort();
                        }
                    }
                    break;
                case NEON_3R_VTST_VCEQ:
                    if(!u) { /* VTST */
                        switch(size) {
                            case 0:
                                gen_helper_neon_tst_u8(tmp, tmp, tmp2);
                                break;
                            case 1:
                                gen_helper_neon_tst_u16(tmp, tmp, tmp2);
                                break;
                            case 2:
                                gen_helper_neon_tst_u32(tmp, tmp, tmp2);
                                break;
                            default:
                                abort();
                        }
                    } else { /* VCEQ */
                        switch(size) {
                            case 0:
                                gen_helper_neon_ceq_u8(tmp, tmp, tmp2);
                                break;
                            case 1:
                                gen_helper_neon_ceq_u16(tmp, tmp, tmp2);
                                break;
                            case 2:
                                gen_helper_neon_ceq_u32(tmp, tmp, tmp2);
                                break;
                            default:
                                abort();
                        }
                    }
                    break;
                case NEON_3R_VML: /* VMLA, VMLAL, VMLS,VMLSL */
                    switch(size) {
                        case 0:
                            gen_helper_neon_mul_u8(tmp, tmp, tmp2);
                            break;
                        case 1:
                            gen_helper_neon_mul_u16(tmp, tmp, tmp2);
                            break;
                        case 2:
                            tcg_gen_mul_i32(tmp, tmp, tmp2);
                            break;
                        default:
                            abort();
                    }
                    tcg_temp_free_i32(tmp2);
                    tmp2 = neon_load_reg(rd, pass);
                    if(u) { /* VMLS */
                        gen_neon_rsb(size, tmp, tmp2);
                    } else { /* VMLA */
                        gen_neon_add(size, tmp, tmp2);
                    }
                    break;
                case NEON_3R_VMUL:
                    if(u) { /* polynomial */
                        gen_helper_neon_mul_p8(tmp, tmp, tmp2);
                    } else { /* Integer */
                        switch(size) {
                            case 0:
                                gen_helper_neon_mul_u8(tmp, tmp, tmp2);
                                break;
                            case 1:
                                gen_helper_neon_mul_u16(tmp, tmp, tmp2);
                                break;
                            case 2:
                                tcg_gen_mul_i32(tmp, tmp, tmp2);
                                break;
                            default:
                                abort();
                        }
                    }
                    break;
                case NEON_3R_VPMAX:
                    GEN_NEON_INTEGER_OP(pmax);
                    break;
                case NEON_3R_VPMIN:
                    GEN_NEON_INTEGER_OP(pmin);
                    break;
                case NEON_3R_VQDMULH_VQRDMULH: /* Multiply high.  */
                    if(!u) {                   /* VQDMULH */
                        switch(size) {
                            case 1:
                                gen_helper_neon_qdmulh_s16(tmp, cpu_env, tmp, tmp2);
                                break;
                            case 2:
                                gen_helper_neon_qdmulh_s32(tmp, cpu_env, tmp, tmp2);
                                break;
                            default:
                                abort();
                        }
                    } else { /* VQRDMULH */
                        switch(size) {
                            case 1:
                                gen_helper_neon_qrdmulh_s16(tmp, cpu_env, tmp, tmp2);
                                break;
                            case 2:
                                gen_helper_neon_qrdmulh_s32(tmp, cpu_env, tmp, tmp2);
                                break;
                            default:
                                abort();
                        }
                    }
                    break;
                case NEON_3R_VPADD:
                    switch(size) {
                        case 0:
                            gen_helper_neon_padd_u8(tmp, tmp, tmp2);
                            break;
                        case 1:
                            gen_helper_neon_padd_u16(tmp, tmp, tmp2);
                            break;
                        case 2:
                            tcg_gen_add_i32(tmp, tmp, tmp2);
                            break;
                        default:
                            abort();
                    }
                    break;
                case NEON_3R_FLOAT_ARITH: /* Floating point arithmetic. */
                {
                    TCGv_ptr fpstatus = get_fpstatus_ptr(1);
                    switch((u << 2) | size) {
                        case 0: /* VADD */
                        case 4: /* VPADD */
                            gen_helper_vfp_adds(tmp, tmp, tmp2, fpstatus);
                            break;
                        case 2: /* VSUB */
                            gen_helper_vfp_subs(tmp, tmp, tmp2, fpstatus);
                            break;
                        case 6: /* VABD */
                            gen_helper_neon_abd_f32(tmp, tmp, tmp2, fpstatus);
                            break;
                        default:
                            abort();
                    }
                    tcg_temp_free_ptr(fpstatus);
                    break;
                }
                case NEON_3R_FLOAT_MULTIPLY: {
                    TCGv_ptr fpstatus = get_fpstatus_ptr(1);
                    gen_helper_vfp_muls(tmp, tmp, tmp2, fpstatus);
                    if(!u) {
                        tcg_temp_free_i32(tmp2);
                        tmp2 = neon_load_reg(rd, pass);
                        if(size == 0) {
                            gen_helper_vfp_adds(tmp, tmp, tmp2, fpstatus);
                        } else {
                            gen_helper_vfp_subs(tmp, tmp2, tmp, fpstatus);
                        }
                    }
                    tcg_temp_free_ptr(fpstatus);
                    break;
                }
                case NEON_3R_FLOAT_CMP: {
                    TCGv_ptr fpstatus = get_fpstatus_ptr(1);
                    if(!u) {
                        gen_helper_neon_ceq_f32(tmp, tmp, tmp2, fpstatus);
                    } else {
                        if(size == 0) {
                            gen_helper_neon_cge_f32(tmp, tmp, tmp2, fpstatus);
                        } else {
                            gen_helper_neon_cgt_f32(tmp, tmp, tmp2, fpstatus);
                        }
                    }
                    tcg_temp_free_ptr(fpstatus);
                    break;
                }
                case NEON_3R_FLOAT_ACMP: {
                    TCGv_ptr fpstatus = get_fpstatus_ptr(1);
                    if(size == 0) {
                        gen_helper_neon_acge_f32(tmp, tmp, tmp2, fpstatus);
                    } else {
                        gen_helper_neon_acgt_f32(tmp, tmp, tmp2, fpstatus);
                    }
                    tcg_temp_free_ptr(fpstatus);
                    break;
                }
                case NEON_3R_FLOAT_MINMAX: {
                    TCGv_ptr fpstatus = get_fpstatus_ptr(1);
                    if(size == 0) {
                        gen_helper_neon_max_f32(tmp, tmp, tmp2, fpstatus);
                    } else {
                        gen_helper_neon_min_f32(tmp, tmp, tmp2, fpstatus);
                    }
                    tcg_temp_free_ptr(fpstatus);
                    break;
                }
                case NEON_3R_VRECPS_VRSQRTS:
                    if(size == 0) {
                        gen_helper_recps_f32(tmp, tmp, tmp2, cpu_env);
                    } else {
                        gen_helper_rsqrts_f32(tmp, tmp, tmp2, cpu_env);
                    }
                    break;
                case NEON_3R_VFM: {
                    /* VFMA, VFMS: fused multiply-add */
                    TCGv_ptr fpstatus = get_fpstatus_ptr(1);
                    TCGv_i32 tmp3 = neon_load_reg(rd, pass);
                    if(size) {
                        /* VFMS */
                        gen_helper_vfp_negs(tmp, tmp);
                    }
                    gen_helper_vfp_muladds(tmp, tmp, tmp2, tmp3, fpstatus);
                    tcg_temp_free_i32(tmp3);
                    tcg_temp_free_ptr(fpstatus);
                    break;
                }
                default:
                    abort();
            }
            tcg_temp_free_i32(tmp2);

            /* Save the result.  For elementwise operations we can put it
               straight into the destination register.  For pairwise operations
               we have to be careful to avoid clobbering the source operands.  */
            if(pairwise && rd == rm) {
                neon_store_scratch(pass, tmp);
            } else {
                neon_store_reg(rd, pass, tmp);
            }

        } /* for pass */
        if(pairwise && rd == rm) {
            for(pass = 0; pass < (q ? 4 : 2); pass++) {
                tmp = neon_load_scratch(pass);
                neon_store_reg(rd, pass, tmp);
            }
        }
        /* End of 3 register same size operations.  */
    } else if(insn & (1 << 4)) {
        if((insn & 0x00380080) != 0) {
            /* Two registers and shift.  */
            op = (insn >> 8) & 0xf;
            if(insn & (1 << 7)) {
                /* 64-bit shift. */
                if(op > 7) {
                    return 1;
                }
                size = 3;
            } else {
                size = 2;
                while((insn & (1 << (size + 19))) == 0) {
                    size--;
                }
            }
            shift = (insn >> 16) & ((1 << (3 + size)) - 1);
            /* To avoid excessive dumplication of ops we implement shift
               by immediate using the variable shift operations.  */
            if(op < 8) {
                /* Shift by immediate:
                   VSHR, VSRA, VRSHR, VRSRA, VSRI, VSHL, VQSHL, VQSHLU.  */
                if(q && ((rd | rm) & 1)) {
                    return 1;
                }
                if(!u && (op == 4 || op == 6)) {
                    return 1;
                }
                /* Right shifts are encoded as N - shift, where N is the
                   element size in bits.  */
                if(op <= 4) {
                    shift = shift - (1 << (size + 3));
                }
                if(size == 3) {
                    count = q + 1;
                } else {
                    count = q ? 4 : 2;
                }
                switch(size) {
                    case 0:
                        imm = (uint8_t)shift;
                        imm |= imm << 8;
                        imm |= imm << 16;
                        break;
                    case 1:
                        imm = (uint16_t)shift;
                        imm |= imm << 16;
                        break;
                    case 2:
                    case 3:
                        imm = shift;
                        break;
                    default:
                        abort();
                }

                for(pass = 0; pass < count; pass++) {
                    if(size == 3) {
                        neon_load_reg64(cpu_V0, rm + pass);
                        tcg_gen_movi_i64(cpu_V1, imm);
                        switch(op) {
                            case 0: /* VSHR */
                            case 1: /* VSRA */
                                if(u) {
                                    gen_helper_neon_shl_u64(cpu_V0, cpu_V0, cpu_V1);
                                } else {
                                    gen_helper_neon_shl_s64(cpu_V0, cpu_V0, cpu_V1);
                                }
                                break;
                            case 2: /* VRSHR */
                            case 3: /* VRSRA */
                                if(u) {
                                    gen_helper_neon_rshl_u64(cpu_V0, cpu_V0, cpu_V1);
                                } else {
                                    gen_helper_neon_rshl_s64(cpu_V0, cpu_V0, cpu_V1);
                                }
                                break;
                            case 4: /* VSRI */
                            case 5: /* VSHL, VSLI */
                                gen_helper_neon_shl_u64(cpu_V0, cpu_V0, cpu_V1);
                                break;
                            case 6: /* VQSHLU */
                                gen_helper_neon_qshlu_s64(cpu_V0, cpu_env, cpu_V0, cpu_V1);
                                break;
                            case 7: /* VQSHL */
                                if(u) {
                                    gen_helper_neon_qshl_u64(cpu_V0, cpu_env, cpu_V0, cpu_V1);
                                } else {
                                    gen_helper_neon_qshl_s64(cpu_V0, cpu_env, cpu_V0, cpu_V1);
                                }
                                break;
                        }
                        if(op == 1 || op == 3) {
                            /* Accumulate.  */
                            neon_load_reg64(cpu_V1, rd + pass);
                            tcg_gen_add_i64(cpu_V0, cpu_V0, cpu_V1);
                        } else if(op == 4 || (op == 5 && u)) {
                            /* Insert */
                            neon_load_reg64(cpu_V1, rd + pass);
                            uint64_t mask;
                            if(shift < -63 || shift > 63) {
                                mask = 0;
                            } else {
                                if(op == 4) {
                                    mask = 0xffffffffffffffffull >> -shift;
                                } else {
                                    mask = 0xffffffffffffffffull << shift;
                                }
                            }
                            tcg_gen_andi_i64(cpu_V1, cpu_V1, ~mask);
                            tcg_gen_or_i64(cpu_V0, cpu_V0, cpu_V1);
                        }
                        neon_store_reg64(cpu_V0, rd + pass);
                    } else { /* size < 3 */
                        /* Operands in T0 and T1.  */
                        tmp = neon_load_reg(rm, pass);
                        tmp2 = tcg_temp_new_i32();
                        tcg_gen_movi_i32(tmp2, imm);
                        switch(op) {
                            case 0: /* VSHR */
                            case 1: /* VSRA */
                                GEN_NEON_INTEGER_OP(shl);
                                break;
                            case 2: /* VRSHR */
                            case 3: /* VRSRA */
                                GEN_NEON_INTEGER_OP(rshl);
                                break;
                            case 4: /* VSRI */
                            case 5: /* VSHL, VSLI */
                                switch(size) {
                                    case 0:
                                        gen_helper_neon_shl_u8(tmp, tmp, tmp2);
                                        break;
                                    case 1:
                                        gen_helper_neon_shl_u16(tmp, tmp, tmp2);
                                        break;
                                    case 2:
                                        gen_helper_neon_shl_u32(tmp, tmp, tmp2);
                                        break;
                                    default:
                                        abort();
                                }
                                break;
                            case 6: /* VQSHLU */
                                switch(size) {
                                    case 0:
                                        gen_helper_neon_qshlu_s8(tmp, cpu_env, tmp, tmp2);
                                        break;
                                    case 1:
                                        gen_helper_neon_qshlu_s16(tmp, cpu_env, tmp, tmp2);
                                        break;
                                    case 2:
                                        gen_helper_neon_qshlu_s32(tmp, cpu_env, tmp, tmp2);
                                        break;
                                    default:
                                        abort();
                                }
                                break;
                            case 7: /* VQSHL */
                                GEN_NEON_INTEGER_OP_ENV(qshl);
                                break;
                        }
                        tcg_temp_free_i32(tmp2);

                        if(op == 1 || op == 3) {
                            /* Accumulate.  */
                            tmp2 = neon_load_reg(rd, pass);
                            gen_neon_add(size, tmp, tmp2);
                            tcg_temp_free_i32(tmp2);
                        } else if(op == 4 || (op == 5 && u)) {
                            /* Insert */
                            switch(size) {
                                case 0:
                                    if(op == 4) {
                                        mask = 0xff >> -shift;
                                    } else {
                                        mask = (uint8_t)(0xff << shift);
                                    }
                                    mask |= mask << 8;
                                    mask |= mask << 16;
                                    break;
                                case 1:
                                    if(op == 4) {
                                        mask = 0xffff >> -shift;
                                    } else {
                                        mask = (uint16_t)(0xffff << shift);
                                    }
                                    mask |= mask << 16;
                                    break;
                                case 2:
                                    if(shift < -31 || shift > 31) {
                                        mask = 0;
                                    } else {
                                        if(op == 4) {
                                            mask = 0xffffffffu >> -shift;
                                        } else {
                                            mask = 0xffffffffu << shift;
                                        }
                                    }
                                    break;
                                default:
                                    abort();
                            }
                            tmp2 = neon_load_reg(rd, pass);
                            tcg_gen_andi_i32(tmp, tmp, mask);
                            tcg_gen_andi_i32(tmp2, tmp2, ~mask);
                            tcg_gen_or_i32(tmp, tmp, tmp2);
                            tcg_temp_free_i32(tmp2);
                        }
                        neon_store_reg(rd, pass, tmp);
                    }
                } /* for pass */
            } else if(op < 10) {
                /* Shift by immediate and narrow:
                   VSHRN, VRSHRN, VQSHRN, VQRSHRN.  */
                int input_unsigned = (op == 8) ? !u : u;
                if(rm & 1) {
                    return 1;
                }
                shift = shift - (1 << (size + 3));
                size++;
                if(size == 3) {
                    tmp64 = tcg_const_i64(shift);
                    neon_load_reg64(cpu_V0, rm);
                    neon_load_reg64(cpu_V1, rm + 1);
                    for(pass = 0; pass < 2; pass++) {
                        TCGv_i64 in;
                        if(pass == 0) {
                            in = cpu_V0;
                        } else {
                            in = cpu_V1;
                        }
                        if(q) {
                            if(input_unsigned) {
                                gen_helper_neon_rshl_u64(cpu_V0, in, tmp64);
                            } else {
                                gen_helper_neon_rshl_s64(cpu_V0, in, tmp64);
                            }
                        } else {
                            if(input_unsigned) {
                                gen_helper_neon_shl_u64(cpu_V0, in, tmp64);
                            } else {
                                gen_helper_neon_shl_s64(cpu_V0, in, tmp64);
                            }
                        }
                        tmp = tcg_temp_new_i32();
                        gen_neon_narrow_op(op == 8, u, size - 1, tmp, cpu_V0);
                        neon_store_reg(rd, pass, tmp);
                    } /* for pass */
                    tcg_temp_free_i64(tmp64);
                } else {
                    if(size == 1) {
                        imm = (uint16_t)shift;
                        imm |= imm << 16;
                    } else {
                        /* size == 2 */
                        imm = (uint32_t)shift;
                    }
                    tmp2 = tcg_const_i32(imm);
                    tmp4 = neon_load_reg(rm + 1, 0);
                    tmp5 = neon_load_reg(rm + 1, 1);
                    for(pass = 0; pass < 2; pass++) {
                        if(pass == 0) {
                            tmp = neon_load_reg(rm, 0);
                        } else {
                            tmp = tmp4;
                        }
                        gen_neon_shift_narrow(size, tmp, tmp2, q, input_unsigned);
                        if(pass == 0) {
                            tmp3 = neon_load_reg(rm, 1);
                        } else {
                            tmp3 = tmp5;
                        }
                        gen_neon_shift_narrow(size, tmp3, tmp2, q, input_unsigned);
                        tcg_gen_concat_i32_i64(cpu_V0, tmp, tmp3);
                        tcg_temp_free_i32(tmp);
                        tcg_temp_free_i32(tmp3);
                        tmp = tcg_temp_new_i32();
                        gen_neon_narrow_op(op == 8, u, size - 1, tmp, cpu_V0);
                        neon_store_reg(rd, pass, tmp);
                    } /* for pass */
                    tcg_temp_free_i32(tmp2);
                }
            } else if(op == 10) {
                /* VSHLL, VMOVL */
                if(q || (rd & 1)) {
                    return 1;
                }
                tmp = neon_load_reg(rm, 0);
                tmp2 = neon_load_reg(rm, 1);
                for(pass = 0; pass < 2; pass++) {
                    if(pass == 1) {
                        tmp = tmp2;
                    }

                    gen_neon_widen(cpu_V0, tmp, size, u);

                    if(shift != 0) {
                        /* The shift is less than the width of the source
                           type, so we can just shift the whole register.  */
                        tcg_gen_shli_i64(cpu_V0, cpu_V0, shift);
                        /* Widen the result of shift: we need to clear
                         * the potential overflow bits resulting from
                         * left bits of the narrow input appearing as
                         * right bits of left the neighbour narrow
                         * input.  */
                        if(size < 2 || !u) {
                            uint64_t imm64;
                            if(size == 0) {
                                imm = (0xffu >> (8 - shift));
                                imm |= imm << 16;
                            } else if(size == 1) {
                                imm = 0xffff >> (16 - shift);
                            } else {
                                /* size == 2 */
                                imm = 0xffffffff >> (32 - shift);
                            }
                            if(size < 2) {
                                imm64 = imm | (((uint64_t)imm) << 32);
                            } else {
                                imm64 = imm;
                            }
                            tcg_gen_andi_i64(cpu_V0, cpu_V0, ~imm64);
                        }
                    }
                    neon_store_reg64(cpu_V0, rd + pass);
                }
            } else if(op >= 14) {
                /* VCVT fixed-point.  */
                if(!(insn & (1 << 21)) || (q && ((rd | rm) & 1))) {
                    return 1;
                }
                /* We have already masked out the must-be-1 top bit of imm6,
                 * hence this 32-shift where the ARM ARM has 64-imm6.
                 */
                shift = 32 - shift;
                for(pass = 0; pass < (q ? 4 : 2); pass++) {
                    tcg_gen_ld_f32(cpu_F0s, cpu_env, neon_reg_offset(rm, pass));
                    if(!(op & 1)) {
                        if(u) {
                            gen_vfp_ulto(0, shift, 1);
                        } else {
                            gen_vfp_slto(0, shift, 1);
                        }
                    } else {
                        if(u) {
                            gen_vfp_toul(0, shift, 1);
                        } else {
                            gen_vfp_tosl(0, shift, 1);
                        }
                    }
                    tcg_gen_st_f32(cpu_F0s, cpu_env, neon_reg_offset(rd, pass));
                }
            } else {
                return 1;
            }
        } else { /* (insn & 0x00380080) == 0 */
            int invert;
            if(q && (rd & 1)) {
                return 1;
            }

            op = (insn >> 8) & 0xf;
            /* One register and immediate.  */
            imm = (u << 7) | ((insn >> 12) & 0x70) | (insn & 0xf);
            invert = (insn & (1 << 5)) != 0;
            /* Note that op = 2,3,4,5,6,7,10,11,12,13 imm=0 is UNPREDICTABLE.
             * We choose to not special-case this and will behave as if a
             * valid constant encoding of 0 had been given.
             */
            switch(op) {
                case 0:
                case 1:
                    /* no-op */
                    break;
                case 2:
                case 3:
                    imm <<= 8;
                    break;
                case 4:
                case 5:
                    imm <<= 16;
                    break;
                case 6:
                case 7:
                    imm <<= 24;
                    break;
                case 8:
                case 9:
                    imm |= imm << 16;
                    break;
                case 10:
                case 11:
                    imm = (imm << 8) | (imm << 24);
                    break;
                case 12:
                    imm = (imm << 8) | 0xff;
                    break;
                case 13:
                    imm = (imm << 16) | 0xffff;
                    break;
                case 14:
                    imm |= (imm << 8) | (imm << 16) | (imm << 24);
                    if(invert) {
                        imm = ~imm;
                    }
                    break;
                case 15:
                    if(invert) {
                        return 1;
                    }
                    imm = ((imm & 0x80) << 24) | ((imm & 0x3f) << 19) | ((imm & 0x40) ? (0x1f << 25) : (1 << 30));
                    break;
            }
            if(invert) {
                imm = ~imm;
            }

            for(pass = 0; pass < (q ? 4 : 2); pass++) {
                if(op & 1 && op < 12) {
                    tmp = neon_load_reg(rd, pass);
                    if(invert) {
                        /* The immediate value has already been inverted, so
                           BIC becomes AND.  */
                        tcg_gen_andi_i32(tmp, tmp, imm);
                    } else {
                        tcg_gen_ori_i32(tmp, tmp, imm);
                    }
                } else {
                    /* VMOV, VMVN.  */
                    tmp = tcg_temp_new_i32();
                    if(op == 14 && invert) {
                        int n;
                        uint32_t val;
                        val = 0;
                        for(n = 0; n < 4; n++) {
                            if(imm & (1 << (n + (pass & 1) * 4))) {
                                val |= 0xff << (n * 8);
                            }
                        }
                        tcg_gen_movi_i32(tmp, val);
                    } else {
                        tcg_gen_movi_i32(tmp, imm);
                    }
                }
                neon_store_reg(rd, pass, tmp);
            }
        }
    } else { /* (insn & 0x00800010 == 0x00800000) */
        if(size != 3) {
            op = (insn >> 8) & 0xf;
            if((insn & (1 << 6)) == 0) {
                /* Three registers of different lengths.  */
                int src1_wide;
                int src2_wide;
                int prewiden;
                /* undefreq: bit 0 : UNDEF if size != 0
                 *           bit 1 : UNDEF if size == 0
                 *           bit 2 : UNDEF if U == 1
                 * Note that [1:0] set implies 'always UNDEF'
                 */
                int undefreq;
                /* prewiden, src1_wide, src2_wide, undefreq */
                static const int neon_3reg_wide[16][4] = {
                    { 1, 0, 0, 0 }, /* VADDL */
                    { 1, 1, 0, 0 }, /* VADDW */
                    { 1, 0, 0, 0 }, /* VSUBL */
                    { 1, 1, 0, 0 }, /* VSUBW */
                    { 0, 1, 1, 0 }, /* VADDHN */
                    { 0, 0, 0, 0 }, /* VABAL */
                    { 0, 1, 1, 0 }, /* VSUBHN */
                    { 0, 0, 0, 0 }, /* VABDL */
                    { 0, 0, 0, 0 }, /* VMLAL */
                    { 0, 0, 0, 6 }, /* VQDMLAL */
                    { 0, 0, 0, 0 }, /* VMLSL */
                    { 0, 0, 0, 6 }, /* VQDMLSL */
                    { 0, 0, 0, 0 }, /* Integer VMULL */
                    { 0, 0, 0, 2 }, /* VQDMULL */
                    { 0, 0, 0, 5 }, /* Polynomial VMULL */
                    { 0, 0, 0, 3 }, /* Reserved: always UNDEF */
                };

                prewiden = neon_3reg_wide[op][0];
                src1_wide = neon_3reg_wide[op][1];
                src2_wide = neon_3reg_wide[op][2];
                undefreq = neon_3reg_wide[op][3];

                if(((undefreq & 1) && (size != 0)) || ((undefreq & 2) && (size == 0)) || ((undefreq & 4) && u)) {
                    return 1;
                }
                if((src1_wide && (rn & 1)) || (src2_wide && (rm & 1)) || (!src2_wide && (rd & 1))) {
                    return 1;
                }

                /* Avoid overlapping operands.  Wide source operands are
                   always aligned so will never overlap with wide
                   destinations in problematic ways.  */
                if(rd == rm && !src2_wide) {
                    tmp = neon_load_reg(rm, 1);
                    neon_store_scratch(2, tmp);
                } else if(rd == rn && !src1_wide) {
                    tmp = neon_load_reg(rn, 1);
                    neon_store_scratch(2, tmp);
                }
                TCGV_UNUSED(tmp3);
                for(pass = 0; pass < 2; pass++) {
                    if(src1_wide) {
                        neon_load_reg64(cpu_V0, rn + pass);
                        TCGV_UNUSED(tmp);
                    } else {
                        if(pass == 1 && rd == rn) {
                            tmp = neon_load_scratch(2);
                        } else {
                            tmp = neon_load_reg(rn, pass);
                        }
                        if(prewiden) {
                            gen_neon_widen(cpu_V0, tmp, size, u);
                        }
                    }
                    if(src2_wide) {
                        neon_load_reg64(cpu_V1, rm + pass);
                        TCGV_UNUSED(tmp2);
                    } else {
                        if(pass == 1 && rd == rm) {
                            tmp2 = neon_load_scratch(2);
                        } else {
                            tmp2 = neon_load_reg(rm, pass);
                        }
                        if(prewiden) {
                            gen_neon_widen(cpu_V1, tmp2, size, u);
                        }
                    }
                    switch(op) {
                        case 0:  //  VADDL, VADDW, VADDHN, VRADDHN
                        case 1:
                        case 4:
                            gen_neon_addl(size);
                            break;
                        case 2:  //  VSUBL, VSUBW, VSUBHN, VRSUBHN
                        case 3:
                        case 6:
                            gen_neon_subl(size);
                            break;
                        case 5:  //  VABAL, VABDL
                        case 7:
                            switch((size << 1) | u) {
                                case 0:
                                    gen_helper_neon_abdl_s16(cpu_V0, tmp, tmp2);
                                    break;
                                case 1:
                                    gen_helper_neon_abdl_u16(cpu_V0, tmp, tmp2);
                                    break;
                                case 2:
                                    gen_helper_neon_abdl_s32(cpu_V0, tmp, tmp2);
                                    break;
                                case 3:
                                    gen_helper_neon_abdl_u32(cpu_V0, tmp, tmp2);
                                    break;
                                case 4:
                                    gen_helper_neon_abdl_s64(cpu_V0, tmp, tmp2);
                                    break;
                                case 5:
                                    gen_helper_neon_abdl_u64(cpu_V0, tmp, tmp2);
                                    break;
                                default:
                                    abort();
                            }
                            tcg_temp_free_i32(tmp2);
                            tcg_temp_free_i32(tmp);
                            break;
                        case 8:
                        case 9:
                        case 10:
                        case 11:
                        case 12:
                        case 13:
                            /* VMLAL, VQDMLAL, VMLSL, VQDMLSL, VMULL, VQDMULL */
                            gen_neon_mull(cpu_V0, tmp, tmp2, size, u);
                            break;
                        case 14: /* Polynomial VMULL */
                            gen_helper_neon_mull_p8(cpu_V0, tmp, tmp2);
                            tcg_temp_free_i32(tmp2);
                            tcg_temp_free_i32(tmp);
                            break;
                        default: /* 15 is RESERVED: caught earlier  */
                            abort();
                    }
                    if(op == 13) {
                        /* VQDMULL */
                        gen_neon_addl_saturate(cpu_V0, cpu_V0, size);
                        neon_store_reg64(cpu_V0, rd + pass);
                    } else if(op == 5 || (op >= 8 && op <= 11)) {
                        /* Accumulate.  */
                        neon_load_reg64(cpu_V1, rd + pass);
                        switch(op) {
                            case 10: /* VMLSL */
                                gen_neon_negl(cpu_V0, size);
                            /* Fall through */
                            case 5:  //  VABAL, VMLAL
                            case 8:
                                gen_neon_addl(size);
                                break;
                            case 9:  //  VQDMLAL, VQDMLSL
                            case 11:
                                gen_neon_addl_saturate(cpu_V0, cpu_V0, size);
                                if(op == 11) {
                                    gen_neon_negl(cpu_V0, size);
                                }
                                gen_neon_addl_saturate(cpu_V0, cpu_V1, size);
                                break;
                            default:
                                abort();
                        }
                        neon_store_reg64(cpu_V0, rd + pass);
                    } else if(op == 4 || op == 6) {
                        /* Narrowing operation.  */
                        tmp = tcg_temp_new_i32();
                        if(!u) {
                            switch(size) {
                                case 0:
                                    gen_helper_neon_narrow_high_u8(tmp, cpu_V0);
                                    break;
                                case 1:
                                    gen_helper_neon_narrow_high_u16(tmp, cpu_V0);
                                    break;
                                case 2:
                                    tcg_gen_shri_i64(cpu_V0, cpu_V0, 32);
                                    tcg_gen_trunc_i64_i32(tmp, cpu_V0);
                                    break;
                                default:
                                    abort();
                            }
                        } else {
                            switch(size) {
                                case 0:
                                    gen_helper_neon_narrow_round_high_u8(tmp, cpu_V0);
                                    break;
                                case 1:
                                    gen_helper_neon_narrow_round_high_u16(tmp, cpu_V0);
                                    break;
                                case 2:
                                    tcg_gen_addi_i64(cpu_V0, cpu_V0, 1u << 31);
                                    tcg_gen_shri_i64(cpu_V0, cpu_V0, 32);
                                    tcg_gen_trunc_i64_i32(tmp, cpu_V0);
                                    break;
                                default:
                                    abort();
                            }
                        }
                        if(pass == 0) {
                            tmp3 = tmp;
                        } else {
                            neon_store_reg(rd, 0, tmp3);
                            neon_store_reg(rd, 1, tmp);
                        }
                    } else {
                        /* Write back the result.  */
                        neon_store_reg64(cpu_V0, rd + pass);
                    }
                }
            } else {
                /* Two registers and a scalar. NB that for ops of this form
                 * the ARM ARM labels bit 24 as Q, but it is in our variable
                 * 'u', not 'q'.
                 */
                if(size == 0) {
                    return 1;
                }
                switch(op) {
                    case 1: /* Float VMLA scalar */
                    case 5: /* Floating point VMLS scalar */
                    case 9: /* Floating point VMUL scalar */
                        if(size == 1) {
                            return 1;
                        }
                    /* fall through */
                    case 0:  /* Integer VMLA scalar */
                    case 4:  /* Integer VMLS scalar */
                    case 8:  /* Integer VMUL scalar */
                    case 12: /* VQDMULH scalar */
                    case 13: /* VQRDMULH scalar */
                        if(u && ((rd | rn) & 1)) {
                            return 1;
                        }
                        tmp = neon_get_scalar(size, rm);
                        neon_store_scratch(0, tmp);
                        for(pass = 0; pass < (u ? 4 : 2); pass++) {
                            tmp = neon_load_scratch(0);
                            tmp2 = neon_load_reg(rn, pass);
                            if(op == 12) {
                                if(size == 1) {
                                    gen_helper_neon_qdmulh_s16(tmp, cpu_env, tmp, tmp2);
                                } else {
                                    gen_helper_neon_qdmulh_s32(tmp, cpu_env, tmp, tmp2);
                                }
                            } else if(op == 13) {
                                if(size == 1) {
                                    gen_helper_neon_qrdmulh_s16(tmp, cpu_env, tmp, tmp2);
                                } else {
                                    gen_helper_neon_qrdmulh_s32(tmp, cpu_env, tmp, tmp2);
                                }
                            } else if(op & 1) {
                                TCGv_ptr fpstatus = get_fpstatus_ptr(1);
                                gen_helper_vfp_muls(tmp, tmp, tmp2, fpstatus);
                                tcg_temp_free_ptr(fpstatus);
                            } else {
                                switch(size) {
                                    case 0:
                                        gen_helper_neon_mul_u8(tmp, tmp, tmp2);
                                        break;
                                    case 1:
                                        gen_helper_neon_mul_u16(tmp, tmp, tmp2);
                                        break;
                                    case 2:
                                        tcg_gen_mul_i32(tmp, tmp, tmp2);
                                        break;
                                    default:
                                        abort();
                                }
                            }
                            tcg_temp_free_i32(tmp2);
                            if(op < 8) {
                                /* Accumulate.  */
                                tmp2 = neon_load_reg(rd, pass);
                                switch(op) {
                                    case 0:
                                        gen_neon_add(size, tmp, tmp2);
                                        break;
                                    case 1: {
                                        TCGv_ptr fpstatus = get_fpstatus_ptr(1);
                                        gen_helper_vfp_adds(tmp, tmp, tmp2, fpstatus);
                                        tcg_temp_free_ptr(fpstatus);
                                        break;
                                    }
                                    case 4:
                                        gen_neon_rsb(size, tmp, tmp2);
                                        break;
                                    case 5: {
                                        TCGv_ptr fpstatus = get_fpstatus_ptr(1);
                                        gen_helper_vfp_subs(tmp, tmp2, tmp, fpstatus);
                                        tcg_temp_free_ptr(fpstatus);
                                        break;
                                    }
                                    default:
                                        abort();
                                }
                                tcg_temp_free_i32(tmp2);
                            }
                            neon_store_reg(rd, pass, tmp);
                        }
                        break;
                    case 3:  /* VQDMLAL scalar */
                    case 7:  /* VQDMLSL scalar */
                    case 11: /* VQDMULL scalar */
                        if(u == 1) {
                            return 1;
                        }
                    /* fall through */
                    case 2:  /* VMLAL sclar */
                    case 6:  /* VMLSL scalar */
                    case 10: /* VMULL scalar */
                        if(rd & 1) {
                            return 1;
                        }
                        tmp2 = neon_get_scalar(size, rm);
                        /* We need a copy of tmp2 because gen_neon_mull
                         * deletes it during pass 0.  */
                        tmp4 = tcg_temp_new_i32();
                        tcg_gen_mov_i32(tmp4, tmp2);
                        tmp3 = neon_load_reg(rn, 1);

                        for(pass = 0; pass < 2; pass++) {
                            if(pass == 0) {
                                tmp = neon_load_reg(rn, 0);
                            } else {
                                tmp = tmp3;
                                tmp2 = tmp4;
                            }
                            gen_neon_mull(cpu_V0, tmp, tmp2, size, u);
                            if(op != 11) {
                                neon_load_reg64(cpu_V1, rd + pass);
                            }
                            switch(op) {
                                case 6:
                                    gen_neon_negl(cpu_V0, size);
                                /* Fall through */
                                case 2:
                                    gen_neon_addl(size);
                                    break;
                                case 3:
                                case 7:
                                    gen_neon_addl_saturate(cpu_V0, cpu_V0, size);
                                    if(op == 7) {
                                        gen_neon_negl(cpu_V0, size);
                                    }
                                    gen_neon_addl_saturate(cpu_V0, cpu_V1, size);
                                    break;
                                case 10:
                                    /* no-op */
                                    break;
                                case 11:
                                    gen_neon_addl_saturate(cpu_V0, cpu_V0, size);
                                    break;
                                default:
                                    abort();
                            }
                            neon_store_reg64(cpu_V0, rd + pass);
                        }

                        break;
                    default: /* 14 and 15 are RESERVED */
                        return 1;
                }
            }
        } else { /* size == 3 */
            if(!u) {
                /* Extract.  */
                imm = (insn >> 8) & 0xf;

                if(imm > 7 && !q) {
                    return 1;
                }

                if(q && ((rd | rn | rm) & 1)) {
                    return 1;
                }

                if(imm == 0) {
                    neon_load_reg64(cpu_V0, rn);
                    if(q) {
                        neon_load_reg64(cpu_V1, rn + 1);
                    }
                } else if(imm == 8) {
                    neon_load_reg64(cpu_V0, rn + 1);
                    if(q) {
                        neon_load_reg64(cpu_V1, rm);
                    }
                } else if(q) {
                    tmp64 = tcg_temp_new_i64();
                    if(imm < 8) {
                        neon_load_reg64(cpu_V0, rn);
                        neon_load_reg64(tmp64, rn + 1);
                    } else {
                        neon_load_reg64(cpu_V0, rn + 1);
                        neon_load_reg64(tmp64, rm);
                    }
                    tcg_gen_shri_i64(cpu_V0, cpu_V0, (imm & 7) * 8);
                    tcg_gen_shli_i64(cpu_V1, tmp64, 64 - ((imm & 7) * 8));
                    tcg_gen_or_i64(cpu_V0, cpu_V0, cpu_V1);
                    if(imm < 8) {
                        neon_load_reg64(cpu_V1, rm);
                    } else {
                        neon_load_reg64(cpu_V1, rm + 1);
                        imm -= 8;
                    }
                    tcg_gen_shli_i64(cpu_V1, cpu_V1, 64 - (imm * 8));
                    tcg_gen_shri_i64(tmp64, tmp64, imm * 8);
                    tcg_gen_or_i64(cpu_V1, cpu_V1, tmp64);
                    tcg_temp_free_i64(tmp64);
                } else {
                    /* BUGFIX */
                    neon_load_reg64(cpu_V0, rn);
                    tcg_gen_shri_i64(cpu_V0, cpu_V0, imm * 8);
                    neon_load_reg64(cpu_V1, rm);
                    tcg_gen_shli_i64(cpu_V1, cpu_V1, 64 - (imm * 8));
                    tcg_gen_or_i64(cpu_V0, cpu_V0, cpu_V1);
                }
                neon_store_reg64(cpu_V0, rd);
                if(q) {
                    neon_store_reg64(cpu_V1, rd + 1);
                }
            } else if((insn & (1 << 11)) == 0) {
                /* Two register misc.  */
                op = ((insn >> 12) & 0x30) | ((insn >> 7) & 0xf);
                size = (insn >> 18) & 3;
                /* UNDEF for unknown op values and bad op-size combinations */
                if((neon_2rm_sizes[op] & (1 << size)) == 0) {
                    return 1;
                }
                if((op != NEON_2RM_VMOVN && op != NEON_2RM_VQMOVN) && q && ((rm | rd) & 1)) {
                    return 1;
                }
                switch(op) {
                    case NEON_2RM_VREV64:
                        for(pass = 0; pass < (q ? 2 : 1); pass++) {
                            tmp = neon_load_reg(rm, pass * 2);
                            tmp2 = neon_load_reg(rm, pass * 2 + 1);
                            switch(size) {
                                case 0:
                                    tcg_gen_bswap32_i32(tmp, tmp);
                                    break;
                                case 1:
                                    gen_swap_half(tmp);
                                    break;
                                case 2: /* no-op */
                                    break;
                                default:
                                    abort();
                            }
                            neon_store_reg(rd, pass * 2 + 1, tmp);
                            if(size == 2) {
                                neon_store_reg(rd, pass * 2, tmp2);
                            } else {
                                switch(size) {
                                    case 0:
                                        tcg_gen_bswap32_i32(tmp2, tmp2);
                                        break;
                                    case 1:
                                        gen_swap_half(tmp2);
                                        break;
                                    default:
                                        abort();
                                }
                                neon_store_reg(rd, pass * 2, tmp2);
                            }
                        }
                        break;
                    case NEON_2RM_VPADDL:
                    case NEON_2RM_VPADDL_U:
                    case NEON_2RM_VPADAL:
                    case NEON_2RM_VPADAL_U:
                        for(pass = 0; pass < q + 1; pass++) {
                            tmp = neon_load_reg(rm, pass * 2);
                            gen_neon_widen(cpu_V0, tmp, size, op & 1);
                            tmp = neon_load_reg(rm, pass * 2 + 1);
                            gen_neon_widen(cpu_V1, tmp, size, op & 1);
                            switch(size) {
                                case 0:
                                    gen_helper_neon_paddl_u16(CPU_V001);
                                    break;
                                case 1:
                                    gen_helper_neon_paddl_u32(CPU_V001);
                                    break;
                                case 2:
                                    tcg_gen_add_i64(CPU_V001);
                                    break;
                                default:
                                    abort();
                            }
                            if(op >= NEON_2RM_VPADAL) {
                                /* Accumulate.  */
                                neon_load_reg64(cpu_V1, rd + pass);
                                gen_neon_addl(size);
                            }
                            neon_store_reg64(cpu_V0, rd + pass);
                        }
                        break;
                    case NEON_2RM_VTRN:
                        if(size == 2) {
                            int n;
                            for(n = 0; n < (q ? 4 : 2); n += 2) {
                                tmp = neon_load_reg(rm, n);
                                tmp2 = neon_load_reg(rd, n + 1);
                                neon_store_reg(rm, n, tmp2);
                                neon_store_reg(rd, n + 1, tmp);
                            }
                        } else {
                            goto elementwise;
                        }
                        break;
                    case NEON_2RM_VUZP:
                        if(gen_neon_unzip(rd, rm, size, q)) {
                            return 1;
                        }
                        break;
                    case NEON_2RM_VZIP:
                        if(gen_neon_zip(rd, rm, size, q)) {
                            return 1;
                        }
                        break;
                    case NEON_2RM_VMOVN:
                    case NEON_2RM_VQMOVN:
                        /* also VQMOVUN; op field and mnemonics don't line up */
                        if(rm & 1) {
                            return 1;
                        }
                        TCGV_UNUSED(tmp2);
                        for(pass = 0; pass < 2; pass++) {
                            neon_load_reg64(cpu_V0, rm + pass);
                            tmp = tcg_temp_new_i32();
                            gen_neon_narrow_op(op == NEON_2RM_VMOVN, q, size, tmp, cpu_V0);
                            if(pass == 0) {
                                tmp2 = tmp;
                            } else {
                                neon_store_reg(rd, 0, tmp2);
                                neon_store_reg(rd, 1, tmp);
                            }
                        }
                        break;
                    case NEON_2RM_VSHLL:
                        if(q || (rd & 1)) {
                            return 1;
                        }
                        tmp = neon_load_reg(rm, 0);
                        tmp2 = neon_load_reg(rm, 1);
                        for(pass = 0; pass < 2; pass++) {
                            if(pass == 1) {
                                tmp = tmp2;
                            }
                            gen_neon_widen(cpu_V0, tmp, size, 1);
                            tcg_gen_shli_i64(cpu_V0, cpu_V0, 8 << size);
                            neon_store_reg64(cpu_V0, rd + pass);
                        }
                        break;
                    case NEON_2RM_VCVT_F16_F32:
                        if(!arm_feature(env, ARM_FEATURE_VFP_FP16) || q || (rm & 1)) {
                            return 1;
                        }
                        tmp = tcg_temp_new_i32();
                        tmp2 = tcg_temp_new_i32();
                        tcg_gen_ld_f32(cpu_F0s, cpu_env, neon_reg_offset(rm, 0));
                        gen_helper_neon_fcvt_f32_to_f16(tmp, cpu_F0s, cpu_env);
                        tcg_gen_ld_f32(cpu_F0s, cpu_env, neon_reg_offset(rm, 1));
                        gen_helper_neon_fcvt_f32_to_f16(tmp2, cpu_F0s, cpu_env);
                        tcg_gen_shli_i32(tmp2, tmp2, 16);
                        tcg_gen_or_i32(tmp2, tmp2, tmp);
                        tcg_gen_ld_f32(cpu_F0s, cpu_env, neon_reg_offset(rm, 2));
                        gen_helper_neon_fcvt_f32_to_f16(tmp, cpu_F0s, cpu_env);
                        tcg_gen_ld_f32(cpu_F0s, cpu_env, neon_reg_offset(rm, 3));
                        neon_store_reg(rd, 0, tmp2);
                        tmp2 = tcg_temp_new_i32();
                        gen_helper_neon_fcvt_f32_to_f16(tmp2, cpu_F0s, cpu_env);
                        tcg_gen_shli_i32(tmp2, tmp2, 16);
                        tcg_gen_or_i32(tmp2, tmp2, tmp);
                        neon_store_reg(rd, 1, tmp2);
                        tcg_temp_free_i32(tmp);
                        break;
                    case NEON_2RM_VCVT_F32_F16:
                        if(!arm_feature(env, ARM_FEATURE_VFP_FP16) || q || (rd & 1)) {
                            return 1;
                        }
                        tmp3 = tcg_temp_new_i32();
                        tmp = neon_load_reg(rm, 0);
                        tmp2 = neon_load_reg(rm, 1);
                        tcg_gen_ext16u_i32(tmp3, tmp);
                        gen_helper_neon_fcvt_f16_to_f32(cpu_F0s, tmp3, cpu_env);
                        tcg_gen_st_f32(cpu_F0s, cpu_env, neon_reg_offset(rd, 0));
                        tcg_gen_shri_i32(tmp3, tmp, 16);
                        gen_helper_neon_fcvt_f16_to_f32(cpu_F0s, tmp3, cpu_env);
                        tcg_gen_st_f32(cpu_F0s, cpu_env, neon_reg_offset(rd, 1));
                        tcg_temp_free_i32(tmp);
                        tcg_gen_ext16u_i32(tmp3, tmp2);
                        gen_helper_neon_fcvt_f16_to_f32(cpu_F0s, tmp3, cpu_env);
                        tcg_gen_st_f32(cpu_F0s, cpu_env, neon_reg_offset(rd, 2));
                        tcg_gen_shri_i32(tmp3, tmp2, 16);
                        gen_helper_neon_fcvt_f16_to_f32(cpu_F0s, tmp3, cpu_env);
                        tcg_gen_st_f32(cpu_F0s, cpu_env, neon_reg_offset(rd, 3));
                        tcg_temp_free_i32(tmp2);
                        tcg_temp_free_i32(tmp3);
                        break;
                    default:
                    elementwise:
                        for(pass = 0; pass < (q ? 4 : 2); pass++) {
                            if(neon_2rm_is_float_op(op)) {
                                tcg_gen_ld_f32(cpu_F0s, cpu_env, neon_reg_offset(rm, pass));
                                TCGV_UNUSED(tmp);
                            } else {
                                tmp = neon_load_reg(rm, pass);
                            }
                            switch(op) {
                                case NEON_2RM_VREV32:
                                    switch(size) {
                                        case 0:
                                            tcg_gen_bswap32_i32(tmp, tmp);
                                            break;
                                        case 1:
                                            gen_swap_half(tmp);
                                            break;
                                        default:
                                            abort();
                                    }
                                    break;
                                case NEON_2RM_VREV16:
                                    gen_rev16(tmp);
                                    break;
                                case NEON_2RM_VCLS:
                                    switch(size) {
                                        case 0:
                                            gen_helper_neon_cls_s8(tmp, tmp);
                                            break;
                                        case 1:
                                            gen_helper_neon_cls_s16(tmp, tmp);
                                            break;
                                        case 2:
                                            gen_helper_neon_cls_s32(tmp, tmp);
                                            break;
                                        default:
                                            abort();
                                    }
                                    break;
                                case NEON_2RM_VCLZ:
                                    switch(size) {
                                        case 0:
                                            gen_helper_neon_clz_u8(tmp, tmp);
                                            break;
                                        case 1:
                                            gen_helper_neon_clz_u16(tmp, tmp);
                                            break;
                                        case 2:
                                            gen_helper_clz(tmp, tmp);
                                            break;
                                        default:
                                            abort();
                                    }
                                    break;
                                case NEON_2RM_VCNT:
                                    gen_helper_neon_cnt_u8(tmp, tmp);
                                    break;
                                case NEON_2RM_VMVN:
                                    tcg_gen_not_i32(tmp, tmp);
                                    break;
                                case NEON_2RM_VQABS:
                                    switch(size) {
                                        case 0:
                                            gen_helper_neon_qabs_s8(tmp, cpu_env, tmp);
                                            break;
                                        case 1:
                                            gen_helper_neon_qabs_s16(tmp, cpu_env, tmp);
                                            break;
                                        case 2:
                                            gen_helper_neon_qabs_s32(tmp, cpu_env, tmp);
                                            break;
                                        default:
                                            abort();
                                    }
                                    break;
                                case NEON_2RM_VQNEG:
                                    switch(size) {
                                        case 0:
                                            gen_helper_neon_qneg_s8(tmp, cpu_env, tmp);
                                            break;
                                        case 1:
                                            gen_helper_neon_qneg_s16(tmp, cpu_env, tmp);
                                            break;
                                        case 2:
                                            gen_helper_neon_qneg_s32(tmp, cpu_env, tmp);
                                            break;
                                        default:
                                            abort();
                                    }
                                    break;
                                case NEON_2RM_VCGT0:
                                case NEON_2RM_VCLE0:
                                    tmp2 = tcg_const_i32(0);
                                    switch(size) {
                                        case 0:
                                            gen_helper_neon_cgt_s8(tmp, tmp, tmp2);
                                            break;
                                        case 1:
                                            gen_helper_neon_cgt_s16(tmp, tmp, tmp2);
                                            break;
                                        case 2:
                                            gen_helper_neon_cgt_s32(tmp, tmp, tmp2);
                                            break;
                                        default:
                                            abort();
                                    }
                                    tcg_temp_free(tmp2);
                                    if(op == NEON_2RM_VCLE0) {
                                        tcg_gen_not_i32(tmp, tmp);
                                    }
                                    break;
                                case NEON_2RM_VCGE0:
                                case NEON_2RM_VCLT0:
                                    tmp2 = tcg_const_i32(0);
                                    switch(size) {
                                        case 0:
                                            gen_helper_neon_cge_s8(tmp, tmp, tmp2);
                                            break;
                                        case 1:
                                            gen_helper_neon_cge_s16(tmp, tmp, tmp2);
                                            break;
                                        case 2:
                                            gen_helper_neon_cge_s32(tmp, tmp, tmp2);
                                            break;
                                        default:
                                            abort();
                                    }
                                    tcg_temp_free(tmp2);
                                    if(op == NEON_2RM_VCLT0) {
                                        tcg_gen_not_i32(tmp, tmp);
                                    }
                                    break;
                                case NEON_2RM_VCEQ0:
                                    tmp2 = tcg_const_i32(0);
                                    switch(size) {
                                        case 0:
                                            gen_helper_neon_ceq_u8(tmp, tmp, tmp2);
                                            break;
                                        case 1:
                                            gen_helper_neon_ceq_u16(tmp, tmp, tmp2);
                                            break;
                                        case 2:
                                            gen_helper_neon_ceq_u32(tmp, tmp, tmp2);
                                            break;
                                        default:
                                            abort();
                                    }
                                    tcg_temp_free(tmp2);
                                    break;
                                case NEON_2RM_VABS:
                                    switch(size) {
                                        case 0:
                                            gen_helper_neon_abs_s8(tmp, tmp);
                                            break;
                                        case 1:
                                            gen_helper_neon_abs_s16(tmp, tmp);
                                            break;
                                        case 2:
                                            tcg_gen_abs_i32(tmp, tmp);
                                            break;
                                        default:
                                            abort();
                                    }
                                    break;
                                case NEON_2RM_VNEG:
                                    tmp2 = tcg_const_i32(0);
                                    gen_neon_rsb(size, tmp, tmp2);
                                    tcg_temp_free(tmp2);
                                    break;
                                case NEON_2RM_VCGT0_F: {
                                    TCGv_ptr fpstatus = get_fpstatus_ptr(1);
                                    tmp2 = tcg_const_i32(0);
                                    gen_helper_neon_cgt_f32(tmp, tmp, tmp2, fpstatus);
                                    tcg_temp_free(tmp2);
                                    tcg_temp_free_ptr(fpstatus);
                                    break;
                                }
                                case NEON_2RM_VCGE0_F: {
                                    TCGv_ptr fpstatus = get_fpstatus_ptr(1);
                                    tmp2 = tcg_const_i32(0);
                                    gen_helper_neon_cge_f32(tmp, tmp, tmp2, fpstatus);
                                    tcg_temp_free(tmp2);
                                    tcg_temp_free_ptr(fpstatus);
                                    break;
                                }
                                case NEON_2RM_VCEQ0_F: {
                                    TCGv_ptr fpstatus = get_fpstatus_ptr(1);
                                    tmp2 = tcg_const_i32(0);
                                    gen_helper_neon_ceq_f32(tmp, tmp, tmp2, fpstatus);
                                    tcg_temp_free(tmp2);
                                    tcg_temp_free_ptr(fpstatus);
                                    break;
                                }
                                case NEON_2RM_VCLE0_F: {
                                    TCGv_ptr fpstatus = get_fpstatus_ptr(1);
                                    tmp2 = tcg_const_i32(0);
                                    gen_helper_neon_cge_f32(tmp, tmp2, tmp, fpstatus);
                                    tcg_temp_free(tmp2);
                                    tcg_temp_free_ptr(fpstatus);
                                    break;
                                }
                                case NEON_2RM_VCLT0_F: {
                                    TCGv_ptr fpstatus = get_fpstatus_ptr(1);
                                    tmp2 = tcg_const_i32(0);
                                    gen_helper_neon_cgt_f32(tmp, tmp2, tmp, fpstatus);
                                    tcg_temp_free(tmp2);
                                    tcg_temp_free_ptr(fpstatus);
                                    break;
                                }
                                case NEON_2RM_VABS_F:
                                    gen_vfp_abs(0);
                                    break;
                                case NEON_2RM_VNEG_F:
                                    gen_vfp_neg(0);
                                    break;
                                case NEON_2RM_VSWP:
                                    tmp2 = neon_load_reg(rd, pass);
                                    neon_store_reg(rm, pass, tmp2);
                                    break;
                                case NEON_2RM_VTRN:
                                    tmp2 = neon_load_reg(rd, pass);
                                    switch(size) {
                                        case 0:
                                            gen_neon_trn_u8(tmp, tmp2);
                                            break;
                                        case 1:
                                            gen_neon_trn_u16(tmp, tmp2);
                                            break;
                                        default:
                                            abort();
                                    }
                                    neon_store_reg(rm, pass, tmp2);
                                    break;
                                case NEON_2RM_VRECPE:
                                    gen_helper_recpe_u32(tmp, tmp, cpu_env);
                                    break;
                                case NEON_2RM_VRSQRTE:
                                    gen_helper_rsqrte_u32(tmp, tmp, cpu_env);
                                    break;
                                case NEON_2RM_VRECPE_F:
                                    gen_helper_recpe_f32(cpu_F0s, cpu_F0s, cpu_env);
                                    break;
                                case NEON_2RM_VRSQRTE_F:
                                    gen_helper_rsqrte_f32(cpu_F0s, cpu_F0s, cpu_env);
                                    break;
                                case NEON_2RM_VCVT_FS: /* VCVT.F32.S32 */
                                    gen_vfp_sito(0, 1);
                                    break;
                                case NEON_2RM_VCVT_FU: /* VCVT.F32.U32 */
                                    gen_vfp_uito(0, 1);
                                    break;
                                case NEON_2RM_VCVT_SF: /* VCVT.S32.F32 */
                                    gen_vfp_tosiz(0, 1);
                                    break;
                                case NEON_2RM_VCVT_UF: /* VCVT.U32.F32 */
                                    gen_vfp_touiz(0, 1);
                                    break;
                                default:
                                    /* Reserved op values were caught by the
                                     * neon_2rm_sizes[] check earlier.
                                     */
                                    abort();
                            }
                            if(neon_2rm_is_float_op(op)) {
                                tcg_gen_st_f32(cpu_F0s, cpu_env, neon_reg_offset(rd, pass));
                            } else {
                                neon_store_reg(rd, pass, tmp);
                            }
                        }
                        break;
                }
            } else if((insn & (1 << 10)) == 0) {
                /* VTBL, VTBX.  */
                int n = ((insn >> 8) & 3) + 1;
                if((rn + n) > 32) {
                    /* This is UNPREDICTABLE; we choose to UNDEF to avoid the
                     * helper function running off the end of the register file.
                     */
                    return 1;
                }
                n <<= 3;
                if(insn & (1 << 6)) {
                    tmp = neon_load_reg(rd, 0);
                } else {
                    tmp = tcg_temp_new_i32();
                    tcg_gen_movi_i32(tmp, 0);
                }
                tmp2 = neon_load_reg(rm, 0);
                tmp4 = tcg_const_i32(rn);
                tmp5 = tcg_const_i32(n);
                gen_helper_neon_tbl(tmp2, tmp2, tmp, tmp4, tmp5);
                tcg_temp_free_i32(tmp);
                if(insn & (1 << 6)) {
                    tmp = neon_load_reg(rd, 1);
                } else {
                    tmp = tcg_temp_new_i32();
                    tcg_gen_movi_i32(tmp, 0);
                }
                tmp3 = neon_load_reg(rm, 1);
                gen_helper_neon_tbl(tmp3, tmp3, tmp, tmp4, tmp5);
                tcg_temp_free_i32(tmp5);
                tcg_temp_free_i32(tmp4);
                neon_store_reg(rd, 0, tmp2);
                neon_store_reg(rd, 1, tmp3);
                tcg_temp_free_i32(tmp);
            } else if((insn & 0x380) == 0) {
                /* VDUP */
                if((insn & (7 << 16)) == 0 || (q && (rd & 1))) {
                    return 1;
                }
                if(insn & (1 << 19)) {
                    tmp = neon_load_reg(rm, 1);
                } else {
                    tmp = neon_load_reg(rm, 0);
                }
                if(insn & (1 << 16)) {
                    gen_neon_dup_u8(tmp, ((insn >> 17) & 3) * 8);
                } else if(insn & (1 << 17)) {
                    if((insn >> 18) & 1) {
                        gen_neon_dup_high16(tmp);
                    } else {
                        gen_neon_dup_low16(tmp);
                    }
                }
                for(pass = 0; pass < (q ? 4 : 2); pass++) {
                    tmp2 = tcg_temp_new_i32();
                    tcg_gen_mov_i32(tmp2, tmp);
                    neon_store_reg(rd, pass, tmp2);
                }
                tcg_temp_free_i32(tmp);
            } else {
                return 1;
            }
        }
    }
    return 0;
}

//  Quirks in CP15 implementation ported from old code, that would be difficult to implement in ttable
static inline void do_coproc_insn_quirks(CPUState *env, DisasContext *s, uint32_t insn, int cpnum, int is64, int *opc1, int *crn,
                                         int *crm, int *opc2, bool isread, int *rt, int *rt2)
{
    /*
     * Ideally, we would handle these cases with ANY
     * We would need a way to override previously defined registers for this to work
     * This might be TODO
     */
    if(arm_feature(env, ARM_FEATURE_OMAPCP)) {
        if(*crn == 0 && isread == false) {
            //  This is a hack to route all writes to artificial NOP register
            *opc2 = 10;
            *opc1 = 10;
            *crm = 10;
        } else if(*crn == 5 || *crn == 1) {
            *opc2 = 0;
        } else if(*crn == 6 && !arm_feature(env, ARM_FEATURE_MPU) && !arm_feature(env, ARM_FEATURE_PMSA)) {
            *opc2 = 0;
        }
    }
    if(*crn == 9 && (arm_feature(env, ARM_FEATURE_OMAPCP) || arm_feature(env, ARM_FEATURE_STRONGARM))) {
        *opc2 = 10;
        *opc1 = 10;
        *crm = 10;
    }
}

//  This code has been taken from a fuction of the same name in `arm64` and modified to suit this library
static int do_coproc_insn(CPUState *env, DisasContext *s, uint32_t insn, int cpnum, int is64, int opc1, int crn, int crm,
                          int opc2, bool isread, int rt, int rt2)
{
    /* M profile cores use memory mapped registers instead of cp15.  */
#ifdef TARGET_PROTO_ARM_M
    if(cpnum == 15) {
        return 1;
    }
#endif

    if(cpnum == 15) {
        if((insn & (COPROCESSOR_INSTR_OP1_PARTIAL_MASK(0x30) | COPROCESSOR_INSTR_OP_MASK)) ==
           (0x20 << COPROCESSOR_INSTR_OP1_OFFSET)) {
            /* cdp */
            return 1;
        }

        //  TODO: these cases should be probably reimplemented with accessfns
        if(s->user && !cp15_special_user_ok(env, s->user, is64, opc1, crn, crm, opc2, isread)) {
            return 1;
        }
        do_coproc_insn_quirks(env, s, insn, cpnum, is64, &opc1, &crn, &crm, &opc2, isread, &rt, &rt2);
    }

    //  XXX: We don't support banked cp15 registers with Security Extension, so set `ns` to true
    uint32_t key = ENCODE_CP_REG(cpnum, is64, true, crn, crm, opc1, opc2);
    const ARMCPRegInfo *ri = ttable_lookup_value_eq(s->cp_regs, &key);

    if(ri) {
        bool need_exit_tb = false;

        /* Check access permissions */
        if(!cp_access_ok(s->user ? 0 : 1, ri, isread)) {
            return 1;
        }

        //  We don't have trapping or hypervisor implemented so let's abort if we try to use this in the future
        if(ri->accessfn || (arm_feature(env, ARM_FEATURE_XSCALE) && cpnum < 14)) {

            tlib_abort("Trapping CP instruction is unimplemented");
            /* Look into our aarch64 impl for how it should be done */

        } else if(ri->type & ARM_CP_RAISES_EXC) {
            /*
             * The readfn or writefn might raise an exception;
             * synchronize the CPU state in case it does.
             */
            gen_set_condexec(s);
            //  We have synced the PC before
        }

        /* Handle special cases first */
        switch(ri->type & ARM_CP_SPECIAL_MASK) {
            case 0:
                break;
            case ARM_CP_NOP:
                return 0;
            case ARM_CP_WFI:
                if(isread) {
                    break;
                }
                if(!tlib_is_wfi_as_nop()) {
                    /* Wait for interrupt */
                    gen_set_pc_im(s->base.pc);
                    s->base.is_jmp = DISAS_WFI;
                }
                return 0;
            case ARM_CP_BARRIER:
                //  Reading such a register shouldn't be possible, they should all be marked as WO.
                assert(!isread);

                //  The instructions have common cp15, op0 and crn parts.
                assert(ri->cp == 15 && ri->op0 == 0 && ri->crn == 7);

                //  crm and op2 are concatenated to analyze them in a single switch-case. crm is multiplied by 100 to simplify
                //  converting decimal crm and op2 to the case values; op2 is a 4-bit part so it never exceeds 15.
                switch(ri->crm * 100 + ri->op2) {
                    case 100:  //  crm=1, op2=0: ICIALLUIS, it's treated like ISB cause it often accompanies self-modifying code.
                    case 504:  //  crm=5, op2=4: CP15ISB
                        gen_isb(s);
                        return 0;
                    case 1004:  //  crm=10, op2=4: CP15DSB (ARMv7) / CP15DWB (preARMv7)
                    case 1005:  //  crm=10, op2=5: CP15DMB
                        gen_dxb(s);
                        return 0;
                    default:
                        tlib_assert_not_reached();
                }
            default:
                tlib_assert_not_reached();
        }

        /* Right now we don't need to make any preparations for ARM_CP_IO,
         * except possibly TODO: taking an exclusive lock in system_registers:set/get_cp_reg
         * But we will end the TB later in the code
         */

        if(isread) {
            /* Read */
            if(is64) {
                TCGv_i64 tmp64;
                TCGv_i32 tmp;
                if(ri->type & ARM_CP_CONST) {
                    tmp64 = tcg_const_i64(ri->resetvalue);
                } else if(ri->readfn) {
                    tmp64 = tcg_temp_new_i64();
                    TCGv_ptr ptr = tcg_const_ptr((tcg_target_long)ri);
                    gen_helper_get_cp_reg64(tmp64, cpu_env, ptr);
                    tcg_temp_free_ptr(ptr);
                } else if(ri->fieldoffset != 0) {
                    tmp64 = tcg_temp_new_i64();
                    tcg_gen_ld_i64(tmp64, cpu_env, ri->fieldoffset);
                } else {
                    tmp64 = tcg_const_i64(0);
                    log_unhandled_sysreg_read(ri->name);
                }
                tmp = tcg_temp_new_i32();
                tcg_gen_extrl_i64_i32(tmp, tmp64);
                store_reg(s, rt, tmp);
                tmp = tcg_temp_new_i32();
                tcg_gen_extrh_i64_i32(tmp, tmp64);
                tcg_temp_free_i64(tmp64);
                store_reg(s, rt2, tmp);
            } else {
                TCGv_i32 tmp;
                if(ri->type & ARM_CP_CONST) {
                    tmp = tcg_const_i32(ri->resetvalue);
                } else if(ri->readfn) {
                    tmp = tcg_temp_new_i32();
                    TCGv_ptr ptr = tcg_const_ptr((tcg_target_long)ri);
                    gen_helper_get_cp_reg(tmp, cpu_env, ptr);
                    tcg_temp_free_ptr(ptr);
                } else if(ri->fieldoffset != 0) {
                    tmp = load_cpu_offset(ri->fieldoffset);
                } else {
                    tmp = tcg_const_i32(0);
                    log_unhandled_sysreg_read(ri->name);
                }

                if(rt == 15) {
                    /* Destination register of r15 for 32 bit loads sets
                     * the condition codes from the high 4 bits of the value
                     */
                    gen_set_nzcv(tmp);
                    tcg_temp_free_i32(tmp);
                } else {
                    store_reg(s, rt, tmp);
                }
            }
        } else {
            /* Write */
            if(ri->type & ARM_CP_CONST) {
                /* If not forbidden by access permissions, treat as WI */
                return 0;
            }

            /* 64-bit wide write from two registers */
            if(is64) {
                TCGv_i32 tmplo, tmphi;
                TCGv_i64 tmp64 = tcg_temp_new_i64();
                tmplo = load_reg(s, rt);
                tmphi = load_reg(s, rt2);
                tcg_gen_concat_i32_i64(tmp64, tmplo, tmphi);
                tcg_temp_free_i32(tmplo);
                tcg_temp_free_i32(tmphi);
                if(ri->writefn) {
                    TCGv_ptr ptr = tcg_const_ptr((tcg_target_long)ri);
                    gen_helper_set_cp_reg64(cpu_env, ptr, tmp64);
                    tcg_temp_free_ptr(ptr);
                } else if(ri->fieldoffset != 0) {
                    tcg_gen_st_i64(tmp64, cpu_env, ri->fieldoffset);
                } else {
                    log_unhandled_sysreg_write(ri->name);
                    tcg_temp_free_i64(tmp64);
                    return 0;
                }
                tcg_temp_free_i64(tmp64);
            } else {
                TCGv_i32 tmp = load_reg(s, rt);
                if(ri->writefn) {
                    TCGv_ptr ptr = tcg_const_ptr((tcg_target_long)ri);
                    gen_helper_set_cp_reg(cpu_env, ptr, tmp);
                    tcg_temp_free_ptr(ptr);
                    tcg_temp_free_i32(tmp);
                } else if(ri->fieldoffset != 0) {
                    store_cpu_offset(tmp, ri->fieldoffset);
                } else {
                    log_unhandled_sysreg_write(ri->name);
                    tcg_temp_free_i32(tmp);
                    return 0;
                }
            }
        }

        /* I/O operations must end the TB here (whether read or write) */
        need_exit_tb = (ri->type & ARM_CP_IO) || (ri->type & ARM_CP_FORCE_TB_END);

        if(!isread && !(ri->type & ARM_CP_SUPPRESS_TB_END)) {
            /*
             * A write to any coprocessor register that ends a TB
             * must rebuild the hflags for the next TB.
             */

            //  We should rebuild hflags here, if we had any in this impl.
            //  gen_rebuild_hflags(s, ri->type & ARM_CP_NEWEL);

            /*
             * We default to ending the TB on a coprocessor register write,
             * but allow this to be suppressed by the register definition
             * (usually only necessary to work around guest bugs).
             */
            need_exit_tb = true;
        }
        if(need_exit_tb) {
            gen_lookup_tb(s);
        }

        return 0;
    }

    /* Unknown register; this might be a guest error or a QEMU
     * unimplemented feature.
     * We can route the request to C# layer (tlib_read/write_cp15_*) but only for CP15
     * otherwise warn the user.
     */
    if(is64) {
        if(cpnum == 15) {
            TCGv_i32 insn_tcg = tcg_const_i32(insn);
            if(isread) {
                TCGv_i64 tmp64 = tcg_temp_new_i64();
                gen_helper_get_cp15_64bit(tmp64, cpu_env, insn_tcg);

                TCGv_i32 mover = tcg_temp_new_i32();
                tcg_gen_extrl_i64_i32(mover, tmp64);
                store_reg(s, rt, mover);

                mover = tcg_temp_new_i32();
                tcg_gen_extrh_i64_i32(mover, tmp64);
                store_reg(s, rt2, mover);

                tcg_temp_free_i64(tmp64);
            } else {
                TCGv_i32 tmplo, tmphi;
                TCGv_i64 tmp64 = tcg_temp_new_i64();
                tmplo = load_reg(s, rt);
                tmphi = load_reg(s, rt2);

                gen_helper_set_cp15_64bit(cpu_env, insn_tcg, tmplo, tmphi);

                tcg_temp_free_i32(tmplo);
                tcg_temp_free_i32(tmphi);
                tcg_temp_free_i64(tmp64);
            }

            tcg_temp_free_i32(insn_tcg);

            /* We always end the TB here, on read or write
             * as access to the external emulation layer
             * can result in unexpected state changes
             */
            gen_lookup_tb(s);
            return 0;
        } else {
            tlib_printf(LOG_LEVEL_ERROR,
                        "%s access to unsupported AArch32 "
                        "64 bit system register cp:%d opc1:%d crm:%d "
                        "(%s)",
                        isread ? "read" : "write", cpnum, opc1, crm, s->user ? "user" : "privilege");
        }
    } else {
        if(cpnum == 15) {
            TCGv_i32 insn_tcg = tcg_const_i32(insn);
            if(isread) {
                TCGv_i32 tmp = tcg_temp_new_i32();
                gen_helper_get_cp15_32bit(tmp, cpu_env, insn_tcg);

                if(rt == 15) {
                    /* Destination register of r15 for 32 bit loads sets
                     * the condition codes from the high 4 bits of the value
                     */
                    gen_set_nzcv(tmp);
                    tcg_temp_free_i32(tmp);
                } else {
                    store_reg(s, rt, tmp);
                }
            } else {
                TCGv_i32 val = load_reg(s, rt);
                gen_helper_set_cp15_32bit(cpu_env, insn_tcg, val);
                tcg_temp_free_i32(val);
            }

            tcg_temp_free_i32(insn_tcg);

            //  Same as in is64 case
            gen_lookup_tb(s);
            return 0;
        } else {
            tlib_printf(LOG_LEVEL_ERROR,
                        "%s access to unsupported AArch32 "
                        "32 bit system register cp:%d opc1:%d crn:%d crm:%d opc2:%d "
                        "(%s)",
                        isread ? "read" : "write", cpnum, opc1, crn, crm, opc2, s->user ? "user" : "privilege");
        }
    }

    return 1;
}

static int disas_coproc_insn(CPUState *env, DisasContext *s, uint32_t insn)
{
    int cpnum = extract32(insn, 8, 4);
    int crn;  //  This is declared here to work around known limitations in older gcc

    if(arm_feature(env, ARM_FEATURE_XSCALE) && ((env->cp15.c15_cpar ^ 0x3fff) & (1 << cpnum))) {
        return 1;
    }

    switch(cpnum) {
        case 0:
        case 1:
            if(arm_feature(env, ARM_FEATURE_IWMMXT)) {
                return disas_iwmmxt_insn(env, s, insn);
            } else if(arm_feature(env, ARM_FEATURE_XSCALE)) {
                return disas_dsp_insn(env, s, insn);
            }
            goto board;
        case 10:
        case 11:
            return disas_vfp_insn(env, s, insn);
        case 14:
        /* This coprocessor should be reserved by ARM and normally it contains debug registers
         * we don't support debug, so we implement only the minimal set.
         * Intel's XSCALE platform might ignore that it's reserved.
         */
        /* fallthrough */
        case 15:
        /* fallthrough */
        default:
            /* Unknown coprocessor.  See if the board has hooked it.  */
        board:
            crn = extract32(insn, 16, 4);
            int crm = extract32(insn, 0, 4);
            bool isread = extract32(insn, 20, 1) == 1;
            int rt = extract32(insn, 12, 4);

            int opc1, opc2;
            /* Whether we transfer one register (MCR/MRC)
                or two (MRRC/MCRR)  */
            int is64 = ((insn & (1 << 25)) == 0);
            if(is64) {
                opc1 = extract32(insn, 4, 4);
                opc2 = 0;
            } else {
                opc1 = extract32(insn, 21, 3);
                opc2 = extract32(insn, 5, 3);
            }
            /*
             * For 64 bit access crn=0 so different combinations of rt2 don't make a difference
             * when decoding the instruction.
             */
            return do_coproc_insn(env, s, insn, cpnum, is64, opc1, is64 ? 0 : crn, crm, opc2, isread, rt, crn);
    }
}

/* Store a 64-bit value to a register pair.  Clobbers val.  */
static void gen_storeq_reg(DisasContext *s, int rlow, int rhigh, TCGv_i64 val)
{
    TCGv tmp;
    tmp = tcg_temp_new_i32();
    tcg_gen_trunc_i64_i32(tmp, val);
    store_reg(s, rlow, tmp);
    tmp = tcg_temp_new_i32();
    tcg_gen_shri_i64(val, val, 32);
    tcg_gen_trunc_i64_i32(tmp, val);
    store_reg(s, rhigh, tmp);
}

/* load a 32-bit value from a register and perform a 64-bit accumulate.  */
static void gen_addq_lo(DisasContext *s, TCGv_i64 val, int rlow)
{
    TCGv_i64 tmp;
    TCGv tmp2;

    /* Load value and extend to 64 bits.  */
    tmp = tcg_temp_new_i64();
    tmp2 = load_reg(s, rlow);
    tcg_gen_extu_i32_i64(tmp, tmp2);
    tcg_temp_free_i32(tmp2);
    tcg_gen_add_i64(val, val, tmp);
    tcg_temp_free_i64(tmp);
}

/* load and add a 64-bit value from a register pair.  */
static void gen_addq(DisasContext *s, TCGv_i64 val, int rlow, int rhigh)
{
    TCGv_i64 tmp;
    TCGv tmpl;
    TCGv tmph;

    /* Load 64-bit value rd:rn.  */
    tmpl = load_reg(s, rlow);
    tmph = load_reg(s, rhigh);
    tmp = tcg_temp_new_i64();
    tcg_gen_concat_i32_i64(tmp, tmpl, tmph);
    tcg_temp_free_i32(tmpl);
    tcg_temp_free_i32(tmph);
    tcg_gen_add_i64(val, val, tmp);
    tcg_temp_free_i64(tmp);
}

/* Set N and Z flags from a 64-bit value.  */
static void gen_logicq_cc(TCGv_i64 val)
{
    TCGv tmp = tcg_temp_new_i32();
    gen_helper_logicq_cc(tmp, val);
    gen_logic_CC(tmp);
    tcg_temp_free_i32(tmp);
}

/* Load/Store exclusive instructions are implemented by remembering
   the value/address loaded, and seeing if these are the same
   when the store is performed. This should be is sufficient to implement
   the architecturally mandated semantics, and avoids having to monitor
   regular stores.

   In system emulation mode only one CPU will be running at once, so
   this sequence is effectively atomic.  In user emulation mode we
   throw an exception and handle the atomic operation elsewhere.  */
static void gen_load_exclusive(DisasContext *s, int rt, int rt2, TCGv addr, int size)
{
    TCGv tmp = 0;

    gen_helper_acquire_global_memory_lock(cpu_env);

    switch(size) {
        case 0:
            tmp = gen_ld8u(addr, context_to_mmu_index(s));
            break;
        case 1:
            tmp = gen_ld16u(addr, context_to_mmu_index(s));
            break;
        case 2:
        case 3:
            tmp = gen_ld32(addr, context_to_mmu_index(s));
            break;
        default:
            abort();
    }
    tcg_gen_mov_i32(cpu_exclusive_val, tmp);
    store_reg(s, rt, tmp);
    if(size == 3) {
        TCGv tmp2 = tcg_temp_new_i32();
        tcg_gen_addi_i32(tmp2, addr, 4);
        tmp = gen_ld32(tmp2, context_to_mmu_index(s));
        tcg_temp_free_i32(tmp2);
        tcg_gen_mov_i32(cpu_exclusive_high, tmp);
        store_reg(s, rt2, tmp);
    }

    gen_helper_reserve_address(cpu_env, addr, tcg_const_i32(1));
    gen_helper_release_global_memory_lock(cpu_env);
}

static void gen_clrex(DisasContext *s)
{
    //  We need to reset the address, for the load/store exclusive instructions to work on single-core systems
    tcg_gen_movi_i32(cpu_exclusive_val, -1);
    tcg_gen_movi_i32(cpu_exclusive_high, -1);

    gen_helper_acquire_global_memory_lock(cpu_env);
    gen_helper_cancel_reservation(cpu_env);
    gen_helper_release_global_memory_lock(cpu_env);
}

static void gen_store_exclusive(DisasContext *s, int rd, int rt, int rt2, TCGv addr, int size)
{
    TCGv tmp = 0;
    int done_label;
    int fail_label;

    /* if (env->exclusive_addr == addr && env->exclusive_val == [addr]) {
         [addr] = {Rt};
         {Rd} = 0;
       } else {
         {Rd} = 1;
       } */
    fail_label = gen_new_label();
    done_label = gen_new_label();

    gen_helper_acquire_global_memory_lock(cpu_env);

    TCGv_i32 has_reservation = tcg_temp_new_i32();
    gen_helper_check_address_reservation(has_reservation, cpu_env, addr);
    tcg_gen_brcondi_i32(TCG_COND_NE, has_reservation, 0, fail_label);
    tcg_temp_free_i32(has_reservation);

    switch(size) {
        case 0:
            tmp = gen_ld8u(addr, context_to_mmu_index(s));
            break;
        case 1:
            tmp = gen_ld16u(addr, context_to_mmu_index(s));
            break;
        case 2:
        case 3:
            tmp = gen_ld32(addr, context_to_mmu_index(s));
            break;
        default:
            abort();
    }
    tcg_gen_brcond_i32(TCG_COND_NE, tmp, cpu_exclusive_val, fail_label);
    tcg_temp_free_i32(tmp);
    if(size == 3) {
        TCGv tmp2 = tcg_temp_new_i32();
        tcg_gen_addi_i32(tmp2, addr, 4);
        tmp = gen_ld32(tmp2, context_to_mmu_index(s));
        tcg_temp_free_i32(tmp2);
        tcg_gen_brcond_i32(TCG_COND_NE, tmp, cpu_exclusive_high, fail_label);
        tcg_temp_free_i32(tmp);
    }
    tmp = load_reg(s, rt);
    switch(size) {
        case 0:
            gen_st8(tmp, addr, context_to_mmu_index(s));
            break;
        case 1:
            gen_st16(tmp, addr, context_to_mmu_index(s));
            break;
        case 2:
        case 3:
            gen_st32(tmp, addr, context_to_mmu_index(s));
            break;
        default:
            abort();
    }
    if(size == 3) {
        tcg_gen_addi_i32(addr, addr, 4);
        tmp = load_reg(s, rt2);
        gen_st32(tmp, addr, context_to_mmu_index(s));
    }
    tcg_gen_movi_i32(cpu_R[rd], 0);
    tcg_gen_br(done_label);
    gen_set_label(fail_label);
    tcg_gen_movi_i32(cpu_R[rd], 1);
    gen_set_label(done_label);

    tcg_gen_movi_i32(cpu_exclusive_val, -1);
    tcg_gen_movi_i32(cpu_exclusive_high, -1);

    gen_helper_cancel_reservation(cpu_env);
    gen_helper_release_global_memory_lock(cpu_env);
}

static void disas_arm_insn(CPUState *env, DisasContext *s)
{
    unsigned int cond, insn, val, op1, i, shift, rm, rs, rn, rd, sh;
    TCGv tmp;
    TCGv tmp2;
    TCGv tmp3;
    TCGv addr;
    TCGv_i64 tmp64;
    target_ulong current_pc = s->base.pc;

    insn = ldl_code(s->base.pc);

    if(env->count_opcodes) {
        generate_opcode_count_increment(env, insn);
    }

    s->base.pc += 4;

    /* M variants do not implement ARM mode.  */
#ifdef TARGET_PROTO_ARM_M
    goto illegal_op;
#endif
    cond = insn >> 28;
    if(cond == 0xf) {
        /* In ARMv3 and v4 the NV condition is UNPREDICTABLE; we
         * choose to UNDEF. In ARMv5 and above the space is used
         * for miscellaneous unconditional instructions.
         */
        ARCH(5);

        /* Unconditional instructions.  */
        if(((insn >> 25) & 7) == 1) {
            /* NEON Data processing.  */
            if(!arm_feature(env, ARM_FEATURE_NEON)) {
                goto illegal_op;
            }

            if(disas_neon_data_insn(env, s, insn)) {
                goto illegal_op;
            }
            return;
        }
        if((insn & 0x0f100000) == 0x04000000) {
            /* NEON load/store.  */
            if(!arm_feature(env, ARM_FEATURE_NEON)) {
                goto illegal_op;
            }
            gen_set_pc(current_pc);

            if(disas_neon_ls_insn(env, s, insn)) {
                goto illegal_op;
            }
            return;
        }
        if(((insn & 0x0f30f000) == 0x0510f000) || ((insn & 0x0f30f010) == 0x0710f000)) {
            if((insn & (1 << 22)) == 0) {
                /* PLDW; v7MP */
                if(!arm_feature(env, ARM_FEATURE_V7MP)) {
                    goto illegal_op;
                }
            }
            /* Otherwise PLD; v5TE+ */
            ARCH(5TE);
            return;
        }
        if(((insn & 0x0f70f000) == 0x0450f000) || ((insn & 0x0f70f010) == 0x0650f000)) {
            ARCH(7);
            return; /* PLI; V7 */
        }
        if(((insn & 0x0f700000) == 0x04100000) || ((insn & 0x0f700010) == 0x06100000)) {
            if(!arm_feature(env, ARM_FEATURE_V7MP)) {
                goto illegal_op;
            }
            return; /* v7MP: Unallocated memory hint: must NOP */
        }

        if((insn & 0x0ffffdff) == 0x01010000) {
            ARCH(6);
            /* setend */
            if(insn & (1 << 9)) {
                /* BE8 mode not implemented.  */
                goto illegal_op;
            }
            return;
        } else if((insn & 0x0fffff00) == 0x057ff000) {
            switch((insn >> 4) & 0xf) {
                case 1: /* clrex */
                    ARCH(6K);
                    gen_clrex(s);
                    return;
                case 4: /* dsb */
                case 5: /* dmb */
                    ARCH(7);
                    gen_dxb(s);
                    return;
                case 6: /* isb */
                    ARCH(7);
                    gen_isb(s);
                    return;
                default:
                    goto illegal_op;
            }
        } else if((insn & 0x0e5fffe0) == 0x084d0500) {
            /* srs */
            int32_t offset = 0;
            if(s->user) {
                goto illegal_op;
            }
            ARCH(6);
            op1 = (insn & 0x1f);
            addr = tcg_temp_new_i32();
            tmp = tcg_const_i32(op1);
            gen_helper_get_r13_banked(addr, cpu_env, tmp);
            tcg_temp_free_i32(tmp);
            i = (insn >> 23) & 3;
            switch(i) {
                case 0:
                    offset = -4;
                    break; /* DA */
                case 1:
                    offset = 0;
                    break; /* IA */
                case 2:
                    offset = -8;
                    break; /* DB */
                case 3:
                    offset = 4;
                    break; /* IB */
                default:
                    abort();
            }
            if(offset) {
                tcg_gen_addi_i32(addr, addr, offset);
            }
            tmp = load_reg(s, 14);
            gen_st32(tmp, addr, context_to_mmu_index(s));
            tmp = load_cpu_field(spsr);
            tcg_gen_addi_i32(addr, addr, 4);
            gen_st32(tmp, addr, context_to_mmu_index(s));
            if(insn & (1 << 21)) {
                /* Base writeback.  */
                switch(i) {
                    case 0:
                        offset = -8;
                        break;
                    case 1:
                        offset = 4;
                        break;
                    case 2:
                        offset = -4;
                        break;
                    case 3:
                        offset = 0;
                        break;
                    default:
                        abort();
                }
                if(offset) {
                    tcg_gen_addi_i32(addr, addr, offset);
                }
                tmp = tcg_const_i32(op1);
                gen_helper_set_r13_banked(cpu_env, tmp, addr);
                tcg_temp_free_i32(tmp);
                tcg_temp_free_i32(addr);
            } else {
                tcg_temp_free_i32(addr);
            }
            return;
        } else if((insn & 0x0e50ffe0) == 0x08100a00) {
            /* rfe */
            int32_t offset = 0;
            if(s->user) {
                goto illegal_op;
            }
            ARCH(6);
            rn = (insn >> 16) & 0xf;
            addr = load_reg(s, rn);
            i = (insn >> 23) & 3;
            switch(i) {
                case 0:
                    offset = -4;
                    break; /* DA */
                case 1:
                    offset = 0;
                    break; /* IA */
                case 2:
                    offset = -8;
                    break; /* DB */
                case 3:
                    offset = 4;
                    break; /* IB */
                default:
                    abort();
            }
            if(offset) {
                tcg_gen_addi_i32(addr, addr, offset);
            }
            /* Load PC into tmp and CPSR into tmp2.  */
            tmp = gen_ld32(addr, context_to_mmu_index(s));
            tcg_gen_addi_i32(addr, addr, 4);
            tmp2 = gen_ld32(addr, context_to_mmu_index(s));
            if(insn & (1 << 21)) {
                /* Base writeback.  */
                switch(i) {
                    case 0:
                        offset = -8;
                        break;
                    case 1:
                        offset = 4;
                        break;
                    case 2:
                        offset = -4;
                        break;
                    case 3:
                        offset = 0;
                        break;
                    default:
                        abort();
                }
                if(offset) {
                    tcg_gen_addi_i32(addr, addr, offset);
                }
                store_reg(s, rn, addr);
            } else {
                tcg_temp_free_i32(addr);
            }
            gen_rfe(s, tmp, tmp2);
            return;
        } else if((insn & 0x0e000000) == 0x0a000000) {
            /* branch link and change to thumb (blx <offset>) */
            int32_t offset;

            val = (uint32_t)s->base.pc;
            tmp = tcg_temp_new_i32();
            tcg_gen_movi_i32(tmp, val);
            store_reg(s, 14, tmp);
            /* Sign-extend the 24-bit offset */
            offset = (((int32_t)insn) << 8) >> 8;
            /* offset * 4 + bit24 * 2 + (thumb bit) */
            val += (offset << 2) | ((insn >> 23) & 2) | 1;
            /* pipeline offset */
            val += 4;
            /* protected by ARCH(5); above, near the start of uncond block */
            /* New stack frame, return address stored in LR */
            gen_bx_im(s, val, STACK_FRAME_ADD);
            return;
        } else if((insn & 0x0e000f00) == 0x0c000100) {
            if(arm_feature(env, ARM_FEATURE_IWMMXT)) {
                /* iWMMXt register transfer.  */
                gen_set_pc(current_pc);
                if(env->cp15.c15_cpar & (1 << 1)) {
                    if(!disas_iwmmxt_insn(env, s, insn)) {
                        return;
                    }
                }
            }
        } else if((insn & 0x0fe00000) == 0x0c400000) {
            /* Coprocessor double register transfer. (MCRR2, MRRC2)  */
            ARCH(5TE);
            goto do_coproc_transfer;
        } else if((insn & 0x0f000010) == 0x0e000010) {
            /* MCR2/MRC2 Encoding A2 */
            goto do_coproc_transfer;
        } else if((insn & 0x0ff10020) == 0x01000000) {
            uint32_t mask;
            uint32_t val;
            /* cps (privileged) */
            if(s->user) {
                return;
            }
            mask = val = 0;
            if(insn & (1 << 19)) {
                if(insn & (1 << 8)) {
                    mask |= CPSR_A;
                }
                if(insn & (1 << 7)) {
                    mask |= CPSR_I;
                }
                if(insn & (1 << 6)) {
                    mask |= CPSR_F;
                }
                if(insn & (1 << 18)) {
                    val |= mask;
                }
            }
            if(insn & (1 << 17)) {
                mask |= CPSR_M;
                val |= (insn & 0x1f);
            }
            if(mask) {
                gen_set_psr_im(s, mask, 0, val);
            }
            return;
        }
        goto illegal_op;
    }
    if(cond != 0xe) {
        /* if not always execute, we generate a conditional jump to
           next instruction */
        s->condlabel = gen_new_label();
        gen_test_cc(cond ^ 1, s->condlabel);
        s->condjmp = 1;
    }
    if((insn & 0x0f900000) == 0x03000000) {
        if((insn & (1 << 21)) == 0) {
            ARCH(6T2);
            rd = (insn >> 12) & 0xf;
            val = ((insn >> 4) & 0xf000) | (insn & 0xfff);
            if((insn & (1 << 22)) == 0) {
                /* MOVW */
                tmp = tcg_temp_new_i32();
                tcg_gen_movi_i32(tmp, val);
            } else {
                /* MOVT */
                tmp = load_reg(s, rd);
                tcg_gen_ext16u_i32(tmp, tmp);
                tcg_gen_ori_i32(tmp, tmp, val << 16);
            }
            store_reg(s, rd, tmp);
        } else {
            if(((insn >> 12) & 0xf) != 0xf) {
                goto illegal_op;
            }
            if(((insn >> 16) & 0xf) == 0) {
                gen_nop_hint(s, insn & 0xff);
            } else {
                /* CPSR = immediate */
                val = insn & 0xff;
                shift = ((insn >> 8) & 0xf) * 2;
                if(shift) {
                    val = (val >> shift) | (val << (32 - shift));
                }
                i = ((insn & (1 << 22)) != 0);
                if(gen_set_psr_im(s, msr_mask(env, s, (insn >> 16) & 0xf, i), i, val)) {
                    goto illegal_op;
                }
            }
        }
    } else if((insn & 0x0f900000) == 0x01000000 && (insn & 0x00000090) != 0x00000090) {
        /* miscellaneous instructions */
        op1 = (insn >> 21) & 3;
        sh = (insn >> 4) & 0xf;
        rm = insn & 0xf;
        switch(sh) {
            case 0x0: /* move program status register */
                if(op1 & 1) {
                    /* PSR = reg */
                    tmp = load_reg(s, rm);
                    i = ((op1 & 2) != 0);
                    if(gen_set_psr(s, msr_mask(env, s, (insn >> 16) & 0xf, i), i, tmp)) {
                        goto illegal_op;
                    }
                } else {
                    /* reg = PSR */
                    rd = (insn >> 12) & 0xf;
                    if(op1 & 2) {
                        if(s->user) {
                            goto illegal_op;
                        }
                        tmp = load_cpu_field(spsr);
                    } else {
                        tmp = tcg_temp_new_i32();
                        gen_helper_cpsr_read(tmp);
                    }
                    store_reg(s, rd, tmp);
                }
                break;
            case 0x1:
                if(op1 == 1) {
                    /* branch/exchange thumb (bx).  */
                    ARCH(4T);
                    tmp = load_reg(s, rm);
                    /* Exit from subroutine if the target register is LR (r14)  */
                    gen_bx(s, tmp, rm == 14 ? STACK_FRAME_POP : STACK_FRAME_NO_CHANGE);
                } else if(op1 == 3) {
                    /* clz */
                    ARCH(5);
                    rd = (insn >> 12) & 0xf;
                    tmp = load_reg(s, rm);
                    gen_helper_clz(tmp, tmp);
                    store_reg(s, rd, tmp);
                } else {
                    goto illegal_op;
                }
                break;
            case 0x2:
                if(op1 == 1) {
                    ARCH(5J); /* bxj */
                    /* Trivial implementation equivalent to bx.  */
                    tmp = load_reg(s, rm);
                    /* Same as bx */
                    gen_bx(s, tmp, rm == 14 ? STACK_FRAME_POP : STACK_FRAME_NO_CHANGE);
                } else {
                    goto illegal_op;
                }
                break;
            case 0x3:
                if(op1 != 1) {
                    goto illegal_op;
                }

                ARCH(5);
                /* branch link/exchange thumb (blx) */
                tmp = load_reg(s, rm);
                tmp2 = tcg_temp_new_i32();
                tcg_gen_movi_i32(tmp2, s->base.pc);
                store_reg(s, 14, tmp2);
                /* Branch with link - new stack frame */
                gen_bx(s, tmp, STACK_FRAME_ADD);
                break;
            case 0x5: /* saturating add/subtract */
                ARCH(5TE);
                rd = (insn >> 12) & 0xf;
                rn = (insn >> 16) & 0xf;
                tmp = load_reg(s, rm);
                tmp2 = load_reg(s, rn);
                if(op1 & 2) {
                    gen_helper_double_saturate(tmp2, tmp2);
                }
                if(op1 & 1) {
                    gen_helper_sub_saturate(tmp, tmp, tmp2);
                } else {
                    gen_helper_add_saturate(tmp, tmp, tmp2);
                }
                tcg_temp_free_i32(tmp2);
                store_reg(s, rd, tmp);
                break;
            case 7:
                /* SMC instruction (op1 == 3)
                   and undefined instructions (op1 == 0 || op1 == 2)
                   will trap */
                if(op1 == 3) {
                    /* TODO: enable L2 cache - currently no-op */
                    break;
                }
                if(op1 != 1) {
                    goto illegal_op;
                }
                /* bkpt */
                ARCH(5);
                gen_exception_insn(s, 4, EXCP_BKPT);
                LOCK_TB(s->base.tb);
                break;
            case 0x8: /* signed multiply */
            case 0xa:
            case 0xc:
            case 0xe:
                ARCH(5TE);
                rs = (insn >> 8) & 0xf;
                rn = (insn >> 12) & 0xf;
                rd = (insn >> 16) & 0xf;
                if(op1 == 1) {
                    /* (32 * 16) >> 16 */
                    tmp = load_reg(s, rm);
                    tmp2 = load_reg(s, rs);
                    if(sh & 4) {
                        tcg_gen_sari_i32(tmp2, tmp2, 16);
                    } else {
                        gen_sxth(tmp2);
                    }
                    tmp64 = gen_muls_i64_i32(tmp, tmp2);
                    tcg_gen_shri_i64(tmp64, tmp64, 16);
                    tmp = tcg_temp_new_i32();
                    tcg_gen_trunc_i64_i32(tmp, tmp64);
                    tcg_temp_free_i64(tmp64);
                    if((sh & 2) == 0) {
                        tmp2 = load_reg(s, rn);
                        gen_helper_add_setq(tmp, tmp, tmp2);
                        tcg_temp_free_i32(tmp2);
                    }
                    store_reg(s, rd, tmp);
                } else {
                    /* 16 * 16 */
                    tmp = load_reg(s, rm);
                    tmp2 = load_reg(s, rs);
                    gen_mulxy(tmp, tmp2, sh & 2, sh & 4);
                    tcg_temp_free_i32(tmp2);
                    if(op1 == 2) {
                        tmp64 = tcg_temp_new_i64();
                        tcg_gen_ext_i32_i64(tmp64, tmp);
                        tcg_temp_free_i32(tmp);
                        gen_addq(s, tmp64, rn, rd);
                        gen_storeq_reg(s, rn, rd, tmp64);
                        tcg_temp_free_i64(tmp64);
                    } else {
                        if(op1 == 0) {
                            tmp2 = load_reg(s, rn);
                            gen_helper_add_setq(tmp, tmp, tmp2);
                            tcg_temp_free_i32(tmp2);
                        }
                        store_reg(s, rd, tmp);
                    }
                }
                break;
            default:
                goto illegal_op;
        }
    } else if(((insn & 0x0e000000) == 0 && (insn & 0x00000090) != 0x90) || ((insn & 0x0e000000) == (1 << 25))) {
        int set_cc, logic_cc, shiftop;

        op1 = (insn >> 21) & 0xf;
        set_cc = (insn >> 20) & 1;
        logic_cc = table_logic_cc[op1] & set_cc;

        /* data processing instruction */
        if(insn & (1 << 25)) {
            /* immediate operand */
            val = insn & 0xff;
            shift = ((insn >> 8) & 0xf) * 2;
            if(shift) {
                val = (val >> shift) | (val << (32 - shift));
            }
            tmp2 = tcg_temp_new_i32();
            tcg_gen_movi_i32(tmp2, val);
            if(logic_cc && shift) {
                gen_set_CF_bit31(tmp2);
            }
        } else {
            /* register */
            rm = (insn) & 0xf;
            tmp2 = load_reg(s, rm);
            shiftop = (insn >> 5) & 3;
            if(!(insn & (1 << 4))) {
                shift = (insn >> 7) & 0x1f;
                gen_arm_shift_im(tmp2, shiftop, shift, logic_cc);
            } else {
                rs = (insn >> 8) & 0xf;
                tmp = load_reg(s, rs);
                gen_arm_shift_reg(tmp2, shiftop, tmp, logic_cc);
            }
        }
        if(op1 != 0x0f && op1 != 0x0d) {
            rn = (insn >> 16) & 0xf;
            tmp = load_reg(s, rn);
        } else {
            TCGV_UNUSED(tmp);
        }
        rd = (insn >> 12) & 0xf;
        switch(op1) {
            case 0x00:
                tcg_gen_and_i32(tmp, tmp, tmp2);
                if(logic_cc) {
                    gen_logic_CC(tmp);
                }
                store_reg_bx(env, s, rd, tmp);
                break;
            case 0x01:
                tcg_gen_xor_i32(tmp, tmp, tmp2);
                if(logic_cc) {
                    gen_logic_CC(tmp);
                }
                store_reg_bx(env, s, rd, tmp);
                break;
            case 0x02:
                if(set_cc && rd == 15) {
                    /* SUBS r15, ... is used for exception return.  */
                    if(s->user) {
                        goto illegal_op;
                    }
                    gen_helper_sub_cc(tmp, tmp, tmp2);
                    gen_exception_return(s, tmp);
                } else {
                    if(set_cc) {
                        gen_helper_sub_cc(tmp, tmp, tmp2);
                    } else {
                        tcg_gen_sub_i32(tmp, tmp, tmp2);
                    }
                    store_reg_bx(env, s, rd, tmp);
                }
                break;
            case 0x03:
                if(set_cc) {
                    gen_helper_sub_cc(tmp, tmp2, tmp);
                } else {
                    tcg_gen_sub_i32(tmp, tmp2, tmp);
                }
                store_reg_bx(env, s, rd, tmp);
                break;
            case 0x04:
                if(set_cc) {
                    gen_helper_add_cc(tmp, tmp, tmp2);
                } else {
                    tcg_gen_add_i32(tmp, tmp, tmp2);
                }
                store_reg_bx(env, s, rd, tmp);
                break;
            case 0x05:
                if(set_cc) {
                    gen_helper_adc_cc(tmp, tmp, tmp2);
                } else {
                    gen_add_carry(tmp, tmp, tmp2);
                }
                store_reg_bx(env, s, rd, tmp);
                break;
            case 0x06:
                if(set_cc) {
                    gen_helper_sbc_cc(tmp, tmp, tmp2);
                } else {
                    gen_sub_carry(tmp, tmp, tmp2);
                }
                store_reg_bx(env, s, rd, tmp);
                break;
            case 0x07:
                if(set_cc) {
                    gen_helper_sbc_cc(tmp, tmp2, tmp);
                } else {
                    gen_sub_carry(tmp, tmp2, tmp);
                }
                store_reg_bx(env, s, rd, tmp);
                break;
            case 0x08:
                if(set_cc) {
                    tcg_gen_and_i32(tmp, tmp, tmp2);
                    gen_logic_CC(tmp);
                }
                tcg_temp_free_i32(tmp);
                break;
            case 0x09:
                if(set_cc) {
                    tcg_gen_xor_i32(tmp, tmp, tmp2);
                    gen_logic_CC(tmp);
                }
                tcg_temp_free_i32(tmp);
                break;
            case 0x0a:
                if(set_cc) {
                    gen_helper_sub_cc(tmp, tmp, tmp2);
                }
                tcg_temp_free_i32(tmp);
                break;
            case 0x0b:
                if(set_cc) {
                    gen_helper_add_cc(tmp, tmp, tmp2);
                }
                tcg_temp_free_i32(tmp);
                break;
            case 0x0c:
                tcg_gen_or_i32(tmp, tmp, tmp2);
                if(logic_cc) {
                    gen_logic_CC(tmp);
                }
                store_reg_bx(env, s, rd, tmp);
                break;
            case 0x0d:
                if(logic_cc && rd == 15) {
                    /* MOVS r15, ... is used for exception return.  */
                    if(s->user) {
                        goto illegal_op;
                    }
                    gen_exception_return(s, tmp2);
                } else {
                    if(logic_cc) {
                        gen_logic_CC(tmp2);
                    }
                    store_reg_bx(env, s, rd, tmp2);
                }
                break;
            case 0x0e:
                tcg_gen_andc_i32(tmp, tmp, tmp2);
                if(logic_cc) {
                    gen_logic_CC(tmp);
                }
                store_reg_bx(env, s, rd, tmp);
                break;
            default:
            case 0x0f:
                tcg_gen_not_i32(tmp2, tmp2);
                if(logic_cc) {
                    gen_logic_CC(tmp2);
                }
                store_reg_bx(env, s, rd, tmp2);
                break;
        }
        if(op1 != 0x0f && op1 != 0x0d) {
            tcg_temp_free_i32(tmp2);
        }
    } else {
        /* other instructions */
        op1 = (insn >> 24) & 0xf;
        gen_set_pc(current_pc);
        switch(op1) {
            case 0x0:
            case 0x1:
                /* multiplies, extra load/stores */
                sh = (insn >> 5) & 3;
                if(sh == 0) {
                    if(op1 == 0x0) {
                        rd = (insn >> 16) & 0xf;
                        rn = (insn >> 12) & 0xf;
                        rs = (insn >> 8) & 0xf;
                        rm = (insn) & 0xf;
                        op1 = (insn >> 20) & 0xf;
                        switch(op1) {
                            case 0:
                            case 1:
                            case 2:
                            case 3:
                            case 6:
                                /* 32 bit mul */
                                tmp = load_reg(s, rs);
                                tmp2 = load_reg(s, rm);
                                tcg_gen_mul_i32(tmp, tmp, tmp2);
                                tcg_temp_free_i32(tmp2);
                                if(insn & (1 << 22)) {
                                    /* Subtract (mls) */
                                    ARCH(6T2);
                                    tmp2 = load_reg(s, rn);
                                    tcg_gen_sub_i32(tmp, tmp2, tmp);
                                    tcg_temp_free_i32(tmp2);
                                } else if(insn & (1 << 21)) {
                                    /* Add */
                                    tmp2 = load_reg(s, rn);
                                    tcg_gen_add_i32(tmp, tmp, tmp2);
                                    tcg_temp_free_i32(tmp2);
                                }
                                if(insn & (1 << 20)) {
                                    gen_logic_CC(tmp);
                                }
                                store_reg(s, rd, tmp);
                                break;
                            case 4:
                                /* 64 bit mul double accumulate (UMAAL) */
                                ARCH(6);
                                tmp = load_reg(s, rs);
                                tmp2 = load_reg(s, rm);
                                tmp64 = gen_mulu_i64_i32(tmp, tmp2);
                                gen_addq_lo(s, tmp64, rn);
                                gen_addq_lo(s, tmp64, rd);
                                gen_storeq_reg(s, rn, rd, tmp64);
                                tcg_temp_free_i64(tmp64);
                                break;
                            case 8:
                            case 9:
                            case 10:
                            case 11:
                            case 12:
                            case 13:
                            case 14:
                            case 15:
                                /* 64 bit mul: UMULL, UMLAL, SMULL, SMLAL. */
                                tmp = load_reg(s, rs);
                                tmp2 = load_reg(s, rm);
                                if(insn & (1 << 22)) {
                                    tmp64 = gen_muls_i64_i32(tmp, tmp2);
                                } else {
                                    tmp64 = gen_mulu_i64_i32(tmp, tmp2);
                                }
                                if(insn & (1 << 21)) { /* mult accumulate */
                                    gen_addq(s, tmp64, rn, rd);
                                }
                                if(insn & (1 << 20)) {
                                    gen_logicq_cc(tmp64);
                                }
                                gen_storeq_reg(s, rn, rd, tmp64);
                                tcg_temp_free_i64(tmp64);
                                break;
                            default:
                                goto illegal_op;
                        }
                    } else {
                        rn = (insn >> 16) & 0xf;
                        rd = (insn >> 12) & 0xf;
                        if(insn & (1 << 23)) {
                            /* load/store exclusive */
                            op1 = (insn >> 21) & 0x3;
                            if(op1) {
                                ARCH(6K);
                            } else {
                                ARCH(6);
                            }
                            addr = tcg_temp_local_new_i32();
                            load_reg_var(s, addr, rn);
                            if(insn & (1 << 20)) {
                                switch(op1) {
                                    case 0: /* ldrex */
                                        gen_load_exclusive(s, rd, 15, addr, 2);
                                        break;
                                    case 1: /* ldrexd */
                                        gen_load_exclusive(s, rd, rd + 1, addr, 3);
                                        break;
                                    case 2: /* ldrexb */
                                        gen_load_exclusive(s, rd, 15, addr, 0);
                                        break;
                                    case 3: /* ldrexh */
                                        gen_load_exclusive(s, rd, 15, addr, 1);
                                        break;
                                    default:
                                        abort();
                                }
                            } else {
                                rm = insn & 0xf;
                                switch(op1) {
                                    case 0: /*  strex */
                                        gen_store_exclusive(s, rd, rm, 15, addr, 2);
                                        break;
                                    case 1: /*  strexd */
                                        gen_store_exclusive(s, rd, rm, rm + 1, addr, 3);
                                        break;
                                    case 2: /*  strexb */
                                        gen_store_exclusive(s, rd, rm, 15, addr, 0);
                                        break;
                                    case 3: /* strexh */
                                        gen_store_exclusive(s, rd, rm, 15, addr, 1);
                                        break;
                                    default:
                                        abort();
                                }
                            }
                            tcg_temp_free(addr);
                        } else {
                            /* SWP instruction */
                            rm = (insn) & 0xf;

                            /* ??? This is not really atomic.  However we know
                               we never have multiple CPUs running in parallel,
                               so it is good enough.  */
                            addr = load_reg(s, rn);
                            tmp = load_reg(s, rm);
                            if(insn & (1 << 22)) {
                                tmp2 = gen_ld8u(addr, context_to_mmu_index(s));
                                gen_st8(tmp, addr, context_to_mmu_index(s));
                            } else {
                                tmp2 = gen_ld32(addr, context_to_mmu_index(s));
                                gen_st32(tmp, addr, context_to_mmu_index(s));
                            }
                            tcg_temp_free_i32(addr);
                            store_reg(s, rd, tmp2);
                        }
                    }
                } else {
                    int address_offset;
                    int load;
                    /* Misc load/store */
                    rn = (insn >> 16) & 0xf;
                    rd = (insn >> 12) & 0xf;
                    addr = load_reg(s, rn);
                    if(insn & (1 << 24)) {
                        gen_add_datah_offset(s, insn, 0, addr);
                    }
                    address_offset = 0;
                    if(insn & (1 << 20)) {
                        /* load */
                        switch(sh) {
                            case 1:
                                tmp = gen_ld16u(addr, context_to_mmu_index(s));
                                break;
                            case 2:
                                tmp = gen_ld8s(addr, context_to_mmu_index(s));
                                break;
                            default:
                            case 3:
                                tmp = gen_ld16s(addr, context_to_mmu_index(s));
                                break;
                        }
                        load = 1;
                    } else if(sh & 2) {
                        ARCH(5TE);
                        /* doubleword */
                        if(sh & 1) {
                            /* store */
                            tmp = load_reg(s, rd);
                            gen_st32(tmp, addr, context_to_mmu_index(s));
                            tcg_gen_addi_i32(addr, addr, 4);
                            tmp = load_reg(s, rd + 1);
                            gen_st32(tmp, addr, context_to_mmu_index(s));
                            load = 0;
                        } else {
                            /* load */
                            tmp = gen_ld32(addr, context_to_mmu_index(s));
                            store_reg(s, rd, tmp);
                            tcg_gen_addi_i32(addr, addr, 4);
                            tmp = gen_ld32(addr, context_to_mmu_index(s));
                            rd++;
                            load = 1;
                        }
                        address_offset = -4;
                    } else {
                        /* store */
                        tmp = load_reg(s, rd);
                        gen_st16(tmp, addr, context_to_mmu_index(s));
                        load = 0;
                    }
                    /* Perform base writeback before the loaded value to
                       ensure correct behavior with overlapping index registers.
                       ldrd with base writeback is is undefined if the
                       destination and index registers overlap.  */
                    if(!(insn & (1 << 24))) {
                        gen_add_datah_offset(s, insn, address_offset, addr);
                        store_reg(s, rn, addr);
                    } else if(insn & (1 << 21)) {
                        if(address_offset) {
                            tcg_gen_addi_i32(addr, addr, address_offset);
                        }
                        store_reg(s, rn, addr);
                    } else {
                        tcg_temp_free_i32(addr);
                    }
                    if(load) {
                        /* Complete the load.  */
                        store_reg(s, rd, tmp);
                    }
                }
                break;
            case 0x4:
            case 0x5:
                goto do_ldst;
            case 0x6:
            case 0x7:
                if(insn & (1 << 4)) {
                    ARCH(6);
                    /* Armv6 Media instructions.  */
                    rm = insn & 0xf;
                    rn = (insn >> 16) & 0xf;
                    rd = (insn >> 12) & 0xf;
                    rs = (insn >> 8) & 0xf;
                    switch((insn >> 23) & 3) {
                        case 0: /* Parallel add/subtract.  */
                            op1 = (insn >> 20) & 7;
                            tmp = load_reg(s, rn);
                            tmp2 = load_reg(s, rm);
                            sh = (insn >> 5) & 7;
                            if((op1 & 3) == 0 || sh == 5 || sh == 6) {
                                goto illegal_op;
                            }
                            gen_arm_parallel_addsub(op1, sh, tmp, tmp2);
                            tcg_temp_free_i32(tmp2);
                            store_reg(s, rd, tmp);
                            break;
                        case 1:
                            if((insn & 0x00700020) == 0) {
                                /* Halfword pack.  */
                                tmp = load_reg(s, rn);
                                tmp2 = load_reg(s, rm);
                                shift = (insn >> 7) & 0x1f;
                                if(insn & (1 << 6)) {
                                    /* pkhtb */
                                    if(shift == 0) {
                                        shift = 31;
                                    }
                                    tcg_gen_sari_i32(tmp2, tmp2, shift);
                                    tcg_gen_andi_i32(tmp, tmp, 0xffff0000);
                                    tcg_gen_ext16u_i32(tmp2, tmp2);
                                } else {
                                    /* pkhbt */
                                    if(shift) {
                                        tcg_gen_shli_i32(tmp2, tmp2, shift);
                                    }
                                    tcg_gen_ext16u_i32(tmp, tmp);
                                    tcg_gen_andi_i32(tmp2, tmp2, 0xffff0000);
                                }
                                tcg_gen_or_i32(tmp, tmp, tmp2);
                                tcg_temp_free_i32(tmp2);
                                store_reg(s, rd, tmp);
                            } else if((insn & 0x00200020) == 0x00200000) {
                                /* [us]sat */
                                tmp = load_reg(s, rm);
                                shift = (insn >> 7) & 0x1f;
                                if(insn & (1 << 6)) {
                                    if(shift == 0) {
                                        shift = 31;
                                    }
                                    tcg_gen_sari_i32(tmp, tmp, shift);
                                } else {
                                    tcg_gen_shli_i32(tmp, tmp, shift);
                                }
                                sh = (insn >> 16) & 0x1f;
                                tmp2 = tcg_const_i32(sh);
                                if(insn & (1 << 22)) {
                                    gen_helper_usat(tmp, tmp, tmp2);
                                } else {
                                    gen_helper_ssat(tmp, tmp, tmp2);
                                }
                                tcg_temp_free_i32(tmp2);
                                store_reg(s, rd, tmp);
                            } else if((insn & 0x00300fe0) == 0x00200f20) {
                                /* [us]sat16 */
                                tmp = load_reg(s, rm);
                                sh = (insn >> 16) & 0x1f;
                                tmp2 = tcg_const_i32(sh);
                                if(insn & (1 << 22)) {
                                    gen_helper_usat16(tmp, tmp, tmp2);
                                } else {
                                    gen_helper_ssat16(tmp, tmp, tmp2);
                                }
                                tcg_temp_free_i32(tmp2);
                                store_reg(s, rd, tmp);
                            } else if((insn & 0x00700fe0) == 0x00000fa0) {
                                /* Select bytes.  */
                                tmp = load_reg(s, rn);
                                tmp2 = load_reg(s, rm);
                                tmp3 = tcg_temp_new_i32();
                                tcg_gen_ld_i32(tmp3, cpu_env, offsetof(CPUState, GE));
                                gen_helper_sel_flags(tmp, tmp3, tmp, tmp2);
                                tcg_temp_free_i32(tmp3);
                                tcg_temp_free_i32(tmp2);
                                store_reg(s, rd, tmp);
                            } else if((insn & 0x000003e0) == 0x00000060) {
                                tmp = load_reg(s, rm);
                                shift = (insn >> 10) & 3;
                                /* ??? In many cases it's not necessary to do a
                                   rotate, a shift is sufficient.  */
                                if(shift != 0) {
                                    tcg_gen_rotri_i32(tmp, tmp, shift * 8);
                                }
                                op1 = (insn >> 20) & 7;
                                switch(op1) {
                                    case 0:
                                        gen_sxtb16(tmp);
                                        break;
                                    case 2:
                                        gen_sxtb(tmp);
                                        break;
                                    case 3:
                                        gen_sxth(tmp);
                                        break;
                                    case 4:
                                        gen_uxtb16(tmp);
                                        break;
                                    case 6:
                                        gen_uxtb(tmp);
                                        break;
                                    case 7:
                                        gen_uxth(tmp);
                                        break;
                                    default:
                                        goto illegal_op;
                                }
                                if(rn != 15) {
                                    tmp2 = load_reg(s, rn);
                                    if((op1 & 3) == 0) {
                                        gen_add16(tmp, tmp2);
                                    } else {
                                        tcg_gen_add_i32(tmp, tmp, tmp2);
                                        tcg_temp_free_i32(tmp2);
                                    }
                                }
                                store_reg(s, rd, tmp);
                            } else if((insn & 0x003f0f60) == 0x003f0f20) {
                                /* rev */
                                tmp = load_reg(s, rm);
                                if(insn & (1 << 22)) {
                                    if(insn & (1 << 7)) {
                                        gen_revsh(tmp);
                                    } else {
                                        ARCH(6T2);
                                        gen_helper_rbit(tmp, tmp);
                                    }
                                } else {
                                    if(insn & (1 << 7)) {
                                        gen_rev16(tmp);
                                    } else {
                                        tcg_gen_bswap32_i32(tmp, tmp);
                                    }
                                }
                                store_reg(s, rd, tmp);
                            } else {
                                goto illegal_op;
                            }
                            break;
                        case 2: /* Multiplies (Type 3).  */
                            switch((insn >> 20) & 0x7) {
                                case 5:
                                    if(((insn >> 6) ^ (insn >> 7)) & 1) {
                                        /* op2 not 00x or 11x : UNDEF */
                                        goto illegal_op;
                                    }
                                    /* Signed multiply most significant [accumulate].
                                       (SMMUL, SMMLA, SMMLS) */
                                    tmp = load_reg(s, rm);
                                    tmp2 = load_reg(s, rs);
                                    tmp64 = gen_muls_i64_i32(tmp, tmp2);

                                    if(rd != 15) {
                                        tmp = load_reg(s, rd);
                                        if(insn & (1 << 6)) {
                                            tmp64 = gen_subq_msw(tmp64, tmp);
                                        } else {
                                            tmp64 = gen_addq_msw(tmp64, tmp);
                                        }
                                    }
                                    if(insn & (1 << 5)) {
                                        tcg_gen_addi_i64(tmp64, tmp64, 0x80000000u);
                                    }
                                    tcg_gen_shri_i64(tmp64, tmp64, 32);
                                    tmp = tcg_temp_new_i32();
                                    tcg_gen_trunc_i64_i32(tmp, tmp64);
                                    tcg_temp_free_i64(tmp64);
                                    store_reg(s, rn, tmp);
                                    break;
                                case 0:
                                case 4:
                                    /* SMLAD, SMUAD, SMLSD, SMUSD, SMLALD, SMLSLD */
                                    if(insn & (1 << 7)) {
                                        goto illegal_op;
                                    }
                                    tmp = load_reg(s, rm);
                                    tmp2 = load_reg(s, rs);
                                    if(insn & (1 << 5)) {
                                        gen_swap_half(tmp2);
                                    }
                                    gen_smul_dual(tmp, tmp2);
                                    if(insn & (1 << 6)) {
                                        /* This subtraction cannot overflow. */
                                        tcg_gen_sub_i32(tmp, tmp, tmp2);
                                    } else {
                                        /* This addition cannot overflow 32 bits;
                                         * however it may overflow considered as a signed
                                         * operation, in which case we must set the Q flag.
                                         */
                                        gen_helper_add_setq(tmp, tmp, tmp2);
                                    }
                                    tcg_temp_free_i32(tmp2);
                                    if(insn & (1 << 22)) {
                                        /* smlald, smlsld */
                                        tmp64 = tcg_temp_new_i64();
                                        tcg_gen_ext_i32_i64(tmp64, tmp);
                                        tcg_temp_free_i32(tmp);
                                        gen_addq(s, tmp64, rd, rn);
                                        gen_storeq_reg(s, rd, rn, tmp64);
                                        tcg_temp_free_i64(tmp64);
                                    } else {
                                        /* smuad, smusd, smlad, smlsd */
                                        if(rd != 15) {
                                            tmp2 = load_reg(s, rd);
                                            gen_helper_add_setq(tmp, tmp, tmp2);
                                            tcg_temp_free_i32(tmp2);
                                        }
                                        store_reg(s, rn, tmp);
                                    }
                                    break;
                                case 1:
                                case 3:
                                    /* SDIV, UDIV */
                                    if(!arm_feature(env, ARM_FEATURE_ARM_DIV)) {
                                        goto illegal_op;
                                    }
                                    if(((insn >> 5) & 7) || (rd != 15)) {
                                        goto illegal_op;
                                    }
                                    tmp = load_reg(s, rm);
                                    tmp2 = load_reg(s, rs);
                                    if(insn & (1 << 21)) {
                                        gen_helper_udiv(tmp, tmp, tmp2);
                                    } else {
                                        gen_helper_sdiv(tmp, tmp, tmp2);
                                    }
                                    tcg_temp_free_i32(tmp2);
                                    store_reg(s, rn, tmp);
                                    break;
                                default:
                                    goto illegal_op;
                            }
                            break;
                        case 3:
                            op1 = ((insn >> 17) & 0x38) | ((insn >> 5) & 7);
                            switch(op1) {
                                case 0: /* Unsigned sum of absolute differences.  */
                                    ARCH(6);
                                    tmp = load_reg(s, rm);
                                    tmp2 = load_reg(s, rs);
                                    gen_helper_usad8(tmp, tmp, tmp2);
                                    tcg_temp_free_i32(tmp2);
                                    if(rd != 15) {
                                        tmp2 = load_reg(s, rd);
                                        tcg_gen_add_i32(tmp, tmp, tmp2);
                                        tcg_temp_free_i32(tmp2);
                                    }
                                    store_reg(s, rn, tmp);
                                    break;
                                case 0x20:
                                case 0x24:
                                case 0x28:
                                case 0x2c:
                                    /* Bitfield insert/clear.  */
                                    ARCH(6T2);
                                    shift = (insn >> 7) & 0x1f;
                                    i = (insn >> 16) & 0x1f;
                                    i = i + 1 - shift;
                                    if(rm == 15) {
                                        tmp = tcg_temp_new_i32();
                                        tcg_gen_movi_i32(tmp, 0);
                                    } else {
                                        tmp = load_reg(s, rm);
                                    }
                                    if(i != 32) {
                                        tmp2 = load_reg(s, rd);
                                        gen_bfi(tmp, tmp2, tmp, shift, (1u << i) - 1);
                                        tcg_temp_free_i32(tmp2);
                                    }
                                    store_reg(s, rd, tmp);
                                    break;
                                case 0x12:  //  sbfx
                                case 0x16:
                                case 0x1a:
                                case 0x1e:
                                case 0x32:  //  ubfx
                                case 0x36:
                                case 0x3a:
                                case 0x3e:
                                    ARCH(6T2);
                                    tmp = load_reg(s, rm);
                                    shift = (insn >> 7) & 0x1f;
                                    i = ((insn >> 16) & 0x1f) + 1;
                                    if(shift + i > 32) {
                                        goto illegal_op;
                                    }
                                    if(i < 32) {
                                        if(op1 & 0x20) {
                                            gen_ubfx(tmp, shift, (1u << i) - 1);
                                        } else {
                                            gen_sbfx(tmp, shift, i);
                                        }
                                    }
                                    store_reg(s, rd, tmp);
                                    break;
                                default:
                                    goto illegal_op;
                            }
                            break;
                    }
                    break;
                }
            do_ldst:
                /* Check for undefined extension instructions
                 * per the ARM Bible IE:
                 * xxxx 0111 1111 xxxx  xxxx xxxx 1111 xxxx
                 */
                sh = (0xf << 20) | (0xf << 4);
                if(op1 == 0x7 && ((insn & sh) == sh)) {
                    goto illegal_op;
                }
                /* load/store byte/word */
                rn = (insn >> 16) & 0xf;
                rd = (insn >> 12) & 0xf;
                tmp2 = load_reg(s, rn);

                MMUMode mmu_mode = context_to_mmu_mode(s);
                if((insn & 0x01200000) == 0x00200000) {
                    mmu_mode.user = true;
                }
                if(insn & (1 << 24)) {
                    gen_add_data_offset(s, insn, tmp2);
                }
                if(insn & (1 << 20)) {
                    /* load */
                    if(insn & (1 << 22)) {
                        tmp = gen_ld8u(tmp2, mmu_mode.index);
                    } else {
                        tmp = gen_ld32(tmp2, mmu_mode.index);
                    }
                } else {
                    /* store */
                    tmp = load_reg(s, rd);
                    if(insn & (1 << 22)) {
                        gen_st8(tmp, tmp2, mmu_mode.index);
                    } else {
                        gen_st32(tmp, tmp2, mmu_mode.index);
                    }
                }
                if(!(insn & (1 << 24))) {
                    gen_add_data_offset(s, insn, tmp2);
                    store_reg(s, rn, tmp2);
                } else if(insn & (1 << 21)) {
                    store_reg(s, rn, tmp2);
                } else {
                    tcg_temp_free_i32(tmp2);
                }
                if(insn & (1 << 20)) {
                    /* Complete the load.  */
                    /* Should be POP - loading PC form stack */
                    store_reg_from_load(env, s, rd, tmp, STACK_FRAME_POP);
                }
                break;
            case 0x08:
            case 0x09: {
                int j, n, user, loaded_base;
                TCGv loaded_var;
                /* load/store multiple words */
                /* XXX: store correct base if write back */
                user = 0;
                if(insn & (1 << 22)) {
                    if(s->user) {
                        goto illegal_op; /* only usable in supervisor mode */
                    }
                    if((insn & (1 << 15)) == 0) {
                        user = 1;
                    }
                }
                rn = (insn >> 16) & 0xf;
                addr = load_reg(s, rn);

                /* compute total size */
                loaded_base = 0;
                TCGV_UNUSED(loaded_var);
                n = 0;
                for(i = 0; i < 16; i++) {
                    if(insn & (1 << i)) {
                        n++;
                    }
                }
                /* XXX: test invalid n == 0 case ? */
                if(insn & (1 << 23)) {
                    if(insn & (1 << 24)) {
                        /* pre increment */
                        tcg_gen_addi_i32(addr, addr, 4);
                    } else {
                        /* post increment */
                    }
                } else {
                    if(insn & (1 << 24)) {
                        /* pre decrement */
                        tcg_gen_addi_i32(addr, addr, -(n * 4));
                    } else {
                        /* post decrement */
                        if(n != 1) {
                            tcg_gen_addi_i32(addr, addr, -((n - 1) * 4));
                        }
                    }
                }
                j = 0;
                for(i = 0; i < 16; i++) {
                    if(insn & (1 << i)) {
                        if(insn & (1 << 20)) {
                            /* load */
                            tmp = gen_ld32(addr, context_to_mmu_index(s));
                            if(user) {
                                tmp2 = tcg_const_i32(i);
                                gen_helper_set_user_reg(tmp2, tmp);
                                tcg_temp_free_i32(tmp2);
                                tcg_temp_free_i32(tmp);
                            } else if(i == rn) {
                                loaded_var = tmp;
                                loaded_base = 1;
                            } else {
                                /* Should be pop when loading PC form stack */
                                store_reg_from_load(env, s, i, tmp, STACK_FRAME_POP);
                            }
                        } else {
                            /* store */
                            if(i == 15) {
                                /* special case: r15 = PC + 8 */
                                val = (long)s->base.pc + 4;
                                tmp = tcg_temp_new_i32();
                                tcg_gen_movi_i32(tmp, val);
                            } else if(user) {
                                tmp = tcg_temp_new_i32();
                                tmp2 = tcg_const_i32(i);
                                gen_helper_get_user_reg(tmp, tmp2);
                                tcg_temp_free_i32(tmp2);
                            } else {
                                tmp = load_reg(s, i);
                            }
                            gen_st32(tmp, addr, context_to_mmu_index(s));
                        }
                        j++;
                        /* no need to add after the last transfer */
                        if(j != n) {
                            tcg_gen_addi_i32(addr, addr, 4);
                        }
                    }
                }
                if(insn & (1 << 21)) {
                    /* write back */
                    if(insn & (1 << 23)) {
                        if(insn & (1 << 24)) {
                            /* pre increment */
                        } else {
                            /* post increment */
                            tcg_gen_addi_i32(addr, addr, 4);
                        }
                    } else {
                        if(insn & (1 << 24)) {
                            /* pre decrement */
                            if(n != 1) {
                                tcg_gen_addi_i32(addr, addr, -((n - 1) * 4));
                            }
                        } else {
                            /* post decrement */
                            tcg_gen_addi_i32(addr, addr, -(n * 4));
                        }
                    }
                    store_reg(s, rn, addr);
                } else {
                    tcg_temp_free_i32(addr);
                }
                if(loaded_base) {
                    store_reg(s, rn, loaded_var);
                }
                if((insn & (1 << 22)) && !user) {
                    /* Restore CPSR from SPSR.  */
                    tmp = load_cpu_field(spsr);
                    gen_set_cpsr(tmp, 0xffffffff);
                    tcg_temp_free_i32(tmp);
                    s->base.is_jmp = DISAS_UPDATE;
                }
            } break;
            case 0xa:
            case 0xb:
                /* branch (and link) */
                if(insn == 0xeafffffe) {
                    tlib_printf(LOG_LEVEL_NOISY, "Loop to itself detected");
                    gen_helper_wfi();
                    s->base.is_jmp = DISAS_JUMP;
                    LOCK_TB(s->base.tb);
                } else {
                    int32_t offset;
                    val = (int32_t)s->base.pc;
                    if(insn & (1 << 24)) {
                        tmp = tcg_temp_new_i32();
                        tcg_gen_movi_i32(tmp, val);
                        store_reg(s, 14, tmp);
                    }
                    offset = (((int32_t)insn << 8) >> 8);
                    val += (offset << 2) + 4;
                    /* Check if link bit is set and announce stack change accordingly */
                    gen_jmp(s, val, insn & (1 << 24) ? STACK_FRAME_ADD : STACK_FRAME_NO_CHANGE);
                }
                break;
            case 0xc:
            case 0xd:
            case 0xe:
                /* Coprocessor.  */
                //  MCR/MRC Encoding A1
            do_coproc_transfer:
                gen_set_pc(current_pc);

                if(disas_coproc_insn(env, s, insn)) {
                    goto illegal_op;
                }
                break;
            case 0xf:
                /* swi */
                gen_set_pc_im(s->base.pc);
                s->base.is_jmp = DISAS_SWI;
                LOCK_TB(s->base.tb);
                break;
            default:
            illegal_op:
                gen_exception_insn(s, 4, EXCP_UDEF);
                LOCK_TB(s->base.tb);
                break;
        }
    }
}

/* Return true if this is a Thumb-2 logical op.  */
static int thumb2_logic_op(int op)
{
    return (op < 8);
}

/* Generate code for a Thumb-2 data processing operation.  If CONDS is nonzero
   then set condition code flags based on the result of the operation.
   If SHIFTER_OUT is nonzero then set the carry flag for logical operations
   to the high bit of T1.
   Returns zero if the opcode is valid.  */

static int gen_thumb2_data_op(DisasContext *s, int op, int conds, uint32_t shifter_out, TCGv t0, TCGv t1)
{
    int logic_cc;

    logic_cc = 0;
    switch(op) {
        case 0: /* and */
            tcg_gen_and_i32(t0, t0, t1);
            logic_cc = conds;
            break;
        case 1: /* bic */
            tcg_gen_andc_i32(t0, t0, t1);
            logic_cc = conds;
            break;
        case 2: /* orr */
            tcg_gen_or_i32(t0, t0, t1);
            logic_cc = conds;
            break;
        case 3: /* orn */
            tcg_gen_orc_i32(t0, t0, t1);
            logic_cc = conds;
            break;
        case 4: /* eor */
            tcg_gen_xor_i32(t0, t0, t1);
            logic_cc = conds;
            break;
        case 8: /* add */
            if(conds) {
                gen_helper_add_cc(t0, t0, t1);
            } else {
                tcg_gen_add_i32(t0, t0, t1);
            }
            break;
        case 10: /* adc */
            if(conds) {
                gen_helper_adc_cc(t0, t0, t1);
            } else {
                gen_adc(t0, t1);
            }
            break;
        case 11: /* sbc */
            if(conds) {
                gen_helper_sbc_cc(t0, t0, t1);
            } else {
                gen_sub_carry(t0, t0, t1);
            }
            break;
        case 13: /* sub */
            if(conds) {
                gen_helper_sub_cc(t0, t0, t1);
            } else {
                tcg_gen_sub_i32(t0, t0, t1);
            }
            break;
        case 14: /* rsb */
            if(conds) {
                gen_helper_sub_cc(t0, t1, t0);
            } else {
                tcg_gen_sub_i32(t0, t1, t0);
            }
            break;
        default: /* 5, 6, 7, 9, 12, 15. */
            return 1;
    }
    if(logic_cc) {
        gen_logic_CC(t0);
        if(shifter_out) {
            gen_set_CF_bit31(t1);
        }
    }
    return 0;
}

#include "translate_lob.h"
#include "translate_mve.h"

/* Translate a 32-bit thumb instruction.  Returns nonzero if the instruction
   is not legal.  */
static int disas_thumb2_insn(CPUState *env, DisasContext *s, uint16_t insn_hw1)
{
    uint32_t insn, imm, shift, offset;
    uint32_t rd, rn, rm, rs;
    TCGv tmp;
    TCGv tmp2;
    TCGv tmp3;
    TCGv addr;
    TCGv_i64 tmp64;
    int op;
    int op1, op4;
    int shiftop;
    int conds;
    int logic_cc;
    target_ulong current_pc = s->base.pc;
#ifndef TARGET_PROTO_ARM_M
    if(!arm_feature(env, ARM_FEATURE_THUMB2)) {
        /* Thumb-1 cores may need to treat bl and blx as a pair of
           16-bit instructions to get correct prefetch abort behavior.  */
        insn = insn_hw1;
        if((insn & (1 << 12)) == 0) {
            ARCH(5);
            /* Second half of blx.  */
            offset = ((insn & 0x7ff) << 1);
            tmp = load_reg(s, 14);
            tcg_gen_addi_i32(tmp, tmp, offset);
            tcg_gen_andi_i32(tmp, tmp, 0xfffffffc);

            tmp2 = tcg_temp_new_i32();
            tcg_gen_movi_i32(tmp2, s->base.pc | 1);
            store_reg(s, 14, tmp2);
            /* Branch with link - new stack frame */
            gen_bx(s, tmp, STACK_FRAME_ADD);
            return 0;
        }
        if(insn & (1 << 11)) {
            /* Second half of bl.  */
            offset = ((insn & 0x7ff) << 1) | 1;
            tmp = load_reg(s, 14);
            tcg_gen_addi_i32(tmp, tmp, offset);

            tmp2 = tcg_temp_new_i32();
            tcg_gen_movi_i32(tmp2, s->base.pc | 1);
            store_reg(s, 14, tmp2);
            /* Branch with link - new stack frame */
            gen_bx(s, tmp, STACK_FRAME_ADD);
            return 0;
        }
        if((s->base.pc & ~TARGET_PAGE_MASK) == 0) {
            /* Instruction spans a page boundary.  Implement it as two
               16-bit instructions in case the second half causes an
               prefetch abort.  */
            offset = ((int32_t)insn << 21) >> 9;
            tcg_gen_movi_i32(cpu_R[14], s->base.pc + 2 + offset);
            return 0;
        }
        /* Fall through to 32-bit decode.  */
    }
#endif

    insn = lduw_code(s->base.pc);
    s->base.pc += 2;
    insn |= (uint32_t)insn_hw1 << 16;

    if((insn & 0xf800e800) != 0xf000e800) {
        ARCH(6T2);
    }

    rn = (insn >> 16) & 0xf;
    rs = (insn >> 12) & 0xf;
    rd = (insn >> 8) & 0xf;
    rm = insn & 0xf;
    switch((insn >> 25) & 0xf) {
        case 0:
        case 1:
        case 2:
        case 3:
            /* 16-bit instructions.  Should never happen.  */
            abort();
            break;
        case 4:
            if(insn & (1 << 22)) {
                /* Other load/store, table branch.  */
#ifdef TARGET_PROTO_ARM_M
                if(insn == 0xe97fe97f) {
                    /* Secure Gateway */
                    gen_sync_pc(s);
                    gen_helper_v8m_sg(cpu_env);
                } else if(insn & 0x01200000) {
#else
                //  This line is duplicated to satisfy the formatter
                if(insn & 0x01200000) {
#endif
                    /* Load/store doubleword.  */
                    gen_set_pc(current_pc);
                    if(rn == 15) {
                        addr = tcg_temp_new_i32();
                        tcg_gen_movi_i32(addr, s->base.pc & ~3);
                    } else {
                        addr = load_reg(s, rn);
                    }
                    offset = (insn & 0xff) * 4;
                    if((insn & (1 << 23)) == 0) {
                        offset = -offset;
                    }
                    if(insn & (1 << 24)) {
                        tcg_gen_addi_i32(addr, addr, offset);
                        offset = 0;
                    }
                    if(insn & (1 << 20)) {
                        /* ldrd */
                        tmp = gen_ld32(addr, context_to_mmu_index(s));
                        store_reg(s, rs, tmp);
                        tcg_gen_addi_i32(addr, addr, 4);
                        tmp = gen_ld32(addr, context_to_mmu_index(s));
                        store_reg(s, rd, tmp);
                    } else {
                        /* strd */
                        tmp = load_reg(s, rs);
                        gen_st32(tmp, addr, context_to_mmu_index(s));
                        tcg_gen_addi_i32(addr, addr, 4);
                        tmp = load_reg(s, rd);
                        gen_st32(tmp, addr, context_to_mmu_index(s));
                    }
                    if(insn & (1 << 21)) {
                        /* Base writeback.  */
                        if(rn == 15) {
                            goto illegal_op;
                        }
                        tcg_gen_addi_i32(addr, addr, offset - 4);
                        store_reg(s, rn, addr);
                    } else {
                        tcg_temp_free_i32(addr);
                    }
                } else if((insn & (1 << 23)) == 0) {

                    if(rs == 15) {
#ifdef TARGET_PROTO_ARM_M
                        if(!(insn & (1 << 20)) && arm_feature(env, ARM_FEATURE_V8)) {
                            /* TT, TTT, TTA, TTAT */
                            TCGv_i32 addr, op, ttresp;
                            uint32_t at;

                            /* UNPREDICTABLE cases */
                            if((insn & 0x3f) || rd == 13 || rd == 15 || rn == 15) {
                                goto illegal_op;
                            }

                            at = extract32(insn, 6, 2);

                            /* TTA and TTAT are UNDEFINED if used from Non-Secure state. */
                            if(s->ns && (at & 0b10)) {
                                goto illegal_op;
                            }

                            addr = load_reg(s, rn);
                            op = tcg_const_i32(at);
                            ttresp = tcg_temp_new_i32();
                            gen_helper_v8m_tt(ttresp, cpu_env, addr, op);
                            tcg_temp_free_i32(addr);
                            tcg_temp_free_i32(op);
                            store_reg(s, rd, ttresp);
                            break;
                        }
#endif
                        goto illegal_op;
                    }
                    /* Load/store exclusive word.  */
                    addr = tcg_temp_local_new();
                    load_reg_var(s, addr, rn);
                    tcg_gen_addi_i32(addr, addr, (insn & 0xff) << 2);
                    if(insn & (1 << 20)) {
                        gen_load_exclusive(s, rs, 15, addr, 2);
                    } else {
                        gen_store_exclusive(s, rd, rs, 15, addr, 2);
                    }
                    tcg_temp_free(addr);
                } else if(((insn >> 5) & 0x7) == 0) {
                    /* Table Branch.  */
                    if(rn == 15) {
                        addr = tcg_temp_new_i32();
                        tcg_gen_movi_i32(addr, s->base.pc);
                    } else {
                        addr = load_reg(s, rn);
                    }
                    tmp = load_reg(s, rm);
                    tcg_gen_add_i32(addr, addr, tmp);
                    if(insn & (1 << 4)) {
                        /* tbh */
                        tcg_gen_add_i32(addr, addr, tmp);
                        tcg_temp_free_i32(tmp);
                        tmp = gen_ld16u(addr, context_to_mmu_index(s));
                    } else { /* tbb */
                        tcg_temp_free_i32(tmp);
                        tmp = gen_ld8u(addr, context_to_mmu_index(s));
                    }
                    tcg_temp_free_i32(addr);
                    tcg_gen_shli_i32(tmp, tmp, 1);
                    tcg_gen_addi_i32(tmp, tmp, s->base.pc);
                    store_reg(s, 15, tmp);
                } else {
                    /* Load/store exclusive byte/halfword/doubleword.  */
                    ARCH(7);
                    op = (insn >> 4) & 0x3;
                    if(op == 2) {
                        ARCH(8);
                        /* LDA(EX)/STL(EX) (and variants -B,-H)
                           LDAEX is LDREX + sync memory barrier, so might require a translation block to be finished
                           For LDA (and EX) we don't implement the memory barrier. This is fine in simple single-CPU
                           scenarios but might cause problems if used with multiple CPUs.
                         */
                        if(((insn >> 6) & 1) == 0) {
                            /* LDA/STL (and variants -B,-H) stub
                               They are likely just Load/Store + memory barrier
                             */
                        }
                    }
                    addr = tcg_temp_local_new();
                    load_reg_var(s, addr, rn);
                    if(insn & (1 << 20)) {
                        gen_load_exclusive(s, rs, rd, addr, op);
                    } else {
                        gen_store_exclusive(s, rm, rs, rd, addr, op);
                    }
                    tcg_temp_free(addr);
                }
            } else {
                /* Load/store multiple, RFE, SRS.  */
                gen_set_pc(current_pc);
                if(((insn >> 23) & 1) == ((insn >> 24) & 1)) {
                    /* Not available in user mode.  */
                    if(s->user) {
                        goto illegal_op;
                    }
                    if(insn & (1 << 20)) {
                        /* rfe */
                        addr = load_reg(s, rn);
                        if((insn & (1 << 24)) == 0) {
                            tcg_gen_addi_i32(addr, addr, -8);
                        }
                        /* Load PC into tmp and CPSR into tmp2.  */
                        tmp = gen_ld32(addr, context_to_mmu_index(s));
                        tcg_gen_addi_i32(addr, addr, 4);
                        tmp2 = gen_ld32(addr, context_to_mmu_index(s));
                        if(insn & (1 << 21)) {
                            /* Base writeback.  */
                            if(insn & (1 << 24)) {
                                tcg_gen_addi_i32(addr, addr, 4);
                            } else {
                                tcg_gen_addi_i32(addr, addr, -4);
                            }
                            store_reg(s, rn, addr);
                        } else {
                            tcg_temp_free_i32(addr);
                        }
                        gen_rfe(s, tmp, tmp2);
                    } else {
                        /* srs */
                        op = (insn & 0x1f);
                        addr = tcg_temp_new_i32();
                        tmp = tcg_const_i32(op);
                        gen_helper_get_r13_banked(addr, cpu_env, tmp);
                        tcg_temp_free_i32(tmp);
                        if((insn & (1 << 24)) == 0) {
                            tcg_gen_addi_i32(addr, addr, -8);
                        }
                        tmp = load_reg(s, 14);
                        gen_st32(tmp, addr, context_to_mmu_index(s));
                        tcg_gen_addi_i32(addr, addr, 4);
                        tmp = load_cpu_field(spsr);
                        gen_st32(tmp, addr, context_to_mmu_index(s));
                        if(insn & (1 << 21)) {
                            if((insn & (1 << 24)) == 0) {
                                tcg_gen_addi_i32(addr, addr, -4);
                            } else {
                                tcg_gen_addi_i32(addr, addr, 4);
                            }
                            tmp = tcg_const_i32(op);
                            gen_helper_set_r13_banked(cpu_env, tmp, addr);
                            tcg_temp_free_i32(tmp);
                        } else {
                            tcg_temp_free_i32(addr);
                        }
                    }
                } else {
                    int i, loaded_base = 0;
                    TCGv loaded_var;
                    /* Load/store multiple.  */
                    addr = load_reg(s, rn);
                    offset = 0;
                    for(i = 0; i < 16; i++) {
                        if(insn & (1 << i)) {
                            offset += 4;
                        }
                    }
                    if(insn & (1 << 24)) {
                        tcg_gen_addi_i32(addr, addr, -offset);
                    }

                    TCGV_UNUSED(loaded_var);
                    for(i = 0; i < 16; i++) {
                        if((insn & (1 << i)) == 0) {
                            continue;
                        }
                        if(insn & (1 << 20)) {
                            /* Load.  */
                            tmp = gen_ld32(addr, context_to_mmu_index(s));
                            if(i == 15) {
                                /* pop - loading PC form stack */
                                gen_bx(s, tmp, STACK_FRAME_POP);
                            } else if(i == rn) {
                                loaded_var = tmp;
                                loaded_base = 1;
                            } else {
                                store_reg(s, i, tmp);
                            }
                        } else {
                            /* Store.  */
                            tmp = load_reg(s, i);
                            gen_st32(tmp, addr, context_to_mmu_index(s));
                        }
                        tcg_gen_addi_i32(addr, addr, 4);
                    }
                    if(loaded_base) {
                        store_reg(s, rn, loaded_var);
                    }
                    if(insn & (1 << 21)) {
                        /* Base register writeback.  */
                        if(insn & (1 << 24)) {
                            tcg_gen_addi_i32(addr, addr, -offset);
                        }
                        /* Fault if writeback register is in register list.  */
                        if(insn & (1 << rn)) {
                            goto illegal_op;
                        }
                        store_reg(s, rn, addr);
                    } else {
                        tcg_temp_free_i32(addr);
                    }
                }
            }
            break;
        case 5:

            op = (insn >> 21) & 0xf;
            if(op == 6) {
                /* Halfword pack.  */
                tmp = load_reg(s, rn);
                tmp2 = load_reg(s, rm);
                shift = ((insn >> 10) & 0x1c) | ((insn >> 6) & 0x3);
                if(insn & (1 << 5)) {
                    /* pkhtb */
                    if(shift == 0) {
                        shift = 31;
                    }
                    tcg_gen_sari_i32(tmp2, tmp2, shift);
                    tcg_gen_andi_i32(tmp, tmp, 0xffff0000);
                    tcg_gen_ext16u_i32(tmp2, tmp2);
                } else {
                    /* pkhbt */
                    if(shift) {
                        tcg_gen_shli_i32(tmp2, tmp2, shift);
                    }
                    tcg_gen_ext16u_i32(tmp, tmp);
                    tcg_gen_andi_i32(tmp2, tmp2, 0xffff0000);
                }
                tcg_gen_or_i32(tmp, tmp, tmp2);
                tcg_temp_free_i32(tmp2);
                store_reg(s, rd, tmp);
            } else {
                /* Data processing register constant shift.  */
                if(rn == 15) {
                    tmp = tcg_temp_new_i32();
                    tcg_gen_movi_i32(tmp, 0);
                } else {
                    tmp = load_reg(s, rn);
                }
                tmp2 = load_reg(s, rm);

                shiftop = (insn >> 4) & 3;
                shift = ((insn >> 6) & 3) | ((insn >> 10) & 0x1c);
                conds = (insn & (1 << 20)) != 0;
                logic_cc = (conds && thumb2_logic_op(op));
                gen_arm_shift_im(tmp2, shiftop, shift, logic_cc);
                if(gen_thumb2_data_op(s, op, conds, 0, tmp, tmp2)) {
                    goto illegal_op;
                }
                tcg_temp_free_i32(tmp2);
                if(rd != 15) {
                    store_reg(s, rd, tmp);
                } else {
                    tcg_temp_free_i32(tmp);
                }
            }
            break;
        case 13: /* Misc data processing.  */
            op = ((insn >> 22) & 6) | ((insn >> 7) & 1);
            if(op < 4 && (insn & 0xf000) != 0xf000) {
                goto illegal_op;
            }
            switch(op) {
                case 0: /* Register controlled shift.  */
                    tmp = load_reg(s, rn);
                    tmp2 = load_reg(s, rm);
                    if((insn & 0x70) != 0) {
                        goto illegal_op;
                    }
                    op = (insn >> 21) & 3;
                    logic_cc = (insn & (1 << 20)) != 0;
                    gen_arm_shift_reg(tmp, op, tmp2, logic_cc);
                    if(logic_cc) {
                        gen_logic_CC(tmp);
                    }
                    store_reg_bx(env, s, rd, tmp);
                    break;
                case 1: /* Sign/zero extend.  */
                    tmp = load_reg(s, rm);
                    shift = (insn >> 4) & 3;
                    /* ??? In many cases it's not necessary to do a
                       rotate, a shift is sufficient.  */
                    if(shift != 0) {
                        tcg_gen_rotri_i32(tmp, tmp, shift * 8);
                    }
                    op = (insn >> 20) & 7;
                    switch(op) {
                        case 0:
                            gen_sxth(tmp);
                            break;
                        case 1:
                            gen_uxth(tmp);
                            break;
                        case 2:
                            gen_sxtb16(tmp);
                            break;
                        case 3:
                            gen_uxtb16(tmp);
                            break;
                        case 4:
                            gen_sxtb(tmp);
                            break;
                        case 5:
                            gen_uxtb(tmp);
                            break;
                        default:
                            goto illegal_op;
                    }
                    if(rn != 15) {
                        tmp2 = load_reg(s, rn);
                        if((op >> 1) == 1) {
                            gen_add16(tmp, tmp2);
                        } else {
                            tcg_gen_add_i32(tmp, tmp, tmp2);
                            tcg_temp_free_i32(tmp2);
                        }
                    }
                    store_reg(s, rd, tmp);
                    break;
                case 2: /* SIMD add/subtract.  */
                    op = (insn >> 20) & 7;
                    shift = (insn >> 4) & 7;
                    if((op & 3) == 3 || (shift & 3) == 3) {
                        goto illegal_op;
                    }
                    tmp = load_reg(s, rn);
                    tmp2 = load_reg(s, rm);
                    gen_thumb2_parallel_addsub(op, shift, tmp, tmp2);
                    tcg_temp_free_i32(tmp2);
                    store_reg(s, rd, tmp);
                    break;
                case 3: /* Other data processing.  */
                    op = ((insn >> 17) & 0x38) | ((insn >> 4) & 7);
                    if(op < 4) {
                        /* Saturating add/subtract.  */
                        tmp = load_reg(s, rn);
                        tmp2 = load_reg(s, rm);
                        if(op & 1) {
                            gen_helper_double_saturate(tmp, tmp);
                        }
                        if(op & 2) {
                            gen_helper_sub_saturate(tmp, tmp2, tmp);
                        } else {
                            gen_helper_add_saturate(tmp, tmp, tmp2);
                        }
                        tcg_temp_free_i32(tmp2);
                    } else {
                        tmp = load_reg(s, rn);
                        switch(op) {
                            case 0x0a: /* rbit */
                                gen_helper_rbit(tmp, tmp);
                                break;
                            case 0x08: /* rev */
                                tcg_gen_bswap32_i32(tmp, tmp);
                                break;
                            case 0x09: /* rev16 */
                                gen_rev16(tmp);
                                break;
                            case 0x0b: /* revsh */
                                gen_revsh(tmp);
                                break;
                            case 0x10: /* sel */
                                tmp2 = load_reg(s, rm);
                                tmp3 = tcg_temp_new_i32();
                                tcg_gen_ld_i32(tmp3, cpu_env, offsetof(CPUState, GE));
                                gen_helper_sel_flags(tmp, tmp3, tmp, tmp2);
                                tcg_temp_free_i32(tmp3);
                                tcg_temp_free_i32(tmp2);
                                break;
                            case 0x18: /* clz */
                                gen_helper_clz(tmp, tmp);
                                break;
                            default:
                                goto illegal_op;
                        }
                    }
                    store_reg(s, rd, tmp);
                    break;
                case 4:  //  32-bit multiply.  Sum of absolute differences.
                case 5:
                    op = (insn >> 4) & 0xf;
                    tmp = load_reg(s, rn);
                    tmp2 = load_reg(s, rm);
                    switch((insn >> 20) & 7) {
                        case 0: /* 32 x 32 -> 32 */
                            tcg_gen_mul_i32(tmp, tmp, tmp2);
                            tcg_temp_free_i32(tmp2);
                            if(rs != 15) {
                                tmp2 = load_reg(s, rs);
                                if(op) {
                                    tcg_gen_sub_i32(tmp, tmp2, tmp);
                                } else {
                                    tcg_gen_add_i32(tmp, tmp, tmp2);
                                }
                                tcg_temp_free_i32(tmp2);
                            }
                            break;
                        case 1: /* 16 x 16 -> 32 */
                            gen_mulxy(tmp, tmp2, op & 2, op & 1);
                            tcg_temp_free_i32(tmp2);
                            if(rs != 15) {
                                tmp2 = load_reg(s, rs);
                                gen_helper_add_setq(tmp, tmp, tmp2);
                                tcg_temp_free_i32(tmp2);
                            }
                            break;
                        case 2: /* Dual multiply add.  */
                        case 4: /* Dual multiply subtract.  */
                            if(op) {
                                gen_swap_half(tmp2);
                            }
                            gen_smul_dual(tmp, tmp2);
                            if(insn & (1 << 22)) {
                                /* This subtraction cannot overflow. */
                                tcg_gen_sub_i32(tmp, tmp, tmp2);
                            } else {
                                /* This addition cannot overflow 32 bits;
                                 * however it may overflow considered as a signed
                                 * operation, in which case we must set the Q flag.
                                 */
                                gen_helper_add_setq(tmp, tmp, tmp2);
                            }
                            tcg_temp_free_i32(tmp2);
                            if(rs != 15) {
                                tmp2 = load_reg(s, rs);
                                gen_helper_add_setq(tmp, tmp, tmp2);
                                tcg_temp_free_i32(tmp2);
                            }
                            break;
                        case 3: /* 32 * 16 -> 32msb */
                            if(op) {
                                tcg_gen_sari_i32(tmp2, tmp2, 16);
                            } else {
                                gen_sxth(tmp2);
                            }
                            tmp64 = gen_muls_i64_i32(tmp, tmp2);
                            tcg_gen_shri_i64(tmp64, tmp64, 16);
                            tmp = tcg_temp_new_i32();
                            tcg_gen_trunc_i64_i32(tmp, tmp64);
                            tcg_temp_free_i64(tmp64);
                            if(rs != 15) {
                                tmp2 = load_reg(s, rs);
                                gen_helper_add_setq(tmp, tmp, tmp2);
                                tcg_temp_free_i32(tmp2);
                            }
                            break;
                        case 5:  //  32 * 32 -> 32msb (SMMUL, SMMLA, SMMLS)
                        case 6:
                            tmp64 = gen_muls_i64_i32(tmp, tmp2);
                            if(rs != 15) {
                                tmp = load_reg(s, rs);
                                if(insn & (1 << 20)) {
                                    tmp64 = gen_addq_msw(tmp64, tmp);
                                } else {
                                    tmp64 = gen_subq_msw(tmp64, tmp);
                                }
                            }
                            if(insn & (1 << 4)) {
                                tcg_gen_addi_i64(tmp64, tmp64, 0x80000000u);
                            }
                            tcg_gen_shri_i64(tmp64, tmp64, 32);
                            tmp = tcg_temp_new_i32();
                            tcg_gen_trunc_i64_i32(tmp, tmp64);
                            tcg_temp_free_i64(tmp64);
                            break;
                        case 7: /* Unsigned sum of absolute differences.  */
                            gen_helper_usad8(tmp, tmp, tmp2);
                            tcg_temp_free_i32(tmp2);
                            if(rs != 15) {
                                tmp2 = load_reg(s, rs);
                                tcg_gen_add_i32(tmp, tmp, tmp2);
                                tcg_temp_free_i32(tmp2);
                            }
                            break;
                    }
                    store_reg(s, rd, tmp);
                    break;
                case 6:  //  64-bit multiply, Divide.
                case 7:
                    op = ((insn >> 4) & 0xf) | ((insn >> 16) & 0x70);
                    tmp = load_reg(s, rn);
                    tmp2 = load_reg(s, rm);
                    if((op & 0x50) == 0x10) {
                        /* sdiv, udiv */
                        if(!arm_feature(env, ARM_FEATURE_THUMB_DIV)) {
                            goto illegal_op;
                        }
                        if(op & 0x20) {
                            gen_helper_udiv(tmp, tmp, tmp2);
                        } else {
                            gen_helper_sdiv(tmp, tmp, tmp2);
                        }
                        tcg_temp_free_i32(tmp2);
                        store_reg(s, rd, tmp);
                    } else if((op & 0xe) == 0xc) {
                        /* Dual multiply accumulate long.  */
                        if(op & 1) {
                            gen_swap_half(tmp2);
                        }
                        gen_smul_dual(tmp, tmp2);
                        if(op & 0x10) {
                            tcg_gen_sub_i32(tmp, tmp, tmp2);
                        } else {
                            tcg_gen_add_i32(tmp, tmp, tmp2);
                        }
                        tcg_temp_free_i32(tmp2);
                        /* BUGFIX */
                        tmp64 = tcg_temp_new_i64();
                        tcg_gen_ext_i32_i64(tmp64, tmp);
                        tcg_temp_free_i32(tmp);
                        gen_addq(s, tmp64, rs, rd);
                        gen_storeq_reg(s, rs, rd, tmp64);
                        tcg_temp_free_i64(tmp64);
                    } else {
                        if(op & 0x20) {
                            /* Unsigned 64-bit multiply  */
                            tmp64 = gen_mulu_i64_i32(tmp, tmp2);
                        } else {
                            if(op & 8) {
                                /* smlalxy */
                                gen_mulxy(tmp, tmp2, op & 2, op & 1);
                                tcg_temp_free_i32(tmp2);
                                tmp64 = tcg_temp_new_i64();
                                tcg_gen_ext_i32_i64(tmp64, tmp);
                                tcg_temp_free_i32(tmp);
                            } else {
                                /* Signed 64-bit multiply  */
                                tmp64 = gen_muls_i64_i32(tmp, tmp2);
                            }
                        }
                        if(op & 4) {
                            /* umaal */
                            gen_addq_lo(s, tmp64, rs);
                            gen_addq_lo(s, tmp64, rd);
                        } else if(op & 0x40) {
                            /* 64-bit accumulate.  */
                            gen_addq(s, tmp64, rs, rd);
                        }
                        gen_storeq_reg(s, rs, rd, tmp64);
                        tcg_temp_free_i64(tmp64);
                    }
                    break;
            }
            break;
        case 6:
        case 7:
        case 14:
        case 15:
            /* Coprocessor.  */
            /*
               6, 14 are MRRC/MCRR T1,T2
               7, 15 are MCR/MRC T1,T2
             */

            op1 = (insn >> 21) & 0xf;
            op4 = (insn >> 6) & 0x7;

            if(arm_feature(env, ARM_FEATURE_V8) && is_insn_vstrw(insn)) {
                ARCH(MVE);
                return trans_vstrw(s, insn);
            } else if(((insn >> 24) & 3) == 3) {
                /* Translate into the equivalent ARM encoding.  */
                insn = (insn & 0xe2ffffff) | ((insn & (1 << 28)) >> 4) | (1 << 28);
                if(disas_neon_data_insn(env, s, insn)) {
                    goto illegal_op;
                }
            } else if(((insn >> 25) & 0xf) == 0b0110 && (op1 & 0b1101) == 0b0001 && (op4 >> 2) == 0) {
#ifdef TARGET_PROTO_ARM_M
                /* VLSTM, VLLDM */
                ARCH(8);
                if(s->ns) {
                    goto illegal_op;
                }
                bool lowRegsOnly = ((insn >> 7) & 1) == 0;
                if(!lowRegsOnly) {
                    ARCH(8_1M);
                }
                /* Sync PC to restore instruction count if an exceptcion is raised at runtime in the helper */
                int op2 = (insn >> 20) & 1;
                gen_sync_pc(s);
                if(op2 == 0) {
                    gen_helper_v8m_vlstm(cpu_env, rn, lowRegsOnly);
                } else {
                    gen_helper_v8m_vlldm(cpu_env, rn, lowRegsOnly);
                }
#else
                goto illegal_op;
#endif
            } else {
                gen_set_pc(current_pc);

                /* MCR/MRC/MRRC/MCRR (Thumb) encoding  */
                if(disas_coproc_insn(env, s, insn)) {
                    goto illegal_op;
                }
            }
            break;
        case 8:
        case 9:
        case 10:
        case 11:
            if(is_insn_wls(insn)) {
                ARCH(8);
                return trans_wls(s, insn);
            } else if(is_insn_dls(insn)) {
                ARCH(8);
                return trans_dls(s, insn);
            } else if(is_insn_le(insn)) {
                ARCH(8);
                return trans_le(s, insn);
            } else if(insn & (1 << 15)) {
                /* Branches, misc control.  */
                if(insn & 0x5000) {
                    /* Unconditional branch.  */
                    /* signextend(hw1[10:0]) -> offset[:12].  */
                    offset = ((int32_t)insn << 5) >> 9 & ~(int32_t)0xfff;
                    /* hw1[10:0] -> offset[11:1].  */
                    offset |= (insn & 0x7ff) << 1;
                    /* (~hw2[13, 11] ^ offset[24]) -> offset[23,22]
                       offset[24:22] already have the same value because of the
                       sign extension above.  */
                    offset ^= ((~insn) & (1 << 13)) << 10;
                    offset ^= ((~insn) & (1 << 11)) << 11;

                    if(insn & (1 << 14)) {
                        /* Branch and link.  */
                        tcg_gen_movi_i32(cpu_R[14], s->base.pc | 1);
                    }

                    offset += s->base.pc;
                    if(insn & (1 << 12)) {
                        /* b/bl */
                        /* Check if this jump is b or bl */
                        gen_jmp(s, offset, insn & (1 << 14) ? STACK_FRAME_ADD : STACK_FRAME_NO_CHANGE);
                    } else {
                        /* blx */
                        offset &= ~(uint32_t)2;
                        /* thumb2 bx, no need to check */
                        /* Branch with link - new stack frame */
                        gen_bx_im(s, offset, STACK_FRAME_ADD);
                    }
                } else if(((insn >> 23) & 7) == 7) {
                    /* Misc control */
                    if(insn & (1 << 13)) {
                        goto illegal_op;
                    }

                    if(insn & (1 << 26)) {
                        /* Secure monitor call (v6Z) */
                        goto illegal_op; /* not implemented.  */
                    } else {
                        op = (insn >> 20) & 7;
                        switch(op) {
                            case 0: /* msr cpsr.  */
#ifdef TARGET_PROTO_ARM_M
                                tmp = load_reg(s, rn);
                                addr = tcg_const_i32(insn & 0xff);
                                gen_helper_v7m_msr(cpu_env, addr, tmp);
                                tcg_temp_free_i32(addr);
                                tcg_temp_free_i32(tmp);
                                gen_lookup_tb(s);
                                break;
#endif
                            /* fall through */
                            case 1: /* msr spsr.  */
#ifdef TARGET_PROTO_ARM_M
                                goto illegal_op;
#endif
                                tmp = load_reg(s, rn);
                                if(gen_set_psr(s, msr_mask(env, s, (insn >> 8) & 0xf, op == 1), op == 1, tmp)) {
                                    goto illegal_op;
                                }
                                break;
                            case 2: /* cps, nop-hint.  */
                                if(((insn >> 8) & 7) == 0) {
                                    gen_nop_hint(s, insn & 0xff);
                                }
                                /* Implemented as NOP in user mode.  */
                                if(s->user) {
                                    break;
                                }
                                offset = 0;
                                imm = 0;
                                if(insn & (1 << 10)) {
                                    if(insn & (1 << 7)) {
                                        offset |= CPSR_A;
                                    }
                                    if(insn & (1 << 6)) {
                                        offset |= CPSR_I;
                                    }
                                    if(insn & (1 << 5)) {
                                        offset |= CPSR_F;
                                    }
                                    if(insn & (1 << 9)) {
                                        imm = CPSR_A | CPSR_I | CPSR_F;
                                    }
                                }
                                if(insn & (1 << 8)) {
                                    offset |= 0x1f;
                                    imm |= (insn & 0x1f);
                                }
                                if(offset) {
                                    gen_set_psr_im(s, offset, 0, imm);
                                }
                                break;
                            case 3: /* Special control operations.  */
                                ARCH(7);
                                op = (insn >> 4) & 0xf;
                                switch(op) {
                                    case 2: /* clrex */
                                        gen_clrex(s);
                                        break;
                                    case 4: /* dsb */
                                    case 5: /* dmb */
                                        gen_dxb(s);
                                        break;
                                    case 6: /* isb */
                                        gen_isb(s);
                                        break;
                                    default:
                                        goto illegal_op;
                                }
                                break;
                            case 4: /* bxj */
                                /* Trivial implementation equivalent to bx.  */
                                tmp = load_reg(s, rn);
                                gen_bx(s, tmp, rn == 14 ? STACK_FRAME_POP : STACK_FRAME_NO_CHANGE);
                                break;
                            case 5: /* Exception return.  */
                                if(s->user) {
                                    goto illegal_op;
                                }
                                if(rn != 14 || rd != 15) {
                                    goto illegal_op;
                                }
                                tmp = load_reg(s, rn);
                                tcg_gen_subi_i32(tmp, tmp, insn & 0xff);
                                gen_exception_return(s, tmp);
                                break;
                            case 6: /* mrs cpsr.  */
                                tmp = tcg_temp_new_i32();
#ifdef TARGET_PROTO_ARM_M
                                addr = tcg_const_i32(insn & 0xff);
                                gen_helper_v7m_mrs(tmp, cpu_env, addr);
                                tcg_temp_free_i32(addr);
#else
                                gen_helper_cpsr_read(tmp);
#endif
                                store_reg(s, rd, tmp);
                                break;
                            case 7: /* mrs spsr.  */
                                    /* Not accessible in user mode.  */
#ifndef TARGET_PROTO_ARM_M
                                if(s->user)
#endif
                                    goto illegal_op;
                                tmp = load_cpu_field(spsr);
                                store_reg(s, rd, tmp);
                                break;
                        }
                    }
                } else {
                    /* Conditional branch.  */
                    op = (insn >> 22) & 0xf;
                    /* Generate a conditional jump to next instruction.  */
                    s->condlabel = gen_new_label();
                    gen_test_cc(op ^ 1, s->condlabel);
                    s->condjmp = 1;

                    /* offset[11:1] = insn[10:0] */
                    offset = (insn & 0x7ff) << 1;
                    /* offset[17:12] = insn[21:16].  */
                    offset |= (insn & 0x003f0000) >> 4;
                    /* offset[31:20] = insn[26].  */
                    offset |= ((int32_t)((insn << 5) & 0x80000000)) >> 11;
                    /* offset[18] = insn[13].  */
                    offset |= (insn & (1 << 13)) << 5;
                    /* offset[19] = insn[11].  */
                    offset |= (insn & (1 << 11)) << 8;

                    /* jump to the offset */
                    gen_jmp(s, s->base.pc + offset, STACK_FRAME_NO_CHANGE);
                }
            } else {
                /* Data processing immediate.  */
                if(insn & (1 << 25)) {
                    if(insn & (1 << 24)) {
                        if(insn & (1 << 20)) {
                            goto illegal_op;
                        }
                        /* Bitfield/Saturate.  */
                        op = (insn >> 21) & 7;
                        imm = insn & 0x1f;
                        shift = ((insn >> 6) & 3) | ((insn >> 10) & 0x1c);
                        if(rn == 15) {
                            tmp = tcg_temp_new_i32();
                            tcg_gen_movi_i32(tmp, 0);
                        } else {
                            tmp = load_reg(s, rn);
                        }
                        switch(op) {
                            case 2: /* Signed bitfield extract.  */
                                imm++;
                                if(shift + imm > 32) {
                                    goto illegal_op;
                                }
                                if(imm < 32) {
                                    gen_sbfx(tmp, shift, imm);
                                }
                                break;
                            case 6: /* Unsigned bitfield extract.  */
                                imm++;
                                if(shift + imm > 32) {
                                    goto illegal_op;
                                }
                                if(imm < 32) {
                                    gen_ubfx(tmp, shift, (1u << imm) - 1);
                                }
                                break;
                            case 3: /* Bitfield insert/clear.  */
                                if(imm < shift) {
                                    goto illegal_op;
                                }
                                imm = imm + 1 - shift;
                                if(imm != 32) {
                                    tmp2 = load_reg(s, rd);
                                    gen_bfi(tmp, tmp2, tmp, shift, (1u << imm) - 1);
                                    tcg_temp_free_i32(tmp2);
                                }
                                break;
                            case 7:
                                goto illegal_op;
                            default: /* Saturate.  */
                                if(shift) {
                                    if(op & 1) {
                                        tcg_gen_sari_i32(tmp, tmp, shift);
                                    } else {
                                        tcg_gen_shli_i32(tmp, tmp, shift);
                                    }
                                }
                                tmp2 = tcg_const_i32(imm);
                                if(op & 4) {
                                    /* Unsigned.  */
                                    if((op & 1) && shift == 0) {
                                        gen_helper_usat16(tmp, tmp, tmp2);
                                    } else {
                                        gen_helper_usat(tmp, tmp, tmp2);
                                    }
                                } else {
                                    /* Signed.  */
                                    if((op & 1) && shift == 0) {
                                        gen_helper_ssat16(tmp, tmp, tmp2);
                                    } else {
                                        gen_helper_ssat(tmp, tmp, tmp2);
                                    }
                                }
                                tcg_temp_free_i32(tmp2);
                                break;
                        }
                        store_reg(s, rd, tmp);
                    } else {
                        imm = ((insn & 0x04000000) >> 15) | ((insn & 0x7000) >> 4) | (insn & 0xff);
                        if(insn & (1 << 22)) {
                            /* 16-bit immediate.  */
                            imm |= (insn >> 4) & 0xf000;
                            if(insn & (1 << 23)) {
                                /* movt */
                                tmp = load_reg(s, rd);
                                tcg_gen_ext16u_i32(tmp, tmp);
                                tcg_gen_ori_i32(tmp, tmp, imm << 16);
                            } else {
                                /* movw */
                                tmp = tcg_temp_new_i32();
                                tcg_gen_movi_i32(tmp, imm);
                            }
                        } else {
                            /* Add/sub 12-bit immediate.  */
                            if(rn == 15) {
                                offset = s->base.pc & ~(uint32_t)3;
                                if(insn & (1 << 23)) {
                                    offset -= imm;
                                } else {
                                    offset += imm;
                                }
                                tmp = tcg_temp_new_i32();
                                tcg_gen_movi_i32(tmp, offset);
                            } else {
                                tmp = load_reg(s, rn);
                                if(insn & (1 << 23)) {
                                    tcg_gen_subi_i32(tmp, tmp, imm);
                                } else {
                                    tcg_gen_addi_i32(tmp, tmp, imm);
                                }
                            }
                        }
                        store_reg(s, rd, tmp);
                    }
                } else {
                    int shifter_out = 0;
                    /* modified 12-bit immediate.  */
                    shift = ((insn & 0x04000000) >> 23) | ((insn & 0x7000) >> 12);
                    imm = (insn & 0xff);
                    switch(shift) {
                        case 0: /* XY */
                            /* Nothing to do.  */
                            break;
                        case 1: /* 00XY00XY */
                            imm |= imm << 16;
                            break;
                        case 2: /* XY00XY00 */
                            imm |= imm << 16;
                            imm <<= 8;
                            break;
                        case 3: /* XYXYXYXY */
                            imm |= imm << 16;
                            imm |= imm << 8;
                            break;
                        default: /* Rotated constant.  */
                            shift = (shift << 1) | (imm >> 7);
                            imm |= 0x80;
                            imm = imm << (32 - shift);
                            shifter_out = 1;
                            break;
                    }
                    tmp2 = tcg_temp_new_i32();
                    tcg_gen_movi_i32(tmp2, imm);
                    rn = (insn >> 16) & 0xf;
                    if(rn == 15) {
                        tmp = tcg_temp_new_i32();
                        tcg_gen_movi_i32(tmp, 0);
                    } else {
                        tmp = load_reg(s, rn);
                    }
                    op = (insn >> 21) & 0xf;
                    if(gen_thumb2_data_op(s, op, (insn & (1 << 20)) != 0, shifter_out, tmp, tmp2)) {
                        goto illegal_op;
                    }
                    tcg_temp_free_i32(tmp2);
                    rd = (insn >> 8) & 0xf;
                    if(rd != 15) {
                        store_reg(s, rd, tmp);
                    } else {
                        tcg_temp_free_i32(tmp);
                    }
                }
            }
            break;
        case 12: /* Load/store single data item.  */
        {
            int postinc = 0;
            int writeback = 0;
            if((insn & 0x01100000) == 0x01000000) {
                if(disas_neon_ls_insn(env, s, insn)) {
                    goto illegal_op;
                }
                break;
            }
            op = ((insn >> 21) & 3) | ((insn >> 22) & 4);
            if(rs == 15) {
                if(!(insn & (1 << 20))) {
                    goto illegal_op;
                }
                if(op != 2) {
                    /* Byte or halfword load space with dest == r15 : memory hints.
                     * Catch them early so we don't emit pointless addressing code.
                     * This space is a mix of:
                     *  PLD/PLDW/PLI,  which we implement as NOPs (note that unlike
                     *     the ARM encodings, PLDW space doesn't UNDEF for non-v7MP
                     *     cores)
                     *  unallocated hints, which must be treated as NOPs
                     *  UNPREDICTABLE space, which we NOP or UNDEF depending on
                     *     which is easiest for the decoding logic
                     *  Some space which must UNDEF
                     */
                    int op1 = (insn >> 23) & 3;
                    int op2 = (insn >> 6) & 0x3f;
                    if(op & 2) {
                        goto illegal_op;
                    }
                    if(rn == 15) {
                        /* UNPREDICTABLE or unallocated hint */
                        return 0;
                    }
                    if(op1 & 1) {
                        return 0; /* PLD* or unallocated hint */
                    }
                    if((op2 == 0) || ((op2 & 0x3c) == 0x30)) {
                        return 0; /* PLD* or unallocated hint */
                    }
                    /* UNDEF space, or an UNPREDICTABLE */
                    return 1;
                }
            }

            MMUMode mode = context_to_mmu_mode(s);
            if(rn == 15) {
                addr = tcg_temp_new_i32();
                /* PC relative.  */
                /* s->base.pc has already been incremented by 4.  */
                imm = s->base.pc & 0xfffffffc;
                if(insn & (1 << 23)) {
                    imm += insn & 0xfff;
                } else {
                    imm -= insn & 0xfff;
                }
                tcg_gen_movi_i32(addr, imm);
            } else {
                addr = load_reg(s, rn);
                if(insn & (1 << 23)) {
                    /* Positive offset.  */
                    imm = insn & 0xfff;
                    tcg_gen_addi_i32(addr, addr, imm);
                } else {
                    imm = insn & 0xff;
                    switch((insn >> 8) & 0xf) {
                        case 0x0: /* Shifted Register.  */
                            shift = (insn >> 4) & 0xf;
                            if(shift > 3) {
                                tcg_temp_free_i32(addr);
                                goto illegal_op;
                            }
                            tmp = load_reg(s, rm);
                            if(shift) {
                                tcg_gen_shli_i32(tmp, tmp, shift);
                            }
                            tcg_gen_add_i32(addr, addr, tmp);
                            tcg_temp_free_i32(tmp);
                            break;
                        case 0xc: /* Negative offset.  */
                            tcg_gen_addi_i32(addr, addr, -imm);
                            break;
                        case 0xe: /* User privilege.  */
                            tcg_gen_addi_i32(addr, addr, imm);
                            mode.user = true;
                            break;
                        case 0x9: /* Post-decrement.  */
                            imm = -imm;
                        /* Fall through.  */
                        case 0xb: /* Post-increment.  */
                            postinc = 1;
                            writeback = 1;
                            break;
                        case 0xd: /* Pre-decrement.  */
                            imm = -imm;
                        /* Fall through.  */
                        case 0xf: /* Pre-increment.  */
                            tcg_gen_addi_i32(addr, addr, imm);
                            writeback = 1;
                            break;
                        default:
                            tcg_temp_free_i32(addr);
                            goto illegal_op;
                    }
                }
            }
            if(insn & (1 << 20)) {
                /* Load.  */
                switch(op) {
                    case 0:
                        tmp = gen_ld8u(addr, mode.index);
                        break;
                    case 4:
                        tmp = gen_ld8s(addr, mode.index);
                        break;
                    case 1:
                        tmp = gen_ld16u(addr, mode.index);
                        break;
                    case 5:
                        tmp = gen_ld16s(addr, mode.index);
                        break;
                    case 2:
                        tmp = gen_ld32(addr, mode.index);
                        break;
                    default:
                        tcg_temp_free_i32(addr);
                        goto illegal_op;
                }
                if(rs == 15) {
                    /* Stack pop - loading PC form stack
                     * Local jump - SP is not used
                     */
                    gen_bx(s, tmp, rn == 13 ? STACK_FRAME_POP : STACK_FRAME_NO_CHANGE);
                } else {
                    store_reg(s, rs, tmp);
                }
            } else {
                /* Store.  */
                tmp = load_reg(s, rs);
                switch(op) {
                    case 0:
                        gen_st8(tmp, addr, mode.index);
                        break;
                    case 1:
                        gen_st16(tmp, addr, mode.index);
                        break;
                    case 2:
                        gen_st32(tmp, addr, mode.index);
                        break;
                    default:
                        tcg_temp_free_i32(addr);
                        goto illegal_op;
                }
            }
            if(postinc) {
                tcg_gen_addi_i32(addr, addr, imm);
            }
            if(writeback) {
                store_reg(s, rn, addr);
            } else {
                tcg_temp_free_i32(addr);
            }
        } break;
        default:
            goto illegal_op;
    }
    return 0;
illegal_op:
    return 1;
}

static void disas_thumb_insn(CPUState *env, DisasContext *s)
{
    uint32_t val, insn, op, rm, rn, rd, shift, cond;
    int32_t offset;
    int i;
    TCGv tmp;
    TCGv tmp2;
    TCGv addr;
    target_ulong current_pc = s->base.pc;

    if(s->condexec_mask) {
        cond = s->condexec_cond;
        if((cond != 0x0e) && (cond != 0x0f)) { /* Skip conditional when condition is AL. */
            s->condlabel = gen_new_label();
            gen_test_cc(cond ^ 1, s->condlabel);
            s->condjmp = 1;
        }
    }

    insn = lduw_code(s->base.pc);

    if(env->count_opcodes) {
        generate_opcode_count_increment(env, insn);
    }

    s->base.pc += 2;
    switch(insn >> 12) {
        case 0:
        case 1:

            rd = insn & 7;
            op = (insn >> 11) & 3;
            if(op == 3) {
                /* add/subtract */
                rn = (insn >> 3) & 7;
                tmp = load_reg(s, rn);
                if(insn & (1 << 10)) {
                    /* immediate */
                    tmp2 = tcg_temp_new_i32();
                    tcg_gen_movi_i32(tmp2, (insn >> 6) & 7);
                } else {
                    /* reg */
                    rm = (insn >> 6) & 7;
                    tmp2 = load_reg(s, rm);
                }
                if(insn & (1 << 9)) {
                    if(s->condexec_mask) {
                        tcg_gen_sub_i32(tmp, tmp, tmp2);
                    } else {
                        gen_helper_sub_cc(tmp, tmp, tmp2);
                    }
                } else {
                    if(s->condexec_mask) {
                        tcg_gen_add_i32(tmp, tmp, tmp2);
                    } else {
                        gen_helper_add_cc(tmp, tmp, tmp2);
                    }
                }
                tcg_temp_free_i32(tmp2);
                store_reg(s, rd, tmp);
            } else {
                /* shift immediate */
                rm = (insn >> 3) & 7;
                shift = (insn >> 6) & 0x1f;
                tmp = load_reg(s, rm);
                gen_arm_shift_im(tmp, op, shift, s->condexec_mask == 0);
                if(!s->condexec_mask) {
                    gen_logic_CC(tmp);
                }
                store_reg(s, rd, tmp);
            }
            break;
        case 2:
        case 3:
            /* arithmetic large immediate */
            op = (insn >> 11) & 3;
            rd = (insn >> 8) & 0x7;
            if(op == 0) { /* mov */
                tmp = tcg_temp_new_i32();
                tcg_gen_movi_i32(tmp, insn & 0xff);
                if(!s->condexec_mask) {
                    gen_logic_CC(tmp);
                }
                store_reg(s, rd, tmp);
            } else {
                tmp = load_reg(s, rd);
                tmp2 = tcg_temp_new_i32();
                tcg_gen_movi_i32(tmp2, insn & 0xff);
                switch(op) {
                    case 1: /* cmp */
                        gen_helper_sub_cc(tmp, tmp, tmp2);
                        tcg_temp_free_i32(tmp);
                        tcg_temp_free_i32(tmp2);
                        break;
                    case 2: /* add */
                        if(s->condexec_mask) {
                            tcg_gen_add_i32(tmp, tmp, tmp2);
                        } else {
                            gen_helper_add_cc(tmp, tmp, tmp2);
                        }
                        tcg_temp_free_i32(tmp2);
                        store_reg(s, rd, tmp);
                        break;
                    case 3: /* sub */
                        if(s->condexec_mask) {
                            tcg_gen_sub_i32(tmp, tmp, tmp2);
                        } else {
                            gen_helper_sub_cc(tmp, tmp, tmp2);
                        }
                        tcg_temp_free_i32(tmp2);
                        store_reg(s, rd, tmp);
                        break;
                }
            }
            break;
        case 4:
            if(insn & (1 << 11)) {
                rd = (insn >> 8) & 7;
                /* load pc-relative.  Bit 1 of PC is ignored.  */
                val = s->base.pc + 2 + ((insn & 0xff) * 4);
                val &= ~(uint32_t)2;
                addr = tcg_temp_new_i32();
                tcg_gen_movi_i32(addr, val);
                tmp = gen_ld32(addr, context_to_mmu_index(s));
                tcg_temp_free_i32(addr);
                store_reg(s, rd, tmp);
                break;
            }
            if(insn & (1 << 10)) {
                /* data processing extended or blx */
                rd = (insn & 7) | ((insn >> 4) & 8);
                rm = (insn >> 3) & 0xf;
                op = (insn >> 8) & 3;
                switch(op) {
                    case 0: /* add */
                        tmp = load_reg(s, rd);
                        tmp2 = load_reg(s, rm);
                        tcg_gen_add_i32(tmp, tmp, tmp2);
                        tcg_temp_free_i32(tmp2);
                        store_reg(s, rd, tmp);
                        break;
                    case 1: /* cmp */
                        tmp = load_reg(s, rd);
                        tmp2 = load_reg(s, rm);
                        gen_helper_sub_cc(tmp, tmp, tmp2);
                        tcg_temp_free_i32(tmp2);
                        tcg_temp_free_i32(tmp);
                        break;
                    case 2: /* mov/cpy */
                        tmp = load_reg(s, rm);
                        store_reg(s, rd, tmp);
                        break;
                    case 3: /* branch [and link] exchange thumb register */
                        tmp = load_reg(s, rm);
                        bool link = insn & (1 << 7);
                        bool ns = (insn >> 2) & 1;
                        if(ns) {
#ifdef TARGET_PROTO_ARM_M
                            /* BXNS/BLXNS */
                            ARCH(8);
                            //  We need to update PC here to push it correctly on stack in the helper
                            gen_sync_pc(s);

                            /* BLXNS/BXNS is UNDEFINED if executed in Non-secure state, or if the Security Extension is not
                             * implemented. */
                            if(s->ns) {
                                goto illegal_op;
                            }

                            tmp2 = tcg_const_i32(link);
                            gen_helper_v8m_blxns(cpu_env, tmp, tmp2);
                            tcg_temp_free_i32(tmp2);
                            s->base.is_jmp = DISAS_UPDATE;
                            break;
#else
                            goto illegal_op;
#endif
                        }
                        if(link) { /* Link bit set */

                            ARCH(5);
                            val = (uint32_t)s->base.pc | 1;
                            tmp2 = tcg_temp_new_i32();
                            tcg_gen_movi_i32(tmp2, val);
                            store_reg(s, 14, tmp2);
                        }
                        /* already thumb, no need to check */
                        /* Check the link bit if set then add frame (blx),
                           else check if the target register is link then remove frame (bx)
                           else there was no stack change (custom jump) */
                        gen_bx(s, tmp, insn & (1 << 7) ? STACK_FRAME_ADD : (rm == 14 ? STACK_FRAME_POP : STACK_FRAME_NO_CHANGE));
                        break;
                }
                break;
            }

            /* data processing register */
            rd = insn & 7;
            rm = (insn >> 3) & 7;
            op = (insn >> 6) & 0xf;
            if(op == 2 || op == 3 || op == 4 || op == 7) {
                /* the shift/rotate ops want the operands backwards */
                val = rm;
                rm = rd;
                rd = val;
                val = 1;
            } else {
                val = 0;
            }

            if(op == 9) { /* neg */
                tmp = tcg_temp_new_i32();
                tcg_gen_movi_i32(tmp, 0);
            } else if(op != 0xf) { /* mvn doesn't read its first operand */
                tmp = load_reg(s, rd);
            } else {
                TCGV_UNUSED(tmp);
            }

            tmp2 = load_reg(s, rm);
            switch(op) {
                case 0x0: /* and */
                    tcg_gen_and_i32(tmp, tmp, tmp2);
                    if(!s->condexec_mask) {
                        gen_logic_CC(tmp);
                    }
                    break;
                case 0x1: /* eor */
                    tcg_gen_xor_i32(tmp, tmp, tmp2);
                    if(!s->condexec_mask) {
                        gen_logic_CC(tmp);
                    }
                    break;
                case 0x2: /* lsl */
                    if(s->condexec_mask) {
                        gen_helper_shl(tmp2, tmp2, tmp);
                    } else {
                        gen_helper_shl_cc(tmp2, tmp2, tmp);
                        gen_logic_CC(tmp2);
                    }
                    break;
                case 0x3: /* lsr */
                    if(s->condexec_mask) {
                        gen_helper_shr(tmp2, tmp2, tmp);
                    } else {
                        gen_helper_shr_cc(tmp2, tmp2, tmp);
                        gen_logic_CC(tmp2);
                    }
                    break;
                case 0x4: /* asr */
                    if(s->condexec_mask) {
                        gen_helper_sar(tmp2, tmp2, tmp);
                    } else {
                        gen_helper_sar_cc(tmp2, tmp2, tmp);
                        gen_logic_CC(tmp2);
                    }
                    break;
                case 0x5: /* adc */
                    if(s->condexec_mask) {
                        gen_adc(tmp, tmp2);
                    } else {
                        gen_helper_adc_cc(tmp, tmp, tmp2);
                    }
                    break;
                case 0x6: /* sbc */
                    if(s->condexec_mask) {
                        gen_sub_carry(tmp, tmp, tmp2);
                    } else {
                        gen_helper_sbc_cc(tmp, tmp, tmp2);
                    }
                    break;
                case 0x7: /* ror */
                    if(s->condexec_mask) {
                        tcg_gen_andi_i32(tmp, tmp, 0x1f);
                        tcg_gen_rotr_i32(tmp2, tmp2, tmp);
                    } else {
                        gen_helper_ror_cc(tmp2, tmp2, tmp);
                        gen_logic_CC(tmp2);
                    }
                    break;
                case 0x8: /* tst */
                    tcg_gen_and_i32(tmp, tmp, tmp2);
                    gen_logic_CC(tmp);
                    rd = 16;
                    break;
                case 0x9: /* neg */
                    if(s->condexec_mask) {
                        tcg_gen_neg_i32(tmp, tmp2);
                    } else {
                        gen_helper_sub_cc(tmp, tmp, tmp2);
                    }
                    break;
                case 0xa: /* cmp */
                    gen_helper_sub_cc(tmp, tmp, tmp2);
                    rd = 16;
                    break;
                case 0xb: /* cmn */
                    gen_helper_add_cc(tmp, tmp, tmp2);
                    rd = 16;
                    break;
                case 0xc: /* orr */
                    tcg_gen_or_i32(tmp, tmp, tmp2);
                    if(!s->condexec_mask) {
                        gen_logic_CC(tmp);
                    }
                    break;
                case 0xd: /* mul */
                    tcg_gen_mul_i32(tmp, tmp, tmp2);
                    if(!s->condexec_mask) {
                        gen_logic_CC(tmp);
                    }
                    break;
                case 0xe: /* bic */
                    tcg_gen_andc_i32(tmp, tmp, tmp2);
                    if(!s->condexec_mask) {
                        gen_logic_CC(tmp);
                    }
                    break;
                case 0xf: /* mvn */
                    tcg_gen_not_i32(tmp2, tmp2);
                    if(!s->condexec_mask) {
                        gen_logic_CC(tmp2);
                    }
                    val = 1;
                    rm = rd;
                    break;
            }
            if(rd != 16) {
                if(val) {
                    store_reg(s, rm, tmp2);
                    if(op != 0xf) {
                        tcg_temp_free_i32(tmp);
                    }
                } else {
                    store_reg(s, rd, tmp);
                    tcg_temp_free_i32(tmp2);
                }
            } else {
                tcg_temp_free_i32(tmp);
                tcg_temp_free_i32(tmp2);
            }
            break;

        case 5:
            /* load/store register offset.  */
            rd = insn & 7;
            rn = (insn >> 3) & 7;
            rm = (insn >> 6) & 7;
            op = (insn >> 9) & 7;
            addr = load_reg(s, rn);
            tmp = load_reg(s, rm);
            tcg_gen_add_i32(addr, addr, tmp);
            tcg_temp_free_i32(tmp);

            if(op < 3) { /* store */
                tmp = load_reg(s, rd);
            }

            gen_set_pc(current_pc);
            switch(op) {
                case 0: /* str */
                    gen_st32(tmp, addr, context_to_mmu_index(s));
                    break;
                case 1: /* strh */
                    gen_st16(tmp, addr, context_to_mmu_index(s));
                    break;
                case 2: /* strb */
                    gen_st8(tmp, addr, context_to_mmu_index(s));
                    break;
                case 3: /* ldrsb */
                    tmp = gen_ld8s(addr, context_to_mmu_index(s));
                    break;
                case 4: /* ldr */
                    tmp = gen_ld32(addr, context_to_mmu_index(s));
                    break;
                case 5: /* ldrh */
                    tmp = gen_ld16u(addr, context_to_mmu_index(s));
                    break;
                case 6: /* ldrb */
                    tmp = gen_ld8u(addr, context_to_mmu_index(s));
                    break;
                case 7: /* ldrsh */
                    tmp = gen_ld16s(addr, context_to_mmu_index(s));
                    break;
            }
            if(op >= 3) { /* load */
                store_reg(s, rd, tmp);
            }
            tcg_temp_free_i32(addr);
            break;

        case 6:
            /* load/store word immediate offset */
            rd = insn & 7;
            rn = (insn >> 3) & 7;
            addr = load_reg(s, rn);
            val = (insn >> 4) & 0x7c;
            tcg_gen_addi_i32(addr, addr, val);

            if(insn & (1 << 11)) {
                /* load */
                tmp = gen_ld32(addr, context_to_mmu_index(s));
                store_reg(s, rd, tmp);
            } else {
                /* store */
                tmp = load_reg(s, rd);
                gen_st32(tmp, addr, context_to_mmu_index(s));
            }
            tcg_temp_free_i32(addr);
            break;

        case 7:
            /* load/store byte immediate offset */
            rd = insn & 7;
            rn = (insn >> 3) & 7;
            addr = load_reg(s, rn);
            val = (insn >> 6) & 0x1f;
            tcg_gen_addi_i32(addr, addr, val);

            if(insn & (1 << 11)) {
                /* load */
                tmp = gen_ld8u(addr, context_to_mmu_index(s));
                store_reg(s, rd, tmp);
            } else {
                /* store */
                tmp = load_reg(s, rd);
                gen_st8(tmp, addr, context_to_mmu_index(s));
            }
            tcg_temp_free_i32(addr);
            break;

        case 8:
            /* load/store halfword immediate offset */
            rd = insn & 7;
            rn = (insn >> 3) & 7;
            addr = load_reg(s, rn);
            val = (insn >> 5) & 0x3e;
            tcg_gen_addi_i32(addr, addr, val);

            if(insn & (1 << 11)) {
                /* load */
                tmp = gen_ld16u(addr, context_to_mmu_index(s));
                store_reg(s, rd, tmp);
            } else {
                /* store */
                tmp = load_reg(s, rd);
                gen_st16(tmp, addr, context_to_mmu_index(s));
            }
            tcg_temp_free_i32(addr);
            break;

        case 9:
            /* load/store from stack */
            rd = (insn >> 8) & 7;
            addr = load_reg(s, 13);
            val = (insn & 0xff) * 4;
            tcg_gen_addi_i32(addr, addr, val);

            if(insn & (1 << 11)) {
                /* load */
                tmp = gen_ld32(addr, context_to_mmu_index(s));
                store_reg(s, rd, tmp);
            } else {
                /* store */
                tmp = load_reg(s, rd);
                gen_st32(tmp, addr, context_to_mmu_index(s));
            }
            tcg_temp_free_i32(addr);
            break;

        case 10:
            /* add to high reg */
            rd = (insn >> 8) & 7;
            if(insn & (1 << 11)) {
                /* SP */
                tmp = load_reg(s, 13);
            } else {
                /* PC. bit 1 is ignored.  */
                tmp = tcg_temp_new_i32();
                tcg_gen_movi_i32(tmp, (s->base.pc + 2) & ~(uint32_t)2);
            }
            val = (insn & 0xff) * 4;
            tcg_gen_addi_i32(tmp, tmp, val);
            store_reg(s, rd, tmp);
            break;

        case 11:
            /* misc */
            op = (insn >> 8) & 0xf;
            switch(op) {
                case 0:
                    /* adjust stack pointer */
                    tmp = load_reg(s, 13);
                    val = (insn & 0x7f) * 4;
                    if(insn & (1 << 7)) {
                        val = -(int32_t)val;
                    }
                    tcg_gen_addi_i32(tmp, tmp, val);
                    store_reg(s, 13, tmp);
                    break;

                case 2: /* sign/zero extend.  */
                    ARCH(6);
                    rd = insn & 7;
                    rm = (insn >> 3) & 7;
                    tmp = load_reg(s, rm);
                    switch((insn >> 6) & 3) {
                        case 0:
                            gen_sxth(tmp);
                            break;
                        case 1:
                            gen_sxtb(tmp);
                            break;
                        case 2:
                            gen_uxth(tmp);
                            break;
                        case 3:
                            gen_uxtb(tmp);
                            break;
                    }
                    store_reg(s, rd, tmp);
                    break;
                case 4:
                case 5:
                case 0xc:
                case 0xd:
                    /* push/pop */
                    addr = load_reg(s, 13);
                    if(insn & (1 << 8)) {
                        offset = 4;
                    } else {
                        offset = 0;
                    }
                    for(i = 0; i < 8; i++) {
                        if(insn & (1 << i)) {
                            offset += 4;
                        }
                    }
                    if((insn & (1 << 11)) == 0) {
                        tcg_gen_addi_i32(addr, addr, -offset);
                    }
                    for(i = 0; i < 8; i++) {
                        if(insn & (1 << i)) {
                            if(insn & (1 << 11)) {
                                /* pop */
                                tmp = gen_ld32(addr, context_to_mmu_index(s));
                                store_reg(s, i, tmp);
                            } else {
                                /* push */
                                tmp = load_reg(s, i);
                                gen_st32(tmp, addr, context_to_mmu_index(s));
                            }
                            /* advance to the next address.  */
                            tcg_gen_addi_i32(addr, addr, 4);
                        }
                    }
                    TCGV_UNUSED(tmp);
                    if(insn & (1 << 8)) {
                        if(insn & (1 << 11)) {
                            /* pop pc */
                            tmp = gen_ld32(addr, context_to_mmu_index(s));
                            /* don't set the pc until the rest of the instruction
                               has completed */
                        } else {
                            /* push lr */
                            tmp = load_reg(s, 14);
                            gen_st32(tmp, addr, context_to_mmu_index(s));
                        }
                        tcg_gen_addi_i32(addr, addr, 4);
                    }
                    if((insn & (1 << 11)) == 0) {
                        tcg_gen_addi_i32(addr, addr, -offset);
                    }
                    /* write back the new stack pointer */
                    store_reg(s, 13, addr);
                    /* set the new PC value */
                    if((insn & 0x0900) == 0x0900) {
                        /* Stack pop - loading the PC from memory */
                        store_reg_from_load(env, s, 15, tmp, STACK_FRAME_POP);
                    }
                    break;

                case 1:  //  czb
                case 3:
                case 9:
                case 11:
                    rm = insn & 7;
                    tmp = load_reg(s, rm);
                    s->condlabel = gen_new_label();
                    s->condjmp = 1;
                    if(insn & (1 << 11)) {
                        tcg_gen_brcondi_i32(TCG_COND_EQ, tmp, 0, s->condlabel);
                    } else {
                        tcg_gen_brcondi_i32(TCG_COND_NE, tmp, 0, s->condlabel);
                    }
                    tcg_temp_free_i32(tmp);
                    offset = ((insn & 0xf8) >> 2) | (insn & 0x200) >> 3;
                    val = (uint32_t)s->base.pc + 2;
                    val += offset;
                    gen_jmp(s, val, STACK_FRAME_NO_CHANGE);
                    break;

                case 15: /* IT, nop-hint.  */
                    if((insn & 0xf) == 0) {
                        gen_nop_hint(s, (insn >> 4) & 0xf);
                        break;
                    }
                    /* If Then.  */
                    s->condexec_cond = (insn >> 4) & 0xe;
                    s->condexec_mask = insn & 0x1f;
                    /* No actual code generated for this insn, just setup state.  */
                    break;

                case 0xe: /* bkpt */
                    ARCH(5);
                    gen_exception_insn(s, 2, EXCP_BKPT);
                    LOCK_TB(s->base.tb);
                    break;

                case 0xa: /* rev */
                    ARCH(6);
                    rn = (insn >> 3) & 0x7;
                    rd = insn & 0x7;
                    tmp = load_reg(s, rn);
                    switch((insn >> 6) & 3) {
                        case 0:
                            tcg_gen_bswap32_i32(tmp, tmp);
                            break;
                        case 1:
                            gen_rev16(tmp);
                            break;
                        case 3:
                            gen_revsh(tmp);
                            break;
                        default:
                            goto illegal_op;
                    }
                    store_reg(s, rd, tmp);
                    break;

                case 6: /* cps */
                    ARCH(6);
                    if(s->user) {
                        break;
                    }
#ifdef TARGET_PROTO_ARM_M
                    tmp = tcg_const_i32((insn & (1 << 4)) != 0);
                    /* PRIMASK */
                    if(insn & 2) {
                        addr = tcg_const_i32(16);
                        gen_helper_v7m_msr(cpu_env, addr, tmp);
                        tcg_temp_free_i32(addr);
                    }
                    /* FAULTMASK */
                    if(insn & 1) {
                        addr = tcg_const_i32(19);
                        gen_helper_v7m_msr(cpu_env, addr, tmp);
                        tcg_temp_free_i32(addr);
                    }
                    tcg_temp_free_i32(tmp);
                    gen_lookup_tb(s);
#else
                    if(insn & (1 << 4)) {
                        shift = CPSR_A | CPSR_I | CPSR_F;
                    } else {
                        shift = 0;
                    }
                    gen_set_psr_im(s, ((insn & 7) << 6), 0, shift);
#endif
                    break;

                default:
                    goto undef;
            }
            break;

        case 12: {
            /* load/store multiple */
            TCGv loaded_var;
            TCGV_UNUSED(loaded_var);
            rn = (insn >> 8) & 0x7;
            addr = load_reg(s, rn);
            for(i = 0; i < 8; i++) {
                if(insn & (1 << i)) {
                    if(insn & (1 << 11)) {
                        /* load */
                        tmp = gen_ld32(addr, context_to_mmu_index(s));
                        if(i == rn) {
                            loaded_var = tmp;
                        } else {
                            store_reg(s, i, tmp);
                        }
                    } else {
                        /* store */
                        tmp = load_reg(s, i);
                        gen_st32(tmp, addr, context_to_mmu_index(s));
                    }
                    /* advance to the next address */
                    tcg_gen_addi_i32(addr, addr, 4);
                }
            }
            if((insn & (1 << rn)) == 0) {
                /* base reg not in list: base register writeback */
                store_reg(s, rn, addr);
            } else {
                /* base reg in list: if load, complete it now */
                if(insn & (1 << 11)) {
                    store_reg(s, rn, loaded_var);
                }
                tcg_temp_free_i32(addr);
            }
            break;
        }
        case 13:
            /* conditional branch or swi */
            cond = (insn >> 8) & 0xf;
            if(cond == 0xe) {
                goto undef;
            }

            if(cond == 0xf) {
                /* swi */
                gen_set_pc_im(s->base.pc);
                s->base.is_jmp = DISAS_SWI;
                LOCK_TB(s->base.tb);
                break;
            }
            /* generate a conditional jump to next instruction */
            s->condlabel = gen_new_label();
            gen_test_cc(cond ^ 1, s->condlabel);
            s->condjmp = 1;

            /* jump to the offset */
            val = (uint32_t)s->base.pc + 2;
            offset = ((int32_t)insn << 24) >> 24;
            val += offset << 1;
            gen_jmp(s, val, STACK_FRAME_NO_CHANGE);
            break;

        case 14:
            if(insn & (1 << 11)) {
                if(disas_thumb2_insn(env, s, insn)) {
                    goto undef32;
                }
                break;
            }
            /* unconditional branch */
            if(insn == 0xe7fe) {
                tlib_printf(LOG_LEVEL_NOISY, "Loop to itself detected");
                gen_helper_wfi();
                s->base.is_jmp = DISAS_JUMP;
                LOCK_TB(s->base.tb);
            } else {
                val = (uint32_t)s->base.pc;
                offset = ((int32_t)insn << 21) >> 21;
                val += (offset << 1) + 2;
                gen_jmp(s, val, STACK_FRAME_NO_CHANGE);
            }
            break;
        case 15:
            if(disas_thumb2_insn(env, s, insn)) {
                goto undef32;
            }
            break;
    }
    return;
undef32:
    gen_exception_insn(s, 4, EXCP_UDEF);
    LOCK_TB(s->base.tb);
    return;
illegal_op:
undef:
    gen_exception_insn(s, 2, EXCP_UDEF);
    LOCK_TB(s->base.tb);
}

int disas_insn(CPUState *env, DisasContext *dc)
{
    target_ulong start_pc = dc->base.pc;
    tcg_gen_insn_start(start_pc, pack_condexec(dc));

    if(dc->thumb) {
        disas_thumb_insn(env, dc);
        if(dc->condexec_mask) {
            dc->condexec_cond = (dc->condexec_cond & 0xe) | ((dc->condexec_mask >> 4) & 1);
            dc->condexec_mask = (dc->condexec_mask << 1) & 0x1f;
            if(dc->condexec_mask == 0) {
                dc->condexec_cond = 0;
            }
        }
    } else {
        disas_arm_insn(env, dc);
    }
    return dc->base.pc - start_pc;
}

void setup_disas_context(DisasContextBase *base, CPUState *env)
{
    DisasContext *dc = (DisasContext *)base;
    dc->condjmp = 0;
    dc->ns = ARM_TBFLAG_NS(dc->base.tb->flags);
    dc->thumb = ARM_TBFLAG_THUMB(dc->base.tb->flags);
    dc->condexec_mask = (ARM_TBFLAG_CONDEXEC(dc->base.tb->flags) & 0xf) << 1;
    dc->condexec_cond = ARM_TBFLAG_CONDEXEC(dc->base.tb->flags) >> 4;
    dc->user = (ARM_TBFLAG_PRIV(dc->base.tb->flags) == 0);
    dc->vfp_enabled = ARM_TBFLAG_VFPEN(dc->base.tb->flags);
    dc->vec_len = ARM_TBFLAG_VECLEN(dc->base.tb->flags);
    dc->vec_stride = ARM_TBFLAG_VECSTRIDE(dc->base.tb->flags);
    dc->cp_regs = env->cp_regs;
    cpu_F0s = tcg_temp_new_i32();
    cpu_F1s = tcg_temp_new_i32();
    cpu_F0d = tcg_temp_new_i64();
    cpu_F1d = tcg_temp_new_i64();
    cpu_V0 = cpu_F0d;
    cpu_V1 = cpu_F1d;
    /* FIXME: cpu_M0 can probably be the same as cpu_V0.  */
    cpu_M0 = tcg_temp_new_i64();

    /* A note on handling of the condexec (IT) bits:
     *
     * We want to avoid the overhead of having to write the updated condexec
     * bits back to the CPUState for every instruction in an IT block. So:
     * (1) if the condexec bits are not already zero then we write
     * zero back into the CPUState now. This avoids complications trying
     * to do it at the end of the block. (For example if we don't do this
     * it's hard to identify whether we can safely skip writing condexec
     * at the end of the TB, which we definitely want to do for the case
     * where a TB doesn't do anything with the IT state at all.)
     * (2) if we are going to leave the TB then we call gen_set_condexec()
     * which will write the correct value into CPUState if zero is wrong.
     * This is done both for leaving the TB at the end, and for leaving
     * it because of an exception we know will happen, which is done in
     * gen_exception_insn(). The latter is necessary because we need to
     * leave the TB with the PC/IT state just prior to execution of the
     * instruction which caused the exception.
     * (3) if we leave the TB unexpectedly (eg a data abort on a load)
     * then the CPUState will be wrong and we need to reset it.
     * This is handled in the same way as restoration of the
     * PC in these situations: we save the value of the condexec bits
     * for each PC via tcg_gen_insn_start(), and restore_state_to_opc()
     * then uses this to restore them after an exception.
     *
     * Note that there are no instructions which can read the condexec
     * bits, and none which can write non-static values to them, so
     * we don't need to care about whether CPUState is correct in the
     * middle of a TB.
     */

    /* Reset the conditional execution bits immediately. This avoids
       complications trying to do it at the end of the block.  */
    if(dc->condexec_mask || dc->condexec_cond) {
        TCGv tmp = tcg_temp_new_i32();
        tcg_gen_movi_i32(tmp, 0);
        store_cpu_field(tmp, condexec_bits);
    }
}

int gen_breakpoint(DisasContextBase *base, CPUBreakpoint *bp)
{
    DisasContext *dc = (DisasContext *)base;
    gen_exception_insn(dc, 0, EXCP_DEBUG);
    LOCK_TB(dc->base.tb);
    /* Advance PC so that clearing the breakpoint will
       invalidate this TB.  */
    dc->base.pc += 2;
    return 1;
}

/* generate intermediate code in gen_opc_buf and gen_opparam_buf for
   basic block 'tb'. Also generate PC information for each
   intermediate instruction. */

int gen_intermediate_code(CPUState *env, DisasContextBase *base)
{
    DisasContext *dc = (DisasContext *)base;

    base->tb->size += disas_insn(env, (DisasContext *)base);

    if(dc->condjmp && !dc->base.is_jmp) {
        gen_set_label(dc->condlabel);
        dc->condjmp = 0;
    }
    if((base->pc - (base->tb->pc & TARGET_PAGE_MASK)) >= TARGET_PAGE_SIZE) {
        return 0;
    }
    return 1;
}

uint32_t gen_intermediate_code_epilogue(CPUState *env, DisasContextBase *base)
{
    DisasContext *dc = (DisasContext *)base;
    /* At this stage dc.condjmp will only be set when the skipped
       instruction was a conditional branch or trap, and the PC has
       already been written.  */
    /* While branches must always occur at the end of an IT block,
       there are a few other things that can cause us to terminate
       the TB in the middle of an IT block:
        - Exception generating instructions (bkpt, swi, undefined).
        - Page boundaries.
        - Hardware watchpoints.
       Hardware breakpoints have already been handled and skip this code.
     */
    gen_set_condexec(dc);
    switch(dc->base.is_jmp) {
        case DISAS_NEXT:
            gen_goto_tb(dc, 1, dc->base.pc);
            break;
        default:
        case DISAS_JUMP:
        case DISAS_UPDATE:
            /* indicate that the hash table must be used to find the next TB */
            gen_exit_tb_no_chaining(dc->base.tb);
            break;
        case DISAS_TB_JUMP:
            /* nothing more to generate */
            break;
        case DISAS_WFI:
            gen_helper_wfi();
            gen_exit_tb_no_chaining(dc->base.tb);
            break;
        case DISAS_WFE:
            gen_helper_wfe();
            gen_exit_tb_no_chaining(dc->base.tb);
            break;
        case DISAS_SWI:
            gen_exception(EXCP_SWI);
            gen_exit_tb_no_chaining(dc->base.tb);
            break;
    }
    if(dc->condjmp) {
        gen_set_label(dc->condlabel);
        gen_set_condexec(dc);
        gen_goto_tb(dc, 1, dc->base.pc);
        dc->condjmp = 0;
    }

    return dc->thumb;
}

void restore_state_to_opc(CPUState *env, TranslationBlock *tb, target_ulong *data)
{
    env->regs[15] = data[0];
    env->condexec_bits = data[1];
}

int process_interrupt(int interrupt_request, CPUState *env)
{
    if(tlib_is_in_debug_mode()) {
        return 0;
    }

    if(interrupt_request & CPU_INTERRUPT_FIQ && !(env->uncached_cpsr & CPSR_F)) {
        env->exception_index = EXCP_FIQ;
        do_interrupt(env);
        return 1;
    }
    /* ARMv7-M interrupt return works by loading a magic value
       into the PC.  On real hardware the load causes the
       return to occur.  The qemu implementation performs the
       jump normally, then does the exception return when the
       CPU tries to execute code at the magic address.
       This will cause the magic PC value to be pushed to
       the stack if an interrupt occurred at the wrong time.
       We avoid this by disabling interrupts when
       pc contains a magic address.  */
    //  fix from https://bugs.launchpad.net/qemu/+bug/942659
    if((interrupt_request & CPU_INTERRUPT_HARD) &&
#ifdef TARGET_PROTO_ARM_M
       (env->regs[15] < 0xffffffe0) && !(env->v7m.primask[env->secure] & PRIMASK_EN))
#else
       !(env->uncached_cpsr & CPSR_I))
#endif
    {
        env->exception_index = EXCP_IRQ;
        do_interrupt(env);
        return 1;
    }
    return 0;
}

#if !defined(TARGET_PROTO_ARM_M)
void gen_block_header_arch_action(TranslationBlock *tb)
{
    //  Let's save a costly function call by branching forward if the lib has the header trampoline disabled at runtime
    int pmu_counters_disabled_label = gen_new_label();
    TCGv_i32 tmp32 = tcg_temp_new_i64();
    tcg_gen_ld_i32(tmp32, cpu_env, offsetof(CPUState, pmu.counters_enabled));
    tcg_gen_brcondi_i32(TCG_COND_EQ, tmp32, false, pmu_counters_disabled_label);
    tcg_temp_free_i32(tmp32);

    TCGv_i64 icount = tcg_temp_new_i64();
    TCGv_ptr tb_pointer = tcg_const_ptr((tcg_target_long)tb);

    tcg_gen_ld32u_i64(icount, tb_pointer, offsetof(TranslationBlock, icount));
    gen_helper_pmu_count_instructions_cycles(icount);

    tcg_temp_free_ptr(tb_pointer);
    tcg_temp_free_i64(icount);

    gen_set_label(pmu_counters_disabled_label);
}
#endif

//  TODO: These empty implementations are required due to problems with weak attribute.
//  Remove this after #7035.
void cpu_exec_epilogue(CPUState *env) { }

void cpu_exec_prologue(CPUState *env) { }

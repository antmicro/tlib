/*
 *  i386 translation
 *
 *  Copyright (c) 2003 Fabrice Bellard
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
#include <signal.h>

#include "cpu.h"
#include "arch_callbacks.h"

#include "tb-helper.h"

#include "debug.h"

#define PREFIX_REPZ  0x01
#define PREFIX_REPNZ 0x02
#define PREFIX_LOCK  0x04
#define PREFIX_DATA  0x08
#define PREFIX_ADR   0x10

#ifdef TARGET_X86_64
#define X86_64_ONLY(x)  x
#define X86_64_DEF(...) __VA_ARGS__
#define CODE64(s)       ((s)->code64)
#define REX_X(s)        ((s)->rex_x)
#define REX_B(s)        ((s)->rex_b)
/* XXX: gcc generates push/pop in some opcodes, so we cannot use them */
#if 1
#define BUGGY_64(x) NULL
#endif
#else
#define X86_64_ONLY(x) NULL
#define X86_64_DEF(...)
#define CODE64(s) 0
#define REX_X(s)  0
#define REX_B(s)  0
#endif

/* global register indexes */
static TCGv cpu_A0, cpu_cc_src, cpu_cc_dst, cpu_cc_tmp;
static TCGv_i32 cpu_cc_op;
static TCGv cpu_regs[CPU_NB_REGS];
/* local temps */
static TCGv cpu_T[2], cpu_T3;
/* local register indexes (only used inside old micro ops) */
static TCGv cpu_tmp0, cpu_tmp4;
static TCGv_ptr cpu_ptr0, cpu_ptr1;
static TCGv_i32 cpu_tmp2_i32, cpu_tmp3_i32;
static TCGv_i64 cpu_tmp1_i64;
static TCGv cpu_tmp5;

#ifdef TARGET_X86_64
static int x86_64_hregs;
#endif

void translate_init(void)
{
    cpu_cc_op = tcg_global_mem_new_i32(TCG_AREG0, offsetof(CPUState, cc_op), "cc_op");
    cpu_cc_src = tcg_global_mem_new(TCG_AREG0, offsetof(CPUState, cc_src), "cc_src");
    cpu_cc_dst = tcg_global_mem_new(TCG_AREG0, offsetof(CPUState, cc_dst), "cc_dst");
    cpu_cc_tmp = tcg_global_mem_new(TCG_AREG0, offsetof(CPUState, cc_tmp), "cc_tmp");

#ifdef TARGET_X86_64
    cpu_regs[R_EAX] = tcg_global_mem_new_i64(TCG_AREG0, offsetof(CPUState, regs[R_EAX]), "rax");
    cpu_regs[R_ECX] = tcg_global_mem_new_i64(TCG_AREG0, offsetof(CPUState, regs[R_ECX]), "rcx");
    cpu_regs[R_EDX] = tcg_global_mem_new_i64(TCG_AREG0, offsetof(CPUState, regs[R_EDX]), "rdx");
    cpu_regs[R_EBX] = tcg_global_mem_new_i64(TCG_AREG0, offsetof(CPUState, regs[R_EBX]), "rbx");
    cpu_regs[R_ESP] = tcg_global_mem_new_i64(TCG_AREG0, offsetof(CPUState, regs[R_ESP]), "rsp");
    cpu_regs[R_EBP] = tcg_global_mem_new_i64(TCG_AREG0, offsetof(CPUState, regs[R_EBP]), "rbp");
    cpu_regs[R_ESI] = tcg_global_mem_new_i64(TCG_AREG0, offsetof(CPUState, regs[R_ESI]), "rsi");
    cpu_regs[R_EDI] = tcg_global_mem_new_i64(TCG_AREG0, offsetof(CPUState, regs[R_EDI]), "rdi");
    cpu_regs[8] = tcg_global_mem_new_i64(TCG_AREG0, offsetof(CPUState, regs[8]), "r8");
    cpu_regs[9] = tcg_global_mem_new_i64(TCG_AREG0, offsetof(CPUState, regs[9]), "r9");
    cpu_regs[10] = tcg_global_mem_new_i64(TCG_AREG0, offsetof(CPUState, regs[10]), "r10");
    cpu_regs[11] = tcg_global_mem_new_i64(TCG_AREG0, offsetof(CPUState, regs[11]), "r11");
    cpu_regs[12] = tcg_global_mem_new_i64(TCG_AREG0, offsetof(CPUState, regs[12]), "r12");
    cpu_regs[13] = tcg_global_mem_new_i64(TCG_AREG0, offsetof(CPUState, regs[13]), "r13");
    cpu_regs[14] = tcg_global_mem_new_i64(TCG_AREG0, offsetof(CPUState, regs[14]), "r14");
    cpu_regs[15] = tcg_global_mem_new_i64(TCG_AREG0, offsetof(CPUState, regs[15]), "r15");
#else
    cpu_regs[R_EAX] = tcg_global_mem_new_i32(TCG_AREG0, offsetof(CPUState, regs[R_EAX]), "eax");
    cpu_regs[R_ECX] = tcg_global_mem_new_i32(TCG_AREG0, offsetof(CPUState, regs[R_ECX]), "ecx");
    cpu_regs[R_EDX] = tcg_global_mem_new_i32(TCG_AREG0, offsetof(CPUState, regs[R_EDX]), "edx");
    cpu_regs[R_EBX] = tcg_global_mem_new_i32(TCG_AREG0, offsetof(CPUState, regs[R_EBX]), "ebx");
    cpu_regs[R_ESP] = tcg_global_mem_new_i32(TCG_AREG0, offsetof(CPUState, regs[R_ESP]), "esp");
    cpu_regs[R_EBP] = tcg_global_mem_new_i32(TCG_AREG0, offsetof(CPUState, regs[R_EBP]), "ebp");
    cpu_regs[R_ESI] = tcg_global_mem_new_i32(TCG_AREG0, offsetof(CPUState, regs[R_ESI]), "esi");
    cpu_regs[R_EDI] = tcg_global_mem_new_i32(TCG_AREG0, offsetof(CPUState, regs[R_EDI]), "edi");
#endif
}

static void gen_eob(DisasContext *s);
static void gen_jmp(DisasContext *s, target_ulong eip);
static void gen_jmp_tb(DisasContext *s, target_ulong eip, int tb_num);

/* i386 arith/logic operations */
enum {
    OP_ADDL,
    OP_ORL,
    OP_ADCL,
    OP_SBBL,
    OP_ANDL,
    OP_SUBL,
    OP_XORL,
    OP_CMPL,
};

/* i386 shift ops */
enum {
    OP_ROL,
    OP_ROR,
    OP_RCL,
    OP_RCR,
    OP_SHL,
    OP_SHR,
    OP_SHL1, /* undocumented */
    OP_SAR = 7,
};

enum {
    JCC_O,
    JCC_B,
    JCC_Z,
    JCC_BE,
    JCC_S,
    JCC_P,
    JCC_L,
    JCC_LE,
};

/* operand size */
enum {
    OT_BYTE = 0,
    OT_WORD,
    OT_LONG,
    OT_QUAD,
};

enum {
    /* I386 int registers */
    OR_EAX, /* MUST be even numbered */
    OR_ECX,
    OR_EDX,
    OR_EBX,
    OR_ESP,
    OR_EBP,
    OR_ESI,
    OR_EDI,

    OR_TMP0 = 16, /* temporary operand register */
    OR_TMP1,
    OR_A0, /* temporary register used when doing address evaluation */
};

static inline void gen_op_movl_T0_0(void)
{
    tcg_gen_movi_tl(cpu_T[0], 0);
}

static inline void gen_op_movl_T0_im(int32_t val)
{
    tcg_gen_movi_tl(cpu_T[0], val);
}

static inline void gen_op_movl_T1_im(int32_t val)
{
    tcg_gen_movi_tl(cpu_T[1], val);
}

static inline void gen_op_movl_T1_imu(uint32_t val)
{
    tcg_gen_movi_tl(cpu_T[1], val);
}

static inline void gen_op_movl_A0_im(uint32_t val)
{
    tcg_gen_movi_tl(cpu_A0, val);
}

#ifdef TARGET_X86_64
static inline void gen_op_movq_A0_im(int64_t val)
{
    tcg_gen_movi_tl(cpu_A0, val);
}
#endif

static inline void gen_movtl_T0_im(target_ulong val)
{
    tcg_gen_movi_tl(cpu_T[0], val);
}

static inline void gen_movtl_T1_im(target_ulong val)
{
    tcg_gen_movi_tl(cpu_T[1], val);
}

static inline void gen_op_andl_T0_ffff(void)
{
    tcg_gen_andi_tl(cpu_T[0], cpu_T[0], 0xffff);
}

static inline void gen_op_andl_T0_im(uint32_t val)
{
    tcg_gen_andi_tl(cpu_T[0], cpu_T[0], val);
}

static inline void gen_op_movl_T0_T1(void)
{
    tcg_gen_mov_tl(cpu_T[0], cpu_T[1]);
}

static inline void gen_op_andl_A0_ffff(void)
{
    tcg_gen_andi_tl(cpu_A0, cpu_A0, 0xffff);
}

#ifdef TARGET_X86_64

#define NB_OP_SIZES 4

#else /* !TARGET_X86_64 */

#define NB_OP_SIZES 3

#endif /* !TARGET_X86_64 */

#if defined(HOST_WORDS_BIGENDIAN)
#define REG_B_OFFSET  (sizeof(target_ulong) - 1)
#define REG_H_OFFSET  (sizeof(target_ulong) - 2)
#define REG_W_OFFSET  (sizeof(target_ulong) - 2)
#define REG_L_OFFSET  (sizeof(target_ulong) - 4)
#define REG_LH_OFFSET (sizeof(target_ulong) - 8)
#else
#define REG_B_OFFSET  0
#define REG_H_OFFSET  1
#define REG_W_OFFSET  0
#define REG_L_OFFSET  0
#define REG_LH_OFFSET 4
#endif

static inline void gen_op_mov_reg_v(int ot, int reg, TCGv t0)
{
    switch(ot) {
        case OT_BYTE:
            if(reg < 4 X86_64_DEF(|| reg >= 8 || x86_64_hregs)) {
                tcg_gen_deposit_tl(cpu_regs[reg], cpu_regs[reg], t0, 0, 8);
            } else {
                tcg_gen_deposit_tl(cpu_regs[reg - 4], cpu_regs[reg - 4], t0, 8, 8);
            }
            break;
        case OT_WORD:
            tcg_gen_deposit_tl(cpu_regs[reg], cpu_regs[reg], t0, 0, 16);
            break;
        default: /* XXX this shouldn't be reached;  abort? */
        case OT_LONG:
            /* For x86_64, this sets the higher half of register to zero.
               For i386, this is equivalent to a mov. */
            tcg_gen_ext32u_tl(cpu_regs[reg], t0);
            break;
#ifdef TARGET_X86_64
        case OT_QUAD:
            tcg_gen_mov_tl(cpu_regs[reg], t0);
            break;
#endif
    }
}

static inline void gen_op_mov_reg_T0(int ot, int reg)
{
    gen_op_mov_reg_v(ot, reg, cpu_T[0]);
}

static inline void gen_op_mov_reg_T1(int ot, int reg)
{
    gen_op_mov_reg_v(ot, reg, cpu_T[1]);
}

static inline void gen_op_mov_reg_A0(int size, int reg)
{
    switch(size) {
        case 0:
            tcg_gen_deposit_tl(cpu_regs[reg], cpu_regs[reg], cpu_A0, 0, 16);
            break;
        default: /* XXX this shouldn't be reached;  abort? */
        case 1:
            /* For x86_64, this sets the higher half of register to zero.
               For i386, this is equivalent to a mov. */
            tcg_gen_ext32u_tl(cpu_regs[reg], cpu_A0);
            break;
#ifdef TARGET_X86_64
        case 2:
            tcg_gen_mov_tl(cpu_regs[reg], cpu_A0);
            break;
#endif
    }
}

static inline void gen_op_mov_v_reg(int ot, TCGv t0, int reg)
{
    switch(ot) {
        case OT_BYTE:
            if(reg < 4 X86_64_DEF(|| reg >= 8 || x86_64_hregs)) {
                goto std_case;
            } else {
                tcg_gen_shri_tl(t0, cpu_regs[reg - 4], 8);
                tcg_gen_ext8u_tl(t0, t0);
            }
            break;
        default:
        std_case:
            tcg_gen_mov_tl(t0, cpu_regs[reg]);
            break;
    }
}

static inline void gen_op_mov_TN_reg(int ot, int t_index, int reg)
{
    gen_op_mov_v_reg(ot, cpu_T[t_index], reg);
}

static inline void gen_op_movl_A0_reg(int reg)
{
    tcg_gen_mov_tl(cpu_A0, cpu_regs[reg]);
}

static inline void gen_op_addl_A0_im(int32_t val)
{
    tcg_gen_addi_tl(cpu_A0, cpu_A0, val);
#ifdef TARGET_X86_64
    tcg_gen_andi_tl(cpu_A0, cpu_A0, 0xffffffff);
#endif
}

#ifdef TARGET_X86_64
static inline void gen_op_addq_A0_im(int64_t val)
{
    tcg_gen_addi_tl(cpu_A0, cpu_A0, val);
}
#endif

static void gen_add_A0_im(DisasContext *s, int val)
{
#ifdef TARGET_X86_64
    if(CODE64(s)) {
        gen_op_addq_A0_im(val);
    } else
#endif
        gen_op_addl_A0_im(val);
}

static inline void gen_op_addl_T0_T1(void)
{
    tcg_gen_add_tl(cpu_T[0], cpu_T[0], cpu_T[1]);
}

static inline void gen_op_jmp_T0(void)
{
    tcg_gen_st_tl(cpu_T[0], cpu_env, offsetof(CPUState, eip));
}

static inline void gen_op_add_reg_im(int size, int reg, int32_t val)
{
    switch(size) {
        case 0:
            tcg_gen_addi_tl(cpu_tmp0, cpu_regs[reg], val);
            tcg_gen_deposit_tl(cpu_regs[reg], cpu_regs[reg], cpu_tmp0, 0, 16);
            break;
        case 1:
            tcg_gen_addi_tl(cpu_tmp0, cpu_regs[reg], val);
            /* For x86_64, this sets the higher half of register to zero.
               For i386, this is equivalent to a nop. */
            tcg_gen_ext32u_tl(cpu_tmp0, cpu_tmp0);
            tcg_gen_mov_tl(cpu_regs[reg], cpu_tmp0);
            break;
#ifdef TARGET_X86_64
        case 2:
            tcg_gen_addi_tl(cpu_regs[reg], cpu_regs[reg], val);
            break;
#endif
    }
}

static inline void gen_op_add_reg_T0(int size, int reg)
{
    switch(size) {
        case 0:
            tcg_gen_add_tl(cpu_tmp0, cpu_regs[reg], cpu_T[0]);
            tcg_gen_deposit_tl(cpu_regs[reg], cpu_regs[reg], cpu_tmp0, 0, 16);
            break;
        case 1:
            tcg_gen_add_tl(cpu_tmp0, cpu_regs[reg], cpu_T[0]);
            /* For x86_64, this sets the higher half of register to zero.
               For i386, this is equivalent to a nop. */
            tcg_gen_ext32u_tl(cpu_tmp0, cpu_tmp0);
            tcg_gen_mov_tl(cpu_regs[reg], cpu_tmp0);
            break;
#ifdef TARGET_X86_64
        case 2:
            tcg_gen_add_tl(cpu_regs[reg], cpu_regs[reg], cpu_T[0]);
            break;
#endif
    }
}

static inline void gen_op_set_cc_op(int32_t val)
{
    tcg_gen_movi_i32(cpu_cc_op, val);
}

static inline void gen_op_addl_A0_reg_sN(int shift, int reg)
{
    tcg_gen_mov_tl(cpu_tmp0, cpu_regs[reg]);
    if(shift != 0) {
        tcg_gen_shli_tl(cpu_tmp0, cpu_tmp0, shift);
    }
    tcg_gen_add_tl(cpu_A0, cpu_A0, cpu_tmp0);
    /* For x86_64, this sets the higher half of register to zero.
       For i386, this is equivalent to a nop. */
    tcg_gen_ext32u_tl(cpu_A0, cpu_A0);
}

static inline void gen_op_movl_A0_seg(int reg)
{
    tcg_gen_ld32u_tl(cpu_A0, cpu_env, offsetof(CPUState, segs[reg].base) + REG_L_OFFSET);
}

static inline void gen_op_addl_A0_seg(int reg)
{
    tcg_gen_ld_tl(cpu_tmp0, cpu_env, offsetof(CPUState, segs[reg].base));
    tcg_gen_add_tl(cpu_A0, cpu_A0, cpu_tmp0);
#ifdef TARGET_X86_64
    tcg_gen_andi_tl(cpu_A0, cpu_A0, 0xffffffff);
#endif
}

#ifdef TARGET_X86_64
static inline void gen_op_movq_A0_seg(int reg)
{
    tcg_gen_ld_tl(cpu_A0, cpu_env, offsetof(CPUState, segs[reg].base));
}

static inline void gen_op_addq_A0_seg(int reg)
{
    tcg_gen_ld_tl(cpu_tmp0, cpu_env, offsetof(CPUState, segs[reg].base));
    tcg_gen_add_tl(cpu_A0, cpu_A0, cpu_tmp0);
}

static inline void gen_op_movq_A0_reg(int reg)
{
    tcg_gen_mov_tl(cpu_A0, cpu_regs[reg]);
}

static inline void gen_op_addq_A0_reg_sN(int shift, int reg)
{
    tcg_gen_mov_tl(cpu_tmp0, cpu_regs[reg]);
    if(shift != 0) {
        tcg_gen_shli_tl(cpu_tmp0, cpu_tmp0, shift);
    }
    tcg_gen_add_tl(cpu_A0, cpu_A0, cpu_tmp0);
}
#endif

static inline void gen_op_lds_T0_A0(int idx)
{
    int mem_idx = (idx >> 2) - 1;
    switch(idx & 3) {
        case 0:
            tcg_gen_qemu_ld8s(cpu_T[0], cpu_A0, mem_idx);
            break;
        case 1:
            tcg_gen_qemu_ld16s(cpu_T[0], cpu_A0, mem_idx);
            break;
        default:
        case 2:
            tcg_gen_qemu_ld32s(cpu_T[0], cpu_A0, mem_idx);
            break;
    }
}

static inline void gen_op_ld_v(int idx, TCGv t0, TCGv a0)
{
    int mem_idx = (idx >> 2) - 1;
    switch(idx & 3) {
        case 0:
            tcg_gen_qemu_ld8u(t0, a0, mem_idx);
            break;
        case 1:
            tcg_gen_qemu_ld16u(t0, a0, mem_idx);
            break;
        case 2:
            tcg_gen_qemu_ld32u(t0, a0, mem_idx);
            break;
        default:
        case 3:
            /* Should never happen on 32-bit targets.  */
#ifdef TARGET_X86_64
            tcg_gen_qemu_ld64(t0, a0, mem_idx);
#endif
            break;
    }
}

/* XXX: always use ldu or lds */
static inline void gen_op_ld_T0_A0(int idx)
{
    gen_op_ld_v(idx, cpu_T[0], cpu_A0);
}

static inline void gen_op_ldu_T0_A0(int idx)
{
    gen_op_ld_v(idx, cpu_T[0], cpu_A0);
}

static inline void gen_op_ld_T1_A0(int idx)
{
    gen_op_ld_v(idx, cpu_T[1], cpu_A0);
}

static inline void gen_op_st_v(int idx, TCGv t0, TCGv a0)
{
    int mem_idx = (idx >> 2) - 1;
    switch(idx & 3) {
        case 0:
            tcg_gen_qemu_st8(t0, a0, mem_idx);
            break;
        case 1:
            tcg_gen_qemu_st16(t0, a0, mem_idx);
            break;
        case 2:
            tcg_gen_qemu_st32(t0, a0, mem_idx);
            break;
        default:
        case 3:
            /* Should never happen on 32-bit targets.  */
#ifdef TARGET_X86_64
            tcg_gen_qemu_st64(t0, a0, mem_idx);
#endif
            break;
    }
}

static inline void gen_op_st_T0_A0(int idx)
{
    gen_op_st_v(idx, cpu_T[0], cpu_A0);
}

static inline void gen_op_st_T1_A0(int idx)
{
    gen_op_st_v(idx, cpu_T[1], cpu_A0);
}

static inline void gen_jmp_im(target_ulong pc)
{
    tcg_gen_movi_tl(cpu_tmp0, pc);
    tcg_gen_st_tl(cpu_tmp0, cpu_env, offsetof(CPUState, eip));
}

static inline void gen_string_movl_A0_ESI(DisasContext *s)
{
    int override;

    override = s->override;
#ifdef TARGET_X86_64
    if(s->aflag == 2) {
        if(override >= 0) {
            gen_op_movq_A0_seg(override);
            gen_op_addq_A0_reg_sN(0, R_ESI);
        } else {
            gen_op_movq_A0_reg(R_ESI);
        }
    } else
#endif
        if(s->aflag) {
        /* 32 bit address */
        if(s->addseg && override < 0) {
            override = R_DS;
        }
        if(override >= 0) {
            gen_op_movl_A0_seg(override);
            gen_op_addl_A0_reg_sN(0, R_ESI);
        } else {
            gen_op_movl_A0_reg(R_ESI);
        }
    } else {
        /* 16 address, always override */
        if(override < 0) {
            override = R_DS;
        }
        gen_op_movl_A0_reg(R_ESI);
        gen_op_andl_A0_ffff();
        gen_op_addl_A0_seg(override);
    }
}

static inline void gen_string_movl_A0_EDI(DisasContext *s)
{
#ifdef TARGET_X86_64
    if(s->aflag == 2) {
        gen_op_movq_A0_reg(R_EDI);
    } else
#endif
        if(s->aflag) {
        if(s->addseg) {
            gen_op_movl_A0_seg(R_ES);
            gen_op_addl_A0_reg_sN(0, R_EDI);
        } else {
            gen_op_movl_A0_reg(R_EDI);
        }
    } else {
        gen_op_movl_A0_reg(R_EDI);
        gen_op_andl_A0_ffff();
        gen_op_addl_A0_seg(R_ES);
    }
}

static inline void gen_op_movl_T0_Dshift(int ot)
{
    tcg_gen_ld32s_tl(cpu_T[0], cpu_env, offsetof(CPUState, df));
    tcg_gen_shli_tl(cpu_T[0], cpu_T[0], ot);
};

static void gen_extu(int ot, TCGv reg)
{
    switch(ot) {
        case OT_BYTE:
            tcg_gen_ext8u_tl(reg, reg);
            break;
        case OT_WORD:
            tcg_gen_ext16u_tl(reg, reg);
            break;
        case OT_LONG:
            tcg_gen_ext32u_tl(reg, reg);
            break;
        default:
            break;
    }
}

static void gen_exts(int ot, TCGv reg)
{
    switch(ot) {
        case OT_BYTE:
            tcg_gen_ext8s_tl(reg, reg);
            break;
        case OT_WORD:
            tcg_gen_ext16s_tl(reg, reg);
            break;
        case OT_LONG:
            tcg_gen_ext32s_tl(reg, reg);
            break;
        default:
            break;
    }
}

static inline void gen_op_jnz_ecx(int size, int label1)
{
    tcg_gen_mov_tl(cpu_tmp0, cpu_regs[R_ECX]);
    gen_extu(size + 1, cpu_tmp0);
    tcg_gen_brcondi_tl(TCG_COND_NE, cpu_tmp0, 0, label1);
}

static inline void gen_op_jz_ecx(int size, int label1)
{
    tcg_gen_mov_tl(cpu_tmp0, cpu_regs[R_ECX]);
    gen_extu(size + 1, cpu_tmp0);
    tcg_gen_brcondi_tl(TCG_COND_EQ, cpu_tmp0, 0, label1);
}

static void gen_helper_in_func(int ot, TCGv v, TCGv_i32 n)
{
    switch(ot) {
        case 0:
            gen_helper_inb(v, n);
            break;
        case 1:
            gen_helper_inw(v, n);
            break;
        case 2:
            gen_helper_inl(v, n);
            break;
    }
}

static void gen_helper_out_func(int ot, TCGv_i32 v, TCGv_i32 n)
{
    switch(ot) {
        case 0:
            gen_helper_outb(v, n);
            break;
        case 1:
            gen_helper_outw(v, n);
            break;
        case 2:
            gen_helper_outl(v, n);
            break;
    }
}

static void gen_check_io(DisasContext *s, int ot, target_ulong cur_eip, uint32_t svm_flags)
{
    int state_saved;
    target_ulong next_eip;

    state_saved = 0;
    if(s->pe && (s->cpl > s->iopl || s->vm86)) {
        if(s->cc_op != CC_OP_DYNAMIC) {
            gen_op_set_cc_op(s->cc_op);
        }
        gen_jmp_im(cur_eip);
        state_saved = 1;
        tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_T[0]);
        switch(ot) {
            case 0:
                gen_helper_check_iob(cpu_tmp2_i32);
                break;
            case 1:
                gen_helper_check_iow(cpu_tmp2_i32);
                break;
            case 2:
                gen_helper_check_iol(cpu_tmp2_i32);
                break;
        }
    }
    if(s->flags & HF_SVMI_MASK) {
        if(!state_saved) {
            if(s->cc_op != CC_OP_DYNAMIC) {
                gen_op_set_cc_op(s->cc_op);
            }
            gen_jmp_im(cur_eip);
        }
        svm_flags |= (1 << (4 + ot));
        next_eip = s->base.pc - s->cs_base;
        tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_T[0]);
        gen_helper_svm_check_io(cpu_tmp2_i32, tcg_const_i32(svm_flags), tcg_const_i32(next_eip - cur_eip));
    }
}

static inline void gen_movs(DisasContext *s, int ot)
{
    gen_string_movl_A0_ESI(s);
    gen_op_ld_T0_A0(ot + s->base.mem_idx);
    gen_string_movl_A0_EDI(s);
    gen_op_st_T0_A0(ot + s->base.mem_idx);
    gen_op_movl_T0_Dshift(ot);
    gen_op_add_reg_T0(s->aflag, R_ESI);
    gen_op_add_reg_T0(s->aflag, R_EDI);
}

static inline void gen_update_cc_op(DisasContext *s)
{
    if(s->cc_op != CC_OP_DYNAMIC) {
        gen_op_set_cc_op(s->cc_op);
        s->cc_op = CC_OP_DYNAMIC;
    }
}

static void gen_op_update1_cc(void)
{
    tcg_gen_discard_tl(cpu_cc_src);
    tcg_gen_mov_tl(cpu_cc_dst, cpu_T[0]);
}

static void gen_op_update2_cc(void)
{
    tcg_gen_mov_tl(cpu_cc_src, cpu_T[1]);
    tcg_gen_mov_tl(cpu_cc_dst, cpu_T[0]);
}

static inline void gen_op_cmpl_T0_T1_cc(void)
{
    tcg_gen_mov_tl(cpu_cc_src, cpu_T[1]);
    tcg_gen_sub_tl(cpu_cc_dst, cpu_T[0], cpu_T[1]);
}

static inline void gen_op_testl_T0_T1_cc(void)
{
    tcg_gen_discard_tl(cpu_cc_src);
    tcg_gen_and_tl(cpu_cc_dst, cpu_T[0], cpu_T[1]);
}

static void gen_op_update_neg_cc(void)
{
    tcg_gen_neg_tl(cpu_cc_src, cpu_T[0]);
    tcg_gen_mov_tl(cpu_cc_dst, cpu_T[0]);
}

/* compute eflags.C to reg */
static void gen_compute_eflags_c(TCGv reg)
{
    gen_helper_cc_compute_c(cpu_tmp2_i32, cpu_cc_op);
    tcg_gen_extu_i32_tl(reg, cpu_tmp2_i32);
}

/* compute all eflags to cc_src */
static void gen_compute_eflags(TCGv reg)
{
    gen_helper_cc_compute_all(cpu_tmp2_i32, cpu_cc_op);
    tcg_gen_extu_i32_tl(reg, cpu_tmp2_i32);
}

static inline void gen_setcc_slow_T0(DisasContext *s, int jcc_op)
{
    if(s->cc_op != CC_OP_DYNAMIC) {
        gen_op_set_cc_op(s->cc_op);
    }
    switch(jcc_op) {
        case JCC_O:
            gen_compute_eflags(cpu_T[0]);
            tcg_gen_shri_tl(cpu_T[0], cpu_T[0], 11);
            tcg_gen_andi_tl(cpu_T[0], cpu_T[0], 1);
            break;
        case JCC_B:
            gen_compute_eflags_c(cpu_T[0]);
            break;
        case JCC_Z:
            gen_compute_eflags(cpu_T[0]);
            tcg_gen_shri_tl(cpu_T[0], cpu_T[0], 6);
            tcg_gen_andi_tl(cpu_T[0], cpu_T[0], 1);
            break;
        case JCC_BE:
            gen_compute_eflags(cpu_tmp0);
            tcg_gen_shri_tl(cpu_T[0], cpu_tmp0, 6);
            tcg_gen_or_tl(cpu_T[0], cpu_T[0], cpu_tmp0);
            tcg_gen_andi_tl(cpu_T[0], cpu_T[0], 1);
            break;
        case JCC_S:
            gen_compute_eflags(cpu_T[0]);
            tcg_gen_shri_tl(cpu_T[0], cpu_T[0], 7);
            tcg_gen_andi_tl(cpu_T[0], cpu_T[0], 1);
            break;
        case JCC_P:
            gen_compute_eflags(cpu_T[0]);
            tcg_gen_shri_tl(cpu_T[0], cpu_T[0], 2);
            tcg_gen_andi_tl(cpu_T[0], cpu_T[0], 1);
            break;
        case JCC_L:
            gen_compute_eflags(cpu_tmp0);
            tcg_gen_shri_tl(cpu_T[0], cpu_tmp0, 11); /* CC_O */
            tcg_gen_shri_tl(cpu_tmp0, cpu_tmp0, 7);  /* CC_S */
            tcg_gen_xor_tl(cpu_T[0], cpu_T[0], cpu_tmp0);
            tcg_gen_andi_tl(cpu_T[0], cpu_T[0], 1);
            break;
        default:
        case JCC_LE:
            gen_compute_eflags(cpu_tmp0);
            tcg_gen_shri_tl(cpu_T[0], cpu_tmp0, 11); /* CC_O */
            tcg_gen_shri_tl(cpu_tmp4, cpu_tmp0, 7);  /* CC_S */
            tcg_gen_shri_tl(cpu_tmp0, cpu_tmp0, 6);  /* CC_Z */
            tcg_gen_xor_tl(cpu_T[0], cpu_T[0], cpu_tmp4);
            tcg_gen_or_tl(cpu_T[0], cpu_T[0], cpu_tmp0);
            tcg_gen_andi_tl(cpu_T[0], cpu_T[0], 1);
            break;
    }
}

/* return true if setcc_slow is not needed (WARNING: must be kept in
   sync with gen_jcc1) */
static int is_fast_jcc_case(DisasContext *s, int b)
{
    int jcc_op;
    jcc_op = (b >> 1) & 7;
    switch(s->cc_op) {
        /* we optimize the cmp/jcc case */
        case CC_OP_SUBB:
        case CC_OP_SUBW:
        case CC_OP_SUBL:
        case CC_OP_SUBQ:
            if(jcc_op == JCC_O || jcc_op == JCC_P) {
                goto slow_jcc;
            }
            break;

        /* some jumps are easy to compute */
        case CC_OP_ADDB:
        case CC_OP_ADDW:
        case CC_OP_ADDL:
        case CC_OP_ADDQ:

        case CC_OP_LOGICB:
        case CC_OP_LOGICW:
        case CC_OP_LOGICL:
        case CC_OP_LOGICQ:

        case CC_OP_INCB:
        case CC_OP_INCW:
        case CC_OP_INCL:
        case CC_OP_INCQ:

        case CC_OP_DECB:
        case CC_OP_DECW:
        case CC_OP_DECL:
        case CC_OP_DECQ:

        case CC_OP_SHLB:
        case CC_OP_SHLW:
        case CC_OP_SHLL:
        case CC_OP_SHLQ:
            if(jcc_op != JCC_Z && jcc_op != JCC_S) {
                goto slow_jcc;
            }
            break;
        default:
        slow_jcc:
            return 0;
    }
    return 1;
}

/* generate a conditional jump to label 'l1' according to jump opcode
   value 'b'. In the fast case, T0 is guaranted not to be used. */
static inline void gen_jcc1(DisasContext *s, int cc_op, int b, int l1)
{
    int inv, jcc_op, size, cond;
    TCGv t0;

    inv = b & 1;
    jcc_op = (b >> 1) & 7;

    switch(cc_op) {
        /* we optimize the cmp/jcc case */
        case CC_OP_SUBB:
        case CC_OP_SUBW:
        case CC_OP_SUBL:
        case CC_OP_SUBQ:

            size = cc_op - CC_OP_SUBB;
            switch(jcc_op) {
                case JCC_Z:
                fast_jcc_z:
                    switch(size) {
                        case 0:
                            tcg_gen_andi_tl(cpu_tmp0, cpu_cc_dst, 0xff);
                            t0 = cpu_tmp0;
                            break;
                        case 1:
                            tcg_gen_andi_tl(cpu_tmp0, cpu_cc_dst, 0xffff);
                            t0 = cpu_tmp0;
                            break;
#ifdef TARGET_X86_64
                        case 2:
                            tcg_gen_andi_tl(cpu_tmp0, cpu_cc_dst, 0xffffffff);
                            t0 = cpu_tmp0;
                            break;
#endif
                        default:
                            t0 = cpu_cc_dst;
                            break;
                    }
                    tcg_gen_brcondi_tl(inv ? TCG_COND_NE : TCG_COND_EQ, t0, 0, l1);
                    break;
                case JCC_S:
                fast_jcc_s:
                    switch(size) {
                        case 0:
                            tcg_gen_andi_tl(cpu_tmp0, cpu_cc_dst, 0x80);
                            tcg_gen_brcondi_tl(inv ? TCG_COND_EQ : TCG_COND_NE, cpu_tmp0, 0, l1);
                            break;
                        case 1:
                            tcg_gen_andi_tl(cpu_tmp0, cpu_cc_dst, 0x8000);
                            tcg_gen_brcondi_tl(inv ? TCG_COND_EQ : TCG_COND_NE, cpu_tmp0, 0, l1);
                            break;
#ifdef TARGET_X86_64
                        case 2:
                            tcg_gen_andi_tl(cpu_tmp0, cpu_cc_dst, 0x80000000);
                            tcg_gen_brcondi_tl(inv ? TCG_COND_EQ : TCG_COND_NE, cpu_tmp0, 0, l1);
                            break;
#endif
                        default:
                            tcg_gen_brcondi_tl(inv ? TCG_COND_GE : TCG_COND_LT, cpu_cc_dst, 0, l1);
                            break;
                    }
                    break;

                case JCC_B:
                    cond = inv ? TCG_COND_GEU : TCG_COND_LTU;
                    goto fast_jcc_b;
                case JCC_BE:
                    cond = inv ? TCG_COND_GTU : TCG_COND_LEU;
                fast_jcc_b:
                    tcg_gen_add_tl(cpu_tmp4, cpu_cc_dst, cpu_cc_src);
                    switch(size) {
                        case 0:
                            t0 = cpu_tmp0;
                            tcg_gen_andi_tl(cpu_tmp4, cpu_tmp4, 0xff);
                            tcg_gen_andi_tl(t0, cpu_cc_src, 0xff);
                            break;
                        case 1:
                            t0 = cpu_tmp0;
                            tcg_gen_andi_tl(cpu_tmp4, cpu_tmp4, 0xffff);
                            tcg_gen_andi_tl(t0, cpu_cc_src, 0xffff);
                            break;
#ifdef TARGET_X86_64
                        case 2:
                            t0 = cpu_tmp0;
                            tcg_gen_andi_tl(cpu_tmp4, cpu_tmp4, 0xffffffff);
                            tcg_gen_andi_tl(t0, cpu_cc_src, 0xffffffff);
                            break;
#endif
                        default:
                            t0 = cpu_cc_src;
                            break;
                    }
                    tcg_gen_brcond_tl(cond, cpu_tmp4, t0, l1);
                    break;

                case JCC_L:
                    cond = inv ? TCG_COND_GE : TCG_COND_LT;
                    goto fast_jcc_l;
                case JCC_LE:
                    cond = inv ? TCG_COND_GT : TCG_COND_LE;
                fast_jcc_l:
                    tcg_gen_add_tl(cpu_tmp4, cpu_cc_dst, cpu_cc_src);
                    switch(size) {
                        case 0:
                            t0 = cpu_tmp0;
                            tcg_gen_ext8s_tl(cpu_tmp4, cpu_tmp4);
                            tcg_gen_ext8s_tl(t0, cpu_cc_src);
                            break;
                        case 1:
                            t0 = cpu_tmp0;
                            tcg_gen_ext16s_tl(cpu_tmp4, cpu_tmp4);
                            tcg_gen_ext16s_tl(t0, cpu_cc_src);
                            break;
#ifdef TARGET_X86_64
                        case 2:
                            t0 = cpu_tmp0;
                            tcg_gen_ext32s_tl(cpu_tmp4, cpu_tmp4);
                            tcg_gen_ext32s_tl(t0, cpu_cc_src);
                            break;
#endif
                        default:
                            t0 = cpu_cc_src;
                            break;
                    }
                    tcg_gen_brcond_tl(cond, cpu_tmp4, t0, l1);
                    break;

                default:
                    goto slow_jcc;
            }
            break;

        /* some jumps are easy to compute */
        case CC_OP_ADDB:
        case CC_OP_ADDW:
        case CC_OP_ADDL:
        case CC_OP_ADDQ:

        case CC_OP_ADCB:
        case CC_OP_ADCW:
        case CC_OP_ADCL:
        case CC_OP_ADCQ:

        case CC_OP_SBBB:
        case CC_OP_SBBW:
        case CC_OP_SBBL:
        case CC_OP_SBBQ:

        case CC_OP_LOGICB:
        case CC_OP_LOGICW:
        case CC_OP_LOGICL:
        case CC_OP_LOGICQ:

        case CC_OP_INCB:
        case CC_OP_INCW:
        case CC_OP_INCL:
        case CC_OP_INCQ:

        case CC_OP_DECB:
        case CC_OP_DECW:
        case CC_OP_DECL:
        case CC_OP_DECQ:

        case CC_OP_SHLB:
        case CC_OP_SHLW:
        case CC_OP_SHLL:
        case CC_OP_SHLQ:

        case CC_OP_SARB:
        case CC_OP_SARW:
        case CC_OP_SARL:
        case CC_OP_SARQ:
            switch(jcc_op) {
                case JCC_Z:
                    size = (cc_op - CC_OP_ADDB) & 3;
                    goto fast_jcc_z;
                case JCC_S:
                    size = (cc_op - CC_OP_ADDB) & 3;
                    goto fast_jcc_s;
                default:
                    goto slow_jcc;
            }
            break;
        default:
        slow_jcc:
            gen_setcc_slow_T0(s, jcc_op);
            tcg_gen_brcondi_tl(inv ? TCG_COND_EQ : TCG_COND_NE, cpu_T[0], 0, l1);
            break;
    }
}

/* XXX: does not work with gdbstub "ice" single step - not a
   serious problem */
static int gen_jz_ecx_string(DisasContext *s, target_ulong next_eip)
{
    int l1, l2;

    l1 = gen_new_label();
    l2 = gen_new_label();
    gen_op_jnz_ecx(s->aflag, l1);
    gen_set_label(l2);
    gen_jmp_tb(s, next_eip, 1);
    gen_set_label(l1);
    return l2;
}

static inline void gen_stos(DisasContext *s, int ot)
{
    gen_op_mov_TN_reg(OT_LONG, 0, R_EAX);
    gen_string_movl_A0_EDI(s);
    gen_op_st_T0_A0(ot + s->base.mem_idx);
    gen_op_movl_T0_Dshift(ot);
    gen_op_add_reg_T0(s->aflag, R_EDI);
}

static inline void gen_lods(DisasContext *s, int ot)
{
    gen_string_movl_A0_ESI(s);
    gen_op_ld_T0_A0(ot + s->base.mem_idx);
    gen_op_mov_reg_T0(ot, R_EAX);
    gen_op_movl_T0_Dshift(ot);
    gen_op_add_reg_T0(s->aflag, R_ESI);
}

static inline void gen_scas(DisasContext *s, int ot)
{
    gen_op_mov_TN_reg(OT_LONG, 0, R_EAX);
    gen_string_movl_A0_EDI(s);
    gen_op_ld_T1_A0(ot + s->base.mem_idx);
    gen_op_cmpl_T0_T1_cc();
    gen_op_movl_T0_Dshift(ot);
    gen_op_add_reg_T0(s->aflag, R_EDI);
}

static inline void gen_cmps(DisasContext *s, int ot)
{
    gen_string_movl_A0_ESI(s);
    gen_op_ld_T0_A0(ot + s->base.mem_idx);
    gen_string_movl_A0_EDI(s);
    gen_op_ld_T1_A0(ot + s->base.mem_idx);
    gen_op_cmpl_T0_T1_cc();
    gen_op_movl_T0_Dshift(ot);
    gen_op_add_reg_T0(s->aflag, R_ESI);
    gen_op_add_reg_T0(s->aflag, R_EDI);
}

static inline void gen_ins(DisasContext *s, int ot)
{
    gen_string_movl_A0_EDI(s);
    /* Note: we must do this dummy write first to be restartable in
       case of page fault. */
    gen_op_movl_T0_0();
    gen_op_st_T0_A0(ot + s->base.mem_idx);
    gen_op_mov_TN_reg(OT_WORD, 1, R_EDX);
    tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_T[1]);
    tcg_gen_andi_i32(cpu_tmp2_i32, cpu_tmp2_i32, 0xffff);
    gen_helper_in_func(ot, cpu_T[0], cpu_tmp2_i32);
    gen_op_st_T0_A0(ot + s->base.mem_idx);
    gen_op_movl_T0_Dshift(ot);
    gen_op_add_reg_T0(s->aflag, R_EDI);
}

static inline void gen_outs(DisasContext *s, int ot)
{
    gen_string_movl_A0_ESI(s);
    gen_op_ld_T0_A0(ot + s->base.mem_idx);

    gen_op_mov_TN_reg(OT_WORD, 1, R_EDX);
    tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_T[1]);
    tcg_gen_andi_i32(cpu_tmp2_i32, cpu_tmp2_i32, 0xffff);
    tcg_gen_trunc_tl_i32(cpu_tmp3_i32, cpu_T[0]);
    gen_helper_out_func(ot, cpu_tmp2_i32, cpu_tmp3_i32);

    gen_op_movl_T0_Dshift(ot);
    gen_op_add_reg_T0(s->aflag, R_ESI);
}

/* same method as Valgrind : we generate jumps to current or next
   instruction */
#define GEN_REPZ(op)                                                                                       \
    static inline void gen_repz_##op(DisasContext *s, int ot, target_ulong cur_eip, target_ulong next_eip) \
    {                                                                                                      \
        int l2;                                                                                            \
        gen_update_cc_op(s);                                                                               \
        l2 = gen_jz_ecx_string(s, next_eip);                                                               \
        gen_##op(s, ot);                                                                                   \
        gen_op_add_reg_im(s->aflag, R_ECX, -1);                                                            \
        /* a loop would cause two single step exceptions if ECX = 1                                        \
           before rep string_insn */                                                                       \
        if(!s->jmp_opt)                                                                                    \
            gen_op_jz_ecx(s->aflag, l2);                                                                   \
        gen_jmp(s, cur_eip);                                                                               \
    }

#define GEN_REPZ2(op)                                                                                              \
    static inline void gen_repz_##op(DisasContext *s, int ot, target_ulong cur_eip, target_ulong next_eip, int nz) \
    {                                                                                                              \
        int l2;                                                                                                    \
        gen_update_cc_op(s);                                                                                       \
        l2 = gen_jz_ecx_string(s, next_eip);                                                                       \
        gen_##op(s, ot);                                                                                           \
        gen_op_add_reg_im(s->aflag, R_ECX, -1);                                                                    \
        gen_op_set_cc_op(CC_OP_SUBB + ot);                                                                         \
        gen_jcc1(s, CC_OP_SUBB + ot, (JCC_Z << 1) | (nz ^ 1), l2);                                                 \
        if(!s->jmp_opt)                                                                                            \
            gen_op_jz_ecx(s->aflag, l2);                                                                           \
        gen_jmp(s, cur_eip);                                                                                       \
    }

GEN_REPZ(movs)
GEN_REPZ(stos)
GEN_REPZ(lods)
GEN_REPZ(ins)
GEN_REPZ(outs)
GEN_REPZ2(scas)
GEN_REPZ2(cmps)

static void gen_helper_fp_arith_ST0_FT0(int op)
{
    switch(op) {
        case 0:
            gen_helper_fadd_ST0_FT0();
            break;
        case 1:
            gen_helper_fmul_ST0_FT0();
            break;
        case 2:
            gen_helper_fcom_ST0_FT0();
            break;
        case 3:
            gen_helper_fcom_ST0_FT0();
            break;
        case 4:
            gen_helper_fsub_ST0_FT0();
            break;
        case 5:
            gen_helper_fsubr_ST0_FT0();
            break;
        case 6:
            gen_helper_fdiv_ST0_FT0();
            break;
        case 7:
            gen_helper_fdivr_ST0_FT0();
            break;
    }
}

/* NOTE the exception in "r" op ordering */
static void gen_helper_fp_arith_STN_ST0(int op, int opreg)
{
    TCGv_i32 tmp = tcg_const_i32(opreg);
    switch(op) {
        case 0:
            gen_helper_fadd_STN_ST0(tmp);
            break;
        case 1:
            gen_helper_fmul_STN_ST0(tmp);
            break;
        case 4:
            gen_helper_fsubr_STN_ST0(tmp);
            break;
        case 5:
            gen_helper_fsub_STN_ST0(tmp);
            break;
        case 6:
            gen_helper_fdivr_STN_ST0(tmp);
            break;
        case 7:
            gen_helper_fdiv_STN_ST0(tmp);
            break;
    }
}

/* if d == OR_TMP0, it means memory operand (address in A0) */
static void gen_op(DisasContext *s, int op, int ot, int d)
{
    if(d != OR_TMP0) {
        gen_op_mov_TN_reg(ot, 0, d);
    } else {
        gen_op_ld_T0_A0(ot + s->base.mem_idx);
    }
    switch(op) {
        case OP_ADCL:
            if(s->cc_op != CC_OP_DYNAMIC) {
                gen_op_set_cc_op(s->cc_op);
            }
            gen_compute_eflags_c(cpu_tmp4);
            tcg_gen_add_tl(cpu_T[0], cpu_T[0], cpu_T[1]);
            tcg_gen_add_tl(cpu_T[0], cpu_T[0], cpu_tmp4);
            if(d != OR_TMP0) {
                gen_op_mov_reg_T0(ot, d);
            } else {
                gen_op_st_T0_A0(ot + s->base.mem_idx);
            }
            tcg_gen_mov_tl(cpu_cc_src, cpu_T[1]);
            tcg_gen_mov_tl(cpu_cc_dst, cpu_T[0]);
            tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_tmp4);
            tcg_gen_shli_i32(cpu_tmp2_i32, cpu_tmp2_i32, 2);
            tcg_gen_addi_i32(cpu_cc_op, cpu_tmp2_i32, CC_OP_ADDB + ot);
            s->cc_op = CC_OP_DYNAMIC;
            break;
        case OP_SBBL:
            if(s->cc_op != CC_OP_DYNAMIC) {
                gen_op_set_cc_op(s->cc_op);
            }
            gen_compute_eflags_c(cpu_tmp4);
            tcg_gen_sub_tl(cpu_T[0], cpu_T[0], cpu_T[1]);
            tcg_gen_sub_tl(cpu_T[0], cpu_T[0], cpu_tmp4);
            if(d != OR_TMP0) {
                gen_op_mov_reg_T0(ot, d);
            } else {
                gen_op_st_T0_A0(ot + s->base.mem_idx);
            }
            tcg_gen_mov_tl(cpu_cc_src, cpu_T[1]);
            tcg_gen_mov_tl(cpu_cc_dst, cpu_T[0]);
            tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_tmp4);
            tcg_gen_shli_i32(cpu_tmp2_i32, cpu_tmp2_i32, 2);
            tcg_gen_addi_i32(cpu_cc_op, cpu_tmp2_i32, CC_OP_SUBB + ot);
            s->cc_op = CC_OP_DYNAMIC;
            break;
        case OP_ADDL:
            gen_op_addl_T0_T1();
            if(d != OR_TMP0) {
                gen_op_mov_reg_T0(ot, d);
            } else {
                gen_op_st_T0_A0(ot + s->base.mem_idx);
            }
            gen_op_update2_cc();
            s->cc_op = CC_OP_ADDB + ot;
            break;
        case OP_SUBL:
            tcg_gen_sub_tl(cpu_T[0], cpu_T[0], cpu_T[1]);
            if(d != OR_TMP0) {
                gen_op_mov_reg_T0(ot, d);
            } else {
                gen_op_st_T0_A0(ot + s->base.mem_idx);
            }
            gen_op_update2_cc();
            s->cc_op = CC_OP_SUBB + ot;
            break;
        default:
        case OP_ANDL:
            tcg_gen_and_tl(cpu_T[0], cpu_T[0], cpu_T[1]);
            if(d != OR_TMP0) {
                gen_op_mov_reg_T0(ot, d);
            } else {
                gen_op_st_T0_A0(ot + s->base.mem_idx);
            }
            gen_op_update1_cc();
            s->cc_op = CC_OP_LOGICB + ot;
            break;
        case OP_ORL:
            tcg_gen_or_tl(cpu_T[0], cpu_T[0], cpu_T[1]);
            if(d != OR_TMP0) {
                gen_op_mov_reg_T0(ot, d);
            } else {
                gen_op_st_T0_A0(ot + s->base.mem_idx);
            }
            gen_op_update1_cc();
            s->cc_op = CC_OP_LOGICB + ot;
            break;
        case OP_XORL:
            tcg_gen_xor_tl(cpu_T[0], cpu_T[0], cpu_T[1]);
            if(d != OR_TMP0) {
                gen_op_mov_reg_T0(ot, d);
            } else {
                gen_op_st_T0_A0(ot + s->base.mem_idx);
            }
            gen_op_update1_cc();
            s->cc_op = CC_OP_LOGICB + ot;
            break;
        case OP_CMPL:
            gen_op_cmpl_T0_T1_cc();
            s->cc_op = CC_OP_SUBB + ot;
            break;
    }
}

/* if d == OR_TMP0, it means memory operand (address in A0) */
static void gen_inc(DisasContext *s, int ot, int d, int c)
{
    if(d != OR_TMP0) {
        gen_op_mov_TN_reg(ot, 0, d);
    } else {
        gen_op_ld_T0_A0(ot + s->base.mem_idx);
    }
    if(s->cc_op != CC_OP_DYNAMIC) {
        gen_op_set_cc_op(s->cc_op);
    }
    if(c > 0) {
        tcg_gen_addi_tl(cpu_T[0], cpu_T[0], 1);
        s->cc_op = CC_OP_INCB + ot;
    } else {
        tcg_gen_addi_tl(cpu_T[0], cpu_T[0], -1);
        s->cc_op = CC_OP_DECB + ot;
    }
    if(d != OR_TMP0) {
        gen_op_mov_reg_T0(ot, d);
    } else {
        gen_op_st_T0_A0(ot + s->base.mem_idx);
    }
    gen_compute_eflags_c(cpu_cc_src);
    tcg_gen_mov_tl(cpu_cc_dst, cpu_T[0]);
}

static void gen_shift_rm_T1(DisasContext *s, int ot, int op1, int is_right, int is_arith)
{
    target_ulong mask;
    int shift_label;
    TCGv t0, t1, t2;

    if(ot == OT_QUAD) {
        mask = 0x3f;
    } else {
        mask = 0x1f;
    }

    /* load */
    if(op1 == OR_TMP0) {
        gen_op_ld_T0_A0(ot + s->base.mem_idx);
    } else {
        gen_op_mov_TN_reg(ot, 0, op1);
    }

    t0 = tcg_temp_local_new();
    t1 = tcg_temp_local_new();
    t2 = tcg_temp_local_new();

    tcg_gen_andi_tl(t2, cpu_T[1], mask);

    if(is_right) {
        if(is_arith) {
            gen_exts(ot, cpu_T[0]);
            tcg_gen_mov_tl(t0, cpu_T[0]);
            tcg_gen_sar_tl(cpu_T[0], cpu_T[0], t2);
        } else {
            gen_extu(ot, cpu_T[0]);
            tcg_gen_mov_tl(t0, cpu_T[0]);
            tcg_gen_shr_tl(cpu_T[0], cpu_T[0], t2);
        }
    } else {
        tcg_gen_mov_tl(t0, cpu_T[0]);
        tcg_gen_shl_tl(cpu_T[0], cpu_T[0], t2);
    }

    /* store */
    if(op1 == OR_TMP0) {
        gen_op_st_T0_A0(ot + s->base.mem_idx);
    } else {
        gen_op_mov_reg_T0(ot, op1);
    }

    /* update eflags if non zero shift */
    if(s->cc_op != CC_OP_DYNAMIC) {
        gen_op_set_cc_op(s->cc_op);
    }

    tcg_gen_mov_tl(t1, cpu_T[0]);

    shift_label = gen_new_label();
    tcg_gen_brcondi_tl(TCG_COND_EQ, t2, 0, shift_label);

    tcg_gen_addi_tl(t2, t2, -1);
    tcg_gen_mov_tl(cpu_cc_dst, t1);

    if(is_right) {
        if(is_arith) {
            tcg_gen_sar_tl(cpu_cc_src, t0, t2);
        } else {
            tcg_gen_shr_tl(cpu_cc_src, t0, t2);
        }
    } else {
        tcg_gen_shl_tl(cpu_cc_src, t0, t2);
    }

    if(is_right) {
        tcg_gen_movi_i32(cpu_cc_op, CC_OP_SARB + ot);
    } else {
        tcg_gen_movi_i32(cpu_cc_op, CC_OP_SHLB + ot);
    }

    gen_set_label(shift_label);
    s->cc_op = CC_OP_DYNAMIC; /* cannot predict flags after */

    tcg_temp_free(t0);
    tcg_temp_free(t1);
    tcg_temp_free(t2);
}

static void gen_shift_rm_im(DisasContext *s, int ot, int op1, int op2, int is_right, int is_arith)
{
    int mask;

    if(ot == OT_QUAD) {
        mask = 0x3f;
    } else {
        mask = 0x1f;
    }

    /* load */
    if(op1 == OR_TMP0) {
        gen_op_ld_T0_A0(ot + s->base.mem_idx);
    } else {
        gen_op_mov_TN_reg(ot, 0, op1);
    }

    op2 &= mask;
    if(op2 != 0) {
        if(is_right) {
            if(is_arith) {
                gen_exts(ot, cpu_T[0]);
                tcg_gen_sari_tl(cpu_tmp4, cpu_T[0], op2 - 1);
                tcg_gen_sari_tl(cpu_T[0], cpu_T[0], op2);
            } else {
                gen_extu(ot, cpu_T[0]);
                tcg_gen_shri_tl(cpu_tmp4, cpu_T[0], op2 - 1);
                tcg_gen_shri_tl(cpu_T[0], cpu_T[0], op2);
            }
        } else {
            tcg_gen_shli_tl(cpu_tmp4, cpu_T[0], op2 - 1);
            tcg_gen_shli_tl(cpu_T[0], cpu_T[0], op2);
        }
    }

    /* store */
    if(op1 == OR_TMP0) {
        gen_op_st_T0_A0(ot + s->base.mem_idx);
    } else {
        gen_op_mov_reg_T0(ot, op1);
    }

    /* update eflags if non zero shift */
    if(op2 != 0) {
        tcg_gen_mov_tl(cpu_cc_src, cpu_tmp4);
        tcg_gen_mov_tl(cpu_cc_dst, cpu_T[0]);
        if(is_right) {
            s->cc_op = CC_OP_SARB + ot;
        } else {
            s->cc_op = CC_OP_SHLB + ot;
        }
    }
}

static inline void tcg_gen_lshift(TCGv ret, TCGv arg1, target_long arg2)
{
    if(arg2 >= 0) {
        tcg_gen_shli_tl(ret, arg1, arg2);
    } else {
        tcg_gen_shri_tl(ret, arg1, -arg2);
    }
}

static void gen_rot_rm_T1(DisasContext *s, int ot, int op1, int is_right)
{
    target_ulong mask;
    int label1, label2, data_bits;
    TCGv t0, t1, t2, a0;

    /* XXX: inefficient, but we must use local temps */
    t0 = tcg_temp_local_new();
    t1 = tcg_temp_local_new();
    t2 = tcg_temp_local_new();
    a0 = tcg_temp_local_new();

    if(ot == OT_QUAD) {
        mask = 0x3f;
    } else {
        mask = 0x1f;
    }

    /* load */
    if(op1 == OR_TMP0) {
        tcg_gen_mov_tl(a0, cpu_A0);
        gen_op_ld_v(ot + s->base.mem_idx, t0, a0);
    } else {
        gen_op_mov_v_reg(ot, t0, op1);
    }

    tcg_gen_mov_tl(t1, cpu_T[1]);

    tcg_gen_andi_tl(t1, t1, mask);

    /* Must test zero case to avoid using undefined behaviour in TCG
       shifts. */
    label1 = gen_new_label();
    tcg_gen_brcondi_tl(TCG_COND_EQ, t1, 0, label1);

    if(ot <= OT_WORD) {
        tcg_gen_andi_tl(cpu_tmp0, t1, (1 << (3 + ot)) - 1);
    } else {
        tcg_gen_mov_tl(cpu_tmp0, t1);
    }

    gen_extu(ot, t0);
    tcg_gen_mov_tl(t2, t0);

    data_bits = 8 << ot;
    /* XXX: rely on behaviour of shifts when operand 2 overflows (XXX:
       fix TCG definition) */
    if(is_right) {
        tcg_gen_shr_tl(cpu_tmp4, t0, cpu_tmp0);
        tcg_gen_subfi_tl(cpu_tmp0, data_bits, cpu_tmp0);
        tcg_gen_shl_tl(t0, t0, cpu_tmp0);
    } else {
        tcg_gen_shl_tl(cpu_tmp4, t0, cpu_tmp0);
        tcg_gen_subfi_tl(cpu_tmp0, data_bits, cpu_tmp0);
        tcg_gen_shr_tl(t0, t0, cpu_tmp0);
    }
    tcg_gen_or_tl(t0, t0, cpu_tmp4);

    gen_set_label(label1);
    /* store */
    if(op1 == OR_TMP0) {
        gen_op_st_v(ot + s->base.mem_idx, t0, a0);
    } else {
        gen_op_mov_reg_v(ot, op1, t0);
    }

    /* update eflags */
    if(s->cc_op != CC_OP_DYNAMIC) {
        gen_op_set_cc_op(s->cc_op);
    }

    label2 = gen_new_label();
    tcg_gen_brcondi_tl(TCG_COND_EQ, t1, 0, label2);

    gen_compute_eflags(cpu_cc_src);
    tcg_gen_andi_tl(cpu_cc_src, cpu_cc_src, ~(CC_O | CC_C));
    tcg_gen_xor_tl(cpu_tmp0, t2, t0);
    tcg_gen_lshift(cpu_tmp0, cpu_tmp0, 11 - (data_bits - 1));
    tcg_gen_andi_tl(cpu_tmp0, cpu_tmp0, CC_O);
    tcg_gen_or_tl(cpu_cc_src, cpu_cc_src, cpu_tmp0);
    if(is_right) {
        tcg_gen_shri_tl(t0, t0, data_bits - 1);
    }
    tcg_gen_andi_tl(t0, t0, CC_C);
    tcg_gen_or_tl(cpu_cc_src, cpu_cc_src, t0);

    tcg_gen_discard_tl(cpu_cc_dst);
    tcg_gen_movi_i32(cpu_cc_op, CC_OP_EFLAGS);

    gen_set_label(label2);
    s->cc_op = CC_OP_DYNAMIC; /* cannot predict flags after */

    tcg_temp_free(t0);
    tcg_temp_free(t1);
    tcg_temp_free(t2);
    tcg_temp_free(a0);
}

static void gen_rot_rm_im(DisasContext *s, int ot, int op1, int op2, int is_right)
{
    int mask;
    int data_bits;
    TCGv t0, t1, a0;

    /* XXX: inefficient, but we must use local temps */
    t0 = tcg_temp_local_new();
    t1 = tcg_temp_local_new();
    a0 = tcg_temp_local_new();

    if(ot == OT_QUAD) {
        mask = 0x3f;
    } else {
        mask = 0x1f;
    }

    /* load */
    if(op1 == OR_TMP0) {
        tcg_gen_mov_tl(a0, cpu_A0);
        gen_op_ld_v(ot + s->base.mem_idx, t0, a0);
    } else {
        gen_op_mov_v_reg(ot, t0, op1);
    }

    gen_extu(ot, t0);
    tcg_gen_mov_tl(t1, t0);

    op2 &= mask;
    data_bits = 8 << ot;
    if(op2 != 0) {
        int shift = op2 & ((1 << (3 + ot)) - 1);
        if(is_right) {
            tcg_gen_shri_tl(cpu_tmp4, t0, shift);
            tcg_gen_shli_tl(t0, t0, data_bits - shift);
        } else {
            tcg_gen_shli_tl(cpu_tmp4, t0, shift);
            tcg_gen_shri_tl(t0, t0, data_bits - shift);
        }
        tcg_gen_or_tl(t0, t0, cpu_tmp4);
    }

    /* store */
    if(op1 == OR_TMP0) {
        gen_op_st_v(ot + s->base.mem_idx, t0, a0);
    } else {
        gen_op_mov_reg_v(ot, op1, t0);
    }

    if(op2 != 0) {
        /* update eflags */
        if(s->cc_op != CC_OP_DYNAMIC) {
            gen_op_set_cc_op(s->cc_op);
        }

        gen_compute_eflags(cpu_cc_src);
        tcg_gen_andi_tl(cpu_cc_src, cpu_cc_src, ~(CC_O | CC_C));
        tcg_gen_xor_tl(cpu_tmp0, t1, t0);
        tcg_gen_lshift(cpu_tmp0, cpu_tmp0, 11 - (data_bits - 1));
        tcg_gen_andi_tl(cpu_tmp0, cpu_tmp0, CC_O);
        tcg_gen_or_tl(cpu_cc_src, cpu_cc_src, cpu_tmp0);
        if(is_right) {
            tcg_gen_shri_tl(t0, t0, data_bits - 1);
        }
        tcg_gen_andi_tl(t0, t0, CC_C);
        tcg_gen_or_tl(cpu_cc_src, cpu_cc_src, t0);

        tcg_gen_discard_tl(cpu_cc_dst);
        tcg_gen_movi_i32(cpu_cc_op, CC_OP_EFLAGS);
        s->cc_op = CC_OP_EFLAGS;
    }

    tcg_temp_free(t0);
    tcg_temp_free(t1);
    tcg_temp_free(a0);
}

/* XXX: add faster immediate = 1 case */
static void gen_rotc_rm_T1(DisasContext *s, int ot, int op1, int is_right)
{
    int label1;

    if(s->cc_op != CC_OP_DYNAMIC) {
        gen_op_set_cc_op(s->cc_op);
    }

    /* load */
    if(op1 == OR_TMP0) {
        gen_op_ld_T0_A0(ot + s->base.mem_idx);
    } else {
        gen_op_mov_TN_reg(ot, 0, op1);
    }

    if(is_right) {
        switch(ot) {
            case 0:
                gen_helper_rcrb(cpu_T[0], cpu_T[0], cpu_T[1]);
                break;
            case 1:
                gen_helper_rcrw(cpu_T[0], cpu_T[0], cpu_T[1]);
                break;
            case 2:
                gen_helper_rcrl(cpu_T[0], cpu_T[0], cpu_T[1]);
                break;
#ifdef TARGET_X86_64
            case 3:
                gen_helper_rcrq(cpu_T[0], cpu_T[0], cpu_T[1]);
                break;
#endif
        }
    } else {
        switch(ot) {
            case 0:
                gen_helper_rclb(cpu_T[0], cpu_T[0], cpu_T[1]);
                break;
            case 1:
                gen_helper_rclw(cpu_T[0], cpu_T[0], cpu_T[1]);
                break;
            case 2:
                gen_helper_rcll(cpu_T[0], cpu_T[0], cpu_T[1]);
                break;
#ifdef TARGET_X86_64
            case 3:
                gen_helper_rclq(cpu_T[0], cpu_T[0], cpu_T[1]);
                break;
#endif
        }
    }
    /* store */
    if(op1 == OR_TMP0) {
        gen_op_st_T0_A0(ot + s->base.mem_idx);
    } else {
        gen_op_mov_reg_T0(ot, op1);
    }

    /* update eflags */
    label1 = gen_new_label();
    tcg_gen_brcondi_tl(TCG_COND_EQ, cpu_cc_tmp, -1, label1);

    tcg_gen_mov_tl(cpu_cc_src, cpu_cc_tmp);
    tcg_gen_discard_tl(cpu_cc_dst);
    tcg_gen_movi_i32(cpu_cc_op, CC_OP_EFLAGS);

    gen_set_label(label1);
    s->cc_op = CC_OP_DYNAMIC; /* cannot predict flags after */
}

/* XXX: add faster immediate case */
static void gen_shiftd_rm_T1_T3(DisasContext *s, int ot, int op1, int is_right)
{
    int label1, label2, data_bits;
    target_ulong mask;
    TCGv t0, t1, t2, a0;

    t0 = tcg_temp_local_new();
    t1 = tcg_temp_local_new();
    t2 = tcg_temp_local_new();
    a0 = tcg_temp_local_new();

    if(ot == OT_QUAD) {
        mask = 0x3f;
    } else {
        mask = 0x1f;
    }

    /* load */
    if(op1 == OR_TMP0) {
        tcg_gen_mov_tl(a0, cpu_A0);
        gen_op_ld_v(ot + s->base.mem_idx, t0, a0);
    } else {
        gen_op_mov_v_reg(ot, t0, op1);
    }

    tcg_gen_andi_tl(cpu_T3, cpu_T3, mask);

    tcg_gen_mov_tl(t1, cpu_T[1]);
    tcg_gen_mov_tl(t2, cpu_T3);

    /* Must test zero case to avoid using undefined behaviour in TCG
       shifts. */
    label1 = gen_new_label();
    tcg_gen_brcondi_tl(TCG_COND_EQ, t2, 0, label1);

    tcg_gen_addi_tl(cpu_tmp5, t2, -1);
    if(ot == OT_WORD) {
        /* Note: we implement the Intel behaviour for shift count > 16 */
        if(is_right) {
            tcg_gen_andi_tl(t0, t0, 0xffff);
            tcg_gen_shli_tl(cpu_tmp0, t1, 16);
            tcg_gen_or_tl(t0, t0, cpu_tmp0);
            tcg_gen_ext32u_tl(t0, t0);

            tcg_gen_shr_tl(cpu_tmp4, t0, cpu_tmp5);

            /* only needed if count > 16, but a test would complicate */
            tcg_gen_subfi_tl(cpu_tmp5, 32, t2);
            tcg_gen_shl_tl(cpu_tmp0, t0, cpu_tmp5);

            tcg_gen_shr_tl(t0, t0, t2);

            tcg_gen_or_tl(t0, t0, cpu_tmp0);
        } else {
            /* XXX: not optimal */
            tcg_gen_andi_tl(t0, t0, 0xffff);
            tcg_gen_shli_tl(t1, t1, 16);
            tcg_gen_or_tl(t1, t1, t0);
            tcg_gen_ext32u_tl(t1, t1);

            tcg_gen_shl_tl(cpu_tmp4, t0, cpu_tmp5);
            tcg_gen_subfi_tl(cpu_tmp0, 32, cpu_tmp5);
            tcg_gen_shr_tl(cpu_tmp5, t1, cpu_tmp0);
            tcg_gen_or_tl(cpu_tmp4, cpu_tmp4, cpu_tmp5);

            tcg_gen_shl_tl(t0, t0, t2);
            tcg_gen_subfi_tl(cpu_tmp5, 32, t2);
            tcg_gen_shr_tl(t1, t1, cpu_tmp5);
            tcg_gen_or_tl(t0, t0, t1);
        }
    } else {
        data_bits = 8 << ot;
        if(is_right) {
            if(ot == OT_LONG) {
                tcg_gen_ext32u_tl(t0, t0);
            }

            tcg_gen_shr_tl(cpu_tmp4, t0, cpu_tmp5);

            tcg_gen_shr_tl(t0, t0, t2);
            tcg_gen_subfi_tl(cpu_tmp5, data_bits, t2);
            tcg_gen_shl_tl(t1, t1, cpu_tmp5);
            tcg_gen_or_tl(t0, t0, t1);

        } else {
            if(ot == OT_LONG) {
                tcg_gen_ext32u_tl(t1, t1);
            }

            tcg_gen_shl_tl(cpu_tmp4, t0, cpu_tmp5);

            tcg_gen_shl_tl(t0, t0, t2);
            tcg_gen_subfi_tl(cpu_tmp5, data_bits, t2);
            tcg_gen_shr_tl(t1, t1, cpu_tmp5);
            tcg_gen_or_tl(t0, t0, t1);
        }
    }
    tcg_gen_mov_tl(t1, cpu_tmp4);

    gen_set_label(label1);
    /* store */
    if(op1 == OR_TMP0) {
        gen_op_st_v(ot + s->base.mem_idx, t0, a0);
    } else {
        gen_op_mov_reg_v(ot, op1, t0);
    }

    /* update eflags */
    if(s->cc_op != CC_OP_DYNAMIC) {
        gen_op_set_cc_op(s->cc_op);
    }

    label2 = gen_new_label();
    tcg_gen_brcondi_tl(TCG_COND_EQ, t2, 0, label2);

    tcg_gen_mov_tl(cpu_cc_src, t1);
    tcg_gen_mov_tl(cpu_cc_dst, t0);
    if(is_right) {
        tcg_gen_movi_i32(cpu_cc_op, CC_OP_SARB + ot);
    } else {
        tcg_gen_movi_i32(cpu_cc_op, CC_OP_SHLB + ot);
    }
    gen_set_label(label2);
    s->cc_op = CC_OP_DYNAMIC; /* cannot predict flags after */

    tcg_temp_free(t0);
    tcg_temp_free(t1);
    tcg_temp_free(t2);
    tcg_temp_free(a0);
}

static void gen_shift(DisasContext *s1, int op, int ot, int d, int s)
{
    if(s != OR_TMP1) {
        gen_op_mov_TN_reg(ot, 1, s);
    }
    switch(op) {
        case OP_ROL:
            gen_rot_rm_T1(s1, ot, d, 0);
            break;
        case OP_ROR:
            gen_rot_rm_T1(s1, ot, d, 1);
            break;
        case OP_SHL:
        case OP_SHL1:
            gen_shift_rm_T1(s1, ot, d, 0, 0);
            break;
        case OP_SHR:
            gen_shift_rm_T1(s1, ot, d, 1, 0);
            break;
        case OP_SAR:
            gen_shift_rm_T1(s1, ot, d, 1, 1);
            break;
        case OP_RCL:
            gen_rotc_rm_T1(s1, ot, d, 0);
            break;
        case OP_RCR:
            gen_rotc_rm_T1(s1, ot, d, 1);
            break;
    }
}

static void gen_shifti(DisasContext *s1, int op, int ot, int d, int c)
{
    switch(op) {
        case OP_ROL:
            gen_rot_rm_im(s1, ot, d, c, 0);
            break;
        case OP_ROR:
            gen_rot_rm_im(s1, ot, d, c, 1);
            break;
        case OP_SHL:
        case OP_SHL1:
            gen_shift_rm_im(s1, ot, d, c, 0, 0);
            break;
        case OP_SHR:
            gen_shift_rm_im(s1, ot, d, c, 1, 0);
            break;
        case OP_SAR:
            gen_shift_rm_im(s1, ot, d, c, 1, 1);
            break;
        default:
            /* currently not optimized */
            gen_op_movl_T1_im(c);
            gen_shift(s1, op, ot, d, OR_TMP1);
            break;
    }
}

void gen_sync_pc(DisasContext *dc)
{
    target_ulong cur_eip = dc->base.pc - dc->cs_base;
    gen_jmp_im(cur_eip);
}

static void gen_lea_modrm(DisasContext *s, int modrm, int *reg_ptr, int *offset_ptr)
{
    target_long disp;
    int havesib;
    int base;
    int index;
    int scale;
    int opreg;
    int mod, rm, code, override, must_add_seg;

    override = s->override;
    must_add_seg = s->addseg;
    if(override >= 0) {
        must_add_seg = 1;
    }
    mod = (modrm >> 6) & 3;
    rm = modrm & 7;

    if(s->aflag) {

        havesib = 0;
        base = rm;
        index = 0;
        scale = 0;

        if(base == 4) {
            havesib = 1;
            code = ldub_code(s->base.pc++);
            scale = (code >> 6) & 3;
            index = ((code >> 3) & 7) | REX_X(s);
            base = (code & 7);
        }
        base |= REX_B(s);

        switch(mod) {
            case 0:
                if((base & 7) == 5) {
                    base = -1;
                    disp = (int32_t)ldl_code(s->base.pc);
                    s->base.pc += 4;
                    if(CODE64(s) && !havesib) {
                        disp += s->base.pc + s->rip_offset;
                    }
                } else {
                    disp = 0;
                }
                break;
            case 1:
                disp = (int8_t)ldub_code(s->base.pc++);
                break;
            default:
            case 2:
                disp = (int32_t)ldl_code(s->base.pc);
                s->base.pc += 4;
                break;
        }

        if(base >= 0) {
            /* for correct popl handling with esp */
            if(base == 4 && s->popl_esp_hack) {
                disp += s->popl_esp_hack;
            }
#ifdef TARGET_X86_64
            if(s->aflag == 2) {
                gen_op_movq_A0_reg(base);
                if(disp != 0) {
                    gen_op_addq_A0_im(disp);
                }
            } else
#endif
            {
                gen_op_movl_A0_reg(base);
                if(disp != 0) {
                    gen_op_addl_A0_im(disp);
                }
            }
        } else {
#ifdef TARGET_X86_64
            if(s->aflag == 2) {
                gen_op_movq_A0_im(disp);
            } else
#endif
            {
                gen_op_movl_A0_im(disp);
            }
        }
        /* index == 4 means no index */
        if(havesib && (index != 4)) {
#ifdef TARGET_X86_64
            if(s->aflag == 2) {
                gen_op_addq_A0_reg_sN(scale, index);
            } else
#endif
            {
                gen_op_addl_A0_reg_sN(scale, index);
            }
        }
        if(must_add_seg) {
            if(override < 0) {
                if(base == R_EBP || base == R_ESP) {
                    override = R_SS;
                } else {
                    override = R_DS;
                }
            }
#ifdef TARGET_X86_64
            if(s->aflag == 2) {
                gen_op_addq_A0_seg(override);
            } else
#endif
            {
                gen_op_addl_A0_seg(override);
            }
        }
    } else {
        switch(mod) {
            case 0:
                if(rm == 6) {
                    disp = lduw_code(s->base.pc);
                    s->base.pc += 2;
                    gen_op_movl_A0_im(disp);
                    rm = 0; /* avoid SS override */
                    goto no_rm;
                } else {
                    disp = 0;
                }
                break;
            case 1:
                disp = (int8_t)ldub_code(s->base.pc++);
                break;
            default:
            case 2:
                disp = lduw_code(s->base.pc);
                s->base.pc += 2;
                break;
        }
        switch(rm) {
            case 0:
                gen_op_movl_A0_reg(R_EBX);
                gen_op_addl_A0_reg_sN(0, R_ESI);
                break;
            case 1:
                gen_op_movl_A0_reg(R_EBX);
                gen_op_addl_A0_reg_sN(0, R_EDI);
                break;
            case 2:
                gen_op_movl_A0_reg(R_EBP);
                gen_op_addl_A0_reg_sN(0, R_ESI);
                break;
            case 3:
                gen_op_movl_A0_reg(R_EBP);
                gen_op_addl_A0_reg_sN(0, R_EDI);
                break;
            case 4:
                gen_op_movl_A0_reg(R_ESI);
                break;
            case 5:
                gen_op_movl_A0_reg(R_EDI);
                break;
            case 6:
                gen_op_movl_A0_reg(R_EBP);
                break;
            default:
            case 7:
                gen_op_movl_A0_reg(R_EBX);
                break;
        }
        if(disp != 0) {
            gen_op_addl_A0_im(disp);
        }
        gen_op_andl_A0_ffff();
    no_rm:
        if(must_add_seg) {
            if(override < 0) {
                if(rm == 2 || rm == 3 || rm == 6) {
                    override = R_SS;
                } else {
                    override = R_DS;
                }
            }
            gen_op_addl_A0_seg(override);
        }
    }

    opreg = OR_A0;
    disp = 0;
    *reg_ptr = opreg;
    *offset_ptr = disp;
}

static void gen_nop_modrm(DisasContext *s, int modrm)
{
    int mod, rm, base, code;

    mod = (modrm >> 6) & 3;
    if(mod == 3) {
        return;
    }
    rm = modrm & 7;

    if(s->aflag) {

        base = rm;

        if(base == 4) {
            code = ldub_code(s->base.pc++);
            base = (code & 7);
        }

        switch(mod) {
            case 0:
                if(base == 5) {
                    s->base.pc += 4;
                }
                break;
            case 1:
                s->base.pc++;
                break;
            default:
            case 2:
                s->base.pc += 4;
                break;
        }
    } else {
        switch(mod) {
            case 0:
                if(rm == 6) {
                    s->base.pc += 2;
                }
                break;
            case 1:
                s->base.pc++;
                break;
            default:
            case 2:
                s->base.pc += 2;
                break;
        }
    }
}

/* used for LEA and MOV AX, mem */
static void gen_add_A0_ds_seg(DisasContext *s)
{
    int override, must_add_seg;
    must_add_seg = s->addseg;
    override = R_DS;
    if(s->override >= 0) {
        override = s->override;
        must_add_seg = 1;
    }
    if(must_add_seg) {
#ifdef TARGET_X86_64
        if(CODE64(s)) {
            gen_op_addq_A0_seg(override);
        } else
#endif
        {
            gen_op_addl_A0_seg(override);
        }
    }
}

/* generate modrm memory load or store of 'reg'. TMP0 is used if reg ==
   OR_TMP0 */
static void gen_ldst_modrm(DisasContext *s, int modrm, int ot, int reg, int is_store)
{
    int mod, rm, opreg, disp;

    mod = (modrm >> 6) & 3;
    rm = (modrm & 7) | REX_B(s);
    if(mod == 3) {
        if(is_store) {
            if(reg != OR_TMP0) {
                gen_op_mov_TN_reg(ot, 0, reg);
            }
            gen_op_mov_reg_T0(ot, rm);
        } else {
            gen_op_mov_TN_reg(ot, 0, rm);
            if(reg != OR_TMP0) {
                gen_op_mov_reg_T0(ot, reg);
            }
        }
    } else {
        gen_lea_modrm(s, modrm, &opreg, &disp);
        if(is_store) {
            if(reg != OR_TMP0) {
                gen_op_mov_TN_reg(ot, 0, reg);
            }
            gen_op_st_T0_A0(ot + s->base.mem_idx);
        } else {
            gen_op_ld_T0_A0(ot + s->base.mem_idx);
            if(reg != OR_TMP0) {
                gen_op_mov_reg_T0(ot, reg);
            }
        }
    }
}

static inline uint32_t insn_get(DisasContext *s, int ot)
{
    uint32_t ret;

    switch(ot) {
        case OT_BYTE:
            ret = ldub_code(s->base.pc);
            s->base.pc++;
            break;
        case OT_WORD:
            ret = lduw_code(s->base.pc);
            s->base.pc += 2;
            break;
        default:
        case OT_LONG:
            ret = ldl_code(s->base.pc);
            s->base.pc += 4;
            break;
    }
    return ret;
}

static inline int insn_const_size(unsigned int ot)
{
    if(ot <= OT_LONG) {
        return 1 << ot;
    } else {
        return 4;
    }
}

static inline void gen_goto_tb(DisasContext *s, int tb_num, target_ulong eip)
{
    TranslationBlock *tb;
    target_ulong pc;

    pc = s->cs_base + eip;
    tb = s->base.tb;
    /* NOTE: we handle the case where the TB spans two pages here */
    if((pc & TARGET_PAGE_MASK) == (tb->pc & TARGET_PAGE_MASK) ||
       (pc & TARGET_PAGE_MASK) == ((s->base.pc - 1) & TARGET_PAGE_MASK)) {
        /* jump to same page: we can use a direct jump */
        tcg_gen_goto_tb(tb_num);
        gen_jmp_im(eip);
        gen_exit_tb(tb, tb_num);
    } else {
        /* jump to another page: currently not optimized */
        gen_jmp_im(eip);
        gen_eob(s);
    }
}

static inline void gen_jcc(DisasContext *s, int b, target_ulong val, target_ulong next_eip)
{
    int l1, l2, cc_op;

    cc_op = s->cc_op;
    gen_update_cc_op(s);
    if(s->jmp_opt) {
        l1 = gen_new_label();
        gen_jcc1(s, cc_op, b, l1);

        gen_goto_tb(s, 0, next_eip);

        gen_set_label(l1);
        gen_goto_tb(s, 1, val);
        s->base.is_jmp = DISAS_TB_JUMP;
    } else {

        l1 = gen_new_label();
        l2 = gen_new_label();
        gen_jcc1(s, cc_op, b, l1);

        gen_jmp_im(next_eip);
        tcg_gen_br(l2);

        gen_set_label(l1);
        gen_jmp_im(val);
        gen_set_label(l2);
        gen_eob(s);
    }
}

static void gen_setcc(DisasContext *s, int b)
{
    int inv, jcc_op, l1;
    TCGv t0;

    if(is_fast_jcc_case(s, b)) {
        /* nominal case: we use a jump */
        /* XXX: make it faster by adding new instructions in TCG */
        t0 = tcg_temp_local_new();
        tcg_gen_movi_tl(t0, 0);
        l1 = gen_new_label();
        gen_jcc1(s, s->cc_op, b ^ 1, l1);
        tcg_gen_movi_tl(t0, 1);
        gen_set_label(l1);
        tcg_gen_mov_tl(cpu_T[0], t0);
        tcg_temp_free(t0);
    } else {
        /* slow case: it is more efficient not to generate a jump,
           although it is questionnable whether this optimization is
           worth to */
        inv = b & 1;
        jcc_op = (b >> 1) & 7;
        gen_setcc_slow_T0(s, jcc_op);
        if(inv) {
            tcg_gen_xori_tl(cpu_T[0], cpu_T[0], 1);
        }
    }
}

static inline void gen_op_movl_T0_seg(int seg_reg)
{
    tcg_gen_ld32u_tl(cpu_T[0], cpu_env, offsetof(CPUState, segs[seg_reg].selector));
}

static inline void gen_op_movl_seg_T0_vm(int seg_reg)
{
    tcg_gen_andi_tl(cpu_T[0], cpu_T[0], 0xffff);
    tcg_gen_st32_tl(cpu_T[0], cpu_env, offsetof(CPUState, segs[seg_reg].selector));
    tcg_gen_shli_tl(cpu_T[0], cpu_T[0], 4);
    tcg_gen_st_tl(cpu_T[0], cpu_env, offsetof(CPUState, segs[seg_reg].base));
}

/* move T0 to seg_reg and compute if the CPU state may change. Never
   call this function with seg_reg == R_CS */
static void gen_movl_seg_T0(DisasContext *s, int seg_reg, target_ulong cur_eip)
{
    if(s->pe && !s->vm86) {
        /* XXX: optimize by finding processor state dynamically */
        if(s->cc_op != CC_OP_DYNAMIC) {
            gen_op_set_cc_op(s->cc_op);
        }
        gen_jmp_im(cur_eip);
        tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_T[0]);
        gen_helper_load_seg(tcg_const_i32(seg_reg), cpu_tmp2_i32);
        /* abort translation because the addseg value may change or
           because ss32 may change. For R_SS, translation must always
           stop as a special handling must be done to disable hardware
           interrupts for the next instruction */
        if(seg_reg == R_SS || (s->code32 && seg_reg < R_FS)) {
            s->base.is_jmp = DISAS_TB_JUMP;
        }
    } else {
        gen_op_movl_seg_T0_vm(seg_reg);
        if(seg_reg == R_SS) {
            s->base.is_jmp = DISAS_TB_JUMP;
        }
    }
}

static inline int svm_is_rep(int prefixes)
{
    return ((prefixes & (PREFIX_REPZ | PREFIX_REPNZ)) ? 8 : 0);
}

static inline void gen_svm_check_intercept_param(DisasContext *s, target_ulong pc_start, uint32_t type, uint64_t param)
{
    /* no SVM activated; fast case */
    if(likely(!(s->flags & HF_SVMI_MASK))) {
        return;
    }
    if(s->cc_op != CC_OP_DYNAMIC) {
        gen_op_set_cc_op(s->cc_op);
    }
    gen_jmp_im(pc_start - s->cs_base);
    gen_helper_svm_check_intercept_param(tcg_const_i32(type), tcg_const_i64(param));
}

static inline void gen_svm_check_intercept(DisasContext *s, target_ulong pc_start, uint64_t type)
{
    gen_svm_check_intercept_param(s, pc_start, type, 0);
}

static inline void gen_stack_update(DisasContext *s, int addend)
{
#ifdef TARGET_X86_64
    if(CODE64(s)) {
        gen_op_add_reg_im(2, R_ESP, addend);
    } else
#endif
        if(s->ss32) {
        gen_op_add_reg_im(1, R_ESP, addend);
    } else {
        gen_op_add_reg_im(0, R_ESP, addend);
    }
}

/* generate a push. It depends on ss32, addseg and dflag */
static void gen_push_T0(DisasContext *s)
{
#ifdef TARGET_X86_64
    if(CODE64(s)) {
        gen_op_movq_A0_reg(R_ESP);
        if(s->dflag) {
            gen_op_addq_A0_im(-8);
            gen_op_st_T0_A0(OT_QUAD + s->base.mem_idx);
        } else {
            gen_op_addq_A0_im(-2);
            gen_op_st_T0_A0(OT_WORD + s->base.mem_idx);
        }
        gen_op_mov_reg_A0(2, R_ESP);
    } else
#endif
    {
        gen_op_movl_A0_reg(R_ESP);
        if(!s->dflag) {
            gen_op_addl_A0_im(-2);
        } else {
            gen_op_addl_A0_im(-4);
        }
        if(s->ss32) {
            if(s->addseg) {
                tcg_gen_mov_tl(cpu_T[1], cpu_A0);
                gen_op_addl_A0_seg(R_SS);
            }
        } else {
            gen_op_andl_A0_ffff();
            tcg_gen_mov_tl(cpu_T[1], cpu_A0);
            gen_op_addl_A0_seg(R_SS);
        }
        gen_op_st_T0_A0(s->dflag + 1 + s->base.mem_idx);
        if(s->ss32 && !s->addseg) {
            gen_op_mov_reg_A0(1, R_ESP);
        } else {
            gen_op_mov_reg_T1(s->ss32 + 1, R_ESP);
        }
    }
}

/* generate a push. It depends on ss32, addseg and dflag */
/* slower version for T1, only used for call Ev */
static void gen_push_T1(DisasContext *s)
{
#ifdef TARGET_X86_64
    if(CODE64(s)) {
        gen_op_movq_A0_reg(R_ESP);
        if(s->dflag) {
            gen_op_addq_A0_im(-8);
            gen_op_st_T1_A0(OT_QUAD + s->base.mem_idx);
        } else {
            gen_op_addq_A0_im(-2);
            gen_op_st_T0_A0(OT_WORD + s->base.mem_idx);
        }
        gen_op_mov_reg_A0(2, R_ESP);
    } else
#endif
    {
        gen_op_movl_A0_reg(R_ESP);
        if(!s->dflag) {
            gen_op_addl_A0_im(-2);
        } else {
            gen_op_addl_A0_im(-4);
        }
        if(s->ss32) {
            if(s->addseg) {
                gen_op_addl_A0_seg(R_SS);
            }
        } else {
            gen_op_andl_A0_ffff();
            gen_op_addl_A0_seg(R_SS);
        }
        gen_op_st_T1_A0(s->dflag + 1 + s->base.mem_idx);

        if(s->ss32 && !s->addseg) {
            gen_op_mov_reg_A0(1, R_ESP);
        } else {
            //  since s->dflag may equal 0, 1 or 2, this seems to be ok
            gen_stack_update(s, (2 << s->dflag) * -1);
        }
    }
}

/* two step pop is necessary for precise exceptions */
static void gen_pop_T0(DisasContext *s)
{
#ifdef TARGET_X86_64
    if(CODE64(s)) {
        gen_op_movq_A0_reg(R_ESP);
        gen_op_ld_T0_A0((s->dflag ? OT_QUAD : OT_WORD) + s->base.mem_idx);
    } else
#endif
    {
        gen_op_movl_A0_reg(R_ESP);
        if(s->ss32) {
            if(s->addseg) {
                gen_op_addl_A0_seg(R_SS);
            }
        } else {
            gen_op_andl_A0_ffff();
            gen_op_addl_A0_seg(R_SS);
        }
        gen_op_ld_T0_A0(s->dflag + 1 + s->base.mem_idx);
    }
}

static void gen_pop_update(DisasContext *s)
{
#ifdef TARGET_X86_64
    if(CODE64(s) && s->dflag) {
        gen_stack_update(s, 8);
    } else
#endif
    {
        gen_stack_update(s, 2 << s->dflag);
    }
}

static void gen_stack_A0(DisasContext *s)
{
    gen_op_movl_A0_reg(R_ESP);
    if(!s->ss32) {
        gen_op_andl_A0_ffff();
    }
    tcg_gen_mov_tl(cpu_T[1], cpu_A0);
    if(s->addseg) {
        gen_op_addl_A0_seg(R_SS);
    }
}

/* NOTE: wrap around in 16 bit not fully handled */
static void gen_pusha(DisasContext *s)
{
    int i;
    gen_op_movl_A0_reg(R_ESP);
    //  since s->dflag may equal 0, 1 or 2, this seems to be ok
    gen_op_addl_A0_im((16 << s->dflag) * -1);
    if(!s->ss32) {
        gen_op_andl_A0_ffff();
    }
    tcg_gen_mov_tl(cpu_T[1], cpu_A0);
    if(s->addseg) {
        gen_op_addl_A0_seg(R_SS);
    }
    for(i = 0; i < 8; i++) {
        gen_op_mov_TN_reg(OT_LONG, 0, 7 - i);
        gen_op_st_T0_A0(OT_WORD + s->dflag + s->base.mem_idx);
        gen_op_addl_A0_im(2 << s->dflag);
    }
    gen_op_mov_reg_T1(OT_WORD + s->ss32, R_ESP);
}

/* NOTE: wrap around in 16 bit not fully handled */
static void gen_popa(DisasContext *s)
{
    int i;
    gen_op_movl_A0_reg(R_ESP);
    if(!s->ss32) {
        gen_op_andl_A0_ffff();
    }
    tcg_gen_mov_tl(cpu_T[1], cpu_A0);
    tcg_gen_addi_tl(cpu_T[1], cpu_T[1], 16 << s->dflag);
    if(s->addseg) {
        gen_op_addl_A0_seg(R_SS);
    }
    for(i = 0; i < 8; i++) {
        /* ESP is not reloaded */
        if(i != 3) {
            gen_op_ld_T0_A0(OT_WORD + s->dflag + s->base.mem_idx);
            gen_op_mov_reg_T0(OT_WORD + s->dflag, 7 - i);
        }
        gen_op_addl_A0_im(2 << s->dflag);
    }
    gen_op_mov_reg_T1(OT_WORD + s->ss32, R_ESP);
}

static void gen_enter(DisasContext *s, int esp_addend, int level)
{
    int ot, opsize;

    level &= 0x1f;
#ifdef TARGET_X86_64
    if(CODE64(s)) {
        ot = s->dflag ? OT_QUAD : OT_WORD;
        opsize = 1 << ot;

        gen_op_movl_A0_reg(R_ESP);
        gen_op_addq_A0_im(-opsize);
        tcg_gen_mov_tl(cpu_T[1], cpu_A0);

        /* push bp */
        gen_op_mov_TN_reg(OT_LONG, 0, R_EBP);
        gen_op_st_T0_A0(ot + s->base.mem_idx);
        if(level) {
            /* XXX: must save state */
            gen_helper_enter64_level(tcg_const_i32(level), tcg_const_i32((ot == OT_QUAD)), cpu_T[1]);
        }
        gen_op_mov_reg_T1(ot, R_EBP);
        tcg_gen_addi_tl(cpu_T[1], cpu_T[1], -esp_addend + (-opsize * level));
        gen_op_mov_reg_T1(OT_QUAD, R_ESP);
    } else
#endif
    {
        ot = s->dflag + OT_WORD;
        opsize = 2 << s->dflag;

        gen_op_movl_A0_reg(R_ESP);
        gen_op_addl_A0_im(-opsize);
        if(!s->ss32) {
            gen_op_andl_A0_ffff();
        }
        tcg_gen_mov_tl(cpu_T[1], cpu_A0);
        if(s->addseg) {
            gen_op_addl_A0_seg(R_SS);
        }
        /* push bp */
        gen_op_mov_TN_reg(OT_LONG, 0, R_EBP);
        gen_op_st_T0_A0(ot + s->base.mem_idx);
        if(level) {
            /* XXX: must save state */
            gen_helper_enter_level(tcg_const_i32(level), tcg_const_i32(s->dflag), cpu_T[1]);
        }
        gen_op_mov_reg_T1(ot, R_EBP);
        tcg_gen_addi_tl(cpu_T[1], cpu_T[1], -esp_addend + (-opsize * level));
        gen_op_mov_reg_T1(OT_WORD + s->ss32, R_ESP);
    }
}

static void gen_exception(DisasContext *s, int trapno, target_ulong cur_eip)
{
    if(s->cc_op != CC_OP_DYNAMIC) {
        gen_op_set_cc_op(s->cc_op);
    }
    gen_jmp_im(cur_eip);
    gen_helper_raise_exception(tcg_const_i32(trapno));
    s->base.is_jmp = DISAS_TB_JUMP;
}

/* an interrupt is different from an exception because of the
   privilege checks */
static void gen_interrupt(DisasContext *s, int intno, target_ulong cur_eip, target_ulong next_eip)
{
    if(s->cc_op != CC_OP_DYNAMIC) {
        gen_op_set_cc_op(s->cc_op);
    }
    gen_jmp_im(cur_eip);
    gen_helper_raise_interrupt(tcg_const_i32(intno), tcg_const_i32(next_eip - cur_eip));
    s->base.is_jmp = DISAS_TB_JUMP;
}

static void gen_debug(DisasContext *s, target_ulong cur_eip)
{
    if(s->cc_op != CC_OP_DYNAMIC) {
        gen_op_set_cc_op(s->cc_op);
    }
    gen_jmp_im(cur_eip);
    gen_helper_debug();
    s->base.is_jmp = DISAS_TB_JUMP;
}

/* generate a generic end of block. Trace exception is also generated
   if needed */
static void gen_eob(DisasContext *s)
{
    if(s->cc_op != CC_OP_DYNAMIC) {
        gen_op_set_cc_op(s->cc_op);
    }
    if(s->base.tb->flags & HF_INHIBIT_IRQ_MASK) {
        gen_helper_reset_inhibit_irq();
    }
    if(s->base.tb->flags & HF_RF_MASK) {
        gen_helper_reset_rf();
    }
    if(s->tf) {
        gen_helper_single_step();
    } else {
        gen_exit_tb_no_chaining(s->base.tb);
    }
    s->base.is_jmp = DISAS_TB_JUMP;
}

/* generate a jump to eip. No segment change must happen before as a
   direct call to the next block may occur */
static void gen_jmp_tb(DisasContext *s, target_ulong eip, int tb_num)
{
    if(s->jmp_opt) {
        gen_update_cc_op(s);
        gen_goto_tb(s, tb_num, eip);
        s->base.is_jmp = DISAS_TB_JUMP;
    } else {
        gen_jmp_im(eip);
        gen_eob(s);
    }
}

static void gen_jmp(DisasContext *s, target_ulong eip)
{
    gen_jmp_tb(s, eip, 0);
}

static inline void gen_ldq_env_A0(int idx, int offset)
{
    int mem_idx = (idx >> 2) - 1;
    tcg_gen_qemu_ld64(cpu_tmp1_i64, cpu_A0, mem_idx);
    tcg_gen_st_i64(cpu_tmp1_i64, cpu_env, offset);
}

static inline void gen_stq_env_A0(int idx, int offset)
{
    int mem_idx = (idx >> 2) - 1;
    tcg_gen_ld_i64(cpu_tmp1_i64, cpu_env, offset);
    tcg_gen_qemu_st64(cpu_tmp1_i64, cpu_A0, mem_idx);
}

static inline void gen_ldo_env_A0(int idx, int offset)
{
    int mem_idx = (idx >> 2) - 1;
    tcg_gen_qemu_ld64(cpu_tmp1_i64, cpu_A0, mem_idx);
    tcg_gen_st_i64(cpu_tmp1_i64, cpu_env, offset + offsetof(XMMReg, XMM_Q(0)));
    tcg_gen_addi_tl(cpu_tmp0, cpu_A0, 8);
    tcg_gen_qemu_ld64(cpu_tmp1_i64, cpu_tmp0, mem_idx);
    tcg_gen_st_i64(cpu_tmp1_i64, cpu_env, offset + offsetof(XMMReg, XMM_Q(1)));
}

static inline void gen_sto_env_A0(int idx, int offset)
{
    int mem_idx = (idx >> 2) - 1;
    tcg_gen_ld_i64(cpu_tmp1_i64, cpu_env, offset + offsetof(XMMReg, XMM_Q(0)));
    tcg_gen_qemu_st64(cpu_tmp1_i64, cpu_A0, mem_idx);
    tcg_gen_addi_tl(cpu_tmp0, cpu_A0, 8);
    tcg_gen_ld_i64(cpu_tmp1_i64, cpu_env, offset + offsetof(XMMReg, XMM_Q(1)));
    tcg_gen_qemu_st64(cpu_tmp1_i64, cpu_tmp0, mem_idx);
}

static inline void gen_op_movo(int d_offset, int s_offset)
{
    tcg_gen_ld_i64(cpu_tmp1_i64, cpu_env, s_offset);
    tcg_gen_st_i64(cpu_tmp1_i64, cpu_env, d_offset);
    tcg_gen_ld_i64(cpu_tmp1_i64, cpu_env, s_offset + 8);
    tcg_gen_st_i64(cpu_tmp1_i64, cpu_env, d_offset + 8);
}

static inline void gen_op_movq(int d_offset, int s_offset)
{
    tcg_gen_ld_i64(cpu_tmp1_i64, cpu_env, s_offset);
    tcg_gen_st_i64(cpu_tmp1_i64, cpu_env, d_offset);
}

static inline void gen_op_movl(int d_offset, int s_offset)
{
    tcg_gen_ld_i32(cpu_tmp2_i32, cpu_env, s_offset);
    tcg_gen_st_i32(cpu_tmp2_i32, cpu_env, d_offset);
}

static inline void gen_op_movq_env_0(int d_offset)
{
    tcg_gen_movi_i64(cpu_tmp1_i64, 0);
    tcg_gen_st_i64(cpu_tmp1_i64, cpu_env, d_offset);
}

#define SSE_SPECIAL ((void *)1)
#define SSE_DUMMY   ((void *)2)

#define MMX_OP2(x) { gen_helper_##x##_mmx, gen_helper_##x##_xmm }
#define SSE_FOP(x)          \
    {                       \
        gen_helper_##x##ps, \
        gen_helper_##x##pd, \
        gen_helper_##x##ss, \
        gen_helper_##x##sd, \
    }

static void *sse_op_table1[256][4] = {
    /* 3DNow! extensions */
    [0x0e] = { SSE_DUMMY }, /* femms */
    [0x0f] = { SSE_DUMMY }, /* pf... */
    /* pure SSE operations */
    [0x10] = { SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL }, /* movups, movupd, movss, movsd */
    [0x11] = { SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL }, /* movups, movupd, movss, movsd */
    [0x12] = { SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL }, /* movlps, movlpd, movsldup, movddup */
    [0x13] = { SSE_SPECIAL, SSE_SPECIAL }, /* movlps, movlpd */
    [0x14] = { gen_helper_punpckldq_xmm, gen_helper_punpcklqdq_xmm },
    [0x15] = { gen_helper_punpckhdq_xmm, gen_helper_punpckhqdq_xmm },
    [0x16] = { SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL }, /* movhps, movhpd, movshdup */
    [0x17] = { SSE_SPECIAL, SSE_SPECIAL }, /* movhps, movhpd */

    [0x28] = { SSE_SPECIAL, SSE_SPECIAL }, /* movaps, movapd */
    [0x29] = { SSE_SPECIAL, SSE_SPECIAL }, /* movaps, movapd */
    [0x2a] = { SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL }, /* cvtpi2ps, cvtpi2pd, cvtsi2ss, cvtsi2sd */
    [0x2b] = { SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL }, /* movntps, movntpd, movntss, movntsd */
    [0x2c] = { SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL }, /* cvttps2pi, cvttpd2pi, cvttsd2si, cvttss2si */
    [0x2d] = { SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL }, /* cvtps2pi, cvtpd2pi, cvtsd2si, cvtss2si */
    [0x2e] = { gen_helper_ucomiss, gen_helper_ucomisd },
    [0x2f] = { gen_helper_comiss, gen_helper_comisd },
    [0x50] = { SSE_SPECIAL, SSE_SPECIAL }, /* movmskps, movmskpd */
    [0x51] = SSE_FOP(sqrt),
    [0x52] = { gen_helper_rsqrtps, NULL, gen_helper_rsqrtss, NULL },
    [0x53] = { gen_helper_rcpps, NULL, gen_helper_rcpss, NULL },
    [0x54] = { gen_helper_pand_xmm, gen_helper_pand_xmm }, /* andps, andpd */
    [0x55] = { gen_helper_pandn_xmm, gen_helper_pandn_xmm }, /* andnps, andnpd */
    [0x56] = { gen_helper_por_xmm, gen_helper_por_xmm }, /* orps, orpd */
    [0x57] = { gen_helper_pxor_xmm, gen_helper_pxor_xmm }, /* xorps, xorpd */
    [0x58] = SSE_FOP(add),
    [0x59] = SSE_FOP(mul),
    [0x5a] = { gen_helper_cvtps2pd, gen_helper_cvtpd2ps, gen_helper_cvtss2sd, gen_helper_cvtsd2ss },
    [0x5b] = { gen_helper_cvtdq2ps, gen_helper_cvtps2dq, gen_helper_cvttps2dq },
    [0x5c] = SSE_FOP(sub),
    [0x5d] = SSE_FOP(min),
    [0x5e] = SSE_FOP(div),
    [0x5f] = SSE_FOP(max),

    [0xc2] = SSE_FOP(cmpeq),
    [0xc6] = { gen_helper_shufps, gen_helper_shufpd },

    [0x38] = { SSE_SPECIAL, SSE_SPECIAL, NULL, SSE_SPECIAL }, /* SSSE3/SSE4 */
    [0x3a] = { SSE_SPECIAL, SSE_SPECIAL }, /* SSSE3/SSE4 */

    /* MMX ops and their SSE extensions */
    [0x60] = MMX_OP2(punpcklbw),
    [0x61] = MMX_OP2(punpcklwd),
    [0x62] = MMX_OP2(punpckldq),
    [0x63] = MMX_OP2(packsswb),
    [0x64] = MMX_OP2(pcmpgtb),
    [0x65] = MMX_OP2(pcmpgtw),
    [0x66] = MMX_OP2(pcmpgtl),
    [0x67] = MMX_OP2(packuswb),
    [0x68] = MMX_OP2(punpckhbw),
    [0x69] = MMX_OP2(punpckhwd),
    [0x6a] = MMX_OP2(punpckhdq),
    [0x6b] = MMX_OP2(packssdw),
    [0x6c] = { NULL, gen_helper_punpcklqdq_xmm },
    [0x6d] = { NULL, gen_helper_punpckhqdq_xmm },
    [0x6e] = { SSE_SPECIAL, SSE_SPECIAL }, /* movd mm, ea */
    [0x6f] = { SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL }, /* movq, movdqa, , movqdu */
    [0x70] = { gen_helper_pshufw_mmx, gen_helper_pshufd_xmm, gen_helper_pshufhw_xmm, gen_helper_pshuflw_xmm },
    [0x71] = { SSE_SPECIAL, SSE_SPECIAL }, /* shiftw */
    [0x72] = { SSE_SPECIAL, SSE_SPECIAL }, /* shiftd */
    [0x73] = { SSE_SPECIAL, SSE_SPECIAL }, /* shiftq */
    [0x74] = MMX_OP2(pcmpeqb),
    [0x75] = MMX_OP2(pcmpeqw),
    [0x76] = MMX_OP2(pcmpeql),
    [0x77] = { SSE_DUMMY }, /* emms */
    [0x78] = { NULL, SSE_SPECIAL, NULL, SSE_SPECIAL }, /* extrq_i, insertq_i */
    [0x79] = { NULL, gen_helper_extrq_r, NULL, gen_helper_insertq_r },
    [0x7c] = { NULL, gen_helper_haddpd, NULL, gen_helper_haddps },
    [0x7d] = { NULL, gen_helper_hsubpd, NULL, gen_helper_hsubps },
    [0x7e] = { SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL }, /* movd, movd, , movq */
    [0x7f] = { SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL }, /* movq, movdqa, movdqu */
    [0xc4] = { SSE_SPECIAL, SSE_SPECIAL }, /* pinsrw */
    [0xc5] = { SSE_SPECIAL, SSE_SPECIAL }, /* pextrw */
    [0xd0] = { NULL, gen_helper_addsubpd, NULL, gen_helper_addsubps },
    [0xd1] = MMX_OP2(psrlw),
    [0xd2] = MMX_OP2(psrld),
    [0xd3] = MMX_OP2(psrlq),
    [0xd4] = MMX_OP2(paddq),
    [0xd5] = MMX_OP2(pmullw),
    [0xd6] = { NULL, SSE_SPECIAL, SSE_SPECIAL, SSE_SPECIAL },
    [0xd7] = { SSE_SPECIAL, SSE_SPECIAL }, /* pmovmskb */
    [0xd8] = MMX_OP2(psubusb),
    [0xd9] = MMX_OP2(psubusw),
    [0xda] = MMX_OP2(pminub),
    [0xdb] = MMX_OP2(pand),
    [0xdc] = MMX_OP2(paddusb),
    [0xdd] = MMX_OP2(paddusw),
    [0xde] = MMX_OP2(pmaxub),
    [0xdf] = MMX_OP2(pandn),
    [0xe0] = MMX_OP2(pavgb),
    [0xe1] = MMX_OP2(psraw),
    [0xe2] = MMX_OP2(psrad),
    [0xe3] = MMX_OP2(pavgw),
    [0xe4] = MMX_OP2(pmulhuw),
    [0xe5] = MMX_OP2(pmulhw),
    [0xe6] = { NULL, gen_helper_cvttpd2dq, gen_helper_cvtdq2pd, gen_helper_cvtpd2dq },
    [0xe7] = { SSE_SPECIAL, SSE_SPECIAL }, /* movntq, movntq */
    [0xe8] = MMX_OP2(psubsb),
    [0xe9] = MMX_OP2(psubsw),
    [0xea] = MMX_OP2(pminsw),
    [0xeb] = MMX_OP2(por),
    [0xec] = MMX_OP2(paddsb),
    [0xed] = MMX_OP2(paddsw),
    [0xee] = MMX_OP2(pmaxsw),
    [0xef] = MMX_OP2(pxor),
    [0xf0] = { NULL, NULL, NULL, SSE_SPECIAL }, /* lddqu */
    [0xf1] = MMX_OP2(psllw),
    [0xf2] = MMX_OP2(pslld),
    [0xf3] = MMX_OP2(psllq),
    [0xf4] = MMX_OP2(pmuludq),
    [0xf5] = MMX_OP2(pmaddwd),
    [0xf6] = MMX_OP2(psadbw),
    [0xf7] = MMX_OP2(maskmov),
    [0xf8] = MMX_OP2(psubb),
    [0xf9] = MMX_OP2(psubw),
    [0xfa] = MMX_OP2(psubl),
    [0xfb] = MMX_OP2(psubq),
    [0xfc] = MMX_OP2(paddb),
    [0xfd] = MMX_OP2(paddw),
    [0xfe] = MMX_OP2(paddl),
};

static void *sse_op_table2[3 * 8][2] = {
    [0 + 2] = MMX_OP2(psrlw),  [0 + 4] = MMX_OP2(psraw),
    [0 + 6] = MMX_OP2(psllw),  [8 + 2] = MMX_OP2(psrld),
    [8 + 4] = MMX_OP2(psrad),  [8 + 6] = MMX_OP2(pslld),
    [16 + 2] = MMX_OP2(psrlq), [16 + 3] = { NULL, gen_helper_psrldq_xmm },
    [16 + 6] = MMX_OP2(psllq), [16 + 7] = { NULL, gen_helper_pslldq_xmm },
};

static void *sse_op_table3[4 * 3] = {
    gen_helper_cvtsi2ss,  gen_helper_cvtsi2sd,  X86_64_ONLY(gen_helper_cvtsq2ss),  X86_64_ONLY(gen_helper_cvtsq2sd),

    gen_helper_cvttss2si, gen_helper_cvttsd2si, X86_64_ONLY(gen_helper_cvttss2sq), X86_64_ONLY(gen_helper_cvttsd2sq),

    gen_helper_cvtss2si,  gen_helper_cvtsd2si,  X86_64_ONLY(gen_helper_cvtss2sq),  X86_64_ONLY(gen_helper_cvtsd2sq),
};

static void *sse_op_table4[8][4] = {
    SSE_FOP(cmpeq),  SSE_FOP(cmplt),  SSE_FOP(cmple),  SSE_FOP(cmpunord),
    SSE_FOP(cmpneq), SSE_FOP(cmpnlt), SSE_FOP(cmpnle), SSE_FOP(cmpord),
};

static void *sse_op_table5[256] = {
    [0x0c] = gen_helper_pi2fw,       [0x0d] = gen_helper_pi2fd,   [0x1c] = gen_helper_pf2iw,    [0x1d] = gen_helper_pf2id,
    [0x8a] = gen_helper_pfnacc,      [0x8e] = gen_helper_pfpnacc, [0x90] = gen_helper_pfcmpge,  [0x94] = gen_helper_pfmin,
    [0x96] = gen_helper_pfrcp,       [0x97] = gen_helper_pfrsqrt, [0x9a] = gen_helper_pfsub,    [0x9e] = gen_helper_pfadd,
    [0xa0] = gen_helper_pfcmpgt,     [0xa4] = gen_helper_pfmax,   [0xa6] = gen_helper_movq, /* pfrcpit1; no need to actually
                                                                                               increase precision */
    [0xa7] = gen_helper_movq,                                                               /* pfrsqit1 */
    [0xaa] = gen_helper_pfsubr,      [0xae] = gen_helper_pfacc,   [0xb0] = gen_helper_pfcmpeq,  [0xb4] = gen_helper_pfmul,
    [0xb6] = gen_helper_movq,                                                                   /* pfrcpit2 */
    [0xb7] = gen_helper_pmulhrw_mmx, [0xbb] = gen_helper_pswapd,  [0xbf] = gen_helper_pavgb_mmx /* pavgusb */
};

struct sse_op_helper_s {
    void *op[2];
    uint32_t ext_mask;
};
#define SSSE3_OP(x) { MMX_OP2(x), CPUID_EXT_SSSE3 }
#define SSE41_OP(x)                     \
    {                                   \
        { NULL, gen_helper_##x##_xmm }, \
        CPUID_EXT_SSE41                 \
}
#define SSE42_OP(x)                     \
    {                                   \
        { NULL, gen_helper_##x##_xmm }, \
        CPUID_EXT_SSE42                 \
    }
#define SSE41_SPECIAL          \
    {                          \
        { NULL, SSE_SPECIAL }, \
        CPUID_EXT_SSE41        \
    }
static struct sse_op_helper_s sse_op_table6[256] = {
    [0x00] = SSSE3_OP(pshufb),   [0x01] = SSSE3_OP(phaddw),    [0x02] = SSSE3_OP(phaddd),
    [0x03] = SSSE3_OP(phaddsw),  [0x04] = SSSE3_OP(pmaddubsw), [0x05] = SSSE3_OP(phsubw),
    [0x06] = SSSE3_OP(phsubd),   [0x07] = SSSE3_OP(phsubsw),   [0x08] = SSSE3_OP(psignb),
    [0x09] = SSSE3_OP(psignw),   [0x0a] = SSSE3_OP(psignd),    [0x0b] = SSSE3_OP(pmulhrsw),
    [0x10] = SSE41_OP(pblendvb), [0x14] = SSE41_OP(blendvps),  [0x15] = SSE41_OP(blendvpd),
    [0x17] = SSE41_OP(ptest),    [0x1c] = SSSE3_OP(pabsb),     [0x1d] = SSSE3_OP(pabsw),
    [0x1e] = SSSE3_OP(pabsd),    [0x20] = SSE41_OP(pmovsxbw),  [0x21] = SSE41_OP(pmovsxbd),
    [0x22] = SSE41_OP(pmovsxbq), [0x23] = SSE41_OP(pmovsxwd),  [0x24] = SSE41_OP(pmovsxwq),
    [0x25] = SSE41_OP(pmovsxdq), [0x28] = SSE41_OP(pmuldq),    [0x29] = SSE41_OP(pcmpeqq),
    [0x2a] = SSE41_SPECIAL, /* movntqda */
    [0x2b] = SSE41_OP(packusdw), [0x30] = SSE41_OP(pmovzxbw),  [0x31] = SSE41_OP(pmovzxbd),
    [0x32] = SSE41_OP(pmovzxbq), [0x33] = SSE41_OP(pmovzxwd),  [0x34] = SSE41_OP(pmovzxwq),
    [0x35] = SSE41_OP(pmovzxdq), [0x37] = SSE42_OP(pcmpgtq),   [0x38] = SSE41_OP(pminsb),
    [0x39] = SSE41_OP(pminsd),   [0x3a] = SSE41_OP(pminuw),    [0x3b] = SSE41_OP(pminud),
    [0x3c] = SSE41_OP(pmaxsb),   [0x3d] = SSE41_OP(pmaxsd),    [0x3e] = SSE41_OP(pmaxuw),
    [0x3f] = SSE41_OP(pmaxud),   [0x40] = SSE41_OP(pmulld),    [0x41] = SSE41_OP(phminposuw),
};

static struct sse_op_helper_s sse_op_table7[256] = {
    [0x08] = SSE41_OP(roundps),   [0x09] = SSE41_OP(roundpd),   [0x0a] = SSE41_OP(roundss),   [0x0b] = SSE41_OP(roundsd),
    [0x0c] = SSE41_OP(blendps),   [0x0d] = SSE41_OP(blendpd),   [0x0e] = SSE41_OP(pblendw),   [0x0f] = SSSE3_OP(palignr),
    [0x14] = SSE41_SPECIAL, /* pextrb */
    [0x15] = SSE41_SPECIAL, /* pextrw */
    [0x16] = SSE41_SPECIAL, /* pextrd/pextrq */
    [0x17] = SSE41_SPECIAL, /* extractps */
    [0x20] = SSE41_SPECIAL, /* pinsrb */
    [0x21] = SSE41_SPECIAL, /* insertps */
    [0x22] = SSE41_SPECIAL, /* pinsrd/pinsrq */
    [0x40] = SSE41_OP(dpps),      [0x41] = SSE41_OP(dppd),      [0x42] = SSE41_OP(mpsadbw),   [0x60] = SSE42_OP(pcmpestrm),
    [0x61] = SSE42_OP(pcmpestri), [0x62] = SSE42_OP(pcmpistrm), [0x63] = SSE42_OP(pcmpistri),
};

static void gen_sse(DisasContext *s, int b, target_ulong pc_start, int rex_r)
{
    int b1, op1_offset, op2_offset, is_xmm, val, ot;
    int modrm, mod, rm, reg, reg_addr, offset_addr;
    void *sse_op2;

    b &= 0xff;
    if(s->prefix & PREFIX_DATA) {
        b1 = 1;
    } else if(s->prefix & PREFIX_REPZ) {
        b1 = 2;
    } else if(s->prefix & PREFIX_REPNZ) {
        b1 = 3;
    } else {
        b1 = 0;
    }
    sse_op2 = sse_op_table1[b][b1];
    if(!sse_op2) {
        goto illegal_op;
    }
    if((b <= 0x5f && b >= 0x10) || b == 0xc6 || b == 0xc2) {
        is_xmm = 1;
    } else {
        if(b1 == 0) {
            /* MMX case */
            is_xmm = 0;
        } else {
            is_xmm = 1;
        }
    }
    /* simple MMX/SSE operation */
    if(s->flags & HF_TS_MASK) {
        gen_exception(s, EXCP07_PREX, pc_start - s->cs_base);
        return;
    }
    if(s->flags & HF_EM_MASK) {
    illegal_op:
        gen_exception(s, EXCP06_ILLOP, pc_start - s->cs_base);
        return;
    }
    if(is_xmm && !(s->flags & HF_OSFXSR_MASK)) {
        if((b != 0x38 && b != 0x3a) || (s->prefix & PREFIX_DATA)) {
            goto illegal_op;
        }
    }
    if(b == 0x0e) {
        if(!(s->cpuid_ext2_features & CPUID_EXT2_3DNOW)) {
            goto illegal_op;
        }
        /* femms */
        gen_helper_emms();
        return;
    }
    if(b == 0x77) {
        /* emms */
        gen_helper_emms();
        return;
    }
    /* prepare MMX state (XXX: optimize by storing fptt and fptags in
       the static cpu state) */
    if(!is_xmm) {
        gen_helper_enter_mmx();
    }

    modrm = ldub_code(s->base.pc++);
    reg = ((modrm >> 3) & 7);
    if(is_xmm) {
        reg |= rex_r;
    }
    mod = (modrm >> 6) & 3;
    if(sse_op2 == SSE_SPECIAL) {
        b |= (b1 << 8);
        switch(b) {
            case 0x0e7: /* movntq */
                if(mod == 3) {
                    goto illegal_op;
                }
                gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                gen_stq_env_A0(s->base.mem_idx, offsetof(CPUState, fpregs[reg].mmx));
                break;
            case 0x1e7: /* movntdq */
            case 0x02b: /* movntps */
            case 0x12b: /* movntps */
                if(mod == 3) {
                    goto illegal_op;
                }
                gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                gen_sto_env_A0(s->base.mem_idx, offsetof(CPUState, xmm_regs[reg]));
                break;
            case 0x3f0: /* lddqu */
                if(mod == 3) {
                    goto illegal_op;
                }
                gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                gen_ldo_env_A0(s->base.mem_idx, offsetof(CPUState, xmm_regs[reg]));
                break;
            case 0x22b: /* movntss */
            case 0x32b: /* movntsd */
                if(mod == 3) {
                    goto illegal_op;
                }
                gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                if(b1 & 1) {
                    gen_stq_env_A0(s->base.mem_idx, offsetof(CPUState, xmm_regs[reg]));
                } else {
                    tcg_gen_ld32u_tl(cpu_T[0], cpu_env, offsetof(CPUState, xmm_regs[reg].XMM_L(0)));
                    gen_op_st_T0_A0(OT_LONG + s->base.mem_idx);
                }
                break;
            case 0x6e: /* movd mm, ea */
#ifdef TARGET_X86_64
                if(s->dflag == 2) {
                    gen_ldst_modrm(s, modrm, OT_QUAD, OR_TMP0, 0);
                    tcg_gen_st_tl(cpu_T[0], cpu_env, offsetof(CPUState, fpregs[reg].mmx));
                } else
#endif
                {
                    gen_ldst_modrm(s, modrm, OT_LONG, OR_TMP0, 0);
                    tcg_gen_addi_ptr(cpu_ptr0, cpu_env, offsetof(CPUState, fpregs[reg].mmx));
                    tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_T[0]);
                    gen_helper_movl_mm_T0_mmx(cpu_ptr0, cpu_tmp2_i32);
                }
                break;
            case 0x16e: /* movd xmm, ea */
#ifdef TARGET_X86_64
                if(s->dflag == 2) {
                    gen_ldst_modrm(s, modrm, OT_QUAD, OR_TMP0, 0);
                    tcg_gen_addi_ptr(cpu_ptr0, cpu_env, offsetof(CPUState, xmm_regs[reg]));
                    gen_helper_movq_mm_T0_xmm(cpu_ptr0, cpu_T[0]);
                } else
#endif
                {
                    gen_ldst_modrm(s, modrm, OT_LONG, OR_TMP0, 0);
                    tcg_gen_addi_ptr(cpu_ptr0, cpu_env, offsetof(CPUState, xmm_regs[reg]));
                    tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_T[0]);
                    gen_helper_movl_mm_T0_xmm(cpu_ptr0, cpu_tmp2_i32);
                }
                break;
            case 0x6f: /* movq mm, ea */
                if(mod != 3) {
                    gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    gen_ldq_env_A0(s->base.mem_idx, offsetof(CPUState, fpregs[reg].mmx));
                } else {
                    rm = (modrm & 7);
                    tcg_gen_ld_i64(cpu_tmp1_i64, cpu_env, offsetof(CPUState, fpregs[rm].mmx));
                    tcg_gen_st_i64(cpu_tmp1_i64, cpu_env, offsetof(CPUState, fpregs[reg].mmx));
                }
                break;
            case 0x010: /* movups */
            case 0x110: /* movupd */
            case 0x028: /* movaps */
            case 0x128: /* movapd */
            case 0x16f: /* movdqa xmm, ea */
            case 0x26f: /* movdqu xmm, ea */
                if(mod != 3) {
                    gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    gen_ldo_env_A0(s->base.mem_idx, offsetof(CPUState, xmm_regs[reg]));
                } else {
                    rm = (modrm & 7) | REX_B(s);
                    gen_op_movo(offsetof(CPUState, xmm_regs[reg]), offsetof(CPUState, xmm_regs[rm]));
                }
                break;
            case 0x210: /* movss xmm, ea */
                if(mod != 3) {
                    gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    gen_op_ld_T0_A0(OT_LONG + s->base.mem_idx);
                    tcg_gen_st32_tl(cpu_T[0], cpu_env, offsetof(CPUState, xmm_regs[reg].XMM_L(0)));
                    gen_op_movl_T0_0();
                    tcg_gen_st32_tl(cpu_T[0], cpu_env, offsetof(CPUState, xmm_regs[reg].XMM_L(1)));
                    tcg_gen_st32_tl(cpu_T[0], cpu_env, offsetof(CPUState, xmm_regs[reg].XMM_L(2)));
                    tcg_gen_st32_tl(cpu_T[0], cpu_env, offsetof(CPUState, xmm_regs[reg].XMM_L(3)));
                } else {
                    rm = (modrm & 7) | REX_B(s);
                    gen_op_movl(offsetof(CPUState, xmm_regs[reg].XMM_L(0)), offsetof(CPUState, xmm_regs[rm].XMM_L(0)));
                }
                break;
            case 0x310: /* movsd xmm, ea */
                if(mod != 3) {
                    gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    gen_ldq_env_A0(s->base.mem_idx, offsetof(CPUState, xmm_regs[reg].XMM_Q(0)));
                    gen_op_movl_T0_0();
                    tcg_gen_st32_tl(cpu_T[0], cpu_env, offsetof(CPUState, xmm_regs[reg].XMM_L(2)));
                    tcg_gen_st32_tl(cpu_T[0], cpu_env, offsetof(CPUState, xmm_regs[reg].XMM_L(3)));
                } else {
                    rm = (modrm & 7) | REX_B(s);
                    gen_op_movq(offsetof(CPUState, xmm_regs[reg].XMM_Q(0)), offsetof(CPUState, xmm_regs[rm].XMM_Q(0)));
                }
                break;
            case 0x012: /* movlps */
            case 0x112: /* movlpd */
                if(mod != 3) {
                    gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    gen_ldq_env_A0(s->base.mem_idx, offsetof(CPUState, xmm_regs[reg].XMM_Q(0)));
                } else {
                    /* movhlps */
                    rm = (modrm & 7) | REX_B(s);
                    gen_op_movq(offsetof(CPUState, xmm_regs[reg].XMM_Q(0)), offsetof(CPUState, xmm_regs[rm].XMM_Q(1)));
                }
                break;
            case 0x212: /* movsldup */
                if(mod != 3) {
                    gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    gen_ldo_env_A0(s->base.mem_idx, offsetof(CPUState, xmm_regs[reg]));
                } else {
                    rm = (modrm & 7) | REX_B(s);
                    gen_op_movl(offsetof(CPUState, xmm_regs[reg].XMM_L(0)), offsetof(CPUState, xmm_regs[rm].XMM_L(0)));
                    gen_op_movl(offsetof(CPUState, xmm_regs[reg].XMM_L(2)), offsetof(CPUState, xmm_regs[rm].XMM_L(2)));
                }
                gen_op_movl(offsetof(CPUState, xmm_regs[reg].XMM_L(1)), offsetof(CPUState, xmm_regs[reg].XMM_L(0)));
                gen_op_movl(offsetof(CPUState, xmm_regs[reg].XMM_L(3)), offsetof(CPUState, xmm_regs[reg].XMM_L(2)));
                break;
            case 0x312: /* movddup */
                if(mod != 3) {
                    gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    gen_ldq_env_A0(s->base.mem_idx, offsetof(CPUState, xmm_regs[reg].XMM_Q(0)));
                } else {
                    rm = (modrm & 7) | REX_B(s);
                    gen_op_movq(offsetof(CPUState, xmm_regs[reg].XMM_Q(0)), offsetof(CPUState, xmm_regs[rm].XMM_Q(0)));
                }
                gen_op_movq(offsetof(CPUState, xmm_regs[reg].XMM_Q(1)), offsetof(CPUState, xmm_regs[reg].XMM_Q(0)));
                break;
            case 0x016: /* movhps */
            case 0x116: /* movhpd */
                if(mod != 3) {
                    gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    gen_ldq_env_A0(s->base.mem_idx, offsetof(CPUState, xmm_regs[reg].XMM_Q(1)));
                } else {
                    /* movlhps */
                    rm = (modrm & 7) | REX_B(s);
                    gen_op_movq(offsetof(CPUState, xmm_regs[reg].XMM_Q(1)), offsetof(CPUState, xmm_regs[rm].XMM_Q(0)));
                }
                break;
            case 0x216: /* movshdup */
                if(mod != 3) {
                    gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    gen_ldo_env_A0(s->base.mem_idx, offsetof(CPUState, xmm_regs[reg]));
                } else {
                    rm = (modrm & 7) | REX_B(s);
                    gen_op_movl(offsetof(CPUState, xmm_regs[reg].XMM_L(1)), offsetof(CPUState, xmm_regs[rm].XMM_L(1)));
                    gen_op_movl(offsetof(CPUState, xmm_regs[reg].XMM_L(3)), offsetof(CPUState, xmm_regs[rm].XMM_L(3)));
                }
                gen_op_movl(offsetof(CPUState, xmm_regs[reg].XMM_L(0)), offsetof(CPUState, xmm_regs[reg].XMM_L(1)));
                gen_op_movl(offsetof(CPUState, xmm_regs[reg].XMM_L(2)), offsetof(CPUState, xmm_regs[reg].XMM_L(3)));
                break;
            case 0x178:
            case 0x378: {
                int bit_index, field_length;

                if(b1 == 1 && reg != 0) {
                    goto illegal_op;
                }
                field_length = ldub_code(s->base.pc++) & 0x3F;
                bit_index = ldub_code(s->base.pc++) & 0x3F;
                tcg_gen_addi_ptr(cpu_ptr0, cpu_env, offsetof(CPUState, xmm_regs[reg]));
                if(b1 == 1) {
                    gen_helper_extrq_i(cpu_ptr0, tcg_const_i32(bit_index), tcg_const_i32(field_length));
                } else {
                    gen_helper_insertq_i(cpu_ptr0, tcg_const_i32(bit_index), tcg_const_i32(field_length));
                }
            } break;
            case 0x7e: /* movd ea, mm */
#ifdef TARGET_X86_64
                if(s->dflag == 2) {
                    tcg_gen_ld_i64(cpu_T[0], cpu_env, offsetof(CPUState, fpregs[reg].mmx));
                    gen_ldst_modrm(s, modrm, OT_QUAD, OR_TMP0, 1);
                } else
#endif
                {
                    tcg_gen_ld32u_tl(cpu_T[0], cpu_env, offsetof(CPUState, fpregs[reg].mmx.MMX_L(0)));
                    gen_ldst_modrm(s, modrm, OT_LONG, OR_TMP0, 1);
                }
                break;
            case 0x17e: /* movd ea, xmm */
#ifdef TARGET_X86_64
                if(s->dflag == 2) {
                    tcg_gen_ld_i64(cpu_T[0], cpu_env, offsetof(CPUState, xmm_regs[reg].XMM_Q(0)));
                    gen_ldst_modrm(s, modrm, OT_QUAD, OR_TMP0, 1);
                } else
#endif
                {
                    tcg_gen_ld32u_tl(cpu_T[0], cpu_env, offsetof(CPUState, xmm_regs[reg].XMM_L(0)));
                    gen_ldst_modrm(s, modrm, OT_LONG, OR_TMP0, 1);
                }
                break;
            case 0x27e: /* movq xmm, ea */
                if(mod != 3) {
                    gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    gen_ldq_env_A0(s->base.mem_idx, offsetof(CPUState, xmm_regs[reg].XMM_Q(0)));
                } else {
                    rm = (modrm & 7) | REX_B(s);
                    gen_op_movq(offsetof(CPUState, xmm_regs[reg].XMM_Q(0)), offsetof(CPUState, xmm_regs[rm].XMM_Q(0)));
                }
                gen_op_movq_env_0(offsetof(CPUState, xmm_regs[reg].XMM_Q(1)));
                break;
            case 0x7f: /* movq ea, mm */
                if(mod != 3) {
                    gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    gen_stq_env_A0(s->base.mem_idx, offsetof(CPUState, fpregs[reg].mmx));
                } else {
                    rm = (modrm & 7);
                    gen_op_movq(offsetof(CPUState, fpregs[rm].mmx), offsetof(CPUState, fpregs[reg].mmx));
                }
                break;
            case 0x011: /* movups */
            case 0x111: /* movupd */
            case 0x029: /* movaps */
            case 0x129: /* movapd */
            case 0x17f: /* movdqa ea, xmm */
            case 0x27f: /* movdqu ea, xmm */
                if(mod != 3) {
                    gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    gen_sto_env_A0(s->base.mem_idx, offsetof(CPUState, xmm_regs[reg]));
                } else {
                    rm = (modrm & 7) | REX_B(s);
                    gen_op_movo(offsetof(CPUState, xmm_regs[rm]), offsetof(CPUState, xmm_regs[reg]));
                }
                break;
            case 0x211: /* movss ea, xmm */
                if(mod != 3) {
                    gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    tcg_gen_ld32u_tl(cpu_T[0], cpu_env, offsetof(CPUState, xmm_regs[reg].XMM_L(0)));
                    gen_op_st_T0_A0(OT_LONG + s->base.mem_idx);
                } else {
                    rm = (modrm & 7) | REX_B(s);
                    gen_op_movl(offsetof(CPUState, xmm_regs[rm].XMM_L(0)), offsetof(CPUState, xmm_regs[reg].XMM_L(0)));
                }
                break;
            case 0x311: /* movsd ea, xmm */
                if(mod != 3) {
                    gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    gen_stq_env_A0(s->base.mem_idx, offsetof(CPUState, xmm_regs[reg].XMM_Q(0)));
                } else {
                    rm = (modrm & 7) | REX_B(s);
                    gen_op_movq(offsetof(CPUState, xmm_regs[rm].XMM_Q(0)), offsetof(CPUState, xmm_regs[reg].XMM_Q(0)));
                }
                break;
            case 0x013: /* movlps */
            case 0x113: /* movlpd */
                if(mod != 3) {
                    gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    gen_stq_env_A0(s->base.mem_idx, offsetof(CPUState, xmm_regs[reg].XMM_Q(0)));
                } else {
                    goto illegal_op;
                }
                break;
            case 0x017: /* movhps */
            case 0x117: /* movhpd */
                if(mod != 3) {
                    gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    gen_stq_env_A0(s->base.mem_idx, offsetof(CPUState, xmm_regs[reg].XMM_Q(1)));
                } else {
                    goto illegal_op;
                }
                break;
            case 0x71:  /* shift mm, im */
            case 0x72:  //  shift xmm, im
            case 0x73:
            case 0x171:
            case 0x172:
            case 0x173:
                if(b1 >= 2) {
                    goto illegal_op;
                }
                val = ldub_code(s->base.pc++);
                if(is_xmm) {
                    gen_op_movl_T0_im(val);
                    tcg_gen_st32_tl(cpu_T[0], cpu_env, offsetof(CPUState, xmm_t0.XMM_L(0)));
                    gen_op_movl_T0_0();
                    tcg_gen_st32_tl(cpu_T[0], cpu_env, offsetof(CPUState, xmm_t0.XMM_L(1)));
                    op1_offset = offsetof(CPUState, xmm_t0);
                } else {
                    gen_op_movl_T0_im(val);
                    tcg_gen_st32_tl(cpu_T[0], cpu_env, offsetof(CPUState, mmx_t0.MMX_L(0)));
                    gen_op_movl_T0_0();
                    tcg_gen_st32_tl(cpu_T[0], cpu_env, offsetof(CPUState, mmx_t0.MMX_L(1)));
                    op1_offset = offsetof(CPUState, mmx_t0);
                }
                sse_op2 = sse_op_table2[((b - 1) & 3) * 8 + (((modrm >> 3)) & 7)][b1];
                if(!sse_op2) {
                    goto illegal_op;
                }
                if(is_xmm) {
                    rm = (modrm & 7) | REX_B(s);
                    op2_offset = offsetof(CPUState, xmm_regs[rm]);
                } else {
                    rm = (modrm & 7);
                    op2_offset = offsetof(CPUState, fpregs[rm].mmx);
                }
                tcg_gen_addi_ptr(cpu_ptr0, cpu_env, op2_offset);
                tcg_gen_addi_ptr(cpu_ptr1, cpu_env, op1_offset);
                ((void (*)(TCGv_ptr, TCGv_ptr))sse_op2)(cpu_ptr0, cpu_ptr1);
                break;
            case 0x050: /* movmskps */
                rm = (modrm & 7) | REX_B(s);
                tcg_gen_addi_ptr(cpu_ptr0, cpu_env, offsetof(CPUState, xmm_regs[rm]));
                gen_helper_movmskps(cpu_tmp2_i32, cpu_ptr0);
                tcg_gen_extu_i32_tl(cpu_T[0], cpu_tmp2_i32);
                gen_op_mov_reg_T0(OT_LONG, reg);
                break;
            case 0x150: /* movmskpd */
                rm = (modrm & 7) | REX_B(s);
                tcg_gen_addi_ptr(cpu_ptr0, cpu_env, offsetof(CPUState, xmm_regs[rm]));
                gen_helper_movmskpd(cpu_tmp2_i32, cpu_ptr0);
                tcg_gen_extu_i32_tl(cpu_T[0], cpu_tmp2_i32);
                gen_op_mov_reg_T0(OT_LONG, reg);
                break;
            case 0x02a: /* cvtpi2ps */
            case 0x12a: /* cvtpi2pd */
                gen_helper_enter_mmx();
                if(mod != 3) {
                    gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    op2_offset = offsetof(CPUState, mmx_t0);
                    gen_ldq_env_A0(s->base.mem_idx, op2_offset);
                } else {
                    rm = (modrm & 7);
                    op2_offset = offsetof(CPUState, fpregs[rm].mmx);
                }
                op1_offset = offsetof(CPUState, xmm_regs[reg]);
                tcg_gen_addi_ptr(cpu_ptr0, cpu_env, op1_offset);
                tcg_gen_addi_ptr(cpu_ptr1, cpu_env, op2_offset);
                switch(b >> 8) {
                    case 0x0:
                        gen_helper_cvtpi2ps(cpu_ptr0, cpu_ptr1);
                        break;
                    default:
                    case 0x1:
                        gen_helper_cvtpi2pd(cpu_ptr0, cpu_ptr1);
                        break;
                }
                break;
            case 0x22a: /* cvtsi2ss */
            case 0x32a: /* cvtsi2sd */
                ot = (s->dflag == 2) ? OT_QUAD : OT_LONG;
                gen_ldst_modrm(s, modrm, ot, OR_TMP0, 0);
                op1_offset = offsetof(CPUState, xmm_regs[reg]);
                tcg_gen_addi_ptr(cpu_ptr0, cpu_env, op1_offset);
                sse_op2 = sse_op_table3[(s->dflag == 2) * 2 + ((b >> 8) - 2)];
                if(ot == OT_LONG) {
                    tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_T[0]);
                    ((void (*)(TCGv_ptr, TCGv_i32))sse_op2)(cpu_ptr0, cpu_tmp2_i32);
                } else {
                    ((void (*)(TCGv_ptr, TCGv))sse_op2)(cpu_ptr0, cpu_T[0]);
                }
                break;
            case 0x02c: /* cvttps2pi */
            case 0x12c: /* cvttpd2pi */
            case 0x02d: /* cvtps2pi */
            case 0x12d: /* cvtpd2pi */
                gen_helper_enter_mmx();
                if(mod != 3) {
                    gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    op2_offset = offsetof(CPUState, xmm_t0);
                    gen_ldo_env_A0(s->base.mem_idx, op2_offset);
                } else {
                    rm = (modrm & 7) | REX_B(s);
                    op2_offset = offsetof(CPUState, xmm_regs[rm]);
                }
                op1_offset = offsetof(CPUState, fpregs[reg & 7].mmx);
                tcg_gen_addi_ptr(cpu_ptr0, cpu_env, op1_offset);
                tcg_gen_addi_ptr(cpu_ptr1, cpu_env, op2_offset);
                switch(b) {
                    case 0x02c:
                        gen_helper_cvttps2pi(cpu_ptr0, cpu_ptr1);
                        break;
                    case 0x12c:
                        gen_helper_cvttpd2pi(cpu_ptr0, cpu_ptr1);
                        break;
                    case 0x02d:
                        gen_helper_cvtps2pi(cpu_ptr0, cpu_ptr1);
                        break;
                    case 0x12d:
                        gen_helper_cvtpd2pi(cpu_ptr0, cpu_ptr1);
                        break;
                }
                break;
            case 0x22c: /* cvttss2si */
            case 0x32c: /* cvttsd2si */
            case 0x22d: /* cvtss2si */
            case 0x32d: /* cvtsd2si */
                ot = (s->dflag == 2) ? OT_QUAD : OT_LONG;
                if(mod != 3) {
                    gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    if((b >> 8) & 1) {
                        gen_ldq_env_A0(s->base.mem_idx, offsetof(CPUState, xmm_t0.XMM_Q(0)));
                    } else {
                        gen_op_ld_T0_A0(OT_LONG + s->base.mem_idx);
                        tcg_gen_st32_tl(cpu_T[0], cpu_env, offsetof(CPUState, xmm_t0.XMM_L(0)));
                    }
                    op2_offset = offsetof(CPUState, xmm_t0);
                } else {
                    rm = (modrm & 7) | REX_B(s);
                    op2_offset = offsetof(CPUState, xmm_regs[rm]);
                }
                sse_op2 = sse_op_table3[(s->dflag == 2) * 2 + ((b >> 8) - 2) + 4 + (b & 1) * 4];
                tcg_gen_addi_ptr(cpu_ptr0, cpu_env, op2_offset);
                if(ot == OT_LONG) {
                    ((void (*)(TCGv_i32, TCGv_ptr))sse_op2)(cpu_tmp2_i32, cpu_ptr0);
                    tcg_gen_extu_i32_tl(cpu_T[0], cpu_tmp2_i32);
                } else {
                    ((void (*)(TCGv, TCGv_ptr))sse_op2)(cpu_T[0], cpu_ptr0);
                }
                gen_op_mov_reg_T0(ot, reg);
                break;
            case 0xc4: /* pinsrw */
            case 0x1c4:
                s->rip_offset = 1;
                gen_ldst_modrm(s, modrm, OT_WORD, OR_TMP0, 0);
                val = ldub_code(s->base.pc++);
                if(b1) {
                    val &= 7;
                    tcg_gen_st16_tl(cpu_T[0], cpu_env, offsetof(CPUState, xmm_regs[reg].XMM_W(val)));
                } else {
                    val &= 3;
                    tcg_gen_st16_tl(cpu_T[0], cpu_env, offsetof(CPUState, fpregs[reg].mmx.MMX_W(val)));
                }
                break;
            case 0xc5: /* pextrw */
            case 0x1c5:
                if(mod != 3) {
                    goto illegal_op;
                }
                ot = (s->dflag == 2) ? OT_QUAD : OT_LONG;
                val = ldub_code(s->base.pc++);
                if(b1) {
                    val &= 7;
                    rm = (modrm & 7) | REX_B(s);
                    tcg_gen_ld16u_tl(cpu_T[0], cpu_env, offsetof(CPUState, xmm_regs[rm].XMM_W(val)));
                } else {
                    val &= 3;
                    rm = (modrm & 7);
                    tcg_gen_ld16u_tl(cpu_T[0], cpu_env, offsetof(CPUState, fpregs[rm].mmx.MMX_W(val)));
                }
                reg = ((modrm >> 3) & 7) | rex_r;
                gen_op_mov_reg_T0(ot, reg);
                break;
            case 0x1d6: /* movq ea, xmm */
                if(mod != 3) {
                    gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    gen_stq_env_A0(s->base.mem_idx, offsetof(CPUState, xmm_regs[reg].XMM_Q(0)));
                } else {
                    rm = (modrm & 7) | REX_B(s);
                    gen_op_movq(offsetof(CPUState, xmm_regs[rm].XMM_Q(0)), offsetof(CPUState, xmm_regs[reg].XMM_Q(0)));
                    gen_op_movq_env_0(offsetof(CPUState, xmm_regs[rm].XMM_Q(1)));
                }
                break;
            case 0x2d6: /* movq2dq */
                gen_helper_enter_mmx();
                rm = (modrm & 7);
                gen_op_movq(offsetof(CPUState, xmm_regs[reg].XMM_Q(0)), offsetof(CPUState, fpregs[rm].mmx));
                gen_op_movq_env_0(offsetof(CPUState, xmm_regs[reg].XMM_Q(1)));
                break;
            case 0x3d6: /* movdq2q */
                gen_helper_enter_mmx();
                rm = (modrm & 7) | REX_B(s);
                gen_op_movq(offsetof(CPUState, fpregs[reg & 7].mmx), offsetof(CPUState, xmm_regs[rm].XMM_Q(0)));
                break;
            case 0xd7: /* pmovmskb */
            case 0x1d7:
                if(mod != 3) {
                    goto illegal_op;
                }
                if(b1) {
                    rm = (modrm & 7) | REX_B(s);
                    tcg_gen_addi_ptr(cpu_ptr0, cpu_env, offsetof(CPUState, xmm_regs[rm]));
                    gen_helper_pmovmskb_xmm(cpu_tmp2_i32, cpu_ptr0);
                } else {
                    rm = (modrm & 7);
                    tcg_gen_addi_ptr(cpu_ptr0, cpu_env, offsetof(CPUState, fpregs[rm].mmx));
                    gen_helper_pmovmskb_mmx(cpu_tmp2_i32, cpu_ptr0);
                }
                tcg_gen_extu_i32_tl(cpu_T[0], cpu_tmp2_i32);
                reg = ((modrm >> 3) & 7) | rex_r;
                gen_op_mov_reg_T0(OT_LONG, reg);
                break;
            case 0x138:
                if(s->prefix & PREFIX_REPNZ) {
                    goto crc32;
                }
                goto case_0x038;  //  todo: is it fine?
            case 0x038:
            case_0x038:
                b = modrm;
                modrm = ldub_code(s->base.pc++);
                rm = modrm & 7;
                reg = ((modrm >> 3) & 7) | rex_r;
                mod = (modrm >> 6) & 3;
                if(b1 >= 2) {
                    goto illegal_op;
                }

                sse_op2 = sse_op_table6[b].op[b1];
                if(!sse_op2) {
                    goto illegal_op;
                }
                if(!(s->cpuid_ext_features & sse_op_table6[b].ext_mask)) {
                    goto illegal_op;
                }

                if(b1) {
                    op1_offset = offsetof(CPUState, xmm_regs[reg]);
                    if(mod == 3) {
                        op2_offset = offsetof(CPUState, xmm_regs[rm | REX_B(s)]);
                    } else {
                        op2_offset = offsetof(CPUState, xmm_t0);
                        gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                        switch(b) {
                            case 0x20:  //  pmovsxbw, pmovzxbw
                            case 0x30:
                            case 0x23:  //  pmovsxwd, pmovzxwd
                            case 0x33:
                            case 0x25:  //  pmovsxdq, pmovzxdq
                            case 0x35:
                                gen_ldq_env_A0(s->base.mem_idx, op2_offset + offsetof(XMMReg, XMM_Q(0)));
                                break;
                            case 0x21:  //  pmovsxbd, pmovzxbd
                            case 0x31:
                            case 0x24:  //  pmovsxwq, pmovzxwq
                            case 0x34:
                                tcg_gen_qemu_ld32u(cpu_tmp0, cpu_A0, (s->base.mem_idx >> 2) - 1);
                                tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_tmp0);
                                tcg_gen_st_i32(cpu_tmp2_i32, cpu_env, op2_offset + offsetof(XMMReg, XMM_L(0)));
                                break;
                            case 0x22:  //  pmovsxbq, pmovzxbq
                            case 0x32:
                                tcg_gen_qemu_ld16u(cpu_tmp0, cpu_A0, (s->base.mem_idx >> 2) - 1);
                                tcg_gen_st16_tl(cpu_tmp0, cpu_env, op2_offset + offsetof(XMMReg, XMM_W(0)));
                                break;
                            case 0x2a: /* movntqda */
                                gen_ldo_env_A0(s->base.mem_idx, op1_offset);
                                return;
                            default:
                                gen_ldo_env_A0(s->base.mem_idx, op2_offset);
                        }
                    }
                } else {
                    op1_offset = offsetof(CPUState, fpregs[reg].mmx);
                    if(mod == 3) {
                        op2_offset = offsetof(CPUState, fpregs[rm].mmx);
                    } else {
                        op2_offset = offsetof(CPUState, mmx_t0);
                        gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                        gen_ldq_env_A0(s->base.mem_idx, op2_offset);
                    }
                }
                if(sse_op2 == SSE_SPECIAL) {
                    goto illegal_op;
                }

                tcg_gen_addi_ptr(cpu_ptr0, cpu_env, op1_offset);
                tcg_gen_addi_ptr(cpu_ptr1, cpu_env, op2_offset);
                ((void (*)(TCGv_ptr, TCGv_ptr))sse_op2)(cpu_ptr0, cpu_ptr1);

                if(b == 0x17) {
                    s->cc_op = CC_OP_EFLAGS;
                }
                break;
            case 0x338: /* crc32 */
            crc32:
                b = modrm;
                modrm = ldub_code(s->base.pc++);
                reg = ((modrm >> 3) & 7) | rex_r;

                if(b != 0xf0 && b != 0xf1) {
                    goto illegal_op;
                }
                if(!(s->cpuid_ext_features & CPUID_EXT_SSE42)) {
                    goto illegal_op;
                }

                if(b == 0xf0) {
                    ot = OT_BYTE;
                } else if(b == 0xf1 && s->dflag != 2) {
                    if(s->prefix & PREFIX_DATA) {
                        ot = OT_WORD;
                    } else {
                        ot = OT_LONG;
                    }
                } else {
                    ot = OT_QUAD;
                }

                gen_op_mov_TN_reg(OT_LONG, 0, reg);
                tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_T[0]);
                gen_ldst_modrm(s, modrm, ot, OR_TMP0, 0);
                gen_helper_crc32(cpu_T[0], cpu_tmp2_i32, cpu_T[0], tcg_const_i32(8 << ot));

                ot = (s->dflag == 2) ? OT_QUAD : OT_LONG;
                gen_op_mov_reg_T0(ot, reg);
                break;
            case 0x03a:
            case 0x13a:
                b = modrm;
                modrm = ldub_code(s->base.pc++);
                rm = modrm & 7;
                reg = ((modrm >> 3) & 7) | rex_r;
                mod = (modrm >> 6) & 3;
                if(b1 >= 2) {
                    goto illegal_op;
                }

                sse_op2 = sse_op_table7[b].op[b1];
                if(!sse_op2) {
                    goto illegal_op;
                }
                if(!(s->cpuid_ext_features & sse_op_table7[b].ext_mask)) {
                    goto illegal_op;
                }

                if(sse_op2 == SSE_SPECIAL) {
                    ot = (s->dflag == 2) ? OT_QUAD : OT_LONG;
                    rm = (modrm & 7) | REX_B(s);
                    if(mod != 3) {
                        gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    }
                    reg = ((modrm >> 3) & 7) | rex_r;
                    val = ldub_code(s->base.pc++);
                    switch(b) {
                        case 0x14: /* pextrb */
                            tcg_gen_ld8u_tl(cpu_T[0], cpu_env, offsetof(CPUState, xmm_regs[reg].XMM_B(val & 15)));
                            if(mod == 3) {
                                gen_op_mov_reg_T0(ot, rm);
                            } else {
                                tcg_gen_qemu_st8(cpu_T[0], cpu_A0, (s->base.mem_idx >> 2) - 1);
                            }
                            break;
                        case 0x15: /* pextrw */
                            tcg_gen_ld16u_tl(cpu_T[0], cpu_env, offsetof(CPUState, xmm_regs[reg].XMM_W(val & 7)));
                            if(mod == 3) {
                                gen_op_mov_reg_T0(ot, rm);
                            } else {
                                tcg_gen_qemu_st16(cpu_T[0], cpu_A0, (s->base.mem_idx >> 2) - 1);
                            }
                            break;
                        case 0x16:
                            if(ot == OT_LONG) { /* pextrd */
                                tcg_gen_ld_i32(cpu_tmp2_i32, cpu_env, offsetof(CPUState, xmm_regs[reg].XMM_L(val & 3)));
                                tcg_gen_extu_i32_tl(cpu_T[0], cpu_tmp2_i32);
                                if(mod == 3) {
                                    gen_op_mov_reg_v(ot, rm, cpu_T[0]);
                                } else {
                                    tcg_gen_qemu_st32(cpu_T[0], cpu_A0, (s->base.mem_idx >> 2) - 1);
                                }
                            } else { /* pextrq */
#ifdef TARGET_X86_64
                                tcg_gen_ld_i64(cpu_tmp1_i64, cpu_env, offsetof(CPUState, xmm_regs[reg].XMM_Q(val & 1)));
                                if(mod == 3) {
                                    gen_op_mov_reg_v(ot, rm, cpu_tmp1_i64);
                                } else {
                                    tcg_gen_qemu_st64(cpu_tmp1_i64, cpu_A0, (s->base.mem_idx >> 2) - 1);
                                }
#else
                                goto illegal_op;
#endif
                            }
                            break;
                        case 0x17: /* extractps */
                            tcg_gen_ld32u_tl(cpu_T[0], cpu_env, offsetof(CPUState, xmm_regs[reg].XMM_L(val & 3)));
                            if(mod == 3) {
                                gen_op_mov_reg_T0(ot, rm);
                            } else {
                                tcg_gen_qemu_st32(cpu_T[0], cpu_A0, (s->base.mem_idx >> 2) - 1);
                            }
                            break;
                        case 0x20: /* pinsrb */
                            if(mod == 3) {
                                gen_op_mov_TN_reg(OT_LONG, 0, rm);
                            } else {
                                tcg_gen_qemu_ld8u(cpu_tmp0, cpu_A0, (s->base.mem_idx >> 2) - 1);
                            }
                            tcg_gen_st8_tl(cpu_tmp0, cpu_env, offsetof(CPUState, xmm_regs[reg].XMM_B(val & 15)));
                            break;
                        case 0x21: /* insertps */
                            if(mod == 3) {
                                tcg_gen_ld_i32(cpu_tmp2_i32, cpu_env, offsetof(CPUState, xmm_regs[rm].XMM_L((val >> 6) & 3)));
                            } else {
                                tcg_gen_qemu_ld32u(cpu_tmp0, cpu_A0, (s->base.mem_idx >> 2) - 1);
                                tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_tmp0);
                            }
                            tcg_gen_st_i32(cpu_tmp2_i32, cpu_env, offsetof(CPUState, xmm_regs[reg].XMM_L((val >> 4) & 3)));
                            if((val >> 0) & 1) {
                                tcg_gen_st_i32(tcg_const_i32(0 /*float32_zero*/), cpu_env,
                                               offsetof(CPUState, xmm_regs[reg].XMM_L(0)));
                            }
                            if((val >> 1) & 1) {
                                tcg_gen_st_i32(tcg_const_i32(0 /*float32_zero*/), cpu_env,
                                               offsetof(CPUState, xmm_regs[reg].XMM_L(1)));
                            }
                            if((val >> 2) & 1) {
                                tcg_gen_st_i32(tcg_const_i32(0 /*float32_zero*/), cpu_env,
                                               offsetof(CPUState, xmm_regs[reg].XMM_L(2)));
                            }
                            if((val >> 3) & 1) {
                                tcg_gen_st_i32(tcg_const_i32(0 /*float32_zero*/), cpu_env,
                                               offsetof(CPUState, xmm_regs[reg].XMM_L(3)));
                            }
                            break;
                        case 0x22:
                            if(ot == OT_LONG) { /* pinsrd */
                                if(mod == 3) {
                                    gen_op_mov_v_reg(ot, cpu_tmp0, rm);
                                } else {
                                    tcg_gen_qemu_ld32u(cpu_tmp0, cpu_A0, (s->base.mem_idx >> 2) - 1);
                                }
                                tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_tmp0);
                                tcg_gen_st_i32(cpu_tmp2_i32, cpu_env, offsetof(CPUState, xmm_regs[reg].XMM_L(val & 3)));
                            } else { /* pinsrq */
#ifdef TARGET_X86_64
                                if(mod == 3) {
                                    gen_op_mov_v_reg(ot, cpu_tmp1_i64, rm);
                                } else {
                                    tcg_gen_qemu_ld64(cpu_tmp1_i64, cpu_A0, (s->base.mem_idx >> 2) - 1);
                                }
                                tcg_gen_st_i64(cpu_tmp1_i64, cpu_env, offsetof(CPUState, xmm_regs[reg].XMM_Q(val & 1)));
#else
                                goto illegal_op;
#endif
                            }
                            break;
                    }
                    return;
                }

                if(b1) {
                    op1_offset = offsetof(CPUState, xmm_regs[reg]);
                    if(mod == 3) {
                        op2_offset = offsetof(CPUState, xmm_regs[rm | REX_B(s)]);
                    } else {
                        op2_offset = offsetof(CPUState, xmm_t0);
                        gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                        gen_ldo_env_A0(s->base.mem_idx, op2_offset);
                    }
                } else {
                    op1_offset = offsetof(CPUState, fpregs[reg].mmx);
                    if(mod == 3) {
                        op2_offset = offsetof(CPUState, fpregs[rm].mmx);
                    } else {
                        op2_offset = offsetof(CPUState, mmx_t0);
                        gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                        gen_ldq_env_A0(s->base.mem_idx, op2_offset);
                    }
                }
                val = ldub_code(s->base.pc++);

                if((b & 0xfc) == 0x60) { /* pcmpXstrX */
                    s->cc_op = CC_OP_EFLAGS;

                    if(s->dflag == 2) {
                        /* The helper must use entire 64-bit gp registers */
                        val |= 1 << 8;
                    }
                }

                tcg_gen_addi_ptr(cpu_ptr0, cpu_env, op1_offset);
                tcg_gen_addi_ptr(cpu_ptr1, cpu_env, op2_offset);
                ((void (*)(TCGv_ptr, TCGv_ptr, TCGv_i32))sse_op2)(cpu_ptr0, cpu_ptr1, tcg_const_i32(val));
                break;
            default:
                goto illegal_op;
        }
    } else {
        /* generic MMX or SSE operation */
        switch(b) {
            case 0x70: /* pshufx insn */
            case 0xc6: /* pshufx insn */
            case 0xc2: /* compare insns */
                s->rip_offset = 1;
                break;
            default:
                break;
        }
        if(is_xmm) {
            op1_offset = offsetof(CPUState, xmm_regs[reg]);
            if(mod != 3) {
                gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                op2_offset = offsetof(CPUState, xmm_t0);
                if(b1 >= 2 && ((b >= 0x50 && b <= 0x5f && b != 0x5b) || b == 0xc2)) {
                    /* specific case for SSE single instructions */
                    if(b1 == 2) {
                        /* 32 bit access */
                        gen_op_ld_T0_A0(OT_LONG + s->base.mem_idx);
                        tcg_gen_st32_tl(cpu_T[0], cpu_env, offsetof(CPUState, xmm_t0.XMM_L(0)));
                    } else {
                        /* 64 bit access */
                        gen_ldq_env_A0(s->base.mem_idx, offsetof(CPUState, xmm_t0.XMM_D(0)));
                    }
                } else {
                    gen_ldo_env_A0(s->base.mem_idx, op2_offset);
                }
            } else {
                rm = (modrm & 7) | REX_B(s);
                op2_offset = offsetof(CPUState, xmm_regs[rm]);
            }
        } else {
            op1_offset = offsetof(CPUState, fpregs[reg].mmx);
            if(mod != 3) {
                gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                op2_offset = offsetof(CPUState, mmx_t0);
                gen_ldq_env_A0(s->base.mem_idx, op2_offset);
            } else {
                rm = (modrm & 7);
                op2_offset = offsetof(CPUState, fpregs[rm].mmx);
            }
        }
        switch(b) {
            case 0x0f: /* 3DNow! data insns */
                if(!(s->cpuid_ext2_features & CPUID_EXT2_3DNOW)) {
                    goto illegal_op;
                }
                val = ldub_code(s->base.pc++);
                sse_op2 = sse_op_table5[val];
                if(!sse_op2) {
                    goto illegal_op;
                }
                tcg_gen_addi_ptr(cpu_ptr0, cpu_env, op1_offset);
                tcg_gen_addi_ptr(cpu_ptr1, cpu_env, op2_offset);
                ((void (*)(TCGv_ptr, TCGv_ptr))sse_op2)(cpu_ptr0, cpu_ptr1);
                break;
            case 0x70: /* pshufx insn */
            case 0xc6: /* pshufx insn */
                val = ldub_code(s->base.pc++);
                tcg_gen_addi_ptr(cpu_ptr0, cpu_env, op1_offset);
                tcg_gen_addi_ptr(cpu_ptr1, cpu_env, op2_offset);
                ((void (*)(TCGv_ptr, TCGv_ptr, TCGv_i32))sse_op2)(cpu_ptr0, cpu_ptr1, tcg_const_i32(val));
                break;
            case 0xc2:
                /* compare insns */
                val = ldub_code(s->base.pc++);
                if(val >= 8) {
                    goto illegal_op;
                }
                sse_op2 = sse_op_table4[val][b1];
                tcg_gen_addi_ptr(cpu_ptr0, cpu_env, op1_offset);
                tcg_gen_addi_ptr(cpu_ptr1, cpu_env, op2_offset);
                ((void (*)(TCGv_ptr, TCGv_ptr))sse_op2)(cpu_ptr0, cpu_ptr1);
                break;
            case 0xf7:
                /* maskmov : we must prepare A0 */
                if(mod != 3) {
                    goto illegal_op;
                }
#ifdef TARGET_X86_64
                if(s->aflag == 2) {
                    gen_op_movq_A0_reg(R_EDI);
                } else
#endif
                {
                    gen_op_movl_A0_reg(R_EDI);
                    if(s->aflag == 0) {
                        gen_op_andl_A0_ffff();
                    }
                }
                gen_add_A0_ds_seg(s);

                tcg_gen_addi_ptr(cpu_ptr0, cpu_env, op1_offset);
                tcg_gen_addi_ptr(cpu_ptr1, cpu_env, op2_offset);
                ((void (*)(TCGv_ptr, TCGv_ptr, TCGv))sse_op2)(cpu_ptr0, cpu_ptr1, cpu_A0);
                break;
            default:
                tcg_gen_addi_ptr(cpu_ptr0, cpu_env, op1_offset);
                tcg_gen_addi_ptr(cpu_ptr1, cpu_env, op2_offset);
                ((void (*)(TCGv_ptr, TCGv_ptr))sse_op2)(cpu_ptr0, cpu_ptr1);
                break;
        }
        if(b == 0x2e || b == 0x2f) {
            s->cc_op = CC_OP_EFLAGS;
        }
    }
}

/* convert one instruction. s->base.is_jmp is set if the translation must
   be stopped. Return the next pc value */
static int disas_insn(CPUState *env, DisasContext *s)
{
    int b, prefixes, aflag, dflag;
    int shift, ot;
    int modrm, reg, rm, mod, reg_addr, op, opreg, offset_addr, val;
    target_ulong next_eip, tval, pc_start;
    int rex_w, rex_r;

    pc_start = s->base.pc;
    prefixes = 0;
    aflag = s->code32;
    dflag = s->code32;
    s->override = -1;
    rex_w = -1;
    rex_r = 0;
#ifdef TARGET_X86_64
    s->rex_x = 0;
    s->rex_b = 0;
    x86_64_hregs = 0;
#endif
    s->rip_offset = 0; /* for relative ip address */
next_byte:
    b = ldub_code(s->base.pc);

    if(env->count_opcodes) {
        generate_opcode_count_increment(env, b);
    }

    s->base.pc++;
    /* check prefixes */
#ifdef TARGET_X86_64
    if(CODE64(s)) {
        switch(b) {
            case 0xf3:
                prefixes |= PREFIX_REPZ;
                goto next_byte;
            case 0xf2:
                prefixes |= PREFIX_REPNZ;
                goto next_byte;
            case 0xf0:
                prefixes |= PREFIX_LOCK;
                goto next_byte;
            case 0x2e:
                s->override = R_CS;
                goto next_byte;
            case 0x36:
                s->override = R_SS;
                goto next_byte;
            case 0x3e:
                s->override = R_DS;
                goto next_byte;
            case 0x26:
                s->override = R_ES;
                goto next_byte;
            case 0x64:
                s->override = R_FS;
                goto next_byte;
            case 0x65:
                s->override = R_GS;
                goto next_byte;
            case 0x66:
                prefixes |= PREFIX_DATA;
                goto next_byte;
            case 0x67:
                prefixes |= PREFIX_ADR;
                goto next_byte;
            case 0x40 ... 0x4f:
                /* REX prefix */
                rex_w = (b >> 3) & 1;
                rex_r = (b & 0x4) << 1;
                s->rex_x = (b & 0x2) << 2;
                REX_B(s) = (b & 0x1) << 3;
                x86_64_hregs = 1; /* select uniform byte register addressing */
                goto next_byte;
        }
        if(rex_w == 1) {
            /* 0x66 is ignored if rex.w is set */
            dflag = 2;
        } else {
            if(prefixes & PREFIX_DATA) {
                dflag ^= 1;
            }
        }
        if(!(prefixes & PREFIX_ADR)) {
            aflag = 2;
        }
    } else
#endif
    {
        switch(b) {
            case 0xf3:
                prefixes |= PREFIX_REPZ;
                goto next_byte;
            case 0xf2:
                prefixes |= PREFIX_REPNZ;
                goto next_byte;
            case 0xf0:
                prefixes |= PREFIX_LOCK;
                goto next_byte;
            case 0x2e:
                s->override = R_CS;
                goto next_byte;
            case 0x36:
                s->override = R_SS;
                goto next_byte;
            case 0x3e:
                s->override = R_DS;
                goto next_byte;
            case 0x26:
                s->override = R_ES;
                goto next_byte;
            case 0x64:
                s->override = R_FS;
                goto next_byte;
            case 0x65:
                s->override = R_GS;
                goto next_byte;
            case 0x66:
                prefixes |= PREFIX_DATA;
                goto next_byte;
            case 0x67:
                prefixes |= PREFIX_ADR;
                goto next_byte;
        }
        if(prefixes & PREFIX_DATA) {
            dflag ^= 1;
        }
        if(prefixes & PREFIX_ADR) {
            aflag ^= 1;
        }
    }

    s->prefix = prefixes;
    s->aflag = aflag;
    s->dflag = dflag;

    /* now check op code */
reswitch:
    switch(b) {
        case 0x0f:
            /**************************/
            /* extended op code */
            b = ldub_code(s->base.pc++) | 0x100;
            goto reswitch;

        /**************************/
        /* arith & logic */
        case 0x00 ... 0x05:
        case 0x08 ... 0x0d:
        case 0x10 ... 0x15:
        case 0x18 ... 0x1d:
        case 0x20 ... 0x25:
        case 0x28 ... 0x2d:
        case 0x30 ... 0x35:
        case 0x38 ... 0x3d: {
            int op, f, val;
            op = (b >> 3) & 7;
            f = (b >> 1) & 3;

            if((b & 1) == 0) {
                ot = OT_BYTE;
            } else {
                ot = dflag + OT_WORD;
            }

            switch(f) {
                case 0: /* OP Ev, Gv */
                    modrm = ldub_code(s->base.pc++);
                    reg = ((modrm >> 3) & 7) | rex_r;
                    mod = (modrm >> 6) & 3;
                    rm = (modrm & 7) | REX_B(s);
                    if(mod != 3) {
                        gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                        opreg = OR_TMP0;
                    } else if(op == OP_XORL && rm == reg) {
                    xor_zero:
                        /* xor reg, reg optimisation */
                        gen_op_movl_T0_0();
                        s->cc_op = CC_OP_LOGICB + ot;
                        gen_op_mov_reg_T0(ot, reg);
                        gen_op_update1_cc();
                        break;
                    } else {
                        opreg = rm;
                    }
                    gen_op_mov_TN_reg(ot, 1, reg);
                    gen_op(s, op, ot, opreg);
                    break;
                case 1: /* OP Gv, Ev */
                    modrm = ldub_code(s->base.pc++);
                    mod = (modrm >> 6) & 3;
                    reg = ((modrm >> 3) & 7) | rex_r;
                    rm = (modrm & 7) | REX_B(s);
                    if(mod != 3) {
                        gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                        gen_op_ld_T1_A0(ot + s->base.mem_idx);
                    } else if(op == OP_XORL && rm == reg) {
                        goto xor_zero;
                    } else {
                        gen_op_mov_TN_reg(ot, 1, rm);
                    }
                    gen_op(s, op, ot, reg);
                    break;
                case 2: /* OP A, Iv */
                    val = insn_get(s, ot);
                    gen_op_movl_T1_im(val);
                    gen_op(s, op, ot, OR_EAX);
                    break;
            }
        } break;

        case 0x82:
            if(CODE64(s)) {
                goto illegal_op;
            }
            goto case_0x83;
        case 0x80: /* GRP1 */
        case 0x81:
        case 0x83:
        case_0x83: {
            int val;

            if((b & 1) == 0) {
                ot = OT_BYTE;
            } else {
                ot = dflag + OT_WORD;
            }

            modrm = ldub_code(s->base.pc++);
            mod = (modrm >> 6) & 3;
            rm = (modrm & 7) | REX_B(s);
            op = (modrm >> 3) & 7;

            if(mod != 3) {
                if(b == 0x83) {
                    s->rip_offset = 1;
                } else {
                    s->rip_offset = insn_const_size(ot);
                }
                gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                opreg = OR_TMP0;
            } else {
                opreg = rm;
            }

            switch(b) {
                default:
                case 0x80:
                case 0x81:
                case 0x82:
                    val = insn_get(s, ot);
                    break;
                case 0x83:
                    val = (int8_t)insn_get(s, OT_BYTE);
                    break;
            }
            gen_op_movl_T1_im(val);
            gen_op(s, op, ot, opreg);
        } break;

        /**************************/
        /* inc, dec, and other misc arith */
        case 0x40 ... 0x47: /* inc Gv */
            ot = dflag ? OT_LONG : OT_WORD;
            gen_inc(s, ot, OR_EAX + (b & 7), 1);
            break;
        case 0x48 ... 0x4f: /* dec Gv */
            ot = dflag ? OT_LONG : OT_WORD;
            gen_inc(s, ot, OR_EAX + (b & 7), -1);
            break;
        case 0xf6: /* GRP3 */
        case 0xf7:
            if((b & 1) == 0) {
                ot = OT_BYTE;
            } else {
                ot = dflag + OT_WORD;
            }

            modrm = ldub_code(s->base.pc++);
            mod = (modrm >> 6) & 3;
            rm = (modrm & 7) | REX_B(s);
            op = (modrm >> 3) & 7;
            if(mod != 3) {
                if(op == 0) {
                    s->rip_offset = insn_const_size(ot);
                }
                gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                gen_op_ld_T0_A0(ot + s->base.mem_idx);
            } else {
                gen_op_mov_TN_reg(ot, 0, rm);
            }

            switch(op) {
                case 0: /* test */
                    val = insn_get(s, ot);
                    gen_op_movl_T1_im(val);
                    gen_op_testl_T0_T1_cc();
                    s->cc_op = CC_OP_LOGICB + ot;
                    break;
                case 2: /* not */
                    tcg_gen_not_tl(cpu_T[0], cpu_T[0]);
                    if(mod != 3) {
                        gen_op_st_T0_A0(ot + s->base.mem_idx);
                    } else {
                        gen_op_mov_reg_T0(ot, rm);
                    }
                    break;
                case 3: /* neg */
                    tcg_gen_neg_tl(cpu_T[0], cpu_T[0]);
                    if(mod != 3) {
                        gen_op_st_T0_A0(ot + s->base.mem_idx);
                    } else {
                        gen_op_mov_reg_T0(ot, rm);
                    }
                    gen_op_update_neg_cc();
                    s->cc_op = CC_OP_SUBB + ot;
                    break;
                case 4: /* mul */
                    switch(ot) {
                        case OT_BYTE:
                            gen_op_mov_TN_reg(OT_BYTE, 1, R_EAX);
                            tcg_gen_ext8u_tl(cpu_T[0], cpu_T[0]);
                            tcg_gen_ext8u_tl(cpu_T[1], cpu_T[1]);
                            /* XXX: use 32 bit mul which could be faster */
                            tcg_gen_mul_tl(cpu_T[0], cpu_T[0], cpu_T[1]);
                            gen_op_mov_reg_T0(OT_WORD, R_EAX);
                            tcg_gen_mov_tl(cpu_cc_dst, cpu_T[0]);
                            tcg_gen_andi_tl(cpu_cc_src, cpu_T[0], 0xff00);
                            s->cc_op = CC_OP_MULB;
                            break;
                        case OT_WORD:
                            gen_op_mov_TN_reg(OT_WORD, 1, R_EAX);
                            tcg_gen_ext16u_tl(cpu_T[0], cpu_T[0]);
                            tcg_gen_ext16u_tl(cpu_T[1], cpu_T[1]);
                            /* XXX: use 32 bit mul which could be faster */
                            tcg_gen_mul_tl(cpu_T[0], cpu_T[0], cpu_T[1]);
                            gen_op_mov_reg_T0(OT_WORD, R_EAX);
                            tcg_gen_mov_tl(cpu_cc_dst, cpu_T[0]);
                            tcg_gen_shri_tl(cpu_T[0], cpu_T[0], 16);
                            gen_op_mov_reg_T0(OT_WORD, R_EDX);
                            tcg_gen_mov_tl(cpu_cc_src, cpu_T[0]);
                            s->cc_op = CC_OP_MULW;
                            break;
                        default:
                        case OT_LONG:
#ifdef TARGET_X86_64
                            gen_op_mov_TN_reg(OT_LONG, 1, R_EAX);
                            tcg_gen_ext32u_tl(cpu_T[0], cpu_T[0]);
                            tcg_gen_ext32u_tl(cpu_T[1], cpu_T[1]);
                            tcg_gen_mul_tl(cpu_T[0], cpu_T[0], cpu_T[1]);
                            gen_op_mov_reg_T0(OT_LONG, R_EAX);
                            tcg_gen_mov_tl(cpu_cc_dst, cpu_T[0]);
                            tcg_gen_shri_tl(cpu_T[0], cpu_T[0], 32);
                            gen_op_mov_reg_T0(OT_LONG, R_EDX);
                            tcg_gen_mov_tl(cpu_cc_src, cpu_T[0]);
#else
                        {
                            TCGv_i64 t0, t1;
                            t0 = tcg_temp_new_i64();
                            t1 = tcg_temp_new_i64();
                            gen_op_mov_TN_reg(OT_LONG, 1, R_EAX);
                            tcg_gen_extu_i32_i64(t0, cpu_T[0]);
                            tcg_gen_extu_i32_i64(t1, cpu_T[1]);
                            tcg_gen_mul_i64(t0, t0, t1);
                            tcg_gen_trunc_i64_i32(cpu_T[0], t0);
                            gen_op_mov_reg_T0(OT_LONG, R_EAX);
                            tcg_gen_mov_tl(cpu_cc_dst, cpu_T[0]);
                            tcg_gen_shri_i64(t0, t0, 32);
                            tcg_gen_trunc_i64_i32(cpu_T[0], t0);
                            gen_op_mov_reg_T0(OT_LONG, R_EDX);
                            tcg_gen_mov_tl(cpu_cc_src, cpu_T[0]);
                        }
#endif
                            s->cc_op = CC_OP_MULL;
                            break;
#ifdef TARGET_X86_64
                        case OT_QUAD:
                            gen_helper_mulq_EAX_T0(cpu_T[0]);
                            s->cc_op = CC_OP_MULQ;
                            break;
#endif
                    }
                    break;
                case 5: /* imul */
                    switch(ot) {
                        case OT_BYTE:
                            gen_op_mov_TN_reg(OT_BYTE, 1, R_EAX);
                            tcg_gen_ext8s_tl(cpu_T[0], cpu_T[0]);
                            tcg_gen_ext8s_tl(cpu_T[1], cpu_T[1]);
                            /* XXX: use 32 bit mul which could be faster */
                            tcg_gen_mul_tl(cpu_T[0], cpu_T[0], cpu_T[1]);
                            gen_op_mov_reg_T0(OT_WORD, R_EAX);
                            tcg_gen_mov_tl(cpu_cc_dst, cpu_T[0]);
                            tcg_gen_ext8s_tl(cpu_tmp0, cpu_T[0]);
                            tcg_gen_sub_tl(cpu_cc_src, cpu_T[0], cpu_tmp0);
                            s->cc_op = CC_OP_MULB;
                            break;
                        case OT_WORD:
                            gen_op_mov_TN_reg(OT_WORD, 1, R_EAX);
                            tcg_gen_ext16s_tl(cpu_T[0], cpu_T[0]);
                            tcg_gen_ext16s_tl(cpu_T[1], cpu_T[1]);
                            /* XXX: use 32 bit mul which could be faster */
                            tcg_gen_mul_tl(cpu_T[0], cpu_T[0], cpu_T[1]);
                            gen_op_mov_reg_T0(OT_WORD, R_EAX);
                            tcg_gen_mov_tl(cpu_cc_dst, cpu_T[0]);
                            tcg_gen_ext16s_tl(cpu_tmp0, cpu_T[0]);
                            tcg_gen_sub_tl(cpu_cc_src, cpu_T[0], cpu_tmp0);
                            tcg_gen_shri_tl(cpu_T[0], cpu_T[0], 16);
                            gen_op_mov_reg_T0(OT_WORD, R_EDX);
                            s->cc_op = CC_OP_MULW;
                            break;
                        default:
                        case OT_LONG:
#ifdef TARGET_X86_64
                            gen_op_mov_TN_reg(OT_LONG, 1, R_EAX);
                            tcg_gen_ext32s_tl(cpu_T[0], cpu_T[0]);
                            tcg_gen_ext32s_tl(cpu_T[1], cpu_T[1]);
                            tcg_gen_mul_tl(cpu_T[0], cpu_T[0], cpu_T[1]);
                            gen_op_mov_reg_T0(OT_LONG, R_EAX);
                            tcg_gen_mov_tl(cpu_cc_dst, cpu_T[0]);
                            tcg_gen_ext32s_tl(cpu_tmp0, cpu_T[0]);
                            tcg_gen_sub_tl(cpu_cc_src, cpu_T[0], cpu_tmp0);
                            tcg_gen_shri_tl(cpu_T[0], cpu_T[0], 32);
                            gen_op_mov_reg_T0(OT_LONG, R_EDX);
#else
                        {
                            TCGv_i64 t0, t1;
                            t0 = tcg_temp_new_i64();
                            t1 = tcg_temp_new_i64();
                            gen_op_mov_TN_reg(OT_LONG, 1, R_EAX);
                            tcg_gen_ext_i32_i64(t0, cpu_T[0]);
                            tcg_gen_ext_i32_i64(t1, cpu_T[1]);
                            tcg_gen_mul_i64(t0, t0, t1);
                            tcg_gen_trunc_i64_i32(cpu_T[0], t0);
                            gen_op_mov_reg_T0(OT_LONG, R_EAX);
                            tcg_gen_mov_tl(cpu_cc_dst, cpu_T[0]);
                            tcg_gen_sari_tl(cpu_tmp0, cpu_T[0], 31);
                            tcg_gen_shri_i64(t0, t0, 32);
                            tcg_gen_trunc_i64_i32(cpu_T[0], t0);
                            gen_op_mov_reg_T0(OT_LONG, R_EDX);
                            tcg_gen_sub_tl(cpu_cc_src, cpu_T[0], cpu_tmp0);
                        }
#endif
                            s->cc_op = CC_OP_MULL;
                            break;
#ifdef TARGET_X86_64
                        case OT_QUAD:
                            gen_helper_imulq_EAX_T0(cpu_T[0]);
                            s->cc_op = CC_OP_MULQ;
                            break;
#endif
                    }
                    break;
                case 6: /* div */
                    switch(ot) {
                        case OT_BYTE:
                            gen_jmp_im(pc_start - s->cs_base);
                            gen_helper_divb_AL(cpu_T[0]);
                            break;
                        case OT_WORD:
                            gen_jmp_im(pc_start - s->cs_base);
                            gen_helper_divw_AX(cpu_T[0]);
                            break;
                        default:
                        case OT_LONG:
                            gen_jmp_im(pc_start - s->cs_base);
                            gen_helper_divl_EAX(cpu_T[0]);
                            break;
#ifdef TARGET_X86_64
                        case OT_QUAD:
                            gen_jmp_im(pc_start - s->cs_base);
                            gen_helper_divq_EAX(cpu_T[0]);
                            break;
#endif
                    }
                    break;
                case 7: /* idiv */
                    switch(ot) {
                        case OT_BYTE:
                            gen_jmp_im(pc_start - s->cs_base);
                            gen_helper_idivb_AL(cpu_T[0]);
                            break;
                        case OT_WORD:
                            gen_jmp_im(pc_start - s->cs_base);
                            gen_helper_idivw_AX(cpu_T[0]);
                            break;
                        default:
                        case OT_LONG:
                            gen_jmp_im(pc_start - s->cs_base);
                            gen_helper_idivl_EAX(cpu_T[0]);
                            break;
#ifdef TARGET_X86_64
                        case OT_QUAD:
                            gen_jmp_im(pc_start - s->cs_base);
                            gen_helper_idivq_EAX(cpu_T[0]);
                            break;
#endif
                    }
                    break;
                default:
                    goto illegal_op;
            }
            break;

        case 0xfe: /* GRP4 */
        case 0xff: /* GRP5 */
            if((b & 1) == 0) {
                ot = OT_BYTE;
            } else {
                ot = dflag + OT_WORD;
            }

            modrm = ldub_code(s->base.pc++);
            mod = (modrm >> 6) & 3;
            rm = (modrm & 7) | REX_B(s);
            op = (modrm >> 3) & 7;
            if(op >= 2 && b == 0xfe) {
                goto illegal_op;
            }
            if(CODE64(s)) {
                if(op == 2 || op == 4) {
                    /* operand size for jumps is 64 bit */
                    ot = OT_QUAD;
                } else if(op == 3 || op == 5) {
                    ot = dflag ? OT_LONG + (rex_w == 1) : OT_WORD;
                } else if(op == 6) {
                    /* default push size is 64 bit */
                    ot = dflag ? OT_QUAD : OT_WORD;
                }
            }
            if(mod != 3) {
                gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                if(op >= 2 && op != 3 && op != 5) {
                    gen_op_ld_T0_A0(ot + s->base.mem_idx);
                }
            } else {
                gen_op_mov_TN_reg(ot, 0, rm);
            }

            switch(op) {
                case 0: /* inc Ev */
                    if(mod != 3) {
                        opreg = OR_TMP0;
                    } else {
                        opreg = rm;
                    }
                    gen_inc(s, ot, opreg, 1);
                    break;
                case 1: /* dec Ev */
                    if(mod != 3) {
                        opreg = OR_TMP0;
                    } else {
                        opreg = rm;
                    }
                    gen_inc(s, ot, opreg, -1);
                    break;
                case 2: /* call Ev */
                    /* XXX: optimize if memory (no 'and' is necessary) */
                    if(s->dflag == 0) {
                        gen_op_andl_T0_ffff();
                    }
                    next_eip = s->base.pc - s->cs_base;
                    gen_movtl_T1_im(next_eip);
                    gen_push_T1(s);
                    gen_op_jmp_T0();
                    gen_eob(s);
                    break;
                case 3: /* lcall Ev */
                    gen_op_ld_T1_A0(ot + s->base.mem_idx);
                    gen_add_A0_im(s, 1 << (ot - OT_WORD + 1));
                    gen_op_ldu_T0_A0(OT_WORD + s->base.mem_idx);
                do_lcall:
                    if(s->pe && !s->vm86) {
                        if(s->cc_op != CC_OP_DYNAMIC) {
                            gen_op_set_cc_op(s->cc_op);
                        }
                        gen_jmp_im(pc_start - s->cs_base);
                        tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_T[0]);
                        gen_helper_lcall_protected(cpu_tmp2_i32, cpu_T[1], tcg_const_i32(dflag),
                                                   tcg_const_i32(s->base.pc - pc_start));
                    } else {
                        tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_T[0]);
                        gen_helper_lcall_real(cpu_tmp2_i32, cpu_T[1], tcg_const_i32(dflag),
                                              tcg_const_i32(s->base.pc - s->cs_base));
                    }
                    gen_eob(s);
                    break;
                case 4: /* jmp Ev */
                    if(s->dflag == 0) {
                        gen_op_andl_T0_ffff();
                    }
                    gen_op_jmp_T0();
                    gen_eob(s);
                    break;
                case 5: /* ljmp Ev */
                    gen_op_ld_T1_A0(ot + s->base.mem_idx);
                    gen_add_A0_im(s, 1 << (ot - OT_WORD + 1));
                    gen_op_ldu_T0_A0(OT_WORD + s->base.mem_idx);
                do_ljmp:
                    if(s->pe && !s->vm86) {
                        if(s->cc_op != CC_OP_DYNAMIC) {
                            gen_op_set_cc_op(s->cc_op);
                        }
                        gen_jmp_im(pc_start - s->cs_base);
                        tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_T[0]);
                        gen_helper_ljmp_protected(cpu_tmp2_i32, cpu_T[1], tcg_const_i32(s->base.pc - pc_start));
                    } else {
                        gen_op_movl_seg_T0_vm(R_CS);
                        gen_op_movl_T0_T1();
                        gen_op_jmp_T0();
                    }
                    gen_eob(s);
                    break;
                case 6: /* push Ev */
                    gen_push_T0(s);
                    break;
                default:
                    goto illegal_op;
            }
            break;

        case 0x84: /* test Ev, Gv */
        case 0x85:
            if((b & 1) == 0) {
                ot = OT_BYTE;
            } else {
                ot = dflag + OT_WORD;
            }

            modrm = ldub_code(s->base.pc++);
            reg = ((modrm >> 3) & 7) | rex_r;

            gen_ldst_modrm(s, modrm, ot, OR_TMP0, 0);
            gen_op_mov_TN_reg(ot, 1, reg);
            gen_op_testl_T0_T1_cc();
            s->cc_op = CC_OP_LOGICB + ot;
            break;

        case 0xa8: /* test eAX, Iv */
        case 0xa9:
            if((b & 1) == 0) {
                ot = OT_BYTE;
            } else {
                ot = dflag + OT_WORD;
            }
            val = insn_get(s, ot);

            gen_op_mov_TN_reg(ot, 0, OR_EAX);
            gen_op_movl_T1_im(val);
            gen_op_testl_T0_T1_cc();
            s->cc_op = CC_OP_LOGICB + ot;
            break;

        case 0x98: /* CWDE/CBW */
#ifdef TARGET_X86_64
            if(dflag == 2) {
                gen_op_mov_TN_reg(OT_LONG, 0, R_EAX);
                tcg_gen_ext32s_tl(cpu_T[0], cpu_T[0]);
                gen_op_mov_reg_T0(OT_QUAD, R_EAX);
            } else
#endif
                if(dflag == 1) {
                gen_op_mov_TN_reg(OT_WORD, 0, R_EAX);
                tcg_gen_ext16s_tl(cpu_T[0], cpu_T[0]);
                gen_op_mov_reg_T0(OT_LONG, R_EAX);
            } else {
                gen_op_mov_TN_reg(OT_BYTE, 0, R_EAX);
                tcg_gen_ext8s_tl(cpu_T[0], cpu_T[0]);
                gen_op_mov_reg_T0(OT_WORD, R_EAX);
            }
            break;
        case 0x99: /* CDQ/CWD */
#ifdef TARGET_X86_64
            if(dflag == 2) {
                gen_op_mov_TN_reg(OT_QUAD, 0, R_EAX);
                tcg_gen_sari_tl(cpu_T[0], cpu_T[0], 63);
                gen_op_mov_reg_T0(OT_QUAD, R_EDX);
            } else
#endif
                if(dflag == 1) {
                gen_op_mov_TN_reg(OT_LONG, 0, R_EAX);
                tcg_gen_ext32s_tl(cpu_T[0], cpu_T[0]);
                tcg_gen_sari_tl(cpu_T[0], cpu_T[0], 31);
                gen_op_mov_reg_T0(OT_LONG, R_EDX);
            } else {
                gen_op_mov_TN_reg(OT_WORD, 0, R_EAX);
                tcg_gen_ext16s_tl(cpu_T[0], cpu_T[0]);
                tcg_gen_sari_tl(cpu_T[0], cpu_T[0], 15);
                gen_op_mov_reg_T0(OT_WORD, R_EDX);
            }
            break;
        case 0x1af: /* imul Gv, Ev */
        case 0x69:  /* imul Gv, Ev, I */
        case 0x6b:
            ot = dflag + OT_WORD;
            modrm = ldub_code(s->base.pc++);
            reg = ((modrm >> 3) & 7) | rex_r;
            if(b == 0x69) {
                s->rip_offset = insn_const_size(ot);
            } else if(b == 0x6b) {
                s->rip_offset = 1;
            }
            gen_ldst_modrm(s, modrm, ot, OR_TMP0, 0);
            if(b == 0x69) {
                val = insn_get(s, ot);
                gen_op_movl_T1_im(val);
            } else if(b == 0x6b) {
                val = (int8_t)insn_get(s, OT_BYTE);
                gen_op_movl_T1_im(val);
            } else {
                gen_op_mov_TN_reg(ot, 1, reg);
            }

#ifdef TARGET_X86_64
            if(ot == OT_QUAD) {
                gen_helper_imulq_T0_T1(cpu_T[0], cpu_T[0], cpu_T[1]);
            } else
#endif
                if(ot == OT_LONG) {
#ifdef TARGET_X86_64
                tcg_gen_ext32s_tl(cpu_T[0], cpu_T[0]);
                tcg_gen_ext32s_tl(cpu_T[1], cpu_T[1]);
                tcg_gen_mul_tl(cpu_T[0], cpu_T[0], cpu_T[1]);
                tcg_gen_mov_tl(cpu_cc_dst, cpu_T[0]);
                tcg_gen_ext32s_tl(cpu_tmp0, cpu_T[0]);
                tcg_gen_sub_tl(cpu_cc_src, cpu_T[0], cpu_tmp0);
#else
                {
                    TCGv_i64 t0, t1;
                    t0 = tcg_temp_new_i64();
                    t1 = tcg_temp_new_i64();
                    tcg_gen_ext_i32_i64(t0, cpu_T[0]);
                    tcg_gen_ext_i32_i64(t1, cpu_T[1]);
                    tcg_gen_mul_i64(t0, t0, t1);
                    tcg_gen_trunc_i64_i32(cpu_T[0], t0);
                    tcg_gen_mov_tl(cpu_cc_dst, cpu_T[0]);
                    tcg_gen_sari_tl(cpu_tmp0, cpu_T[0], 31);
                    tcg_gen_shri_i64(t0, t0, 32);
                    tcg_gen_trunc_i64_i32(cpu_T[1], t0);
                    tcg_gen_sub_tl(cpu_cc_src, cpu_T[1], cpu_tmp0);
                }
#endif
            } else {
                tcg_gen_ext16s_tl(cpu_T[0], cpu_T[0]);
                tcg_gen_ext16s_tl(cpu_T[1], cpu_T[1]);
                /* XXX: use 32 bit mul which could be faster */
                tcg_gen_mul_tl(cpu_T[0], cpu_T[0], cpu_T[1]);
                tcg_gen_mov_tl(cpu_cc_dst, cpu_T[0]);
                tcg_gen_ext16s_tl(cpu_tmp0, cpu_T[0]);
                tcg_gen_sub_tl(cpu_cc_src, cpu_T[0], cpu_tmp0);
            }
            gen_op_mov_reg_T0(ot, reg);
            s->cc_op = CC_OP_MULB + ot;
            break;
        case 0x1c0:  //  xadd Ev, Gv
        case 0x1c1:
            if((b & 1) == 0) {
                ot = OT_BYTE;
            } else {
                ot = dflag + OT_WORD;
            }
            modrm = ldub_code(s->base.pc++);
            reg = ((modrm >> 3) & 7) | rex_r;
            mod = (modrm >> 6) & 3;
            if(mod == 3) {
                rm = (modrm & 7) | REX_B(s);
                gen_op_mov_TN_reg(ot, 0, reg);
                gen_op_mov_TN_reg(ot, 1, rm);
                gen_op_addl_T0_T1();
                gen_op_mov_reg_T1(ot, reg);
                gen_op_mov_reg_T0(ot, rm);
            } else {
                gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                gen_op_mov_TN_reg(ot, 0, reg);
                gen_op_ld_T1_A0(ot + s->base.mem_idx);
                gen_op_addl_T0_T1();
                gen_op_st_T0_A0(ot + s->base.mem_idx);
                gen_op_mov_reg_T1(ot, reg);
            }
            gen_op_update2_cc();
            s->cc_op = CC_OP_ADDB + ot;
            break;
        case 0x1b0:  //  cmpxchg Ev, Gv
        case 0x1b1: {
            int label1, label2;
            TCGv t0, t1, t2, a0;

            if((b & 1) == 0) {
                ot = OT_BYTE;
            } else {
                ot = dflag + OT_WORD;
            }
            modrm = ldub_code(s->base.pc++);
            reg = ((modrm >> 3) & 7) | rex_r;
            mod = (modrm >> 6) & 3;
            t0 = tcg_temp_local_new();
            t1 = tcg_temp_local_new();
            t2 = tcg_temp_local_new();
            a0 = tcg_temp_local_new();
            gen_op_mov_v_reg(ot, t1, reg);
            if(mod == 3) {
                rm = (modrm & 7) | REX_B(s);
                gen_op_mov_v_reg(ot, t0, rm);
            } else {
                gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                tcg_gen_mov_tl(a0, cpu_A0);
                gen_op_ld_v(ot + s->base.mem_idx, t0, a0);
                rm = 0; /* avoid warning */
            }
            label1 = gen_new_label();
            tcg_gen_sub_tl(t2, cpu_regs[R_EAX], t0);
            gen_extu(ot, t2);
            tcg_gen_brcondi_tl(TCG_COND_EQ, t2, 0, label1);
            if(mod == 3) {
                label2 = gen_new_label();
                gen_op_mov_reg_v(ot, R_EAX, t0);
                tcg_gen_br(label2);
                gen_set_label(label1);
                gen_op_mov_reg_v(ot, rm, t1);
                gen_set_label(label2);
            } else {
                tcg_gen_mov_tl(t1, t0);
                gen_op_mov_reg_v(ot, R_EAX, t0);
                gen_set_label(label1);
                /* always store */
                gen_op_st_v(ot + s->base.mem_idx, t1, a0);
            }
            tcg_gen_mov_tl(cpu_cc_src, t0);
            tcg_gen_mov_tl(cpu_cc_dst, t2);
            s->cc_op = CC_OP_SUBB + ot;
            tcg_temp_free(t0);
            tcg_temp_free(t1);
            tcg_temp_free(t2);
            tcg_temp_free(a0);
        } break;
        case 0x1c7: /* cmpxchg8b */
            modrm = ldub_code(s->base.pc++);
            mod = (modrm >> 6) & 3;
            if((mod == 3) || ((modrm & 0x38) != 0x8)) {
                goto illegal_op;
            }
#ifdef TARGET_X86_64
            if(dflag == 2) {
                if(!(s->cpuid_ext_features & CPUID_EXT_CX16)) {
                    goto illegal_op;
                }
                gen_jmp_im(pc_start - s->cs_base);
                if(s->cc_op != CC_OP_DYNAMIC) {
                    gen_op_set_cc_op(s->cc_op);
                }
                gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                gen_helper_cmpxchg16b(cpu_A0);
            } else
#endif
            {
                if(!(s->cpuid_features & CPUID_CX8)) {
                    goto illegal_op;
                }
                gen_jmp_im(pc_start - s->cs_base);
                if(s->cc_op != CC_OP_DYNAMIC) {
                    gen_op_set_cc_op(s->cc_op);
                }
                gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                gen_helper_cmpxchg8b(cpu_A0);
            }
            s->cc_op = CC_OP_EFLAGS;
            break;

        /**************************/
        /* push/pop */
        case 0x50 ... 0x57: /* push */
            gen_op_mov_TN_reg(OT_LONG, 0, (b & 7) | REX_B(s));
            gen_push_T0(s);
            break;
        case 0x58 ... 0x5f: /* pop */
            if(CODE64(s)) {
                ot = dflag ? OT_QUAD : OT_WORD;
            } else {
                ot = dflag + OT_WORD;
            }
            gen_pop_T0(s);
            /* NOTE: order is important for pop %sp */
            gen_pop_update(s);
            gen_op_mov_reg_T0(ot, (b & 7) | REX_B(s));
            break;
        case 0x60: /* pusha */
            if(CODE64(s)) {
                goto illegal_op;
            }
            gen_pusha(s);
            break;
        case 0x61: /* popa */
            if(CODE64(s)) {
                goto illegal_op;
            }
            gen_popa(s);
            break;
        case 0x68: /* push Iv */
        case 0x6a:
            if(CODE64(s)) {
                ot = dflag ? OT_QUAD : OT_WORD;
            } else {
                ot = dflag + OT_WORD;
            }
            if(b == 0x68) {
                val = insn_get(s, ot);
            } else {
                val = (int8_t)insn_get(s, OT_BYTE);
            }
            gen_op_movl_T0_im(val);
            gen_push_T0(s);
            break;
        case 0x8f: /* pop Ev */
            if(CODE64(s)) {
                ot = dflag ? OT_QUAD : OT_WORD;
            } else {
                ot = dflag + OT_WORD;
            }
            modrm = ldub_code(s->base.pc++);
            mod = (modrm >> 6) & 3;
            gen_pop_T0(s);
            if(mod == 3) {
                /* NOTE: order is important for pop %sp */
                gen_pop_update(s);
                rm = (modrm & 7) | REX_B(s);
                gen_op_mov_reg_T0(ot, rm);
            } else {
                /* NOTE: order is important too for MMU exceptions */
                s->popl_esp_hack = 1 << ot;
                gen_ldst_modrm(s, modrm, ot, OR_TMP0, 1);
                s->popl_esp_hack = 0;
                gen_pop_update(s);
            }
            break;
        case 0xc8: /* enter */
        {
            int level;
            val = lduw_code(s->base.pc);
            s->base.pc += 2;
            level = ldub_code(s->base.pc++);
            gen_enter(s, val, level);
        } break;
        case 0xc9: /* leave */
            /* XXX: exception not precise (ESP is updated before potential exception) */
            if(CODE64(s)) {
                gen_op_mov_TN_reg(OT_QUAD, 0, R_EBP);
                gen_op_mov_reg_T0(OT_QUAD, R_ESP);
            } else if(s->ss32) {
                gen_op_mov_TN_reg(OT_LONG, 0, R_EBP);
                gen_op_mov_reg_T0(OT_LONG, R_ESP);
            } else {
                gen_op_mov_TN_reg(OT_WORD, 0, R_EBP);
                gen_op_mov_reg_T0(OT_WORD, R_ESP);
            }
            gen_pop_T0(s);
            if(CODE64(s)) {
                ot = dflag ? OT_QUAD : OT_WORD;
            } else {
                ot = dflag + OT_WORD;
            }
            gen_op_mov_reg_T0(ot, R_EBP);
            gen_pop_update(s);
            break;
        case 0x06: /* push es */
        case 0x0e: /* push cs */
        case 0x16: /* push ss */
        case 0x1e: /* push ds */
            if(CODE64(s)) {
                goto illegal_op;
            }
            gen_op_movl_T0_seg(b >> 3);
            gen_push_T0(s);
            break;
        case 0x1a0: /* push fs */
        case 0x1a8: /* push gs */
            gen_op_movl_T0_seg((b >> 3) & 7);
            gen_push_T0(s);
            break;
        case 0x07: /* pop es */
        case 0x17: /* pop ss */
        case 0x1f: /* pop ds */
            if(CODE64(s)) {
                goto illegal_op;
            }
            reg = b >> 3;
            gen_pop_T0(s);
            gen_movl_seg_T0(s, reg, pc_start - s->cs_base);
            gen_pop_update(s);
            if(reg == R_SS) {
                /* if reg == SS, inhibit interrupts/trace. */
                /* If several instructions disable interrupts, only the
                   _first_ does it */
                if(!(s->base.tb->flags & HF_INHIBIT_IRQ_MASK)) {
                    gen_helper_set_inhibit_irq();
                }
                s->tf = 0;
            }
            if(s->base.is_jmp) {
                gen_jmp_im(s->base.pc - s->cs_base);
                gen_eob(s);
            }
            break;
        case 0x1a1: /* pop fs */
        case 0x1a9: /* pop gs */
            gen_pop_T0(s);
            gen_movl_seg_T0(s, (b >> 3) & 7, pc_start - s->cs_base);
            gen_pop_update(s);
            if(s->base.is_jmp) {
                gen_jmp_im(s->base.pc - s->cs_base);
                gen_eob(s);
            }
            break;

        /**************************/
        /* mov */
        case 0x88:  //  mov Gv, Ev
        case 0x89:
            if((b & 1) == 0) {
                ot = OT_BYTE;
            } else {
                ot = dflag + OT_WORD;
            }
            modrm = ldub_code(s->base.pc++);
            reg = ((modrm >> 3) & 7) | rex_r;

            /* generate a generic store */
            gen_ldst_modrm(s, modrm, ot, reg, 1);
            break;
        case 0xc6:  //  mov Ev, Iv
        case 0xc7:
            if((b & 1) == 0) {
                ot = OT_BYTE;
            } else {
                ot = dflag + OT_WORD;
            }
            modrm = ldub_code(s->base.pc++);
            mod = (modrm >> 6) & 3;
            if(mod != 3) {
                s->rip_offset = insn_const_size(ot);
                gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
            }
            val = insn_get(s, ot);
            gen_op_movl_T0_im(val);
            if(mod != 3) {
                gen_op_st_T0_A0(ot + s->base.mem_idx);
            } else {
                gen_op_mov_reg_T0(ot, (modrm & 7) | REX_B(s));
            }
            break;
        case 0x8a:  //  mov Ev, Gv
        case 0x8b:
            if((b & 1) == 0) {
                ot = OT_BYTE;
            } else {
                ot = OT_WORD + dflag;
            }
            modrm = ldub_code(s->base.pc++);
            reg = ((modrm >> 3) & 7) | rex_r;

            gen_ldst_modrm(s, modrm, ot, OR_TMP0, 0);
            gen_op_mov_reg_T0(ot, reg);
            break;
        case 0x8e: /* mov seg, Gv */
            modrm = ldub_code(s->base.pc++);
            reg = (modrm >> 3) & 7;
            if(reg >= 6 || reg == R_CS) {
                goto illegal_op;
            }
            gen_ldst_modrm(s, modrm, OT_WORD, OR_TMP0, 0);
            gen_movl_seg_T0(s, reg, pc_start - s->cs_base);
            if(reg == R_SS) {
                /* if reg == SS, inhibit interrupts/trace */
                /* If several instructions disable interrupts, only the
                   _first_ does it */
                if(!(s->base.tb->flags & HF_INHIBIT_IRQ_MASK)) {
                    gen_helper_set_inhibit_irq();
                }
                s->tf = 0;
            }
            if(s->base.is_jmp) {
                gen_jmp_im(s->base.pc - s->cs_base);
                gen_eob(s);
            }
            break;
        case 0x8c: /* mov Gv, seg */
            modrm = ldub_code(s->base.pc++);
            reg = (modrm >> 3) & 7;
            mod = (modrm >> 6) & 3;
            if(reg >= 6) {
                goto illegal_op;
            }
            gen_op_movl_T0_seg(reg);
            if(mod == 3) {
                ot = OT_WORD + dflag;
            } else {
                ot = OT_WORD;
            }
            gen_ldst_modrm(s, modrm, ot, OR_TMP0, 1);
            break;

        case 0x1b6: /* movzbS Gv, Eb */
        case 0x1b7: /* movzwS Gv, Eb */
        case 0x1be: /* movsbS Gv, Eb */
        case 0x1bf: /* movswS Gv, Eb */
        {
            int d_ot;
            /* d_ot is the size of destination */
            d_ot = dflag + OT_WORD;
            /* ot is the size of source */
            ot = (b & 1) + OT_BYTE;
            modrm = ldub_code(s->base.pc++);
            reg = ((modrm >> 3) & 7) | rex_r;
            mod = (modrm >> 6) & 3;
            rm = (modrm & 7) | REX_B(s);

            if(mod == 3) {
                gen_op_mov_TN_reg(ot, 0, rm);
                switch(ot | (b & 8)) {
                    case OT_BYTE:
                        tcg_gen_ext8u_tl(cpu_T[0], cpu_T[0]);
                        break;
                    case OT_BYTE | 8:
                        tcg_gen_ext8s_tl(cpu_T[0], cpu_T[0]);
                        break;
                    case OT_WORD:
                        tcg_gen_ext16u_tl(cpu_T[0], cpu_T[0]);
                        break;
                    default:
                    case OT_WORD | 8:
                        tcg_gen_ext16s_tl(cpu_T[0], cpu_T[0]);
                        break;
                }
                gen_op_mov_reg_T0(d_ot, reg);
            } else {
                gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                if(b & 8) {
                    gen_op_lds_T0_A0(ot + s->base.mem_idx);
                } else {
                    gen_op_ldu_T0_A0(ot + s->base.mem_idx);
                }
                gen_op_mov_reg_T0(d_ot, reg);
            }
        } break;

        case 0x8d: /* lea */
            ot = dflag + OT_WORD;
            modrm = ldub_code(s->base.pc++);
            mod = (modrm >> 6) & 3;
            if(mod == 3) {
                goto illegal_op;
            }
            reg = ((modrm >> 3) & 7) | rex_r;
            /* we must ensure that no segment is added */
            s->override = -1;
            val = s->addseg;
            s->addseg = 0;
            gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
            s->addseg = val;
            gen_op_mov_reg_A0(ot - OT_WORD, reg);
            break;

        case 0xa0:  /* mov EAX, Ov */
        case 0xa1:  //  mov Ov, EAX
        case 0xa2:
        case 0xa3: {
            target_ulong offset_addr;

            if((b & 1) == 0) {
                ot = OT_BYTE;
            } else {
                ot = dflag + OT_WORD;
            }
#ifdef TARGET_X86_64
            if(s->aflag == 2) {
                offset_addr = ldq_code(s->base.pc);
                s->base.pc += 8;
                gen_op_movq_A0_im(offset_addr);
            } else
#endif
            {
                if(s->aflag) {
                    offset_addr = insn_get(s, OT_LONG);
                } else {
                    offset_addr = insn_get(s, OT_WORD);
                }
                gen_op_movl_A0_im(offset_addr);
            }
            gen_add_A0_ds_seg(s);
            if((b & 2) == 0) {
                gen_op_ld_T0_A0(ot + s->base.mem_idx);
                gen_op_mov_reg_T0(ot, R_EAX);
            } else {
                gen_op_mov_TN_reg(ot, 0, R_EAX);
                gen_op_st_T0_A0(ot + s->base.mem_idx);
            }
        } break;
        case 0xd7: /* xlat */
#ifdef TARGET_X86_64
            if(s->aflag == 2) {
                gen_op_movq_A0_reg(R_EBX);
                gen_op_mov_TN_reg(OT_QUAD, 0, R_EAX);
                tcg_gen_andi_tl(cpu_T[0], cpu_T[0], 0xff);
                tcg_gen_add_tl(cpu_A0, cpu_A0, cpu_T[0]);
            } else
#endif
            {
                gen_op_movl_A0_reg(R_EBX);
                gen_op_mov_TN_reg(OT_LONG, 0, R_EAX);
                tcg_gen_andi_tl(cpu_T[0], cpu_T[0], 0xff);
                tcg_gen_add_tl(cpu_A0, cpu_A0, cpu_T[0]);
                if(s->aflag == 0) {
                    gen_op_andl_A0_ffff();
                } else {
                    tcg_gen_andi_tl(cpu_A0, cpu_A0, 0xffffffff);
                }
            }
            gen_add_A0_ds_seg(s);
            gen_op_ldu_T0_A0(OT_BYTE + s->base.mem_idx);
            gen_op_mov_reg_T0(OT_BYTE, R_EAX);
            break;
        case 0xb0 ... 0xb7: /* mov R, Ib */
            val = insn_get(s, OT_BYTE);
            gen_op_movl_T0_im(val);
            gen_op_mov_reg_T0(OT_BYTE, (b & 7) | REX_B(s));
            break;
        case 0xb8 ... 0xbf: /* mov R, Iv */
#ifdef TARGET_X86_64
            if(dflag == 2) {
                uint64_t tmp;
                /* 64 bit case */
                tmp = ldq_code(s->base.pc);
                s->base.pc += 8;
                reg = (b & 7) | REX_B(s);
                gen_movtl_T0_im(tmp);
                gen_op_mov_reg_T0(OT_QUAD, reg);
            } else
#endif
            {
                ot = dflag ? OT_LONG : OT_WORD;
                val = insn_get(s, ot);
                reg = (b & 7) | REX_B(s);
                gen_op_movl_T0_im(val);
                gen_op_mov_reg_T0(ot, reg);
            }
            break;

        case 0x91 ... 0x97: /* xchg R, EAX */
        do_xchg_reg_eax:
            ot = dflag + OT_WORD;
            reg = (b & 7) | REX_B(s);
            rm = R_EAX;
            goto do_xchg_reg;
        case 0x86:  //  xchg Ev, Gv
        case 0x87:
            if((b & 1) == 0) {
                ot = OT_BYTE;
            } else {
                ot = dflag + OT_WORD;
            }
            modrm = ldub_code(s->base.pc++);
            reg = ((modrm >> 3) & 7) | rex_r;
            mod = (modrm >> 6) & 3;
            if(mod == 3) {
                rm = (modrm & 7) | REX_B(s);
            do_xchg_reg:
                gen_op_mov_TN_reg(ot, 0, reg);
                gen_op_mov_TN_reg(ot, 1, rm);
                gen_op_mov_reg_T0(ot, rm);
                gen_op_mov_reg_T1(ot, reg);
            } else {
                gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                gen_op_mov_TN_reg(ot, 0, reg);
                gen_op_ld_T1_A0(ot + s->base.mem_idx);
                gen_op_st_T0_A0(ot + s->base.mem_idx);
                gen_op_mov_reg_T1(ot, reg);
            }
            break;
        case 0xc4: /* les Gv */
            if(CODE64(s)) {
                goto illegal_op;
            }
            op = R_ES;
            goto do_lxx;
        case 0xc5: /* lds Gv */
            if(CODE64(s)) {
                goto illegal_op;
            }
            op = R_DS;
            goto do_lxx;
        case 0x1b2: /* lss Gv */
            op = R_SS;
            goto do_lxx;
        case 0x1b4: /* lfs Gv */
            op = R_FS;
            goto do_lxx;
        case 0x1b5: /* lgs Gv */
            op = R_GS;
        do_lxx:
            ot = dflag ? OT_LONG : OT_WORD;
            modrm = ldub_code(s->base.pc++);
            reg = ((modrm >> 3) & 7) | rex_r;
            mod = (modrm >> 6) & 3;
            if(mod == 3) {
                goto illegal_op;
            }
            gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
            gen_op_ld_T1_A0(ot + s->base.mem_idx);
            gen_add_A0_im(s, 1 << (ot - OT_WORD + 1));
            /* load the segment first to handle exceptions properly */
            gen_op_ldu_T0_A0(OT_WORD + s->base.mem_idx);
            gen_movl_seg_T0(s, op, pc_start - s->cs_base);
            /* then put the data */
            gen_op_mov_reg_T1(ot, reg);
            if(s->base.is_jmp) {
                gen_jmp_im(s->base.pc - s->cs_base);
                gen_eob(s);
            }
            break;

        /************************/
        /* shifts */
        case 0xc0:
        case 0xc1:
            /* shift Ev,Ib */
            shift = 2;
        grp2: {
            if((b & 1) == 0) {
                ot = OT_BYTE;
            } else {
                ot = dflag + OT_WORD;
            }

            modrm = ldub_code(s->base.pc++);
            mod = (modrm >> 6) & 3;
            op = (modrm >> 3) & 7;

            if(mod != 3) {
                if(shift == 2) {
                    s->rip_offset = 1;
                }
                gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                opreg = OR_TMP0;
            } else {
                opreg = (modrm & 7) | REX_B(s);
            }

            /* simpler op */
            if(shift == 0) {
                gen_shift(s, op, ot, opreg, OR_ECX);
            } else {
                if(shift == 2) {
                    shift = ldub_code(s->base.pc++);
                }
                gen_shifti(s, op, ot, opreg, shift);
            }
        } break;
        case 0xd0:
        case 0xd1:
            /* shift Ev,1 */
            shift = 1;
            goto grp2;
        case 0xd2:
        case 0xd3:
            /* shift Ev,cl */
            shift = 0;
            goto grp2;

        case 0x1a4: /* shld imm */
            op = 0;
            shift = 1;
            goto do_shiftd;
        case 0x1a5: /* shld cl */
            op = 0;
            shift = 0;
            goto do_shiftd;
        case 0x1ac: /* shrd imm */
            op = 1;
            shift = 1;
            goto do_shiftd;
        case 0x1ad: /* shrd cl */
            op = 1;
            shift = 0;
        do_shiftd:
            ot = dflag + OT_WORD;
            modrm = ldub_code(s->base.pc++);
            mod = (modrm >> 6) & 3;
            rm = (modrm & 7) | REX_B(s);
            reg = ((modrm >> 3) & 7) | rex_r;
            if(mod != 3) {
                gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                opreg = OR_TMP0;
            } else {
                opreg = rm;
            }
            gen_op_mov_TN_reg(ot, 1, reg);

            if(shift) {
                val = ldub_code(s->base.pc++);
                tcg_gen_movi_tl(cpu_T3, val);
            } else {
                tcg_gen_mov_tl(cpu_T3, cpu_regs[R_ECX]);
            }
            gen_shiftd_rm_T1_T3(s, ot, opreg, op);
            break;

        /************************/
        /* floats */
        case 0xd8 ... 0xdf:
            if(s->flags & (HF_EM_MASK | HF_TS_MASK)) {
                /* if CR0.EM or CR0.TS are set, generate an FPU exception */
                /* XXX: what to do if illegal op ? */
                gen_exception(s, EXCP07_PREX, pc_start - s->cs_base);
                break;
            }
            modrm = ldub_code(s->base.pc++);
            mod = (modrm >> 6) & 3;
            rm = modrm & 7;
            op = ((b & 7) << 3) | ((modrm >> 3) & 7);
            if(mod != 3) {
                /* memory op */
                gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                switch(op) {
                    case 0x00 ... 0x07: /* fxxxs */
                    case 0x10 ... 0x17: /* fixxxl */
                    case 0x20 ... 0x27: /* fxxxl */
                    case 0x30 ... 0x37: /* fixxx */
                    {
                        int op1;
                        op1 = op & 7;

                        switch(op >> 4) {
                            case 0:
                                gen_op_ld_T0_A0(OT_LONG + s->base.mem_idx);
                                tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_T[0]);
                                gen_helper_flds_FT0(cpu_tmp2_i32);
                                break;
                            case 1:
                                gen_op_ld_T0_A0(OT_LONG + s->base.mem_idx);
                                tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_T[0]);
                                gen_helper_fildl_FT0(cpu_tmp2_i32);
                                break;
                            case 2:
                                tcg_gen_qemu_ld64(cpu_tmp1_i64, cpu_A0, (s->base.mem_idx >> 2) - 1);
                                gen_helper_fldl_FT0(cpu_tmp1_i64);
                                break;
                            case 3:
                            default:
                                gen_op_lds_T0_A0(OT_WORD + s->base.mem_idx);
                                tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_T[0]);
                                gen_helper_fildl_FT0(cpu_tmp2_i32);
                                break;
                        }

                        gen_helper_fp_arith_ST0_FT0(op1);
                        if(op1 == 3) {
                            /* fcomp needs pop */
                            gen_helper_fpop();
                        }
                    } break;
                    case 0x08:          /* flds */
                    case 0x0a:          /* fsts */
                    case 0x0b:          /* fstps */
                    case 0x18 ... 0x1b: /* fildl, fisttpl, fistl, fistpl */
                    case 0x28 ... 0x2b: /* fldl, fisttpll, fstl, fstpl */
                    case 0x38 ... 0x3b: /* filds, fisttps, fists, fistps */
                        switch(op & 7) {
                            case 0:
                                switch(op >> 4) {
                                    case 0:
                                        gen_op_ld_T0_A0(OT_LONG + s->base.mem_idx);
                                        tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_T[0]);
                                        gen_helper_flds_ST0(cpu_tmp2_i32);
                                        break;
                                    case 1:
                                        gen_op_ld_T0_A0(OT_LONG + s->base.mem_idx);
                                        tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_T[0]);
                                        gen_helper_fildl_ST0(cpu_tmp2_i32);
                                        break;
                                    case 2:
                                        tcg_gen_qemu_ld64(cpu_tmp1_i64, cpu_A0, (s->base.mem_idx >> 2) - 1);
                                        gen_helper_fldl_ST0(cpu_tmp1_i64);
                                        break;
                                    case 3:
                                    default:
                                        gen_op_lds_T0_A0(OT_WORD + s->base.mem_idx);
                                        tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_T[0]);
                                        gen_helper_fildl_ST0(cpu_tmp2_i32);
                                        break;
                                }
                                break;
                            case 1:
                                /* XXX: the corresponding CPUID bit must be tested ! */
                                switch(op >> 4) {
                                    case 1:
                                        gen_helper_fisttl_ST0(cpu_tmp2_i32);
                                        tcg_gen_extu_i32_tl(cpu_T[0], cpu_tmp2_i32);
                                        gen_op_st_T0_A0(OT_LONG + s->base.mem_idx);
                                        break;
                                    case 2:
                                        gen_helper_fisttll_ST0(cpu_tmp1_i64);
                                        tcg_gen_qemu_st64(cpu_tmp1_i64, cpu_A0, (s->base.mem_idx >> 2) - 1);
                                        break;
                                    case 3:
                                    default:
                                        gen_helper_fistt_ST0(cpu_tmp2_i32);
                                        tcg_gen_extu_i32_tl(cpu_T[0], cpu_tmp2_i32);
                                        gen_op_st_T0_A0(OT_WORD + s->base.mem_idx);
                                        break;
                                }
                                gen_helper_fpop();
                                break;
                            default:
                                switch(op >> 4) {
                                    case 0:
                                        gen_helper_fsts_ST0(cpu_tmp2_i32);
                                        tcg_gen_extu_i32_tl(cpu_T[0], cpu_tmp2_i32);
                                        gen_op_st_T0_A0(OT_LONG + s->base.mem_idx);
                                        break;
                                    case 1:
                                        gen_helper_fistl_ST0(cpu_tmp2_i32);
                                        tcg_gen_extu_i32_tl(cpu_T[0], cpu_tmp2_i32);
                                        gen_op_st_T0_A0(OT_LONG + s->base.mem_idx);
                                        break;
                                    case 2:
                                        gen_helper_fstl_ST0(cpu_tmp1_i64);
                                        tcg_gen_qemu_st64(cpu_tmp1_i64, cpu_A0, (s->base.mem_idx >> 2) - 1);
                                        break;
                                    case 3:
                                    default:
                                        gen_helper_fist_ST0(cpu_tmp2_i32);
                                        tcg_gen_extu_i32_tl(cpu_T[0], cpu_tmp2_i32);
                                        gen_op_st_T0_A0(OT_WORD + s->base.mem_idx);
                                        break;
                                }
                                if((op & 7) == 3) {
                                    gen_helper_fpop();
                                }
                                break;
                        }
                        break;
                    case 0x0c: /* fldenv mem */
                        if(s->cc_op != CC_OP_DYNAMIC) {
                            gen_op_set_cc_op(s->cc_op);
                        }
                        gen_jmp_im(pc_start - s->cs_base);
                        gen_helper_fldenv(cpu_A0, tcg_const_i32(s->dflag));
                        break;
                    case 0x0d: /* fldcw mem */
                        gen_op_ld_T0_A0(OT_WORD + s->base.mem_idx);
                        tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_T[0]);
                        gen_helper_fldcw(cpu_tmp2_i32);
                        break;
                    case 0x0e: /* fnstenv mem */
                        if(s->cc_op != CC_OP_DYNAMIC) {
                            gen_op_set_cc_op(s->cc_op);
                        }
                        gen_jmp_im(pc_start - s->cs_base);
                        gen_helper_fstenv(cpu_A0, tcg_const_i32(s->dflag));
                        break;
                    case 0x0f: /* fnstcw mem */
                        gen_helper_fnstcw(cpu_tmp2_i32);
                        tcg_gen_extu_i32_tl(cpu_T[0], cpu_tmp2_i32);
                        gen_op_st_T0_A0(OT_WORD + s->base.mem_idx);
                        break;
                    case 0x1d: /* fldt mem */
                        if(s->cc_op != CC_OP_DYNAMIC) {
                            gen_op_set_cc_op(s->cc_op);
                        }
                        gen_jmp_im(pc_start - s->cs_base);
                        gen_helper_fldt_ST0(cpu_A0);
                        break;
                    case 0x1f: /* fstpt mem */
                        if(s->cc_op != CC_OP_DYNAMIC) {
                            gen_op_set_cc_op(s->cc_op);
                        }
                        gen_jmp_im(pc_start - s->cs_base);
                        gen_helper_fstt_ST0(cpu_A0);
                        gen_helper_fpop();
                        break;
                    case 0x2c: /* frstor mem */
                        if(s->cc_op != CC_OP_DYNAMIC) {
                            gen_op_set_cc_op(s->cc_op);
                        }
                        gen_jmp_im(pc_start - s->cs_base);
                        gen_helper_frstor(cpu_A0, tcg_const_i32(s->dflag));
                        break;
                    case 0x2e: /* fnsave mem */
                        if(s->cc_op != CC_OP_DYNAMIC) {
                            gen_op_set_cc_op(s->cc_op);
                        }
                        gen_jmp_im(pc_start - s->cs_base);
                        gen_helper_fsave(cpu_A0, tcg_const_i32(s->dflag));
                        break;
                    case 0x2f: /* fnstsw mem */
                        gen_helper_fnstsw(cpu_tmp2_i32);
                        tcg_gen_extu_i32_tl(cpu_T[0], cpu_tmp2_i32);
                        gen_op_st_T0_A0(OT_WORD + s->base.mem_idx);
                        break;
                    case 0x3c: /* fbld */
                        if(s->cc_op != CC_OP_DYNAMIC) {
                            gen_op_set_cc_op(s->cc_op);
                        }
                        gen_jmp_im(pc_start - s->cs_base);
                        gen_helper_fbld_ST0(cpu_A0);
                        break;
                    case 0x3e: /* fbstp */
                        if(s->cc_op != CC_OP_DYNAMIC) {
                            gen_op_set_cc_op(s->cc_op);
                        }
                        gen_jmp_im(pc_start - s->cs_base);
                        gen_helper_fbst_ST0(cpu_A0);
                        gen_helper_fpop();
                        break;
                    case 0x3d: /* fildll */
                        tcg_gen_qemu_ld64(cpu_tmp1_i64, cpu_A0, (s->base.mem_idx >> 2) - 1);
                        gen_helper_fildll_ST0(cpu_tmp1_i64);
                        break;
                    case 0x3f: /* fistpll */
                        gen_helper_fistll_ST0(cpu_tmp1_i64);
                        tcg_gen_qemu_st64(cpu_tmp1_i64, cpu_A0, (s->base.mem_idx >> 2) - 1);
                        gen_helper_fpop();
                        break;
                    default:
                        goto illegal_op;
                }
            } else {
                /* register float ops */
                opreg = rm;

                switch(op) {
                    case 0x08: /* fld sti */
                        gen_helper_fpush();
                        gen_helper_fmov_ST0_STN(tcg_const_i32((opreg + 1) & 7));
                        break;
                    case 0x09: /* fxchg sti */
                    case 0x29: /* fxchg4 sti, undocumented op */
                    case 0x39: /* fxchg7 sti, undocumented op */
                        gen_helper_fxchg_ST0_STN(tcg_const_i32(opreg));
                        break;
                    case 0x0a: /* grp d9/2 */
                        switch(rm) {
                            case 0: /* fnop */
                                /* check exceptions (FreeBSD FPU probe) */
                                if(s->cc_op != CC_OP_DYNAMIC) {
                                    gen_op_set_cc_op(s->cc_op);
                                }
                                gen_jmp_im(pc_start - s->cs_base);
                                gen_helper_fwait();
                                break;
                            default:
                                goto illegal_op;
                        }
                        break;
                    case 0x0c: /* grp d9/4 */
                        switch(rm) {
                            case 0: /* fchs */
                                gen_helper_fchs_ST0();
                                break;
                            case 1: /* fabs */
                                gen_helper_fabs_ST0();
                                break;
                            case 4: /* ftst */
                                gen_helper_fldz_FT0();
                                gen_helper_fcom_ST0_FT0();
                                break;
                            case 5: /* fxam */
                                gen_helper_fxam_ST0();
                                break;
                            default:
                                goto illegal_op;
                        }
                        break;
                    case 0x0d: /* grp d9/5 */
                    {
                        switch(rm) {
                            case 0:
                                gen_helper_fpush();
                                gen_helper_fld1_ST0();
                                break;
                            case 1:
                                gen_helper_fpush();
                                gen_helper_fldl2t_ST0();
                                break;
                            case 2:
                                gen_helper_fpush();
                                gen_helper_fldl2e_ST0();
                                break;
                            case 3:
                                gen_helper_fpush();
                                gen_helper_fldpi_ST0();
                                break;
                            case 4:
                                gen_helper_fpush();
                                gen_helper_fldlg2_ST0();
                                break;
                            case 5:
                                gen_helper_fpush();
                                gen_helper_fldln2_ST0();
                                break;
                            case 6:
                                gen_helper_fpush();
                                gen_helper_fldz_ST0();
                                break;
                            default:
                                goto illegal_op;
                        }
                    } break;
                    case 0x0e: /* grp d9/6 */
                        switch(rm) {
                            case 0: /* f2xm1 */
                                gen_helper_f2xm1();
                                break;
                            case 1: /* fyl2x */
                                gen_helper_fyl2x();
                                break;
                            case 2: /* fptan */
                                gen_helper_fptan();
                                break;
                            case 3: /* fpatan */
                                gen_helper_fpatan();
                                break;
                            case 4: /* fxtract */
                                gen_helper_fxtract();
                                break;
                            case 5: /* fprem1 */
                                gen_helper_fprem1();
                                break;
                            case 6: /* fdecstp */
                                gen_helper_fdecstp();
                                break;
                            default:
                            case 7: /* fincstp */
                                gen_helper_fincstp();
                                break;
                        }
                        break;
                    case 0x0f: /* grp d9/7 */
                        switch(rm) {
                            case 0: /* fprem */
                                gen_helper_fprem();
                                break;
                            case 1: /* fyl2xp1 */
                                gen_helper_fyl2xp1();
                                break;
                            case 2: /* fsqrt */
                                gen_helper_fsqrt();
                                break;
                            case 3: /* fsincos */
                                gen_helper_fsincos();
                                break;
                            case 5: /* fscale */
                                gen_helper_fscale();
                                break;
                            case 4: /* frndint */
                                gen_helper_frndint();
                                break;
                            case 6: /* fsin */
                                gen_helper_fsin();
                                break;
                            default:
                            case 7: /* fcos */
                                gen_helper_fcos();
                                break;
                        }
                        break;
                    case 0x00:  //  fxxx st, sti
                    case 0x01:
                    case 0x04 ... 0x07:
                    case 0x20:  //  fxxx sti, st
                    case 0x21:
                    case 0x24 ... 0x27:
                    case 0x30:  //  fxxxp sti, st
                    case 0x31:
                    case 0x34 ... 0x37: {
                        int op1;

                        op1 = op & 7;
                        if(op >= 0x20) {
                            gen_helper_fp_arith_STN_ST0(op1, opreg);
                            if(op >= 0x30) {
                                gen_helper_fpop();
                            }
                        } else {
                            gen_helper_fmov_FT0_STN(tcg_const_i32(opreg));
                            gen_helper_fp_arith_ST0_FT0(op1);
                        }
                    } break;
                    case 0x02: /* fcom */
                    case 0x22: /* fcom2, undocumented op */
                        gen_helper_fmov_FT0_STN(tcg_const_i32(opreg));
                        gen_helper_fcom_ST0_FT0();
                        break;
                    case 0x03: /* fcomp */
                    case 0x23: /* fcomp3, undocumented op */
                    case 0x32: /* fcomp5, undocumented op */
                        gen_helper_fmov_FT0_STN(tcg_const_i32(opreg));
                        gen_helper_fcom_ST0_FT0();
                        gen_helper_fpop();
                        break;
                    case 0x15: /* da/5 */
                        switch(rm) {
                            case 1: /* fucompp */
                                gen_helper_fmov_FT0_STN(tcg_const_i32(1));
                                gen_helper_fucom_ST0_FT0();
                                gen_helper_fpop();
                                gen_helper_fpop();
                                break;
                            default:
                                goto illegal_op;
                        }
                        break;
                    case 0x1c:
                        switch(rm) {
                            case 0: /* feni (287 only, just do nop here) */
                                break;
                            case 1: /* fdisi (287 only, just do nop here) */
                                break;
                            case 2: /* fclex */
                                gen_helper_fclex();
                                break;
                            case 3: /* fninit */
                                gen_helper_fninit();
                                break;
                            case 4: /* fsetpm (287 only, just do nop here) */
                                break;
                            default:
                                goto illegal_op;
                        }
                        break;
                    case 0x1d: /* fucomi */
                        if(s->cc_op != CC_OP_DYNAMIC) {
                            gen_op_set_cc_op(s->cc_op);
                        }
                        gen_helper_fmov_FT0_STN(tcg_const_i32(opreg));
                        gen_helper_fucomi_ST0_FT0();
                        s->cc_op = CC_OP_EFLAGS;
                        break;
                    case 0x1e: /* fcomi */
                        if(s->cc_op != CC_OP_DYNAMIC) {
                            gen_op_set_cc_op(s->cc_op);
                        }
                        gen_helper_fmov_FT0_STN(tcg_const_i32(opreg));
                        gen_helper_fcomi_ST0_FT0();
                        s->cc_op = CC_OP_EFLAGS;
                        break;
                    case 0x28: /* ffree sti */
                        gen_helper_ffree_STN(tcg_const_i32(opreg));
                        break;
                    case 0x2a: /* fst sti */
                        gen_helper_fmov_STN_ST0(tcg_const_i32(opreg));
                        break;
                    case 0x2b: /* fstp sti */
                    case 0x0b: /* fstp1 sti, undocumented op */
                    case 0x3a: /* fstp8 sti, undocumented op */
                    case 0x3b: /* fstp9 sti, undocumented op */
                        gen_helper_fmov_STN_ST0(tcg_const_i32(opreg));
                        gen_helper_fpop();
                        break;
                    case 0x2c: /* fucom st(i) */
                        gen_helper_fmov_FT0_STN(tcg_const_i32(opreg));
                        gen_helper_fucom_ST0_FT0();
                        break;
                    case 0x2d: /* fucomp st(i) */
                        gen_helper_fmov_FT0_STN(tcg_const_i32(opreg));
                        gen_helper_fucom_ST0_FT0();
                        gen_helper_fpop();
                        break;
                    case 0x33: /* de/3 */
                        switch(rm) {
                            case 1: /* fcompp */
                                gen_helper_fmov_FT0_STN(tcg_const_i32(1));
                                gen_helper_fcom_ST0_FT0();
                                gen_helper_fpop();
                                gen_helper_fpop();
                                break;
                            default:
                                goto illegal_op;
                        }
                        break;
                    case 0x38: /* ffreep sti, undocumented op */
                        gen_helper_ffree_STN(tcg_const_i32(opreg));
                        gen_helper_fpop();
                        break;
                    case 0x3c: /* df/4 */
                        switch(rm) {
                            case 0:
                                gen_helper_fnstsw(cpu_tmp2_i32);
                                tcg_gen_extu_i32_tl(cpu_T[0], cpu_tmp2_i32);
                                gen_op_mov_reg_T0(OT_WORD, R_EAX);
                                break;
                            default:
                                goto illegal_op;
                        }
                        break;
                    case 0x3d: /* fucomip */
                        if(s->cc_op != CC_OP_DYNAMIC) {
                            gen_op_set_cc_op(s->cc_op);
                        }
                        gen_helper_fmov_FT0_STN(tcg_const_i32(opreg));
                        gen_helper_fucomi_ST0_FT0();
                        gen_helper_fpop();
                        s->cc_op = CC_OP_EFLAGS;
                        break;
                    case 0x3e: /* fcomip */
                        if(s->cc_op != CC_OP_DYNAMIC) {
                            gen_op_set_cc_op(s->cc_op);
                        }
                        gen_helper_fmov_FT0_STN(tcg_const_i32(opreg));
                        gen_helper_fcomi_ST0_FT0();
                        gen_helper_fpop();
                        s->cc_op = CC_OP_EFLAGS;
                        break;
                    case 0x10 ... 0x13: /* fcmovxx */
                    case 0x18 ... 0x1b: {
                        int op1, l1;
                        static const uint8_t fcmov_cc[8] = {
                            (JCC_B << 1),
                            (JCC_Z << 1),
                            (JCC_BE << 1),
                            (JCC_P << 1),
                        };
                        op1 = fcmov_cc[op & 3] | (((op >> 3) & 1) ^ 1);
                        l1 = gen_new_label();
                        gen_jcc1(s, s->cc_op, op1, l1);
                        gen_helper_fmov_ST0_STN(tcg_const_i32(opreg));
                        gen_set_label(l1);
                    } break;
                    default:
                        goto illegal_op;
                }
            }
            break;
            /************************/
            /* string ops */

        case 0xa4: /* movsS */
        case 0xa5:
            if((b & 1) == 0) {
                ot = OT_BYTE;
            } else {
                ot = dflag + OT_WORD;
            }

            if(prefixes & (PREFIX_REPZ | PREFIX_REPNZ)) {
                gen_repz_movs(s, ot, pc_start - s->cs_base, s->base.pc - s->cs_base);
            } else {
                gen_movs(s, ot);
            }
            break;

        case 0xaa: /* stosS */
        case 0xab:
            if((b & 1) == 0) {
                ot = OT_BYTE;
            } else {
                ot = dflag + OT_WORD;
            }

            if(prefixes & (PREFIX_REPZ | PREFIX_REPNZ)) {
                gen_repz_stos(s, ot, pc_start - s->cs_base, s->base.pc - s->cs_base);
            } else {
                gen_stos(s, ot);
            }
            break;
        case 0xac: /* lodsS */
        case 0xad:
            if((b & 1) == 0) {
                ot = OT_BYTE;
            } else {
                ot = dflag + OT_WORD;
            }
            if(prefixes & (PREFIX_REPZ | PREFIX_REPNZ)) {
                gen_repz_lods(s, ot, pc_start - s->cs_base, s->base.pc - s->cs_base);
            } else {
                gen_lods(s, ot);
            }
            break;
        case 0xae: /* scasS */
        case 0xaf:
            if((b & 1) == 0) {
                ot = OT_BYTE;
            } else {
                ot = dflag + OT_WORD;
            }
            if(prefixes & PREFIX_REPNZ) {
                gen_repz_scas(s, ot, pc_start - s->cs_base, s->base.pc - s->cs_base, 1);
            } else if(prefixes & PREFIX_REPZ) {
                gen_repz_scas(s, ot, pc_start - s->cs_base, s->base.pc - s->cs_base, 0);
            } else {
                gen_scas(s, ot);
                s->cc_op = CC_OP_SUBB + ot;
            }
            break;

        case 0xa6: /* cmpsS */
        case 0xa7:
            if((b & 1) == 0) {
                ot = OT_BYTE;
            } else {
                ot = dflag + OT_WORD;
            }
            if(prefixes & PREFIX_REPNZ) {
                gen_repz_cmps(s, ot, pc_start - s->cs_base, s->base.pc - s->cs_base, 1);
            } else if(prefixes & PREFIX_REPZ) {
                gen_repz_cmps(s, ot, pc_start - s->cs_base, s->base.pc - s->cs_base, 0);
            } else {
                gen_cmps(s, ot);
                s->cc_op = CC_OP_SUBB + ot;
            }
            break;
        case 0x6c: /* insS */
        case 0x6d:
            if((b & 1) == 0) {
                ot = OT_BYTE;
            } else {
                ot = dflag ? OT_LONG : OT_WORD;
            }
            gen_op_mov_TN_reg(OT_WORD, 0, R_EDX);
            gen_op_andl_T0_ffff();
            gen_check_io(s, ot, pc_start - s->cs_base, SVM_IOIO_TYPE_MASK | svm_is_rep(prefixes) | 4);
            if(prefixes & (PREFIX_REPZ | PREFIX_REPNZ)) {
                gen_repz_ins(s, ot, pc_start - s->cs_base, s->base.pc - s->cs_base);
            } else {
                gen_ins(s, ot);
            }
            break;
        case 0x6e: /* outsS */
        case 0x6f:
            if((b & 1) == 0) {
                ot = OT_BYTE;
            } else {
                ot = dflag ? OT_LONG : OT_WORD;
            }
            gen_op_mov_TN_reg(OT_WORD, 0, R_EDX);
            gen_op_andl_T0_ffff();
            gen_check_io(s, ot, pc_start - s->cs_base, svm_is_rep(prefixes) | 4);
            if(prefixes & (PREFIX_REPZ | PREFIX_REPNZ)) {
                gen_repz_outs(s, ot, pc_start - s->cs_base, s->base.pc - s->cs_base);
            } else {
                gen_outs(s, ot);
            }
            break;

            /************************/
            /* port I/O */

        case 0xe4:
        case 0xe5:
            if((b & 1) == 0) {
                ot = OT_BYTE;
            } else {
                ot = dflag ? OT_LONG : OT_WORD;
            }
            val = ldub_code(s->base.pc++);
            gen_op_movl_T0_im(val);
            gen_check_io(s, ot, pc_start - s->cs_base, SVM_IOIO_TYPE_MASK | svm_is_rep(prefixes));
            tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_T[0]);
            gen_helper_in_func(ot, cpu_T[1], cpu_tmp2_i32);
            gen_op_mov_reg_T1(ot, R_EAX);
            break;
        case 0xe6:
        case 0xe7:
            if((b & 1) == 0) {
                ot = OT_BYTE;
            } else {
                ot = dflag ? OT_LONG : OT_WORD;
            }
            val = ldub_code(s->base.pc++);
            gen_op_movl_T0_im(val);
            gen_check_io(s, ot, pc_start - s->cs_base, svm_is_rep(prefixes));
            gen_op_mov_TN_reg(ot, 1, R_EAX);

            tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_T[0]);
            tcg_gen_trunc_tl_i32(cpu_tmp3_i32, cpu_T[1]);
            gen_helper_out_func(ot, cpu_tmp2_i32, cpu_tmp3_i32);
            break;
        case 0xec:
        case 0xed:
            if((b & 1) == 0) {
                ot = OT_BYTE;
            } else {
                ot = dflag ? OT_LONG : OT_WORD;
            }
            gen_op_mov_TN_reg(OT_WORD, 0, R_EDX);
            gen_op_andl_T0_ffff();
            gen_check_io(s, ot, pc_start - s->cs_base, SVM_IOIO_TYPE_MASK | svm_is_rep(prefixes));
            tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_T[0]);
            gen_helper_in_func(ot, cpu_T[1], cpu_tmp2_i32);
            gen_op_mov_reg_T1(ot, R_EAX);
            break;
        case 0xee:
        case 0xef:
            if((b & 1) == 0) {
                ot = OT_BYTE;
            } else {
                ot = dflag ? OT_LONG : OT_WORD;
            }
            gen_op_mov_TN_reg(OT_WORD, 0, R_EDX);
            gen_op_andl_T0_ffff();
            gen_check_io(s, ot, pc_start - s->cs_base, svm_is_rep(prefixes));
            gen_op_mov_TN_reg(ot, 1, R_EAX);

            tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_T[0]);
            tcg_gen_trunc_tl_i32(cpu_tmp3_i32, cpu_T[1]);
            gen_helper_out_func(ot, cpu_tmp2_i32, cpu_tmp3_i32);
            break;

        /************************/
        /* control */
        case 0xc2: /* ret im */
            val = ldsw_code(s->base.pc);
            s->base.pc += 2;
            gen_pop_T0(s);
            if(CODE64(s) && s->dflag) {
                s->dflag = 2;
            }
            gen_stack_update(s, val + (2 << s->dflag));
            if(s->dflag == 0) {
                gen_op_andl_T0_ffff();
            }
            gen_op_jmp_T0();
            gen_eob(s);
            break;
        case 0xc3: /* ret */
            gen_pop_T0(s);
            gen_pop_update(s);
            if(s->dflag == 0) {
                gen_op_andl_T0_ffff();
            }
            gen_op_jmp_T0();
            gen_eob(s);
            break;
        case 0xca: /* lret im */
            val = ldsw_code(s->base.pc);
            s->base.pc += 2;
        do_lret:
            if(s->pe && !s->vm86) {
                if(s->cc_op != CC_OP_DYNAMIC) {
                    gen_op_set_cc_op(s->cc_op);
                }
                gen_jmp_im(pc_start - s->cs_base);
                gen_helper_lret_protected(tcg_const_i32(s->dflag), tcg_const_i32(val));
            } else {
                gen_stack_A0(s);
                /* pop offset */
                gen_op_ld_T0_A0(1 + s->dflag + s->base.mem_idx);
                if(s->dflag == 0) {
                    gen_op_andl_T0_ffff();
                }
                /* NOTE: keeping EIP updated is not a problem in case of
                   exception */
                gen_op_jmp_T0();
                /* pop selector */
                gen_op_addl_A0_im(2 << s->dflag);
                gen_op_ld_T0_A0(1 + s->dflag + s->base.mem_idx);
                gen_op_movl_seg_T0_vm(R_CS);
                /* add stack offset */
                gen_stack_update(s, val + (4 << s->dflag));
            }
            gen_eob(s);
            break;
        case 0xcb: /* lret */
            val = 0;
            goto do_lret;
        case 0xcf: /* iret */
            gen_svm_check_intercept(s, pc_start, SVM_EXIT_IRET);
            if(!s->pe) {
                /* real mode */
                gen_helper_iret_real(tcg_const_i32(s->dflag));
                s->cc_op = CC_OP_EFLAGS;
            } else if(s->vm86) {
                if(s->iopl != 3) {
                    gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
                } else {
                    gen_helper_iret_real(tcg_const_i32(s->dflag));
                    s->cc_op = CC_OP_EFLAGS;
                }
            } else {
                if(s->cc_op != CC_OP_DYNAMIC) {
                    gen_op_set_cc_op(s->cc_op);
                }
                gen_jmp_im(pc_start - s->cs_base);
                gen_helper_iret_protected(tcg_const_i32(s->dflag), tcg_const_i32(s->base.pc - s->cs_base));
                s->cc_op = CC_OP_EFLAGS;
            }
            gen_eob(s);
            break;
        case 0xe8: /* call im */
        {
            if(dflag) {
                tval = (int32_t)insn_get(s, OT_LONG);
            } else {
                tval = (int16_t)insn_get(s, OT_WORD);
            }
            next_eip = s->base.pc - s->cs_base;
            tval += next_eip;
            if(s->dflag == 0) {
                tval &= 0xffff;
            } else if(!CODE64(s)) {
                tval &= 0xffffffff;
            }
            gen_movtl_T0_im(next_eip);
            gen_push_T0(s);
            gen_jmp(s, tval);
        } break;
        case 0x9a: /* lcall im */
        {
            unsigned int selector, offset;

            if(CODE64(s)) {
                goto illegal_op;
            }
            ot = dflag ? OT_LONG : OT_WORD;
            offset = insn_get(s, ot);
            selector = insn_get(s, OT_WORD);

            gen_op_movl_T0_im(selector);
            gen_op_movl_T1_imu(offset);
        }
            goto do_lcall;
        case 0xe9: /* jmp im */
            if(dflag) {
                tval = (int32_t)insn_get(s, OT_LONG);
            } else {
                tval = (int16_t)insn_get(s, OT_WORD);
            }
            tval += s->base.pc - s->cs_base;
            if(s->dflag == 0) {
                tval &= 0xffff;
            } else if(!CODE64(s)) {
                tval &= 0xffffffff;
            }
            gen_jmp(s, tval);
            break;
        case 0xea: /* ljmp im */
        {
            unsigned int selector, offset;

            if(CODE64(s)) {
                goto illegal_op;
            }
            ot = dflag ? OT_LONG : OT_WORD;
            offset = insn_get(s, ot);
            selector = insn_get(s, OT_WORD);

            gen_op_movl_T0_im(selector);
            gen_op_movl_T1_imu(offset);
        }
            goto do_ljmp;
        case 0xeb: /* jmp Jb */
            tval = (int8_t)insn_get(s, OT_BYTE);
            tval += s->base.pc - s->cs_base;
            if(s->dflag == 0) {
                tval &= 0xffff;
            }
            gen_jmp(s, tval);
            break;
        case 0x70 ... 0x7f: /* jcc Jb */
            tval = (int8_t)insn_get(s, OT_BYTE);
            goto do_jcc;
        case 0x180 ... 0x18f: /* jcc Jv */
            if(dflag) {
                tval = (int32_t)insn_get(s, OT_LONG);
            } else {
                tval = (int16_t)insn_get(s, OT_WORD);
            }
        do_jcc:
            next_eip = s->base.pc - s->cs_base;
            tval += next_eip;
            if(s->dflag == 0) {
                tval &= 0xffff;
            }
            gen_jcc(s, b, tval, next_eip);
            break;

        case 0x190 ... 0x19f: /* setcc Gv */
            modrm = ldub_code(s->base.pc++);
            gen_setcc(s, b);
            gen_ldst_modrm(s, modrm, OT_BYTE, OR_TMP0, 1);
            break;
        case 0x140 ... 0x14f: /* cmov Gv, Ev */
        {
            int l1;
            TCGv t0;

            ot = dflag + OT_WORD;
            modrm = ldub_code(s->base.pc++);
            reg = ((modrm >> 3) & 7) | rex_r;
            mod = (modrm >> 6) & 3;
            t0 = tcg_temp_local_new();
            if(mod != 3) {
                gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                gen_op_ld_v(ot + s->base.mem_idx, t0, cpu_A0);
            } else {
                rm = (modrm & 7) | REX_B(s);
                gen_op_mov_v_reg(ot, t0, rm);
            }
#ifdef TARGET_X86_64
            if(ot == OT_LONG) {
                /* XXX: specific Intel behaviour ? */
                l1 = gen_new_label();
                gen_jcc1(s, s->cc_op, b ^ 1, l1);
                tcg_gen_mov_tl(cpu_regs[reg], t0);
                gen_set_label(l1);
                tcg_gen_ext32u_tl(cpu_regs[reg], cpu_regs[reg]);
            } else
#endif
            {
                l1 = gen_new_label();
                gen_jcc1(s, s->cc_op, b ^ 1, l1);
                gen_op_mov_reg_v(ot, reg, t0);
                gen_set_label(l1);
            }
            tcg_temp_free(t0);
        } break;

        /************************/
        /* flags */
        case 0x9c: /* pushf */
            gen_svm_check_intercept(s, pc_start, SVM_EXIT_PUSHF);
            if(s->vm86 && s->iopl != 3) {
                gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
            } else {
                if(s->cc_op != CC_OP_DYNAMIC) {
                    gen_op_set_cc_op(s->cc_op);
                }
                gen_helper_read_eflags(cpu_T[0]);
                gen_push_T0(s);
            }
            break;
        case 0x9d: /* popf */
            gen_svm_check_intercept(s, pc_start, SVM_EXIT_POPF);
            if(s->vm86 && s->iopl != 3) {
                gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
            } else {
                gen_pop_T0(s);
                if(s->cpl == 0) {
                    if(s->dflag) {
                        gen_helper_write_eflags(cpu_T[0],
                                                tcg_const_i32((TF_MASK | AC_MASK | ID_MASK | NT_MASK | IF_MASK | IOPL_MASK)));
                    } else {
                        gen_helper_write_eflags(
                            cpu_T[0], tcg_const_i32((TF_MASK | AC_MASK | ID_MASK | NT_MASK | IF_MASK | IOPL_MASK) & 0xffff));
                    }
                } else {
                    if(s->cpl <= s->iopl) {
                        if(s->dflag) {
                            gen_helper_write_eflags(cpu_T[0], tcg_const_i32((TF_MASK | AC_MASK | ID_MASK | NT_MASK | IF_MASK)));
                        } else {
                            gen_helper_write_eflags(cpu_T[0],
                                                    tcg_const_i32((TF_MASK | AC_MASK | ID_MASK | NT_MASK | IF_MASK) & 0xffff));
                        }
                    } else {
                        if(s->dflag) {
                            gen_helper_write_eflags(cpu_T[0], tcg_const_i32((TF_MASK | AC_MASK | ID_MASK | NT_MASK)));
                        } else {
                            gen_helper_write_eflags(cpu_T[0], tcg_const_i32((TF_MASK | AC_MASK | ID_MASK | NT_MASK) & 0xffff));
                        }
                    }
                }
                gen_pop_update(s);
                s->cc_op = CC_OP_EFLAGS;
                /* abort translation because TF flag may change */
                gen_jmp_im(s->base.pc - s->cs_base);
                gen_eob(s);
            }
            break;
        case 0x9e: /* sahf */
            if(CODE64(s) && !(s->cpuid_ext3_features & CPUID_EXT3_LAHF_LM)) {
                goto illegal_op;
            }
            gen_op_mov_TN_reg(OT_BYTE, 0, R_AH);
            if(s->cc_op != CC_OP_DYNAMIC) {
                gen_op_set_cc_op(s->cc_op);
            }
            gen_compute_eflags(cpu_cc_src);
            tcg_gen_andi_tl(cpu_cc_src, cpu_cc_src, CC_O);
            tcg_gen_andi_tl(cpu_T[0], cpu_T[0], CC_S | CC_Z | CC_A | CC_P | CC_C);
            tcg_gen_or_tl(cpu_cc_src, cpu_cc_src, cpu_T[0]);
            s->cc_op = CC_OP_EFLAGS;
            break;
        case 0x9f: /* lahf */
            if(CODE64(s) && !(s->cpuid_ext3_features & CPUID_EXT3_LAHF_LM)) {
                goto illegal_op;
            }
            if(s->cc_op != CC_OP_DYNAMIC) {
                gen_op_set_cc_op(s->cc_op);
            }
            gen_compute_eflags(cpu_T[0]);
            /* Note: gen_compute_eflags() only gives the condition codes */
            tcg_gen_ori_tl(cpu_T[0], cpu_T[0], 0x02);
            gen_op_mov_reg_T0(OT_BYTE, R_AH);
            break;
        case 0xf5: /* cmc */
            if(s->cc_op != CC_OP_DYNAMIC) {
                gen_op_set_cc_op(s->cc_op);
            }
            gen_compute_eflags(cpu_cc_src);
            tcg_gen_xori_tl(cpu_cc_src, cpu_cc_src, CC_C);
            s->cc_op = CC_OP_EFLAGS;
            break;
        case 0xf8: /* clc */
            if(s->cc_op != CC_OP_DYNAMIC) {
                gen_op_set_cc_op(s->cc_op);
            }
            gen_compute_eflags(cpu_cc_src);
            tcg_gen_andi_tl(cpu_cc_src, cpu_cc_src, ~CC_C);
            s->cc_op = CC_OP_EFLAGS;
            break;
        case 0xf9: /* stc */
            if(s->cc_op != CC_OP_DYNAMIC) {
                gen_op_set_cc_op(s->cc_op);
            }
            gen_compute_eflags(cpu_cc_src);
            tcg_gen_ori_tl(cpu_cc_src, cpu_cc_src, CC_C);
            s->cc_op = CC_OP_EFLAGS;
            break;
        case 0xfc: /* cld */
            tcg_gen_movi_i32(cpu_tmp2_i32, 1);
            tcg_gen_st_i32(cpu_tmp2_i32, cpu_env, offsetof(CPUState, df));
            break;
        case 0xfd: /* std */
            tcg_gen_movi_i32(cpu_tmp2_i32, -1);
            tcg_gen_st_i32(cpu_tmp2_i32, cpu_env, offsetof(CPUState, df));
            break;

        /************************/
        /* bit operations */
        case 0x1ba: /* bt/bts/btr/btc Gv, im */
            ot = dflag + OT_WORD;
            modrm = ldub_code(s->base.pc++);
            op = (modrm >> 3) & 7;
            mod = (modrm >> 6) & 3;
            rm = (modrm & 7) | REX_B(s);
            if(mod != 3) {
                s->rip_offset = 1;
                gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                gen_op_ld_T0_A0(ot + s->base.mem_idx);
            } else {
                gen_op_mov_TN_reg(ot, 0, rm);
            }
            /* load shift */
            val = ldub_code(s->base.pc++);
            gen_op_movl_T1_im(val);
            if(op < 4) {
                goto illegal_op;
            }
            op -= 4;
            goto bt_op;
        case 0x1a3: /* bt Gv, Ev */
            op = 0;
            goto do_btx;
        case 0x1ab: /* bts */
            op = 1;
            goto do_btx;
        case 0x1b3: /* btr */
            op = 2;
            goto do_btx;
        case 0x1bb: /* btc */
            op = 3;
        do_btx:
            ot = dflag + OT_WORD;
            modrm = ldub_code(s->base.pc++);
            reg = ((modrm >> 3) & 7) | rex_r;
            mod = (modrm >> 6) & 3;
            rm = (modrm & 7) | REX_B(s);
            gen_op_mov_TN_reg(OT_LONG, 1, reg);
            if(mod != 3) {
                gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                /* specific case: we need to add a displacement */
                gen_exts(ot, cpu_T[1]);
                tcg_gen_sari_tl(cpu_tmp0, cpu_T[1], 3 + ot);
                tcg_gen_shli_tl(cpu_tmp0, cpu_tmp0, ot);
                tcg_gen_add_tl(cpu_A0, cpu_A0, cpu_tmp0);
                gen_op_ld_T0_A0(ot + s->base.mem_idx);
            } else {
                gen_op_mov_TN_reg(ot, 0, rm);
            }
        bt_op:
            tcg_gen_andi_tl(cpu_T[1], cpu_T[1], (1 << (3 + ot)) - 1);
            switch(op) {
                case 0:
                    tcg_gen_shr_tl(cpu_cc_src, cpu_T[0], cpu_T[1]);
                    tcg_gen_movi_tl(cpu_cc_dst, 0);
                    break;
                case 1:
                    tcg_gen_shr_tl(cpu_tmp4, cpu_T[0], cpu_T[1]);
                    tcg_gen_movi_tl(cpu_tmp0, 1);
                    tcg_gen_shl_tl(cpu_tmp0, cpu_tmp0, cpu_T[1]);
                    tcg_gen_or_tl(cpu_T[0], cpu_T[0], cpu_tmp0);
                    break;
                case 2:
                    tcg_gen_shr_tl(cpu_tmp4, cpu_T[0], cpu_T[1]);
                    tcg_gen_movi_tl(cpu_tmp0, 1);
                    tcg_gen_shl_tl(cpu_tmp0, cpu_tmp0, cpu_T[1]);
                    tcg_gen_not_tl(cpu_tmp0, cpu_tmp0);
                    tcg_gen_and_tl(cpu_T[0], cpu_T[0], cpu_tmp0);
                    break;
                default:
                case 3:
                    tcg_gen_shr_tl(cpu_tmp4, cpu_T[0], cpu_T[1]);
                    tcg_gen_movi_tl(cpu_tmp0, 1);
                    tcg_gen_shl_tl(cpu_tmp0, cpu_tmp0, cpu_T[1]);
                    tcg_gen_xor_tl(cpu_T[0], cpu_T[0], cpu_tmp0);
                    break;
            }
            s->cc_op = CC_OP_SARB + ot;
            if(op != 0) {
                if(mod != 3) {
                    gen_op_st_T0_A0(ot + s->base.mem_idx);
                } else {
                    gen_op_mov_reg_T0(ot, rm);
                }
                tcg_gen_mov_tl(cpu_cc_src, cpu_tmp4);
                tcg_gen_movi_tl(cpu_cc_dst, 0);
            }
            break;
        case 0x1bc: /* bsf */
        case 0x1bd: /* bsr */
        {
            int label1;
            TCGv t0;

            ot = dflag + OT_WORD;
            modrm = ldub_code(s->base.pc++);
            reg = ((modrm >> 3) & 7) | rex_r;
            gen_ldst_modrm(s, modrm, ot, OR_TMP0, 0);
            gen_extu(ot, cpu_T[0]);
            t0 = tcg_temp_local_new();
            tcg_gen_mov_tl(t0, cpu_T[0]);
            if((b & 1) && (prefixes & PREFIX_REPZ) && (s->cpuid_ext3_features & CPUID_EXT3_ABM)) {
                switch(ot) {
                    case OT_WORD:
                        gen_helper_lzcnt(cpu_T[0], t0, tcg_const_i32(16));
                        break;
                    case OT_LONG:
                        gen_helper_lzcnt(cpu_T[0], t0, tcg_const_i32(32));
                        break;
                    case OT_QUAD:
                        gen_helper_lzcnt(cpu_T[0], t0, tcg_const_i32(64));
                        break;
                }
                gen_op_mov_reg_T0(ot, reg);
            } else {
                label1 = gen_new_label();
                tcg_gen_movi_tl(cpu_cc_dst, 0);
                tcg_gen_brcondi_tl(TCG_COND_EQ, t0, 0, label1);
                if(b & 1) {
                    gen_helper_bsr(cpu_T[0], t0);
                } else {
                    gen_helper_bsf(cpu_T[0], t0);
                }
                gen_op_mov_reg_T0(ot, reg);
                tcg_gen_movi_tl(cpu_cc_dst, 1);
                gen_set_label(label1);
                tcg_gen_discard_tl(cpu_cc_src);
                s->cc_op = CC_OP_LOGICB + ot;
            }
            tcg_temp_free(t0);
        } break;
        /************************/
        /* bcd */
        case 0x27: /* daa */
            if(CODE64(s)) {
                goto illegal_op;
            }
            if(s->cc_op != CC_OP_DYNAMIC) {
                gen_op_set_cc_op(s->cc_op);
            }
            gen_helper_daa();
            s->cc_op = CC_OP_EFLAGS;
            break;
        case 0x2f: /* das */
            if(CODE64(s)) {
                goto illegal_op;
            }
            if(s->cc_op != CC_OP_DYNAMIC) {
                gen_op_set_cc_op(s->cc_op);
            }
            gen_helper_das();
            s->cc_op = CC_OP_EFLAGS;
            break;
        case 0x37: /* aaa */
            if(CODE64(s)) {
                goto illegal_op;
            }
            if(s->cc_op != CC_OP_DYNAMIC) {
                gen_op_set_cc_op(s->cc_op);
            }
            gen_helper_aaa();
            s->cc_op = CC_OP_EFLAGS;
            break;
        case 0x3f: /* aas */
            if(CODE64(s)) {
                goto illegal_op;
            }
            if(s->cc_op != CC_OP_DYNAMIC) {
                gen_op_set_cc_op(s->cc_op);
            }
            gen_helper_aas();
            s->cc_op = CC_OP_EFLAGS;
            break;
        case 0xd4: /* aam */
            if(CODE64(s)) {
                goto illegal_op;
            }
            val = ldub_code(s->base.pc++);
            if(val == 0) {
                gen_exception(s, EXCP00_DIVZ, pc_start - s->cs_base);
            } else {
                gen_helper_aam(tcg_const_i32(val));
                s->cc_op = CC_OP_LOGICB;
            }
            break;
        case 0xd5: /* aad */
            if(CODE64(s)) {
                goto illegal_op;
            }
            val = ldub_code(s->base.pc++);
            gen_helper_aad(tcg_const_i32(val));
            s->cc_op = CC_OP_LOGICB;
            break;
        /************************/
        /* misc */
        case 0x90: /* nop */
            /* XXX: correct lock test for all insn */
            if(prefixes & PREFIX_LOCK) {
                goto illegal_op;
            }
            /* If REX_B is set, then this is xchg eax, r8d, not a nop.  */
            if(REX_B(s)) {
                goto do_xchg_reg_eax;
            }
            if(prefixes & PREFIX_REPZ) {
                gen_svm_check_intercept(s, pc_start, SVM_EXIT_PAUSE);
            }
            break;
        case 0x9b: /* fwait */
            if((s->flags & (HF_MP_MASK | HF_TS_MASK)) == (HF_MP_MASK | HF_TS_MASK)) {
                gen_exception(s, EXCP07_PREX, pc_start - s->cs_base);
            } else {
                if(s->cc_op != CC_OP_DYNAMIC) {
                    gen_op_set_cc_op(s->cc_op);
                }
                gen_jmp_im(pc_start - s->cs_base);
                gen_helper_fwait();
            }
            break;
        case 0xcc: /* int3 */
            gen_interrupt(s, EXCP03_INT3, pc_start - s->cs_base, s->base.pc - s->cs_base);
            break;
        case 0xcd: /* int N */
            val = ldub_code(s->base.pc++);
            if(s->vm86 && s->iopl != 3) {
                gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
            } else {
                gen_interrupt(s, val, pc_start - s->cs_base, s->base.pc - s->cs_base);
            }
            break;
        case 0xce: /* into */
            if(CODE64(s)) {
                goto illegal_op;
            }
            if(s->cc_op != CC_OP_DYNAMIC) {
                gen_op_set_cc_op(s->cc_op);
            }
            gen_jmp_im(pc_start - s->cs_base);
            gen_helper_into(tcg_const_i32(s->base.pc - pc_start));
            break;
#ifdef WANT_ICEBP
        case 0xf1: /* icebp (undocumented, exits to external debugger) */
            gen_svm_check_intercept(s, pc_start, SVM_EXIT_ICEBP);
            gen_debug(s, pc_start - s->cs_base);
            break;
#endif
        case 0xfa: /* cli */
            if(!s->vm86) {
                if(s->cpl <= s->iopl) {
                    gen_helper_cli();
                } else {
                    gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
                }
            } else {
                if(s->iopl == 3) {
                    gen_helper_cli();
                } else {
                    gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
                }
            }
            break;
        case 0xfb: /* sti */
            if(!s->vm86) {
                if(s->cpl <= s->iopl) {
                gen_sti:
                    gen_helper_sti();
                    /* interruptions are enabled only the first insn after sti */
                    /* If several instructions disable interrupts, only the
                       _first_ does it */
                    if(!(s->base.tb->flags & HF_INHIBIT_IRQ_MASK)) {
                        gen_helper_set_inhibit_irq();
                    }
                    /* give a chance to handle pending irqs */
                    gen_jmp_im(s->base.pc - s->cs_base);
                    gen_eob(s);
                } else {
                    gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
                }
            } else {
                if(s->iopl == 3) {
                    goto gen_sti;
                } else {
                    gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
                }
            }
            break;
        case 0x62: /* bound */
            if(CODE64(s)) {
                goto illegal_op;
            }
            ot = dflag ? OT_LONG : OT_WORD;
            modrm = ldub_code(s->base.pc++);
            reg = (modrm >> 3) & 7;
            mod = (modrm >> 6) & 3;
            if(mod == 3) {
                goto illegal_op;
            }
            gen_op_mov_TN_reg(ot, 0, reg);
            gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
            gen_jmp_im(pc_start - s->cs_base);
            tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_T[0]);
            if(ot == OT_WORD) {
                gen_helper_boundw(cpu_A0, cpu_tmp2_i32);
            } else {
                gen_helper_boundl(cpu_A0, cpu_tmp2_i32);
            }
            break;
        case 0x1c8 ... 0x1cf: /* bswap reg */
            reg = (b & 7) | REX_B(s);
#ifdef TARGET_X86_64
            if(dflag == 2) {
                gen_op_mov_TN_reg(OT_QUAD, 0, reg);
                tcg_gen_bswap64_i64(cpu_T[0], cpu_T[0]);
                gen_op_mov_reg_T0(OT_QUAD, reg);
            } else
#endif
            {
                gen_op_mov_TN_reg(OT_LONG, 0, reg);
                tcg_gen_ext32u_tl(cpu_T[0], cpu_T[0]);
                tcg_gen_bswap32_tl(cpu_T[0], cpu_T[0]);
                gen_op_mov_reg_T0(OT_LONG, reg);
            }
            break;
        case 0xd6: /* salc */
            if(CODE64(s)) {
                goto illegal_op;
            }
            if(s->cc_op != CC_OP_DYNAMIC) {
                gen_op_set_cc_op(s->cc_op);
            }
            gen_compute_eflags_c(cpu_T[0]);
            tcg_gen_neg_tl(cpu_T[0], cpu_T[0]);
            gen_op_mov_reg_T0(OT_BYTE, R_EAX);
            break;
        case 0xe0: /* loopnz */
        case 0xe1: /* loopz */
        case 0xe2: /* loop */
        case 0xe3: /* jecxz */
        {
            int l1, l2, l3;

            tval = (int8_t)insn_get(s, OT_BYTE);
            next_eip = s->base.pc - s->cs_base;
            tval += next_eip;
            if(s->dflag == 0) {
                tval &= 0xffff;
            }

            l1 = gen_new_label();
            l2 = gen_new_label();
            l3 = gen_new_label();
            b &= 3;
            switch(b) {
                case 0: /* loopnz */
                case 1: /* loopz */
                    if(s->cc_op != CC_OP_DYNAMIC) {
                        gen_op_set_cc_op(s->cc_op);
                    }
                    gen_op_add_reg_im(s->aflag, R_ECX, -1);
                    gen_op_jz_ecx(s->aflag, l3);
                    gen_compute_eflags(cpu_tmp0);
                    tcg_gen_andi_tl(cpu_tmp0, cpu_tmp0, CC_Z);
                    if(b == 0) {
                        tcg_gen_brcondi_tl(TCG_COND_EQ, cpu_tmp0, 0, l1);
                    } else {
                        tcg_gen_brcondi_tl(TCG_COND_NE, cpu_tmp0, 0, l1);
                    }
                    break;
                case 2: /* loop */
                    gen_op_add_reg_im(s->aflag, R_ECX, -1);
                    gen_op_jnz_ecx(s->aflag, l1);
                    break;
                default:
                case 3: /* jcxz */
                    gen_op_jz_ecx(s->aflag, l1);
                    break;
            }

            gen_set_label(l3);
            gen_jmp_im(next_eip);
            tcg_gen_br(l2);

            gen_set_label(l1);
            gen_jmp_im(tval);
            gen_set_label(l2);
            gen_eob(s);
        } break;
        case 0x130: /* wrmsr */
        case 0x132: /* rdmsr */
            if(s->cpl != 0) {
                gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
            } else {
                if(s->cc_op != CC_OP_DYNAMIC) {
                    gen_op_set_cc_op(s->cc_op);
                }
                gen_jmp_im(pc_start - s->cs_base);
                if(b & 2) {
                    gen_helper_rdmsr();
                } else {
                    gen_helper_wrmsr();
                }
            }
            break;
        case 0x131: /* rdtsc */
            if(s->cc_op != CC_OP_DYNAMIC) {
                gen_op_set_cc_op(s->cc_op);
            }
            gen_jmp_im(pc_start - s->cs_base);
            gen_helper_rdtsc();
            break;
        case 0x133: /* rdpmc */
            if(s->cc_op != CC_OP_DYNAMIC) {
                gen_op_set_cc_op(s->cc_op);
            }
            gen_jmp_im(pc_start - s->cs_base);
            gen_helper_rdpmc();
            break;
        case 0x134: /* sysenter */
            /* For Intel SYSENTER is valid on 64-bit */
            if(CODE64(s) && cpu->cpuid_vendor1 != CPUID_VENDOR_INTEL_1) {
                goto illegal_op;
            }
            if(!s->pe) {
                gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
            } else {
                gen_update_cc_op(s);
                gen_jmp_im(pc_start - s->cs_base);
                gen_helper_sysenter();
                gen_eob(s);
            }
            break;
        case 0x135: /* sysexit */
            /* For Intel SYSEXIT is valid on 64-bit */
            if(CODE64(s) && cpu->cpuid_vendor1 != CPUID_VENDOR_INTEL_1) {
                goto illegal_op;
            }
            if(!s->pe) {
                gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
            } else {
                gen_update_cc_op(s);
                gen_jmp_im(pc_start - s->cs_base);
                gen_helper_sysexit(tcg_const_i32(dflag));
                gen_eob(s);
            }
            break;
#ifdef TARGET_X86_64
        case 0x105: /* syscall */
            /* XXX: is it usable in real mode ? */
            gen_update_cc_op(s);
            gen_jmp_im(pc_start - s->cs_base);
            gen_helper_syscall(tcg_const_i32(s->base.pc - pc_start));
            gen_eob(s);
            break;
        case 0x107: /* sysret */
            if(!s->pe) {
                gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
            } else {
                gen_update_cc_op(s);
                gen_jmp_im(pc_start - s->cs_base);
                gen_helper_sysret(tcg_const_i32(s->dflag));
                /* condition codes are modified only in long mode */
                if(s->lma) {
                    s->cc_op = CC_OP_EFLAGS;
                }
                gen_eob(s);
            }
            break;
#endif
        case 0x1a2: /* cpuid */
            if(s->cc_op != CC_OP_DYNAMIC) {
                gen_op_set_cc_op(s->cc_op);
            }
            gen_jmp_im(pc_start - s->cs_base);
            gen_helper_cpuid();
            break;
        case 0xf4: /* hlt */
            if(s->cpl != 0) {
                gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
            } else {
                if(s->cc_op != CC_OP_DYNAMIC) {
                    gen_op_set_cc_op(s->cc_op);
                }
                gen_jmp_im(pc_start - s->cs_base);
                gen_helper_hlt(tcg_const_i32(s->base.pc - pc_start));
                s->base.is_jmp = DISAS_TB_JUMP;
            }
            break;
        case 0x100:
            modrm = ldub_code(s->base.pc++);
            mod = (modrm >> 6) & 3;
            op = (modrm >> 3) & 7;
            switch(op) {
                case 0: /* sldt */
                    if(!s->pe || s->vm86) {
                        goto illegal_op;
                    }
                    gen_svm_check_intercept(s, pc_start, SVM_EXIT_LDTR_READ);
                    tcg_gen_ld32u_tl(cpu_T[0], cpu_env, offsetof(CPUState, ldt.selector));
                    ot = OT_WORD;
                    if(mod == 3) {
                        ot += s->dflag;
                    }
                    gen_ldst_modrm(s, modrm, ot, OR_TMP0, 1);
                    break;
                case 2: /* lldt */
                    if(!s->pe || s->vm86) {
                        goto illegal_op;
                    }
                    if(s->cpl != 0) {
                        gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
                    } else {
                        gen_svm_check_intercept(s, pc_start, SVM_EXIT_LDTR_WRITE);
                        gen_ldst_modrm(s, modrm, OT_WORD, OR_TMP0, 0);
                        gen_jmp_im(pc_start - s->cs_base);
                        tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_T[0]);
                        gen_helper_lldt(cpu_tmp2_i32);
                    }
                    break;
                case 1: /* str */
                    if(!s->pe || s->vm86) {
                        goto illegal_op;
                    }
                    gen_svm_check_intercept(s, pc_start, SVM_EXIT_TR_READ);
                    tcg_gen_ld32u_tl(cpu_T[0], cpu_env, offsetof(CPUState, tr.selector));
                    ot = OT_WORD;
                    if(mod == 3) {
                        ot += s->dflag;
                    }
                    gen_ldst_modrm(s, modrm, ot, OR_TMP0, 1);
                    break;
                case 3: /* ltr */
                    if(!s->pe || s->vm86) {
                        goto illegal_op;
                    }
                    if(s->cpl != 0) {
                        gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
                    } else {
                        gen_svm_check_intercept(s, pc_start, SVM_EXIT_TR_WRITE);
                        gen_ldst_modrm(s, modrm, OT_WORD, OR_TMP0, 0);
                        gen_jmp_im(pc_start - s->cs_base);
                        tcg_gen_trunc_tl_i32(cpu_tmp2_i32, cpu_T[0]);
                        gen_helper_ltr(cpu_tmp2_i32);
                    }
                    break;
                case 4: /* verr */
                case 5: /* verw */
                    if(!s->pe || s->vm86) {
                        goto illegal_op;
                    }
                    gen_ldst_modrm(s, modrm, OT_WORD, OR_TMP0, 0);
                    if(s->cc_op != CC_OP_DYNAMIC) {
                        gen_op_set_cc_op(s->cc_op);
                    }
                    if(op == 4) {
                        gen_helper_verr(cpu_T[0]);
                    } else {
                        gen_helper_verw(cpu_T[0]);
                    }
                    s->cc_op = CC_OP_EFLAGS;
                    break;
                default:
                    goto illegal_op;
            }
            break;
        case 0x101:
            modrm = ldub_code(s->base.pc++);
            mod = (modrm >> 6) & 3;
            op = (modrm >> 3) & 7;
            rm = modrm & 7;
            switch(op) {
                case 0: /* sgdt */
                    if(mod == 3) {
                        goto illegal_op;
                    }
                    gen_svm_check_intercept(s, pc_start, SVM_EXIT_GDTR_READ);
                    gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    tcg_gen_ld32u_tl(cpu_T[0], cpu_env, offsetof(CPUState, gdt.limit));
                    gen_op_st_T0_A0(OT_WORD + s->base.mem_idx);
                    gen_add_A0_im(s, 2);
                    tcg_gen_ld_tl(cpu_T[0], cpu_env, offsetof(CPUState, gdt.base));
                    if(!s->dflag) {
                        gen_op_andl_T0_im(0xffffff);
                    }
                    gen_op_st_T0_A0(CODE64(s) + OT_LONG + s->base.mem_idx);
                    break;
                case 1:
                    if(mod == 3) {
                        switch(rm) {
                            case 0: /* monitor */
                                if(!(s->cpuid_ext_features & CPUID_EXT_MONITOR) || s->cpl != 0) {
                                    goto illegal_op;
                                }
                                if(s->cc_op != CC_OP_DYNAMIC) {
                                    gen_op_set_cc_op(s->cc_op);
                                }
                                gen_jmp_im(pc_start - s->cs_base);
#ifdef TARGET_X86_64
                                if(s->aflag == 2) {
                                    gen_op_movq_A0_reg(R_EAX);
                                } else
#endif
                                {
                                    gen_op_movl_A0_reg(R_EAX);
                                    if(s->aflag == 0) {
                                        gen_op_andl_A0_ffff();
                                    }
                                }
                                gen_add_A0_ds_seg(s);
                                gen_helper_monitor(cpu_A0);
                                break;
                            case 1: /* mwait */
                                if(!(s->cpuid_ext_features & CPUID_EXT_MONITOR) || s->cpl != 0) {
                                    goto illegal_op;
                                }
                                gen_update_cc_op(s);
                                gen_jmp_im(pc_start - s->cs_base);
                                gen_helper_mwait(tcg_const_i32(s->base.pc - pc_start));
                                gen_eob(s);
                                break;
                            default:
                                goto illegal_op;
                        }
                    } else { /* sidt */
                        gen_svm_check_intercept(s, pc_start, SVM_EXIT_IDTR_READ);
                        gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                        tcg_gen_ld32u_tl(cpu_T[0], cpu_env, offsetof(CPUState, idt.limit));
                        gen_op_st_T0_A0(OT_WORD + s->base.mem_idx);
                        gen_add_A0_im(s, 2);
                        tcg_gen_ld_tl(cpu_T[0], cpu_env, offsetof(CPUState, idt.base));
                        if(!s->dflag) {
                            gen_op_andl_T0_im(0xffffff);
                        }
                        gen_op_st_T0_A0(CODE64(s) + OT_LONG + s->base.mem_idx);
                    }
                    break;
                case 2: /* lgdt */
                case 3: /* lidt */
                    if(mod == 3) {
                        if(s->cc_op != CC_OP_DYNAMIC) {
                            gen_op_set_cc_op(s->cc_op);
                        }
                        gen_jmp_im(pc_start - s->cs_base);
                        switch(rm) {
                            case 0: /* VMRUN */
                                if(!(s->flags & HF_SVME_MASK) || !s->pe) {
                                    goto illegal_op;
                                }
                                if(s->cpl != 0) {
                                    gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
                                    break;
                                } else {
                                    gen_helper_vmrun(tcg_const_i32(s->aflag), tcg_const_i32(s->base.pc - pc_start));
                                    gen_exit_tb_no_chaining(s->base.tb);
                                    s->base.is_jmp = DISAS_TB_JUMP;
                                }
                                break;
                            case 1: /* VMMCALL */
                                if(!(s->flags & HF_SVME_MASK)) {
                                    goto illegal_op;
                                }
                                gen_helper_vmmcall();
                                break;
                            case 2: /* VMLOAD */
                                if(!(s->flags & HF_SVME_MASK) || !s->pe) {
                                    goto illegal_op;
                                }
                                if(s->cpl != 0) {
                                    gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
                                    break;
                                } else {
                                    gen_helper_vmload(tcg_const_i32(s->aflag));
                                }
                                break;
                            case 3: /* VMSAVE */
                                if(!(s->flags & HF_SVME_MASK) || !s->pe) {
                                    goto illegal_op;
                                }
                                if(s->cpl != 0) {
                                    gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
                                    break;
                                } else {
                                    gen_helper_vmsave(tcg_const_i32(s->aflag));
                                }
                                break;
                            case 4: /* STGI */
                                if((!(s->flags & HF_SVME_MASK) && !(s->cpuid_ext3_features & CPUID_EXT3_SKINIT)) || !s->pe) {
                                    goto illegal_op;
                                }
                                if(s->cpl != 0) {
                                    gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
                                    break;
                                } else {
                                    gen_helper_stgi();
                                }
                                break;
                            case 5: /* CLGI */
                                if(!(s->flags & HF_SVME_MASK) || !s->pe) {
                                    goto illegal_op;
                                }
                                if(s->cpl != 0) {
                                    gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
                                    break;
                                } else {
                                    gen_helper_clgi();
                                }
                                break;
                            case 6: /* SKINIT */
                                if((!(s->flags & HF_SVME_MASK) && !(s->cpuid_ext3_features & CPUID_EXT3_SKINIT)) || !s->pe) {
                                    goto illegal_op;
                                }
                                gen_helper_skinit();
                                break;
                            case 7: /* INVLPGA */
                                if(!(s->flags & HF_SVME_MASK) || !s->pe) {
                                    goto illegal_op;
                                }
                                if(s->cpl != 0) {
                                    gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
                                    break;
                                } else {
                                    gen_helper_invlpga(tcg_const_i32(s->aflag));
                                }
                                break;
                            default:
                                goto illegal_op;
                        }
                    } else if(s->cpl != 0) {
                        gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
                    } else {
                        gen_svm_check_intercept(s, pc_start, op == 2 ? SVM_EXIT_GDTR_WRITE : SVM_EXIT_IDTR_WRITE);
                        gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                        gen_op_ld_T1_A0(OT_WORD + s->base.mem_idx);
                        gen_add_A0_im(s, 2);
                        gen_op_ld_T0_A0(CODE64(s) + OT_LONG + s->base.mem_idx);
                        if(!s->dflag) {
                            gen_op_andl_T0_im(0xffffff);
                        }
                        if(op == 2) {
                            tcg_gen_st_tl(cpu_T[0], cpu_env, offsetof(CPUState, gdt.base));
                            tcg_gen_st32_tl(cpu_T[1], cpu_env, offsetof(CPUState, gdt.limit));
                        } else {
                            tcg_gen_st_tl(cpu_T[0], cpu_env, offsetof(CPUState, idt.base));
                            tcg_gen_st32_tl(cpu_T[1], cpu_env, offsetof(CPUState, idt.limit));
                        }
                    }
                    break;
                case 4: /* smsw */
                    gen_svm_check_intercept(s, pc_start, SVM_EXIT_READ_CR0);
#if defined TARGET_X86_64 && defined HOST_WORDS_BIGENDIAN
                    tcg_gen_ld32u_tl(cpu_T[0], cpu_env, offsetof(CPUState, cr[0]) + 4);
#else
                    tcg_gen_ld32u_tl(cpu_T[0], cpu_env, offsetof(CPUState, cr[0]));
#endif
                    gen_ldst_modrm(s, modrm, OT_WORD, OR_TMP0, 1);
                    break;
                case 6: /* lmsw */
                    if(s->cpl != 0) {
                        gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
                    } else {
                        gen_svm_check_intercept(s, pc_start, SVM_EXIT_WRITE_CR0);
                        gen_ldst_modrm(s, modrm, OT_WORD, OR_TMP0, 0);
                        gen_helper_lmsw(cpu_T[0]);
                        gen_jmp_im(s->base.pc - s->cs_base);
                        gen_eob(s);
                    }
                    break;
                case 7:
                    if(mod != 3) { /* invlpg */
                        if(s->cpl != 0) {
                            gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
                        } else {
                            if(s->cc_op != CC_OP_DYNAMIC) {
                                gen_op_set_cc_op(s->cc_op);
                            }
                            gen_jmp_im(pc_start - s->cs_base);
                            gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                            gen_helper_invlpg(cpu_A0);
                            gen_jmp_im(s->base.pc - s->cs_base);
                            gen_eob(s);
                        }
                    } else {
                        switch(rm) {
                            case 0: /* swapgs */
#ifdef TARGET_X86_64
                                if(CODE64(s)) {
                                    if(s->cpl != 0) {
                                        gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
                                    } else {
                                        tcg_gen_ld_tl(cpu_T[0], cpu_env, offsetof(CPUState, segs[R_GS].base));
                                        tcg_gen_ld_tl(cpu_T[1], cpu_env, offsetof(CPUState, kernelgsbase));
                                        tcg_gen_st_tl(cpu_T[1], cpu_env, offsetof(CPUState, segs[R_GS].base));
                                        tcg_gen_st_tl(cpu_T[0], cpu_env, offsetof(CPUState, kernelgsbase));
                                    }
                                } else
#endif
                                {
                                    goto illegal_op;
                                }
                                break;
                            case 1: /* rdtscp */
                                if(!(s->cpuid_ext2_features & CPUID_EXT2_RDTSCP)) {
                                    goto illegal_op;
                                }
                                if(s->cc_op != CC_OP_DYNAMIC) {
                                    gen_op_set_cc_op(s->cc_op);
                                }
                                gen_jmp_im(pc_start - s->cs_base);
                                gen_helper_rdtscp();
                                break;
                            default:
                                goto illegal_op;
                        }
                    }
                    break;
                default:
                    goto illegal_op;
            }
            break;
        case 0x108: /* invd */
        case 0x109: /* wbinvd */
            if(s->cpl != 0) {
                gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
            } else {
                gen_svm_check_intercept(s, pc_start, (b & 2) ? SVM_EXIT_INVD : SVM_EXIT_WBINVD);
                /* nothing to do */
            }
            break;
        case 0x63: /* arpl or movslS (x86_64) */
#ifdef TARGET_X86_64
            if(CODE64(s)) {
                int d_ot;
                /* d_ot is the size of destination */
                d_ot = dflag + OT_WORD;

                modrm = ldub_code(s->base.pc++);
                reg = ((modrm >> 3) & 7) | rex_r;
                mod = (modrm >> 6) & 3;
                rm = (modrm & 7) | REX_B(s);

                if(mod == 3) {
                    gen_op_mov_TN_reg(OT_LONG, 0, rm);
                    /* sign extend */
                    if(d_ot == OT_QUAD) {
                        tcg_gen_ext32s_tl(cpu_T[0], cpu_T[0]);
                    }
                    gen_op_mov_reg_T0(d_ot, reg);
                } else {
                    gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    if(d_ot == OT_QUAD) {
                        gen_op_lds_T0_A0(OT_LONG + s->base.mem_idx);
                    } else {
                        gen_op_ld_T0_A0(OT_LONG + s->base.mem_idx);
                    }
                    gen_op_mov_reg_T0(d_ot, reg);
                }
            } else
#endif
            {
                int label1;
                TCGv t0, t1, t2, a0;

                if(!s->pe || s->vm86) {
                    goto illegal_op;
                }
                t0 = tcg_temp_local_new();
                t1 = tcg_temp_local_new();
                t2 = tcg_temp_local_new();
                ot = OT_WORD;
                modrm = ldub_code(s->base.pc++);
                reg = (modrm >> 3) & 7;
                mod = (modrm >> 6) & 3;
                rm = modrm & 7;
                if(mod != 3) {
                    gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    gen_op_ld_v(ot + s->base.mem_idx, t0, cpu_A0);
                    a0 = tcg_temp_local_new();
                    tcg_gen_mov_tl(a0, cpu_A0);
                } else {
                    gen_op_mov_v_reg(ot, t0, rm);
                    TCGV_UNUSED(a0);
                }
                gen_op_mov_v_reg(ot, t1, reg);
                tcg_gen_andi_tl(cpu_tmp0, t0, 3);
                tcg_gen_andi_tl(t1, t1, 3);
                tcg_gen_movi_tl(t2, 0);
                label1 = gen_new_label();
                tcg_gen_brcond_tl(TCG_COND_GE, cpu_tmp0, t1, label1);
                tcg_gen_andi_tl(t0, t0, ~3);
                tcg_gen_or_tl(t0, t0, t1);
                tcg_gen_movi_tl(t2, CC_Z);
                gen_set_label(label1);
                if(mod != 3) {
                    gen_op_st_v(ot + s->base.mem_idx, t0, a0);
                    tcg_temp_free(a0);
                } else {
                    gen_op_mov_reg_v(ot, rm, t0);
                }
                if(s->cc_op != CC_OP_DYNAMIC) {
                    gen_op_set_cc_op(s->cc_op);
                }
                gen_compute_eflags(cpu_cc_src);
                tcg_gen_andi_tl(cpu_cc_src, cpu_cc_src, ~CC_Z);
                tcg_gen_or_tl(cpu_cc_src, cpu_cc_src, t2);
                s->cc_op = CC_OP_EFLAGS;
                tcg_temp_free(t0);
                tcg_temp_free(t1);
                tcg_temp_free(t2);
            }
            break;
        case 0x102: /* lar */
        case 0x103: /* lsl */
        {
            int label1;
            TCGv t0;
            if(!s->pe || s->vm86) {
                goto illegal_op;
            }
            ot = dflag ? OT_LONG : OT_WORD;
            modrm = ldub_code(s->base.pc++);
            reg = ((modrm >> 3) & 7) | rex_r;
            gen_ldst_modrm(s, modrm, OT_WORD, OR_TMP0, 0);
            t0 = tcg_temp_local_new();
            if(s->cc_op != CC_OP_DYNAMIC) {
                gen_op_set_cc_op(s->cc_op);
            }
            if(b == 0x102) {
                gen_helper_lar(t0, cpu_T[0]);
            } else {
                gen_helper_lsl(t0, cpu_T[0]);
            }
            tcg_gen_andi_tl(cpu_tmp0, cpu_cc_src, CC_Z);
            label1 = gen_new_label();
            tcg_gen_brcondi_tl(TCG_COND_EQ, cpu_tmp0, 0, label1);
            gen_op_mov_reg_v(ot, reg, t0);
            gen_set_label(label1);
            s->cc_op = CC_OP_EFLAGS;
            tcg_temp_free(t0);
        } break;
        case 0x118:
            modrm = ldub_code(s->base.pc++);
            mod = (modrm >> 6) & 3;
            op = (modrm >> 3) & 7;
            switch(op) {
                case 0: /* prefetchnta */
                case 1: /* prefetchnt0 */
                case 2: /* prefetchnt0 */
                case 3: /* prefetchnt0 */
                    if(mod == 3) {
                        goto illegal_op;
                    }
                    gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    /* nothing more to do */
                    break;
                default: /* nop (multi byte) */
                    gen_nop_modrm(s, modrm);
                    break;
            }
            break;
        case 0x119 ... 0x11f: /* nop (multi byte) */
            modrm = ldub_code(s->base.pc++);
            gen_nop_modrm(s, modrm);
            break;
        case 0x120: /* mov reg, crN */
        case 0x122: /* mov crN, reg */
            if(s->cpl != 0) {
                gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
            } else {
                modrm = ldub_code(s->base.pc++);
                if((modrm & 0xc0) != 0xc0) {
                    goto illegal_op;
                }
                rm = (modrm & 7) | REX_B(s);
                reg = ((modrm >> 3) & 7) | rex_r;
                if(CODE64(s)) {
                    ot = OT_QUAD;
                } else {
                    ot = OT_LONG;
                }
                if((prefixes & PREFIX_LOCK) && (reg == 0) && (s->cpuid_ext3_features & CPUID_EXT3_CR8LEG)) {
                    reg = 8;
                }
                switch(reg) {
                    case 0:
                    case 2:
                    case 3:
                    case 4:
                    case 8:
                        if(s->cc_op != CC_OP_DYNAMIC) {
                            gen_op_set_cc_op(s->cc_op);
                        }
                        gen_jmp_im(pc_start - s->cs_base);
                        if(b & 2) {
                            gen_op_mov_TN_reg(ot, 0, rm);
                            gen_helper_write_crN(tcg_const_i32(reg), cpu_T[0]);
                            gen_jmp_im(s->base.pc - s->cs_base);
                            gen_eob(s);
                        } else {
                            gen_helper_read_crN(cpu_T[0], tcg_const_i32(reg));
                            gen_op_mov_reg_T0(ot, rm);
                        }
                        break;
                    default:
                        goto illegal_op;
                }
            }
            break;
        case 0x121: /* mov reg, drN */
        case 0x123: /* mov drN, reg */
            if(s->cpl != 0) {
                gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
            } else {
                modrm = ldub_code(s->base.pc++);
                if((modrm & 0xc0) != 0xc0) {
                    goto illegal_op;
                }
                rm = (modrm & 7) | REX_B(s);
                reg = ((modrm >> 3) & 7) | rex_r;
                if(CODE64(s)) {
                    ot = OT_QUAD;
                } else {
                    ot = OT_LONG;
                }
                /* XXX: do it dynamically with CR4.DE bit */
                if(reg == 4 || reg == 5 || reg >= 8) {
                    goto illegal_op;
                }
                if(b & 2) {
                    gen_svm_check_intercept(s, pc_start, SVM_EXIT_WRITE_DR0 + reg);
                    gen_op_mov_TN_reg(ot, 0, rm);
                    gen_helper_movl_drN_T0(tcg_const_i32(reg), cpu_T[0]);
                    gen_jmp_im(s->base.pc - s->cs_base);
                    gen_eob(s);
                } else {
                    gen_svm_check_intercept(s, pc_start, SVM_EXIT_READ_DR0 + reg);
                    tcg_gen_ld_tl(cpu_T[0], cpu_env, offsetof(CPUState, dr[reg]));
                    gen_op_mov_reg_T0(ot, rm);
                }
            }
            break;
        case 0x106: /* clts */
            if(s->cpl != 0) {
                gen_exception(s, EXCP0D_GPF, pc_start - s->cs_base);
            } else {
                gen_svm_check_intercept(s, pc_start, SVM_EXIT_WRITE_CR0);
                gen_helper_clts();
                /* abort block because static cpu state changed */
                gen_jmp_im(s->base.pc - s->cs_base);
                gen_eob(s);
            }
            break;
        /* MMX/3DNow!/SSE/SSE2/SSE3/SSSE3/SSE4 support */
        case 0x1c3: /* MOVNTI reg, mem */
            if(!(s->cpuid_features & CPUID_SSE2)) {
                goto illegal_op;
            }
            ot = s->dflag == 2 ? OT_QUAD : OT_LONG;
            modrm = ldub_code(s->base.pc++);
            mod = (modrm >> 6) & 3;
            if(mod == 3) {
                goto illegal_op;
            }
            reg = ((modrm >> 3) & 7) | rex_r;
            /* generate a generic store */
            gen_ldst_modrm(s, modrm, ot, reg, 1);
            break;
        case 0x1ae:
            modrm = ldub_code(s->base.pc++);
            mod = (modrm >> 6) & 3;
            op = (modrm >> 3) & 7;
            switch(op) {
                case 0: /* fxsave */
                    if(mod == 3 || !(s->cpuid_features & CPUID_FXSR) || (s->prefix & PREFIX_LOCK)) {
                        goto illegal_op;
                    }
                    if((s->flags & HF_EM_MASK) || (s->flags & HF_TS_MASK)) {
                        gen_exception(s, EXCP07_PREX, pc_start - s->cs_base);
                        break;
                    }
                    gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    if(s->cc_op != CC_OP_DYNAMIC) {
                        gen_op_set_cc_op(s->cc_op);
                    }
                    gen_jmp_im(pc_start - s->cs_base);
                    gen_helper_fxsave(cpu_A0, tcg_const_i32((s->dflag == 2)));
                    break;
                case 1: /* fxrstor */
                    if(mod == 3 || !(s->cpuid_features & CPUID_FXSR) || (s->prefix & PREFIX_LOCK)) {
                        goto illegal_op;
                    }
                    if((s->flags & HF_EM_MASK) || (s->flags & HF_TS_MASK)) {
                        gen_exception(s, EXCP07_PREX, pc_start - s->cs_base);
                        break;
                    }
                    gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    if(s->cc_op != CC_OP_DYNAMIC) {
                        gen_op_set_cc_op(s->cc_op);
                    }
                    gen_jmp_im(pc_start - s->cs_base);
                    gen_helper_fxrstor(cpu_A0, tcg_const_i32((s->dflag == 2)));
                    break;
                case 2: /* ldmxcsr */
                case 3: /* stmxcsr */
                    if(s->flags & HF_TS_MASK) {
                        gen_exception(s, EXCP07_PREX, pc_start - s->cs_base);
                        break;
                    }
                    if((s->flags & HF_EM_MASK) || !(s->flags & HF_OSFXSR_MASK) || mod == 3) {
                        goto illegal_op;
                    }
                    gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    if(op == 2) {
                        gen_op_ld_T0_A0(OT_LONG + s->base.mem_idx);
                        tcg_gen_st32_tl(cpu_T[0], cpu_env, offsetof(CPUState, mxcsr));
                    } else {
                        tcg_gen_ld32u_tl(cpu_T[0], cpu_env, offsetof(CPUState, mxcsr));
                        gen_op_st_T0_A0(OT_LONG + s->base.mem_idx);
                    }
                    break;
                case 5: /* lfence */
                case 6: /* mfence */
                    if((modrm & 0xc7) != 0xc0 || !(s->cpuid_features & CPUID_SSE2)) {
                        goto illegal_op;
                    }
                    break;
                case 7: /* sfence / clflush */
                    if((modrm & 0xc7) == 0xc0) {
                        /* sfence */
                        /* XXX: also check for cpuid_ext2_features & CPUID_EXT2_EMMX */
                        if(!(s->cpuid_features & CPUID_SSE)) {
                            goto illegal_op;
                        }
                    } else {
                        /* clflush */
                        if(!(s->cpuid_features & CPUID_CLFLUSH)) {
                            goto illegal_op;
                        }
                        gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
                    }
                    break;
                default:
                    goto illegal_op;
            }
            break;
        case 0x10d: /* 3DNow! prefetch(w) */
            modrm = ldub_code(s->base.pc++);
            mod = (modrm >> 6) & 3;
            if(mod == 3) {
                goto illegal_op;
            }
            gen_lea_modrm(s, modrm, &reg_addr, &offset_addr);
            /* ignore for now */
            break;
        case 0x1aa: /* rsm */
            gen_svm_check_intercept(s, pc_start, SVM_EXIT_RSM);
            if(!(s->flags & HF_SMM_MASK)) {
                goto illegal_op;
            }
            gen_update_cc_op(s);
            gen_jmp_im(s->base.pc - s->cs_base);
            gen_helper_rsm();
            gen_eob(s);
            break;
        case 0x1b8: /* SSE4.2 popcnt */
            if((prefixes & (PREFIX_REPZ | PREFIX_LOCK | PREFIX_REPNZ)) != PREFIX_REPZ) {
                goto illegal_op;
            }
            if(!(s->cpuid_ext_features & CPUID_EXT_POPCNT)) {
                goto illegal_op;
            }

            modrm = ldub_code(s->base.pc++);
            reg = ((modrm >> 3) & 7);

            if(s->prefix & PREFIX_DATA) {
                ot = OT_WORD;
            } else if(s->dflag != 2) {
                ot = OT_LONG;
            } else {
                ot = OT_QUAD;
            }

            gen_ldst_modrm(s, modrm, ot, OR_TMP0, 0);
            gen_helper_popcnt(cpu_T[0], cpu_T[0], tcg_const_i32(ot));
            gen_op_mov_reg_T0(ot, reg);

            s->cc_op = CC_OP_EFLAGS;
            break;
        case 0x10e ... 0x10f:
            /* 3DNow! instructions, ignore prefixes */
            s->prefix &= ~(PREFIX_REPZ | PREFIX_REPNZ | PREFIX_DATA);
            goto case_0x1d0_0x1fe;
        case 0x110 ... 0x117:
        case 0x128 ... 0x12f:
        case 0x138 ... 0x13a:
        case 0x150 ... 0x179:
        case 0x17c ... 0x17f:
        case 0x1c2:
        case 0x1c4 ... 0x1c6:
        case 0x1d0 ... 0x1fe:
        case_0x1d0_0x1fe:
            gen_sse(s, b, pc_start, rex_r);
            break;
        default:
            goto illegal_op;
    }
    return (int)(s->base.pc - pc_start);
illegal_op:
    /* XXX: ensure that no lock was generated */
    gen_exception(s, EXCP06_ILLOP, pc_start - s->cs_base);
    return (int)(s->base.pc - pc_start);
}

void setup_disas_context(DisasContextBase *base, CPUState *env)
{
    DisasContext *dc = (DisasContext *)base;
    dc->pe = (dc->base.tb->flags >> HF_PE_SHIFT) & 1;
    dc->code32 = (dc->base.tb->flags >> HF_CS32_SHIFT) & 1;
    dc->ss32 = (dc->base.tb->flags >> HF_SS32_SHIFT) & 1;
    dc->addseg = (dc->base.tb->flags >> HF_ADDSEG_SHIFT) & 1;
    dc->f_st = 0;
    dc->vm86 = (dc->base.tb->flags >> VM_SHIFT) & 1;
    dc->cpl = (dc->base.tb->flags >> HF_CPL_SHIFT) & 3;
    dc->iopl = (dc->base.tb->flags >> IOPL_SHIFT) & 3;
    dc->tf = (dc->base.tb->flags >> TF_SHIFT) & 1;
    dc->cc_op = CC_OP_DYNAMIC;
    dc->cs_base = dc->base.tb->cs_base;
    dc->popl_esp_hack = 0;
    /* select memory access functions */
    dc->base.mem_idx = 0;
    if(dc->base.tb->flags & HF_SOFTMMU_MASK) {
        if(dc->cpl == 3) {
            dc->base.mem_idx = 2 * 4;
        } else {
            dc->base.mem_idx = 1 * 4;
        }
    }
    dc->cpuid_features = env->cpuid_features;
    dc->cpuid_ext_features = env->cpuid_ext_features;
    dc->cpuid_ext2_features = env->cpuid_ext2_features;
    dc->cpuid_ext3_features = env->cpuid_ext3_features;
#ifdef TARGET_X86_64
    dc->lma = (dc->base.tb->flags >> HF_LMA_SHIFT) & 1;
    dc->code64 = (dc->base.tb->flags >> HF_CS64_SHIFT) & 1;
#endif
    dc->flags = dc->base.tb->flags;
    dc->jmp_opt = !(dc->tf || (dc->base.tb->flags & HF_INHIBIT_IRQ_MASK));

    cpu_T[0] = tcg_temp_new();
    cpu_T[1] = tcg_temp_new();
    cpu_A0 = tcg_temp_new();
    cpu_T3 = tcg_temp_new();

    cpu_tmp0 = tcg_temp_new();
    cpu_tmp1_i64 = tcg_temp_new_i64();
    cpu_tmp2_i32 = tcg_temp_new_i32();
    cpu_tmp3_i32 = tcg_temp_new_i32();
    cpu_tmp4 = tcg_temp_new();
    cpu_tmp5 = tcg_temp_new();
    cpu_ptr0 = tcg_temp_new_ptr();
    cpu_ptr1 = tcg_temp_new_ptr();
}

int gen_breakpoint(DisasContextBase *base, CPUBreakpoint *bp)
{
    DisasContext *dc = (DisasContext *)base;
    if(!((bp->flags & BP_CPU) && (dc->base.tb->flags & HF_RF_MASK))) {
        gen_debug(dc, dc->base.pc - dc->cs_base);
        return 1;
    }
    return 0;
}

/* generate intermediate code in gen_opc_buf and gen_opparam_buf for
   basic block 'tb'. Also generate PC information for each
   intermediate instruction. */
int gen_intermediate_code(CPUState *env, DisasContextBase *base)
{
    DisasContext *dc = (DisasContext *)base;

    tcg_gen_insn_start(base->pc, dc->cc_op);

    base->tb->size += disas_insn(env, (DisasContext *)base);

    /* if irq were inhibited with HF_INHIBIT_IRQ_MASK, we clear
       the flag and abort the translation to give the irqs a
       change to be happen */
    if(dc->tf || (dc->flags & HF_INHIBIT_IRQ_MASK)) {
        return 0;
    }
    if((base->pc & (TARGET_PAGE_SIZE - 1)) == 0) {
        return 0;
    }
    return 1;
}

uint32_t gen_intermediate_code_epilogue(CPUState *env, DisasContextBase *base)
{
    DisasContext *dc = (DisasContext *)base;
    gen_jmp_im(dc->base.pc - dc->cs_base);
    gen_eob(dc);
#ifdef TARGET_X86_64
    if(dc->code64) {
        return 2;
    }
#endif
    return !(dc->code32);
}

void restore_state_to_opc(CPUState *env, TranslationBlock *tb, target_ulong *data)
{
    int cc_op = data[1];
    env->eip = data[0] - tb->cs_base;
    if(cc_op != CC_OP_DYNAMIC) {
        env->cc_op = cc_op;
    }
}

void cpu_exec_prologue(CPUState *env)
{
    /* put eflags in CPU temporary format */
    CC_SRC = env->eflags & (CC_O | CC_S | CC_Z | CC_A | CC_P | CC_C);
    DF = 1 - (2 * ((env->eflags >> 10) & 1));
    CC_OP = CC_OP_EFLAGS;
    env->eflags &= ~(DF_MASK | CC_O | CC_S | CC_Z | CC_A | CC_P | CC_C);
}

void cpu_exec_epilogue(CPUState *env)
{
    /* restore flags in standard format */
    env->eflags = env->eflags | cpu_cc_compute_all(env, CC_OP) | (DF & DF_MASK);
}

int process_interrupt(int interrupt_request, CPUState *env)
{
    if(tlib_is_in_debug_mode()) {
        return 0;
    }

    if(interrupt_request & CPU_INTERRUPT_INIT) {
        svm_check_intercept(env, SVM_EXIT_INIT);
        do_cpu_init(env);
        env->exception_index = EXCP_WFI;
        cpu_loop_exit(env);
    } else if(interrupt_request & CPU_INTERRUPT_SIPI) {
        do_cpu_sipi(env);
    } else if(env->hflags2 & HF2_GIF_MASK) {
        if((interrupt_request & CPU_INTERRUPT_SMI) && !(env->hflags & HF_SMM_MASK)) {
            svm_check_intercept(env, SVM_EXIT_SMI);
            clear_interrupt_pending(env, CPU_INTERRUPT_SMI);
            do_smm_enter(env);
            return 1;
        } else if((interrupt_request & CPU_INTERRUPT_NMI) && !(env->hflags2 & HF2_NMI_MASK)) {
            clear_interrupt_pending(env, CPU_INTERRUPT_NMI);
            env->hflags2 |= HF2_NMI_MASK;
            do_interrupt_x86_hardirq(env, EXCP02_NMI, 1);
            return 1;
        } else if(interrupt_request & CPU_INTERRUPT_MCE) {
            clear_interrupt_pending(env, CPU_INTERRUPT_MCE);
            do_interrupt_x86_hardirq(env, EXCP12_MCHK, 0);
            return 1;
        } else if((interrupt_request & CPU_INTERRUPT_HARD) &&
                  (((env->hflags2 & HF2_VINTR_MASK) && (env->hflags2 & HF2_HIF_MASK)) ||
                   (!(env->hflags2 & HF2_VINTR_MASK) && (env->eflags & IF_MASK && !(env->hflags & HF_INHIBIT_IRQ_MASK))))) {
            int intno;
            svm_check_intercept(env, SVM_EXIT_INTR);
            clear_interrupt_pending(env, CPU_INTERRUPT_HARD | CPU_INTERRUPT_VIRQ);
            intno = cpu_get_pic_interrupt(env);
            do_interrupt_x86_hardirq(env, intno, 1);
            /* ensure that no TB jump will be modified as
               the program flow was changed */
            return 1;
        } else if((interrupt_request & CPU_INTERRUPT_VIRQ) && (env->eflags & IF_MASK) && !(env->hflags & HF_INHIBIT_IRQ_MASK)) {
            int intno;
            /* FIXME: this should respect TPR */
            svm_check_intercept(env, SVM_EXIT_VINTR);
            intno = ldl_phys(env->vm_vmcb + offsetof(struct vmcb, control.int_vector));
            do_interrupt_x86_hardirq(env, intno, 1);
            clear_interrupt_pending(env, CPU_INTERRUPT_VIRQ);
            return 1;
        }
    }
    return 0;
}

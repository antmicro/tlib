/*
 *  ARM helper routines
 *
 *  Copyright (c) 2005-2007 CodeSourcery, LLC
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
#include "softmmu_exec.h"
#include "helper.h"

#define SIGNBIT   (uint32_t)0x80000000
#define SIGNBIT64 ((uint64_t)1 << 63)

uint32_t HELPER(neon_tbl)(uint32_t ireg, uint32_t def, uint32_t rn, uint32_t maxindex)
{
    uint32_t val;
    uint32_t tmp;
    int index;
    int shift;
    uint64_t *table;
    table = (uint64_t *)&env->vfp.regs[rn];
    val = 0;
    for(shift = 0; shift < 32; shift += 8) {
        index = (ireg >> shift) & 0xff;
        if(index < maxindex) {
            tmp = (table[index >> 3] >> ((index & 7) << 3)) & 0xff;
            val |= tmp << shift;
        } else {
            val |= def & (0xff << shift);
        }
    }
    return val;
}

/* try to fill the TLB and return an exception if error. If retaddr is
   NULL, it means that the function was called in C code (i.e. not
   from generated code or from helper.c) */
/* XXX: fix it to restore all registers */
int tlb_fill(CPUState *env1, target_ulong addr, int access_type, int mmu_idx, void *retaddr, int no_page_fault, int access_width)
{
    CPUState *saved_env;
    int ret;

    saved_env = env;
    env = env1;
    ret = cpu_handle_mmu_fault(env, addr, access_type, mmu_idx, no_page_fault);
    if(unlikely(ret == TRANSLATE_FAIL && !no_page_fault)) {
        //  access_type == CODE ACCESS - do not fire block_end hooks!
        cpu_loop_exit_restore(env, (uintptr_t)retaddr, access_type != ACCESS_INST_FETCH);
    }
    env = saved_env;
    return ret;
}

/* FIXME: Pass an axplicit pointer to QF to CPUState, and move saturating
   instructions into helper.c  */
uint32_t HELPER(add_setq)(uint32_t a, uint32_t b)
{
    uint32_t res = a + b;
    if(((res ^ a) & SIGNBIT) && !((a ^ b) & SIGNBIT)) {
        env->QF = 1;
    }
    return res;
}

uint32_t HELPER(add_saturate)(uint32_t a, uint32_t b)
{
    uint32_t res = a + b;
    if(((res ^ a) & SIGNBIT) && !((a ^ b) & SIGNBIT)) {
        env->QF = 1;
        res = ~(((int32_t)a >> 31) ^ SIGNBIT);
    }
    return res;
}

uint32_t HELPER(sub_saturate)(uint32_t a, uint32_t b)
{
    uint32_t res = a - b;
    if(((res ^ a) & SIGNBIT) && ((a ^ b) & SIGNBIT)) {
        env->QF = 1;
        res = ~(((int32_t)a >> 31) ^ SIGNBIT);
    }
    return res;
}

uint32_t HELPER(double_saturate)(int32_t val)
{
    uint32_t res;
    if(val >= 0x40000000) {
        res = ~SIGNBIT;
        env->QF = 1;
    } else if(val <= (int32_t)0xc0000000) {
        res = SIGNBIT;
        env->QF = 1;
    } else {
        res = val << 1;
    }
    return res;
}

uint32_t HELPER(add_usaturate)(uint32_t a, uint32_t b)
{
    uint32_t res = a + b;
    if(res < a) {
        env->QF = 1;
        res = ~0;
    }
    return res;
}

uint32_t HELPER(sub_usaturate)(uint32_t a, uint32_t b)
{
    uint32_t res = a - b;
    if(res > a) {
        env->QF = 1;
        res = 0;
    }
    return res;
}

/* Signed saturation.  */
static inline uint32_t do_ssat(int32_t val, int shift)
{
    int32_t top;
    uint32_t mask;

    top = val >> shift;
    mask = (1u << shift) - 1;
    if(top > 0) {
        env->QF = 1;
        return mask;
    } else if(top < -1) {
        env->QF = 1;
        return ~mask;
    }
    return val;
}

/* Unsigned saturation.  */
static inline uint32_t do_usat(int32_t val, int shift)
{
    uint32_t max;

    max = (1u << shift) - 1;
    if(val < 0) {
        env->QF = 1;
        return 0;
    } else if(val > max) {
        env->QF = 1;
        return max;
    }
    return val;
}

/* Signed saturate.  */
uint32_t HELPER(ssat)(uint32_t x, uint32_t shift)
{
    return do_ssat(x, shift);
}

/* Dual halfword signed saturate.  */
uint32_t HELPER(ssat16)(uint32_t x, uint32_t shift)
{
    uint32_t res;

    res = (uint16_t)do_ssat((int16_t)x, shift);
    res |= do_ssat(((int32_t)x) >> 16, shift) << 16;
    return res;
}

/* Unsigned saturate.  */
uint32_t HELPER(usat)(uint32_t x, uint32_t shift)
{
    return do_usat(x, shift);
}

/* Dual halfword unsigned saturate.  */
uint32_t HELPER(usat16)(uint32_t x, uint32_t shift)
{
    uint32_t res;

    res = (uint16_t)do_usat((int16_t)x, shift);
    res |= do_usat(((int32_t)x) >> 16, shift) << 16;
    return res;
}

void HELPER(wfi)(void)
{
    env->exception_index = EXCP_WFI;
    env->wfi = 1;
}

void HELPER(wfe)(void)
{
    env->exception_index = EXCP_WFI;
    env->wfe = 1;
}

void HELPER(exception)(uint32_t excp)
{
    env->exception_index = excp;
}

uint32_t HELPER(cpsr_read)(void)
{
    return cpsr_read(env) & ~CPSR_EXEC;
}

void HELPER(cpsr_write)(uint32_t val, uint32_t mask)
{
    cpsr_write(env, val, mask);
}

/* Access to user mode registers from privileged modes.  */
uint32_t HELPER(get_user_reg)(uint32_t regno)
{
    uint32_t val;
    /*
     * We need to know the current mode, to update registers immediately
     * if we are in the correct mode already (User or System).
     * Just storing values in banks means nothing if the changes aren't propagated
     */
    int mode = cpu_get_current_execution_mode();

    if(regno == 13) {
        val = env->banked_r13[0];
        if(mode == ARM_CPU_MODE_USR || mode == ARM_CPU_MODE_SYS) {
            env->regs[13] = env->banked_r13[0];
        }
    } else if(regno == 14) {
        val = env->banked_r14[0];
        if(mode == ARM_CPU_MODE_USR || mode == ARM_CPU_MODE_SYS) {
            env->regs[14] = env->banked_r14[0];
        }
    } else if(regno >= 8 && (env->uncached_cpsr & 0x1f) == ARM_CPU_MODE_FIQ) {
        val = env->usr_regs[regno - 8];
        if(mode == ARM_CPU_MODE_USR || mode == ARM_CPU_MODE_SYS) {
            env->regs[regno] = env->usr_regs[regno - 8];
        }
    } else {
        val = env->regs[regno];
    }
    return val;
}

void HELPER(set_user_reg)(uint32_t regno, uint32_t val)
{
    if(regno == 13) {
        env->banked_r13[0] = val;
    } else if(regno == 14) {
        env->banked_r14[0] = val;
    } else if(regno >= 8 && (env->uncached_cpsr & 0x1f) == ARM_CPU_MODE_FIQ) {
        env->usr_regs[regno - 8] = val;
    } else {
        env->regs[regno] = val;
    }
}

/* ??? Flag setting arithmetic is awkward because we need to do comparisons.
   The only way to do that in TCG is a conditional branch, which clobbers
   all our temporaries.  For now implement these as helper functions.  */

uint32_t HELPER(add_cc)(uint32_t a, uint32_t b)
{
    uint32_t result;
    result = a + b;
    env->NF = env->ZF = result;
    env->CF = result < a;
    env->VF = (a ^ b ^ -1) & (a ^ result);
    return result;
}

uint32_t HELPER(adc_cc)(uint32_t a, uint32_t b)
{
    uint32_t result;
    if(!env->CF) {
        result = a + b;
        env->CF = result < a;
    } else {
        result = a + b + 1;
        env->CF = result <= a;
    }
    env->VF = (a ^ b ^ -1) & (a ^ result);
    env->NF = env->ZF = result;
    return result;
}

uint32_t HELPER(sub_cc)(uint32_t a, uint32_t b)
{
    uint32_t result;
    result = a - b;
    env->NF = env->ZF = result;
    env->CF = a >= b;
    env->VF = (a ^ b) & (a ^ result);
    return result;
}

uint32_t HELPER(sbc_cc)(uint32_t a, uint32_t b)
{
    uint32_t result;
    if(!env->CF) {
        result = a - b - 1;
        env->CF = a > b;
    } else {
        result = a - b;
        env->CF = a >= b;
    }
    env->VF = (a ^ b) & (a ^ result);
    env->NF = env->ZF = result;
    return result;
}

/* Similarly for variable shift instructions.  */

uint32_t HELPER(shl)(uint32_t x, uint32_t i)
{
    int shift = i & 0xff;
    if(shift >= 32) {
        return 0;
    }
    return x << shift;
}

uint32_t HELPER(shr)(uint32_t x, uint32_t i)
{
    int shift = i & 0xff;
    if(shift >= 32) {
        return 0;
    }
    return (uint32_t)x >> shift;
}

uint32_t HELPER(sar)(uint32_t x, uint32_t i)
{
    int shift = i & 0xff;
    if(shift >= 32) {
        shift = 31;
    }
    return (int32_t)x >> shift;
}

uint32_t HELPER(shl_cc)(uint32_t x, uint32_t i)
{
    int shift = i & 0xff;
    if(shift >= 32) {
        if(shift == 32) {
            env->CF = x & 1;
        } else {
            env->CF = 0;
        }
        return 0;
    } else if(shift != 0) {
        env->CF = (x >> (32 - shift)) & 1;
        return x << shift;
    }
    return x;
}

uint32_t HELPER(shr_cc)(uint32_t x, uint32_t i)
{
    int shift = i & 0xff;
    if(shift >= 32) {
        if(shift == 32) {
            env->CF = (x >> 31) & 1;
        } else {
            env->CF = 0;
        }
        return 0;
    } else if(shift != 0) {
        env->CF = (x >> (shift - 1)) & 1;
        return x >> shift;
    }
    return x;
}

uint32_t HELPER(sar_cc)(uint32_t x, uint32_t i)
{
    int shift = i & 0xff;
    if(shift >= 32) {
        env->CF = (x >> 31) & 1;
        return (int32_t)x >> 31;
    } else if(shift != 0) {
        env->CF = (x >> (shift - 1)) & 1;
        return (int32_t)x >> shift;
    }
    return x;
}

uint32_t HELPER(ror_cc)(uint32_t x, uint32_t i)
{
    int shift1, shift;
    shift1 = i & 0xff;
    shift = shift1 & 0x1f;
    if(shift == 0) {
        if(shift1 != 0) {
            env->CF = (x >> 31) & 1;
        }
        return x;
    } else {
        env->CF = (x >> (shift - 1)) & 1;
        return ((uint32_t)x >> shift) | (x << (32 - shift));
    }
}

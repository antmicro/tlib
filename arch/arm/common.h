/*
 * ARM common
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

#pragma once

#include "cpu.h"

static inline TCGv load_cpu_offset(int offset)
{
    TCGv tmp = tcg_temp_local_new_i32();
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

static inline TCGv gen_ld32(TCGv addr, int index)
{
    TCGv tmp = tcg_temp_local_new_i32();
    tcg_gen_qemu_ld32u(tmp, addr, index);
    return tmp;
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

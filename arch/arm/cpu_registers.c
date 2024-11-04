/*
 *  ARM registers interface.
 *
 *  Copyright (c) Antmicro
 *  Copyright (c) Realtime Embedded
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
#include <stdint.h>

#include "cpu.h"
#include "cpu_registers.h"
#include "unwind.h"

#ifdef TARGET_ARM64
uint64_t *get_reg_pointer_64(int reg)
{
    switch(reg) {
        //  64-bit regs:
        case X_0_64 ... X_31_64:
            return &(cpu->xregs[reg - X_0_64]);
        case PC_64:
            return &(cpu->pc);
        default:
            return NULL;
    }
}

CPU_REGISTER_ACCESSOR(64)
#endif
#if defined(TARGET_ARM32) || defined(TARGET_ARM64)
uint32_t *get_reg_pointer_32(int reg)
{
    switch(reg) {
        case R_0_32 ... R_15_32:
            return &(cpu->regs[reg]);
        case CPSR_32:
            return &(cpu->uncached_cpsr);
#if defined(TARGET_ARM32) && defined(TARGET_PROTO_ARM_M)
        case Control_32:
            return &(cpu->v7m.control[cpu->secure]);
        case BasePri_32:
            return &(cpu->v7m.basepri[cpu->secure]);
        case VecBase_32:
            return &(cpu->v7m.vecbase);
        case CurrentSP_32:
            return &(cpu->v7m.process_sp);
        case OtherSP_32:
            return &(cpu->v7m.other_sp);
        case FPCCR_32:
            return &(cpu->v7m.fpccr);
        case FPCAR_32:
            return &(cpu->v7m.fpcar);
        case FPDSCR_32:
            return &(cpu->v7m.fpdscr);
        case CPACR_32:
            return &(cpu->v7m.cpacr);
        case PRIMASK_32:
            return &(cpu->v7m.primask[cpu->secure]);
        case FAULTMASK_32:
            return &(cpu->v7m.faultmask[cpu->secure]);
#endif
        default:
            return NULL;
    }
}

uint32_t tlib_get_register_value_32(int reg_number)
{
    if(reg_number == CPSR_32) {
#if defined(TARGET_ARM32) && defined(TARGET_PROTO_ARM_M)
        return xpsr_read(cpu);
#else
        return cpsr_read(cpu);
#endif
    }
#ifdef TARGET_PROTO_ARM_M
    else if(reg_number == PRIMASK_32) {
        //  PRIMASK: b0: IRQ mask enabled/disabled, b1-b31: reserved.
        return cpu->v7m.primask[cpu->secure] & PRIMASK_EN ? 1 : 0;
    }
#endif

    uint32_t *ptr = get_reg_pointer_32(reg_number);
    if(ptr == NULL) {
        tlib_abortf("Read from undefined CPU register number %d detected", reg_number);
    }

    return *ptr;
}

EXC_INT_1(uint32_t, tlib_get_register_value_32, int, reg_number)

void tlib_set_register_value_32(int reg_number, uint32_t value)
{
    if(reg_number == CPSR_32) {
#if defined(TARGET_ARM32) && defined(TARGET_PROTO_ARM_M)
        xpsr_write(cpu, value, 0xffffffff);
#else
        cpsr_write(cpu, value, 0xffffffff);
#endif
        return;
    }
#ifdef TARGET_PROTO_ARM_M
    else if(reg_number == PRIMASK_32) {
        cpu->v7m.primask[cpu->secure] &= !PRIMASK_EN;
        //  PRIMASK: b0: IRQ mask enabled/disabled, b1-b31: reserved.
        if(value == 1) {
            cpu->v7m.primask[cpu->secure] |= PRIMASK_EN;
            tlib_nvic_find_pending_irq();
        }
        return;
    }
#endif

    uint32_t *ptr = get_reg_pointer_32(reg_number);
    if(ptr == NULL) {
        tlib_abortf("Write to undefined CPU register number %d detected", reg_number);
    }

    *ptr = value;
}

EXC_VOID_2(tlib_set_register_value_32, int, reg_number, uint32_t, value)
#endif

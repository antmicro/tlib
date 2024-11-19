/*
 *  ARM interface functions.
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
#include "system_registers.h"
#include "unwind.h"
#include "tightly_coupled_memory.h"
#include "bit_helper.h"
#include "host-utils.h"

//  This is an id representing processor's model (not registration id, and neither SMP id)
uint32_t tlib_get_cpu_model_id()
{
    return cpu->cp15.c0_cpuid;
}

EXC_INT_0(uint32_t, tlib_get_cpu_model_id)

uint32_t tlib_get_it_state()
{
    return cpu->condexec_bits;
}

EXC_INT_0(uint32_t, tlib_get_it_state)

uint32_t tlib_evaluate_condition_code(uint32_t condition)
{
    uint8_t ZF = (env->ZF == 0);
    uint8_t NF = (env->NF & 0x80000000) > 0;
    uint8_t CF = (env->CF == 1);
    uint8_t VF = (env->VF & 0x80000000) > 0;
    switch(condition) {
        case 0b0000:  //  EQ
            return ZF == 1;
        case 0b0001:  //  NE
            return ZF == 0;
        case 0b0010:  //  CS
            return CF == 1;
        case 0b0011:  //  CC
            return CF == 0;
        case 0b0100:  //  MI
            return NF == 1;
        case 0b0101:  //  PL
            return NF == 0;
        case 0b0110:  //  VS
            return VF == 1;
        case 0b0111:  //  VC
            return VF == 0;
        case 0b1000:  //  HI
            return CF == 1 && ZF == 0;
        case 0b1001:  //  LS
            return CF == 0 || ZF == 1;
        case 0b1010:  //  GE
            return NF == VF;
        case 0b1011:  //  LT
            return NF != VF;
        case 0b1100:  //  GT
            return ZF == 0 && NF == VF;
        case 0b1101:  //  LE
            return ZF == 1 || NF != VF;
        case 0b1110:  //  AL
            return 1;
        case 0b1111:  //  NV
            return 0;
        default:
            tlib_printf(LOG_LEVEL_ERROR, "trying to evaluate incorrect condition code (0x%x)", condition);
            return 0;
    }
}

EXC_INT_1(uint32_t, tlib_evaluate_condition_code, uint32_t, condition)

void tlib_set_cpu_model_id(uint32_t value)
{
    cpu->cp15.c0_cpuid = value;
}

EXC_VOID_1(tlib_set_cpu_model_id, uint32_t, value)

void tlib_toggle_fpu(int32_t enabled)
{
    if(enabled) {
        cpu->vfp.xregs[ARM_VFP_FPEXC] |= ARM_VFP_FPEXC_FPUEN_MASK;
    } else {
        cpu->vfp.xregs[ARM_VFP_FPEXC] &= ~ARM_VFP_FPEXC_FPUEN_MASK;
    }
}

EXC_VOID_1(tlib_toggle_fpu, int32_t, enabled)

void tlib_set_sev_on_pending(int32_t value)
{
    cpu->sev_on_pending = !!value;
}

EXC_VOID_1(tlib_set_sev_on_pending, int32_t, value)

void tlib_set_event_flag(int value)
{
    cpu->sev_pending = !!value;
}

EXC_VOID_1(tlib_set_event_flag, int, value)

void tlib_set_thumb(int value)
{
    cpu->thumb = value != 0;
}

EXC_VOID_1(tlib_set_thumb, int, value)

void tlib_set_number_of_mpu_regions(uint32_t value)
{
    if(value >= MAX_MPU_REGIONS) {
        tlib_abortf("Failed to set number of unified MPU regions to %u, maximal supported value is %u", value, MAX_MPU_REGIONS);
    }
    cpu->number_of_mpu_regions = value;
}

EXC_VOID_1(tlib_set_number_of_mpu_regions, uint32_t, value)

uint32_t tlib_get_number_of_mpu_regions()
{
    return cpu->number_of_mpu_regions;
}

EXC_INT_0(uint32_t, tlib_get_number_of_mpu_regions)

void tlib_register_tcm_region(uint32_t address, uint64_t size, uint64_t index)
{
    //  interface index is the opc2 value when addressing region register via MRC/MCR
    //  region index is the selection register value
    uint32_t interface_index = index >> 32;
    uint32_t region_index = index;
    if(interface_index >= 2) {
        tlib_abortf("Attempted to register TCM region for interface #%u. Only 2 TCM interfaces are supported", interface_index);
    }
    if(size == 0) {
        cpu->cp15.c9_tcmregion[interface_index][region_index] = 0;
        return;
    }

    validate_tcm_region(address, size, region_index, TARGET_PAGE_SIZE);

    cpu->cp15.c9_tcmregion[interface_index][region_index] = address | (ctz64(size / TCM_UNIT_SIZE) << 2) | 1;  //  always enable
}

EXC_VOID_3(tlib_register_tcm_region, uint32_t, address, uint64_t, size, uint64_t, index)

void tlib_update_pmu_counters(int event_id, uint32_t amount)
{
    helper_pmu_update_event_counters(cpu, event_id, amount);
}

EXC_VOID_2(tlib_update_pmu_counters, int, event_id, uint32_t, amount);

void tlib_pmu_set_debug(uint32_t debug)
{
    env->pmu.extra_logs_enabled = debug != 0;
}

EXC_VOID_1(tlib_pmu_set_debug, uint32_t, debug)

uint32_t tlib_get_exception_vector_address(void)
{
    return cpu->cp15.c12_vbar;
}
EXC_INT_0(uint32_t, tlib_get_exception_vector_address)

void tlib_set_exception_vector_address(uint32_t address)
{
    cpu->cp15.c12_vbar = address;
}
EXC_VOID_1(tlib_set_exception_vector_address, uint32_t, address)

#ifdef TARGET_PROTO_ARM_M

void tlib_set_security_state(uint32_t state)
{
    /* Update information about TrustZone support here
     * this is required to turn TZ support on in the core and is a bit hackish
     * since we don't have this information available during first cpu_init
     */
    cpu->v7m.has_trustzone = tlib_has_enabled_trustzone() > 0;

    if(!arm_feature(env, ARM_FEATURE_V8)) {
        if(env->v7m.has_trustzone) {
            cpu_abort(env, "TrustZone enabled for M-Architecture different than V8 is not supported");
        }
    }
    if(!cpu->v7m.has_trustzone) {
        tlib_abort("Changing Security State for CPU with disabled TrustZone");
    }
    cpu->secure = state;
}
EXC_VOID_1(tlib_set_security_state, uint32_t, state)

uint32_t tlib_get_security_state()
{
    if(!cpu->v7m.has_trustzone) {
        tlib_printf(LOG_LEVEL_WARNING, "This CPU has TrustZone disabled, so its security state is bogus");
    }
    return cpu->secure;
}
EXC_INT_0(uint32_t, tlib_get_security_state)

void tlib_set_sleep_on_exception_exit(int32_t value)
{
    cpu->sleep_on_exception_exit = !!value;
}

EXC_VOID_1(tlib_set_sleep_on_exception_exit, int32_t, value)

void tlib_set_interrupt_vector_base(uint32_t address, bool secure)
{
    cpu->v7m.vecbase[secure] = address;
}

EXC_VOID_2(tlib_set_interrupt_vector_base, uint32_t, address, bool, secure)

uint32_t tlib_get_interrupt_vector_base(bool secure)
{
    return cpu->v7m.vecbase[secure];
}

EXC_INT_1(uint32_t, tlib_get_interrupt_vector_base, bool, secure)

uint32_t tlib_get_xpsr()
{
    return xpsr_read(cpu);
}

EXC_INT_0(uint32_t, tlib_get_xpsr)

uint32_t tlib_get_fault_status()
{
    return cpu->v7m.fault_status;
}

EXC_INT_0(uint32_t, tlib_get_fault_status)

void tlib_set_fault_status(uint32_t value)
{
    cpu->v7m.fault_status = value;
}

EXC_VOID_1(tlib_set_fault_status, uint32_t, value)

uint32_t tlib_get_memory_fault_address()
{
    return cpu->cp15.c6_data;
}

EXC_INT_0(uint32_t, tlib_get_memory_fault_address)

uint32_t tlib_is_mpu_enabled()
{
    return cpu->cp15.c1_sys & 0x1;
}

EXC_INT_0(uint32_t, tlib_is_mpu_enabled)

void tlib_enable_mpu(int32_t enabled)
{
    if(!!enabled != (cpu->cp15.c1_sys & 1)) {
        cpu->cp15.c1_sys ^= 1;
        tlb_flush(cpu, 1, false);
    }
}

EXC_VOID_1(tlib_enable_mpu, int32_t, enabled)

void tlib_set_mpu_region_number(uint32_t value)
{
    if(value >= cpu->number_of_mpu_regions) {
        tlib_abortf("MPU: Trying to use non-existent MPU region. Number of regions: %d, faulting region number: %d",
                    cpu->number_of_mpu_regions, value);
    }
    cpu->cp15.c6_region_number = value;
    tlb_flush(cpu, 1, false);
}

EXC_VOID_1(tlib_set_mpu_region_number, uint32_t, value)

//  This function mimics mpu configuration through the "Region Base Address" register
void tlib_set_mpu_region_base_address(uint32_t value)
{
    if(value & 0x10) {
        /* If VALID (0x10) bit is set, we change the region number to zero-extended value of youngest 4 bits */
        tlib_set_mpu_region_number(value & 0xF);
    }
    cpu->cp15.c6_base_address[cpu->cp15.c6_region_number] = value & 0xFFFFFFE0;
#if DEBUG
    tlib_printf(LOG_LEVEL_DEBUG, "MPU: Set base address 0x%x, for region %lld", value & 0xFFFFFFE0, cpu->cp15.c6_region_number);
#endif
    tlb_flush(cpu, 1, false);
}

EXC_VOID_1(tlib_set_mpu_region_base_address, uint32_t, value)

//  This function mimics mpu configuration through the "Region Attribute and Size" register
void tlib_set_mpu_region_size_and_enable(uint32_t value)
{
    uint32_t index = cpu->cp15.c6_region_number;
    cpu->cp15.c6_size_and_enable[index] = value & MPU_SIZE_AND_ENABLE_FIELD_MASK;
    cpu->cp15.c6_subregion_disable[index] = (value & MPU_SUBREGION_DISABLE_FIELD_MASK) >> MPU_SUBREGION_DISABLE_FIELD_OFFSET;
    cpu->cp15.c6_access_control[index] = value >> 16;
#if DEBUG
    tlib_printf(LOG_LEVEL_DEBUG, "MPU: Set access control 0x%x, permissions 0x%x, size 0x%x, enable 0x%x, for region %lld",
                value >> 16, ((value >> 16) & MPU_PERMISSION_FIELD_MASK) >> 8, (value & MPU_SIZE_FIELD_MASK) >> 1,
                value & MPU_REGION_ENABLED_BIT, index);
#endif
    tlb_flush(cpu, 1, false);
}

EXC_VOID_1(tlib_set_mpu_region_size_and_enable, uint32_t, value)

//  This function mimics mpu configuration through the "Region Base Address" register
uint32_t tlib_get_mpu_region_base_address()
{
    return cpu->cp15.c6_base_address[cpu->cp15.c6_region_number] | cpu->cp15.c6_region_number;
}

EXC_INT_0(uint32_t, tlib_get_mpu_region_base_address)

//  This function mimics mpu configuration through the "Region Attribute and Size" register
uint32_t tlib_get_mpu_region_size_and_enable()
{
    uint32_t index = cpu->cp15.c6_region_number;
    return (cpu->cp15.c6_access_control[index] << 16) | (cpu->cp15.c6_subregion_disable[index] << 8) |
           cpu->cp15.c6_size_and_enable[index];
}

EXC_INT_0(uint32_t, tlib_get_mpu_region_size_and_enable)

uint32_t tlib_get_mpu_region_number()
{
    return cpu->cp15.c6_region_number;
}

EXC_INT_0(uint32_t, tlib_get_mpu_region_number)

void tlib_set_number_of_sau_regions(uint32_t value)
{
    if(value >= MAX_SAU_REGIONS) {
        tlib_abortf("Failed to set number of SAU regions to %u, maximal supported value is %u", value, MAX_SAU_REGIONS);
    }

    if(!cpu->v7m.has_trustzone) {
        tlib_printf(LOG_LEVEL_WARNING, "Setting SAU regions to %u, but TrustZone is not enabled", value);
    }

    cpu->number_of_sau_regions = value;
}

EXC_VOID_1(tlib_set_number_of_sau_regions, uint32_t, value)

uint32_t tlib_get_number_of_sau_regions()
{
    return cpu->number_of_sau_regions;
}

EXC_INT_0(uint32_t, tlib_get_number_of_sau_regions)

void tlib_set_sau_region_number(uint32_t value)
{
    if(!cpu->secure) {
        //  These are RAZ/WI when accessed from Non-Secure state
        return;
    }
    if(value >= cpu->number_of_sau_regions) {
        tlib_abortf("SAU: Trying to use non-existent SAU region. Number of regions: %d, faulting region number: %d",
                    cpu->number_of_sau_regions, value);
    }
    cpu->sau.rnr = value;
}

EXC_VOID_1(tlib_set_sau_region_number, uint32_t, value)

void tlib_set_sau_region_base_address(uint32_t value)
{
    if(!cpu->secure) {
        //  These are RAZ/WI when accessed from Non-Secure state
        return;
    }
    cpu->sau.rbar[cpu->sau.rnr] = value;
    tlb_flush(cpu, 1, false);
}

EXC_VOID_1(tlib_set_sau_region_base_address, uint32_t, value)

void tlib_set_sau_region_limit_address(uint32_t value)
{
    if(!cpu->secure) {
        //  These are RAZ/WI when accessed from Non-Secure state
        return;
    }
    cpu->sau.rlar[cpu->sau.rnr] = value;
    tlb_flush(cpu, 1, false);
}

EXC_VOID_1(tlib_set_sau_region_limit_address, uint32_t, value)

uint32_t tlib_get_sau_region_number()
{
    if(!cpu->secure) {
        //  These are RAZ/WI when accessed from Non-Secure state
        return 0;
    }
    return cpu->sau.rnr;
}

EXC_INT_0(uint32_t, tlib_get_sau_region_number)

uint32_t tlib_get_sau_region_base_address()
{
    if(!cpu->secure) {
        //  These are RAZ/WI when accessed from Non-Secure state
        return 0;
    }
    uint32_t result = cpu->sau.rbar[cpu->sau.rnr];
    return result;
}

EXC_INT_0(uint32_t, tlib_get_sau_region_base_address)

uint32_t tlib_get_sau_region_limit_address()
{
    if(!cpu->secure) {
        //  These are RAZ/WI when accessed from Non-Secure state
        return 0;
    }
    uint32_t result = cpu->sau.rlar[cpu->sau.rnr];
    return result;
}

EXC_INT_0(uint32_t, tlib_get_sau_region_limit_address)

/* See vfp_trigger_exception for irq_number value interpretation */
void tlib_set_fpu_interrupt_number(int32_t irq_number)
{
    cpu->vfp.fpu_interrupt_irq_number = irq_number;
}

EXC_VOID_1(tlib_set_fpu_interrupt_number, int32_t, irq_number)

uint32_t tlib_is_v8()
{
    return arm_feature(env, ARM_FEATURE_V8);
}
EXC_INT_0(uint32_t, tlib_is_v8)

/* PMSAv8 */

static void guard_pmsav8(bool is_write)
{
    if(!arm_feature(env, ARM_FEATURE_V8)) {
        tlib_abort("This feature is only supported on ARM v8-M architecture");
    }
    if(is_write) {
        tlb_flush(cpu, 1, false);
    }
}

void tlib_set_pmsav8_ctrl(uint32_t value)
{
    guard_pmsav8(true);
    cpu->pmsav8[cpu->secure].ctrl = value;
}
EXC_VOID_1(tlib_set_pmsav8_ctrl, uint32_t, value)

void tlib_set_pmsav8_rnr(uint32_t value)
{
    guard_pmsav8(true);
    if(value > MAX_MPU_REGIONS) {
        tlib_printf(LOG_LEVEL_ERROR, "Requested RNR value is greater than the maximum MPU regions");
        return;
    }
    cpu->pmsav8[cpu->secure].rnr = value;
}
EXC_VOID_1(tlib_set_pmsav8_rnr, uint32_t, value)

void tlib_set_pmsav8_rbar(uint32_t value)
{
    guard_pmsav8(true);
    uint32_t index = cpu->pmsav8[cpu->secure].rnr;
    cpu->pmsav8[cpu->secure].rbar[index] = value;
}
EXC_VOID_1(tlib_set_pmsav8_rbar, uint32_t, value)

void tlib_set_pmsav8_rlar(uint32_t value)
{
    guard_pmsav8(true);
    uint32_t index = cpu->pmsav8[cpu->secure].rnr;
    cpu->pmsav8[cpu->secure].rlar[index] = value;
}
EXC_VOID_1(tlib_set_pmsav8_rlar, uint32_t, value)

void tlib_set_pmsav8_mair(uint32_t index, uint32_t value)
{
    guard_pmsav8(true);
    if(index > 1) {
        tlib_printf(LOG_LEVEL_ERROR, "Only indexes {0,1} are supported by MAIR registers");
        return;
    }
    cpu->pmsav8[cpu->secure].mair[index] = value;
}
EXC_VOID_2(tlib_set_pmsav8_mair, uint32_t, index, uint32_t, value)

uint32_t tlib_get_pmsav8_ctrl()
{
    guard_pmsav8(false);
    return cpu->pmsav8[cpu->secure].ctrl;
}
EXC_INT_0(uint32_t, tlib_get_pmsav8_ctrl)

uint32_t tlib_get_pmsav8_rnr()
{
    guard_pmsav8(false);
    return cpu->pmsav8[cpu->secure].rnr;
}
EXC_INT_0(uint32_t, tlib_get_pmsav8_rnr)

uint32_t tlib_get_pmsav8_rbar()
{
    guard_pmsav8(false);
    uint32_t index = cpu->pmsav8[cpu->secure].rnr;
    return cpu->pmsav8[cpu->secure].rbar[index];
}
EXC_INT_0(uint32_t, tlib_get_pmsav8_rbar)

uint32_t tlib_get_pmsav8_rlar()
{
    guard_pmsav8(false);
    uint32_t index = cpu->pmsav8[cpu->secure].rnr;
    return cpu->pmsav8[cpu->secure].rlar[index];
}
EXC_INT_0(uint32_t, tlib_get_pmsav8_rlar)

uint32_t tlib_get_pmsav8_mair(uint32_t index)
{
    guard_pmsav8(false);
    if(index > 1) {
        tlib_printf(LOG_LEVEL_ERROR, "Only indexes {0,1} are supported by MAIR registers");
        return 0;
    }
    return cpu->pmsav8[cpu->secure].mair[index];
}
EXC_INT_1(uint32_t, tlib_get_pmsav8_mair, uint32_t, index)

#endif

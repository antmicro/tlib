/*
 *  ARM callbacks.
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
#include "callbacks.h"
#include "arch_callbacks.h"

#ifdef TARGET_PROTO_ARM_M
int32_t tlib_nvic_acknowledge_irq(void) __attribute__((weak));

int32_t tlib_nvic_acknowledge_irq(void)
{
    return -1;
}

DEFAULT_VOID_HANDLER1(void tlib_nvic_complete_irq, int32_t number)

DEFAULT_VOID_HANDLER2(void tlib_nvic_write_basepri, int32_t number, uint32_t secure)

DEFAULT_INT_HANDLER1(int32_t tlib_nvic_find_pending_irq, void)

DEFAULT_INT_HANDLER1(int32_t tlib_nvic_get_pending_masked_irq, void)

DEFAULT_VOID_HANDLER1(void tlib_nvic_set_pending_irq, int32_t number)

DEFAULT_INT_HANDLER1(uint32_t tlib_has_enabled_trustzone, void)

DEFAULT_INT_HANDLER1(uint32_t tlib_nvic_interrupt_targets_secure, int32_t number)

#endif

DEFAULT_INT_HANDLER1(uint32_t tlib_read_cp15_32, uint32_t instruction)

DEFAULT_VOID_HANDLER2(void tlib_write_cp15_32, uint32_t instruction, uint32_t value)

DEFAULT_INT_HANDLER1(uint64_t tlib_read_cp15_64, uint32_t instruction)

DEFAULT_VOID_HANDLER2(void tlib_write_cp15_64, uint32_t instruction, uint64_t value)

DEFAULT_INT_HANDLER1(uint32_t tlib_is_wfi_as_nop, void)

DEFAULT_INT_HANDLER1(uint32_t tlib_is_wfe_and_sev_as_nop, void)

DEFAULT_INT_HANDLER1(uint32_t tlib_do_semihosting, void)

DEFAULT_VOID_HANDLER1(void tlib_set_system_event, int32_t value)

DEFAULT_VOID_HANDLER1(void tlib_report_pmu_overflow, int32_t value)

#include "configuration_signals.h"
DEFAULT_VOID_HANDLER1(void tlib_fill_configuration_signals_state, void *state_pointer)

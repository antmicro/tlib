/*
 * Copyright (c) Antmicro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "callbacks.h"
#include "arch_callbacks.h"

DEFAULT_INT_HANDLER1(uint64_t tlib_read_system_register_generic_timer_64, uint32_t offset)
DEFAULT_VOID_HANDLER2(void tlib_write_system_register_generic_timer_64, uint32_t offset, uint64_t value)

DEFAULT_INT_HANDLER1(uint32_t tlib_read_system_register_generic_timer_32, uint32_t offset)
DEFAULT_VOID_HANDLER2(void tlib_write_system_register_generic_timer_32, uint32_t offset, uint32_t value)

DEFAULT_INT_HANDLER1(uint64_t tlib_read_system_register_interrupt_cpu_interface, uint32_t offset)
DEFAULT_VOID_HANDLER2(void tlib_write_system_register_interrupt_cpu_interface, uint32_t offset, uint64_t value)

DEFAULT_VOID_HANDLER2(void tlib_on_execution_mode_changed, uint32_t el, uint32_t is_secure)

DEFAULT_VOID_HANDLER0(void tlib_handle_psci_call)

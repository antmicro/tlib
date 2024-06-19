#pragma once

#include <stdint.h>

uint64_t tlib_get_cpu_time();

uint64_t tlib_read_csr(uint64_t csr);
void tlib_write_csr(uint64_t csr, uint64_t value);
void tlib_mip_changed(uint64_t value);

int32_t tlib_handle_custom_instruction(uint64_t id, uint64_t opcode);
void tlib_handle_post_opcode_execution_hook(uint32_t id, uint64_t pc);
void tlib_handle_post_gpr_access_hook(uint32_t register_index, uint32_t isWrite);
void tlib_clic_clear_edge_interrupt(void);
void tlib_clic_acknowledge_interrupt(void);

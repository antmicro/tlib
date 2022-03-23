#ifndef EXPORTS_H_
#define EXPORTS_H_

#include <stdint.h>

uint32_t tlib_set_maximum_block_size(uint32_t size);
uint32_t tlib_get_maximum_block_size(void);

void tlib_set_cycles_per_instruction(uint32_t size);
uint32_t tlib_get_cycles_per_instruction(void);

void gen_helpers(void);

char *tlib_get_arch();

int32_t tlib_init(char *cpu_name);
void tlib_atomic_memory_state_init(int id, uintptr_t atomic_memory_state_ptr);
void tlib_dispose(void);
int32_t tlib_get_executed_instructions(void);
void tlib_reset_executed_instrucions(uint64_t val);
void tlib_reset(void);
int32_t tlib_execute(int32_t max_insns);
void tlib_restart_translation_block(void);
void  tlib_set_return_request(void);
void tlib_set_paused(void);
void tlib_clear_paused(void);
int32_t tlib_is_wfi(void);

uint32_t tlib_get_page_size(void);
void tlib_map_range(uint64_t start_addr, uint64_t length);
void tlib_unmap_range(uint64_t start, uint64_t end);
uint32_t tlib_is_range_mapped(uint64_t start, uint64_t end);

void tlib_invalidate_translation_blocks(uintptr_t start, uintptr_t end);

uint64_t tlib_translate_to_physical_address(uint64_t address, uint32_t access_type);

void tlib_set_irq(int32_t interrupt, int32_t state);
int32_t tlib_is_irq_set(void);

void tlib_add_breakpoint(uint64_t address);
void tlib_remove_breakpoint(uint64_t address);
void tlib_set_block_begin_hook_present(uint32_t val);

uint64_t tlib_get_total_executed_instructions(void);

void tlib_set_translation_cache_size(uintptr_t size);
void tlib_invalidate_translation_cache(void);

int tlib_restore_context(void);
void *tlib_export_state(void);
int32_t tlib_get_state_size(void);

void tlib_set_chaining_enabled(uint32_t val);
uint32_t tlib_get_chaining_enabled(void);

void tlib_set_tb_cache_enabled(uint32_t val);
uint32_t tlib_get_tb_cache_enabled(void);

void tlib_set_block_finished_hook_present(uint32_t val);

int32_t tlib_set_return_on_exception(int32_t value);
void tlib_flush_page(uint64_t address);

uint64_t tlib_get_register_value(int reg_number);
void tlib_set_register_value(int reg_number, uint64_t val);

void tlib_set_event_flag(int value);

uint32_t tlib_get_current_tb_disas_flags(void);

uint32_t tlib_get_mmu_windows_count(void);

void tlib_enable_external_window_mmu(uint32_t value);

int32_t tlib_acquire_mmu_window();

void tlib_set_mmu_window_start(uint32_t index, uint64_t addr_start);

void tlib_set_mmu_window_end(uint32_t index, uint64_t addr_end);

void tlib_set_window_privileges(uint32_t index, int32_t privileges);

void tlib_set_mmu_window_addend(uint32_t index, uint64_t addend);

void tlib_get_mmu_window_start(uint32_t index);

void tlib_get_mmu_window_end(uint32_t index);

void tlib_get_window_privileges(uint32_t index);

void tlib_get_mmu_window_addend(uint32_t index);

#endif

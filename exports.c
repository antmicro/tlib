/*
 *  Common interface for translation libraries.
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
#include "cpu-defs.h"
#include "tcg.h"
#include "tcg-additional.h"
#include "exec-all.h"

static tcg_t stcg;

static void init_tcg()
{
    stcg.ldb = __ldb_mmu;
    stcg.ldw = __ldw_mmu;
    stcg.ldl = __ldl_mmu;
    stcg.ldq = __ldq_mmu;
    stcg.stb = __stb_mmu;
    stcg.stw = __stw_mmu;
    stcg.stl = __stl_mmu;
    stcg.stq = __stq_mmu;
    tcg_attach(&stcg);
    set_temp_buf_offset(offsetof(CPUState, temp_buf));
    int i;
    for (i = 0; i < 7; i++) {
        set_tlb_table_n_0_rwa(i, offsetof(CPUState, tlb_table[i][0].addr_read), offsetof(CPUState,
                                                                                         tlb_table[i][0].addr_write),
                              offsetof(CPUState, tlb_table[i][0].addend));
        set_tlb_table_n_0(i, offsetof(CPUState, tlb_table[i][0]));
    }
    set_tlb_entry_addr_rwu(offsetof(CPUTLBEntry, addr_read), offsetof(CPUTLBEntry, addr_write), offsetof(CPUTLBEntry, addend));
    set_sizeof_CPUTLBEntry(sizeof(CPUTLBEntry));
    set_TARGET_PAGE_BITS(TARGET_PAGE_BITS);
    attach_malloc(tlib_malloc);
    attach_realloc(tlib_realloc);
    attach_free(tlib_free);
}

char *tlib_get_arch()
{
   #ifdef TARGET_RISCV32
   return "rv32";
   #endif
   #ifdef TARGET_RISCV64
   return "rv64";
   #endif
   #ifdef TARGET_ARM
   return "arm";
   #endif
   #ifdef TARGET_I386
   return "i386";
   #endif
   #ifdef TARGET_PPC
   return "ppc";
   #endif
   #ifdef TARGET_PPC64
   return "ppc64";
   #endif
   return "unknown";
}

uint32_t maximum_block_size;

uint32_t tlib_set_maximum_block_size(uint32_t size)
{
    maximum_block_size = size;
    return maximum_block_size;
}

uint32_t tlib_get_maximum_block_size()
{
    return maximum_block_size;
}

void tlib_set_cycles_per_instruction(uint32_t count)
{
    env->cycles_per_instruction = count;
}

uint32_t tlib_get_cycles_per_instruction()
{
    return env->cycles_per_instruction;
}

void gen_helpers(void);

int32_t tlib_init(char *cpu_name)
{
    init_tcg();
    env = tlib_mallocz(sizeof(CPUState));
    cpu_exec_init(env);
    cpu_exec_init_all();
    gen_helpers();
    translate_init();
    if (cpu_init(cpu_name) != 0) {
        tlib_free(env);
        return -1;
    }
    tlib_set_maximum_block_size(10000);
    env->atomic_memory_state = NULL;
    return 0;
}

void tlib_atomic_memory_state_init(int id, uintptr_t atomic_memory_state_ptr)
{
    cpu->id = id;
    cpu->atomic_memory_state = (atomic_memory_state_t *)atomic_memory_state_ptr;
    register_in_atomic_memory_state(cpu->atomic_memory_state, id);
}

static void free_phys_dirty()
{
    if (dirty_ram.phys_dirty) {
        tlib_free(dirty_ram.phys_dirty);
    }
}

void tlib_dispose()
{
    tlib_arch_dispose();
    code_gen_free();
    free_all_page_descriptors();
    free_phys_dirty();
    tlib_free(cpu);
    tcg_dispose();
}

// this function returns number of instructions executed since the previous call
// there is `cpu->instructions_count_total_value` that contains the cumulative value
uint64_t tlib_get_executed_instructions()
{
    uint64_t result = cpu->instructions_count_value;
    cpu->instructions_count_value = 0;
    cpu->instructions_count_threshold -= result;
    return result;
}

// `TranslationCPU` uses the number of executed instructions to calculate the elapsed virtual time.
// This number is divided by `PerformanceInMIPS` value, but may leave a remainder, that is not reflected in `TranslationCPU` state.
// To account for that, we have to report this remainder back to tlib, so that the next call to `tlib_get_executed_instructions`
// includes it in the returned value.
void tlib_reset_executed_instructions(uint64_t val)
{
    cpu->instructions_count_value = val;
    cpu->instructions_count_threshold += val;
}

uint64_t tlib_get_total_executed_instructions()
{
    return cpu->instructions_count_total_value;
}

void tlib_reset()
{
    cpu_reset(cpu);
}

int32_t tlib_execute(int32_t max_insns)
{
    if (cpu->instructions_count_value != 0) {
        tlib_abortf("Tried to execute cpu without reading executed instructions count first.");
    }
    cpu->instructions_count_threshold = max_insns;

    int32_t local_counter = 0;
    int32_t result = EXCP_INTERRUPT;
    while ((result == EXCP_INTERRUPT) && (cpu->instructions_count_threshold > 0)) {
        result = cpu_exec(cpu);

        local_counter += cpu->instructions_count_value;
        cpu->instructions_count_threshold -= cpu->instructions_count_value;
        cpu->instructions_count_value = 0;

        if(cpu->exit_request)
        {
            cpu->exit_request = 0;
            break;
        }
    }

    // we need to reset the instructions count value
    // as this is might be accessed after calling `tlib_execute`
    // to read the progress
    cpu->instructions_count_value = local_counter;
    
    return result;
}

int tlib_restore_context(void);

extern void *global_retaddr;

void tlib_restart_translation_block()
{
    target_ulong pc, cs_base;
    int cpu_flags;
    TranslationBlock *tb;
    int executed_instructions = -1;

    tb = tb_find_pc((uintptr_t)global_retaddr);
    if (tb != 0) {
        executed_instructions = cpu_restore_state_and_restore_instructions_count(cpu, tb, (uintptr_t)global_retaddr);
    }

    cpu_get_tb_cpu_state(cpu, &pc, &cs_base, &cpu_flags);
    tb_phys_invalidate(cpu->current_tb, -1);
    tb_gen_code(cpu, pc, cs_base, cpu_flags, 0);

    if (cpu->block_finished_hook_present) {
        tlib_on_block_finished(pc, executed_instructions);
    }

    cpu->exception_index = EXCP_WATCHPOINT;
    longjmp(cpu->jmp_env, 1); //for watchpoints!
}

void tlib_set_return_request()
{
    cpu->exit_request = 1;
}

int32_t tlib_is_wfi()
{
    return cpu->wfi;
}

uint32_t tlib_get_page_size()
{
    return TARGET_PAGE_SIZE;
}

void tlib_map_range(uint64_t start_addr, uint64_t length)
{
    ram_addr_t phys_offset = start_addr;
    ram_addr_t size = length;
    //remember that phys_dirty covers the whole memory range from 0 to the end
    //of the registered memory. Most offsets are probably unused. When a new
    //region is registered before any already registered memory, the array
    //does not need to be expanded.
    uint8_t *phys_dirty;
    size_t array_start_addr, array_size, new_size;
    array_start_addr = start_addr >> TARGET_PAGE_BITS;
    array_size = size >> TARGET_PAGE_BITS;
    new_size = array_start_addr + array_size;
    if (new_size > dirty_ram.current_size) {
        phys_dirty = tlib_malloc(new_size);
        memcpy(phys_dirty, dirty_ram.phys_dirty, dirty_ram.current_size);
        if (dirty_ram.phys_dirty != NULL) {
            tlib_free(dirty_ram.phys_dirty);
        }
        dirty_ram.phys_dirty = phys_dirty;
        dirty_ram.current_size = new_size;
    }
    memset(dirty_ram.phys_dirty + array_start_addr, 0xff, array_size);
    cpu_register_physical_memory(start_addr, size, phys_offset | IO_MEM_RAM);
}

void tlib_unmap_range(uint64_t start, uint64_t end)
{
    uint64_t new_start;

    while (start <= end) {
        unmap_page(start);
        new_start = start + TARGET_PAGE_SIZE;
        if (new_start < start) {
            return;
        }
        start = new_start;
    }
}

uint32_t tlib_is_range_mapped(uint64_t start, uint64_t end)
{
    PhysPageDesc *pd;

    while (start < end) {
        pd = phys_page_find((target_phys_addr_t)start >> TARGET_PAGE_BITS);
        if (pd != NULL && pd->phys_offset != IO_MEM_UNASSIGNED) {
            return 1; // at least one page of this region is mapped
        }
        start += TARGET_PAGE_SIZE;
    }
    return 0;
}

void tlib_invalidate_translation_blocks(uintptr_t start, uintptr_t end)
{
    tb_invalidate_phys_page_range_inner(start, end, 0, 0);
}

uint64_t tlib_translate_to_physical_address(uint64_t address, uint32_t access_type)
{
    uint64_t ret = virt_to_phys(address, access_type, 1);
    if (ret == TARGET_ULONG_MAX) {
        ret = (uint64_t)-1;
    }
    return ret;
}

void tlib_set_irq(int32_t interrupt, int32_t state)
{
    if (state) {
        cpu_interrupt(cpu, interrupt);
    } else {
        cpu_reset_interrupt(cpu, interrupt);
    }
}

int32_t tlib_is_irq_set()
{
    return cpu->interrupt_request;
}

void tlib_add_breakpoint(uint64_t address)
{
    cpu_breakpoint_insert(cpu, address, BP_GDB, NULL);
}

void tlib_remove_breakpoint(uint64_t address)
{
    cpu_breakpoint_remove(cpu, address, BP_GDB);
}

uintptr_t translation_cache_size;

void tlib_set_translation_cache_size(uintptr_t size)
{
    translation_cache_size = size;
}

void tlib_invalidate_translation_cache()
{
    if (cpu) {
        tb_flush(cpu);
    }
}

int tlib_restore_context()
{
    uintptr_t pc;
    TranslationBlock *tb;

    pc = (uintptr_t)global_retaddr;
    tb = tb_find_pc(pc);
    if (tb == 0) {
        // this happens when PC is outside RAM or ROM
        return -1;
    }
    return cpu_restore_state(cpu, tb, pc);
}

void *tlib_export_state()
{
    return cpu;
}

int32_t tlib_get_state_size()
{
    // Cpu state size is reported as
    // an offset of `current_tb` field
    // provided by CPU_COMMON definition.
    // It is a convention that all
    // architecture-specific, non-pointer
    // fields should be located in this
    // range. As a result this size can
    // be interpreted as an amount of bytes
    // to store during serialization.
    return (ssize_t)(&((CPUState *)0)->current_tb);
}

void tlib_set_chaining_enabled(uint32_t val)
{
    cpu->chaining_disabled = !val;
}

uint32_t tlib_get_chaining_enabled()
{
    return !cpu->chaining_disabled;
}

void tlib_set_tb_cache_enabled(uint32_t val)
{
    cpu->tb_cache_disabled = !val;
}

uint32_t tlib_get_tb_cache_enabled()
{
    return !cpu->tb_cache_disabled;
}

void tlib_set_block_finished_hook_present(uint32_t val)
{
    cpu->block_finished_hook_present = !!val;
}

void tlib_set_block_begin_hook_present(uint32_t val)
{
    cpu->block_begin_hook_present = !!val;
}

int32_t tlib_set_return_on_exception(int32_t value)
{
    int32_t previousValue = cpu->return_on_exception;
    cpu->return_on_exception = !!value;
    return previousValue;
}

void tlib_flush_page(uint64_t address)
{
    tlb_flush_page(cpu, address);
}

#if TARGET_LONG_BITS == 32
uint32_t *get_reg_pointer_32(int reg_number);
#elif TARGET_LONG_BITS == 64
uint64_t *get_reg_pointer_64(int reg_number);
#else
#error "Unknown number of bits"
#endif

uint64_t tlib_get_register_value(int reg_number)
{
#if TARGET_LONG_BITS == 32
    uint32_t *ptr = get_reg_pointer_32(reg_number);
    if (ptr == NULL) {
        tlib_abortf("Write to undefined CPU register number %d detected", reg_number);
        return 0;
    }
    return (uint64_t)*ptr;
#elif TARGET_LONG_BITS == 64
    uint64_t *ptr = get_reg_pointer_64(reg_number);
    if (ptr == NULL) {
        tlib_abortf("Write to undefined CPU register number %d detected", reg_number);
        return 0;
    }
    return *ptr;
#else
#error "Unknown number of bits"
#endif
}

void tlib_set_register_value(int reg_number, uint64_t val)
{
#if TARGET_LONG_BITS == 32
    uint32_t *ptr = get_reg_pointer_32(reg_number);
    if (ptr == NULL) {
        tlib_abortf("Write to undefined CPU register number %d detected", reg_number);
        return;
    }
    *ptr = val & 0xFFFFFFFF;
#elif TARGET_LONG_BITS == 64
    uint64_t *ptr = get_reg_pointer_64(reg_number);
    if (ptr == NULL) {
        tlib_abortf("Write to undefined CPU register number %d detected", reg_number);
        return;
    }
    *ptr = val;
#else
#error "Unknown number of bits"
#endif
}

void tlib_set_interrupt_begin_hook_present(uint32_t val)
{
    cpu->interrupt_begin_callback_enabled = !!val;
}

void tlib_set_interrupt_end_hook_present(uint32_t val)
{
    // Supported in RISC-V architecture only
    cpu->interrupt_end_callback_enabled = !!val;
}

void tlib_on_memory_access_event_enabled(int32_t value)
{
    cpu->tlib_is_on_memory_access_enabled = !!value;
}

void tlib_clean_wfi_proc_state(void)
{
    // Invalidates "Wait for interrupt" state, and makes the core ready to resume execution
    cpu->exception_index &= ~EXCP_WFI;
    cpu->wfi = 0;
}

void tlib_enable_opcodes_counting(uint32_t value)
{
    cpu->count_opcodes = !!value;
}

uint32_t tlib_get_opcode_counter(uint32_t opcode_id)
{
    return cpu->opcode_counters[opcode_id - 1].counter;
}

void tlib_reset_opcode_counters()
{
    for(int i = 0; i < cpu->opcode_counters_size; i++)
    {
        cpu->opcode_counters[i].counter = 0;
    }
}

uint32_t tlib_install_opcode_counter(uint32_t opcode, uint32_t mask)
{
    if(cpu->opcode_counters_size == MAX_OPCODE_COUNTERS)
    {
        return 0;
    }

    cpu->opcode_counters[cpu->opcode_counters_size].opcode = opcode;
    cpu->opcode_counters[cpu->opcode_counters_size].mask = mask;
    cpu->opcode_counters_size++;
    
    return cpu->opcode_counters_size;
}

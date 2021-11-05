#include "cpu.h"
#include <global_helper.h>
#include "callbacks.h"
#include "debug.h"
#include "atomic.h"

// verify if there are instructions left to execute, update instructions count
// and trim the block and exit to the main loop if necessary
void HELPER(prepare_block_for_execution)(void *tb)
{
    cpu->current_tb = (TranslationBlock *)tb;

    uint64_t instructions_left = cpu->instructions_count_threshold - cpu->instructions_count_value;
    uint64_t current_block_size = cpu->current_tb->icount;

    if (instructions_left == 0) {
        // setting `tb_restart_request` to 1 will stop executing this block at the end of the header
        cpu->tb_restart_request = 1;
    } else if (current_block_size > instructions_left) {
        // invalidate this block and jump back to the main loop
        tb_phys_invalidate(cpu->current_tb, -1);
        cpu->tb_restart_request = 1;
    }
}

void HELPER(update_instructions_count)(uint32_t current_block_size)
{
    cpu->instructions_count_value += current_block_size;
    cpu->instructions_count_total_value += current_block_size;
    cpu->current_tb->instructions_count_dirty = 1;
}

uint32_t HELPER(block_begin_event)(target_ulong address, uint32_t size)
{
    return tlib_on_block_begin(address, size);
}

void HELPER(block_finished_event)(target_ulong address, uint32_t executed_instructions)
{
    tlib_on_block_finished(address, executed_instructions);
}

void HELPER(abort)(void) {
    tlib_abort("aborted by gen_abort!");
}

void HELPER(log)(uint32_t id, uint32_t pc)
{
    tlib_printf(LOG_LEVEL_INFO, "Log @ pc=0x%08X (block start: 0x%08X) : '%s'", pc, CPU_PC(
                    cpu), msgs[id] == NULL ? "unknown??" : msgs[id]);
}

void HELPER(acquire_global_memory_lock)(CPUState * env)
{
    acquire_global_memory_lock(env);
}

void HELPER(release_global_memory_lock)(CPUState * env)
{
    release_global_memory_lock(env);
}

void HELPER(reserve_address)(CPUState * env, ram_addr_t address)
{
    reserve_address(env, address);
}

target_ulong HELPER(check_address_reservation)(CPUState * env, ram_addr_t address)
{
    return check_address_reservation(env, address);
}

void HELPER(var_log)(target_ulong v)
{
    tlib_printf(LOG_LEVEL_INFO, "Var Log: 0x" TARGET_FMT_lx, v);
}

void HELPER(count_opcode_inner)(uint32_t instruction_id)
{
   cpu->opcode_counters[instruction_id].counter++;
}

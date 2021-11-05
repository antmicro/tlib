/*
 *  Host code generation
 *
 *  Copyright (c) 2003 Fabrice Bellard
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
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "cpu.h"
#include "tcg-op.h"
#include "debug.h"

#include <global_helper.h>
#define GEN_HELPER 1
#include <global_helper.h>

int gen_new_label(void);

extern TCGv_ptr cpu_env;
extern CPUState *cpu;
static TCGArg *event_size_arg;
static TCGArg *event_size2_arg;

static int exit_no_hook_label;
static int block_header_interrupted_label;

CPUBreakpoint *process_breakpoints(CPUState *env, target_ulong pc)
{
    CPUBreakpoint *bp;
    QTAILQ_FOREACH(bp, &env->breakpoints, entry) {
        if (bp->pc == pc) {
            return bp;
        }
    }
    return NULL;
}

static inline void gen_block_header(TranslationBlock *tb)
{
    TCGv_i32 flag;
    exit_no_hook_label = gen_new_label();
    block_header_interrupted_label = gen_new_label();
    int execute_block_label = gen_new_label();
    flag = tcg_temp_local_new_i32();
    tcg_gen_ld_i32(flag, cpu_env, offsetof(CPUState, exit_request));
    tcg_gen_brcondi_i32(TCG_COND_NE, flag, 0, exit_no_hook_label);
    tcg_temp_free_i32(flag);

    TCGv_ptr tb_pointer = tcg_const_ptr((tcg_target_long)tb);
    gen_helper_prepare_block_for_execution(tb_pointer);
    tcg_temp_free_ptr(tb_pointer);

    flag = tcg_temp_local_new_i32();
    tcg_gen_ld_i32(flag, cpu_env, offsetof(CPUState, tb_restart_request));
    tcg_gen_brcondi_i32(TCG_COND_NE, flag, 0, exit_no_hook_label);
    tcg_temp_free_i32(flag);

    if (cpu->block_begin_hook_present) {
        TCGv event_address = tcg_const_tl(tb->pc);
        event_size_arg = gen_opparam_ptr + 1;
        TCGv_i32 event_size = tcg_const_i32(0xFFFF); // bogus value that is to be fixed at later point

        TCGv_i32 result = tcg_temp_new_i32();
        gen_helper_block_begin_event(result, event_address, event_size);
        tcg_temp_free(event_address);
        tcg_temp_free_i32(event_size);

        tcg_gen_brcondi_i64(TCG_COND_NE, result, 0, execute_block_label);
        tcg_temp_free_i32(result);

        TCGv_i32 const_one = tcg_const_i32(1);
        tcg_gen_st_i32(const_one, cpu_env, offsetof(CPUState, exit_request));
        tcg_temp_free_i32(const_one);

        tcg_gen_br(block_header_interrupted_label);
    }

    gen_set_label(execute_block_label);

    // it looks like we cannot re-use tcg_const in two places - that's why I create a second copy of it here
    event_size2_arg = gen_opparam_ptr + 1;
    TCGv_i32 event_size2 = tcg_const_i32(0xFFFF); // bogus value that is to be fixed at later point
    gen_helper_update_instructions_count(event_size2);
    tcg_temp_free_i32(event_size2);
}

static void gen_exit_tb_inner(uintptr_t val, TranslationBlock *tb, uint32_t instructions_count)
{
    if (cpu->block_finished_hook_present) {
        // This line may be missleading - we do not raport exact pc + size,
        // as the size of the current instruction is not yet taken into account.
        // Effectively it gives us the PC of the current instruction.
        TCGv last_instruction = tcg_const_tl(tb->pc + tb->prev_size);
        TCGv_i32 executed_instructions = tcg_const_i32(instructions_count);
        gen_helper_block_finished_event(last_instruction, executed_instructions);
        tcg_temp_free_i32(executed_instructions);
        tcg_temp_free(last_instruction);
    }
    tcg_gen_exit_tb(val);
}

static void gen_interrupt_tb(uintptr_t val, TranslationBlock *tb)
{
    // since the block was interrupted before executing any instruction we return 0
    gen_exit_tb_inner(val, tb, 0);
}

void gen_exit_tb(uintptr_t val, TranslationBlock *tb)
{
    gen_exit_tb_inner(val, tb, tb->icount);
}

void gen_exit_tb_no_chaining(TranslationBlock *tb)
{
    gen_exit_tb_inner(0, tb, tb->icount);
}

static inline void gen_block_footer(TranslationBlock *tb)
{
    if (tlib_is_on_block_translation_enabled) {
        tlib_on_block_translation(tb->pc, tb->size, tb->disas_flags);
    }
    if (cpu->block_begin_hook_present) {
        *event_size_arg = tb->icount;
    }
    *event_size2_arg = tb->icount;

    int finish_label = gen_new_label();
    gen_exit_tb((uintptr_t)tb + 2, tb);
    tcg_gen_br(finish_label);

    gen_set_label(block_header_interrupted_label);
    gen_interrupt_tb((uintptr_t)tb + 2, tb);
    tcg_gen_br(finish_label);

    gen_set_label(exit_no_hook_label);
    tcg_gen_exit_tb((uintptr_t)tb + 2);

    gen_set_label(finish_label);
    *gen_opc_ptr = INDEX_op_end;
}

static inline uint64_t get_max_instruction_count(CPUState *env, TranslationBlock *tb)
{
    return maximum_block_size > env->instructions_count_threshold ? env->instructions_count_threshold : maximum_block_size;
}

static void cpu_gen_code_inner(CPUState *env, TranslationBlock *tb, int search_pc)
{
    DisasContext dcc;
    CPUBreakpoint *bp;
    DisasContextBase *dc = (DisasContextBase *)&dcc;

    memset((void *)tcg->gen_opc_instr_start, 0, OPC_BUF_SIZE);

    tb->icount = 0;
    tb->size = 0;
    tb->search_pc = search_pc;
    dc->tb = tb;
    dc->is_jmp = 0;
    dc->pc = tb->pc;

    gen_block_header(tb);
    setup_disas_context(dc, env);
    tcg_clear_temp_count();
    UNLOCK_TB(tb);
    while (1) {
        CHECK_LOCKED(tb);
        if (unlikely(!QTAILQ_EMPTY(&env->breakpoints))) {
            bp = process_breakpoints(env, dc->pc);
            if (bp != NULL && gen_breakpoint(dc, bp)) {
                break;
            }
        }
        tb->prev_size = tb->size;

        if (tb->search_pc) {
            tcg->gen_opc_pc[gen_opc_ptr - tcg->gen_opc_buf] = dc->pc;
            tcg->gen_opc_instr_start[gen_opc_ptr - tcg->gen_opc_buf] = 1;
        }
        int do_break = 0;
        tb->icount++;
        if (!gen_intermediate_code(env, dc)) {
            do_break = 1;
        }
        if (tcg_check_temp_count()) {
            tlib_abortf("TCG temps leak detected at PC %08X", dc->pc);
        }
        if (!tb->search_pc) {
            // it looks like `search_pc` is set to 1 only when restoring the state;
            // the intention here is to set `original_size` value only during the first block generation
            // so it can be used later when restoring the block
            tb->original_size = tb->size;
        }
        if (do_break) {
            break;
        }
        if ((gen_opc_ptr - tcg->gen_opc_buf) >= OPC_MAX_SIZE) {
            break;
        }
        if (tb->icount >= get_max_instruction_count(env, tb)) {
            break;
        }
        if (dc->is_jmp) {
            break;
        }
        if (tb->search_pc && tb->size == tb->original_size) {
            // `search_pc` is set to 1 only when restoring the block;
            // this is to ensure that the size of restored block is not bigger than the size of the original one
            break;
        }
    }
    tb->disas_flags = gen_intermediate_code_epilogue(env, dc);
    gen_block_footer(tb);
}

/* '*gen_code_size_ptr' contains the size of the generated code (host
   code).
 */
void cpu_gen_code(CPUState *env, TranslationBlock *tb, int *gen_code_size_ptr)
{
    TCGContext *s = tcg->ctx;
    uint8_t *gen_code_buf;
    int gen_code_size;

    tcg_func_start(s);
    cpu_gen_code_inner(env, tb, 0);

    /* generate machine code */
    gen_code_buf = tb->tc_ptr;
    tb->tb_next_offset[0] = 0xffff;
    tb->tb_next_offset[1] = 0xffff;

    s->tb_next_offset = tb->tb_next_offset;
    s->tb_jmp_offset = tb->tb_jmp_offset;
    s->tb_next = NULL;

    gen_code_size = tcg_gen_code(s, gen_code_buf);
    *gen_code_size_ptr = gen_code_size;
}

/* The cpu state corresponding to 'searched_pc' is restored.
 */
int cpu_restore_state(CPUState *env, TranslationBlock *tb, uintptr_t searched_pc)
{
    TCGContext *s = tcg->ctx;
    int j, k;
    uintptr_t tc_ptr;
    int instructions_executed_so_far = 0;

    tcg_func_start(s);
    cpu_gen_code_inner(env, tb, 1);

    /* find opc index corresponding to search_pc */
    tc_ptr = (uintptr_t)tb->tc_ptr;
    if (searched_pc < tc_ptr) {
        return -1;
    }

    s->tb_next_offset = tb->tb_next_offset;
    s->tb_jmp_offset = tb->tb_jmp_offset;
    s->tb_next = NULL;
    j = tcg_gen_code_search_pc(s, (uint8_t *)tc_ptr, searched_pc - tc_ptr);
    if (j < 0) {
        return -1;
    }
    /* now find start of instruction before */
    while (tcg->gen_opc_instr_start[j] == 0) {
        j--;
    }

    k = j;
    while (k >= 0) {
        instructions_executed_so_far += tcg->gen_opc_instr_start[k];
        k--;
    }

    restore_state_to_opc(env, tb, j);

    return instructions_executed_so_far;
}

int cpu_restore_state_and_restore_instructions_count(CPUState *env, TranslationBlock *tb, uintptr_t searched_pc)
{
    int executed_instructions = cpu_restore_state(env, tb, searched_pc);
    if (executed_instructions != -1 && tb->instructions_count_dirty) {
        cpu->instructions_count_value -= (tb->icount - executed_instructions);
        cpu->instructions_count_total_value -= (tb->icount - executed_instructions);
        tb->instructions_count_dirty = 0;
    }
    return executed_instructions;
}

void generate_opcode_count_increment(CPUState *env, uint32_t opcode)
{
    uint32_t masked_opcode;
    for (uint32_t i = 0; i < env->opcode_counters_size; i++) {
        // mask out non-opcode fields
        masked_opcode = opcode & env->opcode_counters[i].mask;
        if (env->opcode_counters[i].opcode == masked_opcode) {
            TCGv_i32 p = tcg_const_i32(i);
            gen_helper_count_opcode_inner(p);
            tcg_temp_free_i32(p);
            break;
        }
    }
}

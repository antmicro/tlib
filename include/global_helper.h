#include "def-helper.h"

DEF_HELPER_1(prepare_block_for_execution, i32, ptr)
DEF_HELPER_0(block_begin_event, i32)
DEF_HELPER_2(block_finished_event, void, tl, i32)
DEF_HELPER_2(log, void, i32, i32)
DEF_HELPER_1(var_log, void, tl)
DEF_HELPER_0(abort, void)
DEF_HELPER_2(announce_stack_change, void, tl, i32)
DEF_HELPER_1(on_interrupt_end_event, void, i64)

DEF_HELPER_3(mark_tbs_as_dirty, void, env, tl, i32)

DEF_HELPER_1(count_opcode_inner, void, i32)

#include "def-helper.h"

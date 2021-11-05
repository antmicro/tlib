#include "def-helper.h"

DEF_HELPER_1(prepare_block_for_execution, void, ptr)
DEF_HELPER_1(update_instructions_count, void, i32)
DEF_HELPER_2(block_begin_event, i32, tl, i32)
DEF_HELPER_2(block_finished_event, void, tl, i32)
DEF_HELPER_2(log, void, i32, i32)
DEF_HELPER_1(var_log, void, tl)
DEF_HELPER_0(abort, void)

DEF_HELPER_1(count_opcode_inner, void, i32)

#include "def-helper.h"

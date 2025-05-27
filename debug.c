/*
 *  Debug functions.
 *
 *  Copyright (c) Antmicro
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
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include "infrastructure.h"
#include "cpu.h"
#include "tcg-op.h"
#include <string.h>
#include "debug.h"

void tcg_gen_debug_print(uint64_t debugCode)
{
    TCGv_i64 msg = tcg_temp_new_i64();
    tcg_gen_movi_i64(msg, debugCode);
    gen_helper_debug_print(msg);
    tcg_temp_free_i64(msg);
}

void tcg_gen_debug_printv(TCGv_i64 msg)
{
    gen_helper_debug_print(msg);
}

void tcg_gen_error_print(uint64_t errorCode)
{
    TCGv_i64 msg = tcg_temp_new_i64();
    tcg_gen_movi_i64(msg, errorCode);
    TCGv_i32 logLevel = tcg_temp_new_i32();
    tcg_gen_movi_i32(logLevel, 3);  //  error
    gen_helper_print(logLevel, msg);
    tcg_temp_free_i64(msg);
    tcg_temp_free_i32(logLevel);
}

void tcg_gen_error_printv(TCGv_i64 msg)
{
    TCGv_i32 logLevel = tcg_temp_new_i32();
    tcg_gen_movi_i32(logLevel, 3);  //  error
    gen_helper_print(logLevel, msg);
    tcg_temp_free_i32(logLevel);
}

char *msgs[MAX_MSG_COUNT];
#define MAX_MSG_LENGTH 4096

#ifdef DEBUG_ON
static uint32_t log_set_msg(char *msg)
{
    int id = 0;
    while(msgs[id] != NULL) {
        if((strcmp(msgs[id], msg)) == 0) {
            return id;
        }
        id++;
    }
    if(id >= MAX_MSG_COUNT) {
        msgs[0] = tlib_strdup("MSG_COUNT_OVERFLOW");
        return 0;  //  overflow
    }
    msgs[id] = tlib_strdup(msg);
    return id;
}
#endif

void generate_log(uint32_t pc, char *format, ...)
{
#ifdef DEBUG_ON
    char msg[MAX_MSG_LENGTH];
    va_list argList;
    va_start(argList, format);
    vsprintf(msg, format, argList);
    va_end(argList);
    TCGv ll = tcg_temp_new();
    TCGv ll2 = tcg_temp_new();
    int id = log_set_msg(msg);
    tcg_gen_movi_tl(ll, id);
    tcg_gen_movi_tl(ll2, pc);
    gen_helper_log(ll, ll2);
    tcg_temp_free(ll);
    tcg_temp_free(ll2);
#endif
}

void mark_as_locked(struct TranslationBlock *tb, char *filename, int line_number)
{
#if DEBUG
    tb->lock_active = 1;
    tb->lock_file = filename;
    tb->lock_line = line_number;
#endif
}

void check_locked(struct TranslationBlock *tb)
{
#if DEBUG
    if(tb->lock_active) {
        tlib_abortf("Translation after locking the TB detected @ %s:%d", tb->lock_file, tb->lock_line);
    }
#endif
}

void generate_var_log(TCGv v)
{
#ifdef DEBUG_ON
    gen_helper_var_log(v);
#endif
}

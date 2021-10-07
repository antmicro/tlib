#include "tcg/additional.h"

#ifdef GENERATE_PERF_MAP
/*
    We place the helper here, since TB internals require cpu.h to be included,
    so this is the only util of the labeling pack, that has to be compiled
    not into tcg, but into tlibs.
 */

#include "cpu.h"
#include "include/infrastructure.h"
#include <stdio.h>

void tcg_perf_out_symbol_from_tb(TranslationBlock *tb, int host_size, const char *comment)
{
    char buffer[100];

    snprintf(buffer, sizeof(buffer), ";size:%x;jmp_next:%p,%p;jmp_first:%p,icount:%x;%s;", tb->size, tb->jmp_next[0],
             tb->jmp_next[1], tb->jmp_first, tb->icount, comment == NULL ? "" : comment);
    tcg_perf_out_symbol_s(tb->tc_ptr, host_size, buffer);
}
#else
inline void tcg_perf_out_symbol_from_tb(struct TranslationBlock *tb, int host_size, const char *comment)
{
}
#endif
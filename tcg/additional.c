/*
 * Copyright (c) 2010-2015 Antmicro Ltd <www.antmicro.com>
 * Copyright (c) 2011-2015 Realtime Embedded AB <www.rte.se>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdint.h>
#include <string.h>
#include "tcg.h"

tcg_t *tcg;

void *(*_TCG_malloc)(size_t);

void attach_malloc(void *malloc_callback)
{
    _TCG_malloc = malloc_callback;
}

void *TCG_malloc(size_t size)
{
    return _TCG_malloc(size);
}

void *(*_TCG_realloc)(void *, size_t);

void attach_realloc(void *reall)
{
    _TCG_realloc = reall;
}

void *TCG_realloc(void *ptr, size_t size)
{
    return _TCG_realloc(ptr, size);
}

void (*_TCG_free)(void *ptr);

void attach_free(void *free_callback)
{
    _TCG_free = free_callback;
}

void TCG_free(void *ptr)
{
    _TCG_free(ptr);
}

void TCG_pstrcpy(char *buf, int buf_size, const char *str)
{
    int c;
    char *q = buf;

    if (buf_size <= 0) {
        return;
    }

    for (;;) {
        c = *str++;
        if (c == 0 || q >= buf + buf_size - 1) {
            break;
        }
        *q++ = c;
    }
    *q = '\0';
}

char *TCG_pstrcat(char *buf, int buf_size, const char *s)
{
    int len;
    len = strlen(buf);
    if (len < buf_size) {
        TCG_pstrcpy(buf + len, buf_size - len, s);
    }
    return buf;
}

unsigned int temp_buf_offset;
unsigned int tlb_table_n_0[MMU_MODES_MAX];
unsigned int tlb_table_n_0_addr_read[MMU_MODES_MAX];
unsigned int tlb_table_n_0_addr_write[MMU_MODES_MAX];
unsigned int tlb_table_n_0_addend[MMU_MODES_MAX];
unsigned int tlb_entry_addr_read;
unsigned int tlb_entry_addr_write;
unsigned int tlb_entry_addend;
unsigned int sizeof_CPUTLBEntry;
int TARGET_PAGE_BITS;

void set_TARGET_PAGE_BITS(int val)
{
    TARGET_PAGE_BITS = val;
}

void set_sizeof_CPUTLBEntry(unsigned int sz)
{
    sizeof_CPUTLBEntry = sz;
}

void set_temp_buf_offset(unsigned int offset)
{
    temp_buf_offset = offset;
}

void set_tlb_entry_addr_rwu(unsigned int read, unsigned int write, unsigned int addend)
{
    tlb_entry_addr_read = read;
    tlb_entry_addr_write = write;
    tlb_entry_addend = addend;
}

void set_tlb_table_n_0(int i, unsigned int offset)
{
    tlb_table_n_0[i] = offset;
}

void set_tlb_table_n_0_rwa(int i, unsigned int read, unsigned int write, unsigned int addend)
{
    tlb_table_n_0_addr_read[i] = read;
    tlb_table_n_0_addr_write[i] = write;
    tlb_table_n_0_addend[i] = addend;
}

#ifdef GENERATE_PERF_MAP

#include <unistd.h>
#include "../include/infrastructure.h"

// not thread safe
static FILE *perf_map_handle;

typedef struct tcg_perf_map_symbol
{
    void *ptr;
    int size;
    char *label;
    bool reused;

    struct tcg_perf_map_symbol *left, *right;
} tcg_perf_map_symbol;

static tcg_perf_map_symbol *symbols = NULL;

#define RETURN_EMPTY_PERF_HANDLE \
    if(unlikely(perf_map_handle == NULL)) \
    {   \
        return; \
    }

void tcg_perf_init_labeling()
{
    char target[30];

    snprintf(target, sizeof(target), "/tmp/perf-%d.map", getpid());
    perf_map_handle = fopen(target, "a");

    if (unlikely(perf_map_handle == NULL)) {
        tlib_printf(LOG_LEVEL_WARNING, "Cannot generate perf.map.");
    }
}

static void tcg_perf_symbol_tree_recurse_helper(tcg_perf_map_symbol *s, void (*callback)(tcg_perf_map_symbol *))
{
    if (s == NULL) {
        return;
    }

    tcg_perf_symbol_tree_recurse_helper(s->left, callback);
    tcg_perf_symbol_tree_recurse_helper(s->right, callback);
    (*callback)(s);
}

static void tcg_perf_flush_symbol(tcg_perf_map_symbol *s)
{
    if (s->label) {
        fprintf(perf_map_handle, "%p %x %stcg_jit_code:%p:%s\n", s->ptr, s->size, s->reused ? "[REUSED]" : "", s->ptr, s->label);
    } else {
        fprintf(perf_map_handle, "%p %x %stcg_jit_code:%p\n", s->ptr, s->size, s->reused ? "[REUSED]" : "", s->ptr);
    }
}

void tcg_perf_flush_map()
{
    RETURN_EMPTY_PERF_HANDLE;
    tcg_perf_symbol_tree_recurse_helper(symbols, tcg_perf_flush_symbol);
}

static void tcg_perf_free_symbol(tcg_perf_map_symbol *s)
{
    TCG_free(s->label);
    TCG_free(s);
}

void tcg_perf_fini_labeling()
{
    if (likely(perf_map_handle != NULL)) {
        tcg_perf_flush_map();
        fclose(perf_map_handle);

        tcg_perf_symbol_tree_recurse_helper(symbols, tcg_perf_free_symbol);
    }
}

static tcg_perf_map_symbol **tcg_perf_append_symbol_inner(tcg_perf_map_symbol *s)
{
    tcg_perf_map_symbol **next = &symbols;
    while (*next != NULL) {
        if (s->ptr == (*next)->ptr) {
            s->reused = true;
            s->left = (*next)->left;
            s->right = (*next)->right;
            tcg_perf_free_symbol(*next);
            return next;
        } else if (s->ptr < (*next)->ptr) {
            next = &(*next)->left;
        } else {
            next = &(*next)->right;
        }
    }
    return next;
}

void tcg_perf_append_symbol(tcg_perf_map_symbol *s)
{
    tcg_perf_map_symbol **place = tcg_perf_append_symbol_inner(s);
    *place = s;
}

void tcg_perf_out_symbol_s(void *s, int size, const char *label)
{
    RETURN_EMPTY_PERF_HANDLE;

    size_t len = strlen(label) + 1;
    char *_label = TCG_malloc(sizeof(char) * len);
    strncpy(_label, label, len);

    tcg_perf_map_symbol *symbol = TCG_malloc(sizeof(tcg_perf_map_symbol));
    *symbol = (tcg_perf_map_symbol) { .ptr = s, .size = size, .label = _label, .reused = false, .left = NULL, .right = NULL};
    tcg_perf_append_symbol(symbol);
}

void tcg_perf_out_symbol(void *s, int size)
{
    RETURN_EMPTY_PERF_HANDLE;

    tcg_perf_out_symbol_s(s, size, "");
}

void tcg_perf_out_symbol_i(void *s, int size, int label)
{
    RETURN_EMPTY_PERF_HANDLE;
    char buffer[20];

    snprintf(buffer, sizeof(buffer), "%x", label);
    tcg_perf_out_symbol_s(s, size, buffer);
}

#else
void tcg_perf_init_labeling()
{
}

void tcg_perf_fini_labeling()
{
}

void tcg_perf_out_symbol(void *s, int size)
{
}

void tcg_perf_out_symbol_s(void *s, int size, const char *label)
{
}

void tcg_perf_out_symbol_i(void *s, int size, int label)
{
}
#endif

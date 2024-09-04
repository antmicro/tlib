#pragma once

#include <stdint.h>

extern uint8_t *tcg_rw_buffer;
extern uint8_t *tcg_rx_buffer;
extern intptr_t tcg_wx_diff;

const void* rw_ptr_to_rx(void *ptr);
void* rx_ptr_to_rw(const void *ptr);
bool alloc_code_gen_buf(uint64_t size);
void free_code_gen_buf();

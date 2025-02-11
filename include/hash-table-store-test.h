#pragma once

#include <stdint.h>

#if TARGET_LONG_BITS == 32
    #define TCGv          TCGv_i32
    typedef uint32_t target_ulong __attribute__((aligned(TARGET_LONG_ALIGNMENT)));
#elif TARGET_LONG_BITS == 64
    #define TCGv          TCGv_i64
    typedef uint64_t target_ulong __attribute__((aligned(TARGET_LONG_ALIGNMENT)));
#else
    #error Unhandled TARGET_LONG_BITS value
#endif

#define TCGv_guestptr TCGv

#if HOST_LONG_BITS == 32
    #define TCGv_hostptr  TCGv_i32
#elif HOST_LONG_BITS == 64
    #define TCGv_hostptr  TCGv_i64
#else
    #error Unhandled HOST_LONG_BITS value
#endif

//  For masking away the bits used to identify the table in memory.
extern uint64_t hst_guest_prefix_mask;

/* Derives the `hst_guest_prefix_mask` from the given number of table bits. */
void calculate_hst_mask(uint8_t store_table_bits);

#define HST_UNLOCKED    0xFFFFFFFF
#define HST_INIT_VALUES 0xFFFFFFFF

typedef struct hst_entry
{
    // The ID of the thread which last wrote to this address.
    uint32_t lastAccessedByThreadId;

    // Synchronization variable for keeping SCs atomic.
    uint32_t lock;
} hst_entry_t;

typedef int TCGv_i32;
typedef int TCGv_i64;
typedef struct CPUState CPUState;

void initialize_store_table(hst_entry_t* store_table);
uint32_t get_thread_id(CPUState *env);

void gen_store_table_set(CPUState *env, TCGv_guestptr guestAddress);
/* Places 1 in `result` if the address is reserved by the current thread, 0 otherwise. */
void gen_store_table_check(CPUState *env, TCGv result, TCGv_guestptr guestAddress);
void gen_store_table_lock(CPUState *env, TCGv_guestptr guestAddress, uint32_t mem_index);
void gen_store_table_unlock(CPUState *env, TCGv_guestptr guestAddress);
/* Simple hash function to find the correct index for an address in the hash table */
uintptr_t address_hash(CPUState* cpu_env, target_ulong address);

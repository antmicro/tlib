#pragma once

#include <stdint.h>

#if TARGET_LONG_BITS == 32
#define TCGv TCGv_i32
typedef uint32_t target_ulong __attribute__((aligned(TARGET_LONG_ALIGNMENT)));
#elif TARGET_LONG_BITS == 64
#define TCGv TCGv_i64
typedef uint64_t target_ulong __attribute__((aligned(TARGET_LONG_ALIGNMENT)));
#else
#error Unhandled TARGET_LONG_BITS value
#endif

#define TCGv_guestptr TCGv

#if HOST_LONG_BITS == 32
#define TCGv_hostptr TCGv_i32
#elif HOST_LONG_BITS == 64
#define TCGv_hostptr TCGv_i64
#else
#error Unhandled HOST_LONG_BITS value
#endif

#define HST_UNLOCKED 0xFFFFFFFF
#define HST_NO_CORE  0xFFFFFFFF

/* A single entry in the store table. */
typedef struct {
    /*  The ID of the core that last wrote to (or reserved)
     *  one of the addresses represented by this entry.
     */
    uint32_t last_accessed_by_core_id;

    /*  A fine-grained lock used to ensure mutual exclusion when modifying the above field. */
    uint32_t lock;
} store_table_entry_t;

typedef int TCGv_i32;
typedef int TCGv_i64;
typedef struct CPUState CPUState;

void initialize_store_table(store_table_entry_t *store_table, uint8_t store_table_bits);

uint32_t get_core_id(CPUState *env);

/* Generates code to update the hash table entry corresponding to the given
 * `guest_address` with the current core's ID.
 */
void gen_store_table_set(CPUState *env, TCGv_guestptr guest_address);
void gen_store_table_check(CPUState *env, TCGv result, TCGv_guestptr guest_address);
void gen_store_table_lock(CPUState *env, TCGv_guestptr guest_address);
void gen_store_table_unlock(CPUState *env, TCGv_guestptr guest_address);

/* Computes which hash table entry address corresponds to the given `guest_address`. */
uintptr_t address_hash(CPUState *cpu_env, target_ulong guest_address);

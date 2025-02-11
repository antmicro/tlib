#pragma once

#include <stdint.h>
#include <stdbool.h>

#if TARGET_LONG_BITS == 32
typedef uint32_t target_ulong __attribute__((aligned(TARGET_LONG_ALIGNMENT)));
#elif TARGET_LONG_BITS == 64
typedef uint64_t target_ulong __attribute__((aligned(TARGET_LONG_ALIGNMENT)));
#endif

//  How many prefix bits are necessary to uniquely address the table on a 32-bit host.
//  i.e. 4 bits on a 32-bit host results in a hash table of size 256 MiB.
#define HST_TABLE_BITS_32 4

#if HOST_LONG_BITS == 32
#define HST_TABLE_BITS HST_TABLE_BITS_32
#elif HOST_LONG_BITS == 64
#define HST_TABLE_BITS (32 + HST_TABLE_BITS_32)
#else
#error Unhandled HOST_LONG_BITS value
#endif

//  How many bytes the table takes up in memory.
#define HST_TABLE_BYTES (1 << (HOST_LONG_BITS - HST_TABLE_BITS))

//  How many hst_entry_t the table takes up in memory. (hst_entry_t is 8=2^3 bytes)
#define HST_TABLE_ENTRIES (HST_TABLE_BYTES >> 3)

//  Mask out the bits used by the table in host size.
#define HST_PREFIX_MASK ~(((1UL << HST_TABLE_BITS) - 1) << (HOST_LONG_BITS - HST_TABLE_BITS))

#if TARGET_LONG_BITS >= HOST_LONG_BITS
//  Since the guest has wider registers than the host, it's fine to use the smaller host mask.
#define HST_GUEST_PREFIX_MASK HST_PREFIX_MASK
#else
//  Mask out the bits used by the table in guest size.
#define HST_GUEST_PREFIX_MASK ((target_ulong)(HST_PREFIX_MASK & ((1UL << TARGET_LONG_BITS) - 1)))
#endif

//  Mask out bits for alignment.
#define HST_ALIGNMENT_MASK ~(0b11)

//  Mask out the bit for the fine-grained lock.
#define HST_LOCK_MASK ~(0b100)

//  Mask an address the same way it is hashed for lookup in the table.
#define HST_GUEST_ADDRESS_MASK (HST_GUEST_PREFIX_MASK & HST_ALIGNMENT_MASK & HST_LOCK_MASK)

#define HST_UNLOCKED    0xFFFFFFFF
#define HST_INIT_VALUES 0xFFFFFFFF

typedef struct CPUState CPUState;

typedef struct hst_entry {
    //  The ID of the thread which last wrote to this address.
    uint32_t lastAccessedByThreadId;

    //  Synchronization variable for keeping SCs atomic.
    uint32_t lock;
} hst_entry_t;

/*
    `FALLBACK` is used when a store-conditional is either on
    a MMIO or spans two pages in memory. As of writing, fallback means
    using the global memory lock and normal soft-mmu.
*/
typedef enum sc_result {
    SUCCESS,
    FAILURE,
    FALLBACK,
} sc_result_t;

typedef union sc_size {
    uint32_t size_32;
    uint64_t size_64;
} sc_size_t;

void register_thread_address_access(CPUState *cpu_env, target_ulong address);
uint32_t store_conditional_u32(CPUState *cpu_env, target_ulong dest, uint32_t value, uint32_t mem_index, void *return_address);
uint32_t store_conditional_u64(CPUState *cpu_env, target_ulong dest, uint64_t value, uint32_t mem_index, void *return_address);
void hash_table_lock(CPUState *cpu_env, target_ulong address);
void hash_table_unlock(CPUState *cpu_env, target_ulong address);
uintptr_t address_hash(CPUState *cpu_env, target_ulong address);
void initialize_store_table(hst_entry_t *store_table);

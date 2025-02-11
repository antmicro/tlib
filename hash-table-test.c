#include "hash-table-test.h"
#include "stdatomic.h"
#include "cpu.h"
#include "address-translation.h"

/*
    This macro will retrieve the host address via
    `translate_page_aligned_address_and_fill_tlb_u32[64]` and will
    compare the current host address value with the `reserved_value`.
    If these are still the same, write `new_value` to the host address.

    If the guest address either points to an MMIO or is page spanning,
    return a `FALLBACK` to trigger fallback logic.
*/
#define HANDLE_STORE_CONDITIONAL(BIT_SIZE, TYPE, VALUE_FIELD, BYTES)                                                       \
    do {                                                                                                                   \
        if(address_spanning_pages(dest, data_size)) {                                                                      \
            return FALLBACK;                                                                                               \
        }                                                                                                                  \
        uintptr_t host_address = translate_page_aligned_address_and_fill_tlb_u##BIT_SIZE(dest, mem_index, return_address); \
        if(host_address == dest) {                                                                                         \
            return FALLBACK;                                                                                               \
        }                                                                                                                  \
                                                                                                                           \
        TYPE reserved_value = ((TYPE)cpu_env->reserved_val);                                                               \
        TYPE new_value = value.VALUE_FIELD;                                                                                \
        if(!atomic_compare_exchange_strong((TYPE *)host_address, &reserved_value, new_value)) {                            \
            return FAILURE;                                                                                                \
        }                                                                                                                  \
        mark_tbs_containing_pc_as_dirty(dest, BYTES, 1);                                                                   \
    } while(0)

/* Check if the calling thread is holding a reservation for `address` */
static bool check_thread_address_access(CPUState *cpu_env, target_ulong address)
{
    uint32_t atomic_id = cpu_env->atomic_id;
    uint32_t *hashed_address = (uint32_t *)address_hash(cpu_env, address);
    return atomic_id == *hashed_address;
}

/* Once available, this thread will acquire the fine grained lock for `address` */
void hash_table_lock(CPUState *cpu_env, target_ulong address)
{
    uint32_t tid = (uint32_t)cpu_env->atomic_id;
    uint32_t *lock_address = (uint32_t *)(address_hash(cpu_env, address) + sizeof(uint32_t));
    uint32_t expected = HST_UNLOCKED;
    while(!atomic_compare_exchange_strong(lock_address, &expected, tid)) {
        /*
            This is needed because if `*lock_address != expected` then
            `expected = *lock_address`

        */
        expected = HST_UNLOCKED;
    }
    cpu_env->locked_address = address;
}

/* Thread releases fine grained lock for `address` */
void hash_table_unlock(CPUState *cpu_env, target_ulong address)
{
    uint32_t *lock_address = (uint32_t *)(address_hash(cpu_env, address) + sizeof(uint32_t));
#if DEBUG
    if(cpu_env->atomic_id != *lock_address) {
        tlib_abortf("tid %x tried to release a lock for address %p which it does not own!", cpu_env->atomic_id, address);
    }
#endif
    *lock_address = HST_UNLOCKED;
    cpu_env->locked_address = 0;
}

/* Check if a guest address spans between two pages */
static bool address_spanning_pages(target_ulong guest_address, uint16_t data_size)
{
    return ((guest_address & ~TARGET_PAGE_MASK) + data_size - 1) >= TARGET_PAGE_SIZE;
}

/*  `return_address` needs to be an address pointing to the callers translation block  */
inline static uint32_t store_conditional(CPUState *cpu_env, target_ulong dest, sc_size_t value, uint32_t mem_index,
                                         void *return_address, uint16_t data_size)
{
    /* Check that the SC is performed on the same address as the previous LR instruction reserved */
    if(cpu_env->reserved_address != dest) {
        return FAILURE;
    }

    bool reserved_by_me = check_thread_address_access(cpu_env, dest);
    if(!reserved_by_me) {
        return FAILURE;
    }

    /* Perform a CAS on old value */
    if(data_size == sizeof(uint32_t)) {
        HANDLE_STORE_CONDITIONAL(32, uint32_t, size_32, 4);
    } else {
        HANDLE_STORE_CONDITIONAL(64, uint64_t, size_64, 8);
    }

    return SUCCESS;
}

inline uint32_t store_conditional_u32(CPUState *cpu_env, target_ulong dest, uint32_t value, uint32_t mem_index,
                                      void *return_address)
{
    sc_size_t sc_value;
    sc_value.size_32 = value;
    return store_conditional(cpu_env, dest, sc_value, mem_index, return_address, sizeof(uint32_t));
}

inline uint32_t store_conditional_u64(CPUState *cpu_env, target_ulong dest, uint64_t value, uint32_t mem_index,
                                      void *return_address)
{
    sc_size_t sc_value;
    sc_value.size_64 = value;
    return store_conditional(cpu_env, dest, sc_value, mem_index, return_address, sizeof(uint64_t));
}

/* Stores this thread id in the hash table on `address` */
void register_thread_address_access(CPUState *cpu_env, target_ulong address)
{
    volatile uint32_t *tid_address = (volatile uint32_t *)address_hash(cpu_env, address);
    uint32_t atomic_id = cpu_env->atomic_id;
    atomic_store(tid_address, atomic_id);
}

/* Simple hash function (not cryptographic) to find the correct index of `address` in the hash table */
uintptr_t address_hash(CPUState *cpu_env, target_ulong address)
{
    uintptr_t table_offset = (uintptr_t)(cpu_env->store_table);
#if TARGET_LONG_BITS == 64 && HOST_LONG_BITS == 32
    uintptr_t hashed_address = (uint32_t)(HST_UNLOCKED & address)
#else
    uintptr_t hashed_address = address;
#endif
        hashed_address &= HST_GUEST_ADDRESS_MASK;
    hashed_address |= table_offset;
    return hashed_address;
}

void initialize_store_table(hst_entry_t *store_table)
{
    tlib_printf(LOG_LEVEL_DEBUG, "initialize_store_table: initializing with ptr %p", store_table);
    /* Initialize store table. */
    //  todo: is initialization flag necessary, like in atomic.c?
    static const hst_entry_t defaultEntry = { .lastAccessedByThreadId = HST_INIT_VALUES, .lock = HST_UNLOCKED };
    for(uint64_t i = 0; i < HST_TABLE_ENTRIES; i++) {
        store_table[i] = defaultEntry;
    }
}

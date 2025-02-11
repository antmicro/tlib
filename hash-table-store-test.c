#include "hash-table-store-test.h"

#include "cpu.h"
#include "infrastructure.h"
#include "tcg-op.h"
#include "tcg-op-atomic.h"
#include "atomic-intrinsics.h"

void tcg_gen_error_print(uint64_t errorCode);
void tcg_gen_debug_printv(TCGv_i64 msg);
void tcg_gen_error_printv(TCGv_i64 msg);


uint64_t hst_guest_prefix_mask = 0;
uint64_t hst_table_entries_count = 0;
void calculate_hst_mask(const uint8_t store_table_bits)
{
    //  _bits means how many bits are used in the addresses, not how many bits
    //  large the contents is. I.e. if an entry is 64 bits large it uses 3 bits.
    const uint64_t content_bits = sizeof(uintptr_t) * 8 - (uint64_t)store_table_bits;

    const uint64_t table_size_bytes = 1UL << content_bits;
    const uint64_t entry_size_bytes = sizeof(hst_entry_t);
    hst_table_entries_count = table_size_bytes / entry_size_bytes;

#ifdef DEBUG
    uint64_t size_kib = table_size_bytes >> 10;
    uint64_t size_mib = table_size_bytes >> 20;
    uint64_t size_gib = table_size_bytes >> 30;
    if(size_kib == 0) {
        tlib_printf(LOG_LEVEL_DEBUG, "Store table is %u B (%u bits)", table_size_bytes, store_table_bits);
    } else if(size_mib == 0) {
        tlib_printf(LOG_LEVEL_DEBUG, "Store table is %u KiB (%u bits)", size_kib, store_table_bits);
    } else if(size_gib == 0) {
        tlib_printf(LOG_LEVEL_DEBUG, "Store table is %u MiB (%u bits)", size_mib, store_table_bits);
    } else {
        tlib_printf(LOG_LEVEL_DEBUG, "Store table is %u GiB (%u bits)", size_gib, store_table_bits);
    }
#endif

    const uint64_t n_bits = (1UL << store_table_bits) - 1;
    const uint64_t table_mask = n_bits << content_bits;
    const uint64_t interior_mask = ~table_mask;

    const uint64_t entry_mask = sizeof(hst_entry_t) - 1;
    const uint64_t alignment_mask = ~entry_mask;
    hst_guest_prefix_mask = interior_mask & alignment_mask;
}

void initialize_store_table(hst_entry_t *store_table)
{
    tlib_assert(hst_table_entries_count != 0);
    tlib_printf(LOG_LEVEL_DEBUG, "initialize_store_table: initializing with ptr %p", store_table);
    /* Initialize store table. */
    //  todo: is initialization flag necessary, like in atomic.c?
    static const hst_entry_t defaultEntry = { .lastAccessedByThreadId = HST_INIT_VALUES, .lock = HST_UNLOCKED };
    for(uint64_t i = 0; i < hst_table_entries_count; i++) {
        store_table[i] = defaultEntry;
    }
}


/* Hashes `guestAddress` and places the result in `hashedAddress`. */
static void gen_hash_address(CPUState *env, TCGv_hostptr hashedAddress, TCGv_guestptr guestAddress)
{
    tcg_gen_mov_tl(hashedAddress, guestAddress);

    // Zero out upper bits of address, to make room for the address of the table.
    // Zero out lower bits, both for alignment and to make room for the fine-grained lock.
    tcg_gen_andi_i64(hashedAddress, hashedAddress, hst_guest_prefix_mask);

    // Replace the upper bits of address with start of table.
    uintptr_t storeTableStartValue = (uintptr_t) env->store_table;
    tcg_gen_ori_i64(hashedAddress, hashedAddress, storeTableStartValue);
}

// Abstract away what the actual thread id comes from.
uint32_t get_thread_id(CPUState *env)
{
    // Use the atomic_id as the "thread id" required for HST.
    return env->atomic_id;
}

#ifndef __llvm__
// todo: might not be necessary in the end
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

void gen_store_table_set(CPUState *env, TCGv_guestptr guestAddress)
{
    TCGv_hostptr hashedAddress = tcg_temp_new_hostptr();
    gen_hash_address(env, hashedAddress, guestAddress);

    TCGv_i32 threadId = tcg_temp_new_i32();
    tcg_gen_movi_i32(threadId, get_thread_id(env));

    // The hashed address now points to the table entry for the thread id, so store it there.
    // Note that the table entry update occurs atomically, with a single store.
    tcg_gen_st32_tl(threadId, hashedAddress, offsetof(hst_entry_t, lastAccessedByThreadId));

    tcg_temp_free_hostptr(hashedAddress);
    tcg_temp_free_i32(threadId);
}

// Result is 1 if check succeeds, 0 otherwise.
void gen_store_table_check(CPUState *env, TCGv result, TCGv_guestptr guestAddress)
{
    TCGv_hostptr hashedAddress = tcg_temp_new_hostptr();
    gen_hash_address(env, hashedAddress, guestAddress);

    // Load thread id from store table, to see which thread last accessed the address.
    tcg_gen_ld32u_tl(result, hashedAddress, offsetof(hst_entry_t, lastAccessedByThreadId));

    // See if the current thread is the one who last accessed the reserved address.
    // returns 1 if condition succeeds, 0 otherwise.
    tcg_gen_setcondi_tl(TCG_COND_EQ, result, result, get_thread_id(env));

    tcg_temp_free_hostptr(hashedAddress);
}

void gen_store_table_lock(CPUState *env, TCGv_guestptr guestAddress, uint32_t mem_index)
{
    TCGv_hostptr hashedAddress = tcg_temp_local_new_hostptr();
    gen_hash_address(env, hashedAddress, guestAddress);

    // Add the offset of the lock field, since we want to access the lock and not the thread id.
    TCGv_hostptr lockAddress = tcg_temp_local_new_hostptr();
    tcg_gen_addi_i64(lockAddress, hashedAddress, offsetof(hst_entry_t, lock));

    TCGv_i32 expectedLock = tcg_temp_local_new_i32();
    tcg_gen_movi_i32(expectedLock, HST_UNLOCKED);

    // Acquiring the lock means storing this thread's id.
    TCGv_i32 newLock = tcg_temp_local_new_i32();
    tcg_gen_movi_i32(newLock, get_thread_id(env));

    TCGv_i32 result = tcg_temp_local_new_i32();

    int retry = gen_new_label();
    gen_set_label(retry);

    // Optimistically try to atomically acquire the lock (only succeeds if it's currently unlocked).
    tcg_gen_atomic_compare_and_swap_host_intrinsic_i32(result, expectedLock, lockAddress, newLock, mem_index);
    // todo: _shouldn't_ be necessary
    // tcg_gen_host_atomic_cmpxchg_i32(result, lockAddress, newLock, newLock, mem_index, MO_32);

    // todo: debugging code. removeme
    int skipDebugLog = gen_new_label();
    tcg_gen_brcondi_i64(TCG_COND_NE, result, get_thread_id(env), skipDebugLog);

    // todo: UGLY FIX. removeme
    //tcg_gen_st32_tl(expectedLock, hashedAddress, offsetof(hst_entry_t, lock));
    tcg_gen_atomic_compare_and_swap_host_intrinsic_i32(result, newLock, lockAddress, newLock, mem_index);
    int done = gen_new_label();
    tcg_gen_brcond_i32(TCG_COND_EQ, result, newLock, done);

    // todo: debugging code. removeme
    gen_set_label(skipDebugLog);

    // If result != HST_UNLOCKED, then the lock is taken and we should keep retrying.
    tcg_gen_brcondi_i32(TCG_COND_NE, result, HST_UNLOCKED, retry);

    tcg_temp_free_hostptr(hashedAddress);
    tcg_temp_free_hostptr(lockAddress);
    tcg_temp_free_i32(result);
    tcg_temp_free_i32(expectedLock);
    tcg_temp_free_i32(newLock);

    gen_set_label(done);
}

void gen_store_table_unlock(CPUState *env, TCGv_guestptr guestAddress)
{
    TCGv_hostptr hashedAddress = tcg_temp_new_hostptr();
    gen_hash_address(env, hashedAddress, guestAddress);

    TCGv_i32 unlocked = tcg_temp_new_i32();
    tcg_gen_movi_i32(unlocked, HST_UNLOCKED);

    // Unlock the table entry.
    tcg_gen_st32_tl(unlocked, hashedAddress, offsetof(hst_entry_t, lock));

    tcg_temp_free_hostptr(hashedAddress);
    tcg_temp_free_i32(unlocked);
}

uintptr_t address_hash(CPUState* cpu_env, target_ulong address)
{
    uintptr_t table_offset = (uintptr_t) (cpu_env->store_table);
#if TARGET_LONG_BITS == 64 && HOST_LONG_BITS == 32
    uintptr_t hashed_address = (uint32_t) (0xffffffff & address)
#else
    uintptr_t hashed_address = address;
#endif
    hashed_address &= hst_guest_prefix_mask;
    hashed_address |= table_offset;
    return hashed_address;
}

#ifndef __llvm__
#pragma GCC diagnostic pop
#endif

#include "hash-table-store-test.h"

#include "cpu.h"
#include "infrastructure.h"
#include "tcg-op.h"
#include "tcg-op-atomic.h"
#include "atomic-intrinsics.h"

#include "debug.h"

static uint64_t hst_guest_address_mask = 0;
static uint64_t hst_table_entries_count = 0;

static void calculate_hst_mask(const uint8_t store_table_bits)
{
    //  _bits means how many bits are used in the addresses, not how many bits
    //  large the contents is. I.e. if an entry is 64 bits large it uses 3 bits.
    const uint64_t content_bits = sizeof(uintptr_t) * 8 - (uint64_t)store_table_bits;

    const uint64_t table_size_bytes = 1ULL << content_bits;
    const uint64_t entry_size_bytes = sizeof(store_table_entry_t);
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

    const uint64_t n_bits = (1ULL << store_table_bits) - 1;
    const uint64_t table_mask = n_bits << content_bits;
    const uint64_t interior_mask = ~table_mask;

    const uint64_t entry_mask = sizeof(store_table_entry_t) - 1;
    const uint64_t alignment_mask = ~entry_mask;
    hst_guest_address_mask = interior_mask & alignment_mask;
}

void initialize_store_table(store_table_entry_t *store_table, uint8_t store_table_bits, bool after_deserialization)
{
    calculate_hst_mask(store_table_bits);

    tlib_printf(LOG_LEVEL_DEBUG, "%s: initializing with ptr 0x%016llx", __func__, store_table);
    tlib_assert(hst_table_entries_count != 0);
    bool all_entries_unlocked = true;
    /* Initialize store table. */
    for(uint64_t i = 0; i < hst_table_entries_count; i++) {
        if(after_deserialization) {
            /*
             * Every entry must already be unlocked when deserializing, because
             * we assume that when serializing the current instruction gets to
             * finish executing, meaning it _should_ have been able to release
             * its store table lock. If the lock was never released, something
             * has gone wrong.
             */
            uint32_t locked_by_cpu_id = store_table[i].lock;
            if(unlikely(locked_by_cpu_id != HST_UNLOCKED)) {
                tlib_printf(LOG_LEVEL_WARNING,
                            "%s: serialized store table entry at offset 0x%x contains dangling lock for cpu %u", __func__, i,
                            locked_by_cpu_id);
                all_entries_unlocked = false;
            }
        } else {
            /*
             * Initialize a new entry from scratch.
             */
            store_table[i].last_accessed_by_core_id = HST_NO_CORE;
            store_table[i].lock = HST_UNLOCKED;
        }
    }
    tlib_assert(all_entries_unlocked);
}

/* Hashes `guest_address` and places the result in `hashed_address`. */
static void gen_hash_address(CPUState *env, TCGv_hostptr hashed_address, TCGv_guestptr guest_address)
{
    tcg_gen_mov_tl(hashed_address, guest_address);

    //  Zero out upper bits of address, to make room for the address of the table.
    //  Zero out lower bits, both for alignment and to make room for the fine-grained lock.
    tcg_gen_andi_i64(hashed_address, hashed_address, hst_guest_address_mask);

    //  Replace the upper bits of address with start of table.
    uintptr_t store_table_address = (uintptr_t)env->store_table;
    tcg_gen_ori_i64(hashed_address, hashed_address, store_table_address);
}

//  Abstract away what the actual core id comes from.
uint32_t get_core_id(CPUState *env)
{
    //  Use the atomic_id as the "core id" required for HST.
    return env->atomic_id;
}

/*  Result is 1 if check succeeds, 0 otherwise. */
void gen_store_table_check(CPUState *env, TCGv result, TCGv_guestptr guest_address)
{
    TCGv_hostptr hashed_address = tcg_temp_new_hostptr();
    gen_hash_address(env, hashed_address, guest_address);

    //  Load core id from store table, to see which core last accessed the address.
    tcg_gen_ld32u_tl(result, hashed_address, offsetof(store_table_entry_t, last_accessed_by_core_id));

    //  See if the current core is the one who last accessed the reserved address.
    //  returns 1 if condition succeeds, 0 otherwise.
    tcg_gen_setcondi_tl(TCG_COND_EQ, result, result, get_core_id(env));

    tcg_temp_free_hostptr(hashed_address);
}

/* Debug assert that ensures the current core owns the hash table entry lock
 * of the entry associated with the given `guest_address`.
 */
void ensure_entry_locked(CPUState *env, TCGv_guestptr guest_address, const char *function_name)
{
#ifdef DEBUG
    TCGv_hostptr hashed_address = tcg_temp_local_new_hostptr();
    gen_hash_address(env, hashed_address, guest_address);

    TCGv_i32 lock = tcg_temp_local_new_i32();
    //  Load lock from store table, to see which core holds it.
    tcg_gen_ld32u_tl(lock, hashed_address, offsetof(store_table_entry_t, lock));

    int done = gen_new_label();
    uint32_t core_id = get_core_id(env);
    //  Check if the lock is owned by the current core.
    tcg_gen_brcondi_i32(TCG_COND_EQ, lock, core_id, done);

    //  Lock isn't owned by the current core, abort.
    generate_log(0, "%s: %s: hash table entry lock for guest address:", __func__, function_name);
    generate_var_log(guest_address);
    generate_log(0, "is not held by current core (id %u), it is held by:", core_id);
    generate_var_log(lock);
    generate_backtrace_print();
    gen_helper_abort();

    gen_set_label(done);

    tcg_temp_free_i32(lock);
    tcg_temp_free_hostptr(hashed_address);
#endif
}

void gen_store_table_set(CPUState *env, TCGv_guestptr guest_address)
{
    ensure_entry_locked(env, guest_address, __func__);

    TCGv_hostptr hashed_address = tcg_temp_local_new_hostptr();
    gen_hash_address(env, hashed_address, guest_address);

    TCGv_i32 core_id = tcg_const_i32(get_core_id(env));

    //  The hashed address now points to the table entry for the core id, so store it there.
    //  Note that the table entry update occurs atomically, with a single store.
    tcg_gen_st32_tl(core_id, hashed_address, offsetof(store_table_entry_t, last_accessed_by_core_id));
    //  Memory barrier to ensure that this store doesn't get reordered with a store
    //  that will release the lock.
    tcg_gen_mb(TCG_MO_ST_ST);

    tcg_temp_free_hostptr(hashed_address);
    tcg_temp_free_i32(core_id);
}

static void gen_store_table_lock_address(CPUState *env, TCGv_guestptr guest_address, tcg_target_long locked_address_offset)
{
    TCGv_hostptr hashed_address = tcg_temp_local_new_hostptr();
    gen_hash_address(env, hashed_address, guest_address);

    //  Add the offset of the lock field, since we want to access the lock and not the core id.
    TCGv_hostptr lock_address = tcg_temp_local_new_hostptr();
    tcg_gen_addi_i64(lock_address, hashed_address, offsetof(store_table_entry_t, lock));

    TCGv_i32 expected_lock = tcg_const_local_i32(HST_UNLOCKED);

    //  Acquiring the lock means storing this core's id.
    TCGv_i32 new_lock = tcg_const_local_i32(get_core_id(env));

    /*  We need to check two cases, hence the two branching instructions
     *  after the initial CAS. The table entry is either unlocked,
     *  locked by another thread, or locked by the current thread.
     *
     *                         │
     * ┌───────────────────────▼────────────────────────────┐
     * │result = CAS(expected_lock, lock_address, new_lock) |◄────────┐
     * └───────────────────────┬────────────────────────────┘         │
     *        true    ┌────────▼──────────┐                           │
     *         ┌──────┼ result == core_id │ "already locked by me?"   │
     *         ▼      └────────┬──────────┘                           │
     *       abort             │ false                                │
     *                ┌────────▼───────────────┐  true                │
     *      "locked?" │ result != HST_UNLOCKED ┼──────────────────────┘
     *                └────────┬───────────────┘
     *                         │ false
     *                         ▼
     *                   lock acquired!
     */
    int retry = gen_new_label();
    gen_set_label(retry);

    TCGv_i32 result = tcg_temp_local_new_i32();

    //  Optimistically try to atomically acquire the lock (only succeeds if it's currently unlocked).
    tcg_gen_atomic_compare_and_swap_host_intrinsic_i32(result, expected_lock, lock_address, new_lock);

    int start_retrying = gen_new_label();
    //  Locks are not reentrant, so it is an implementation bug
    //  if the lock is already taken by this core.
    tcg_gen_brcondi_i32(TCG_COND_NE, result, get_core_id(env), start_retrying);
    //  Abort if result == get_core_id(env) (reentrant lock attempt)
    TCGv_hostptr abort_message =
        tcg_const_hostptr((uintptr_t)"Attempted to acquire a store table lock that this CPU already holds");
    gen_helper_abort_message(abort_message);
    tcg_temp_free_hostptr(abort_message);

    gen_set_label(start_retrying);
    //  If result != HST_UNLOCKED, then the lock is taken, and we should keep retrying.
    tcg_gen_brcondi_i32(TCG_COND_NE, result, HST_UNLOCKED, retry);

    //  Lock is now owned by the current core.

    //  Update cpu's currently locked address.
    tcg_gen_st_tl(guest_address, cpu_env, locked_address_offset);

    tcg_temp_free_hostptr(hashed_address);
    tcg_temp_free_hostptr(lock_address);
    tcg_temp_free_i32(expected_lock);
    tcg_temp_free_i32(new_lock);
    tcg_temp_free_i32(result);
}

void gen_store_table_lock(CPUState *env, TCGv_guestptr guest_address)
{
    gen_store_table_lock_address(env, guest_address, offsetof(CPUState, locked_address));
}

static void gen_store_table_unlock_address(CPUState *env, TCGv_guestptr guest_address, tcg_target_long locked_address_offset)
{
    ensure_entry_locked(env, guest_address, __func__);

    TCGv_hostptr hashed_address = tcg_temp_new_hostptr();
    gen_hash_address(env, hashed_address, guest_address);

    TCGv_i32 unlocked = tcg_const_i32(HST_UNLOCKED);

    //  Unlock the table entry.
    tcg_gen_st32_tl(unlocked, hashed_address, offsetof(store_table_entry_t, lock));
    //  Emit a barrier to ensure that the store is visible to other processors.
    tcg_gen_mb(TCG_MO_ST_ST);

    //  Update cpu's currently locked address.
    TCGv_guestptr null = tcg_const_tl(0);
    tcg_gen_st_tl(null, cpu_env, locked_address_offset);

    tcg_temp_free(null);
    tcg_temp_free_hostptr(hashed_address);
    tcg_temp_free_i32(unlocked);
}

void gen_store_table_unlock(CPUState *env, TCGv_guestptr guest_address)
{
    gen_store_table_unlock_address(env, guest_address, offsetof(CPUState, locked_address));
}

uintptr_t address_hash(CPUState *env, target_ulong guest_address)
{
    uintptr_t table_offset = (uintptr_t)env->store_table;
    uintptr_t hashed_address = guest_address;
    hashed_address &= hst_guest_address_mask;
    hashed_address |= table_offset;
    return hashed_address;
}

static void gen_store_table_lock_high(CPUState *env, TCGv_guestptr guest_address)
{
    gen_store_table_lock_address(env, guest_address, offsetof(CPUState, locked_address_high));
}

static void gen_store_table_unlock_high(CPUState *env, TCGv_guestptr guest_address)
{
    gen_store_table_unlock_address(env, guest_address, offsetof(CPUState, locked_address_high));
}

void gen_store_table_lock_128(CPUState *env, TCGv_guestptr guest_addr_low, TCGv_guestptr guest_addr_high)
{
#ifdef DEBUG
    int addr_is_equal = gen_new_label();
    TCGv_guestptr comp_addr = tcg_temp_local_new_ptr();
    tcg_gen_addi_tl(comp_addr, guest_addr_low, sizeof(uint64_t));
    tcg_gen_brcond_tl(TCG_COND_EQ, comp_addr, guest_addr_high, addr_is_equal);

    /* If we didn't jump, the address pair is illegal */
    generate_log(0, "Illegal pair of guest addresses in %s", __func__);
    generate_log(0, "guest_addr_low:");
    generate_var_log(guest_addr_low);
    generate_log(0, "guest_addr_high:");
    generate_var_log(guest_addr_high);
    generate_log(0, "Should be: guest_addr_high == guest_addr_low+8bytes");
    generate_backtrace_print();
    gen_helper_abort();

    gen_set_label(addr_is_equal);
    tcg_temp_free_ptr(comp_addr);
#endif

    /* To avoid deadlocks, the order is important and should always lock the lowest 64 bits first */
    gen_store_table_lock(env, guest_addr_low);
    gen_store_table_lock_high(env, guest_addr_high);
}

void gen_store_table_unlock_128(CPUState *env, TCGv_guestptr guest_addr_low, TCGv_guestptr guest_addr_high)
{
    gen_store_table_unlock(env, guest_addr_low);
    gen_store_table_unlock_high(env, guest_addr_high);
}

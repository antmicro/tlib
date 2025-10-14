#include "hash-table-store-test.h"

#include "cpu.h"
#include "infrastructure.h"
#include "tcg-op.h"

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

void initialize_store_table(store_table_entry_t *store_table, uint8_t store_table_bits)
{
    calculate_hst_mask(store_table_bits);

    tlib_printf(LOG_LEVEL_DEBUG, "%s: initializing with ptr 0x%016llx", __func__, store_table);
    tlib_assert(hst_table_entries_count != 0);
    /* Initialize store table. */
    static const store_table_entry_t defaultEntry = { .last_accessed_by_core_id = HST_NO_CORE, .lock = HST_UNLOCKED };
    for(uint64_t i = 0; i < hst_table_entries_count; i++) {
        store_table[i] = defaultEntry;
    }
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

void gen_store_table_set(CPUState *env, TCGv_guestptr guest_address)
{
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

void gen_store_table_lock(CPUState *env, TCGv_guestptr guest_address)
{
    tlib_abortf("gen_store_table_lock: not yet implemented");
}

uintptr_t address_hash(CPUState *env, target_ulong guest_address)
{
    tlib_abortf("gen_store_table_unlock: not yet implemented");
    return 0;
}

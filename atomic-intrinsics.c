#include "cpu.h"
#include "tb-helper.h"

#if __llvm__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

typedef union TCGv_unknown {
    TCGv_i32 size32;
    TCGv_i64 size64;
} TCGv_unknown;

#if defined(TCG_TARGET_HAS_atomic_fetch_add_intrinsic_i32) || defined(TCG_TARGET_HAS_atomic_fetch_add_intrinsic_i64)

/*
 * Branches to `unalignedLabel` if the given `guestAddress` spans two pages.
 */
static void tcg_gen_brcond_page_spanning_check(TCGv_ptr guestAddress, uint16_t dataSize, int unalignedLabel)
{
    TCGv negatedTargetPageMask = tcg_temp_new();
    TCGv maskedAddress = tcg_temp_new();

    //  if ((addr & ~TARGET_PAGE_MASK) + dataSize - 1) >= TARGET_PAGE_SIZE then goto unalignedLabel
    tcg_gen_movi_tl(negatedTargetPageMask, ~TARGET_PAGE_MASK);
    tcg_gen_and_i64(maskedAddress, guestAddress, negatedTargetPageMask);
    tcg_gen_addi_i64(maskedAddress, maskedAddress, dataSize - 1);
    tcg_gen_brcondi_i64(TCG_COND_GEU, maskedAddress, TARGET_PAGE_SIZE, unalignedLabel);

    tcg_temp_free(negatedTargetPageMask);
    tcg_temp_free(maskedAddress);
}

static inline void tcg_try_gen_atomic_fetch_add_intrinsic(TCGv_unknown result, TCGv_ptr guestAddress, TCGv_unknown toAdd,
                                                          uint32_t memIndex, int fallbackLabel, uint8_t size)
{
    tlib_assert(size == 64 || size == 32);

    //  If the address spans two pages, it can't be implemented by a single host intrinsic,
    //  will have to fall back on the global memory lock.
    tcg_gen_brcond_page_spanning_check(guestAddress, size, fallbackLabel);

    /*
     * If address is page-aligned:
     */
    TCGv_hostptr hostAddress = tcg_temp_local_new_hostptr();
    TCGv_i32 memIndexVar = tcg_temp_new_i32();
    tcg_gen_movi_i32(memIndexVar, memIndex);
    if(size == 64) {
        gen_helper_translate_page_aligned_address_and_fill_tlb_u64(hostAddress, guestAddress, memIndexVar);
    } else {
        gen_helper_translate_page_aligned_address_and_fill_tlb_u32(hostAddress, guestAddress, memIndexVar);
    }
    tcg_temp_free(memIndexVar);

    //  If it's an MMIO address, it will be returned unchanged.
    //  Since we can't handle that case, we'll have to jump to the fallback.
    tcg_gen_brcond_i64(TCG_COND_EQ, hostAddress, guestAddress, fallbackLabel);

    /*
     * If address is both page-aligned and _not_ MMIO:
     */
    if(size == 64) {
        tcg_gen_atomic_fetch_add_intrinsic_i64(result.size64, hostAddress, toAdd.size64);
    } else {
        tcg_gen_atomic_fetch_add_intrinsic_i32(result.size32, hostAddress, toAdd.size32);
    }
    tcg_temp_free_hostptr(hostAddress);
}

#endif

#if TCG_TARGET_HAS_atomic_fetch_add_intrinsic_i32
/*
 * Attempts to generate an atomic fetch and add, possibly failing and needing a fallback.
 *
 * The `fallback` label is jumped to if it is not feasible to atomically operate on it.
 */
void tcg_try_gen_atomic_fetch_add_intrinsic_i32(TCGv_i32 result, TCGv_ptr guestAddress, TCGv_i32 toAdd, uint32_t memIndex,
                                                int fallbackLabel)
{
    TCGv_unknown retUnknown, toAddUnknown;
    retUnknown.size32 = result;
    toAddUnknown.size32 = toAdd;
    tcg_try_gen_atomic_fetch_add_intrinsic(retUnknown, guestAddress, toAddUnknown, memIndex, fallbackLabel, 32);
}
#endif

#if TCG_TARGET_HAS_atomic_fetch_add_intrinsic_i64
/*
 * Attempts to generate an atomic fetch and add, possibly failing and needing a fallback.
 *
 * The `fallback` label is jumped to if it is not feasible to atomically operate on it.
 */
void tcg_try_gen_atomic_fetch_add_intrinsic_i64(TCGv_i64 result, TCGv_ptr guestAddress, TCGv_i64 toAdd, uint32_t memIndex,
                                                int fallbackLabel)
{
    TCGv_unknown retUnknown, toAddUnknown;
    retUnknown.size64 = result;
    toAddUnknown.size64 = toAdd;
    tcg_try_gen_atomic_fetch_add_intrinsic(retUnknown, guestAddress, toAddUnknown, memIndex, fallbackLabel, 64);
}
#endif

#ifdef __llvm__
#pragma clang diagnostic pop
#else
#pragma GCC diagnostic pop
#endif

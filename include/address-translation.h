#pragma once

/*
 *  Translates a page-aligned guest address into its corresponding host address,
 *  filling the translation lookaside buffer (TLB) if it wasn't already cached.
 *  The access width is 32.
 *
 *  The address must not span pages, as the pages are not necessarily contiguous
 *  in memory and parts of the value may therefore lie at completely different
 *  host addresses.
 *
 *  In case the guest address corresponds to an MMIO address, it is returned unchanged.
 */
uintptr_t translate_page_aligned_address_and_fill_tlb_u32(target_ulong addr, uint32_t mmu_idx, void *return_address);

/*
 *  Translates a page-aligned guest address into its corresponding host address,
 *  filling the translation lookaside buffer (TLB) if it wasn't already cached.
 *  The access width is 64.
 *
 *  The address must not span pages, as the pages are not necessarily contiguous
 *  in memory and parts of the value may therefore lie at completely different
 *  host addresses.
 *
 *  In case the guest address corresponds to an MMIO address, it is returned unchanged.
 */
uintptr_t translate_page_aligned_address_and_fill_tlb_u64(target_ulong addr, uint32_t mmu_idx, void *return_address);
uintptr_t translate_page_aligned_address_and_fill_tlb_u128(target_ulong addr, uint32_t mmu_idx, void *return_address);

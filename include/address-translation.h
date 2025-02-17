#pragma once

uintptr_t translate_page_aligned_address_and_fill_tlb_u32(target_ulong addr, uint32_t mmu_idx);
uintptr_t translate_page_aligned_address_and_fill_tlb_u64(target_ulong addr, uint32_t mmu_idx);

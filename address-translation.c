#include "cpu.h"

uintptr_t REGPARM translate_page_aligned_address_and_fill_tlb(target_ulong addr, uint32_t mmu_idx, uint16_t dataSize)
{
    int index;
    target_ulong tlb_addr;
    uintptr_t addend;

    index = (addr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);

redo:
    tlb_addr = cpu->tlb_table[mmu_idx][index].addr_read & ~TLB_ONE_SHOT;

    bool addresses_match = (addr & TARGET_PAGE_MASK) == (tlb_addr & TARGET_PAGE_MASK);
    bool is_invalid = tlb_addr & TLB_INVALID_MASK;
    if(likely(addresses_match && !is_invalid)) {
        if(unlikely((tlb_addr & TLB_MMIO) == TLB_MMIO)) {
            /* IO access */
            tlib_printf(LOG_LEVEL_WARNING, "%s: Atomically accessing MMIO addr 0x%llx mmu_idx %d", __func__, addr, mmu_idx);
            return addr;
        }

#if DEBUG
        if(unlikely(((addr & ~TARGET_PAGE_MASK) + dataSize - 1) >= TARGET_PAGE_SIZE)) {
            /* slow unaligned access (it spans two pages) */
            tcg_abortf("%s: Spanning two pages not supported on addr 0x%llx mmu_idx %d", __func__, addr, mmu_idx);
        }
#endif

        /* unaligned/aligned access in the same page */
        addend = cpu->tlb_table[mmu_idx][index].addend;
        return addr + addend;
    } else {
        void *retaddr = GETPC();
        /* the page is not in the TLB : fill it */
        if(!tlb_fill(cpu, addr, PAGE_READ, mmu_idx, retaddr, 0, dataSize)) {
            goto redo;
        }
    }
    tlib_printf(LOG_LEVEL_DEBUG, "%s: Failed to fill TLB", __func__);
    return (uintptr_t)NULL;
}

uintptr_t translate_page_aligned_address_and_fill_tlb_u32(target_ulong addr, uint32_t mmu_idx)
{
    return translate_page_aligned_address_and_fill_tlb(addr, mmu_idx, sizeof(uint32_t));
}

uintptr_t translate_page_aligned_address_and_fill_tlb_u64(target_ulong addr, uint32_t mmu_idx)
{
    return translate_page_aligned_address_and_fill_tlb(addr, mmu_idx, sizeof(uint64_t));
}

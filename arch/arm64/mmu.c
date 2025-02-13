/*
 * Copyright (c) Antmicro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cpu.h"
#include "mmu.h"
#include "syndrome.h"

const int ips_bits[] = { 32, 36, 40, 42, 44, 48, 52 };

/* Check section/page access permissions.
   Returns the page protection flags, or zero if the access is not
   permitted.  */
static inline int check_ap(int ap, int is_user)
{
    int prot = 0;

    switch(ap) {
        case 0:
            prot |= is_user ? 0 : PAGE_READ | PAGE_WRITE;
            break;
        case 1:
            prot |= PAGE_READ | PAGE_WRITE;
            break;
        case 2:
            prot |= is_user ? 0 : PAGE_READ;
            break;
        case 3:
            prot |= PAGE_READ;
            break;
        default:
            tlib_assert_not_reached();
    }

    return prot;
}

static uint64_t get_table_address(CPUState *env, target_ulong address, uint64_t base_addr, int page_size_shift, int level)
{
    uint64_t table_offset = 0;
    uint64_t mask = (1 << MMU_Ln_XLAT_VA_SIZE_SHIFT(page_size_shift)) - 1;
    table_offset = (address >> MMU_LEVEL_TO_VA_SIZE_SHIFT(level, page_size_shift)) & mask;

    return base_addr + (table_offset * 8);
}

static inline void parse_desc(uint32_t va_size_shift, uint64_t desc, uint32_t ips, target_ulong address, int is_user,
                              target_ulong *phys_ptr, int *prot, target_ulong *page_size)
{
    int ap, uxn, pxn;

    *phys_ptr = extract64(desc, va_size_shift, ips_bits[ips] - va_size_shift) << va_size_shift |
                extract64(address, 0, va_size_shift);

#if DEBUG
    tlib_printf(LOG_LEVEL_NOISY, "%s: phys_addr=0x%" PRIx64, __func__, *phys_ptr);
#endif

    ap = extract64(desc, 6, 2);
    uxn = extract64(desc, 54, 1);
    pxn = extract64(desc, 53, 1);

    *prot = check_ap(ap, is_user);
    if((is_user && uxn == 0) || (!is_user && pxn == 0)) {
        *prot |= PAGE_EXEC;
    }

    *page_size = 1 << va_size_shift;
}

void handle_mmu_fault_v8(CPUState *env, target_ulong address, int access_type, uintptr_t return_address, bool suppress_faults,
                         ISSFaultStatusCode fault_code, bool at_instruction_or_cache_maintenance,
                         bool s1ptw /* "stage 2 fault on an access made for a stage 1 translation table walk" */)
{
    //  The 'suppress_faults' (AKA 'no_page_fault') argument can be used to skip translation failure handling.
    if(unlikely(suppress_faults)) {
        return;
    }

    uint32_t target_el = exception_target_el(env);
    bool same_el = target_el == arm_current_el(env);

    uint32_t exception_type;
    uint64_t syndrome = 0;
    if(access_type == ACCESS_INST_FETCH) {
        exception_type = EXCP_PREFETCH_ABORT;

        syndrome = syn_instruction_abort(same_el, s1ptw, fault_code);
    } else {
        exception_type = EXCP_DATA_ABORT;

        bool is_write = access_type == ACCESS_DATA_STORE;
        bool wnr = is_write || at_instruction_or_cache_maintenance;

        //  TODO: Get partial syndrome from insn_start params instead.
        syndrome = syn_data_abort_no_iss(same_el, 0, 0, at_instruction_or_cache_maintenance, s1ptw, wnr, fault_code);
    }

    env->exception.vaddress = address;

    //  'return_address' is a host address for the generated code that triggered TLB miss.
    if(return_address) {
        tlib_assert(env->current_tb);

        //  The state has to be restored before calling 'raise_exception'. Otherwise, 'env->exception.syndrome'
        //  set in 'raise_exception' gets overwritten in 'restore_state_to_opc'.
        cpu_restore_state_and_restore_instructions_count(env, env->current_tb, return_address, true);
    }

    //  Both always end with longjmp to the main cpu loop so they never return.
    if(access_type == ACCESS_INST_FETCH) {
        raise_exception_without_block_end_hooks(env, exception_type, syndrome, target_el);
    } else {
        raise_exception(env, exception_type, syndrome, target_el);
    }
    tlib_assert_not_reached();
}

static inline uint32_t get_tcr_ips_offset(CPUState *env, uint32_t current_el)
{
    //  We need to get the real EL here (not the one in which we started translating)
    //  Otherwise we can get an invalid EL after exception return
    const int translation_el = address_translation_el(env, current_el);
    if(translation_el == 1) {
        return 32;
    } else if(translation_el == 2 && (arm_hcr_el2_eff(env) & HCR_E2H)) {
        return 32;
    } else {
        return 16;
    }
}

int get_phys_addr_v8(CPUState *env, target_ulong address, int access_type, int mmu_idx, uintptr_t return_address,
                     bool suppress_faults, target_ulong *phys_ptr, int *prot, target_ulong *page_size,
                     bool at_instruction_or_cache_maintenance)
{
    ARMMMUIdx arm_mmu_idx = core_to_aa64_mmu_idx(mmu_idx);
    uint32_t current_el = arm_mmu_idx_to_el(arm_mmu_idx);
    uint32_t tcr_ips_offset = get_tcr_ips_offset(env, current_el);

    uint64_t tcr = arm_tcr(env, current_el);
    uint64_t ttbr = 0;
    uint64_t tsz = 0;
    uint64_t tg = 0;
    uint32_t ips = 0;
    uint32_t page_size_shift = 0;

    //  Check bit 55 to determine which TTBR to use
    //  Bit 55 is used instead of checking all top bits above TxSZ, as bits >55
    //  can be used for tagged pointers, and bit 55 is valid for all region sizes.
    if(extract64(address, 55, 1)) {
        ttbr = arm_ttbr1(env, current_el);
        tsz = extract64(tcr, 16, 6);  //  T1SZ
        tg = extract64(tcr, 30, 2);   //  TG1
        switch(tg) {
            case 1:
                page_size_shift = 14;  //  16kB
                break;
            case 2:
                page_size_shift = 12;  //  4kB
                break;
            case 3:
                page_size_shift = 16;  //  64kB
                break;
            default:
                tlib_abortf("Incorrect TG1 value: %d", tg);
                break;
        }
    } else {
        ttbr = arm_ttbr0(env, current_el);
        tsz = extract64(tcr, 0, 6);  //  T0SZ
        tg = extract64(tcr, 14, 2);  //  TG0
        switch(tg) {
            case 0:
                page_size_shift = 12;  //  4kB
                break;
            case 1:
                page_size_shift = 16;  //  64kB
                break;
            case 2:
                page_size_shift = 14;  //  16kB
                break;
            default:
                tlib_abortf("Incorrect TG0 value: %d", tg);
                break;
        }
    }

    ips = extract64(tcr, tcr_ips_offset, 3);

#if DEBUG
    tlib_printf(LOG_LEVEL_NOISY, "%s: vaddr=0x%" PRIx64 " attbr=0x%" PRIx64 ", tsz=%d, tg=%d, page_size_shift=%d", __func__,
                address, ttbr, tsz, tg, page_size_shift);
#endif

    //  Table address is low 48 bits of TTBR. The CnP bit is currently ignored.
    uint64_t table_addr = extract64(ttbr, 0, 48) & (UINT64_MAX ^ TTBR_CNP);

    int level;
    ISSFaultStatusCode fault_code = -1;
    uint64_t desc_addr;
    uint64_t desc;
    for(level = MMU_GET_BASE_XLAT_LEVEL(64 - tsz, page_size_shift); level <= MMU_XLAT_LAST_LEVEL; level++) {
        desc_addr = get_table_address(env, address, table_addr, page_size_shift, level);
        desc = ldq_phys(desc_addr);

#if DEBUG
        tlib_printf(LOG_LEVEL_NOISY, "%s: level=%d, desc=0x%" PRIx64 " (addr: 0x%" PRIx64 ")", __func__, level, desc, desc_addr);
#endif

        uint32_t desc_type = extract64(desc, 0, 2);
        switch(desc_type) {
            case DESCRIPTOR_TYPE_BLOCK_ENTRY:
                if(level == 1 && page_size_shift != 12) {
                    tlib_printf(LOG_LEVEL_ERROR, "%s: block entry allowed on level 1 only with 4K pages!", __func__);
                    goto do_fault;
                }

                if(level > 2) {
                    tlib_printf(LOG_LEVEL_ERROR, "%s: block descriptor not allowed on level %d!", __func__, level);
                    goto do_fault;
                }

                goto success;
            case DESCRIPTOR_TYPE_TABLE_DESCRIPTOR_OR_ENTRY:
                if(level == 3) {
                    goto success;
                }

                table_addr = extract64(desc, page_size_shift, ips_bits[ips] - page_size_shift) << page_size_shift;
#if DEBUG
                tlib_printf(LOG_LEVEL_NOISY, "%s: page_size_shift=%u, ips=%u, ips_bit=%d, EL=%d, addr_EL=%d, e2h=%d", __func__,
                            page_size_shift, ips, ips_bits[ips], current_el, address_translation_el(env, current_el),
                            (arm_hcr_el2_eff(env) & HCR_E2H) > 0);
#endif
                break;
            default:
                //  It's debug because translation failures can be caused by a valid software behaviour.
                //  For example Coreboot uses them to find out the memory size.
                tlib_printf(LOG_LEVEL_DEBUG, "%s: Invalid descriptor type %d!", __func__, desc_type);
                goto do_fault;
        }
    }

success:
    parse_desc(MMU_GET_XLAT_VA_SIZE_SHIFT(level, page_size_shift), desc, ips, address, current_el == 0, phys_ptr, prot,
               page_size);
    if(is_page_access_valid(*prot, access_type)) {
        return TRANSLATE_SUCCESS;
    } else {
        fault_code = SYN_FAULT_PERMISSION_LEVEL_0 + level;
    }

do_fault:
    //  By default, use a Translation Fault status code.
    if(fault_code == -1) {
        fault_code = SYN_FAULT_TRANSLATION_LEVEL_0 + level;
    }
    handle_mmu_fault_v8(env, address, access_type, return_address, suppress_faults, fault_code,
                        at_instruction_or_cache_maintenance, false);
    return TRANSLATE_FAIL;
}

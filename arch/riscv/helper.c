/*
 *  RISC-V emulation helpers
 *
 *  Author: Sagar Karandikar, sagark@eecs.berkeley.edu
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#include "cpu.h"

#include "def-helper.h"
#include "cpu-common.h"
#include "arch_callbacks.h"

// This is used as a part of MISA register, indicating the architecture width.
#if defined(TARGET_RISCV32)
#define RVXLEN ((target_ulong)1 << (TARGET_LONG_BITS - 2))
#elif defined(TARGET_RISCV64)
#define RVXLEN ((target_ulong)2 << (TARGET_LONG_BITS - 2))
#endif

void cpu_reset(CPUState *env)
{
    tlb_flush(env, 1);

    int32_t interrupt_mode = env->interrupt_mode;
    int32_t csr_validation_level = env->csr_validation_level;
    int privilege = env->privilege_architecture;
    target_ulong mhartid = env->mhartid;
    target_ulong misa_mask = env->misa_mask;
    target_ulong silenced_extensions = env->silenced_extensions;
    int32_t custom_instructions_count = env->custom_instructions_count;
    custom_instruction_descriptor_t custom_instructions[CPU_CUSTOM_INSTRUCTIONS_LIMIT];
    memcpy(custom_instructions, env->custom_instructions,
           sizeof(custom_instruction_descriptor_t) * CPU_CUSTOM_INSTRUCTIONS_LIMIT);
    uint64_t custom_csrs[CSRS_SLOTS];
    memcpy(custom_csrs, env->custom_csrs,
           sizeof(uint64_t) * CSRS_SLOTS);
    target_ulong vlenb = env->vlenb;
    target_ulong elen = env->elen;

    memset(env, 0, offsetof(CPUState, breakpoints));

    env->interrupt_mode = interrupt_mode;
    env->csr_validation_level = csr_validation_level;
    env->mhartid = mhartid;
    env->privilege_architecture = privilege;
    env->misa = misa_mask;
    env->misa_mask = misa_mask;
    env->silenced_extensions = silenced_extensions;
    env->priv = PRV_M;
    env->mtvec = DEFAULT_MTVEC;
    env->pc = DEFAULT_RSTVEC;
    env->exception_index = EXCP_NONE;
    set_default_nan_mode(1, &env->fp_status);
    set_default_mstatus();
    env->custom_instructions_count = custom_instructions_count;
    memcpy(env->custom_instructions, custom_instructions,
           sizeof(custom_instruction_descriptor_t) * CPU_CUSTOM_INSTRUCTIONS_LIMIT);
    memcpy(env->custom_csrs, custom_csrs,
           sizeof(uint64_t) * CSRS_SLOTS);
    env->pmp_napot_grain = -1;

    env->vlenb = vlenb ? vlenb : 64;
    env->elen = elen ? elen : 64;
}

int get_interrupts_in_order(target_ulong pending_interrupts, target_ulong priv)
{
    /* Interrupt shoud be taken in order:
     * External = priv | 0x1000
     * Software = priv | 0x0000
     * Timer    = priv | 0x0100
     * We should take interrupts for priority >= env->priv
     * If no interrupt should be taken returns -1 */
    int bit = EXCP_NONE;
    if (pending_interrupts & 0b1111) {
        for (int i = PRV_M; i >= priv; i--) {
            if ((1 << (bit = i | 8)) & pending_interrupts) { /* external int */
                break;
            }
            if ((1 << (bit = i | 0)) & pending_interrupts) { /* software int */
                break;
            }
            if ((1 << (bit = i | 4)) & pending_interrupts) { /* timer int */
                break;
            }
        }
        return bit;
    } else {
        /* synchronous exceptions */
        bit = ctz64(pending_interrupts);
        return (bit >= TARGET_LONG_BITS - 1) ? EXCP_NONE : bit;
    }
}

/*
 * Return RISC-V IRQ number if an interrupt should be taken, else -1.
 * Used in cpu-exec.c
 *
 * Adapted from Spike's processor_t::take_interrupt()
 */
int riscv_cpu_hw_interrupts_pending(CPUState *env)
{
    target_ulong pending_interrupts = env->mip & env->mie;
    target_ulong priv = env->priv;
    target_ulong enabled_interrupts = (target_ulong) - 1UL;

    switch (priv) {
    /* Disable interrupts for lower privileges, if interrupt is not delegated it is for higher level */
    case PRV_M:
        pending_interrupts &= ~((IRQ_SS | IRQ_ST | IRQ_SE) & env->mideleg);                /* fall through */
    case PRV_S:
        /* For future use, extension N not implemented yet */
        pending_interrupts &= ~((IRQ_US | IRQ_UT | IRQ_UE) & env->mideleg & env->sideleg); /* fall through */
    case PRV_U:
        break;
    }

    if (priv == PRV_M && !get_field(env->mstatus, MSTATUS_MIE)) {
        enabled_interrupts = 0;
    } else if (priv == PRV_S && !get_field(env->mstatus, MSTATUS_SIE)) {
        enabled_interrupts &= ~(env->mideleg);
    }

    enabled_interrupts &= pending_interrupts;

    if (!enabled_interrupts) {
        return EXCP_NONE;
    }

    return (env->privilege_architecture >= RISCV_PRIV1_11) ? get_interrupts_in_order(enabled_interrupts, priv) : ctz64(
        enabled_interrupts);
}

/* get_physical_address - get the physical address for this virtual address
 *
 * Do a page table walk to obtain the physical address corresponding to a
 * virtual address. Returns 0 if the translation was successful
 *
 * Adapted from Spike's mmu_t::translate and mmu_t::walk
 *
 */
static int get_physical_address(CPUState *env, target_phys_addr_t *physical, int *prot, target_ulong address, int access_type,
                                int mmu_idx)
{
    /* NOTE: the env->pc value visible here will not be
     * correct, but the value visible to the exception handler
     * (riscv_cpu_do_interrupt) is correct */
    int mode = mmu_idx;

    if (mode == PRV_M && access_type != ACCESS_INST_FETCH) {
        if (get_field(env->mstatus, MSTATUS_MPRV)) {
            mode = get_field(env->mstatus, MSTATUS_MPP);
        }
    }

    if (mode == PRV_M) {
        *physical = address;
        *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        return TRANSLATE_SUCCESS;
    }

    *prot = 0;

    target_ulong addr = address;
    target_ulong base;

    int levels = 0, ptidxbits = 0, ptesize = 0, vm = 0, sum = 0;
    int mxr = get_field(env->mstatus, MSTATUS_MXR);

    if (env->privilege_architecture >= RISCV_PRIV1_10) {
        base = get_field(env->satp, SATP_PPN) << PGSHIFT;
        sum = get_field(env->mstatus, MSTATUS_SUM);
        vm = get_field(env->satp, SATP_MODE);
        switch (vm) {
        case VM_1_10_SV32:
            levels = 2; ptidxbits = 10; ptesize = 4; break;
        case VM_1_10_SV39:
            levels = 3; ptidxbits = 9; ptesize = 8; break;
        case VM_1_10_SV48:
            levels = 4; ptidxbits = 9; ptesize = 8; break;
        case VM_1_10_SV57:
            levels = 5; ptidxbits = 9; ptesize = 8; break;
        case VM_1_10_MBARE:
            *physical = addr;
            *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
            return TRANSLATE_SUCCESS;
        default:
            tlib_abort("unsupported SATP_MODE value\n");
        }
    } else {
        base = env->sptbr << PGSHIFT;
        sum = !get_field(env->mstatus, MSTATUS_PUM);
        vm = get_field(env->mstatus, MSTATUS_VM);
        switch (vm) {
        case VM_1_09_SV32:
            levels = 2; ptidxbits = 10; ptesize = 4; break;
        case VM_1_09_SV39:
            levels = 3; ptidxbits = 9; ptesize = 8; break;
        case VM_1_09_SV48:
            levels = 4; ptidxbits = 9; ptesize = 8; break;
        case VM_1_09_MBARE:
            *physical = addr;
            *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
            return TRANSLATE_SUCCESS;
        default:
            tlib_abort("unsupported MSTATUS_VM value\n");
        }
    }

    int va_bits = PGSHIFT + levels * ptidxbits;
    target_ulong mask = (1L << (TARGET_LONG_BITS - (va_bits - 1))) - 1;
    target_ulong masked_msbs = (addr >> (va_bits - 1)) & mask;
    if (masked_msbs != 0 && masked_msbs != mask) {
        return TRANSLATE_FAIL;
    }

    int ptshift = (levels - 1) * ptidxbits;
    int i;
    for (i = 0; i < levels; i++, ptshift -= ptidxbits) {
        target_ulong idx = (addr >> (PGSHIFT + ptshift)) & ((1 << ptidxbits) - 1);

        /* check that physical address of PTE is legal */
        target_ulong pte_addr = base + idx * ptesize;
#if defined(TARGET_RISCV64)
        target_ulong pte = ldq_phys(pte_addr);
#else
        target_ulong pte = ldl_phys(pte_addr);
#endif
        target_ulong ppn = pte >> PTE_PPN_SHIFT;

        if (PTE_TABLE(pte)) { /* next level of page table */
            base = ppn << PGSHIFT;
        } else if ((pte & PTE_U) && (mode == PRV_S) &&
                   (!sum || ((env->privilege_architecture >= RISCV_PRIV1_11) && access_type == ACCESS_INST_FETCH))) {
            break;
        } else if (!(pte & PTE_U) && (mode != PRV_S)) {
            break;
        } else if (!(pte & PTE_V) || (!(pte & PTE_R) && (pte & PTE_W))) {
            break;
        } else if (access_type == ACCESS_INST_FETCH ? !(pte & PTE_X) : access_type == ACCESS_DATA_LOAD ?  !(pte & PTE_R) &&
                   !(mxr && (pte & PTE_X)) : !((pte & PTE_R) && (pte & PTE_W))) {
            break;
        } else {
            target_ulong updated_pte = pte | PTE_A | ((access_type == ACCESS_DATA_STORE) * PTE_D);
            if (pte != updated_pte) {
                /* set accessed and possibly dirty bits.
                   we only put it in the TLB if it has the right stuff */
#if defined(TARGET_RISCV64)
                stq_phys(pte_addr, updated_pte);
#else
                stl_phys(pte_addr, updated_pte);
#endif
            }

            /* for superpage mappings, make a fake leaf PTE for the TLB's
               benefit. */
            target_ulong vpn = addr >> PGSHIFT;
            *physical = (ppn | (vpn & ((1L << ptshift) - 1))) << PGSHIFT;

            /* we do not give all prots indicated by the PTE
             * this is because future accesses need to do things like set the
             * dirty bit on the PTE
             *
             * at this point, we assume that protection checks have occurred */
            if ((pte & PTE_X) && access_type == ACCESS_INST_FETCH) {
                *prot |= PAGE_EXEC;
            } else if ((pte & PTE_W) && access_type == ACCESS_DATA_STORE) {
                *prot |= PAGE_WRITE;
            } else if ((pte & PTE_R) && access_type == ACCESS_DATA_LOAD) {
                *prot |= PAGE_READ;
            } else {
                tlib_abort("err in translation prots");
            }
            return TRANSLATE_SUCCESS;
        }
    }
    return TRANSLATE_FAIL;
}

static void raise_mmu_exception(CPUState *env, target_ulong address, int access_type)
{
    int page_fault_exceptions =
        (env->privilege_architecture >= RISCV_PRIV1_10) && get_field(env->satp, SATP_MODE) != VM_1_10_MBARE;
    int exception = 0;
    if (access_type == ACCESS_INST_FETCH) {        /* inst access */
        exception = page_fault_exceptions ? RISCV_EXCP_INST_PAGE_FAULT : RISCV_EXCP_INST_ACCESS_FAULT;
        env->badaddr = address;
    } else if (access_type == ACCESS_DATA_STORE) { /* store access */
        exception = page_fault_exceptions ? RISCV_EXCP_STORE_PAGE_FAULT : RISCV_EXCP_STORE_AMO_ACCESS_FAULT;
        env->badaddr = address;
    } else if (access_type == ACCESS_DATA_LOAD) {  /* load access */
        exception = page_fault_exceptions ? RISCV_EXCP_LOAD_PAGE_FAULT : RISCV_EXCP_LOAD_ACCESS_FAULT;
        env->badaddr = address;
    } else {
        tlib_abortf("Unsupported mmu exception raised: %d", access_type);
    }
    env->exception_index = exception;
}

target_phys_addr_t cpu_get_phys_page_debug(CPUState *env, target_ulong addr)
{
    target_phys_addr_t phys_addr;
    int prot;
    int mem_idx = cpu_mmu_index(env);

    if (get_physical_address(env, &phys_addr, &prot, addr, ACCESS_DATA_LOAD, mem_idx)) {
        return -1;
    }
    return phys_addr;
}

/*
 * Assuming system mode, only called in tlb_fill
 */
int cpu_handle_mmu_fault(CPUState *env, target_ulong address, int access_type, int mmu_idx, int access_width)
{
    target_phys_addr_t pa = 0;
    int prot;
    int overlapping_region_id;
    int ret = TRANSLATE_FAIL;
    target_ulong page_size = TARGET_PAGE_SIZE;

    ret = get_physical_address(env, &pa, &prot, address, access_type, mmu_idx);
    if (!pmp_hart_has_privs(env, pa, access_width, 1 << access_type)) {
        ret = TRANSLATE_FAIL;
    }
    if (ret == TRANSLATE_SUCCESS) {
        overlapping_region_id = pmp_find_overlapping(env, pa & TARGET_PAGE_MASK, TARGET_PAGE_SIZE, 0);

        // are there any PMP regions defined for this page?
        if (overlapping_region_id != -1) {
            // does the PMP region cover the whole page?
            if (env->pmp_state.addr[overlapping_region_id].sa > (pa & TARGET_PAGE_MASK)
                || env->pmp_state.addr[overlapping_region_id].ea < (pa & TARGET_PAGE_MASK) + TARGET_PAGE_SIZE - 1)
            {
                // this effectively makes the tlb page entry one-shot:
                // thanks to this every access to this page will be verified against PMP
                page_size = access_width;
            }
        }

        tlb_set_page(env, address & TARGET_PAGE_MASK, pa & TARGET_PAGE_MASK, prot, mmu_idx, page_size);
    } else if (ret == TRANSLATE_FAIL) {
        raise_mmu_exception(env, address, access_type);
    }
    return ret;
}

/*
 * Handle Traps
 *
 * Adapted from Spike's processor_t::take_trap.
 *
 */
void do_interrupt(CPUState *env)
{
    if (env->nmi_pending > NMI_NONE) {
        do_nmi(env);
        return;
    }

    if (env->exception_index == EXCP_NONE) {
        return;
    }

    if (env->exception_index == RISCV_EXCP_BREAKPOINT) {
        env->interrupt_request |= CPU_INTERRUPT_EXITTB;
        return;
    }

    if(env->interrupt_begin_callback_enabled) {
        tlib_on_interrupt_begin(env->exception_index);
    }

    target_ulong fixed_cause = 0;
    target_ulong bit = 0;
    uint8_t is_interrupt = 0;

    if (env->exception_index & (RISCV_EXCP_INT_FLAG)) {
        /* hacky for now. the MSB (bit 63) indicates interrupt but cs->exception
           index is only 32 bits wide */
        fixed_cause = env->exception_index & RISCV_EXCP_INT_MASK;
        bit = fixed_cause;
        fixed_cause |= ((target_ulong)1) << (TARGET_LONG_BITS - 1);
        is_interrupt = 1;
    } else {
        /* fixup User ECALL -> correct priv ECALL */
        if (env->exception_index == RISCV_EXCP_U_ECALL) {
            switch (env->priv) {
            case PRV_U:
                fixed_cause = RISCV_EXCP_U_ECALL;
                break;
            case PRV_S:
                fixed_cause = RISCV_EXCP_S_ECALL;
                break;
            case PRV_H:
                fixed_cause = RISCV_EXCP_H_ECALL;
                break;
            case PRV_M:
                fixed_cause = RISCV_EXCP_M_ECALL;
                break;
            }
        } else {
            fixed_cause = env->exception_index;
        }
        bit = fixed_cause;
    }

    uint8_t hasbadaddr =
        (fixed_cause == RISCV_EXCP_ILLEGAL_INST) || (fixed_cause == RISCV_EXCP_INST_ADDR_MIS) ||
        (fixed_cause == RISCV_EXCP_INST_ACCESS_FAULT) || (fixed_cause == RISCV_EXCP_LOAD_ADDR_MIS) ||
        (fixed_cause == RISCV_EXCP_STORE_AMO_ADDR_MIS) || (fixed_cause == RISCV_EXCP_LOAD_ACCESS_FAULT) ||
        (fixed_cause == RISCV_EXCP_STORE_AMO_ACCESS_FAULT) || (fixed_cause == RISCV_EXCP_INST_PAGE_FAULT) ||
        (fixed_cause == RISCV_EXCP_LOAD_PAGE_FAULT) || (fixed_cause == RISCV_EXCP_STORE_PAGE_FAULT);

    if (env->priv == PRV_M || !((is_interrupt ? env->mideleg : env->medeleg) & (1 << bit))) {
        /* handle the trap in M-mode */
        env->mepc = env->pc;
        env->mcause = fixed_cause;
        if (hasbadaddr) {
            env->mtval = env->badaddr;
        }

        /* Lowest bit of MTVEC changes mode to vectored interrupt */
        if ((env->mtvec & 1) && is_interrupt && env->privilege_architecture >= RISCV_PRIV1_10) {
            env->pc = (env->mtvec & ~0x3) + (fixed_cause * 4);
        } else {
            env->pc = env->mtvec & ~0x3;
        }

        target_ulong ms = env->mstatus;
        ms =
            set_field(ms, MSTATUS_MPIE,
                      (env->privilege_architecture >= RISCV_PRIV1_10) ? get_field(ms, MSTATUS_MIE) : get_field(ms,
                                                                                                               1 << env->priv));
        ms = set_field(ms, MSTATUS_MIE, 0);
        ms = set_field(ms, MSTATUS_MPP, env->priv);
        csr_write_helper(env, ms, CSR_MSTATUS);
        riscv_set_mode(env, PRV_M);
    } else {
        /* handle the trap in S-mode */
        env->sepc = env->pc;
        env->scause = fixed_cause;
        if (hasbadaddr) {
            env->stval = env->badaddr;
        }

        /* Lowest bit of STVEC changes mode to vectored interrupt */
        if ((env->stvec & 1) && is_interrupt && (env->privilege_architecture >= RISCV_PRIV1_10)) {
            env->pc = (env->stvec & ~0x3) + (fixed_cause * 4);
        } else {
            env->pc = env->stvec & ~0x3;
        }

        target_ulong s = env->mstatus;
        s =
            set_field(s, SSTATUS_SPIE,
                      (env->privilege_architecture >= RISCV_PRIV1_10) ? get_field(s, SSTATUS_SIE) : get_field(s, 1 << env->priv));
        s = set_field(s, SSTATUS_SIE, 0);
        s = set_field(s, SSTATUS_SPP, env->priv);
        csr_write_helper(env, s, CSR_SSTATUS);
        riscv_set_mode(env, PRV_S);
    }
    /* TODO yield load reservation  */
    env->exception_index = EXCP_NONE; /* mark as handled */
}

void do_nmi(CPUState *env)
{
    if (env->nmi_pending == NMI_NONE) {
        return;
    }
    target_ulong s = env->mstatus;
    s =
        set_field(s, MSTATUS_MPIE,
                  (env->privilege_architecture >= RISCV_PRIV1_10) ? get_field(s, MSTATUS_MIE) : get_field(s,
                                                                                                          MSTATUS_UIE <<
    env->priv));
    s = set_field(s, MSTATUS_MPP, env->priv); /* store current priv level */
    s = set_field(s, MSTATUS_MIE, 0);
    csr_write_helper(env, s, CSR_MSTATUS);

    riscv_set_mode(env, PRV_M);

    int32_t nmi_index = ctz64(env->nmi_pending);

    csr_write_helper(env, nmi_index, CSR_MCAUSE);
    env->mepc = env->pc;
    env->pc = env->nmi_address + (nmi_index << 2);

    env->nmi_pending &= ~(1 << nmi_index); /* mark this nmi as handled */
}

void tlib_arch_dispose()
{
}

int cpu_init(const char *cpu_model)
{
    cpu->csr_validation_level = CSR_VALIDATION_FULL;
    cpu->misa_mask = cpu->misa = RVXLEN;
    pthread_mutex_init(&cpu->mip_lock, NULL);

    cpu_reset(cpu);

    return 0;
}

// returns 1 if the PC has already been modified by the instruction
uint32_t HELPER(handle_custom_instruction)(uint64_t id, uint64_t opcode)
{
    return tlib_handle_custom_instruction(id, opcode);
}

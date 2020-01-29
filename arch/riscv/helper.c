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
#define RVXLEN  ((target_ulong)1 << (TARGET_LONG_BITS - 2))
#elif defined(TARGET_RISCV64)
#define RVXLEN  ((target_ulong)2 << (TARGET_LONG_BITS - 2))
#endif

void cpu_reset(CPUState *env)
{
    tlb_flush(env, 1);

    bool disable_csr_validation = env->disable_csr_validation;
    bool privilege = env->privilege_architecture_1_10;
    target_ulong mhartid = env->mhartid;
    target_ulong misa_mask = env->misa_mask;
    target_ulong silenced_extensions = env->silenced_extensions;
    int32_t custom_instructions_count = env->custom_instructions_count;
    custom_instruction_descriptor_t custom_instructions[CPU_CUSTOM_INSTRUCTIONS_LIMIT];
    memcpy(custom_instructions, env->custom_instructions, sizeof(custom_instruction_descriptor_t) * CPU_CUSTOM_INSTRUCTIONS_LIMIT);

    memset(env, 0, offsetof(CPUState, breakpoints));

    env->disable_csr_validation = disable_csr_validation;
    env->mhartid = mhartid;
    env->privilege_architecture_1_10 = privilege;
    env->misa = misa_mask;
    env->misa_mask = misa_mask;
    env->silenced_extensions = silenced_extensions;
    env->priv = PRV_M;
    env->mtvec = DEFAULT_MTVEC;
    env->pc = DEFAULT_RSTVEC;
    env->exception_index = EXCP_NONE;
    set_default_nan_mode(1, &env->fp_status);
    env->custom_instructions_count = custom_instructions_count;
    memcpy(env->custom_instructions, custom_instructions, sizeof(custom_instruction_descriptor_t) * CPU_CUSTOM_INSTRUCTIONS_LIMIT);
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

    target_ulong mie = get_field(env->mstatus, MSTATUS_MIE);
    target_ulong m_enabled = env->priv < PRV_M || (env->priv == PRV_M && mie);
    target_ulong enabled_interrupts = pending_interrupts &
                                      ~env->mideleg & -m_enabled;

    target_ulong sie = get_field(env->mstatus, MSTATUS_SIE);
    target_ulong s_enabled = env->priv < PRV_S || (env->priv == PRV_S && sie);
    enabled_interrupts |= pending_interrupts & env->mideleg &
                          -s_enabled;

    if (enabled_interrupts) {
        return ctz64(enabled_interrupts); /* since non-zero */
    } else {
        return EXCP_NONE; /* indicates no pending interrupt */
    }
}

/* get_physical_address - get the physical address for this virtual address
 *
 * Do a page table walk to obtain the physical address corresponding to a
 * virtual address. Returns 0 if the translation was successful
 *
 * Adapted from Spike's mmu_t::translate and mmu_t::walk
 *
 */
static int get_physical_address(CPUState *env, target_phys_addr_t *physical,
                                int *prot, target_ulong address,
                                int access_type, int mmu_idx)
{
    /* NOTE: the env->pc value visible here will not be
     * correct, but the value visible to the exception handler
     * (riscv_cpu_do_interrupt) is correct */
    int mode = mmu_idx;

    if (mode == PRV_M && access_type != MMU_INST_FETCH) {
        if(get_field(env->mstatus, MSTATUS_MPRV)) {
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

    int levels=0, ptidxbits=0, ptesize=0, vm=0, sum=0;
    int mxr = get_field(env->mstatus, MSTATUS_MXR);

    if (env->privilege_architecture_1_10) {
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
        target_ulong idx = (addr >> (PGSHIFT + ptshift)) &
                           ((1 << ptidxbits) - 1);

        /* check that physical address of PTE is legal */
        target_ulong pte_addr = base + idx * ptesize;
        target_ulong pte = ldq_phys(pte_addr);
        target_ulong ppn = pte >> PTE_PPN_SHIFT;

        if (PTE_TABLE(pte)) { /* next level of page table */
            base = ppn << PGSHIFT;
        } else if ((pte & PTE_U) ? (mode == PRV_S) && !sum : !(mode == PRV_S)) {
            break;
        } else if (!(pte & PTE_V) || (!(pte & PTE_R) && (pte & PTE_W))) {
            break;
        } else if (access_type == MMU_INST_FETCH ? !(pte & PTE_X) :
                  access_type == MMU_DATA_LOAD ?  !(pte & PTE_R) &&
                  !(mxr && (pte & PTE_X)) : !((pte & PTE_R) && (pte & PTE_W))) {
            break;
        } else {
            /* set accessed and possibly dirty bits.
               we only put it in the TLB if it has the right stuff */
            stq_phys(pte_addr, ldq_phys(pte_addr) | PTE_A |
                    ((access_type == MMU_DATA_STORE) * PTE_D));

            /* for superpage mappings, make a fake leaf PTE for the TLB's
               benefit. */
            target_ulong vpn = addr >> PGSHIFT;
            *physical = (ppn | (vpn & ((1L << ptshift) - 1))) << PGSHIFT;

            /* we do not give all prots indicated by the PTE
             * this is because future accesses need to do things like set the
             * dirty bit on the PTE
             *
             * at this point, we assume that protection checks have occurred */
            if (mode == PRV_S) {
                if ((pte & PTE_X) && access_type == MMU_INST_FETCH) {
                    *prot |= PAGE_EXEC;
                } else if ((pte & PTE_W) && access_type == MMU_DATA_STORE) {
                    *prot |= PAGE_WRITE;
                } else if ((pte & PTE_R) && access_type == MMU_DATA_LOAD) {
                    *prot |= PAGE_READ;
                } else {
                    tlib_abort("err in translation prots");
                }
            } else {
                if ((pte & PTE_X) && access_type == MMU_INST_FETCH) {
                    *prot |= PAGE_EXEC;
                } else if ((pte & PTE_W) && access_type == MMU_DATA_STORE) {
                    *prot |= PAGE_WRITE;
                } else if ((pte & PTE_R) && access_type == MMU_DATA_LOAD) {
                    *prot |= PAGE_READ;
                } else {
                    tlib_abort("err in translation prots");
                }
            }
            return TRANSLATE_SUCCESS;
        }
    }
    return TRANSLATE_FAIL;
}

static void raise_mmu_exception(CPUState *env, target_ulong address,
                                int access_type)
{
    int page_fault_exceptions =
        (env->privilege_architecture_1_10) &&
        get_field(env->satp, SATP_MODE) != VM_1_10_MBARE;
    int exception = 0;
    if (access_type == MMU_INST_FETCH) { /* inst access */
        exception = page_fault_exceptions ?
            RISCV_EXCP_INST_PAGE_FAULT : RISCV_EXCP_INST_ACCESS_FAULT;
        env->badaddr = address;
    } else if (access_type == MMU_DATA_STORE) { /* store access */
        exception = page_fault_exceptions ?
            RISCV_EXCP_STORE_PAGE_FAULT : RISCV_EXCP_STORE_AMO_ACCESS_FAULT;
        env->badaddr = address;
    } else if (access_type == MMU_DATA_LOAD) { /* load access */
        exception = page_fault_exceptions ?
            RISCV_EXCP_LOAD_PAGE_FAULT : RISCV_EXCP_LOAD_ACCESS_FAULT;
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

    if (get_physical_address(env, &phys_addr, &prot, addr, MMU_DATA_LOAD, mem_idx)) {
        return -1;
    }
    return phys_addr;
}

/*
 * Assuming system mode, only called in tlb_fill
 */
int cpu_handle_mmu_fault(CPUState *env, target_ulong address, int access_type, int mmu_idx)
{
    target_phys_addr_t pa = 0;
    int prot;
    int ret = TRANSLATE_FAIL;

    ret = get_physical_address(env, &pa, &prot, address, access_type,
                               mmu_idx);
    if (!pmp_hart_has_privs(env, pa, TARGET_PAGE_SIZE, 1 << access_type)) {
        ret = TRANSLATE_FAIL;
    }
    if (ret == TRANSLATE_SUCCESS) {
        tlb_set_page(env, address & TARGET_PAGE_MASK, pa & TARGET_PAGE_MASK,
                     prot, mmu_idx, TARGET_PAGE_SIZE);
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
    if (env->exception_index == EXCP_NONE)
        return;
    if (env->exception_index == RISCV_EXCP_BREAKPOINT) {
        env->interrupt_request |= CPU_INTERRUPT_EXITTB;
        return;
    }

    /* skip dcsr cause check */

    target_ulong fixed_cause = 0;
    char is_interrupt = 0;
    if (env->exception_index & (RISCV_EXCP_INT_FLAG)) {
        /* hacky for now. the MSB (bit 63) indicates interrupt but cs->exception
           index is only 32 bits wide */
        fixed_cause = env->exception_index & RISCV_EXCP_INT_MASK;
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
    }

    target_ulong backup_epc = env->pc;

    target_ulong bit = fixed_cause;
    target_ulong deleg = env->medeleg;

    int hasbadaddr =
        (fixed_cause == RISCV_EXCP_ILLEGAL_INST) ||
        (fixed_cause == RISCV_EXCP_INST_ADDR_MIS) ||
        (fixed_cause == RISCV_EXCP_INST_ACCESS_FAULT) ||
        (fixed_cause == RISCV_EXCP_LOAD_ADDR_MIS) ||
        (fixed_cause == RISCV_EXCP_STORE_AMO_ADDR_MIS) ||
        (fixed_cause == RISCV_EXCP_LOAD_ACCESS_FAULT) ||
        (fixed_cause == RISCV_EXCP_STORE_AMO_ACCESS_FAULT) ||
        (fixed_cause == RISCV_EXCP_INST_PAGE_FAULT) ||
        (fixed_cause == RISCV_EXCP_LOAD_PAGE_FAULT) ||
        (fixed_cause == RISCV_EXCP_STORE_PAGE_FAULT);

    if (bit & ((target_ulong)1 << (TARGET_LONG_BITS - 1))) {
        deleg = env->mideleg;
        bit &= ~((target_ulong)1 << (TARGET_LONG_BITS - 1));
    }

    if (env->priv <= PRV_S && bit < 64 && ((deleg >> bit) & 1)) {
        /* handle the trap in S-mode */
        if ((env->stvec & 1) && is_interrupt && env->privilege_architecture_1_10) {
            env->pc = (env->stvec & ~0x1) + (fixed_cause * 4);
        } else {
            env->pc = env->stvec;
        }

        env->scause = fixed_cause;
        env->sepc = backup_epc;

        if (hasbadaddr) {
            env->sbadaddr = env->badaddr;
        }

        target_ulong s = env->mstatus;
        s = set_field(s, MSTATUS_SPIE,
            env->privilege_architecture_1_10 ? get_field(s, MSTATUS_SIE) : get_field(s, MSTATUS_UIE << env->priv));
        s = set_field(s, MSTATUS_SPP, env->priv);
        s = set_field(s, MSTATUS_SIE, 0);
        csr_write_helper(env, s, CSR_MSTATUS);
        riscv_set_mode(env, PRV_S);
    } else {
        /* Lowest bit of MTVEC changes mode to vectored interrupt */
        if ((env->mtvec & 1) && is_interrupt && env->privilege_architecture_1_10) {
            env->pc = (env->mtvec & ~0x1) + (fixed_cause * 4);
        } else {
            env->pc = env->mtvec;
        }
        env->mepc = backup_epc;
        env->mcause = fixed_cause;

        if (hasbadaddr) {
            env->mbadaddr = env->badaddr;
        }

        target_ulong s = env->mstatus;
        s = set_field(s, MSTATUS_MPIE, env->privilege_architecture_1_10 ? get_field(s, MSTATUS_MIE) : get_field(s, MSTATUS_UIE << env->priv));
        s = set_field(s, MSTATUS_MPP, env->priv);
        s = set_field(s, MSTATUS_MIE, 0);
        riscv_set_mode(env, PRV_M);
        csr_write_helper(env, s, CSR_MSTATUS);
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
    s = set_field(s, MSTATUS_MPIE, env->privilege_architecture_1_10 ? get_field(s, MSTATUS_MIE) : get_field(s, MSTATUS_UIE << env->priv));
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

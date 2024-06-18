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
#define ALIGNED_ONLY
#include "softmmu_exec.h"

// Vector helpers require 128-bit ints which aren't supported on 32-bit hosts.
#if HOST_LONG_BITS != 32

// Note that MASKED is not defined for the 2nd include
#define MASKED
#define SHIFT     0
#include "vector_helper_template.h"
#define SHIFT     0
#include "vector_helper_template.h"

#define MASKED
#define SHIFT     1
#include "vector_helper_template.h"
#define SHIFT     1
#include "vector_helper_template.h"

#define MASKED
#define SHIFT     2
#include "vector_helper_template.h"
#define SHIFT     2
#include "vector_helper_template.h"

#define MASKED
#define SHIFT     3
#include "vector_helper_template.h"
#define SHIFT     3
#include "vector_helper_template.h"

#endif  // HOST_LONG_BITS != 32

#include "arch_callbacks.h"
#include "global_helper.h"

#if defined(TARGET_RISCV32)
static const char valid_vm_1_09[16] = {
    [VM_1_09_MBARE] = 1, [VM_1_09_SV32] = 1,
};
static const char valid_vm_1_10[16] = {
    [VM_1_10_MBARE] = 1, [VM_1_10_SV32] = 1
};
#elif defined(TARGET_RISCV64)
static const char valid_vm_1_09[16] = {
    [VM_1_09_MBARE] = 1, [VM_1_09_SV39] = 1, [VM_1_09_SV48] = 1,
};
static const char valid_vm_1_10[16] = {
    [VM_1_10_MBARE] = 1, [VM_1_10_SV39] = 1, [VM_1_10_SV48] = 1, [VM_1_10_SV57] = 1
};
#endif

static int validate_vm(CPUState *env, target_ulong vm)
{
    return (env->privilege_architecture >= RISCV_PRIV1_10) ? valid_vm_1_10[vm & 0xf] : valid_vm_1_09[vm & 0xf];
}

static inline uint64_t cpu_riscv_read_instret(CPUState *env)
{
    uint64_t retval = env->instructions_count_total_value;
    return retval;
}

/* Exceptions processing helpers */
static inline void __attribute__ ((__noreturn__)) do_raise_exception_err(CPUState *env, uint32_t exception, uintptr_t pc,
                                                                         uint32_t call_hook)
{
    env->exception_index = exception;
    cpu_loop_exit_restore(env, pc, call_hook);
}

void helper_raise_exception(CPUState *env, uint32_t exception)
{
    do_raise_exception_err(env, exception, 0, 1);
}

void helper_raise_exception_debug(CPUState *env)
{
    do_raise_exception_err(env, EXCP_DEBUG, 0, 1);
}

void helper_raise_exception_mbadaddr(CPUState *env, uint32_t exception, target_ulong bad_pc)
{
    env->badaddr = bad_pc;
    do_raise_exception_err(env, exception, 0, 1);
}

void helper_raise_illegal_instruction(CPUState *env)
{
    env->badaddr = env->opcode;
    do_raise_exception_err(env, RISCV_EXCP_ILLEGAL_INST, 0, 1);
}

static inline uint64_t get_minstret_current(CPUState *env)
{
    return cpu_riscv_read_instret(env) - env->minstret_snapshot + env->minstret_snapshot_offset;
}

static inline uint64_t get_mcycles_current(CPUState *env)
{
    uint64_t instructions = cpu_riscv_read_instret(env) - env->mcycle_snapshot + env->mcycle_snapshot_offset;
    return instructions_to_cycles(env, instructions);
}

target_ulong priv_version_csr_filter(CPUState *env, target_ulong csrno)
{
    // CLIC CSRs?
    if (env->privilege_architecture == RISCV_PRIV1_11) {
        switch (csrno) {
        /* CSR_MUCOUNTEREN register is now used for CSR_MCOUNTINHIBIT */
        case CSR_MSCOUNTEREN:
            return CSR_UNHANDLED;
        }
    } else if (env->privilege_architecture == RISCV_PRIV1_10) {
        switch (csrno) {
        case CSR_MUCOUNTEREN:
        case CSR_MSCOUNTEREN:
            return CSR_UNHANDLED;
        }
    } else if (env->privilege_architecture == RISCV_PRIV1_09) {
        switch (csrno) {
        case CSR_SCOUNTEREN:
        case CSR_MCOUNTEREN:
        case CSR_PMPCFG0:
        case CSR_PMPCFG1:
        case CSR_PMPCFG2:
        case CSR_PMPCFG3:
        case CSR_PMPADDR0:
        case CSR_PMPADDR1:
        case CSR_PMPADDR2:
        case CSR_PMPADDR3:
        case CSR_PMPADDR4:
        case CSR_PMPADDR5:
        case CSR_PMPADDR6:
        case CSR_PMPADDR7:
        case CSR_PMPADDR8:
        case CSR_PMPADDR9:
        case CSR_PMPADDR10:
        case CSR_PMPADDR11:
        case CSR_PMPADDR12:
        case CSR_PMPADDR13:
        case CSR_PMPADDR14:
        case CSR_PMPADDR15:
            return CSR_UNHANDLED;
        }
    }
    return csrno;
}

static target_ulong mtvec_stvec_write_handler(target_ulong val_to_write, char* register_name)
{
    target_ulong new_value = 0;

    if (((env->privilege_architecture >= RISCV_PRIV1_10 && env->privilege_architecture < RISCV_PRIV1_11) && (val_to_write & 0x2)) ||
        (env->privilege_architecture < RISCV_PRIV1_10 && (val_to_write & 0x3))) {
        tlib_printf(LOG_LEVEL_WARNING, "Trying to set unaligned %s: 0x%X, aligning to 4-byte boundary.", register_name, val_to_write);
    }

    switch (cpu->interrupt_mode)
    {
        case INTERRUPT_MODE_AUTO:
            if (env->privilege_architecture >= RISCV_PRIV1_11) {
                new_value = val_to_write;
            } else if (env->privilege_architecture >= RISCV_PRIV1_10) {
                new_value = val_to_write & ~0x2;
            } else {
                new_value = val_to_write & ~0x3;
            }
        break;

        case INTERRUPT_MODE_DIRECT:
            new_value = val_to_write & ~0x3;
            break;

        case INTERRUPT_MODE_VECTORED:
            new_value = (val_to_write & ~0x3) | 0x1;
            break;

        default:
            tlib_abortf("Unexpected interrupt mode: %d", cpu->interrupt_mode);
    }

    if (new_value != val_to_write)
    {
        tlib_printf(LOG_LEVEL_WARNING, "%s value written to CSR corrected from 0x%x to 0x%x because of the selected interrupt mode and privilege architecture", register_name, val_to_write, new_value);
    }

    return new_value;
}

static inline void warn_nonexistent_csr_read(const int no)
{
    tlib_printf(LOG_LEVEL_WARNING, "Reading from CSR #%d that is not implemented.", no);
}
static inline void warn_nonexistent_csr_write(const int no, const target_ulong val)
{
    tlib_printf(LOG_LEVEL_WARNING, "Writing value 0x%X to CSR #%d that is not implemented.", val, no);
}

uint32_t has_custom_csr(CPUState *env, uint64_t id)
{
    if (id > MAX_CSR_ID) {
        return 0;
    }

    int slotId = id / CSRS_PER_SLOT;
    int slotOffset = id % CSRS_PER_SLOT;

    return !!(env->custom_csrs[slotId] & (1 << slotOffset));
}

/*
 * Handle writes to CSRs and any resulting special behavior
 *
 * Adapted from Spike's processor_t::set_csr
 */
inline void csr_write_helper(CPUState *env, target_ulong val_to_write, target_ulong csrno)
{
    uint64_t delegable_ints = IRQ_SS | IRQ_ST | IRQ_SE | (1 << IRQ_X_COP) | ~((1 << 12) - 1); //all local interrupts are delegable as well
    uint64_t all_ints = delegable_ints | IRQ_MS | IRQ_MT | IRQ_ME;

    csrno = priv_version_csr_filter(env, csrno);

    // testing for non-standard CSRs here (i.e., before the switch)
    // allows us to override existing CSRs with our custom implementation;
    // this is necessary for RI5CY core
    if (has_custom_csr(env, csrno) != 0) {
        tlib_write_csr(csrno, val_to_write);
        return;
    }

    switch (csrno) {
    case CSR_FFLAGS:
        if (riscv_mstatus_fs(env)) {
            env->fflags = val_to_write & (FSR_AEXC >> FSR_AEXC_SHIFT);
            mark_fs_dirty();
        } else {
            helper_raise_illegal_instruction(env);
        }
        break;
    case CSR_FRM:
        if (riscv_mstatus_fs(env)) {
            env->frm = val_to_write & (FSR_RD >> FSR_RD_SHIFT);
            mark_fs_dirty();
        } else {
            helper_raise_illegal_instruction(env);
        }
        break;
    case CSR_FCSR:
        if (riscv_mstatus_fs(env)) {
            env->fflags = (val_to_write & FSR_AEXC) >> FSR_AEXC_SHIFT;
            env->frm = (val_to_write & FSR_RD) >> FSR_RD_SHIFT;
            mark_fs_dirty();
        } else {
            helper_raise_illegal_instruction(env);
        }
        break;
    case CSR_MSTATUS: {
        target_ulong mstatus = env->mstatus;
        target_ulong mask = 0;
        if (env->privilege_architecture < RISCV_PRIV1_10) {
            if ((val_to_write ^ mstatus) & (MSTATUS_MXR | MSTATUS_MPP | MSTATUS_MPRV | MSTATUS_SUM | MSTATUS_VM)) {
                tlb_flush(env, 1, true);
            }
            mask = MSTATUS_SIE | MSTATUS_SPIE | MSTATUS_MIE | MSTATUS_MPIE | MSTATUS_SPP | MSTATUS_FS | MSTATUS_MPRV |
                   MSTATUS_SUM | MSTATUS_MPP | MSTATUS_MXR | MSTATUS_VS |
                   (validate_vm(env, get_field(val_to_write, MSTATUS_VM)) ? MSTATUS_VM : 0);
        }
        if (env->privilege_architecture >= RISCV_PRIV1_10) {
            if ((val_to_write ^ mstatus) & (MSTATUS_MXR | MSTATUS_MPP | MSTATUS_MPRV | MSTATUS_SUM)) {
                tlb_flush(env, 1, true);
            }
            mask = MSTATUS_SIE | MSTATUS_SPIE | MSTATUS_MIE | MSTATUS_MPIE | MSTATUS_SPP | MSTATUS_FS | MSTATUS_MPRV |
                   MSTATUS_SUM | MSTATUS_MPP | MSTATUS_MXR | MSTATUS_VS;
        }
#ifdef TARGET_RISCV64
        mask |= MSTATUS_UXL | MSTATUS_SXL;
#endif
        mstatus = (mstatus & ~mask) | (val_to_write & mask);

        int dirty = (mstatus & MSTATUS_FS) == MSTATUS_FS;
        dirty |= (mstatus & MSTATUS_XS) == MSTATUS_XS;
        mstatus = set_field(mstatus, MSTATUS_SD, dirty);
        env->mstatus = mstatus;
        break;
    }
    case CSR_MIP: {
        /* MIP is hardwired to zero in CLIC mode */
        if (get_field(env->mtvec, MTVEC_MODE) == MTVEC_MODE_CLIC) {
            break;
        }
        target_ulong mask = IRQ_SS | IRQ_ST | IRQ_SE;
        pthread_mutex_lock(&env->mip_lock);
        env->mip = (env->mip & ~mask) | (val_to_write & mask);
        pthread_mutex_unlock(&env->mip_lock);
        tlib_mip_changed(env->mip);
        if (env->mip != 0) {
            set_interrupt_pending(env, CPU_INTERRUPT_HARD);
        }
        break;
    }
    case CSR_MIE: {
        /* MIE is hardwired to zero in CLIC mode */
        if (get_field(env->mtvec, MTVEC_MODE) == MTVEC_MODE_CLIC) {
            break;
        }
        env->mie = (env->mie & ~all_ints) | (val_to_write & all_ints);
        break;
    }
    case CSR_MIDELEG:
        /* MIDELEG is hardwired to zero in CLIC mode */
        if (get_field(env->mtvec, MTVEC_MODE) == MTVEC_MODE_CLIC) {
            break;
        }
        env->mideleg = (env->mideleg & ~delegable_ints) | (val_to_write & delegable_ints);
        break;
    case CSR_MEDELEG: {
        target_ulong mask = 0;
        mask |= 1ULL << (RISCV_EXCP_INST_ADDR_MIS);
        mask |= 1ULL << (RISCV_EXCP_INST_ACCESS_FAULT);
        mask |= 1ULL << (RISCV_EXCP_ILLEGAL_INST);
        mask |= 1ULL << (RISCV_EXCP_BREAKPOINT);
        mask |= 1ULL << (RISCV_EXCP_LOAD_ADDR_MIS);
        mask |= 1ULL << (RISCV_EXCP_LOAD_ACCESS_FAULT);
        mask |= 1ULL << (RISCV_EXCP_STORE_AMO_ADDR_MIS);
        mask |= 1ULL << (RISCV_EXCP_STORE_AMO_ACCESS_FAULT);
        mask |= 1ULL << (RISCV_EXCP_U_ECALL);
        mask |= 1ULL << (RISCV_EXCP_S_ECALL);
        mask |= 1ULL << (RISCV_EXCP_H_ECALL);
        mask |= 1ULL << (RISCV_EXCP_M_ECALL);
        mask |= 1ULL << (RISCV_EXCP_INST_PAGE_FAULT);
        mask |= 1ULL << (RISCV_EXCP_LOAD_PAGE_FAULT);
        mask |= 1ULL << (RISCV_EXCP_STORE_PAGE_FAULT);
        env->medeleg = (env->medeleg & ~mask) | (val_to_write & mask);
        break;
    }
    case CSR_MCOUNTINHIBIT:
        /* There are different CSRs under this address in different privilege architecture versions:
         * - version 1.9.1: this address is used by mucounteren csr,
         * - version 1.10: this address is not used, and all calls are filtered by priv_version_csr_filter()
         * - since version 1.11: this address is used by mcountinhibit csr. */
        if (env->privilege_architecture == RISCV_PRIV1_09) {
            env->mucounteren = val_to_write;
        } else if (env->privilege_architecture >= RISCV_PRIV1_11) {
            env->mcountinhibit = val_to_write;
        }
        break;
    case CSR_MSCOUNTEREN:
        env->mscounteren = val_to_write;
        break;
    case CSR_SSTATUS: {
        target_ulong s = env->mstatus;
        target_ulong mask = SSTATUS_SIE | SSTATUS_SPIE | SSTATUS_UIE | SSTATUS_UPIE | SSTATUS_SPP | SSTATUS_FS | SSTATUS_XS |
                            SSTATUS_SUM | SSTATUS_MXR | SSTATUS_SD;
#ifdef TARGET_RISCV64
        mask |= SSTATUS_UXL;
#endif
        s = (s & ~mask) | (val_to_write & mask);
        csr_write_helper(env, s, CSR_MSTATUS);
        break;
    }
    case CSR_SIP: {
        /* SIP is hardwired to zero in CLIC mode */
        if (get_field(env->mtvec, MTVEC_MODE) == MTVEC_MODE_CLIC) {
            break;
        }
        target_ulong deleg = env->mideleg;
        target_ulong s = env->mip;
        target_ulong mask = IRQ_US | IRQ_SS | IRQ_UT | IRQ_ST | IRQ_UE | IRQ_SE;
        env->mip = (s & ~mask) | ((val_to_write & deleg) & mask);
        break;
    }
    case CSR_SIE: {
        /* SIE is hardwired to zero in CLIC mode */
        if (get_field(env->mtvec, MTVEC_MODE) == MTVEC_MODE_CLIC) {
            break;
        }
        target_ulong deleg = env->mideleg;
        target_ulong s = env->mie;
        target_ulong mask = IRQ_US | IRQ_SS | IRQ_UT | IRQ_ST | IRQ_UE | IRQ_SE;
        env->mie = (s & ~mask) | ((val_to_write & deleg) & mask);
        break;
    }
    case CSR_SATP: /* CSR_SPTBR */ {
        if ((env->privilege_architecture < RISCV_PRIV1_10) && (val_to_write ^ env->sptbr)) {
            tlb_flush(env, 1, true);
            env->sptbr = val_to_write & (((target_ulong)
                                          1 << (TARGET_PHYS_ADDR_SPACE_BITS - PGSHIFT)) - 1);
            if (unlikely(env->guest_profiler_enabled)) {
                tlib_announce_context_change(env->sptbr);
            }
        }
        if (env->privilege_architecture >= RISCV_PRIV1_10 &&
            validate_vm(env,
                        get_field(val_to_write,
                                  SATP_MODE)) && ((val_to_write ^ env->satp) & (SATP_MODE | SATP_ASID | SATP_PPN))) {
            tlb_flush(env, 1, true);
            env->satp = val_to_write;
            if (unlikely(env->guest_profiler_enabled)) {
                // We use the entire content of the SATP register to identify a context, even though it contains multiple fields
                tlib_announce_context_change(env->satp);
            }
        }
        break;
    }
    case CSR_SEPC:
        env->sepc = val_to_write;
        break;
    case CSR_STVEC:
        env->stvec = mtvec_stvec_write_handler(val_to_write, "STVEC");
        break;
    case CSR_SCOUNTEREN:
        env->scounteren = val_to_write;
        break;
    case CSR_STVT:
        env->stvt = val_to_write & ~MTVEC_MODE_SUBMODE;
        break;
    case CSR_SINTTHRESH:
        env->sintthresh = val_to_write;
        break;
    case CSR_SSCRATCH:
        env->sscratch = val_to_write;
        break;
    case CSR_SCAUSE:
        env->scause = val_to_write;
        break;
    case CSR_STVAL:
        env->stval = val_to_write;
        break;
    case CSR_MEPC:
        env->mepc = val_to_write;
        break;
    case CSR_MTVEC:
        env->mtvec = mtvec_stvec_write_handler(val_to_write, "MTVEC");
        break;
    case CSR_MCOUNTEREN:
        env->mcounteren = val_to_write;
        break;
    case CSR_MTVT:
        env->mtvt = val_to_write & ~MTVEC_MODE_SUBMODE;
        break;
    case CSR_SINTSTATUS:
        env->mintstatus = set_field(env->mintstatus, SINTSTATUS_MASK, val_to_write);
        break;
    case CSR_MINTSTATUS:
        env->mintstatus = val_to_write;
        break;
    case CSR_MINTTHRESH:
        env->mintthresh = val_to_write;
        break;
    case CSR_MSCRATCH:
        env->mscratch = val_to_write;
        break;
    case CSR_MCAUSE:
        env->mcause = val_to_write;
        break;
    case CSR_MTVAL:
        env->mtval = val_to_write;
        break;
    case CSR_MISA: {
        if (!(val_to_write & RISCV_FEATURE_RVF)) {
            val_to_write &= ~RISCV_FEATURE_RVD;
        }

        // allow MAFDCSUV bits in MISA to be modified
        target_ulong mask = 0;
        mask |= RISCV_FEATURE_RVM;
        mask |= RISCV_FEATURE_RVA;
        mask |= RISCV_FEATURE_RVF;
        mask |= RISCV_FEATURE_RVD;
        mask |= RISCV_FEATURE_RVC;
        mask |= RISCV_FEATURE_RVS;
        mask |= RISCV_FEATURE_RVU;
        mask |= RISCV_FEATURE_RVV;
        mask &= env->misa_mask;

        env->misa = (val_to_write & mask) | (env->misa & ~mask);
        break;
    }
    case CSR_TSELECT:
        // TSELECT is hardwired in this implementation
        break;
    case CSR_TDATA1:
        tlib_abort("CSR_TDATA1 write not implemented");
        break;
    case CSR_TDATA2:
        tlib_abort("CSR_TDATA2 write not implemented");
        break;
    case CSR_DCSR:
        tlib_abort("CSR_DCSR write not implemented");
        break;
    case CSR_MCYCLE:
#if defined(TARGET_RISCV32)
        env->mcycle_snapshot_offset = (get_mcycles_current(env) & 0xFFFFFFFF00000000) | val_to_write;
#else
        env->mcycle_snapshot_offset = get_mcycles_current(env);
#endif
        env->mcycle_snapshot = cpu_riscv_read_instret(env);
        break;
    case CSR_MCYCLEH:
#if defined(TARGET_RISCV32)
        env->mcycle_snapshot_offset = (get_mcycles_current(env) & 0x00000000FFFFFFFF) | ((uint64_t)val_to_write << 32);
        env->mcycle_snapshot = cpu_riscv_read_instret(env);
#endif
        break;
    case CSR_MINSTRET:
#if defined(TARGET_RISCV32)
        env->minstret_snapshot_offset = (get_minstret_current(env) & 0xFFFFFFFF00000000) | val_to_write;
#else
        env->minstret_snapshot_offset = get_minstret_current(env);
#endif
        env->minstret_snapshot = cpu_riscv_read_instret(env);
        break;
    case CSR_MINSTRETH:
#if defined(TARGET_RISCV32)
        env->minstret_snapshot_offset = (get_minstret_current(env) & 0x00000000FFFFFFFF) | ((uint64_t)val_to_write << 32);
        env->minstret_snapshot = cpu_riscv_read_instret(env);
#endif
        break;
    case CSR_PMPCFG0:
    case CSR_PMPCFG1:
    case CSR_PMPCFG2:
    case CSR_PMPCFG3:
        pmpcfg_csr_write(env, csrno - CSR_PMPCFG0, val_to_write);
        break;
    case CSR_PMPADDR0:
    case CSR_PMPADDR1:
    case CSR_PMPADDR2:
    case CSR_PMPADDR3:
    case CSR_PMPADDR4:
    case CSR_PMPADDR5:
    case CSR_PMPADDR6:
    case CSR_PMPADDR7:
    case CSR_PMPADDR8:
    case CSR_PMPADDR9:
    case CSR_PMPADDR10:
    case CSR_PMPADDR11:
    case CSR_PMPADDR12:
    case CSR_PMPADDR13:
    case CSR_PMPADDR14:
    case CSR_PMPADDR15:
        pmpaddr_csr_write(env, csrno - CSR_PMPADDR0, val_to_write);
        break;
    case CSR_VSTART:
        env->vstart = val_to_write;
        break;
    case CSR_VXSAT:
        env->vxsat = val_to_write;
        break;
    case CSR_VXRM:
        env->vxrm = val_to_write;
        break;
    case CSR_VCSR:
        env->vcsr = val_to_write;
        break;
    default:
        warn_nonexistent_csr_write(csrno, val_to_write);
        helper_raise_illegal_instruction(env);
    }
}

/*
 * Handle reads to CSRs and any resulting special behavior
 *
 * Adapted from Spike's processor_t::get_csr
 */
static inline target_ulong csr_read_helper(CPUState *env, target_ulong csrno)
{
    target_ulong ctr_en = env->priv == PRV_U ? env->mucounteren : env->priv == PRV_S ? env->mscounteren : -1U;
    target_ulong ctr_ok = (ctr_en >> (csrno & 31)) & 1;

    if (ctr_ok) {
        if (csrno >= CSR_HPMCOUNTER3 && csrno <= CSR_HPMCOUNTER31) {
            return 0;
        }
#if defined(TARGET_RISCV32)
        if (csrno >= CSR_HPMCOUNTER3H && csrno <= CSR_HPMCOUNTER31H) {
            return 0;
        }
#endif
    }
    if (env->privilege_architecture >= RISCV_PRIV1_10) {
        if (csrno >= CSR_MHPMCOUNTER3 && csrno <= CSR_MHPMCOUNTER31) {
            return 0;
        }
#if defined(TARGET_RISCV32)
        if (csrno >= CSR_MHPMCOUNTER3H && csrno <= CSR_MHPMCOUNTER31H) {
            return 0;
        }
#endif
        if (csrno >= CSR_MHPMEVENT3 && csrno <= CSR_MHPMEVENT31) {
            return 0;
        }
    }

    csrno = priv_version_csr_filter(env, csrno);

    // testing for non-standard CSRs here (i.e., before the switch)
    // allows us to override existing CSRs with our custom implementation;
    // this is necessary for RI5CY core
    if (has_custom_csr(env, csrno) != 0) {
        return tlib_read_csr(csrno);
    }

    switch (csrno) {
    case CSR_FFLAGS:
        if (riscv_mstatus_fs(env)) {
            return env->fflags;
        } else {
            helper_raise_illegal_instruction(env);
            break;
        }
    case CSR_FRM:
        if (riscv_mstatus_fs(env)) {
            return env->frm;
        } else {
            helper_raise_illegal_instruction(env);
            break;
        }
    case CSR_FCSR:
        if (riscv_mstatus_fs(env)) {
            return env->fflags << FSR_AEXC_SHIFT | env->frm << FSR_RD_SHIFT;
        } else {
            helper_raise_illegal_instruction(env);
            break;
        }
    case CSR_TIME:
        return tlib_get_cpu_time();
    case CSR_TIMEH:
#if defined(TARGET_RISCV32)
        return tlib_get_cpu_time() >> 32;
#endif
        break;
    case CSR_INSTRET:
    case CSR_MINSTRET:
        return get_minstret_current(env);
    case CSR_CYCLE:
    case CSR_MCYCLE:
        return get_mcycles_current(env);
    case CSR_MINSTRETH:
#if defined(TARGET_RISCV32)
        return get_minstret_current(env) >> 32;
#endif
        break;
    case CSR_CYCLEH:
    case CSR_MCYCLEH:
#if defined(TARGET_RISCV32)
        return get_mcycles_current(env) >> 32;
#endif
        break;
    case CSR_MCOUNTINHIBIT:
        /* There are different CSRs under this address on different privilege architecture version:
         * - version 1.9.1: this address is used by mucounteren csr,
         * - version 1.10: this address is not used, and all calls are filtered by priv_version_csr_filter()
         * - since version 1.11: this address is used by mcountinhibit csr. */
        if (env->privilege_architecture ==  RISCV_PRIV1_09) {
            return env->mucounteren;
        } else if (env->privilege_architecture >= RISCV_PRIV1_11) {
            return env->mcountinhibit;
        }
        break;
    case CSR_MSCOUNTEREN:
        return env->mscounteren;
    case CSR_SSTATUS: {
        target_ulong mask = SSTATUS_SIE | SSTATUS_SPIE | SSTATUS_UIE | SSTATUS_UPIE | SSTATUS_SPP | SSTATUS_FS | SSTATUS_XS |
                            SSTATUS_SUM |  SSTATUS_SD;
        if (env->privilege_architecture >= RISCV_PRIV1_10) {
            mask |= SSTATUS_MXR;
        }
#ifdef TARGET_RISCV64
        mask |= SSTATUS_UXL;
#endif
        return env->mstatus & mask;
    }
    case CSR_SIP: {
        /* SIP is hardwired to zero in CLIC mode */
        if (get_field(env->mtvec, MTVEC_MODE) == MTVEC_MODE_CLIC) {
            return 0;
        }
        target_ulong mask = IRQ_US | IRQ_SS | IRQ_UT | IRQ_ST | IRQ_UE | IRQ_SE;
        return env->mip & env->mideleg & mask;
    }
    case CSR_SIE: {
        /* SIP is hardwired to zero in CLIC mode */
        if (get_field(env->mtvec, MTVEC_MODE) == MTVEC_MODE_CLIC) {
            return 0;
        }
        target_ulong mask = IRQ_US | IRQ_SS | IRQ_UT | IRQ_ST | IRQ_UE | IRQ_SE;
        return env->mie & env->mideleg & mask;
    }
    case CSR_SEPC:
        return env->sepc;
    case CSR_STVAL:
        return env->stval;
    case CSR_STVEC:
        return env->stvec;
    case CSR_SCOUNTEREN:
        return env->scounteren;
    case CSR_STVT:
        return env->stvt;
    case CSR_SINTTHRESH:
        return env->sintthresh;
    case CSR_SCAUSE:
        return env->scause;
    case CSR_SATP: /* CSR_SPTBR */
        if (env->privilege_architecture >= RISCV_PRIV1_10) {
            return env->satp;
        } else {
            return env->sptbr;
        }
    case CSR_SSCRATCH:
        return env->sscratch;
    case CSR_MSTATUS:
        return env->mstatus;
    case CSR_MIP:
        /* MIP is hardwired to zero in CLIC mode */
        return (get_field(env->mtvec, MTVEC_MODE) == MTVEC_MODE_CLIC) ? 0 : env->mip;
    case CSR_MIE:
        /* MIE is hardwired to zero in CLIC mode */
        return (get_field(env->mtvec, MTVEC_MODE) == MTVEC_MODE_CLIC) ? 0 : env->mie;
    case CSR_MEPC:
        return env->mepc;
    case CSR_MSCRATCH:
        return env->mscratch;
    case CSR_MCAUSE:
        return env->mcause;
    case CSR_MTVAL:
        return env->mtval;
    case CSR_MISA:
        env->misa |= 0x00040000; // 'S'
        return env->misa;
    case CSR_MARCHID:
        return 0;                /* as spike does */
    case CSR_MIMPID:
        return 0;                /* as spike does */
    case CSR_MVENDORID:
        return 0;                /* as spike does */
    case CSR_MHARTID:
        return env->mhartid;
    case CSR_MTVEC:
        return env->mtvec;
    case CSR_MCOUNTEREN:
        return env->mcounteren;
    case CSR_MTVT:
        return env->mtvt;
    case CSR_MINTTHRESH:
        return env->mintthresh;
    case CSR_MEDELEG:
        return env->medeleg;
    case CSR_MIDELEG:
        /* MIDELEG is hardwired to zero in CLIC mode */
        return (get_field(env->mtvec, MTVEC_MODE) == MTVEC_MODE_CLIC) ? 0 : env->mideleg;
    case CSR_PMPCFG0:
    case CSR_PMPCFG1:
    case CSR_PMPCFG2:
    case CSR_PMPCFG3:
        return pmpcfg_csr_read(env, csrno - CSR_PMPCFG0);
    case CSR_PMPADDR0:
    case CSR_PMPADDR1:
    case CSR_PMPADDR2:
    case CSR_PMPADDR3:
    case CSR_PMPADDR4:
    case CSR_PMPADDR5:
    case CSR_PMPADDR6:
    case CSR_PMPADDR7:
    case CSR_PMPADDR8:
    case CSR_PMPADDR9:
    case CSR_PMPADDR10:
    case CSR_PMPADDR11:
    case CSR_PMPADDR12:
    case CSR_PMPADDR13:
    case CSR_PMPADDR14:
    case CSR_PMPADDR15:
        return pmpaddr_csr_read(env, csrno - CSR_PMPADDR0);
    case CSR_VSTART:
        return env->vstart;
    case CSR_VXSAT:
        return env->vxsat;
    case CSR_VXRM:
        return env->vxrm;
    case CSR_VCSR:
        return env->vcsr;
    case CSR_VL:
        return env->vl;
    case CSR_VTYPE:
        return env->vtype;
    case CSR_VLENB:
        return env->vlenb;
    case CSR_SINTSTATUS:
        return env->mintstatus & SINTSTATUS_MASK;
    case CSR_MINTSTATUS:
        return env->mintstatus;
    default:
        /* used by e.g. MTIME read */
        warn_nonexistent_csr_read(csrno);
        helper_raise_illegal_instruction(env);
    }
    return 0;
}

/*
 * Check that CSR access is allowed.
 *
 * Adapted from Spike's decode.h:validate_csr
 */
void validate_csr(CPUState *env, uint64_t which, uint64_t write)
{
    unsigned csr_priv = get_field((which), 0x300);
    unsigned csr_read_only = get_field((which), 0xC00) == 3;

    switch (env->csr_validation_level) {
    case CSR_VALIDATION_FULL:
        if (((write) && csr_read_only) || (env->priv < csr_priv)) {
            helper_raise_illegal_instruction(env);
        }
        break;

    case CSR_VALIDATION_PRIV:
        if (env->priv < csr_priv) {
            helper_raise_illegal_instruction(env);
        }
        break;

    case CSR_VALIDATION_NONE:
        break;

    default:
        tlib_abortf("Unexpected CSR validation level: %d", env->csr_validation_level);
        break;
    }
}

target_ulong helper_csrrw(CPUState *env, target_ulong src, target_ulong csr)
{
    validate_csr(env, csr, 1);
    uint64_t csr_backup = csr_read_helper(env, csr);
    csr_write_helper(env, src, csr);
    return csr_backup;
}

target_ulong helper_csrrs(CPUState *env, target_ulong src, target_ulong csr, target_ulong rs1_pass)
{
    validate_csr(env, csr, rs1_pass != 0);
    uint64_t csr_backup = csr_read_helper(env, csr);
    if (rs1_pass != 0) {
        csr_write_helper(env, src | csr_backup, csr);
    }
    return csr_backup;
}

target_ulong helper_csrrc(CPUState *env, target_ulong src, target_ulong csr, target_ulong rs1_pass)
{
    validate_csr(env, csr, rs1_pass != 0);
    uint64_t csr_backup = csr_read_helper(env, csr);
    if (rs1_pass != 0) {
        csr_write_helper(env, (~src) & csr_backup, csr);
    }
    return csr_backup;
}

void riscv_set_mode(CPUState *env, target_ulong newpriv)
{
    if (newpriv > PRV_M) {
        tlib_abort("invalid privilege level");
    }
    if (newpriv == PRV_H) {
        newpriv = PRV_U;
    }
    env->priv = newpriv;
}

target_ulong helper_sret(CPUState *env, target_ulong cpu_pc_deb)
{
    if (env->priv < PRV_S) {
        tlib_printf(LOG_LEVEL_ERROR, "Trying to execute Sret from privilege level %u", env->priv);
        helper_raise_illegal_instruction(env);
    }

    target_ulong retpc = env->sepc;
    if (!riscv_has_ext(env, RISCV_FEATURE_RVC) && (retpc & 0x3)) {
        helper_raise_exception(env, RISCV_EXCP_INST_ADDR_MIS);
    }

    target_ulong sstatus = env->mstatus;
    target_ulong prev_priv = get_field(sstatus, SSTATUS_SPP);
    sstatus =
        set_field(sstatus, (env->privilege_architecture >= RISCV_PRIV1_10) ? SSTATUS_SIE : 1 << prev_priv,
            get_field(sstatus, SSTATUS_SPIE));
    sstatus = set_field(sstatus, SSTATUS_SPIE, 1);
    sstatus = set_field(sstatus, SSTATUS_SPP, PRV_U);
    if (env->privilege_architecture >= RISCV_PRIV1_12) {
        sstatus = set_field(sstatus, MSTATUS_MPRV, 0);
    }
    
    riscv_set_mode(env, prev_priv);
    csr_write_helper(env, sstatus, CSR_SSTATUS);

    acquire_global_memory_lock(env);
    cancel_reservation(env);
    release_global_memory_lock(env);
    if(env->interrupt_end_callback_enabled)
    {
        tlib_on_interrupt_end(env->exception_index);
    }

    return retpc;
}

target_ulong helper_mret(CPUState *env, target_ulong cpu_pc_deb)
{
    if (env->priv < PRV_M) {
        tlib_printf(LOG_LEVEL_ERROR, "Trying to execute Mret from privilege level %u", env->priv);
        helper_raise_illegal_instruction(env);
    }

    target_ulong retpc = env->mepc;
    if (!riscv_has_ext(env, RISCV_FEATURE_RVC) && (retpc & 0x3)) {
        helper_raise_exception(env, RISCV_EXCP_INST_ADDR_MIS);
    }

    target_ulong mstatus = env->mstatus;
    target_ulong prev_priv = get_field(mstatus, MSTATUS_MPP);
    mstatus =
        set_field(mstatus, env->privilege_architecture >= RISCV_PRIV1_10 ? MSTATUS_MIE : 1 << prev_priv,
            get_field(mstatus, MSTATUS_MPIE));
    mstatus = set_field(mstatus, MSTATUS_MPIE, 1);
    mstatus = set_field(mstatus, MSTATUS_MPP, PRV_U);
    if (env->privilege_architecture >= RISCV_PRIV1_12 && prev_priv != PRV_M) {
        mstatus = set_field(mstatus, MSTATUS_MPRV, 0);
    }
    riscv_set_mode(env, prev_priv);
    csr_write_helper(env, mstatus, CSR_MSTATUS);

    acquire_global_memory_lock(env);
    cancel_reservation(env);
    release_global_memory_lock(env);
    if(env->interrupt_end_callback_enabled)
    {
        tlib_on_interrupt_end(env->exception_index);
    }

    return retpc;
}

void helper_wfi(CPUState *env)
{
    /* To treat wfi as nop (as described in the specification), additional facilities, outside of this translation library, are required. */
    env->wfi = 1;
    env->exception_index = EXCP_WFI;
}

void helper_fence_i(CPUState *env)
{
    helper_invalidate_dirty_addresses_shared(env);
}

void do_unaligned_access(target_ulong addr, int access_type, int mmu_idx, void *retaddr)
{
    env->badaddr = addr;
    switch (access_type) {
    case ACCESS_DATA_LOAD:
        do_raise_exception_err(env,  RISCV_EXCP_LOAD_ADDR_MIS, (uintptr_t)retaddr, 1);
        break;
    case ACCESS_DATA_STORE:
        do_raise_exception_err(env, RISCV_EXCP_STORE_AMO_ADDR_MIS, (uintptr_t)retaddr, 1);
        break;
    case ACCESS_INST_FETCH:
        // we don't restore intructions count / fire block_end hooks on translation
        do_raise_exception_err(env, RISCV_EXCP_INST_ADDR_MIS, 0, 0);
        break;
    default:
        tlib_abort("Illegal memory access type!");
    }
}

/* called to fill tlb */
int tlb_fill(CPUState *env, target_ulong addr, int access_type, int mmu_idx, void *retaddr, int no_page_fault, int access_width)
{
    int ret;
    ret = cpu_handle_mmu_fault(env, addr, access_type, mmu_idx, access_width, no_page_fault);
    if (ret == TRANSLATE_FAIL && !no_page_fault) {
        // access_type == 2 ==> CODE ACCESS - do not fire block_end hooks!
        do_raise_exception_err(env, env->exception_index, (uintptr_t)retaddr, access_type != 2);
    }
    return ret;
}

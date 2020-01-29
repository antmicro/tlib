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
#include "arch_callbacks.h"

#if defined(TARGET_RISCV32)
static const char valid_vm_1_09[16] = {
    [VM_1_09_MBARE] = 1,
    [VM_1_09_SV32] = 1,
};
static const char valid_vm_1_10[16] = {
    [VM_1_10_MBARE] = 1,
    [VM_1_10_SV32] = 1
};
#elif defined(TARGET_RISCV64)
static const char valid_vm_1_09[16] = {
    [VM_1_09_MBARE] = 1,
    [VM_1_09_SV39] = 1,
    [VM_1_09_SV48] = 1,
};
static const char valid_vm_1_10[16] = {
    [VM_1_10_MBARE] = 1,
    [VM_1_10_SV39] = 1,
    [VM_1_10_SV48] = 1,
    [VM_1_10_SV57] = 1
};
#endif

static int validate_vm(CPUState *env, target_ulong vm)
{
    return (env->privilege_architecture_1_10) ?
        valid_vm_1_10[vm & 0xf] : valid_vm_1_09[vm & 0xf];
}

static inline uint64_t cpu_riscv_read_instret(CPUState *env)
{
    uint64_t retval = env->instructions_count_total_value;
    return retval;
}

/* Exceptions processing helpers */
static inline void __attribute__ ((__noreturn__)) do_raise_exception_err(CPUState *env,
                                          uint32_t exception, uintptr_t pc, uint32_t call_hook)
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

void helper_raise_exception_mbadaddr(CPUState *env, uint32_t exception,
        target_ulong bad_pc) {
    env->badaddr = bad_pc;
    do_raise_exception_err(env, exception, 0, 1);
}

void helper_tlb_flush(CPUState *env);

static inline uint64_t get_minstret_current(CPUState *env)
{
    return cpu_riscv_read_instret(env) - env->minstret_snapshot + env->minstret_snapshot_offset;
}

static inline uint64_t get_mcycles_current(CPUState *env)
{
    return cpu_riscv_read_instret(env) - env->mcycle_snapshot + env->mcycle_snapshot_offset;
}

target_ulong priv_version_csr_filter(CPUState *env, target_ulong csrno)
{
    if (env->privilege_architecture_1_10) {
        switch(csrno) {
        case CSR_MUCOUNTEREN:
        case CSR_MSCOUNTEREN:
            return CSR_UNHANDLED;
        }
        return csrno;
    } else {
        switch(csrno) {
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
        return csrno;
    }
}

/*
 * Handle writes to CSRs and any resulting special behavior
 *
 * Adapted from Spike's processor_t::set_csr
 */
inline void csr_write_helper(CPUState *env, target_ulong val_to_write,
        target_ulong csrno)
{
    uint64_t delegable_ints = MIP_SSIP | MIP_STIP | MIP_SEIP | (1 << IRQ_X_COP) | ~((1 << 12) - 1); //all local interrupts are delegable as well
    uint64_t all_ints = delegable_ints | MIP_MSIP | MIP_MTIP | MIP_MEIP;

    csrno = priv_version_csr_filter(env, csrno);

    // testing for non-standard CSRs here (i.e., before the switch)
    // allows us to override existing CSRs with our custom implementation;
    // this is necessary for RI5CY core
    if(tlib_has_nonstandard_csr(csrno) != 0)
    {
        tlib_write_csr(csrno, val_to_write);
        return;
    }

    // If the requested CSR is inaccessible in the current privilege
    // level, generate an illegal instruction exception.
    if (env->priv < ((csrno >> 8) & 3)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }

    switch (csrno) {
    case CSR_FFLAGS:
        if (riscv_mstatus_fs(env)) {
            env->fflags = val_to_write & (FSR_AEXC >> FSR_AEXC_SHIFT);
        } else {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        }
        break;
    case CSR_FRM:
        if (riscv_mstatus_fs(env)) {
            env->frm = val_to_write & (FSR_RD >> FSR_RD_SHIFT);
        } else {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        }
        break;
    case CSR_FCSR:
        if (riscv_mstatus_fs(env)) {
            env->fflags = (val_to_write & FSR_AEXC) >> FSR_AEXC_SHIFT;
            env->frm = (val_to_write & FSR_RD) >> FSR_RD_SHIFT;
        } else {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        }
        break;
    case CSR_MSTATUS: {
        target_ulong mstatus = env->mstatus;
        target_ulong mask = 0;
        if (!env->privilege_architecture_1_10) {
            if ((val_to_write ^ mstatus) & (MSTATUS_MXR | MSTATUS_MPP |
                    MSTATUS_MPRV | MSTATUS_SUM | MSTATUS_VM)) {
                helper_tlb_flush(env);
            }
            mask = MSTATUS_SIE | MSTATUS_SPIE | MSTATUS_MIE | MSTATUS_MPIE |
                MSTATUS_SPP | MSTATUS_FS | MSTATUS_MPRV | MSTATUS_SUM |
                MSTATUS_MPP | MSTATUS_MXR |
                (validate_vm(env, get_field(val_to_write, MSTATUS_VM)) ?
                    MSTATUS_VM : 0);
        }
        if (env->privilege_architecture_1_10) {
           if ((val_to_write ^ mstatus) & (MSTATUS_MXR | MSTATUS_MPP |
                   MSTATUS_MPRV | MSTATUS_SUM)) {
                helper_tlb_flush(env);
            }
            mask = MSTATUS_SIE | MSTATUS_SPIE | MSTATUS_MIE | MSTATUS_MPIE |
                MSTATUS_SPP | MSTATUS_FS | MSTATUS_MPRV | MSTATUS_SUM |
                MSTATUS_MPP | MSTATUS_MXR;
        }
        mstatus = (mstatus & ~mask) | (val_to_write & mask);

        int dirty = (mstatus & MSTATUS_FS) == MSTATUS_FS;
        dirty |= (mstatus & MSTATUS_XS) == MSTATUS_XS;
        mstatus = set_field(mstatus, MSTATUS_SD, dirty);
        env->mstatus = mstatus;
        break;
    }
    case CSR_MIP: {
        target_ulong mask = MIP_SSIP | MIP_STIP | MIP_SEIP;
        pthread_mutex_lock(&env->mip_lock);
        env->mip = (env->mip & ~mask) | (val_to_write & mask);
        pthread_mutex_unlock(&env->mip_lock);
        tlib_mip_changed(env->mip);
        if(env->mip != 0)
        {
            env->interrupt_request = CPU_INTERRUPT_HARD;
        }
        break;
    }
    case CSR_MIE: {
        env->mie = (env->mie & ~all_ints) |
            (val_to_write & all_ints);
        break;
    }
    case CSR_MIDELEG:
        env->mideleg = (env->mideleg & ~delegable_ints)
                                | (val_to_write & delegable_ints);
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
        env->medeleg = (env->medeleg & ~mask)
                                | (val_to_write & mask);
        break;
    }
    case CSR_MUCOUNTEREN:
        env->mucounteren = val_to_write;
        break;
    case CSR_MSCOUNTEREN:
        env->mscounteren = val_to_write;
        break;
    case CSR_SSTATUS: {
        target_ulong ms = env->mstatus;
        target_ulong mask = SSTATUS_SIE | SSTATUS_SPIE | SSTATUS_UIE
            | SSTATUS_UPIE | SSTATUS_SPP | SSTATUS_FS | SSTATUS_XS
            | SSTATUS_SUM | SSTATUS_MXR | SSTATUS_SD;
        ms = (ms & ~mask) | (val_to_write & mask);
        csr_write_helper(env, ms, CSR_MSTATUS);
        break;
    }
    case CSR_SIP: {
        target_ulong next_mip = (env->mip & ~env->mideleg)
                                | (val_to_write & env->mideleg);
        csr_write_helper(env, next_mip, CSR_MIP);
        /* note: stw_phys should be done by the call to set MIP if necessary, */
        /* so we don't do it here */
        break;
    }
    case CSR_SIE: {
        target_ulong next_mie = (env->mie & ~env->mideleg)
                                | (val_to_write & env->mideleg);
        csr_write_helper(env, next_mie, CSR_MIE);
        break;
    }
    case CSR_SATP: /* CSR_SPTBR */ {
        if (!env->privilege_architecture_1_10 && (val_to_write ^ env->sptbr))
        {
            helper_tlb_flush(env);
            env->sptbr = val_to_write & (((target_ulong)
                1 << (TARGET_PHYS_ADDR_SPACE_BITS - PGSHIFT)) - 1);
        }
        if (env->privilege_architecture_1_10 &&
            validate_vm(env, get_field(val_to_write, SATP_MODE)) &&
            ((val_to_write ^ env->satp) & (SATP_MODE | SATP_ASID | SATP_PPN)))
        {
            helper_tlb_flush(env);
            env->satp = val_to_write;
        }
        break;
    }
    case CSR_SEPC:
        env->sepc = val_to_write;
        break;
    case CSR_STVEC:
        if ((env->privilege_architecture_1_10 && (val_to_write & 0x2)) || (val_to_write & 0x3)) {
            tlib_printf(LOG_LEVEL_WARNING,
                "Trying to set unaligned stvec: 0x{0:X}, aligning to 4-byte boundary.", val_to_write);
        }
        if (env->privilege_architecture_1_10){
            env->stvec = val_to_write & ~0x2;
        } else {
            env->stvec = val_to_write & ~0x3;
        }
        break;
    case CSR_SCOUNTEREN:
        env->scounteren = val_to_write;
        break;
    case CSR_SSCRATCH:
        env->sscratch = val_to_write;
        break;
    case CSR_SCAUSE:
        env->scause = val_to_write;
        break;
    case CSR_SBADADDR:
        env->sbadaddr = val_to_write;
        break;
    case CSR_MEPC:
        env->mepc = val_to_write;
        break;
    case CSR_MTVEC:
        if ((env->privilege_architecture_1_10 && (val_to_write & 0x2)) || (val_to_write & 0x3)) {
            tlib_printf(LOG_LEVEL_WARNING,
                "Trying to set unaligned mtvec: 0x{0:X}, aligning to 4-byte boundary.", val_to_write);
        }
        if (env->privilege_architecture_1_10){
            env->mtvec = val_to_write & ~0x2;
        } else {
            env->mtvec = val_to_write & ~0x3;
        }
        break;
    case CSR_MCOUNTEREN:
        env->mcounteren = val_to_write;
        break;
    case CSR_MSCRATCH:
        env->mscratch = val_to_write;
        break;
    case CSR_MCAUSE:
        env->mcause = val_to_write;
        break;
    case CSR_MBADADDR:
        env->mbadaddr = val_to_write;
        break;
    case CSR_MISA: {
        if (!(val_to_write & RISCV_FEATURE_RVF)) {
            val_to_write &= ~RISCV_FEATURE_RVD;
        }

        // allow MAFDCSU bits in MISA to be modified
        target_ulong mask = 0;
        mask |= RISCV_FEATURE_RVM;
        mask |= RISCV_FEATURE_RVA;
        mask |= RISCV_FEATURE_RVF;
        mask |= RISCV_FEATURE_RVD;
        mask |= RISCV_FEATURE_RVC;
        mask |= RISCV_FEATURE_RVS;
        mask |= RISCV_FEATURE_RVU;
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
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
}

/*
 * Handle reads to CSRs and any resulting special behavior
 *
 * Adapted from Spike's processor_t::get_csr
 */
static inline target_ulong csr_read_helper(CPUState *env, target_ulong csrno)
{
    target_ulong ctr_en = env->priv == PRV_U ? env->mucounteren :
                   env->priv == PRV_S ? env->mscounteren : -1U;
    target_ulong ctr_ok = (ctr_en >> (csrno & 31)) & 1;

    // If the requested CSR is inaccessible in the current privilege
    // level, generate an illegal instruction exception.
    if (env->priv < ((csrno >> 8) & 3)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return 0;
    }

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
    if (env->privilege_architecture_1_10) {
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
    if(tlib_has_nonstandard_csr(csrno) != 0)
    {
        return tlib_read_csr(csrno);
    }

    switch (csrno) {
    case CSR_FFLAGS:
        if (riscv_mstatus_fs(env)) {
            return env->fflags;
        } else {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    case CSR_FRM:
        if (riscv_mstatus_fs(env)) {
            return env->frm;
        } else {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    case CSR_FCSR:
        if (riscv_mstatus_fs(env)) {
            return env->fflags << FSR_AEXC_SHIFT | env->frm << FSR_RD_SHIFT;
        } else {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
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
    case CSR_MUCOUNTEREN:
        return env->mucounteren;
    case CSR_MSCOUNTEREN:
        return env->mscounteren;
    case CSR_SSTATUS: {
        target_ulong mask = SSTATUS_SIE | SSTATUS_SPIE | SSTATUS_UIE
            | SSTATUS_UPIE | SSTATUS_SPP | SSTATUS_FS | SSTATUS_XS
            | SSTATUS_SUM |  SSTATUS_SD;
        if (env->privilege_architecture_1_10) {
            mask |= SSTATUS_MXR;
        }
        return env->mstatus & mask;
    }
    case CSR_SIP:
        return env->mip & env->mideleg;
    case CSR_SIE:
        return env->mie & env->mideleg;
    case CSR_SEPC:
        return env->sepc;
    case CSR_SBADADDR:
        return env->sbadaddr;
    case CSR_STVEC:
        return env->stvec;
    case CSR_SCOUNTEREN:
        return env->scounteren;
    case CSR_SCAUSE:
        return env->scause;
    case CSR_SATP: /* CSR_SPTBR */
        if (env->privilege_architecture_1_10) {
            return env->satp;
        } else {
            return env->sptbr;
        }
    case CSR_SSCRATCH:
        return env->sscratch;
    case CSR_MSTATUS:
        return env->mstatus;
    case CSR_MIP:
        return env->mip;
    case CSR_MIE:
        return env->mie;
    case CSR_MEPC:
        return env->mepc;
    case CSR_MSCRATCH:
        return env->mscratch;
    case CSR_MCAUSE:
        return env->mcause;
    case CSR_MBADADDR:
        return env->mbadaddr;
    case CSR_MISA:
        env->misa |= 0x00040000; // 'S'
        return env->misa;
    case CSR_MARCHID:
        return 0; /* as spike does */
    case CSR_MIMPID:
        return 0; /* as spike does */
    case CSR_MVENDORID:
        return 0; /* as spike does */
    case CSR_MHARTID:
        return env->mhartid;
    case CSR_MTVEC:
        return env->mtvec;
    case CSR_MCOUNTEREN:
        return env->mcounteren;
    case CSR_MEDELEG:
        return env->medeleg;
    case CSR_MIDELEG:
        return env->mideleg;
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
    default:
        /* used by e.g. MTIME read */
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
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
    if(env->disable_csr_validation)
    {
        return;
    }

    unsigned csr_priv = get_field((which), 0x300);
    unsigned csr_read_only = get_field((which), 0xC00) == 3;
    if (((write) && csr_read_only) || (env->priv < csr_priv)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
}

target_ulong helper_csrrw(CPUState *env, target_ulong src,
        target_ulong csr)
{
    validate_csr(env, csr, 1);
    uint64_t csr_backup = csr_read_helper(env, csr);
    csr_write_helper(env, src, csr);
    return csr_backup;
}

target_ulong helper_csrrs(CPUState *env, target_ulong src,
        target_ulong csr, target_ulong rs1_pass)
{
    validate_csr(env, csr, rs1_pass != 0);
    uint64_t csr_backup = csr_read_helper(env, csr);
    if (rs1_pass != 0) {
        csr_write_helper(env, src | csr_backup, csr);
    }
    return csr_backup;
}

target_ulong helper_csrrc(CPUState *env, target_ulong src,
        target_ulong csr, target_ulong rs1_pass)
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
    helper_tlb_flush(env);
    env->priv = newpriv;
}

target_ulong helper_sret(CPUState *env, target_ulong cpu_pc_deb)
{
    if (!(env->priv >= PRV_S)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }

    target_ulong retpc = env->sepc;
    if (!riscv_has_ext(env, RISCV_FEATURE_RVC) && (retpc & 0x3)) {
        helper_raise_exception(env, RISCV_EXCP_INST_ADDR_MIS);
    }

    target_ulong mstatus = env->mstatus;
    target_ulong prev_priv = get_field(mstatus, MSTATUS_SPP);
    mstatus = set_field(mstatus,
        env->privilege_architecture_1_10 ? MSTATUS_SIE : MSTATUS_UIE << prev_priv,
        get_field(mstatus, MSTATUS_SPIE));
    mstatus = set_field(mstatus, MSTATUS_SPIE, 0);
    mstatus = set_field(mstatus, MSTATUS_SPP, PRV_U);
    riscv_set_mode(env, prev_priv);
    csr_write_helper(env, mstatus, CSR_MSTATUS);

    acquire_global_memory_lock(env);
    cancel_reservation(env);
    release_global_memory_lock(env);

    return retpc;
}

target_ulong helper_mret(CPUState *env, target_ulong cpu_pc_deb)
{
    if (!(env->priv >= PRV_M)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }

    target_ulong retpc = env->mepc;
    if (!riscv_has_ext(env, RISCV_FEATURE_RVC) && (retpc & 0x3)) {
        helper_raise_exception(env, RISCV_EXCP_INST_ADDR_MIS);
    }

    target_ulong mstatus = env->mstatus;
    target_ulong prev_priv = get_field(mstatus, MSTATUS_MPP);
    mstatus = set_field(mstatus,
        env->privilege_architecture_1_10 ? MSTATUS_MIE : MSTATUS_UIE << prev_priv,
        get_field(mstatus, MSTATUS_MPIE));
    mstatus = set_field(mstatus, MSTATUS_MPIE, 0);
    mstatus = set_field(mstatus, MSTATUS_MPP, PRV_U);
    riscv_set_mode(env, prev_priv);
    csr_write_helper(env, mstatus, CSR_MSTATUS);

    acquire_global_memory_lock(env);
    cancel_reservation(env);
    release_global_memory_lock(env);

    return retpc;
}

void helper_wfi(CPUState *env)
{
    if(tlib_is_in_debug_mode())
    {
        //According to the debug spec draft, the debug mode implies all interrupts are masked (even NMI)
        //and the WFI acts as NOP.
        return;
    }
    env->wfi = 1;
    env->exception_index = EXCP_WFI;
}

void helper_fence_i(CPUState *env)
{
    /* Flush TLB */
    tlb_flush(env, 1);
    /* ARM port seems to not know if this is okay inside a TB
       But we need to do it */
    tb_flush(env);
}

void helper_tlb_flush(CPUState *env)
{
    tlb_flush(env, 1);
}

void do_unaligned_access(target_ulong addr, int access_type, int mmu_idx,
                                void *retaddr)
{
    env->badaddr = addr;
    switch(access_type) {
        case MMU_DATA_LOAD:
           do_raise_exception_err(env,  RISCV_EXCP_LOAD_ADDR_MIS, (uintptr_t)retaddr, 1);
            break;
        case MMU_DATA_STORE:
            do_raise_exception_err(env, RISCV_EXCP_STORE_AMO_ADDR_MIS, (uintptr_t)retaddr, 1);
            break;
        case MMU_INST_FETCH:
            // we don't restore intructions count / fire block_end hooks on translation
            do_raise_exception_err(env, RISCV_EXCP_INST_ADDR_MIS, 0, 0);
            break;
        default:
            tlib_abort("Illegal memory access type!");
    }
}

/* called to fill tlb */
void tlb_fill(CPUState *env, target_ulong addr, int is_write, int mmu_idx,
              void* retaddr)
{
    int ret;
    ret = cpu_handle_mmu_fault(env, addr, is_write, mmu_idx);
    if (ret == TRANSLATE_FAIL) {
        // is_write == 2 ==> CODE ACCESS - do not fire block_end hooks!
        do_raise_exception_err(env, env->exception_index, 0, is_write != 2);
    }
}

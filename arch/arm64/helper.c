/*
 * Copyright (c) Antmicro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <limits.h>

#include "cpu.h"
#include "helper.h"
#include "mmu.h"
#include "syndrome.h"
#include "system_registers.h"

uint64_t arm_sctlr(struct CPUState *env, int el)
{
    tlib_assert(el >= 0 && el <= 3);
    if (el == 0) {
        el = arm_is_el2_enabled(env) && hcr_e2h_and_tge_set(env) ? 2 : 1;
    }
    return env->cp15.sctlr_el[el];
}

void HELPER(exception_bkpt_insn)(CPUARMState *env, uint32_t syndrome)
{
    helper_exception_with_syndrome(env, EXCP_BKPT, syndrome);
}

void HELPER(memory_barrier_assert)(CPUARMState *env)
{
    // A safety measure not to forget this isn't really implemented.
    tlib_assert((env->current_tb->cflags & CF_PARALLEL) == 0);
}

void HELPER(sysreg_tlb_flush)(CPUARMState *env, void *info_ptr)
{
    // TODO: Use register info to flush precisely.
    const ARMCPRegInfo *info = info_ptr;
    (void)info;

    tlb_flush(env, 1);
}

/* Functions called by arch-independent code. */

void cpu_exec_epilogue(CPUState *env)
{
    // Intentionally left blank.
}

void cpu_exec_prologue(CPUState *env)
{
    // Intentionally left blank.
}

void cpu_reset(CPUState *env)
{
    cpu_reset_state(env);
    cpu_reset_vfp(env);
    system_instructions_and_registers_reset(env);

    // TODO? 64-bit ARMv8 can start with AArch32 based on the AA64nAA32 configuration signal.
    if (arm_feature(env, ARM_FEATURE_AARCH64)) {
        cpu_reset_v8_a64(env);
    }
    arm_rebuild_hflags(env);
}

void do_interrupt(CPUState *env)
{
    do_interrupt_v8a(env);
}

int process_interrupt(int interrupt_request, CPUState *env)
{
    // CPU_INTERRUPT_EXITTB is handled in arch-independent code.
    if (interrupt_request & CPU_INTERRUPT_EXITTB || tlib_is_in_debug_mode()) {
        return 0;
    }

    return process_interrupt_v8a(interrupt_request, env);
}

void tlib_arch_dispose()
{
    ttable_remove(cpu->arm_core_config->cp_regs);
    tlib_free(cpu->arm_core_config);
}

/* CPU initialization and reset. */

#include "cpu_names.h"

void cpu_init_a75_a76(CPUState *env, uint32_t id)
{
    set_feature(env, ARM_FEATURE_AARCH64);
    set_feature(env, ARM_FEATURE_V8);
    set_feature(env, ARM_FEATURE_NEON);
    set_feature(env, ARM_FEATURE_GENERIC_TIMER);
    set_feature(env, ARM_FEATURE_CBAR_RO);
    set_feature(env, ARM_FEATURE_PMU);

    env->arm_core_config->has_el2 = true;
    env->arm_core_config->has_el3 = true;

    // From B2.4 AArch64 registers
    env->arm_core_config->clidr = 0x82000023;
    env->arm_core_config->ctr = 0x8444C004;
    env->arm_core_config->dcz_blocksize = 4;
    env->arm_core_config->id_aa64afr0 = 0;
    env->arm_core_config->id_aa64afr1 = 0;
    env->arm_core_config->isar.id_aa64dfr0  = 0x0000000010305408ull;
    env->arm_core_config->isar.id_aa64isar0 = 0x0000100010210000ull;
    env->arm_core_config->isar.id_aa64isar1 = 0x0000000000100001ull;
    env->arm_core_config->isar.id_aa64mmfr0 = 0x0000000000101122ull;
    env->arm_core_config->isar.id_aa64mmfr1 = 0x0000000010212122ull;
    env->arm_core_config->isar.id_aa64mmfr2 = 0x0000000000001011ull;
    env->arm_core_config->isar.id_aa64pfr0  = 0x1100000010111112ull;
    env->arm_core_config->isar.id_aa64pfr1  = 0x0000000000000010ull;
    env->arm_core_config->id_afr0       = 0x00000000;
    env->arm_core_config->isar.id_dfr0  = 0x04010088;
    env->arm_core_config->isar.id_isar0 = 0x02101110;
    env->arm_core_config->isar.id_isar1 = 0x13112111;
    env->arm_core_config->isar.id_isar2 = 0x21232042;
    env->arm_core_config->isar.id_isar3 = 0x01112131;
    env->arm_core_config->isar.id_isar4 = 0x00010142;
    env->arm_core_config->isar.id_isar5 = 0x01010001;
    env->arm_core_config->isar.id_isar6 = 0x00000010;
    env->arm_core_config->isar.id_mmfr0 = 0x10201105;
    env->arm_core_config->isar.id_mmfr1 = 0x40000000;
    env->arm_core_config->isar.id_mmfr2 = 0x01260000;
    env->arm_core_config->isar.id_mmfr3 = 0x02122211;
    env->arm_core_config->isar.id_mmfr4 = 0x00021110;
    env->arm_core_config->isar.id_pfr0  = 0x10010131;
    env->arm_core_config->isar.id_pfr1  = 0x00010000;
    env->arm_core_config->isar.id_pfr2  = 0x00000011;

    // TODO: MPIDR should depend on CPUID, CLUSTERIDAFF2 and CLUSTERIDAFF3 configuration signals.
    env->arm_core_config->mpidr = (1u << 31) /* RES1 */ | (0u << 30) /* U */ | (1u << 24) /* MT */;
    env->arm_core_config->revidr = 0;

    // From B2.23
    env->arm_core_config->ccsidr[0] = 0x701fe01a;
    env->arm_core_config->ccsidr[1] = 0x201fe01a;
    env->arm_core_config->ccsidr[2] = 0x707fe03a;

    // From B2.97
    // Bit 20 is RES1 in both A75 and A76 for SCTLR, not sure why Qemu uses 0x30c50838.
    env->arm_core_config->reset_sctlr = 0x30d50838;

    // From B4.23
    env->arm_core_config->gic_num_lrs = 4;
    env->arm_core_config->gic_vpribits = 5;
    env->arm_core_config->gic_vprebits = 5;
    // From B4.7
    env->arm_core_config->gic_pribits = 5;

    // From B5.1
    env->arm_core_config->isar.mvfr0 = 0x10110222;
    env->arm_core_config->isar.mvfr1 = 0x13211111;
    env->arm_core_config->isar.mvfr2 = 0x00000043;

    // From D5.1
    env->arm_core_config->pmceid0 = 0x7FFF0F3F;
    env->arm_core_config->pmceid1 = 0x00F2AE7F;

    // From D5.4
    env->arm_core_config->isar.reset_pmcr_el0 = 0x410b3000;

    // TODO: Add missing ones? reset_fpsid, reset_cbar, reset_auxcr, reset_hivecs
    // reset_cbar should be based on GIC PERIPHBASE signal.
}

void cpu_init_a53(CPUState *env, uint32_t id)
{
    set_feature(env, ARM_FEATURE_AARCH64);
    set_feature(env, ARM_FEATURE_V8);
    set_feature(env, ARM_FEATURE_NEON);
    set_feature(env, ARM_FEATURE_GENERIC_TIMER);
    set_feature(env, ARM_FEATURE_CBAR_RO);
    set_feature(env, ARM_FEATURE_PMU);

    env->arm_core_config->has_el2 = true;
    env->arm_core_config->has_el3 = true;

    env->arm_core_config->clidr = 0x0A200023;
    env->arm_core_config->ctr = 0x84448004;
    env->arm_core_config->dcz_blocksize = 4;
    env->arm_core_config->id_aa64afr0 = 0;
    env->arm_core_config->id_aa64afr1 = 0;
    env->arm_core_config->isar.id_aa64dfr0  = 0x10305106ull;
    env->arm_core_config->isar.id_aa64isar0 = 0x00011120ull;
    env->arm_core_config->isar.id_aa64isar1 = 0x00000000ull;
    env->arm_core_config->isar.id_aa64mmfr0 = 0x00001122ull;
    env->arm_core_config->isar.id_aa64mmfr1 = 0x00000000ull;
    env->arm_core_config->isar.id_aa64pfr0  = 0x00002222ull;
    env->arm_core_config->isar.id_aa64pfr1  = 0x00000000ull;
    env->arm_core_config->id_afr0       = 0x00000000;
    env->arm_core_config->isar.id_dfr0  = 0x03010066;
    env->arm_core_config->isar.id_isar0 = 0x02101110;
    env->arm_core_config->isar.id_isar1 = 0x13112111;
    env->arm_core_config->isar.id_isar2 = 0x21232042;
    env->arm_core_config->isar.id_isar3 = 0x01112131;
    env->arm_core_config->isar.id_isar4 = 0x00011142;
    env->arm_core_config->isar.id_isar5 = 0x00011121;
    env->arm_core_config->isar.id_mmfr0 = 0x10201105;
    env->arm_core_config->isar.id_mmfr1 = 0x40000000;
    env->arm_core_config->isar.id_mmfr2 = 0x01260000;
    env->arm_core_config->isar.id_mmfr3 = 0x02102211;
    env->arm_core_config->isar.id_pfr0  = 0x00000131;
    env->arm_core_config->isar.id_pfr1  = 0x10011011;

    // TODO: MPIDR should depend on CPUID, CLUSTERIDAFF2 and CLUSTERIDAFF3 configuration signals.
    env->arm_core_config->mpidr = (1u << 31) /* RES1 */ | (0u << 30) /* U */ | (0u << 24) /* MT */;
    env->arm_core_config->revidr = 0;

    env->arm_core_config->ccsidr[0] = 0x700fe01a;
    env->arm_core_config->ccsidr[1] = 0x201fe01a;
    env->arm_core_config->ccsidr[2] = 0x707fe07a;

    env->arm_core_config->reset_sctlr = 0x00C50838;

    env->arm_core_config->gic_num_lrs = 4;
    env->arm_core_config->gic_vpribits = 5;
    env->arm_core_config->gic_vprebits = 5;
    env->arm_core_config->gic_pribits = 5;

    env->arm_core_config->isar.mvfr0 = 0x10110222;
    env->arm_core_config->isar.mvfr1 = 0x13211111;
    env->arm_core_config->isar.mvfr2 = 0x00000043;

    env->arm_core_config->pmceid0 = 0x7FFF0F3F;
    env->arm_core_config->pmceid1 = 0x00F2AE7F;

    env->arm_core_config->isar.reset_pmcr_el0 = 0x41033000;

    env->arm_core_config->midr = 0x410FD034;
}

void cpu_init_r52(CPUState *env, uint32_t id)
{
    // Comments point to sections from
    // the Arm Cortex-R52 Processor Technical Reference Manual (version: r1p3)

    set_feature(env, ARM_FEATURE_V8);
    set_feature(env, ARM_FEATURE_V7VE); // enables ERET
    set_feature(env, ARM_FEATURE_V7);
    set_feature(env, ARM_FEATURE_V4T); // enables BX
    set_feature(env, ARM_FEATURE_V6);
    set_feature(env, ARM_FEATURE_V5);

    set_feature(env, ARM_FEATURE_NEON); // from 1.2.2
    set_feature(env, ARM_FEATURE_GENERIC_TIMER); // from 1.2
    set_feature(env, ARM_FEATURE_PMSA); // from 1.1.
    set_feature(env, ARM_FEATURE_PMU); // from 1.1.1
    set_feature(env, ARM_FEATURE_THUMB2); // from 3.3.83
    set_feature(env, ARM_FEATURE_MVFR); // from 15.5

    env->arm_core_config->has_el2 = true;        // EL2 virtualization, from 1.2

    env->arm_core_config->isar.id_isar0 = 0x02101110; // from 3.2.1
    env->arm_core_config->isar.id_isar1 = 0x13112111; // from 3.2.1
    env->arm_core_config->isar.id_isar2 = 0x21232142; // from 3.2.1
    env->arm_core_config->isar.id_isar3 = 0x01112131; // from 3.2.1
    env->arm_core_config->isar.id_isar4 = 0x00010142; // from 3.2.1
    env->arm_core_config->isar.id_isar5 = 0x00010001; // from 3.2.1
    env->arm_core_config->isar.id_mmfr0 = 0x00211040; // from 3.2.1
    env->arm_core_config->isar.id_mmfr1 = 0x40000000; // from 3.2.1
    env->arm_core_config->isar.id_mmfr2 = 0x01200000; // from 3.2.1
    env->arm_core_config->isar.id_mmfr3 = 0xF0102211; // from 3.2.1
    env->arm_core_config->isar.id_mmfr4 = 0x00000010; // from 3.2.1
    env->arm_core_config->isar.id_pfr0 = 0x00000131; // from 3.2.1
    env->arm_core_config->isar.id_pfr1 = 0x10111001; // from 3.2.1
    env->arm_core_config->isar.mvfr0 = 0x10110222; // full advanced SIMD, 0x10110021 for SP-only, from 15.5
    env->arm_core_config->isar.mvfr1 = 0x12111111; // full advanced SIMD, 0x11000011 for SP-only, from 15.5
    env->arm_core_config->isar.mvfr2 = 0x00000043; // full advanced SIMD, 0x00000040 for SP-only, from 15.5

    env->arm_core_config->isar.id_dfr0 = // 32bit, from 3.3.24
        (0x0 << 28) | // RES0
        (0x3 << 24) | // PerfMon
        (0x0 << 20) | // MProfDbg
        (0x1 << 16) | // MMapTrc
        (0x0 << 12) | // CopTrc
        (0x0 << 8) | // MMapDbg, RES0
        (0x0 << 4) | // CopSDbg, RES0
        (0x6 << 0); // CopDbg

    env->arm_core_config->isar.dbgdidr = // 32bit, from 11.4.1
        (0x7 << 28) | // WRPs
        (0x7 << 24) | // BRPs
        (0x1 << 23) | // CTX_CMPs
        (0x6 << 16) | // Version
        (0x1 << 15) | // RES1
        (0x0 << 14) | // nSUHD_imp
        (0x0 << 13) | // RES0
        (0x0 << 12) | // SE_imp
        (0x0 << 0); // RES0

    env->arm_core_config->isar.dbgdevid = // 32bit, from 11.4.2
        (0x0 << 28) | // CIDMask
        (0x0 << 24) | // AuxRegs
        (0x1 << 20) | // DoubleLock
        (0x1 << 16) | // VirExtns
        (0x0 << 12) | // VectorCatch
        (0xF << 8) | // BPAddrMask
        (0x1 << 4) | // WPAddrMask
        (0x3 << 0);  // PCsample

    env->arm_core_config->isar.dbgdevid1 = // 32bit, from 11.4.3
        (0x0 << 4) | // RES0
        (0x2 << 0); // PCSROffset

    env->arm_core_config->revidr = 0x00000000; // from 3.2.1
    env->arm_core_config->reset_fpsid = 0x41034023; // from 15.5
    env->arm_core_config->ctr = 0x8144c004; // from 3.2.1

    env->arm_core_config->reset_sctlr = // 32bit, from 3.3.92
        (0x0 << 31) | // RES0
        (0x0 << 30) | // TE, here exceptions taken in A32 state
        (0x3 << 28) | // RES1
        (0x0 << 26) | // RES0
        (0x0 << 25) | // EE, here little endianness exception, 0 in CPSR.E
        (0x0 << 24) | // RES0
        (0x3 << 22) | // RES1
        (0x0 << 21) | // FI
        (0x0 << 20) | // UWXN
        (0x0 << 19) | // WXN
        (0x1 << 18) | // nTWE
        (0x0 << 17) | // BR
        (0x1 << 16) | // nTWI
        (0x0 << 13) | // RES0
        (0x0 << 12) | // I
        (0x1 << 11) | // RES1
        (0x0 << 9) | // RES0
        (0x0 << 8) | // SED
        (0x0 << 7) | // ITD
        (0x0 << 6) | // RES0
        (0x1 << 5) | // CP15BEN
        (0x3 << 3) | // RES1
        (0x0 << 2) | // C
        (0x0 << 1) | // A
        (0x0 << 0); // M

    env->arm_core_config->pmceid0 = 0x6E1FFFDB; // 3.2.11
    env->arm_core_config->pmceid1 = 0x0000001E; // 3.2.11
    env->arm_core_config->id_afr0 = 0x00000000; // 3.2.19

    env->arm_core_config->clidr = // 32bit, from 3.3.13
        (0x0 << 30) | // ICB
        (0x1 << 27) | // LoUU, set if either cache is implemented
        (0x1 << 24) | // LoC, set if either cache is implemented
        (0x0 << 21) | // LoUIS
        (0x0 << 18) | // Ctype7
        (0x0 << 15) | // Ctype6
        (0x0 << 12) | // Ctype5
        (0x0 << 9) | // Ctype4
        (0x0 << 6) | // Ctype3
        (0x0 << 3) | // Ctype2
        (0x3 << 0); // Ctype1, separate instructions and data caches

    // TODO: Make affinity configurable from CPU class in C#
    env->arm_core_config->mpidr = // 32bit, from 3.3.78
        (0x1 << 31) | // M, RES1
        (0x0 << 30) | // U, core is part of cluster (no single core)
        (0x0 << 25) | // RES0
        (0x0 << 24) | // MT
        (0x0 << 16) | // Aff2
        (0x0 << 8) | // Aff1
        (0x0 << 0); // Aff0

    env->arm_core_config->ccsidr[0] = // 32bit, 3.3.20
        (0x0 << 31) | // WT, here no Write-Through
        (0x1 << 30) | // WB, here support Write-Back
        (0x1 << 29) | // RA, here support Read-Allocation
        (0x1 << 28) | // WA, here support Write-Allocation
        (0x7F << 13) | // NumSets, config for 32KB
        (0x3 << 3) | // Associativity
        (0x2 << 0); // LineSize

    env->arm_core_config->ccsidr[1] = // 32bit, 3.3.20
        (0x0 << 31) | // WT, here support Write-Through
        (0x0 << 30) | // WB, here no Write-Back
        (0x1 << 29) | // RA, here support Read-Allocation
        (0x0 << 28) | // WA, here no Write-Allocation
        (0x7F << 13) | // NumSets, config for 32KB
        (0x3 << 3) | // Associativity
        (0x2 << 0); // LineSize

    env->arm_core_config->gic_num_lrs = 4; // from 3.2.14
    env->arm_core_config->gic_vpribits = 5; // from 9.3.3
    env->arm_core_config->gic_vprebits = 5; // from 9.3.3
    env->arm_core_config->gic_pribits = 5; // from 9.3.4

    env->arm_core_config->gt_cntfrq_hz = 0; // from 3.2.16

    // TODO: Make amount of MPU regions configurable from CPU class in C#
    env->arm_core_config->mpuir = // from 3.3.76
        (16 << 8); // DREGION, here 16 EL1-controlled MPU regions
    env->arm_core_config->hmpuir =
        (16 << 0); // REGION, here 16 EL2-controlled MPU regions

    // TODO: Add missing ones: reset_fpsid, reset_cbar, reset_auxcr, reset_hivecs
}

static void cpu_init_core_config(CPUState *env, uint32_t id)
{
    env->arm_core_config = tlib_mallocz(sizeof(ARMCoreConfig));

    // Main ID Register.
    env->arm_core_config->midr = id;

    switch (id) {
    case ARM_CPUID_CORTEXA53:
        cpu_init_a53(env, id);
        break;
    case ARM_CPUID_CORTEXA75:
    case ARM_CPUID_CORTEXA76:
        cpu_init_a75_a76(env, id);
        break;
    case ARM_CPUID_CORTEXR52:
        cpu_init_r52(env, id);
        break;
    default:
        cpu_abort(env, "Bad CPU ID: %x\n", id);
        break;
    }

    set_el_features(env, env->arm_core_config->has_el2, env->arm_core_config->has_el3);
}

void cpu_init_v8(CPUState *env, uint32_t id)
{
    cpu_init_core_config(env, id);
    system_instructions_and_registers_init(env, id);
}

void cpu_reset_state(CPUState *env)
{
    // Let's preserve arm_core_config, features and CPU ID.
    ARMCoreConfig *config = env->arm_core_config;
    uint64_t features = env->features;
    uint32_t id = env->cp15.c0_cpuid;

    memset(env, 0, offsetof(CPUState, breakpoints));

    // Based on 'gen_clrex' and 'gen_store_exclusive' it seems -1 means the address isn't valid.
    env->exclusive_addr = -1;

    // Restore preserved fields.
    env->arm_core_config = config;
    env->features = features;
    env->cp15.c0_cpuid = id;
}

void cpu_reset_v8_a64(CPUState *env)
{
    tlib_assert(arm_feature(env, ARM_FEATURE_AARCH64));

    uint32_t pstate;

    env->aarch64 = 1;

    // Reset values of some registers are defined per CPU model.
    env->cp15.sctlr_el[1] = env->arm_core_config->reset_sctlr;
    env->cp15.sctlr_el[2] = env->arm_core_config->reset_sctlr;
    env->cp15.sctlr_el[3] = env->arm_core_config->reset_sctlr;
    env->cp15.vmpidr_el2 = env->arm_core_config->mpidr;
    env->cp15.vpidr_el2 = env->arm_core_config->midr;
    env->cp15.c9_pmcr = env->arm_core_config->isar.reset_pmcr_el0;

    // The default reset state for AArch64 is the highest available ELx (handler=true: use SP_ELx).
    pstate = aarch64_pstate_mode(arm_highest_el(env), true);

    // Reset value for each of the Interrupt Mask Bits (DAIF) is 1.
    pstate |= PSTATE_DAIF;

    // Zero flag should be unset after reset.
    // It's interpreted as set if PSTATE_Z bit is zero.
    pstate |= PSTATE_Z;

    pstate_write(env, pstate);
}

void do_interrupt_v8a(CPUState *env)
{
    uint32_t current_el = arm_current_el(env);
    uint32_t target_el = env->exception.target_el;
    // TODO: for now we only handle aarch64 exceptions
    if (!arm_el_is_aa64(env, target_el)) {
        tlib_abortf("do_interrupt: unimplemented aarch32 exception");
    }
    if (current_el > target_el) {
        tlib_abortf("do_interrupt: exception level can never go down by taking an exception");
    }
    if (target_el == 0) {
        tlib_abortf("do_interrupt: exceptions cannot be taken to EL0");
    }

    // ARMv8-A manual's rule RDPLSC
    if (current_el == 0 && arm_el_is_aa64(env, 2)) {
        bool hcr_tge_set = arm_hcr_el2_eff(env) & HCR_TGE;
        bool mdcr_tde_set = env->cp15.mdcr_el2 & MDCR_TDE;
        switch (syn_get_ec(env->exception.syndrome)) {
        case SYN_EC_DATA_ABORT_LOWER_EL:
        case SYN_EC_INSTRUCTION_ABORT_LOWER_EL:
            // The rule only applies to Stage 1 Data/Instruction aborts.
            if (env->exception.syndrome & SYN_DATA_ABORT_S1PTW) {
                target_el = hcr_tge_set ? 2 : 1;
            }
            break;
        case SYN_EC_PC_ALIGNMENT_FAULT:
        case SYN_EC_SP_ALIGNMENT_FAULT:
        case SYN_EC_BRANCH_TARGET:
        case SYN_EC_ILLEGAL_EXECUTION_STATE:
        case SYN_EC_AA32_TRAPPED_FLOATING_POINT:
        case SYN_EC_AA64_TRAPPED_FLOATING_POINT:
        case SYN_EC_AA32_SVC:
        case SYN_EC_AA64_SVC:
        // TODO: case for Undefined Instruction Exception
        case SYN_EC_TRAPPED_SVE:
        case SYN_EC_POINTER_AUTHENTICATION:
        case SYN_EC_TRAPPED_WF:
        case SYN_EC_TRAPPED_SME_SVE_SIMD_FP:
            // TODO: case for Synchronous External Aborts
            // TODO: case for Memory Copy and Memory Set Exceptions
            target_el = hcr_tge_set ? 2 : 1;
            break;
        case SYN_EC_AA32_VECTOR_CATCH:
            tlib_assert(hcr_tge_set || mdcr_tde_set);
            /* fall through */
        case SYN_EC_BREAKPOINT_LOWER_EL:
        case SYN_EC_AA32_BKPT:
        case SYN_EC_AA64_BKPT:
        case SYN_EC_SOFTWARESTEP_LOWER_EL:
        case SYN_EC_WATCHPOINT_LOWER_EL:
            target_el = hcr_tge_set || mdcr_tde_set ? 2 : 1;
            break;
        default:
            break;
        }
    }

    // new pstate mode according to the ARMv8-A manual's rule WTXBY
    // set new exception level and 'PSTATE.SP' field
    uint32_t new_pstate = aarch64_pstate_mode(target_el, true);
    // set DAIF bits
    new_pstate |= PSTATE_DAIF;  // TODO: Set also TCO bit after adding support for ARMv8.5-MTE
    // set PSTATE.SSBS to value of SCTLR.DSSBS
    new_pstate |= (!!(arm_sctlr(env, target_el) & SCTLR_DSSBS_64) << 12);

    // TODO: set PSTATE.SS according to the rules in Chapter D2 AArch64 Self-hosted Debug
    if (current_el == 0 &&
        target_el == 2  &&
        (arm_hcr_el2_eff(env) & HCR_TGE) &&
        (arm_hcr_el2_eff(env) & HCR_E2H) &&
        !((arm_sctlr(env, target_el) & SCTLR_SPAN))) {
        new_pstate |= PSTATE_PAN;
        // TODO: set PSTATE_PAN also when PSTATE.ALLINT is set to the inverse value of SCTLR_ELx.SPINTMASK
    }

    // current pstate mode
    uint32_t old_pstate = pstate_read(env);
    // exception vector table, base adress for target el
    target_ulong addr = env->cp15.vbar_el[target_el];
    // save current pstate in SPSR_ELn
    env->banked_spsr[aarch64_banked_spsr_index(target_el)] = old_pstate;

    if (current_el == target_el) {
        if (old_pstate & PSTATE_SP) {
            addr += 0x200;
        }
    } else {
        if (is_a64(env)) {
            // Lower EL using AArch64
            addr += 0x400;
        } else {
            // Lower EL using AArch32
            addr += 0x600;
        }
    }

    switch (env->exception_index) {
    case EXCP_DATA_ABORT:
    case EXCP_PREFETCH_ABORT:
        // Fault Address Register, holds the faulting virtual address
        env->cp15.far_el[target_el] = env->exception.vaddress;
        break;
    case EXCP_IRQ:
    case EXCP_VIRQ:
        addr += 0x80;
        break;
    case EXCP_FIQ:
    case EXCP_VFIQ:
        addr += 0x100;
        break;
    case EXCP_VSERR:
        tlib_abortf("do_interrupt: unsupported SError exception");
        break;
    case EXCP_BKPT:
        tlib_printf(LOG_LEVEL_DEBUG, "Handling BKPT exception");
        break;
    case EXCP_HVC:
        tlib_printf(LOG_LEVEL_DEBUG, "Handling HVC exception");
        break;
    case EXCP_SMC:
        tlib_printf(LOG_LEVEL_DEBUG, "Handling SMC exception");
        break;
    case EXCP_SWI_SVC:
        // The ARMv8-A manual states it was previously called SWI (see: F5.1.250 "SVC").
        tlib_printf(LOG_LEVEL_DEBUG, "Handling SVC exception");
        break;
    case EXCP_UDEF:
        tlib_printf(LOG_LEVEL_ERROR, "Unknown instruction: 0x%" PRIx32, ldl_code(env->pc));
        break;
    default:
        cpu_abort(env, "Unhandled exception 0x%x\n", env->exception_index);
        __builtin_unreachable();
    }
    env->cp15.esr_el[target_el] = env->exception.syndrome;

    // save current PC to ELR_ELn
    env->elr_el[target_el] = env->pc;
    pstate_write_with_sp_change(env, new_pstate);

    tlib_printf(LOG_LEVEL_DEBUG, "%s: excp=%d, addr=0x%" PRIx64 ", target_el=%d, syndrome=0x%x, pc=0x%" PRIx64 ", far=0x%" PRIx64,
                __func__, env->exception_index, addr, target_el, env->exception.syndrome, env->pc, env->exception.vaddress);

    // execute exception handler
    env->pc = addr;

    // Reset the exception structure.
    memset(&env->exception, 0, sizeof(env->exception));

    set_interrupt_pending(env, CPU_INTERRUPT_EXITTB);
    if (unlikely(env->guest_profiler_enabled)) {
        tlib_announce_stack_change(CPU_PC(env), STACK_FRAME_ADD);
    }
}

// Pass '-1' if the given field should have no influence on the result.
bool check_scr_el3(int ns, int eel2, int ea, int irq, int fiq, int rw)
{
    bool result = 1;
    if (ns != -1) {
        result &= (!!(env->cp15.scr_el3 & SCR_NS) == ns);
    }
    if (eel2 != -1) {
        result &= (!!(env->cp15.scr_el3 & SCR_EEL2) == eel2);
    }
    if (ea != -1) {
        result &= (!!(env->cp15.scr_el3 & SCR_EA) == ea);
    }
    if (irq != -1) {
        result &= (!!(env->cp15.scr_el3 & SCR_IRQ) == irq);
    }
    if (fiq != -1) {
        result &= (!!(env->cp15.scr_el3 & SCR_FIQ) == fiq);
    }
    if (rw != -1) {
        result &= (!!(env->cp15.scr_el3 & SCR_RW) == rw);
    }
    return result;
}

// Pass '-1' if the given field should have no influence on the result.
bool check_hcr_el2(int tge, int amo, int imo, int fmo, int e2h, int rw)
{
    bool result = 1;
    if (tge != -1) {
        result &= (!!(arm_hcr_el2_eff(env) & HCR_TGE) == tge);
    }
    if (amo != -1) {
        result &= (!!(arm_hcr_el2_eff(env) & HCR_AMO) == amo);
    }
    if (imo != -1) {
        result &= (!!(arm_hcr_el2_eff(env) & HCR_IMO) == imo);
    }
    if (fmo != -1) {
        result &= (!!(arm_hcr_el2_eff(env) & HCR_FMO) == fmo);
    }
    if (e2h != -1) {
        result &= (!!(arm_hcr_el2_eff(env) & HCR_E2H) == e2h);
    }
    if (rw != -1) {
        result &= (!!(arm_hcr_el2_eff(env) & HCR_RW) == rw);
    }
    return result;
}

bool interrupt_masked(bool pstate_mask_bit, bool sctlr_nmi, bool allintmask, bool superpriority)
{
    bool masked;
    if (pstate_mask_bit) {
        masked = !sctlr_nmi || allintmask || !superpriority;
    } else {
        masked = sctlr_nmi && allintmask;
    }
    return masked;
}

bool irq_masked(CPUState *env, uint32_t target_el, bool superpriority, bool ignore_pstate_aif)
{
    uint32_t pstate = pstate_read(env);
    uint64_t sctlr = arm_sctlr(env, target_el);

    bool pstate_i = pstate & PSTATE_I;
    if (ignore_pstate_aif) {
        pstate_i = false;
    }
    bool sctlr_nmi = sctlr & SCTLR_NMI;
    bool allintmask = pstate & PSTATE_ALLINT || (pstate & PSTATE_SP && sctlr & SCTLR_SPINTMASK);
    return interrupt_masked(pstate_i, sctlr_nmi, allintmask, superpriority);
}

bool fiq_masked(CPUState *env, uint32_t target_el, bool superpriority, bool ignore_pstate_aif)
{
    uint32_t pstate = pstate_read(env);
    uint64_t sctlr = arm_sctlr(env, target_el);

    bool pstate_f = pstate & PSTATE_F;
    if (ignore_pstate_aif) {
        pstate_f = false;
    }
    bool sctlr_nmi = sctlr & SCTLR_NMI;
    bool allintmask = pstate & PSTATE_ALLINT || (pstate & PSTATE_SP && sctlr & SCTLR_SPINTMASK);
    return interrupt_masked(pstate_f, sctlr_nmi, allintmask, superpriority);
}

#define IRQ_IGNORED UINT32_MAX
uint32_t establish_interrupts_target_el(uint32_t current_el, uint64_t scr_el3, uint64_t hcr_el2)
{
    tlib_assert(current_el <= 3);

    // Establishing the target Exception level of an asynchronous exception (ARMv8-A manual's rule NMMXK).
    //
    // The 'check_scr_el3' and 'check_hcr_el2' will return true only if the state of the bits passed matches
    // their current state in SCR_EL3 and HCR_EL2 (respectively). The bit is ignored if '-1' is passed.
    if (check_scr_el3(0, 0, 0, 0, 0, 0)) {
        switch (current_el) {
        case 0:
        case 1:
            // TODO: Implement AArch32 exception handling or at least implement AArch32 exception masking and abort if unmasked.
            tlib_printf(LOG_LEVEL_DEBUG,
                        "Ignoring IRQ request that should be handled at the FIQ/IRQ/Abort mode (unless masked). AArch32 exceptions aren't currently supported.");
            return IRQ_IGNORED;
        case 2:
            // Not applicable
            tlib_abortf("Invalid SCR_EL3 (0x%" PRIx64 ") state for an EL2 interrupt", scr_el3);
            break;
        case 3:
            // interrupt not taken and ignored, just return
            return IRQ_IGNORED;
        }
    } else if (check_scr_el3(0, 0, 0, 0, 0, 1)) {
        switch (current_el) {
        case 0:
        case 1:
            return 1;
        case 2:
            // Not applicable
            tlib_abortf("Invalid SCR_EL3 (0x%" PRIx64 ") for an EL2 interrupt", scr_el3);
            break;
        case 3:
            // interrupt not taken and ignored, just return
            return IRQ_IGNORED;
        }
        // TODO: does all EA, IRQ, FIQ needs to be set at single time
        // or only one of them, depending on irq type needs to be set?
    } else if (check_scr_el3(0, 0, 1, 1, 1, -1)) {
        switch (current_el) {
        case 0:
        case 1:
        case 3:
            return 3;
        case 2:
            // Not applicable
            tlib_abortf("Invalid SCR_EL3 (0x%" PRIx64 ") for an EL2 interrupt", scr_el3);
            break;
        }
    } else if (check_scr_el3(0, 1, 0, 0, 0, -1) && check_hcr_el2(0, 0, 0, 0, 0, 0)) {
        switch (current_el) {
        case 0:
        case 1:
            // TODO: Implement AArch32 exception handling or at least implement AArch32 exception masking and abort if unmasked.
            tlib_printf(LOG_LEVEL_DEBUG,
                        "Ignoring IRQ request that should be handled at the FIQ/IRQ/Abort mode (unless masked). AArch32 exceptions aren't currently supported.");
            return IRQ_IGNORED;
        case 2:
        case 3:
            // interrupt not taken and ignored, just return
            return IRQ_IGNORED;
        }
    } else if (check_scr_el3(0, 1, 0, 0, 0, -1) && check_hcr_el2(0, 0, 0, 0, 0, 1)) {
        switch (current_el) {
        case 0:
        case 1:
            return 1;
        case 2:
        case 3:
            // interrupt not taken and ignored, just return
            return IRQ_IGNORED;
        }
    } else if (check_scr_el3(0, 1, 0, 0, 0, -1) && check_hcr_el2(0, 0, 0, 0, 1, -1)) {
        switch (current_el) {
        case 0:
        case 1:
            return 1;
        case 2:
        case 3:
            // interrupt not taken and ignored, just return
            return IRQ_IGNORED;
        }
    // TODO: does all AMO, IMO, FMO needs to be set at single time
    // or only one of them?
    } else if (check_scr_el3(0, 1, 0, 0, 0, -1) && check_hcr_el2(0, 1, 1, 1, -1, -1)) {
        switch (current_el) {
        case 0:
        case 1:
        case 2:
            return 2;
        case 3:
            // interrupt not taken and ignored, just return
            return IRQ_IGNORED;
        }
    } else if (check_scr_el3(0, 1, 0, 0, 0, -1) && check_hcr_el2(1, -1, -1, -1, -1, -1)) {
        switch (current_el) {
        case 0:
        case 2:
            return 2;
        case 1:
            // Not applicable
            tlib_abortf("Invalid SCR_EL3 (0x%" PRIx64 ") and HCR_EL2 (0x%" PRIx64 ") for an EL1 interrupt", scr_el3, hcr_el2);
            break;
        case 3:
            // interrupt not taken and ignored, just return
            return IRQ_IGNORED;
        }
    // TODO: does all EA, IRQ, FIQ needs to be set at single time
    // or only one of them, depending on irq type needs to be set?
    } else if (check_scr_el3(0, 1, 1, 1, 1, -1) && check_hcr_el2(0, -1, -1, -1, -1, -1)) {
        switch (current_el) {
        case 0:
        case 1:
        case 2:
        case 3:
            return 3;
        }
    } else if (check_scr_el3(0, 1, 1, 1, 1, -1) && check_hcr_el2(1, -1, -1, -1, -1, -1)) {
        switch (current_el) {
        case 0:
        case 2:
        case 3:
            return 3;
        case 1:
            // Not applicable
            tlib_abortf("Invalid SCR_EL3 (0x%" PRIx64 ") and HCR_EL2 (0x%" PRIx64 ") for an EL1 interrupt", scr_el3, hcr_el2);
            break;
        }
    } else if (check_scr_el3(1, -1, 0, 0, 0, 0) && check_hcr_el2(0, 0, 0, 0, -1, -1)) {
        switch (current_el) {
        case 0:
        case 1:
            // TODO: Implement AArch32 exception handling or at least implement AArch32 exception masking and abort if unmasked.
            tlib_printf(LOG_LEVEL_DEBUG,
                        "Ignoring IRQ request that should be handled at the FIQ/IRQ/Abort mode (unless masked). AArch32 exceptions aren't currently supported.");
            return IRQ_IGNORED;
        case 2:
            // TODO: Implement AArch32 exception handling or at least implement AArch32 exception masking and abort if unmasked.
            tlib_printf(LOG_LEVEL_DEBUG,
                        "Ignoring IRQ request that should be handled at the HYP mode (unless masked). AArch32 exceptions aren't currently supported.");
            return IRQ_IGNORED;
        case 3:
            // interrupt not taken and ignored, just return
            return IRQ_IGNORED;
        }
    } else if (check_scr_el3(1, -1, 0, 0, 0, 0) && check_hcr_el2(0, 1, 1, 1, -1, -1)) {
        switch (current_el) {
        case 0:
        case 1:
        case 2:
            // TODO: Implement AArch32 exception handling or at least implement AArch32 exception masking and abort if unmasked.
            tlib_printf(LOG_LEVEL_DEBUG,
                        "Ignoring IRQ request that should be handled at the HYP mode (unless masked). AArch32 exceptions aren't currently supported.");
            return IRQ_IGNORED;
        case 3:
            // interrupt not taken and ignored, just return
            return IRQ_IGNORED;
        }
    } else if (check_scr_el3(1, -1, 0, 0, 0, 0) && check_hcr_el2(1, -1, -1, -1, -1, -1)) {
        switch (current_el) {
        case 0:
        case 2:
            // TODO: Implement AArch32 exception handling or at least implement AArch32 exception masking and abort if unmasked.
            tlib_printf(LOG_LEVEL_DEBUG,
                        "Ignoring IRQ request that should be handled at the HYP mode (unless masked). AArch32 exceptions aren't currently supported.");
            return IRQ_IGNORED;
        case 1:
            // Not applicable
            tlib_abortf("Invalid SCR_EL3 (0x%" PRIx64 ") and HCR_EL2 (0x%" PRIx64 ") for an EL1 interrupt", scr_el3, hcr_el2);
            break;
        case 3:
            // interrupt not taken and ignored, just return
            return IRQ_IGNORED;
        }
    } else if (check_scr_el3(1, -1, 0, 0, 0, 1) && check_hcr_el2(0, 0, 0, 0, 0, 0)) {
        switch (current_el) {
        case 0:
        case 1:
            // TODO: Implement AArch32 exception handling or at least implement AArch32 exception masking and abort if unmasked.
            tlib_printf(LOG_LEVEL_DEBUG,
                        "Ignoring IRQ request that should be handled at the FIQ mode (unless masked). AArch32 exceptions aren't currently supported.");
            return IRQ_IGNORED;
        case 2:
        case 3:
            // interrupt not taken and ignored, just return
            return IRQ_IGNORED;
        }
    } else if (check_scr_el3(1, -1, 0, 0, 0, 1) && check_hcr_el2(0, 0, 0, 0, 0, 1)) {
        switch (current_el) {
        case 0:
        case 1:
            return 1;
        case 2:
        case 3:
            // interrupt not taken and ignored, just return
            return IRQ_IGNORED;
        }
    } else if (check_scr_el3(1, -1, 0, 0, 0, 1) && check_hcr_el2(0, 0, 0, 0, 1, -1)) {
        switch (current_el) {
        case 0:
        case 1:
            return 1;
        case 2:
        case 3:
            // interrupt not taken and ignored, just return
            return IRQ_IGNORED;
        }
    } else if (check_scr_el3(1, -1, 0, 0, 0, 1) && check_hcr_el2(0, 1, 1, 1, -1, -1)) {
        switch (current_el) {
        case 0:
        case 1:
        case 2:
            return 2;
        case 3:
            // interrupt not taken and ignored, just return
            return IRQ_IGNORED;
        }
    } else if (check_scr_el3(1, -1, 0, 0, 0, 1) && check_hcr_el2(1, -1, -1, -1, -1, -1)) {
        switch (current_el) {
        case 0:
        case 2:
            return 2;
        case 1:
            // Not applicable
            tlib_abortf("Invalid SCR_EL3 (0x%" PRIx64 ") and HCR_EL2 (0x%" PRIx64 ") for an EL1 interrupt", scr_el3, hcr_el2);
            break;
        case 3:
            // interrupt not taken and ignored, just return
            return IRQ_IGNORED;
        }
    } else if (check_scr_el3(1, -1, 1, 1, 1, -1) && check_hcr_el2(0, -1, -1, -1, -1, -1)) {
        switch (current_el) {
        case 0:
        case 1:
        case 2:
        case 3:
            return 3;
        }
    } else if (check_scr_el3(1, -1, 1, 1, 1, -1) && check_hcr_el2(1, -1, -1, -1, -1, -1)) {
        switch (current_el) {
        case 0:
        case 2:
        case 3:
            return 3;
        case 1:
            // Not applicable
            tlib_abortf("Invalid SCR_EL3 (0x%" PRIx64 ") and HCR_EL2 (0x%" PRIx64 ") for an EL1 interrupt", scr_el3, hcr_el2);
            break;
        }
    } else {
        tlib_abortf("Unexpected register state in process_interrupt!");
    }
    tlib_assert_not_reached();
}

int process_interrupt_v8a(int interrupt_request, CPUState *env)
{
    uint32_t current_el = arm_current_el(env);
    uint32_t target_el = 1;

    bool el2_enabled = arm_feature(env, ARM_FEATURE_EL2);
    bool el3_enabled = arm_feature(env, ARM_FEATURE_EL3);
    if (el2_enabled || el3_enabled) {
        // TODO: Fix 'establish_interrupts_target_el' so that such a case is handled properly.
        if (!el2_enabled || !el3_enabled) {
            tlib_printf(LOG_LEVEL_WARNING, "IRQ processing might not work properly with only one of EL2/EL3 enabled.");
        }

        target_el = establish_interrupts_target_el(current_el, env->cp15.scr_el3, arm_hcr_el2_eff(env));

        if (target_el == IRQ_IGNORED) {
            return 0;
        }
    }

    // ARMv8-A manual's rule LMWZH
    if (is_a64(env) && target_el < current_el) {
        // mask interrupt
        return 0;
    }

    if (interrupt_request & (CPU_INTERRUPT_FIQ | CPU_INTERRUPT_HARD)) {
        bool ignore_pstate_aif = false;
        if (target_el > current_el) {
            // ARMv8-A manual's rule RXBYXL
            if (target_el == 3) {
                ignore_pstate_aif = true;
            } else if (target_el == 2) {
                if (!hcr_e2h_and_tge_set(env)) {
                    ignore_pstate_aif = true;
                }
            }
        }

        if (interrupt_request & CPU_INTERRUPT_FIQ) {
            // TODO: when physical fiq have superpriority?
            // ARMv8-A manual's rule (RPBKNX) says it is 'IMPLEMENTATION DEFINED'
            if (fiq_masked(env, target_el, false, ignore_pstate_aif)) {
                return 0;
            }

            env->exception_index = EXCP_FIQ;
        } else if (interrupt_request & CPU_INTERRUPT_HARD) {
            // TODO: when physical irq have superpriority?
            // ARMv8-A manual's rule (RPBKNX) says it is 'IMPLEMENTATION DEFINED'
            if (irq_masked(env, target_el, false, ignore_pstate_aif)) {
                return 0;
            }

            env->exception_index = EXCP_IRQ;
        }
    } else if (interrupt_request & CPU_INTERRUPT_VFIQ) {
        if (current_el > 1) {
            // ARMv8-A manual's rule GYGBD
            return 0;
        }
        if (target_el != 1) {
            // ARMv8-A manual's rule GYGBD
            tlib_abortf("Wrong current_el or target_el while handling vfiq!");
        }
        if (target_el == current_el) {
            if (fiq_masked(env, target_el, env->cp15.hcrx_el2 & HCRX_VFNMI, false)) {
                return 0;
            }
        }
        env->exception_index = EXCP_VFIQ;
    } else if (interrupt_request & CPU_INTERRUPT_VIRQ) {
        if (current_el > 1) {
            // ARMv8-A manual's rule GYGBD
            return 0;
        }
        if (target_el != 1) {
            // ARMv8-A manual's rule GYGBD
            tlib_abortf("Wrong current_el or target_el while handling virq!");
        }
        if (target_el == current_el) {
            if (irq_masked(env, target_el, env->cp15.hcrx_el2 & HCRX_VINMI, false)) {
                return 0;
            }
        }
        env->exception_index = EXCP_VIRQ;
    } else if (interrupt_request & CPU_INTERRUPT_VSERR) {
        if (target_el == current_el) {
            if (!(env->cp15.scr_el3 & SCR_NMEA) && !(pstate_read(env) & PSTATE_A)) {
                return 0;
            }
        } else if (target_el > current_el) {
            bool ignore_pstate_aif = false;
            if (target_el == 3) {
                ignore_pstate_aif = true;
            } else if (target_el == 2) {
                if (!hcr_e2h_and_tge_set(env)) {
                    ignore_pstate_aif = true;
                }
            }
            // TODO: when physical irq have superpriority?
            // ARMv8-A manual's rule (RPBKNX) says it is 'IMPLEMENTATION DEFINED'
            if (irq_masked(env, target_el, false, ignore_pstate_aif)) {
                return 0;
            }
        }
        env->exception_index = EXCP_VSERR;
    } else {
        tlib_printf(LOG_LEVEL_ERROR, "process_interrupt: interrupt not masked and didn't throw exception!");
        return 0;
    }
    env->exception.target_el = target_el;
    do_interrupt_v8a(env);
    return 1;
}

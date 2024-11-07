#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu.h"
#include "helper.h"
#include "host-utils.h"
#include "infrastructure.h"
#include "arch_callbacks.h"
#include "pmu.h"

static uint32_t cortexa15_cp15_c0_c1[8] = { 0x00001131, 0x00011011, 0x02010555, 0x00000000,
                                            0x10201105, 0x20000000, 0x01240000, 0x02102211 };

static uint32_t cortexr5_cp15_c0_c1[8] = { 0x00000131, 0x00000001, 0x00010400, 0x00000000,
                                           0x00110130, 0x00000000, 0x01200000, 0x00000211 };

static uint32_t cortexr8_cp15_c0_c1[8] = { 0x00000131, 0x00000001, 0x00010404, 0x00000000,
                                           0x00210030, 0x00000000, 0x01200000, 0x00002111 };

static uint32_t cortexa15_cp15_c0_c2[8] = { 0x02101110, 0x13112111, 0x21232041, 0x11112131, 0x10011142, 0, 0, 0 };

//  since Cortex-R5, r1p0
static uint32_t cortexr5_cp15_c0_c2[8] = { 0x02101111, 0x13112111, 0x21232141, 0x01112131, 0x00010142, 0, 0, 0 };

static uint32_t cortexr8_cp15_c0_c2[8] = { 0x02101111, 0x13112111, 0x21232141, 0x01112131, 0x00010142, 0, 0, 0 };

static uint32_t cortexa9_cp15_c0_c1[8] = { 0x1031, 0x11, 0x000, 0, 0x00100103, 0x20000000, 0x01230000, 0x00002111 };

static uint32_t cortexa9_cp15_c0_c2[8] = { 0x00101111, 0x13112111, 0x21232041, 0x11112131, 0x00111142, 0, 0, 0 };

static uint32_t cortexa8_cp15_c0_c1[8] = { 0x1031, 0x11, 0x400, 0, 0x31100003, 0x20000000, 0x01202000, 0x11 };

static uint32_t cortexa8_cp15_c0_c2[8] = { 0x00101111, 0x12112111, 0x21232031, 0x11112131, 0x00111142, 0, 0, 0 };

static uint32_t mpcore_cp15_c0_c1[8] = { 0x111, 0x1, 0, 0x2, 0x01100103, 0x10020302, 0x01222000, 0 };

static uint32_t mpcore_cp15_c0_c2[8] = { 0x00100011, 0x12002111, 0x11221011, 0x01102131, 0x141, 0, 0, 0 };

static uint32_t arm1136_cp15_c0_c1[8] = { 0x111, 0x1, 0x2, 0x3, 0x01130003, 0x10030302, 0x01222110, 0 };

static uint32_t arm1136_cp15_c0_c2[8] = { 0x00140011, 0x12002111, 0x11231111, 0x01102131, 0x141, 0, 0, 0 };

static uint32_t arm1176_cp15_c0_c1[8] = { 0x111, 0x11, 0x33, 0, 0x01130003, 0x10030302, 0x01222100, 0 };

static uint32_t arm1176_cp15_c0_c2[8] = { 0x0140011, 0x12002111, 0x11231121, 0x01102131, 0x01141, 0, 0, 0 };

static uint32_t cpu_arm_find_by_name(const char *name);
int get_phys_addr(CPUState *env, uint32_t address, int access_type, int is_user, uint32_t *phys_ptr, int *prot,
                  target_ulong *page_size, int no_page_fault);

static inline void set_feature(CPUState *env, int feature)
{
    env->features |= 1u << feature;
}

static void cpu_reset_model_id(CPUState *env, uint32_t id)
{
    env->cp15.c0_cpuid = id;
    switch(id) {
        case ARM_CPUID_ARM7TDMI:
            set_feature(env, ARM_FEATURE_V4T);
            break;
        case ARM_CPUID_ARM926:
            set_feature(env, ARM_FEATURE_V4T);
            set_feature(env, ARM_FEATURE_V5);
            set_feature(env, ARM_FEATURE_VFP);
            env->vfp.xregs[ARM_VFP_FPSID] = 0x41011090;
            env->cp15.c0_cachetype = 0x1dd20d2;
            env->cp15.c1_sys = 0x00090078;
            break;
        case ARM_CPUID_ARM946:
            set_feature(env, ARM_FEATURE_V4T);
            set_feature(env, ARM_FEATURE_V5);
            set_feature(env, ARM_FEATURE_MPU);
            env->cp15.c0_cachetype = 0x0f004006;
            env->cp15.c1_sys = 0x00000078;
            break;
        case ARM_CPUID_ARM1026:
            set_feature(env, ARM_FEATURE_V4T);
            set_feature(env, ARM_FEATURE_V5);
            set_feature(env, ARM_FEATURE_VFP);
            set_feature(env, ARM_FEATURE_AUXCR);
            env->vfp.xregs[ARM_VFP_FPSID] = 0x410110a0;
            env->cp15.c0_cachetype = 0x1dd20d2;
            env->cp15.c1_sys = 0x00090078;
            break;
        case ARM_CPUID_ARM1136:
            /* This is the 1136 r1, which is a v6K core */
            set_feature(env, ARM_FEATURE_V6K);
        /* Fall through */
        case ARM_CPUID_ARM1136_R2:
            /* What qemu calls "arm1136_r2" is actually the 1136 r0p2, ie an
             * older core than plain "arm1136". In particular this does not
             * have the v6K features.
             */
            set_feature(env, ARM_FEATURE_V4T);
            set_feature(env, ARM_FEATURE_V5);
            set_feature(env, ARM_FEATURE_V6);
            set_feature(env, ARM_FEATURE_VFP);
            set_feature(env, ARM_FEATURE_AUXCR);
            /* These ID register values are correct for 1136 but may be wrong
             * for 1136_r2 (in particular r0p2 does not actually implement most
             * of the ID registers).
             */
            env->vfp.xregs[ARM_VFP_FPSID] = 0x410120b4;
            env->vfp.xregs[ARM_VFP_MVFR0] = 0x11111111;
            env->vfp.xregs[ARM_VFP_MVFR1] = 0x00000000;
            memcpy(env->cp15.c0_c1, arm1136_cp15_c0_c1, 8 * sizeof(uint32_t));
            memcpy(env->cp15.c0_c2, arm1136_cp15_c0_c2, 8 * sizeof(uint32_t));
            env->cp15.c0_cachetype = 0x1dd20d2;
            env->cp15.c1_sys = 0x00050078;
            break;
        case ARM_CPUID_ARM1176:
            set_feature(env, ARM_FEATURE_V4T);
            set_feature(env, ARM_FEATURE_V5);
            set_feature(env, ARM_FEATURE_V6);
            set_feature(env, ARM_FEATURE_V6K);
            set_feature(env, ARM_FEATURE_VFP);
            set_feature(env, ARM_FEATURE_AUXCR);
            set_feature(env, ARM_FEATURE_VAPA);
            env->vfp.xregs[ARM_VFP_FPSID] = 0x410120b5;
            env->vfp.xregs[ARM_VFP_MVFR0] = 0x11111111;
            env->vfp.xregs[ARM_VFP_MVFR1] = 0x00000000;
            memcpy(env->cp15.c0_c1, arm1176_cp15_c0_c1, 8 * sizeof(uint32_t));
            memcpy(env->cp15.c0_c2, arm1176_cp15_c0_c2, 8 * sizeof(uint32_t));
            env->cp15.c0_cachetype = 0x1dd20d2;
            env->cp15.c1_sys = 0x00050078;
            break;
        case ARM_CPUID_ARM11MPCORE:
            set_feature(env, ARM_FEATURE_V4T);
            set_feature(env, ARM_FEATURE_V5);
            set_feature(env, ARM_FEATURE_V6);
            set_feature(env, ARM_FEATURE_V6K);
            set_feature(env, ARM_FEATURE_VFP);
            set_feature(env, ARM_FEATURE_AUXCR);
            set_feature(env, ARM_FEATURE_VAPA);
            env->vfp.xregs[ARM_VFP_FPSID] = 0x410120b4;
            env->vfp.xregs[ARM_VFP_MVFR0] = 0x11111111;
            env->vfp.xregs[ARM_VFP_MVFR1] = 0x00000000;
            memcpy(env->cp15.c0_c1, mpcore_cp15_c0_c1, 8 * sizeof(uint32_t));
            memcpy(env->cp15.c0_c2, mpcore_cp15_c0_c2, 8 * sizeof(uint32_t));
            env->cp15.c0_cachetype = 0x1dd20d2;
            break;
        case ARM_CPUID_CORTEXA8:
            set_feature(env, ARM_FEATURE_V4T);
            set_feature(env, ARM_FEATURE_V5);
            set_feature(env, ARM_FEATURE_V6);
            set_feature(env, ARM_FEATURE_V6K);
            set_feature(env, ARM_FEATURE_V7);
            set_feature(env, ARM_FEATURE_V7SEC);
            set_feature(env, ARM_FEATURE_AUXCR);
            set_feature(env, ARM_FEATURE_THUMB2);
            set_feature(env, ARM_FEATURE_VFP);
            set_feature(env, ARM_FEATURE_VFP3);
            set_feature(env, ARM_FEATURE_NEON);
            set_feature(env, ARM_FEATURE_THUMB2EE);
            set_feature(env, ARM_FEATURE_LPAE);
            env->vfp.xregs[ARM_VFP_FPSID] = 0x410330c0;
            env->vfp.xregs[ARM_VFP_MVFR0] = 0x11110222;
            env->vfp.xregs[ARM_VFP_MVFR1] = 0x00011100;
            memcpy(env->cp15.c0_c1, cortexa8_cp15_c0_c1, 8 * sizeof(uint32_t));
            memcpy(env->cp15.c0_c2, cortexa8_cp15_c0_c2, 8 * sizeof(uint32_t));
            env->cp15.c0_cachetype = 0x82048004;
            env->cp15.c0_clid = (1 << 27) | (2 << 24) | 3;
            env->cp15.c0_ccsid[0] = 0xe007e01a; /* 16k L1 dcache. */
            env->cp15.c0_ccsid[1] = 0x2007e01a; /* 16k L1 icache. */
            env->cp15.c0_ccsid[2] = 0xf0000000; /* No L2 icache. */
            env->cp15.c1_sys = 0x00c50078;
            break;
        //  treating A5 as A9 is a simplification and should
        //  be improved in the future
        case ARM_CPUID_CORTEXA5:
        case ARM_CPUID_CORTEXA9:
            set_feature(env, ARM_FEATURE_V4T);
            set_feature(env, ARM_FEATURE_V5);
            set_feature(env, ARM_FEATURE_V6);
            set_feature(env, ARM_FEATURE_V6K);
            set_feature(env, ARM_FEATURE_V7);
            set_feature(env, ARM_FEATURE_V7SEC);
            set_feature(env, ARM_FEATURE_AUXCR);
            set_feature(env, ARM_FEATURE_THUMB2);
            set_feature(env, ARM_FEATURE_VFP);
            set_feature(env, ARM_FEATURE_VFP3);
            set_feature(env, ARM_FEATURE_VFP_FP16);
            set_feature(env, ARM_FEATURE_NEON);
            set_feature(env, ARM_FEATURE_THUMB2EE);
            /* Note that A9 supports the MP extensions even for
             * A9UP and single-core A9MP (which are both different
             * and valid configurations; we don't model A9UP).
             */
            set_feature(env, ARM_FEATURE_V7MP);
            env->vfp.xregs[ARM_VFP_FPSID] = 0x41034000; /* Guess */
            env->vfp.xregs[ARM_VFP_MVFR0] = 0x11110222;
            env->vfp.xregs[ARM_VFP_MVFR1] = 0x01111111;
            memcpy(env->cp15.c0_c1, cortexa9_cp15_c0_c1, 8 * sizeof(uint32_t));
            memcpy(env->cp15.c0_c2, cortexa9_cp15_c0_c2, 8 * sizeof(uint32_t));
            env->cp15.c0_cachetype = 0x80038003;
            env->cp15.c0_clid = (1 << 27) | (1 << 24) | 3;
            env->cp15.c0_ccsid[0] = 0xe00fe015; /* 16k L1 dcache. */
            env->cp15.c0_ccsid[1] = 0x200fe015; /* 16k L1 icache. */
            env->cp15.c1_sys = 0x00c50078;
            break;
        case ARM_CPUID_CORTEXA15:
            set_feature(env, ARM_FEATURE_V4T);
            set_feature(env, ARM_FEATURE_V5);
            set_feature(env, ARM_FEATURE_V6);
            set_feature(env, ARM_FEATURE_V6K);
            set_feature(env, ARM_FEATURE_THUMB2);
            set_feature(env, ARM_FEATURE_V7);
            set_feature(env, ARM_FEATURE_V7SEC);
            set_feature(env, ARM_FEATURE_VFP4);
            set_feature(env, ARM_FEATURE_VFP_FP16);
            set_feature(env, ARM_FEATURE_NEON);
            set_feature(env, ARM_FEATURE_AUXCR);
            set_feature(env, ARM_FEATURE_GENERIC_TIMER);
            set_feature(env, ARM_FEATURE_THUMB2EE);
            set_feature(env, ARM_FEATURE_ARM_DIV);
            set_feature(env, ARM_FEATURE_V7MP);
            set_feature(env, ARM_FEATURE_LPAE);
            env->vfp.xregs[ARM_VFP_FPSID] = 0x410430f0;
            env->vfp.xregs[ARM_VFP_MVFR0] = 0x10110222;
            env->vfp.xregs[ARM_VFP_MVFR1] = 0x11111111;
            memcpy(env->cp15.c0_c1, cortexa15_cp15_c0_c1, 8 * sizeof(uint32_t));
            memcpy(env->cp15.c0_c2, cortexa15_cp15_c0_c2, 8 * sizeof(uint32_t));
            env->cp15.c0_cachetype = 0x8444c004;
            env->cp15.c0_clid = 0x0a200023;
            env->cp15.c0_ccsid[0] = 0x701fe00a; /* 32K L1 dcache */
            env->cp15.c0_ccsid[1] = 0x201fe00a; /* 32K L1 icache */
            env->cp15.c0_ccsid[2] = 0x711fe07a; /* 4096K L2 unified cache */
            env->cp15.c1_sys = 0x00c50078;
            break;
#ifdef TARGET_PROTO_ARM_M
        case ARM_CPUID_CORTEXM85:
            //  TODO: Add DSP once it's available
            set_feature(env, ARM_FEATURE_VFP_FP16);
            set_feature(env, ARM_FEATURE_VFP4);
            set_feature(env, ARM_FEATURE_VFP3);
            set_feature(env, ARM_FEATURE_VFP);

            set_feature(env, ARM_FEATURE_V8);
            set_feature(env, ARM_FEATURE_V7);
            set_feature(env, ARM_FEATURE_V6);
            set_feature(env, ARM_FEATURE_V5);
            set_feature(env, ARM_FEATURE_V4T);

            set_feature(env, ARM_FEATURE_MPU);

            set_feature(env, ARM_FEATURE_THUMB_DIV);
            set_feature(env, ARM_FEATURE_THUMB2);
            set_feature(env, ARM_FEATURE_MVE);
            break;
        case ARM_CPUID_CORTEXM7:
            //  TODO: This should not be present on M7 processors,
            //        but some of our samples break without it.
            set_feature(env, ARM_FEATURE_VFP_FP16);

            //  TODO: Add DSP once it's available
            set_feature(env, ARM_FEATURE_VFP4);
            set_feature(env, ARM_FEATURE_VFP3);
            set_feature(env, ARM_FEATURE_VFP);

            set_feature(env, ARM_FEATURE_V7);
            set_feature(env, ARM_FEATURE_V6);
            set_feature(env, ARM_FEATURE_V5);
            set_feature(env, ARM_FEATURE_V4T);

            set_feature(env, ARM_FEATURE_MPU);

            set_feature(env, ARM_FEATURE_THUMB2);
            set_feature(env, ARM_FEATURE_THUMB_DIV);
            break;
        case ARM_CPUID_CORTEXM4:
            //  TODO: This should not be present on M4 processors,
            //        but some of our samples break without it.
            set_feature(env, ARM_FEATURE_VFP_FP16);

            //  TODO: We don't yet implement the DSP (although it is available in our arm64).
            //        It should be added when available
            //  TODO: Remove FPU from non-f variant CPUs
            set_feature(env, ARM_FEATURE_VFP4);
            set_feature(env, ARM_FEATURE_VFP3);
            set_feature(env, ARM_FEATURE_VFP);

            set_feature(env, ARM_FEATURE_V7);
            set_feature(env, ARM_FEATURE_V6);
            set_feature(env, ARM_FEATURE_V5);
            set_feature(env, ARM_FEATURE_V4T);

            set_feature(env, ARM_FEATURE_MPU);

            set_feature(env, ARM_FEATURE_THUMB_DIV);
            set_feature(env, ARM_FEATURE_THUMB2);
            break;
        case ARM_CPUID_CORTEXM33:
            set_feature(env, ARM_FEATURE_VFP4);
            set_feature(env, ARM_FEATURE_VFP3);
            set_feature(env, ARM_FEATURE_VFP);

            set_feature(env, ARM_FEATURE_V8);
            set_feature(env, ARM_FEATURE_V7);
            set_feature(env, ARM_FEATURE_V6);
            set_feature(env, ARM_FEATURE_V5);
            set_feature(env, ARM_FEATURE_V4T);

            set_feature(env, ARM_FEATURE_MPU);

            set_feature(env, ARM_FEATURE_THUMB_DIV);
            set_feature(env, ARM_FEATURE_THUMB2);
            break;
        case ARM_CPUID_CORTEXM3:
            set_feature(env, ARM_FEATURE_V7);
            set_feature(env, ARM_FEATURE_V6);
            set_feature(env, ARM_FEATURE_V5);
            set_feature(env, ARM_FEATURE_V4T);

            set_feature(env, ARM_FEATURE_MPU);

            set_feature(env, ARM_FEATURE_THUMB_DIV);
            set_feature(env, ARM_FEATURE_THUMB2);
            break;
        case ARM_CPUID_CORTEXM23:
            set_feature(env, ARM_FEATURE_V8);
            set_feature(env, ARM_FEATURE_V7);
            set_feature(env, ARM_FEATURE_V6);
            set_feature(env, ARM_FEATURE_V5);
            set_feature(env, ARM_FEATURE_V4T);

            set_feature(env, ARM_FEATURE_MPU);

            set_feature(env, ARM_FEATURE_THUMB_DIV);
            set_feature(env, ARM_FEATURE_THUMB2);
            break;
        case ARM_CPUID_CORTEXM0:
            //  TODO: Those should not be present on M0 processors,
            //        but some of our samples break without them.
            set_feature(env, ARM_FEATURE_V7);
            set_feature(env, ARM_FEATURE_THUMB_DIV);
            set_feature(env, ARM_FEATURE_MPU);

            set_feature(env, ARM_FEATURE_V6);
            set_feature(env, ARM_FEATURE_V5);
            set_feature(env, ARM_FEATURE_V4T);

            set_feature(env, ARM_FEATURE_THUMB2);
            break;
#endif
        case ARM_CPUID_ANY: /* For userspace emulation.  */
            set_feature(env, ARM_FEATURE_V4T);
            set_feature(env, ARM_FEATURE_V5);
            set_feature(env, ARM_FEATURE_V6);
            set_feature(env, ARM_FEATURE_V6K);
            set_feature(env, ARM_FEATURE_V7);
            set_feature(env, ARM_FEATURE_THUMB2);
            set_feature(env, ARM_FEATURE_VFP);
            set_feature(env, ARM_FEATURE_VFP3);
            set_feature(env, ARM_FEATURE_VFP4);
            set_feature(env, ARM_FEATURE_VFP_FP16);
            set_feature(env, ARM_FEATURE_NEON);
            set_feature(env, ARM_FEATURE_THUMB2EE);
            set_feature(env, ARM_FEATURE_ARM_DIV);
            set_feature(env, ARM_FEATURE_V7MP);
            break;
        case ARM_CPUID_TI915T:
        case ARM_CPUID_TI925T:
            set_feature(env, ARM_FEATURE_V4T);
            set_feature(env, ARM_FEATURE_OMAPCP);
            env->cp15.c0_cpuid = ARM_CPUID_TI925T; /* Depends on wiring.  */
            env->cp15.c0_cachetype = 0x5109149;
            env->cp15.c1_sys = 0x00000070;
            env->cp15.c15_i_max = 0x000;
            env->cp15.c15_i_min = 0xff0;
            break;
        case ARM_CPUID_PXA250:
        case ARM_CPUID_PXA255:
        case ARM_CPUID_PXA260:
        case ARM_CPUID_PXA261:
        case ARM_CPUID_PXA262:
            set_feature(env, ARM_FEATURE_V4T);
            set_feature(env, ARM_FEATURE_V5);
            set_feature(env, ARM_FEATURE_XSCALE);
            /* JTAG_ID is ((id << 28) | 0x09265013) */
            env->cp15.c0_cachetype = 0xd172172;
            env->cp15.c1_sys = 0x00000078;
            break;
        case ARM_CPUID_PXA270_A0:
        case ARM_CPUID_PXA270_A1:
        case ARM_CPUID_PXA270_B0:
        case ARM_CPUID_PXA270_B1:
        case ARM_CPUID_PXA270_C0:
        case ARM_CPUID_PXA270_C5:
            set_feature(env, ARM_FEATURE_V4T);
            set_feature(env, ARM_FEATURE_V5);
            set_feature(env, ARM_FEATURE_XSCALE);
            /* JTAG_ID is ((id << 28) | 0x09265013) */
            set_feature(env, ARM_FEATURE_IWMMXT);
            env->iwmmxt.cregs[ARM_IWMMXT_wCID] = 0x69051000 | 'Q';
            env->cp15.c0_cachetype = 0xd172172;
            env->cp15.c1_sys = 0x00000078;
            break;
        case ARM_CPUID_SA1100:
        case ARM_CPUID_SA1110:
            set_feature(env, ARM_FEATURE_STRONGARM);
            env->cp15.c1_sys = 0x00000070;
            break;
        case ARM_CPUID_CORTEXR5F:
            set_feature(env, ARM_FEATURE_VFP);
            set_feature(env, ARM_FEATURE_VFP3);
            set_feature(env, ARM_FEATURE_VFP_FP16);
            set_feature(env, ARM_FEATURE_NEON);

            env->vfp.xregs[ARM_VFP_FPSID] = 0x41023150;
            env->vfp.xregs[ARM_VFP_MVFR0] = 0x10110221;
            env->vfp.xregs[ARM_VFP_MVFR1] = 0x00000011;
            /* fallthrough */
        case ARM_CPUID_CORTEXR5:
            set_feature(env, ARM_FEATURE_V4T);
            set_feature(env, ARM_FEATURE_V5);
            set_feature(env, ARM_FEATURE_V6);
            set_feature(env, ARM_FEATURE_V6K);
            set_feature(env, ARM_FEATURE_V7);
            set_feature(env, ARM_FEATURE_V7MP);
            set_feature(env, ARM_FEATURE_ARM_DIV);  //  not for rp0p0

            set_feature(env, ARM_FEATURE_THUMB2);
            set_feature(env, ARM_FEATURE_THUMB_DIV);

            set_feature(env, ARM_FEATURE_AUXCR);
            set_feature(env, ARM_FEATURE_GENERIC_TIMER);
            set_feature(env, ARM_FEATURE_PMSA);

            memcpy(env->cp15.c0_c1, cortexr5_cp15_c0_c1, 8 * sizeof(uint32_t));
            memcpy(env->cp15.c0_c2, cortexr5_cp15_c0_c2, 8 * sizeof(uint32_t));

            env->cp15.c0_cachetype = 0x80030003;  //  CTR
            env->cp15.c0_tcmtype = 0x00010001;    //  TCMTR
            env->cp15.c0_clid = 0x09200003;       //  CLIDR, for all caches implemented
            env->cp15.c0_ccsid[0] = 0xf01fe019;   /* 32K L1 dcache */
            env->cp15.c0_ccsid[1] = 0xf01fe019;   /* 32K L1 icache */

            env->cp15.c1_sys = 0xe50878;                                             //  SCTLR
            env->cp15.c1_coproc |= (1 << 30 /* D32DIS */) | (1 << 31 /* ASEDIS */);  //  CPACR
            break;
        case ARM_CPUID_CORTEXR8:
            set_feature(env, ARM_FEATURE_V4T);
            set_feature(env, ARM_FEATURE_V5);
            set_feature(env, ARM_FEATURE_V6);
            set_feature(env, ARM_FEATURE_V6K);
            set_feature(env, ARM_FEATURE_V7);
            set_feature(env, ARM_FEATURE_V7MP);
            set_feature(env, ARM_FEATURE_ARM_DIV);

            set_feature(env, ARM_FEATURE_VFP);
            set_feature(env, ARM_FEATURE_VFP3);
            set_feature(env, ARM_FEATURE_VFP_FP16);
            set_feature(env, ARM_FEATURE_NEON);

            set_feature(env, ARM_FEATURE_THUMB2);

            set_feature(env, ARM_FEATURE_AUXCR);
            set_feature(env, ARM_FEATURE_CBAR_RO);
            set_feature(env, ARM_FEATURE_GENERIC_TIMER);
            set_feature(env, ARM_FEATURE_PMSA);

            env->vfp.xregs[ARM_VFP_FPSID] = 0x41023180;
            env->vfp.xregs[ARM_VFP_MVFR0] = 0x10110021 | /* if f64 supported */ 0x00000200;
            env->vfp.xregs[ARM_VFP_MVFR1] = 0x01000011;

            memcpy(env->cp15.c0_c1, cortexr8_cp15_c0_c1, 8 * sizeof(uint32_t));
            memcpy(env->cp15.c0_c2, cortexr8_cp15_c0_c2, 8 * sizeof(uint32_t));

            env->cp15.c0_cachetype = 0x8333C003;  //  CTR
            env->cp15.c0_tcmtype = 0x80010001;    //  TCMTR
            env->cp15.c0_clid = 0x09200003;       //  CLIDR, for cache implemented
            env->cp15.c0_ccsid[0] = 0x701fe019;   /* 32K L1 dcache */
            env->cp15.c0_ccsid[1] = 0x201fe019;   /* 32K L1 icache */
            env->cp15.c1_sys = 0xc50078;          //  SCTLR
            env->cp15.c1_coproc = 0xC0000000;     //  CPACR
            break;
        default:
            cpu_abort(env, "Bad CPU ID: %x\n", id);
            break;
    }

    /* Some features automatically imply others: */
    if(arm_feature(env, ARM_FEATURE_V7)) {
        set_feature(env, ARM_FEATURE_VAPA);
    }
    if(arm_feature(env, ARM_FEATURE_ARM_DIV)) {
        set_feature(env, ARM_FEATURE_THUMB_DIV);
    }
    if(arm_feature(env, ARM_FEATURE_PMSA)) {
        set_feature(env, ARM_FEATURE_MPU);
    }
}

void system_instructions_and_registers_reset(CPUState *env);
void system_instructions_and_registers_init(CPUState *env);

void cpu_on_leaving_reset_state(CPUState *env)
{
    configuration_signals_apply(env);
}

void cpu_reset(CPUState *env)
{
    uint32_t id = env->cp15.c0_cpuid;
    uint32_t number_of_mpu_regions = env->number_of_mpu_regions;
#ifdef TARGET_PROTO_ARM_M
    uint32_t number_of_sau_regions = env->number_of_sau_regions;
#endif
    memset(env, 0, RESET_OFFSET);
    if(id) {
        cpu_reset_model_id(env, id);
    }
    env->number_of_mpu_regions = number_of_mpu_regions;
#ifdef TARGET_PROTO_ARM_M
    env->number_of_sau_regions = number_of_sau_regions;
#endif
    /* SVC mode with interrupts disabled.  */
    env->uncached_cpsr = ARM_CPU_MODE_SVC | CPSR_A | CPSR_F | CPSR_I;

#ifdef TARGET_PROTO_ARM_M
    env->v7m.has_trustzone = tlib_has_enabled_trustzone() > 0;
    //  Set initial Security State to Secure if there is TrustZone support
    env->secure = env->v7m.has_trustzone;

    env->v7m.fpccr = (env->v7m.fpccr & ~ARM_FPCCR_LSPACT_MASK) | ARM_FPCCR_ASPEN_MASK | ARM_FPCCR_LSPEN_MASK;
#endif

    env->vfp.xregs[ARM_VFP_FPEXC] = 0;
    env->cp15.c2_base_mask = 0xffffc000u;
    /* v7 performance monitor control register: same implementor
     * field as main ID register, and we implement no event counters.
     */
    env->cp15.c9_pmcr = (env->cp15.c0_cpuid & 0xff000000);

    set_flush_to_zero(1, &env->vfp.standard_fp_status);
    set_flush_inputs_to_zero(1, &env->vfp.standard_fp_status);
    set_default_nan_mode(1, &env->vfp.standard_fp_status);
    set_float_detect_tininess(float_tininess_before_rounding, &env->vfp.fp_status);
    set_float_detect_tininess(float_tininess_before_rounding, &env->vfp.standard_fp_status);

    system_instructions_and_registers_reset(env);

    pmu_init_reset(env);
}

int cpu_init(const char *cpu_model)
{
    uint32_t id = cpu_arm_find_by_name(cpu_model);
    if(id == 0) {
        return -1;
    }
    cpu->cp15.c0_cpuid = id;

    //  We need this to set CPU feature flags, before calling `system_instructions_and_registers_init`
    cpu_reset_model_id(env, id);

    system_instructions_and_registers_init(env);

    cpu_reset(cpu);
    return 0;
}

struct arm_cpu_t {
    uint32_t id;
    const char *name;
};

static const struct arm_cpu_t arm_cpu_names[] = {
    { ARM_CPUID_ARM7TDMI,    "arm7tdmi"    },
    { ARM_CPUID_ARM926,      "arm926"      },
    { ARM_CPUID_ARM946,      "arm946"      },
    { ARM_CPUID_ARM1026,     "arm1026"     },
    { ARM_CPUID_ARM1136,     "arm1136"     },
    { ARM_CPUID_ARM1136_R2,  "arm1136-r2"  },
    { ARM_CPUID_ARM1176,     "arm1176"     },
    { ARM_CPUID_ARM11MPCORE, "arm11mpcore" },

    //  TODO: M0+ shouldn't be the same as M3. It doesn't support hardware division.
    { ARM_CPUID_CORTEXM0,    "cortex-m0"   },
    { ARM_CPUID_CORTEXM3,    "cortex-m0+"  },
    { ARM_CPUID_CORTEXM3,    "cortex-m1"   },
    { ARM_CPUID_CORTEXM23,   "cortex-m23"  },
    { ARM_CPUID_CORTEXM3,    "cortex-m3"   },
    { ARM_CPUID_CORTEXM33,   "cortex-m33"  },
    //  TODO: M4F should be separate from M4.
    { ARM_CPUID_CORTEXM4,    "cortex-m4"   },
    { ARM_CPUID_CORTEXM7,    "cortex-m4f"  },
    { ARM_CPUID_CORTEXM7,    "cortex-m7"   },
    { ARM_CPUID_CORTEXM85,   "cortex-m85"  },

    { ARM_CPUID_CORTEXR5,    "cortex-r5"   },
    { ARM_CPUID_CORTEXR5F,   "cortex-r5f"  },
    { ARM_CPUID_CORTEXR8,    "cortex-r8"   },

    { ARM_CPUID_CORTEXA5,    "cortex-a5"   },
    { ARM_CPUID_CORTEXA8,    "cortex-a8"   },
    { ARM_CPUID_CORTEXA9,    "cortex-a9"   },
    { ARM_CPUID_CORTEXA15,   "cortex-a15"  },

    { ARM_CPUID_TI925T,      "ti925t"      },
    { ARM_CPUID_PXA250,      "pxa250"      },
    { ARM_CPUID_SA1100,      "sa1100"      },
    { ARM_CPUID_SA1110,      "sa1110"      },
    { ARM_CPUID_PXA255,      "pxa255"      },
    { ARM_CPUID_PXA260,      "pxa260"      },
    { ARM_CPUID_PXA261,      "pxa261"      },
    { ARM_CPUID_PXA262,      "pxa262"      },
    { ARM_CPUID_PXA270,      "pxa270"      },
    { ARM_CPUID_PXA270_A0,   "pxa270-a0"   },
    { ARM_CPUID_PXA270_A1,   "pxa270-a1"   },
    { ARM_CPUID_PXA270_B0,   "pxa270-b0"   },
    { ARM_CPUID_PXA270_B1,   "pxa270-b1"   },
    { ARM_CPUID_PXA270_C0,   "pxa270-c0"   },
    { ARM_CPUID_PXA270_C5,   "pxa270-c5"   },
    { ARM_CPUID_ANY,         "any"         },
    { 0,                     NULL          }
};

/* return 0 if not found */
static uint32_t cpu_arm_find_by_name(const char *name)
{
    int i;
    uint32_t id;

    id = 0;
    for(i = 0; arm_cpu_names[i].name; i++) {
        if(strcmp(name, arm_cpu_names[i].name) == 0) {
            id = arm_cpu_names[i].id;
            break;
        }
    }
    return id;
}

uint32_t cpsr_read(CPUState *env)
{
    int ZF;
    ZF = (env->ZF == 0);
    return env->uncached_cpsr | (env->NF & 0x80000000) | (ZF << 30) | (env->CF << 29) | ((env->VF & 0x80000000) >> 3) |
           (env->QF << 27) | (env->thumb << 5) | ((env->condexec_bits & 3) << 25) | ((env->condexec_bits & 0xfc) << 8) |
           (env->GE << 16);
}

void cpsr_write(CPUState *env, uint32_t val, uint32_t mask)
{
    if(mask & CPSR_NZCV) {
        env->ZF = (~val) & CPSR_Z;
        env->NF = val;
        env->CF = (val >> 29) & 1;
        env->VF = (val << 3) & 0x80000000;
    }
    if(mask & CPSR_Q) {
        env->QF = ((val & CPSR_Q) != 0);
    }
    if(mask & CPSR_T) {
        env->thumb = ((val & CPSR_T) != 0);
    }
    if(mask & CPSR_IT_0_1) {
        env->condexec_bits &= ~3;
        env->condexec_bits |= (val >> 25) & 3;
    }
    if(mask & CPSR_IT_2_7) {
        env->condexec_bits &= 3;
        env->condexec_bits |= (val >> 8) & 0xfc;
    }
    if(mask & CPSR_GE) {
        env->GE = (val >> 16) & 0xf;
    }

    if((env->uncached_cpsr ^ val) & mask & CPSR_M) {
        switch_mode(env, val & CPSR_M);
    }
    mask &= ~CACHED_CPSR_BITS;
    env->uncached_cpsr = (env->uncached_cpsr & ~mask) | (val & mask);

    find_pending_irq_if_primask_unset(env);
}

/* Sign/zero extend */
uint32_t HELPER(sxtb16)(uint32_t x)
{
    uint32_t res;
    res = (uint16_t)(int8_t)x;
    res |= (uint32_t)(int8_t)(x >> 16) << 16;
    return res;
}

uint32_t HELPER(uxtb16)(uint32_t x)
{
    uint32_t res;
    res = (uint16_t)(uint8_t)x;
    res |= (uint32_t)(uint8_t)(x >> 16) << 16;
    return res;
}

uint32_t HELPER(clz)(uint32_t x)
{
    return clz32(x);
}

int32_t HELPER(sdiv)(int32_t num, int32_t den)
{
    if(den == 0) {
        return 0;
    }
    if(num == INT_MIN && den == -1) {
        return INT_MIN;
    }
    return num / den;
}

uint32_t HELPER(udiv)(uint32_t num, uint32_t den)
{
    if(den == 0) {
        return 0;
    }
    return num / den;
}

uint32_t HELPER(rbit)(uint32_t x)
{
    x = ((x & 0xff000000) >> 24) | ((x & 0x00ff0000) >> 8) | ((x & 0x0000ff00) << 8) | ((x & 0x000000ff) << 24);
    x = ((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4);
    x = ((x & 0x88888888) >> 3) | ((x & 0x44444444) >> 1) | ((x & 0x22222222) << 1) | ((x & 0x11111111) << 3);
    return x;
}

uint32_t HELPER(abs)(uint32_t x)
{
    return ((int32_t)x < 0) ? -x : x;
}

/* Map CPU modes onto saved register banks.  */
/* 26-bit mode currently affects only the bank number. */
static inline int bank_number(int mode)
{
    switch(mode) {
        case ARM_CPU_MODE_USR:
        case ARM_CPU_MODE_USR26:
        case ARM_CPU_MODE_SYS:
            return 0;
        case ARM_CPU_MODE_SVC:
        case ARM_CPU_MODE_SVC26:
            return 1;
        case ARM_CPU_MODE_ABT:
            return 2;
        case ARM_CPU_MODE_UND:
            return 3;
        case ARM_CPU_MODE_IRQ:
        case ARM_CPU_MODE_IRQ26:
            return 4;
        case ARM_CPU_MODE_FIQ:
        case ARM_CPU_MODE_FIQ26:
            return 5;
    }
    cpu_abort(cpu, "Bad mode %x\n", mode);
    return -1;
}

void switch_mode(CPUState *env, int mode)
{
    int old_mode;
    int i;

    old_mode = env->uncached_cpsr & CPSR_M;
    if(mode == old_mode) {
        return;
    }

    //  PMU only has to be informed about changes between Privilege Levels
    //  but it doesn't care about mode changes within the same PL
    if(unlikely(env->pmu.counters_enabled) && (mode == ARM_CPU_MODE_USR || old_mode == ARM_CPU_MODE_USR)) {
        pmu_switch_mode_user(mode);
    }

    if(old_mode == ARM_CPU_MODE_FIQ) {
        memcpy(env->fiq_regs, env->regs + 8, 5 * sizeof(uint32_t));
        memcpy(env->regs + 8, env->usr_regs, 5 * sizeof(uint32_t));
    } else if(mode == ARM_CPU_MODE_FIQ) {
        memcpy(env->usr_regs, env->regs + 8, 5 * sizeof(uint32_t));
        memcpy(env->regs + 8, env->fiq_regs, 5 * sizeof(uint32_t));
    }

    i = bank_number(old_mode);
    env->banked_r13[i] = env->regs[13];
    env->banked_r14[i] = env->regs[14];
    env->banked_spsr[i] = env->spsr;

    i = bank_number(mode);
    env->regs[13] = env->banked_r13[i];
    env->regs[14] = env->banked_r14[i];
    env->spsr = env->banked_spsr[i];
}

static inline void arm_announce_stack_change()
{
    if(unlikely(env->guest_profiler_enabled)) {
        tlib_announce_stack_change(CPU_PC(env), STACK_FRAME_ADD);
    }
}

#ifdef TARGET_PROTO_ARM_M
static int v7m_push(CPUState *env, uint32_t val)
{
    uint32_t phys_ptr = 0;
    target_ulong page_size = 0;
    int ret, prot = 0;
    uint32_t address = env->regs[13] - 4;
    ret = get_phys_addr(env, address, ACCESS_DATA_STORE, !in_privileged_mode(env), &phys_ptr, &prot, &page_size, false);
    if(ret == TRANSLATE_SUCCESS) {
        env->regs[13] = address;
        stl_phys(env->regs[13], val);
    } else {
        //  Stacking error - MSTKERR
        env->cp15.c5_data = ret;
        if(arm_feature(env, ARM_FEATURE_V6)) {
            env->cp15.c5_data |= (1 << 11);
        }
        env->cp15.c6_data = address;
        env->v7m.fault_status |= MEM_FAULT_MSTKERR;
        return 1;
    }
    return 0;
}

static uint32_t v7m_pop(CPUState *env)
{
    uint32_t val;
    val = ldl_phys(env->regs[13]);
    env->regs[13] += 4;
    return val;
}

static inline void swap_u32(uint32_t *a, uint32_t *b)
{
    uint32_t tmp;
    tmp = *a;
    *a = *b;
    *b = tmp;
}

/* Switch to V7M main or process stack pointer.  */
static void switch_v7m_sp(CPUState *env, bool process)
{
    if(env->v7m.process_sp != process) {
        swap_u32(&env->v7m.other_sp, &env->regs[13]);
        env->v7m.process_sp = process;
    }
}

static void switch_v7m_security_state(CPUState *env, bool secure)
{
    if(secure == env->secure) {
        return;
    }

    /* If we entered this function but have no TZ, means we have a bug somewhere */
    if(unlikely(!env->v7m.has_trustzone)) {
        cpu_abort(env, "Tried to internally switch CPU state to %s but TrustZone is not enabled. This is a translation lib bug",
                  secure ? "Secure" : "Non-secure");
    }

    /* If we were in thread mode before the switch, we need to remember to swap them
     * to make sure that MSP is really the Main Stack Pointer
     */
    if(env->v7m.process_sp) {
        swap_u32(&env->v7m.other_ss_psp, &env->regs[13]);
        swap_u32(&env->v7m.other_ss_msp, &env->v7m.other_sp);
    } else {
        swap_u32(&env->v7m.other_ss_msp, &env->regs[13]);
        swap_u32(&env->v7m.other_ss_psp, &env->v7m.other_sp);
    }

    env->secure = secure;
}

void do_v7m_exception_exit(CPUState *env)
{
    uint32_t type;
    uint32_t xpsr;

    /* Restore FAULTMASK to 0 only if the interrupt that we are exiting is not NMI */
    /* See ARMv7-M Architecture Reference Manual - B1.4.3 */
    if(env->v7m.exception != 2) {
        cpu->v7m.faultmask[env->secure] = 0;
    }

    type = env->regs[15];
    if(env->v7m.exception != 0) {
        tlib_nvic_complete_irq(env->v7m.exception);
    }

    if(env->interrupt_end_callback_enabled) {
        tlib_on_interrupt_end(env->exception_index);
    }

    if(env->v7m.has_trustzone) {
        /* Location of return stack type (Secure/Non-Secure) */
        switch_v7m_security_state(env, type & (1 << 6));
    }
    /* Switch to the target stack.  */
    switch_v7m_sp(env, (type & 4) != 0);
    /* Pop registers.  */
    env->regs[0] = v7m_pop(env);
    env->regs[1] = v7m_pop(env);
    env->regs[2] = v7m_pop(env);
    env->regs[3] = v7m_pop(env);
    env->regs[12] = v7m_pop(env);
    env->regs[14] = v7m_pop(env);
    env->regs[15] = v7m_pop(env) & ~1;
    xpsr = v7m_pop(env);
    xpsr_write(env, xpsr, 0xfffffdff);
    /* Pop extended frame  */
    if(~type & ARM_EXC_RETURN_NFPCA_MASK) {
        if(env->v7m.fpccr & ARM_FPCCR_LSPACT_MASK) {
            /* FP state is still valid, pop space from stack  */
            env->v7m.fpccr ^= ARM_FPCCR_LSPACT_MASK;
            env->regs[13] += 0x48;
        } else {
            if(~env->vfp.xregs[ARM_VFP_FPEXC] & ARM_VFP_FPEXC_FPUEN_MASK) {
                /* FPU is disabled, revert SP and rise Usage Fault  */
                env->regs[13] -= 0x20;
                env->v7m.control[env->secure] &= ~ARM_CONTROL_FPCA_MASK;
                env->exception_index = EXCP_UDEF;
                cpu_loop_exit(env);
            }
            for(int i = 0; i < 8; ++i) {
                env->vfp.regs[i] = v7m_pop(env);
                env->vfp.regs[i] |= ((uint64_t)v7m_pop(env)) << 32;
            }
            vfp_set_fpscr(env, v7m_pop(env));
            /* Pop Reserved field  */
            env->regs[13] += 0x4;
        }
    }
    /* Set CONTROL.FPCA to NOT(type[ARM_EXC_RETURN_NFPCA])  */
    env->v7m.control[env->secure] ^= (env->v7m.control[env->secure] ^ ~type >> (ARM_EXC_RETURN_NFPCA - ARM_CONTROL_FPCA)) &
                                     ARM_CONTROL_FPCA_MASK;
    /* Undo stack alignment.  */
    if(xpsr & 0x200) {
        env->regs[13] |= 4;
    }
    /* ??? The exception return type specifies Thread/Handler mode.  However
       this is also implied by the xPSR value. Not sure what to do
       if there is a mismatch.  */
    /* ??? Likewise for mismatches between the CONTROL register and the stack
       pointer.  */
    if((type & ARM_EXC_RETURN_HANDLER_MODE_MASK) != 0) {
        env->v7m.handler_mode = false;
    } else {
        env->v7m.handler_mode = true;
    }
}

void do_v7m_secure_return(CPUState *env)
{
    uint32_t xpsr;

    switch_v7m_security_state(env, true);
    /* Only Thumb mode is supported for this architecture */
    env->thumb = true;

    xpsr = v7m_pop(env);
    xpsr_write(env, xpsr, 0xffffffff);
    env->regs[15] = v7m_pop(env) & ~1;

    tlib_printf(LOG_LEVEL_NOISY, "Secure return to 0x%08" PRIx32 ", xpsr: 0x%08" PRIx32, env->regs[15], xpsr);
}

void HELPER(fp_lsp)(CPUState *env)
{
    /* Save FP state if FPCCR.LSPACT is set  */
    if(unlikely(env->v7m.fpccr & ARM_FPCCR_LSPACT_MASK)) {
        env->v7m.fpccr ^= ARM_FPCCR_LSPACT_MASK;
        uint32_t fpcar = env->v7m.fpcar & ~0x3;
        for(int i = 0; i < 8; ++i) {
            stl_phys(fpcar + i * 8, env->vfp.regs[i]);
            stl_phys(fpcar + i * 8 + 4, env->vfp.regs[i] >> 32);
        }
        uint32_t fpscr = vfp_get_fpscr(env);
        stl_phys(fpcar + 0x40, fpscr);
        /* Set default values from FPDSCR to FPSCR in new context */
        vfp_set_fpscr(env, (fpscr & ~ARM_FPDSCR_VALUES_MASK) | (env->v7m.fpdscr & ARM_FPDSCR_VALUES_MASK));
    }
}

static void do_interrupt_v7m(CPUState *env)
{
    uint32_t xpsr = xpsr_read(env);
    uint32_t lr;
    uint32_t addr;
    int nr;
    int stack_status = 0;

    if(arm_feature(env, ARM_FEATURE_V8)) {
        /* [31:7] PREFIX and RES1.
         *
         * All SecureExtensions bits are set to their disabled state:
         * [6]: 0
         * [5]: 1
         * [0]: 0
         */
        lr = 0xffffffb0;

        /* Mode */
        if(env->v7m.handler_mode == 0) {
            lr |= 1 << 3;
        }

        /* SPSEL */
        if(env->v7m.process_sp != 0) {
            lr |= 1 << 2;
        }

        if(env->v7m.has_trustzone) {
            /* There are two most relevant bits here
             * [0] ES (Exception Secure) - "The security domain the exception was taken to"
             * so whether we will be in secure mode after taking this exception
             * [6] S (Secure or Non-secure stack) - "Indicates whether a Secure or Non-secure stack is used to restore stack frame
             * on exception return" so if we will return to secure or non-secure mode, when executin exception return later
             *
             * By default set to the same mode the exception was taken from, but this will be changed later depending on:
             * - value of NVIC_ITNSx registers for hardware IRQ
             * - value of AIRCR.BFHFNMINS for HardFault, NMI, BusFault
             * Look at "EXCP_IRQ" for how this logic works
             */
            lr |= env->secure << 6;
        }

    } else {
        lr = 0xfffffff1;
        if(env->v7m.exception == 0) {
            lr |= 0x8;
            lr |= (env->v7m.process_sp != 0) << 2;
        }
    }

    /* v7-M and v8-M share FP stack FP context active fields */
    if(env->v7m.control[env->secure] & ARM_CONTROL_FPCA_MASK) {
        lr ^= ARM_EXC_RETURN_NFPCA_MASK;
    }

    /* For exceptions we just mark as pending on the NVIC, and let that
       handle it.  */
    /* TODO: Need to escalate if the current priority is higher than the
       one we're raising.  */
    switch(env->exception_index) {
        case EXCP_UDEF:
            tlib_nvic_set_pending_irq(ARMV7M_EXCP_USAGE);
            env->v7m.fault_status |= USAGE_FAULT_UNDEFINSTR;
            return;
    }
    tlib_nvic_set_pending_irq(ARMV7M_EXCP_DEBUG);
    return;
    case EXCP_IRQ:
        env->v7m.exception = tlib_nvic_acknowledge_irq();
        if(env->v7m.has_trustzone) {
            /* If we have TrustZone, NVIC_ITNSx determines the security state
             * the hardware IRQ is taken to */
            bool secure_target = env->secure;
            if(env->v7m.exception >= ARMV7M_EXCP_HARDIRQ0) {
                secure_target = tlib_nvic_interrupt_targets_secure(env->v7m.exception);
            } else {
                switch(env->v7m.exception) {
                    case ARMV7M_EXCP_NMI:
                    case ARMV7M_EXCP_BUS:
                        /* `AIRCR.BFHFNMINS` determines this behavior, but we store its value
                         * within this structure, the same as for hard IRQ */
                        secure_target = tlib_nvic_interrupt_targets_secure(env->v7m.exception);
                        break;
                    case ARMV7M_EXCP_HARD:
                        /* According to pseudocode "ExceptionTargetsSecure" */
                        secure_target = tlib_nvic_interrupt_targets_secure(env->v7m.exception) || env->secure;
                        break;
                    /* Reset and Secure Fault are secure only */
                    case ARMV7M_EXCP_RESET:
                    case ARMV7M_EXCP_SECURE:
                        secure_target = true;
                        break;
                    /* Debug monitor (ARMV7M_EXCP_DEBUG) should be configured with `DEMCR.SDME` but since it's unimplemented
                     * we implement it as banked, to minimize side-effects
                     * Any other exception is banked too */
                    default:
                        break;
                }
            }
            lr |= deposit32(lr, 0, 1, secure_target);
        }
        break;
    default:
        cpu_abort(env, "Unhandled exception 0x%x\n", env->exception_index);
        return; /* Never happens.  Keep compiler happy.  */
    }

    env->v7m.handler_mode = true;
    env->condexec_bits = 0;

    /* Align stack pointer.  */
    /* ??? Should do this if Configuration Control Register
       STACKALIGN bit is set or extended frame is being pushed.  */
    if(env->regs[13] & 4) {
        env->regs[13] -= 4;
        xpsr |= 0x200;
    }
    /* Push extended frame  */
    if(env->v7m.control[env->secure] & ARM_CONTROL_FPCA_MASK) {
        env->v7m.control[env->secure] &= ~ARM_CONTROL_FPCA_MASK;
        if(env->v7m.fpccr & ARM_FPCCR_LSPEN_MASK) {
            /* Set lazy FP state preservation  */
            env->v7m.fpccr |= ARM_FPCCR_LSPACT_MASK;
            env->regs[13] -= 0x48;
            env->v7m.fpcar = env->regs[13];
        } else {
            if(~env->vfp.xregs[ARM_VFP_FPEXC] & ARM_VFP_FPEXC_FPUEN_MASK) {
                /* FPU is disabled, revert SP and rise Usage Fault  */
                if(xpsr & 0x200) {
                    env->regs[13] |= 4;
                }
                env->exception_index = EXCP_UDEF;
                cpu_loop_exit(env);
            }
            /* Push Reserved field  */
            env->regs[13] -= 0x4;
            uint32_t fpscr = vfp_get_fpscr(env);
            v7m_push(env, fpscr);
            /* Set default values from FPDSCR to FPSCR in new context */
            vfp_set_fpscr(env, (fpscr & ~ARM_FPDSCR_VALUES_MASK) | (env->v7m.fpdscr & ARM_FPDSCR_VALUES_MASK));
            for(int i = 0; i < 8; ++i) {
                /* We need to swap low and high register parts, to pop them correctly on state restore.
                 * The state can be restored on excp exit, or by specific load instructions */
                v7m_push(env, env->vfp.regs[8 - i] >> 32);
                v7m_push(env, env->vfp.regs[8 - i]);
            }
        }
    }
    /* Switch to the handler mode.  */
    stack_status |= v7m_push(env, xpsr);
    stack_status |= v7m_push(env, env->regs[15]);
    stack_status |= v7m_push(env, env->regs[14]);
    stack_status |= v7m_push(env, env->regs[12]);
    stack_status |= v7m_push(env, env->regs[3]);
    stack_status |= v7m_push(env, env->regs[2]);
    stack_status |= v7m_push(env, env->regs[1]);
    stack_status |= v7m_push(env, env->regs[0]);

    if(env->v7m.has_trustzone) {
        switch_v7m_security_state(env, lr & 1);
    }
    switch_v7m_sp(env, 0);

    env->uncached_cpsr &= ~CPSR_IT;

    find_pending_irq_if_primask_unset(env);

    env->regs[14] = lr;
    addr = ldl_phys(env->v7m.vecbase[env->secure] + env->v7m.exception * 4);
    env->regs[15] = addr & 0xfffffffe;
    env->thumb = addr & 1;
    if(stack_status) {
        do_v7m_exception_exit(env);
        env->exception_index = EXCP_DATA_ABORT;
        do_interrupt_v7m(env);
    }

    arm_announce_stack_change();
}
#endif

/* Handle a CPU exception.  */
void do_interrupt(CPUState *env)
{
    uint32_t addr;
    uint32_t mask;
    int new_mode;
    uint32_t offset;

    if(env->interrupt_begin_callback_enabled) {
        tlib_on_interrupt_begin(env->exception_index);
    }

#ifdef TARGET_PROTO_ARM_M
    do_interrupt_v7m(env);
    return;
#endif
    /* TODO: Vectored interrupt controller.  */
    switch(env->exception_index) {
        case EXCP_UDEF:
            new_mode = ARM_CPU_MODE_UND;
            addr = 0x04;
            mask = CPSR_I;
            if(env->thumb) {
                offset = 2;
            } else {
                offset = 4;
            }
            break;
        case EXCP_SWI:
            /* Check for semihosting interrupt.  */
            if(env->thumb) {
                mask = lduw_code(env->regs[15] - 2) & 0xff;
            } else {
                mask = ldl_code(env->regs[15] - 4) & 0xffffff;
            }
            /* Only intercept calls from privileged modes, to provide some
               semblance of security.  */
            if(((mask == 0x123456 && !env->thumb) || (mask == 0xab && env->thumb)) &&
               (env->uncached_cpsr & CPSR_M) != ARM_CPU_MODE_USR) {
                env->regs[0] = tlib_do_semihosting();
                return;
            }
            new_mode = ARM_CPU_MODE_SVC;
            addr = 0x08;
            mask = CPSR_I;
            /* The PC already points to the next instruction.  */
            offset = 0;
            break;
        case EXCP_BKPT:
            /* See if this is a semihosting syscall.  */
            mask = lduw_code(env->regs[15]) & 0xff;
            if(mask == 0xab && (env->uncached_cpsr & CPSR_M) != ARM_CPU_MODE_USR) {
                env->regs[15] += 2;
                env->regs[0] = tlib_do_semihosting();
                return;
            }
            env->cp15.c5_insn = 2;
            /* Go to prefetch abort.  */
            goto case_EXCP_PREFETCH_ABORT;
        case EXCP_PREFETCH_ABORT:
        case_EXCP_PREFETCH_ABORT:
            new_mode = ARM_CPU_MODE_ABT;
            addr = 0x0c;
            mask = CPSR_A | CPSR_I;
            offset = 4;
            break;
        case EXCP_DATA_ABORT:
            new_mode = ARM_CPU_MODE_ABT;
            addr = 0x10;
            mask = CPSR_A | CPSR_I;
            offset = 8;
            break;
        case EXCP_IRQ:
            new_mode = ARM_CPU_MODE_IRQ;
            addr = 0x18;
            /* Disable IRQ and imprecise data aborts.  */
            mask = CPSR_A | CPSR_I;
            offset = 4;
            break;
        case EXCP_FIQ:
            new_mode = ARM_CPU_MODE_FIQ;
            addr = 0x1c;
            /* Disable FIQ, IRQ and imprecise data aborts.  */
            mask = CPSR_A | CPSR_I | CPSR_F;
            offset = 4;
            break;
        default:
            cpu_abort(env, "Unhandled exception 0x%x\n", env->exception_index);
            return; /* Never happens.  Keep compiler happy.  */
    }
    /* High vectors.  */
    if(env->cp15.c1_sys & (1 << 13)) {
        /* High vectors are not affected by VBAR */
        addr += 0xffff0000;
    } else {
        /* CPUs w/ Security Extensions allow for relocation of the vector table.
         * c12_vbar is initialized to zero so the following maintains compatible
         * with targets that don't have Security Extensions.
         *
         * Even though VBAR can only be set by software for such CPUs, accessors
         * are exported for all pre-v8 A-profile and R-profile CPUs. Therefore
         * it can be set for all such CPUs.
         */
        addr += env->cp15.c12_vbar;
    }
    switch_mode(env, new_mode);
    env->spsr = cpsr_read(env);
    /* Clear IT bits.  */
    env->condexec_bits = 0;
    /* Switch to the new mode, and to the correct instruction set.  */
    env->uncached_cpsr = (env->uncached_cpsr & ~CPSR_M) | new_mode;
    env->uncached_cpsr |= mask;

    /* this is a lie, as the was no c1_sys on V4T/V5, but who cares
     * and we should just guard the thumb mode on V4 */
    if(arm_feature(env, ARM_FEATURE_V4T)) {
        env->thumb = (env->cp15.c1_sys & (1 << 30)) != 0;
    }
    env->regs[14] = env->regs[15] + offset;
    env->regs[15] = addr;
    set_interrupt_pending(env, CPU_INTERRUPT_EXITTB);

    arm_announce_stack_change();
}

/* Check section/page access permissions.
   Returns the page protection flags, or zero if the access is not
   permitted.  */
static inline int check_ap(CPUState *env, int ap, int domain, int access_type, int is_user)
{
    int prot_ro;

    if(domain == 3) {
        return PAGE_READ | PAGE_WRITE;
    }

    if(access_type == ACCESS_DATA_STORE) {
        prot_ro = 0;
    } else {
        prot_ro = PAGE_READ;
    }

    switch(ap) {
        case 0:
            if(access_type == ACCESS_DATA_STORE) {
                return 0;
            }
            switch((env->cp15.c1_sys >> 8) & 3) {
                case 1:
                    return is_user ? 0 : PAGE_READ;
                case 2:
                    return PAGE_READ;
                default:
                    return 0;
            }
        case 1:
            return is_user ? 0 : PAGE_READ | PAGE_WRITE;
        case 2:
            if(is_user) {
                return prot_ro;
            } else {
                return PAGE_READ | PAGE_WRITE;
            }
        case 3:
            return PAGE_READ | PAGE_WRITE;
        case 4: /* Reserved.  */
            return 0;
        case 5:
            return is_user ? 0 : prot_ro;
        case 6:
            return prot_ro;
        case 7:
            if(!arm_feature(env, ARM_FEATURE_V6K)) {
                return 0;
            }
            return prot_ro;
        default:
            abort();
    }
}

static uint32_t get_level1_table_address(CPUState *env, uint32_t address)
{
    uint32_t table;

    if(address & env->cp15.c2_mask) {
        table = env->cp15.c2_base1 & 0xffffc000;
    } else {
        table = env->cp15.c2_base0 & env->cp15.c2_base_mask;
    }

    table |= (address >> 18) & 0x3ffc;
    return table;
}

static int get_phys_addr_v5(CPUState *env, uint32_t address, int access_type, int is_user, uint32_t *phys_ptr, int *prot,
                            target_ulong *page_size)
{
    int code;
    uint32_t table;
    uint32_t desc;
    int type;
    int ap;
    int domain;
    uint32_t phys_addr;

    /* Pagetable walk.  */
    /* Lookup l1 descriptor.  */
    table = get_level1_table_address(env, address);
    desc = ldl_phys(table);
    type = (desc & 3);
    domain = (env->cp15.c3 >> ((desc >> 4) & 0x1e)) & 3;
    if(type == 0) {
        /* Section translation fault.  */
        code = 5;
        goto do_fault;
    }
    if(domain == 0 || domain == 2) {
        if(type == 2) {
            code = 9; /* Section domain fault.  */
        } else {
            code = 11; /* Page domain fault.  */
        }
        goto do_fault;
    }
    if(type == 2) {
        /* 1Mb section.  */
        phys_addr = (desc & 0xfff00000) | (address & 0x000fffff);
        ap = (desc >> 10) & 3;
        code = 13;
        *page_size = 1024 * 1024;
    } else {
        /* Lookup l2 entry.  */
        if(type == 1) {
            /* Coarse pagetable.  */
            table = (desc & 0xfffffc00) | ((address >> 10) & 0x3fc);
        } else {
            /* Fine pagetable.  */
            table = (desc & 0xfffff000) | ((address >> 8) & 0xffc);
        }
        desc = ldl_phys(table);
        switch(desc & 3) {
            case 0: /* Page translation fault.  */
                code = 7;
                goto do_fault;
            case 1: /* 64k page.  */
                phys_addr = (desc & 0xffff0000) | (address & 0xffff);
                ap = (desc >> (4 + ((address >> 13) & 6))) & 3;
                *page_size = 0x10000;
                break;
            case 2: /* 4k page.  */
                phys_addr = (desc & 0xfffff000) | (address & 0xfff);
                ap = (desc >> (4 + ((address >> 13) & 6))) & 3;
                *page_size = 0x1000;
                break;
            case 3: /* 1k page.  */
                if(type == 1) {
                    if(arm_feature(env, ARM_FEATURE_XSCALE)) {
                        phys_addr = (desc & 0xfffff000) | (address & 0xfff);
                    } else {
                        /* Page translation fault.  */
                        code = 7;
                        goto do_fault;
                    }
                } else {
                    phys_addr = (desc & 0xfffffc00) | (address & 0x3ff);
                }
                ap = (desc >> 4) & 3;
                *page_size = 0x400;
                break;
            default:
                /* Never happens, but compiler isn't smart enough to tell.  */
                abort();
        }
        code = 15;
    }
    *prot = check_ap(env, ap, domain, access_type, is_user);
    if(!*prot) {
        /* Access permission fault.  */
        goto do_fault;
    }
    *prot |= PAGE_EXEC;
    *phys_ptr = phys_addr;
    return TRANSLATE_SUCCESS;
do_fault:
    return code | (domain << 4);  //  TRANSLATE_FAIL
}

static int get_phys_addr_lpae(CPUState *env, uint32_t address, int access_type, int is_user, uint32_t *phys_ptr, int *prot,
                              target_ulong *page_size)
{
    //  NOTE: the implementation is limited to u-boot usecase (i.e. identity mapping, no faults)
    uint32_t table;
    uint64_t desc;
    uint32_t index;
    int type;
    uint32_t phys_addr;

    /* page table walk */

    /* LEVEL 1 */
    table = env->cp15.c2_base0_ea;
    index = address >> 30;
    desc = ldq_phys(table + index * 8);
    type = desc & 3;

    switch(type) {
        case 0:
        case 2:
            //  descriptor type: invalid
            goto do_fault;
        case 1:
            //  descriptor type: block
            phys_addr = (desc & 0xC0000000) | (address & 0x3FFFFFFF);
            *page_size = 0x40000000;
            goto success;
    }
    //  type: table
    //  TODO: check page fault etc.
    table = desc & 0xfffff000;  //  level 2 PT address = desc[31:13]

    /* LEVEL 2 */
    index = (address >> 21) & 0b111111111;
    desc = ldq_phys(table + index * 8);
    type = desc & 3;

    switch(type) {
        case 0:
        case 2:
            //  descriptor type: invalid
            goto do_fault;
        case 1:
            //  descriptor type: block
            phys_addr = (desc & 0xFFE00000) | (address & 0x1FFFFF);
            *page_size = 0x200000;
            goto success;
    }
    //  type: table
    //  TODO: check page fault etc.
    table = desc & 0xfffff000;

    /* LEVEL 3 */
    index = (address >> 12) & 0b111111111;
    desc = ldq_phys(table + index * 8);
    //  TODO: check page fault etc.
    phys_addr = (desc & 0xfffff000) | (address & 0xfff);
    *page_size = 0x1000;
success:
    *phys_ptr = phys_addr;
    *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
    return TRANSLATE_SUCCESS;

do_fault:
    return TRANSLATE_FAIL;
}

static int get_phys_addr_v6(CPUState *env, uint32_t address, int access_type, int is_user, uint32_t *phys_ptr, int *prot,
                            target_ulong *page_size)
{
    int code;
    uint32_t table;
    uint32_t desc;
    uint32_t xn;
    int type;
    int ap;
    int domain;
    uint32_t phys_addr;

    /* Pagetable walk.  */
    /* Lookup l1 descriptor.  */
    table = get_level1_table_address(env, address);
    desc = ldl_phys(table);
    type = (desc & 3);
    if(type == 0) {
        /* Section translation fault.  */
        code = 5;
        domain = 0;
        goto do_fault;
    } else if(type == 2 && (desc & (1 << 18))) {
        /* Supersection.  */
        domain = 0;
    } else {
        /* Section or page.  */
        domain = (desc >> 4) & 0x1e;
    }
    domain = (env->cp15.c3 >> domain) & 3;
    if(domain == 0 || domain == 2) {
        if(type == 2) {
            code = 9; /* Section domain fault.  */
        } else {
            code = 11; /* Page domain fault.  */
        }
        goto do_fault;
    }
    if(type == 2) {
        if(desc & (1 << 18)) {
            /* Supersection.  */
            phys_addr = (desc & 0xff000000) | (address & 0x00ffffff);
            *page_size = 0x1000000;
        } else {
            /* Section.  */
            phys_addr = (desc & 0xfff00000) | (address & 0x000fffff);
            *page_size = 0x100000;
        }
        ap = ((desc >> 10) & 3) | ((desc >> 13) & 4);
        xn = desc & (1 << 4);
        code = 13;
    } else {
        /* Lookup l2 entry.  */
        table = (desc & 0xfffffc00) | ((address >> 10) & 0x3fc);
        desc = ldl_phys(table);
        ap = ((desc >> 4) & 3) | ((desc >> 7) & 4);
        switch(desc & 3) {
            case 0: /* Page translation fault.  */
                code = 7;
                goto do_fault;
            case 1: /* 64k page.  */
                phys_addr = (desc & 0xffff0000) | (address & 0xffff);
                xn = desc & (1 << 15);
                *page_size = 0x10000;
                break;
            case 2:  //  4k page.
            case 3:
                phys_addr = (desc & 0xfffff000) | (address & 0xfff);
                xn = desc & 1;
                *page_size = 0x1000;
                break;
            default:
                /* Never happens, but compiler isn't smart enough to tell.  */
                abort();
        }
        code = 15;
    }
    if(domain == 3) {
        *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
    } else {
        if(xn && access_type == ACCESS_INST_FETCH) {
            goto do_fault;
        }

        /* The simplified model uses AP[0] as an access control bit.  */
        if((env->cp15.c1_sys & (1 << 29)) && (ap & 1) == 0) {
            /* Access flag fault.  */
            code = (code == 15) ? 6 : 3;
            goto do_fault;
        }
        *prot = check_ap(env, ap, domain, access_type, is_user);
        if(!*prot) {
            /* Access permission fault.  */
            goto do_fault;
        }
        if(!xn) {
            *prot |= PAGE_EXEC;
        }
    }
    *phys_ptr = phys_addr;
    return TRANSLATE_SUCCESS;
do_fault:
    return code | (domain << 4);  //  TRANSLATE_FAIL
}

static int cortexm_check_default_mapping(uint32_t address, int *prot, int access_type)
{
    switch(address) {
        case 0x00000000 ... 0x1FFEFFFF:
            *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
            break;
        case 0x1FFF0000 ... 0x1FFF77FF:
            *prot = PAGE_READ | PAGE_EXEC;
            break;
        case 0x1FFF7800 ... 0x1FFFFFFF:
        case 0x20000000 ... 0x3FFFFFFF:
        case 0x60000000 ... 0x7FFFFFFF:
        case 0x80000000 ... 0x9FFFFFFF:
            *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
            break;
        case 0x40000000 ... 0x5FFFFFFF:
        case 0xA0000000 ... 0xBFFFFFFF:
        case 0xC0000000 ... 0xDFFFFFFF:
        case 0xE0000000 ... 0xE00FFFFF:
            *prot = PAGE_READ | PAGE_WRITE;
            break;
        case 0xE0100000 ... 0xFFFFFFFF:
        default:
            *prot = 0;
            return PAGE_READ | PAGE_WRITE | PAGE_EXEC;
    }
    return !(*prot & (1 << access_type));
}

static int pmsav7_check_default_mapping(uint32_t address, int *prot, int access_type)
{
    *prot = PAGE_READ | PAGE_WRITE;
    switch(address) {
        case 0xF0000000 ... 0xFFFFFFFF:
            //  executable if high exception vectors are selected
            if(!(env->cp15.c1_sys & (1 << 13))) {
                break;
            }
            /* fallthrough */
        case 0x00000000 ... 0x7FFFFFFF:
            *prot |= PAGE_EXEC;
        default:
            break;
    }
    return (*prot & (1 << access_type)) ? MPU_SUCCESS : MPU_PERMISSION_FAULT;
}

static int get_mpu_subregion_number(uint32_t region_base_address, uint32_t region_size, uint32_t address)
{
    /* Subregion size is 2^(region_size - 3) */
    uint32_t subregion_size = 1 << (region_size - 3);
    return (address - region_base_address) / subregion_size;
}

static bool page_with_address_is_fully_covered_by_consistent_mpu_subregions(uint32_t subregion_disable_mask,
                                                                            uint32_t region_base_address, uint32_t region_size,
                                                                            uint32_t address)
{
    int i, first_subregion_number, last_subregion_number;
    uint32_t first_subregion_state;
    uint32_t page_start = address & TARGET_PAGE_MASK;
    uint32_t page_size = TARGET_PAGE_SIZE;

    if(region_base_address > page_start || region_base_address + region_size < page_start + page_size) {
        //  No need to check particular subregions as a page is not contained within the whole region
        return false;
    }

    first_subregion_number = get_mpu_subregion_number(region_base_address, region_size, page_start);
    last_subregion_number = get_mpu_subregion_number(region_base_address, region_size, page_start + page_size - 1);

    if(first_subregion_number == last_subregion_number) {
        return true;
    }

    first_subregion_state = !(subregion_disable_mask & (1 << first_subregion_number));
    for(i = first_subregion_number + 1; i <= last_subregion_number; i++) {
        if(first_subregion_state != !(subregion_disable_mask & (1 << i))) {
            //  There are mixed disabled and enabled subregions covering a single page
            return false;
        }
    }
    return true;
}

static int get_phys_addr_mpu(CPUState *env, uint32_t address, int access_type, int is_user, uint32_t *phys_ptr, int *prot,
                             target_ulong *page_size)
{
    int n;
    uint32_t base;
    uint32_t size;
    uint32_t mask;
    uint32_t perms;
    bool page_contains_mpu_region = false;

    *phys_ptr = address;
    *prot = 0;

#if DEBUG
    tlib_printf(LOG_LEVEL_DEBUG, "MPU: Trying to access address 0x%X", address);
#endif

    for(n = env->number_of_mpu_regions - 1; n >= 0; n--) {
        if(!(env->cp15.c6_size_and_enable[n] & MPU_REGION_ENABLED_BIT)) {
            continue;
        }
        switch((env->cp15.c6_size_and_enable[n] & MPU_SIZE_FIELD_MASK) >> 1) {
            case 0 ... 3:
                tlib_printf(LOG_LEVEL_WARNING,
                            "Encountered MPU region size smaller than 32bytes, this is an unpredictable setting!");
                continue;
            default:
                size = ((env->cp15.c6_size_and_enable[n] & MPU_SIZE_FIELD_MASK) >> 1) + 1;
                break;
        }

        base = env->cp15.c6_base_address[n];
        mask = (1ull << size) - 1;

        if((address & TARGET_PAGE_MASK) == (base & TARGET_PAGE_MASK)) {
            page_contains_mpu_region = true;
        }

        if(base & mask) {
            /* Misaligned base addr to region */
            continue;
        }
        /* Check if the region is enabled */
        if(address >= base && address <= base + mask) {
            /* Check subregions, only if region size is equal to or bigger than 256 bytes (region size = 2^size) */
            if(size >= 8) {
                if(!page_with_address_is_fully_covered_by_consistent_mpu_subregions(env->cp15.c6_subregion_disable[n], base,
                                                                                    1 << size, address)) {
                    /* MPU subregions with the same state (enabled/disabled) don't cover the whole page.
                     * Setting page size != TARGET_PAGE_SIZE effectively makes the tlb page entry one-shot:
                     * Thanks to this every access to this page will be verified against MPU.
                     */
                    *page_size = 0;
                }
                if(env->cp15.c6_subregion_disable[n] & (1 << get_mpu_subregion_number(base, size, address))) {
                    /* Subregion containing this address is disabled, try to match this address to a different region. */
                    continue;
                }
            } else {
                /* The page is not fully covered by a single MPU region */
                *page_size = 0;
            }

            break;
        }
    }

    if(n < 0) {  //  background fault
        int background_result;
        if(arm_feature(env, ARM_FEATURE_PMSA)) {
            if(is_user || !(env->cp15.c1_sys & (1 << 17 /* BR, Background Region */))) {
                background_result = MPU_BACKGROUND_FAULT;
            } else {
                background_result = pmsav7_check_default_mapping(address, prot, access_type);
            }
        } else if(!is_user) {
            background_result = cortexm_check_default_mapping(address, prot, access_type);
        } else {
            background_result = TRANSLATE_FAIL;
        }

        if(background_result == TRANSLATE_SUCCESS && page_contains_mpu_region) {
            /* Background pages cannot be stored in tlb if those pages contain any MPU regions
             * as access checks will not be performed for pages that are present in TLB.
             * Setting page size != TARGET_PAGE_SIZE effectively makes the tlb page entry one-shot:
             * Thanks to this every access to this page will be verified against MPU.
             */
            *page_size = 0;
        }
        return background_result;
    }

    perms = (env->cp15.c6_access_control[n] & MPU_PERMISSION_FIELD_MASK) >> 8;

    switch(perms) {
        case 0:
            return MPU_PERMISSION_FAULT;
        case 1:
            if(is_user) {
                return MPU_PERMISSION_FAULT;
            }
            *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
            break;
        case 2:
            *prot = PAGE_READ | PAGE_EXEC;
            if(!is_user) {
                *prot |= PAGE_WRITE;
            }
            break;
        case 3:
            *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
            break;
        case 5:
            if(is_user) {
                return MPU_PERMISSION_FAULT;
            }
            *prot = PAGE_READ | PAGE_EXEC;
            break;
        case 6:
            *prot = PAGE_READ | PAGE_EXEC;
            break;
#ifdef TARGET_PROTO_ARM_M
        case 7:
            *prot |= PAGE_READ | PAGE_EXEC;
            break;
#endif
        default:
            /* Bad permission.  */
            break;
    }

    /* Check if the region is executable */
    if((env->cp15.c6_access_control[n] & MPU_NEVER_EXECUTE_BIT)) {
        *prot &= ~PAGE_EXEC;
    }

    /* PAGE_READ  = 1 ; ACCESS_TYPE = 0
     * PAGE_WRITE = 2 ; ACCESS_TYPE = 1
     * PAGE_EXEC  = 3 ; ACCESS_TYPE = 2
     */
    if(*prot & (1 << access_type)) {
        return TRANSLATE_SUCCESS;
    }
    return MPU_PERMISSION_FAULT;
}

#ifdef TARGET_PROTO_ARM_M

static int cortexm_check_default_mapping_v8(uint32 address)
{
    switch(address) {
        case 0x00000000 ... 0x7FFFFFFF:
            return PAGE_READ | PAGE_WRITE | PAGE_EXEC;
            break;
        //  Devices
        case 0x80000000 ... 0xFFFFFFFF:
            return PAGE_READ | PAGE_WRITE;
            break;
        default:
            tlib_abortf("Address out of range. This should never happen");
            return 0; /* Never happens.  Keep compiler happy.  */
    }
}

static inline bool pmsav8_get_region(CPUState *env, uint32_t address, bool secure, int *region_index, bool *multiple_regions)
{
    int n;
    bool hit = false;
    *multiple_regions = false;
    *region_index = -1;

    for(n = env->number_of_mpu_regions - 1; n >= 0; n--) {

        if(!(env->pmsav8[secure].rlar[n] & 0x1)) {
            /* Region disabled */
            continue;
        }

        uint32_t base = env->pmsav8[secure].rbar[n] & ~0x1f;
        uint32_t limit = env->pmsav8[secure].rlar[n] | 0x1f;
        if(address < base || address > limit) {
            /* Addr not in this region */
            continue;
        }

        /* region matched */
        if(hit) {
            /* multiple regions always return a failure
             * in this case region_index _must not_ be used
             */
            *multiple_regions = true;
            *region_index = -1;
            break;
        }

        hit = true;
        *region_index = n;
    }
    return hit;
}

#define PMSA_ENABLED(ctrl)    ((ctrl & 0b001))
#define PMSA_PRIVDEFENA(ctrl) ((ctrl & 0b100))
#define PMSA_AP_PRIVONLY(ap)  ((ap & 0b01) == 0)
#define PMSA_AP_READONLY(ap)  ((ap & 0b10) != 0)

static inline int pmsav8_get_phys_addr(CPUState *env, uint32_t address, bool secure, int access_type, int is_user,
                                       uint32_t *phys_ptr, int *prot, target_ulong *page_size)
{
    bool hit;
    int resolved_region;
    bool multiple_regions = false;
    bool mpu_enabled = PMSA_ENABLED(env->pmsav8[secure].ctrl);

    /* flat memory mapping */
    *phys_ptr = address;
    *prot = 0;

    if(!mpu_enabled) {
        hit = false;
    } else {
        hit = pmsav8_get_region(env, address, secure, &resolved_region, &multiple_regions);

        /* Overlapping regions generate MemManage Fault
         * R_LLLP in Arm® v8-M Architecture Reference Manual DDI0553B.l ID30062020 */
        if(multiple_regions) {
            goto do_fault;
        }
    }

    if(hit) {
        int rbar = env->pmsav8[secure].rbar[resolved_region];
        int xn = extract32(rbar, 0, 1);
        int ap = extract32(rbar, 1, 2);

        if(!PMSA_AP_PRIVONLY(ap) || !is_user) {
            *prot |= PAGE_READ;
            if(!PMSA_AP_READONLY(ap)) {
                *prot |= PAGE_WRITE;
            }
        }

        if(!xn) {
            *prot |= PAGE_EXEC;
        }

        /* Check that the hit region fully covers the tlb page
         */
        uint32_t region_start = env->pmsav8[secure].rbar[resolved_region] & ~0x1f;
        uint32_t region_end = env->pmsav8[secure].rlar[resolved_region] | 0x1f;
        if((address & TARGET_PAGE_MASK) == (region_start & TARGET_PAGE_MASK)) {
            //  Region starts mid page
            *page_size -= (region_start & ~TARGET_PAGE_MASK);
        }
        if((address & TARGET_PAGE_MASK) == (region_end & TARGET_PAGE_MASK)) {
            //  Region ends mid page
            *page_size -= TARGET_PAGE_SIZE - (region_end & ~TARGET_PAGE_MASK);
        }
    } else {
        /* No region hit, use background region if:
         * - MPU disabled: for all accesses
         * - MPU enabled: for privileged accesses if default memory map is enabled (PRIVDEFENA)
         */
        if(!mpu_enabled || (!is_user && PMSA_PRIVDEFENA(env->pmsav8[secure].ctrl))) {
            *prot = cortexm_check_default_mapping_v8(address);
        } else {
            goto do_fault;
        }
    }

    if(is_page_access_valid(*prot, access_type)) {
        return TRANSLATE_SUCCESS;
    }

do_fault:
    return TRANSLATE_FAIL;
}
#endif

inline int get_phys_addr(CPUState *env, uint32_t address, int access_type, int is_user, uint32_t *phys_ptr, int *prot,
                         target_ulong *page_size, int no_page_fault)
{
    if(unlikely(cpu->external_mmu_enabled)) {
        return get_external_mmu_phys_addr(env, address, access_type, phys_ptr, prot, no_page_fault);
    }

    /* Fast Context Switch Extension.  */
    if(address < 0x02000000) {
        address += env->cp15.c13_fcse;
    }

    /* Handle v8M specific MPU */
#ifdef TARGET_PROTO_ARM_M
    if(arm_feature(env, ARM_FEATURE_V8)) {
        *page_size = TARGET_PAGE_SIZE;
        if(env->number_of_mpu_regions == 0) {
            /* MPU disabled */
            *phys_ptr = address;
            *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
            return TRANSLATE_SUCCESS;
        }
        return pmsav8_get_phys_addr(env, address, env->secure, access_type, is_user, phys_ptr, prot, page_size);
    }
#endif

    if((env->cp15.c1_sys & 1) == 0) {
        /* MMU/MPU disabled.  */
        *phys_ptr = address;
        *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        *page_size = TARGET_PAGE_SIZE;
        return TRANSLATE_SUCCESS;
    } else if(arm_feature(env, ARM_FEATURE_MPU)) {
        //  Set default page_size for MPU background fault checks.
        //  Size of region for background mappings is bigger than TARGET_PAGE_SIZE
        //  and our TLB does not support large pages (tlb_add_large_page is suboptimal) so it's a sane default.
        //  We could also extend pmsav7_check_default_mapping and cortexm_check_default_mapping
        //  to return region size but it doesn't bring any advantage.
        //  If MPU uses more granular permissions, it will result in `TLB_ONE_SHOT` tlb entry
        //  on successful translation.
        *page_size = TARGET_PAGE_SIZE;
        return get_phys_addr_mpu(env, address, access_type, is_user, phys_ptr, prot, page_size);
    } else if(env->cp15.c2_ttbcr_eae) {
        return get_phys_addr_lpae(env, address, access_type, is_user, phys_ptr, prot, page_size);
    } else if(env->cp15.c1_sys & (1 << 23)) {
        return get_phys_addr_v6(env, address, access_type, is_user, phys_ptr, prot, page_size);
    } else {
        return get_phys_addr_v5(env, address, access_type, is_user, phys_ptr, prot, page_size);
    }
}

int cpu_handle_mmu_fault(CPUState *env, target_ulong address, int access_type, int mmu_idx, int no_page_fault)
{
    uint32_t phys_addr = 0;
    target_ulong page_size = 0;
    int prot = 0;
    int ret, is_user;

    is_user = mmu_idx == MMU_USER_IDX;
    ret = get_phys_addr(env, address, access_type, is_user, &phys_addr, &prot, &page_size, no_page_fault);
    //  returns TRANSLATE_SUCCESS (0x0) on success
    //  for no PMSA returns c5_data/insn value
    //  for PMSA returns enum mpu_result

    if(ret == TRANSLATE_SUCCESS) {
        /* Map a single [sub]page.  */
        phys_addr &= TARGET_PAGE_MASK;
        address &= TARGET_PAGE_MASK;
        tlb_set_page(env, address, phys_addr, prot, mmu_idx, page_size);
        return TRANSLATE_SUCCESS;
    } else if(cpu->external_mmu_enabled) {
        return TRANSLATE_FAIL;
    }

    uint32_t c5_value = ret;
    if(arm_feature(env, ARM_FEATURE_PMSA)) {
        c5_value = (ret == MPU_PERMISSION_FAULT) ? PERMISSION_FAULT_STATUS_BITS : BACKGROUND_FAULT_STATUS_BITS;
    }

    if(access_type == ACCESS_INST_FETCH) {
        env->cp15.c5_insn = c5_value;
        env->cp15.c6_insn = address;
        env->exception_index = EXCP_PREFETCH_ABORT;
    } else {
        env->cp15.c5_data = c5_value;
        if(access_type == ACCESS_DATA_STORE && (arm_feature(env, ARM_FEATURE_PMSA) || arm_feature(env, ARM_FEATURE_V6))) {
            env->cp15.c5_data |= (1 << 11);
        }
        env->cp15.c6_data = address;
        env->exception_index = EXCP_DATA_ABORT;
    }
    return TRANSLATE_FAIL;
}

target_phys_addr_t cpu_get_phys_page_debug(CPUState *env, target_ulong addr)
{
    uint32_t phys_addr = 0;
    target_ulong page_size = 0;
    int prot = 0;
    int ret;

    ret = get_phys_addr(env, addr, 0, 0, &phys_addr, &prot, &page_size, 0);

    if(ret != 0) {
        return -1;
    }

    return phys_addr;
}

/* Return basic MPU access permission bits.  */
uint32_t simple_mpu_ap_bits(uint32_t val)
{
    uint32_t ret;
    uint32_t mask;
    int i;
    ret = 0;
    mask = 3;
    for(i = 0; i < 16; i += 2) {
        ret |= (val >> i) & mask;
        mask <<= 2;
    }
    return ret;
}

/* Pad basic MPU access permission bits to extended format.  */
uint32_t extended_mpu_ap_bits(uint32_t val)
{
    uint32_t ret;
    uint32_t mask;
    int i;
    ret = 0;
    mask = 3;
    for(i = 0; i < 16; i += 2) {
        ret |= (val & mask) << i;
        mask <<= 2;
    }
    return ret;
}

uint64_t HELPER(get_cp15_64bit)(CPUState *env, uint32_t insn)
{
    return tlib_read_cp15_64(insn);
}

uint32_t HELPER(get_cp15_32bit)(CPUState *env, uint32_t insn)
{
    return tlib_read_cp15_32(insn);
}

void HELPER(set_cp15_64bit)(CPUState *env, uint32_t insn, uint32_t val_1, uint32_t val_2)
{
    uint64_t val = ((uint64_t)val_2 << 32) | val_1;
    tlib_write_cp15_64(insn, val);
}

void HELPER(set_cp15_32bit)(CPUState *env, uint32_t insn, uint32_t val)
{
    tlib_write_cp15_32(insn, val);
}

void HELPER(set_r13_banked)(CPUState *env, uint32_t mode, uint32_t val)
{
    if((env->uncached_cpsr & CPSR_M) == mode) {
        env->regs[13] = val;
    } else {
        env->banked_r13[bank_number(mode)] = val;
    }
}

uint32_t HELPER(get_r13_banked)(CPUState *env, uint32_t mode)
{
    if((env->uncached_cpsr & CPSR_M) == mode) {
        return env->regs[13];
    } else {
        return env->banked_r13[bank_number(mode)];
    }
}

#ifdef TARGET_PROTO_ARM_M
/* Helpful note: TrustZone sets bit 7, to indicate non-secure bank for the register
 * Otherwise, the encoding is the same (see: "Armv8-M Architecture Reference Manual")
 */
#define NON_SECURE_REG(x) (x | (1 << 7))
#define IS_REG_NS(x)      (x & (1 << 7))

/* An access to a register not ending in _NS returns the register associated with the current
 * Security state. Access to a register ending in _NS in Secure state returns the Non-secure
 * register. */
uint32_t HELPER(v7m_mrs)(CPUState *env, uint32_t reg)
{
    bool is_secure = env->secure;
    if(IS_REG_NS(reg)) {
        if(!env->secure) {
            //  Access to a register ending in _NS in Non-secure state is RAZ/WI
            return 0;
        }
        is_secure = false;
    }

    switch(reg) {
        case 0: /* APSR */
            return xpsr_read(env) & 0xf8000000;
        case 1: /* IAPSR */
            return xpsr_read(env) & 0xf80001ff;
        case 2: /* EAPSR */
            return xpsr_read(env) & 0xff00fc00;
        case 3: /* xPSR */
            return xpsr_read(env) & 0xff00fdff;
        case 5: /* IPSR */
            return xpsr_read(env) & 0x000001ff;
        case 6: /* EPSR */
            return xpsr_read(env) & 0x0700fc00;
        case 7: /* IEPSR */
            return xpsr_read(env) & 0x0700edff;
        case 8: /* MSP */
            return env->v7m.process_sp ? env->v7m.other_sp : env->regs[13];
        case NON_SECURE_REG(8): /* MSP_NS */
            return env->v7m.other_ss_msp;
        case 9: /* PSP */
            return env->v7m.process_sp ? env->regs[13] : env->v7m.other_sp;
        case NON_SECURE_REG(9): /* PSP_NS */
            return env->v7m.other_ss_psp;
        case NON_SECURE_REG(10):
        case 10: /* MSPLIM - armv8-m specific */
            return env->v7m.msplim[is_secure];
        case NON_SECURE_REG(11):
        case 11: /* PSPLIM - armv8-m specific */
            return env->v7m.psplim[is_secure];
        case NON_SECURE_REG(16):
        case 16: /* PRIMASK */
            return (env->v7m.primask[is_secure] & 1) != 0;
        case NON_SECURE_REG(17):
        case 17: /* BASEPRI */
        case 18: /* BASEPRI_MAX */
            return env->v7m.basepri[is_secure];
        case NON_SECURE_REG(19):
        case 19: /* FAULTMASK */
            return env->v7m.faultmask[is_secure];
        case NON_SECURE_REG(20):
        case 20: /* CONTROL */
            return env->v7m.control[is_secure];
        case NON_SECURE_REG(24): /* SP_NS */
            return env->v7m.process_sp ? env->v7m.other_ss_psp : env->v7m.other_ss_msp;
        default:
            /* ??? For debugging only.  */
            cpu_abort(env, "Unimplemented system register read (%d)\n", reg);
            return 0;
    }
}

void HELPER(v7m_msr)(CPUState *env, uint32_t reg, uint32_t val)
{
    bool is_secure = env->secure;
    if(IS_REG_NS(reg)) {
        if(!env->secure) {
            //  Access to a register ending in _NS in Non-secure state is RAZ/WI
            return;
        }
        is_secure = false;
    }

    switch(reg) {
        case 0: /* APSR */
            if(!in_privileged_mode(env)) {
                return;
            }
            xpsr_write(env, val, 0xf8000000);
            break;
        case 1: /* IAPSR */
            if(!in_privileged_mode(env)) {
                return;
            }
            xpsr_write(env, val, 0xf8000000);
            break;
        case 2: /* EAPSR */
            if(!in_privileged_mode(env)) {
                return;
            }
            xpsr_write(env, val, 0xfe00fc00);
            break;
        case 3: /* xPSR */
            if(!in_privileged_mode(env)) {
                return;
            }
            xpsr_write(env, val, 0xfe00fc00);
            break;
        case 5: /* IPSR */
            if(!in_privileged_mode(env)) {
                return;
            }
            /* IPSR bits are readonly.  */
            break;
        case 6: /* EPSR */
            if(!in_privileged_mode(env)) {
                return;
            }
            xpsr_write(env, val, 0x0600fc00);
            break;
        case 7: /* IEPSR */
            if(!in_privileged_mode(env)) {
                return;
            }
            xpsr_write(env, val, 0x0600fc00);
            break;
        case 8: /* MSP */
            if(!in_privileged_mode(env)) {
                return;
            } else if(env->v7m.process_sp) {
                env->v7m.other_sp = val;
            } else {
                env->regs[13] = val;
            }
            break;
        case NON_SECURE_REG(8): /* MSP_NS */
            env->v7m.other_ss_msp = val;
            break;
        case 9: /* PSP */
            if(env->v7m.process_sp) {
                env->regs[13] = val;
            } else {
                env->v7m.other_sp = val;
            }
            break;
        case NON_SECURE_REG(9): /* PSP_NS */
            env->v7m.other_ss_psp = val;
            break;
        case NON_SECURE_REG(10):
        case 10: /* MSPLIM - armv8-m specific */
            env->v7m.msplim[is_secure] = val;
            break;
        case NON_SECURE_REG(11):
        case 11: /* PSPLIM - armv8-m specific */
            env->v7m.psplim[is_secure] = val;
            break;
        case NON_SECURE_REG(16):
        case 16: /* PRIMASK */
            if(!in_privileged_mode(env)) {
                return;
            } else if(val & 1) {
                env->v7m.primask[is_secure] |= PRIMASK_EN;
            } else {
                env->v7m.primask[is_secure] &= ~PRIMASK_EN;
                tlib_nvic_find_pending_irq();
            }
            break;
        case NON_SECURE_REG(17):
        case 17: /* BASEPRI */
            if(!in_privileged_mode(env)) {
                return;
            }
            env->v7m.basepri[is_secure] = val & 0xff;
            tlib_nvic_write_basepri(val & 0xff);
            break;
        case 18: /* BASEPRI_MAX */
            if(!in_privileged_mode(env)) {
                return;
            }
            val &= 0xff;
            if(val != 0 && (val < env->v7m.basepri[cpu->secure] || env->v7m.basepri[cpu->secure] == 0)) {
                env->v7m.basepri[cpu->secure] = val;
                tlib_nvic_write_basepri(val);
            }
            break;
        case NON_SECURE_REG(19):
        case 19: /* FAULTMASK */
            if(!in_privileged_mode(env)) {
                return;
            }
            env->v7m.faultmask[is_secure] = val & 1;
            break;
        case NON_SECURE_REG(20):
        case 20: /* CONTROL */
            if(!in_privileged_mode(env)) {
                return;
            }
            env->v7m.control[is_secure] = val & 3;
            //  only switch the stack if in thread mode (handler mode always uses MSP stack)
            if(env->v7m.exception == 0) {
                //  If security states don't match, we need to use other SPs
                bool other_sps = is_secure != cpu->secure;
                if(other_sps) {
                    switch_v7m_security_state(env, is_secure);
                }
                switch_v7m_sp(env, (val & 2) != 0);
                //  ... and restore them afterwards
                if(other_sps) {
                    //  negation is ok, since we for sure have switched Security State before
                    switch_v7m_security_state(env, !is_secure);
                }
            }
            break;
        case NON_SECURE_REG(24): /* SP_NS */
            if(env->v7m.process_sp) {
                env->v7m.other_ss_psp = val;
            } else {
                env->v7m.other_ss_msp = val;
            }
            break;
        default:
            /* ??? For debugging only.  */
            cpu_abort(env, "Unimplemented system register write (%d)\n", reg);
            return;
    }
}
#undef NON_SECURE_REG
#undef IS_REG_NS
#endif

/* Note that signed overflow is undefined in C.  The following routines are
   careful to use unsigned types where modulo arithmetic is required.
   Failure to do so _will_ break on newer gcc.  */

/* Signed saturating arithmetic.  */

/* Perform 16-bit signed saturating addition.  */
#define PFX_Q
#include "op_addsub.h"
#undef PFX_Q

#define PFX_UQ
#include "op_addsub.h"
#undef PFX_UQ

/* Signed modulo arithmetic.  */
#define PFX_S
#define ARITH_GE
#include "op_addsub.h"
#undef ARITH_GE
#undef PFX_S

#define PFX_U
#define ARITH_GE
#include "op_addsub.h"
#undef ARITH_GE
#undef PFX_U

#define PFX_SH
#include "op_addsub.h"
#undef PFX_SH

#define PFX_UH
#include "op_addsub.h"
#undef PFX_UH

static inline uint8_t do_usad(uint8_t a, uint8_t b)
{
    if(a > b) {
        return a - b;
    } else {
        return b - a;
    }
}

/* Unsigned sum of absolute byte differences.  */
uint32_t HELPER(usad8)(uint32_t a, uint32_t b)
{
    uint32_t sum;
    sum = do_usad(a, b);
    sum += do_usad(a >> 8, b >> 8);
    sum += do_usad(a >> 16, b >> 16);
    sum += do_usad(a >> 24, b >> 24);
    return sum;
}

/* For ARMv6 SEL instruction.  */
uint32_t HELPER(sel_flags)(uint32_t flags, uint32_t a, uint32_t b)
{
    uint32_t mask;

    mask = 0;
    if(flags & 1) {
        mask |= 0xff;
    }
    if(flags & 2) {
        mask |= 0xff00;
    }
    if(flags & 4) {
        mask |= 0xff0000;
    }
    if(flags & 8) {
        mask |= 0xff000000;
    }
    return (a & mask) | (b & ~mask);
}

uint32_t HELPER(logicq_cc)(uint64_t val)
{
    return (val >> 32) | (val != 0);
}

/* VFP support.  We follow the convention used for VFP instrunctions:
   Single precition routines have a "s" suffix, double precision a
   "d" suffix.  */

/* Convert host exception flags to vfp form.  */
static inline int vfp_exceptbits_from_host(int host_bits)
{
    int target_bits = 0;

    if(host_bits & float_flag_invalid) {
        target_bits |= 1;
    }
    if(host_bits & float_flag_divbyzero) {
        target_bits |= 2;
    }
    if(host_bits & float_flag_overflow) {
        target_bits |= 4;
    }
    if(host_bits & (float_flag_underflow | float_flag_output_denormal)) {
        target_bits |= 8;
    }
    if(host_bits & float_flag_inexact) {
        target_bits |= 0x10;
    }
    if(host_bits & float_flag_input_denormal) {
        target_bits |= 0x80;
    }
    return target_bits;
}

uint32_t HELPER(vfp_get_fpscr)(CPUState *env)
{
    int i;
    uint32_t fpscr;

    fpscr = (env->vfp.xregs[ARM_VFP_FPSCR] & 0xffc8ffff) | (env->vfp.vec_len << 16) | (env->vfp.vec_stride << 20);
    i = get_float_exception_flags(&env->vfp.fp_status);
    i |= get_float_exception_flags(&env->vfp.standard_fp_status);
    fpscr |= vfp_exceptbits_from_host(i);
    return fpscr;
}

uint32_t vfp_get_fpscr(CPUState *env)
{
    return HELPER(vfp_get_fpscr)(env);
}

#ifdef TARGET_PROTO_ARM_M
void vfp_trigger_exception()
{
    /* Number of an NVIC interrupt that should be triggered when an fpu exception occures
     * On some platforms this line is not physically connected (eg. STM32H7 - errata ES0392 Rev 8,
     * 2.1.2 Cortex-M7 FPU interrupt not present on NVIC line 81), so a negative value means
     * don't trigger the interrupt
     */
    if(unlikely(cpu->vfp.fpu_interrupt_irq_number >= 0)) {
        /* This interrupt is an external interrupt. We add 16 to offset this number
         * and allow the user to pass IRQ numbers from the board's documentation
         */
        tlib_nvic_set_pending_irq(16 + cpu->vfp.fpu_interrupt_irq_number);
    }
}
#endif

/* Convert vfp exception flags to target form.  */
static inline int vfp_exceptbits_to_host(int target_bits)
{
    int host_bits = 0;

    if(target_bits & 1) {
        host_bits |= float_flag_invalid;
    }
    if(target_bits & 2) {
        host_bits |= float_flag_divbyzero;
    }
    if(target_bits & 4) {
        host_bits |= float_flag_overflow;
    }
    if(target_bits & 8) {
        host_bits |= float_flag_underflow;
    }
    if(target_bits & 0x10) {
        host_bits |= float_flag_inexact;
    }
    if(target_bits & 0x80) {
        host_bits |= float_flag_input_denormal;
    }
    return host_bits;
}

void HELPER(vfp_set_fpscr)(CPUState *env, uint32_t val)
{
    int i;
    uint32_t changed;

    changed = env->vfp.xregs[ARM_VFP_FPSCR];
    env->vfp.xregs[ARM_VFP_FPSCR] = (val & 0xffc8ffff);
    env->vfp.vec_len = (val >> 16) & 7;
    env->vfp.vec_stride = (val >> 20) & 3;

    changed ^= val;
    if(changed & (3 << 22)) {
        i = (val >> 22) & 3;
        switch(i) {
            case 0:
                i = float_round_nearest_even;
                break;
            case 1:
                i = float_round_up;
                break;
            case 2:
                i = float_round_down;
                break;
            case 3:
                i = float_round_to_zero;
                break;
        }
        set_float_rounding_mode(i, &env->vfp.fp_status);
    }
    if(changed & (1 << 24)) {
        set_flush_to_zero((val & (1 << 24)) != 0, &env->vfp.fp_status);
        set_flush_inputs_to_zero((val & (1 << 24)) != 0, &env->vfp.fp_status);
    }
    if(changed & (1 << 25)) {
        set_default_nan_mode((val & (1 << 25)) != 0, &env->vfp.fp_status);
    }

    i = vfp_exceptbits_to_host(val);
    set_float_exception_flags(i, &env->vfp.fp_status);
    set_float_exception_flags(0, &env->vfp.standard_fp_status);
}

void vfp_set_fpscr(CPUState *env, uint32_t val)
{
    HELPER(vfp_set_fpscr)(env, val);
}

#define VFP_HELPER(name, p) HELPER(glue(glue(vfp_, name), p))

#define VFP_BINOP(name)                                            \
    float32 VFP_HELPER(name, s)(float32 a, float32 b, void *fpstp) \
    {                                                              \
        float_status *fpst = fpstp;                                \
        return float32_##name(a, b, fpst);                         \
    }                                                              \
    float64 VFP_HELPER(name, d)(float64 a, float64 b, void *fpstp) \
    {                                                              \
        float_status *fpst = fpstp;                                \
        return float64_##name(a, b, fpst);                         \
    }
VFP_BINOP(add)
VFP_BINOP(sub)
VFP_BINOP(mul)
VFP_BINOP(div)
#undef VFP_BINOP

float32 VFP_HELPER(neg, s)(float32 a)
{
    return float32_chs(a);
}

float64 VFP_HELPER(neg, d)(float64 a)
{
    return float64_chs(a);
}

float32 VFP_HELPER(abs, s)(float32 a)
{
    return float32_abs(a);
}

float64 VFP_HELPER(abs, d)(float64 a)
{
    return float64_abs(a);
}

float32 VFP_HELPER(sqrt, s)(float32 a, CPUState *env)
{
    return float32_sqrt(a, &env->vfp.fp_status);
}

float64 VFP_HELPER(sqrt, d)(float64 a, CPUState *env)
{
    return float64_sqrt(a, &env->vfp.fp_status);
}

/* XXX: check quiet/signaling case */
#define DO_VFP_cmp(p, type)                                                                           \
    void VFP_HELPER(cmp, p)(type a, type b, CPUState * env)                                           \
    {                                                                                                 \
        uint32_t flags;                                                                               \
        switch(type##_compare_quiet(a, b, &env->vfp.fp_status)) {                                     \
            case 0:                                                                                   \
                flags = 0x6;                                                                          \
                break;                                                                                \
            case -1:                                                                                  \
                flags = 0x8;                                                                          \
                break;                                                                                \
            case 1:                                                                                   \
                flags = 0x2;                                                                          \
                break;                                                                                \
            default:                                                                                  \
            case 2:                                                                                   \
                flags = 0x3;                                                                          \
                break;                                                                                \
        }                                                                                             \
        env->vfp.xregs[ARM_VFP_FPSCR] = (flags << 28) | (env->vfp.xregs[ARM_VFP_FPSCR] & 0x0fffffff); \
    }                                                                                                 \
    void VFP_HELPER(cmpe, p)(type a, type b, CPUState * env)                                          \
    {                                                                                                 \
        uint32_t flags;                                                                               \
        switch(type##_compare(a, b, &env->vfp.fp_status)) {                                           \
            case 0:                                                                                   \
                flags = 0x6;                                                                          \
                break;                                                                                \
            case -1:                                                                                  \
                flags = 0x8;                                                                          \
                break;                                                                                \
            case 1:                                                                                   \
                flags = 0x2;                                                                          \
                break;                                                                                \
            default:                                                                                  \
            case 2:                                                                                   \
                flags = 0x3;                                                                          \
                break;                                                                                \
        }                                                                                             \
        env->vfp.xregs[ARM_VFP_FPSCR] = (flags << 28) | (env->vfp.xregs[ARM_VFP_FPSCR] & 0x0fffffff); \
    }
DO_VFP_cmp(s, float32) DO_VFP_cmp(d, float64)
#undef DO_VFP_cmp

/* Integer to float and float to integer conversions */

#define CONV_ITOF(name, fsz, sign)                   \
    float##fsz HELPER(name)(uint32_t x, void *fpstp) \
    {                                                \
        float_status *fpst = fpstp;                  \
        return sign##int32_to_##float##fsz(x, fpst); \
    }

#define CONV_FTOI(name, fsz, sign, round)                     \
    uint32_t HELPER(name)(float##fsz x, void *fpstp)          \
    {                                                         \
        float_status *fpst = fpstp;                           \
        if(float##fsz##_is_any_nan(x)) {                      \
            float_raise(float_flag_invalid, fpst);            \
            return 0;                                         \
        }                                                     \
        return float##fsz##_to_##sign##int32##round(x, fpst); \
    }

#define FLOAT_CONVS(name, p, fsz, sign)     \
    CONV_ITOF(vfp_##name##to##p, fsz, sign) \
    CONV_FTOI(vfp_to##name##p, fsz, sign, ) \
    CONV_FTOI(vfp_to##name##z##p, fsz, sign, _round_to_zero)

    FLOAT_CONVS(si, s, 32, ) FLOAT_CONVS(si, d, 64, ) FLOAT_CONVS(ui, s, 32, u) FLOAT_CONVS(ui, d, 64, u)

#undef CONV_ITOF
#undef CONV_FTOI
#undef FLOAT_CONVS

    /* floating point conversion */
    float64 VFP_HELPER(fcvtd, s)(float32 x, CPUState *env)
{
    float64 r = float32_to_float64(x, &env->vfp.fp_status);
    /* ARM requires that S<->D conversion of any kind of NaN generates
     * a quiet NaN by forcing the most significant frac bit to 1.
     */
    return float64_maybe_silence_nan(r, &env->vfp.fp_status);
}

float32 VFP_HELPER(fcvts, d)(float64 x, CPUState *env)
{
    float32 r = float64_to_float32(x, &env->vfp.fp_status);
    /* ARM requires that S<->D conversion of any kind of NaN generates
     * a quiet NaN by forcing the most significant frac bit to 1.
     */
    return float32_maybe_silence_nan(r, &env->vfp.fp_status);
}

/* VFP3 fixed point conversion.  */
#define VFP_CONV_FIX(name, p, fsz, itype, sign)                                        \
    float##fsz HELPER(vfp_##name##to##p)(uint##fsz##_t x, uint32_t shift, void *fpstp) \
    {                                                                                  \
        float_status *fpst = fpstp;                                                    \
        float##fsz tmp;                                                                \
        tmp = sign##int32_to_##float##fsz((itype##_t)x, fpst);                         \
        return float##fsz##_scalbn(tmp, -(int)shift, fpst);                            \
    }                                                                                  \
    uint##fsz##_t HELPER(vfp_to##name##p)(float##fsz x, uint32_t shift, void *fpstp)   \
    {                                                                                  \
        float_status *fpst = fpstp;                                                    \
        float##fsz tmp;                                                                \
        if(float##fsz##_is_any_nan(x)) {                                               \
            float_raise(float_flag_invalid, fpst);                                     \
            return 0;                                                                  \
        }                                                                              \
        tmp = float##fsz##_scalbn(x, shift, fpst);                                     \
        return float##fsz##_to_##itype##_round_to_zero(tmp, fpst);                     \
    }

VFP_CONV_FIX(sh, d, 64, int16, )
VFP_CONV_FIX(sl, d, 64, int32, )
VFP_CONV_FIX(uh, d, 64, uint16, u)
VFP_CONV_FIX(ul, d, 64, uint32, u)
VFP_CONV_FIX(sh, s, 32, int16, )
VFP_CONV_FIX(sl, s, 32, int32, )
VFP_CONV_FIX(uh, s, 32, uint16, u)
VFP_CONV_FIX(ul, s, 32, uint32, u)
#undef VFP_CONV_FIX

/* Half precision conversions.  */
static float32 do_fcvt_f16_to_f32(uint32_t a, CPUState *env, float_status *s)
{
    int ieee = (env->vfp.xregs[ARM_VFP_FPSCR] & (1 << 26)) == 0;
    float32 r = float16_to_float32(make_float16(a), ieee, s);
    if(ieee) {
        return float32_maybe_silence_nan(r, s);
    }
    return r;
}

static uint32_t do_fcvt_f32_to_f16(float32 a, CPUState *env, float_status *s)
{
    int ieee = (env->vfp.xregs[ARM_VFP_FPSCR] & (1 << 26)) == 0;
    float16 r = float32_to_float16(a, ieee, s);
    if(ieee) {
        r = float16_maybe_silence_nan(r, s);
    }
    return float16_val(r);
}

float32 HELPER(neon_fcvt_f16_to_f32)(uint32_t a, CPUState *env)
{
    return do_fcvt_f16_to_f32(a, env, &env->vfp.standard_fp_status);
}

uint32_t HELPER(neon_fcvt_f32_to_f16)(float32 a, CPUState *env)
{
    return do_fcvt_f32_to_f16(a, env, &env->vfp.standard_fp_status);
}

float32 HELPER(vfp_fcvt_f16_to_f32)(uint32_t a, CPUState *env)
{
    return do_fcvt_f16_to_f32(a, env, &env->vfp.fp_status);
}

uint32_t HELPER(vfp_fcvt_f32_to_f16)(float32 a, CPUState *env)
{
    return do_fcvt_f32_to_f16(a, env, &env->vfp.fp_status);
}

float32 HELPER(recps_f32)(float32 a, float32 b, CPUState *env)
{
    float_status *s = &env->vfp.standard_fp_status;
    if((float32_is_infinity(a) && float32_is_zero_or_denormal(b)) || (float32_is_infinity(b) && float32_is_zero_or_denormal(a))) {
        if(!(float32_is_zero(a) || float32_is_zero(b))) {
            float_raise(float_flag_input_denormal, s);
        }
        return float32_two;
    }
    return float32_sub(float32_two, float32_mul(a, b, s), s);
}

float32 HELPER(rsqrts_f32)(float32 a, float32 b, CPUState *env)
{
    float_status *s = &env->vfp.standard_fp_status;
    float32 product;
    if((float32_is_infinity(a) && float32_is_zero_or_denormal(b)) || (float32_is_infinity(b) && float32_is_zero_or_denormal(a))) {
        if(!(float32_is_zero(a) || float32_is_zero(b))) {
            float_raise(float_flag_input_denormal, s);
        }
        return float32_one_point_five;
    }
    product = float32_mul(a, b, s);
    return float32_div(float32_sub(float32_three, product, s), float32_two, s);
}

/* NEON helpers.  */

/* Constants 256 and 512 are used in some helpers; we avoid relying on
 * int->float conversions at run-time.  */
#define float64_256 make_float64(0x4070000000000000LL)
#define float64_512 make_float64(0x4080000000000000LL)

/* The algorithm that must be used to calculate the estimate
 * is specified by the ARM ARM.
 */
static float64 recip_estimate(float64 a, CPUState *env)
{
    /* These calculations mustn't set any fp exception flags,
     * so we use a local copy of the fp_status.
     */
    float_status dummy_status = env->vfp.standard_fp_status;
    float_status *s = &dummy_status;
    /* q = (int)(a * 512.0) */
    float64 q = float64_mul(float64_512, a, s);
    int64_t q_int = float64_to_int64_round_to_zero(q, s);

    /* r = 1.0 / (((double)q + 0.5) / 512.0) */
    q = int64_to_float64(q_int, s);
    q = float64_add(q, float64_half, s);
    q = float64_div(q, float64_512, s);
    q = float64_div(float64_one, q, s);

    /* s = (int)(256.0 * r + 0.5) */
    q = float64_mul(q, float64_256, s);
    q = float64_add(q, float64_half, s);
    q_int = float64_to_int64_round_to_zero(q, s);

    /* return (double)s / 256.0 */
    return float64_div(int64_to_float64(q_int, s), float64_256, s);
}

float32 HELPER(recpe_f32)(float32 a, CPUState *env)
{
    float_status *s = &env->vfp.standard_fp_status;
    float64 f64;
    uint32_t val32 = float32_val(a);

    int result_exp;
    int a_exp = (val32 & 0x7f800000) >> 23;
    int sign = val32 & 0x80000000;

    if(float32_is_any_nan(a)) {
        if(float32_is_signaling_nan(a, s)) {
            float_raise(float_flag_invalid, s);
        }
        return float32_default_nan;
    } else if(float32_is_infinity(a)) {
        return float32_set_sign(float32_zero, float32_is_neg(a));
    } else if(float32_is_zero_or_denormal(a)) {
        if(!float32_is_zero(a)) {
            float_raise(float_flag_input_denormal, s);
        }
        float_raise(float_flag_divbyzero, s);
        return float32_set_sign(float32_infinity, float32_is_neg(a));
    } else if(a_exp >= 253) {
        float_raise(float_flag_underflow, s);
        return float32_set_sign(float32_zero, float32_is_neg(a));
    }

    f64 = make_float64((0x3feULL << 52) | ((int64_t)(val32 & 0x7fffff) << 29));

    result_exp = 253 - a_exp;

    f64 = recip_estimate(f64, env);

    val32 = sign | ((result_exp & 0xff) << 23) | ((float64_val(f64) >> 29) & 0x7fffff);
    return make_float32(val32);
}

/* The algorithm that must be used to calculate the estimate
 * is specified by the ARM ARM.
 */
static float64 recip_sqrt_estimate(float64 a, CPUState *env)
{
    /* These calculations mustn't set any fp exception flags,
     * so we use a local copy of the fp_status.
     */
    float_status dummy_status = env->vfp.standard_fp_status;
    float_status *s = &dummy_status;
    float64 q;
    int64_t q_int;

    if(float64_lt(a, float64_half, s)) {
        /* range 0.25 <= a < 0.5 */

        /* a in units of 1/512 rounded down */
        /* q0 = (int)(a * 512.0);  */
        q = float64_mul(float64_512, a, s);
        q_int = float64_to_int64_round_to_zero(q, s);

        /* reciprocal root r */
        /* r = 1.0 / sqrt(((double)q0 + 0.5) / 512.0);  */
        q = int64_to_float64(q_int, s);
        q = float64_add(q, float64_half, s);
        q = float64_div(q, float64_512, s);
        q = float64_sqrt(q, s);
        q = float64_div(float64_one, q, s);
    } else {
        /* range 0.5 <= a < 1.0 */

        /* a in units of 1/256 rounded down */
        /* q1 = (int)(a * 256.0); */
        q = float64_mul(float64_256, a, s);
        int64_t q_int = float64_to_int64_round_to_zero(q, s);

        /* reciprocal root r */
        /* r = 1.0 /sqrt(((double)q1 + 0.5) / 256); */
        q = int64_to_float64(q_int, s);
        q = float64_add(q, float64_half, s);
        q = float64_div(q, float64_256, s);
        q = float64_sqrt(q, s);
        q = float64_div(float64_one, q, s);
    }
    /* r in units of 1/256 rounded to nearest */
    /* s = (int)(256.0 * r + 0.5); */

    q = float64_mul(q, float64_256, s);
    q = float64_add(q, float64_half, s);
    q_int = float64_to_int64_round_to_zero(q, s);

    /* return (double)s / 256.0;*/
    return float64_div(int64_to_float64(q_int, s), float64_256, s);
}

float32 HELPER(rsqrte_f32)(float32 a, CPUState *env)
{
    float_status *s = &env->vfp.standard_fp_status;
    int result_exp;
    float64 f64;
    uint32_t val;
    uint64_t val64;

    val = float32_val(a);

    if(float32_is_any_nan(a)) {
        if(float32_is_signaling_nan(a, s)) {
            float_raise(float_flag_invalid, s);
        }
        return float32_default_nan;
    } else if(float32_is_zero_or_denormal(a)) {
        if(!float32_is_zero(a)) {
            float_raise(float_flag_input_denormal, s);
        }
        float_raise(float_flag_divbyzero, s);
        return float32_set_sign(float32_infinity, float32_is_neg(a));
    } else if(float32_is_neg(a)) {
        float_raise(float_flag_invalid, s);
        return float32_default_nan;
    } else if(float32_is_infinity(a)) {
        return float32_zero;
    }

    /* Normalize to a double-precision value between 0.25 and 1.0,
     * preserving the parity of the exponent.  */
    if((val & 0x800000) == 0) {
        f64 = make_float64(((uint64_t)(val & 0x80000000) << 32) | (0x3feULL << 52) | ((uint64_t)(val & 0x7fffff) << 29));
    } else {
        f64 = make_float64(((uint64_t)(val & 0x80000000) << 32) | (0x3fdULL << 52) | ((uint64_t)(val & 0x7fffff) << 29));
    }

    result_exp = (380 - ((val & 0x7f800000) >> 23)) / 2;

    f64 = recip_sqrt_estimate(f64, env);

    val64 = float64_val(f64);

    val = ((result_exp & 0xff) << 23) | ((val64 >> 29) & 0x7fffff);
    return make_float32(val);
}

uint32_t HELPER(recpe_u32)(uint32_t a, CPUState *env)
{
    float64 f64;

    if((a & 0x80000000) == 0) {
        return 0xffffffff;
    }

    f64 = make_float64((0x3feULL << 52) | ((int64_t)(a & 0x7fffffff) << 21));

    f64 = recip_estimate(f64, env);

    return 0x80000000 | ((float64_val(f64) >> 21) & 0x7fffffff);
}

uint32_t HELPER(rsqrte_u32)(uint32_t a, CPUState *env)
{
    float64 f64;

    if((a & 0xc0000000) == 0) {
        return 0xffffffff;
    }

    if(a & 0x80000000) {
        f64 = make_float64((0x3feULL << 52) | ((uint64_t)(a & 0x7fffffff) << 21));
    } else { /* bits 31-30 == '01' */
        f64 = make_float64((0x3fdULL << 52) | ((uint64_t)(a & 0x3fffffff) << 22));
    }

    f64 = recip_sqrt_estimate(f64, env);

    return 0x80000000 | ((float64_val(f64) >> 21) & 0x7fffffff);
}

/* VFPv4 fused multiply-accumulate */
float32 VFP_HELPER(muladd, s)(float32 a, float32 b, float32 c, void *fpstp)
{
    float_status *fpst = fpstp;
    return float32_muladd(a, b, c, 0, fpst);
}

float64 VFP_HELPER(muladd, d)(float64 a, float64 b, float64 c, void *fpstp)
{
    float_status *fpst = fpstp;
    return float64_muladd(a, b, c, 0, fpst);
}

void HELPER(set_teecr)(CPUState *env, uint32_t val)
{
    val &= 1;
    if(env->teecr != val) {
        env->teecr = val;
        tb_flush(env);
    }
}

#ifdef TARGET_PROTO_ARM_M
uint32_t HELPER(v8m_tt)(CPUState *env, uint32_t addr, uint32_t op)
{
    int prot;
    bool priv_access;
    uint32_t phys_ptr;      /* Not used, but needed for get_phys_addr_mpu */
    target_ulong page_size; /* Same as above */
    bool a, t;
    int resolved_region;
    bool multiple_regions;
    bool secure;

    union {
        struct {
            unsigned mpu_region : 8;
            unsigned sau_region : 8;
            unsigned mpu_region_valid : 1;
            unsigned sau_region_valid : 1;
            unsigned read_ok : 1;
            unsigned readwrite_ok : 1;
            unsigned nonsecure_read_ok : 1;
            unsigned nonsecure_readwrite_ok : 1;
            unsigned secure : 1;
            unsigned idau_region_valid : 1;
            unsigned idau_region : 8;
        } flags;
        uint32_t value;
    } addr_info = { .value = 0 };

    /* Decode instruction variant
     * TT:    a == 0 && t == 0
     * TTA:   a == 1 && t == 0
     * TTT:   a == 0 && t == 1
     * TTAT:  a == 1 && t == 1
     */
    a = op & 0b10;
    t = op & 0b01;

    /* Alternate Domain (A) variants are used to query the Non-secure MPU from Secure code */
    if(a) {
        if(!env->secure) {
            env->exception_index = EXCP_UDEF;
            cpu_loop_exit(env);
        }
        secure = false;
    } else {
        secure = env->secure;
    }

    if(!PMSA_ENABLED(env->pmsav8[secure].ctrl)) {
        goto invalid;
    }

    if(t) {
        /* Force user access */
        priv_access = false;
    } else {
        /* Check privilege level for the M profile, return true otherwise */
        priv_access = in_privileged_mode(env);
    }

    if(!pmsav8_get_region(env, addr, secure, &resolved_region, &multiple_regions) || multiple_regions) {
        /* No region hit or multiple regions */
        goto invalid;
    }
    addr_info.flags.mpu_region = resolved_region;
    addr_info.flags.mpu_region_valid = true;

    pmsav8_get_phys_addr(env, addr, secure, PAGE_READ, !priv_access, &phys_ptr, &prot, &page_size);
    addr_info.flags.read_ok = (prot & (1 << PAGE_READ)) != 0;
    addr_info.flags.readwrite_ok = addr_info.flags.read_ok && (prot & (1 << PAGE_WRITE)) != 0;

    return addr_info.value;

invalid:
    /* The Arm® v8-M Architecture Reference Manual specifies that if MREGION content is not valid if:
     * - The MPU is not implemented or MPU_CTRL.ENABLE is set to zero,
     * - The address specified by the TT instruction variant does not match any enabled MPU regions,
     * - The address matched multiple MPU regions,
     * - The TT or TTT instruction variants, without the A flag specified, were executed from an unprivileged mode. *unimpl*
     * In this case R and RW fields are RAZ
     */
    return addr_info.value;
}

void HELPER(v8m_blxns)(CPUState *env, uint32_t addr, uint32_t link)
{
    tlib_printf(LOG_LEVEL_NOISY, "B%sXNS jump at 0x%x to 0x%x", link ? "L" : "", env->regs[15], addr);

    /* Only switch to Non-Secure if bit[0] of target addr is 0 */
    if((addr & 1) == 0) {
        if(link) {
            v7m_push(env, env->regs[15] + 2);
            /* According to docs "some processor state information" is pushed here
             * the ARM pseudocode specifies exactly:
             * """
             *   EPSR.B is not preserved on the stack
             *   savedPSR.Exception = IPSR.Exception;
             *   savedPSR.SFPA = CONTROL_S.SFPA;
             * """
             * pushing entire xpsr for now, but it's an overkill */
            v7m_push(env, xpsr_read(env));
        }

        /* Now we switch stacks and jump to non-secure mode */
        switch_v7m_security_state(env, false);
        if(link) {
            env->regs[14] = FNC_RETURN;
            /* If in handler mode, we should set exception number to invalid but non-zero value
             * to prevent information leakage */
            if(env->v7m.handler_mode) {
                env->v7m.exception = ARMV7M_EXCP_RESET;
            }
        }

        if(!link) {
            /* BXNS variant uses BranchReturn pseudoinstruction, which can have different behavior here.
             * Otherwise will behave like a regular branch */
            if(addr >= 0xfeffff00) {
                /* TODO: BXNS can also be used with FNC_RETURN or EXCP_RETURN and works differently then */
                cpu_abort(env, "FNC_RETURN or EXC_RETURN is yet unsupported in BXNS");
            }
        }
    }
    env->regs[15] = addr;
}

void HELPER(v8m_sg)(CPUState *env)
{
    tlib_assert(!env->secure);
    /* Clear bit[0] of LR to indicate we will return to Non-Secure mode, if we were previously in Non-Secure state */
    env->regs[14] &= ~1;
    //  TODO: should be in Non-secure Callable SAU region
    //  TODO: bx/blx to any other instruction in Secure memory should cause a SecureFault no matter whether it is NSC or no (see
    //  below, this will happen 'automatically')
    //  TODO: once SAU is implemented then lduw_code will not be able to fetch this from Non-secure state
    switch_v7m_security_state(env, true);
    tlib_printf(LOG_LEVEL_NOISY, "Executed SG at 0x%" PRIx32, env->regs[15]);
}
#endif

void tlib_arch_dispose()
{
    ttable_remove(cpu->cp_regs);
}

void HELPER(set_system_event)(void)
{
    tlib_set_system_event(1);
}

void cpu_before_cycles_per_instruction_change(CPUState *env)
{
    pmu_recalculate_all_lazy();
}

void cpu_after_cycles_per_instruction_change(CPUState *env)
{
    pmu_take_all_snapshots();
    pmu_recalculate_cycles_instruction_limit();
}

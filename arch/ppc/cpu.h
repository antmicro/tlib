/*
 *  PowerPC emulation cpu definitions for qemu.
 *
 *  Copyright (c) 2003-2007 Jocelyn Mayer
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
#pragma once

#include <stdbool.h>
#include <strings.h>

//  #define PPC_EMULATE_32BITS_HYPV

#ifndef TARGET_LONG_BITS
#define TARGET_LONG_BITS 32
#endif

#if TARGET_LONG_BITS == 64
/* PowerPC 64 definitions */
#define TARGET_PPC64
#define TARGET_PAGE_BITS 12

/* Note that the official physical address space bits is 62-M where M
   is implementation dependent.  I've not looked up M for the set of
   cpus we emulate at the system level.  */
#define TARGET_PHYS_ADDR_SPACE_BITS 62

/* Note that the PPC environment architecture talks about 80 bit virtual
   addresses, with segmentation.  Obviously that's not all visible to a
   single process, which is all we're concerned with here.  */
#ifdef TARGET_ABI32
#define TARGET_VIRT_ADDR_SPACE_BITS 32
#else
#define TARGET_VIRT_ADDR_SPACE_BITS 64
#endif

#define TARGET_PAGE_BITS_16M 24

#elif TARGET_LONG_BITS == 32
/* PowerPC 32 definitions */
#define TARGET_LONG_BITS 32
#define TARGET_PPC32

#if defined(TARGET_PPCEMB)
/* Specific definitions for PowerPC embedded */
/* BookE have 36 bits physical address space */
/* Pages can be 1 kB small */
#define TARGET_PAGE_BITS 10
#else /* defined(TARGET_PPCEMB) */
/* "standard" PowerPC 32 definitions */
#define TARGET_PAGE_BITS 12
#endif /* defined(TARGET_PPCEMB) */

#define TARGET_PHYS_ADDR_SPACE_BITS 36
#define TARGET_VIRT_ADDR_SPACE_BITS 32

#else
#error "Target arch can be only 32-bit or 64-bit"
#endif /* TARGET_LONG_BITS */

#include "cpu-defs.h"

#include "softfloat-2.h"

#if defined(TARGET_PPC64)
#define ELF_MACHINE EM_PPC64
#else
#define ELF_MACHINE EM_PPC
#endif

#ifdef __MINGW32__
#define ffs __builtin_ffs
#endif

#define ABORT_UNSUPPORTED_FEATURE tlib_abortf("%s is unimplemented.", __func__)

/*****************************************************************************/
/* MMU model                                                                 */
typedef enum powerpc_mmu_t powerpc_mmu_t;
enum powerpc_mmu_t {
    POWERPC_MMU_UNKNOWN = 0x00000000,
    /* Standard 32 bits PowerPC MMU                            */
    POWERPC_MMU_32B = 0x00000001,
    /* PowerPC 6xx MMU with software TLB                       */
    POWERPC_MMU_SOFT_6xx = 0x00000002,
    /* PowerPC 74xx MMU with software TLB                      */
    POWERPC_MMU_SOFT_74xx = 0x00000003,
    /* PowerPC 4xx MMU with software TLB                       */
    POWERPC_MMU_SOFT_4xx = 0x00000004,
    /* PowerPC 4xx MMU with software TLB and zones protections */
    POWERPC_MMU_SOFT_4xx_Z = 0x00000005,
    /* PowerPC MMU in real mode only                           */
    POWERPC_MMU_REAL = 0x00000006,
    /* Freescale MPC8xx MMU model                              */
    POWERPC_MMU_MPC8xx = 0x00000007,
    /* BookE MMU model                                         */
    POWERPC_MMU_BOOKE = 0x00000008,
    /* BookE 2.06 MMU model                                    */
    POWERPC_MMU_BOOKE206 = 0x00000009,
    /* PowerPC 601 MMU model (specific BATs format)            */
    POWERPC_MMU_601 = 0x0000000A,
#if defined(TARGET_PPC64)
#define POWERPC_MMU_64    0x00010000
#define POWERPC_MMU_1TSEG 0x00020000
    /* 64 bits PowerPC MMU                                     */
    POWERPC_MMU_64B = POWERPC_MMU_64 | 0x00000001,
    /* 620 variant (no segment exceptions)                     */
    POWERPC_MMU_620 = POWERPC_MMU_64 | 0x00000002,
    /* Architecture 2.06 variant                               */
    POWERPC_MMU_2_06 = POWERPC_MMU_64 | POWERPC_MMU_1TSEG | 0x00000003,
#endif /* defined(TARGET_PPC64) */
};

/*****************************************************************************/
/* Exception model                                                           */
typedef enum powerpc_excp_t powerpc_excp_t;
enum powerpc_excp_t {
    POWERPC_EXCP_UNKNOWN = 0,
    /* Standard PowerPC exception model */
    POWERPC_EXCP_STD,
    /* PowerPC 40x exception model      */
    POWERPC_EXCP_40x,
    /* PowerPC 601 exception model      */
    POWERPC_EXCP_601,
    /* PowerPC 602 exception model      */
    POWERPC_EXCP_602,
    /* PowerPC 603 exception model      */
    POWERPC_EXCP_603,
    /* PowerPC 603e exception model     */
    POWERPC_EXCP_603E,
    /* PowerPC G2 exception model       */
    POWERPC_EXCP_G2,
    /* PowerPC 604 exception model      */
    POWERPC_EXCP_604,
    /* PowerPC 7x0 exception model      */
    POWERPC_EXCP_7x0,
    /* PowerPC 7x5 exception model      */
    POWERPC_EXCP_7x5,
    /* PowerPC 74xx exception model     */
    POWERPC_EXCP_74xx,
    /* BookE exception model            */
    POWERPC_EXCP_BOOKE,
#if defined(TARGET_PPC64)
    /* PowerPC 970 exception model      */
    POWERPC_EXCP_970,
    /* POWER7 exception model           */
    POWERPC_EXCP_POWER7,
#endif /* defined(TARGET_PPC64) */
};

/*****************************************************************************/
/* Exception vectors definitions                                             */
enum {
    POWERPC_EXCP_NONE = -1,
    /* The 64 first entries are used by the PowerPC embedded specification   */
    POWERPC_EXCP_CRITICAL = 0, /* Critical input                            */
    POWERPC_EXCP_MCHECK = 1,   /* Machine check exception                   */
    POWERPC_EXCP_DSI = 2,      /* Data storage exception                    */
    POWERPC_EXCP_ISI = 3,      /* Instruction storage exception             */
    POWERPC_EXCP_EXTERNAL = 4, /* External input                            */
    POWERPC_EXCP_ALIGN = 5,    /* Alignment exception                       */
    POWERPC_EXCP_PROGRAM = 6,  /* Program exception                         */
    POWERPC_EXCP_FPU = 7,      /* Floating-point unavailable exception      */
    POWERPC_EXCP_SYSCALL = 8,  /* System call exception                     */
    POWERPC_EXCP_APU = 9,      /* Auxiliary processor unavailable           */
    POWERPC_EXCP_DECR = 10,    /* Decrementer exception                     */
    POWERPC_EXCP_FIT = 11,     /* Fixed-interval timer interrupt            */
    POWERPC_EXCP_WDT = 12,     /* Watchdog timer interrupt                  */
    POWERPC_EXCP_DTLB = 13,    /* Data TLB miss                             */
    POWERPC_EXCP_ITLB = 14,    /* Instruction TLB miss                      */
    POWERPC_EXCP_DEBUG = 15,   /* Debug interrupt                           */
    /* Vectors 16 to 31 are reserved                                         */
    POWERPC_EXCP_SPEU = 32,   /* SPE/embedded floating-point unavailable   */
    POWERPC_EXCP_EFPDI = 33,  /* Embedded floating-point data interrupt    */
    POWERPC_EXCP_EFPRI = 34,  /* Embedded floating-point round interrupt   */
    POWERPC_EXCP_EPERFM = 35, /* Embedded performance monitor interrupt    */
    POWERPC_EXCP_DOORI = 36,  /* Embedded doorbell interrupt               */
    POWERPC_EXCP_DOORCI = 37, /* Embedded doorbell critical interrupt      */
    /* Vectors 38 to 63 are reserved                                         */
    /* Exceptions defined in the PowerPC server specification                */
    POWERPC_EXCP_RESET = 64, /* System reset exception                    */
    POWERPC_EXCP_DSEG = 65,  /* Data segment exception                    */
    POWERPC_EXCP_ISEG = 66,  /* Instruction segment exception             */
    POWERPC_EXCP_HDECR = 67, /* Hypervisor decrementer exception          */
    POWERPC_EXCP_TRACE = 68, /* Trace exception                           */
    POWERPC_EXCP_HDSI = 69,  /* Hypervisor data storage exception         */
    POWERPC_EXCP_HISI = 70,  /* Hypervisor instruction storage exception  */
    POWERPC_EXCP_HDSEG = 71, /* Hypervisor data segment exception         */
    POWERPC_EXCP_HISEG = 72, /* Hypervisor instruction segment exception  */
    POWERPC_EXCP_VPU = 73,   /* Vector unavailable exception              */
    /* 40x specific exceptions                                               */
    POWERPC_EXCP_PIT = 74, /* Programmable interval timer interrupt     */
    /* 601 specific exceptions                                               */
    POWERPC_EXCP_IO = 75,   /* IO error exception                        */
    POWERPC_EXCP_RUNM = 76, /* Run mode exception                        */
    /* 602 specific exceptions                                               */
    POWERPC_EXCP_EMUL = 77, /* Emulation trap exception                  */
    /* 602/603 specific exceptions                                           */
    POWERPC_EXCP_IFTLB = 78, /* Instruction fetch TLB miss                */
    POWERPC_EXCP_DLTLB = 79, /* Data load TLB miss                        */
    POWERPC_EXCP_DSTLB = 80, /* Data store TLB miss                       */
    /* Exceptions available on most PowerPC                                  */
    POWERPC_EXCP_FPA = 81,   /* Floating-point assist exception           */
    POWERPC_EXCP_DABR = 82,  /* Data address breakpoint                   */
    POWERPC_EXCP_IABR = 83,  /* Instruction address breakpoint            */
    POWERPC_EXCP_SMI = 84,   /* System management interrupt               */
    POWERPC_EXCP_PERFM = 85, /* Embedded performance monitor interrupt    */
    /* 7xx/74xx specific exceptions                                          */
    POWERPC_EXCP_THERM = 86, /* Thermal interrupt                         */
    /* 74xx specific exceptions                                              */
    POWERPC_EXCP_VPUA = 87, /* Vector assist exception                   */
    /* 970FX specific exceptions                                             */
    POWERPC_EXCP_SOFTP = 88, /* Soft patch exception                      */
    POWERPC_EXCP_MAINT = 89, /* Maintenance exception                     */
    /* Freescale embedded cores specific exceptions                          */
    POWERPC_EXCP_MEXTBR = 90,  /* Maskable external breakpoint              */
    POWERPC_EXCP_NMEXTBR = 91, /* Non maskable external breakpoint          */
    POWERPC_EXCP_ITLBE = 92,   /* Instruction TLB error                     */
    POWERPC_EXCP_DTLBE = 93,   /* Data TLB error                            */
    /* EOL                                                                   */
    POWERPC_EXCP_NB = 96,
    /* Qemu exceptions: used internally during code translation              */
    POWERPC_EXCP_STOP = 0x200,   /* stop translation                   */
    POWERPC_EXCP_BRANCH = 0x201, /* branch instruction                 */
    /* Qemu exceptions: special cases we want to stop translation            */
    POWERPC_EXCP_SYNC = 0x202,         /* context synchronizing instruction  */
    POWERPC_EXCP_SYSCALL_USER = 0x203, /* System call in user mode only      */
    POWERPC_EXCP_STCX = 0x204          /* Conditional stores in user mode     */
};

/* Exceptions error codes                                                    */
enum {
    /* Exception subtypes for POWERPC_EXCP_ALIGN                             */
    POWERPC_EXCP_ALIGN_FP = 0x01,    /* FP alignment exception            */
    POWERPC_EXCP_ALIGN_LST = 0x02,   /* Unaligned mult/extern load/store  */
    POWERPC_EXCP_ALIGN_LE = 0x03,    /* Multiple little-endian access     */
    POWERPC_EXCP_ALIGN_PROT = 0x04,  /* Access cross protection boundary  */
    POWERPC_EXCP_ALIGN_BAT = 0x05,   /* Access cross a BAT/seg boundary   */
    POWERPC_EXCP_ALIGN_CACHE = 0x06, /* Impossible dcbz access            */
    /* Exception subtypes for POWERPC_EXCP_PROGRAM                           */
    /* FP exceptions                                                         */
    POWERPC_EXCP_FP = 0x10,
    POWERPC_EXCP_FP_OX = 0x01,     /* FP overflow                       */
    POWERPC_EXCP_FP_UX = 0x02,     /* FP underflow                      */
    POWERPC_EXCP_FP_ZX = 0x03,     /* FP divide by zero                 */
    POWERPC_EXCP_FP_XX = 0x04,     /* FP inexact                        */
    POWERPC_EXCP_FP_VXSNAN = 0x05, /* FP invalid SNaN op                */
    POWERPC_EXCP_FP_VXISI = 0x06,  /* FP invalid infinite subtraction   */
    POWERPC_EXCP_FP_VXIDI = 0x07,  /* FP invalid infinite divide        */
    POWERPC_EXCP_FP_VXZDZ = 0x08,  /* FP invalid zero divide            */
    POWERPC_EXCP_FP_VXIMZ = 0x09,  /* FP invalid infinite * zero        */
    POWERPC_EXCP_FP_VXVC = 0x0A,   /* FP invalid compare                */
    POWERPC_EXCP_FP_VXSOFT = 0x0B, /* FP invalid operation              */
    POWERPC_EXCP_FP_VXSQRT = 0x0C, /* FP invalid square root            */
    POWERPC_EXCP_FP_VXCVI = 0x0D,  /* FP invalid integer conversion     */
    /* Invalid instruction                                                   */
    POWERPC_EXCP_INVAL = 0x20,
    POWERPC_EXCP_INVAL_INVAL = 0x01, /* Invalid instruction               */
    POWERPC_EXCP_INVAL_LSWX = 0x02,  /* Invalid lswx instruction          */
    POWERPC_EXCP_INVAL_SPR = 0x03,   /* Invalid SPR access                */
    POWERPC_EXCP_INVAL_FP = 0x04,    /* Unimplemented mandatory fp instr  */
    /* Privileged instruction                                                */
    POWERPC_EXCP_PRIV = 0x30,
    POWERPC_EXCP_PRIV_OPC = 0x01, /* Privileged operation exception    */
    POWERPC_EXCP_PRIV_REG = 0x02, /* Privileged register exception     */
    /* Trap                                                                  */
    POWERPC_EXCP_TRAP = 0x40,
};

/*****************************************************************************/
/* Input pins model                                                          */
typedef enum powerpc_input_t powerpc_input_t;
enum powerpc_input_t {
    PPC_FLAGS_INPUT_UNKNOWN = 0,
    /* PowerPC 6xx bus                  */
    PPC_FLAGS_INPUT_6xx,
    /* BookE bus                        */
    PPC_FLAGS_INPUT_BookE,
    /* PowerPC 405 bus                  */
    PPC_FLAGS_INPUT_405,
    /* PowerPC 970 bus                  */
    PPC_FLAGS_INPUT_970,
    /* PowerPC POWER7 bus               */
    PPC_FLAGS_INPUT_POWER7,
    /* PowerPC 401 bus                  */
    PPC_FLAGS_INPUT_401,
    /* Freescale RCPU bus               */
    PPC_FLAGS_INPUT_RCPU,
};

#define PPC_INPUT(env) (env->bus_model)

/*****************************************************************************/
typedef struct ppc_def_t ppc_def_t;
typedef struct opc_handler_t opc_handler_t;

/*****************************************************************************/
/* Types used to describe some PowerPC registers */
typedef struct CPUState CPUState;
typedef struct ppc_tb_t ppc_tb_t;
typedef struct ppc_spr_t ppc_spr_t;
typedef struct ppc_dcr_t ppc_dcr_t;
typedef union ppc_avr_t ppc_avr_t;
typedef union ppc_tlb_t ppc_tlb_t;

/* SPR access micro-ops generations callbacks */
struct ppc_spr_t {
    void (*uea_read)(void *opaque, int gpr_num, int spr_num);
    void (*uea_write)(void *opaque, int spr_num, int gpr_num);
    void (*oea_read)(void *opaque, int gpr_num, int spr_num);
    void (*oea_write)(void *opaque, int spr_num, int gpr_num);
    void (*hea_read)(void *opaque, int gpr_num, int spr_num);
    void (*hea_write)(void *opaque, int spr_num, int gpr_num);
    const char *name;
};

/* Altivec registers (128 bits) */
union ppc_avr_t {
    float32 f[4];
    uint8_t u8[16];
    uint16_t u16[8];
    uint32_t u32[4];
    int8_t s8[16];
    int16_t s16[8];
    int32_t s32[4];
    uint64_t u64[2];
};

/* Software TLB cache */
typedef struct ppc6xx_tlb_t ppc6xx_tlb_t;
struct ppc6xx_tlb_t {
    target_ulong pte0;
    target_ulong pte1;
    target_ulong EPN;
};

typedef struct ppcemb_tlb_t ppcemb_tlb_t;
struct ppcemb_tlb_t {
    target_phys_addr_t RPN;
    target_ulong EPN;
    target_ulong PID;
    target_ulong size;
    uint32_t prot;
    uint32_t attr; /* Storage attributes */
};

typedef struct ppcmas_tlb_t {
    uint32_t mas8;
    uint32_t mas1;
    uint64_t mas2;
    uint64_t mas7_3;
} ppcmas_tlb_t;

union ppc_tlb_t {
    ppc6xx_tlb_t *tlb6;
    ppcemb_tlb_t *tlbe;
    ppcmas_tlb_t *tlbm;
};

/* possible TLB variants */
#define TLB_NONE 0
#define TLB_6XX  1
#define TLB_EMB  2
#define TLB_MAS  3

#define SDR_32_HTABORG  0xFFFF0000UL
#define SDR_32_HTABMASK 0x000001FFUL

#if defined(TARGET_PPC64)
#define SDR_64_HTABORG  0xFFFFFFFFFFFC0000ULL
#define SDR_64_HTABSIZE 0x000000000000001FULL
#endif /* defined(TARGET_PPC64 */

#define HASH_PTE_SIZE_32 8
#define HASH_PTE_SIZE_64 16

typedef struct ppc_slb_t ppc_slb_t;
struct ppc_slb_t {
    uint64_t esid;
    uint64_t vsid;
};

/* Bits in the SLB ESID word */
#define SLB_ESID_ESID 0xFFFFFFFFF0000000ULL
#define SLB_ESID_V    0x0000000008000000ULL /* valid */

/* Bits in the SLB VSID word */
#define SLB_VSID_SHIFT       12
#define SLB_VSID_SHIFT_1T    24
#define SLB_VSID_SSIZE_SHIFT 62
#define SLB_VSID_B           0xc000000000000000ULL
#define SLB_VSID_B_256M      0x0000000000000000ULL
#define SLB_VSID_B_1T        0x4000000000000000ULL
#define SLB_VSID_VSID        0x3FFFFFFFFFFFF000ULL
#define SLB_VSID_PTEM        (SLB_VSID_B | SLB_VSID_VSID)
#define SLB_VSID_KS          0x0000000000000800ULL
#define SLB_VSID_KP          0x0000000000000400ULL
#define SLB_VSID_N           0x0000000000000200ULL /* no-execute */
#define SLB_VSID_L           0x0000000000000100ULL
#define SLB_VSID_C           0x0000000000000080ULL /* class */
#define SLB_VSID_LP          0x0000000000000030ULL
#define SLB_VSID_ATTR        0x0000000000000FFFULL

#define SEGMENT_SHIFT_256M 28
#define SEGMENT_MASK_256M  (~((1ULL << SEGMENT_SHIFT_256M) - 1))

#define SEGMENT_SHIFT_1T 40
#define SEGMENT_MASK_1T  (~((1ULL << SEGMENT_SHIFT_1T) - 1))

/*****************************************************************************/
/* Machine state register bits definition                                    */
#define MSR_SF   63 /* Sixty-four-bit mode                            hflags */
#define MSR_TAG  62 /* Tag-active mode (POWERx ?)                            */
#define MSR_ISF  61 /* Sixty-four-bit interrupt mode on 630                  */
#define MSR_SHV  60 /* hypervisor state                               hflags */
#define MSR_CM   31 /* Computation mode for BookE                     hflags */
#define MSR_ICM  30 /* Interrupt computation mode for BookE                  */
#define MSR_THV  29 /* hypervisor state for 32 bits PowerPC           hflags */
#define MSR_GS   28 /* guest state for BookE                                 */
#define MSR_UCLE 26 /* User-mode cache lock enable for BookE                 */
#define MSR_VR   25 /* altivec available                            x hflags */
#define MSR_SPE  25 /* SPE enable for BookE                         x hflags */
#define MSR_AP   23 /* Access privilege state on 602                  hflags */
#define MSR_SA   22 /* Supervisor access mode on 602                  hflags */
#define MSR_KEY  19 /* key bit on 603e                                       */
#define MSR_POW  18 /* Power management                                      */
#define MSR_TGPR 17 /* TGPR usage on 602/603                        x        */
#define MSR_CE   17 /* Critical interrupt enable on embedded PowerPC x       */
#define MSR_ILE  16 /* Interrupt little-endian mode                          */
#define MSR_EE   15 /* External interrupt enable                             */
#define MSR_PR   14 /* Problem state                                  hflags */
#define MSR_FP   13 /* Floating point available                       hflags */
#define MSR_ME   12 /* Machine check interrupt enable                        */
#define MSR_FE0  11 /* Floating point exception mode 0                hflags */
#define MSR_SE   10 /* Single-step trace enable                     x hflags */
#define MSR_DWE  10 /* Debug wait enable on 405                     x        */
#define MSR_UBLE 10 /* User BTB lock enable on e500                 x        */
#define MSR_BE   9  /* Branch trace enable                          x hflags */
#define MSR_DE   9  /* Debug interrupts enable on embedded PowerPC  x        */
#define MSR_FE1  8  /* Floating point exception mode 1                hflags */
#define MSR_AL   7  /* AL bit on POWER                                       */
#define MSR_EP   6  /* Exception prefix on 601                               */
#define MSR_IR   5  /* Instruction relocate                                  */
#define MSR_DR   4  /* Data relocate                                         */
#define MSR_PE   3  /* Protection enable on 403                              */
#define MSR_PX   2  /* Protection exclusive on 403                  x        */
#define MSR_PMM  2  /* Performance monitor mark on POWER            x        */
#define MSR_RI   1  /* Recoverable interrupt                        1        */
#define MSR_LE   0  /* Little-endian mode                           1 hflags */

#define msr_sf   ((env->msr >> MSR_SF) & 1)
#define msr_isf  ((env->msr >> MSR_ISF) & 1)
#define msr_shv  ((env->msr >> MSR_SHV) & 1)
#define msr_cm   ((env->msr >> MSR_CM) & 1)
#define msr_icm  ((env->msr >> MSR_ICM) & 1)
#define msr_thv  ((env->msr >> MSR_THV) & 1)
#define msr_gs   ((env->msr >> MSR_GS) & 1)
#define msr_ucle ((env->msr >> MSR_UCLE) & 1)
#define msr_vr   ((env->msr >> MSR_VR) & 1)
#define msr_spe  ((env->msr >> MSR_SPE) & 1)
#define msr_ap   ((env->msr >> MSR_AP) & 1)
#define msr_sa   ((env->msr >> MSR_SA) & 1)
#define msr_key  ((env->msr >> MSR_KEY) & 1)
#define msr_pow  ((env->msr >> MSR_POW) & 1)
#define msr_tgpr ((env->msr >> MSR_TGPR) & 1)
#define msr_ce   ((env->msr >> MSR_CE) & 1)
#define msr_ile  ((env->msr >> MSR_ILE) & 1)
#define msr_ee   ((env->msr >> MSR_EE) & 1)
#define msr_pr   ((env->msr >> MSR_PR) & 1)
#define msr_fp   ((env->msr >> MSR_FP) & 1)
#define msr_me   ((env->msr >> MSR_ME) & 1)
#define msr_fe0  ((env->msr >> MSR_FE0) & 1)
#define msr_se   ((env->msr >> MSR_SE) & 1)
#define msr_dwe  ((env->msr >> MSR_DWE) & 1)
#define msr_uble ((env->msr >> MSR_UBLE) & 1)
#define msr_be   ((env->msr >> MSR_BE) & 1)
#define msr_de   ((env->msr >> MSR_DE) & 1)
#define msr_fe1  ((env->msr >> MSR_FE1) & 1)
#define msr_al   ((env->msr >> MSR_AL) & 1)
#define msr_ep   ((env->msr >> MSR_EP) & 1)
#define msr_ir   ((env->msr >> MSR_IR) & 1)
#define msr_dr   ((env->msr >> MSR_DR) & 1)
#define msr_pe   ((env->msr >> MSR_PE) & 1)
#define msr_px   ((env->msr >> MSR_PX) & 1)
#define msr_pmm  ((env->msr >> MSR_PMM) & 1)
#define msr_ri   ((env->msr >> MSR_RI) & 1)
#define msr_le   ((env->msr >> MSR_LE) & 1)
/* Hypervisor bit is more specific */
#if defined(TARGET_PPC64)
#define MSR_HVB (1ULL << MSR_SHV)
#define msr_hv  msr_shv
#else
#if defined(PPC_EMULATE_32BITS_HYPV)
#define MSR_HVB (1ULL << MSR_THV)
#define msr_hv  msr_thv
#else
#define MSR_HVB (0ULL)
#define msr_hv  (0)
#endif
#endif

/* Local Partitioning Control Register (LPCR) Definitions */
/* TODO: LPCR register is 84bit wide, current implementation uses 32bit */
#define LPCR_HR   (1 << (63 - 43)) /* Host Radix Translation Enable        */
#define LPCR_UPRT (1 << (63 - 41)) /* Use Process Table                    */
#define LPCR_LD   (1 << (63 - 46)) /* Large Decrementer (32 or 64 bit)     */

/* Exception state register bits definition                                  */
#define ESR_PIL   (1 << (63 - 36)) /* Illegal Instruction                    */
#define ESR_PPR   (1 << (63 - 37)) /* Privileged Instruction                 */
#define ESR_PTR   (1 << (63 - 38)) /* Trap                                   */
#define ESR_FP    (1 << (63 - 39)) /* Floating-Point Operation               */
#define ESR_ST    (1 << (63 - 40)) /* Store Operation                        */
#define ESR_AP    (1 << (63 - 44)) /* Auxiliary Processor Operation          */
#define ESR_PUO   (1 << (63 - 45)) /* Unimplemented Operation                */
#define ESR_BO    (1 << (63 - 46)) /* Byte Ordering                          */
#define ESR_PIE   (1 << (63 - 47)) /* Imprecise exception                    */
#define ESR_DATA  (1 << (63 - 53)) /* Data Access (Embedded page table)      */
#define ESR_TLBI  (1 << (63 - 54)) /* TLB Ineligible (Embedded page table)   */
#define ESR_PT    (1 << (63 - 55)) /* Page Table (Embedded page table)       */
#define ESR_SPV   (1 << (63 - 56)) /* SPE/VMX operation                      */
#define ESR_EPID  (1 << (63 - 57)) /* External Process ID operation          */
#define ESR_VLEMI (1 << (63 - 58)) /* VLE operation                          */
#define ESR_MIF   (1 << (63 - 62)) /* Misaligned instruction (VLE)           */

enum {
    POWERPC_FLAG_NONE = 0x00000000,
    /* Flag for MSR bit 25 signification (VRE/SPE)                           */
    POWERPC_FLAG_SPE = 0x00000001,
    POWERPC_FLAG_VRE = 0x00000002,
    /* Flag for MSR bit 17 signification (TGPR/CE)                           */
    POWERPC_FLAG_TGPR = 0x00000004,
    POWERPC_FLAG_CE = 0x00000008,
    /* Flag for MSR bit 10 signification (SE/DWE/UBLE)                       */
    POWERPC_FLAG_SE = 0x00000010,
    POWERPC_FLAG_DWE = 0x00000020,
    POWERPC_FLAG_UBLE = 0x00000040,
    /* Flag for MSR bit 9 signification (BE/DE)                              */
    POWERPC_FLAG_BE = 0x00000080,
    POWERPC_FLAG_DE = 0x00000100,
    /* Flag for MSR bit 2 signification (PX/PMM)                             */
    POWERPC_FLAG_PX = 0x00000200,
    POWERPC_FLAG_PMM = 0x00000400,
    /* Flag for special features                                             */
    /* Decrementer clock: RTC clock (POWER, 601) or bus clock                */
    POWERPC_FLAG_RTC_CLK = 0x00010000,
    POWERPC_FLAG_BUS_CLK = 0x00020000,
    /* Has CFAR                                                              */
    POWERPC_FLAG_CFAR = 0x00040000,
};

/*****************************************************************************/
/* Floating point status and control register                                */
#define FPSCR_FX     31 /* Floating-point exception summary                  */
#define FPSCR_FEX    30 /* Floating-point enabled exception summary          */
#define FPSCR_VX     29 /* Floating-point invalid operation exception summ.  */
#define FPSCR_OX     28 /* Floating-point overflow exception                 */
#define FPSCR_UX     27 /* Floating-point underflow exception                */
#define FPSCR_ZX     26 /* Floating-point zero divide exception              */
#define FPSCR_XX     25 /* Floating-point inexact exception                  */
#define FPSCR_VXSNAN 24 /* Floating-point invalid operation exception (sNan) */
#define FPSCR_VXISI  23 /* Floating-point invalid operation exception (inf)  */
#define FPSCR_VXIDI  22 /* Floating-point invalid operation exception (inf)  */
#define FPSCR_VXZDZ  21 /* Floating-point invalid operation exception (zero) */
#define FPSCR_VXIMZ  20 /* Floating-point invalid operation exception (inf)  */
#define FPSCR_VXVC   19 /* Floating-point invalid operation exception (comp) */
#define FPSCR_FR     18 /* Floating-point fraction rounded                   */
#define FPSCR_FI     17 /* Floating-point fraction inexact                   */
#define FPSCR_C      16 /* Floating-point result class descriptor            */
#define FPSCR_FL     15 /* Floating-point less than or negative              */
#define FPSCR_FG     14 /* Floating-point greater than or negative           */
#define FPSCR_FE     13 /* Floating-point equal or zero                      */
#define FPSCR_FU     12 /* Floating-point unordered or NaN                   */
#define FPSCR_FPCC   12 /* Floating-point condition code                     */
#define FPSCR_FPRF   12 /* Floating-point result flags                       */
#define FPSCR_VXSOFT 10 /* Floating-point invalid operation exception (soft) */
#define FPSCR_VXSQRT 9  /* Floating-point invalid operation exception (sqrt) */
#define FPSCR_VXCVI  8  /* Floating-point invalid operation exception (int)  */
#define FPSCR_VE     7  /* Floating-point invalid operation exception enable */
#define FPSCR_OE     6  /* Floating-point overflow exception enable          */
#define FPSCR_UE     5  /* Floating-point undeflow exception enable          */
#define FPSCR_ZE     4  /* Floating-point zero divide exception enable       */
#define FPSCR_XE     3  /* Floating-point inexact exception enable           */
#define FPSCR_NI     2  /* Floating-point non-IEEE mode                      */
#define FPSCR_RN1    1
#define FPSCR_RN     0 /* Floating-point rounding control                   */
#define fpscr_fex    (((env->fpscr) >> FPSCR_FEX) & 0x1)
#define fpscr_vx     (((env->fpscr) >> FPSCR_VX) & 0x1)
#define fpscr_ox     (((env->fpscr) >> FPSCR_OX) & 0x1)
#define fpscr_ux     (((env->fpscr) >> FPSCR_UX) & 0x1)
#define fpscr_zx     (((env->fpscr) >> FPSCR_ZX) & 0x1)
#define fpscr_xx     (((env->fpscr) >> FPSCR_XX) & 0x1)
#define fpscr_vxsnan (((env->fpscr) >> FPSCR_VXSNAN) & 0x1)
#define fpscr_vxisi  (((env->fpscr) >> FPSCR_VXISI) & 0x1)
#define fpscr_vxidi  (((env->fpscr) >> FPSCR_VXIDI) & 0x1)
#define fpscr_vxzdz  (((env->fpscr) >> FPSCR_VXZDZ) & 0x1)
#define fpscr_vximz  (((env->fpscr) >> FPSCR_VXIMZ) & 0x1)
#define fpscr_vxvc   (((env->fpscr) >> FPSCR_VXVC) & 0x1)
#define fpscr_fpcc   (((env->fpscr) >> FPSCR_FPCC) & 0xF)
#define fpscr_vxsoft (((env->fpscr) >> FPSCR_VXSOFT) & 0x1)
#define fpscr_vxsqrt (((env->fpscr) >> FPSCR_VXSQRT) & 0x1)
#define fpscr_vxcvi  (((env->fpscr) >> FPSCR_VXCVI) & 0x1)
#define fpscr_ve     (((env->fpscr) >> FPSCR_VE) & 0x1)
#define fpscr_oe     (((env->fpscr) >> FPSCR_OE) & 0x1)
#define fpscr_ue     (((env->fpscr) >> FPSCR_UE) & 0x1)
#define fpscr_ze     (((env->fpscr) >> FPSCR_ZE) & 0x1)
#define fpscr_xe     (((env->fpscr) >> FPSCR_XE) & 0x1)
#define fpscr_ni     (((env->fpscr) >> FPSCR_NI) & 0x1)
#define fpscr_rn     (((env->fpscr) >> FPSCR_RN) & 0x3)
/* Invalid operation exception summary */
#define fpscr_ix                                                                                                               \
    ((env->fpscr) & ((1 << FPSCR_VXSNAN) | (1 << FPSCR_VXISI) | (1 << FPSCR_VXIDI) | (1 << FPSCR_VXZDZ) | (1 << FPSCR_VXIMZ) | \
                     (1 << FPSCR_VXVC) | (1 << FPSCR_VXSOFT) | (1 << FPSCR_VXSQRT) | (1 << FPSCR_VXCVI)))
/* exception summary */
#define fpscr_ex (((env->fpscr) >> FPSCR_XX) & 0x1F)
/* enabled exception summary */
#define fpscr_eex (((env->fpscr) >> FPSCR_XX) & ((env->fpscr) >> FPSCR_XE) & 0x1F)

/*****************************************************************************/
/* Vector status and control register */
#define VSCR_NJ  16 /* Vector non-java */
#define VSCR_SAT 0  /* Vector saturation */
#define vscr_nj  (((env->vscr) >> VSCR_NJ) & 0x1)
#define vscr_sat (((env->vscr) >> VSCR_SAT) & 0x1)

/*****************************************************************************/
/* BookE e500 MMU registers */

#define MAS0_NV_SHIFT 0
#define MAS0_NV_MASK  (0xfff << MAS0_NV_SHIFT)

#define MAS0_WQ_SHIFT 12
#define MAS0_WQ_MASK  (3 << MAS0_WQ_SHIFT)
/* Write TLB entry regardless of reservation */
#define MAS0_WQ_ALWAYS (0 << MAS0_WQ_SHIFT)
/* Write TLB entry only already in use */
#define MAS0_WQ_COND (1 << MAS0_WQ_SHIFT)
/* Clear TLB entry */
#define MAS0_WQ_CLR_RSRV (2 << MAS0_WQ_SHIFT)

#define MAS0_HES_SHIFT 14
#define MAS0_HES       (1 << MAS0_HES_SHIFT)

#define MAS0_ESEL_SHIFT 16
#define MAS0_ESEL_MASK  (0xfff << MAS0_ESEL_SHIFT)

#define MAS0_TLBSEL_SHIFT 28
#define MAS0_TLBSEL_MASK  (3 << MAS0_TLBSEL_SHIFT)
#define MAS0_TLBSEL_TLB0  (0 << MAS0_TLBSEL_SHIFT)
#define MAS0_TLBSEL_TLB1  (1 << MAS0_TLBSEL_SHIFT)
#define MAS0_TLBSEL_TLB2  (2 << MAS0_TLBSEL_SHIFT)
#define MAS0_TLBSEL_TLB3  (3 << MAS0_TLBSEL_SHIFT)

#define MAS0_ATSEL_SHIFT 31
#define MAS0_ATSEL       (1 << MAS0_ATSEL_SHIFT)
#define MAS0_ATSEL_TLB   0
#define MAS0_ATSEL_LRAT  MAS0_ATSEL

#define MAS1_TSIZE_SHIFT 7
#define MAS1_TSIZE_MASK  (0x1f << MAS1_TSIZE_SHIFT)

#define MAS1_TS_SHIFT 12
#define MAS1_TS       (1 << MAS1_TS_SHIFT)

#define MAS1_IND_SHIFT 13
#define MAS1_IND       (1 << MAS1_IND_SHIFT)

#define MAS1_TID_SHIFT 16
#define MAS1_TID_MASK  (0x3fff << MAS1_TID_SHIFT)

#define MAS1_IPROT_SHIFT 30
#define MAS1_IPROT       (1 << MAS1_IPROT_SHIFT)

#define MAS1_VALID_SHIFT 31
#define MAS1_VALID       0x80000000

#define MAS2_EPN_SHIFT 12
#define MAS2_EPN_MASK  (0xfffff << MAS2_EPN_SHIFT)

#define MAS2_ACM_SHIFT 6
#define MAS2_ACM       (1 << MAS2_ACM_SHIFT)

#define MAS2_VLE_SHIFT 5
#define MAS2_VLE       (1 << MAS2_VLE_SHIFT)

#define MAS2_W_SHIFT 4
#define MAS2_W       (1 << MAS2_W_SHIFT)

#define MAS2_I_SHIFT 3
#define MAS2_I       (1 << MAS2_I_SHIFT)

#define MAS2_M_SHIFT 2
#define MAS2_M       (1 << MAS2_M_SHIFT)

#define MAS2_G_SHIFT 1
#define MAS2_G       (1 << MAS2_G_SHIFT)

#define MAS2_E_SHIFT 0
#define MAS2_E       (1 << MAS2_E_SHIFT)

#define MAS3_RPN_SHIFT 12
#define MAS3_RPN_MASK  (0xfffff << MAS3_RPN_SHIFT)

#define MAS3_U0           0x00000200
#define MAS3_U1           0x00000100
#define MAS3_U2           0x00000080
#define MAS3_U3           0x00000040
#define MAS3_UX           0x00000020
#define MAS3_SX           0x00000010
#define MAS3_UW           0x00000008
#define MAS3_SW           0x00000004
#define MAS3_UR           0x00000002
#define MAS3_SR           0x00000001
#define MAS3_SPSIZE_SHIFT 1
#define MAS3_SPSIZE_MASK  (0x3e << MAS3_SPSIZE_SHIFT)

#define MAS4_TLBSELD_SHIFT MAS0_TLBSEL_SHIFT
#define MAS4_TLBSELD_MASK  MAS0_TLBSEL_MASK
#define MAS4_TIDSELD_MASK  0x00030000
#define MAS4_TIDSELD_PID0  0x00000000
#define MAS4_TIDSELD_PID1  0x00010000
#define MAS4_TIDSELD_PID2  0x00020000
#define MAS4_TIDSELD_PIDZ  0x00030000
#define MAS4_INDD          0x00008000 /* Default IND */
#define MAS4_TSIZED_SHIFT  MAS1_TSIZE_SHIFT
#define MAS4_TSIZED_MASK   MAS1_TSIZE_MASK
#define MAS4_ACMD          0x00000040
#define MAS4_VLED          0x00000020
#define MAS4_WD            0x00000010
#define MAS4_ID            0x00000008
#define MAS4_MD            0x00000004
#define MAS4_GD            0x00000002
#define MAS4_ED            0x00000001
#define MAS4_WIMGED_MASK   0x0000001f /* Default WIMGE */
#define MAS4_WIMGED_SHIFT  0

#define MAS5_SGS        0x80000000
#define MAS5_SLPID_MASK 0x00000fff

#define MAS6_SPID0       0x3fff0000
#define MAS6_SPID1       0x00007ffe
#define MAS6_ISIZE(x)    MAS1_TSIZE(x)
#define MAS6_SAS         0x00000001
#define MAS6_SPID        MAS6_SPID0
#define MAS6_SIND        0x00000002 /* Indirect page */
#define MAS6_SIND_SHIFT  1
#define MAS6_SPID_MASK   0x3fff0000
#define MAS6_SPID_SHIFT  16
#define MAS6_ISIZE_MASK  0x00000f80
#define MAS6_ISIZE_SHIFT 7

#define MAS7_RPN 0xffffffff

#define MAS8_TGS    0x80000000
#define MAS8_VF     0x40000000
#define MAS8_TLBPID 0x00000fff

/* Bit definitions for MMUCFG */
#define MMUCFG_MAVN     0x00000003 /* MMU Architecture Version Number */
#define MMUCFG_MAVN_V1  0x00000000 /* v1.0 */
#define MMUCFG_MAVN_V2  0x00000001 /* v2.0 */
#define MMUCFG_NTLBS    0x0000000c /* Number of TLBs */
#define MMUCFG_PIDSIZE  0x000007c0 /* PID Reg Size */
#define MMUCFG_TWC      0x00008000 /* TLB Write Conditional (v2.0) */
#define MMUCFG_LRAT     0x00010000 /* LRAT Supported (v2.0) */
#define MMUCFG_RASIZE   0x00fe0000 /* Real Addr Size */
#define MMUCFG_LPIDSIZE 0x0f000000 /* LPID Reg Size */

/* Bit definitions for MMUCSR0 */
#define MMUCSR0_TLB1FI 0x00000002 /* TLB1 Flash invalidate */
#define MMUCSR0_TLB0FI 0x00000004 /* TLB0 Flash invalidate */
#define MMUCSR0_TLB2FI 0x00000040 /* TLB2 Flash invalidate */
#define MMUCSR0_TLB3FI 0x00000020 /* TLB3 Flash invalidate */
#define MMUCSR0_TLBFI  (MMUCSR0_TLB0FI | MMUCSR0_TLB1FI | MMUCSR0_TLB2FI | MMUCSR0_TLB3FI)
#define MMUCSR0_TLB0PS 0x00000780 /* TLB0 Page Size */
#define MMUCSR0_TLB1PS 0x00007800 /* TLB1 Page Size */
#define MMUCSR0_TLB2PS 0x00078000 /* TLB2 Page Size */
#define MMUCSR0_TLB3PS 0x00780000 /* TLB3 Page Size */

/* TLBnCFG encoding */
#define TLBnCFG_N_ENTRY       0x00000fff /* number of entries */
#define TLBnCFG_HES           0x00002000 /* HW select supported */
#define TLBnCFG_AVAIL         0x00004000 /* variable page size */
#define TLBnCFG_IPROT         0x00008000 /* IPROT supported */
#define TLBnCFG_GTWE          0x00010000 /* Guest can write */
#define TLBnCFG_IND           0x00020000 /* IND entries supported */
#define TLBnCFG_PT            0x00040000 /* Can load from page table */
#define TLBnCFG_MINSIZE       0x00f00000 /* Minimum Page Size (v1.0) */
#define TLBnCFG_MINSIZE_SHIFT 20
#define TLBnCFG_MAXSIZE       0x000f0000 /* Maximum Page Size (v1.0) */
#define TLBnCFG_MAXSIZE_SHIFT 16
#define TLBnCFG_ASSOC         0xff000000 /* Associativity */
#define TLBnCFG_ASSOC_SHIFT   24

/* TLBnPS encoding */
#define TLBnPS_4K   0x00000004
#define TLBnPS_8K   0x00000008
#define TLBnPS_16K  0x00000010
#define TLBnPS_32K  0x00000020
#define TLBnPS_64K  0x00000040
#define TLBnPS_128K 0x00000080
#define TLBnPS_256K 0x00000100
#define TLBnPS_512K 0x00000200
#define TLBnPS_1M   0x00000400
#define TLBnPS_2M   0x00000800
#define TLBnPS_4M   0x00001000
#define TLBnPS_8M   0x00002000
#define TLBnPS_16M  0x00004000
#define TLBnPS_32M  0x00008000
#define TLBnPS_64M  0x00010000
#define TLBnPS_128M 0x00020000
#define TLBnPS_256M 0x00040000
#define TLBnPS_512M 0x00080000
#define TLBnPS_1G   0x00100000
#define TLBnPS_2G   0x00200000
#define TLBnPS_4G   0x00400000
#define TLBnPS_8G   0x00800000
#define TLBnPS_16G  0x01000000
#define TLBnPS_32G  0x02000000
#define TLBnPS_64G  0x04000000
#define TLBnPS_128G 0x08000000
#define TLBnPS_256G 0x10000000

/* tlbilx action encoding */
#define TLBILX_T_ALL       0
#define TLBILX_T_TID       1
#define TLBILX_T_FULLMATCH 3
#define TLBILX_T_CLASS0    4
#define TLBILX_T_CLASS1    5
#define TLBILX_T_CLASS2    6
#define TLBILX_T_CLASS3    7

/* BookE 2.06 helper defines */

#define BOOKE206_FLUSH_TLB0 (1 << 0)
#define BOOKE206_FLUSH_TLB1 (1 << 1)
#define BOOKE206_FLUSH_TLB2 (1 << 2)
#define BOOKE206_FLUSH_TLB3 (1 << 3)

/* number of possible TLBs */
#define BOOKE206_MAX_TLBN 4

/*****************************************************************************/
/* The whole PowerPC CPU context */
#define NB_MMU_MODES 3

typedef struct DisasContext {
    struct DisasContextBase base;
    uint32_t opcode;
    uint32_t exception;
    int access_type;
    /* Translation flags */
    int le_mode;
#if defined(TARGET_PPC64)
    int sf_mode;
    int has_cfar;
#endif
    int fpu_enabled;
    int altivec_enabled;
    int spe_enabled;
    ppc_spr_t *spr_cb; /* Needed to check rights for mfspr/mtspr */
    uint32_t vle_enabled;
} DisasContext;

struct ppc_def_t {
    const char *name;
    uint32_t pvr;
    uint32_t svr;
    uint64_t insns_flags;
    uint64_t insns_flags2;
    uint64_t msr_mask;
    powerpc_mmu_t mmu_model;
    powerpc_excp_t excp_model;
    powerpc_input_t bus_model;
    uint32_t flags;
    int bfd_mach;
    void (*init_proc)(CPUState *env);
    int (*check_pow)(CPUState *env);
};

#define CPU_STATE_SIZE ((size_t)&((CPUState *)0)->current_tb)
//  This ought to be the pc, not the next pc
#define CPU_PC(x) x->nip - 4

//  +---------------------------------------+
//  | ALL FIELDS WHICH STATE MUST BE STORED |
//  | DURING SERIALIZATION SHOULD BE PLACED |
//  | BEFORE >CPU_COMMON< SECTION.          |
//  +---------------------------------------+
struct CPUState {
    /* First are the most commonly used resources
     * during translated code execution
     */
    /* general purpose registers */
    target_ulong gpr[32];
    /* Storage for GPR MSB, used by the SPE extension */
    target_ulong gprh[32];
    /* LR */
    target_ulong lr;
    /* CTR */
    target_ulong ctr;
    /* condition register */
    uint32_t crf[8];
#if defined(TARGET_PPC64)
    /* CFAR */
    target_ulong cfar;
#endif
    /* XER */
    target_ulong xer;
    /* Reservation address */
    target_ulong reserve_addr;
    /* Reservation value */
    target_ulong reserve_val;
    /* Reservation store address */
    target_ulong reserve_ea;
    /* Reserved store source register and size */
    target_ulong reserve_info;
    /* Those ones are used in supervisor mode only */
    /* machine state register */
    target_ulong msr;
    /* temporary general purpose registers */
    target_ulong tgpr[4]; /* Used to speed-up TLB assist handlers */

    /* Floating point execution context */
    float_status fp_status;
    /* floating point registers */
    float64 fpr[32];
    /* floating point status and control register */
    uint32_t fpscr;

    /* Next instruction pointer */
    target_ulong nip;

    int access_type; /* when a memory exception occurs, the access
                        type is stored here */

    /* MMU context - only relevant for full system emulation */
#if defined(TARGET_PPC64)
    /* Address space register */
    target_ulong asr;
    /* PowerPC 64 SLB area */
    int slb_nr;
#endif
    /* segment registers */
    target_ulong sr[32];
    target_ulong DBAT[2][8];
    target_ulong IBAT[2][8];
    /* PowerPC TLB registers (for 4xx, e500 and 60x software driven TLBs) */
    int nb_tlb;      /* Total number of TLB                                  */
    int tlb_per_way; /* Speed-up helper: used to avoid divisions at run time */
    int nb_ways;     /* Number of ways in the TLB set                        */
    int last_way;    /* Last used way used to allocate TLB in a LRU way      */
    int id_tlbs;     /* If 1, MMU has separated TLBs for instructions & data */
    int nb_pids;     /* Number of available PID registers                    */
    /* 403 dedicated access protection registers */
    target_ulong pb[4];

    /* Other registers */
    /* Special purpose registers */
    target_ulong spr[1024];
    uint32_t vscr;
    /* SPE registers */
    uint64_t spe_acc;
    uint32_t spe_fscr;

    /* Those resources are used during exception processing */
    /* CPU model definition */
    target_ulong msr_mask;
    int error_code;
    uint32_t pending_interrupts;
    /* This is the IRQ controller, which is implementation dependent
     * and only relevant when emulating a complete machine.
     */
    uint32_t irq_input_state;
    /* Exception vectors */
    target_ulong excp_vectors[POWERPC_EXCP_NB];
    target_ulong excp_prefix;
    target_ulong hreset_excp_prefix;
    target_ulong ivor_mask;
    target_ulong ivpr_mask;
    target_ulong hreset_vector;

    /* Those resources are used only in Qemu core */
    target_ulong hflags;      /* hflags is a MSR & HFLAGS_MASK         */
    target_ulong hflags_nmsr; /* specific hflags, not coming from MSR */
    int mmu_idx;              /* precomputed MMU index to speed up mem accesses */

    /* Power management */
    int power_mode;

    /***************/

    CPU_COMMON

    ppc_tlb_t tlb; /* TLB is optional. Allocate them only if needed        */
#if defined(TARGET_PPC64)
    ppc_slb_t slb[64];
#endif

    target_phys_addr_t htab_base;
    target_phys_addr_t htab_mask;

    /* externally stored hash table */
    uint8_t *external_htab;
    /* BATs */
    int nb_BATs;
    int tlb_type;   /* Type of TLB we're dealing with                       */
    bool tlb_dirty; /* Set to non-zero when modifying TLB                  */

    ppc_spr_t spr_cb[1024];
    /* Altivec registers */
    ppc_avr_t avr[32];

    /* SPE and Altivec can share a status since they will never be used
     * simultaneously */
    float_status vec_status;

    /* Internal devices resources */

    int dcache_line_size;
    int icache_line_size;

    powerpc_mmu_t mmu_model;
    powerpc_excp_t excp_model;
    powerpc_input_t bus_model;
    int bfd_mach;
    uint32_t flags;
    uint64_t insns_flags;
    uint64_t insns_flags2;

#if defined(TARGET_PPC64)
    target_phys_addr_t vpa;
    target_phys_addr_t slb_shadow;
    target_phys_addr_t dispatch_trace_log;
    uint32_t dtl_size;
#endif /* TARGET_PPC64 */

    void **irq_inputs;

    /* Those resources are used only during code translation */
    /* opcode handlers */
    opc_handler_t *opcodes[0x40];
    opc_handler_t *vle_opcodes[0x40];

    int (*check_pow)(CPUState *env);

    /* booke timers */

    /* Specifies bit locations of the Time Base used to signal a fixed timer
     * exception on a transition from 0 to 1. (watchdog or fixed-interval timer)
     *
     * 0 selects the least significant bit.
     * 63 selects the most significant bit.
     */
    uint8_t fit_period[4];
    uint8_t wdt_period[4];
};

#define SET_FIT_PERIOD(a_, b_, c_, d_) \
    do {                               \
        env->fit_period[0] = (a_);     \
        env->fit_period[1] = (b_);     \
        env->fit_period[2] = (c_);     \
        env->fit_period[3] = (d_);     \
    } while(0)

#define SET_WDT_PERIOD(a_, b_, c_, d_) \
    do {                               \
        env->wdt_period[0] = (a_);     \
        env->wdt_period[1] = (b_);     \
        env->wdt_period[2] = (c_);     \
        env->wdt_period[3] = (d_);     \
    } while(0)

/* Context used internally during MMU translations */
typedef struct mmu_ctx_t mmu_ctx_t;
struct mmu_ctx_t {
    target_phys_addr_t raddr;   /* Real address              */
    target_phys_addr_t eaddr;   /* Effective address         */
    int prot;                   /* Protection bits           */
    target_phys_addr_t hash[2]; /* Pagetable hash values     */
    target_ulong ptem;          /* Virtual segment ID | API  */
    int key;                    /* Access key                */
    int nx;                     /* Non-execute area          */
};

/*****************************************************************************/
int cpu_handle_mmu_fault(CPUState *env, target_ulong address, int rw, int mmu_idx, int no_page_fault);
int get_physical_address(CPUState *env, mmu_ctx_t *ctx, target_ulong vaddr, int rw, int access_type);
void ppc_hw_interrupt(CPUState *env);

void ppc6xx_tlb_store(CPUState *env, target_ulong EPN, int way, int is_code, target_ulong pte0, target_ulong pte1);
void ppc_store_ibatu(CPUState *env, int nr, target_ulong value);
void ppc_store_ibatl(CPUState *env, int nr, target_ulong value);
void ppc_store_dbatu(CPUState *env, int nr, target_ulong value);
void ppc_store_dbatl(CPUState *env, int nr, target_ulong value);
void ppc_store_ibatu_601(CPUState *env, int nr, target_ulong value);
void ppc_store_ibatl_601(CPUState *env, int nr, target_ulong value);
void ppc_store_sdr1(CPUState *env, target_ulong value);

#if defined(TARGET_PPC64)
void ppc_store_asr(CPUState *env, target_ulong value);
target_ulong ppc_load_slb(CPUState *env, int slb_nr);
target_ulong ppc_load_sr(CPUState *env, int sr_nr);
int ppc_store_slb(CPUState *env, target_ulong rb, target_ulong rs);
int ppc_load_slb_esid(CPUState *env, target_ulong rb, target_ulong *rt);
int ppc_load_slb_vsid(CPUState *env, target_ulong rb, target_ulong *rt);
#endif /* defined(TARGET_PPC64) */

void ppc_store_sr(CPUState *env, int srnum, target_ulong value);
void ppc_store_msr(CPUState *env, target_ulong value);

const ppc_def_t *ppc_find_by_pvr(uint32_t pvr);
const ppc_def_t *cpu_ppc_find_by_name(const char *name);
int cpu_ppc_register_internal(CPUState *env, const ppc_def_t *def);

/* Time-base and decrementer management */
void booke206_flush_tlb(CPUState *env, int flags, const int check_iprot);
target_phys_addr_t booke206_tlb_to_page_size(CPUState *env, ppcmas_tlb_t *tlb);
int ppcemb_tlb_check(CPUState *env, ppcemb_tlb_t *tlb, target_phys_addr_t *raddrp, target_ulong address, uint32_t pid, int ext,
                     int i);
int ppcmas_tlb_check(CPUState *env, ppcmas_tlb_t *tlb, target_phys_addr_t *raddrp, target_ulong address, uint32_t pid);
#if defined(TARGET_PPC64)
void ppc_slb_invalidate_all(CPUState *env);
void ppc_slb_invalidate_one(CPUState *env, uint64_t T0);
#endif
void ppc_tlb_invalidate_all(CPUState *env);
void ppc_tlb_invalidate_one(CPUState *env, target_ulong addr);
int ppcemb_tlb_search(CPUState *env, target_ulong address, uint32_t pid);

/* MMU modes definitions */
#define MMU_MODE0_SUFFIX _user
#define MMU_MODE1_SUFFIX _kernel
#define MMU_MODE2_SUFFIX _hypv
#define MMU_USER_IDX     0
static inline int cpu_mmu_index(CPUState *env)
{
    return env->mmu_idx;
}

#include "cpu-all.h"

/*****************************************************************************/
/* CRF definitions */
#define CRF_LT        3
#define CRF_GT        2
#define CRF_EQ        1
#define CRF_SO        0
#define CRF_CH        (1 << CRF_LT)
#define CRF_CL        (1 << CRF_GT)
#define CRF_CH_OR_CL  (1 << CRF_EQ)
#define CRF_CH_AND_CL (1 << CRF_SO)

/* XER definitions */
#define XER_SO  31
#define XER_OV  30
#define XER_CA  29
#define XER_CMP 8
#define XER_BC  0
#define xer_so  ((env->xer >> XER_SO) & 1)
#define xer_ov  ((env->xer >> XER_OV) & 1)
#define xer_ca  ((env->xer >> XER_CA) & 1)
#define xer_cmp ((env->xer >> XER_CMP) & 0xFF)
#define xer_bc  ((env->xer >> XER_BC) & 0x7F)

/* SPR definitions */
#define SPR_MQ            (0x000)
#define SPR_XER           (0x001)
#define SPR_601_VRTCU     (0x004)
#define SPR_601_VRTCL     (0x005)
#define SPR_601_UDECR     (0x006)
#define SPR_LR            (0x008)
#define SPR_CTR           (0x009)
#define SPR_DSCR          (0x011)
#define SPR_DSISR         (0x012)
#define SPR_DAR           (0x013) /* DAE for PowerPC 601 */
#define SPR_601_RTCU      (0x014)
#define SPR_601_RTCL      (0x015)
#define SPR_DECR          (0x016)
#define SPR_SDR1          (0x019)
#define SPR_SRR0          (0x01A)
#define SPR_SRR1          (0x01B)
#define SPR_CFAR          (0x01C)
#define SPR_AMR           (0x01D)
#define SPR_BOOKE_PID     (0x030)
#define SPR_BOOKE_DECAR   (0x036)
#define SPR_BOOKE_CSRR0   (0x03A)
#define SPR_BOOKE_CSRR1   (0x03B)
#define SPR_BOOKE_DEAR    (0x03D)
#define SPR_BOOKE_ESR     (0x03E)
#define SPR_BOOKE_IVPR    (0x03F)
#define SPR_MPC_EIE       (0x050)
#define SPR_MPC_EID       (0x051)
#define SPR_MPC_NRI       (0x052)
#define SPR_CTRL          (0x088)
#define SPR_MPC_CMPA      (0x090)
#define SPR_MPC_CMPB      (0x091)
#define SPR_MPC_CMPC      (0x092)
#define SPR_MPC_CMPD      (0x093)
#define SPR_MPC_ECR       (0x094)
#define SPR_MPC_DER       (0x095)
#define SPR_MPC_COUNTA    (0x096)
#define SPR_MPC_COUNTB    (0x097)
#define SPR_UCTRL         (0x098)
#define SPR_MPC_CMPE      (0x098)
#define SPR_MPC_CMPF      (0x099)
#define SPR_MPC_CMPG      (0x09A)
#define SPR_MPC_CMPH      (0x09B)
#define SPR_MPC_LCTRL1    (0x09C)
#define SPR_MPC_LCTRL2    (0x09D)
#define SPR_MPC_ICTRL     (0x09E)
#define SPR_MPC_BAR       (0x09F)
#define SPR_VRSAVE        (0x100)
#define SPR_USPRG0        (0x100)
#define SPR_USPRG1        (0x101)
#define SPR_USPRG2        (0x102)
#define SPR_USPRG3        (0x103)
#define SPR_USPRG4        (0x104)
#define SPR_USPRG5        (0x105)
#define SPR_USPRG6        (0x106)
#define SPR_USPRG7        (0x107)
#define SPR_VTBL          (0x10C)
#define SPR_VTBU          (0x10D)
#define SPR_SPRG0         (0x110)
#define SPR_SPRG1         (0x111)
#define SPR_SPRG2         (0x112)
#define SPR_SPRG3         (0x113)
#define SPR_SPRG4         (0x114)
#define SPR_SCOMC         (0x114)
#define SPR_SPRG5         (0x115)
#define SPR_SCOMD         (0x115)
#define SPR_SPRG6         (0x116)
#define SPR_SPRG7         (0x117)
#define SPR_ASR           (0x118)
#define SPR_EAR           (0x11A)
#define SPR_TBL           (0x11C)
#define SPR_TBU           (0x11D)
#define SPR_TBU40         (0x11E)
#define SPR_SVR           (0x11E)
#define SPR_BOOKE_PIR     (0x11E)
#define SPR_PVR           (0x11F)
#define SPR_HSPRG0        (0x130)
#define SPR_BOOKE_DBSR    (0x130)
#define SPR_HSPRG1        (0x131)
#define SPR_HDSISR        (0x132)
#define SPR_HDAR          (0x133)
#define SPR_BOOKE_EPCR    (0x133)
#define SPR_SPURR         (0x134)
#define SPR_BOOKE_DBCR0   (0x134)
#define SPR_IBCR          (0x135)
#define SPR_PURR          (0x135)
#define SPR_BOOKE_DBCR1   (0x135)
#define SPR_DBCR          (0x136)
#define SPR_HDEC          (0x136)
#define SPR_BOOKE_DBCR2   (0x136)
#define SPR_HIOR          (0x137)
#define SPR_MBAR          (0x137)
#define SPR_RMOR          (0x138)
#define SPR_BOOKE_IAC1    (0x138)
#define SPR_HRMOR         (0x139)
#define SPR_BOOKE_IAC2    (0x139)
#define SPR_HSRR0         (0x13A)
#define SPR_BOOKE_IAC3    (0x13A)
#define SPR_HSRR1         (0x13B)
#define SPR_BOOKE_IAC4    (0x13B)
#define SPR_LPCR          (0x13E)
#define SPR_BOOKE_DAC1    (0x13C)
#define SPR_LPIDR         (0x13D)
#define SPR_DABR2         (0x13D)
#define SPR_BOOKE_DAC2    (0x13D)
#define SPR_BOOKE_DVC1    (0x13E)
#define SPR_BOOKE_DVC2    (0x13F)
#define SPR_BOOKE_TSR     (0x150)
#define SPR_BOOKE_TCR     (0x154)
#define SPR_BOOKE_MAS8    (0x155)
#define SPR_BOOKE_IVOR0   (0x190)
#define SPR_BOOKE_IVOR1   (0x191)
#define SPR_BOOKE_IVOR2   (0x192)
#define SPR_BOOKE_IVOR3   (0x193)
#define SPR_BOOKE_IVOR4   (0x194)
#define SPR_BOOKE_IVOR5   (0x195)
#define SPR_BOOKE_IVOR6   (0x196)
#define SPR_BOOKE_IVOR7   (0x197)
#define SPR_BOOKE_IVOR8   (0x198)
#define SPR_BOOKE_IVOR9   (0x199)
#define SPR_BOOKE_IVOR10  (0x19A)
#define SPR_BOOKE_IVOR11  (0x19B)
#define SPR_BOOKE_IVOR12  (0x19C)
#define SPR_BOOKE_IVOR13  (0x19D)
#define SPR_BOOKE_IVOR14  (0x19E)
#define SPR_BOOKE_IVOR15  (0x19F)
#define SPR_BOOKE_IVOR38  (0x1B0)
#define SPR_BOOKE_IVOR39  (0x1B1)
#define SPR_BOOKE_IVOR40  (0x1B2)
#define SPR_BOOKE_IVOR41  (0x1B3)
#define SPR_BOOKE_IVOR42  (0x1B4)
#define SPR_BOOKE_SPEFSCR (0x200)
#define SPR_Exxx_BBEAR    (0x201)
#define SPR_Exxx_BBTAR    (0x202)
#define SPR_Exxx_L1CFG0   (0x203)
#define SPR_Exxx_NPIDR    (0x205)
#define SPR_ATBL          (0x20E)
#define SPR_ATBU          (0x20F)
#define SPR_IBAT0U        (0x210)
#define SPR_BOOKE_IVOR32  (0x210)
#define SPR_RCPU_MI_GRA   (0x210)
#define SPR_IBAT0L        (0x211)
#define SPR_BOOKE_IVOR33  (0x211)
#define SPR_IBAT1U        (0x212)
#define SPR_BOOKE_IVOR34  (0x212)
#define SPR_IBAT1L        (0x213)
#define SPR_BOOKE_IVOR35  (0x213)
#define SPR_IBAT2U        (0x214)
#define SPR_BOOKE_IVOR36  (0x214)
#define SPR_IBAT2L        (0x215)
#define SPR_BOOKE_IVOR37  (0x215)
#define SPR_IBAT3U        (0x216)
#define SPR_IBAT3L        (0x217)
#define SPR_DBAT0U        (0x218)
#define SPR_RCPU_L2U_GRA  (0x218)
#define SPR_DBAT0L        (0x219)
#define SPR_DBAT1U        (0x21A)
#define SPR_DBAT1L        (0x21B)
#define SPR_DBAT2U        (0x21C)
#define SPR_DBAT2L        (0x21D)
#define SPR_DBAT3U        (0x21E)
#define SPR_DBAT3L        (0x21F)
#define SPR_IBAT4U        (0x230)
#define SPR_RPCU_BBCMCR   (0x230)
#define SPR_MPC_IC_CST    (0x230)
#define SPR_Exxx_CTXCR    (0x230)
#define SPR_IBAT4L        (0x231)
#define SPR_MPC_IC_ADR    (0x231)
#define SPR_Exxx_DBCR3    (0x231)
#define SPR_IBAT5U        (0x232)
#define SPR_MPC_IC_DAT    (0x232)
#define SPR_Exxx_DBCNT    (0x232)
#define SPR_IBAT5L        (0x233)
#define SPR_IBAT6U        (0x234)
#define SPR_IBAT6L        (0x235)
#define SPR_IBAT7U        (0x236)
#define SPR_IBAT7L        (0x237)
#define SPR_DBAT4U        (0x238)
#define SPR_RCPU_L2U_MCR  (0x238)
#define SPR_MPC_DC_CST    (0x238)
#define SPR_Exxx_ALTCTXCR (0x238)
#define SPR_DBAT4L        (0x239)
#define SPR_MPC_DC_ADR    (0x239)
#define SPR_DBAT5U        (0x23A)
#define SPR_BOOKE_MCSRR0  (0x23A)
#define SPR_MPC_DC_DAT    (0x23A)
#define SPR_DBAT5L        (0x23B)
#define SPR_BOOKE_MCSRR1  (0x23B)
#define SPR_DBAT6U        (0x23C)
#define SPR_BOOKE_MCSR    (0x23C)
#define SPR_DBAT6L        (0x23D)
#define SPR_Exxx_MCAR     (0x23D)
#define SPR_DBAT7U        (0x23E)
#define SPR_BOOKE_DSRR0   (0x23E)
#define SPR_DBAT7L        (0x23F)
#define SPR_BOOKE_DSRR1   (0x23F)
#define SPR_BOOKE_SPRG8   (0x25C)
#define SPR_BOOKE_SPRG9   (0x25D)
#define SPR_BOOKE_MAS0    (0x270)
#define SPR_BOOKE_MAS1    (0x271)
#define SPR_BOOKE_MAS2    (0x272)
#define SPR_BOOKE_MAS3    (0x273)
#define SPR_BOOKE_MAS4    (0x274)
#define SPR_BOOKE_MAS5    (0x275)
#define SPR_BOOKE_MAS6    (0x276)
#define SPR_BOOKE_PID1    (0x279)
#define SPR_BOOKE_PID2    (0x27A)
#define SPR_MPC_DPDR      (0x280)
#define SPR_MPC_IMMR      (0x288)
#define SPR_BOOKE_TLB0CFG (0x2B0)
#define SPR_BOOKE_TLB1CFG (0x2B1)
#define SPR_BOOKE_TLB2CFG (0x2B2)
#define SPR_BOOKE_TLB3CFG (0x2B3)
#define SPR_BOOKE_EPR     (0x2BE)
#define SPR_PERF0         (0x300)
#define SPR_RCPU_MI_RBA0  (0x300)
#define SPR_MPC_MI_CTR    (0x300)
#define SPR_PERF1         (0x301)
#define SPR_RCPU_MI_RBA1  (0x301)
#define SPR_PERF2         (0x302)
#define SPR_RCPU_MI_RBA2  (0x302)
#define SPR_MPC_MI_AP     (0x302)
#define SPR_PERF3         (0x303)
#define SPR_620_PMC1R     (0x303)
#define SPR_RCPU_MI_RBA3  (0x303)
#define SPR_MPC_MI_EPN    (0x303)
#define SPR_PERF4         (0x304)
#define SPR_620_PMC2R     (0x304)
#define SPR_PERF5         (0x305)
#define SPR_MPC_MI_TWC    (0x305)
#define SPR_PERF6         (0x306)
#define SPR_MPC_MI_RPN    (0x306)
#define SPR_PERF7         (0x307)
#define SPR_PERF8         (0x308)
#define SPR_RCPU_L2U_RBA0 (0x308)
#define SPR_MPC_MD_CTR    (0x308)
#define SPR_PERF9         (0x309)
#define SPR_RCPU_L2U_RBA1 (0x309)
#define SPR_MPC_MD_CASID  (0x309)
#define SPR_PERFA         (0x30A)
#define SPR_RCPU_L2U_RBA2 (0x30A)
#define SPR_MPC_MD_AP     (0x30A)
#define SPR_PERFB         (0x30B)
#define SPR_620_MMCR0R    (0x30B)
#define SPR_RCPU_L2U_RBA3 (0x30B)
#define SPR_MPC_MD_EPN    (0x30B)
#define SPR_PERFC         (0x30C)
#define SPR_MPC_MD_TWB    (0x30C)
#define SPR_PERFD         (0x30D)
#define SPR_MPC_MD_TWC    (0x30D)
#define SPR_PERFE         (0x30E)
#define SPR_MPC_MD_RPN    (0x30E)
#define SPR_PERFF         (0x30F)
#define SPR_MPC_MD_TW     (0x30F)
#define SPR_UPERF0        (0x310)
#define SPR_UPERF1        (0x311)
#define SPR_UPERF2        (0x312)
#define SPR_UPERF3        (0x313)
#define SPR_620_PMC1W     (0x313)
#define SPR_UPERF4        (0x314)
#define SPR_620_PMC2W     (0x314)
#define SPR_UPERF5        (0x315)
#define SPR_UPERF6        (0x316)
#define SPR_UPERF7        (0x317)
#define SPR_UPERF8        (0x318)
#define SPR_UPERF9        (0x319)
#define SPR_UPERFA        (0x31A)
#define SPR_UPERFB        (0x31B)
#define SPR_620_MMCR0W    (0x31B)
#define SPR_UPERFC        (0x31C)
#define SPR_UPERFD        (0x31D)
#define SPR_UPERFE        (0x31E)
#define SPR_UPERFF        (0x31F)
#define SPR_RCPU_MI_RA0   (0x320)
#define SPR_MPC_MI_DBCAM  (0x320)
#define SPR_RCPU_MI_RA1   (0x321)
#define SPR_MPC_MI_DBRAM0 (0x321)
#define SPR_RCPU_MI_RA2   (0x322)
#define SPR_MPC_MI_DBRAM1 (0x322)
#define SPR_RCPU_MI_RA3   (0x323)
#define SPR_RCPU_L2U_RA0  (0x328)
#define SPR_MPC_MD_DBCAM  (0x328)
#define SPR_RCPU_L2U_RA1  (0x329)
#define SPR_MPC_MD_DBRAM0 (0x329)
#define SPR_RCPU_L2U_RA2  (0x32A)
#define SPR_MPC_MD_DBRAM1 (0x32A)
#define SPR_RCPU_L2U_RA3  (0x32B)
#define SPR_440_INV0      (0x370)
#define SPR_440_INV1      (0x371)
#define SPR_440_INV2      (0x372)
#define SPR_440_INV3      (0x373)
#define SPR_440_ITV0      (0x374)
#define SPR_440_ITV1      (0x375)
#define SPR_440_ITV2      (0x376)
#define SPR_440_ITV3      (0x377)
#define SPR_440_CCR1      (0x378)
#define SPR_DCRIPR        (0x37B)
#define SPR_PPR           (0x380)
#define SPR_750_GQR0      (0x390)
#define SPR_440_DNV0      (0x390)
#define SPR_750_GQR1      (0x391)
#define SPR_440_DNV1      (0x391)
#define SPR_750_GQR2      (0x392)
#define SPR_440_DNV2      (0x392)
#define SPR_750_GQR3      (0x393)
#define SPR_440_DNV3      (0x393)
#define SPR_750_GQR4      (0x394)
#define SPR_440_DTV0      (0x394)
#define SPR_750_GQR5      (0x395)
#define SPR_440_DTV1      (0x395)
#define SPR_750_GQR6      (0x396)
#define SPR_440_DTV2      (0x396)
#define SPR_750_GQR7      (0x397)
#define SPR_440_DTV3      (0x397)
#define SPR_750_THRM4     (0x398)
#define SPR_750CL_HID2    (0x398)
#define SPR_440_DVLIM     (0x398)
#define SPR_750_WPAR      (0x399)
#define SPR_440_IVLIM     (0x399)
#define SPR_750_DMAU      (0x39A)
#define SPR_750_DMAL      (0x39B)
#define SPR_440_RSTCFG    (0x39B)
#define SPR_BOOKE_DCDBTRL (0x39C)
#define SPR_BOOKE_DCDBTRH (0x39D)
#define SPR_BOOKE_ICDBTRL (0x39E)
#define SPR_BOOKE_ICDBTRH (0x39F)
#define SPR_UMMCR2        (0x3A0)
#define SPR_UPMC5         (0x3A1)
#define SPR_UPMC6         (0x3A2)
#define SPR_UBAMR         (0x3A7)
#define SPR_UMMCR0        (0x3A8)
#define SPR_UPMC1         (0x3A9)
#define SPR_UPMC2         (0x3AA)
#define SPR_USIAR         (0x3AB)
#define SPR_UMMCR1        (0x3AC)
#define SPR_UPMC3         (0x3AD)
#define SPR_UPMC4         (0x3AE)
#define SPR_USDA          (0x3AF)
#define SPR_40x_ZPR       (0x3B0)
#define SPR_BOOKE_MAS7    (0x3B0)
#define SPR_620_PMR0      (0x3B0)
#define SPR_MMCR2         (0x3B0)
#define SPR_PMC5          (0x3B1)
#define SPR_40x_PID       (0x3B1)
#define SPR_620_PMR1      (0x3B1)
#define SPR_PMC6          (0x3B2)
#define SPR_440_MMUCR     (0x3B2)
#define SPR_620_PMR2      (0x3B2)
#define SPR_4xx_CCR0      (0x3B3)
#define SPR_BOOKE_EPLC    (0x3B3)
#define SPR_620_PMR3      (0x3B3)
#define SPR_405_IAC3      (0x3B4)
#define SPR_BOOKE_EPSC    (0x3B4)
#define SPR_620_PMR4      (0x3B4)
#define SPR_405_IAC4      (0x3B5)
#define SPR_620_PMR5      (0x3B5)
#define SPR_405_DVC1      (0x3B6)
#define SPR_620_PMR6      (0x3B6)
#define SPR_405_DVC2      (0x3B7)
#define SPR_620_PMR7      (0x3B7)
#define SPR_BAMR          (0x3B7)
#define SPR_MMCR0         (0x3B8)
#define SPR_620_PMR8      (0x3B8)
#define SPR_PMC1          (0x3B9)
#define SPR_40x_SGR       (0x3B9)
#define SPR_620_PMR9      (0x3B9)
#define SPR_PMC2          (0x3BA)
#define SPR_40x_DCWR      (0x3BA)
#define SPR_620_PMRA      (0x3BA)
#define SPR_SIAR          (0x3BB)
#define SPR_405_SLER      (0x3BB)
#define SPR_620_PMRB      (0x3BB)
#define SPR_MMCR1         (0x3BC)
#define SPR_405_SU0R      (0x3BC)
#define SPR_620_PMRC      (0x3BC)
#define SPR_401_SKR       (0x3BC)
#define SPR_PMC3          (0x3BD)
#define SPR_405_DBCR1     (0x3BD)
#define SPR_620_PMRD      (0x3BD)
#define SPR_PMC4          (0x3BE)
#define SPR_620_PMRE      (0x3BE)
#define SPR_SDA           (0x3BF)
#define SPR_620_PMRF      (0x3BF)
#define SPR_403_VTBL      (0x3CC)
#define SPR_403_VTBU      (0x3CD)
#define SPR_DMISS         (0x3D0)
#define SPR_DCMP          (0x3D1)
#define SPR_HASH1         (0x3D2)
#define SPR_HASH2         (0x3D3)
#define SPR_BOOKE_ICDBDR  (0x3D3)
#define SPR_TLBMISS       (0x3D4)
#define SPR_IMISS         (0x3D4)
#define SPR_40x_ESR       (0x3D4)
#define SPR_PTEHI         (0x3D5)
#define SPR_ICMP          (0x3D5)
#define SPR_40x_DEAR      (0x3D5)
#define SPR_PTELO         (0x3D6)
#define SPR_RPA           (0x3D6)
#define SPR_40x_EVPR      (0x3D6)
#define SPR_L3PM          (0x3D7)
#define SPR_403_CDBCR     (0x3D7)
#define SPR_L3ITCR0       (0x3D8)
#define SPR_TCR           (0x3D8)
#define SPR_40x_TSR       (0x3D8)
#define SPR_IBR           (0x3DA)
#define SPR_40x_TCR       (0x3DA)
#define SPR_ESASRR        (0x3DB)
#define SPR_40x_PIT       (0x3DB)
#define SPR_403_TBL       (0x3DC)
#define SPR_403_TBU       (0x3DD)
#define SPR_SEBR          (0x3DE)
#define SPR_40x_SRR2      (0x3DE)
#define SPR_SER           (0x3DF)
#define SPR_40x_SRR3      (0x3DF)
#define SPR_L3OHCR        (0x3E8)
#define SPR_L3ITCR1       (0x3E9)
#define SPR_L3ITCR2       (0x3EA)
#define SPR_L3ITCR3       (0x3EB)
#define SPR_HID0          (0x3F0)
#define SPR_40x_DBSR      (0x3F0)
#define SPR_HID1          (0x3F1)
#define SPR_IABR          (0x3F2)
#define SPR_40x_DBCR0     (0x3F2)
#define SPR_601_HID2      (0x3F2)
#define SPR_Exxx_L1CSR0   (0x3F2)
#define SPR_ICTRL         (0x3F3)
#define SPR_HID2          (0x3F3)
#define SPR_750CL_HID4    (0x3F3)
#define SPR_Exxx_L1CSR1   (0x3F3)
#define SPR_440_DBDR      (0x3F3)
#define SPR_LDSTDB        (0x3F4)
#define SPR_750_TDCL      (0x3F4)
#define SPR_40x_IAC1      (0x3F4)
#define SPR_MMUCSR0       (0x3F4)
#define SPR_DABR          (0x3F5)
#define DABR_MASK         (~(target_ulong)0x7)
#define SPR_Exxx_BUCSR    (0x3F5)
#define SPR_40x_IAC2      (0x3F5)
#define SPR_601_HID5      (0x3F5)
#define SPR_40x_DAC1      (0x3F6)
#define SPR_MSSCR0        (0x3F6)
#define SPR_970_HID5      (0x3F6)
#define SPR_MSSSR0        (0x3F7)
#define SPR_MSSCR1        (0x3F7)
#define SPR_DABRX         (0x3F7)
#define SPR_40x_DAC2      (0x3F7)
#define SPR_MMUCFG        (0x3F7)
#define SPR_LDSTCR        (0x3F8)
#define SPR_L2PMCR        (0x3F8)
#define SPR_750FX_HID2    (0x3F8)
#define SPR_620_BUSCSR    (0x3F8)
#define SPR_Exxx_L1FINV0  (0x3F8)
#define SPR_L2CR          (0x3F9)
#define SPR_620_L2CR      (0x3F9)
#define SPR_L3CR          (0x3FA)
#define SPR_750_TDCH      (0x3FA)
#define SPR_IABR2         (0x3FA)
#define SPR_40x_DCCR      (0x3FA)
#define SPR_620_L2SR      (0x3FA)
#define SPR_ICTC          (0x3FB)
#define SPR_40x_ICCR      (0x3FB)
#define SPR_THRM1         (0x3FC)
#define SPR_403_PBL1      (0x3FC)
#define SPR_SP            (0x3FD)
#define SPR_THRM2         (0x3FD)
#define SPR_403_PBU1      (0x3FD)
#define SPR_604_HID13     (0x3FD)
#define SPR_LT            (0x3FE)
#define SPR_THRM3         (0x3FE)
#define SPR_RCPU_FPECR    (0x3FE)
#define SPR_403_PBL2      (0x3FE)
#define SPR_PIR           (0x3FF)
#define SPR_403_PBU2      (0x3FF)
#define SPR_601_HID15     (0x3FF)
#define SPR_604_HID15     (0x3FF)
#define SPR_E500_SVR      (0x3FF)

/*****************************************************************************/
/* PowerPC Instructions types definitions                                    */
enum {
    PPC_NONE = 0x0000000000000000ULL,
    /* PowerPC base instructions set                                         */
    PPC_INSNS_BASE = 0x0000000000000001ULL,
/*   integer operations instructions                                     */
#define PPC_INTEGER PPC_INSNS_BASE
/*   flow control instructions                                           */
#define PPC_FLOW PPC_INSNS_BASE
/*   virtual memory instructions                                         */
#define PPC_MEM PPC_INSNS_BASE
/*   ld/st with reservation instructions                                 */
#define PPC_RES PPC_INSNS_BASE
/*   spr/msr access instructions                                         */
#define PPC_MISC PPC_INSNS_BASE
/*   VLE encoded instruction                                             */
#define PPC_VLE PPC_INSNS_BASE
    /* Deprecated instruction sets                                           */
    /*   Original POWER instruction set                                      */
    PPC_POWER = 0x0000000000000002ULL,
    /*   POWER2 instruction set extension                                    */
    PPC_POWER2 = 0x0000000000000004ULL,
    /*   Power RTC support                                                   */
    PPC_POWER_RTC = 0x0000000000000008ULL,
    /*   Power-to-PowerPC bridge (601)                                       */
    PPC_POWER_BR = 0x0000000000000010ULL,
    /* 64 bits PowerPC instruction set                                       */
    PPC_64B = 0x0000000000000020ULL,
    /*   New 64 bits extensions (PowerPC 2.0x)                               */
    PPC_64BX = 0x0000000000000040ULL,
    /*   64 bits hypervisor extensions                                       */
    PPC_64H = 0x0000000000000080ULL,
    /*   New wait instruction (PowerPC 2.0x)                                 */
    PPC_WAIT = 0x0000000000000100ULL,
    /*   Time base mftb instruction                                          */
    PPC_MFTB = 0x0000000000000200ULL,

    /* Fixed-point unit extensions                                           */
    /*   PowerPC 602 specific                                                */
    PPC_602_SPEC = 0x0000000000000400ULL,
    /*   isel instruction                                                    */
    PPC_ISEL = 0x0000000000000800ULL,
    /*   popcntb instruction                                                 */
    PPC_POPCNTB = 0x0000000000001000ULL,
    /*   string load / store                                                 */
    PPC_STRING = 0x0000000000002000ULL,

    /* Floating-point unit extensions                                        */
    /*   Optional floating point instructions                                */
    PPC_FLOAT = 0x0000000000010000ULL,
    /* New floating-point extensions (PowerPC 2.0x)                          */
    PPC_FLOAT_EXT = 0x0000000000020000ULL,
    PPC_FLOAT_FSQRT = 0x0000000000040000ULL,
    PPC_FLOAT_FRES = 0x0000000000080000ULL,
    PPC_FLOAT_FRSQRTE = 0x0000000000100000ULL,
    PPC_FLOAT_FRSQRTES = 0x0000000000200000ULL,
    PPC_FLOAT_FSEL = 0x0000000000400000ULL,
    PPC_FLOAT_STFIWX = 0x0000000000800000ULL,

    /* Vector/SIMD extensions                                                */
    /*   Altivec support                                                     */
    PPC_ALTIVEC = 0x0000000001000000ULL,
    /*   PowerPC 2.03 SPE extension                                          */
    PPC_SPE = 0x0000000002000000ULL,
    /*   PowerPC 2.03 SPE single-precision floating-point extension          */
    PPC_SPE_SINGLE = 0x0000000004000000ULL,
    /*   PowerPC 2.03 SPE double-precision floating-point extension          */
    PPC_SPE_DOUBLE = 0x0000000008000000ULL,

    /* Optional memory control instructions                                  */
    PPC_MEM_TLBIA = 0x0000000010000000ULL,
    PPC_MEM_TLBIE = 0x0000000020000000ULL,
    PPC_MEM_TLBSYNC = 0x0000000040000000ULL,
    /*   sync instruction                                                    */
    PPC_MEM_SYNC = 0x0000000080000000ULL,
    /*   eieio instruction                                                   */
    PPC_MEM_EIEIO = 0x0000000100000000ULL,

    /* Cache control instructions                                            */
    PPC_CACHE = 0x0000000200000000ULL,
    /*   icbi instruction                                                    */
    PPC_CACHE_ICBI = 0x0000000400000000ULL,
    /*   dcbz instruction with fixed cache line size                         */
    PPC_CACHE_DCBZ = 0x0000000800000000ULL,
    /*   dcbz instruction with tunable cache line size                       */
    PPC_CACHE_DCBZT = 0x0000001000000000ULL,
    /*   dcba instruction                                                    */
    PPC_CACHE_DCBA = 0x0000002000000000ULL,
    /*   Freescale cache locking instructions                                */
    PPC_CACHE_LOCK = 0x0000004000000000ULL,

    /* MMU related extensions                                                */
    /*   external control instructions                                       */
    PPC_EXTERN = 0x0000010000000000ULL,
    /*   segment register access instructions                                */
    PPC_SEGMENT = 0x0000020000000000ULL,
    /*   PowerPC 6xx TLB management instructions                             */
    PPC_6xx_TLB = 0x0000040000000000ULL,
    /* PowerPC 74xx TLB management instructions                              */
    PPC_74xx_TLB = 0x0000080000000000ULL,
    /*   PowerPC 40x TLB management instructions                             */
    PPC_40x_TLB = 0x0000100000000000ULL,
    /*   segment register access instructions for PowerPC 64 "bridge"        */
    PPC_SEGMENT_64B = 0x0000200000000000ULL,
    /*   SLB management                                                      */
    PPC_SLBI = 0x0000400000000000ULL,

    /* Embedded PowerPC dedicated instructions                               */
    PPC_WRTEE = 0x0001000000000000ULL,
    /* PowerPC 40x exception model                                           */
    PPC_40x_EXCP = 0x0002000000000000ULL,
    /* PowerPC 405 Mac instructions                                          */
    PPC_405_MAC = 0x0004000000000000ULL,
    /* PowerPC 440 specific instructions                                     */
    PPC_440_SPEC = 0x0008000000000000ULL,
    /* BookE (embedded) PowerPC specification                                */
    PPC_BOOKE = 0x0010000000000000ULL,
    /* mfapidi instruction                                                   */
    PPC_MFAPIDI = 0x0020000000000000ULL,
    /* tlbiva instruction                                                    */
    PPC_TLBIVA = 0x0040000000000000ULL,
    /* tlbivax instruction                                                   */
    PPC_TLBIVAX = 0x0080000000000000ULL,
    /* PowerPC 4xx dedicated instructions                                    */
    PPC_4xx_COMMON = 0x0100000000000000ULL,
    /* PowerPC 40x ibct instructions                                         */
    PPC_40x_ICBT = 0x0200000000000000ULL,
    /* rfmci is not implemented in all BookE PowerPC                         */
    PPC_RFMCI = 0x0400000000000000ULL,
    /* rfdi instruction                                                      */
    PPC_RFDI = 0x0800000000000000ULL,
    /* DCR accesses                                                          */
    PPC_DCR = 0x1000000000000000ULL,
    /* DCR extended accesse                                                  */
    PPC_DCRX = 0x2000000000000000ULL,
    /* user-mode DCR access, implemented in PowerPC 460                      */
    PPC_DCRUX = 0x4000000000000000ULL,
    /* popcntw and popcntd instructions                                      */
    PPC_POPCNTWD = 0x8000000000000000ULL,

#define PPC_TCG_INSNS                                                                                                      \
    (PPC_INSNS_BASE | PPC_POWER | PPC_POWER2 | PPC_POWER_RTC | PPC_POWER_BR | PPC_64B | PPC_64BX | PPC_64H | PPC_WAIT |    \
     PPC_MFTB | PPC_602_SPEC | PPC_ISEL | PPC_POPCNTB | PPC_STRING | PPC_FLOAT | PPC_FLOAT_EXT | PPC_FLOAT_FSQRT |         \
     PPC_FLOAT_FRES | PPC_FLOAT_FRSQRTE | PPC_FLOAT_FRSQRTES | PPC_FLOAT_FSEL | PPC_FLOAT_STFIWX | PPC_ALTIVEC | PPC_SPE | \
     PPC_SPE_SINGLE | PPC_SPE_DOUBLE | PPC_MEM_TLBIA | PPC_MEM_TLBIE | PPC_MEM_TLBSYNC | PPC_MEM_SYNC | PPC_MEM_EIEIO |    \
     PPC_CACHE | PPC_CACHE_ICBI | PPC_CACHE_DCBZ | PPC_CACHE_DCBZT | PPC_CACHE_DCBA | PPC_CACHE_LOCK | PPC_EXTERN |        \
     PPC_SEGMENT | PPC_6xx_TLB | PPC_74xx_TLB | PPC_40x_TLB | PPC_SEGMENT_64B | PPC_SLBI | PPC_WRTEE | PPC_40x_EXCP |      \
     PPC_405_MAC | PPC_440_SPEC | PPC_BOOKE | PPC_MFAPIDI | PPC_TLBIVA | PPC_TLBIVAX | PPC_4xx_COMMON | PPC_40x_ICBT |     \
     PPC_RFMCI | PPC_RFDI | PPC_DCR | PPC_DCRX | PPC_DCRUX | PPC_POPCNTWD)

    /* extended type values */

    /* BookE 2.06 PowerPC specification                                      */
    PPC2_BOOKE206 = 0x0000000000000001ULL,
    /* VSX (extensions to Altivec / VMX)                                     */
    PPC2_VSX = 0x0000000000000002ULL,
    /* Decimal Floating Point (DFP)                                          */
    PPC2_DFP = 0x0000000000000004ULL,

#define PPC_TCG_INSNS2 (PPC2_BOOKE206)
};

/*****************************************************************************/
/* Memory access type :
 * may be needed for precise access rights control and precise exceptions.
 */
enum {
    /* 1 bit to define user level / supervisor access */
    ACCESS_USER = 0x00,
    ACCESS_SUPER = 0x01,
    /* Type of instruction that generated the access */
    ACCESS_CODE = 0x10,  /* Code fetch access                */
    ACCESS_INT = 0x20,   /* Integer load/store access        */
    ACCESS_FLOAT = 0x30, /* floating point load/store access */
    ACCESS_RES = 0x40,   /* load/store with reservation      */
    ACCESS_EXT = 0x50,   /* external access                  */
    ACCESS_CACHE = 0x60, /* Cache manipulation               */
};

/* Hardware interruption sources:
 * all those exception can be raised simulteaneously
 */
/* Input pins definitions */
enum {
    /* 6xx bus input pins */
    PPC6xx_INPUT_HRESET = 0,
    PPC6xx_INPUT_SRESET = 1,
    PPC6xx_INPUT_CKSTP_IN = 2,
    PPC6xx_INPUT_MCP = 3,
    PPC6xx_INPUT_SMI = 4,
    PPC6xx_INPUT_INT = 5,
    PPC6xx_INPUT_TBEN = 6,
    PPC6xx_INPUT_WAKEUP = 7,
    PPC6xx_INPUT_NB,
};

enum {
    /* Embedded PowerPC input pins */
    PPCBookE_INPUT_HRESET = 0,
    PPCBookE_INPUT_SRESET = 1,
    PPCBookE_INPUT_CKSTP_IN = 2,
    PPCBookE_INPUT_MCP = 3,
    PPCBookE_INPUT_SMI = 4,
    PPCBookE_INPUT_INT = 5,
    PPCBookE_INPUT_CINT = 6,
    PPCBookE_INPUT_NB,
};

enum {
    /* PowerPC E500 input pins */
    PPCE500_INPUT_RESET_CORE = 0,
    PPCE500_INPUT_MCK = 1,
    PPCE500_INPUT_CINT = 3,
    PPCE500_INPUT_INT = 4,
    PPCE500_INPUT_DEBUG = 6,
    PPCE500_INPUT_NB,
};

enum {
    /* PowerPC 40x input pins */
    PPC40x_INPUT_RESET_CORE = 0,
    PPC40x_INPUT_RESET_CHIP = 1,
    PPC40x_INPUT_RESET_SYS = 2,
    PPC40x_INPUT_CINT = 3,
    PPC40x_INPUT_INT = 4,
    PPC40x_INPUT_HALT = 5,
    PPC40x_INPUT_DEBUG = 6,
    PPC40x_INPUT_NB,
};

enum {
    /* RCPU input pins */
    PPCRCPU_INPUT_PORESET = 0,
    PPCRCPU_INPUT_HRESET = 1,
    PPCRCPU_INPUT_SRESET = 2,
    PPCRCPU_INPUT_IRQ0 = 3,
    PPCRCPU_INPUT_IRQ1 = 4,
    PPCRCPU_INPUT_IRQ2 = 5,
    PPCRCPU_INPUT_IRQ3 = 6,
    PPCRCPU_INPUT_IRQ4 = 7,
    PPCRCPU_INPUT_IRQ5 = 8,
    PPCRCPU_INPUT_IRQ6 = 9,
    PPCRCPU_INPUT_IRQ7 = 10,
    PPCRCPU_INPUT_NB,
};

#if defined(TARGET_PPC64)
enum {
    /* PowerPC 970 input pins */
    PPC970_INPUT_HRESET = 0,
    PPC970_INPUT_SRESET = 1,
    PPC970_INPUT_CKSTP = 2,
    PPC970_INPUT_TBEN = 3,
    PPC970_INPUT_MCP = 4,
    PPC970_INPUT_INT = 5,
    PPC970_INPUT_THINT = 6,
    PPC970_INPUT_NB,
};

enum {
    /* POWER7 input pins */
    POWER7_INPUT_INT = 0,
    /* POWER7 probably has other inputs, but we don't care about them
     * for any existing machine.  We can wire these up when we need
     * them */
    POWER7_INPUT_NB,
};
#endif

/* Hardware exceptions definitions */
enum {
    /* External hardware exception sources */
    PPC_INTERRUPT_RESET = 0, /* Reset exception                      */
    PPC_INTERRUPT_WAKEUP,    /* Wakeup exception                     */
    PPC_INTERRUPT_MCK,       /* Machine check exception              */
    PPC_INTERRUPT_EXT,       /* External interrupt                   */
    PPC_INTERRUPT_SMI,       /* System management interrupt          */
    PPC_INTERRUPT_CEXT,      /* Critical external interrupt          */
    PPC_INTERRUPT_DEBUG,     /* External debug exception             */
    PPC_INTERRUPT_THERM,     /* Thermal exception                    */
    /* Internal hardware exception sources */
    PPC_INTERRUPT_DECR,      /* Decrementer exception                */
    PPC_INTERRUPT_HDECR,     /* Hypervisor decrementer exception     */
    PPC_INTERRUPT_PIT,       /* Programmable inteval timer interrupt */
    PPC_INTERRUPT_FIT,       /* Fixed interval timer interrupt       */
    PPC_INTERRUPT_WDT,       /* Watchdog timer interrupt             */
    PPC_INTERRUPT_CDOORBELL, /* Critical doorbell interrupt          */
    PPC_INTERRUPT_DOORBELL,  /* Doorbell interrupt                   */
    PPC_INTERRUPT_PERFM,     /* Performance monitor interrupt        */
};

/*****************************************************************************/

static inline void cpu_get_tb_cpu_state(CPUState *env, target_ulong *pc, target_ulong *cs_base, int *flags)
{
    *pc = env->nip;
    *cs_base = 0;
    *flags = env->hflags;
}

static inline int booke206_tlbm_id(CPUState *env, ppcmas_tlb_t *tlbm)
{
    uintptr_t tlbml = (uintptr_t)tlbm;
    uintptr_t tlbl = (uintptr_t)env->tlb.tlbm;

    return (tlbml - tlbl) / sizeof(env->tlb.tlbm[0]);
}

static inline int booke206_tlb_size(CPUState *env, int tlbn)
{
    uint32_t tlbncfg = env->spr[SPR_BOOKE_TLB0CFG + tlbn];
    int r = tlbncfg & TLBnCFG_N_ENTRY;
    return r;
}

static inline int booke206_tlb_ways(CPUState *env, int tlbn)
{
    uint32_t tlbncfg = env->spr[SPR_BOOKE_TLB0CFG + tlbn];
    int r = tlbncfg >> TLBnCFG_ASSOC_SHIFT;
    return r;
}

static inline int booke206_tlbm_to_tlbn(CPUState *env, ppcmas_tlb_t *tlbm)
{
    int id = booke206_tlbm_id(env, tlbm);
    int end = 0;
    int i;

    for(i = 0; i < BOOKE206_MAX_TLBN; i++) {
        end += booke206_tlb_size(env, i);
        if(id < end) {
            return i;
        }
    }

    cpu_abort(env, "Unknown TLBe: %d\n", id);
    return 0;
}

static inline int booke206_tlbm_to_way(CPUState *env, ppcmas_tlb_t *tlb)
{
    int tlbn = booke206_tlbm_to_tlbn(env, tlb);
    int tlbid = booke206_tlbm_id(env, tlb);
    return tlbid & (booke206_tlb_ways(env, tlbn) - 1);
}

static inline ppcmas_tlb_t *booke206_get_tlbm(CPUState *env, const int tlbn, target_ulong ea, int way)
{
    int r;
    uint32_t ways = booke206_tlb_ways(env, tlbn);
    int ways_bits = ffs(ways) - 1;
    int tlb_bits = ffs(booke206_tlb_size(env, tlbn)) - 1;
    int i;

    way &= ways - 1;
    ea >>= MAS2_EPN_SHIFT;
    ea &= (1 << (tlb_bits - ways_bits)) - 1;
    r = (ea << ways_bits) | way;

    /* bump up to tlbn index */
    for(i = 0; i < tlbn; i++) {
        r += booke206_tlb_size(env, i);
    }

    return &env->tlb.tlbm[r];
}

extern void (*cpu_ppc_hypercall)(CPUState *);

static inline bool cpu_has_work(CPUState *env)
{
    //  clear WFI if waking up condition is met
    env->wfi &= !(msr_ee && is_interrupt_pending(env, CPU_INTERRUPT_HARD));
    return !env->wfi;
}

#include "exec-all.h"

static inline void cpu_pc_from_tb(CPUState *env, TranslationBlock *tb)
{
    env->nip = tb->pc;
}

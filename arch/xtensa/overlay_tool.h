/*
 * Copyright (c) 2011, Max Filippov, Open Source and Linux Lab.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Open Source and Linux Lab nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define XTREG(idx, ofs, bi, sz, al, no, fl, cp, typ, grp, name, a1, a2, a3, a4, a5, a6) \
    {                                                                                   \
        .targno = (no),                                                                 \
        .flags = (fl),                                                                  \
        .type = (typ),                                                                  \
        .group = (grp),                                                                 \
        .size = (sz),                                                                   \
    },
#define XTREG_END { .targno = -1 },

#ifndef XCHAL_HAVE_DEPBITS
#define XCHAL_HAVE_DEPBITS 0
#endif

#ifndef XCHAL_HAVE_DFP
#define XCHAL_HAVE_DFP 0
#endif

#ifndef XCHAL_HAVE_DFPU_SINGLE_ONLY
#define XCHAL_HAVE_DFPU_SINGLE_ONLY 0
#endif

#ifndef XCHAL_HAVE_DFPU_SINGLE_DOUBLE
#define XCHAL_HAVE_DFPU_SINGLE_DOUBLE XCHAL_HAVE_DFP
#endif

/*
 * We need to know the type of FP unit, not only its precision.
 * Unfortunately XCHAL macros don't tell this explicitly.
 */
#define XCHAL_HAVE_DFPU (XCHAL_HAVE_DFP || XCHAL_HAVE_DFPU_SINGLE_ONLY || XCHAL_HAVE_DFPU_SINGLE_DOUBLE)

#ifndef XCHAL_HAVE_DIV32
#define XCHAL_HAVE_DIV32 0
#endif

#ifndef XCHAL_UNALIGNED_LOAD_HW
#define XCHAL_UNALIGNED_LOAD_HW 0
#endif

#ifndef XCHAL_HAVE_VECBASE
#define XCHAL_HAVE_VECBASE        0
#define XCHAL_VECBASE_RESET_VADDR 0
#endif

#ifndef XCHAL_RESET_VECTOR0_VADDR
#define XCHAL_RESET_VECTOR0_VADDR XCHAL_RESET_VECTOR_VADDR
#endif

#ifndef XCHAL_RESET_VECTOR1_VADDR
#define XCHAL_RESET_VECTOR1_VADDR XCHAL_RESET_VECTOR_VADDR
#endif

#ifndef XCHAL_HW_VERSION
#define XCHAL_HW_VERSION (XCHAL_HW_VERSION_MAJOR * 100 + XCHAL_HW_VERSION_MINOR)
#endif

#ifndef XCHAL_LOOP_BUFFER_SIZE
#define XCHAL_LOOP_BUFFER_SIZE 0
#endif

#ifndef XCHAL_HAVE_EXTERN_REGS
#define XCHAL_HAVE_EXTERN_REGS 0
#endif

#ifndef XCHAL_HAVE_MPU
#define XCHAL_HAVE_MPU 0
#endif

#ifndef XCHAL_HAVE_EXCLUSIVE
#define XCHAL_HAVE_EXCLUSIVE 0
#endif

#define XCHAL_OPTION(xchal, qemu) ((xchal) ? XTENSA_OPTION_BIT(qemu) : 0)

#define XTENSA_OPTIONS                                                                                                           \
    (XCHAL_OPTION(XCHAL_HAVE_DENSITY, XTENSA_OPTION_CODE_DENSITY) | XCHAL_OPTION(XCHAL_HAVE_LOOPS, XTENSA_OPTION_LOOP) |         \
     XCHAL_OPTION(XCHAL_HAVE_ABSOLUTE_LITERALS, XTENSA_OPTION_EXTENDED_L32R) |                                                   \
     XCHAL_OPTION(XCHAL_HAVE_MUL16, XTENSA_OPTION_16_BIT_IMUL) | XCHAL_OPTION(XCHAL_HAVE_MUL32, XTENSA_OPTION_32_BIT_IMUL) |     \
     XCHAL_OPTION(XCHAL_HAVE_MUL32_HIGH, XTENSA_OPTION_32_BIT_IMUL_HIGH) |                                                       \
     XCHAL_OPTION(XCHAL_HAVE_DIV32, XTENSA_OPTION_32_BIT_IDIV) | XCHAL_OPTION(XCHAL_HAVE_MAC16, XTENSA_OPTION_MAC16) |           \
     XCHAL_OPTION(XCHAL_HAVE_NSA, XTENSA_OPTION_MISC_OP_NSA) | XCHAL_OPTION(XCHAL_HAVE_MINMAX, XTENSA_OPTION_MISC_OP_MINMAX) |   \
     XCHAL_OPTION(XCHAL_HAVE_SEXT, XTENSA_OPTION_MISC_OP_SEXT) | XCHAL_OPTION(XCHAL_HAVE_CLAMPS, XTENSA_OPTION_MISC_OP_CLAMPS) | \
     XCHAL_OPTION(XCHAL_HAVE_CP, XTENSA_OPTION_COPROCESSOR) | XCHAL_OPTION(XCHAL_HAVE_BOOLEANS, XTENSA_OPTION_BOOLEAN) |         \
     XCHAL_OPTION(XCHAL_HAVE_FP, XTENSA_OPTION_FP_COPROCESSOR) | XCHAL_OPTION(XCHAL_HAVE_DFPU, XTENSA_OPTION_DFP_COPROCESSOR) |  \
     XCHAL_OPTION(XCHAL_HAVE_DFPU_SINGLE_ONLY, XTENSA_OPTION_DFPU_SINGLE_ONLY) |                                                 \
     XCHAL_OPTION(XCHAL_HAVE_RELEASE_SYNC, XTENSA_OPTION_MP_SYNCHRO) |                                                           \
     XCHAL_OPTION(XCHAL_HAVE_S32C1I, XTENSA_OPTION_CONDITIONAL_STORE) |                                                          \
     XCHAL_OPTION(((XCHAL_HAVE_S32C1I && XCHAL_HW_VERSION >= 230000) || XCHAL_HAVE_EXCLUSIVE), XTENSA_OPTION_ATOMCTL) |          \
     XCHAL_OPTION(XCHAL_HAVE_DEPBITS, XTENSA_OPTION_DEPBITS) | /* Interrupts and exceptions */                                   \
     XCHAL_OPTION(XCHAL_HAVE_EXCEPTIONS, XTENSA_OPTION_EXCEPTION) |                                                              \
     XCHAL_OPTION(XCHAL_HAVE_VECBASE, XTENSA_OPTION_RELOCATABLE_VECTOR) |                                                        \
     XCHAL_OPTION(XCHAL_UNALIGNED_LOAD_EXCEPTION, XTENSA_OPTION_UNALIGNED_EXCEPTION) |                                           \
     XCHAL_OPTION(XCHAL_HAVE_INTERRUPTS, XTENSA_OPTION_INTERRUPT) |                                                              \
     XCHAL_OPTION(XCHAL_HAVE_HIGHPRI_INTERRUPTS, XTENSA_OPTION_HIGH_PRIORITY_INTERRUPT) |                                        \
     XCHAL_OPTION(XCHAL_HAVE_CCOUNT, XTENSA_OPTION_TIMER_INTERRUPT) | /* Local memory, TODO */                                   \
     XCHAL_OPTION(XCHAL_ICACHE_SIZE, XTENSA_OPTION_ICACHE) |                                                                     \
     XCHAL_OPTION(XCHAL_ICACHE_LINE_LOCKABLE, XTENSA_OPTION_ICACHE_INDEX_LOCK) |                                                 \
     XCHAL_OPTION(XCHAL_DCACHE_SIZE, XTENSA_OPTION_DCACHE) |                                                                     \
     XCHAL_OPTION(XCHAL_DCACHE_LINE_LOCKABLE, XTENSA_OPTION_DCACHE_INDEX_LOCK) |                                                 \
     XCHAL_OPTION(XCHAL_UNALIGNED_LOAD_HW, XTENSA_OPTION_HW_ALIGNMENT) |                                                         \
     XCHAL_OPTION(XCHAL_HAVE_MEM_ECC_PARITY, XTENSA_OPTION_MEMORY_ECC_PARITY) |                                                  \
     XCHAL_OPTION(XCHAL_HAVE_PREFETCH, XTENSA_OPTION_PREFETCH) | /* Memory protection and translation */                         \
     XCHAL_OPTION(XCHAL_HAVE_MIMIC_CACHEATTR, XTENSA_OPTION_REGION_PROTECTION) |                                                 \
     XCHAL_OPTION(XCHAL_HAVE_XLT_CACHEATTR, XTENSA_OPTION_REGION_TRANSLATION) |                                                  \
     XCHAL_OPTION(XCHAL_HAVE_MPU, XTENSA_OPTION_MPU) | XCHAL_OPTION(XCHAL_HAVE_PTP_MMU, XTENSA_OPTION_MMU) |                     \
     XCHAL_OPTION(XCHAL_HAVE_CACHEATTR, XTENSA_OPTION_CACHEATTR) | /* Other, TODO */                                             \
     XCHAL_OPTION(XCHAL_HAVE_WINDOWED, XTENSA_OPTION_WINDOWED_REGISTER) | XCHAL_OPTION(XCHAL_HAVE_DEBUG, XTENSA_OPTION_DEBUG) |  \
     XCHAL_OPTION(XCHAL_NUM_MISC_REGS > 0, XTENSA_OPTION_MISC_SR) |                                                              \
     XCHAL_OPTION(XCHAL_HAVE_THREADPTR, XTENSA_OPTION_THREAD_POINTER) |                                                          \
     XCHAL_OPTION(XCHAL_HAVE_PRID, XTENSA_OPTION_PROCESSOR_ID) |                                                                 \
     XCHAL_OPTION(XCHAL_HAVE_EXTERN_REGS, XTENSA_OPTION_EXTERN_REGS))

#ifndef XCHAL_WINDOW_OF4_VECOFS
#define XCHAL_WINDOW_OF4_VECOFS  0x00000000
#define XCHAL_WINDOW_UF4_VECOFS  0x00000040
#define XCHAL_WINDOW_OF8_VECOFS  0x00000080
#define XCHAL_WINDOW_UF8_VECOFS  0x000000C0
#define XCHAL_WINDOW_OF12_VECOFS 0x00000100
#define XCHAL_WINDOW_UF12_VECOFS 0x00000140
#endif

#if XCHAL_HAVE_WINDOWED
#define WINDOW_VECTORS                                                               \
    [EXC_WINDOW_OVERFLOW4] = XCHAL_WINDOW_OF4_VECOFS + XCHAL_WINDOW_VECTORS_VADDR,   \
    [EXC_WINDOW_UNDERFLOW4] = XCHAL_WINDOW_UF4_VECOFS + XCHAL_WINDOW_VECTORS_VADDR,  \
    [EXC_WINDOW_OVERFLOW8] = XCHAL_WINDOW_OF8_VECOFS + XCHAL_WINDOW_VECTORS_VADDR,   \
    [EXC_WINDOW_UNDERFLOW8] = XCHAL_WINDOW_UF8_VECOFS + XCHAL_WINDOW_VECTORS_VADDR,  \
    [EXC_WINDOW_OVERFLOW12] = XCHAL_WINDOW_OF12_VECOFS + XCHAL_WINDOW_VECTORS_VADDR, \
    [EXC_WINDOW_UNDERFLOW12] = XCHAL_WINDOW_UF12_VECOFS + XCHAL_WINDOW_VECTORS_VADDR,
#else
#define WINDOW_VECTORS
#endif

#define EXCEPTION_VECTORS                                       \
    {                                                           \
        [EXC_RESET0] = XCHAL_RESET_VECTOR0_VADDR,               \
        [EXC_RESET1] = XCHAL_RESET_VECTOR1_VADDR,               \
        WINDOW_VECTORS[EXC_KERNEL] = XCHAL_KERNEL_VECTOR_VADDR, \
        [EXC_USER] = XCHAL_USER_VECTOR_VADDR,                   \
        [EXC_DOUBLE] = XCHAL_DOUBLEEXC_VECTOR_VADDR,            \
        [EXC_DEBUG] = XCHAL_DEBUG_VECTOR_VADDR,                 \
    }

#define INTERRUPT_VECTORS             \
    {                                 \
        0,                            \
        0,                            \
        XCHAL_INTLEVEL2_VECTOR_VADDR, \
        XCHAL_INTLEVEL3_VECTOR_VADDR, \
        XCHAL_INTLEVEL4_VECTOR_VADDR, \
        XCHAL_INTLEVEL5_VECTOR_VADDR, \
        XCHAL_INTLEVEL6_VECTOR_VADDR, \
        XCHAL_INTLEVEL7_VECTOR_VADDR, \
    }

#define LEVEL_MASKS                                                                                                     \
    {                                                                                                                   \
        [1] = XCHAL_INTLEVEL1_MASK, [2] = XCHAL_INTLEVEL2_MASK, [3] = XCHAL_INTLEVEL3_MASK, [4] = XCHAL_INTLEVEL4_MASK, \
        [5] = XCHAL_INTLEVEL5_MASK, [6] = XCHAL_INTLEVEL6_MASK, [7] = XCHAL_INTLEVEL7_MASK,                             \
    }

#define INTTYPE_MASKS                                     \
    {                                                     \
        [INTTYPE_EDGE] = XCHAL_INTTYPE_MASK_EXTERN_EDGE,  \
        [INTTYPE_NMI] = XCHAL_INTTYPE_MASK_NMI,           \
        [INTTYPE_SOFTWARE] = XCHAL_INTTYPE_MASK_SOFTWARE, \
    }

#define XTHAL_INTTYPE_EXTERN_LEVEL INTTYPE_LEVEL
#define XTHAL_INTTYPE_EXTERN_EDGE  INTTYPE_EDGE
#define XTHAL_INTTYPE_NMI          INTTYPE_NMI
#define XTHAL_INTTYPE_SOFTWARE     INTTYPE_SOFTWARE
#define XTHAL_INTTYPE_TIMER        INTTYPE_TIMER
#define XTHAL_INTTYPE_TBD1         INTTYPE_DEBUG
#define XTHAL_INTTYPE_TBD2         INTTYPE_WRITE_ERR
#define XTHAL_INTTYPE_WRITE_ERROR  INTTYPE_WRITE_ERR
#define XTHAL_INTTYPE_PROFILING    INTTYPE_PROFILING
#define XTHAL_INTTYPE_IDMA_DONE    INTTYPE_IDMA_DONE
#define XTHAL_INTTYPE_IDMA_ERR     INTTYPE_IDMA_ERR
#define XTHAL_INTTYPE_GS_ERR       INTTYPE_GS_ERR

#ifndef XCHAL_NMILEVEL
#define XCHAL_NMILEVEL (XCHAL_NUM_INTLEVELS + 1)
#endif

#define INTERRUPT(i)                    \
    {                                   \
        .level = XCHAL_INT##i##_LEVEL,  \
        .inttype = XCHAL_INT##i##_TYPE, \
    }

#define INTERRUPTS                                                                                                    \
    {                                                                                                                 \
        [0] = INTERRUPT(0),   [1] = INTERRUPT(1),   [2] = INTERRUPT(2),   [3] = INTERRUPT(3),   [4] = INTERRUPT(4),   \
        [5] = INTERRUPT(5),   [6] = INTERRUPT(6),   [7] = INTERRUPT(7),   [8] = INTERRUPT(8),   [9] = INTERRUPT(9),   \
        [10] = INTERRUPT(10), [11] = INTERRUPT(11), [12] = INTERRUPT(12), [13] = INTERRUPT(13), [14] = INTERRUPT(14), \
        [15] = INTERRUPT(15), [16] = INTERRUPT(16), [17] = INTERRUPT(17), [18] = INTERRUPT(18), [19] = INTERRUPT(19), \
        [20] = INTERRUPT(20), [21] = INTERRUPT(21), [22] = INTERRUPT(22), [23] = INTERRUPT(23), [24] = INTERRUPT(24), \
        [25] = INTERRUPT(25), [26] = INTERRUPT(26), [27] = INTERRUPT(27), [28] = INTERRUPT(28), [29] = INTERRUPT(29), \
        [30] = INTERRUPT(30), [31] = INTERRUPT(31),                                                                   \
    }

#define TIMERINTS                     \
    {                                 \
        [0] = XCHAL_TIMER0_INTERRUPT, \
        [1] = XCHAL_TIMER1_INTERRUPT, \
        [2] = XCHAL_TIMER2_INTERRUPT, \
    }

#define EXTINTS                                                                                                     \
    {                                                                                                               \
        [0] = XCHAL_EXTINT0_NUM,   [1] = XCHAL_EXTINT1_NUM,   [2] = XCHAL_EXTINT2_NUM,   [3] = XCHAL_EXTINT3_NUM,   \
        [4] = XCHAL_EXTINT4_NUM,   [5] = XCHAL_EXTINT5_NUM,   [6] = XCHAL_EXTINT6_NUM,   [7] = XCHAL_EXTINT7_NUM,   \
        [8] = XCHAL_EXTINT8_NUM,   [9] = XCHAL_EXTINT9_NUM,   [10] = XCHAL_EXTINT10_NUM, [11] = XCHAL_EXTINT11_NUM, \
        [12] = XCHAL_EXTINT12_NUM, [13] = XCHAL_EXTINT13_NUM, [14] = XCHAL_EXTINT14_NUM, [15] = XCHAL_EXTINT15_NUM, \
        [16] = XCHAL_EXTINT16_NUM, [17] = XCHAL_EXTINT17_NUM, [18] = XCHAL_EXTINT18_NUM, [19] = XCHAL_EXTINT19_NUM, \
        [20] = XCHAL_EXTINT20_NUM, [21] = XCHAL_EXTINT21_NUM, [22] = XCHAL_EXTINT22_NUM, [23] = XCHAL_EXTINT23_NUM, \
        [24] = XCHAL_EXTINT24_NUM, [25] = XCHAL_EXTINT25_NUM, [26] = XCHAL_EXTINT26_NUM, [27] = XCHAL_EXTINT27_NUM, \
        [28] = XCHAL_EXTINT28_NUM, [29] = XCHAL_EXTINT29_NUM, [30] = XCHAL_EXTINT30_NUM, [31] = XCHAL_EXTINT31_NUM, \
    }

#define EXCEPTIONS_SECTION \
    .excm_level = XCHAL_EXCM_LEVEL, .vecbase = XCHAL_VECBASE_RESET_VADDR, .exception_vector = EXCEPTION_VECTORS

#define INTERRUPTS_SECTION                                                                                                    \
    .ninterrupt = XCHAL_NUM_INTERRUPTS, .nlevel = XCHAL_NUM_INTLEVELS + XCHAL_HAVE_NMI, .nmi_level = XCHAL_NMILEVEL,          \
    .interrupt_vector = INTERRUPT_VECTORS, .level_mask = LEVEL_MASKS, .inttype_mask = INTTYPE_MASKS, .interrupt = INTERRUPTS, \
    .nccompare = XCHAL_NUM_TIMERS, .timerint = TIMERINTS, .nextint = XCHAL_NUM_EXTINTERRUPTS, .extint = EXTINTS

#if XCHAL_HAVE_PTP_MMU

#define TLB_TEMPLATE(ways, refill_way_size, way56) \
    { \
        .nways = ways, \
        .way_size = { \
            (refill_way_size), (refill_way_size), \
            (refill_way_size), (refill_way_size), \
            4, (way56) ? 4 : 2, (way56) ? 8 : 2, 1, 1, 1, \
        }, \
        .varway56 = (way56), \
        .nrefillentries = (refill_way_size) * 4, \
    }

#define ITLB(varway56) TLB_TEMPLATE(7, 1 << XCHAL_ITLB_ARF_ENTRIES_LOG2, varway56)

#define DTLB(varway56) TLB_TEMPLATE(10, 1 << XCHAL_DTLB_ARF_ENTRIES_LOG2, varway56)

#define TLB_SECTION .itlb = ITLB(XCHAL_HAVE_SPANNING_WAY), .dtlb = DTLB(XCHAL_HAVE_SPANNING_WAY)

#ifndef XCHAL_SYSROM0_PADDR
#define XCHAL_SYSROM0_PADDR 0xfe000000
#define XCHAL_SYSROM0_SIZE  0x02000000
#endif

#ifndef XCHAL_SYSRAM0_PADDR
#define XCHAL_SYSRAM0_PADDR 0x00000000
#define XCHAL_SYSRAM0_SIZE  0x08000000
#endif

#elif XCHAL_HAVE_XLT_CACHEATTR || XCHAL_HAVE_MIMIC_CACHEATTR

#define TLB_TEMPLATE              \
    {                             \
        .nways = 1, .way_size = { \
            8,                    \
        }                         \
    }

#define TLB_SECTION .itlb = TLB_TEMPLATE, .dtlb = TLB_TEMPLATE

#ifndef XCHAL_SYSROM0_PADDR
#define XCHAL_SYSROM0_PADDR 0x50000000
#define XCHAL_SYSROM0_SIZE  0x04000000
#endif

#ifndef XCHAL_SYSRAM0_PADDR
#define XCHAL_SYSRAM0_PADDR 0x60000000
#define XCHAL_SYSRAM0_SIZE  0x04000000
#endif

#elif XCHAL_HAVE_MPU

#ifndef XTENSA_MPU_BG_MAP
#ifdef XCHAL_MPU_BACKGROUND_MAP
#define XCHAL_MPU_BGMAP(s, vaddr_start, vaddr_last, rights, memtype, x...) \
    {                                                                      \
        .vaddr = (vaddr_start),                                            \
        .attr = ((rights) << 8) | ((memtype) << 12),                       \
    },

#define XTENSA_MPU_BG_MAP           \
    (xtensa_mpu_entry[])            \
    {                               \
        XCHAL_MPU_BACKGROUND_MAP(0) \
    }

#define XTENSA_MPU_BG_MAP_ENTRIES XCHAL_MPU_BACKGROUND_ENTRIES
#else
#define XTENSA_MPU_BG_MAP       \
    (xtensa_mpu_entry[])        \
    {                           \
        {                       \
            .vaddr = 0,         \
            .attr = 0x00006700, \
        },                      \
    }

#define XTENSA_MPU_BG_MAP_ENTRIES 1
#endif
#endif

#define TLB_SECTION                                                                                                       \
    .mpu_align = XCHAL_MPU_ALIGN, .n_mpu_fg_segments = XCHAL_MPU_ENTRIES, .n_mpu_bg_segments = XTENSA_MPU_BG_MAP_ENTRIES, \
    .mpu_bg = XTENSA_MPU_BG_MAP

#ifndef XCHAL_SYSROM0_PADDR
#define XCHAL_SYSROM0_PADDR 0x50000000
#define XCHAL_SYSROM0_SIZE  0x04000000
#endif

#ifndef XCHAL_SYSRAM0_PADDR
#define XCHAL_SYSRAM0_PADDR 0x60000000
#define XCHAL_SYSRAM0_SIZE  0x04000000
#endif

#else

#ifndef XCHAL_SYSROM0_PADDR
#define XCHAL_SYSROM0_PADDR 0x50000000
#define XCHAL_SYSROM0_SIZE  0x04000000
#endif

#ifndef XCHAL_SYSRAM0_PADDR
#define XCHAL_SYSRAM0_PADDR 0x60000000
#define XCHAL_SYSRAM0_SIZE  0x04000000
#endif

#endif

#define DEBUG_SECTION .debug_level = XCHAL_DEBUGLEVEL, .nibreak = XCHAL_NUM_IBREAK, .ndbreak = XCHAL_NUM_DBREAK

#define CACHE_SECTION                                                                                                    \
    .icache_ways = XCHAL_ICACHE_WAYS, .dcache_ways = XCHAL_DCACHE_WAYS, .dcache_line_bytes = XCHAL_DCACHE_LINESIZE,      \
    .memctl_mask = (XCHAL_ICACHE_SIZE ? MEMCTL_IUSEWAYS_MASK : 0) |                                                      \
                   (XCHAL_DCACHE_SIZE ? MEMCTL_DALLOCWAYS_MASK | MEMCTL_DUSEWAYS_MASK : 0) | MEMCTL_ISNP | MEMCTL_DSNP | \
                   (XCHAL_HAVE_LOOPS && XCHAL_LOOP_BUFFER_SIZE ? MEMCTL_IL0EN : 0)

#define MEM_LOCATION(name, n)            \
    {                                    \
        .addr = XCHAL_##name##n##_PADDR, \
        .size = XCHAL_##name##n##_SIZE,  \
    }

#define MEM_SECTIONS(name) MEM_LOCATION(name, 0), MEM_LOCATION(name, 1), MEM_LOCATION(name, 2), MEM_LOCATION(name, 3)

#define MEM_SECTION(name) .num = XCHAL_NUM_##name, .location = { MEM_SECTIONS(name) }

#define SYSMEM_SECTION(name)               \
    .num = 1, .location = {                \
        {                                  \
            .addr = XCHAL_##name##0_PADDR, \
            .size = XCHAL_##name##0_SIZE,  \
        }                                  \
    }

#define LOCAL_MEMORIES_SECTION                                                                                     \
    .instrom = { MEM_SECTION(INSTROM) }, .instram = { MEM_SECTION(INSTRAM) }, .datarom = { MEM_SECTION(DATAROM) }, \
    .dataram = { MEM_SECTION(DATARAM) }, .sysrom = { SYSMEM_SECTION(SYSROM) }, .sysram = { SYSMEM_SECTION(SYSRAM) }

#define CONFIG_SECTION                            \
    .hw_version = XCHAL_HW_VERSION, .configid = { \
        XCHAL_HW_CONFIGID0,                       \
        XCHAL_HW_CONFIGID1,                       \
    }

#define DEFAULT_SECTIONS                                                                                                        \
    .options = XTENSA_OPTIONS, .nareg = XCHAL_NUM_AREGS, .ndepc = (XCHAL_XEA_VERSION >= 2),                                     \
    .inst_fetch_width = XCHAL_INST_FETCH_WIDTH, .max_insn_size = XCHAL_MAX_INSTRUCTION_SIZE, .use_first_nan = !XCHAL_HAVE_DFPU, \
    EXCEPTIONS_SECTION, INTERRUPTS_SECTION, TLB_SECTION, DEBUG_SECTION, CACHE_SECTION, LOCAL_MEMORIES_SECTION, CONFIG_SECTION

#if XCHAL_NUM_INTLEVELS + XCHAL_HAVE_NMI + 1 <= 2
#define XCHAL_INTLEVEL2_VECTOR_VADDR 0
#endif
#if XCHAL_NUM_INTLEVELS + XCHAL_HAVE_NMI + 1 <= 3
#define XCHAL_INTLEVEL3_VECTOR_VADDR 0
#endif
#if XCHAL_NUM_INTLEVELS + XCHAL_HAVE_NMI + 1 <= 4
#define XCHAL_INTLEVEL4_VECTOR_VADDR 0
#endif
#if XCHAL_NUM_INTLEVELS + XCHAL_HAVE_NMI + 1 <= 5
#define XCHAL_INTLEVEL5_VECTOR_VADDR 0
#endif
#if XCHAL_NUM_INTLEVELS + XCHAL_HAVE_NMI + 1 <= 6
#define XCHAL_INTLEVEL6_VECTOR_VADDR 0
#endif
#if XCHAL_NUM_INTLEVELS + XCHAL_HAVE_NMI + 1 <= 7
#define XCHAL_INTLEVEL7_VECTOR_VADDR 0
#endif

#if XCHAL_NUM_INTERRUPTS <= 0
#define XCHAL_INT0_LEVEL 0
#define XCHAL_INT0_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 1
#define XCHAL_INT1_LEVEL 0
#define XCHAL_INT1_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 2
#define XCHAL_INT2_LEVEL 0
#define XCHAL_INT2_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 3
#define XCHAL_INT3_LEVEL 0
#define XCHAL_INT3_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 4
#define XCHAL_INT4_LEVEL 0
#define XCHAL_INT4_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 5
#define XCHAL_INT5_LEVEL 0
#define XCHAL_INT5_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 6
#define XCHAL_INT6_LEVEL 0
#define XCHAL_INT6_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 7
#define XCHAL_INT7_LEVEL 0
#define XCHAL_INT7_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 8
#define XCHAL_INT8_LEVEL 0
#define XCHAL_INT8_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 9
#define XCHAL_INT9_LEVEL 0
#define XCHAL_INT9_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 10
#define XCHAL_INT10_LEVEL 0
#define XCHAL_INT10_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 11
#define XCHAL_INT11_LEVEL 0
#define XCHAL_INT11_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 12
#define XCHAL_INT12_LEVEL 0
#define XCHAL_INT12_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 13
#define XCHAL_INT13_LEVEL 0
#define XCHAL_INT13_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 14
#define XCHAL_INT14_LEVEL 0
#define XCHAL_INT14_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 15
#define XCHAL_INT15_LEVEL 0
#define XCHAL_INT15_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 16
#define XCHAL_INT16_LEVEL 0
#define XCHAL_INT16_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 17
#define XCHAL_INT17_LEVEL 0
#define XCHAL_INT17_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 18
#define XCHAL_INT18_LEVEL 0
#define XCHAL_INT18_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 19
#define XCHAL_INT19_LEVEL 0
#define XCHAL_INT19_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 20
#define XCHAL_INT20_LEVEL 0
#define XCHAL_INT20_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 21
#define XCHAL_INT21_LEVEL 0
#define XCHAL_INT21_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 22
#define XCHAL_INT22_LEVEL 0
#define XCHAL_INT22_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 23
#define XCHAL_INT23_LEVEL 0
#define XCHAL_INT23_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 24
#define XCHAL_INT24_LEVEL 0
#define XCHAL_INT24_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 25
#define XCHAL_INT25_LEVEL 0
#define XCHAL_INT25_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 26
#define XCHAL_INT26_LEVEL 0
#define XCHAL_INT26_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 27
#define XCHAL_INT27_LEVEL 0
#define XCHAL_INT27_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 28
#define XCHAL_INT28_LEVEL 0
#define XCHAL_INT28_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 29
#define XCHAL_INT29_LEVEL 0
#define XCHAL_INT29_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 30
#define XCHAL_INT30_LEVEL 0
#define XCHAL_INT30_TYPE  0
#endif
#if XCHAL_NUM_INTERRUPTS <= 31
#define XCHAL_INT31_LEVEL 0
#define XCHAL_INT31_TYPE  0
#endif

#if XCHAL_NUM_EXTINTERRUPTS <= 0
#define XCHAL_EXTINT0_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 1
#define XCHAL_EXTINT1_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 2
#define XCHAL_EXTINT2_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 3
#define XCHAL_EXTINT3_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 4
#define XCHAL_EXTINT4_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 5
#define XCHAL_EXTINT5_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 6
#define XCHAL_EXTINT6_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 7
#define XCHAL_EXTINT7_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 8
#define XCHAL_EXTINT8_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 9
#define XCHAL_EXTINT9_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 10
#define XCHAL_EXTINT10_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 11
#define XCHAL_EXTINT11_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 12
#define XCHAL_EXTINT12_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 13
#define XCHAL_EXTINT13_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 14
#define XCHAL_EXTINT14_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 15
#define XCHAL_EXTINT15_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 16
#define XCHAL_EXTINT16_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 17
#define XCHAL_EXTINT17_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 18
#define XCHAL_EXTINT18_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 19
#define XCHAL_EXTINT19_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 20
#define XCHAL_EXTINT20_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 21
#define XCHAL_EXTINT21_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 22
#define XCHAL_EXTINT22_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 23
#define XCHAL_EXTINT23_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 24
#define XCHAL_EXTINT24_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 25
#define XCHAL_EXTINT25_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 26
#define XCHAL_EXTINT26_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 27
#define XCHAL_EXTINT27_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 28
#define XCHAL_EXTINT28_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 29
#define XCHAL_EXTINT29_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 30
#define XCHAL_EXTINT30_NUM 0
#endif
#if XCHAL_NUM_EXTINTERRUPTS <= 31
#define XCHAL_EXTINT31_NUM 0
#endif

#define XTHAL_TIMER_UNCONFIGURED 0

#if XCHAL_NUM_INSTROM < 1
#define XCHAL_INSTROM0_PADDR 0
#define XCHAL_INSTROM0_SIZE  0
#endif
#if XCHAL_NUM_INSTROM < 2
#define XCHAL_INSTROM1_PADDR 0
#define XCHAL_INSTROM1_SIZE  0
#endif
#if XCHAL_NUM_INSTROM < 3
#define XCHAL_INSTROM2_PADDR 0
#define XCHAL_INSTROM2_SIZE  0
#endif
#if XCHAL_NUM_INSTROM < 4
#define XCHAL_INSTROM3_PADDR 0
#define XCHAL_INSTROM3_SIZE  0
#endif
#if XCHAL_NUM_INSTROM > MAX_NMEMORY
#error XCHAL_NUM_INSTROM > MAX_NMEMORY
#endif

#if XCHAL_NUM_INSTRAM < 1
#define XCHAL_INSTRAM0_PADDR 0
#define XCHAL_INSTRAM0_SIZE  0
#endif
#if XCHAL_NUM_INSTRAM < 2
#define XCHAL_INSTRAM1_PADDR 0
#define XCHAL_INSTRAM1_SIZE  0
#endif
#if XCHAL_NUM_INSTRAM < 3
#define XCHAL_INSTRAM2_PADDR 0
#define XCHAL_INSTRAM2_SIZE  0
#endif
#if XCHAL_NUM_INSTRAM < 4
#define XCHAL_INSTRAM3_PADDR 0
#define XCHAL_INSTRAM3_SIZE  0
#endif
#if XCHAL_NUM_INSTRAM > MAX_NMEMORY
#error XCHAL_NUM_INSTRAM > MAX_NMEMORY
#endif

#if XCHAL_NUM_DATAROM < 1
#define XCHAL_DATAROM0_PADDR 0
#define XCHAL_DATAROM0_SIZE  0
#endif
#if XCHAL_NUM_DATAROM < 2
#define XCHAL_DATAROM1_PADDR 0
#define XCHAL_DATAROM1_SIZE  0
#endif
#if XCHAL_NUM_DATAROM < 3
#define XCHAL_DATAROM2_PADDR 0
#define XCHAL_DATAROM2_SIZE  0
#endif
#if XCHAL_NUM_DATAROM < 4
#define XCHAL_DATAROM3_PADDR 0
#define XCHAL_DATAROM3_SIZE  0
#endif
#if XCHAL_NUM_DATAROM > MAX_NMEMORY
#error XCHAL_NUM_DATAROM > MAX_NMEMORY
#endif

#if XCHAL_NUM_DATARAM < 1
#define XCHAL_DATARAM0_PADDR 0
#define XCHAL_DATARAM0_SIZE  0
#endif
#if XCHAL_NUM_DATARAM < 2
#define XCHAL_DATARAM1_PADDR 0
#define XCHAL_DATARAM1_SIZE  0
#endif
#if XCHAL_NUM_DATARAM < 3
#define XCHAL_DATARAM2_PADDR 0
#define XCHAL_DATARAM2_SIZE  0
#endif
#if XCHAL_NUM_DATARAM < 4
#define XCHAL_DATARAM3_PADDR 0
#define XCHAL_DATARAM3_SIZE  0
#endif
#if XCHAL_NUM_DATARAM > MAX_NMEMORY
#error XCHAL_NUM_DATARAM > MAX_NMEMORY
#endif

#if !defined (__RISCV_CPU_H__)
#define __RISCV_CPU_H__

#include "cpu-defs.h"
#include "softfloat.h"
#include "host-utils.h"

// This could possibly be generalized. 0 and 1 values are used as "is_write". This conflicts in a way with READ_ACCESS_TYPE et al.
#define MMU_DATA_LOAD               0
#define MMU_DATA_STORE              1
#define MMU_INST_FETCH              2

#define TARGET_PAGE_BITS            12/* 4 KiB Pages */
#if TARGET_LONG_BITS == 64
#define TARGET_RISCV64
#define TARGET_PHYS_ADDR_SPACE_BITS 50
#define TARGET_VIRT_ADDR_SPACE_BITS 39
#elif TARGET_LONG_BITS == 32
#define TARGET_RISCV32
#define TARGET_PHYS_ADDR_SPACE_BITS 34
#define TARGET_VIRT_ADDR_SPACE_BITS 32
#else
#error "Target arch can be only 32-bit or 64-bit."
#endif

#include "cpu_bits.h"

#define RV(x) ((target_ulong)1 << (x - 'A'))

#define TRANSLATE_FAIL    1
#define TRANSLATE_SUCCESS 0
#define NB_MMU_MODES      4

#define MAX_RISCV_PMPS    (16)

#define get_field(reg, mask) (((reg) & (target_ulong)(mask)) / ((mask) & ~((mask) << 1)))
#define set_field(reg, mask, val) \
                             (((reg) & ~(target_ulong)(mask)) | (((target_ulong)(val) * ((mask) & ~((mask) << 1))) & (target_ulong)(mask)))

#define assert(x)            {if (!(x)) tlib_abortf("Assert not met in %s:%d: %s", __FILE__, __LINE__, #x);}while(0)

typedef struct custom_instruction_descriptor_t {
    uint64_t id;
    uint64_t length;
    uint64_t mask;
    uint64_t pattern;
} custom_instruction_descriptor_t;
#define CPU_CUSTOM_INSTRUCTIONS_LIMIT 256

#define MAX_CSR_ID 0xFFF
#define CSRS_PER_SLOT 64
#define CSRS_SLOTS (MAX_CSR_ID + 1) / CSRS_PER_SLOT

#define VLEN_MAX (1 << 16)

typedef struct DisasContext {
    struct DisasContextBase base;
    uint64_t opcode;
} DisasContext;

typedef struct CPUState CPUState;

#include "cpu-common.h"
#include "pmp.h"

// +---------------------------------------+
// | ALL FIELDS WHICH STATE MUST BE STORED |
// | DURING SERIALIZATION SHOULD BE PLACED |
// | BEFORE >CPU_COMMON< SECTION.          |
// +---------------------------------------+
struct CPUState {
    target_ulong gpr[32];
    uint64_t fpr[32]; /* assume both F and D extensions */
    uint8_t vr[32 * (VLEN_MAX / 8)];
    target_ulong pc;

    target_ulong frm;
    target_ulong fflags;

    target_ulong badaddr;

    target_ulong priv;

    target_ulong misa;
    target_ulong misa_mask;
    target_ulong mstatus;

    target_ulong mhartid;

    pthread_mutex_t mip_lock;
    target_ulong mip;
    target_ulong mie;
    target_ulong mideleg;

    target_ulong sptbr;  /* until: priv-1.9.1;  replaced by satp */
    target_ulong medeleg;

    target_ulong stvec;
    target_ulong sepc;
    target_ulong scause;
    target_ulong stval;  /* renamed from sbadaddr since: priv-1.10.0 */
    target_ulong satp;   /* since: priv-1.10.0 */
    target_ulong sedeleg;
    target_ulong sideleg;

    target_ulong mtvec;
    target_ulong mepc;
    target_ulong mcause;
    target_ulong mtval;      /*  renamed from mbadaddr since: priv-1.10.0 */

    uint32_t mucounteren;    /* until 1.10.0 */
    uint32_t mscounteren;    /* until 1.10.0 */
    target_ulong scounteren; /* since: priv-1.10.0 */
    target_ulong mcounteren; /* since: priv-1.10.0 */
    uint32_t mcountinhibit;  /* since: priv-1.11 */

    target_ulong sscratch;
    target_ulong mscratch;

    target_ulong vstart;
    target_ulong vxsat;
    target_ulong vxrm;
    target_ulong vcsr;
    target_ulong vl;
    target_ulong vtype;
    target_ulong vlenb;

    /* Vector shadow state */
    target_ulong elen;
    target_ulong vlmax;

    target_ulong vsew;
    target_ulong vlmul;
    float vflmul;
    target_ulong vill;
    target_ulong vta;
    target_ulong vma;

    /* temporary htif regs */
    uint64_t mfromhost;
    uint64_t mtohost;
    uint64_t timecmp;

    /* physical memory protection */
    pmp_table_t pmp_state;

    float_status fp_status;

    uint64_t mcycle_snapshot_offset;
    uint64_t mcycle_snapshot;

    uint64_t minstret_snapshot_offset;
    uint64_t minstret_snapshot;

    /* non maskable interrupts */
    uint32_t nmi_pending;
    target_ulong nmi_address;
    uint32_t nmi_length;

    int privilege_architecture;

    int32_t custom_instructions_count;
    custom_instruction_descriptor_t custom_instructions[CPU_CUSTOM_INSTRUCTIONS_LIMIT];

    // bitmap keeping information about CSRs that have custom external implementation
    uint64_t custom_csrs[CSRS_SLOTS];

    /*
       Supported CSR validation levels:
     * 0 - (CSR_VALIDATION_NONE): no validation
     * 1 - (CSR_VALIDATION_PRIV): privilege level validation only
     * 2 - (CSR_VALIDATION_FULL): full validation - privilege level and read/write bit validation

     * Illegal Instruction Exception* is generated when validation fails

       Levels are defined in `cpu_bits.h`
     */
    int32_t csr_validation_level;

    /* flags indicating extensions from which instructions
       that are *not* enabled for this CPU should *not* be logged as errors;

       this is useful when some instructions are `software-emulated`,
       i.e., the ILLEGAL INSTRUCTION exception is generated and handled by the software */
    target_ulong silenced_extensions;

    /* since priv-1.11.0 pmp grain size must be the same across all pmp regions */
    int32_t pmp_napot_grain;

    /* Supported modes:
         * 0 (INTERRUPT_MODE_AUTO) - chceck mtvec's LSB to detect mode: 0->direct, 1->vectored
         * 1 (INTERRUPT_MODE_DIRECT) - all exceptions set pc to mtvec's BASE
         * 2 (INTERRUPT_MODE_VECTORED) - asynchronous interrupts set pc to mtvec's BASE + 4 * cause
     */
    int32_t interrupt_mode;

    CPU_COMMON
};

void riscv_set_mode(CPUState *env, target_ulong newpriv);

void helper_raise_exception(CPUState *env, uint32_t exception);

int cpu_handle_mmu_fault(CPUState *cpu, target_ulong address, int rw, int mmu_idx, int access_width);

static inline int cpu_mmu_index(CPUState *env)
{
    return env->priv;
}

int riscv_cpu_hw_interrupts_pending(CPUState *env);

#include "cpu-all.h"
#include "exec-all.h"

static inline void cpu_get_tb_cpu_state(CPUState *env, target_ulong *pc, target_ulong *cs_base, int *flags)
{
    *pc = env->pc;
    *cs_base = 0;
    *flags = 0; // necessary to avoid compiler warning
}

static inline bool cpu_has_work(CPUState *env)
{
    // clear WFI if waking up condition is met
    env->wfi &= !(cpu->mip & cpu->mie);
    return !env->wfi;
}

static inline int riscv_mstatus_fs(CPUState *env)
{
    return env->mstatus & MSTATUS_FS;
}

void cpu_set_nmi(CPUState *env, int number);

void cpu_reset_nmi(CPUState *env, int number);

void csr_write_helper(CPUState *env, target_ulong val_to_write, target_ulong csrno);

void do_nmi(CPUState *env);

static inline void cpu_pc_from_tb(CPUState *cs, TranslationBlock *tb)
{
    cs->pc = tb->pc;
}

enum riscv_features {
    RISCV_FEATURE_RVI = RV('I'),
    RISCV_FEATURE_RVM = RV('M'),
    RISCV_FEATURE_RVA = RV('A'),
    RISCV_FEATURE_RVF = RV('F'),
    RISCV_FEATURE_RVD = RV('D'),
    RISCV_FEATURE_RVC = RV('C'),
    RISCV_FEATURE_RVS = RV('S'),
    RISCV_FEATURE_RVU = RV('U'),
    RISCV_FEATURE_RVV = RV('V'),
};

enum privilege_architecture {
    RISCV_PRIV1_09,
    RISCV_PRIV1_10,
    RISCV_PRIV1_11
};

static inline int riscv_has_ext(CPUState *env, target_ulong ext)
{
    return (env->misa & ext) != 0;
}

static inline int riscv_silent_ext(CPUState *env, target_ulong ext)
{
    return (env->silenced_extensions & ext) != 0;
}

static inline int riscv_features_to_string(uint32_t features, char *buffer, int size)
{
    // features are encoded on the first 26 bits
    // bit #0: 'A', bit #1: 'B', ..., bit #25: 'Z'
    int i, pos = 0;
    for (i = 0; i < 26 && pos < size; i++) {
        if (features & (1 << i)) {
            buffer[pos++] = 'A' + i;
        }
    }
    return pos;
}

static inline void mark_fs_dirty()
{
    env->mstatus |= (MSTATUS_FS | MSTATUS_XS);
}

static inline void set_default_mstatus()
{
    if (riscv_has_ext(env, RISCV_FEATURE_RVD) || riscv_has_ext(env, RISCV_FEATURE_RVF)) {
        env->mstatus = (MSTATUS_FS_INITIAL | MSTATUS_XS_INITIAL);
    } else {
        env->mstatus = 0;
    }
}

static inline uint32_t extract32(uint32_t value, uint8_t start, uint8_t length)
{
    return (value >> start) & ((((uint32_t)1) << length) - 1);
}

#define GET_VTYPE_VLMUL(inst)    extract32(inst, 0, 3)
#define GET_VTYPE_VSEW(inst)     extract32(inst, 3, 3)
#define GET_VTYPE_VTA(inst)      extract32(inst, 6, 1)
#define GET_VTYPE_VMA(inst)      extract32(inst, 7, 1)

#define V(x) (env->vr + (x) * env->vlenb)
#define SEW() GET_VTYPE_VSEW(env->vtype)
#define EMUL(eew) (((int8_t)(env->vlmul & 0x4 ? env->vlmul | 0xf8 : env->vlmul) + (eew) - SEW()) & 0x7)
#define V_IDX_INVALID_EMUL(n, emul) ((emul) < 0x4 && ((n) & ((1 << (emul)) - 1)) != 0)
#define V_IDX_INVALID_EEW(n, eew) V_IDX_INVALID_EMUL(n, EMUL(eew))
#define V_IDX_INVALID(n) V_IDX_INVALID_EMUL(n, env->vlmul)
#define V_INVALID_NF(vd, nf, emul)  (((emul) & 0x4) != 0 && (((nf) << (emul)) >= 8 || ((vd) + ((nf) << (emul))) >= 32))

#endif /* !defined (__RISCV_CPU_H__) */

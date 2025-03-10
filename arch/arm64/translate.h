#pragma once

#include "syndrome.h"

typedef struct DisasCompare {
    TCGCond cond;
    TCGv_i32 value;
    bool value_global;
} DisasCompare;

/* Share the TCG temporaries common between 32 and 64 bit modes.  */
extern TCGv_i32 cpu_NF, cpu_ZF, cpu_CF, cpu_VF;
extern TCGv_i64 cpu_exclusive_addr;
extern TCGv_i64 cpu_exclusive_val;

/*
 * Constant expanders for the decoders.
 */

static inline int negate(DisasContext *s, int x)
{
    return -x;
}

static inline int plus_1(DisasContext *s, int x)
{
    return x + 1;
}

static inline int plus_2(DisasContext *s, int x)
{
    return x + 2;
}

static inline int plus_12(DisasContext *s, int x)
{
    return x + 12;
}

static inline int times_2(DisasContext *s, int x)
{
    return x * 2;
}

static inline int times_4(DisasContext *s, int x)
{
    return x * 4;
}

static inline int times_2_plus_1(DisasContext *s, int x)
{
    return x * 2 + 1;
}

static inline int rsub_64(DisasContext *s, int x)
{
    return 64 - x;
}

static inline int rsub_32(DisasContext *s, int x)
{
    return 32 - x;
}

static inline int rsub_16(DisasContext *s, int x)
{
    return 16 - x;
}

static inline int rsub_8(DisasContext *s, int x)
{
    return 8 - x;
}

static inline int neon_3same_fp_size(DisasContext *s, int x)
{
    /* Convert 0==fp32, 1==fp16 into a MO_* value */
    return MO_32 - x;
}

static inline int arm_dc_feature(DisasContext *dc, int feature)
{
    return (dc->features & (1ULL << feature)) != 0;
}

static inline int get_mem_index(DisasContext *s)
{
    return arm_to_core_mmu_idx(s->mmu_idx);
}

static inline void disas_set_insn_syndrome(DisasContext *s, uint32_t syn)
{
    /* We don't need to save all of the syndrome so we mask and shift
     * out unneeded bits to help the sleb128 encoder do a better job.
     */
    syn &= ARM_INSN_START_WORD2_MASK;
    syn >>= ARM_INSN_START_WORD2_SHIFT;

    /* We clear insn_start_args after setting the param to catch multiple updates.  */
    tcg_set_insn_start_param(s->insn_start_args, 2, syn);
    s->insn_start_args = NULL;
}

/* is_jmp field values */
/* CPU state was modified dynamically; exit to main loop for interrupts. */
#define DISAS_UPDATE_EXIT DISAS_TARGET_1
/* These instructions trap after executing, so the A32/T32 decoder must
 * defer them until after the conditional execution state has been updated.
 * WFI also needs special handling when single-stepping.
 */
#define DISAS_WFI DISAS_TARGET_2
#define DISAS_SWI DISAS_TARGET_3
/* WFE */
#define DISAS_WFE   DISAS_TARGET_4
#define DISAS_HVC   DISAS_TARGET_5
#define DISAS_SMC   DISAS_TARGET_6
#define DISAS_YIELD DISAS_TARGET_7
/* M profile branch which might be an exception return (and so needs
 * custom end-of-TB code)
 */
#define DISAS_BX_EXCRET DISAS_TARGET_8
/*
 * For instructions which want an immediate exit to the main loop, as opposed
 * to attempting to use lookup_and_goto_ptr.  Unlike DISAS_UPDATE_EXIT, this
 * doesn't write the PC on exiting the translation loop so you need to ensure
 * something (gen_a64_set_pc_im or runtime helper) has done so before we reach
 * return from cpu_tb_exec.
 */
#define DISAS_EXIT DISAS_TARGET_9
/* CPU state was modified dynamically; no need to exit, but do not chain. */
#define DISAS_UPDATE_NOCHAIN DISAS_TARGET_10

#ifdef TARGET_AARCH64
void a64_translate_init(void);
void gen_a64_set_pc_im(uint64_t val);
void gen_exception_internal(int excp);
extern const TranslatorOps aarch64_translator_ops;
#else
static inline void a64_translate_init(void) { }

static inline void gen_a64_set_pc_im(uint64_t val) { }
#endif

void arm_test_cc(DisasCompare *cmp, int cc);
void arm_free_cc(DisasCompare *cmp);
void arm_jump_cc(DisasCompare *cmp, int label);
void arm_gen_test_cc(int cc, int label);
MemOp pow2_align(unsigned i);
void unallocated_encoding(DisasContext *s);
void gen_exception_insn_el(DisasContext *s, uint64_t pc, int excp, uint32_t syn, uint32_t target_el);
void gen_exception_insn(DisasContext *s, uint64_t pc, int excp, uint32_t syn);

/* Return state of Alternate Half-precision flag, caller frees result */
static inline TCGv_i32 get_ahp_flag(void)
{
    TCGv_i32 ret = tcg_temp_new_i32();

    tcg_gen_ld_i32(ret, cpu_env, offsetof(CPUARMState, vfp.xregs[ARM_VFP_FPSCR]));
    tcg_gen_extract_i32(ret, ret, 26, 1);

    return ret;
}

/* Set bits within PSTATE.  */
static inline void set_pstate_bits(uint32_t bits)
{
    TCGv_i32 p = tcg_temp_new_i32();

    tcg_debug_assert(!(bits & CACHED_PSTATE_BITS));

    tcg_gen_ld_i32(p, cpu_env, offsetof(CPUARMState, pstate));
    tcg_gen_ori_i32(p, p, bits);
    tcg_gen_st_i32(p, cpu_env, offsetof(CPUARMState, pstate));
    tcg_temp_free_i32(p);
}

/* Clear bits within PSTATE.  */
static inline void clear_pstate_bits(uint32_t bits)
{
    TCGv_i32 p = tcg_temp_new_i32();

    tcg_debug_assert(!(bits & CACHED_PSTATE_BITS));

    tcg_gen_ld_i32(p, cpu_env, offsetof(CPUARMState, pstate));
    tcg_gen_andi_i32(p, p, ~bits);
    tcg_gen_st_i32(p, cpu_env, offsetof(CPUARMState, pstate));
    tcg_temp_free_i32(p);
}

/* If the singlestep state is Active-not-pending, advance to Active-pending. */
static inline void gen_ss_advance(DisasContext *s)
{
    if(s->ss_active) {
        s->pstate_ss = 0;
        clear_pstate_bits(PSTATE_SS);
    }
}

/* Generate an architectural singlestep exception */
static inline void gen_swstep_exception(DisasContext *s, int isv, int ex)
{
    /* Fill in the same_el field of the syndrome in the helper. */
    uint32_t syn = syn_swstep(false, isv, ex);
    gen_helper_exception_swstep(cpu_env, tcg_constant_i32(syn));
}

/*
 * Given a VFP floating point constant encoded into an 8 bit immediate in an
 * instruction, expand it to the actual constant value of the specified
 * size, as per the VFPExpandImm() pseudocode in the Arm ARM.
 */
uint64_t vfp_expand_imm(int size, uint8_t imm8);

/* Vector operations shared between ARM and AArch64.  */
void gen_gvec_ceq0(unsigned vece, uint32_t rd_ofs, uint32_t rm_ofs, uint32_t opr_sz, uint32_t max_sz);
void gen_gvec_clt0(unsigned vece, uint32_t rd_ofs, uint32_t rm_ofs, uint32_t opr_sz, uint32_t max_sz);
void gen_gvec_cgt0(unsigned vece, uint32_t rd_ofs, uint32_t rm_ofs, uint32_t opr_sz, uint32_t max_sz);
void gen_gvec_cle0(unsigned vece, uint32_t rd_ofs, uint32_t rm_ofs, uint32_t opr_sz, uint32_t max_sz);
void gen_gvec_cge0(unsigned vece, uint32_t rd_ofs, uint32_t rm_ofs, uint32_t opr_sz, uint32_t max_sz);

void gen_gvec_mla(unsigned vece, uint32_t rd_ofs, uint32_t rn_ofs, uint32_t rm_ofs, uint32_t opr_sz, uint32_t max_sz);
void gen_gvec_mls(unsigned vece, uint32_t rd_ofs, uint32_t rn_ofs, uint32_t rm_ofs, uint32_t opr_sz, uint32_t max_sz);

void gen_gvec_cmtst(unsigned vece, uint32_t rd_ofs, uint32_t rn_ofs, uint32_t rm_ofs, uint32_t opr_sz, uint32_t max_sz);
void gen_gvec_sshl(unsigned vece, uint32_t rd_ofs, uint32_t rn_ofs, uint32_t rm_ofs, uint32_t opr_sz, uint32_t max_sz);
void gen_gvec_ushl(unsigned vece, uint32_t rd_ofs, uint32_t rn_ofs, uint32_t rm_ofs, uint32_t opr_sz, uint32_t max_sz);

void gen_cmtst_i64(TCGv_i64 d, TCGv_i64 a, TCGv_i64 b);
void gen_ushl_i32(TCGv_i32 d, TCGv_i32 a, TCGv_i32 b);
void gen_sshl_i32(TCGv_i32 d, TCGv_i32 a, TCGv_i32 b);
void gen_ushl_i64(TCGv_i64 d, TCGv_i64 a, TCGv_i64 b);
void gen_sshl_i64(TCGv_i64 d, TCGv_i64 a, TCGv_i64 b);

void gen_gvec_uqadd_qc(unsigned vece, uint32_t rd_ofs, uint32_t rn_ofs, uint32_t rm_ofs, uint32_t opr_sz, uint32_t max_sz);
void gen_gvec_sqadd_qc(unsigned vece, uint32_t rd_ofs, uint32_t rn_ofs, uint32_t rm_ofs, uint32_t opr_sz, uint32_t max_sz);
void gen_gvec_uqsub_qc(unsigned vece, uint32_t rd_ofs, uint32_t rn_ofs, uint32_t rm_ofs, uint32_t opr_sz, uint32_t max_sz);
void gen_gvec_sqsub_qc(unsigned vece, uint32_t rd_ofs, uint32_t rn_ofs, uint32_t rm_ofs, uint32_t opr_sz, uint32_t max_sz);

void gen_gvec_ssra(unsigned vece, uint32_t rd_ofs, uint32_t rm_ofs, int64_t shift, uint32_t opr_sz, uint32_t max_sz);
void gen_gvec_usra(unsigned vece, uint32_t rd_ofs, uint32_t rm_ofs, int64_t shift, uint32_t opr_sz, uint32_t max_sz);

void gen_gvec_srshr(unsigned vece, uint32_t rd_ofs, uint32_t rm_ofs, int64_t shift, uint32_t opr_sz, uint32_t max_sz);
void gen_gvec_urshr(unsigned vece, uint32_t rd_ofs, uint32_t rm_ofs, int64_t shift, uint32_t opr_sz, uint32_t max_sz);
void gen_gvec_srsra(unsigned vece, uint32_t rd_ofs, uint32_t rm_ofs, int64_t shift, uint32_t opr_sz, uint32_t max_sz);
void gen_gvec_ursra(unsigned vece, uint32_t rd_ofs, uint32_t rm_ofs, int64_t shift, uint32_t opr_sz, uint32_t max_sz);

void gen_gvec_sri(unsigned vece, uint32_t rd_ofs, uint32_t rm_ofs, int64_t shift, uint32_t opr_sz, uint32_t max_sz);
void gen_gvec_sli(unsigned vece, uint32_t rd_ofs, uint32_t rm_ofs, int64_t shift, uint32_t opr_sz, uint32_t max_sz);

void gen_gvec_sqrdmlah_qc(unsigned vece, uint32_t rd_ofs, uint32_t rn_ofs, uint32_t rm_ofs, uint32_t opr_sz, uint32_t max_sz);
void gen_gvec_sqrdmlsh_qc(unsigned vece, uint32_t rd_ofs, uint32_t rn_ofs, uint32_t rm_ofs, uint32_t opr_sz, uint32_t max_sz);

void gen_gvec_sabd(unsigned vece, uint32_t rd_ofs, uint32_t rn_ofs, uint32_t rm_ofs, uint32_t opr_sz, uint32_t max_sz);
void gen_gvec_uabd(unsigned vece, uint32_t rd_ofs, uint32_t rn_ofs, uint32_t rm_ofs, uint32_t opr_sz, uint32_t max_sz);

void gen_gvec_saba(unsigned vece, uint32_t rd_ofs, uint32_t rn_ofs, uint32_t rm_ofs, uint32_t opr_sz, uint32_t max_sz);
void gen_gvec_uaba(unsigned vece, uint32_t rd_ofs, uint32_t rn_ofs, uint32_t rm_ofs, uint32_t opr_sz, uint32_t max_sz);

/*
 * Forward to the isar_feature_* tests given a DisasContext pointer.
 */
#define dc_isar_feature(name, ctx)       \
    ({                                   \
        DisasContext *ctx_ = (ctx);      \
        isar_feature_##name(ctx_->isar); \
    })

/* Note that the gvec expanders operate on offsets + sizes.  */
typedef void GVecGen2Fn(unsigned, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void GVecGen2iFn(unsigned, uint32_t, uint32_t, int64_t, uint32_t, uint32_t);
typedef void GVecGen3Fn(unsigned, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void GVecGen4Fn(unsigned, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

/* Function prototype for gen_ functions for calling Neon helpers */
typedef void NeonGenOneOpFn(TCGv_i32, TCGv_i32);
typedef void NeonGenOneOpEnvFn(TCGv_i32, TCGv_ptr, TCGv_i32);
typedef void NeonGenTwoOpFn(TCGv_i32, TCGv_i32, TCGv_i32);
typedef void NeonGenTwoOpEnvFn(TCGv_i32, TCGv_ptr, TCGv_i32, TCGv_i32);
typedef void NeonGenThreeOpEnvFn(TCGv_i32, TCGv_env, TCGv_i32, TCGv_i32, TCGv_i32);
typedef void NeonGenTwo64OpFn(TCGv_i64, TCGv_i64, TCGv_i64);
typedef void NeonGenTwo64OpEnvFn(TCGv_i64, TCGv_ptr, TCGv_i64, TCGv_i64);
typedef void NeonGenNarrowFn(TCGv_i32, TCGv_i64);
typedef void NeonGenNarrowEnvFn(TCGv_i32, TCGv_ptr, TCGv_i64);
typedef void NeonGenWidenFn(TCGv_i64, TCGv_i32);
typedef void NeonGenTwoOpWidenFn(TCGv_i64, TCGv_i32, TCGv_i32);
typedef void NeonGenOneSingleOpFn(TCGv_i32, TCGv_i32, TCGv_ptr);
typedef void NeonGenTwoSingleOpFn(TCGv_i32, TCGv_i32, TCGv_i32, TCGv_ptr);
typedef void NeonGenTwoDoubleOpFn(TCGv_i64, TCGv_i64, TCGv_i64, TCGv_ptr);
typedef void NeonGenOne64OpFn(TCGv_i64, TCGv_i64);
typedef void CryptoTwoOpFn(TCGv_ptr, TCGv_ptr);
typedef void CryptoThreeOpIntFn(TCGv_ptr, TCGv_ptr, TCGv_i32);
typedef void CryptoThreeOpFn(TCGv_ptr, TCGv_ptr, TCGv_ptr);
typedef void AtomicThreeOpFn(TCGv_i64, TCGv_i64, TCGv_i64, TCGArg, MemOp);
typedef void WideShiftImmFn(TCGv_i64, TCGv_i64, int64_t shift);
typedef void WideShiftFn(TCGv_i64, TCGv_ptr, TCGv_i64, TCGv_i32);
typedef void ShiftImmFn(TCGv_i32, TCGv_i32, int32_t shift);
typedef void ShiftFn(TCGv_i32, TCGv_ptr, TCGv_i32, TCGv_i32);

/**
 * arm_tbflags_from_tb:
 * @tb: the TranslationBlock
 *
 * Extract the flag values from @tb.
 */
static inline CPUARMTBFlags arm_tbflags_from_tb(const TranslationBlock *tb)
{
    return (CPUARMTBFlags) { tb->flags, tb->cs_base };
}

/*
 * Enum for argument to fpstatus_ptr().
 */
typedef enum ARMFPStatusFlavour {
    FPST_FPCR,
    FPST_FPCR_F16,
    FPST_STD,
    FPST_STD_F16,
} ARMFPStatusFlavour;

/**
 * fpstatus_ptr: return TCGv_ptr to the specified fp_status field
 *
 * We have multiple softfloat float_status fields in the Arm CPU state struct
 * (see the comment in cpu.h for details). Return a TCGv_ptr which has
 * been set up to point to the requested field in the CPU state struct.
 * The options are:
 *
 * FPST_FPCR
 *   for non-FP16 operations controlled by the FPCR
 * FPST_FPCR_F16
 *   for operations controlled by the FPCR where FPCR.FZ16 is to be used
 * FPST_STD
 *   for A32/T32 Neon operations using the "standard FPSCR value"
 * FPST_STD_F16
 *   as FPST_STD, but where FPCR.FZ16 is to be used
 */
static inline TCGv_ptr fpstatus_ptr(ARMFPStatusFlavour flavour)
{
    TCGv_ptr statusptr = tcg_temp_new_ptr();
    int offset;

    switch(flavour) {
        case FPST_FPCR:
            offset = offsetof(CPUARMState, vfp.fp_status);
            break;
        case FPST_FPCR_F16:
            offset = offsetof(CPUARMState, vfp.fp_status_f16);
            break;
        case FPST_STD:
            offset = offsetof(CPUARMState, vfp.standard_fp_status);
            break;
        case FPST_STD_F16:
            offset = offsetof(CPUARMState, vfp.standard_fp_status_f16);
            break;
        default:
            g_assert_not_reached();
    }
    tcg_gen_addi_ptr(statusptr, cpu_env, offset);
    return statusptr;
}

/**
 * finalize_memop:
 * @s: DisasContext
 * @opc: size+sign+align of the memory operation
 *
 * Build the complete MemOp for a memory operation, including alignment
 * and endianness.
 *
 * If (op & MO_AMASK) then the operation already contains the required
 * alignment, e.g. for AccType_ATOMIC.  Otherwise, this an optionally
 * unaligned operation, e.g. for AccType_NORMAL.
 *
 * In the latter case, there are configuration bits that require alignment,
 * and this is applied here.  Note that there is no way to indicate that
 * no alignment should ever be enforced; this must be handled manually.
 */
static inline MemOp finalize_memop(DisasContext *s, MemOp opc)
{
    if(s->align_mem && !(opc & MO_AMASK)) {
        opc |= MO_ALIGN;
    }
    return opc | s->be_data;
}

/**
 * asimd_imm_const: Expand an encoded SIMD constant value
 *
 * Expand a SIMD constant value. This is essentially the pseudocode
 * AdvSIMDExpandImm, except that we also perform the boolean NOT needed for
 * VMVN and VBIC (when cmode < 14 && op == 1).
 *
 * The combination cmode == 15 op == 1 is a reserved encoding for AArch32;
 * callers must catch this; we return the 64-bit constant value defined
 * for AArch64.
 *
 * cmode = 2,3,4,5,6,7,10,11,12,13 imm=0 was UNPREDICTABLE in v7A but
 * is either not unpredictable or merely CONSTRAINED UNPREDICTABLE in v8A;
 * we produce an immediate constant value of 0 in these cases.
 */
uint64_t asimd_imm_const(uint32_t imm, int cmode, int op);

/*
 * Helpers for implementing sets of trans_* functions.
 * Defer the implementation of NAME to FUNC, with optional extra arguments.
 */
#define TRANS(NAME, FUNC, ...)                               \
    static bool trans_##NAME(DisasContext *s, arg_##NAME *a) \
    {                                                        \
        return FUNC(s, __VA_ARGS__);                         \
    }
#define TRANS_FEAT(NAME, FEAT, FUNC, ...)                        \
    static bool trans_##NAME(DisasContext *s, arg_##NAME *a)     \
    {                                                            \
        return dc_isar_feature(FEAT, s) && FUNC(s, __VA_ARGS__); \
    }

#define TRANS_FEAT_NONSTREAMING(NAME, FEAT, FUNC, ...)           \
    static bool trans_##NAME(DisasContext *s, arg_##NAME *a)     \
    {                                                            \
        s->is_nonstreaming = true;                               \
        return dc_isar_feature(FEAT, s) && FUNC(s, __VA_ARGS__); \
    }

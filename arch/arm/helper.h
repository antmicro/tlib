#include <stdint.h>

int get_phys_addr(CPUState *env, uint32_t address, bool is_secure, int access_type, bool is_user, uint32_t *phys_ptr, int *prot,
                  target_ulong *page_size, int no_page_fault);

bool is_impl_def_exempt_from_attribution(uint32_t address, bool *applies_to_whole_page);
bool try_get_impl_def_attr_exemption_region(uint32_t address, uint32_t start_at, uint32_t *found_index,
                                            bool *applies_to_whole_page);

#include "def-helper.h"

DEF_HELPER_1(clz, i32, i32)
DEF_HELPER_1(sxtb16, i32, i32)
DEF_HELPER_1(uxtb16, i32, i32)

DEF_HELPER_2(add_setq, i32, i32, i32)
DEF_HELPER_2(add_saturate, i32, i32, i32)
DEF_HELPER_2(sub_saturate, i32, i32, i32)
DEF_HELPER_2(add_usaturate, i32, i32, i32)
DEF_HELPER_2(sub_usaturate, i32, i32, i32)
DEF_HELPER_1(double_saturate, i32, s32)
DEF_HELPER_2(sdiv, s32, s32, s32)
DEF_HELPER_2(udiv, i32, i32, i32)
DEF_HELPER_1(rbit, i32, i32)
DEF_HELPER_1(abs, i32, i32)

#define PAS_OP(pfx)                                \
    DEF_HELPER_3(pfx##add8, i32, i32, i32, ptr)    \
    DEF_HELPER_3(pfx##sub8, i32, i32, i32, ptr)    \
    DEF_HELPER_3(pfx##sub16, i32, i32, i32, ptr)   \
    DEF_HELPER_3(pfx##add16, i32, i32, i32, ptr)   \
    DEF_HELPER_3(pfx##addsubx, i32, i32, i32, ptr) \
    DEF_HELPER_3(pfx##subaddx, i32, i32, i32, ptr)

PAS_OP(s)
PAS_OP(u)
#undef PAS_OP

#define PAS_OP(pfx)                           \
    DEF_HELPER_2(pfx##add8, i32, i32, i32)    \
    DEF_HELPER_2(pfx##sub8, i32, i32, i32)    \
    DEF_HELPER_2(pfx##sub16, i32, i32, i32)   \
    DEF_HELPER_2(pfx##add16, i32, i32, i32)   \
    DEF_HELPER_2(pfx##addsubx, i32, i32, i32) \
    DEF_HELPER_2(pfx##subaddx, i32, i32, i32)
PAS_OP(q)
PAS_OP(sh)
PAS_OP(uq)
PAS_OP(uh)
#undef PAS_OP

DEF_HELPER_2(ssat, i32, i32, i32)
DEF_HELPER_2(usat, i32, i32, i32)
DEF_HELPER_2(ssat16, i32, i32, i32)
DEF_HELPER_2(usat16, i32, i32, i32)

DEF_HELPER_2(usad8, i32, i32, i32)

DEF_HELPER_1(logicq_cc, i32, i64)

DEF_HELPER_3(sel_flags, i32, i32, i32, i32)
DEF_HELPER_1(exception, void, i32)
DEF_HELPER_0(wfi, void)
DEF_HELPER_0(wfe, void)

DEF_HELPER_2(cpsr_write, void, i32, i32)
DEF_HELPER_0(cpsr_read, i32)

#ifdef TARGET_PROTO_ARM_M
DEF_HELPER_3(v7m_msr, void, env, i32, i32)
DEF_HELPER_2(v7m_mrs, i32, env, i32)

DEF_HELPER_1(fp_lsp, void, env)
DEF_HELPER_3(v8m_blxns, void, env, i32, i32)
DEF_HELPER_1(v8m_sg, void, env)

DEF_HELPER_2(v8m_bx_update_pc, void, env, i32)

DEF_HELPER_3(v8m_vlstm, void, env, i32, i32)
DEF_HELPER_3(v8m_vlldm, void, env, i32, i32)
#endif

DEF_HELPER_4(set_cp15_64bit, void, env, i32, i32, i32)
DEF_HELPER_2(get_cp15_64bit, i64, env, i32)

DEF_HELPER_3(set_cp15_32bit, void, env, i32, i32)
DEF_HELPER_2(get_cp15_32bit, i32, env, i32)

DEF_HELPER_2(get_r13_banked, i32, env, i32)
DEF_HELPER_3(set_r13_banked, void, env, i32, i32)

DEF_HELPER_1(get_user_reg, i32, i32)
DEF_HELPER_2(set_user_reg, void, i32, i32)

DEF_HELPER_1(vfp_get_fpscr, i32, env)
DEF_HELPER_2(vfp_set_fpscr, void, env, i32)

DEF_HELPER_3(vfp_adds, f32, f32, f32, ptr)
DEF_HELPER_3(vfp_addd, f64, f64, f64, ptr)
DEF_HELPER_3(vfp_subs, f32, f32, f32, ptr)
DEF_HELPER_3(vfp_subd, f64, f64, f64, ptr)
DEF_HELPER_3(vfp_muls, f32, f32, f32, ptr)
DEF_HELPER_3(vfp_muld, f64, f64, f64, ptr)
DEF_HELPER_3(vfp_divs, f32, f32, f32, ptr)
DEF_HELPER_3(vfp_divd, f64, f64, f64, ptr)
DEF_HELPER_1(vfp_negs, f32, f32)
DEF_HELPER_1(vfp_negd, f64, f64)
DEF_HELPER_1(vfp_abss, f32, f32)
DEF_HELPER_1(vfp_absd, f64, f64)
DEF_HELPER_2(vfp_sqrts, f32, f32, env)
DEF_HELPER_2(vfp_sqrtd, f64, f64, env)
DEF_HELPER_3(vfp_cmps, void, f32, f32, env)
DEF_HELPER_3(vfp_cmpd, void, f64, f64, env)
DEF_HELPER_3(vfp_cmpes, void, f32, f32, env)
DEF_HELPER_3(vfp_cmped, void, f64, f64, env)

DEF_HELPER_2(vfp_fcvtds, f64, f32, env)
DEF_HELPER_2(vfp_fcvtsd, f32, f64, env)

DEF_HELPER_2(vfp_uitos, f32, i32, ptr)
DEF_HELPER_2(vfp_uitod, f64, i32, ptr)
DEF_HELPER_2(vfp_sitos, f32, i32, ptr)
DEF_HELPER_2(vfp_sitod, f64, i32, ptr)

DEF_HELPER_2(vfp_touis, i32, f32, ptr)
DEF_HELPER_2(vfp_touid, i32, f64, ptr)
DEF_HELPER_2(vfp_touizs, i32, f32, ptr)
DEF_HELPER_2(vfp_touizd, i32, f64, ptr)
DEF_HELPER_2(vfp_tosis, i32, f32, ptr)
DEF_HELPER_2(vfp_tosid, i32, f64, ptr)
DEF_HELPER_2(vfp_tosizs, i32, f32, ptr)
DEF_HELPER_2(vfp_tosizd, i32, f64, ptr)

DEF_HELPER_3(vfp_toshs, i32, f32, i32, ptr)
DEF_HELPER_3(vfp_tosls, i32, f32, i32, ptr)
DEF_HELPER_3(vfp_touhs, i32, f32, i32, ptr)
DEF_HELPER_3(vfp_touls, i32, f32, i32, ptr)
DEF_HELPER_3(vfp_toshd, i64, f64, i32, ptr)
DEF_HELPER_3(vfp_tosld, i64, f64, i32, ptr)
DEF_HELPER_3(vfp_touhd, i64, f64, i32, ptr)
DEF_HELPER_3(vfp_tould, i64, f64, i32, ptr)
DEF_HELPER_3(vfp_shtos, f32, i32, i32, ptr)
DEF_HELPER_3(vfp_sltos, f32, i32, i32, ptr)
DEF_HELPER_3(vfp_uhtos, f32, i32, i32, ptr)
DEF_HELPER_3(vfp_ultos, f32, i32, i32, ptr)
DEF_HELPER_3(vfp_shtod, f64, i64, i32, ptr)
DEF_HELPER_3(vfp_sltod, f64, i64, i32, ptr)
DEF_HELPER_3(vfp_uhtod, f64, i64, i32, ptr)
DEF_HELPER_3(vfp_ultod, f64, i64, i32, ptr)

DEF_HELPER_2(vfp_fcvt_f16_to_f32, f32, i32, env)
DEF_HELPER_2(vfp_fcvt_f32_to_f16, i32, f32, env)
DEF_HELPER_2(neon_fcvt_f16_to_f32, f32, i32, env)
DEF_HELPER_2(neon_fcvt_f32_to_f16, i32, f32, env)

DEF_HELPER_4(vfp_muladdd, f64, f64, f64, f64, ptr)
DEF_HELPER_4(vfp_muladds, f32, f32, f32, f32, ptr)

DEF_HELPER_3(recps_f32, f32, f32, f32, env)
DEF_HELPER_3(rsqrts_f32, f32, f32, f32, env)
DEF_HELPER_2(recpe_f32, f32, f32, env)
DEF_HELPER_2(rsqrte_f32, f32, f32, env)
DEF_HELPER_2(recpe_u32, i32, i32, env)
DEF_HELPER_2(rsqrte_u32, i32, i32, env)
DEF_HELPER_4(neon_tbl, i32, i32, i32, i32, i32)

DEF_HELPER_2(add_cc, i32, i32, i32)
DEF_HELPER_2(adc_cc, i32, i32, i32)
DEF_HELPER_2(sub_cc, i32, i32, i32)
DEF_HELPER_2(sbc_cc, i32, i32, i32)

DEF_HELPER_2(shl, i32, i32, i32)
DEF_HELPER_2(shr, i32, i32, i32)
DEF_HELPER_2(sar, i32, i32, i32)
DEF_HELPER_2(shl_cc, i32, i32, i32)
DEF_HELPER_2(shr_cc, i32, i32, i32)
DEF_HELPER_2(sar_cc, i32, i32, i32)
DEF_HELPER_2(ror_cc, i32, i32, i32)

/* neon_helper.c */
DEF_HELPER_3(neon_qadd_u8, i32, env, i32, i32)
DEF_HELPER_3(neon_qadd_s8, i32, env, i32, i32)
DEF_HELPER_3(neon_qadd_u16, i32, env, i32, i32)
DEF_HELPER_3(neon_qadd_s16, i32, env, i32, i32)
DEF_HELPER_3(neon_qadd_u32, i32, env, i32, i32)
DEF_HELPER_3(neon_qadd_s32, i32, env, i32, i32)
DEF_HELPER_3(neon_qsub_u8, i32, env, i32, i32)
DEF_HELPER_3(neon_qsub_s8, i32, env, i32, i32)
DEF_HELPER_3(neon_qsub_u16, i32, env, i32, i32)
DEF_HELPER_3(neon_qsub_s16, i32, env, i32, i32)
DEF_HELPER_3(neon_qsub_u32, i32, env, i32, i32)
DEF_HELPER_3(neon_qsub_s32, i32, env, i32, i32)
DEF_HELPER_3(neon_qadd_u64, i64, env, i64, i64)
DEF_HELPER_3(neon_qadd_s64, i64, env, i64, i64)
DEF_HELPER_3(neon_qsub_u64, i64, env, i64, i64)
DEF_HELPER_3(neon_qsub_s64, i64, env, i64, i64)

DEF_HELPER_2(neon_hadd_s8, i32, i32, i32)
DEF_HELPER_2(neon_hadd_u8, i32, i32, i32)
DEF_HELPER_2(neon_hadd_s16, i32, i32, i32)
DEF_HELPER_2(neon_hadd_u16, i32, i32, i32)
DEF_HELPER_2(neon_hadd_s32, s32, s32, s32)
DEF_HELPER_2(neon_hadd_u32, i32, i32, i32)
DEF_HELPER_2(neon_rhadd_s8, i32, i32, i32)
DEF_HELPER_2(neon_rhadd_u8, i32, i32, i32)
DEF_HELPER_2(neon_rhadd_s16, i32, i32, i32)
DEF_HELPER_2(neon_rhadd_u16, i32, i32, i32)
DEF_HELPER_2(neon_rhadd_s32, s32, s32, s32)
DEF_HELPER_2(neon_rhadd_u32, i32, i32, i32)
DEF_HELPER_2(neon_hsub_s8, i32, i32, i32)
DEF_HELPER_2(neon_hsub_u8, i32, i32, i32)
DEF_HELPER_2(neon_hsub_s16, i32, i32, i32)
DEF_HELPER_2(neon_hsub_u16, i32, i32, i32)
DEF_HELPER_2(neon_hsub_s32, s32, s32, s32)
DEF_HELPER_2(neon_hsub_u32, i32, i32, i32)

DEF_HELPER_2(neon_cgt_u8, i32, i32, i32)
DEF_HELPER_2(neon_cgt_s8, i32, i32, i32)
DEF_HELPER_2(neon_cgt_u16, i32, i32, i32)
DEF_HELPER_2(neon_cgt_s16, i32, i32, i32)
DEF_HELPER_2(neon_cgt_u32, i32, i32, i32)
DEF_HELPER_2(neon_cgt_s32, i32, i32, i32)
DEF_HELPER_2(neon_cge_u8, i32, i32, i32)
DEF_HELPER_2(neon_cge_s8, i32, i32, i32)
DEF_HELPER_2(neon_cge_u16, i32, i32, i32)
DEF_HELPER_2(neon_cge_s16, i32, i32, i32)
DEF_HELPER_2(neon_cge_u32, i32, i32, i32)
DEF_HELPER_2(neon_cge_s32, i32, i32, i32)

DEF_HELPER_2(neon_min_u8, i32, i32, i32)
DEF_HELPER_2(neon_min_s8, i32, i32, i32)
DEF_HELPER_2(neon_min_u16, i32, i32, i32)
DEF_HELPER_2(neon_min_s16, i32, i32, i32)
DEF_HELPER_2(neon_min_u32, i32, i32, i32)
DEF_HELPER_2(neon_min_s32, i32, s32, s32)
DEF_HELPER_2(neon_max_u8, i32, i32, i32)
DEF_HELPER_2(neon_max_s8, i32, i32, i32)
DEF_HELPER_2(neon_max_u16, i32, i32, i32)
DEF_HELPER_2(neon_max_s16, i32, i32, i32)
DEF_HELPER_2(neon_max_u32, i32, i32, i32)
DEF_HELPER_2(neon_max_s32, i32, s32, s32)
DEF_HELPER_2(neon_pmin_u8, i32, i32, i32)
DEF_HELPER_2(neon_pmin_s8, i32, i32, i32)
DEF_HELPER_2(neon_pmin_u16, i32, i32, i32)
DEF_HELPER_2(neon_pmin_s16, i32, i32, i32)
DEF_HELPER_2(neon_pmax_u8, i32, i32, i32)
DEF_HELPER_2(neon_pmax_s8, i32, i32, i32)
DEF_HELPER_2(neon_pmax_u16, i32, i32, i32)
DEF_HELPER_2(neon_pmax_s16, i32, i32, i32)

DEF_HELPER_2(neon_abd_u8, i32, i32, i32)
DEF_HELPER_2(neon_abd_s8, i32, i32, i32)
DEF_HELPER_2(neon_abd_u16, i32, i32, i32)
DEF_HELPER_2(neon_abd_s16, i32, i32, i32)
DEF_HELPER_2(neon_abd_u32, i32, i32, i32)
DEF_HELPER_2(neon_abd_s32, i32, s32, s32)

DEF_HELPER_2(neon_shl_u8, i32, i32, i32)
DEF_HELPER_2(neon_shl_s8, i32, i32, i32)
DEF_HELPER_2(neon_shl_u16, i32, i32, i32)
DEF_HELPER_2(neon_shl_s16, i32, i32, i32)
DEF_HELPER_2(neon_shl_u32, i32, i32, i32)
DEF_HELPER_2(neon_shl_s32, i32, i32, i32)
DEF_HELPER_2(neon_shl_u64, i64, i64, i64)
DEF_HELPER_2(neon_shl_s64, i64, i64, i64)
DEF_HELPER_2(neon_rshl_u8, i32, i32, i32)
DEF_HELPER_2(neon_rshl_s8, i32, i32, i32)
DEF_HELPER_2(neon_rshl_u16, i32, i32, i32)
DEF_HELPER_2(neon_rshl_s16, i32, i32, i32)
DEF_HELPER_2(neon_rshl_u32, i32, i32, i32)
DEF_HELPER_2(neon_rshl_s32, i32, i32, i32)
DEF_HELPER_2(neon_rshl_u64, i64, i64, i64)
DEF_HELPER_2(neon_rshl_s64, i64, i64, i64)
DEF_HELPER_3(neon_qshl_u8, i32, env, i32, i32)
DEF_HELPER_3(neon_qshl_s8, i32, env, i32, i32)
DEF_HELPER_3(neon_qshl_u16, i32, env, i32, i32)
DEF_HELPER_3(neon_qshl_s16, i32, env, i32, i32)
DEF_HELPER_3(neon_qshl_u32, i32, env, i32, i32)
DEF_HELPER_3(neon_qshl_s32, i32, env, i32, i32)
DEF_HELPER_3(neon_qshl_u64, i64, env, i64, i64)
DEF_HELPER_3(neon_qshl_s64, i64, env, i64, i64)
DEF_HELPER_3(neon_qshlu_s8, i32, env, i32, i32);
DEF_HELPER_3(neon_qshlu_s16, i32, env, i32, i32);
DEF_HELPER_3(neon_qshlu_s32, i32, env, i32, i32);
DEF_HELPER_3(neon_qshlu_s64, i64, env, i64, i64);
DEF_HELPER_3(neon_qrshl_u8, i32, env, i32, i32)
DEF_HELPER_3(neon_qrshl_s8, i32, env, i32, i32)
DEF_HELPER_3(neon_qrshl_u16, i32, env, i32, i32)
DEF_HELPER_3(neon_qrshl_s16, i32, env, i32, i32)
DEF_HELPER_3(neon_qrshl_u32, i32, env, i32, i32)
DEF_HELPER_3(neon_qrshl_s32, i32, env, i32, i32)
DEF_HELPER_3(neon_qrshl_u64, i64, env, i64, i64)
DEF_HELPER_3(neon_qrshl_s64, i64, env, i64, i64)

DEF_HELPER_2(neon_add_u8, i32, i32, i32)
DEF_HELPER_2(neon_add_u16, i32, i32, i32)
DEF_HELPER_2(neon_padd_u8, i32, i32, i32)
DEF_HELPER_2(neon_padd_u16, i32, i32, i32)
DEF_HELPER_2(neon_sub_u8, i32, i32, i32)
DEF_HELPER_2(neon_sub_u16, i32, i32, i32)
DEF_HELPER_2(neon_mul_u8, i32, i32, i32)
DEF_HELPER_2(neon_mul_u16, i32, i32, i32)
DEF_HELPER_2(neon_mul_p8, i32, i32, i32)
DEF_HELPER_2(neon_mull_p8, i64, i32, i32)

DEF_HELPER_2(neon_tst_u8, i32, i32, i32)
DEF_HELPER_2(neon_tst_u16, i32, i32, i32)
DEF_HELPER_2(neon_tst_u32, i32, i32, i32)
DEF_HELPER_2(neon_ceq_u8, i32, i32, i32)
DEF_HELPER_2(neon_ceq_u16, i32, i32, i32)
DEF_HELPER_2(neon_ceq_u32, i32, i32, i32)

DEF_HELPER_1(neon_abs_s8, i32, i32)
DEF_HELPER_1(neon_abs_s16, i32, i32)
DEF_HELPER_1(neon_clz_u8, i32, i32)
DEF_HELPER_1(neon_clz_u16, i32, i32)
DEF_HELPER_1(neon_cls_s8, i32, i32)
DEF_HELPER_1(neon_cls_s16, i32, i32)
DEF_HELPER_1(neon_cls_s32, i32, i32)
DEF_HELPER_1(neon_cnt_u8, i32, i32)

DEF_HELPER_3(neon_qdmulh_s16, i32, env, i32, i32)
DEF_HELPER_3(neon_qrdmulh_s16, i32, env, i32, i32)
DEF_HELPER_3(neon_qdmulh_s32, i32, env, i32, i32)
DEF_HELPER_3(neon_qrdmulh_s32, i32, env, i32, i32)

DEF_HELPER_1(neon_narrow_u8, i32, i64)
DEF_HELPER_1(neon_narrow_u16, i32, i64)
DEF_HELPER_2(neon_unarrow_sat8, i32, env, i64)
DEF_HELPER_2(neon_narrow_sat_u8, i32, env, i64)
DEF_HELPER_2(neon_narrow_sat_s8, i32, env, i64)
DEF_HELPER_2(neon_unarrow_sat16, i32, env, i64)
DEF_HELPER_2(neon_narrow_sat_u16, i32, env, i64)
DEF_HELPER_2(neon_narrow_sat_s16, i32, env, i64)
DEF_HELPER_2(neon_unarrow_sat32, i32, env, i64)
DEF_HELPER_2(neon_narrow_sat_u32, i32, env, i64)
DEF_HELPER_2(neon_narrow_sat_s32, i32, env, i64)
DEF_HELPER_1(neon_narrow_high_u8, i32, i64)
DEF_HELPER_1(neon_narrow_high_u16, i32, i64)
DEF_HELPER_1(neon_narrow_round_high_u8, i32, i64)
DEF_HELPER_1(neon_narrow_round_high_u16, i32, i64)
DEF_HELPER_1(neon_widen_u8, i64, i32)
DEF_HELPER_1(neon_widen_s8, i64, i32)
DEF_HELPER_1(neon_widen_u16, i64, i32)
DEF_HELPER_1(neon_widen_s16, i64, i32)

DEF_HELPER_2(neon_addl_u16, i64, i64, i64)
DEF_HELPER_2(neon_addl_u32, i64, i64, i64)
DEF_HELPER_2(neon_paddl_u16, i64, i64, i64)
DEF_HELPER_2(neon_paddl_u32, i64, i64, i64)
DEF_HELPER_2(neon_subl_u16, i64, i64, i64)
DEF_HELPER_2(neon_subl_u32, i64, i64, i64)
DEF_HELPER_3(neon_addl_saturate_s32, i64, env, i64, i64)
DEF_HELPER_3(neon_addl_saturate_s64, i64, env, i64, i64)
DEF_HELPER_2(neon_abdl_u16, i64, i32, i32)
DEF_HELPER_2(neon_abdl_s16, i64, i32, i32)
DEF_HELPER_2(neon_abdl_u32, i64, i32, i32)
DEF_HELPER_2(neon_abdl_s32, i64, i32, i32)
DEF_HELPER_2(neon_abdl_u64, i64, i32, i32)
DEF_HELPER_2(neon_abdl_s64, i64, i32, i32)
DEF_HELPER_2(neon_mull_u8, i64, i32, i32)
DEF_HELPER_2(neon_mull_s8, i64, i32, i32)
DEF_HELPER_2(neon_mull_u16, i64, i32, i32)
DEF_HELPER_2(neon_mull_s16, i64, i32, i32)

DEF_HELPER_1(neon_negl_u16, i64, i64)
DEF_HELPER_1(neon_negl_u32, i64, i64)

DEF_HELPER_2(neon_qabs_s8, i32, env, i32)
DEF_HELPER_2(neon_qabs_s16, i32, env, i32)
DEF_HELPER_2(neon_qabs_s32, i32, env, i32)
DEF_HELPER_2(neon_qneg_s8, i32, env, i32)
DEF_HELPER_2(neon_qneg_s16, i32, env, i32)
DEF_HELPER_2(neon_qneg_s32, i32, env, i32)

DEF_HELPER_3(neon_min_f32, i32, i32, i32, ptr)
DEF_HELPER_3(neon_max_f32, i32, i32, i32, ptr)
DEF_HELPER_3(neon_abd_f32, i32, i32, i32, ptr)
DEF_HELPER_3(neon_ceq_f32, i32, i32, i32, ptr)
DEF_HELPER_3(neon_cge_f32, i32, i32, i32, ptr)
DEF_HELPER_3(neon_cgt_f32, i32, i32, i32, ptr)
DEF_HELPER_3(neon_acge_f32, i32, i32, i32, ptr)
DEF_HELPER_3(neon_acgt_f32, i32, i32, i32, ptr)

/* iwmmxt_helper.c */
DEF_HELPER_2(iwmmxt_maddsq, i64, i64, i64)
DEF_HELPER_2(iwmmxt_madduq, i64, i64, i64)
DEF_HELPER_2(iwmmxt_sadb, i64, i64, i64)
DEF_HELPER_2(iwmmxt_sadw, i64, i64, i64)
DEF_HELPER_2(iwmmxt_mulslw, i64, i64, i64)
DEF_HELPER_2(iwmmxt_mulshw, i64, i64, i64)
DEF_HELPER_2(iwmmxt_mululw, i64, i64, i64)
DEF_HELPER_2(iwmmxt_muluhw, i64, i64, i64)
DEF_HELPER_2(iwmmxt_macsw, i64, i64, i64)
DEF_HELPER_2(iwmmxt_macuw, i64, i64, i64)
DEF_HELPER_1(iwmmxt_setpsr_nz, i32, i64)

#define DEF_IWMMXT_HELPER_SIZE_ENV(name)               \
    DEF_HELPER_3(iwmmxt_##name##b, i64, env, i64, i64) \
    DEF_HELPER_3(iwmmxt_##name##w, i64, env, i64, i64) \
    DEF_HELPER_3(iwmmxt_##name##l, i64, env, i64, i64)

DEF_IWMMXT_HELPER_SIZE_ENV(unpackl)
DEF_IWMMXT_HELPER_SIZE_ENV(unpackh)

DEF_HELPER_2(iwmmxt_unpacklub, i64, env, i64)
DEF_HELPER_2(iwmmxt_unpackluw, i64, env, i64)
DEF_HELPER_2(iwmmxt_unpacklul, i64, env, i64)
DEF_HELPER_2(iwmmxt_unpackhub, i64, env, i64)
DEF_HELPER_2(iwmmxt_unpackhuw, i64, env, i64)
DEF_HELPER_2(iwmmxt_unpackhul, i64, env, i64)
DEF_HELPER_2(iwmmxt_unpacklsb, i64, env, i64)
DEF_HELPER_2(iwmmxt_unpacklsw, i64, env, i64)
DEF_HELPER_2(iwmmxt_unpacklsl, i64, env, i64)
DEF_HELPER_2(iwmmxt_unpackhsb, i64, env, i64)
DEF_HELPER_2(iwmmxt_unpackhsw, i64, env, i64)
DEF_HELPER_2(iwmmxt_unpackhsl, i64, env, i64)

DEF_IWMMXT_HELPER_SIZE_ENV(cmpeq)
DEF_IWMMXT_HELPER_SIZE_ENV(cmpgtu)
DEF_IWMMXT_HELPER_SIZE_ENV(cmpgts)

DEF_IWMMXT_HELPER_SIZE_ENV(mins)
DEF_IWMMXT_HELPER_SIZE_ENV(minu)
DEF_IWMMXT_HELPER_SIZE_ENV(maxs)
DEF_IWMMXT_HELPER_SIZE_ENV(maxu)

DEF_IWMMXT_HELPER_SIZE_ENV(subn)
DEF_IWMMXT_HELPER_SIZE_ENV(addn)
DEF_IWMMXT_HELPER_SIZE_ENV(subu)
DEF_IWMMXT_HELPER_SIZE_ENV(addu)
DEF_IWMMXT_HELPER_SIZE_ENV(subs)
DEF_IWMMXT_HELPER_SIZE_ENV(adds)

DEF_HELPER_3(iwmmxt_avgb0, i64, env, i64, i64)
DEF_HELPER_3(iwmmxt_avgb1, i64, env, i64, i64)
DEF_HELPER_3(iwmmxt_avgw0, i64, env, i64, i64)
DEF_HELPER_3(iwmmxt_avgw1, i64, env, i64, i64)

DEF_HELPER_2(iwmmxt_msadb, i64, i64, i64)

DEF_HELPER_3(iwmmxt_align, i64, i64, i64, i32)
DEF_HELPER_4(iwmmxt_insr, i64, i64, i32, i32, i32)

DEF_HELPER_1(iwmmxt_bcstb, i64, i32)
DEF_HELPER_1(iwmmxt_bcstw, i64, i32)
DEF_HELPER_1(iwmmxt_bcstl, i64, i32)

DEF_HELPER_1(iwmmxt_addcb, i64, i64)
DEF_HELPER_1(iwmmxt_addcw, i64, i64)
DEF_HELPER_1(iwmmxt_addcl, i64, i64)

DEF_HELPER_1(iwmmxt_msbb, i32, i64)
DEF_HELPER_1(iwmmxt_msbw, i32, i64)
DEF_HELPER_1(iwmmxt_msbl, i32, i64)

DEF_HELPER_3(iwmmxt_srlw, i64, env, i64, i32)
DEF_HELPER_3(iwmmxt_srll, i64, env, i64, i32)
DEF_HELPER_3(iwmmxt_srlq, i64, env, i64, i32)
DEF_HELPER_3(iwmmxt_sllw, i64, env, i64, i32)
DEF_HELPER_3(iwmmxt_slll, i64, env, i64, i32)
DEF_HELPER_3(iwmmxt_sllq, i64, env, i64, i32)
DEF_HELPER_3(iwmmxt_sraw, i64, env, i64, i32)
DEF_HELPER_3(iwmmxt_sral, i64, env, i64, i32)
DEF_HELPER_3(iwmmxt_sraq, i64, env, i64, i32)
DEF_HELPER_3(iwmmxt_rorw, i64, env, i64, i32)
DEF_HELPER_3(iwmmxt_rorl, i64, env, i64, i32)
DEF_HELPER_3(iwmmxt_rorq, i64, env, i64, i32)
DEF_HELPER_3(iwmmxt_shufh, i64, env, i64, i32)

DEF_HELPER_3(iwmmxt_packuw, i64, env, i64, i64)
DEF_HELPER_3(iwmmxt_packul, i64, env, i64, i64)
DEF_HELPER_3(iwmmxt_packuq, i64, env, i64, i64)
DEF_HELPER_3(iwmmxt_packsw, i64, env, i64, i64)
DEF_HELPER_3(iwmmxt_packsl, i64, env, i64, i64)
DEF_HELPER_3(iwmmxt_packsq, i64, env, i64, i64)

DEF_HELPER_3(iwmmxt_muladdsl, i64, i64, i32, i32)
DEF_HELPER_3(iwmmxt_muladdsw, i64, i64, i32, i32)
DEF_HELPER_3(iwmmxt_muladdswl, i64, i64, i32, i32)

DEF_HELPER_2(set_teecr, void, env, i32)

DEF_HELPER_3(neon_unzip8, void, env, i32, i32)
DEF_HELPER_3(neon_unzip16, void, env, i32, i32)
DEF_HELPER_3(neon_qunzip8, void, env, i32, i32)
DEF_HELPER_3(neon_qunzip16, void, env, i32, i32)
DEF_HELPER_3(neon_qunzip32, void, env, i32, i32)
DEF_HELPER_3(neon_zip8, void, env, i32, i32)
DEF_HELPER_3(neon_zip16, void, env, i32, i32)
DEF_HELPER_3(neon_qzip8, void, env, i32, i32)
DEF_HELPER_3(neon_qzip16, void, env, i32, i32)
DEF_HELPER_3(neon_qzip32, void, env, i32, i32)

DEF_HELPER_0(set_system_event, void)

#ifdef TARGET_PROTO_ARM_M
DEF_HELPER_3(v8m_tt, i32, env, i32, i32)
#endif
DEF_HELPER_3(set_cp_reg, void, env, ptr, i32)
DEF_HELPER_3(set_cp_reg64, void, env, ptr, i64)
DEF_HELPER_2(get_cp_reg, i32, env, ptr)
DEF_HELPER_2(get_cp_reg64, i64, env, ptr)

DEF_HELPER_3(pmu_update_event_counters, void, env, int, i32)
DEF_HELPER_1(pmu_count_instructions_cycles, void, i32)

#include "def-helper.h"

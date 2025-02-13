#include "def-helper.h"

DEF_HELPER_2(exception, noreturn, env, i32)
DEF_HELPER_3(exception_cause, noreturn, env, i32, i32)
DEF_HELPER_4(exception_cause_vaddr, noreturn, env, i32, i32, i32)
DEF_HELPER_3(debug_exception, noreturn, env, i32, i32)

DEF_HELPER_1(sync_windowbase, void, env)
DEF_HELPER_4(entry, void, env, i32, i32, i32)
DEF_HELPER_2(test_ill_retw, void, env, i32)
DEF_HELPER_2(test_underflow_retw, void, env, i32)
DEF_HELPER_2(retw, void, env, i32)
DEF_HELPER_3(window_check, noreturn, env, i32, i32)
DEF_HELPER_1(restore_owb, void, env)
DEF_HELPER_2(movsp, void, env, i32)
DEF_HELPER_1(simcall, void, env)

DEF_HELPER_3(waiti, void, env, i32, i32)
DEF_HELPER_1(update_ccount, void, env)
DEF_HELPER_2(wsr_ccount, void, env, i32)
DEF_HELPER_2(update_ccompare, void, env, i32)
DEF_HELPER_1(check_interrupts, void, env)
DEF_HELPER_2(intset, void, env, i32)
DEF_HELPER_2(intclear, void, env, i32)
DEF_HELPER_3(check_atomctl, void, env, i32, i32)
DEF_HELPER_4(check_exclusive, void, env, i32, i32, i32)
DEF_HELPER_2(wsr_memctl, void, env, i32)

DEF_HELPER_2(itlb_hit_test, void, env, i32)
DEF_HELPER_2(wsr_rasid, void, env, i32)
DEF_HELPER_FLAGS_3(rtlb0, TCG_CALL_PURE | TCG_CALL_CONST, i32, env, i32, i32)
DEF_HELPER_FLAGS_3(rtlb1, TCG_CALL_PURE | TCG_CALL_CONST, i32, env, i32, i32)
DEF_HELPER_3(itlb, void, env, i32, i32)
DEF_HELPER_3(ptlb, i32, env, i32, i32)
DEF_HELPER_4(wtlb, void, env, i32, i32, i32)
DEF_HELPER_2(wsr_mpuenb, void, env, i32)
DEF_HELPER_3(wptlb, void, env, i32, i32)
DEF_HELPER_FLAGS_2(rptlb0, TCG_CALL_PURE | TCG_CALL_CONST, i32, env, i32)
DEF_HELPER_FLAGS_2(rptlb1, TCG_CALL_PURE | TCG_CALL_CONST, i32, env, i32)
DEF_HELPER_2(pptlb, i32, env, i32)

DEF_HELPER_2(wsr_ibreakenable, void, env, i32)
DEF_HELPER_3(wsr_ibreaka, void, env, i32, i32)
DEF_HELPER_3(wsr_dbreaka, void, env, i32, i32)
DEF_HELPER_3(wsr_dbreakc, void, env, i32, i32)

DEF_HELPER_2(wur_fpu2k_fcr, void, env, i32)
DEF_HELPER_FLAGS_1(abs_s, TCG_CALL_PURE | TCG_CALL_CONST, f32, f32)
DEF_HELPER_FLAGS_1(neg_s, TCG_CALL_PURE | TCG_CALL_CONST, f32, f32)
DEF_HELPER_3(fpu2k_add_s, f32, env, f32, f32)
DEF_HELPER_3(fpu2k_sub_s, f32, env, f32, f32)
DEF_HELPER_3(fpu2k_mul_s, f32, env, f32, f32)
DEF_HELPER_4(fpu2k_madd_s, f32, env, f32, f32, f32)
DEF_HELPER_4(fpu2k_msub_s, f32, env, f32, f32, f32)
DEF_HELPER_4(ftoi_s, i32, env, f32, i32, i32)
DEF_HELPER_4(ftoui_s, i32, env, f32, i32, i32)
DEF_HELPER_3(itof_s, f32, env, i32, i32)
DEF_HELPER_3(uitof_s, f32, env, i32, i32)
DEF_HELPER_2(cvtd_s, f64, env, f32)

DEF_HELPER_3(un_s, i32, env, f32, f32)
DEF_HELPER_3(oeq_s, i32, env, f32, f32)
DEF_HELPER_3(ueq_s, i32, env, f32, f32)
DEF_HELPER_3(olt_s, i32, env, f32, f32)
DEF_HELPER_3(ult_s, i32, env, f32, f32)
DEF_HELPER_3(ole_s, i32, env, f32, f32)
DEF_HELPER_3(ule_s, i32, env, f32, f32)

DEF_HELPER_2(wur_fpu_fcr, void, env, i32)
DEF_HELPER_1(rur_fpu_fsr, i32, env)
DEF_HELPER_2(wur_fpu_fsr, void, env, i32)
DEF_HELPER_FLAGS_1(abs_d, TCG_CALL_PURE | TCG_CALL_CONST, f64, f64)
DEF_HELPER_FLAGS_1(neg_d, TCG_CALL_PURE | TCG_CALL_CONST, f64, f64)
DEF_HELPER_3(add_d, f64, env, f64, f64)
DEF_HELPER_3(add_s, f32, env, f32, f32)
DEF_HELPER_3(sub_d, f64, env, f64, f64)
DEF_HELPER_3(sub_s, f32, env, f32, f32)
DEF_HELPER_3(mul_d, f64, env, f64, f64)
DEF_HELPER_3(mul_s, f32, env, f32, f32)
DEF_HELPER_4(madd_d, f64, env, f64, f64, f64)
DEF_HELPER_4(madd_s, f32, env, f32, f32, f32)
DEF_HELPER_4(msub_d, f64, env, f64, f64, f64)
DEF_HELPER_4(msub_s, f32, env, f32, f32, f32)
DEF_HELPER_3(mkdadj_d, f64, env, f64, f64)
DEF_HELPER_3(mkdadj_s, f32, env, f32, f32)
DEF_HELPER_2(mksadj_d, f64, env, f64)
DEF_HELPER_2(mksadj_s, f32, env, f32)
DEF_HELPER_4(ftoi_d, i32, env, f64, i32, i32)
DEF_HELPER_4(ftoui_d, i32, env, f64, i32, i32)
DEF_HELPER_3(itof_d, f64, env, i32, i32)
DEF_HELPER_3(uitof_d, f64, env, i32, i32)
DEF_HELPER_2(cvts_d, f32, env, f64)

DEF_HELPER_3(un_d, i32, env, f64, f64)
DEF_HELPER_3(oeq_d, i32, env, f64, f64)
DEF_HELPER_3(ueq_d, i32, env, f64, f64)
DEF_HELPER_3(olt_d, i32, env, f64, f64)
DEF_HELPER_3(ult_d, i32, env, f64, f64)
DEF_HELPER_3(ole_d, i32, env, f64, f64)
DEF_HELPER_3(ule_d, i32, env, f64, f64)

DEF_HELPER_2(rer, i32, env, i32)
DEF_HELPER_3(wer, void, env, i32, i32)

#include "def-helper.h"

#include "def-helper.h"

/* Exceptions */
DEF_HELPER_2(raise_exception, void, env, i32)
DEF_HELPER_1(raise_exception_debug, void, env)
DEF_HELPER_3(raise_exception_mbadaddr, void, env, i32, tl)

/* Floating Point - fused */
DEF_HELPER_5(fmadd_s, i64, env, i64, i64, i64, i64)
DEF_HELPER_5(fmadd_d, i64, env, i64, i64, i64, i64)
DEF_HELPER_5(fmsub_s, i64, env, i64, i64, i64, i64)
DEF_HELPER_5(fmsub_d, i64, env, i64, i64, i64, i64)
DEF_HELPER_5(fnmsub_s, i64, env, i64, i64, i64, i64)
DEF_HELPER_5(fnmsub_d, i64, env, i64, i64, i64, i64)
DEF_HELPER_5(fnmadd_s, i64, env, i64, i64, i64, i64)
DEF_HELPER_5(fnmadd_d, i64, env, i64, i64, i64, i64)

/* Floating Point - Single Precision */
DEF_HELPER_4(fadd_s, i64, env, i64, i64, i64)
DEF_HELPER_4(fsub_s, i64, env, i64, i64, i64)
DEF_HELPER_4(fmul_s, i64, env, i64, i64, i64)
DEF_HELPER_4(fdiv_s, i64, env, i64, i64, i64)
DEF_HELPER_3(fmin_s, i64, env, i64, i64)
DEF_HELPER_3(fmax_s, i64, env, i64, i64)
DEF_HELPER_3(fsqrt_s, i64, env, i64, i64)
DEF_HELPER_3(fle_s, tl, env, i64, i64)
DEF_HELPER_3(flt_s, tl, env, i64, i64)
DEF_HELPER_3(feq_s, tl, env, i64, i64)
DEF_HELPER_3(fcvt_w_s, tl, env, i64, i64)
DEF_HELPER_3(fcvt_wu_s, tl, env, i64, i64)
#if defined(TARGET_RISCV64)
DEF_HELPER_3(fcvt_l_s, i64, env, i64, i64)
DEF_HELPER_3(fcvt_lu_s, i64, env, i64, i64)
#endif
DEF_HELPER_3(fcvt_s_w, i64, env, tl, i64)
DEF_HELPER_3(fcvt_s_wu, i64, env, tl, i64)
#if defined(TARGET_RISCV64)
DEF_HELPER_3(fcvt_s_l, i64, env, i64, i64)
DEF_HELPER_3(fcvt_s_lu, i64, env, i64, i64)
#endif
DEF_HELPER_2(fclass_s, tl, env, i64)

/* Floating Point - Double Precision */
DEF_HELPER_4(fadd_d, i64, env, i64, i64, i64)
DEF_HELPER_4(fsub_d, i64, env, i64, i64, i64)
DEF_HELPER_4(fmul_d, i64, env, i64, i64, i64)
DEF_HELPER_4(fdiv_d, i64, env, i64, i64, i64)
DEF_HELPER_3(fmin_d, i64, env, i64, i64)
DEF_HELPER_3(fmax_d, i64, env, i64, i64)
DEF_HELPER_3(fcvt_s_d, i64, env, i64, i64)
DEF_HELPER_3(fcvt_d_s, i64, env, i64, i64)
DEF_HELPER_3(fsqrt_d, i64, env, i64, i64)
DEF_HELPER_3(fle_d, tl, env, i64, i64)
DEF_HELPER_3(flt_d, tl, env, i64, i64)
DEF_HELPER_3(feq_d, tl, env, i64, i64)
DEF_HELPER_3(fcvt_w_d, tl, env, i64, i64)
DEF_HELPER_3(fcvt_wu_d, tl, env, i64, i64)
#if defined(TARGET_RISCV64)
DEF_HELPER_3(fcvt_l_d, i64, env, i64, i64)
DEF_HELPER_3(fcvt_lu_d, i64, env, i64, i64)
#endif
DEF_HELPER_3(fcvt_d_w, i64, env, tl, i64)
DEF_HELPER_3(fcvt_d_wu, i64, env, tl, i64)
#if defined(TARGET_RISCV64)
DEF_HELPER_3(fcvt_d_l, i64, env, i64, i64)
DEF_HELPER_3(fcvt_d_lu, i64, env, i64, i64)
#endif
DEF_HELPER_2(fclass_d, tl, env, i64)

/* Special functions */
DEF_HELPER_3(csrrw, tl, env, tl, tl)
DEF_HELPER_4(csrrs, tl, env, tl, tl, tl)
DEF_HELPER_4(csrrc, tl, env, tl, tl, tl)
DEF_HELPER_2(sret, tl, env, tl)
DEF_HELPER_2(mret, tl, env, tl)
DEF_HELPER_1(wfi, void, env)
DEF_HELPER_1(tlb_flush, void, env)
DEF_HELPER_1(fence_i, void, env)

DEF_HELPER_1(acquire_global_memory_lock, void, env)
DEF_HELPER_1(release_global_memory_lock, void, env)
DEF_HELPER_2(reserve_address, void, env, tl)
DEF_HELPER_2(check_address_reservation, tl, env, tl)

/* Vector Extension */
DEF_HELPER_5(vsetvl, tl, env, tl, tl, tl, tl)

DEF_HELPER_2(handle_custom_instruction, i32, i64, i64)

void do_nmi(CPUState *env);

#include "def-helper.h"

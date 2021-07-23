#include "cpu.h"

static inline void require_vec(CPUState *env)
{
    if (!(env->mstatus & MSTATUS_VS)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
}

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

/*
 * Handle configuration to vector registers
 *
 * Adapted from Spike's processor_t::vectorUnit_t::set_vl
 */
target_ulong helper_vsetvl(CPUState *env, target_ulong rd, target_ulong rs1, target_ulong rs1_pass, target_ulong rs2_pass)
{
    require_vec(env);

    target_ulong prev_csr_vl = env->vl;
    target_ulong vlen = env->vlenb * 8;

    env->vtype = rs2_pass;
    env->vsew = 1 << (GET_VTYPE_VSEW(rs2_pass) + 3);
    env->vlmul = GET_VTYPE_VLMUL(rs2_pass);
    int8_t vlmul = (int8_t)(env->vlmul << 5) >> 5;
    env->vflmul = vlmul >= 0 ? 1 << vlmul : 1.0 / (1 << -vlmul);
    env->vlmax = (target_ulong)(vlen / env->vsew * env->vflmul);
    env->vta = GET_VTYPE_VTA(rs2_pass);
    env->vma = GET_VTYPE_VMA(rs2_pass);

    float ceil_vfmul = MIN(env->vflmul, 1.0f);
    env->vill = !(env->vflmul >= 0.125 && env->vflmul <= 8)
           || env->vsew > (ceil_vfmul * env->elen)
           || (rs2_pass >> 8) != 0;

    if (env->vill) {
        env->vtype |= ((target_ulong)1) << (TARGET_LONG_BITS - 1);
        env->vlmax = 0;
    }

    if (env->vlmax == 0) {
        env->vl = 0;
    } else if (rd == 0 && rs1 == 0) {
        // Keep existing VL value
        env->vl = MIN(prev_csr_vl, env->vlmax);
    } else if (rs1 == 0 && rd != 0) {
        env->vl = env->vlmax;
    } else {  // Normal stripmining (rs1 != 0)
        env->vl = MIN(rs1_pass, env->vlmax);
    }
    env->vstart = 0;
    return env->vl;
}

void helper_vmv_ivi(CPUState *env, uint32_t vd, int64_t imm)
{
    if (V_IDX_INVALID(vd)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong dst_eew = env->vsew;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
        switch (dst_eew) {
        case 8:
            V(vd)[ei] = imm;
            break;
        case 16:
            ((uint16_t *)V(vd))[ei] = imm;
            break;
        case 32:
            ((uint32_t *)V(vd))[ei] = imm;
            break;
        case 64: 
            ((uint64_t *)V(vd))[ei] = imm;
            break;
        default:
            tlib_abortf("Unsupported EEW");
            break;
        }
    }
}

void helper_vmv_ivv(CPUState *env, uint32_t vd, int32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int i = env->vstart; i < env->vl; ++i) {
        switch (eew) {
        case 8:
            V(vd)[i] = V(vs1)[i];
            break;
        case 16:
            ((uint16_t *)V(vd))[i] = ((uint16_t *)V(vs1))[i];
            break;
        case 32:
            ((uint32_t *)V(vd))[i] = ((uint32_t *)V(vs1))[i];
            break;
        case 64: 
            ((uint64_t *)V(vd))[i] = ((uint64_t *)V(vs1))[i];
            break;
        default:
            tlib_abortf("Unsupported EEW");
            break;
        }
    }
}

void helper_vadd_ivi(CPUState *env, uint32_t vd, int32_t vs2, int64_t imm)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
        switch (eew) {
        case 8:
            V(vd)[ei] = V(vs2)[ei] + imm;
            break;
        case 16:
            ((uint16_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei] + imm;
            break;
        case 32:
            ((uint32_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] + imm;
            break;
        case 64: 
            ((uint64_t *)V(vd))[ei] = ((uint64_t *)V(vs2))[ei] + imm;
            break;
        default:
            tlib_abortf("Unsupported EEW");
            break;
        }
    }
}

void helper_vadd_ivi_m(CPUState *env, uint32_t vd, int32_t vs2, int64_t imm)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
        if (!env->vma && !(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
        switch (eew) {
        case 8:
            V(vd)[ei] = V(vs2)[ei] + imm;
            break;
        case 16:
            ((uint16_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei] + imm;
            break;
        case 32:
            ((uint32_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] + imm;
            break;
        case 64: 
            ((uint64_t *)V(vd))[ei] = ((uint64_t *)V(vs2))[ei] + imm;
            break;
        default:
            tlib_abortf("Unsupported EEW");
            break;
        }
    }
}

void helper_vadd_ivv(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
        switch (eew) {
        case 8:
            V(vd)[ei] = V(vs2)[ei] + V(vs1)[ei];
            break;
        case 16:
            ((uint16_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei] + ((uint16_t *)V(vs1))[ei];
            break;
        case 32:
            ((uint32_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] + ((uint32_t *)V(vs1))[ei];
            break;
        case 64: 
            ((uint64_t *)V(vd))[ei] = ((uint64_t *)V(vs2))[ei] + ((uint64_t *)V(vs1))[ei];
            break;
        default:
            tlib_abortf("Unsupported EEW");
            break;
        }
    }
}

void helper_vadd_ivv_m(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
        if (!env->vma && !(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
        switch (eew) {
        case 8:
            V(vd)[ei] = V(vs2)[ei] + V(vs1)[ei];
            break;
        case 16:
            ((uint16_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei] + ((uint16_t *)V(vs1))[ei];
            break;
        case 32:
            ((uint32_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] + ((uint32_t *)V(vs1))[ei];
            break;
        case 64: 
            ((uint64_t *)V(vd))[ei] = ((uint64_t *)V(vs2))[ei] + ((uint64_t *)V(vs1))[ei];
            break;
        default:
            tlib_abortf("Unsupported EEW");
            break;
        }
    }
}

void helper_vsub_ivv(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
        switch (eew) {
        case 8:
            V(vd)[ei] = V(vs2)[ei] - V(vs1)[ei];
            break;
        case 16:
            ((uint16_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei] - ((uint16_t *)V(vs1))[ei];
            break;
        case 32:
            ((uint32_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] - ((uint32_t *)V(vs1))[ei];
            break;
        case 64: 
            ((uint64_t *)V(vd))[ei] = ((uint64_t *)V(vs2))[ei] - ((uint64_t *)V(vs1))[ei];
            break;
        default:
            tlib_abortf("Unsupported EEW");
            break;
        }
    }
}

void helper_vsub_ivv_m(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
        if (!env->vma && !(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
        switch (eew) {
        case 8:
            V(vd)[ei] = V(vs2)[ei] - V(vs1)[ei];
            break;
        case 16:
            ((uint16_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei] - ((uint16_t *)V(vs1))[ei];
            break;
        case 32:
            ((uint32_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] - ((uint32_t *)V(vs1))[ei];
            break;
        case 64: 
            ((uint64_t *)V(vd))[ei] = ((uint64_t *)V(vs2))[ei] - ((uint64_t *)V(vs1))[ei];
            break;
        default:
            tlib_abortf("Unsupported EEW");
            break;
        }
    }
}

void helper_vrsub_ivi(CPUState *env, uint32_t vd, int32_t vs2, int64_t imm)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
        switch (eew) {
        case 8:
            V(vd)[ei] = - V(vs2)[ei] + imm;
            break;
        case 16:
            ((uint16_t *)V(vd))[ei] = - ((uint16_t *)V(vs2))[ei] + imm;
            break;
        case 32:
            ((uint32_t *)V(vd))[ei] = - ((uint32_t *)V(vs2))[ei] + imm;
            break;
        case 64: 
            ((uint64_t *)V(vd))[ei] = - ((uint64_t *)V(vs2))[ei] + imm;
            break;
        default:
            tlib_abortf("Unsupported EEW");
            break;
        }
    }
}

void helper_vrsub_ivi_m(CPUState *env, uint32_t vd, int32_t vs2, int64_t imm)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
        if (!env->vma && !(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
        switch (eew) {
        case 8:
            V(vd)[ei] = V(vs2)[ei] + imm;
            break;
        case 16:
            ((uint16_t *)V(vd))[ei] = - ((uint16_t *)V(vs2))[ei] + imm;
            break;
        case 32:
            ((uint32_t *)V(vd))[ei] = - ((uint32_t *)V(vs2))[ei] + imm;
            break;
        case 64: 
            ((uint64_t *)V(vd))[ei] = - ((uint64_t *)V(vs2))[ei] + imm;
            break;
        default:
            tlib_abortf("Unsupported EEW");
            break;
        }
    }
}

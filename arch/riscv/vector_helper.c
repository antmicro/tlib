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
target_ulong helper_vsetvl(CPUState *env, target_ulong rd, target_ulong rs1,
                           target_ulong rs1_pass, target_ulong rs2_pass,
                           uint32_t is_rs1_imm)
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
    if (is_rs1_imm == 1) {  // vsetivli
        env->vl = MIN(rs1_pass, env->vlmax);
    } else if (env->vlmax == 0) {  // AVL encoding for vsetvl and vsetvli
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
            ((uint8_t *)V(vd))[ei] = imm;
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
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
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
            ((uint8_t *)V(vd))[i] = ((uint8_t *)V(vs1))[i];
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
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void helper_vfmv_vf(CPUState *env, uint32_t vd, uint64_t f1)
{
    if (V_IDX_INVALID(vd)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    switch (eew) {
    case 32:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    case 64:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = f1;
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = f1;
            break;
        }
    }
}

void helper_vmerge_ivv(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
        uint8_t mask = !(V(0)[ei >> 3] & (1 << (ei & 0x7)));
        switch (eew) {
        case 8:
            ((int8_t *)V(vd))[ei] = mask ? ((int8_t *)V(vs2))[ei] : ((int8_t *)V(vs1))[ei];
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = mask ? ((int16_t *)V(vs2))[ei] : ((int16_t *)V(vs1))[ei];
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = mask ? ((int32_t *)V(vs2))[ei] : ((int32_t *)V(vs1))[ei];
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = mask ? ((int64_t *)V(vs2))[ei] : ((int64_t *)V(vs1))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void helper_vmerge_ivi(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
        uint8_t mask = !(V(0)[ei >> 3] & (1 << (ei & 0x7)));
        switch (eew) {
        case 8:
            ((int8_t *)V(vd))[ei] = mask ? ((int8_t *)V(vs2))[ei] : rs1;
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = mask ? ((int16_t *)V(vs2))[ei] : rs1;
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = mask ? ((int32_t *)V(vs2))[ei] : rs1;
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = mask ? ((int64_t *)V(vs2))[ei] : rs1;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void helper_vfmerge_vfm(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    switch (eew) {
    case 32:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    case 64:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
        uint8_t mask = !(V(0)[ei >> 3] & (1 << (ei & 0x7)));
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = mask ? ((uint32_t *)V(vs2))[ei] : f1;
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = mask ? ((uint64_t *)V(vs2))[ei] : f1;
            break;
        }
    }
}

void helper_vcompress_mvv(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (env->vstart != 0 || V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    int di = 0;
    for (int i = 0; i < env->vl; ++i) {
        if (!(V(vs1)[i >> 3] & (1 << (i & 0x7)))) {
            continue;
        }
        switch (eew) {
        case 8:
            ((uint8_t *)V(vd))[di] = ((uint8_t *)V(vs2))[i];
            break;
        case 16:
            ((uint16_t *)V(vd))[di] = ((uint16_t *)V(vs2))[i];
            break;
        case 32:
            ((uint32_t *)V(vd))[di] = ((uint32_t *)V(vs2))[i];
            break;
        case 64:
            ((uint64_t *)V(vd))[di] = ((uint64_t *)V(vs2))[i];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
        di += 1;
    }
}

void helper_vadc_vvm(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int i = 0; i < env->vl; ++i) {
        uint8_t carry = !!(V(0)[i >> 3] & (1 << (i & 0x7)));
        switch (eew) {
        case 8:
            ((uint8_t *)V(vd))[i] = ((uint8_t *)V(vs2))[i] + ((uint8_t *)V(vs1))[i] + carry;
            break;
        case 16:
            ((uint16_t *)V(vd))[i] = ((uint16_t *)V(vs2))[i] + ((uint16_t *)V(vs1))[i] + carry;
            break;
        case 32:
            ((uint32_t *)V(vd))[i] = ((uint32_t *)V(vs2))[i] + ((uint32_t *)V(vs1))[i] + carry;
            break;
        case 64:
            ((uint64_t *)V(vd))[i] = ((uint64_t *)V(vs2))[i] + ((uint64_t *)V(vs1))[i] + carry;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void helper_vmadc_vv(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int i = 0; i < env->vl; ++i) {
        if (!(i & 0x7)) {
            V(vd)[i >> 3] = 0;
        }
        switch (eew) {
        case 8: {
                uint8_t a = ((uint8_t *)V(vs2))[i];
                V(vd)[i >> 3] |= (a + ((uint8_t *)V(vs1))[i] < a) << (i & 0x7);
                break;
            }
        case 16: {
                uint16_t a = ((uint16_t *)V(vs2))[i];
                V(vd)[i >> 3] |= (a + ((uint16_t *)V(vs1))[i] < a) << (i & 0x7);
                break;
            }
        case 32: {
                uint32_t a = ((uint32_t *)V(vs2))[i];
                V(vd)[i >> 3] |= (a + ((uint32_t *)V(vs1))[i] < a) << (i & 0x7);
                break;
            }
        case 64: {
                uint64_t a = ((uint64_t *)V(vs2))[i];
                V(vd)[i >> 3] |= (a + ((uint64_t *)V(vs1))[i] < a) << (i & 0x7);
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void helper_vmadc_vvm(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int i = 0; i < env->vl; ++i) {
        if (!(i & 0x7)) {
            V(vd)[i >> 3] = 0;
        }
        uint8_t carry = !!(V(0)[i >> 3] & (1 << (i & 0x7)));
        switch (eew) {
        case 8: {
                uint8_t a = ((uint8_t *)V(vs2))[i];
                uint8_t ab = a + ((uint8_t *)V(vs1))[i];
                V(vd)[i >> 3] |= (ab < a || (carry && !(ab + 1))) << (i & 0x7);
                break;
            }
        case 16: {
                uint16_t a = ((uint16_t *)V(vs2))[i];
                uint16_t ab = a + ((uint16_t *)V(vs1))[i];
                V(vd)[i >> 3] |= (ab < a || (carry && !(ab + 1))) << (i & 0x7);
                break;
            }
        case 32: {
                uint32_t a = ((uint32_t *)V(vs2))[i];
                uint32_t ab = a + ((uint32_t *)V(vs1))[i];
                V(vd)[i >> 3] |= (ab < a || (carry && !(ab + 1))) << (i & 0x7);
                break;
            }
        case 64: {
                uint64_t a = ((uint64_t *)V(vs2))[i];
                uint64_t ab = a + ((uint64_t *)V(vs1))[i];
                V(vd)[i >> 3] |= (ab < a || (carry && !(ab + 1))) << (i & 0x7);
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void helper_vsbc_vvm(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int i = 0; i < env->vl; ++i) {
        uint8_t borrow = !!(V(0)[i >> 3] & (1 << (i & 0x7)));
        switch (eew) {
        case 8:
            ((uint8_t *)V(vd))[i] = ((uint8_t *)V(vs2))[i] - ((uint8_t *)V(vs1))[i] - borrow;
            break;
        case 16:
            ((uint16_t *)V(vd))[i] = ((uint16_t *)V(vs2))[i] - ((uint16_t *)V(vs1))[i] - borrow;
            break;
        case 32:
            ((uint32_t *)V(vd))[i] = ((uint32_t *)V(vs2))[i] - ((uint32_t *)V(vs1))[i] - borrow;
            break;
        case 64:
            ((uint64_t *)V(vd))[i] = ((uint64_t *)V(vs2))[i] - ((uint64_t *)V(vs1))[i] - borrow;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void helper_vmsbc_vv(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int i = 0; i < env->vl; ++i) {
        if (!(i & 0x7)) {
            V(vd)[i >> 3] = 0;
        }
        switch (eew) {
        case 8:
            V(vd)[i >> 3] |= (((uint8_t *)V(vs2))[i] < ((uint8_t *)V(vs1))[i]) << (i & 0x7);
            break;
        case 16:
            V(vd)[i >> 3] |= (((uint16_t *)V(vs2))[i] < ((uint16_t *)V(vs1))[i]) << (i & 0x7);
            break;
        case 32:
            V(vd)[i >> 3] |= (((uint32_t *)V(vs2))[i] < ((uint32_t *)V(vs1))[i]) << (i & 0x7);
            break;
        case 64:
            V(vd)[i >> 3] |= (((uint64_t *)V(vs2))[i] < ((uint64_t *)V(vs1))[i]) << (i & 0x7);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void helper_vmsbc_vvm(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int i = 0; i < env->vl; ++i) {
        if (!(i & 0x7)) {
            V(vd)[i >> 3] = 0;
        }
        uint8_t borrow = !!(V(0)[i >> 3] & (1 << (i & 0x7)));
        switch (eew) {
        case 8: {
                uint8_t b = ((uint8_t *)V(vs1))[i];
                V(vd)[i >> 3] |= (((uint8_t *)V(vs2))[i] < b || (borrow && !(b + 1))) << (i & 0x7);
                break;
            }
        case 16: {
                uint16_t b = ((uint16_t *)V(vs1))[i];
                V(vd)[i >> 3] |= (((uint16_t *)V(vs2))[i] < b || (borrow && !(b + 1))) << (i & 0x7);
                break;
            }
        case 32: {
                uint32_t b = ((uint32_t *)V(vs1))[i];
                V(vd)[i >> 3] |= (((uint32_t *)V(vs2))[i] < b || (borrow && !(b + 1))) << (i & 0x7);
                break;
            }
        case 64: {
                uint64_t b = ((uint64_t *)V(vs1))[i];
                V(vd)[i >> 3] |= (((uint64_t *)V(vs2))[i] < b || (borrow && !(b + 1))) << (i & 0x7);
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void helper_vadc_vi(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int i = 0; i < env->vl; ++i) {
        uint8_t carry = !!(V(0)[i >> 3] & (1 << (i & 0x7)));
        switch (eew) {
        case 8:
            ((uint8_t *)V(vd))[i] = ((uint8_t *)V(vs2))[i] + rs1 + carry;
            break;
        case 16:
            ((uint16_t *)V(vd))[i] = ((uint16_t *)V(vs2))[i] + rs1 + carry;
            break;
        case 32:
            ((uint32_t *)V(vd))[i] = ((uint32_t *)V(vs2))[i] + rs1 + carry;
            break;
        case 64:
            ((uint64_t *)V(vd))[i] = ((uint64_t *)V(vs2))[i] + rs1 + carry;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void helper_vmadc_vi(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
{
    if (V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int i = 0; i < env->vl; ++i) {
        if (!(i & 0x7)) {
            V(vd)[i >> 3] = 0;
        }
        switch (eew) {
        case 8: {
                uint8_t a = ((uint8_t *)V(vs2))[i];
                V(vd)[i >> 3] |= (a + rs1 < a) << (i & 0x7);
                break;
            }
        case 16: {
                uint16_t a = ((uint16_t *)V(vs2))[i];
                V(vd)[i >> 3] |= (a + rs1 < a) << (i & 0x7);
                break;
            }
        case 32: {
                uint32_t a = ((uint32_t *)V(vs2))[i];
                V(vd)[i >> 3] |= (a + rs1 < a) << (i & 0x7);
                break;
            }
        case 64: {
                uint64_t a = ((uint64_t *)V(vs2))[i];
                V(vd)[i >> 3] |= (a + rs1 < a) << (i & 0x7);
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void helper_vmadc_vim(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
{
    if (V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int i = 0; i < env->vl; ++i) {
        if (!(i & 0x7)) {
            V(vd)[i >> 3] = 0;
        }
        uint8_t carry = !!(V(0)[i >> 3] & (1 << (i & 0x7)));
        switch (eew) {
        case 8: {
                uint8_t a = ((uint8_t *)V(vs2))[i];
                uint64_t ab = a + rs1;
                V(vd)[i >> 3] |= (ab < a || (carry && !(ab + 1))) << (i & 0x7);
                break;
            }
        case 16: {
                uint16_t a = ((uint16_t *)V(vs2))[i];
                uint64_t ab = a + rs1;
                V(vd)[i >> 3] |= (ab < a || (carry && !(ab + 1))) << (i & 0x7);
                break;
            }
        case 32: {
                uint32_t a = ((uint32_t *)V(vs2))[i];
                uint32_t ab = a + rs1;
                V(vd)[i >> 3] |= (ab < a || (carry && !(ab + 1))) << (i & 0x7);
                break;
            }
        case 64: {
                uint64_t a = ((uint64_t *)V(vs2))[i];
                uint64_t ab = a + rs1;
                V(vd)[i >> 3] |= (ab < a || (carry && !(ab + 1))) << (i & 0x7);
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void helper_vsbc_vi(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int i = 0; i < env->vl; ++i) {
        uint8_t borrow = !!(V(0)[i >> 3] & (1 << (i & 0x7)));
        switch (eew) {
        case 8:
            ((uint8_t *)V(vd))[i] = ((uint8_t *)V(vs2))[i] - rs1 - borrow;
            break;
        case 16:
            ((uint16_t *)V(vd))[i] = ((uint16_t *)V(vs2))[i] - rs1 - borrow;
            break;
        case 32:
            ((uint32_t *)V(vd))[i] = ((uint32_t *)V(vs2))[i] - rs1 - borrow;
            break;
        case 64:
            ((uint64_t *)V(vd))[i] = ((uint64_t *)V(vs2))[i] - rs1 - borrow;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void helper_vmsbc_vi(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
{
    if (V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int i = 0; i < env->vl; ++i) {
        if (!(i & 0x7)) {
            V(vd)[i >> 3] = 0;
        }
        switch (eew) {
        case 8:
            V(vd)[i >> 3] |= (((uint8_t *)V(vs2))[i] < rs1) << (i & 0x7);
            break;
        case 16:
            V(vd)[i >> 3] |= (((uint16_t *)V(vs2))[i] < rs1) << (i & 0x7);
            break;
        case 32:
            V(vd)[i >> 3] |= (((uint32_t *)V(vs2))[i] < rs1) << (i & 0x7);
            break;
        case 64:
            V(vd)[i >> 3] |= (((uint64_t *)V(vs2))[i] < rs1) << (i & 0x7);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void helper_vmsbc_vim(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
{
    if (V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int i = 0; i < env->vl; ++i) {
        if (!(i & 0x7)) {
            V(vd)[i >> 3] = 0;
        }
        uint8_t borrow = !!(V(0)[i >> 3] & (1 << (i & 0x7)));
        switch (eew) {
        case 8:
            V(vd)[i >> 3] |= (((uint8_t *)V(vs2))[i] < rs1 || (borrow && !(rs1 + 1))) << (i & 0x7);
            break;
        case 16:
            V(vd)[i >> 3] |= (((uint16_t *)V(vs2))[i] < rs1 || (borrow && !(rs1 + 1))) << (i & 0x7);
            break;
        case 32:
            V(vd)[i >> 3] |= (((uint32_t *)V(vs2))[i] < rs1 || (borrow && !(rs1 + 1))) << (i & 0x7);
            break;
        case 64:
            V(vd)[i >> 3] |= (((uint64_t *)V(vs2))[i] < rs1 || (borrow && !(rs1 + 1))) << (i & 0x7);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

target_ulong helper_vmv_xs(CPUState *env, int32_t vs2)
{
    const target_ulong eew = env->vsew;
    switch (eew) {
    case 8:
        return ((uint8_t *)V(vs2))[0];
    case 16:
        return ((uint16_t *)V(vs2))[0];
    case 32:
        return ((uint32_t *)V(vs2))[0];
    case 64:
        return ((uint64_t *)V(vs2))[0];
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return 0;
    }
}

void helper_vmv_sx(CPUState *env, uint32_t vd, target_ulong rs1)
{
    const target_ulong eew = env->vsew;
    switch (eew) {
    case 8:
        ((uint8_t *)V(vd))[0] = rs1;
        break;
    case 16:
        ((uint16_t *)V(vd))[0] = rs1;
        break;
    case 32:
        ((uint32_t *)V(vd))[0] = rs1;
        break;
    case 64:
        ((uint64_t *)V(vd))[0] = rs1;
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        break;
    }
}

void helper_vmv1r_v(CPUState *env, uint32_t vd, uint32_t vs2)
{
    const uint8_t emul = 0;
    if (V_IDX_INVALID_EMUL(vd, emul) || V_IDX_INVALID_EMUL(vs2, emul)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    memcpy(V(vd), V(vs2), env->vlen * env->vsew << emul);
}

void helper_vmv2r_v(CPUState *env, uint32_t vd, uint32_t vs2)
{
    const uint8_t emul = 1;
    if (V_IDX_INVALID_EMUL(vd, emul) || V_IDX_INVALID_EMUL(vs2, emul)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    memcpy(V(vd), V(vs2), env->vlen * env->vsew << emul);
}

void helper_vmv4r_v(CPUState *env, uint32_t vd, uint32_t vs2)
{
    const uint8_t emul = 2;
    if (V_IDX_INVALID_EMUL(vd, emul) || V_IDX_INVALID_EMUL(vs2, emul)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    memcpy(V(vd), V(vs2), env->vlen * env->vsew << emul);
}

void helper_vmv8r_v(CPUState *env, uint32_t vd, uint32_t vs2)
{
    const uint8_t emul = 3;
    if (V_IDX_INVALID_EMUL(vd, emul) || V_IDX_INVALID_EMUL(vs2, emul)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    memcpy(V(vd), V(vs2), env->vlen * env->vsew << emul);
}

#define MASK_OP_GEN(name, OP)                                                                                               \
void glue(glue(helper_vm, name), _mm)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)                               \
{                                                                                                                           \
    int i = env->vstart >> 3;                                                                                               \
    if (env->vl - env->vstart < 8) {                                                                                        \
        V(vd)[i] ^= ((0xffu << (env->vstart & 0x7)) & (0xffu >> (env->vl & 0x7))) & (V(vd)[i] ^ OP(V(vs2)[i], V(vs1)[i]));  \
        return;                                                                                                             \
    }                                                                                                                       \
    V(vd)[i] ^= (0xffu << (env->vstart & 0x7)) & (V(vd)[i] ^ OP(V(vs2)[i], V(vs1)[i]));                                     \
                                                                                                                            \
    for (; i < env->vl >> 3; ++i) {                                                                                         \
        V(vd)[i] = OP(V(vs2)[i], V(vs1)[i]);                                                                                \
    }                                                                                                                       \
    if (env->vl & 0x7) {                                                                                                    \
        V(vd)[i] ^= (0xffu >> (env->vl & 0x7)) & (V(vd)[i] ^ OP(V(vs2)[i], V(vs1)[i]));                                     \
    }                                                                                                                       \
}

#define MASK_OP_GEN_OP_AND(a, b) ((a) & (b))
MASK_OP_GEN(and, MASK_OP_GEN_OP_AND)
#define MASK_OP_GEN_OP_NAND(a, b) (~((a) & (b)))
MASK_OP_GEN(nand, MASK_OP_GEN_OP_NAND)
#define MASK_OP_GEN_OP_ANDNOT(a, b) ((a) & ~(b))
MASK_OP_GEN(andnot, MASK_OP_GEN_OP_ANDNOT)
#define MASK_OP_GEN_OP_XOR(a, b) ((a) ^ (b))
MASK_OP_GEN(xor, MASK_OP_GEN_OP_XOR)
#define MASK_OP_GEN_OP_OR(a, b) ((a) | (b))
MASK_OP_GEN(or, MASK_OP_GEN_OP_OR)
#define MASK_OP_GEN_OP_NOR(a, b) (~((a) & (b)))
MASK_OP_GEN(nor, MASK_OP_GEN_OP_NOR)
#define MASK_OP_GEN_OP_ORNOT(a, b) ((a) & ~(b))
MASK_OP_GEN(ornot, MASK_OP_GEN_OP_ORNOT)
#define MASK_OP_GEN_OP_XNOR(a, b) (~((a) ^ (b)))
MASK_OP_GEN(xnor, MASK_OP_GEN_OP_XNOR)

static uint8_t bitcnt(uint8_t a)
{
    a = (a & 0x55) + ((a >> 1) & 0x55);
    a = (a & 0x33) + ((a >> 2) & 0x33);
    return (a & 0x0f) + ((a >> 4) & 0x0f);
}

target_ulong helper_vpopc(CPUState *env, uint32_t vs2)
{
    target_ulong cnt = 0;
    int i = env->vstart >> 3;
    if (env->vl - env->vstart < 8) {
        return bitcnt(((0xffu << (env->vstart & 0x7)) & (0xffu >> (env->vl & 0x7))) & V(vs2)[i]);
    }
    cnt += bitcnt((0xffu << (env->vstart & 0x7)) & V(vs2)[i]);

    for (; i < env->vl >> 3; ++i) {
        cnt += bitcnt(V(vs2)[i]);
    }
    if (env->vl & 0x7) {
        cnt += bitcnt((0xffu >> (env->vl & 0x7)) & V(vs2)[i]);
    }
    return cnt;
}

target_ulong helper_vpopc_m(CPUState *env, uint32_t vs2)
{
    target_ulong cnt = 0;
    int i = env->vstart >> 3;
    if (env->vl - env->vstart < 8) {
        return bitcnt(((0xffu << (env->vstart & 0x7)) & (0xffu >> (env->vl & 0x7))) & V(0)[i] & V(vs2)[i]);
    }
    cnt += bitcnt((0xffu << (env->vstart & 0x7)) & V(0)[i] & V(vs2)[i]);

    for (; i < env->vl >> 3; ++i) {
        cnt += bitcnt(V(0)[i] & V(vs2)[i]);
    }
    if (env->vl & 0x7) {
        cnt += bitcnt((0xffu >> (env->vl & 0x7)) & V(0)[i] & V(vs2)[i]);
    }
    return cnt;
}

static target_long first_bit(uint8_t a)
{
    target_long idx = a & 0xf ? 4 : 0;
    idx += (a >> idx) & 0x3 ? 2 : 0; 
    idx += (a >> idx) & 0x1 ? 1 : 0; 
    return idx - 1;
}

target_long helper_vfirst(CPUState *env, uint32_t vs2)
{
    if (env->vstart) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    target_long tmp = 0;
    int i = 0;
    for (; i < env->vl >> 3; ++i) {
        tmp = first_bit(V(vs2)[i]);
        if (~tmp) {
            return (i << 3) + tmp;
        }
    }
    if (env->vl & 0x7) {
        tmp = first_bit((0xffu >> (env->vl & 0x7)) & V(vs2)[i]);
    }
    return ~tmp ? (i << 3) + tmp : tmp;
}

target_long helper_vfirst_m(CPUState *env, uint32_t vs2)
{
    if (env->vstart) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    target_long tmp = 0;
    int i = 0;
    for (; i < env->vl >> 3; ++i) {
        tmp = first_bit(V(0)[i] & V(vs2)[i]);
        if (~tmp) {
            return (i << 3) + tmp;
        }
    }
    if (env->vl & 0x7) {
        tmp = first_bit((0xffu >> (env->vl & 0x7)) & V(0)[i] & V(vs2)[i]);
    }
    return ~tmp ? (i << 3) + tmp : tmp;
}

static uint8_t set_before_first_bit(uint8_t a) {
    return (a & -a) - 1;
}

void helper_vmsbf(CPUState *env, uint32_t vd, uint32_t vs2)
{
    if (env->vstart) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    target_long tmp = 0;
    int i = 0;
    for (; i < env->vl >> 3; ++i) {
        tmp = set_before_first_bit(V(vs2)[i]);
        if (~tmp) {
            break;
        }
        V(vd)[i] |= 0xff;
    }
    if (!~tmp && env->vl & 0x7) {
        tmp = set_before_first_bit((0xffu >> (env->vl & 0x7)) & V(vs2)[i]);
        if (~tmp) {
            V(vd)[i] |= (0xffu >> (env->vl & 0x7)) & tmp;
            V(vd)[i] &= ~(0xffu >> (env->vl & 0x7)) | tmp;
        } else {
            V(vd)[i] |= (0xffu >> (env->vl & 0x7));
        }
        return;
    }

    V(vd)[i] |= tmp;
    V(vd)[i] &= tmp;

    for (; i < env->vl >> 3; ++i) {
        V(vd)[i] &= 0xff;
    }
    if (env->vl & 0x7) {
        V(vd)[i] &= ~(0xffu >> (env->vl & 0x7));
    }
}

void helper_vmsbf_m(CPUState *env, uint32_t vd, uint32_t vs2)
{
    if (env->vstart) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    target_long tmp = 0;
    int i = 0;
    for (; i < env->vl >> 3; ++i) {
        tmp = set_before_first_bit(V(0)[i] & V(vs2)[i]);
        if (~tmp) {
            break;
        }
        V(vd)[i] |= V(0)[i];
    }
    if (!~tmp && env->vl & 0x7) {
        tmp = set_before_first_bit((0xffu >> (env->vl & 0x7)) & V(0)[i] & V(vs2)[i]);
        if (~tmp) {
            V(vd)[i] |= (0xffu >> (env->vl & 0x7)) & V(0)[i] & tmp;
            V(vd)[i] &= ~(0xffu >> (env->vl & 0x7)) | ~V(0)[i] | tmp;
        } else {
            V(vd)[i] |= (0xffu >> (env->vl & 0x7)) & V(0)[i];
        }
        return;
    }

    V(vd)[i] |= V(0)[i] & tmp;
    V(vd)[i] &= ~V(0)[i] | tmp;

    for (; i < env->vl >> 3; ++i) {
        V(vd)[i] &= ~V(0)[i];
    }
    if (env->vl & 0x7) {
        V(vd)[i] &= ~(0xffu >> (env->vl & 0x7)) | ~V(0)[i];
    }
}

void helper_vmsif(CPUState *env, uint32_t vd, uint32_t vs2)
{
    if (env->vstart) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    target_long tmp = 0;
    int i = 0;
    for (; i < env->vl >> 3; ++i) {
        tmp = set_before_first_bit(V(vs2)[i]);
        if (~tmp) {
            break;
        }
        V(vd)[i] |= 0xff;
    }
    if (!~tmp && env->vl & 0x7) {
        tmp = set_before_first_bit((0xffu >> (env->vl & 0x7)) & V(vs2)[i]);
        if (~tmp) {
            tmp = tmp << 1 | 1;
            V(vd)[i] |= (0xffu >> (env->vl & 0x7)) & tmp;
            V(vd)[i] &= ~(0xffu >> (env->vl & 0x7)) | tmp;
        } else {
            V(vd)[i] |= (0xffu >> (env->vl & 0x7));
        }
        return;
    }

    tmp = tmp << 1 | 1;
    V(vd)[i] |= tmp;
    V(vd)[i] &= tmp;

    for (; i < env->vl >> 3; ++i) {
        V(vd)[i] &= 0xff;
    }
    if (env->vl & 0x7) {
        V(vd)[i] &= ~(0xffu >> (env->vl & 0x7));
    }
}

void helper_vmsif_m(CPUState *env, uint32_t vd, uint32_t vs2)
{
    if (env->vstart) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    target_long tmp = 0;
    int i = 0;
    for (; i < env->vl >> 3; ++i) {
        tmp = set_before_first_bit(V(0)[i] & V(vs2)[i]);
        if (~tmp) {
            break;
        }
        V(vd)[i] |= V(0)[i];
    }
    if (!~tmp && env->vl & 0x7) {
        tmp = set_before_first_bit((0xffu >> (env->vl & 0x7)) & V(0)[i] & V(vs2)[i]);
        if (~tmp) {
            tmp = tmp << 1 | 1;
            V(vd)[i] |= (0xffu >> (env->vl & 0x7)) & V(0)[i] & tmp;
            V(vd)[i] &= ~(0xffu >> (env->vl & 0x7)) | ~V(0)[i] | tmp;
        } else {
            V(vd)[i] |= (0xffu >> (env->vl & 0x7)) & V(0)[i];
        }
        return;
    }

    tmp = tmp << 1 | 1;
    V(vd)[i] |= V(0)[i] & tmp;
    V(vd)[i] &= ~V(0)[i] | tmp;

    for (; i < env->vl >> 3; ++i) {
        V(vd)[i] &= ~V(0)[i];
    }
    if (env->vl & 0x7) {
        V(vd)[i] &= ~(0xffu >> (env->vl & 0x7)) | ~V(0)[i];
    }
}

void helper_vmsof(CPUState *env, uint32_t vd, uint32_t vs2)
{
    if (env->vstart) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    target_long tmp = 0;
    int i = 0;
    for (; i < env->vl >> 3; ++i) {
        tmp = set_before_first_bit(V(vs2)[i]);
        if (~tmp) {
            break;
        }
        V(vd)[i] = 0;
    }
    if (!~tmp && env->vl & 0x7) {
        tmp = set_before_first_bit((0xffu >> (env->vl & 0x7)) & V(vs2)[i]);
        if (~tmp) {
            tmp = (tmp << 1 | 1) ^ tmp;
            V(vd)[i] |= (0xffu >> (env->vl & 0x7)) & tmp;
            V(vd)[i] &= ~(0xffu >> (env->vl & 0x7)) | tmp;
        } else {
            V(vd)[i] |= (0xffu >> (env->vl & 0x7));
        }
        return;
    }

    tmp = (tmp << 1 | 1) ^ tmp;
    V(vd)[i] |= tmp;
    V(vd)[i] &= tmp;

    for (; i < env->vl >> 3; ++i) {
        V(vd)[i] = 0;
    }
    if (env->vl & 0x7) {
        V(vd)[i] &= ~(0xffu >> (env->vl & 0x7));
    }
}

void helper_vmsof_m(CPUState *env, uint32_t vd, uint32_t vs2)
{
    if (env->vstart) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    target_long tmp = 0;
    int i = 0;
    for (; i < env->vl >> 3; ++i) {
        tmp = set_before_first_bit(V(0)[i] & V(vs2)[i]);
        if (~tmp) {
            break;
        }
        V(vd)[i] &= ~V(0)[i];
    }
    if (!~tmp && env->vl & 0x7) {
        tmp = set_before_first_bit((0xffu >> (env->vl & 0x7)) & V(0)[i] & V(vs2)[i]);
        if (~tmp) {
            tmp = (tmp << 1 | 1) ^ tmp;
            V(vd)[i] |= (0xffu >> (env->vl & 0x7)) & V(0)[i] & tmp;
            V(vd)[i] &= ~(0xffu >> (env->vl & 0x7)) | ~V(0)[i] | tmp;
        } else {
            V(vd)[i] |= (0xffu >> (env->vl & 0x7)) & V(0)[i];
        }
        return;
    }

    tmp = (tmp << 1 | 1) ^ tmp;
    V(vd)[i] |= V(0)[i] & tmp;
    V(vd)[i] &= ~V(0)[i] | tmp;

    for (; i < env->vl >> 3; ++i) {
        V(vd)[i] &= ~V(0)[i];
    }
    if (env->vl & 0x7) {
        V(vd)[i] &= ~(0xffu >> (env->vl & 0x7)) | ~V(0)[i];
    }
}

uint64_t helper_vfmv_fs(CPUState *env, int32_t vr, int32_t vs2)
{
    if (env->vstart >= env->vl)
    {
        // No operation is performed
        return -1u;
    }
    const target_ulong eew = env->vsew;
    switch(eew)
    {
        case 32:
           if (!riscv_has_ext(env, RISCV_FEATURE_RVF))
           {
               helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
               break;
           }
           return (uint64_t)((uint32_t *)V(vs2))[0] | (-1ul << 32);
           break;
        case 64:
           if (!riscv_has_ext(env, RISCV_FEATURE_RVD))
           {
               helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
               break;
           }
           return ((uint64_t *)V(vs2))[0];
           break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
    }
    return -1u;
}

void helper_vfmv_sf(CPUState *env, target_ulong vd, int64_t rs1)
{
    if (env->vstart >= env->vl)
    {
        return;
    }
    switch(env->vsew)
    {
        case 32:
           if (!riscv_has_ext(env, RISCV_FEATURE_RVF))
           {
               helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
               break;
           }
           ((int32_t *)V(vd))[0] = (int32_t)rs1;
           break;
        case 64:
           if (!riscv_has_ext(env, RISCV_FEATURE_RVD))
           {
               helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
               break;
           }
           ((int64_t *)V(vd))[0] = rs1;
           break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
    }
}

#undef MASK_OP_GEN_OP_AND
#undef MASK_OP_GEN_OP_NAND
#undef MASK_OP_GEN_OP_ANDNOT
#undef MASK_OP_GEN_OP_XOR
#undef MASK_OP_GEN_OP_OR
#undef MASK_OP_GEN_OP_NOR
#undef MASK_OP_GEN_OP_ORNOT
#undef MASK_OP_GEN_OP_XNOR
#undef MASK_OP_GEN

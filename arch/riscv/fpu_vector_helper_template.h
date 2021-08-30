

#ifdef MASKED
#define POSTFIX _m
#else
#define POSTFIX
#endif

void glue(helper_vfadd_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fadd_s(env, ((uint32_t *)V(vs2))[ei], ((uint32_t *)V(vs1))[ei], env->frm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fadd_d(env, ((uint64_t *)V(vs2))[ei], ((uint64_t *)V(vs1))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfadd_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fadd_s(env, ((uint32_t *)V(vs2))[ei], f1, env->frm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fadd_d(env, ((uint64_t *)V(vs2))[ei], f1, env->frm);
            break;
        }
    }
}

void glue(helper_vfsub_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fsub_s(env, ((uint32_t *)V(vs2))[ei], ((uint32_t *)V(vs1))[ei], env->frm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fsub_d(env, ((uint64_t *)V(vs2))[ei], ((uint64_t *)V(vs1))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfsub_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fsub_s(env, ((uint32_t *)V(vs2))[ei], f1, env->frm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fsub_d(env, ((uint64_t *)V(vs2))[ei], f1, env->frm);
            break;
        }
    }
}

void glue(helper_vfrsub_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fsub_s(env, f1, ((uint32_t *)V(vs2))[ei], env->frm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fsub_d(env, f1, ((uint64_t *)V(vs2))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfwadd_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew << 1) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 32:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint64_t *)V(vd))[ei] = helper_fadd_d(env, helper_fcvt_s_d(env, ((uint32_t *)V(vs2))[ei], env->frm), helper_fcvt_s_d(env, ((uint32_t *)V(vs1))[ei], env->frm), env->frm);
            break;
        }
    }
}

void glue(helper_vfwadd_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew << 1) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 32:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    switch (eew) {
    case 32:
        f1 = helper_fcvt_s_d(env, f1, env->frm);
        break;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint64_t *)V(vd))[ei] = helper_fadd_d(env, helper_fcvt_s_d(env, ((uint32_t *)V(vs2))[ei], env->frm), f1, env->frm);
            break;
        }
    }
}

void glue(helper_vfwadd_wv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew << 1) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 32:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint64_t *)V(vd))[ei] = helper_fadd_d(env, ((uint64_t *)V(vs2))[ei], helper_fcvt_s_d(env, ((uint32_t *)V(vs1))[ei], env->frm), env->frm);
            break;
        }
    }
}

void glue(helper_vfwadd_wf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew << 1) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 32:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    switch (eew) {
    case 32:
        f1 = helper_fcvt_s_d(env, f1, env->frm);
        break;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint64_t *)V(vd))[ei] = helper_fadd_d(env, ((uint64_t *)V(vs2))[ei], f1, env->frm);
            break;
        }
    }
}

void glue(helper_vfwsub_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew << 1) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 32:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint64_t *)V(vd))[ei] = helper_fsub_d(env, helper_fcvt_s_d(env, ((uint32_t *)V(vs2))[ei], env->frm), helper_fcvt_s_d(env, ((uint32_t *)V(vs1))[ei], env->frm), env->frm);
            break;
        }
    }
}

void glue(helper_vfwsub_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew << 1) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 32:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    switch (eew) {
    case 32:
        f1 = helper_fcvt_s_d(env, f1, env->frm);
        break;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint64_t *)V(vd))[ei] = helper_fsub_d(env, helper_fcvt_s_d(env, ((uint32_t *)V(vs2))[ei], env->frm), f1, env->frm);
            break;
        }
    }
}

void glue(helper_vfwsub_wv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew << 1) || V_IDX_INVALID_EEW(vs2, eew << 1) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 32:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint64_t *)V(vd))[ei] = helper_fsub_d(env, ((uint64_t *)V(vs2))[ei], helper_fcvt_s_d(env, ((uint32_t *)V(vs1))[ei], env->frm), env->frm);
            break;
        }
    }
}

void glue(helper_vfwsub_wf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew << 1) || V_IDX_INVALID_EEW(vs2, eew << 1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 32:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    switch (eew) {
    case 32:
        f1 = helper_fcvt_s_d(env, f1, env->frm);
        break;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint64_t *)V(vd))[ei] = helper_fsub_d(env, ((uint64_t *)V(vs2))[ei], f1, env->frm);
            break;
        }
    }
}

void glue(helper_vfmul_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fmul_s(env, ((uint32_t *)V(vs2))[ei], ((uint32_t *)V(vs1))[ei], env->frm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fmul_d(env, ((uint64_t *)V(vs2))[ei], ((uint64_t *)V(vs1))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfmul_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fmul_s(env, ((uint32_t *)V(vs2))[ei], f1, env->frm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fmul_d(env, ((uint64_t *)V(vs2))[ei], f1, env->frm);
            break;
        }
    }
}

void glue(helper_vfdiv_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fdiv_s(env, ((uint32_t *)V(vs2))[ei], ((uint32_t *)V(vs1))[ei], env->frm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fdiv_d(env, ((uint64_t *)V(vs2))[ei], ((uint64_t *)V(vs1))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfdiv_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fdiv_s(env, ((uint32_t *)V(vs2))[ei], f1, env->frm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fdiv_d(env, ((uint64_t *)V(vs2))[ei], f1, env->frm);
            break;
        }
    }
}

void glue(helper_vfrdiv_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fdiv_s(env, f1, ((uint32_t *)V(vs2))[ei], env->frm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fdiv_d(env, f1, ((uint64_t *)V(vs2))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfwmul_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew << 1) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 32:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint64_t *)V(vd))[ei] = helper_fmul_d(env, helper_fcvt_s_d(env, ((uint32_t *)V(vs2))[ei], env->frm), helper_fcvt_s_d(env, ((uint32_t *)V(vs1))[ei], env->frm), env->frm);
            break;
        }
    }
}

void glue(helper_vfwmul_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew << 1) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 32:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    switch (eew) {
    case 32:
        f1 = helper_fcvt_s_d(env, f1, env->frm);
        break;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint64_t *)V(vd))[ei] = helper_fmul_d(env, helper_fcvt_s_d(env, ((uint32_t *)V(vs2))[ei], env->frm), f1, env->frm);
            break;
        }
    }
}

void glue(helper_vfmacc_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fmadd_s(env, ((uint32_t *)V(vs2))[ei], ((uint32_t *)V(vs1))[ei], ((uint32_t *)V(vd))[ei], env->frm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fmadd_d(env, ((uint64_t *)V(vs2))[ei], ((uint64_t *)V(vs1))[ei], ((uint64_t *)V(vd))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfmacc_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fmadd_s(env, ((uint32_t *)V(vs2))[ei], f1, ((uint32_t *)V(vd))[ei], env->frm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fmadd_d(env, ((uint64_t *)V(vs2))[ei], f1, ((uint64_t *)V(vd))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfnmacc_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fnmadd_s(env, ((uint32_t *)V(vs2))[ei], ((uint32_t *)V(vs1))[ei], ((uint32_t *)V(vd))[ei], env->frm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fnmadd_d(env, ((uint64_t *)V(vs2))[ei], ((uint64_t *)V(vs1))[ei], ((uint64_t *)V(vd))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfnmacc_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fnmadd_s(env, ((uint32_t *)V(vs2))[ei], f1, ((uint32_t *)V(vd))[ei], env->frm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fnmadd_d(env, ((uint64_t *)V(vs2))[ei], f1, ((uint64_t *)V(vd))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfmsac_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fmsub_s(env, ((uint32_t *)V(vs2))[ei], ((uint32_t *)V(vs1))[ei], ((uint32_t *)V(vd))[ei], env->frm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fmsub_d(env, ((uint64_t *)V(vs2))[ei], ((uint64_t *)V(vs1))[ei], ((uint64_t *)V(vd))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfmsac_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fmsub_s(env, ((uint32_t *)V(vs2))[ei], f1, ((uint32_t *)V(vd))[ei], env->frm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fmsub_d(env, ((uint64_t *)V(vs2))[ei], f1, ((uint64_t *)V(vd))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfnmsac_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fnmsub_s(env, ((uint32_t *)V(vs2))[ei], ((uint32_t *)V(vs1))[ei], ((uint32_t *)V(vd))[ei], env->frm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fnmsub_d(env, ((uint64_t *)V(vs2))[ei], ((uint64_t *)V(vs1))[ei], ((uint64_t *)V(vd))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfnmsac_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fnmsub_s(env, ((uint32_t *)V(vs2))[ei], f1, ((uint32_t *)V(vd))[ei], env->frm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fnmsub_d(env, ((uint64_t *)V(vs2))[ei], f1, ((uint64_t *)V(vd))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfmadd_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fmadd_s(env, ((uint32_t *)V(vs1))[ei], ((uint32_t *)V(vd))[ei], ((uint32_t *)V(vs2))[ei], env->frm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fmadd_d(env, ((uint64_t *)V(vs1))[ei], ((uint64_t *)V(vd))[ei], ((uint64_t *)V(vs2))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfmadd_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fmadd_s(env, f1, ((uint32_t *)V(vd))[ei], ((uint32_t *)V(vs2))[ei], env->frm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fmadd_d(env, f1, ((uint64_t *)V(vd))[ei], ((uint64_t *)V(vs2))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfnmadd_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fnmadd_s(env, ((uint32_t *)V(vs1))[ei], ((uint32_t *)V(vd))[ei], ((uint32_t *)V(vs2))[ei], env->frm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fnmadd_d(env, ((uint64_t *)V(vs1))[ei], ((uint64_t *)V(vd))[ei], ((uint64_t *)V(vs2))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfnmadd_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fnmadd_s(env, f1, ((uint32_t *)V(vd))[ei], ((uint32_t *)V(vs2))[ei], env->frm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fnmadd_d(env, f1, ((uint64_t *)V(vd))[ei], ((uint64_t *)V(vs2))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfmsub_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fmsub_s(env, ((uint32_t *)V(vs1))[ei], ((uint32_t *)V(vd))[ei], ((uint32_t *)V(vs2))[ei], env->frm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fmsub_d(env, ((uint64_t *)V(vs1))[ei], ((uint64_t *)V(vd))[ei], ((uint64_t *)V(vs2))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfmsub_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fmsub_s(env, f1, ((uint32_t *)V(vd))[ei], ((uint32_t *)V(vs2))[ei], env->frm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fmsub_d(env, f1, ((uint64_t *)V(vd))[ei], ((uint64_t *)V(vs2))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfnmsub_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fnmsub_s(env, ((uint32_t *)V(vs1))[ei], ((uint32_t *)V(vd))[ei], ((uint32_t *)V(vs2))[ei], env->frm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fnmsub_d(env, ((uint64_t *)V(vs1))[ei], ((uint64_t *)V(vd))[ei], ((uint64_t *)V(vs2))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfnmsub_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fnmsub_s(env, f1, ((uint32_t *)V(vd))[ei], ((uint32_t *)V(vs2))[ei], env->frm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fnmsub_d(env, f1, ((uint64_t *)V(vd))[ei], ((uint64_t *)V(vs2))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfwmacc_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew << 1) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 32:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint64_t *)V(vd))[ei] = helper_fmadd_d(env, helper_fcvt_s_d(env, ((uint32_t *)V(vs2))[ei], env->frm), helper_fcvt_s_d(env, ((uint32_t *)V(vs1))[ei], env->frm), ((uint64_t *)V(vd))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfwmacc_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew << 1) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 32:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    switch (eew) {
    case 32:
        f1 = helper_fcvt_s_d(env, f1, env->frm);
        break;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint64_t *)V(vd))[ei] = helper_fmadd_d(env,helper_fcvt_s_d(env, ((uint32_t *)V(vs2))[ei], env->frm), f1, ((uint64_t *)V(vd))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfwnmacc_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew << 1) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 32:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint64_t *)V(vd))[ei] = helper_fnmadd_d(env, helper_fcvt_s_d(env, ((uint32_t *)V(vs2))[ei], env->frm), helper_fcvt_s_d(env, ((uint32_t *)V(vs1))[ei], env->frm), ((uint64_t *)V(vd))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfwnmacc_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew << 1) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 32:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    switch (eew) {
    case 32:
        f1 = helper_fcvt_s_d(env, f1, env->frm);
        break;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint64_t *)V(vd))[ei] = helper_fnmadd_d(env, helper_fcvt_s_d(env, ((uint32_t *)V(vs2))[ei], env->frm), f1, ((uint64_t *)V(vd))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfwmsac_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew << 1) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 32:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint64_t *)V(vd))[ei] = helper_fmsub_d(env, helper_fcvt_s_d(env, ((uint32_t *)V(vs2))[ei], env->frm), helper_fcvt_s_d(env, ((uint32_t *)V(vs1))[ei], env->frm), ((uint64_t *)V(vd))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfwmsac_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew << 1) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 32:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    switch (eew) {
    case 32:
        f1 = helper_fcvt_s_d(env, f1, env->frm);
        break;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint64_t *)V(vd))[ei] = helper_fmsub_d(env, helper_fcvt_s_d(env, ((uint32_t *)V(vs2))[ei], env->frm), f1, ((uint64_t *)V(vd))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfwnmsac_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew << 1) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 32:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint64_t *)V(vd))[ei] = helper_fnmsub_d(env, helper_fcvt_s_d(env, ((uint32_t *)V(vs2))[ei], env->frm), helper_fcvt_s_d(env, ((uint32_t *)V(vs1))[ei], env->frm), ((uint64_t *)V(vd))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfwnmsac_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew << 1) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 32:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    switch (eew) {
    case 32:
        f1 = helper_fcvt_s_d(env, f1, env->frm);
        break;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint64_t *)V(vd))[ei] = helper_fnmsub_d(env, helper_fcvt_s_d(env, ((uint32_t *)V(vs2))[ei], env->frm), f1, ((uint64_t *)V(vd))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfsqrt_v, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fsqrt_s(env, ((uint32_t *)V(vs2))[ei], env->frm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fsqrt_d(env, ((uint64_t *)V(vs2))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfmin_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fmin_s(env, ((uint32_t *)V(vs2))[ei], ((uint32_t *)V(vs1))[ei]);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fmin_d(env, ((uint64_t *)V(vs2))[ei], ((uint64_t *)V(vs1))[ei]);
            break;
        }
    }
}

void glue(helper_vfmin_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fmin_s(env, ((uint32_t *)V(vs2))[ei], f1);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fmin_d(env, ((uint64_t *)V(vs2))[ei], f1);
            break;
        }
    }
}

void glue(helper_vfmax_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fmax_s(env, ((uint32_t *)V(vs2))[ei], ((uint32_t *)V(vs1))[ei]);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fmax_d(env, ((uint64_t *)V(vs2))[ei], ((uint64_t *)V(vs1))[ei]);
            break;
        }
    }
}

void glue(helper_vfmax_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fmax_s(env, ((uint32_t *)V(vs2))[ei], f1);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fmax_d(env, ((uint64_t *)V(vs2))[ei], f1);
            break;
        }
    }
}

void glue(helper_vfsgnj_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = (((uint32_t *)V(vs2))[ei] & ~INT32_MIN) | (((uint32_t *)V(vs1))[ei] & INT32_MIN);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = (((uint64_t *)V(vs2))[ei] & ~INT64_MIN) | (((uint64_t *)V(vs1))[ei] & INT64_MIN);
            break;
        }
    }
}

void glue(helper_vfsgnj_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
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
    switch (eew) {
    case 32:
        f1 &= INT32_MIN;
        break;
    case 64:
        f1 &= INT64_MIN;
        break;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = (((uint32_t *)V(vs2))[ei] & ~INT32_MIN) | f1;
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = (((uint64_t *)V(vs2))[ei] & ~INT64_MIN) | f1;
            break;
        }
    }
}

void glue(helper_vfsgnjn_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = (((uint32_t *)V(vs2))[ei] & ~INT32_MIN) | (~((uint32_t *)V(vs1))[ei] & INT32_MIN);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = (((uint64_t *)V(vs2))[ei] & ~INT64_MIN) | (~((uint64_t *)V(vs1))[ei] & INT64_MIN);
            break;
        }
    }
}

void glue(helper_vfsgnjn_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
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
    switch (eew) {
    case 32:
        f1 = ~f1 & INT32_MIN;
        break;
    case 64:
        f1 = ~f1 & INT64_MIN;
        break;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = (((uint32_t *)V(vs2))[ei] & ~INT32_MIN) | f1;
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = (((uint64_t *)V(vs2))[ei] & ~INT64_MIN) | f1;
            break;
        }
    }
}

void glue(helper_vfsgnjx_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] ^ (((uint32_t *)V(vs1))[ei] & INT32_MIN);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = ((uint64_t *)V(vs2))[ei] ^ (((uint64_t *)V(vs1))[ei] & INT64_MIN);
            break;
        }
    }
}

void glue(helper_vfsgnjx_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
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
    switch (eew) {
    case 32:
        f1 &= INT32_MIN;
        break;
    case 64:
        f1 &= INT64_MIN;
        break;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] ^ f1;
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = ((uint64_t *)V(vs2))[ei] ^ f1;
            break;
        }
    }
}

void glue(helper_vfeq_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
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
#ifdef MASKED
    uint16_t mask = 2;
#endif
    for (int i = 0; i < env->vl; ++i) {
#ifdef MASKED
        mask >>= 1;
        if (mask == 1) {
            mask = (mask << 16) | V(0)[i >> 3];
            V(vd)[i >> 3] = 0;
        }
        if (!(mask & 1)) {
            continue;
        }
#else
        if (!(i & 0x7)) {
            V(vd)[i >> 3] = 0;
        }
#endif
        switch (eew) {
        case 32:
            V(vd)[i >> 3] |= helper_feq_s(env, ((uint32_t *)V(vs2))[i], ((uint32_t *)V(vs1))[i]) << (i & 0x7);
            break;
        case 64:
            V(vd)[i >> 3] |= helper_feq_d(env, ((uint64_t *)V(vs2))[i], ((uint64_t *)V(vs1))[i]) << (i & 0x7);
            break;
        }
    }
}

void glue(helper_vfeq_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
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
#ifdef MASKED
    uint16_t mask = 2;
#endif
    for (int i = 0; i < env->vl; ++i) {
#ifdef MASKED
        mask >>= 1;
        if (mask == 1) {
            mask = (mask << 16) | V(0)[i >> 3];
            V(vd)[i >> 3] = 0;
        }
        if (!(mask & 1)) {
            continue;
        }
#else
        if (!(i & 0x7)) {
            V(vd)[i >> 3] = 0;
        }
#endif
        switch (eew) {
        case 32:
            V(vd)[i >> 3] |= helper_feq_s(env, ((uint32_t *)V(vs2))[i], f1) << (i & 0x7);
            break;
        case 64:
            V(vd)[i >> 3] |= helper_feq_d(env, ((uint64_t *)V(vs2))[i], f1) << (i & 0x7);
            break;
        }
    }
}

void glue(helper_vfne_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
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
#ifdef MASKED
    uint16_t mask = 2;
#endif
    for (int i = 0; i < env->vl; ++i) {
#ifdef MASKED
        mask >>= 1;
        if (mask == 1) {
            mask = (mask << 16) | V(0)[i >> 3];
            V(vd)[i >> 3] = 0;
        }
        if (!(mask & 1)) {
            continue;
        }
#else
        if (!(i & 0x7)) {
            V(vd)[i >> 3] = 0;
        }
#endif
        switch (eew) {
        case 32:
            V(vd)[i >> 3] |= !helper_feq_s(env, ((uint32_t *)V(vs2))[i], ((uint32_t *)V(vs1))[i]) << (i & 0x7);
            break;
        case 64:
            V(vd)[i >> 3] |= !helper_feq_d(env, ((uint64_t *)V(vs2))[i], ((uint64_t *)V(vs1))[i]) << (i & 0x7);
            break;
        }
    }
}

void glue(helper_vfne_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
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
#ifdef MASKED
    uint16_t mask = 2;
#endif
    for (int i = 0; i < env->vl; ++i) {
#ifdef MASKED
        mask >>= 1;
        if (mask == 1) {
            mask = (mask << 16) | V(0)[i >> 3];
            V(vd)[i >> 3] = 0;
        }
        if (!(mask & 1)) {
            continue;
        }
#else
        if (!(i & 0x7)) {
            V(vd)[i >> 3] = 0;
        }
#endif
        switch (eew) {
        case 32:
            V(vd)[i >> 3] |= !helper_feq_s(env, ((uint32_t *)V(vs2))[i], f1) << (i & 0x7);
            break;
        case 64:
            V(vd)[i >> 3] |= !helper_feq_d(env, ((uint64_t *)V(vs2))[i], f1) << (i & 0x7);
            break;
        }
    }
}

void glue(helper_vflt_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
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
#ifdef MASKED
    uint16_t mask = 2;
#endif
    for (int i = 0; i < env->vl; ++i) {
#ifdef MASKED
        mask >>= 1;
        if (mask == 1) {
            mask = (mask << 16) | V(0)[i >> 3];
            V(vd)[i >> 3] = 0;
        }
        if (!(mask & 1)) {
            continue;
        }
#else
        if (!(i & 0x7)) {
            V(vd)[i >> 3] = 0;
        }
#endif
        switch (eew) {
        case 32:
            V(vd)[i >> 3] |= !helper_flt_s(env, ((uint32_t *)V(vs2))[i], ((uint32_t *)V(vs1))[i]) << (i & 0x7);
            break;
        case 64:
            V(vd)[i >> 3] |= !helper_flt_d(env, ((uint64_t *)V(vs2))[i], ((uint64_t *)V(vs1))[i]) << (i & 0x7);
            break;
        }
    }
}

void glue(helper_vflt_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
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
#ifdef MASKED
    uint16_t mask = 2;
#endif
    for (int i = 0; i < env->vl; ++i) {
#ifdef MASKED
        mask >>= 1;
        if (mask == 1) {
            mask = (mask << 16) | V(0)[i >> 3];
            V(vd)[i >> 3] = 0;
        }
        if (!(mask & 1)) {
            continue;
        }
#else
        if (!(i & 0x7)) {
            V(vd)[i >> 3] = 0;
        }
#endif
        switch (eew) {
        case 32:
            V(vd)[i >> 3] |= !helper_flt_s(env, ((uint32_t *)V(vs2))[i], f1) << (i & 0x7);
            break;
        case 64:
            V(vd)[i >> 3] |= !helper_flt_d(env, ((uint64_t *)V(vs2))[i], f1) << (i & 0x7);
            break;
        }
    }
}

void glue(helper_vfle_vv, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
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
#ifdef MASKED
    uint16_t mask = 2;
#endif
    for (int i = 0; i < env->vl; ++i) {
#ifdef MASKED
        mask >>= 1;
        if (mask == 1) {
            mask = (mask << 16) | V(0)[i >> 3];
            V(vd)[i >> 3] = 0;
        }
        if (!(mask & 1)) {
            continue;
        }
#else
        if (!(i & 0x7)) {
            V(vd)[i >> 3] = 0;
        }
#endif
        switch (eew) {
        case 32:
            V(vd)[i >> 3] |= !helper_fle_s(env, ((uint32_t *)V(vs2))[i], ((uint32_t *)V(vs1))[i]) << (i & 0x7);
            break;
        case 64:
            V(vd)[i >> 3] |= !helper_fle_d(env, ((uint64_t *)V(vs2))[i], ((uint64_t *)V(vs1))[i]) << (i & 0x7);
            break;
        }
    }
}

void glue(helper_vfle_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
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
#ifdef MASKED
    uint16_t mask = 2;
#endif
    for (int i = 0; i < env->vl; ++i) {
#ifdef MASKED
        mask >>= 1;
        if (mask == 1) {
            mask = (mask << 16) | V(0)[i >> 3];
            V(vd)[i >> 3] = 0;
        }
        if (!(mask & 1)) {
            continue;
        }
#else
        if (!(i & 0x7)) {
            V(vd)[i >> 3] = 0;
        }
#endif
        switch (eew) {
        case 32:
            V(vd)[i >> 3] |= !helper_fle_s(env, ((uint32_t *)V(vs2))[i], f1) << (i & 0x7);
            break;
        case 64:
            V(vd)[i >> 3] |= !helper_fle_d(env, ((uint64_t *)V(vs2))[i], f1) << (i & 0x7);
            break;
        }
    }
}

void glue(helper_vfgt_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
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
#ifdef MASKED
    uint16_t mask = 2;
#endif
    for (int i = 0; i < env->vl; ++i) {
#ifdef MASKED
        mask >>= 1;
        if (mask == 1) {
            mask = (mask << 16) | V(0)[i >> 3];
            V(vd)[i >> 3] = 0;
        }
        if (!(mask & 1)) {
            continue;
        }
#else
        if (!(i & 0x7)) {
            V(vd)[i >> 3] = 0;
        }
#endif
        switch (eew) {
        case 32:
            V(vd)[i >> 3] |= !helper_fle_s(env, f1, ((uint32_t *)V(vs2))[i]) << (i & 0x7);
            break;
        case 64:
            V(vd)[i >> 3] |= !helper_fle_d(env, f1, ((uint64_t *)V(vs2))[i]) << (i & 0x7);
            break;
        }
    }
}

void glue(helper_vfge_vf, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint64_t f1)
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
#ifdef MASKED
    uint16_t mask = 2;
#endif
    for (int i = 0; i < env->vl; ++i) {
#ifdef MASKED
        mask >>= 1;
        if (mask == 1) {
            mask = (mask << 16) | V(0)[i >> 3];
            V(vd)[i >> 3] = 0;
        }
        if (!(mask & 1)) {
            continue;
        }
#else
        if (!(i & 0x7)) {
            V(vd)[i >> 3] = 0;
        }
#endif
        switch (eew) {
        case 32:
            V(vd)[i >> 3] |= !helper_flt_s(env, f1, ((uint32_t *)V(vs2))[i]) << (i & 0x7);
            break;
        case 64:
            V(vd)[i >> 3] |= !helper_flt_d(env, f1, ((uint64_t *)V(vs2))[i]) << (i & 0x7);
            break;
        }
    }
}

void glue(helper_vfclass_v, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fclass_s(env, ((uint32_t *)V(vs2))[ei]);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fclass_d(env, ((uint64_t *)V(vs2))[ei]);
            break;
        }
    }
}

void glue(helper_vfcvt_xuf_v, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            if (env->vxrm == 0b11) {
                ((uint32_t *)V(vd))[ei] = helper_fcvt_wu_s_rod(env, ((uint32_t *)V(vs2))[ei]);
            } else {
                ((uint32_t *)V(vd))[ei] = helper_fcvt_wu_s(env, ((uint32_t *)V(vs2))[ei], ieee_vxrm[env->vxrm]);
            }
            break;
        case 64:
            if (env->vxrm == 0b11) {
                ((uint64_t *)V(vd))[ei] = helper_fcvt_lu_d_rod(env, ((uint64_t *)V(vs2))[ei]);
            } else {
                ((uint64_t *)V(vd))[ei] = helper_fcvt_lu_d(env, ((uint64_t *)V(vs2))[ei], ieee_vxrm[env->vxrm]);
            }
            break;
        }
    }
}

void glue(helper_vfcvt_xf_v, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            if (env->vxrm == 0b11) {
                ((uint32_t *)V(vd))[ei] = helper_fcvt_w_s_rod(env, ((uint32_t *)V(vs2))[ei]);
            } else {
                ((uint32_t *)V(vd))[ei] = helper_fcvt_w_s(env, ((uint32_t *)V(vs2))[ei], ieee_vxrm[env->vxrm]);
            }
            break;
        case 64:
            if (env->vxrm == 0b11) {
                ((uint64_t *)V(vd))[ei] = helper_fcvt_l_d_rod(env, ((uint64_t *)V(vs2))[ei]);
            } else {
                ((uint64_t *)V(vd))[ei] = helper_fcvt_l_d(env, ((uint64_t *)V(vs2))[ei], ieee_vxrm[env->vxrm]);
            }
            break;
        }
    }
}

void glue(helper_vfcvt_rtz_xuf_v, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fcvt_wu_s(env, ((uint32_t *)V(vs2))[ei], float_round_to_zero);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fcvt_lu_d(env, ((uint64_t *)V(vs2))[ei], float_round_to_zero);
            break;
        }
    }
}

void glue(helper_vfcvt_rtz_xf_v, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fcvt_w_s(env, ((uint32_t *)V(vs2))[ei], float_round_to_zero);
            break;        
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fcvt_l_d(env, ((uint64_t *)V(vs2))[ei], float_round_to_zero);
            break;
        }
    }
}

void glue(helper_vfcvt_fxu_v, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fcvt_s_wu(env, ((uint32_t *)V(vs2))[ei], float_round_to_zero);
            break;        
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fcvt_d_lu(env, ((uint64_t *)V(vs2))[ei], float_round_to_zero);
            break;
        }
    }
}

void glue(helper_vfcvt_fx_v, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
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
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = helper_fcvt_s_w(env, ((uint32_t *)V(vs2))[ei], float_round_to_zero);
            break;        
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fcvt_d_l(env, ((uint64_t *)V(vs2))[ei], float_round_to_zero);
            break;
        }
    }
}

void glue(helper_vfwcvt_xuf_v, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew << 1) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 32:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            if (env->vxrm == 0b11) {
                ((uint64_t *)V(vd))[ei] = helper_fcvt_lu_s_rod(env, ((uint32_t *)V(vs2))[ei]);
            } else {
                ((uint64_t *)V(vd))[ei] = helper_fcvt_lu_s(env, ((uint32_t *)V(vs2))[ei], ieee_vxrm[env->vxrm]);
            }
            break;
        }
    }
}

void glue(helper_vfwcvt_xf_v, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew << 1) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 32:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            if (env->vxrm == 0b11) {
                ((uint64_t *)V(vd))[ei] = helper_fcvt_l_s_rod(env, ((uint32_t *)V(vs2))[ei]);
            } else {
                ((uint64_t *)V(vd))[ei] = helper_fcvt_l_s(env, ((uint32_t *)V(vs2))[ei], ieee_vxrm[env->vxrm]);
            }
            break;
        }
    }
}

void glue(helper_vfwcvt_rtz_xuf_v, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew << 1) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 32:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint64_t *)V(vd))[ei] = helper_fcvt_lu_s(env, ((uint32_t *)V(vs2))[ei], float_round_to_zero);
            break;
        }
    }
}

void glue(helper_vfwcvt_rtz_xf_v, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew << 1) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 32:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint64_t *)V(vd))[ei] = helper_fcvt_l_s(env, ((uint32_t *)V(vs2))[ei], float_round_to_zero);
            break;
        }
    }
}

void glue(helper_vfwcvt_fxu_v, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew << 1) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 32:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint64_t *)V(vd))[ei] = helper_fcvt_d_wu(env, ((uint32_t *)V(vs2))[ei], float_round_to_zero);
            break;
        }
    }
}

void glue(helper_vfwcvt_fx_v, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew << 1) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 32:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint64_t *)V(vd))[ei] = helper_fcvt_d_w(env, ((uint32_t *)V(vs2))[ei], float_round_to_zero);
            break;
        }
    }
}

void glue(helper_vfwcvt_ff_v, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew << 1) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 32:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint64_t *)V(vd))[ei] = helper_fcvt_d_s(env, ((uint32_t *)V(vs2))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfncvt_xuf_w, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew >> 1) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 64:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 64:
            if (env->vxrm == 0b11) {
                ((uint32_t *)V(vd))[ei] = helper_fcvt_wu_d_rod(env, ((uint64_t *)V(vs2))[ei]);
            } else {
                ((uint32_t *)V(vd))[ei] = helper_fcvt_wu_d(env, ((uint64_t *)V(vs2))[ei], ieee_vxrm[env->vxrm]);
            }
            break;
        }
    }
}

void glue(helper_vfncvt_xf_w, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew >> 1) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 64:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 64:
            if (env->vxrm == 0b11) {
                ((uint32_t *)V(vd))[ei] = helper_fcvt_w_d_rod(env, ((uint64_t *)V(vs2))[ei]);
            } else {
                ((uint32_t *)V(vd))[ei] = helper_fcvt_w_d(env, ((uint64_t *)V(vs2))[ei], ieee_vxrm[env->vxrm]);
            }
            break;
        }
    }
}

void glue(helper_vfncvt_rtz_xuf_w, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew >> 1) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 64:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 64:
            ((uint64_t *)V(vd))[ei] = helper_fcvt_wu_d(env, ((uint32_t *)V(vs2))[ei], float_round_to_zero);
            break;
        }
    }
}

void glue(helper_vfncvt_rtz_xf_w, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew >> 1) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 64:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 64:
            ((uint32_t *)V(vd))[ei] = helper_fcvt_w_d(env, ((uint64_t *)V(vs2))[ei], float_round_to_zero);
            break;
        }
    }
}

void glue(helper_vfncvt_fxu_w, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew >> 1) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 64:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 64:
            ((uint32_t *)V(vd))[ei] = helper_fcvt_s_lu(env, ((uint64_t *)V(vs2))[ei], float_round_to_zero);
            break;
        }
    }
}

void glue(helper_vfncvt_fx_w, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew >> 1) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 64:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 64:
            ((uint32_t *)V(vd))[ei] = helper_fcvt_s_l(env, ((uint64_t *)V(vs2))[ei], float_round_to_zero);
            break;
        }
    }
}

void glue(helper_vfncvt_ff_w, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew >> 1) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 64:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 64:
            ((uint32_t *)V(vd))[ei] = helper_fcvt_s_d(env, ((uint64_t *)V(vs2))[ei], env->frm);
            break;
        }
    }
}

void glue(helper_vfncvt_rod_ff_w, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
{
    const target_ulong eew = env->vsew;
    if (V_IDX_INVALID_EEW(vd, eew >> 1) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    switch (eew) {
    case 64:
        if (!riscv_has_ext(env, RISCV_FEATURE_RVF) || !riscv_has_ext(env, RISCV_FEATURE_RVD)) {
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 64:
            ((uint32_t *)V(vd))[ei] = helper_fcvt_s_d_rod(env, ((uint64_t *)V(vs2))[ei]);
            break;
        }
    }
}

#undef MASKED
#undef POSTFIX

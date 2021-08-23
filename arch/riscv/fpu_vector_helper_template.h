

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

#undef MASKED
#undef POSTFIX

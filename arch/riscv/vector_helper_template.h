
#define DATA_SIZE (1 << SHIFT)

#ifdef MASKED
#define POSTFIX _m
#else
#define POSTFIX
#endif

#if DATA_SIZE == 8
#define BITS      64
#define SUFFIX    q
#define USUFFIX   q
#define DATA_TYPE uint64_t
#elif DATA_SIZE == 4
#define BITS      32
#define SUFFIX    l
#define USUFFIX   l
#define DATA_TYPE uint32_t
#elif DATA_SIZE == 2
#define BITS      16
#define SUFFIX    w
#define USUFFIX   uw
#define DATA_TYPE uint16_t
#elif DATA_SIZE == 1
#define BITS      8
#define SUFFIX    b
#define USUFFIX   ub
#define DATA_TYPE uint8_t
#else
#error unsupported data size
#endif

void glue(glue(helper_vle, BITS), POSTFIX)(CPUState *env, uint32_t vd, uint32_t rs1, uint32_t lumop, uint32_t nf)
{
    const target_ulong emul = EMUL(SHIFT);
    if (emul == 0x7 || V_IDX_INVALID_EMUL(vd, emul) || V_INVALID_NF(vd, nf, emul)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    target_ulong src_addr = env->gpr[rs1];
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        env->vstart = ei;
        for (int fi = 0; fi <= nf; ++fi) {
            ((DATA_TYPE *)V(vd + (fi << SHIFT)))[ei] = glue(ld, USUFFIX)(src_addr + ei * DATA_SIZE);
        }
    }
}

void glue(glue(glue(helper_vle, BITS), ff), POSTFIX)(CPUState *env, uint32_t vd, uint32_t rs1, uint32_t lumop, uint32_t nf)
{
    const target_ulong emul = EMUL(SHIFT);
    if (emul == 0x7 || V_IDX_INVALID_EMUL(vd, emul) || V_INVALID_NF(vd, nf, emul)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    target_ulong src_addr = env->gpr[rs1];
    cpu->graceful_memory_access_exception = 1;
    bool first = true;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        for (int fi = 0; fi <= nf; ++fi) {
            DATA_TYPE value = glue(ld, USUFFIX)(src_addr + ei * DATA_SIZE);
            if (!cpu->graceful_memory_access_exception) {
                if (first) {
                    env->vstart = ei;
                    helper_raise_exception(env, env->exception_index);
                } else {
                    env->vl = ei;
                    env->exception_index = 0;
                }
                break;
            }
            ((DATA_TYPE *)V(vd + (fi << SHIFT)))[ei] = value;
            first = false;
        }
    }
    cpu->graceful_memory_access_exception = 0;
}

void glue(glue(helper_vlse, BITS), POSTFIX)(CPUState *env, uint32_t vd, uint32_t rs1, uint32_t rs2, uint32_t nf)
{
    const target_ulong emul = EMUL(SHIFT);
    if (emul == 0x7 || V_IDX_INVALID_EMUL(vd, emul) || V_INVALID_NF(vd, nf, emul)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    target_ulong src_addr = env->gpr[rs1];
    target_long offset = env->gpr[rs2];
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        env->vstart = ei;
        for (int fi = 0; fi <= nf; ++fi) {
            ((DATA_TYPE *)V(vd + (fi << SHIFT)))[ei] = glue(ld, USUFFIX)(src_addr + ei * offset);
        }
    }
}

void glue(glue(helper_vlxei, BITS), POSTFIX)(CPUState *env, uint32_t vd, uint32_t rs1, uint32_t vs2, uint32_t nf)
{
    const target_ulong emul = EMUL(SHIFT);
    if (emul == 0x7 || V_IDX_INVALID(vd) || V_IDX_INVALID_EMUL(vs2, emul) || V_INVALID_NF(vd, nf, emul)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    target_ulong src_addr = env->gpr[rs1];
    DATA_TYPE *offsets = (DATA_TYPE *)V(vs2);
    const target_ulong dst_eew = env->vsew;

    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        env->vstart = ei;
        for (int fi = 0; fi <= nf; ++fi) {
            switch (dst_eew) {
            case 8:
                V(vd + (fi << SHIFT))[ei] = ldub(src_addr + (target_ulong)offsets[ei] + fi);
                break;
            case 16:
                ((uint16_t *)V(vd + (fi << SHIFT)))[ei] = lduw(src_addr + (target_ulong)offsets[ei] + sizeof(DATA_TYPE) * fi);
                break;
            case 32:
                ((uint32_t *)V(vd + (fi << SHIFT)))[ei] = ldl(src_addr + (target_ulong)offsets[ei] + sizeof(DATA_TYPE) * fi);
                break;
            case 64: 
                ((uint64_t *)V(vd + (fi << SHIFT)))[ei] = ldq(src_addr + (target_ulong)offsets[ei] + sizeof(DATA_TYPE) * fi);
                break;
            default:
                tlib_abortf("Unsupported EEW");
                break;
            }
        }
    }
}

void glue(glue(helper_vse, BITS), POSTFIX)(CPUState *env, uint32_t vd, uint32_t rs1, uint32_t sumop, uint32_t nf)
{
    const target_ulong emul = EMUL(SHIFT);
    if (emul == 0x7 || V_IDX_INVALID_EMUL(vd, emul) || V_INVALID_NF(vd, nf, emul)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    target_ulong src_addr = env->gpr[rs1];
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        env->vstart = ei;
        for (int fi = 0; fi <= nf; ++fi) {
            glue(st, SUFFIX)(src_addr + ei * DATA_SIZE + (fi << SHIFT), ((DATA_TYPE *)V(vd + (fi << SHIFT)))[ei]);
        }
    }
}

void glue(glue(helper_vsse, BITS), POSTFIX)(CPUState *env, uint32_t vd, uint32_t rs1, uint32_t rs2, uint32_t nf)
{
    const target_ulong emul = EMUL(SHIFT);
    if (emul == 0x7 || V_IDX_INVALID_EMUL(vd, emul) || V_INVALID_NF(vd, nf, emul)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    target_ulong src_addr = env->gpr[rs1];
    target_long offset = env->gpr[rs2];
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        env->vstart = ei;
        for (int fi = 0; fi <= nf; ++fi) {
            glue(st, SUFFIX)(src_addr + ei * offset + (fi << SHIFT), ((DATA_TYPE *)V(vd + (fi << SHIFT)))[ei]);
        }
    }
}

void glue(glue(helper_vsxei, BITS), POSTFIX)(CPUState *env, uint32_t vd, uint32_t rs1, uint32_t vs2, uint32_t nf)
{
    const target_ulong emul = EMUL(SHIFT);
    if (emul == 0x7 || V_IDX_INVALID(vd) || V_IDX_INVALID_EMUL(vs2, emul) || V_INVALID_NF(vd, nf, emul)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    target_ulong src_addr = env->gpr[rs1];
    DATA_TYPE *offsets = (DATA_TYPE *)V(vs2);
    const target_ulong dst_eew = env->vsew;

    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        env->vstart = ei;
        for (int fi = 0; fi <= nf; ++fi) {
            switch (dst_eew) {
            case 8:
                stb(src_addr + (target_ulong)offsets[ei] + (fi << 0), V(vd + (fi << SHIFT))[ei]);
                break;
            case 16:
                stw(src_addr + (target_ulong)offsets[ei] + (fi << 1), ((uint16_t *)V(vd + (fi << SHIFT)))[ei]);
                break;
            case 32:
                stl(src_addr + (target_ulong)offsets[ei] + (fi << 2), ((uint32_t *)V(vd + (fi << SHIFT)))[ei]);
                break;
            case 64:
                stq(src_addr + (target_ulong)offsets[ei] + (fi << 3), ((uint64_t *)V(vd + (fi << SHIFT)))[ei]);
                break;
            default:
                tlib_abortf("Unsupported EEW");
                break;
            }
        }
    }
}

#ifndef V_HELPER_ONCE
#define V_HELPER_ONCE

void helper_vl_wr(CPUState *env, uint32_t vd, uint32_t rs1, uint32_t nf)
{
    uint8_t *v = V(vd);
    target_ulong src_addr = env->gpr[rs1];
    for (int i = 0; i < env->vlenb * nf; ++i) {
        env->vstart = i;
        v[i] = ldub(src_addr + i);
    }
}

void helper_vs_wr(CPUState *env, uint32_t vd, uint32_t rs1, uint32_t nf)
{
    uint8_t *v = V(vd);
    target_ulong src_addr = env->gpr[rs1];
    for (int i = 0; i < env->vlenb * nf; ++i) {
        env->vstart = i;
        stb(src_addr + i, v[i]);
    }
}

void helper_vlm(CPUState *env, uint32_t vd, uint32_t rs1)
{
    uint8_t *v = V(vd);
    target_ulong src_addr = env->gpr[rs1];
    for (int i = env->vstart; i < (env->vl + 7) / 8; ++i) {
        env->vstart = i;
        v[i] = ldub(src_addr + i);
    }
}

void helper_vsm(CPUState *env, uint32_t vd, uint32_t rs1)
{
    uint8_t *v = V(vd);
    target_ulong src_addr = env->gpr[rs1];
    for (int i = env->vstart; i < (env->vl + 7) / 8; ++i) {
        env->vstart = i;
        stb(src_addr + i, v[i]);
    }
}

#endif

#if SHIFT == 0

void glue(helper_vadd_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int64_t imm)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            ((uint8_t *)V(vd))[ei] = ((uint8_t *)V(vs2))[ei] + imm;
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
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vadd_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            ((uint8_t *)V(vd))[ei] = ((uint8_t *)V(vs2))[ei] + ((uint8_t *)V(vs1))[ei];
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
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vsub_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
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
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vrsub_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int64_t imm)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
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
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

#endif

#undef SHIFT
#undef DATA_TYPE
#undef BITS
#undef SUFFIX
#undef USUFFIX
#undef DATA_SIZE
#undef MASKED
#undef POSTFIX

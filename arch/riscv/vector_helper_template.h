
#define DATA_SIZE (1 << SHIFT)

#ifdef MASKED
#define POSTFIX _m
#else
#define POSTFIX
#endif

#if DATA_SIZE == 8
#define BITS       64
#define SUFFIX     q
#define USUFFIX    q
#define DATA_TYPE  uint64_t
#define DATA_STYPE int64_t
#elif DATA_SIZE == 4
#define BITS       32
#define SUFFIX     l
#define USUFFIX    l
#define DATA_TYPE  uint32_t
#define DATA_STYPE int32_t
#elif DATA_SIZE == 2
#define BITS       16
#define SUFFIX     w
#define USUFFIX    uw
#define DATA_TYPE  uint16_t
#define DATA_STYPE int16_t
#elif DATA_SIZE == 1
#define BITS       8
#define SUFFIX     b
#define USUFFIX    ub
#define DATA_TYPE  uint8_t
#define DATA_STYPE int8_t
#else
#error unsupported data size
#endif

#ifdef MASKED

static inline DATA_TYPE glue(roundoff_u, BITS)(DATA_TYPE v, uint16_t d, uint8_t rm)
{
    if (d == 0) {
        return v;
    }
    DATA_TYPE r = 0;
    switch (rm & 0b11) {
    case 0b00: // rnu
        r = (v >> (d - 1)) & 0b1;
        break;
    case 0b01: // rne
        r = ((v >> (d - 1)) & 0b1) && (((v >> d) & 0b1) || (v & ((1 << (d - 1)) - 1)));
        break;
    case 0b10: // rdn
        r = 0;
        break;
    case 0b11: // rod
        r = !((v >> d) & 0b1) && (v & ((1 << d) - 1));
        break;
    }
    return (v >> d) + r;
}

static inline DATA_STYPE glue(roundoff_i, BITS)(DATA_STYPE v, uint16_t d, uint8_t rm)
{
    if (d == 0) {
        return v;
    }
    DATA_STYPE r = 0;
    switch (rm & 0b11) {
    case 0b00: // rnu
        r = (v >> (d - 1)) & 0b1;
        break;
    case 0b01: // rne
        r = ((v >> (d - 1)) & 0b1) && (((v >> d) & 0b1) || (v & ((1 << (d - 1)) - 1)));
        break;
    case 0b10: // rdn
        r = 0;
        break;
    case 0b11: // rod
        r = !((v >> d) & 0b1) && (v & ((1 << d) - 1));
        break;
    }
    return (v >> d) + r;
}

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

#if SHIFT == 3

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

void glue(helper_vwaddu_mvv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((uint16_t *)V(vd))[ei] = ((uint8_t *)V(vs2))[ei] + ((uint8_t *)V(vs1))[ei];
            break;
        case 16:
            ((uint32_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei] + ((uint16_t *)V(vs1))[ei];
            break;
        case 32:
            ((uint64_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] + ((uint32_t *)V(vs1))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwadd_mvv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((uint16_t *)V(vd))[ei] = ((int8_t *)V(vs2))[ei] + ((int8_t *)V(vs1))[ei];
            break;
        case 16:
            ((uint32_t *)V(vd))[ei] = ((int16_t *)V(vs2))[ei] + ((int16_t *)V(vs1))[ei];
            break;
        case 32:
            ((uint64_t *)V(vd))[ei] = ((int32_t *)V(vs2))[ei] + ((int32_t *)V(vs1))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwsubu_mvv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((uint16_t *)V(vd))[ei] = ((uint8_t *)V(vs2))[ei] - ((uint8_t *)V(vs1))[ei];
            break;
        case 16:
            ((uint32_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei] - ((uint16_t *)V(vs1))[ei];
            break;
        case 32:
            ((uint64_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] - ((uint32_t *)V(vs1))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwsub_mvv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((uint16_t *)V(vd))[ei] = ((int8_t *)V(vs2))[ei] - ((int8_t *)V(vs1))[ei];
            break;
        case 16:
            ((uint32_t *)V(vd))[ei] = ((int16_t *)V(vs2))[ei] - ((int16_t *)V(vs1))[ei];
            break;
        case 32:
            ((uint64_t *)V(vd))[ei] = ((int32_t *)V(vs2))[ei] - ((int32_t *)V(vs1))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwaddu_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
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
            ((uint16_t *)V(vd))[ei] = ((uint8_t *)V(vs2))[ei] + rs1;
            break;
        case 16:
            ((uint32_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei] + rs1;
            break;
        case 32:
            ((uint64_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] + rs1;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwadd_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
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
            ((uint16_t *)V(vd))[ei] = ((int8_t *)V(vs2))[ei] + rs1;
            break;
        case 16:
            ((uint32_t *)V(vd))[ei] = ((int16_t *)V(vs2))[ei] + rs1;
            break;
        case 32:
            ((uint64_t *)V(vd))[ei] = ((int32_t *)V(vs2))[ei] + rs1;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwsubu_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
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
            ((uint16_t *)V(vd))[ei] = ((uint8_t *)V(vs2))[ei] - rs1;
            break;
        case 16:
            ((uint32_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei] - rs1;
            break;
        case 32:
            ((uint64_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] - rs1;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwsub_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
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
            ((uint16_t *)V(vd))[ei] = ((int8_t *)V(vs2))[ei] - rs1;
            break;
        case 16:
            ((uint32_t *)V(vd))[ei] = ((int16_t *)V(vs2))[ei] - rs1;
            break;
        case 32:
            ((uint64_t *)V(vd))[ei] = ((int32_t *)V(vs2))[ei] - rs1;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwaddu_mwv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((uint16_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei] + ((uint8_t *)V(vs1))[ei];
            break;
        case 16:
            ((uint32_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] + ((uint16_t *)V(vs1))[ei];
            break;
        case 32:
            ((uint64_t *)V(vd))[ei] = ((uint64_t *)V(vs2))[ei] + ((uint32_t *)V(vs1))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwadd_mwv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((uint16_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei] + ((int8_t *)V(vs1))[ei];
            break;
        case 16:
            ((uint32_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] + ((int16_t *)V(vs1))[ei];
            break;
        case 32:
            ((uint64_t *)V(vd))[ei] = ((uint64_t *)V(vs2))[ei] + ((int32_t *)V(vs1))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwsubu_mwv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((uint16_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei] - ((uint8_t *)V(vs1))[ei];
            break;
        case 16:
            ((uint32_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] - ((uint16_t *)V(vs1))[ei];
            break;
        case 32:
            ((uint64_t *)V(vd))[ei] = ((uint64_t *)V(vs2))[ei] - ((uint32_t *)V(vs1))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwsub_mwv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((uint16_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei] - ((int8_t *)V(vs1))[ei];
            break;
        case 16:
            ((uint32_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] - ((int16_t *)V(vs1))[ei];
            break;
        case 32:
            ((uint64_t *)V(vd))[ei] = ((uint64_t *)V(vs2))[ei] - ((int32_t *)V(vs1))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwaddu_mwx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
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
            ((uint16_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei] + rs1;
            break;
        case 16:
            ((uint32_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] + rs1;
            break;
        case 32:
            ((uint64_t *)V(vd))[ei] = ((uint64_t *)V(vs2))[ei] + rs1;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwadd_mwx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
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
            ((uint16_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei] + rs1;
            break;
        case 16:
            ((uint32_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] + rs1;
            break;
        case 32:
            ((uint64_t *)V(vd))[ei] = ((uint64_t *)V(vs2))[ei] + rs1;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwsubu_mwx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
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
            ((uint16_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei] - rs1;
            break;
        case 16:
            ((uint32_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] - rs1;
            break;
        case 32:
            ((uint64_t *)V(vd))[ei] = ((uint64_t *)V(vs2))[ei] - rs1;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwsub_mwx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
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
            ((uint16_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei] - rs1;
            break;
        case 16:
            ((uint32_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] - rs1;
            break;
        case 32:
            ((uint64_t *)V(vd))[ei] = ((uint64_t *)V(vs2))[ei] - rs1;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmul_mvv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((int8_t *)V(vd))[ei] = ((int8_t *)V(vs2))[ei] * ((int8_t *)V(vs1))[ei];
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = ((int16_t *)V(vs2))[ei] * ((int16_t *)V(vs1))[ei];
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = ((int32_t *)V(vs2))[ei] * ((int32_t *)V(vs1))[ei];
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = ((int64_t *)V(vs2))[ei] * ((int64_t *)V(vs1))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmul_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
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
            ((int8_t *)V(vd))[ei] = ((int8_t *)V(vs2))[ei] * rs1;
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = ((int16_t *)V(vs2))[ei] * rs1;
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = ((int32_t *)V(vs2))[ei] * rs1;
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = ((int64_t *)V(vs2))[ei] * rs1;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmulh_mvv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((int8_t *)V(vd))[ei] = ((int16_t)((int8_t *)V(vs2))[ei] * (int16_t)((int8_t *)V(vs1))[ei]) >> eew;
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = ((int32_t)((int16_t *)V(vs2))[ei] * (int32_t)((int16_t *)V(vs1))[ei]) >> eew;
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = ((int64_t)((int32_t *)V(vs2))[ei] * (int64_t)((int32_t *)V(vs1))[ei]) >> eew;
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = ((__int128_t)((int64_t *)V(vs2))[ei] * (__int128_t)((int64_t *)V(vs1))[ei]) >> eew;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmulh_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
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
            ((int8_t *)V(vd))[ei] = ((int16_t)((int8_t *)V(vs2))[ei] * rs1) >> eew;
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = ((int32_t)((int16_t *)V(vs2))[ei] * rs1) >> eew;
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = ((int64_t)((int32_t *)V(vs2))[ei] * rs1) >> eew;
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = ((__int128_t)((int64_t *)V(vs2))[ei] * rs1) >> eew;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmulhu_mvv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((int8_t *)V(vd))[ei] = ((uint16_t)((uint8_t *)V(vs2))[ei] * (uint16_t)((uint8_t *)V(vs1))[ei]) >> eew;
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = ((uint32_t)((uint16_t *)V(vs2))[ei] * (uint32_t)((uint16_t *)V(vs1))[ei]) >> eew;
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = ((uint64_t)((uint32_t *)V(vs2))[ei] * (uint64_t)((uint32_t *)V(vs1))[ei]) >> eew;
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = ((__uint128_t)((uint64_t *)V(vs2))[ei] * (__uint128_t)((uint64_t *)V(vs1))[ei]) >> eew;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmulhu_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
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
            ((int8_t *)V(vd))[ei] = ((uint16_t)((uint8_t *)V(vs2))[ei] * rs1) >> eew;
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = ((uint32_t)((uint16_t *)V(vs2))[ei] * rs1) >> eew;
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = ((uint64_t)((uint32_t *)V(vs2))[ei] * rs1) >> eew;
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = ((__uint128_t)((uint64_t *)V(vs2))[ei] * rs1) >> eew;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmulhsu_mvv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((int8_t *)V(vd))[ei] = ((int16_t)((int8_t *)V(vs2))[ei] * (uint16_t)((uint8_t *)V(vs1))[ei]) >> eew;
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = ((int32_t)((int16_t *)V(vs2))[ei] * (uint32_t)((uint16_t *)V(vs1))[ei]) >> eew;
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = ((int64_t)((int32_t *)V(vs2))[ei] * (uint64_t)((uint32_t *)V(vs1))[ei]) >> eew;
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = ((__int128_t)((int64_t *)V(vs2))[ei] * (__uint128_t)((uint64_t *)V(vs1))[ei]) >> eew;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmulhsu_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
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
            ((int8_t *)V(vd))[ei] = ((int16_t)((int8_t *)V(vs2))[ei] * rs1) >> eew;
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = ((int32_t)((int16_t *)V(vs2))[ei] * rs1) >> eew;
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = ((int64_t)((int32_t *)V(vs2))[ei] * rs1) >> eew;
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = ((__int128_t)((int64_t *)V(vs2))[ei] * rs1) >> eew;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwmul_mvv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((int16_t *)V(vd))[ei] = ((int16_t)((int8_t *)V(vs2))[ei] * (int16_t)((int8_t *)V(vs1))[ei]) >> eew;
            break;
        case 16:
            ((int32_t *)V(vd))[ei] = ((int32_t)((int16_t *)V(vs2))[ei] * (int32_t)((int16_t *)V(vs1))[ei]) >> eew;
            break;
        case 32:
            ((int64_t *)V(vd))[ei] = ((int64_t)((int32_t *)V(vs2))[ei] * (int64_t)((int32_t *)V(vs1))[ei]) >> eew;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwmul_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
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
            ((int16_t *)V(vd))[ei] = ((int16_t)((int8_t *)V(vs2))[ei] * rs1) >> eew;
            break;
        case 16:
            ((int32_t *)V(vd))[ei] = ((int32_t)((int16_t *)V(vs2))[ei] * rs1) >> eew;
            break;
        case 32:
            ((int64_t *)V(vd))[ei] = ((int64_t)((int32_t *)V(vs2))[ei] * rs1) >> eew;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwmulu_mvv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((uint16_t *)V(vd))[ei] = ((uint16_t)((uint8_t *)V(vs2))[ei] * (uint16_t)((uint8_t *)V(vs1))[ei]) >> eew;
            break;
        case 16:
            ((uint32_t *)V(vd))[ei] = ((uint32_t)((uint16_t *)V(vs2))[ei] * (uint32_t)((uint16_t *)V(vs1))[ei]) >> eew;
            break;
        case 32:
            ((uint64_t *)V(vd))[ei] = ((uint64_t)((uint32_t *)V(vs2))[ei] * (uint64_t)((uint32_t *)V(vs1))[ei]) >> eew;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwmulu_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
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
            ((uint16_t *)V(vd))[ei] = ((uint16_t)((uint8_t *)V(vs2))[ei] * rs1) >> eew;
            break;
        case 16:
            ((uint32_t *)V(vd))[ei] = ((uint32_t)((uint16_t *)V(vs2))[ei] * rs1) >> eew;
            break;
        case 32:
            ((uint64_t *)V(vd))[ei] = ((uint64_t)((uint32_t *)V(vs2))[ei] * rs1) >> eew;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwmulsu_mvv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((int16_t *)V(vd))[ei] = ((int16_t)((int8_t *)V(vs2))[ei] * (uint16_t)((uint8_t *)V(vs1))[ei]) >> eew;
            break;
        case 16:
            ((int32_t *)V(vd))[ei] = ((int32_t)((int16_t *)V(vs2))[ei] * (uint32_t)((uint16_t *)V(vs1))[ei]) >> eew;
            break;
        case 32:
            ((int64_t *)V(vd))[ei] = ((int64_t)((int32_t *)V(vs2))[ei] * (uint64_t)((uint32_t *)V(vs1))[ei]) >> eew;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwmulsu_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
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
            ((int16_t *)V(vd))[ei] = ((int16_t)((int8_t *)V(vs2))[ei] * rs1) >> eew;
            break;
        case 16:
            ((int32_t *)V(vd))[ei] = ((int32_t)((int16_t *)V(vs2))[ei] * rs1) >> eew;
            break;
        case 32:
            ((int64_t *)V(vd))[ei] = ((int64_t)((int32_t *)V(vs2))[ei] * rs1) >> eew;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vminu_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
        case 8: {
            uint8_t v2 = ((uint8_t *)V(vs2))[ei];
            uint8_t v1 = ((uint8_t *)V(vs1))[ei];
            ((uint8_t *)V(vd))[ei] = v2 < v1 ? v2 : v1;
            break;
        }
        case 16: {
            uint16_t v2 = ((uint16_t *)V(vs2))[ei];
            uint16_t v1 = ((uint16_t *)V(vs1))[ei];
            ((uint16_t *)V(vd))[ei] = v2 < v1 ? v2 : v1;
            break;
        }
        case 32: {
            uint32_t v2 = ((uint32_t *)V(vs2))[ei];
            uint32_t v1 = ((uint32_t *)V(vs1))[ei];
            ((uint32_t *)V(vd))[ei] = v2 < v1 ? v2 : v1;
            break;
        }
        case 64: {
            uint64_t v2 = ((uint64_t *)V(vs2))[ei];
            uint64_t v1 = ((uint64_t *)V(vs1))[ei];
            ((uint64_t *)V(vd))[ei] = v2 < v1 ? v2 : v1;
            break;
        }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vminu_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
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
        case 8: {
                uint8_t v2 = ((uint8_t *)V(vs2))[ei];
                ((uint8_t *)V(vd))[ei] = v2 < rs1 ? v2 : rs1;
                break;
            }
        case 16: {
                uint16_t v2 = ((uint16_t *)V(vs2))[ei];
                ((uint16_t *)V(vd))[ei] = v2 < rs1 ? v2 : rs1;
                break;
            }
        case 32: {
                uint32_t v2 = ((uint32_t *)V(vs2))[ei];
                ((uint32_t *)V(vd))[ei] = v2 < rs1 ? v2 : rs1;
                break;
            }
        case 64: {
                uint64_t v2 = ((uint64_t *)V(vs2))[ei];
                ((uint64_t *)V(vd))[ei] = v2 < rs1 ? v2 : rs1;
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmin_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
        case 8: {
            int8_t v2 = ((int8_t *)V(vs2))[ei];
            int8_t v1 = ((int8_t *)V(vs1))[ei];
            ((int8_t *)V(vd))[ei] = v2 < v1 ? v2 : v1;
            break;
        }
        case 16: {
            int16_t v2 = ((int16_t *)V(vs2))[ei];
            int16_t v1 = ((int16_t *)V(vs1))[ei];
            ((int16_t *)V(vd))[ei] = v2 < v1 ? v2 : v1;
            break;
        }
        case 32: {
            int32_t v2 = ((int32_t *)V(vs2))[ei];
            int32_t v1 = ((int32_t *)V(vs1))[ei];
            ((int32_t *)V(vd))[ei] = v2 < v1 ? v2 : v1;
            break;
        }
        case 64: {
            int64_t v2 = ((int64_t *)V(vs2))[ei];
            int64_t v1 = ((int64_t *)V(vs1))[ei];
            ((int64_t *)V(vd))[ei] = v2 < v1 ? v2 : v1;
            break;
        }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmin_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
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
        case 8: {
                int8_t v2 = ((int8_t *)V(vs2))[ei];
                ((int8_t *)V(vd))[ei] = v2 < rs1 ? v2 : rs1;
                break;
            }
        case 16: {
                int16_t v2 = ((int16_t *)V(vs2))[ei];
                ((int16_t *)V(vd))[ei] = v2 < rs1 ? v2 : rs1;
                break;
            }
        case 32: {
                int32_t v2 = ((int32_t *)V(vs2))[ei];
                ((int32_t *)V(vd))[ei] = v2 < rs1 ? v2 : rs1;
                break;
            }
        case 64: {
                int64_t v2 = ((int64_t *)V(vs2))[ei];
                ((int64_t *)V(vd))[ei] = v2 < rs1 ? v2 : rs1;
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmaxu_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
        case 8: {
            uint8_t v2 = ((uint8_t *)V(vs2))[ei];
            uint8_t v1 = ((uint8_t *)V(vs1))[ei];
            ((uint8_t *)V(vd))[ei] = v2 > v1 ? v2 : v1;
            break;
        }
        case 16: {
            uint16_t v2 = ((uint16_t *)V(vs2))[ei];
            uint16_t v1 = ((uint16_t *)V(vs1))[ei];
            ((uint16_t *)V(vd))[ei] = v2 > v1 ? v2 : v1;
            break;
        }
        case 32: {
            uint32_t v2 = ((uint32_t *)V(vs2))[ei];
            uint32_t v1 = ((uint32_t *)V(vs1))[ei];
            ((uint32_t *)V(vd))[ei] = v2 > v1 ? v2 : v1;
            break;
        }
        case 64: {
            uint64_t v2 = ((uint64_t *)V(vs2))[ei];
            uint64_t v1 = ((uint64_t *)V(vs1))[ei];
            ((uint64_t *)V(vd))[ei] = v2 > v1 ? v2 : v1;
            break;
        }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmaxu_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
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
        case 8: {
                uint8_t v2 = ((uint8_t *)V(vs2))[ei];
                ((uint8_t *)V(vd))[ei] = v2 > rs1 ? v2 : rs1;
                break;
            }
        case 16: {
                uint16_t v2 = ((uint16_t *)V(vs2))[ei];
                ((uint16_t *)V(vd))[ei] = v2 > rs1 ? v2 : rs1;
                break;
            }
        case 32: {
                uint32_t v2 = ((uint32_t *)V(vs2))[ei];
                ((uint32_t *)V(vd))[ei] = v2 > rs1 ? v2 : rs1;
                break;
            }
        case 64: {
                uint64_t v2 = ((uint64_t *)V(vs2))[ei];
                ((uint64_t *)V(vd))[ei] = v2 > rs1 ? v2 : rs1;
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmax_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
        case 8: {
            int8_t v2 = ((int8_t *)V(vs2))[ei];
            int8_t v1 = ((int8_t *)V(vs1))[ei];
            ((int8_t *)V(vd))[ei] = v2 > v1 ? v2 : v1;
            break;
        }
        case 16: {
            int16_t v2 = ((int16_t *)V(vs2))[ei];
            int16_t v1 = ((int16_t *)V(vs1))[ei];
            ((int16_t *)V(vd))[ei] = v2 > v1 ? v2 : v1;
            break;
        }
        case 32: {
            int32_t v2 = ((int32_t *)V(vs2))[ei];
            int32_t v1 = ((int32_t *)V(vs1))[ei];
            ((int32_t *)V(vd))[ei] = v2 > v1 ? v2 : v1;
            break;
        }
        case 64: {
            int64_t v2 = ((int64_t *)V(vs2))[ei];
            int64_t v1 = ((int64_t *)V(vs1))[ei];
            ((int64_t *)V(vd))[ei] = v2 > v1 ? v2 : v1;
            break;
        }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmax_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
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
        case 8: {
                int8_t v2 = ((int8_t *)V(vs2))[ei];
                ((int8_t *)V(vd))[ei] = v2 > rs1 ? v2 : rs1;
                break;
            }
        case 16: {
                int16_t v2 = ((int16_t *)V(vs2))[ei];
                ((int16_t *)V(vd))[ei] = v2 > rs1 ? v2 : rs1;
                break;
            }
        case 32: {
                int32_t v2 = ((int32_t *)V(vs2))[ei];
                ((int32_t *)V(vd))[ei] = v2 > rs1 ? v2 : rs1;
                break;
            }
        case 64: {
                int64_t v2 = ((int64_t *)V(vs2))[ei];
                ((int64_t *)V(vd))[ei] = v2 > rs1 ? v2 : rs1;
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vnsrl_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint16_t shift = rs1 & ((eew << 1) - 1);
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            ((uint8_t *)V(vd))[ei] = ((uint16_t *)V(vd))[vs2] >> shift;
            break;
        case 16:
            ((uint16_t *)V(vd))[ei] = ((uint32_t *)V(vd))[vs2] >> shift;
            break;
        case 32:
            ((uint32_t *)V(vd))[ei] = ((uint64_t *)V(vd))[vs2] >> shift;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vnsrl_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint16_t v1_mask = (eew << 1) - 1;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            ((uint8_t *)V(vd))[ei] = ((uint16_t *)V(vd))[vs2] >> (((uint8_t *)V(vd))[vs1] & v1_mask);
            break;
        case 16:
            ((uint16_t *)V(vd))[ei] = ((uint32_t *)V(vd))[vs2] >> (((uint16_t *)V(vd))[vs1] & v1_mask);
            break;
        case 32:
            ((uint32_t *)V(vd))[ei] = ((uint64_t *)V(vd))[vs2] >> (((uint32_t *)V(vd))[vs1] & v1_mask);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vnsra_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint16_t shift = rs1 & ((eew << 1) - 1);
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            ((int8_t *)V(vd))[ei] = ((int16_t *)V(vd))[vs2] >> shift;
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = ((int32_t *)V(vd))[vs2] >> shift;
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = ((int64_t *)V(vd))[vs2] >> shift;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vnsra_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint16_t v1_mask = (eew << 1) - 1;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            ((int8_t *)V(vd))[ei] = ((int16_t *)V(vd))[vs2] >> (((uint8_t *)V(vd))[vs1] & v1_mask);
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = ((int32_t *)V(vd))[vs2] >> (((uint16_t *)V(vd))[vs1] & v1_mask);
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = ((int64_t *)V(vd))[vs2] >> (((uint32_t *)V(vd))[vs1] & v1_mask);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vnclipu_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint16_t v1_mask = (eew << 1) - 1;
    const uint8_t rm = env->vxrm & 0b11;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            ((uint8_t *)V(vd))[ei] = roundoff_u16(((uint16_t *)V(vd))[vs2], ((uint8_t *)V(vd))[vs1] & v1_mask, rm);
            break;
        case 16:
            ((uint16_t *)V(vd))[ei] = roundoff_u32(((uint32_t *)V(vd))[vs2], ((uint16_t *)V(vd))[vs1] & v1_mask, rm);
            break;
        case 32:
            ((uint32_t *)V(vd))[ei] = roundoff_u64(((uint64_t *)V(vd))[vs2], ((uint32_t *)V(vd))[vs1] & v1_mask, rm);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vnclipu_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint16_t shift = rs1 & ((eew << 1) - 1);
    const uint8_t rm = env->vxrm & 0b11;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            ((uint8_t *)V(vd))[ei] = roundoff_u16(((uint16_t *)V(vd))[vs2], shift, rm);
            break;
        case 16:
            ((uint16_t *)V(vd))[ei] = roundoff_u32(((uint32_t *)V(vd))[vs2], shift, rm);
            break;
        case 32:
            ((uint32_t *)V(vd))[ei] = roundoff_u64(((uint64_t *)V(vd))[vs2], shift, rm);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vnclip_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint16_t v1_mask = (eew << 1) - 1;
    const uint8_t rm = env->vxrm & 0b11;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            ((int8_t *)V(vd))[ei] = roundoff_i16(((int16_t *)V(vd))[vs2], ((uint8_t *)V(vd))[vs1] & v1_mask, rm);
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = roundoff_i32(((int32_t *)V(vd))[vs2], ((uint16_t *)V(vd))[vs1] & v1_mask, rm);
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = roundoff_i64(((int64_t *)V(vd))[vs2], ((uint32_t *)V(vd))[vs1] & v1_mask, rm);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vnclip_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint16_t shift = rs1 & ((eew << 1) - 1);
    const uint8_t rm = env->vxrm & 0b11;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            ((int8_t *)V(vd))[ei] = roundoff_i16(((int16_t *)V(vd))[vs2], shift, rm);
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = roundoff_i32(((int32_t *)V(vd))[vs2], shift, rm);
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = roundoff_i64(((int64_t *)V(vd))[vs2], shift, rm);
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
#undef DATA_STYPE
#undef BITS
#undef SUFFIX
#undef USUFFIX
#undef DATA_SIZE
#undef MASKED
#undef POSTFIX

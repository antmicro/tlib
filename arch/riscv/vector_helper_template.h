
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
        r = ((v >> (d - 1)) & 0b1) && (((v >> d) & 0b1) || (d > 1 && (v & ((1 << (d - 1)) - 1))));
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
        r = ((v >> (d - 1)) & 0b1) && (((v >> d) & 0b1) || (d > 1 && (v & ((1 << (d - 1)) - 1))));
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

static inline DATA_TYPE glue(divu_, BITS)(DATA_TYPE dividend, DATA_TYPE divisor)
{
    if (divisor == 0) {
        return glue(glue(UINT, BITS), _MAX);
    } else {
        return dividend / divisor;
    }
}

static inline DATA_STYPE glue(div_, BITS)(DATA_STYPE dividend, DATA_STYPE divisor)
{
    if (divisor == 0) {
        return -1;
    } else if (divisor == -1 && dividend == glue(glue(INT, BITS), _MIN)) {
        return glue(glue(INT, BITS), _MIN);
    } else {
        return dividend / divisor;
    }
}

static inline DATA_TYPE glue(remu_, BITS)(DATA_TYPE dividend, DATA_TYPE divisor)
{
    if (divisor == 0) {
        return dividend;
    } else {
        return dividend % divisor;
    }
}

static inline DATA_STYPE glue(rem_, BITS)(DATA_STYPE dividend, DATA_STYPE divisor)
{
    if (divisor == 0) {
        return dividend;
    } else if (divisor == -1 && dividend == glue(glue(INT, BITS), _MIN)) {
        return 0;
    } else {
        return dividend % divisor;
    }
}

#endif

void glue(glue(helper_vle, BITS), POSTFIX)(CPUState *env, uint32_t vd, uint32_t rs1, uint32_t nf)
{
    const target_ulong emul = EMUL(SHIFT);
    if (emul == 0x4 || V_IDX_INVALID_EMUL(vd, emul) || V_INVALID_NF(vd, nf, emul)) {
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

void glue(glue(glue(helper_vle, BITS), ff), POSTFIX)(CPUState *env, uint32_t vd, uint32_t rs1, uint32_t nf)
{
    const target_ulong emul = EMUL(SHIFT);
    if (emul == 0x4 || V_IDX_INVALID_EMUL(vd, emul) || V_INVALID_NF(vd, nf, emul)) {
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
        DATA_TYPE value = glue(ld, USUFFIX)(src_addr + ei * DATA_SIZE);
        for (int fi = 0; fi <= nf; ++fi) {
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
    if (emul == 0x4 || V_IDX_INVALID_EMUL(vd, emul) || V_INVALID_NF(vd, nf, emul)) {
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
        DATA_TYPE data = glue(ld, USUFFIX)(src_addr + ei * offset);
        for (int fi = 0; fi <= nf; ++fi) {
            ((DATA_TYPE *)V(vd + (fi << SHIFT)))[ei] = data;
        }
    }
}

void glue(glue(helper_vlxei, BITS), POSTFIX)(CPUState *env, uint32_t vd, uint32_t rs1, uint32_t vs2, uint32_t nf)
{
    const target_ulong emul = EMUL(SHIFT);
    if (emul == 0x4 || V_IDX_INVALID(vd) || V_IDX_INVALID_EMUL(vs2, emul) || V_INVALID_NF(vd, nf, emul)) {
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
                helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
                break;
            }
        }
    }
}

void glue(glue(helper_vse, BITS), POSTFIX)(CPUState *env, uint32_t vd, uint32_t rs1, uint32_t nf)
{
    const target_ulong emul = EMUL(SHIFT);
    if (emul == 0x4 || V_IDX_INVALID_EMUL(vd, emul) || V_INVALID_NF(vd, nf, emul)) {
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
    if (emul == 0x4 || V_IDX_INVALID_EMUL(vd, emul) || V_INVALID_NF(vd, nf, emul)) {
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
    if (emul == 0x4 || V_IDX_INVALID(vd) || V_IDX_INVALID_EMUL(vs2, emul) || V_INVALID_NF(vd, nf, emul)) {
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
                helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
                break;
            }
        }
    }
}

#ifndef V_HELPER_ONCE
#define V_HELPER_ONCE

static inline __uint128_t roundoff_u128(__uint128_t v, uint16_t d, uint8_t rm)
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
        r = ((v >> (d - 1)) & 0b1) && (((v >> d) & 0b1) || (d > 1 && (v & ((1 << (d - 1)) - 1))));
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

static inline __int128_t roundoff_i128(__int128_t v, uint16_t d, uint8_t rm)
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
        r = ((v >> (d - 1)) & 0b1) && (((v >> d) & 0b1) || (d > 1 && (v & ((1 << (d - 1)) - 1))));
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
            ((uint16_t *)V(vd))[ei] = ((uint8_t *)V(vs2))[ei] + ((uint8_t)rs1);
            break;
        case 16:
            ((uint32_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei] + ((uint16_t)rs1);
            break;
        case 32:
            ((uint64_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] + ((uint32_t)rs1);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwadd_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
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
            ((int16_t *)V(vd))[ei] = ((int8_t *)V(vs2))[ei] + (int16_t)(int8_t)rs1;
            break;
        case 16:
            ((int32_t *)V(vd))[ei] = ((int16_t *)V(vs2))[ei] + (int32_t)(int16_t)rs1;
            break;
        case 32:
            ((int64_t *)V(vd))[ei] = ((int32_t *)V(vs2))[ei] + (int64_t)(int32_t)rs1;
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
            ((uint16_t *)V(vd))[ei] = ((uint8_t *)V(vs2))[ei] - ((uint8_t)rs1);
            break;
        case 16:
            ((uint32_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei] - ((uint16_t)rs1);
            break;
        case 32:
            ((uint64_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] - ((uint32_t)rs1);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwsub_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
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
            ((uint16_t *)V(vd))[ei] = ((int8_t *)V(vs2))[ei] - (int16_t)(int8_t)rs1;
            break;
        case 16:
            ((uint32_t *)V(vd))[ei] = ((int16_t *)V(vs2))[ei] - (int32_t)(int16_t)rs1;
            break;
        case 32:
            ((uint64_t *)V(vd))[ei] = ((int32_t *)V(vs2))[ei] - (int64_t)(int32_t)rs1;
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
            ((uint16_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei] + ((uint16_t)rs1);
            break;
        case 16:
            ((uint32_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] + ((uint32_t)rs1);
            break;
        case 32:
            ((uint64_t *)V(vd))[ei] = ((uint64_t *)V(vs2))[ei] + ((uint64_t)rs1);
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
            ((int16_t *)V(vd))[ei] = ((int16_t *)V(vs2))[ei] + (int8_t)rs1;
            break;
        case 16:
            ((int32_t *)V(vd))[ei] = ((int32_t *)V(vs2))[ei] + (int16_t)rs1;
            break;
        case 32:
            ((int64_t *)V(vd))[ei] = ((int64_t *)V(vs2))[ei] + (int32_t)rs1;
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
            ((uint16_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei] - ((uint16_t)rs1);
            break;
        case 16:
            ((uint32_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] - ((uint32_t)rs1);
            break;
        case 32:
            ((uint64_t *)V(vd))[ei] = ((uint64_t *)V(vs2))[ei] - ((uint64_t)rs1);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwsub_mwx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
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
            ((int16_t *)V(vd))[ei] = ((int16_t *)V(vs2))[ei] - (int8_t)rs1;
            break;
        case 16:
            ((int32_t *)V(vd))[ei] = ((int32_t *)V(vs2))[ei] - (int16_t)rs1;
            break;
        case 32:
            ((int64_t *)V(vd))[ei] = ((int64_t *)V(vs2))[ei] - (int32_t)rs1;
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
            ((int8_t *)V(vd))[ei] = ((int8_t *)V(vs2))[ei] * ((int8_t)rs1);
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = ((int16_t *)V(vs2))[ei] * ((int16_t)rs1);
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = ((int32_t *)V(vs2))[ei] * ((int32_t)rs1);
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = ((int64_t *)V(vs2))[ei] * ((int64_t)rs1);
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
            ((int8_t *)V(vd))[ei] = ((int16_t)((int8_t *)V(vs2))[ei] * ((int16_t)((int8_t)rs1))) >> eew;
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = ((int32_t)((int16_t *)V(vs2))[ei] * ((int32_t)((int16_t)rs1))) >> eew;
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = ((int64_t)((int32_t *)V(vs2))[ei] * ((int64_t)((int32_t)rs1))) >> eew;
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = ((__int128_t)((int64_t *)V(vs2))[ei] * ((__int128_t)((int64_t)rs1))) >> eew;
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
            ((int8_t *)V(vd))[ei] = ((uint16_t)((uint8_t *)V(vs2))[ei] * ((uint16_t)((uint8_t)rs1))) >> eew;
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = ((uint32_t)((uint16_t *)V(vs2))[ei] * ((uint32_t)((uint16_t)rs1))) >> eew;
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = ((uint64_t)((uint32_t *)V(vs2))[ei] * ((uint64_t)((uint32_t)rs1))) >> eew;
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = ((__uint128_t)((uint64_t *)V(vs2))[ei] * ((__uint128_t)((uint64_t)rs1))) >> eew;
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
            ((int8_t *)V(vd))[ei] = ((int16_t)((int8_t *)V(vs2))[ei] * ((int16_t)((int8_t)rs1))) >> eew;
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = ((int32_t)((int16_t *)V(vs2))[ei] * ((int32_t)((int16_t)rs1))) >> eew;
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = ((int64_t)((int32_t *)V(vs2))[ei] * ((int64_t)((int32_t)rs1))) >> eew;
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = ((__int128_t)((int64_t *)V(vs2))[ei] * ((__int128_t)((int64_t)rs1))) >> eew;
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
            ((int16_t *)V(vd))[ei] = ((int16_t)((int8_t *)V(vs2))[ei] * ((int16_t)((int8_t)rs1))) >> eew;
            break;
        case 16:
            ((int32_t *)V(vd))[ei] = ((int32_t)((int16_t *)V(vs2))[ei] * ((int32_t)((int16_t)rs1))) >> eew;
            break;
        case 32:
            ((int64_t *)V(vd))[ei] = ((int64_t)((int32_t *)V(vs2))[ei] * ((int64_t)((int32_t)rs1))) >> eew;
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
            ((uint16_t *)V(vd))[ei] = ((uint16_t)((uint8_t *)V(vs2))[ei] * ((uint16_t)((uint8_t)rs1))) >> eew;
            break;
        case 16:
            ((uint32_t *)V(vd))[ei] = ((uint32_t)((uint16_t *)V(vs2))[ei] * ((uint32_t)((uint16_t)rs1))) >> eew;
            break;
        case 32:
            ((uint64_t *)V(vd))[ei] = ((uint64_t)((uint32_t *)V(vs2))[ei] * ((uint64_t)((uint32_t)rs1))) >> eew;
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
            ((int16_t *)V(vd))[ei] = ((int16_t)((int8_t *)V(vs2))[ei] * ((int16_t)((int8_t)rs1))) >> eew;
            break;
        case 16:
            ((int32_t *)V(vd))[ei] = ((int32_t)((int16_t *)V(vs2))[ei] * ((int32_t)((int16_t)rs1))) >> eew;
            break;
        case 32:
            ((int64_t *)V(vd))[ei] = ((int64_t)((int32_t *)V(vs2))[ei] * ((int64_t)((int32_t)rs1))) >> eew;
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
                ((uint8_t *)V(vd))[ei] = v2 < (uint8_t)rs1 ? v2 : rs1;
                break;
            }
        case 16: {
                uint16_t v2 = ((uint16_t *)V(vs2))[ei];
                ((uint16_t *)V(vd))[ei] = v2 < (uint16_t)rs1 ? v2 : rs1;
                break;
            }
        case 32: {
                uint32_t v2 = ((uint32_t *)V(vs2))[ei];
                ((uint32_t *)V(vd))[ei] = v2 < (uint32_t)rs1 ? v2 : rs1;
                break;
            }
        case 64: {
                uint64_t v2 = ((uint64_t *)V(vs2))[ei];
                ((uint64_t *)V(vd))[ei] = v2 < (uint64_t)rs1 ? v2 : rs1;
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
                ((int8_t *)V(vd))[ei] = v2 < (int8_t)rs1 ? v2 : rs1;
                break;
            }
        case 16: {
                int16_t v2 = ((int16_t *)V(vs2))[ei];
                ((int16_t *)V(vd))[ei] = v2 < (int16_t)rs1 ? v2 : rs1;
                break;
            }
        case 32: {
                int32_t v2 = ((int32_t *)V(vs2))[ei];
                ((int32_t *)V(vd))[ei] = v2 < (int32_t)rs1 ? v2 : rs1;
                break;
            }
        case 64: {
                int64_t v2 = ((int64_t *)V(vs2))[ei];
                ((int64_t *)V(vd))[ei] = v2 < (int64_t)rs1 ? v2 : rs1;
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
                ((uint8_t *)V(vd))[ei] = v2 > (uint8_t)rs1 ? v2 : rs1;
                break;
            }
        case 16: {
                uint16_t v2 = ((uint16_t *)V(vs2))[ei];
                ((uint16_t *)V(vd))[ei] = v2 > (uint16_t)rs1 ? v2 : rs1;
                break;
            }
        case 32: {
                uint32_t v2 = ((uint32_t *)V(vs2))[ei];
                ((uint32_t *)V(vd))[ei] = v2 > (uint32_t)rs1 ? v2 : rs1;
                break;
            }
        case 64: {
                uint64_t v2 = ((uint64_t *)V(vs2))[ei];
                ((uint64_t *)V(vd))[ei] = v2 > (uint64_t)rs1 ? v2 : rs1;
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
                ((int8_t *)V(vd))[ei] = v2 > (int8_t)rs1 ? v2 : (int8_t)rs1;
                break;
            }
        case 16: {
                int16_t v2 = ((int16_t *)V(vs2))[ei];
                ((int16_t *)V(vd))[ei] = v2 > (int16_t)rs1 ? v2 : (int16_t)rs1;
                break;
            }
        case 32: {
                int32_t v2 = ((int32_t *)V(vs2))[ei];
                ((int32_t *)V(vd))[ei] = v2 > (int32_t)rs1 ? v2 : (int32_t)rs1;
                break;
            }
        case 64: {
                int64_t v2 = ((int64_t *)V(vs2))[ei];
                ((int64_t *)V(vd))[ei] = v2 > (int64_t)rs1 ? v2 : (int64_t)rs1;
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

void glue(helper_vwredsumu_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    uint64_t acc = 0;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            acc += ((uint8_t *)V(vs2))[ei];
            break;
        case 16:
            acc += ((uint16_t *)V(vs2))[ei];
            break;
        case 32:
            acc += ((uint32_t *)V(vs2))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
    switch (eew) {
    case 8:
        ((uint16_t *)V(vd))[0] = acc + ((uint16_t *)V(vs1))[0];
        break;
    case 16:
        ((uint32_t *)V(vd))[0] = acc + ((uint32_t *)V(vs1))[0];
        break;
    case 32:
        ((uint64_t *)V(vd))[0] = acc + ((uint64_t *)V(vs1))[0];
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        break;
    }
}

void glue(helper_vwredsum_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    int64_t acc = 0;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            acc += ((int8_t *)V(vs2))[ei];
            break;
        case 16:
            acc += ((int16_t *)V(vs2))[ei];
            break;
        case 32:
            acc += ((int32_t *)V(vs2))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
    switch (eew) {
    case 8:
        ((int16_t *)V(vd))[0] = acc + ((int16_t *)V(vs1))[0];
        break;
    case 16:
        ((int32_t *)V(vd))[0] = acc + ((int32_t *)V(vs1))[0];
        break;
    case 32:
        ((int64_t *)V(vd))[0] = acc + ((int64_t *)V(vs1))[0];
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        break;
    }
}

void glue(helper_vslideup_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const int start = rs1 > env->vstart ? rs1 : env->vstart;
    for (int ei = start; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            ((int8_t *)V(vd))[ei] = ((int8_t *)V(vs2))[ei - rs1];
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = ((int16_t *)V(vs2))[ei - rs1];
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = ((int32_t *)V(vs2))[ei - rs1];
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = ((int64_t *)V(vs2))[ei - rs1];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vslidedown_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const int src_max = env->vl < env->vlmax - rs1 ? env->vl : env->vlmax - rs1;
    for (int ei = env->vstart; ei < src_max; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            ((int8_t *)V(vd))[ei] = ((int8_t *)V(vs2))[ei + rs1];
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = ((int16_t *)V(vs2))[ei + rs1];
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = ((int32_t *)V(vs2))[ei + rs1];
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = ((int64_t *)V(vs2))[ei + rs1];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
    for (int ei = src_max; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            ((int8_t *)V(vd))[ei] = 0;
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = 0;
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = 0;
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = 0;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vslide1up, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, uint64_t rs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;

    if (env->vstart == 0
#ifdef MASKED
        && (V(0)[0] & 0x1)
#endif
    ) {
        switch (eew) {
        case 8:
            ((int8_t *)V(vd))[0] = rs1;
            break;
        case 16:
            ((int16_t *)V(vd))[0] = rs1;
            break;
        case 32:
            ((int32_t *)V(vd))[0] = rs1;
            break;
        case 64:
            ((int64_t *)V(vd))[0] = rs1;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            ((int8_t *)V(vd))[ei] = ((int8_t *)V(vs2))[ei - 1];
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = ((int16_t *)V(vs2))[ei - 1];
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = ((int32_t *)V(vs2))[ei - 1];
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = ((int64_t *)V(vs2))[ei - 1];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vslide1down, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, uint64_t rs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const int src_max = env->vl - 1;

    for (int ei = env->vstart; ei < src_max; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            ((int8_t *)V(vd))[ei] = ((int8_t *)V(vs2))[ei + 1];
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = ((int16_t *)V(vs2))[ei + 1];
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = ((int32_t *)V(vs2))[ei + 1];
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = ((int64_t *)V(vs2))[ei + 1];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
#ifdef MASKED
    if(V(0)[src_max >> 3] & (1 << (src_max & 0x7))) {
#endif
        switch (eew) {
        case 8:
            ((int8_t *)V(vd))[src_max] = rs1;
            break;
        case 16:
            ((int16_t *)V(vd))[src_max] = rs1;
            break;
        case 32:
            ((int32_t *)V(vd))[src_max] = rs1;
            break;
        case 64:
            ((int64_t *)V(vd))[src_max] = rs1;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
#ifdef MASKED
    }
#endif
}

void glue(helper_vrgather_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((int8_t *)V(vs2))[ei] = ((uint8_t *)V(vs1))[ei] >= env->vlmax ? 0 : ((int8_t *)V(vs2))[((uint8_t *)V(vs1))[ei]];
            break;
        case 16:
            ((int16_t *)V(vs2))[ei] = ((uint16_t *)V(vs1))[ei] >= env->vlmax ? 0 : ((int16_t *)V(vs2))[((uint16_t *)V(vs1))[ei]];
            break;
        case 32:
            ((int32_t *)V(vs2))[ei] = ((uint32_t *)V(vs1))[ei] >= env->vlmax ? 0 : ((int32_t *)V(vs2))[((uint32_t *)V(vs1))[ei]];
            break;
        case 64:
            ((int64_t *)V(vs2))[ei] = ((uint64_t *)V(vs1))[ei] >= env->vlmax ? 0 : ((int64_t *)V(vs2))[((uint64_t *)V(vs1))[ei]];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vrgatherei16_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
        uint16_t idx = ((uint16_t *)V(vs1))[ei];
        switch (eew) {
        case 8:
            ((int8_t *)V(vs2))[ei] = idx >= env->vlmax ? 0 : ((int8_t *)V(vs2))[idx];
            break;
        case 16:
            ((int16_t *)V(vs2))[ei] = idx >= env->vlmax ? 0 : ((int16_t *)V(vs2))[idx];
            break;
        case 32:
            ((int32_t *)V(vs2))[ei] = idx >= env->vlmax ? 0 : ((int32_t *)V(vs2))[idx];
            break;
        case 64:
            ((int64_t *)V(vs2))[ei] = idx >= env->vlmax ? 0 : ((int64_t *)V(vs2))[idx];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vrgather_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
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
            ((int8_t *)V(vs2))[ei] = rs1 >= env->vlmax ? 0 : ((int8_t *)V(vs2))[rs1];
            break;
        case 16:
            ((int16_t *)V(vs2))[ei] = rs1 >= env->vlmax ? 0 : ((int16_t *)V(vs2))[rs1];
            break;
        case 32:
            ((int32_t *)V(vs2))[ei] = rs1 >= env->vlmax ? 0 : ((int32_t *)V(vs2))[rs1];
            break;
        case 64:
            ((int64_t *)V(vs2))[ei] = rs1 >= env->vlmax ? 0 : ((int64_t *)V(vs2))[rs1];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vzext_vf2, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
{
    const target_ulong eew = env->vsew;
    if (eew < 16 || V_IDX_INVALID(vd) || V_IDX_INVALID_EMUL(vs2, eew >> 1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }

    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 16:
            ((uint16_t *)V(vd))[ei] = ((uint8_t *)V(vs2))[ei];
            break;
        case 32:
            ((uint32_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei];
            break;
        case 64: 
            ((uint64_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vsext_vf2, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
{
    const target_ulong eew = env->vsew;
    if (eew < 16 || V_IDX_INVALID(vd) || V_IDX_INVALID_EMUL(vs2, eew >> 1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }

    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 16:
            ((int16_t *)V(vd))[ei] = ((int8_t *)V(vs2))[ei];
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = ((int16_t *)V(vs2))[ei];
            break;
        case 64: 
            ((int64_t *)V(vd))[ei] = ((int32_t *)V(vs2))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vzext_vf4, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
{
    const target_ulong eew = env->vsew;
    if (eew < 32 || V_IDX_INVALID(vd) || V_IDX_INVALID_EMUL(vs2, eew >> 2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }

    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((uint32_t *)V(vd))[ei] = ((uint8_t *)V(vs2))[ei];
            break;
        case 64: 
            ((uint64_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vsext_vf4, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
{
    const target_ulong eew = env->vsew;
    if (eew < 32 || V_IDX_INVALID(vd) || V_IDX_INVALID_EMUL(vs2, eew >> 2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }

    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 32:
            ((int32_t *)V(vd))[ei] = ((int8_t *)V(vs2))[ei];
            break;
        case 64: 
            ((int64_t *)V(vd))[ei] = ((int16_t *)V(vs2))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vzext_vf8, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
{
    const target_ulong eew = env->vsew;
    if (eew < 64 || V_IDX_INVALID(vd) || V_IDX_INVALID_EMUL(vs2, eew >> 3)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }

    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 64: 
            ((uint64_t *)V(vd))[ei] = ((uint8_t *)V(vs2))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vsext_vf8, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2)
{
    const target_ulong eew = env->vsew;
    if (eew < 64 || V_IDX_INVALID(vd) || V_IDX_INVALID_EMUL(vs2, eew >> 3)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }

    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 64: 
            ((int64_t *)V(vd))[ei] = ((int8_t *)V(vs2))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vand_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((int8_t *)V(vd))[ei] = ((int8_t *)V(vs2))[ei] & ((int8_t *)V(vs1))[ei];
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = ((int16_t *)V(vs2))[ei] & ((int16_t *)V(vs1))[ei];
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = ((int32_t *)V(vs2))[ei] & ((int32_t *)V(vs1))[ei];
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = ((int64_t *)V(vs2))[ei] & ((int64_t *)V(vs1))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vor_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((int8_t *)V(vd))[ei] = ((int8_t *)V(vs2))[ei] | ((int8_t *)V(vs1))[ei];
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = ((int16_t *)V(vs2))[ei] | ((int16_t *)V(vs1))[ei];
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = ((int32_t *)V(vs2))[ei] | ((int32_t *)V(vs1))[ei];
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = ((int64_t *)V(vs2))[ei] | ((int64_t *)V(vs1))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vxor_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((int8_t *)V(vd))[ei] = ((int8_t *)V(vs2))[ei] ^ ((int8_t *)V(vs1))[ei];
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = ((int16_t *)V(vs2))[ei] ^ ((int16_t *)V(vs1))[ei];
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = ((int32_t *)V(vs2))[ei] ^ ((int32_t *)V(vs1))[ei];
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = ((int64_t *)V(vs2))[ei] ^ ((int64_t *)V(vs1))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vand_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
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
            ((int8_t *)V(vd))[ei] = ((int8_t *)V(vs2))[ei] & rs1;
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = ((int16_t *)V(vs2))[ei] & rs1;
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = ((int32_t *)V(vs2))[ei] & rs1;
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = ((int64_t *)V(vs2))[ei] & rs1;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vor_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
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
            ((int8_t *)V(vd))[ei] = ((int8_t *)V(vs2))[ei] | rs1;
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = ((int16_t *)V(vs2))[ei] | rs1;
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = ((int32_t *)V(vs2))[ei] | rs1;
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = ((int64_t *)V(vs2))[ei] | rs1;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vxor_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
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
            ((int8_t *)V(vd))[ei] = ((int8_t *)V(vs2))[ei] ^ rs1;
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = ((int16_t *)V(vs2))[ei] ^ rs1;
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = ((int32_t *)V(vs2))[ei] ^ rs1;
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = ((int64_t *)V(vs2))[ei] ^ rs1;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vsll_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint16_t mask = eew - 1;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            ((uint8_t *)V(vd))[ei] = ((uint8_t *)V(vs2))[ei] << (((uint8_t *)V(vs1))[ei] & mask);
            break;
        case 16:
            ((uint16_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei] << (((uint16_t *)V(vs1))[ei] & mask);
            break;
        case 32:
            ((uint32_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] << (((uint32_t *)V(vs1))[ei] & mask);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = ((uint64_t *)V(vs2))[ei] << (((uint64_t *)V(vs1))[ei] & mask);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vsrl_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint16_t mask = eew - 1;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            ((uint8_t *)V(vd))[ei] = ((uint8_t *)V(vs2))[ei] >> (((uint8_t *)V(vs1))[ei] & mask);
            break;
        case 16:
            ((uint16_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei] >> (((uint16_t *)V(vs1))[ei] & mask);
            break;
        case 32:
            ((uint32_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] >> (((uint32_t *)V(vs1))[ei] & mask);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = ((uint64_t *)V(vs2))[ei] >> (((uint64_t *)V(vs1))[ei] & mask);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vsra_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint16_t mask = eew - 1;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            ((int8_t *)V(vd))[ei] = ((int8_t *)V(vs2))[ei] >> (((int8_t *)V(vs1))[ei] & mask);
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = ((int16_t *)V(vs2))[ei] >> (((int16_t *)V(vs1))[ei] & mask);
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = ((int32_t *)V(vs2))[ei] >> (((int32_t *)V(vs1))[ei] & mask);
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = ((int64_t *)V(vs2))[ei] >> (((int64_t *)V(vs1))[ei] & mask);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vsll_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint16_t shift = rs1 & (eew - 1);
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            ((uint8_t *)V(vd))[ei] = ((uint8_t *)V(vs2))[ei] << shift;
            break;
        case 16:
            ((uint16_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei] << shift;
            break;
        case 32:
            ((uint32_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] << shift;
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = ((uint64_t *)V(vs2))[ei] << shift;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vsrl_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint16_t shift = rs1 & (eew - 1);
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            ((uint8_t *)V(vd))[ei] = ((uint8_t *)V(vs2))[ei] >> shift;
            break;
        case 16:
            ((uint16_t *)V(vd))[ei] = ((uint16_t *)V(vs2))[ei] >> shift;
            break;
        case 32:
            ((uint32_t *)V(vd))[ei] = ((uint32_t *)V(vs2))[ei] >> shift;
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = ((uint64_t *)V(vs2))[ei] >> shift;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vsra_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint16_t shift = rs1 & (eew - 1);
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            ((int8_t *)V(vd))[ei] = ((int8_t *)V(vs2))[ei] >> shift;
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = ((int16_t *)V(vs2))[ei] >> shift;
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = ((int32_t *)V(vs2))[ei] >> shift;
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = ((int64_t *)V(vs2))[ei] >> shift;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmseq_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
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
        case 8:
            V(vd)[i >> 3] |= (((uint8_t *)V(vs2))[i] == ((uint8_t *)V(vs1))[i]) << (i & 0x7);
            break;
        case 16:
            V(vd)[i >> 3] |= (((uint16_t *)V(vs2))[i] == ((uint16_t *)V(vs1))[i]) << (i & 0x7);
            break;
        case 32:
            V(vd)[i >> 3] |= (((uint32_t *)V(vs2))[i] == ((uint32_t *)V(vs1))[i]) << (i & 0x7);
            break;
        case 64:
            V(vd)[i >> 3] |= (((uint64_t *)V(vs2))[i] == ((uint64_t *)V(vs1))[i]) << (i & 0x7);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmsne_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
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
        case 8:
            V(vd)[i >> 3] |= (((uint8_t *)V(vs2))[i] != ((uint8_t *)V(vs1))[i]) << (i & 0x7);
            break;
        case 16:
            V(vd)[i >> 3] |= (((uint16_t *)V(vs2))[i] != ((uint16_t *)V(vs1))[i]) << (i & 0x7);
            break;
        case 32:
            V(vd)[i >> 3] |= (((uint32_t *)V(vs2))[i] != ((uint32_t *)V(vs1))[i]) << (i & 0x7);
            break;
        case 64:
            V(vd)[i >> 3] |= (((uint64_t *)V(vs2))[i] != ((uint64_t *)V(vs1))[i]) << (i & 0x7);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmsltu_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
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

void glue(helper_vmslt_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
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
        case 8:
            V(vd)[i >> 3] |= (((int8_t *)V(vs2))[i] < ((int8_t *)V(vs1))[i]) << (i & 0x7);
            break;
        case 16:
            V(vd)[i >> 3] |= (((int16_t *)V(vs2))[i] < ((int16_t *)V(vs1))[i]) << (i & 0x7);
            break;
        case 32:
            V(vd)[i >> 3] |= (((int32_t *)V(vs2))[i] < ((int32_t *)V(vs1))[i]) << (i & 0x7);
            break;
        case 64:
            V(vd)[i >> 3] |= (((int64_t *)V(vs2))[i] < ((int64_t *)V(vs1))[i]) << (i & 0x7);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmsleu_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
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
        case 8:
            V(vd)[i >> 3] |= (((uint8_t *)V(vs2))[i] <= ((uint8_t *)V(vs1))[i]) << (i & 0x7);
            break;
        case 16:
            V(vd)[i >> 3] |= (((uint16_t *)V(vs2))[i] <= ((uint16_t *)V(vs1))[i]) << (i & 0x7);
            break;
        case 32:
            V(vd)[i >> 3] |= (((uint32_t *)V(vs2))[i] <= ((uint32_t *)V(vs1))[i]) << (i & 0x7);
            break;
        case 64:
            V(vd)[i >> 3] |= (((uint64_t *)V(vs2))[i] <= ((uint64_t *)V(vs1))[i]) << (i & 0x7);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmsle_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
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
        case 8:
            V(vd)[i >> 3] |= (((int8_t *)V(vs2))[i] <= ((int8_t *)V(vs1))[i]) << (i & 0x7);
            break;
        case 16:
            V(vd)[i >> 3] |= (((int16_t *)V(vs2))[i] <= ((int16_t *)V(vs1))[i]) << (i & 0x7);
            break;
        case 32:
            V(vd)[i >> 3] |= (((int32_t *)V(vs2))[i] <= ((int32_t *)V(vs1))[i]) << (i & 0x7);
            break;
        case 64:
            V(vd)[i >> 3] |= (((int64_t *)V(vs2))[i] <= ((int64_t *)V(vs1))[i]) << (i & 0x7);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmseq_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
{
    if (V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
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
        case 8:
            V(vd)[i >> 3] |= (((uint8_t *)V(vs2))[i] == (uint8_t)rs1) << (i & 0x7);
            break;
        case 16:
            V(vd)[i >> 3] |= (((uint16_t *)V(vs2))[i] == (uint16_t)rs1) << (i & 0x7);
            break;
        case 32:
            V(vd)[i >> 3] |= (((uint32_t *)V(vs2))[i] == (uint32_t)rs1) << (i & 0x7);
            break;
        case 64:
            V(vd)[i >> 3] |= (((uint64_t *)V(vs2))[i] == (uint64_t)rs1) << (i & 0x7);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmsne_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
{
    if (V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
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
        case 8:
            V(vd)[i >> 3] |= (((uint8_t *)V(vs2))[i] != (uint8_t)rs1) << (i & 0x7);
            break;
        case 16:
            V(vd)[i >> 3] |= (((uint16_t *)V(vs2))[i] != (uint16_t)rs1) << (i & 0x7);
            break;
        case 32:
            V(vd)[i >> 3] |= (((uint32_t *)V(vs2))[i] != (uint32_t)rs1) << (i & 0x7);
            break;
        case 64:
            V(vd)[i >> 3] |= (((uint64_t *)V(vs2))[i] != (uint64_t)rs1) << (i & 0x7);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmsltu_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
{
    if (V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
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
        case 8:
            V(vd)[i >> 3] |= (((uint8_t *)V(vs2))[i] < (uint8_t)rs1) << (i & 0x7);
            break;
        case 16:
            V(vd)[i >> 3] |= (((uint16_t *)V(vs2))[i] < (uint16_t)rs1) << (i & 0x7);
            break;
        case 32:
            V(vd)[i >> 3] |= (((uint32_t *)V(vs2))[i] < (uint32_t)rs1) << (i & 0x7);
            break;
        case 64:
            V(vd)[i >> 3] |= (((uint64_t *)V(vs2))[i] < (uint64_t)rs1) << (i & 0x7);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmslt_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
{
    if (V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
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
        case 8:
            V(vd)[i >> 3] |= (((int8_t *)V(vs2))[i] < (int8_t)rs1) << (i & 0x7);
            break;
        case 16:
            V(vd)[i >> 3] |= (((int16_t *)V(vs2))[i] < (int16_t)rs1) << (i & 0x7);
            break;
        case 32:
            V(vd)[i >> 3] |= (((int32_t *)V(vs2))[i] < (int32_t)rs1) << (i & 0x7);
            break;
        case 64:
            V(vd)[i >> 3] |= (((int64_t *)V(vs2))[i] < (int64_t)rs1) << (i & 0x7);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmsleu_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
{
    if (V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
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
        case 8:
            V(vd)[i >> 3] |= (((uint8_t *)V(vs2))[i] <= (uint8_t)rs1) << (i & 0x7);
            break;
        case 16:
            V(vd)[i >> 3] |= (((uint16_t *)V(vs2))[i] <= (uint16_t)rs1) << (i & 0x7);
            break;
        case 32:
            V(vd)[i >> 3] |= (((uint32_t *)V(vs2))[i] <= (uint32_t)rs1) << (i & 0x7);
            break;
        case 64:
            V(vd)[i >> 3] |= (((uint64_t *)V(vs2))[i] <= (uint64_t)rs1) << (i & 0x7);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmsle_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
{
    if (V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
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
        case 8:
            V(vd)[i >> 3] |= (((int8_t *)V(vs2))[i] <= (int8_t)rs1) << (i & 0x7);
            break;
        case 16:
            V(vd)[i >> 3] |= (((int16_t *)V(vs2))[i] <= (int16_t)rs1) << (i & 0x7);
            break;
        case 32:
            V(vd)[i >> 3] |= (((int32_t *)V(vs2))[i] <= (int32_t)rs1) << (i & 0x7);
            break;
        case 64:
            V(vd)[i >> 3] |= (((int64_t *)V(vs2))[i] <= (int64_t)rs1) << (i & 0x7);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmsgtu_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
{
    if (V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
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
        case 8:
            V(vd)[i >> 3] |= (((uint8_t *)V(vs2))[i] > (uint8_t)rs1) << (i & 0x7);
            break;
        case 16:
            V(vd)[i >> 3] |= (((uint16_t *)V(vs2))[i] > (uint16_t)rs1) << (i & 0x7);
            break;
        case 32:
            V(vd)[i >> 3] |= (((uint32_t *)V(vs2))[i] > (uint32_t)rs1) << (i & 0x7);
            break;
        case 64:
            V(vd)[i >> 3] |= (((uint64_t *)V(vs2))[i] > (uint64_t)rs1) << (i & 0x7);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmsgt_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
{
    if (V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
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
        case 8:
            V(vd)[i >> 3] |= (((int8_t *)V(vs2))[i] > (int8_t)rs1) << (i & 0x7);
            break;
        case 16:
            V(vd)[i >> 3] |= (((int16_t *)V(vs2))[i] > (int16_t)rs1) << (i & 0x7);
            break;
        case 32:
            V(vd)[i >> 3] |= (((int32_t *)V(vs2))[i] > (int32_t)rs1) << (i & 0x7);
            break;
        case 64:
            V(vd)[i >> 3] |= (((int64_t *)V(vs2))[i] > (int64_t)rs1) << (i & 0x7);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vdivu_mvv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((uint8_t *)V(vd))[ei] = divu_8(((uint8_t *)V(vs2))[ei], ((uint8_t *)V(vs1))[ei]);
            break;
        case 16:
            ((uint16_t *)V(vd))[ei] = divu_16(((uint16_t *)V(vs2))[ei], ((uint16_t *)V(vs1))[ei]);
            break;
        case 32:
            ((uint32_t *)V(vd))[ei] = divu_32(((uint32_t *)V(vs2))[ei], ((uint32_t *)V(vs1))[ei]);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = divu_64(((uint64_t *)V(vs2))[ei], ((uint64_t *)V(vs1))[ei]);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vdiv_mvv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((int8_t *)V(vd))[ei] = div_8(((int8_t *)V(vs2))[ei], ((int8_t *)V(vs1))[ei]);
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = div_16(((int16_t *)V(vs2))[ei], ((int16_t *)V(vs1))[ei]);
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = div_32(((int32_t *)V(vs2))[ei], ((int32_t *)V(vs1))[ei]);
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = div_64(((int64_t *)V(vs2))[ei], ((int64_t *)V(vs1))[ei]);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vremu_mvv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((uint8_t *)V(vd))[ei] = remu_8(((uint8_t *)V(vs2))[ei], ((uint8_t *)V(vs1))[ei]);
            break;
        case 16:
            ((uint16_t *)V(vd))[ei] = remu_16(((uint16_t *)V(vs2))[ei], ((uint16_t *)V(vs1))[ei]);
            break;
        case 32:
            ((uint32_t *)V(vd))[ei] = remu_32(((uint32_t *)V(vs2))[ei], ((uint32_t *)V(vs1))[ei]);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = remu_64(((uint64_t *)V(vs2))[ei], ((uint64_t *)V(vs1))[ei]);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vrem_mvv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((int8_t *)V(vd))[ei] = rem_8(((int8_t *)V(vs2))[ei], ((int8_t *)V(vs1))[ei]);
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = rem_16(((int16_t *)V(vs2))[ei], ((int16_t *)V(vs1))[ei]);
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = rem_32(((int32_t *)V(vs2))[ei], ((int32_t *)V(vs1))[ei]);
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = rem_64(((int64_t *)V(vs2))[ei], ((int64_t *)V(vs1))[ei]);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vdivu_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
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
            ((uint8_t *)V(vd))[ei] = divu_8(((uint8_t *)V(vs2))[ei], rs1);
            break;
        case 16:
            ((uint16_t *)V(vd))[ei] = divu_16(((uint16_t *)V(vs2))[ei], rs1);
            break;
        case 32:
            ((uint32_t *)V(vd))[ei] = divu_32(((uint32_t *)V(vs2))[ei], rs1);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = divu_64(((uint64_t *)V(vs2))[ei], rs1);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vdiv_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
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
            ((int8_t *)V(vd))[ei] = div_8(((int8_t *)V(vs2))[ei], rs1);
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = div_16(((int16_t *)V(vs2))[ei], rs1);
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = div_32(((int32_t *)V(vs2))[ei], rs1);
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = div_64(((int64_t *)V(vs2))[ei], rs1);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vremu_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
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
            ((uint8_t *)V(vd))[ei] = remu_8(((uint8_t *)V(vs2))[ei], rs1);
            break;
        case 16:
            ((uint16_t *)V(vd))[ei] = remu_16(((uint16_t *)V(vs2))[ei], rs1);
            break;
        case 32:
            ((uint32_t *)V(vd))[ei] = remu_32(((uint32_t *)V(vs2))[ei], rs1);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = remu_64(((uint64_t *)V(vs2))[ei], rs1);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vrem_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
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
            ((int8_t *)V(vd))[ei] = rem_8(((int8_t *)V(vs2))[ei], rs1);
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = rem_16(((int16_t *)V(vs2))[ei], rs1);
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = rem_32(((int32_t *)V(vs2))[ei], rs1);
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = rem_64(((int64_t *)V(vs2))[ei], rs1);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmacc_mvv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((int8_t *)V(vd))[ei] += ((int8_t *)V(vs2))[ei] * ((int8_t *)V(vs1))[ei];
            break;
        case 16:
            ((int16_t *)V(vd))[ei] += ((int16_t *)V(vs2))[ei] * ((int16_t *)V(vs1))[ei];
            break;
        case 32:
            ((int32_t *)V(vd))[ei] += ((int32_t *)V(vs2))[ei] * ((int32_t *)V(vs1))[ei];
            break;
        case 64:
            ((int64_t *)V(vd))[ei] += ((int64_t *)V(vs2))[ei] * ((int64_t *)V(vs1))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmacc_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
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
            ((int8_t *)V(vd))[ei] += ((int8_t *)V(vs2))[ei] * (int8_t)rs1;
            break;
        case 16:
            ((int16_t *)V(vd))[ei] += ((int16_t *)V(vs2))[ei] * (int16_t)rs1;
            break;
        case 32:
            ((int32_t *)V(vd))[ei] += ((int32_t *)V(vs2))[ei] * (int32_t)rs1;
            break;
        case 64:
            ((int64_t *)V(vd))[ei] += ((int64_t *)V(vs2))[ei] * (int64_t)rs1;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vnmsac_mvv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((int8_t *)V(vd))[ei] -= ((int8_t *)V(vs2))[ei] * ((int8_t *)V(vs1))[ei];
            break;
        case 16:
            ((int16_t *)V(vd))[ei] -= ((int16_t *)V(vs2))[ei] * ((int16_t *)V(vs1))[ei];
            break;
        case 32:
            ((int32_t *)V(vd))[ei] -= ((int32_t *)V(vs2))[ei] * ((int32_t *)V(vs1))[ei];
            break;
        case 64:
            ((int64_t *)V(vd))[ei] -= ((int64_t *)V(vs2))[ei] * ((int64_t *)V(vs1))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vnmsac_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
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
            ((int8_t *)V(vd))[ei] -= ((int8_t *)V(vs2))[ei] * (int8_t)rs1;
            break;
        case 16:
            ((int16_t *)V(vd))[ei] -= ((int16_t *)V(vs2))[ei] * (int16_t)rs1;
            break;
        case 32:
            ((int32_t *)V(vd))[ei] -= ((int32_t *)V(vs2))[ei] * (int32_t)rs1;
            break;
        case 64:
            ((int64_t *)V(vd))[ei] -= ((int64_t *)V(vs2))[ei] * (int64_t)rs1;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmadd_mvv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((int8_t *)V(vd))[ei] = (((int8_t *)V(vd))[ei] * ((int8_t *)V(vs1))[ei]) + ((int8_t *)V(vs2))[ei];
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = (((int16_t *)V(vd))[ei] * ((int16_t *)V(vs1))[ei]) + ((int16_t *)V(vs2))[ei];
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = (((int32_t *)V(vd))[ei] * ((int32_t *)V(vs1))[ei]) + ((int32_t *)V(vs2))[ei];
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = (((int64_t *)V(vd))[ei] * ((int64_t *)V(vs1))[ei]) + ((int64_t *)V(vs2))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vmadd_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
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
            ((int8_t *)V(vd))[ei] = (((int8_t *)V(vd))[ei] * (int8_t)rs1) + ((int8_t *)V(vs2))[ei];
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = (((int16_t *)V(vd))[ei] * (int16_t)rs1) + ((int16_t *)V(vs2))[ei];
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = (((int32_t *)V(vd))[ei] * (int32_t)rs1) + ((int32_t *)V(vs2))[ei];
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = (((int64_t *)V(vd))[ei] * (int64_t)rs1) + ((int64_t *)V(vs2))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vnmsub_mvv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((int8_t *)V(vd))[ei] = -(((int8_t *)V(vd))[ei] * ((int8_t *)V(vs1))[ei]) + ((int8_t *)V(vs2))[ei];
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = -(((int16_t *)V(vd))[ei] * ((int16_t *)V(vs1))[ei]) + ((int16_t *)V(vs2))[ei];
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = -(((int32_t *)V(vd))[ei] * ((int32_t *)V(vs1))[ei]) + ((int32_t *)V(vs2))[ei];
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = -(((int64_t *)V(vd))[ei] * ((int64_t *)V(vs1))[ei]) + ((int64_t *)V(vs2))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vnmsub_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
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
            ((int8_t *)V(vd))[ei] = -(((int8_t *)V(vd))[ei] * (int8_t)rs1) + ((int8_t *)V(vs2))[ei];
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = -(((int16_t *)V(vd))[ei] * (int16_t)rs1) + ((int16_t *)V(vs2))[ei];
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = -(((int32_t *)V(vd))[ei] * (int32_t)rs1) + ((int32_t *)V(vs2))[ei];
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = -(((int64_t *)V(vd))[ei] * (int64_t)rs1) + ((int64_t *)V(vs2))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwmaccu_mvv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((uint16_t *)V(vd))[ei] += (uint16_t)((uint8_t *)V(vs2))[ei] * (uint16_t)((uint8_t *)V(vs1))[ei];
            break;
        case 16:
            ((uint32_t *)V(vd))[ei] += (uint32_t)((uint16_t *)V(vs2))[ei] * (uint32_t)((uint16_t *)V(vs1))[ei];
            break;
        case 32:
            ((uint64_t *)V(vd))[ei] += (uint64_t)((uint32_t *)V(vs2))[ei] * (uint64_t)((uint32_t *)V(vs1))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwmaccu_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
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
            ((uint16_t *)V(vd))[ei] += (uint16_t)((uint8_t *)V(vs2))[ei] * (uint16_t)((uint8_t)rs1);
            break;
        case 16:
            ((uint32_t *)V(vd))[ei] += (uint32_t)((uint16_t *)V(vs2))[ei] * (uint32_t)((uint16_t)rs1);
            break;
        case 32:
            ((uint64_t *)V(vd))[ei] += (uint64_t)((uint32_t *)V(vs2))[ei] * (uint64_t)((uint32_t)rs1);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwmacc_mvv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((int16_t *)V(vd))[ei] += (int16_t)((int8_t *)V(vs2))[ei] * (int16_t)((int8_t *)V(vs1))[ei];
            break;
        case 16:
            ((int32_t *)V(vd))[ei] += (int32_t)((int16_t *)V(vs2))[ei] * (int32_t)((int16_t *)V(vs1))[ei];
            break;
        case 32:
            ((int64_t *)V(vd))[ei] += (int64_t)((int32_t *)V(vs2))[ei] * (int64_t)((int32_t *)V(vs1))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwmacc_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
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
            ((int16_t *)V(vd))[ei] += (int16_t)((int8_t *)V(vs2))[ei] * (int16_t)((int8_t)rs1);
            break;
        case 16:
            ((int32_t *)V(vd))[ei] += (int32_t)((int16_t *)V(vs2))[ei] * (int32_t)((int16_t)rs1);
            break;
        case 32:
            ((int64_t *)V(vd))[ei] += (int64_t)((int32_t *)V(vs2))[ei] * (int64_t)((int32_t)rs1);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwmaccsu_mvv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
            ((int16_t *)V(vd))[ei] += (uint16_t)((uint8_t *)V(vs2))[ei] * (int16_t)((int8_t *)V(vs1))[ei];
            break;
        case 16:
            ((int32_t *)V(vd))[ei] += (uint32_t)((uint16_t *)V(vs2))[ei] * (int32_t)((int16_t *)V(vs1))[ei];
            break;
        case 32:
            ((int64_t *)V(vd))[ei] += (uint64_t)((uint32_t *)V(vs2))[ei] * (int64_t)((int32_t *)V(vs1))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwmaccsu_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
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
            ((int16_t *)V(vd))[ei] += (uint16_t)((uint8_t *)V(vs2))[ei] * (uint16_t)((uint8_t)rs1);
            break;
        case 16:
            ((int32_t *)V(vd))[ei] += (uint32_t)((uint16_t *)V(vs2))[ei] * (uint32_t)((uint16_t)rs1);
            break;
        case 32:
            ((int64_t *)V(vd))[ei] += (uint64_t)((uint32_t *)V(vs2))[ei] * (uint64_t)((uint32_t)rs1);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vwmaccus_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
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
            ((int16_t *)V(vd))[ei] += (int16_t)((int8_t *)V(vs2))[ei] * (int16_t)((int8_t)rs1);
            break;
        case 16:
            ((int32_t *)V(vd))[ei] += (int32_t)((int16_t *)V(vs2))[ei] * (int32_t)((int16_t)rs1);
            break;
        case 32:
            ((int64_t *)V(vd))[ei] += (int64_t)((int32_t *)V(vs2))[ei] * (int64_t)((int32_t)rs1);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vsaddu_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
                uint8_t a = ((uint8_t *)V(vs2))[ei];
                uint8_t b = ((uint8_t *)V(vs1))[ei];
                uint8_t ab = a + b;
                uint8_t sat = ab < a;
                env->vxsat |= sat;
                ((uint8_t *)V(vd))[ei] = sat ? ~0 : ab;
                break;
            }
        case 16:  {
                uint16_t a = ((uint16_t *)V(vs2))[ei];
                uint16_t b = ((uint16_t *)V(vs1))[ei];
                uint16_t ab = a + b;
                uint8_t sat = ab < a;
                env->vxsat |= sat;
                ((uint16_t *)V(vd))[ei] = sat ? ~0 : ab;
                break;
            }
        case 32:  {
                uint32_t a = ((uint32_t *)V(vs2))[ei];
                uint32_t b = ((uint32_t *)V(vs1))[ei];
                uint32_t ab = a + b;
                uint8_t sat = ab < a;
                env->vxsat |= sat;
                ((uint32_t *)V(vd))[ei] = sat ? ~0 : ab;
                break;
            }
        case 64:  {
                uint64_t a = ((uint64_t *)V(vs2))[ei];
                uint64_t b = ((uint64_t *)V(vs1))[ei];
                uint64_t ab = a + b;
                uint8_t sat = ab < a;
                env->vxsat |= sat;
                ((uint64_t *)V(vd))[ei] = sat ? ~0 : ab;
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vsaddu_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
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
                uint8_t ab = ((uint8_t *)V(vs2))[ei] + (uint8_t)rs1;
                uint8_t sat = ab < (uint8_t)rs1;
                env->vxsat |= sat;
                ((uint8_t *)V(vd))[ei] = sat ? ~0 : ab;
                break;
            }
        case 16: {
                uint16_t ab = ((uint16_t *)V(vs2))[ei] + (uint16_t)rs1;
                uint8_t sat = ab < (uint16_t)rs1;
                env->vxsat |= sat;
                ((uint16_t *)V(vd))[ei] = sat ? ~0 : ab;
                break;
            }
        case 32: {
                uint32_t ab = ((uint32_t *)V(vs2))[ei] + (uint32_t)rs1;
                uint8_t sat = ab < (uint32_t)rs1;
                env->vxsat |= sat;
                ((uint32_t *)V(vd))[ei] = sat ? ~0 : ab;
                break;
            }
        case 64: {
                uint64_t ab = ((uint64_t *)V(vs2))[ei] + (uint64_t)rs1;
                uint8_t sat = ab < (uint64_t)rs1;
                env->vxsat |= sat;
                ((uint64_t *)V(vd))[ei] = sat ? ~0 : ab;
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vsadd_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
                int8_t a = ((int8_t *)V(vs2))[ei];
                int8_t b = ((int8_t *)V(vs1))[ei];
                int8_t sat_value = INT8_MAX + (a < 0);
                uint8_t sat = (a < 0) != (b > sat_value - a);
                env->vxsat |= sat;
                ((int8_t *)V(vd))[ei] = sat ? sat_value : a + b;
                break;
            }
        case 16: {
                int16_t a = ((int16_t *)V(vs2))[ei];
                int16_t b = ((int16_t *)V(vs1))[ei];
                int16_t sat_value = INT16_MAX + (a < 0);
                uint8_t sat = (a < 0) != (b > sat_value - a);
                env->vxsat |= sat;
                ((int16_t *)V(vd))[ei] = sat ? sat_value : a + b;
                break;
            }
        case 32: {
                int32_t a = ((int32_t *)V(vs2))[ei];
                int32_t b = ((int32_t *)V(vs1))[ei];
                int32_t sat_value = INT32_MAX + (a < 0);
                uint8_t sat = (a < 0) != (b > sat_value - a);
                env->vxsat |= sat;
                ((int32_t *)V(vd))[ei] = sat ? sat_value : a + b;
                break;
            }
        case 64: {
                int64_t a = ((int64_t *)V(vs2))[ei];
                int64_t b = ((int64_t *)V(vs1))[ei];
                int64_t sat_value = INT64_MAX + (a < 0);
                uint8_t sat = (a < 0) != (b > sat_value - a);
                env->vxsat |= sat;
                ((int64_t *)V(vd))[ei] = sat ? sat_value : a + b;
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vsadd_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
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
                int8_t a = ((int8_t *)V(vs2))[ei];
                int8_t sat_value = INT8_MAX + (a < 0);
                uint8_t sat = (a < 0) != ((int8_t)rs1 > sat_value - a);
                env->vxsat |= sat;
                ((int8_t *)V(vd))[ei] = sat ? sat_value : a + (int8_t)rs1;
                break;
            }
        case 16: {
                int16_t a = ((int16_t *)V(vs2))[ei];
                int16_t sat_value = INT16_MAX + (a < 0);
                uint8_t sat = (a < 0) != ((int16_t)rs1 > sat_value - a);
                env->vxsat |= sat;
                ((int16_t *)V(vd))[ei] = sat ? sat_value : a + (int16_t)rs1;
                break;
            }
        case 32: {
                int32_t a = ((int32_t *)V(vs2))[ei];
                int32_t sat_value = INT32_MAX + (a < 0);
                uint8_t sat = (a < 0) != ((int32_t)rs1 > sat_value - a);
                env->vxsat |= sat;
                ((int32_t *)V(vd))[ei] = sat ? sat_value : a + (int32_t)rs1;
                break;
            }
        case 64: {
                int64_t a = ((int64_t *)V(vs2))[ei];
                int64_t sat_value = INT64_MAX + (a < 0);
                uint8_t sat = (a < 0) != ((int64_t)rs1 > sat_value - a);
                env->vxsat |= sat;
                ((int64_t *)V(vd))[ei] = sat ? sat_value : a + (int64_t)rs1;
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vssubu_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
                uint8_t a = ((uint8_t *)V(vs2))[ei];
                uint8_t b = ((uint8_t *)V(vs1))[ei];
                uint8_t sat = a < b;
                env->vxsat |= sat;
                ((uint8_t *)V(vd))[ei] = sat ? 0 : a - b;
                break;
            }
        case 16: {
                uint16_t a = ((uint16_t *)V(vs2))[ei];
                uint16_t b = ((uint16_t *)V(vs1))[ei];
                uint8_t sat = a < b;
                env->vxsat |= sat;
                ((uint16_t *)V(vd))[ei] = sat ? 0 : a - b;
                break;
            }
        case 32: {
                uint32_t a = ((uint32_t *)V(vs2))[ei];
                uint32_t b = ((uint32_t *)V(vs1))[ei];
                uint8_t sat = a < b;
                env->vxsat |= sat;
                ((uint32_t *)V(vd))[ei] = sat ? 0 : a - b;
                break;
            }
        case 64: {
                uint64_t a = ((uint64_t *)V(vs2))[ei];
                uint64_t b = ((uint64_t *)V(vs1))[ei];
                uint8_t sat = a < b;
                env->vxsat |= sat;
                ((uint64_t *)V(vd))[ei] = sat ? 0 : a - b;
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vssubu_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
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
                uint8_t a = ((uint8_t *)V(vs2))[ei];
                uint8_t sat = a < (uint8_t)rs1;
                env->vxsat |= sat;
                ((uint8_t *)V(vd))[ei] = sat ? 0 : a - (uint8_t)rs1;
                break;
            }
        case 16: {
                uint16_t a = ((uint16_t *)V(vs2))[ei];
                uint8_t sat = a < (uint16_t)rs1;
                env->vxsat |= sat;
                ((uint16_t *)V(vd))[ei] = sat ? 0 : a - (uint16_t)rs1;
                break;
            }
        case 32: {
                uint32_t a = ((uint32_t *)V(vs2))[ei];
                uint8_t sat = a < (uint32_t)rs1;
                env->vxsat |= sat;
                ((uint32_t *)V(vd))[ei] = sat ? 0 : a - (uint32_t)rs1;
                break;
            }
        case 64: {
                uint64_t a = ((uint64_t *)V(vs2))[ei];
                uint8_t sat = a < (uint64_t)rs1;
                env->vxsat |= sat;
                ((uint64_t *)V(vd))[ei] = sat ? 0 : a - (uint64_t)rs1;
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vssub_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
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
                int8_t a = ((int8_t *)V(vs2))[ei];
                int8_t b = ((int8_t *)V(vs1))[ei];
                int8_t sat_value = INT8_MAX + (a < 0);
                uint8_t sat = b < 0 ? a < 0 && b - 1 > sat_value + a : a >= 0 && b <= sat_value + a;
                env->vxsat |= sat;
                ((int8_t *)V(vd))[ei] = sat ? sat_value : a - b;
                break;
            }
        case 16: {
                int16_t a = ((int16_t *)V(vs2))[ei];
                int16_t b = ((int8_t *)V(vs1))[ei];
                int16_t sat_value = INT16_MAX + (a < 0);
                uint8_t sat = b < 0 ? a < 0 && b - 1 > sat_value + a : a >= 0 && b <= sat_value + a;
                env->vxsat |= sat;
                ((int16_t *)V(vd))[ei] = sat ? sat_value : a - b;
                break;
            }
        case 32: {
                int32_t a = ((int32_t *)V(vs2))[ei];
                int32_t b = ((int32_t *)V(vs1))[ei];
                int32_t sat_value = INT32_MAX + (a < 0);
                uint8_t sat = b < 0 ? a < 0 && b - 1 > sat_value + a : a >= 0 && b <= sat_value + a;
                env->vxsat |= sat;
                ((int32_t *)V(vd))[ei] = sat ? sat_value : a - b;
                break;
            }
        case 64: {
                int64_t a = ((int64_t *)V(vs2))[ei];
                int64_t b = ((int64_t *)V(vs1))[ei];
                int64_t sat_value = INT64_MAX + (a < 0);
                uint8_t sat = b < 0 ? a < 0 && b - 1 > sat_value + a : a >= 0 && b <= sat_value + a;
                env->vxsat |= sat;
                ((int64_t *)V(vd))[ei] = sat ? sat_value : a - b;
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vssub_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
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
                int8_t a = ((int8_t *)V(vs2))[ei];
                int8_t sat_value = INT8_MAX + (a < 0);
                uint8_t sat = (int8_t)rs1 < 0 ? a < 0 && (int8_t)rs1 - 1 > sat_value + a : a >= 0 && (int8_t)rs1 <= sat_value + a;
                env->vxsat |= sat;
                ((int8_t *)V(vd))[ei] = sat ? sat_value : a - (int8_t)rs1;
                break;
            }
        case 16: {
                int16_t a = ((int16_t *)V(vs2))[ei];
                int16_t sat_value = INT16_MAX + (a < 0);
                uint8_t sat = (int16_t)rs1 < 0 ? a < 0 && (int16_t)rs1 - 1 > sat_value + a : a >= 0 && (int16_t)rs1 <= sat_value + a;
                env->vxsat |= sat;
                ((int16_t *)V(vd))[ei] = sat ? sat_value : a - (int16_t)rs1;
                break;
            }
        case 32: {
                int32_t a = ((int32_t *)V(vs2))[ei];
                int32_t sat_value = INT32_MAX + (a < 0);
                uint8_t sat = (int32_t)rs1 < 0 ? a < 0 && (int32_t)rs1 - 1 > sat_value + a : a >= 0 && (int32_t)rs1 <= sat_value + a;
                env->vxsat |= sat;
                ((int32_t *)V(vd))[ei] = sat ? sat_value : a - (int32_t)rs1;
                break;
            }
        case 64: {
                int64_t a = ((int64_t *)V(vs2))[ei];
                int64_t sat_value = INT64_MAX + (a < 0);
                uint8_t sat = (int64_t)rs1 < 0 ? a < 0 && (int64_t)rs1 - 1 > sat_value + a : a >= 0 && (int64_t)rs1 <= sat_value + a;
                env->vxsat |= sat;
                ((int64_t *)V(vd))[ei] = sat ? sat_value : a - (int64_t)rs1;
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vaadd_mvv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint8_t rm = env->vxrm & 0b11;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8: {
                int16_t a = ((int8_t *)V(vs2))[ei];
                int16_t b = ((int8_t *)V(vs1))[ei];
                ((int8_t *)V(vd))[ei] = roundoff_i16(a + b, 1, rm);
                break;
            }
        case 16: {
                int32_t a = ((int16_t *)V(vs2))[ei];
                int32_t b = ((int16_t *)V(vs1))[ei];
                ((int16_t *)V(vd))[ei] = roundoff_i32(a + b, 1, rm);
                break;
            }
        case 32: {
                int64_t a = ((int32_t *)V(vs2))[ei];
                int64_t b = ((int32_t *)V(vs1))[ei];
                ((int32_t *)V(vd))[ei] = roundoff_i64(a + b, 1, rm);
                break;
            }
        case 64: {
                __int128_t a = ((int64_t *)V(vs2))[ei];
                __int128_t b = ((int64_t *)V(vs1))[ei];
                ((int64_t *)V(vd))[ei] = roundoff_i128(a + b, 1, rm);
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vaadd_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint8_t rm = env->vxrm & 0b11;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8: {
                int16_t a = ((int8_t *)V(vs2))[ei];
                ((int8_t *)V(vd))[ei] = roundoff_i16(a + (int16_t)rs1, 1, rm);
                break;
            }
        case 16: {
                int32_t a = ((int16_t *)V(vs2))[ei];
                ((int16_t *)V(vd))[ei] = roundoff_i32(a + (int32_t)rs1, 1, rm);
                break;
            }
        case 32: {
                int64_t a = ((int32_t *)V(vs2))[ei];
                ((int32_t *)V(vd))[ei] = roundoff_i64(a + (int64_t)rs1, 1, rm);
                break;
            }
        case 64: {
                __int128_t a = ((int64_t *)V(vs2))[ei];
                ((int64_t *)V(vd))[ei] = roundoff_i128(a + (__int128_t)rs1, 1, rm);
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vaaddu_mvv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint8_t rm = env->vxrm & 0b11;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8: {
                uint16_t a = ((uint8_t *)V(vs2))[ei];
                uint16_t b = ((uint8_t *)V(vs1))[ei];
                ((uint8_t *)V(vd))[ei] = roundoff_u16(a + b, 1, rm);
                break;
            }
        case 16: {
                uint32_t a = ((uint16_t *)V(vs2))[ei];
                uint32_t b = ((uint16_t *)V(vs1))[ei];
                ((uint16_t *)V(vd))[ei] = roundoff_u32(a + b, 1, rm);
                break;
            }
        case 32: {
                uint64_t a = ((uint32_t *)V(vs2))[ei];
                uint64_t b = ((uint32_t *)V(vs1))[ei];
                ((uint32_t *)V(vd))[ei] = roundoff_u64(a + b, 1, rm);
                break;
            }
        case 64: {
                __uint128_t a = ((uint64_t *)V(vs2))[ei];
                __uint128_t b = ((uint64_t *)V(vs1))[ei];
                ((uint64_t *)V(vd))[ei] = roundoff_u128(a + b, 1, rm);
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vaaddu_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint8_t rm = env->vxrm & 0b11;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8: {
                uint16_t a = ((uint8_t *)V(vs2))[ei];
                ((uint8_t *)V(vd))[ei] = roundoff_u16(a + (uint16_t)rs1, 1, rm);
                break;
            }
        case 16: {
                uint32_t a = ((uint16_t *)V(vs2))[ei];
                ((uint16_t *)V(vd))[ei] = roundoff_u32(a + (uint32_t)rs1, 1, rm);
                break;
            }
        case 32: {
                uint64_t a = ((uint32_t *)V(vs2))[ei];
                ((uint32_t *)V(vd))[ei] = roundoff_u64(a + (uint64_t)rs1, 1, rm);
                break;
            }
        case 64: {
                __uint128_t a = ((uint64_t *)V(vs2))[ei];
                ((uint64_t *)V(vd))[ei] = roundoff_u128(a + (__uint128_t)rs1, 1, rm);
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vasub_mvv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint8_t rm = env->vxrm & 0b11;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8: {
                int16_t a = ((int8_t *)V(vs2))[ei];
                int16_t b = ((int8_t *)V(vs1))[ei];
                ((int8_t *)V(vd))[ei] = roundoff_i16(a - b, 1, rm);
                break;
            }
        case 16: {
                int32_t a = ((int16_t *)V(vs2))[ei];
                int32_t b = ((int16_t *)V(vs1))[ei];
                ((int16_t *)V(vd))[ei] = roundoff_i32(a - b, 1, rm);
                break;
            }
        case 32: {
                int64_t a = ((int32_t *)V(vs2))[ei];
                int64_t b = ((int32_t *)V(vs1))[ei];
                ((int32_t *)V(vd))[ei] = roundoff_i64(a - b, 1, rm);
                break;
            }
        case 64: {
                __int128_t a = ((int64_t *)V(vs2))[ei];
                __int128_t b = ((int64_t *)V(vs1))[ei];
                ((int64_t *)V(vd))[ei] = roundoff_i128(a - b, 1, rm);
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vasub_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint8_t rm = env->vxrm & 0b11;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8: {
                int16_t a = ((int8_t *)V(vs2))[ei];
                ((int8_t *)V(vd))[ei] = roundoff_i16(a - (int16_t)rs1, 1, rm);
                break;
            }
        case 16: {
                int32_t a = ((int16_t *)V(vs2))[ei];
                ((int16_t *)V(vd))[ei] = roundoff_i32(a - (int32_t)rs1, 1, rm);
                break;
            }
        case 32: {
                int64_t a = ((int32_t *)V(vs2))[ei];
                ((int32_t *)V(vd))[ei] = roundoff_i64(a - (int64_t)rs1, 1, rm);
                break;
            }
        case 64: {
                __int128_t a = ((int64_t *)V(vs2))[ei];
                ((int64_t *)V(vd))[ei] = roundoff_i128(a - (__int128_t)rs1, 1, rm);
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vasubu_mvv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint8_t rm = env->vxrm & 0b11;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8: {
                uint16_t a = ((uint8_t *)V(vs2))[ei];
                uint16_t b = ((uint8_t *)V(vs1))[ei];
                ((uint8_t *)V(vd))[ei] = roundoff_u16(a - b, 1, rm);
                break;
            }
        case 16: {
                uint32_t a = ((uint16_t *)V(vs2))[ei];
                uint32_t b = ((uint16_t *)V(vs1))[ei];
                ((uint16_t *)V(vd))[ei] = roundoff_u32(a - b, 1, rm);
                break;
            }
        case 32: {
                uint64_t a = ((uint32_t *)V(vs2))[ei];
                uint64_t b = ((uint32_t *)V(vs1))[ei];
                ((uint32_t *)V(vd))[ei] = roundoff_u64(a - b, 1, rm);
                break;
            }
        case 64: {
                __uint128_t a = ((uint64_t *)V(vs2))[ei];
                __uint128_t b = ((uint64_t *)V(vs1))[ei];
                ((uint64_t *)V(vd))[ei] = roundoff_u128(a - b, 1, rm);
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vasubu_mvx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint8_t rm = env->vxrm & 0b11;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8: {
                uint16_t a = ((uint8_t *)V(vs2))[ei];
                ((uint8_t *)V(vd))[ei] = roundoff_u16(a - (uint16_t)rs1, 1, rm);
                break;
            }
        case 16: {
                uint32_t a = ((uint16_t *)V(vs2))[ei];
                ((uint16_t *)V(vd))[ei] = roundoff_u32(a - (uint32_t)rs1, 1, rm);
                break;
            }
        case 32: {
                uint64_t a = ((uint32_t *)V(vs2))[ei];
                ((uint32_t *)V(vd))[ei] = roundoff_u64(a - (uint64_t)rs1, 1, rm);
                break;
            }
        case 64: {
                __uint128_t a = ((uint64_t *)V(vs2))[ei];
                ((uint64_t *)V(vd))[ei] = roundoff_u128(a - (__uint128_t)rs1, 1, rm);
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vsmul_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint8_t rm = env->vxrm & 0b11;
    const uint16_t shift = eew - 1;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8: {
                int16_t a = ((int8_t *)V(vs2))[ei];
                int16_t b = ((int8_t *)V(vs1))[ei];
                int16_t res = roundoff_i16(a * b, shift, rm);
                if (res < INT8_MIN) {
                    res = INT8_MIN;
                    env->vxsat |= 1;
                } else if (res > INT8_MAX) {
                    res = INT8_MAX;
                    env->vxsat |= 1;
                }
                ((int8_t *)V(vd))[ei] = res;
                break;
            }
        case 16: {
                int32_t a = ((int16_t *)V(vs2))[ei];
                int32_t b = ((int16_t *)V(vs1))[ei];
                int32_t res = roundoff_i32(a * b, shift, rm);
                if (res < INT16_MIN) {
                    res = INT16_MIN;
                    env->vxsat |= 1;
                } else if (res > INT16_MAX) {
                    res = INT16_MAX;
                    env->vxsat |= 1;
                }
                ((int16_t *)V(vd))[ei] = res;
                break;
            }
        case 32: {
                int64_t a = ((int32_t *)V(vs2))[ei];
                int64_t b = ((int32_t *)V(vs1))[ei];
                int64_t res = roundoff_i64(a * b, shift, rm);
                if (res < INT32_MIN) {
                    res = INT32_MIN;
                    env->vxsat |= 1;
                } else if (res > INT32_MAX) {
                    res = INT32_MAX;
                    env->vxsat |= 1;
                }
                ((int32_t *)V(vd))[ei] = res;
                break;
            }
        case 64: {
                __int128_t a = ((int64_t *)V(vs2))[ei];
                __int128_t b = ((int64_t *)V(vs1))[ei];
                __int128_t res = roundoff_i128(a * b, shift, rm);
                if (res < INT64_MIN) {
                    res = INT64_MIN;
                    env->vxsat |= 1;
                } else if (res > INT64_MAX) {
                    res = INT64_MAX;
                    env->vxsat |= 1;
                }
                ((int64_t *)V(vd))[ei] = res;
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vsmul_ivx, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_long rs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint8_t rm = env->vxrm & 0b11;
    const uint16_t shift = eew - 1;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8: {
                int16_t a = ((int8_t *)V(vs2))[ei];
                int16_t res = roundoff_i16(a * (int16_t)rs1, shift, rm);
                if (res < INT8_MIN) {
                    res = INT8_MIN;
                    env->vxsat |= 1;
                } else if (res > INT8_MAX) {
                    res = INT8_MAX;
                    env->vxsat |= 1;
                }
                ((int8_t *)V(vd))[ei] = res;
                break;
            }
        case 16: {
                int32_t a = ((int16_t *)V(vs2))[ei];
                int32_t res = roundoff_i32(a * (int32_t)rs1, shift, rm);
                if (res < INT16_MIN) {
                    res = INT16_MIN;
                    env->vxsat |= 1;
                } else if (res > INT16_MAX) {
                    res = INT16_MAX;
                    env->vxsat |= 1;
                }
                ((int16_t *)V(vd))[ei] = res;
                break;
            }
        case 32: {
                int64_t a = ((int32_t *)V(vs2))[ei];
                int64_t res = roundoff_i64(a * (int64_t)rs1, shift, rm);
                if (res < INT32_MIN) {
                    res = INT32_MIN;
                    env->vxsat |= 1;
                } else if (res > INT32_MAX) {
                    res = INT32_MAX;
                    env->vxsat |= 1;
                }
                ((int32_t *)V(vd))[ei] = res;
                break;
            }
        case 64: {
                __int128_t a = ((int64_t *)V(vs2))[ei];
                __int128_t res = roundoff_i128(a * (__int128_t)rs1, shift, rm);
                if (res < INT64_MIN) {
                    res = INT64_MIN;
                    env->vxsat |= 1;
                } else if (res > INT64_MAX) {
                    res = INT64_MAX;
                    env->vxsat |= 1;
                }
                ((int64_t *)V(vd))[ei] = res;
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vssrl_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint8_t rm = env->vxrm & 0b11;
    const uint16_t mask = eew - 1;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            ((uint8_t *)V(vd))[ei] = roundoff_u8(((uint8_t *)V(vs2))[ei], ((uint8_t *)V(vs1))[ei] & mask, rm);
            break;
        case 16:
            ((uint16_t *)V(vd))[ei] = roundoff_u16(((uint16_t *)V(vs2))[ei], ((uint16_t *)V(vs1))[ei] & mask, rm);
            break;
        case 32:
            ((uint32_t *)V(vd))[ei] = roundoff_u32(((uint32_t *)V(vs2))[ei], ((uint32_t *)V(vs1))[ei] & mask, rm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = roundoff_u64(((uint64_t *)V(vs2))[ei], ((uint64_t *)V(vs1))[ei] & mask, rm);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vssrl_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint8_t rm = env->vxrm & 0b11;
    const uint16_t shift = rs1 & (eew - 1);
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            ((uint8_t *)V(vd))[ei] = roundoff_u8(((uint8_t *)V(vs2))[ei], shift, rm);
            break;
        case 16:
            ((uint16_t *)V(vd))[ei] = roundoff_u16(((uint16_t *)V(vs2))[ei], shift, rm);
            break;
        case 32:
            ((uint32_t *)V(vd))[ei] = roundoff_u32(((uint32_t *)V(vs2))[ei], shift, rm);
            break;
        case 64:
            ((uint64_t *)V(vd))[ei] = roundoff_u64(((uint64_t *)V(vs2))[ei], shift, rm);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vssra_ivv, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, int32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint8_t rm = env->vxrm & 0b11;
    const uint16_t mask = eew - 1;
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            ((int8_t *)V(vd))[ei] = roundoff_i8(((int8_t *)V(vs2))[ei], ((int8_t *)V(vs1))[ei] & mask, rm);
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = roundoff_i16(((int16_t *)V(vs2))[ei], ((int16_t *)V(vs1))[ei] & mask, rm);
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = roundoff_i32(((int32_t *)V(vs2))[ei], ((int32_t *)V(vs1))[ei] & mask, rm);
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = roundoff_i64(((int64_t *)V(vs2))[ei], ((int64_t *)V(vs1))[ei] & mask, rm);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vssra_ivi, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2, target_ulong rs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    const uint8_t rm = env->vxrm & 0b11;
    const uint16_t shift = rs1 & (eew - 1);
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            ((int8_t *)V(vd))[ei] = roundoff_i8(((int8_t *)V(vs2))[ei], shift, rm);
            break;
        case 16:
            ((int16_t *)V(vd))[ei] = roundoff_i16(((int16_t *)V(vs2))[ei], shift, rm);
            break;
        case 32:
            ((int32_t *)V(vd))[ei] = roundoff_i32(((int32_t *)V(vs2))[ei], shift, rm);
            break;
        case 64:
            ((int64_t *)V(vd))[ei] = roundoff_i64(((int64_t *)V(vs2))[ei], shift, rm);
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
    }
}

void glue(helper_vredsum_vs, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    int64_t acc = 0;
    switch (eew) {
    case 8:
        acc = ((int8_t *)V(vs1))[0];
        break;
    case 16:
        acc = ((int16_t *)V(vs1))[0];
        break;
    case 32:
        acc = ((int32_t *)V(vs1))[0];
        break;
    case 64:
        acc = ((int64_t *)V(vs1))[0];
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            acc += ((int8_t *)V(vs2))[ei];
            break;
        case 16:
            acc += ((int16_t *)V(vs2))[ei];
            break;
        case 32:
            acc += ((int32_t *)V(vs2))[ei];
            break;
        case 64:
            acc += ((int64_t *)V(vs2))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
    }
    switch (eew) {
    case 8:
        ((int8_t *)V(vd))[0] = acc;
        break;
    case 16:
        ((int16_t *)V(vd))[0] = acc;
        break;
    case 32:
        ((int32_t *)V(vd))[0] = acc;
        break;
    case 64:
        ((int64_t *)V(vd))[0] = acc;
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
}

void glue(helper_vredmaxu_vs, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    uint64_t max = 0;
    switch (eew) {
    case 8:
        max = ((uint8_t *)V(vs1))[0];
        break;
    case 16:
        max = ((uint16_t *)V(vs1))[0];
        break;
    case 32:
        max = ((uint32_t *)V(vs1))[0];
        break;
    case 64:
        max = ((uint64_t *)V(vs1))[0];
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8: {
                uint8_t t = ((uint8_t *)V(vs2))[ei];
                max = t > max ? t : max;
                break;
            }
        case 16: {
                uint16_t t = ((uint16_t *)V(vs2))[ei];
                max = t > max ? t : max;
                break;
            }
        case 32: {
                uint32_t t = ((uint32_t *)V(vs2))[ei];
                max = t > max ? t : max;
                break;
            }
        case 64: {
                uint64_t t = ((uint64_t *)V(vs2))[ei];
                max = t > max ? t : max;
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
    }
    switch (eew) {
    case 8:
        ((uint8_t *)V(vd))[0] = max;
        break;
    case 16:
        ((uint16_t *)V(vd))[0] = max;
        break;
    case 32:
        ((uint32_t *)V(vd))[0] = max;
        break;
    case 64:
        ((uint64_t *)V(vd))[0] = max;
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
}

void glue(helper_vredmax_vs, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    int64_t max = 0;
    switch (eew) {
    case 8:
        max = ((int8_t *)V(vs1))[0];
        break;
    case 16:
        max = ((int16_t *)V(vs1))[0];
        break;
    case 32:
        max = ((int32_t *)V(vs1))[0];
        break;
    case 64:
        max = ((int64_t *)V(vs1))[0];
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8: {
                int8_t t = ((int8_t *)V(vs2))[ei];
                max = t > max ? t : max;
                break;
            }
        case 16: {
                int16_t t = ((int16_t *)V(vs2))[ei];
                max = t > max ? t : max;
                break;
            }
        case 32: {
                int32_t t = ((int32_t *)V(vs2))[ei];
                max = t > max ? t : max;
                break;
            }
        case 64: {
                int64_t t = ((int64_t *)V(vs2))[ei];
                max = t > max ? t : max;
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
    }
    switch (eew) {
    case 8:
        ((int8_t *)V(vd))[0] = max;
        break;
    case 16:
        ((int16_t *)V(vd))[0] = max;
        break;
    case 32:
        ((int32_t *)V(vd))[0] = max;
        break;
    case 64:
        ((int64_t *)V(vd))[0] = max;
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
}

void glue(helper_vredminu_vs, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    uint64_t min = 0;
    switch (eew) {
    case 8:
        min = ((uint8_t *)V(vs1))[0];
        break;
    case 16:
        min = ((uint16_t *)V(vs1))[0];
        break;
    case 32:
        min = ((uint32_t *)V(vs1))[0];
        break;
    case 64:
        min = ((uint64_t *)V(vs1))[0];
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8: {
                uint8_t t = ((uint8_t *)V(vs2))[ei];
                min = t < min ? t : min;
                break;
            }
        case 16: {
                uint16_t t = ((uint16_t *)V(vs2))[ei];
                min = t < min ? t : min;
                break;
            }
        case 32: {
                uint32_t t = ((uint32_t *)V(vs2))[ei];
                min = t < min ? t : min;
                break;
            }
        case 64: {
                uint64_t t = ((uint64_t *)V(vs2))[ei];
                min = t < min ? t : min;
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
    }
    switch (eew) {
    case 8:
        ((uint8_t *)V(vd))[0] = min;
        break;
    case 16:
        ((uint16_t *)V(vd))[0] = min;
        break;
    case 32:
        ((uint32_t *)V(vd))[0] = min;
        break;
    case 64:
        ((uint64_t *)V(vd))[0] = min;
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
}

void glue(helper_vredmin_vs, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    int64_t min = 0;
    switch (eew) {
    case 8:
        min = ((int8_t *)V(vs1))[0];
        break;
    case 16:
        min = ((int16_t *)V(vs1))[0];
        break;
    case 32:
        min = ((int32_t *)V(vs1))[0];
        break;
    case 64:
        min = ((int64_t *)V(vs1))[0];
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8: {
                int8_t t = ((int8_t *)V(vs2))[ei];
                min = t < min ? t : min;
                break;
            }
        case 16: {
                int16_t t = ((int16_t *)V(vs2))[ei];
                min = t < min ? t : min;
                break;
            }
        case 32: {
                int32_t t = ((int32_t *)V(vs2))[ei];
                min = t < min ? t : min;
                break;
            }
        case 64: {
                int64_t t = ((int64_t *)V(vs2))[ei];
                min = t < min ? t : min;
                break;
            }
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
    }
    switch (eew) {
    case 8:
        ((int8_t *)V(vd))[0] = min;
        break;
    case 16:
        ((int16_t *)V(vd))[0] = min;
        break;
    case 32:
        ((int32_t *)V(vd))[0] = min;
        break;
    case 64:
        ((int64_t *)V(vd))[0] = min;
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
}

void glue(helper_vredand_vs, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    int64_t acc = 0;
    switch (eew) {
    case 8:
        acc = ((int8_t *)V(vs1))[0];
        break;
    case 16:
        acc = ((int16_t *)V(vs1))[0];
        break;
    case 32:
        acc = ((int32_t *)V(vs1))[0];
        break;
    case 64:
        acc = ((int64_t *)V(vs1))[0];
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            acc &= ((int8_t *)V(vs2))[ei];
            break;
        case 16:
            acc &= ((int16_t *)V(vs2))[ei];
            break;
        case 32:
            acc &= ((int32_t *)V(vs2))[ei];
            break;
        case 64:
            acc &= ((int64_t *)V(vs2))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
    }
    switch (eew) {
    case 8:
        ((int8_t *)V(vd))[0] = acc;
        break;
    case 16:
        ((int16_t *)V(vd))[0] = acc;
        break;
    case 32:
        ((int32_t *)V(vd))[0] = acc;
        break;
    case 64:
        ((int64_t *)V(vd))[0] = acc;
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
}

void glue(helper_vredor_vs, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    int64_t acc = 0;
    switch (eew) {
    case 8:
        acc = ((int8_t *)V(vs1))[0];
        break;
    case 16:
        acc = ((int16_t *)V(vs1))[0];
        break;
    case 32:
        acc = ((int32_t *)V(vs1))[0];
        break;
    case 64:
        acc = ((int64_t *)V(vs1))[0];
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            acc |= ((int8_t *)V(vs2))[ei];
            break;
        case 16:
            acc |= ((int16_t *)V(vs2))[ei];
            break;
        case 32:
            acc |= ((int32_t *)V(vs2))[ei];
            break;
        case 64:
            acc |= ((int64_t *)V(vs2))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
    }
    switch (eew) {
    case 8:
        ((int8_t *)V(vd))[0] = acc;
        break;
    case 16:
        ((int16_t *)V(vd))[0] = acc;
        break;
    case 32:
        ((int32_t *)V(vd))[0] = acc;
        break;
    case 64:
        ((int64_t *)V(vd))[0] = acc;
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
}

void glue(helper_vredxor_vs, POSTFIX)(CPUState *env, uint32_t vd, uint32_t vs2, uint32_t vs1)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID(vs2) || V_IDX_INVALID(vs1)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    int64_t acc = 0;
    switch (eew) {
    case 8:
        acc = ((int8_t *)V(vs1))[0];
        break;
    case 16:
        acc = ((int16_t *)V(vs1))[0];
        break;
    case 32:
        acc = ((int32_t *)V(vs1))[0];
        break;
    case 64:
        acc = ((int64_t *)V(vs1))[0];
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            acc ^= ((int8_t *)V(vs2))[ei];
            break;
        case 16:
            acc ^= ((int16_t *)V(vs2))[ei];
            break;
        case 32:
            acc ^= ((int32_t *)V(vs2))[ei];
            break;
        case 64:
            acc ^= ((int64_t *)V(vs2))[ei];
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            return;
        }
    }
    switch (eew) {
    case 8:
        ((int8_t *)V(vd))[0] = acc;
        break;
    case 16:
        ((int16_t *)V(vd))[0] = acc;
        break;
    case 32:
        ((int32_t *)V(vd))[0] = acc;
        break;
    case 64:
        ((int64_t *)V(vd))[0] = acc;
        break;
    default:
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
        return;
    }
}

void glue(helper_viota, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2)
{
    if (V_IDX_INVALID(vd) || env->vstart) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    uint64_t cnt = 0;
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            ((uint8_t *)V(vd))[ei] = cnt;
            break;
        case 16:
            ((uint16_t *)V(vd))[ei] = cnt;
            break;
        case 32:
            ((uint32_t *)V(vd))[ei] = cnt;
            break;
        case 64:
            ((uint64_t *)V(vs2))[ei] = cnt;
            break;
        default:
            helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
            break;
        }
        cnt += !!(V(vs2)[ei >> 3] & (1 << (ei & 0x7)));
    }
}

void glue(helper_vid, POSTFIX)(CPUState *env, uint32_t vd, int32_t vs2)
{
    if (V_IDX_INVALID(vd) || env->vstart) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    const target_ulong eew = env->vsew;
    for (int ei = 0; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        switch (eew) {
        case 8:
            ((uint8_t *)V(vd))[ei] = ei;
            break;
        case 16:
            ((uint16_t *)V(vd))[ei] = ei;
            break;
        case 32:
            ((uint32_t *)V(vd))[ei] = ei;
            break;
        case 64:
            ((uint64_t *)V(vs2))[ei] = ei;
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

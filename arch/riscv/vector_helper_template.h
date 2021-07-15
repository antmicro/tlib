
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
    if (V_IDX_INVALID_EEW(vd, SHIFT)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    target_ulong src_addr = env->gpr[rs1];
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        for (int fi = 0; fi <= nf; ++fi) {
            ((DATA_TYPE *)V(vd + fi))[ei] = glue(ld, USUFFIX)(src_addr + ei * DATA_SIZE);
        }
    }
}

void glue(glue(helper_vlse, BITS), POSTFIX)(CPUState *env, uint32_t vd, uint32_t rs1, uint32_t rs2, uint32_t nf)
{
    if (V_IDX_INVALID_EEW(vd, SHIFT)) {
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
        for (int fi = 0; fi <= nf; ++fi) {
            ((DATA_TYPE *)V(vd + fi))[ei] = glue(ld, USUFFIX)(src_addr + ei * offset);
        }
    }
}

void glue(glue(helper_vlxei, BITS), POSTFIX)(CPUState *env, uint32_t vd, uint32_t rs1, uint32_t vs2, uint32_t nf)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID_EEW(vs2, SHIFT)) {
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
        for (int fi = 0; fi <= nf; ++fi) {
            switch (dst_eew) {
            case 8:
                V(vd + fi)[ei] = ldub(src_addr + ((DATA_TYPE *)V(vs2 + fi))[ei]);
                break;
            case 16:
                ((uint16_t *)V(vd + fi))[ei] = lduw(src_addr + offsets[ei] + sizeof(DATA_TYPE) * fi);
                break;
            case 32:
                ((uint32_t *)V(vd + fi))[ei] = ldl(src_addr + offsets[ei] + sizeof(DATA_TYPE) * fi);
                break;
            case 64: 
                ((uint64_t *)V(vd + fi))[ei] = ldq(src_addr + offsets[ei] + sizeof(DATA_TYPE) * fi);
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
    if (V_IDX_INVALID_EEW(vd, SHIFT)) {
        helper_raise_exception(env, RISCV_EXCP_ILLEGAL_INST);
    }
    target_ulong src_addr = env->gpr[rs1];
    for (int ei = env->vstart; ei < env->vl; ++ei) {
#ifdef MASKED
        if (!(V(0)[ei >> 3] & (1 << (ei & 0x7)))) {
            continue;
        }
#endif
        for (int fi = 0; fi <= nf; ++fi) {
            glue(st, SUFFIX)(src_addr + ei * DATA_SIZE, ((DATA_TYPE *)V(vd + fi))[ei]);
        }
    }
}

void glue(glue(helper_vsse, BITS), POSTFIX)(CPUState *env, uint32_t vd, uint32_t rs1, uint32_t rs2, uint32_t nf)
{
    if (V_IDX_INVALID_EEW(vd, SHIFT)) {
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
        for (int fi = 0; fi <= nf; ++fi) {
            glue(st, SUFFIX)(src_addr + ei * offset, ((DATA_TYPE *)V(vd + fi))[ei]);
        }
    }
}

void glue(glue(helper_vsxei, BITS), POSTFIX)(CPUState *env, uint32_t vd, uint32_t rs1, uint32_t vs2, uint32_t nf)
{
    if (V_IDX_INVALID(vd) || V_IDX_INVALID_EEW(vs2, SHIFT)) {
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
        for (int fi = 0; fi <= nf; ++fi) {
            switch (dst_eew) {
            case 0:
                stb(src_addr + ((DATA_TYPE *)V(vs2 + fi))[ei], V(vd + fi)[ei]);
                break;
            case 16:
                stw(src_addr + offsets[ei] + sizeof(DATA_TYPE) * fi, ((uint16_t *)V(vd + fi))[ei]);
                break;
            case 32:
                stl(src_addr + offsets[ei] + sizeof(DATA_TYPE) * fi, ((uint32_t *)V(vd + fi))[ei]);
                break;
            case 64: 
                stq(src_addr + offsets[ei] + sizeof(DATA_TYPE) * fi, ((uint64_t *)V(vd + fi))[ei]);
                break;
            default:
                tlib_abortf("Unsupported EEW");
                break;
            }
        }
    }
}

#undef SHIFT
#undef DATA_TYPE
#undef BITS
#undef SUFFIX
#undef USUFFIX
#undef DATA_SIZE
#undef MASKED
#undef POSTFIX

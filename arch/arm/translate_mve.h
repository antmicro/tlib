/*
 *  ARM translation for M-Profile Vector Extension (MVE)
 *
 *  Copyright (c) Antmicro
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

typedef void MVEGenLdStFn(TCGv_ptr, TCGv_ptr, TCGv_i32);
typedef void MVEGenLdStIlFn(DisasContext *, uint32_t, TCGv_i32);
typedef void MVEGenTwoOpScalarFn(TCGv_ptr, TCGv_ptr, TCGv_ptr, TCGv_i32);
typedef void MVEGenTwoOpFn(TCGv_ptr, TCGv_ptr, TCGv_ptr, TCGv_ptr);
typedef void MVEGenCmpFn(TCGv_ptr, TCGv_ptr, TCGv_ptr);
typedef void MVEGenScalarCmpFn(TCGv_ptr, TCGv_ptr, TCGv_i32);
typedef void MVEGenVIDUPFn(TCGv_i32, TCGv_ptr, TCGv_ptr, TCGv_i32, TCGv_i32);
typedef void MVEGenVIWDUPFn(TCGv_i32, TCGv_ptr, TCGv_ptr, TCGv_i32, TCGv_i32, TCGv_i32);
typedef void MVEGenVADDVFn(TCGv_i32, TCGv_ptr, TCGv_ptr, TCGv_i32);
typedef void MVEGenOneOpFn(TCGv_ptr, TCGv_ptr, TCGv_ptr);

/* Note that the gvec expanders operate on offsets + sizes.  */
typedef void GVecGen3Fn(unsigned, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void GVecGen2Fn(unsigned, uint32_t, uint32_t, uint32_t, uint32_t);

/*
 * Arguments of stores/loads:
 * VSTRB, VSTRH, VSTRW, VLDRB, VLDRH, VLDRW
 */
typedef struct {
    int rn;
    int qd;
    int imm;
    int p;
    int a;
    int w;
    int size;
    /* Used to tell store/load apart */
    int l;
    int u;
} arg_vldr_vstr;

/*
 * Arguments of (de)interleaving stores/loads:
 * VLD2, VLD4, VST2, VST4
 */
typedef struct {
    int qd;
    int rn;
    int size;
    int pat;
    int w;
} arg_vldst_il;

/* Arguments of 2 operand scalar instructions */
typedef struct {
    int qd;
    int qm;
    int qn;
    int size;
} arg_2op;

/* Arguments of 2 operand scalar instructions */
typedef struct {
    int qd;
    int qn;
    int rm;
    int size;
} arg_2scalar;

/* Arguments of VDUP instruction */
typedef struct {
    int qd;
    int rt;
    /* See comment in trans function */
    int size;
} arg_vdup;

/* Arguments of VCTP instruction */
typedef struct {
    int rn;
    int size;
} arg_vctp;

/* Arguments of VPST instruction */
typedef struct {
    int mask;
} arg_vpst;

/* Arguments of VCMP (non-scalar) instruction (both floating-point and vector) */
typedef struct {
    int qm;
    int qn;
    int size;
    int mask;
} arg_vcmp;

/* Arguments of VCMP scalar instructions (both floating-point and vector) */
typedef struct {
    int qn;
    int rm;
    int size;
    int mask;
} arg_vcmp_scalar;

/* Arguments of VIDUP and VDDUP instructions */
typedef struct {
    int qd;
    int rn;
    int size;
    int imm;
} arg_vidup;

/* Arguments of VIWDUP and VDWDUP instructions */
typedef struct {
    int qd;
    int rn;
    int rm;
    int size;
    int imm;
} arg_viwdup;

/* Arguments of VMAX*V and VMIN*V instructions */
typedef struct {
    int qm;
    int rda;
    int size;
} arg_vmaxv;

/* Arguments of 1 operand vector instruction */
typedef struct {
    int qd;
    int qm;
    int size;
} arg_1op;

void gen_mve_vld40b(DisasContext *s, uint32_t qnindx, TCGv_i32 base);
void gen_mve_vld41b(DisasContext *s, uint32_t qnindx, TCGv_i32 base);
void gen_mve_vld42b(DisasContext *s, uint32_t qnindx, TCGv_i32 base);
void gen_mve_vld43b(DisasContext *s, uint32_t qnindx, TCGv_i32 base);
void gen_mve_vld40h(DisasContext *s, uint32_t qnindx, TCGv_i32 base);
void gen_mve_vld41h(DisasContext *s, uint32_t qnindx, TCGv_i32 base);
void gen_mve_vld42h(DisasContext *s, uint32_t qnindx, TCGv_i32 base);
void gen_mve_vld43h(DisasContext *s, uint32_t qnindx, TCGv_i32 base);
void gen_mve_vld40w(DisasContext *s, uint32_t qnindx, TCGv_i32 base);
void gen_mve_vld41w(DisasContext *s, uint32_t qnindx, TCGv_i32 base);
void gen_mve_vld42w(DisasContext *s, uint32_t qnindx, TCGv_i32 base);
void gen_mve_vld43w(DisasContext *s, uint32_t qnindx, TCGv_i32 base);
void gen_mve_vpst(DisasContext *s, uint32_t mask);

/*
 * Vector load/store register (encodings T5, T6, T7)
 * VLDRB, VLDRH, VLDRW, VSTRB, VSTRH, VSTRW
 */
static inline bool is_insn_vldst(uint32_t insn)
{
    return (insn & 0xEE401E00) == 0xEC000E00;
}

static inline bool is_insn_vldr_vstr(uint32_t insn)
{
    if((insn & 0xFE001E00) == 0xEC001E00) {
        uint32_t p = extract32(insn, 24, 1);
        uint32_t w = extract32(insn, 21, 1);

        /* P == 0 && W == 0 is related encodings */

        return (p == 0 && w == 1) || p == 1;
    }

    return false;
}

static inline bool is_insn_vadd_fp(uint32_t insn)
{
    return (insn & 0xFFA11F51) == 0xEF000D40;
}

static inline bool is_insn_vadd_fp_scalar(uint32_t insn)
{
    return (insn & 0xEFB11F70) == 0xEE300F40;
}

static inline bool is_insn_vsub_fp(uint32_t insn)
{
    return (insn & 0xFFA11F51) == 0xEF200D40;
}

static inline bool is_insn_vsub_fp_scalar(uint32_t insn)
{
    return (insn & 0xEFB11F70) == 0xEE301F40;
}

/* VMUL (floating-point) T1 */
static inline bool is_insn_vmul_fp(uint32_t insn)
{
    return (insn & 0xFFAF1F51) == 0xFF000D50;
}

/* VMUL (floating-point) T2 */
static inline bool is_insn_vmul_fp_scalar(uint32_t insn)
{
    return (insn & 0xEFB11F70) == 0xEE310E60;
}

static inline bool is_insn_vfma_scalar(uint32_t insn)
{
    return (insn & 0xEFB11F70) == 0xEE310E40;
}

static inline bool is_insn_vfmas_scalar(uint32_t insn)
{
    return (insn & 0xEFB11F70) == 0xEE311E40;
}

static inline bool is_insn_vdup(uint32_t insn)
{
    return (insn & 0xFFB10F50) == 0xEEA00B10;
}

static inline bool is_insn_vctp(uint32_t insn)
{
    /* Rn == 0b1111 is related-encoding */
    return extract32(insn, 16, 4) != 15 && (insn & 0xFFC0F801) == 0xF000E801;
}

static inline bool is_insn_vpst(uint32_t insn)
{
    uint32_t mask = deposit32(extract32(insn, 13, 3), 3, 29, extract32(insn, 22, 1));
    return mask != 0 && (insn & 0xFFB10F5F) == 0xFE310F4D;
}

static inline bool is_insn_vcmp_fp(uint32_t insn)
{
    uint32_t fca = extract32(insn, 0, 0);
    uint32_t fcb = extract32(insn, 0, 0);

    /* fcA == 1 && fcB == 1 is related encodings */
    if(fca == 1 && fcb == 1) {
        return false;
    }
    return (insn & 0xEFF1EF50) == 0xEE310F00;
}

static inline bool is_insn_vcmp_fp_scalar(uint32_t insn)
{
    uint32_t rm = extract32(insn, 0, 4);

    /* rm == 0b1101 is related encodings */
    if(rm == 13) {
        return false;
    }
    return (insn & 0xEFF1EF50) == 0xEE310F40;
}

static inline bool is_insn_vidup(uint32_t insn)
{
    uint32_t size = extract32(insn, 20, 2);
    /* size = 0b11 is related encodings */
    if(size == 3) {
        return false;
    }
    return (insn & 0xFF811F7E) == 0xEE010F6E;
}

static inline bool is_insn_vddup(uint32_t insn)
{
    uint32_t size = extract32(insn, 20, 2);
    /* size = 0b11 is related encodings */
    if(size == 3) {
        return false;
    }
    return (insn & 0xFF811F7E) == 0xEE011F6E;
}

static inline bool is_insn_viwdup(uint32_t insn)
{
    uint32_t size = extract32(insn, 20, 2);
    /* size = 0b11 is related encodings */
    if(size == 3) {
        return false;
    }
    uint32_t rm = extract32(insn, 1, 3);
    /* rm == 0b111 related encodings */
    if(rm == 7) {
        return false;
    }
    return (insn & 0xFF811F70) == 0xEE010F60;
}

static inline bool is_insn_vdwdup(uint32_t insn)
{
    uint32_t size = extract32(insn, 20, 2);
    /* size = 0b11 is related encodings */
    if(size == 3) {
        return false;
    }
    uint32_t rm = extract32(insn, 1, 3);
    /* rm == 0b111 related encodings */
    if(rm == 7) {
        return false;
    }
    return (insn & 0xFF811F70) == 0xEE011F60;
}

static inline bool is_insn_vpsel(uint32_t insn)
{
    return (insn & 0xFFB11F51) == 0xFE310F01;
}

static inline bool is_insn_vld4(uint32_t insn)
{
    return (insn & 0xFF901E01) == 0xFC901E01;
}

static inline bool is_insn_vcmul(uint32_t insn)
{
    return (insn & 0xEFB10F50) == 0xEE300E00;
}

static inline bool is_insn_vcls(uint32_t insn)
{
    return (insn & 0xFFB31FD1) == 0xFFB00440;
}

static inline bool is_insn_vclz(uint32_t insn)
{
    return (insn & 0xFFB31FD1) == 0xFFB004C0;
}

static inline bool is_insn_vabs(uint32_t insn)
{
    return (insn & 0xFFB31FD1) == 0xFFB10340;
}

static inline bool is_insn_vneg(uint32_t insn)
{
    return (insn & 0xFFB31FD1) == 0xFFB103C0;
}

static inline bool is_insn_vabs_fp(uint32_t insn)
{
    return (insn & 0xFFB31FD1) == 0xFFB10740;
}

static inline bool is_insn_vneg_fp(uint32_t insn)
{
    return (insn & 0xFFB31FD1) == 0xFFB107C0;
}

static inline bool is_insn_vmaxa(uint32_t insn)
{
    uint32_t size = extract32(insn, 18, 2);
    return size != 3 && (insn & 0xFFB31FD1) == 0xEE330E81;
}

static inline bool is_insn_vmina(uint32_t insn)
{
    uint32_t size = extract32(insn, 18, 2);
    return size != 3 && (insn & 0xFFB31FD1) == 0xEE331E81;
}

static inline bool is_insn_vrev(uint32_t insn)
{
    uint32_t mask = extract32(insn, 7, 2);
    return mask != 3 && (insn & 0xFFB31E51) == 0xFFB00040;
}

static inline bool is_insn_vmaxnm(uint32_t insn)
{
    return (insn & 0xFFA11F51) == 0xFF000F50;
}

static inline bool is_insn_vminnm(uint32_t insn)
{
    return (insn & 0xFFA11F51) == 0xFF200F50;
}

static inline bool is_insn_vmaxnma(uint32_t insn)
{
    return (insn & 0xEFBF1FD1) == 0xEE3F0E81;
}

static inline bool is_insn_vminnma(uint32_t insn)
{
    return (insn & 0xEFBF1FD1) == 0xEE3F1E81;
}

/* Extract arguments of loads/stores */
static void mve_extract_vldr_vstr(arg_vldr_vstr *a, uint32_t insn)
{
    a->rn = extract32(insn, 16, 4);
    a->l = extract32(insn, 20, 1);
    a->qd = deposit32(extract32(insn, 13, 3), 3, 29, extract32(insn, 22, 1));
    a->u = 0;
    a->imm = extract32(insn, 0, 7);
    a->p = extract32(insn, 24, 1);
    a->a = extract32(insn, 23, 1);
    a->w = extract32(insn, 21, 1);
    a->size = extract32(insn, 7, 2);
}

/* Extract arguments of widening/narrowing loads/stores */
static void mve_extract_vldst_wn(arg_vldr_vstr *a, uint32_t insn)
{
    a->rn = extract32(insn, 16, 3);
    a->l = extract32(insn, 20, 1);
    a->qd = extract32(insn, 13, 3);
    a->u = extract32(insn, 28, 1);
    a->imm = extract32(insn, 0, 7);
    a->p = extract32(insn, 24, 1);
    a->a = extract32(insn, 23, 1);
    a->w = extract32(insn, 21, 1);
    a->size = extract32(insn, 7, 2);
}

/* Extract arguments of (de)interleaving stores/loads */
static void extract_arg_vldst_il(arg_vldst_il *a, uint32_t insn)
{
    a->qd = extract32(insn, 13, 3);
    a->rn = extract32(insn, 16, 4);
    a->size = extract32(insn, 7, 2);
    a->pat = extract32(insn, 5, 2);
    a->w = extract32(insn, 21, 1);
}

/* Extract arguments of 2-operand scalar floating-point */
static void mve_extract_2op_fp_scalar(arg_2scalar *a, uint32_t insn)
{
    a->size = extract32(insn, 28, 1);
    a->qd = deposit32(extract32(insn, 13, 3), 3, 29, extract32(insn, 22, 1));
    a->qn = deposit32(extract32(insn, 17, 3), 3, 29, extract32(insn, 7, 1));
    a->rm = extract32(insn, 0, 4);
}

/* Extract arguments of 2 operand floating operations */
static void mve_extract_2op_fp(arg_2op *a, uint32_t insn)
{
    a->size = extract32(insn, 20, 1);
    a->qd = deposit32(extract32(insn, 13, 3), 3, 29, extract32(insn, 22, 1));
    a->qn = deposit32(extract32(insn, 17, 3), 3, 29, extract32(insn, 7, 1));
    a->qm = deposit32(extract32(insn, 1, 3), 3, 29, extract32(insn, 5, 1));
}

/* Extract arguments for VDUP instruction */
static void mve_extract_vdup(arg_vdup *a, uint32_t insn)
{
    a->size = deposit32(extract32(insn, 22, 1), 1, 31, extract32(insn, 5, 1));
    a->qd = deposit32(extract32(insn, 17, 3), 3, 29, extract32(insn, 7, 1));
    a->rt = extract32(insn, 12, 4);
}

/* Extract arguments of VCTP instruction */
static void mve_extract_vctp(arg_vctp *a, uint32_t insn)
{
    a->size = extract32(insn, 20, 2);
    a->rn = extract32(insn, 16, 4);
}

/* Extract arguments of VPST instruction */
static void mve_extract_vpst(arg_vpst *a, uint32_t insn)
{
    a->mask = deposit32(extract32(insn, 13, 3), 3, 29, extract32(insn, 22, 1));
}

/* Extract arguments of VCMP floating-point instruction */
static void mve_extract_vcmp_fp(arg_vcmp *a, uint32_t insn)
{
    a->qn = extract32(insn, 17, 3);
    a->mask = deposit32(extract32(insn, 13, 3), 3, 29, extract32(insn, 22, 1));
    a->qm = deposit32(extract32(insn, 1, 3), 3, 29, extract32(insn, 5, 1));
    a->size = extract32(insn, 28, 1);
}

/* Extract arguments of VCMP scalar floating-point instruction */
static void mve_extract_vcmp_fp_scalar(arg_vcmp_scalar *a, uint32_t insn)
{
    a->qn = extract32(insn, 17, 3);
    a->mask = deposit32(extract32(insn, 13, 3), 3, 29, extract32(insn, 22, 1));
    a->rm = extract32(insn, 0, 4);
    a->size = extract32(insn, 28, 1);
}

/* Extract arguments for VIDUP and VDDUP */
static void mve_extract_vidup(arg_vidup *a, uint32_t insn)
{
    uint32_t imm = deposit32(extract32(insn, 0, 1), 1, 31, extract32(insn, 7, 1));
    a->imm = 1 << imm;
    a->qd = deposit32(extract32(insn, 13, 3), 3, 29, extract32(insn, 22, 1));
    a->size = extract32(insn, 20, 2);
    /* Decode for this instruction puts a zero at least significant bit */
    a->rn = extract32(insn, 17, 3) << 1;
}

/* Extract arguments of VIWDUP and VDWDUP instructions */
static void mve_extract_viwdup(arg_viwdup *a, uint32_t insn)
{
    uint32_t imm = deposit32(extract32(insn, 0, 1), 1, 31, extract32(insn, 7, 1));
    a->imm = 1 << imm;
    a->qd = deposit32(extract32(insn, 13, 3), 3, 29, extract32(insn, 22, 1));
    a->size = extract32(insn, 20, 2);
    /* Decode for this instruction puts a one at least significant bit */
    a->rm = (extract32(insn, 1, 3) << 1) + 1;
    /* Decode for this instruction puts a zero at least significant bit */
    a->rn = extract32(insn, 17, 3) << 1;
}

/* Extract arguments of VMAXV/VMAXAV/VMINV/VMINAV instructions */
static void mve_extract_vmaxv(arg_vmaxv *a, uint32_t insn)
{
    a->qm = deposit32(extract32(insn, 1, 3), 3, 29, extract32(insn, 5, 1));
    a->rda = extract32(insn, 12, 4);
    a->size = extract32(insn, 18, 2);
}

/* Extract arguments of VMAXNMV/VMAXNMAV/VMINNMV/VMINNMAV instructions */
static void mve_extract_vmaxnmv(arg_vmaxv *a, uint32_t insn)
{
    a->qm = deposit32(extract32(insn, 1, 3), 3, 29, extract32(insn, 5, 1));
    a->rda = extract32(insn, 12, 4);
    a->size = extract32(insn, 28, 1);
}

/* Extract arguments of VMAXNMA/VMINNMA instructions */
static void mve_extract_vmaxnma(arg_2op *a, uint32_t insn)
{
    a->qm = deposit32(extract32(insn, 1, 3), 3, 29, extract32(insn, 5, 1));
    a->qd = deposit32(extract32(insn, 13, 3), 3, 29, extract32(insn, 22, 1));
    a->qn = deposit32(extract32(insn, 13, 3), 3, 29, extract32(insn, 22, 1));
    a->size = extract32(insn, 28, 1);
}

/* Extract arguments of 2-operand instructions without a size */
static void mve_extract_2op_no_size(arg_2op *a, uint32_t insn)
{
    a->qd = deposit32(extract32(insn, 13, 3), 3, 29, extract32(insn, 22, 1));
    a->qm = deposit32(extract32(insn, 1, 3), 3, 29, extract32(insn, 5, 1));
    a->qn = deposit32(extract32(insn, 17, 3), 3, 29, extract32(insn, 7, 1));
    a->size = 0;
}

/* Extract arguments of VMUL instruction */
static void mve_extract_vmul(arg_2op *a, uint32_t insn)
{
    a->qd = deposit32(extract32(insn, 13, 3), 3, 29, extract32(insn, 22, 1));
    a->qm = deposit32(extract32(insn, 1, 3), 3, 29, extract32(insn, 5, 1));
    a->qn = deposit32(extract32(insn, 17, 3), 3, 29, extract32(insn, 7, 1));
    /*
     * We have to swap it here because for most instruction size of 1 means F32
     * but for some size of 0 means F32. We pass the size into `DO_TRANS_2OP_FP`
     * where it expect size of 0 zero to mean F32.
     */
    a->size = extract32(insn, 28, 1) == 0 ? 1 : 0;
}

/* Extract arguments of 1-operand instruction */
static void mve_extract_1op(arg_1op *a, uint32_t insn)
{
    a->qd = deposit32(extract32(insn, 13, 3), 3, 29, extract32(insn, 22, 1));
    a->qm = deposit32(extract32(insn, 1, 3), 3, 29, extract32(insn, 5, 1));
    a->size = extract32(insn, 18, 2);
}

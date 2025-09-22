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
/* Note that the gvec expanders operate on offsets + sizes.  */
typedef void GVecGen3Fn(unsigned, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

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

static inline bool is_insn_vld4(uint32_t insn)
{
    return (insn & 0xFF901E01) == 0xFC901E01;
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

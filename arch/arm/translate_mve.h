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

#define MVE_INSN_VSTRW_MASK 0xFE101F80
#define MVE_INSN_VSTRW      0xEC001F00

#define MVE_P_BIT_IDX 24
#define MVE_A_BIT_IDX 23
#define MVE_D_BIT_IDX 22
#define MVE_W_BIT_IDX 21

#define MVE_RN_IDX 16
#define MVE_RN_LEN 4

#define MVE_VD_IDX 12
#define MVE_VD_LEN 4

#define MVE_QD_IDX 13
#define MVE_QD_LEN 3

#define MVE_IMM_IDX 0
#define MVE_IMM_LEN 7

static inline bool is_insn_vstrw(uint32_t insn);

static inline int trans_vstrw(DisasContext *s, uint32_t insn);

static inline void mve_gen_vstrw_ir(DisasContext *s, uint32_t insn);
static inline TCGv_i64 mve_gen_vstrw_addr_ir(DisasContext *s, uint32_t insn);
static inline void mve_gen_vstrw_write_back_ir(DisasContext *s, uint32_t insn);

static inline bool mve_vstrw_is_constrained_unpredictable(DisasContext *s, uint32_t insn);
static inline bool mve_vstrw_is_undefined(uint32_t insn);

static inline long mve_get_reg64_offset(uint32_t reg_idx);
static inline long mve_get_reg128_offset(uint32_t reg_idx, uint32_t half_idx);

static inline uint32_t mve_extract_rn(uint32_t insn);
static inline uint32_t mve_extract_qd(uint32_t insn);
static inline uint32_t mve_extract_imm(uint32_t insn);
static inline uint32_t mve_extract_p_bit(uint32_t insn);
static inline uint32_t mve_extract_d_bit(uint32_t insn);
static inline uint32_t mve_extract_a_bit(uint32_t insn);
static inline uint32_t mve_extract_w_bit(uint32_t insn);

/* Check if given instruction is VSTRW */
static inline bool is_insn_vstrw(uint32_t insn)
{
    return (insn & MVE_INSN_VSTRW_MASK) == MVE_INSN_VSTRW;
}

/* Translate VSTRW instruction */
static inline int trans_vstrw(DisasContext *s, uint32_t insn)
{
    /* TODO: If we decide to implement MVE completely we will need to
       implement lane prediction mechanism. This instruction use
       it to mask which elements to store. Right now VSTRW is
       the only implemented instruction so we have no way to modify lane
       mask, it will always mask entire register.*/
    if(mve_vstrw_is_undefined(insn)) {
        return TRANS_STATUS_ILLEGAL_INSN;
    }

    if(mve_vstrw_is_constrained_unpredictable(s, insn)) {
        return TRANS_STATUS_ILLEGAL_INSN;
    }

    mve_gen_vstrw_ir(s, insn);

    return TRANS_STATUS_SUCCESS;
}

/* Generate intermediate representation for VSTRW instruction */
static inline void mve_gen_vstrw_ir(DisasContext *s, uint32_t insn)
{
    uint32_t mve_reg_idx = (mve_extract_d_bit(insn) << 3) + mve_extract_qd(insn);

    //  Read first 64 bits from memory representing MVE registers and store them
    uint32_t mve_reg_offset_front = mve_get_reg128_offset(mve_reg_idx, 0);
    TCGv_i64 addr_front = mve_gen_vstrw_addr_ir(s, insn);
    TCGv_i32 value_front = tcg_temp_new_i64();
    tcg_gen_ld_i64(value_front, cpu_env, mve_reg_offset_front);
    gen_st64(value_front, addr_front, context_to_mmu_index(s));

    //  Store remaining 64 bits, we have to increase target address by 8 bytes.
    uint32_t mve_reg_offset_back = mve_get_reg128_offset(mve_reg_idx, 1);
    TCGv_i64 addr_back = tcg_temp_new_i64();
    tcg_gen_addi_i64(addr_back, addr_front, 8);
    TCGv_i32 value_back = tcg_temp_new_i64();
    tcg_gen_ld_i64(value_back, cpu_env, mve_reg_offset_back);
    gen_st64(value_back, addr_back, context_to_mmu_index(s));

    //  If write back bit was set in insn, store address with offset in original register
    uint32_t is_write_back = mve_extract_w_bit(insn);
    if(is_write_back) {
        mve_gen_vstrw_write_back_ir(s, insn);
    }
    tcg_temp_free_i64(addr_front);
    tcg_temp_free_i64(addr_back);
}

/* Generate intermediate representation for runtime address creation of VSTRW instruction
   and return it as TCG variable */
static inline TCGv_i64 mve_gen_vstrw_addr_ir(DisasContext *s, uint32_t insn)
{
    uint32_t addr_reg = mve_extract_rn(insn);
    bool is_pre_increment = mve_extract_p_bit(insn);

    TCGv_i64 addr = load_reg(s, addr_reg);
    if(is_pre_increment) {
        uint32_t addr_offset = mve_extract_imm(insn) << 2;
        bool is_add = mve_extract_a_bit(insn);

        if(is_add) {
            tcg_gen_addi_i64(addr, addr, addr_offset);
        } else {
            tcg_gen_subi_i64(addr, addr, addr_offset);
        }
    }

    return addr;
}

/* Generate intermediate representation for address write back of VSTRW instruction */
static inline void mve_gen_vstrw_write_back_ir(DisasContext *s, uint32_t insn)
{
    uint32_t addr_reg = mve_extract_rn(insn);
    uint32_t addr_offset = mve_extract_imm(insn) << 2;
    bool is_add = mve_extract_a_bit(insn);

    TCGv_i64 addr = load_reg(s, addr_reg);
    if(is_add) {
        tcg_gen_addi_i64(addr, addr, addr_offset);
    } else {
        tcg_gen_subi_i64(addr, addr, addr_offset);
    }
    store_reg(s, addr_reg, addr);
}

/* Check if VSTRW instruction is CONSTRAINED_UNPREDICTABLE */
static inline bool mve_vstrw_is_constrained_unpredictable(DisasContext *s, uint32_t insn)
{
    uint32_t rn = mve_extract_rn(insn);
    uint32_t w_bit = mve_extract_w_bit(insn);
    return (s->condexec_mask) || (rn == 15) || (rn == 13 && w_bit == 1);
}

/* Check if VSTRW instruction is UNDEFINED */
static inline bool mve_vstrw_is_undefined(uint32_t insn)
{
    uint32_t d_bit = mve_extract_d_bit(insn);
    return d_bit == 1;
}

/* Calculate FPV register offset in CPUState struct */
static inline long mve_get_reg64_offset(uint32_t reg_idx)
{
    //  MVE is using VFP registers.
    return vfp_reg_offset(1, reg_idx);
}

/* Calculate MVE register offset in CPUState struct */
static inline long mve_get_reg128_offset(uint32_t reg_idx, uint32_t half_idx)
{
    //  2 VFP registers = 1 MVE register
    return mve_get_reg64_offset(2 * reg_idx + half_idx);
}

/* Extract rn from MVE instruction */
static inline uint32_t mve_extract_rn(uint32_t insn)
{
    return extract32(insn, MVE_RN_IDX, MVE_RN_LEN);
}

/* Extract qd from MVE instruction */
static inline uint32_t mve_extract_qd(uint32_t insn)
{
    return extract32(insn, MVE_QD_IDX, MVE_QD_LEN);
}

/* Extract imm from MVE instruction */
static inline uint32_t mve_extract_imm(uint32_t insn)
{
    return extract32(insn, MVE_IMM_IDX, MVE_IMM_LEN);
}

/* Extract p bit from MVE instruction */
static inline uint32_t mve_extract_p_bit(uint32_t insn)
{
    return extract32(insn, MVE_P_BIT_IDX, 1);
}

/* Extract a bit from MVE instruction */
static inline uint32_t mve_extract_a_bit(uint32_t insn)
{
    return extract32(insn, MVE_A_BIT_IDX, 1);
}

/* Extract d bit from MVE instruction */
static inline uint32_t mve_extract_d_bit(uint32_t insn)
{
    return extract32(insn, MVE_D_BIT_IDX, 1);
}

/* Extract w bit from MVE instruction */
static inline uint32_t mve_extract_w_bit(uint32_t insn)
{
    return extract32(insn, MVE_W_BIT_IDX, 1);
}

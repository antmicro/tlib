/*
 *  ARM translation for Low Overhead Branch (LOB) extension
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

#include "cpu_registers.h"

#define LOB_COUNTER_REG LR_32

#define LOB_INSN_WLS_MASK 0xFFF0F001
#define LOB_INSN_WLS      0xF040C001

#define LOB_INSN_DLS_MASK 0xFFF0FFFF
#define LOB_INSN_DLS      0xF040E001

#define LOB_INSN_LE_MASK 0xFFCFF001
#define LOB_INSN_LE      0xF00FC001

#define LOB_RN_IDX 16
#define LOB_RN_LEN 4

#define LOB_IMML_IDX 11
#define LOB_IMML_LEN 1

#define LOB_IMMH_IDX 1
#define LOB_IMMH_LEN 10

#define LOB_LE_INF_IDX 21
#define LOB_LE_INF_LEN 1

#define LOB_LS_TP_MASK 0x00400000
#define LOB_LS_TP      0x00000000

#define LOB_LE_TP_MASK 0x00100000
#define LOB_LE_TP      0x00100000

static inline bool is_insn_wls(uint32_t insn);
static inline bool is_insn_dls(uint32_t insn);
static inline bool is_insn_le(uint32_t insn);

static inline int trans_wls(DisasContext *s, uint32_t insn);
static inline int trans_dls(DisasContext *s, uint32_t insn);
static inline int trans_le(DisasContext *s, uint32_t insn);

static inline void lob_gen_wls_ir(DisasContext *s, uint32_t rn, uint32_t imm);
static inline void lob_gen_dls_ir(DisasContext *s, uint32_t rn);
static inline void lob_gen_le_ir(DisasContext *s, bool is_inf_loop, uint32_t imm);

static inline bool lob_is_ls_constraint_unpredictable(DisasContext *s, bool is_tp, uint32_t rn);
static inline bool lob_is_le_constraint_unpredictable(DisasContext *s);
static inline bool lob_is_ls_tp(uint32_t insn);
static inline bool lob_is_le_tp(uint32_t insn);

static inline uint32_t lob_extract_rn(uint32_t insn);
static inline uint32_t lob_extract_imm(uint32_t insn);
static inline uint32_t lob_extract_le_inf(uint32_t insn);

/* Check if given instruction is WLS(TP) */
static inline bool is_insn_wls(uint32_t insn)
{
    return (insn & LOB_INSN_WLS_MASK) == LOB_INSN_WLS;
}

/* Check if given instruction is DLS(TP) */
static inline bool is_insn_dls(uint32_t insn)
{
    return (insn & LOB_INSN_DLS_MASK) == LOB_INSN_DLS;
}

/* Check if given instruction is LE(TP) */
static inline bool is_insn_le(uint32_t insn)
{
    return (insn & LOB_INSN_LE_MASK) == LOB_INSN_LE;
}

/* Translate WLS(TP) instruction (TP not implemented) */
static inline int trans_wls(DisasContext *s, uint32_t insn)
{
    bool is_tp = lob_is_ls_tp(insn);
    uint32_t rn = lob_extract_rn(insn);
    uint32_t imm = lob_extract_imm(insn);

    if(lob_is_ls_constraint_unpredictable(s, is_tp, rn)) {
        return TRANS_STATUS_ILLEGAL_INSN;
    }

    if(is_tp) {
        //  TODO: Implement after MVE extension is ready
        return TRANS_STATUS_ILLEGAL_INSN;
    }

    lob_gen_wls_ir(s, rn, imm);

    return TRANS_STATUS_SUCCESS;
}

/* Translate DLS(TP) instruction (TP not implemented) */
static inline int trans_dls(DisasContext *s, uint32_t insn)
{
    bool is_tp = lob_is_ls_tp(insn);
    uint32_t rn = lob_extract_rn(insn);

    if(lob_is_ls_constraint_unpredictable(s, is_tp, rn)) {
        return TRANS_STATUS_ILLEGAL_INSN;
    }

    if(is_tp) {
        //  TODO: Implement after MVE extension is ready
        return TRANS_STATUS_ILLEGAL_INSN;
    }

    lob_gen_dls_ir(s, rn);

    return TRANS_STATUS_SUCCESS;
}

/* Translate LE(TP) instruction (TP not implemented) */
static inline int trans_le(DisasContext *s, uint32_t insn)
{
    bool is_tp = lob_is_le_tp(insn);
    bool is_inf_loop = lob_extract_le_inf(insn);
    uint32_t imm = lob_extract_imm(insn);

    if(lob_is_le_constraint_unpredictable(s)) {
        return TRANS_STATUS_ILLEGAL_INSN;
    }

    if(is_tp) {
        //  TODO: Implement after MVE extension is ready
        return TRANS_STATUS_ILLEGAL_INSN;
    }

    lob_gen_le_ir(s, is_inf_loop, imm);

    return TRANS_STATUS_SUCCESS;
}

/* Generate intermediate representation for WLS instruction */
static inline void lob_gen_wls_ir(DisasContext *s, uint32_t rn, uint32_t imm)
{
    //  If counter == 0 skip loop
    int next_insn_label = gen_new_label();
    tcg_gen_brcondi_i32(TCG_COND_EQ, cpu_R[rn], 0, next_insn_label);
    //  Setup loop counter
    TCGv_i32 loop_counter = load_reg(s, rn);
    store_reg(s, LOB_COUNTER_REG, loop_counter);
    //  Jump to loop content (next TB)
    gen_goto_tb(s, 1, s->base.pc);
    //  Loop ending instructions
    gen_set_label(next_insn_label);
    gen_jmp(s, s->base.pc + imm, STACK_FRAME_NO_CHANGE);
}

/* Generate intermediate representation for DLS instruction */
static inline void lob_gen_dls_ir(DisasContext *s, uint32_t rn)
{
    //  Setup loop counter
    TCGv_i32 loop_counter = load_reg(s, rn);
    store_reg(s, LOB_COUNTER_REG, loop_counter);
}

/* Generate intermediate representation for LE instruction */
static inline void lob_gen_le_ir(DisasContext *s, bool is_inf_loop, uint32_t imm)
{
    uint32_t loop_start_addr = s->base.pc - imm;
    if(is_inf_loop) {
        //  Jump back to the loop start
        gen_jmp(s, loop_start_addr, STACK_FRAME_NO_CHANGE);
    } else {
        int loop_end_label = gen_new_label();
        TCGv_i32 loop_counter_reg = cpu_R[LOB_COUNTER_REG];
        //  Jump outside loop if counter == 0
        tcg_gen_brcondi_i32(TCG_COND_LEU, loop_counter_reg, 1, loop_end_label);
        //  Decrement loop counter
        tcg_gen_addi_i32(loop_counter_reg, loop_counter_reg, -1);
        //  Jump back to the loop start
        gen_jmp(s, loop_start_addr, STACK_FRAME_NO_CHANGE);
        //  Loop ending instructions
        gen_set_label(loop_end_label);
        gen_goto_tb(s, 1, s->base.pc);
    }
}

/* Check if loop start instruction is CONSTRAINED_UNPREDICTABLE */
static inline bool lob_is_ls_constraint_unpredictable(DisasContext *s, bool is_tp, uint32_t rn)
{
    return (rn == 13) || (!is_tp && rn == 15) || (s->condexec_mask);
}

/* Check if loop end instruction is CONSTRAINED_UNPREDICTABLE */
static inline bool lob_is_le_constraint_unpredictable(DisasContext *s)
{
    return s->condexec_mask;
}

/* Check if loop start instruction is tail prediction variant */
static inline bool lob_is_ls_tp(uint32_t insn)
{
    return (insn & LOB_LS_TP_MASK) == LOB_LS_TP;
}

/* Check if loop end instruction is tail prediction variant */
static inline bool lob_is_le_tp(uint32_t insn)
{
    return (insn & LOB_LE_TP_MASK) == LOB_LE_TP;
}

/* Extract rn from LOB instruction */
static inline uint32_t lob_extract_rn(uint32_t insn)
{
    return extract32(insn, LOB_RN_IDX, LOB_RN_LEN);
}

/* Extract imm from LOB instruction */
static inline uint32_t lob_extract_imm(uint32_t insn)
{
    uint32_t imml = extract32(insn, LOB_IMML_IDX, LOB_IMML_LEN);
    uint32_t immh = extract32(insn, LOB_IMMH_IDX, LOB_IMMH_LEN);
    return (immh << 2) + (imml << 1);
}

/* Extract infinite loop bit from loop end instruction */
static inline uint32_t lob_extract_le_inf(uint32_t insn)
{
    return extract32(insn, LOB_LE_INF_IDX, LOB_LE_INF_LEN);
}

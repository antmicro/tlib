/*
 * Tiny Code Generator for tlib
 *
 * Copyright (c) 2024 Antmicro
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

//  Order registers get picked in
static const int tcg_target_reg_alloc_order[] = {
    TCG_REG_R8,  TCG_REG_R9,  TCG_REG_R10, TCG_REG_R11, TCG_REG_R12, TCG_REG_R13, TCG_REG_R14,
    TCG_REG_R15, TCG_REG_R16, TCG_REG_R17, TCG_REG_R18, TCG_REG_R19, TCG_REG_R20, TCG_REG_R0,
    TCG_REG_R1,  TCG_REG_R2,  TCG_REG_R3,  TCG_REG_R4,  TCG_REG_R5,  TCG_REG_R6,  TCG_REG_R7,
    TCG_REG_R21, TCG_REG_R22, TCG_REG_R23, TCG_REG_R24, TCG_REG_R25, TCG_REG_R26, TCG_REG_R27,
};

//  Registers that can be used for input functions arguments
static const int tcg_target_call_iarg_regs[8] = {
    TCG_REG_R0, TCG_REG_R1, TCG_REG_R2, TCG_REG_R3, TCG_REG_R4, TCG_REG_R5, TCG_REG_R6, TCG_REG_R7,
};
static inline int tcg_target_get_call_iarg_regs_count(int flags)
{
    return 8;
}
//  Registers that can be used for output functions arguments
static const int tcg_target_call_oarg_regs[8] = {
    TCG_REG_R0, TCG_REG_R1, TCG_REG_R2, TCG_REG_R3, TCG_REG_R4, TCG_REG_R5, TCG_REG_R6, TCG_REG_R7,
};

//  Taken from the arm32 target
enum arm_cond_code_e {
    COND_EQ = 0x0,
    COND_NE = 0x1,
    COND_CS = 0x2, /* Unsigned greater or equal */
    COND_CC = 0x3, /* Unsigned less than */
    COND_MI = 0x4, /* Negative */
    COND_PL = 0x5, /* Zero or greater */
    COND_VS = 0x6, /* Overflow */
    COND_VC = 0x7, /* No overflow */
    COND_HI = 0x8, /* Unsigned greater than */
    COND_LS = 0x9, /* Unsigned less or equal */
    COND_GE = 0xa,
    COND_LT = 0xb,
    COND_GT = 0xc,
    COND_LE = 0xd,
    COND_AL = 0xe,
};
static const uint8_t tcg_cond_to_arm_cond[] = {
    [TCG_COND_EQ] = COND_EQ,
    [TCG_COND_NE] = COND_NE,
    [TCG_COND_LT] = COND_LT,
    [TCG_COND_GE] = COND_GE,
    [TCG_COND_LE] = COND_LE,
    [TCG_COND_GT] = COND_GT,
    /* unsigned */
    [TCG_COND_LTU] = COND_CC,
    [TCG_COND_GEU] = COND_CS,
    [TCG_COND_LEU] = COND_LS,
    [TCG_COND_GTU] = COND_HI,
};

//  Tells tcg what registers can be used for a argument with a certain flag
//  Flag are set in arm_op_defs later in this file, and are used if some
//  registers can not be used with a certain instruction
static int target_parse_constraint(TCGArgConstraint *ct, const char **pct_str)
{
    //  This is just taken from the arm target, might want to refactor to something cleaner
    const char *ct_str;
    ct_str = *pct_str;
    switch(ct_str[0]) {
        case 'r':
            //  Any general purpose register
            ct->ct |= TCG_CT_REG;
            tcg_regset_set(ct->u.regs, (1L << TCG_TARGET_GP_REGS) - 1);
            break;
        default:
            tcg_abortf("Constraint %c not implemented", ct_str[0]);
            return -1;
    }
    ct_str++;
    *pct_str = ct_str;

    return 0;
}
//  Tells tcg if a constant `val` can be used with the set arg constraints
//  Can be used to check if tcg should put a value as an immediate or
//  place it in a register before host code is generated,
//  allowing it to generate better code
static inline int tcg_target_const_match(tcg_target_long val, const TCGArgConstraint *arg_ct)
{
    //  TODO: Add constraints and checks for instructions to cut down on generated movi instructions
    int ct;
    ct = arg_ct->ct;
    if(ct & TCG_CT_CONST) {
        //  Register sized constant
        return 1;
    } else {
        return 0;
    }
}

//  Defines to match reloc names and ids from elf standard
#define R_AARCH64_JUMP26   282
#define R_AARCH64_CONDBR19 280
#define R_AARCH64_PREL32   261
static void reloc_pc32(void *code_ptr, tcg_target_long target)
{
    //  code_ptr should be set to target - current PC
    //  Adapted from arm32 target
    uint32_t offset = target - ((tcg_target_long)code_ptr + 8);
    *(uint32_t *)code_ptr = offset;
}
//  Patch the conditional branch instruction, address is 19-bits long
static void reloc_condbr_19(void *code_ptr, tcg_target_long target, int cond)
{
    //  code_ptr should have bits [23, 5] set to target - current PC, 4 to zero and [3, 0] to cond
    //  Its a bit of a hack to set cond this way, but tcg does not seem to offer a better solution
    uint32_t offset = target - ((tcg_target_long)code_ptr + 8);
    //  Mask out 19 bits from the offset
    *(uint32_t *)code_ptr |= ((offset & 0x7FFFF) << 5) | cond;
    //  set bit 4 to zero
    *(uint32_t *)code_ptr &= (~(1 << 4));
}
//  Path the unconditional branch instruction, address is 26-bits long
static void reloc_jump26(void *code_ptr, tcg_target_long target)
{
    //  code_ptr should have bits [25, 0] set to target - current PC
    uint32_t offset = target - ((tcg_target_long)code_ptr + 8);
    *(uint32_t *)code_ptr |= (offset & 0x3FFFFFF);
}
static void patch_reloc(uint8_t *code_ptr, int type, tcg_target_long value, tcg_target_long addend)
{
    switch(type) {
        case R_AARCH64_JUMP26:
            reloc_jump26(code_ptr, value);
            break;
        case R_AARCH64_CONDBR19:
            reloc_condbr_19(code_ptr, value, addend);
            break;
        case R_AARCH64_PREL32:
            /* PC-relative 32-bit.	*/
            reloc_pc32(code_ptr, value);
            break;
        default:
            tcg_abortf("patch reloc for type %i not implemented", type);
            break;
    }
}

static inline void tcg_out_br(TCGContext *s, int addr_reg)
{
    tcg_out32(s, 0xd61f0000 | (addr_reg << 5));
}
static inline void tcg_out_b(TCGContext *s, int offset)
{
    tcg_out32(s, 0x14000000 | (offset << 0));
}
static inline void tcg_out_bl(TCGContext *s, int offset)
{
    tcg_out32(s, 0x94000000 | (offset << 0));
}
static inline void tcg_out_blr(TCGContext *s, int reg)
{
    tcg_out32(s, 0xd63f0000 | (reg << 5));
}
static inline void tcg_out_ret(TCGContext *s, int reg)
{
    tcg_out32(s, 0xd65f0000 | (reg << 5));
}
static inline void tcg_out_b_noaddr(TCGContext *s)
{
    //  This sets up a unconditional branch to an adress that will be emited later
    //  by either translation block linking or a reloc
    s->code_ptr += 3;
    //  Unconditional branch opcode
    tcg_out8(s, 0b000101 << 2);
}

//  Value used for addend when nothing extra is needed by the reloc
#define TCG_UNUSED_CONSTANT 31337
static inline void tcg_out_goto_label(TCGContext *s, int cond, int label_index)
{
    TCGLabel *l = &s->labels[label_index];

    if(l->has_value) {
        //  Label has a target address so we just branch to it
        if(cond == COND_AL) {
            //  Unconditional branch
            tcg_out_movi(s, TCG_TYPE_PTR, TCG_TMP_REG, (tcg_target_long)rw_ptr_to_rx((void *)l->u.value));
            tcg_out_br(s, TCG_TMP_REG);
        } else {
            //  Conditional branch, takes 19-bit PC-relative offset
            int offset = l->u.value - (tcg_target_long)s->code_ptr;
            if(abs(offset) > 0x7FFFF) {
                tcg_abortf("Conditional branches further than %u not supported yet", 0x7FFFF);
            } else {
                //  Offset needs to be masked to 19-bits
                tcg_out32(s, 0x54000000 | ((0x7FFFF & offset) << 5) | (cond << 0));
            }
        }
    } else {
        //  Label does not have the address so we need a reloc
        //  reloc names are based on the ones from the elf format
        //  but might not have exacly the same semantics, see patch_reloc() for details
        if(cond == COND_AL) {
            //  Unconditional branch
            tcg_out_reloc(s, s->code_ptr, R_AARCH64_JUMP26, label_index, TCG_UNUSED_CONSTANT);
            tcg_out_b_noaddr(s);
        } else {
            tcg_out_reloc(s, s->code_ptr, R_AARCH64_CONDBR19, label_index,
                          cond);  //  Reloc needs to set the condition bits correctly
            s->code_ptr += 3;
            tcg_out8(s, 0b01010100);  //  Conditional branch opcode
        }
    }
}

//  Helper to generate function calls to constant address
static inline void tcg_out_calli(TCGContext *s, tcg_target_ulong addr)
{
    //  The target address can either be one we have generated, or somehting outside that.
    //  So we need to check if the target has to be translated
    tcg_target_ulong target;
    if(is_ptr_in_rw_buf((const void *)addr)) {
        target = (tcg_target_ulong)rw_ptr_to_rx((void *)addr);
    } else {
        target = addr;
    }
    tcg_out_movi(s, 0, TCG_TMP_REG, target);
    tcg_out_blr(s, TCG_TMP_REG);
}
//  Helper function to emit STP, store pair instructions with offset adressing mode (i.e no changing the base register)
static inline void tcg_out_stp(TCGContext *s, int reg1, int reg2, int reg_base, tcg_target_long offset)
{
    //  Offset needs to be a multiple of 8, it gets encoded as offset/8
    tlib_assert(offset % 8 == 0);
    //  Offset is 7 bits
    tcg_out32(s, 0xa9000000 | (((offset / 8) & 0x7f) << 15) | (reg2 << 10) | (reg_base << 5) | (reg1 << 0));
}

//  Helper function to emit LDP, load pair instructions with offset adressing mode (i.e no changing the base register)
static inline void tcg_out_ldp(TCGContext *s, int reg1, int reg2, int reg_base, tcg_target_long offset)
{
    //  Offset needs to be a multiple of 8, it gets encoded as offset/8
    tlib_assert(offset % 8 == 0);
    //  Offset is 7 bits
    tcg_out32(s, 0xa9400000 | (((offset / 8) & 0x7f) << 15) | (reg2 << 10) | (reg_base << 5) | (reg1 << 0));
}

static inline void tcg_out_mov(TCGContext *s, TCGType type, TCGReg ret, TCGReg arg)
{
    tcg_out32(s, 0xaa000000 | (arg << 16) | (0b11111 << 5) | (ret << 0));
}

static const int SHIFT_0 = 0b00;
static const int SHIFT_16 = 0b01;
static const int SHIFT_32 = 0b10;
static const int SHIFT_48 = 0b11;
static inline void tcg_out_movi64(TCGContext *s, int reg1, tcg_target_long imm)
{
    tcg_out32(s, 0xd2800000 | (SHIFT_0 << 21) | ((0xffff & imm) << 5) | (reg1 << 0));  //  MOVZ
    imm = imm >> 16;
    tcg_out32(s, 0xf2800000 | (SHIFT_16 << 21) | ((0xffff & imm) << 5) | (reg1 << 0));  //  MOVK
    imm = imm >> 16;
    tcg_out32(s, 0xf2800000 | (SHIFT_32 << 21) | ((0xffff & imm) << 5) | (reg1 << 0));  //  MOVK
    imm = imm >> 16;
    tcg_out32(s, 0xf2800000 | (SHIFT_48 << 21) | ((0xffff & imm) << 5) | (reg1 << 0));  //  MOVK
}
static inline void tcg_out_movi32(TCGContext *s, int reg1, tcg_target_long imm)
{
    tcg_out32(s, 0x52800000 | (SHIFT_0 << 21) | ((0xffff & imm) << 5) | (reg1 << 0));  //  MOVZ
    imm = imm >> 16;
    tcg_out32(s, 0x72800000 | (SHIFT_16 << 21) | ((0xffff & imm) << 5) | (reg1 << 0));  //  MOVK
}

//  Fills reg_dest with nbit of data from reg_base + offset_reg
static inline void tcg_out_ld_reg_offset(TCGContext *s, int bits, int reg_dest, int reg_base, int offset_reg)
{
    switch(bits) {
        case 32:
            tcg_out32(s, 0x78600800 | (offset_reg << 16) | (0b011 << 13) | (reg_base << 5) | (reg_dest << 0));
            break;
        case 64:
            tcg_out32(s, 0xf8600800 | (offset_reg << 16) | (0b011 << 13) | (reg_base << 5) | (reg_dest << 0));
            break;
        default:
            tcg_abortf("ld %s bits wide not implemented", bits);
            break;
    }
}

static inline void tcg_out_ld_offset(TCGContext *s, int bits, int reg_dest, int reg_base, tcg_target_long offset)
{
    tcg_out_movi64(s, TCG_TMP_REG, offset);
    tcg_out_ld_reg_offset(s, bits, reg_dest, reg_base, TCG_TMP_REG);
}

//  Read 64-bits from base + offset to dest
static inline void tcg_out_ld(TCGContext *s, TCGType type, TCGReg dest, TCGReg base, tcg_target_long offset)
{
    tcg_out_movi64(s, TCG_TMP_REG, offset);
    tcg_out_ld_reg_offset(s, 64, dest, base, TCG_TMP_REG);
}

//  Stores n-bits of data from reg_src to reg_base + offset_reg
static inline void tcg_out_st_reg_offset(TCGContext *s, int bits, int reg_src, int reg_base, int offset_reg)
{
    switch(bits) {
        case 32:
            tcg_out32(s, 0x78200800 | (offset_reg << 16) | (0b011 << 13) | (reg_base << 5) | (reg_src << 0));
            break;
        case 64:
            //  The constant is to set some needed flags
            tcg_out32(s, 0xf8200800 | (offset_reg << 16) | (0b011 << 13) | (reg_base << 5) | (reg_src << 0));
            break;
        default:
            tcg_abortf("st %s bits wide not implemented", bits);
            break;
    }
}
static inline void tcg_out_st_offset(TCGContext *s, int bits, int reg_src, int reg_base, tcg_target_long offset)
{
    tcg_out_movi64(s, TCG_TMP_REG, offset);
    tcg_out_st_reg_offset(s, bits, reg_src, reg_base, TCG_TMP_REG);
}

static inline void tcg_out_st(TCGContext *s, TCGType type, TCGReg arg, TCGReg arg1, tcg_target_long offset)
{
    //  Write the content of arg to the address in arg1 + offset
    //  For offsets that fit in 9-bits this could be one instruction, future optimization work

    //  Move offset into designated tmp reg
    tcg_out_movi64(s, TCG_TMP_REG, offset);
    tcg_out_st_reg_offset(s, 64, arg, arg1, TCG_TMP_REG);
}

static inline void tcg_out_movi(TCGContext *s, TCGType type, TCGReg ret, tcg_target_long arg)
{
    //  TODO: Optimize this in case the immidiate fits in less than 64-bits
    tcg_out_movi64(s, ret, arg);
}

//  Helper function to emit SUB, with immediate
static inline void tcg_out_subi(TCGContext *s, int reg1, int reg2, tcg_target_long imm)
{
    tcg_out32(s, 0xd1000000 | (imm << 10) | (reg2 << 5) | (reg1 << 0));
}
//  Helper function to emit ADD, with immediate
static inline void tcg_out_addi(TCGContext *s, int reg1, int reg2, tcg_target_long imm)
{
    tcg_out32(s, 0x91000000 | (imm << 10) | (reg2 << 5) | (reg1 << 0));
}

static inline void tcg_out_add_reg(TCGContext *s, int bits, int reg_dest, int reg1, int reg2)
{
    switch(bits) {
        case 32:
            tcg_out32(s, 0x0b200000 | (reg2 << 16) | (reg1 << 5) | (reg_dest << 0));
            break;
        case 64:
            tcg_out32(s, 0x8b200000 | (reg2 << 16) | (reg1 << 5) | (reg_dest << 0));
            break;
        default:
            tcg_abortf("add_reg called with unsupported bit width: %i", bits);
    }
}
static inline void tcg_out_add_imm(TCGContext *s, int bits, int reg_dest, int reg_in, tcg_target_long imm)
{
    switch(bits) {
        case 32:
            tcg_out_movi32(s, TCG_TMP_REG, imm);
            break;
        case 64:
            tcg_out_movi64(s, TCG_TMP_REG, imm);
            break;
        default:
            tcg_abortf("add_imm for %i bits not implemented", bits);
    }
    tcg_out_add_reg(s, bits, reg_dest, reg_in, TCG_TMP_REG);
}

static inline void tcg_out_and_reg(TCGContext *s, int bits, int reg_dest, int reg1, int reg2)
{
    switch(bits) {
        case 32:
            tcg_out32(s, 0x0a000000 | (reg2 << 16) | (reg1 << 5) | (reg_dest << 0));
            break;
        case 64:
            tcg_out32(s, 0x8a000000 | (reg2 << 16) | (reg1 << 5) | (reg_dest << 0));
            break;
        default:
            tcg_abortf("and_reg called with unsupported bit width: %i", bits);
    }
}
static inline void tcg_out_and_imm(TCGContext *s, int bits, int reg_dest, int reg_in, tcg_target_long imm)
{
    switch(bits) {
        case 32:
            tcg_out_movi32(s, TCG_TMP_REG, imm);
            break;
        case 64:
            tcg_out_movi64(s, TCG_TMP_REG, imm);
            break;
        default:
            tcg_abortf("and_imm for %i bits not implemented", bits);
    }
    tcg_out_and_reg(s, bits, reg_dest, reg_in, TCG_TMP_REG);
}
static inline void tcg_out_cmp(TCGContext *s, int reg1, int reg2)
{
    //  CMP is actually just subs with the zero register as the destination
    tcg_out32(s, 0xeb000000 | (reg2 << 16) | (reg1 << 5) | (TCG_REG_RZR << 0));
}
static inline void tcg_out_cmpi(TCGContext *s, int reg, tcg_target_long imm)
{
    //  Mov immediate into a register
    tcg_out_movi(s, 0, TCG_TMP_REG, imm);
    tcg_out_cmp(s, reg, TCG_TMP_REG);
}
//  Helper for tcg's conditional branch. branch if arg1 COND arg2 == true
static inline void tcg_out_br_cond(TCGContext *s, int arg1, int tcg_cond, int arg2, int addr)
{
    tcg_out_cmp(s, arg1, arg2);
    tcg_out32(s, 0x54000000 | (addr << 5) | (tcg_cond_to_arm_cond[tcg_cond] << 0));
}
//  Version of above but with an immediate as the second argument
static inline void tcg_out_br_condi(TCGContext *s, int arg1, int tcg_cond, int imm, int addr)
{
    tcg_out_cmpi(s, arg1, imm);
    tcg_out32(s, 0x54000000 | (addr << 5) | (tcg_cond_to_arm_cond[tcg_cond] << 0));
}

//  Variable used to store the return address. Used in INDEX_op_exit_tb
static uint8_t *tb_ret_addr;

static inline void tcg_out_op(TCGContext *s, TCGOpcode opc, const TCGArg *args, const int *const_args)
{
    //  args contains actual arguments, const_args holds flags indicating if the argument is a constant
    //  const_args[n] == true => args[n] is a constant, otherwise it is a register
    switch(opc) {
        case INDEX_op_exit_tb:
            tcg_out_movi(s, TCG_TYPE_PTR, TCG_REG_R0, args[0]);  //  Put return value in output register
            tcg_out_movi(s, TCG_TYPE_PTR, TCG_TMP_REG, (tcg_target_ulong)tb_ret_addr);
            tcg_out_br(s, TCG_TMP_REG);
            break;
        case INDEX_op_goto_tb:
            if(s->tb_jmp_offset) {
                //  Direct jump
                //  The branch target will be written later during tb linking
                s->tb_jmp_offset[args[0]] = s->code_ptr - s->code_buf;
                tcg_out_b_noaddr(s);

            } else {
                //  Indirect jump
                tcg_abortf("op_goto_tb indirect jump not implemented");
            }
            s->tb_next_offset[args[0]] = s->code_ptr - s->code_buf;
            break;
        case INDEX_op_call:
            if(const_args[0]) {
                //  Target function adress is an immediate
                tcg_out_calli(s, args[0]);
            } else {
                //  Register target we can just branch and link directly
                tcg_out_blr(s, args[0]);
            }
            break;
        case INDEX_op_jmp:
            tcg_abortf("op_jmp not implemented");
            break;
        case INDEX_op_br:
            //  Unconditional branch to label
            tcg_out_goto_label(s, COND_AL, args[0]);
            break;
        case INDEX_op_mb:
            tcg_abortf("op_mp not implemented");
            break;
        case INDEX_op_ld8u_i32:
            tcg_abortf("op_ld8u_i32 not implemented");
            break;
        case INDEX_op_qemu_ld8u:
            tcg_abortf("op_ld8u not implemented");
            break;
        case INDEX_op_ld16s_i32:
            tcg_abortf("op_ld16s_i32 not implemented");
            break;
        case INDEX_op_ld_i32:
            tcg_abortf("op_ld_i32 not implemented");
            break;
        case INDEX_op_st8_i32:
            tcg_abortf("op_st8_i32 not implemented");
            break;
        case INDEX_op_st16_i32:
            tcg_abortf("op_st16_i32 not implemented");
            break;
        case INDEX_op_st_i32:
            tcg_abortf("op_st_i32 not implemented");
            break;
        case INDEX_op_mov_i32:
            tcg_abortf("op_mov_i32 not implemented");
            break;
        case INDEX_op_movi_i32:
            tcg_abortf("op_movi_i32 not implemented");
            break;
        case INDEX_op_add_i32:
            if(const_args[2]) {
                //  Add with immediate
                tcg_out_add_imm(s, 32, args[0], args[1], args[2]);
            } else {
                //  Add with registers
                tcg_out_add_reg(s, 32, args[0], args[1], args[2]);
            }
            break;
        case INDEX_op_sub_i32:
            tcg_abortf("op_sub_i32 not implemented");
            break;
        case INDEX_op_and_i32:
            if(const_args[2]) {
                //  And with immediate
                tcg_out_and_imm(s, 32, args[0], args[1], args[2]);
            } else {
                //  And with registers
                tcg_out_and_reg(s, 32, args[0], args[1], args[2]);
            }
            break;
        case INDEX_op_andc_i32:
            tcg_abortf("op_andc_i32 not implemented");
            break;
        case INDEX_op_or_i32:
            tcg_abortf("op_or_i32 not implemented");
            break;
        case INDEX_op_xor_i32:
            tcg_abortf("op_xor_i32 not implemented");
            break;
        case INDEX_op_neg_i32:
            tcg_abortf("op_neg_i32 not implemented");
            break;
        case INDEX_op_not_i32:
            tcg_abortf("op_not_i32 not implemented");
            break;
        case INDEX_op_mul_i32:
            tcg_abortf("op_mul_i32 not implemented");
            break;
        case INDEX_op_mulu2_i32:
            tcg_abortf("op_mulu2_i32 not implemented");
            break;
        case INDEX_op_muls2_i32:
            tcg_abortf("op_muls2_i32 not implemented");
            break;
        case INDEX_op_shl_i32:
            tcg_abortf("op_shl_i32 not implemented");
            break;
        case INDEX_op_shr_i32:
            tcg_abortf("op_shr_i32 not implemented");
            break;
        case INDEX_op_sar_i32:
            tcg_abortf("op_sar_i32 not implemented");
            break;
        case INDEX_op_rotr_i32:
            tcg_abortf("op_rotr_i32 not implemented");
            break;
        case INDEX_op_rotl_i32:
            tcg_abortf("op_rotl_i32 not implemented");
            break;
        case INDEX_op_brcond_i32:
            if(const_args[1]) {
                //  Second arg is an immediate
                tcg_out_cmpi(s, args[0], args[1]);
            } else {
                //  Second arg is a register
                tcg_out_cmp(s, args[0], args[1]);
            }
            tcg_out_goto_label(s, tcg_cond_to_arm_cond[args[2]], args[3]);
            break;
        case INDEX_op_brcond2_i32:
            tcg_abortf("op_brcond2_i32 not implemented");
            break;
        case INDEX_op_setcond_i32:
            tcg_abortf("op_setcond_i32 not implemented");
            break;
        case INDEX_op_setcond2_i32:
            tcg_abortf("op_setcond2_i32 not implemented");
            break;
        case INDEX_op_qemu_ld8s:
            tcg_abortf("op_qemu_ld8s not implemented");
            break;
        case INDEX_op_qemu_ld16u:
            tcg_abortf("op_qemu_ld16u not implemented");
            break;
        case INDEX_op_qemu_ld16s:
            tcg_abortf("op_qemu_ld16s not implemented");
            break;
        case INDEX_op_qemu_ld32:
            tcg_abortf("op_qemu_ld32 not implemented");
            break;
        case INDEX_op_qemu_ld64:
            tcg_abortf("op_qemu_ld64 not implemented");
            break;
        case INDEX_op_qemu_st8:
            tcg_abortf("op_qemu_st8 not implemented");
            break;
        case INDEX_op_qemu_st16:
            tcg_abortf("op_qemu_st16 not implemented");
            break;
        case INDEX_op_qemu_st32:
            tcg_abortf("op_qemu_st32 not implemented");
            break;
        case INDEX_op_qemu_st64:
            tcg_abortf("op_qemu_st64 not implemented");
            break;
        case INDEX_op_bswap16_i32:
            tcg_abortf("op_bswap16_i32 not implemented");
            break;
        case INDEX_op_bswap32_i32:
            tcg_abortf("op_bswap32_i32 not implemented");
            break;
        case INDEX_op_ext8s_i32:
            tcg_abortf("op_ext8s_i32 not implemented");
            break;
        case INDEX_op_ext16s_i32:
            tcg_abortf("op_ext16s_i32 not implemented");
            break;
        case INDEX_op_ext16u_i32:
            tcg_abortf("op_ext16u_i32 not implemented");
            break;
        case INDEX_op_ld32u_i64:
            tcg_out_ld_offset(s, 32, args[0], args[1], args[2]);
            break;
        case INDEX_op_ld_i64:
            tcg_out_ld_offset(s, 64, args[0], args[1], args[2]);
            break;
        case INDEX_op_st32_i64:
            tcg_out_st_offset(s, 32, args[0], args[1], args[2]);
            break;
        case INDEX_op_st_i64:
            tcg_out_st_offset(s, 64, args[0], args[1], args[2]);
            break;
        case INDEX_op_add_i64:
            if(const_args[2]) {
                //  Add with immediate
                tcg_out_add_imm(s, 64, args[0], args[1], args[2]);
            } else {
                //  Add with registers
                tcg_out_add_reg(s, 64, args[0], args[1], args[2]);
            }
            break;
        case INDEX_op_sub_i64:
            tcg_abortf("op_sub_i64 not implemented");
            break;
        default:
            tcg_abortf("TCGOpcode %u not implemented", opc);
    }
}

//  Used by parse_constraints and const_match to help tcg select apropriate registers
//  and to determine what immediates fit in instructions
static const TCGTargetOpDef arm_op_defs[] = {
    { INDEX_op_exit_tb, {} },
    { INDEX_op_goto_tb, {} },
    { INDEX_op_call, { "ri" } },

    { INDEX_op_mov_i32, { "r", "r" } },

    { INDEX_op_brcond_i32, { "r", "r" } },

    { INDEX_op_ld32u_i64, { "r", "r" } },
    { INDEX_op_ld_i64, { "r", "r" } },

    { INDEX_op_st32_i64, { "r", "r" } },
    { INDEX_op_st_i64, { "r", "r" } },

    { INDEX_op_add_i64, { "r", "r", "r" } },
    { INDEX_op_sub_i64, { "r", "r", "r" } },

    { INDEX_op_add_i32, { "r", "r", "r" } },
    { INDEX_op_sub_i32, { "r", "r", "r" } },
    { INDEX_op_mul_i32, { "r", "r", "r" } },
    { INDEX_op_and_i32, { "r", "r", "r" } },

    { -1 },
};

static void tcg_target_init(TCGContext *s)
{
    /* fail safe */ if((1 << CPU_TLB_ENTRY_BITS) != sizeof_CPUTLBEntry) { tcg_abort(); }

    tcg_regset_set32(tcg_target_available_regs[TCG_TYPE_I32], 0, 0xffff);
    tcg_regset_set32(tcg_target_available_regs[TCG_TYPE_I64], 0, 0xffff);
    //  Set wich registers can get clobbered by a function call
    tcg_regset_set32(tcg_target_call_clobber_regs, 0,
                     //  Parameter and results registers
                     (1 << TCG_REG_R0) | (1 << TCG_REG_R1) | (1 << TCG_REG_R2) | (1 << TCG_REG_R3) | (1 << TCG_REG_R4) |
                         (1 << TCG_REG_R5) | (1 << TCG_REG_R6) | (1 << TCG_REG_R7) |
                         //  Indirect result registers
                         (1 << TCG_REG_R8) |
                         //  Corruptable registers
                         (1 << TCG_REG_R9) | (1 << TCG_REG_R10) | (1 << TCG_REG_R11) | (1 << TCG_REG_R12) | (1 << TCG_REG_R13) |
                         (1 << TCG_REG_R14) | (1 << TCG_REG_R15) |
                         //  Intra-procedure-call corruptable registers
                         (1 << TCG_REG_R16) | (1 << TCG_REG_R17) | (1 << TCG_REG_R18) |
                         //  Frame pointer
                         (1 << TCG_REG_R29) |
                         //  Link register
                         (1 << TCG_REG_R30));

    //  Setup reserved registers
    tcg_regset_clear(s->reserved_regs);
    tcg_regset_set_reg(s->reserved_regs, TCG_REG_CALL_STACK);  //  SP
    tcg_regset_set_reg(s->reserved_regs, TCG_TMP_REG);         //  R28 to use as an intermediate
    tcg_regset_set_reg(s->reserved_regs, TCG_REG_PC);          //  PC

    tcg_add_target_add_op_defs(arm_op_defs);
    //  temp_buf_offset is set by init_tcg(), here used to find the start of the tcg frame
    tcg_set_frame(s, TCG_AREG0, temp_buf_offset, CPU_TEMP_BUF_NLONGS * sizeof(long));
}

static void tcg_target_qemu_prologue(TCGContext *s)
{
    //  Prologue

    //  The ARMv8 calling conventions allow compilers to put temporary values
    //  up to 16 bytes BELOW the current stack pointer, the so called red zone
    //  Normally the compiler will not do this for external calls, but the jump to generated
    //  code is not treated as an external function call so there might be data we must preserve there
    //  To do this we first move the SP 16 bytes down before doing anything else
    tcg_out_subi(s, TCG_REG_SP, TCG_REG_SP, 16);
    //  Make space on the stack for callee saved registers
    tcg_out_subi(s, TCG_REG_SP, TCG_REG_SP, 80);
    //  aarch64 calling conventions needs us to save R19-R30
    tcg_out_stp(s, TCG_REG_R19, TCG_REG_R20, TCG_REG_SP, 0);
    tcg_out_stp(s, TCG_REG_R21, TCG_REG_R22, TCG_REG_SP, 16);
    tcg_out_stp(s, TCG_REG_R23, TCG_REG_R24, TCG_REG_SP, 32);
    tcg_out_stp(s, TCG_REG_R25, TCG_REG_R26, TCG_REG_SP, 48);
    tcg_out_stp(s, TCG_REG_R27, TCG_REG_R28, TCG_REG_SP, 64);
    tcg_out_stp(s, TCG_REG_R29, TCG_REG_R30, TCG_REG_SP, 80);

    //  Branch to code
    tcg_out_mov(s, TCG_TYPE_PTR, TCG_AREG0, tcg_target_call_iarg_regs[0]);
    tcg_out_br(s, tcg_target_call_iarg_regs[1]);

    //  Epilogue
    tb_ret_addr = rw_ptr_to_rx(s->code_ptr);
    //  Load all the registers we saved above to restore system state
    tcg_out_ldp(s, TCG_REG_R29, TCG_REG_R30, TCG_REG_SP, 80);
    tcg_out_ldp(s, TCG_REG_R27, TCG_REG_R28, TCG_REG_SP, 64);
    tcg_out_ldp(s, TCG_REG_R25, TCG_REG_R26, TCG_REG_SP, 48);
    tcg_out_ldp(s, TCG_REG_R23, TCG_REG_R24, TCG_REG_SP, 32);
    tcg_out_ldp(s, TCG_REG_R21, TCG_REG_R22, TCG_REG_SP, 16);
    tcg_out_ldp(s, TCG_REG_R19, TCG_REG_R20, TCG_REG_SP, 0);
    tcg_out_addi(s, TCG_REG_SP, TCG_REG_SP, 80);  //  Pop the stack
    //  Restore the SP above the possible temporary values
    tcg_out_addi(s, TCG_REG_SP, TCG_REG_SP, 16);
    //  Return based on the link address
    tcg_out_ret(s, TCG_REG_R30);
}

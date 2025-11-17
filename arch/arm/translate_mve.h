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

typedef void MVEGenLdStIlFn(DisasContext *, uint32_t, TCGv_i32);

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

/* Extract arguments of (de)interleaving stores/loads */
static void extract_arg_vldst_il(arg_vldst_il *a, uint32_t insn)
{
    a->qd = extract32(insn, 13, 3);
    a->rn = extract32(insn, 16, 4);
    a->size = extract32(insn, 7, 2);
    a->pat = extract32(insn, 5, 2);
    a->w = extract32(insn, 21, 1);
}

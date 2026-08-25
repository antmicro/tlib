/*
 * Copyright (c) 2018, Max Filippov, Open Source and Linux Lab.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Open Source and Linux Lab nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "osdep.h"
#include "cpu.h"

#include "core-esp32s3/core-isa.h"
#include "overlay_tool.h"

#define xtensa_modules xtensa_modules_esp32s3

//  use the common implementation of ESP32
#include "core-esp32/xtensa-modules.c.inc"

/*
 * ESP32S3 has a single-precision FPU (XCHAL_HAVE_FP is 1 in
 * core-esp32s3/core-isa.h, together with FP_DIV, FP_SQRT, FP_RECIP and
 * FP_RSQRT). Without an .opcode_translators list, no floating point opcode
 * table is attached to this configuration: opcode_ops[opc] stays NULL and
 * every FP instruction fails with "unimplemented opcode 'lsi'".
 *
 * xtensa_fpu_opcodes is the right table rather than xtensa_fpu2000_opcodes:
 * the lsip/ssip forms emitted by the compiler only exist in the former, and
 * FPU2000 has neither divide nor square root, which this core advertises.
 */
XtensaConfig esp32s3 __attribute__((unused)) = {
    .name = "esp32s3",
    .isa_internal = &xtensa_modules,
    .clock_freq_khz = 140000,
    .opcode_translators =
        (const XtensaOpcodeTranslators *[]) {
                                             &xtensa_core_opcodes,
                                             &xtensa_fpu_opcodes,
                                             NULL, },
    DEFAULT_SECTIONS
};

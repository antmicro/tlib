/*
 * Copyright (c) Antmicro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arch_exports.h"
#include "cpu.h"
#include "unwind.h"

void tlib_set_cs_descriptor(uint32_t selector, uint32_t base, uint32_t limit, uint32_t flags)
{
    cpu_x86_load_seg_cache(env, R_CS, selector, base, limit, flags);
}

EXC_VOID_4(tlib_set_cs_descriptor, uint32_t, selector, uint32_t, base, uint32_t, limit, uint32_t, flags)

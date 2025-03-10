/*
 * Xtensa CPU
 *
 * Copyright (c) 2011, Max Filippov, Open Source and Linux Lab.
 * Copyright (c) 2012 SUSE LINUX Products GmbH
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

#include "cpu.h"
#include "osdep.h"
#include "softfloat-2.h"

static void xtensa_cpu_reset(CPUState *env)
{
    /* Reset the CPU_COMMON part */
    int common_offset = offsetof(CPUState, cpu_common_first_field);
    memset((char *)env + common_offset, 0, RESET_OFFSET - common_offset);
    env->exception_index = -1;

    /* Reset Xtensa-specific parts */

    bool dfpu = xtensa_option_enabled(env->config, XTENSA_OPTION_DFP_COPROCESSOR);

    env->exception_taken = 0;
    env->pc = env->config->exception_vector[EXC_RESET0];
    env->sregs[LITBASE] &= ~1;
    env->sregs[PS] = xtensa_option_enabled(env->config, XTENSA_OPTION_INTERRUPT) ? 0x1f : 0x10;
    env->pending_irq_level = 0;
    env->sregs[VECBASE] = env->config->vecbase;
    env->sregs[IBREAKENABLE] = 0;
    env->sregs[MEMCTL] = MEMCTL_IL0EN & env->config->memctl_mask;
    env->sregs[ATOMCTL] = xtensa_option_enabled(env->config, XTENSA_OPTION_ATOMCTL) ? 0x28 : 0x15;
    env->sregs[CONFIGID0] = env->config->configid[0];
    env->sregs[CONFIGID1] = env->config->configid[1];
    env->exclusive_addr = -1;

    reset_mmu(env);
    set_no_signaling_nans(!dfpu, &env->fp_status);
    set_use_first_nan(!dfpu, &env->fp_status);
}

void cpu_reset(CPUState *env)
{
    xtensa_cpu_reset(env);
}

static void xtensa_cpu_initfn(CPUState *env, XtensaConfig *config)
{
    env->config = config;

    pthread_mutex_init(&env->io_lock, NULL);
}

int cpu_init(const char *cpu_model)
{
    XtensaConfig *config = xtensa_finalize_config(cpu_model);
    //  Has to be run after 'xtensa_finalize_config' which calls
    //  'xtensa_collect_sr_names' required to initialize cpu_SR array.
    xtensa_translate_init();
    xtensa_cpu_initfn(cpu, config);
    cpu_reset(cpu);
    return 0;
}

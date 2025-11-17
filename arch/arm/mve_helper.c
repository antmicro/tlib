/*
 *  ARM helper functions for M-Profile Vector Extension (MVE)
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

#ifdef TARGET_PROTO_ARM_M

#include "cpu.h"
#include "helper.h"

static uint16_t mve_eci_mask(CPUState *env)
{
    /*
     * Return the mask of which elements in the MVE vector correspond
     * to beats being executed. The mask has 1 bits for executed lanes
     * and 0 bits where ECI says this beat was already executed.
     */
    uint32_t eci = env->condexec_bits;

    if((eci & 0xf) != 0) {
        return 0xffff;
    }

    switch(eci >> 4) {
        case ECI_NONE:
            return 0xffff;
        case ECI_A0:
            return 0xfff0;
        case ECI_A0A1:
            return 0xff00;
        case ECI_A0A1A2:
        case ECI_A0A1A2B0:
            return 0xf000;
        default:
            g_assert_not_reached();
    }
}

#endif

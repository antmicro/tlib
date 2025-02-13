/*
 * ARM cpu parameters.
 *
 * Copyright (c) 2003 Fabrice Bellard
 * SPDX-License-Identifier: LGPL-2.0+
 */

#pragma once

#ifdef TARGET_AARCH64
#define TARGET_PHYS_ADDR_SPACE_BITS 52
#define TARGET_VIRT_ADDR_SPACE_BITS 52
#else
#define TARGET_PHYS_ADDR_SPACE_BITS 40
#define TARGET_VIRT_ADDR_SPACE_BITS 32
#endif

#define TARGET_PAGE_BITS 12  //  4k

#define NB_MMU_MODES 15

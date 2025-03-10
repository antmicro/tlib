/*
 *  PowerPC emulation helpers for qemu.
 *
 *  Copyright (c) 2003-2007 Jocelyn Mayer
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
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "cpu.h"
#include "helper_regs.h"
#include "infrastructure.h"
#include "arch_callbacks.h"

/*****************************************************************************/
/* PowerPC Hypercall emulation */

void (*cpu_ppc_hypercall)(CPUState *);

/*****************************************************************************/
/* PowerPC MMU emulation */

/* Common routines used by software and hardware TLBs emulation */
static inline int pte_is_valid(target_ulong pte0)
{
    return pte0 & 0x80000000 ? 1 : 0;
}

static inline void pte_invalidate(target_ulong *pte0)
{
    *pte0 &= ~0x80000000;
}

#if defined(TARGET_PPC64)
static inline int pte64_is_valid(target_ulong pte0)
{
    return pte0 & 0x0000000000000001ULL ? 1 : 0;
}

__attribute__((unused)) static inline void pte64_invalidate(target_ulong *pte0)
{
    *pte0 &= ~0x0000000000000001ULL;
}
#endif

#define PTE_PTEM_MASK  0x7FFFFFBF
#define PTE_CHECK_MASK (target_ulong)(TARGET_PAGE_MASK | 0x7B)
#if defined(TARGET_PPC64)
#define PTE64_PTEM_MASK  0xFFFFFFFFFFFFFF80ULL
#define PTE64_CHECK_MASK (TARGET_PAGE_MASK | 0x7F)
#endif

static inline int pp_check(int key, int pp, int nx)
{
    int access;

    /* Compute access rights */
    /* When pp is 3/7, the result is undefined. Set it to noaccess */
    access = 0;
    if(key == 0) {
        switch(pp) {
            case 0x0:
            case 0x1:
            case 0x2:
                access |= PAGE_WRITE;
                /* No break here */
                goto case_0x6;
            case 0x3:
            case 0x6:
            case_0x6:
                access |= PAGE_READ;
                break;
        }
    } else {
        switch(pp) {
            case 0x0:
            case 0x6:
                access = 0;
                break;
            case 0x1:
            case 0x3:
                access = PAGE_READ;
                break;
            case 0x2:
                access = PAGE_READ | PAGE_WRITE;
                break;
        }
    }
    if(nx == 0) {
        access |= PAGE_EXEC;
    }

    return access;
}

static inline int check_prot(int prot, int rw, int access_type)
{
    int ret;

    if(access_type == ACCESS_CODE) {
        if(prot & PAGE_EXEC) {
            ret = 0;
        } else {
            ret = -2;
        }
    } else if(rw) {
        if(prot & PAGE_WRITE) {
            ret = 0;
        } else {
            ret = -2;
        }
    } else {
        if(prot & PAGE_READ) {
            ret = 0;
        } else {
            ret = -2;
        }
    }

    return ret;
}

static inline int _pte_check(mmu_ctx_t *mmu_ctx, int is_64b, target_ulong pte0, target_ulong pte1, int h, int rw, int type)
{
    target_ulong ptem, mmask;
    int access, ret, pteh, ptev, pp;

    ret = -1;
    /* Check validity and table match */
#if defined(TARGET_PPC64)
    if(is_64b) {
        ptev = pte64_is_valid(pte0);
        pteh = (pte0 >> 1) & 1;
    } else
#endif
    {
        ptev = pte_is_valid(pte0);
        pteh = (pte0 >> 6) & 1;
    }
    if(ptev && h == pteh) {
        /* Check vsid & api */
#if defined(TARGET_PPC64)
        if(is_64b) {
            ptem = pte0 & PTE64_PTEM_MASK;
            mmask = PTE64_CHECK_MASK;
            pp = (pte1 & 0x00000003) | ((pte1 >> 61) & 0x00000004);
            mmu_ctx->nx = (pte1 >> 2) & 1;  /* No execute bit */
            mmu_ctx->nx |= (pte1 >> 3) & 1; /* Guarded bit    */
        } else
#endif
        {
            ptem = pte0 & PTE_PTEM_MASK;
            mmask = PTE_CHECK_MASK;
            pp = pte1 & 0x00000003;
        }
        if(ptem == mmu_ctx->ptem) {
            if(mmu_ctx->raddr != (target_phys_addr_t)-1ULL) {
                /* all matches should have equal RPN, WIMG & PP */
                if((mmu_ctx->raddr & mmask) != (pte1 & mmask)) {
                    return -3;
                }
            }
            /* Compute access rights */
            access = pp_check(mmu_ctx->key, pp, mmu_ctx->nx);
            /* Keep the matching PTE informations */
            mmu_ctx->raddr = pte1;
            mmu_ctx->prot = access;
            ret = check_prot(mmu_ctx->prot, rw, type);
        }
    }

    return ret;
}

static inline int pte32_check(mmu_ctx_t *mmu_ctx, target_ulong pte0, target_ulong pte1, int h, int rw, int type)
{
    return _pte_check(mmu_ctx, 0, pte0, pte1, h, rw, type);
}

#if defined(TARGET_PPC64)
static inline int pte64_check(mmu_ctx_t *ctx, target_ulong pte0, target_ulong pte1, int h, int rw, int type)
{
    return _pte_check(ctx, 1, pte0, pte1, h, rw, type);
}
#endif

static inline int pte_update_flags(mmu_ctx_t *mmu_ctx, target_ulong *pte1p, int ret, int rw)
{
    int store = 0;

    /* Update page flags */
    if(!(*pte1p & 0x00000100)) {
        /* Update accessed flag */
        *pte1p |= 0x00000100;
        store = 1;
    }
    if(!(*pte1p & 0x00000080)) {
        if(rw == 1 && ret == 0) {
            /* Update changed flag */
            *pte1p |= 0x00000080;
            store = 1;
        } else {
            /* Force page fault for first write access */
            mmu_ctx->prot &= ~PAGE_WRITE;
        }
    }

    return store;
}

/* Software driven TLB helpers */
static inline int ppc6xx_tlb_getnum(CPUState *env, target_ulong eaddr, int way, int is_code)
{
    int nr;

    /* Select TLB num in a way from address */
    nr = (eaddr >> TARGET_PAGE_BITS) & (env->tlb_per_way - 1);
    /* Select TLB way */
    nr += env->tlb_per_way * way;
    /* 6xx have separate TLBs for instructions and data */
    if(is_code && env->id_tlbs == 1) {
        nr += env->nb_tlb;
    }

    return nr;
}

static inline void ppc6xx_tlb_invalidate_all(CPUState *env)
{
    ppc6xx_tlb_t *tlb;
    int nr, max;

    /* Invalidate all defined software TLB */
    max = env->nb_tlb;
    if(env->id_tlbs == 1) {
        max *= 2;
    }
    for(nr = 0; nr < max; nr++) {
        tlb = &env->tlb.tlb6[nr];
        pte_invalidate(&tlb->pte0);
    }
    tlb_flush(env, 1, true);
}

static inline void __ppc6xx_tlb_invalidate_virt(CPUState *env, target_ulong eaddr, int is_code, int match_epn)
{
    ppc6xx_tlb_t *tlb;
    int way, nr;

    /* Invalidate ITLB + DTLB, all ways */
    for(way = 0; way < env->nb_ways; way++) {
        nr = ppc6xx_tlb_getnum(env, eaddr, way, is_code);
        tlb = &env->tlb.tlb6[nr];
        if(pte_is_valid(tlb->pte0) && (match_epn == 0 || eaddr == tlb->EPN)) {
            pte_invalidate(&tlb->pte0);
            tlb_flush_page(env, tlb->EPN, true);
        }
    }
}

static inline void ppc6xx_tlb_invalidate_virt(CPUState *env, target_ulong eaddr, int is_code)
{
    __ppc6xx_tlb_invalidate_virt(env, eaddr, is_code, 0);
}

void ppc6xx_tlb_store(CPUState *env, target_ulong EPN, int way, int is_code, target_ulong pte0, target_ulong pte1)
{
    ppc6xx_tlb_t *tlb;
    int nr;

    nr = ppc6xx_tlb_getnum(env, EPN, way, is_code);
    tlb = &env->tlb.tlb6[nr];
    /* Invalidate any pending reference in Qemu for this virtual address */
    __ppc6xx_tlb_invalidate_virt(env, EPN, is_code, 1);
    tlb->pte0 = pte0;
    tlb->pte1 = pte1;
    tlb->EPN = EPN;
    /* Store last way for LRU mechanism */
    env->last_way = way;
}

static inline int ppc6xx_tlb_check(CPUState *env, mmu_ctx_t *mmu_ctx, target_ulong eaddr, int rw, int access_type)
{
    ppc6xx_tlb_t *tlb;
    int nr, best, way;
    int ret;

    best = -1;
    ret = -1; /* No TLB found */
    for(way = 0; way < env->nb_ways; way++) {
        nr = ppc6xx_tlb_getnum(env, eaddr, way, access_type == ACCESS_CODE ? 1 : 0);
        tlb = &env->tlb.tlb6[nr];
        /* This test "emulates" the PTE index match for hardware TLBs */
        if((eaddr & TARGET_PAGE_MASK) != tlb->EPN) {
            continue;
        }
        switch(pte32_check(mmu_ctx, tlb->pte0, tlb->pte1, 0, rw, access_type)) {
            case -3:
                /* TLB inconsistency */
                return -1;
            case -2:
                /* Access violation */
                ret = -2;
                best = nr;
                break;
            case -1:
            default:
                /* No match */
                break;
            case 0:
                /* access granted */
                /* XXX: we should go on looping to check all TLBs consistency
                 *      but we can speed-up the whole thing as the
                 *      result would be undefined if TLBs are not consistent.
                 */
                ret = 0;
                best = nr;
                goto done;
        }
    }
    if(best != -1) {
    done:
        /* Update page flags */
        pte_update_flags(mmu_ctx, &env->tlb.tlb6[best].pte1, ret, rw);
    }

    return ret;
}

/* Perform BAT hit & translation */
static inline void bat_size_prot(CPUState *env, target_ulong *blp, int *validp, int *protp, target_ulong *BATu,
                                 target_ulong *BATl)
{
    target_ulong bl;
    int pp, valid, prot;

    bl = (*BATu & 0x00001FFC) << 15;
    valid = 0;
    prot = 0;
    if(((msr_pr == 0) && (*BATu & 0x00000002)) || ((msr_pr != 0) && (*BATu & 0x00000001))) {
        valid = 1;
        pp = *BATl & 0x00000003;
        if(pp != 0) {
            prot = PAGE_READ | PAGE_EXEC;
            if(pp == 0x2) {
                prot |= PAGE_WRITE;
            }
        }
    }
    *blp = bl;
    *validp = valid;
    *protp = prot;
}

static inline void bat_601_size_prot(CPUState *env, target_ulong *blp, int *validp, int *protp, target_ulong *BATu,
                                     target_ulong *BATl)
{
    target_ulong bl;
    int key, pp, valid, prot;

    bl = (*BATl & 0x0000003F) << 17;
    prot = 0;
    valid = (*BATl >> 6) & 1;
    if(valid) {
        pp = *BATu & 0x00000003;
        if(msr_pr == 0) {
            key = (*BATu >> 3) & 1;
        } else {
            key = (*BATu >> 2) & 1;
        }
        prot = pp_check(key, pp, 0);
    }
    *blp = bl;
    *validp = valid;
    *protp = prot;
}

static inline int get_bat(CPUState *env, mmu_ctx_t *mmu_ctx, target_ulong virtual, int rw, int type)
{
    target_ulong *BATlt, *BATut, *BATu, *BATl;
    target_ulong BEPIl, BEPIu, bl;
    int i, valid, prot;
    int ret = -1;

    switch(type) {
        case ACCESS_CODE:
            BATlt = env->IBAT[1];
            BATut = env->IBAT[0];
            break;
        default:
            BATlt = env->DBAT[1];
            BATut = env->DBAT[0];
            break;
    }
    for(i = 0; i < env->nb_BATs; i++) {
        BATu = &BATut[i];
        BATl = &BATlt[i];
        BEPIu = *BATu & 0xF0000000;
        BEPIl = *BATu & 0x0FFE0000;
        if(unlikely(env->mmu_model == POWERPC_MMU_601)) {
            bat_601_size_prot(env, &bl, &valid, &prot, BATu, BATl);
        } else {
            bat_size_prot(env, &bl, &valid, &prot, BATu, BATl);
        }
        if((virtual & 0xF0000000) == BEPIu && ((virtual & 0x0FFE0000) & ~bl) == BEPIl) {
            /* BAT matches */
            if(valid != 0) {
                /* Get physical address */
                mmu_ctx->raddr = (*BATl & 0xF0000000) | ((virtual & 0x0FFE0000 & bl) | (*BATl & 0x0FFE0000)) |
                                 (virtual & 0x0001F000);
                /* Compute access rights */
                mmu_ctx->prot = prot;
                ret = check_prot(mmu_ctx->prot, rw, type);
                break;
            }
        }
    }
    /* No hit */
    return ret;
}

static inline target_phys_addr_t get_pteg_offset(CPUState *env, target_phys_addr_t hash, int pte_size)
{
    return (hash * pte_size * 8) & env->htab_mask;
}

/* PTE table lookup */
static inline int _find_pte(CPUState *env, mmu_ctx_t *mmu_ctx, int is_64b, int h, int rw, int type, int target_page_bits)
{
    target_phys_addr_t pteg_off;
    target_ulong pte0, pte1;
    int i, good = -1;
    int ret, r;

    ret = -1; /* No entry found */
    pteg_off = get_pteg_offset(env, mmu_ctx->hash[h], HASH_PTE_SIZE_32);
    for(i = 0; i < 8; i++) {
#if defined(TARGET_PPC64)
        if(is_64b) {
            if(env->external_htab) {
                pte0 = ldq_p(env->external_htab + pteg_off + (i * 16));
                pte1 = ldq_p(env->external_htab + pteg_off + (i * 16) + 8);
            } else {
                pte0 = ldq_phys(env->htab_base + pteg_off + (i * 16));
                pte1 = ldq_phys(env->htab_base + pteg_off + (i * 16) + 8);
            }

            /* We have a TLB that saves 4K pages, so let's
             * split a huge page to 4k chunks */
            if(target_page_bits != TARGET_PAGE_BITS) {
                pte1 |= (mmu_ctx->eaddr & ((1 << target_page_bits) - 1)) & TARGET_PAGE_MASK;
            }

            r = pte64_check(mmu_ctx, pte0, pte1, h, rw, type);
        } else
#endif
        {
            if(env->external_htab) {
                pte0 = ldl_p(env->external_htab + pteg_off + (i * 8));
                pte1 = ldl_p(env->external_htab + pteg_off + (i * 8) + 4);
            } else {
                pte0 = ldl_phys(env->htab_base + pteg_off + (i * 8));
                pte1 = ldl_phys(env->htab_base + pteg_off + (i * 8) + 4);
            }
        }
        r = pte32_check(mmu_ctx, pte0, pte1, h, rw, type);
        switch(r) {
            case -3:
                /* PTE inconsistency */
                return -1;
            case -2:
                /* Access violation */
                ret = -2;
                good = i;
                break;
            case -1:
            default:
                /* No PTE match */
                break;
            case 0:
                /* access granted */
                /* XXX: we should go on looping to check all PTEs consistency
                 *      but if we can speed-up the whole thing as the
                 *      result would be undefined if PTEs are not consistent.
                 */
                ret = 0;
                good = i;
                goto done;
        }
    }
    if(good != -1) {
    done:
        /* Update page flags */
        pte1 = mmu_ctx->raddr;
        if(pte_update_flags(mmu_ctx, &pte1, ret, rw) == 1) {
            if(env->external_htab) {
                stl_p(env->external_htab + pteg_off + (good * 8) + 4, pte1);
            } else {
                stl_phys_notdirty(env->htab_base + pteg_off + (good * 8) + 4, pte1);
            }
        }
    }

    return ret;
}

static inline int find_pte(CPUState *env, mmu_ctx_t *ctx, int h, int rw, int type, int target_page_bits)
{
#if defined(TARGET_PPC64)
    if(env->mmu_model & POWERPC_MMU_64) {
        return _find_pte(env, ctx, 1, h, rw, type, target_page_bits);
    }
#endif

    return _find_pte(env, ctx, 0, h, rw, type, target_page_bits);
}

#if defined(TARGET_PPC64)
static inline ppc_slb_t *slb_lookup(CPUState *env, target_ulong eaddr)
{
    uint64_t esid_256M, esid_1T;
    int n;

    esid_256M = (eaddr & SEGMENT_MASK_256M) | SLB_ESID_V;
    esid_1T = (eaddr & SEGMENT_MASK_1T) | SLB_ESID_V;

    for(n = 0; n < env->slb_nr; n++) {
        ppc_slb_t *slb = &env->slb[n];

        /* We check for 1T matches on all MMUs here - if the MMU
         * doesn't have 1T segment support, we will have prevented 1T
         * entries from being inserted in the slbmte code. */
        if(((slb->esid == esid_256M) && ((slb->vsid & SLB_VSID_B) == SLB_VSID_B_256M)) ||
           ((slb->esid == esid_1T) && ((slb->vsid & SLB_VSID_B) == SLB_VSID_B_1T))) {
            return slb;
        }
    }

    return NULL;
}

void ppc_slb_invalidate_all(CPUState *env)
{
    int n, do_invalidate;

    do_invalidate = 0;
    /* XXX: Warning: slbia never invalidates the first segment */
    for(n = 1; n < env->slb_nr; n++) {
        ppc_slb_t *slb = &env->slb[n];

        if(slb->esid & SLB_ESID_V) {
            slb->esid &= ~SLB_ESID_V;
            /* XXX: given the fact that segment size is 256 MB or 1TB,
             *      and we still don't have a tlb_flush_mask(env, n, mask)
             *      in Qemu, we just invalidate all TLBs
             */
            do_invalidate = 1;
        }
    }
    if(do_invalidate) {
        tlb_flush(env, 1, true);
    }
}

void ppc_slb_invalidate_one(CPUState *env, uint64_t T0)
{
    ppc_slb_t *slb;

    slb = slb_lookup(env, T0);
    if(!slb) {
        return;
    }

    if(slb->esid & SLB_ESID_V) {
        slb->esid &= ~SLB_ESID_V;

        /* XXX: given the fact that segment size is 256 MB or 1TB,
         *      and we still don't have a tlb_flush_mask(env, n, mask)
         *      in Qemu, we just invalidate all TLBs
         */
        tlb_flush(env, 1, true);
    }
}

int ppc_store_slb(CPUState *env, target_ulong rb, target_ulong rs)
{
    int slot = rb & 0xfff;
    ppc_slb_t *slb = &env->slb[slot];

    if(rb & (0x1000 - env->slb_nr)) {
        return -1; /* Reserved bits set or slot too high */
    }
    if(rs & (SLB_VSID_B & ~SLB_VSID_B_1T)) {
        return -1; /* Bad segment size */
    }
    if((rs & SLB_VSID_B) && !(env->mmu_model & POWERPC_MMU_1TSEG)) {
        return -1; /* 1T segment on MMU that doesn't support it */
    }

    /* Mask out the slot number as we store the entry */
    slb->esid = rb & (SLB_ESID_ESID | SLB_ESID_V);
    slb->vsid = rs;

    return 0;
}

int ppc_load_slb_esid(CPUState *env, target_ulong rb, target_ulong *rt)
{
    int slot = rb & 0xfff;
    ppc_slb_t *slb = &env->slb[slot];

    if(slot >= env->slb_nr) {
        return -1;
    }

    *rt = slb->esid;
    return 0;
}

int ppc_load_slb_vsid(CPUState *env, target_ulong rb, target_ulong *rt)
{
    int slot = rb & 0xfff;
    ppc_slb_t *slb = &env->slb[slot];

    if(slot >= env->slb_nr) {
        return -1;
    }

    *rt = slb->vsid;
    return 0;
}
#endif /* defined(TARGET_PPC64) */

/* Perform segment based translation */
static inline int get_segment(CPUState *env, mmu_ctx_t *mmu_ctx, target_ulong eaddr, int rw, int type)
{
    target_phys_addr_t hash;
    target_ulong vsid;
    int ds, pr, target_page_bits;
    int ret, ret2 = 0;

    pr = msr_pr;
    mmu_ctx->eaddr = eaddr;
#if defined(TARGET_PPC64)
    if(env->mmu_model & POWERPC_MMU_64) {
        ppc_slb_t *slb;
        target_ulong pageaddr;
        int segment_bits;

        slb = slb_lookup(env, eaddr);
        if(!slb) {
            return -5;
        }

        if(slb->vsid & SLB_VSID_B) {
            vsid = (slb->vsid & SLB_VSID_VSID) >> SLB_VSID_SHIFT_1T;
            segment_bits = 40;
        } else {
            vsid = (slb->vsid & SLB_VSID_VSID) >> SLB_VSID_SHIFT;
            segment_bits = 28;
        }

        target_page_bits = (slb->vsid & SLB_VSID_L) ? TARGET_PAGE_BITS_16M : TARGET_PAGE_BITS;
        mmu_ctx->key = !!(pr ? (slb->vsid & SLB_VSID_KP) : (slb->vsid & SLB_VSID_KS));
        ds = 0;
        mmu_ctx->nx = !!(slb->vsid & SLB_VSID_N);

        pageaddr = eaddr & ((1ULL << segment_bits) - (1ULL << target_page_bits));
        if(slb->vsid & SLB_VSID_B) {
            hash = vsid ^ (vsid << 25) ^ (pageaddr >> target_page_bits);
        } else {
            hash = vsid ^ (pageaddr >> target_page_bits);
        }
        /* Only 5 bits of the page index are used in the AVPN */
        mmu_ctx->ptem = (slb->vsid & SLB_VSID_PTEM) | ((pageaddr >> 16) & ((1ULL << segment_bits) - 0x80));
    } else
#endif /* defined(TARGET_PPC64) */
    {
        target_ulong sr, pgidx;

        sr = env->sr[eaddr >> 28];
        mmu_ctx->key = (((sr & 0x20000000) && (pr != 0)) || ((sr & 0x40000000) && (pr == 0))) ? 1 : 0;
        ds = sr & 0x80000000 ? 1 : 0;
        mmu_ctx->nx = sr & 0x10000000 ? 1 : 0;
        vsid = sr & 0x00FFFFFF;
        target_page_bits = TARGET_PAGE_BITS;
        pgidx = (eaddr & ~SEGMENT_MASK_256M) >> target_page_bits;
        hash = vsid ^ pgidx;
        mmu_ctx->ptem = (vsid << 7) | (pgidx >> 10);
    }
    ret = -1;
    if(!ds) {
        /* Check if instruction fetch is allowed, if needed */
        if(type != ACCESS_CODE || mmu_ctx->nx == 0) {
            /* Page address translation */
            mmu_ctx->hash[0] = hash;
            mmu_ctx->hash[1] = ~hash;

            /* Initialize real address with an invalid value */
            mmu_ctx->raddr = (target_phys_addr_t)-1ULL;
            if(unlikely(env->mmu_model == POWERPC_MMU_SOFT_6xx || env->mmu_model == POWERPC_MMU_SOFT_74xx)) {
                /* Software TLB search */
                ret = ppc6xx_tlb_check(env, mmu_ctx, eaddr, rw, type);
            } else {
                /* Primary table lookup */
                ret = find_pte(env, mmu_ctx, 0, rw, type, target_page_bits);
                if(ret < 0) {
                    /* Secondary table lookup */
                    if(eaddr != 0xEFFFFFFF) {
                        ret2 = find_pte(env, mmu_ctx, 1, rw, type, target_page_bits);
                    }
                    if(ret2 != -1) {
                        ret = ret2;
                    }
                }
            }
        } else {
            ret = -3;
        }
    } else {
        target_ulong sr;
        /* Direct-store segment : absolutely *BUGGY* for now */

        /* Direct-store implies a 32-bit MMU.
         * Check the Segment Register's bus unit ID (BUID).
         */
        sr = env->sr[eaddr >> 28];
        if((sr & 0x1FF00000) >> 20 == 0x07f) {
            /* Memory-forced I/O controller interface access */
            /* If T=1 and BUID=x'07F', the 601 performs a memory access
             * to SR[28-31] LA[4-31], bypassing all protection mechanisms.
             */
            mmu_ctx->raddr = ((sr & 0xF) << 28) | (eaddr & 0x0FFFFFFF);
            mmu_ctx->prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
            return 0;
        }

        switch(type) {
            case ACCESS_INT:
                /* Integer load/store : only access allowed */
                break;
            case ACCESS_CODE:
                /* No code fetch is allowed in direct-store areas */
                return -4;
            case ACCESS_FLOAT:
                /* Floating point load/store */
                return -4;
            case ACCESS_RES:
                /* lwarx, ldarx or srwcx. */
                return -4;
            case ACCESS_CACHE:
                /* dcba, dcbt, dcbtst, dcbf, dcbi, dcbst, dcbz, or icbi */
                /* Should make the instruction do no-op.
                 * As it already do no-op, it's quite easy :-)
                 */
                mmu_ctx->raddr = eaddr;
                return 0;
            case ACCESS_EXT:
                /* eciwx or ecowx */
                return -4;
            default:
                return -4;
        }
        if((rw == 1 || mmu_ctx->key != 1) && (rw == 0 || mmu_ctx->key != 0)) {
            mmu_ctx->raddr = eaddr;
            ret = 2;
        } else {
            ret = -2;
        }
    }

    return ret;
}

/* Generic TLB check function for embedded PowerPC implementations */
int ppcemb_tlb_check(CPUState *env, ppcemb_tlb_t *tlb, target_phys_addr_t *raddrp, target_ulong address, uint32_t pid, int ext,
                     int i)
{
    target_ulong mask;

    /* Check valid flag */
    if(!(tlb->prot & PAGE_VALID)) {
        return -1;
    }
    mask = ~(tlb->size - 1);
    /* Check PID */
    if(tlb->PID != 0 && tlb->PID != pid) {
        return -1;
    }
    /* Check effective address */
    if((address & mask) != tlb->EPN) {
        return -1;
    }
    *raddrp = (tlb->RPN & mask) | (address & ~mask);
#if (TARGET_PHYS_ADDR_BITS >= 36)
    if(ext) {
        /* Extend the physical address to 36 bits */
        *raddrp |= (target_phys_addr_t)(tlb->RPN & 0xF) << 32;
    }
#endif

    return 0;
}

/* Generic TLB search function for PowerPC embedded implementations */
int ppcemb_tlb_search(CPUState *env, target_ulong address, uint32_t pid)
{
    ppcemb_tlb_t *tlb;
    target_phys_addr_t raddr;
    int i, ret;

    /* Default return value is no match */
    ret = -1;
    for(i = 0; i < env->nb_tlb; i++) {
        tlb = &env->tlb.tlbe[i];
        if(ppcemb_tlb_check(env, tlb, &raddr, address, pid, 0, i) == 0) {
            ret = i;
            break;
        }
    }

    return ret;
}

/* Helpers specific to PowerPC 40x implementations */
static inline void ppc4xx_tlb_invalidate_all(CPUState *env)
{
    ppcemb_tlb_t *tlb;
    int i;

    for(i = 0; i < env->nb_tlb; i++) {
        tlb = &env->tlb.tlbe[i];
        tlb->prot &= ~PAGE_VALID;
    }
    tlb_flush(env, 1, true);
}

static inline void ppc4xx_tlb_invalidate_virt(CPUState *env, target_ulong eaddr, uint32_t pid)
{
    ppcemb_tlb_t *tlb;
    target_phys_addr_t raddr;
    target_ulong page, end;
    int i;

    for(i = 0; i < env->nb_tlb; i++) {
        tlb = &env->tlb.tlbe[i];
        if(ppcemb_tlb_check(env, tlb, &raddr, eaddr, pid, 0, i) == 0) {
            end = tlb->EPN + tlb->size;
            for(page = tlb->EPN; page < end; page += TARGET_PAGE_SIZE) {
                tlb_flush_page(env, page, true);
            }
            tlb->prot &= ~PAGE_VALID;
            break;
        }
    }
}

static int mmu40x_get_physical_address(CPUState *env, mmu_ctx_t *mmu_ctx, target_ulong address, int rw, int access_type)
{
    ppcemb_tlb_t *tlb;
    target_phys_addr_t raddr;
    int i, ret, zsel, zpr, pr;

    ret = -1;
    raddr = (target_phys_addr_t)-1ULL;
    pr = msr_pr;
    for(i = 0; i < env->nb_tlb; i++) {
        tlb = &env->tlb.tlbe[i];
        if(ppcemb_tlb_check(env, tlb, &raddr, address, env->spr[SPR_40x_PID], 0, i) < 0) {
            continue;
        }
        zsel = (tlb->attr >> 4) & 0xF;
        zpr = (env->spr[SPR_40x_ZPR] >> (30 - (2 * zsel))) & 0x3;
        /* Check execute enable bit */
        switch(zpr) {
            case 0x2:
                if(pr != 0) {
                    goto check_perms;
                }
                /* No break here */
                goto case_0x3;
            case 0x3:
            case_0x3:
                /* All accesses granted */
                mmu_ctx->prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
                ret = 0;
                break;
            case 0x0:
                if(pr != 0) {
                    /* Raise Zone protection fault.  */
                    env->spr[SPR_40x_ESR] = 1 << 22;
                    mmu_ctx->prot = 0;
                    ret = -2;
                    break;
                }
                /* No break here */
                goto check_perms;
            case 0x1:
            check_perms:
                /* Check from TLB entry */
                mmu_ctx->prot = tlb->prot;
                ret = check_prot(mmu_ctx->prot, rw, access_type);
                if(ret == -2) {
                    env->spr[SPR_40x_ESR] = 0;
                }
                break;
        }
        if(ret >= 0) {
            mmu_ctx->raddr = raddr;
            return 0;
        }
    }

    return ret;
}

void store_40x_sler(CPUState *env, uint32_t val)
{
    /* XXX: TO BE FIXED */
    if(val != 0x00000000) {
        cpu_abort(env, "Little-endian regions are not supported by now\n");
    }
    env->spr[SPR_405_SLER] = val;
}

static inline int mmubooke_check_tlb(CPUState *env, ppcemb_tlb_t *tlb, target_phys_addr_t *raddr, int *prot, target_ulong address,
                                     int rw, int access_type, int i)
{
    int ret, _prot;

    if(ppcemb_tlb_check(env, tlb, raddr, address, env->spr[SPR_BOOKE_PID], !env->nb_pids, i) >= 0) {
        goto found_tlb;
    }

    if(env->spr[SPR_BOOKE_PID1] && ppcemb_tlb_check(env, tlb, raddr, address, env->spr[SPR_BOOKE_PID1], 0, i) >= 0) {
        goto found_tlb;
    }

    if(env->spr[SPR_BOOKE_PID2] && ppcemb_tlb_check(env, tlb, raddr, address, env->spr[SPR_BOOKE_PID2], 0, i) >= 0) {
        goto found_tlb;
    }

    return -1;

found_tlb:

    if(msr_pr != 0) {
        _prot = tlb->prot & 0xF;
    } else {
        _prot = (tlb->prot >> 4) & 0xF;
    }

    /* Check the address space */
    if(access_type == ACCESS_CODE) {
        if(msr_ir != (tlb->attr & 1)) {
            return -1;
        }

        *prot = _prot;
        if(_prot & PAGE_EXEC) {
            return 0;
        }

        ret = -3;
    } else {
        if(msr_dr != (tlb->attr & 1)) {
            return -1;
        }

        *prot = _prot;
        if((!rw && _prot & PAGE_READ) || (rw && (_prot & PAGE_WRITE))) {
            return 0;
        }

        ret = -2;
    }

    return ret;
}

static int mmubooke_get_physical_address(CPUState *env, mmu_ctx_t *mmu_ctx, target_ulong address, int rw, int access_type)
{
    ppcemb_tlb_t *tlb;
    target_phys_addr_t raddr;
    int i, ret;

    ret = -1;
    raddr = (target_phys_addr_t)-1ULL;
    for(i = 0; i < env->nb_tlb; i++) {
        tlb = &env->tlb.tlbe[i];
        ret = mmubooke_check_tlb(env, tlb, &raddr, &mmu_ctx->prot, address, rw, access_type, i);
        if(!ret) {
            break;
        }
    }

    if(ret >= 0) {
        mmu_ctx->raddr = raddr;
    }

    return ret;
}

void booke206_flush_tlb(CPUState *env, int flags, const int check_iprot)
{
    int tlb_size;
    int i, j;
    ppcmas_tlb_t *tlb = env->tlb.tlbm;

    for(i = 0; i < BOOKE206_MAX_TLBN; i++) {
        if(flags & (1 << i)) {
            tlb_size = booke206_tlb_size(env, i);
            for(j = 0; j < tlb_size; j++) {
                if(!check_iprot || !(tlb[j].mas1 & MAS1_IPROT)) {
                    tlb[j].mas1 &= ~MAS1_VALID;
                }
            }
        }
        tlb += booke206_tlb_size(env, i);
    }

    tlb_flush(env, 1, true);
}

target_phys_addr_t booke206_tlb_to_page_size(CPUState *env, ppcmas_tlb_t *tlb)
{
    uint32_t tlbncfg;
    int tlbn = booke206_tlbm_to_tlbn(env, tlb);
    int tlbm_size;

    tlbncfg = env->spr[SPR_BOOKE_TLB0CFG + tlbn];

    if(tlbncfg & TLBnCFG_AVAIL) {
        tlbm_size = (tlb->mas1 & MAS1_TSIZE_MASK) >> MAS1_TSIZE_SHIFT;
    } else {
        tlbm_size = (tlbncfg & TLBnCFG_MINSIZE) >> TLBnCFG_MINSIZE_SHIFT;
        tlbm_size <<= 1;
    }

    return 1024ULL << tlbm_size;
}

/* TLB check function for MAS based SoftTLBs */
int ppcmas_tlb_check(CPUState *env, ppcmas_tlb_t *tlb, target_phys_addr_t *raddrp, target_ulong address, uint32_t pid)
{
    target_ulong mask;
    uint32_t tlb_pid;

    /* Check valid flag */
    if(!(tlb->mas1 & MAS1_VALID)) {
        return -1;
    }

    mask = ~(booke206_tlb_to_page_size(env, tlb) - 1);

    /* Check PID */
    tlb_pid = (tlb->mas1 & MAS1_TID_MASK) >> MAS1_TID_SHIFT;
    if(tlb_pid != 0 && tlb_pid != pid) {
        return -1;
    }

    /* Check effective address */
    if((address & mask) != (tlb->mas2 & MAS2_EPN_MASK)) {
        return -1;
    }
    *raddrp = (tlb->mas7_3 & mask) | (address & ~mask);

    return 0;
}

static int mmubooke206_check_tlb(CPUState *env, ppcmas_tlb_t *tlb, target_phys_addr_t *raddr, int *prot, target_ulong address,
                                 int rw, int access_type)
{
    int ret;
    int _prot = 0;

    if(ppcmas_tlb_check(env, tlb, raddr, address, env->spr[SPR_BOOKE_PID]) >= 0) {
        goto found_tlb;
    }

    if(env->spr[SPR_BOOKE_PID1] && ppcmas_tlb_check(env, tlb, raddr, address, env->spr[SPR_BOOKE_PID1]) >= 0) {
        goto found_tlb;
    }

    if(env->spr[SPR_BOOKE_PID2] && ppcmas_tlb_check(env, tlb, raddr, address, env->spr[SPR_BOOKE_PID2]) >= 0) {
        goto found_tlb;
    }
    return -1;

found_tlb:

    if(msr_pr != 0) {
        if(tlb->mas7_3 & MAS3_UR) {
            _prot |= PAGE_READ;
        }
        if(tlb->mas7_3 & MAS3_UW) {
            _prot |= PAGE_WRITE;
        }
        if(tlb->mas7_3 & MAS3_UX) {
            _prot |= PAGE_EXEC;
        }
    } else {
        if(tlb->mas7_3 & MAS3_SR) {
            _prot |= PAGE_READ;
        }
        if(tlb->mas7_3 & MAS3_SW) {
            _prot |= PAGE_WRITE;
        }
        if(tlb->mas7_3 & MAS3_SX) {
            _prot |= PAGE_EXEC;
        }
    }

    /* Check the address space and permissions */
    if(access_type == ACCESS_CODE) {
        if(msr_ir != ((tlb->mas1 & MAS1_TS) >> MAS1_TS_SHIFT)) {
            return -1;
        }

        *prot = _prot;
        if(_prot & PAGE_EXEC) {
            return 0;
        }

        ret = -3;
    } else {
        if(msr_dr != ((tlb->mas1 & MAS1_TS) >> MAS1_TS_SHIFT)) {
            return -1;
        }

        *prot = _prot;
        if((!rw && _prot & PAGE_READ) || (rw && (_prot & PAGE_WRITE))) {
            return 0;
        }

        ret = -2;
    }

    return ret;
}

static int mmubooke206_get_physical_address(CPUState *env, mmu_ctx_t *mmu_ctx, target_ulong address, int rw, int access_type)
{
    ppcmas_tlb_t *tlb;
    target_phys_addr_t raddr;
    int i, j, ret;

    ret = -1;
    raddr = (target_phys_addr_t)-1ULL;
    for(i = 0; i < BOOKE206_MAX_TLBN; i++) {
        int ways = booke206_tlb_ways(env, i);

        for(j = 0; j < ways; j++) {
            tlb = booke206_get_tlbm(env, i, address, j);
            ret = mmubooke206_check_tlb(env, tlb, &raddr, &mmu_ctx->prot, address, rw, access_type);
            if(ret != -1) {
                goto found_tlb;
            }
        }
    }

found_tlb:

    if(ret >= 0) {
        mmu_ctx->raddr = raddr;
    } else {
        if(address < 0x600000) {
            mmu_ctx->raddr = (address & 0xFFFFFFFF);
            mmu_ctx->prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
            ret = 0;
        }
        if(address > 0x700000) {
            mmu_ctx->raddr = (address & 0xFFFFFFFF);
            mmu_ctx->prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
            ret = 0;
        }
    }
    return ret;
}

static inline int check_physical(CPUState *env, mmu_ctx_t *mmu_ctx, target_ulong eaddr, int rw)
{
    int in_plb, ret;

    mmu_ctx->raddr = eaddr;
    mmu_ctx->prot = PAGE_READ | PAGE_EXEC;
    ret = 0;
    switch(env->mmu_model) {
        case POWERPC_MMU_32B:
        case POWERPC_MMU_601:
        case POWERPC_MMU_SOFT_6xx:
        case POWERPC_MMU_SOFT_74xx:
        case POWERPC_MMU_SOFT_4xx:
        case POWERPC_MMU_REAL:
        case POWERPC_MMU_BOOKE:
            mmu_ctx->prot |= PAGE_WRITE;
            break;
#if defined(TARGET_PPC64)
        case POWERPC_MMU_620:
        case POWERPC_MMU_64B:
        case POWERPC_MMU_2_06:
            /* Real address are 60 bits long */
            mmu_ctx->raddr &= 0x0FFFFFFFFFFFFFFFULL;
            mmu_ctx->prot |= PAGE_WRITE;
            break;
#endif
        case POWERPC_MMU_SOFT_4xx_Z:
            if(unlikely(msr_pe != 0)) {
                /* 403 family add some particular protections,
                 * using PBL/PBU registers for accesses with no translation.
                 */
                /* Check PLB validity and address in plb area */
                in_plb = (env->pb[0] < env->pb[1] && eaddr >= env->pb[0] && eaddr < env->pb[1]) ||
                                 (env->pb[2] < env->pb[3] && eaddr >= env->pb[2] && eaddr < env->pb[3])
                             ? 1
                             : 0;
                if(in_plb ^ msr_px) {
                    /* Access in protected area */
                    if(rw == 1) {
                        /* Access is not allowed */
                        ret = -2;
                    }
                } else {
                    /* Read-write access is allowed */
                    mmu_ctx->prot |= PAGE_WRITE;
                }
            }
            break;
        case POWERPC_MMU_MPC8xx:
            /* XXX: TODO */
            cpu_abort(env, "MPC8xx MMU model is not implemented\n");
            break;
        case POWERPC_MMU_BOOKE206:
            cpu_abort(env, "BookE 2.06 MMU doesn't have physical real mode\n");
            break;
        default:
            cpu_abort(env, "Unknown or invalid MMU model\n");
            return -1;
    }

    return ret;
}

int get_physical_address(CPUState *env, mmu_ctx_t *mmu_ctx, target_ulong eaddr, int rw, int ppc_access_type)
{
    int ret;

    if((ppc_access_type == ACCESS_CODE && msr_ir == 0) || (ppc_access_type != ACCESS_CODE && msr_dr == 0)) {
        if(env->mmu_model == POWERPC_MMU_BOOKE) {
            /* The BookE MMU always performs address translation. The
               IS and DS bits only affect the address space.  */
            ret = mmubooke_get_physical_address(env, mmu_ctx, eaddr, rw, ppc_access_type);
        } else if(env->mmu_model == POWERPC_MMU_BOOKE206) {
            ret = mmubooke206_get_physical_address(env, mmu_ctx, eaddr, rw, ppc_access_type);
        } else {
            /* No address translation.  */
            ret = check_physical(env, mmu_ctx, eaddr, rw);
        }
    } else {
        ret = -1;  //  TRANSLATE_FAIL
        switch(env->mmu_model) {
            case POWERPC_MMU_32B:
            case POWERPC_MMU_601:
            case POWERPC_MMU_SOFT_6xx:
            case POWERPC_MMU_SOFT_74xx:
                /* Try to find a BAT */
                if(env->nb_BATs != 0) {
                    ret = get_bat(env, mmu_ctx, eaddr, rw, ppc_access_type);
                }
#if defined(TARGET_PPC64)
                goto gph_nofallthrough;
            case POWERPC_MMU_620:
            case POWERPC_MMU_64B:
            case POWERPC_MMU_2_06:

            gph_nofallthrough:
#endif
                if(ret < 0) {  //  TRANSLATE_FAIL
                    /* We didn't match any BAT entry or don't have BATs */
                    ret = get_segment(env, mmu_ctx, eaddr, rw, ppc_access_type);
                }
                break;
            case POWERPC_MMU_SOFT_4xx:
            case POWERPC_MMU_SOFT_4xx_Z:
                ret = mmu40x_get_physical_address(env, mmu_ctx, eaddr, rw, ppc_access_type);
                break;
            case POWERPC_MMU_BOOKE:
                ret = mmubooke_get_physical_address(env, mmu_ctx, eaddr, rw, ppc_access_type);
                break;
            case POWERPC_MMU_BOOKE206:
                ret = mmubooke206_get_physical_address(env, mmu_ctx, eaddr, rw, ppc_access_type);
                break;
            case POWERPC_MMU_MPC8xx:
                /* XXX: TODO */
                cpu_abort(env, "MPC8xx MMU model is not implemented\n");
                break;
            case POWERPC_MMU_REAL:
                cpu_abort(env, "PowerPC in real mode do not do any translation\n");
                return -1;  //  TRANSLATE_FAIL
            default:
                cpu_abort(env, "Unknown or invalid MMU model\n");
                return -1;  //  TRANSLATE_FAIL
        }
    }
    return ret;
}

target_phys_addr_t cpu_get_phys_page_debug(CPUState *env, target_ulong addr)
{
    mmu_ctx_t mmu_ctx;

    if(unlikely(get_physical_address(env, &mmu_ctx, addr, 0, ACCESS_INT) != 0)) {
        return -1;
    }

    return mmu_ctx.raddr & TARGET_PAGE_MASK;
}

/* Transaction filtering by state is not yet implemented for this architecture.
 * This placeholder function is here to make it clear that more CPUs are expected to support this in the future. */
uint64_t cpu_get_state_for_memory_transaction(CPUState *env, target_ulong addr, int access_type)
{
    return 0;
}

static void booke206_update_mas_tlb_miss(CPUState *env, target_ulong address, int rw)
{
    env->spr[SPR_BOOKE_MAS0] = env->spr[SPR_BOOKE_MAS4] & MAS4_TLBSELD_MASK;
    env->spr[SPR_BOOKE_MAS1] = env->spr[SPR_BOOKE_MAS4] & MAS4_TSIZED_MASK;
    env->spr[SPR_BOOKE_MAS2] = env->spr[SPR_BOOKE_MAS4] & MAS4_WIMGED_MASK;
    env->spr[SPR_BOOKE_MAS3] = 0;
    env->spr[SPR_BOOKE_MAS6] = 0;
    env->spr[SPR_BOOKE_MAS7] = 0;
    env->spr[SPR_BOOKE_MAS8] = 0;

    /* AS */
    if(((rw == 2) && msr_ir) || ((rw != 2) && msr_dr)) {
        env->spr[SPR_BOOKE_MAS1] |= MAS1_TS;
        env->spr[SPR_BOOKE_MAS6] |= MAS6_SAS;
    }

    env->spr[SPR_BOOKE_MAS1] |= MAS1_VALID;
    env->spr[SPR_BOOKE_MAS2] |= address & MAS2_EPN_MASK;

    switch(env->spr[SPR_BOOKE_MAS4] & MAS4_TIDSELD_PIDZ) {
        case MAS4_TIDSELD_PID0:
            env->spr[SPR_BOOKE_MAS1] |= env->spr[SPR_BOOKE_PID] << MAS1_TID_SHIFT;
            break;
        case MAS4_TIDSELD_PID1:
            env->spr[SPR_BOOKE_MAS1] |= env->spr[SPR_BOOKE_PID1] << MAS1_TID_SHIFT;
            break;
        case MAS4_TIDSELD_PID2:
            env->spr[SPR_BOOKE_MAS1] |= env->spr[SPR_BOOKE_PID2] << MAS1_TID_SHIFT;
            break;
    }

    env->spr[SPR_BOOKE_MAS6] |= env->spr[SPR_BOOKE_PID] << 16;

    /* next victim logic */
    env->spr[SPR_BOOKE_MAS0] |= env->last_way << MAS0_ESEL_SHIFT;
    env->last_way++;
    env->last_way &= booke206_tlb_ways(env, 0) - 1;
    env->spr[SPR_BOOKE_MAS0] |= env->last_way << MAS0_NV_SHIFT;
}

/* Perform address translation */
int cpu_handle_mmu_fault(CPUState *env, target_ulong address, int access_type, int mmu_idx, int no_page_fault)
{
    if(unlikely(cpu->external_mmu_enabled)) {
        target_phys_addr_t phys_addr;
        int prot;

        if(TRANSLATE_SUCCESS == get_external_mmu_phys_addr(env, address, access_type, &phys_addr, &prot, no_page_fault)) {
            tlb_set_page(env, address & TARGET_PAGE_MASK, phys_addr & TARGET_PAGE_MASK, prot, mmu_idx, TARGET_PAGE_SIZE);
            return TRANSLATE_SUCCESS;
        }
        return TRANSLATE_FAIL;
    }

    mmu_ctx_t mmu_ctx;
    int ret = TRANSLATE_SUCCESS;
    int is_write = access_type == ACCESS_DATA_STORE ? 1 : 0;
    int ppc_access_type;

    if(access_type == ACCESS_INST_FETCH) {
        /* code access */
        ppc_access_type = ACCESS_CODE;
    } else {
        /* data access */
        ppc_access_type = env->access_type;
    }
    ret = get_physical_address(env, &mmu_ctx, address, is_write, ppc_access_type);
    if(ret == TRANSLATE_SUCCESS) {
        tlb_set_page(env, address & TARGET_PAGE_MASK, mmu_ctx.raddr & TARGET_PAGE_MASK, mmu_ctx.prot, mmu_idx, TARGET_PAGE_SIZE);
    } else if(ret < 0) {  //  TRANSLATE_FAIL
        tlib_printf(LOG_LEVEL_WARNING, "we got mmu fail @ %X on %s\n", address,
                    (ppc_access_type == ACCESS_CODE) ? "CODE" : "DATA");
        if(ppc_access_type == ACCESS_CODE) {
            tlib_printf(LOG_LEVEL_WARNING, "ret is %d\n", ret);
            switch(ret) {
                case -1:
                    /* No matches in page tables or TLB */
                    switch(env->mmu_model) {
                        case POWERPC_MMU_SOFT_6xx:
                            env->exception_index = POWERPC_EXCP_IFTLB;
                            env->error_code = 1 << 18;
                            env->spr[SPR_IMISS] = address;
                            env->spr[SPR_ICMP] = 0x80000000 | mmu_ctx.ptem;
                            goto tlb_miss;
                        case POWERPC_MMU_SOFT_74xx:
                            env->exception_index = POWERPC_EXCP_IFTLB;
                            goto tlb_miss_74xx;
                        case POWERPC_MMU_SOFT_4xx:
                        case POWERPC_MMU_SOFT_4xx_Z:
                            env->exception_index = POWERPC_EXCP_ITLB;
                            env->error_code = 0;
                            env->spr[SPR_40x_DEAR] = address;
                            env->spr[SPR_40x_ESR] = 0x00000000;
                            break;
                        case POWERPC_MMU_32B:
                        case POWERPC_MMU_601:
#if defined(TARGET_PPC64)
                        case POWERPC_MMU_620:
                        case POWERPC_MMU_64B:
                        case POWERPC_MMU_2_06:
#endif
                            env->exception_index = POWERPC_EXCP_ISI;
                            env->error_code = 0x40000000;
                            break;
                        case POWERPC_MMU_BOOKE206:
                            booke206_update_mas_tlb_miss(env, address, is_write);
                        /* fall through */
                        case POWERPC_MMU_BOOKE:
                            env->exception_index = POWERPC_EXCP_ITLB;
                            env->error_code = 0;
                            env->spr[SPR_BOOKE_DEAR] = address;
                            return -1;  //  TRANSLATE_FAIL
                        case POWERPC_MMU_MPC8xx:
                            /* XXX: TODO */
                            cpu_abort(env, "MPC8xx MMU model is not implemented\n");
                            break;
                        case POWERPC_MMU_REAL:
                            cpu_abort(env, "PowerPC in real mode should never raise "
                                           "any MMU exceptions\n");
                            return -1;  //  TRANSLATE_FAIL
                        default:
                            cpu_abort(env, "Unknown or invalid MMU model\n");
                            return -1;  //  TRANSLATE_FAIL
                    }
                    break;
                case -2:
                    /* Access rights violation */
                    env->exception_index = POWERPC_EXCP_ISI;
                    env->error_code = 0x08000000;
                    break;
                case -3:
                    /* No execute protection violation */
                    if((env->mmu_model == POWERPC_MMU_BOOKE) || (env->mmu_model == POWERPC_MMU_BOOKE206)) {
                        env->spr[SPR_BOOKE_ESR] = 0x00000000;
                    }
                    env->exception_index = POWERPC_EXCP_ISI;
                    env->error_code = 0x10000000;
                    break;
                case -4:
                    /* Direct store exception */
                    /* No code fetch is allowed in direct-store areas */
                    env->exception_index = POWERPC_EXCP_ISI;
                    env->error_code = 0x10000000;
                    break;
#if defined(TARGET_PPC64)
                case -5:
                    /* No match in segment table */
                    if(env->mmu_model == POWERPC_MMU_620) {
                        env->exception_index = POWERPC_EXCP_ISI;
                        /* XXX: this might be incorrect */
                        env->error_code = 0x40000000;
                    } else {
                        env->exception_index = POWERPC_EXCP_ISEG;
                        env->error_code = 0;
                    }
                    break;
#endif
            }
        } else {
            switch(ret) {
                case -1:
                    /* No matches in page tables or TLB */
                    switch(env->mmu_model) {
                        case POWERPC_MMU_SOFT_6xx:
                            if(is_write) {
                                env->exception_index = POWERPC_EXCP_DSTLB;
                                env->error_code = 1 << 16;
                            } else {
                                env->exception_index = POWERPC_EXCP_DLTLB;
                                env->error_code = 0;
                            }
                            env->spr[SPR_DMISS] = address;
                            env->spr[SPR_DCMP] = 0x80000000 | mmu_ctx.ptem;
                        tlb_miss:
                            env->error_code |= mmu_ctx.key << 19;
                            env->spr[SPR_HASH1] = env->htab_base + get_pteg_offset(env, mmu_ctx.hash[0], HASH_PTE_SIZE_32);
                            env->spr[SPR_HASH2] = env->htab_base + get_pteg_offset(env, mmu_ctx.hash[1], HASH_PTE_SIZE_32);
                            break;
                        case POWERPC_MMU_SOFT_74xx:
                            if(is_write) {
                                env->exception_index = POWERPC_EXCP_DSTLB;
                            } else {
                                env->exception_index = POWERPC_EXCP_DLTLB;
                            }
                        tlb_miss_74xx:
                            /* Implement LRU algorithm */
                            env->error_code = mmu_ctx.key << 19;
                            env->spr[SPR_TLBMISS] = (address & ~((target_ulong)0x3)) | ((env->last_way + 1) & (env->nb_ways - 1));
                            env->spr[SPR_PTEHI] = 0x80000000 | mmu_ctx.ptem;
                            break;
                        case POWERPC_MMU_SOFT_4xx:
                        case POWERPC_MMU_SOFT_4xx_Z:
                            env->exception_index = POWERPC_EXCP_DTLB;
                            env->error_code = 0;
                            env->spr[SPR_40x_DEAR] = address;
                            if(is_write) {
                                env->spr[SPR_40x_ESR] = 0x00800000;
                            } else {
                                env->spr[SPR_40x_ESR] = 0x00000000;
                            }
                            break;
                        case POWERPC_MMU_32B:
                        case POWERPC_MMU_601:
#if defined(TARGET_PPC64)
                        case POWERPC_MMU_620:
                        case POWERPC_MMU_64B:
                        case POWERPC_MMU_2_06:
#endif
                            env->exception_index = POWERPC_EXCP_DSI;
                            env->error_code = 0;
                            env->spr[SPR_DAR] = address;
                            if(is_write) {
                                env->spr[SPR_DSISR] = 0x42000000;
                            } else {
                                env->spr[SPR_DSISR] = 0x40000000;
                            }
                            break;
                        case POWERPC_MMU_MPC8xx:
                            /* XXX: TODO */
                            cpu_abort(env, "MPC8xx MMU model is not implemented\n");
                            break;
                        case POWERPC_MMU_BOOKE206:
                            booke206_update_mas_tlb_miss(env, address, is_write);
                        /* fall through */
                        case POWERPC_MMU_BOOKE:
                            env->exception_index = POWERPC_EXCP_DTLB;
                            env->error_code = 0;
                            env->spr[SPR_BOOKE_DEAR] = address;
                            env->spr[SPR_BOOKE_ESR] = is_write ? ESR_ST : 0;
                            return -1;  //  TRANSLATE_FAIL
                        case POWERPC_MMU_REAL:
                            cpu_abort(env, "PowerPC in real mode should never raise "
                                           "any MMU exceptions\n");
                            return -1;  //  TRANSLATE_FAIL
                        default:
                            cpu_abort(env, "Unknown or invalid MMU model\n");
                            return -1;  //  TRANSLATE_FAIL
                    }
                    break;
                case -2:
                    /* Access rights violation */
                    env->exception_index = POWERPC_EXCP_DSI;
                    env->error_code = 0;
                    if(env->mmu_model == POWERPC_MMU_SOFT_4xx || env->mmu_model == POWERPC_MMU_SOFT_4xx_Z) {
                        env->spr[SPR_40x_DEAR] = address;
                        if(is_write) {
                            env->spr[SPR_40x_ESR] |= 0x00800000;
                        }
                    } else if((env->mmu_model == POWERPC_MMU_BOOKE) || (env->mmu_model == POWERPC_MMU_BOOKE206)) {
                        env->spr[SPR_BOOKE_DEAR] = address;
                        env->spr[SPR_BOOKE_ESR] = is_write ? ESR_ST : 0;
                    } else {
                        env->spr[SPR_DAR] = address;
                        if(is_write) {
                            env->spr[SPR_DSISR] = 0x0A000000;
                        } else {
                            env->spr[SPR_DSISR] = 0x08000000;
                        }
                    }
                    break;
                case -4:
                    /* Direct store exception */
                    switch(ppc_access_type) {
                        case ACCESS_FLOAT:
                            /* Floating point load/store */
                            env->exception_index = POWERPC_EXCP_ALIGN;
                            env->error_code = POWERPC_EXCP_ALIGN_FP;
                            env->spr[SPR_DAR] = address;
                            break;
                        case ACCESS_RES:
                            /* lwarx, ldarx or stwcx. */
                            env->exception_index = POWERPC_EXCP_DSI;
                            env->error_code = 0;
                            env->spr[SPR_DAR] = address;
                            if(is_write) {
                                env->spr[SPR_DSISR] = 0x06000000;
                            } else {
                                env->spr[SPR_DSISR] = 0x04000000;
                            }
                            break;
                        case ACCESS_EXT:
                            /* eciwx or ecowx */
                            env->exception_index = POWERPC_EXCP_DSI;
                            env->error_code = 0;
                            env->spr[SPR_DAR] = address;
                            if(is_write) {
                                env->spr[SPR_DSISR] = 0x06100000;
                            } else {
                                env->spr[SPR_DSISR] = 0x04100000;
                            }
                            break;
                        default:
                            tlib_printf(LOG_LEVEL_ERROR, "invalid exception (%d)\n", ret);
                            env->exception_index = POWERPC_EXCP_PROGRAM;
                            env->error_code = POWERPC_EXCP_INVAL | POWERPC_EXCP_INVAL_INVAL;
                            env->spr[SPR_DAR] = address;
                            break;
                    }
                    break;
#if defined(TARGET_PPC64)
                case -5:
                    /* No match in segment table */
                    if(env->mmu_model == POWERPC_MMU_620) {
                        env->exception_index = POWERPC_EXCP_DSI;
                        env->error_code = 0;
                        env->spr[SPR_DAR] = address;
                        /* XXX: this might be incorrect */
                        if(is_write) {
                            env->spr[SPR_DSISR] = 0x42000000;
                        } else {
                            env->spr[SPR_DSISR] = 0x40000000;
                        }
                    } else {
                        env->exception_index = POWERPC_EXCP_DSEG;
                        env->error_code = 0;
                        env->spr[SPR_DAR] = address;
                    }
                    break;
#endif
            }
        }
        tlib_printf(LOG_LEVEL_WARNING, "%s: set exception to %02x\n", __func__, env->error_code);
        ret = TRANSLATE_FAIL;
    }
    return ret;
}

/*****************************************************************************/
/* BATs management */
static inline void do_invalidate_BAT(CPUState *env, target_ulong BATu, target_ulong mask)
{
    target_ulong base, end, page;

    base = BATu & ~0x0001FFFF;
    end = base + mask + 0x00020000;
    for(page = base; page != end; page += TARGET_PAGE_SIZE) {
        tlb_flush_page(env, page, true);
    }
}

void ppc_store_ibatu(CPUState *env, int nr, target_ulong value)
{
    target_ulong mask;

    if(env->IBAT[0][nr] != value) {
        mask = (value << 15) & 0x0FFE0000UL;
        do_invalidate_BAT(env, env->IBAT[0][nr], mask);
        /* When storing valid upper BAT, mask BEPI and BRPN
         * and invalidate all TLBs covered by this BAT
         */
        mask = (value << 15) & 0x0FFE0000UL;
        env->IBAT[0][nr] = (value & 0x00001FFFUL) | (value & ~0x0001FFFFUL & ~mask);
        env->IBAT[1][nr] = (env->IBAT[1][nr] & 0x0000007B) | (env->IBAT[1][nr] & ~0x0001FFFF & ~mask);
        do_invalidate_BAT(env, env->IBAT[0][nr], mask);
    }
}

void ppc_store_ibatl(CPUState *env, int nr, target_ulong value)
{
    env->IBAT[1][nr] = value;
}

void ppc_store_dbatu(CPUState *env, int nr, target_ulong value)
{
    target_ulong mask;

    if(env->DBAT[0][nr] != value) {
        /* When storing valid upper BAT, mask BEPI and BRPN
         * and invalidate all TLBs covered by this BAT
         */
        mask = (value << 15) & 0x0FFE0000UL;
        do_invalidate_BAT(env, env->DBAT[0][nr], mask);
        mask = (value << 15) & 0x0FFE0000UL;
        env->DBAT[0][nr] = (value & 0x00001FFFUL) | (value & ~0x0001FFFFUL & ~mask);
        env->DBAT[1][nr] = (env->DBAT[1][nr] & 0x0000007B) | (env->DBAT[1][nr] & ~0x0001FFFF & ~mask);
        do_invalidate_BAT(env, env->DBAT[0][nr], mask);
    }
}

void ppc_store_dbatl(CPUState *env, int nr, target_ulong value)
{
    env->DBAT[1][nr] = value;
}

void ppc_store_ibatu_601(CPUState *env, int nr, target_ulong value)
{
    target_ulong mask;

    if(env->IBAT[0][nr] != value) {
        mask = (env->IBAT[1][nr] << 17) & 0x0FFE0000UL;
        if(env->IBAT[1][nr] & 0x40) {
            /* Invalidate BAT only if it is valid */
            do_invalidate_BAT(env, env->IBAT[0][nr], mask);
        }
        /* When storing valid upper BAT, mask BEPI and BRPN
         * and invalidate all TLBs covered by this BAT
         */
        env->IBAT[0][nr] = (value & 0x00001FFFUL) | (value & ~0x0001FFFFUL & ~mask);
        env->DBAT[0][nr] = env->IBAT[0][nr];
        if(env->IBAT[1][nr] & 0x40) {
            do_invalidate_BAT(env, env->IBAT[0][nr], mask);
        }
    }
}

void ppc_store_ibatl_601(CPUState *env, int nr, target_ulong value)
{
    target_ulong mask;

    if(env->IBAT[1][nr] != value) {
        if(env->IBAT[1][nr] & 0x40) {
            mask = (env->IBAT[1][nr] << 17) & 0x0FFE0000UL;
            do_invalidate_BAT(env, env->IBAT[0][nr], mask);
        }
        if(value & 0x40) {
            mask = (value << 17) & 0x0FFE0000UL;
            do_invalidate_BAT(env, env->IBAT[0][nr], mask);
        }
        env->IBAT[1][nr] = value;
        env->DBAT[1][nr] = value;
    }
}

/*****************************************************************************/
/* TLB management */
void ppc_tlb_invalidate_all(CPUState *env)
{
    switch(env->mmu_model) {
        case POWERPC_MMU_SOFT_6xx:
        case POWERPC_MMU_SOFT_74xx:
            ppc6xx_tlb_invalidate_all(env);
            break;
        case POWERPC_MMU_SOFT_4xx:
        case POWERPC_MMU_SOFT_4xx_Z:
            ppc4xx_tlb_invalidate_all(env);
            break;
        case POWERPC_MMU_REAL:
            cpu_abort(env, "No TLB for PowerPC 4xx in real mode\n");
            break;
        case POWERPC_MMU_MPC8xx:
            /* XXX: TODO */
            cpu_abort(env, "MPC8xx MMU model is not implemented\n");
            break;
        case POWERPC_MMU_BOOKE:
            tlb_flush(env, 1, true);
            break;
        case POWERPC_MMU_BOOKE206:
            booke206_flush_tlb(env, -1, 0);
            break;
        case POWERPC_MMU_32B:
        case POWERPC_MMU_601:
#if defined(TARGET_PPC64)
        case POWERPC_MMU_620:
        case POWERPC_MMU_64B:
        case POWERPC_MMU_2_06:
#endif /* defined(TARGET_PPC64) */
            tlb_flush(env, 1, true);
            break;
        default:
            /* XXX: TODO */
            cpu_abort(env, "Unknown MMU model\n");
            break;
    }
}

void ppc_tlb_invalidate_one(CPUState *env, target_ulong addr)
{
    addr &= TARGET_PAGE_MASK;
    switch(env->mmu_model) {
        case POWERPC_MMU_SOFT_6xx:
        case POWERPC_MMU_SOFT_74xx:
            ppc6xx_tlb_invalidate_virt(env, addr, 0);
            if(env->id_tlbs == 1) {
                ppc6xx_tlb_invalidate_virt(env, addr, 1);
            }
            break;
        case POWERPC_MMU_SOFT_4xx:
        case POWERPC_MMU_SOFT_4xx_Z:
            ppc4xx_tlb_invalidate_virt(env, addr, env->spr[SPR_40x_PID]);
            break;
        case POWERPC_MMU_REAL:
            cpu_abort(env, "No TLB for PowerPC 4xx in real mode\n");
            break;
        case POWERPC_MMU_MPC8xx:
            /* XXX: TODO */
            cpu_abort(env, "MPC8xx MMU model is not implemented\n");
            break;
        case POWERPC_MMU_BOOKE:
            /* XXX: TODO */
            cpu_abort(env, "BookE MMU model is not implemented\n");
            break;
        case POWERPC_MMU_BOOKE206:
            /* XXX: TODO */
            cpu_abort(env, "BookE 2.06 MMU model is not implemented\n");
            break;
        case POWERPC_MMU_32B:
        case POWERPC_MMU_601:
            /* tlbie invalidate TLBs for all segments */
            addr &= ~((target_ulong)-1ULL << 28);
            /* XXX: this case should be optimized,
             * giving a mask to tlb_flush_page
             */
            tlb_flush_page(env, addr | (0x0 << 28), true);
            tlb_flush_page(env, addr | (0x1 << 28), true);
            tlb_flush_page(env, addr | (0x2 << 28), true);
            tlb_flush_page(env, addr | (0x3 << 28), true);
            tlb_flush_page(env, addr | (0x4 << 28), true);
            tlb_flush_page(env, addr | (0x5 << 28), true);
            tlb_flush_page(env, addr | (0x6 << 28), true);
            tlb_flush_page(env, addr | (0x7 << 28), true);
            tlb_flush_page(env, addr | (0x8 << 28), true);
            tlb_flush_page(env, addr | (0x9 << 28), true);
            tlb_flush_page(env, addr | (0xA << 28), true);
            tlb_flush_page(env, addr | (0xB << 28), true);
            tlb_flush_page(env, addr | (0xC << 28), true);
            tlb_flush_page(env, addr | (0xD << 28), true);
            tlb_flush_page(env, addr | (0xE << 28), true);
            tlb_flush_page(env, addr | (0xF << 28), true);
            break;
#if defined(TARGET_PPC64)
        case POWERPC_MMU_620:
        case POWERPC_MMU_64B:
        case POWERPC_MMU_2_06:
            /* tlbie invalidate TLBs for all segments */
            /* XXX: given the fact that there are too many segments to invalidate,
             *      and we still don't have a tlb_flush_mask(env, n, mask) in Qemu,
             *      we just invalidate all TLBs
             */
            tlb_flush(env, 1, true);
            break;
#endif /* defined(TARGET_PPC64) */
        default:
            /* XXX: TODO */
            cpu_abort(env, "Unknown MMU model\n");
            break;
    }
}

/*****************************************************************************/
/* Special registers manipulation */

#if defined(TARGET_PPC64)
void ppc_store_asr(CPUState *env, target_ulong value)
{
    if(env->asr != value) {
        env->asr = value;
        tlb_flush(env, 1, false);
    }
}
#endif

void ppc_store_sdr1(CPUState *env, target_ulong value)
{
    if(env->spr[SPR_SDR1] != value) {
        env->spr[SPR_SDR1] = value;
#if defined(TARGET_PPC64)
        if(env->mmu_model & POWERPC_MMU_64) {
            target_ulong htabsize = value & SDR_64_HTABSIZE;

            if(htabsize > 28) {
                tlib_printf(LOG_LEVEL_WARNING, "Invalid HTABSIZE 0x" TARGET_FMT_lx " stored in SDR1. Trimming it to 0x1C.\n",
                            htabsize);
                htabsize = 28;
            }
            env->htab_mask = (1ULL << (htabsize + 18)) - 1;
            env->htab_base = value & SDR_64_HTABORG;
        } else
#endif /* defined(TARGET_PPC64) */
        {
            /* FIXME: Should check for valid HTABMASK values */
            env->htab_mask = ((value & SDR_32_HTABMASK) << 16) | 0xFFFF;
            env->htab_base = value & SDR_32_HTABORG;
        }
        tlb_flush(env, 1, true);
    }
}

#if defined(TARGET_PPC64)
target_ulong ppc_load_sr(CPUState *env, int slb_nr)
{
    //  XXX
    int slot = slb_nr & 0xf;  //  16 first entries into SLB table
    ppc_slb_t *slb = &env->slb[slot];
    return slb->vsid;
}
#endif

void ppc_store_sr(CPUState *env, int srnum, target_ulong value)
{
#if defined(TARGET_PPC64)
    if(env->mmu_model & POWERPC_MMU_64) {
        uint64_t rb = 0, rs = 0;

        /* ESID = srnum */
        rb |= ((uint32_t)srnum & 0xf) << 28;
        /* Set the valid bit */
        rb |= 1 << 27;
        /* Index = ESID */
        rb |= (uint32_t)srnum;

        /* VSID = VSID */
        rs |= (value & 0xfffffff) << 12;
        /* flags = flags */
        rs |= ((value >> 27) & 0xf) << 8;

        ppc_store_slb(env, rb, rs);
    } else
#endif
        if(env->sr[srnum] != value) {
        env->sr[srnum] = value;
/* Invalidating 256MB of virtual memory in 4kB pages is way longer than
   flusing the whole TLB. */
#if !defined(FLUSH_ALL_TLBS) && 0
        {
            target_ulong page, end;
            /* Invalidate 256 MB of virtual memory */
            page = (16 << 20) * srnum;
            end = page + (16 << 20);
            for(; page != end; page += TARGET_PAGE_SIZE) {
                tlb_flush_page(env, page, true);
            }
        }
#else
        tlb_flush(env, 1, true);
#endif
    }
}

/* GDBstub can read and write MSR... */
void ppc_store_msr(CPUState *env, target_ulong value)
{
    hreg_store_msr(env, value, 0);
}

/* Note that this function should be greatly optimized
 * when called with a constant excp, from ppc_hw_interrupt
 */
static inline void powerpc_excp(CPUState *env, int excp_model, int excp)
{
    target_ulong msr, new_msr, vector;
    int srr0, srr1, asrr0, asrr1;
    int lpes0, lpes1, lev;

    if(env->interrupt_begin_callback_enabled) {
        tlib_on_interrupt_begin(excp);
    }

    if(0) {
        /* XXX: find a suitable condition to enable the hypervisor mode */
        lpes0 = (env->spr[SPR_LPCR] >> 1) & 1;
        lpes1 = (env->spr[SPR_LPCR] >> 2) & 1;
    } else {
        /* Those values ensure we won't enter the hypervisor mode */
        lpes0 = 0;
        lpes1 = 1;
    }

    /* new srr1 value excluding must-be-zero bits */
    msr = env->msr & ~0x783f0000ULL;

    /* new interrupt handler msr */
    new_msr = env->msr & ((target_ulong)1 << MSR_ME);

    /* target registers */
    srr0 = SPR_SRR0;
    srr1 = SPR_SRR1;
    asrr0 = -1;
    asrr1 = -1;

    switch(excp) {
        case POWERPC_EXCP_NONE:
            /* Should never happen */
            return;
        case POWERPC_EXCP_CRITICAL: /* Critical input                         */
            switch(excp_model) {
                case POWERPC_EXCP_40x:
                    srr0 = SPR_40x_SRR2;
                    srr1 = SPR_40x_SRR3;
                    break;
                case POWERPC_EXCP_BOOKE:
                    srr0 = SPR_BOOKE_CSRR0;
                    srr1 = SPR_BOOKE_CSRR1;
                    break;
                case POWERPC_EXCP_G2:
                    break;
                default:
                    goto excp_invalid;
            }
            goto store_next;
        case POWERPC_EXCP_MCHECK: /* Machine check exception                  */
            if(msr_me == 0) {
                /* Machine check exception is not enabled.
                 * Enter checkstop state.
                 */
                env->wfi = 1;
                set_interrupt_pending(env, CPU_INTERRUPT_EXITTB);
            }
            if(0) {
                /* XXX: find a suitable condition to enable the hypervisor mode */
                new_msr |= (target_ulong)MSR_HVB;
            }

            /* machine check exceptions don't have ME set */
            new_msr &= ~((target_ulong)1 << MSR_ME);

            /* XXX: should also have something loaded in DAR / DSISR */
            switch(excp_model) {
                case POWERPC_EXCP_40x:
                    srr0 = SPR_40x_SRR2;
                    srr1 = SPR_40x_SRR3;
                    break;
                case POWERPC_EXCP_BOOKE:
                    srr0 = SPR_BOOKE_MCSRR0;
                    srr1 = SPR_BOOKE_MCSRR1;
                    asrr0 = SPR_BOOKE_CSRR0;
                    asrr1 = SPR_BOOKE_CSRR1;
                    break;
                default:
                    break;
            }
            goto store_next;
        case POWERPC_EXCP_DSI: /* Data storage exception                   */
            if(lpes1 == 0) {
                new_msr |= (target_ulong)MSR_HVB;
            }
            goto store_next;
        case POWERPC_EXCP_ISI: /* Instruction storage exception            */
            if(lpes1 == 0) {
                new_msr |= (target_ulong)MSR_HVB;
            }
            msr |= env->error_code;
            goto store_next;
        case POWERPC_EXCP_EXTERNAL: /* External input                           */
            if(lpes0 == 1) {
                new_msr |= (target_ulong)MSR_HVB;
            }
            goto store_next;
        case POWERPC_EXCP_ALIGN: /* Alignment exception                      */
            if(lpes1 == 0) {
                new_msr |= (target_ulong)MSR_HVB;
            }
            /* XXX: this is false */
            /* Get rS/rD and rA from faulting opcode */
            env->spr[SPR_DSISR] |= (ldl_code((env->nip - 4)) & 0x03FF0000) >> 16;
            goto store_current;
        case POWERPC_EXCP_PROGRAM: /* Program exception                        */
            switch(env->error_code & ~0xF) {
                case POWERPC_EXCP_FP:
                    if((msr_fe0 == 0 && msr_fe1 == 0) || msr_fp == 0) {
                        env->exception_index = POWERPC_EXCP_NONE;
                        env->error_code = 0;
                        return;
                    }
                    if(lpes1 == 0) {
                        new_msr |= (target_ulong)MSR_HVB;
                    }
                    msr |= 0x00100000;
                    if(msr_fe0 == msr_fe1) {
                        goto store_next;
                    }
                    msr |= 0x00010000;
                    break;
                case POWERPC_EXCP_INVAL:
                    if(lpes1 == 0) {
                        new_msr |= (target_ulong)MSR_HVB;
                    }
                    msr |= 0x00080000;
                    env->spr[SPR_BOOKE_ESR] = ESR_PIL;
                    break;
                case POWERPC_EXCP_PRIV:
                    if(lpes1 == 0) {
                        new_msr |= (target_ulong)MSR_HVB;
                    }
                    msr |= 0x00040000;
                    env->spr[SPR_BOOKE_ESR] = ESR_PPR;
                    break;
                case POWERPC_EXCP_TRAP:
                    if(lpes1 == 0) {
                        new_msr |= (target_ulong)MSR_HVB;
                    }
                    msr |= 0x00020000;
                    env->spr[SPR_BOOKE_ESR] = ESR_PTR;
                    break;
                default:
                    /* Should never occur */
                    cpu_abort(env, "Invalid program exception %d. Aborting\n", env->error_code);
                    break;
            }
            goto store_current;
        case POWERPC_EXCP_FPU: /* Floating-point unavailable exception     */
            if(lpes1 == 0) {
                new_msr |= (target_ulong)MSR_HVB;
            }
            goto store_current;
        case POWERPC_EXCP_SYSCALL: /* System call exception                    */
            lev = env->error_code;
            if((lev == 1) && cpu_ppc_hypercall) {
                cpu_ppc_hypercall(env);
                return;
            }
            if(lev == 1 || (lpes0 == 0 && lpes1 == 0)) {
                new_msr |= (target_ulong)MSR_HVB;
            }
            goto store_next;
        case POWERPC_EXCP_APU: /* Auxiliary processor unavailable          */
            goto store_current;
        case POWERPC_EXCP_DECR: /* Decrementer exception                    */
            if(lpes1 == 0) {
                new_msr |= (target_ulong)MSR_HVB;
            }
            goto store_next;
        case POWERPC_EXCP_FIT: /* Fixed-interval timer interrupt           */
            /* FIT on 4xx */
            goto store_next;
        case POWERPC_EXCP_WDT: /* Watchdog timer interrupt                 */
            switch(excp_model) {
                case POWERPC_EXCP_BOOKE:
                    srr0 = SPR_BOOKE_CSRR0;
                    srr1 = SPR_BOOKE_CSRR1;
                    break;
                default:
                    break;
            }
            goto store_next;
        case POWERPC_EXCP_DTLB: /* Data TLB error                           */
            goto store_next;
        case POWERPC_EXCP_ITLB: /* Instruction TLB error                    */
            goto store_next;
        case POWERPC_EXCP_DEBUG: /* Debug interrupt                          */
            switch(excp_model) {
                case POWERPC_EXCP_BOOKE:
                    srr0 = SPR_BOOKE_DSRR0;
                    srr1 = SPR_BOOKE_DSRR1;
                    asrr0 = SPR_BOOKE_CSRR0;
                    asrr1 = SPR_BOOKE_CSRR1;
                    break;
                default:
                    break;
            }
            /* XXX: TODO */
            cpu_abort(env, "Debug exception is not implemented yet !\n");
            goto store_next;
        case POWERPC_EXCP_SPEU: /* SPE/embedded floating-point unavailable  */
            env->spr[SPR_BOOKE_ESR] = ESR_SPV;
            goto store_current;
        case POWERPC_EXCP_EFPDI: /* Embedded floating-point data interrupt   */
            /* XXX: TODO */
            cpu_abort(env, "Embedded floating point data exception "
                           "is not implemented yet !\n");
            env->spr[SPR_BOOKE_ESR] = ESR_SPV;
            goto store_next;
        case POWERPC_EXCP_EFPRI: /* Embedded floating-point round interrupt  */
            /* XXX: TODO */
            cpu_abort(env, "Embedded floating point round exception "
                           "is not implemented yet !\n");
            env->spr[SPR_BOOKE_ESR] = ESR_SPV;
            goto store_next;
        case POWERPC_EXCP_EPERFM: /* Embedded performance monitor interrupt   */
            /* XXX: TODO */
            cpu_abort(env, "Performance counter exception is not implemented yet !\n");
            goto store_next;
        case POWERPC_EXCP_DOORI: /* Embedded doorbell interrupt              */
            /* XXX: TODO */
            cpu_abort(env, "Embedded doorbell interrupt is not implemented yet !\n");
            goto store_next;
        case POWERPC_EXCP_DOORCI: /* Embedded doorbell critical interrupt     */
            switch(excp_model) {
                case POWERPC_EXCP_BOOKE:
                    srr0 = SPR_BOOKE_CSRR0;
                    srr1 = SPR_BOOKE_CSRR1;
                    break;
                default:
                    break;
            }
            /* XXX: TODO */
            cpu_abort(env, "Embedded doorbell critical interrupt "
                           "is not implemented yet !\n");
            goto store_next;
        case POWERPC_EXCP_RESET: /* System reset exception                   */
            if(msr_pow) {
                /* indicate that we resumed from power save mode */
                msr |= 0x10000;
            } else {
                new_msr &= ~((target_ulong)1 << MSR_ME);
            }

            if(0) {
                /* XXX: find a suitable condition to enable the hypervisor mode */
                new_msr |= (target_ulong)MSR_HVB;
            }
            goto store_next;
        case POWERPC_EXCP_DSEG: /* Data segment exception                   */
            if(lpes1 == 0) {
                new_msr |= (target_ulong)MSR_HVB;
            }
            goto store_next;
        case POWERPC_EXCP_ISEG: /* Instruction segment exception            */
            if(lpes1 == 0) {
                new_msr |= (target_ulong)MSR_HVB;
            }
            goto store_next;
        case POWERPC_EXCP_HDECR: /* Hypervisor decrementer exception         */
            srr0 = SPR_HSRR0;
            srr1 = SPR_HSRR1;
            new_msr |= (target_ulong)MSR_HVB;
            new_msr |= env->msr & ((target_ulong)1 << MSR_RI);
            goto store_next;
        case POWERPC_EXCP_TRACE: /* Trace exception                          */
            if(lpes1 == 0) {
                new_msr |= (target_ulong)MSR_HVB;
            }
            goto store_next;
        case POWERPC_EXCP_HDSI: /* Hypervisor data storage exception        */
            srr0 = SPR_HSRR0;
            srr1 = SPR_HSRR1;
            new_msr |= (target_ulong)MSR_HVB;
            new_msr |= env->msr & ((target_ulong)1 << MSR_RI);
            goto store_next;
        case POWERPC_EXCP_HISI: /* Hypervisor instruction storage exception */
            srr0 = SPR_HSRR0;
            srr1 = SPR_HSRR1;
            new_msr |= (target_ulong)MSR_HVB;
            new_msr |= env->msr & ((target_ulong)1 << MSR_RI);
            goto store_next;
        case POWERPC_EXCP_HDSEG: /* Hypervisor data segment exception        */
            srr0 = SPR_HSRR0;
            srr1 = SPR_HSRR1;
            new_msr |= (target_ulong)MSR_HVB;
            new_msr |= env->msr & ((target_ulong)1 << MSR_RI);
            goto store_next;
        case POWERPC_EXCP_HISEG: /* Hypervisor instruction segment exception */
            srr0 = SPR_HSRR0;
            srr1 = SPR_HSRR1;
            new_msr |= (target_ulong)MSR_HVB;
            new_msr |= env->msr & ((target_ulong)1 << MSR_RI);
            goto store_next;
        case POWERPC_EXCP_VPU: /* Vector unavailable exception             */
            if(lpes1 == 0) {
                new_msr |= (target_ulong)MSR_HVB;
            }
            goto store_current;
        case POWERPC_EXCP_PIT: /* Programmable interval timer interrupt    */
            goto store_next;
        case POWERPC_EXCP_IO: /* IO error exception                       */
            /* XXX: TODO */
            cpu_abort(env, "601 IO error exception is not implemented yet !\n");
            goto store_next;
        case POWERPC_EXCP_RUNM: /* Run mode exception                       */
            /* XXX: TODO */
            cpu_abort(env, "601 run mode exception is not implemented yet !\n");
            goto store_next;
        case POWERPC_EXCP_EMUL: /* Emulation trap exception                 */
            /* XXX: TODO */
            cpu_abort(env, "602 emulation trap exception "
                           "is not implemented yet !\n");
            goto store_next;
        case POWERPC_EXCP_IFTLB: /* Instruction fetch TLB error              */
            if(lpes1 == 0) {     /* XXX: check this */
                new_msr |= (target_ulong)MSR_HVB;
            }
            switch(excp_model) {
                case POWERPC_EXCP_602:
                case POWERPC_EXCP_603:
                case POWERPC_EXCP_603E:
                case POWERPC_EXCP_G2:
                    goto tlb_miss_tgpr;
                case POWERPC_EXCP_7x5:
                    goto tlb_miss;
                case POWERPC_EXCP_74xx:
                    goto tlb_miss_74xx;
                default:
                    cpu_abort(env, "Invalid instruction TLB miss exception\n");
                    break;
            }
            break;
        case POWERPC_EXCP_DLTLB: /* Data load TLB miss                       */
            if(lpes1 == 0) {     /* XXX: check this */
                new_msr |= (target_ulong)MSR_HVB;
            }
            switch(excp_model) {
                case POWERPC_EXCP_602:
                case POWERPC_EXCP_603:
                case POWERPC_EXCP_603E:
                case POWERPC_EXCP_G2:
                    goto tlb_miss_tgpr;
                case POWERPC_EXCP_7x5:
                    goto tlb_miss;
                case POWERPC_EXCP_74xx:
                    goto tlb_miss_74xx;
                default:
                    cpu_abort(env, "Invalid data load TLB miss exception\n");
                    break;
            }
            break;
        case POWERPC_EXCP_DSTLB: /* Data store TLB miss                      */
            if(lpes1 == 0) {     /* XXX: check this */
                new_msr |= (target_ulong)MSR_HVB;
            }
            switch(excp_model) {
                case POWERPC_EXCP_602:
                case POWERPC_EXCP_603:
                case POWERPC_EXCP_603E:
                case POWERPC_EXCP_G2:
                tlb_miss_tgpr:
                    /* Swap temporary saved registers with GPRs */
                    if(!(new_msr & ((target_ulong)1 << MSR_TGPR))) {
                        new_msr |= (target_ulong)1 << MSR_TGPR;
                        hreg_swap_gpr_tgpr(env);
                    }
                    goto tlb_miss;
                case POWERPC_EXCP_7x5:
                tlb_miss:
                    msr |= env->crf[0] << 28;
                    msr |= env->error_code; /* key, D/I, S/L bits */
                    /* Set way using a LRU mechanism */
                    msr |= ((env->last_way + 1) & (env->nb_ways - 1)) << 17;
                    break;
                case POWERPC_EXCP_74xx:
                tlb_miss_74xx:
                    msr |= env->error_code; /* key bit */
                    break;
                default:
                    cpu_abort(env, "Invalid data store TLB miss exception\n");
                    break;
            }
            goto store_next;
        case POWERPC_EXCP_FPA: /* Floating-point assist exception          */
            /* XXX: TODO */
            cpu_abort(env, "Floating point assist exception "
                           "is not implemented yet !\n");
            goto store_next;
        case POWERPC_EXCP_DABR: /* Data address breakpoint                  */
            /* XXX: TODO */
            cpu_abort(env, "DABR exception is not implemented yet !\n");
            goto store_next;
        case POWERPC_EXCP_IABR: /* Instruction address breakpoint           */
            /* XXX: TODO */
            cpu_abort(env, "IABR exception is not implemented yet !\n");
            goto store_next;
        case POWERPC_EXCP_SMI: /* System management interrupt              */
            /* XXX: TODO */
            cpu_abort(env, "SMI exception is not implemented yet !\n");
            goto store_next;
        case POWERPC_EXCP_THERM: /* Thermal interrupt                        */
            /* XXX: TODO */
            cpu_abort(env, "Thermal management exception "
                           "is not implemented yet !\n");
            goto store_next;
        case POWERPC_EXCP_PERFM: /* Embedded performance monitor interrupt   */
            if(lpes1 == 0) {
                new_msr |= (target_ulong)MSR_HVB;
            }
            /* XXX: TODO */
            cpu_abort(env, "Performance counter exception is not implemented yet !\n");
            goto store_next;
        case POWERPC_EXCP_VPUA: /* Vector assist exception                  */
            /* XXX: TODO */
            cpu_abort(env, "VPU assist exception is not implemented yet !\n");
            goto store_next;
        case POWERPC_EXCP_SOFTP: /* Soft patch exception                     */
            /* XXX: TODO */
            cpu_abort(env, "970 soft-patch exception is not implemented yet !\n");
            goto store_next;
        case POWERPC_EXCP_MAINT: /* Maintenance exception                    */
            /* XXX: TODO */
            cpu_abort(env, "970 maintenance exception is not implemented yet !\n");
            goto store_next;
        case POWERPC_EXCP_MEXTBR: /* Maskable external breakpoint             */
            /* XXX: TODO */
            cpu_abort(env, "Maskable external exception "
                           "is not implemented yet !\n");
            goto store_next;
        case POWERPC_EXCP_NMEXTBR: /* Non maskable external breakpoint         */
            /* XXX: TODO */
            cpu_abort(env, "Non maskable external exception "
                           "is not implemented yet !\n");
            goto store_next;
        default:
        excp_invalid:
            cpu_abort(env, "Invalid PowerPC exception %d. Aborting\n", excp);
            break;
        store_current:
            /* save current instruction location */
            env->spr[srr0] = env->nip - 4;
            break;
        store_next:
            /* save next instruction location */
            env->spr[srr0] = env->nip;
            break;
    }
    /* Save MSR */
    env->spr[srr1] = msr;
    /* If any alternate SRR register are defined, duplicate saved values */
    if(asrr0 != -1) {
        env->spr[asrr0] = env->spr[srr0];
    }
    if(asrr1 != -1) {
        env->spr[asrr1] = env->spr[srr1];
    }
    /* If we disactivated any translation, flush TLBs */
    if(new_msr & ((1 << MSR_IR) | (1 << MSR_DR))) {
        tlb_flush(env, 1, false);
    }

    if(msr_ile) {
        new_msr |= (target_ulong)1 << MSR_LE;
    }

    /* Jump to handler */
    vector = env->excp_vectors[excp];
    if(vector == (target_ulong)-1ULL) {
        cpu_abort(env, "Raised an exception without defined vector %d\n", excp);
    }
    vector |= env->excp_prefix;
#if defined(TARGET_PPC64)
    if(excp_model == POWERPC_EXCP_BOOKE) {
        if(!msr_icm) {
            vector = (uint32_t)vector;
        } else {
            new_msr |= (target_ulong)1 << MSR_CM;
        }
    } else {
        if(!msr_isf && !(env->mmu_model & POWERPC_MMU_64)) {
            vector = (uint32_t)vector;
        } else {
            new_msr |= (target_ulong)1 << MSR_SF;
        }
    }
#endif
    /* XXX: we don't use hreg_store_msr here as already have treated
     *      any special case that could occur. Just store MSR and update hflags
     */
    env->msr = new_msr & env->msr_mask;
    hreg_compute_hflags(env);
    env->nip = vector;
    /* Reset exception state */
    env->exception_index = POWERPC_EXCP_NONE;
    env->error_code = 0;

    if((env->mmu_model == POWERPC_MMU_BOOKE) || (env->mmu_model == POWERPC_MMU_BOOKE206)) {
        /* XXX: The BookE changes address space when switching modes,
                we should probably implement that as different MMU indexes,
                but for the moment we do it the slow way and flush all.  */
        tlb_flush(env, 1, false);
    }
}

void do_interrupt(CPUState *env)
{
    powerpc_excp(env, env->excp_model, env->exception_index);
}

void ppc_hw_interrupt(CPUState *env)
{
    int hdice;

    /* External reset */
    if(env->pending_interrupts & (1 << PPC_INTERRUPT_RESET)) {
        env->pending_interrupts &= ~(1 << PPC_INTERRUPT_RESET);
        powerpc_excp(env, env->excp_model, POWERPC_EXCP_RESET);
        return;
    }
    /* Machine check exception */
    if(env->pending_interrupts & (1 << PPC_INTERRUPT_MCK)) {
        env->pending_interrupts &= ~(1 << PPC_INTERRUPT_MCK);
        powerpc_excp(env, env->excp_model, POWERPC_EXCP_MCHECK);
        return;
    }
    if(0) {
        /* XXX: find a suitable condition to enable the hypervisor mode */
        hdice = env->spr[SPR_LPCR] & 1;
    } else {
        hdice = 0;
    }
    if((msr_ee != 0 || msr_hv == 0 || msr_pr != 0) && hdice != 0) {
        /* Hypervisor decrementer exception */
        if(env->pending_interrupts & (1 << PPC_INTERRUPT_HDECR)) {
            env->pending_interrupts &= ~(1 << PPC_INTERRUPT_HDECR);
            powerpc_excp(env, env->excp_model, POWERPC_EXCP_HDECR);
            return;
        }
    }
    if(msr_ce != 0) {
        /* External critical interrupt */
        if(env->pending_interrupts & (1 << PPC_INTERRUPT_CEXT)) {
            /* Taking a critical external interrupt does not clear the external
             * critical interrupt status
             */
            powerpc_excp(env, env->excp_model, POWERPC_EXCP_CRITICAL);
            return;
        }
    }
    if(msr_ee != 0) {
        /* Watchdog timer on embedded PowerPC */
        if(env->pending_interrupts & (1 << PPC_INTERRUPT_WDT)) {
            env->pending_interrupts &= ~(1 << PPC_INTERRUPT_WDT);
            powerpc_excp(env, env->excp_model, POWERPC_EXCP_WDT);
            return;
        }
        if(env->pending_interrupts & (1 << PPC_INTERRUPT_CDOORBELL)) {
            env->pending_interrupts &= ~(1 << PPC_INTERRUPT_CDOORBELL);
            powerpc_excp(env, env->excp_model, POWERPC_EXCP_DOORCI);
            return;
        }
        /* Fixed interval timer on embedded PowerPC */
        if(env->pending_interrupts & (1 << PPC_INTERRUPT_FIT)) {
            env->pending_interrupts &= ~(1 << PPC_INTERRUPT_FIT);
            powerpc_excp(env, env->excp_model, POWERPC_EXCP_FIT);
            return;
        }
        /* Programmable interval timer on embedded PowerPC */
        if(env->pending_interrupts & (1 << PPC_INTERRUPT_PIT)) {
            env->pending_interrupts &= ~(1 << PPC_INTERRUPT_PIT);
            powerpc_excp(env, env->excp_model, POWERPC_EXCP_PIT);
            return;
        }
        /* Decrementer exception */
        if(env->pending_interrupts & (1 << PPC_INTERRUPT_DECR)) {
            env->pending_interrupts &= ~(1 << PPC_INTERRUPT_DECR);
            powerpc_excp(env, env->excp_model, POWERPC_EXCP_DECR);
            return;
        }
        /* External interrupt */
        if(env->pending_interrupts & (1 << PPC_INTERRUPT_EXT)) {
            /* Taking an external interrupt does not clear the external
             * interrupt status
             */
            powerpc_excp(env, env->excp_model, POWERPC_EXCP_EXTERNAL);
            return;
        }
        if(env->pending_interrupts & (1 << PPC_INTERRUPT_DOORBELL)) {
            env->pending_interrupts &= ~(1 << PPC_INTERRUPT_DOORBELL);
            powerpc_excp(env, env->excp_model, POWERPC_EXCP_DOORI);
            return;
        }
        if(env->pending_interrupts & (1 << PPC_INTERRUPT_PERFM)) {
            env->pending_interrupts &= ~(1 << PPC_INTERRUPT_PERFM);
            powerpc_excp(env, env->excp_model, POWERPC_EXCP_PERFM);
            return;
        }
        /* Thermal interrupt */
        if(env->pending_interrupts & (1 << PPC_INTERRUPT_THERM)) {
            env->pending_interrupts &= ~(1 << PPC_INTERRUPT_THERM);
            powerpc_excp(env, env->excp_model, POWERPC_EXCP_THERM);
            return;
        }
    }
}

void cpu_reset(CPUState *env)
{
    target_ulong msr;

    msr = (target_ulong)1 << MSR_EP;
#if defined(DO_SINGLE_STEP) && 0
    /* Single step trace mode */
    msr |= (target_ulong)1 << MSR_SE;
    msr |= (target_ulong)1 << MSR_BE;
#endif
    env->excp_prefix = env->hreset_excp_prefix;
    env->nip = env->hreset_vector | env->excp_prefix;
    if(env->mmu_model != POWERPC_MMU_REAL) {
        ppc_tlb_invalidate_all(env);
    }
    env->msr = msr & env->msr_mask;
#if defined(TARGET_PPC64)
    if(env->mmu_model & POWERPC_MMU_64) {
        env->msr |= (1ULL << MSR_SF);
    }
#endif
    hreg_compute_hflags(env);
    env->reserve_addr = (target_ulong)-1ULL;
    /* Be sure no exception or interrupt is pending */
    env->pending_interrupts = 0;
    env->exception_index = POWERPC_EXCP_NONE;
    env->error_code = 0;
}

int cpu_init(const char *cpu_model)
{
    const ppc_def_t *def;
    def = cpu_ppc_find_by_name(cpu_model);
    if(!def) {
        return -1;
    }
    cpu_ppc_register_internal(cpu, def);
    return 0;
}

void dispose_opcodes(opc_handler_t **array);

void tlib_arch_dispose()
{
    switch(cpu->tlb_type) {
        case TLB_6XX:
            tlib_free(cpu->tlb.tlb6);
            break;
        case TLB_EMB:
            tlib_free(cpu->tlb.tlbe);
            break;
        case TLB_MAS:
            tlib_free(cpu->tlb.tlbm);
            break;
    }
    dispose_opcodes(cpu->opcodes);
    dispose_opcodes(cpu->vle_opcodes);
}

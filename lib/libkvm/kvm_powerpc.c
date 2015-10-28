/*	$NetBSD: kvm_powerpc.c,v 1.13 2014/01/27 21:00:01 matt Exp $	*/

/*
 * Copyright (c) 2005 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Allen Briggs for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*-
 * Copyright (C) 1996 Wolfgang Solfrank.
 * Copyright (C) 1996 TooLs GmbH.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * PowerPC machine dependent routines for kvm.
 */

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/types.h>

#include <uvm/uvm_extern.h>

#include <db.h>
#include <limits.h>
#include <kvm.h>
#include <stdlib.h>
#include <unistd.h>

#include "kvm_private.h"

#include <sys/kcore.h>
#include <machine/kcore.h>

#include <powerpc/spr.h>
#include <powerpc/oea/spr.h>
#include <powerpc/oea/bat.h>
#include <powerpc/oea/pte.h>

__RCSID("$NetBSD: kvm_powerpc.c,v 1.13 2014/01/27 21:00:01 matt Exp $");

static int	_kvm_match_601bat(kvm_t *, vaddr_t, paddr_t *, int *);
static int	_kvm_match_bat(kvm_t *, vaddr_t, paddr_t *, int *);
static int	_kvm_match_sr(kvm_t *, vaddr_t, paddr_t *, int *);
static struct pte *_kvm_scan_pteg(struct pteg *, uint32_t, uint32_t, int);

void
_kvm_freevtop(kvm_t *kd)
{
	if (kd->vmst != 0)
		free(kd->vmst);
}

/*ARGSUSED*/
int
_kvm_initvtop(kvm_t *kd)
{

	return 0;
}

#define BAT601_SIZE(b)  ((((b) << 17) | ~BAT601_BLPI) + 1)

static int
_kvm_match_601bat(kvm_t *kd, vaddr_t va, paddr_t *pa, int *off)
{
	cpu_kcore_hdr_t	*cpu_kh;
	u_long		pgoff;
	size_t		size;
	int		i, nbat;

	cpu_kh = kd->cpu_data;
	nbat = 4;
	for (i=0 ; i<nbat ; i++) {
		if (!BAT601_VALID_P(cpu_kh->dbatu[i]))
			continue;
		if (BAT601_VA_MATCH_P(cpu_kh->dbatu[i], cpu_kh->dbatl[i], va)) {
			size = BAT601_SIZE(cpu_kh->dbatu[i] & BAT601_BSM);
			pgoff = va & (size-1);
			*pa = (cpu_kh->dbatl[i] & BAT601_PBN) + pgoff;
			*off = size - pgoff;
			return 1;
		}
	}
	return 0;
}

#undef BAT601_SIZE

#define BAT_SIZE(b)     ((((b) << 15) | ~BAT_EPI) + 1)

static int
_kvm_match_bat(kvm_t *kd, vaddr_t va, paddr_t *pa, int *off)
{
	cpu_kcore_hdr_t	*cpu_kh;
	u_long		pgoff;
	size_t		size;
	int		i, nbat;

	cpu_kh = kd->cpu_data;
	/*
	 * Assume that we're looking for data and check only the dbats.
	 */
	nbat = 8;
	for (i=0 ; i<nbat ; i++) {
		if (   ((cpu_kh->dbatu[i] & BAT_Vs) != 0)
		    && (BAT_VA_MATCH_P(cpu_kh->dbatu[i], va))) {
			size = BAT_SIZE(cpu_kh->dbatu[i] & BAT_BL);
			pgoff = va & (size-1);
			*pa = (cpu_kh->dbatl[i] & BAT_RPN) + pgoff;
			*off = size - pgoff;
			return 1;
		}
	}
	return 0;
}

#undef BAT_SIZE

#define SR_VSID_HASH_MASK	0x0007ffff

static struct pte *
_kvm_scan_pteg(struct pteg *pteg, uint32_t vsid, uint32_t api, int secondary)
{
	struct pte	*pte;
	u_long		ptehi;
	int		i;

	for (i=0 ; i<8 ; i++) {
		pte = &pteg->pt[i];
		ptehi = (u_long) pte->pte_hi;
		if ((ptehi & PTE_VALID) == 0)
			continue;
		if ((ptehi & PTE_HID) != secondary)
			continue;
		if (((ptehi & PTE_VSID) >> PTE_VSID_SHFT) != vsid)
			continue;
		if (((ptehi & PTE_API) >> PTE_API_SHFT) != api)
			continue;
		return pte;
	}
	return NULL;
}

#define HASH_MASK	0x0007ffff

static int
_kvm_match_sr(kvm_t *kd, vaddr_t va, paddr_t *pa, int *off)
{
	cpu_kcore_hdr_t	*cpu_kh;
	struct pteg	pteg;
	struct pte	*pte;
	uint32_t	sr, pgoff, vsid, pgidx, api, hash;
	uint32_t	htaborg, htabmask, mhash;
	paddr_t		pteg_vaddr;

	cpu_kh = kd->cpu_data;

	sr = cpu_kh->sr[(va >> 28) & 0xf];
	if ((sr & SR_TYPE) != 0) {
		/* Direct-store segment (shouldn't be) */
		return 0;
	}

	pgoff = va & ADDR_POFF;
	vsid = sr & SR_VSID;
	pgidx = (va & ADDR_PIDX) >> ADDR_PIDX_SHFT;
	api = pgidx >> 10;
	hash = (vsid & HASH_MASK) ^ pgidx;

	htaborg = cpu_kh->sdr1 & 0xffff0000;
	htabmask = cpu_kh->sdr1 & 0x1ff;

	mhash = (hash >> 10) & htabmask;

	pteg_vaddr = ( htaborg & 0xfe000000) | ((hash & 0x3ff) << 6)
		   | ((htaborg & 0x01ff0000) | (mhash << 16));

	if (_kvm_pread(kd, kd->pmfd, (void *) &pteg, sizeof(pteg),
		  _kvm_pa2off(kd, pteg_vaddr)) != sizeof(pteg)) {
		_kvm_syserr(kd, 0, "could not read primary PTEG");
		return 0;
	}

	if ((pte = _kvm_scan_pteg(&pteg, vsid, api, 0)) != NULL) {
		*pa = (pte->pte_lo & PTE_RPGN) | pgoff;
		*off = NBPG - pgoff;
		return 1;
	}

	hash = (~hash) & HASH_MASK;
	mhash = (hash >> 10) & htabmask;

	pteg_vaddr = ( htaborg & 0xfe000000) | ((hash & 0x3ff) << 6)
		   | ((htaborg & 0x01ff0000) | (mhash << 16));

	if (_kvm_pread(kd, kd->pmfd, (void *) &pteg, sizeof(pteg),
		  _kvm_pa2off(kd, pteg_vaddr)) != sizeof(pteg)) {
		_kvm_syserr(kd, 0, "could not read secondary PTEG");
		return 0;
	}

	if ((pte = _kvm_scan_pteg(&pteg, vsid, api, 0)) != NULL) {
		*pa = (pte->pte_lo & PTE_RPGN) | pgoff;
		*off = NBPG - pgoff;
		return 1;
	}

	return 0;
}

/*
 * Translate a KVA to a PA
 */
int
_kvm_kvatop(kvm_t *kd, vaddr_t va, paddr_t *pa)
{
	cpu_kcore_hdr_t	*cpu_kh;
	int		offs;
	uint32_t	pvr;

	if (ISALIVE(kd)) {
		_kvm_err(kd, 0, "vatop called in live kernel!");
		return 0;
	}

	cpu_kh = kd->cpu_data;

	pvr = (cpu_kh->pvr >> 16);
	if (MPC745X_P(pvr))
		pvr = MPC7450;

	switch (pvr) {
	case MPC601:
		/* Check for a BAT hit first */
		if (_kvm_match_601bat(kd, va, pa, &offs)) {
			return offs;
		}

		/* No BAT hit; check page tables */
		if (_kvm_match_sr(kd, va, pa, &offs)) {
			return offs;
		}
		break;

	case MPC603:
	case MPC603e:
	case MPC603ev:
	case MPC604:
	case MPC604ev:
	case MPC750:
	case IBM750FX:
	case MPC7400:
	case MPC7450:
	case MPC7410:
	case MPC8240:
	case MPC8245:
		/* Check for a BAT hit first */
		if (_kvm_match_bat(kd, va, pa, &offs)) {
			return offs;
		}

		/* No BAT hit; check page tables */
		if (_kvm_match_sr(kd, va, pa, &offs)) {
			return offs;
		}
		break;

	default:
		_kvm_err(kd, 0, "Unsupported CPU type (pvr 0x%08lx)!",
		    (unsigned long) cpu_kh->pvr);
		break;
	}

	/* No hit -- no translation */
	*pa = (paddr_t)~0UL;
	return 0;
}

off_t
_kvm_pa2off(kvm_t *kd, paddr_t pa)
{
	cpu_kcore_hdr_t	*cpu_kh;
	phys_ram_seg_t	*ram;
	off_t		off;
	void		*e;

	cpu_kh = kd->cpu_data;
	e = (char *) kd->cpu_data + kd->cpu_dsize;
        ram = (void *)((char *)(void *)cpu_kh + ALIGN(sizeof *cpu_kh));
	off = kd->dump_off;
	do {
		if (pa >= ram->start && (pa - ram->start) < ram->size) {
			return off + (pa - ram->start);
		}
		ram++;
		off += ram->size;
	} while ((void *) ram < e && ram->size);

	_kvm_err(kd, 0, "pa2off failed for pa %#" PRIxPADDR "\n", pa);
	return (off_t) -1;
}

/*
 * Machine-dependent initialization for ALL open kvm descriptors,
 * not just those for a kernel crash dump.  Some architectures
 * have to deal with these NOT being constants!  (i.e. m68k)
 */
int
_kvm_mdopen(kvm_t *kd)
{
	uintptr_t max_uva;
	extern struct ps_strings *__ps_strings;

#if 0   /* XXX - These vary across powerpc machines... */
	kd->usrstack = USRSTACK;
	kd->min_uva = VM_MIN_ADDRESS;
	kd->max_uva = VM_MAXUSER_ADDRESS;
#endif
	/* This is somewhat hack-ish, but it works. */
	max_uva = (uintptr_t) (__ps_strings + 1);
	kd->usrstack = max_uva;
	kd->max_uva  = max_uva;
	kd->min_uva  = 0;

	return (0);
}

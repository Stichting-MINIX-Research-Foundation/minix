/*	$NetBSD: kvm_sparc.c,v 1.34 2015/10/07 11:56:41 martin Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software developed by the Computer Systems
 * Engineering group at Lawrence Berkeley Laboratory under DARPA contract
 * BG 91-66 and contributed to Berkeley.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)kvm_sparc.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: kvm_sparc.c,v 1.34 2015/10/07 11:56:41 martin Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

/*
 * Sparc machine dependent routines for kvm.  Hopefully, the forthcoming
 * vm code will one day obsolete this module.
 */

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/core.h>
#include <sys/kcore.h>
#include <unistd.h>
#include <nlist.h>
#include <kvm.h>

#include <uvm/uvm_extern.h>

#include <sparc/pmap.h>
#include <sparc/kcore.h>

#include <limits.h>
#include <db.h>

#include "kvm_private.h"


static int cputyp = -1;
static int pgshift;
static int nptesg;	/* [sun4/sun4c] only */

#undef VA_VPG
#define VA_VPG(va)	((cputyp == CPU_SUN4C || cputyp == CPU_SUN4M) \
				? VA_SUN4C_VPG(va) \
				: VA_SUN4_VPG(va))

#undef VA_OFF
#define VA_OFF(va) (va & (kd->nbpg - 1))

int _kvm_kvatop44c(kvm_t *, vaddr_t, paddr_t *);
int _kvm_kvatop4m (kvm_t *, vaddr_t, paddr_t *);
int _kvm_kvatop4u (kvm_t *, vaddr_t, paddr_t *);

/*
 * XXX
 * taken from /sys/arch/sparc64/include/kcore.h.
 * this is the same as the sparc one, except for the kphys addition,
 * so luckily we can use this here...
 */
typedef struct sparc64_cpu_kcore_hdr {
	int	cputype;		/* CPU type associated with this dump */
	u_long	kernbase;		/* copy of KERNBASE goes here */
	int	nmemseg;		/* # of physical memory segments */
	u_long	memsegoffset;		/* start of memseg array (relative */
					/*  to the start of this header) */
	int	nsegmap;		/* # of segmaps following */
	u_long	segmapoffset;		/* start of segmap array (relative */
					/*  to the start of this header) */
	int	npmeg;			/* # of PMEGs; [sun4/sun4c] only */
	u_long	pmegoffset;		/* start of pmeg array (relative */
					/*  to the start of this header) */
/* SPARC64 stuff */
	paddr_t	kphys;			/* Physical address of 4MB locked TLB */
} sparc64_cpu_kcore_hdr_t;

void
_kvm_freevtop(kvm_t *kd)
{
	if (kd->vmst != 0) {
		_kvm_err(kd, kd->program, "_kvm_freevtop: internal error");
		kd->vmst = 0;
	}
}

/*
 * Prepare for translation of kernel virtual addresses into offsets
 * into crash dump files. We use the MMU specific goop written at the
 * front of the crash dump by pmap_dumpmmu().
 */
int
_kvm_initvtop(kvm_t *kd)
{
	sparc64_cpu_kcore_hdr_t *cpup = kd->cpu_data;

	switch (cputyp = cpup->cputype) {
	case CPU_SUN4:
	case CPU_SUN4U:
		kd->nbpg = 8196;
		pgshift = 13;
		break;
	case CPU_SUN4C:
	case CPU_SUN4M:
		kd->nbpg = 4096;
		pgshift = 12;
		break;
	default:
		_kvm_err(kd, kd->program, "Unsupported CPU type");
		return (-1);
	}
	nptesg = NBPSG / kd->nbpg;
	return (0);
}

/*
 * Translate a kernel virtual address to a physical address using the
 * mapping information in kd->vm.  Returns the result in pa, and returns
 * the number of bytes that are contiguously available from this
 * physical address.  This routine is used only for crash dumps.
 */
int
_kvm_kvatop(kvm_t *kd, vaddr_t va, paddr_t *pa)
{
	if (cputyp == -1)
		if (_kvm_initvtop(kd) != 0)
			return (-1);

	switch (cputyp) {
	case CPU_SUN4:
	case CPU_SUN4C:
		return _kvm_kvatop44c(kd, va, pa);
		break;
	case CPU_SUN4M:
		return _kvm_kvatop4m(kd, va, pa);
		break;
	case CPU_SUN4U:
	default:
		return _kvm_kvatop4u(kd, va, pa);
	}
}

/*
 * (note: sun4 3-level MMU not yet supported)
 */
int
_kvm_kvatop44c(kvm_t *kd, vaddr_t va, paddr_t *pa)
{
	int vr, vs, pte;
	sparc64_cpu_kcore_hdr_t *cpup = kd->cpu_data;
	struct segmap *sp, *segmaps;
	int *ptes;
	int nkreg, nureg;
	u_long kernbase = cpup->kernbase;

	if (va < kernbase)
		goto err;

	/*
	 * Layout of CPU segment:
	 *	cpu_kcore_hdr_t;
	 *	[alignment]
	 *	phys_ram_seg_t[cpup->nmemseg];
	 *	segmap[cpup->nsegmap];
	 *	ptes[cpup->npmegs];
	 */
	segmaps = (struct segmap *)((long)kd->cpu_data + cpup->segmapoffset);
	ptes = (int *)((int)kd->cpu_data + cpup->pmegoffset);
	nkreg = ((int)((-(unsigned)kernbase) / NBPRG));
	nureg = 256 - nkreg;

	vr = VA_VREG(va);
	vs = VA_VSEG(va);

	sp = &segmaps[(vr-nureg)*NSEGRG + vs];
	if (sp->sg_npte == 0)
		goto err;
	if (sp->sg_pmeg == cpup->npmeg - 1) /* =seginval */
		goto err;
	pte = ptes[sp->sg_pmeg * nptesg + VA_VPG(va)];
	if ((pte & PG_V) != 0) {
		paddr_t p, off = VA_OFF(va);

		p = (pte & PG_PFNUM) << pgshift;
		*pa = p + off;
		return (kd->nbpg - off);
	}
err:
	_kvm_err(kd, 0, "invalid address (%#"PRIxVADDR")", va);
	return (0);
}

int
_kvm_kvatop4m(kvm_t *kd, vaddr_t va, paddr_t *pa)
{
	sparc64_cpu_kcore_hdr_t *cpup = kd->cpu_data;
	int vr, vs;
	int pte;
	off_t foff;
	struct segmap *sp, *segmaps;
	int nkreg, nureg;
	u_long kernbase = cpup->kernbase;

	if (va < kernbase)
		goto err;

	/*
	 * Layout of CPU segment:
	 *	cpu_kcore_hdr_t;
	 *	[alignment]
	 *	phys_ram_seg_t[cpup->nmemseg];
	 *	segmap[cpup->nsegmap];
	 */
	segmaps = (struct segmap *)((long)kd->cpu_data + cpup->segmapoffset);
	nkreg = ((int)((-(unsigned)kernbase) / NBPRG));
	nureg = 256 - nkreg;

	vr = VA_VREG(va);
	vs = VA_VSEG(va);

	sp = &segmaps[(vr-nureg)*NSEGRG + vs];
	if (sp->sg_npte == 0)
		goto err;

	/* XXX - assume page tables in initial kernel DATA or BSS. */
	foff = _kvm_pa2off(kd, (u_long)&sp->sg_pte[VA_VPG(va)] - kernbase);
	if (foff == (off_t)-1)
		return (0);

	if (_kvm_pread(kd, kd->pmfd, &pte, sizeof(pte), foff) != sizeof(pte)) {
		_kvm_syserr(kd, kd->program, "cannot read pte for "
		    "%#" PRIxVADDR, va);
		return (0);
	}

	if ((pte & SRMMU_TETYPE) == SRMMU_TEPTE) {
		long p, off = VA_OFF(va);

		p = (pte & SRMMU_PPNMASK) << SRMMU_PPNPASHIFT;
		*pa = p + off;
		return (kd->nbpg - off);
	}
err:
	_kvm_err(kd, 0, "invalid address (%#"PRIxVADDR")", va);
	return (0);
}

/*
 * sparc64 pmap's 32-bit page table format
 */
int
_kvm_kvatop4u(kvm_t *kd, vaddr_t va, paddr_t *pa)
{
	sparc64_cpu_kcore_hdr_t *cpup = kd->cpu_data;
	int64_t **segmaps;
	int64_t *ptes;
	int64_t pte;
	int64_t kphys = cpup->kphys;
	u_long kernbase = cpup->kernbase;

	if (va < kernbase)
		goto err;

	/*
	 * Kernel layout:
	 *
	 * kernbase:
	 *	4MB locked TLB (text+data+BSS)
	 *	Random other stuff.
	 */
	if (va >= kernbase && va < kernbase + 4*1024*1024)
		return (va - kernbase) + kphys;

/* XXX: from sparc64/include/pmap.h */
#define	SPARC64_PTSZ		(kd->nbpg/8)
#define	SPARC64_STSZ		(SPARC64_PTSZ)
#define	SPARC64_PTMASK		(SPARC64_PTSZ-1)
#define	SPARC64_PTSHIFT		(13)
#define	SPARC64_PDSHIFT		(10+SPARC64_PTSHIFT)
#define	SPARC64_STSHIFT		(10+SPARC64_PDSHIFT)
#define	SPARC64_STMASK		(SPARC64_STSZ-1)
#define	sparc64_va_to_seg(v)	(int)((((int64_t)(v))>>SPARC64_STSHIFT)&SPARC64_STMASK)
#define	sparc64_va_to_pte(v)	(int)((((int64_t)(v))>>SPARC64_PTSHIFT)&SPARC64_PTMASK)

/* XXX: from sparc64/include/pte.h */
#define	SPARC64_TLB_V			0x8000000000000000LL
#define	SPARC64_TLB_PA_MASK		0x000001ffffffe000LL

	/*
	 * Layout of CPU segment:
	 *	cpu_kcore_hdr_t;
	 *	[alignment]
	 *	phys_ram_seg_t[cpup->nmemseg];
	 *	segmap[cpup->nsegmap];
	 */
	segmaps = (int64_t **)((long)kd->cpu_data + cpup->segmapoffset);
	ptes = (int64_t *)(intptr_t)_kvm_pa2off(kd,
	    (paddr_t)(intptr_t)segmaps[sparc64_va_to_seg(va)]);
	pte = ptes[sparc64_va_to_pte(va)];
	if ((pte & SPARC64_TLB_V) != 0)
		return ((pte & SPARC64_TLB_PA_MASK) | (va & (kd->nbpg - 1)));
err:
	_kvm_err(kd, 0, "invalid address (%#"PRIxVADDR")", va);
	return (0);
}


/*
 * Translate a physical address to a file-offset in the crash dump.
 */
off_t
_kvm_pa2off(kvm_t *kd, paddr_t pa)
{
	sparc64_cpu_kcore_hdr_t *cpup = kd->cpu_data;
	phys_ram_seg_t *mp;
	off_t off;
	int nmem;

	/*
	 * Layout of CPU segment:
	 *	cpu_kcore_hdr_t;
	 *	[alignment]
	 *	phys_ram_seg_t[cpup->nmemseg];
	 */
	mp = (phys_ram_seg_t *)((int)kd->cpu_data + cpup->memsegoffset);
	off = 0;

	/* Translate (sparse) pfnum to (packed) dump offset */
	for (nmem = cpup->nmemseg; --nmem >= 0; mp++) {
		if (mp->start <= pa && pa < mp->start + mp->size)
			break;
		off += mp->size;
	}
	if (nmem < 0) {
		_kvm_err(kd, 0, "invalid address (%lx)", (unsigned long)pa);
		return (-1);
	}

	return (kd->dump_off + off + pa - mp->start);
}

/*
 * Machine-dependent initialization for ALL open kvm descriptors,
 * not just those for a kernel crash dump.  Some architectures
 * have to deal with these NOT being constants!  (i.e. m68k)
 */
int
_kvm_mdopen(kvm_t *kd)
{
	u_long max_uva;
	extern struct ps_strings *__ps_strings;

	max_uva = (u_long) (__ps_strings + 1);
	kd->usrstack = max_uva;
	kd->max_uva  = max_uva;
	kd->min_uva  = 0;

	return (0);
}

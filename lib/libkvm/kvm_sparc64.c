/*	$NetBSD: kvm_sparc64.c,v 1.17 2014/02/21 18:00:09 palle Exp $	*/

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
__RCSID("$NetBSD: kvm_sparc64.c,v 1.17 2014/02/21 18:00:09 palle Exp $");
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
#include <sys/types.h>

#include <unistd.h>
#include <nlist.h>
#include <kvm.h>

#include <uvm/uvm_extern.h>

#include <machine/pmap.h>
#include <machine/kcore.h>
#include <machine/vmparam.h>
#include <machine/param.h>

#include <limits.h>
#include <db.h>

#include "kvm_private.h"

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
 *
 * We should read in and cache the ksegs here to speed up operations...
 */
int
_kvm_initvtop(kvm_t *kd)
{
	kd->nbpg = 0x2000;

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
	cpu_kcore_hdr_t *cpup = kd->cpu_data;
	u_long kernbase = cpup->kernbase;
	uint64_t *pseg, *pdir, *ptbl;
	struct cpu_kcore_4mbseg *ktlb;
	int64_t data;
	int i;

	if (va < kernbase)
		goto lose;

	/* Handle the wired 4MB TTEs and per-CPU mappings */
	if (cpup->memsegoffset > sizeof(cpu_kcore_hdr_t) &&
	    cpup->newmagic == SPARC64_KCORE_NEWMAGIC) {
		/*
		 * new format: we have a list of 4 MB mappings
		 */
		ktlb = (struct cpu_kcore_4mbseg *)
			((uintptr_t)kd->cpu_data + cpup->off4mbsegs);
		for (i = 0; i < cpup->num4mbsegs; i++) {
			uint64_t start = ktlb[i].va;
			if (va < start || va >= start+PAGE_SIZE_4M)
				continue;
			*pa = ktlb[i].pa + va - start;
			return (int)(start+PAGE_SIZE_4M - va);
		}

		if (cpup->numcpuinfos > 0) {
			/* we have per-CPU mapping info */
			uint64_t start, base;

			base = cpup->cpubase - 32*1024;
			if (va >= base && va < (base + cpup->percpusz)) {
				start = va - base;
				*pa = cpup->cpusp
				    + cpup->thiscpu*cpup->percpusz
				    + start;
				return cpup->percpusz - start;
			}
		}
	} else {
		/*
		 * old format: just a textbase/size and database/size
		 */
		if (va > cpup->ktextbase && va < 
		    (cpup->ktextbase + cpup->ktextsz)) {
			u_long vaddr;

			vaddr = va - cpup->ktextbase;
			*pa = cpup->ktextp + vaddr;
			return (int)(cpup->ktextsz - vaddr);
		}
		if (va > cpup->kdatabase && va < 
		    (cpup->kdatabase + cpup->kdatasz)) {
			u_long vaddr;

			vaddr = va - cpup->kdatabase;
			*pa = cpup->kdatap + vaddr;
			return (int)(cpup->kdatasz - vaddr);
		}
	}

	/*
	 * Parse kernel page table.
	 */
	pseg = (uint64_t *)(u_long)cpup->segmapoffset;
	if (_kvm_pread(kd, kd->pmfd, &pdir, sizeof(pdir),
		_kvm_pa2off(kd, (paddr_t)&pseg[va_to_seg(va)])) 
		!= sizeof(pdir)) {
		_kvm_syserr(kd, 0, "could not read L1 PTE");
		goto lose;
	}

	if (!pdir) {
		_kvm_err(kd, 0, "invalid L1 PTE");
		goto lose;
	}

	if (_kvm_pread(kd, kd->pmfd, &ptbl, sizeof(ptbl),
		_kvm_pa2off(kd, (paddr_t)&pdir[va_to_dir(va)])) 
		!= sizeof(ptbl)) {
		_kvm_syserr(kd, 0, "could not read L2 PTE");
		goto lose;
	}

	if (!ptbl) {
		_kvm_err(kd, 0, "invalid L2 PTE");
		goto lose;
	}

	if (_kvm_pread(kd, kd->pmfd, &data, sizeof(data),
		_kvm_pa2off(kd, (paddr_t)&ptbl[va_to_pte(va)])) 
		!= sizeof(data)) {
		_kvm_syserr(kd, 0, "could not read TTE");
		goto lose;
	}

	if (data >= 0) {
		_kvm_err(kd, 0, "invalid L2 TTE");
		goto lose;
	}
	
	/* 
	 * Calculate page offsets and things.
	 *
	 * XXXX -- We could support multiple page sizes.
	 */
	va = va & (kd->nbpg - 1);
	data &= SUN4U_TLB_PA_MASK; /* XXX handle sun4u/sun4v */
	*pa = data + va;

	/*
	 * Parse and trnslate our TTE.
	 */

	return (int)(kd->nbpg - va);

lose:
	*pa = (u_long)-1;
	_kvm_err(kd, 0, "invalid address (%#"PRIxVADDR")", va);
	return (0);
}


/*
 * Translate a physical address to a file-offset in the crash dump.
 */
off_t
_kvm_pa2off(kvm_t *kd, paddr_t pa)
{
	cpu_kcore_hdr_t *cpup = kd->cpu_data;
	phys_ram_seg_t *mp;
	off_t off;
	int nmem;

	/*
	 * Layout of CPU segment:
	 *	cpu_kcore_hdr_t;
	 *	[alignment]
	 *	phys_ram_seg_t[cpup->nmemseg];
	 */
	mp = (phys_ram_seg_t *)((long)kd->cpu_data + cpup->memsegoffset);
	off = 0;

	/* Translate (sparse) pfnum to (packed) dump offset */
	for (nmem = cpup->nmemseg; --nmem >= 0; mp++) {
		if (mp->start <= pa && pa < mp->start + mp->size)
			break;
		off += mp->size;
	}
	if (nmem < 0) {
		_kvm_err(kd, 0, "invalid address (%#"PRIxPADDR")", pa);
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

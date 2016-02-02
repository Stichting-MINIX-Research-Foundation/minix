/* $NetBSD: kvm_i386pae.c,v 1.2 2014/02/19 20:21:22 dsl Exp $ */

/*
 * Copyright (c) 2010 Jean-Yves Migeon.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: kvm_i386pae.c,v 1.2 2014/02/19 20:21:22 dsl Exp $");

/*
 * This will expose PAE functions, macros, definitions and constants.
 * Note: this affects all virtual memory related functions. Only their
 * PAE versions can be used below.
 */
#define PAE

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/kcore.h>
#include <sys/types.h>

#include <stdlib.h>
#include <unistd.h>
#include <nlist.h>
#include <kvm.h>

#include <uvm/uvm_extern.h>

#include <limits.h>
#include <db.h>

#include "kvm_private.h"

#include <i386/kcore.h>
#include <i386/pmap.h>
#include <i386/pte.h>
#include <i386/vmparam.h>

int _kvm_kvatop_i386pae(kvm_t *, vaddr_t, paddr_t *);

/*
 * Used to translate a virtual address to a physical address for systems
 * running under PAE mode. Three levels of virtual memory pages are handled
 * here: the per-CPU L3 page, the 4 L2 PDs and the PTs.
 */
int
_kvm_kvatop_i386pae(kvm_t *kd, vaddr_t va, paddr_t *pa)
{
	cpu_kcore_hdr_t *cpu_kh;
	u_long page_off;
	pd_entry_t pde;
	pt_entry_t pte;
	paddr_t pde_pa, pte_pa;

	cpu_kh = kd->cpu_data;
	page_off = va & PGOFSET;
	
	/*
	 * Find and read the PDE. Ignore the L3, as it is only a per-CPU
	 * page, not needed for kernel VA => PA translations.
	 * Remember that the 4 L2 pages are contiguous, so it is safe
	 * to increment pdppaddr to compute the address of the PDE.
	 * pdppaddr being PAGE_SIZE aligned, we mask the option bits.
	 */
	pde_pa = (cpu_kh->pdppaddr & PG_FRAME) + (pl2_pi(va) * sizeof(pde));
	if (_kvm_pread(kd, kd->pmfd, (void *)&pde, sizeof(pde),
	    _kvm_pa2off(kd, pde_pa)) != sizeof(pde)) {
		_kvm_syserr(kd, 0, "could not read PDE");
		goto lose;
	}

	/*
	 * Find and read the page table entry.
	 */
	if ((pde & PG_V) == 0) {
		_kvm_err(kd, 0, "invalid translation (invalid PDE)");
		goto lose;
	}
	if ((pde & PG_PS) != 0) {
		/*
		 * This is a 2MB page.
		 */
		page_off = va & ((vaddr_t)~PG_LGFRAME);
		*pa = (pde & PG_LGFRAME) + page_off;
		return (int)(NBPD_L2 - page_off);
	}

	pte_pa = (pde & PG_FRAME) + (pl1_pi(va) * sizeof(pt_entry_t));
	if (_kvm_pread(kd, kd->pmfd, (void *) &pte, sizeof(pte),
	    _kvm_pa2off(kd, pte_pa)) != sizeof(pte)) {
		_kvm_syserr(kd, 0, "could not read PTE");
		goto lose;
	}

	/*
	 * Validate the PTE and return the physical address.
	 */
	if ((pte & PG_V) == 0) {
		_kvm_err(kd, 0, "invalid translation (invalid PTE)");
		goto lose;
	}
	*pa = (pte & PG_FRAME) + page_off;
	return (int)(NBPG - page_off);

lose:
	*pa = (paddr_t)~0L;
	return 0;

}

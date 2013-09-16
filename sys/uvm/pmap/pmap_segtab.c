/*	$NetBSD: pmap_segtab.c,v 1.1 2012/10/03 00:51:46 christos Exp $	*/

/*-
 * Copyright (c) 1998, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center and by Chris G. Demetriou.
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

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 *
 *	@(#)pmap.c	8.4 (Berkeley) 1/26/94
 */

#include <sys/cdefs.h>

__KERNEL_RCSID(0, "$NetBSD: pmap_segtab.c,v 1.1 2012/10/03 00:51:46 christos Exp $");

/*
 *	Manages physical address maps.
 *
 *	In addition to hardware address maps, this
 *	module is called upon to provide software-use-only
 *	maps which may or may not be stored in the same
 *	form as hardware maps.  These pseudo-maps are
 *	used to store intermediate results from copy
 *	operations to and from address spaces.
 *
 *	Since the information managed by this module is
 *	also stored by the logical address mapping module,
 *	this module may throw away valid virtual-to-physical
 *	mappings at almost any time.  However, invalidations
 *	of virtual-to-physical mappings must be done as
 *	requested.
 *
 *	In order to cope with hardware architectures which
 *	make virtual-to-physical map invalidates expensive,
 *	this module may delay invalidate or reduced protection
 *	operations until such time as they are actually
 *	necessary.  This module is given full information as
 *	to which processors are currently using which maps,
 *	and to when physical maps must be made correct.
 */

#define __PMAP_PRIVATE

#include "opt_multiprocessor.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mutex.h>
#include <sys/atomic.h>

#include <uvm/uvm.h>

CTASSERT(NBPG >= sizeof(pmap_segtab_t));

struct pmap_segtab_info {
	pmap_segtab_t *free_segtab;	/* free list kept locally */
#ifdef DEBUG
	uint32_t nget_segtab;
	uint32_t nput_segtab;
	uint32_t npage_segtab;
#define	SEGTAB_ADD(n, v)	(pmap_segtab_info.n ## _segtab += (v))
#else
#define	SEGTAB_ADD(n, v)	((void) 0)
#endif
#ifdef PMAP_PTP_CACHE
	struct pgflist ptp_pgflist;	/* Keep a list of idle page tables. */
#endif
} pmap_segtab_info = {
#ifdef PMAP_PTP_CACHE
	.ptp_pgflist = LIST_HEAD_INITIALIZER(pmap_segtab_info.ptp_pgflist),
#endif
};

kmutex_t pmap_segtab_lock __cacheline_aligned;

static inline struct vm_page *
pmap_pte_pagealloc(void)
{
	struct vm_page *pg;

	pg = pmap_md_alloc_poolpage(UVM_PGA_ZERO|UVM_PGA_USERESERVE);
	if (pg) {
#ifdef UVM_PAGE_TRKOWN
		pg->owner_tag = NULL;
#endif
		UVM_PAGE_OWN(pg, "pmap-ptp");
	}

	return pg;
}

static inline pt_entry_t *
pmap_segmap(struct pmap *pmap, vaddr_t va)
{
	pmap_segtab_t *stp = pmap->pm_segtab;
	KASSERT(pmap != pmap_kernel() || !pmap_md_direct_mapped_vaddr_p(va));
#ifdef _LP64
	stp = stp->seg_seg[(va >> XSEGSHIFT) & (NSEGPG - 1)];
	if (stp == NULL)
		return NULL;
#endif

	return stp->seg_tab[(va >> SEGSHIFT) & (PMAP_SEGTABSIZE - 1)];
}

pt_entry_t *
pmap_pte_lookup(pmap_t pmap, vaddr_t va)
{
	pt_entry_t *pte = pmap_segmap(pmap, va);
	if (pte == NULL)
		return NULL;

	return pte + ((va >> PGSHIFT) & (NPTEPG - 1));
}

static void
pmap_segtab_free(pmap_segtab_t *stp)
{
	/*
	 * Insert the the segtab into the segtab freelist.
	 */
	mutex_spin_enter(&pmap_segtab_lock);
	stp->seg_seg[0] = pmap_segtab_info.free_segtab;
	pmap_segtab_info.free_segtab = stp;
	SEGTAB_ADD(nput, 1);
	mutex_spin_exit(&pmap_segtab_lock);
}

static void
pmap_segtab_release(pmap_t pmap, pmap_segtab_t **stp_p, bool free_stp,
	pte_callback_t callback, uintptr_t flags,
	vaddr_t va, vsize_t vinc)
{
	pmap_segtab_t *stp = *stp_p;

	for (size_t i = va / vinc; i < PMAP_SEGTABSIZE; i++, va += vinc) {
#ifdef _LP64
		if (vinc > NBSEG) {
			if (stp->seg_seg[i] != NULL) {
				pmap_segtab_release(pmap, &stp->seg_seg[i],
				    true, callback, flags, va, vinc / NSEGPG);
				KASSERT(stp->seg_seg[i] == NULL);
			}
			continue;
		}
#endif
		KASSERT(vinc == NBSEG);

		/* get pointer to segment map */
		pt_entry_t *pte = stp->seg_tab[i];
		if (pte == NULL)
			continue;

		/*
		 * If our caller want a callback, do so.
		 */
		if (callback != NULL) {
			(*callback)(pmap, va, va + vinc, pte, flags);
		}
#ifdef DEBUG
		for (size_t j = 0; j < NPTEPG; j++) {
			if (pte[j])
				panic("%s: pte entry %p not 0 (%#x)",
				    __func__, &pte[j], pte[j]);
		}
#endif
		paddr_t pa = PMAP_UNMAP_POOLPAGE((vaddr_t)pte);
		struct vm_page *pg = PHYS_TO_VM_PAGE(pa);
		pmap_md_vca_clean(pg, (vaddr_t)pte, 0);
#ifdef PMAP_PTP_CACHE
		mutex_spin_enter(&pmap_segtab_lock);
		LIST_INSERT_HEAD(&pmap_segtab_info.ptp_pgflist, pg, listq.list);
		mutex_spin_exit(&pmap_segtab_lock);
#else
		uvm_pagefree(pg);
#endif

		stp->seg_tab[i] = NULL;
	}

	if (free_stp) {
		pmap_segtab_free(stp);
		*stp_p = NULL;
	}
}

/*
 *	Create and return a physical map.
 *
 *	If the size specified for the map
 *	is zero, the map is an actual physical
 *	map, and may be referenced by the
 *	hardware.
 *
 *	If the size specified is non-zero,
 *	the map will be used in software only, and
 *	is bounded by that size.
 */
static pmap_segtab_t *
pmap_segtab_alloc(void)
{
	pmap_segtab_t *stp;

 again:
	mutex_spin_enter(&pmap_segtab_lock);
	if (__predict_true((stp = pmap_segtab_info.free_segtab) != NULL)) {
		pmap_segtab_info.free_segtab = stp->seg_seg[0];
		stp->seg_seg[0] = NULL;
		SEGTAB_ADD(nget, 1);
	}
	mutex_spin_exit(&pmap_segtab_lock);

	if (__predict_false(stp == NULL)) {
		struct vm_page * const stp_pg = pmap_pte_pagealloc();

		if (__predict_false(stp_pg == NULL)) {
			/*
			 * XXX What else can we do?  Could we deadlock here?
			 */
			uvm_wait("pmap_create");
			goto again;
		}
		SEGTAB_ADD(npage, 1);
		const paddr_t stp_pa = VM_PAGE_TO_PHYS(stp_pg);

		stp = (pmap_segtab_t *)PMAP_MAP_POOLPAGE(stp_pa);
		const size_t n = NBPG / sizeof(*stp);
		if (n > 1) {
			/*
			 * link all the segtabs in this page together
			 */
			for (size_t i = 1; i < n - 1; i++) {
				stp[i].seg_seg[0] = &stp[i+1];
			}
			/*
			 * Now link the new segtabs into the free segtab list.
			 */
			mutex_spin_enter(&pmap_segtab_lock);
			stp[n-1].seg_seg[0] = pmap_segtab_info.free_segtab;
			pmap_segtab_info.free_segtab = stp + 1;
			SEGTAB_ADD(nput, n - 1);
			mutex_spin_exit(&pmap_segtab_lock);
		}
	}

#ifdef PARANOIADIAG
	for (i = 0; i < PMAP_SEGTABSIZE; i++) {
		if (stp->seg_tab[i] != 0)
			panic("pmap_create: pm_segtab.seg_tab[%zu] != 0");
	}
#endif
	return stp;
}

/*
 * Allocate the top segment table for the pmap.
 */
void
pmap_segtab_init(pmap_t pmap)
{

	pmap->pm_segtab = pmap_segtab_alloc();
}

/*
 *	Retire the given physical map from service.
 *	Should only be called if the map contains
 *	no valid mappings.
 */
void
pmap_segtab_destroy(pmap_t pmap, pte_callback_t func, uintptr_t flags)
{
	if (pmap->pm_segtab == NULL)
		return;

#ifdef _LP64
	const vsize_t vinc = NBXSEG;
#else
	const vsize_t vinc = NBSEG;
#endif
	pmap_segtab_release(pmap, &pmap->pm_segtab,
	    func == NULL, func, flags, pmap->pm_minaddr, vinc);
}

/*
 *	Make a new pmap (vmspace) active for the given process.
 */
void
pmap_segtab_activate(struct pmap *pm, struct lwp *l)
{
	if (l == curlwp) {
		KASSERT(pm == l->l_proc->p_vmspace->vm_map.pmap);
		if (pm == pmap_kernel()) {
			l->l_cpu->ci_pmap_user_segtab = (void*)0xdeadbabe;
#ifdef _LP64
			l->l_cpu->ci_pmap_user_seg0tab = (void*)0xdeadbabe;
#endif
		} else {
			l->l_cpu->ci_pmap_user_segtab = pm->pm_segtab;
#ifdef _LP64
			l->l_cpu->ci_pmap_user_seg0tab = pm->pm_segtab->seg_seg[0];
#endif
		}
	}
}

/*
 *	Act on the given range of addresses from the specified map.
 *
 *	It is assumed that the start and end are properly rounded to
 *	the page size.
 */
void
pmap_pte_process(pmap_t pmap, vaddr_t sva, vaddr_t eva,
	pte_callback_t callback, uintptr_t flags)
{
#if 0
	printf("%s: %p, %"PRIxVADDR", %"PRIxVADDR", %p, %"PRIxPTR"\n",
	    __func__, pmap, sva, eva, callback, flags);
#endif
	while (sva < eva) {
		vaddr_t lastseg_va = pmap_trunc_seg(sva) + NBSEG;
		if (lastseg_va == 0 || lastseg_va > eva)
			lastseg_va = eva;

		/*
		 * If VA belongs to an unallocated segment,
		 * skip to the next segment boundary.
		 */
		pt_entry_t * const pte = pmap_pte_lookup(pmap, sva);
		if (pte != NULL) {
			/*
			 * Callback to deal with the ptes for this segment.
			 */
			(*callback)(pmap, sva, lastseg_va, pte, flags);
		}
		/*
		 * In theory we could release pages with no entries,
		 * but that takes more effort than we want here.
		 */
		sva = lastseg_va;
	}
}

/*
 *	Return a pointer for the pte that corresponds to the specified virtual
 *	address (va) in the target physical map, allocating if needed.
 */
pt_entry_t *
pmap_pte_reserve(pmap_t pmap, vaddr_t va, int flags)
{
	pmap_segtab_t *stp = pmap->pm_segtab;
	pt_entry_t *pte;

	pte = pmap_pte_lookup(pmap, va);
	if (__predict_false(pte == NULL)) {
#ifdef _LP64
		pmap_segtab_t ** const stp_p =
		    &stp->seg_seg[(va >> XSEGSHIFT) & (NSEGPG - 1)];
		if (__predict_false((stp = *stp_p) == NULL)) {
			pmap_segtab_t *nstp = pmap_segtab_alloc();
#ifdef MULTIPROCESSOR
			pmap_segtab_t *ostp = atomic_cas_ptr(stp_p, NULL, nstp);
			if (__predict_false(ostp != NULL)) {
				pmap_segtab_free(nstp);
				nstp = ostp;
			}
#else
			*stp_p = nstp;
#endif /* MULTIPROCESSOR */
			stp = nstp;
		}
		KASSERT(stp == pmap->pm_segtab->seg_seg[(va >> XSEGSHIFT) & (NSEGPG - 1)]);
#endif /* _LP64 */
		struct vm_page *pg = NULL;
#ifdef PMAP_PTP_CACHE
		mutex_spin_enter(&pmap_segtab_lock);
		if ((pg = LIST_FIRST(&pmap_segtab_info.ptp_pgflist)) != NULL) {
			LIST_REMOVE(pg, listq.list);
			KASSERT(LIST_FIRST(&pmap_segtab_info.ptp_pgflist) != pg);
		}
		mutex_spin_exit(&pmap_segtab_lock);
#endif
		if (pg == NULL)
			pg = pmap_pte_pagealloc();
		if (pg == NULL) {
			if (flags & PMAP_CANFAIL)
				return NULL;
			panic("%s: cannot allocate page table page "
			    "for va %" PRIxVADDR, __func__, va);
		}

		const paddr_t pa = VM_PAGE_TO_PHYS(pg);
		pte = (pt_entry_t *)POOL_PHYSTOV(pa);
		pt_entry_t ** const pte_p =
		    &stp->seg_tab[(va >> SEGSHIFT) & (PMAP_SEGTABSIZE - 1)];
#ifdef MULTIPROCESSOR
		pt_entry_t *opte = atomic_cas_ptr(pte_p, NULL, pte);
		/*
		 * If another thread allocated the segtab needed for this va
		 * free the page we just allocated.
		 */
		if (__predict_false(opte != NULL)) {
#ifdef PMAP_PTP_CACHE
			mutex_spin_enter(&pmap_segtab_lock);
			LIST_INSERT_HEAD(&pmap_segtab_info.ptp_pgflist,
			    pg, listq.list);
			mutex_spin_exit(&pmap_segtab_lock);
#else
			uvm_pagefree(pg);
#endif
			pte = opte;
		}
#else
		*pte_p = pte;
#endif
		KASSERT(pte == stp->seg_tab[(va >> SEGSHIFT) & (PMAP_SEGTABSIZE - 1)]);

		pte += (va >> PGSHIFT) & (NPTEPG - 1);
#ifdef PARANOIADIAG
		for (size_t i = 0; i < NPTEPG; i++) {
			if ((pte+i)->pt_entry)
				panic("pmap_enter: new segmap not empty");
		}
#endif
	}

	return pte;
}

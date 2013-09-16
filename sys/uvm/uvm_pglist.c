/*	$NetBSD: uvm_pglist.c,v 1.62 2011/09/27 01:02:39 jym Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 * uvm_pglist.c: pglist functions
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_pglist.c,v 1.62 2011/09/27 01:02:39 jym Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <uvm/uvm.h>
#include <uvm/uvm_pdpolicy.h>

#ifdef VM_PAGE_ALLOC_MEMORY_STATS
#define	STAT_INCR(v)	(v)++
#define	STAT_DECR(v)	do { \
		if ((v) == 0) \
			printf("%s:%d -- Already 0!\n", __FILE__, __LINE__); \
		else \
			(v)--; \
	} while (/*CONSTCOND*/ 0)
u_long	uvm_pglistalloc_npages;
#else
#define	STAT_INCR(v)
#define	STAT_DECR(v)
#endif

/*
 * uvm_pglistalloc: allocate a list of pages
 *
 * => allocated pages are placed onto an rlist.  rlist is
 *    initialized by uvm_pglistalloc.
 * => returns 0 on success or errno on failure
 * => implementation allocates a single segment if any constraints are
 *	imposed by call arguments.
 * => doesn't take into account clean non-busy pages on inactive list
 *	that could be used(?)
 * => params:
 *	size		the size of the allocation, rounded to page size.
 *	low		the low address of the allowed allocation range.
 *	high		the high address of the allowed allocation range.
 *	alignment	memory must be aligned to this power-of-two boundary.
 *	boundary	no segment in the allocation may cross this
 *			power-of-two boundary (relative to zero).
 */

static void
uvm_pglist_add(struct vm_page *pg, struct pglist *rlist)
{
	int free_list, color, pgflidx;

	KASSERT(mutex_owned(&uvm_fpageqlock));

#if PGFL_NQUEUES != 2
#error uvm_pglistalloc needs to be updated
#endif

	free_list = uvm_page_lookup_freelist(pg);
	color = VM_PGCOLOR_BUCKET(pg);
	pgflidx = (pg->flags & PG_ZERO) ? PGFL_ZEROS : PGFL_UNKNOWN;
#ifdef NOT_DEBUG
	struct vm_page *tp;
	LIST_FOREACH(tp,
	    &uvm.page_free[free_list].pgfl_buckets[color].pgfl_queues[pgflidx],
	    pageq.list) {
		if (tp == pg)
			break;
	}
	if (tp == NULL)
		panic("uvm_pglistalloc: page not on freelist");
#endif
	LIST_REMOVE(pg, pageq.list);	/* global */
	LIST_REMOVE(pg, listq.list);	/* cpu */
	uvmexp.free--;
	if (pg->flags & PG_ZERO)
		uvmexp.zeropages--;
	VM_FREE_PAGE_TO_CPU(pg)->pages[pgflidx]--;
	pg->flags = PG_CLEAN;
	pg->pqflags = 0;
	pg->uobject = NULL;
	pg->uanon = NULL;
	TAILQ_INSERT_TAIL(rlist, pg, pageq.queue);
	STAT_INCR(uvm_pglistalloc_npages);
}

static int
uvm_pglistalloc_c_ps(struct vm_physseg *ps, int num, paddr_t low, paddr_t high,
    paddr_t alignment, paddr_t boundary, struct pglist *rlist)
{
	signed int try, limit, tryidx, end, idx, skip;
	struct vm_page *pgs;
	int pagemask;
	bool second_pass;
#ifdef DEBUG
	paddr_t idxpa, lastidxpa;
	int cidx = 0;	/* XXX: GCC */
#endif
#ifdef PGALLOC_VERBOSE
	printf("pgalloc: contig %d pgs from psi %zd\n", num, ps - vm_physmem);
#endif

	KASSERT(mutex_owned(&uvm_fpageqlock));

	low = atop(low);
	high = atop(high);
	alignment = atop(alignment);

	/*
	 * Make sure that physseg falls within with range to be allocated from.
	 */
	if (high <= ps->avail_start || low >= ps->avail_end)
		return 0;

	/*
	 * We start our search at the just after where the last allocation
	 * succeeded.
	 */
	try = roundup2(max(low, ps->avail_start + ps->start_hint), alignment);
	limit = min(high, ps->avail_end);
	pagemask = ~((boundary >> PAGE_SHIFT) - 1);
	skip = 0;
	second_pass = false;
	pgs = ps->pgs;

	for (;;) {
		bool ok = true;
		signed int cnt;

		if (try + num > limit) {
			if (ps->start_hint == 0 || second_pass) {
				/*
				 * We've run past the allowable range.
				 */
				return 0; /* FAIL = 0 pages*/
			}
			/*
			 * We've wrapped around the end of this segment
			 * so restart at the beginning but now our limit
			 * is were we started.
			 */
			second_pass = true;
			try = roundup2(max(low, ps->avail_start), alignment);
			limit = min(limit, ps->avail_start + ps->start_hint);
			skip = 0;
			continue;
		}
		if (boundary != 0 &&
		    ((try ^ (try + num - 1)) & pagemask) != 0) {
			/*
			 * Region crosses boundary. Jump to the boundary
			 * just crossed and ensure alignment.
			 */
			try = (try + num - 1) & pagemask;
			try = roundup2(try, alignment);
			skip = 0;
			continue;
		}
#ifdef DEBUG
		/*
		 * Make sure this is a managed physical page.
		 */

		if (vm_physseg_find(try, &cidx) != ps - vm_physmem)
			panic("pgalloc contig: botch1");
		if (cidx != try - ps->start)
			panic("pgalloc contig: botch2");
		if (vm_physseg_find(try + num - 1, &cidx) != ps - vm_physmem)
			panic("pgalloc contig: botch3");
		if (cidx != try - ps->start + num - 1)
			panic("pgalloc contig: botch4");
#endif
		tryidx = try - ps->start;
		end = tryidx + num;

		/*
		 * Found a suitable starting page.  See if the range is free.
		 */
#ifdef PGALLOC_VERBOSE
		printf("%s: ps=%p try=%#x end=%#x skip=%#x, align=%#"PRIxPADDR,
		    __func__, ps, tryidx, end, skip, alignment);
#endif
		/*
		 * We start at the end and work backwards since if we find a
		 * non-free page, it makes no sense to continue.
		 *
		 * But on the plus size we have "vetted" some number of free
		 * pages.  If this iteration fails, we may be able to skip
		 * testing most of those pages again in the next pass.
		 */
		for (idx = end - 1; idx >= tryidx + skip; idx--) {
			if (VM_PAGE_IS_FREE(&pgs[idx]) == 0) {
				ok = false;
				break;
			}

#ifdef DEBUG
			if (idx > tryidx) {
				idxpa = VM_PAGE_TO_PHYS(&pgs[idx]);
				lastidxpa = VM_PAGE_TO_PHYS(&pgs[idx - 1]);
				if ((lastidxpa + PAGE_SIZE) != idxpa) {
					/*
					 * Region not contiguous.
					 */
					panic("pgalloc contig: botch5");
				}
				if (boundary != 0 &&
				    ((lastidxpa ^ idxpa) & ~(boundary - 1))
				    != 0) {
					/*
					 * Region crosses boundary.
					 */
					panic("pgalloc contig: botch6");
				}
			}
#endif
		}

		if (ok) {
			while (skip-- > 0) {
				KDASSERT(VM_PAGE_IS_FREE(&pgs[tryidx + skip]));
			}
#ifdef PGALLOC_VERBOSE
			printf(": ok\n");
#endif
			break;
		}

#ifdef PGALLOC_VERBOSE
		printf(": non-free at %#x\n", idx - tryidx);
#endif
		/*
		 * count the number of pages we can advance
		 * since we know they aren't all free.
		 */
		cnt = idx + 1 - tryidx;
		/*
		 * now round up that to the needed alignment.
		 */
		cnt = roundup2(cnt, alignment);
		/*
		 * The number of pages we can skip checking 
		 * (might be 0 if cnt > num).
		 */
		skip = max(num - cnt, 0);
		try += cnt;
	}

	/*
	 * we have a chunk of memory that conforms to the requested constraints.
	 */
	for (idx = tryidx, pgs += idx; idx < end; idx++, pgs++)
		uvm_pglist_add(pgs, rlist);

	/*
	 * the next time we need to search this segment, start after this
	 * chunk of pages we just allocated.
	 */
	ps->start_hint = try + num - ps->avail_start;
	KASSERTMSG(ps->start_hint <= ps->avail_end - ps->avail_start,
	    "%x %u (%#x) <= %#"PRIxPADDR" - %#"PRIxPADDR" (%#"PRIxPADDR")",
	    try + num,
	    ps->start_hint, ps->start_hint, ps->avail_end, ps->avail_start,
	    ps->avail_end - ps->avail_start);

#ifdef PGALLOC_VERBOSE
	printf("got %d pgs\n", num);
#endif
	return num; /* number of pages allocated */
}

static int
uvm_pglistalloc_contig(int num, paddr_t low, paddr_t high, paddr_t alignment,
    paddr_t boundary, struct pglist *rlist)
{
	int fl, psi;
	struct vm_physseg *ps;
	int error;

	/* Default to "lose". */
	error = ENOMEM;

	/*
	 * Block all memory allocation and lock the free list.
	 */
	mutex_spin_enter(&uvm_fpageqlock);

	/* Are there even any free pages? */
	if (uvmexp.free <= (uvmexp.reserve_pagedaemon + uvmexp.reserve_kernel))
		goto out;

	for (fl = 0; fl < VM_NFREELIST; fl++) {
#if (VM_PHYSSEG_STRAT == VM_PSTRAT_BIGFIRST)
		for (psi = vm_nphysseg - 1 ; psi >= 0 ; psi--)
#else
		for (psi = 0 ; psi < vm_nphysseg ; psi++)
#endif
		{
			ps = &vm_physmem[psi];

			if (ps->free_list != fl)
				continue;

			num -= uvm_pglistalloc_c_ps(ps, num, low, high,
						    alignment, boundary, rlist);
			if (num == 0) {
#ifdef PGALLOC_VERBOSE
				printf("pgalloc: %"PRIxMAX"-%"PRIxMAX"\n",
				       (uintmax_t) VM_PAGE_TO_PHYS(TAILQ_FIRST(rlist)),
				       (uintmax_t) VM_PAGE_TO_PHYS(TAILQ_LAST(rlist, pglist)));
#endif
				error = 0;
				goto out;
			}
		}
	}

out:
	/*
	 * check to see if we need to generate some free pages waking
	 * the pagedaemon.
	 */

	uvm_kick_pdaemon();
	mutex_spin_exit(&uvm_fpageqlock);
	return (error);
}

static int
uvm_pglistalloc_s_ps(struct vm_physseg *ps, int num, paddr_t low, paddr_t high,
    struct pglist *rlist)
{
	int todo, limit, try;
	struct vm_page *pg;
	bool second_pass;
#ifdef PGALLOC_VERBOSE
	printf("pgalloc: simple %d pgs from psi %zd\n", num, ps - vm_physmem);
#endif

	KASSERT(mutex_owned(&uvm_fpageqlock));
	KASSERT(ps->start <= ps->avail_start);
	KASSERT(ps->start <= ps->avail_end);
	KASSERT(ps->avail_start <= ps->end);
	KASSERT(ps->avail_end <= ps->end);

	low = atop(low);
	high = atop(high);
	todo = num;
	try = max(low, ps->avail_start + ps->start_hint);
	limit = min(high, ps->avail_end);
	pg = &ps->pgs[try - ps->start];
	second_pass = false;

	/*
	 * Make sure that physseg falls within with range to be allocated from.
	 */
	if (high <= ps->avail_start || low >= ps->avail_end)
		return 0;

again:
	for (;; try++, pg++) {
		if (try >= limit) {
			if (ps->start_hint == 0 || second_pass) {
				try = limit - 1;
				break;
			}
			second_pass = true;
			try = max(low, ps->avail_start);
			limit = min(limit, ps->avail_start + ps->start_hint);
			pg = &ps->pgs[try - ps->start];
			goto again;
		}
#if defined(DEBUG)
		{
			int cidx = 0;
			const int bank = vm_physseg_find(try, &cidx);
			KDASSERTMSG(bank == ps - vm_physmem,
			    "vm_physseg_find(%#x) (%d) != ps %zd",
			     try, bank, ps - vm_physmem);
			KDASSERTMSG(cidx == try - ps->start,
			    "vm_physseg_find(%#x): %#x != off %"PRIxPADDR,
			     try, cidx, try - ps->start);
		}
#endif
		if (VM_PAGE_IS_FREE(pg) == 0)
			continue;

		uvm_pglist_add(pg, rlist);
		if (--todo == 0) {
			break;
		}
	}

	/*
	 * The next time we need to search this segment,
	 * start just after the pages we just allocated.
	 */
	ps->start_hint = try + 1 - ps->avail_start;
	KASSERTMSG(ps->start_hint <= ps->avail_end - ps->avail_start,
	    "%#x %u (%#x) <= %#"PRIxPADDR" - %#"PRIxPADDR" (%#"PRIxPADDR")",
	    try + 1,
	    ps->start_hint, ps->start_hint, ps->avail_end, ps->avail_start,
	    ps->avail_end - ps->avail_start);

#ifdef PGALLOC_VERBOSE
	printf("got %d pgs\n", num - todo);
#endif
	return (num - todo); /* number of pages allocated */
}

static int
uvm_pglistalloc_simple(int num, paddr_t low, paddr_t high,
    struct pglist *rlist, int waitok)
{
	int fl, psi, error;
	struct vm_physseg *ps;

	/* Default to "lose". */
	error = ENOMEM;

again:
	/*
	 * Block all memory allocation and lock the free list.
	 */
	mutex_spin_enter(&uvm_fpageqlock);

	/* Are there even any free pages? */
	if (uvmexp.free <= (uvmexp.reserve_pagedaemon + uvmexp.reserve_kernel))
		goto out;

	for (fl = 0; fl < VM_NFREELIST; fl++) {
#if (VM_PHYSSEG_STRAT == VM_PSTRAT_BIGFIRST)
		for (psi = vm_nphysseg - 1 ; psi >= 0 ; psi--)
#else
		for (psi = 0 ; psi < vm_nphysseg ; psi++)
#endif
		{
			ps = &vm_physmem[psi];

			if (ps->free_list != fl)
				continue;

			num -= uvm_pglistalloc_s_ps(ps, num, low, high, rlist);
			if (num == 0) {
				error = 0;
				goto out;
			}
		}

	}

out:
	/*
	 * check to see if we need to generate some free pages waking
	 * the pagedaemon.
	 */

	uvm_kick_pdaemon();
	mutex_spin_exit(&uvm_fpageqlock);

	if (error) {
		if (waitok) {
			/* XXX perhaps some time limitation? */
#ifdef DEBUG
			printf("pglistalloc waiting\n");
#endif
			uvm_wait("pglalloc");
			goto again;
		} else
			uvm_pglistfree(rlist);
	}
#ifdef PGALLOC_VERBOSE
	if (!error)
		printf("pgalloc: %"PRIxMAX"..%"PRIxMAX"\n",
		       (uintmax_t) VM_PAGE_TO_PHYS(TAILQ_FIRST(rlist)),
		       (uintmax_t) VM_PAGE_TO_PHYS(TAILQ_LAST(rlist, pglist)));
#endif
	return (error);
}

int
uvm_pglistalloc(psize_t size, paddr_t low, paddr_t high, paddr_t alignment,
    paddr_t boundary, struct pglist *rlist, int nsegs, int waitok)
{
	int num, res;

	KASSERT((alignment & (alignment - 1)) == 0);
	KASSERT((boundary & (boundary - 1)) == 0);

	/*
	 * Our allocations are always page granularity, so our alignment
	 * must be, too.
	 */
	if (alignment < PAGE_SIZE)
		alignment = PAGE_SIZE;
	if (boundary != 0 && boundary < size)
		return (EINVAL);
	num = atop(round_page(size));
	low = roundup2(low, alignment);

	TAILQ_INIT(rlist);

	if ((nsegs < size >> PAGE_SHIFT) || (alignment != PAGE_SIZE) ||
	    (boundary != 0))
		res = uvm_pglistalloc_contig(num, low, high, alignment,
					     boundary, rlist);
	else
		res = uvm_pglistalloc_simple(num, low, high, rlist, waitok);

	return (res);
}

/*
 * uvm_pglistfree: free a list of pages
 *
 * => pages should already be unmapped
 */

void
uvm_pglistfree(struct pglist *list)
{
	struct uvm_cpu *ucpu;
	struct vm_page *pg;
	int index, color, queue;
	bool iszero;

	/*
	 * Lock the free list and free each page.
	 */

	mutex_spin_enter(&uvm_fpageqlock);
	ucpu = curcpu()->ci_data.cpu_uvm;
	while ((pg = TAILQ_FIRST(list)) != NULL) {
		KASSERT(!uvmpdpol_pageisqueued_p(pg));
		TAILQ_REMOVE(list, pg, pageq.queue);
		iszero = (pg->flags & PG_ZERO);
		pg->pqflags = PQ_FREE;
#ifdef DEBUG
		pg->uobject = (void *)0xdeadbeef;
		pg->uanon = (void *)0xdeadbeef;
#endif /* DEBUG */
#ifdef DEBUG
		if (iszero)
			uvm_pagezerocheck(pg);
#endif /* DEBUG */
		index = uvm_page_lookup_freelist(pg);
		color = VM_PGCOLOR_BUCKET(pg);
		queue = iszero ? PGFL_ZEROS : PGFL_UNKNOWN;
		pg->offset = (uintptr_t)ucpu;
		LIST_INSERT_HEAD(&uvm.page_free[index].pgfl_buckets[color].
		    pgfl_queues[queue], pg, pageq.list);
		LIST_INSERT_HEAD(&ucpu->page_free[index].pgfl_buckets[color].
		    pgfl_queues[queue], pg, listq.list);
		uvmexp.free++;
		if (iszero)
			uvmexp.zeropages++;
		ucpu->pages[queue]++;
		STAT_DECR(uvm_pglistalloc_npages);
	}
	if (ucpu->pages[PGFL_ZEROS] < ucpu->pages[PGFL_UNKNOWN])
		ucpu->page_idle_zero = vm_page_zero_enable;
	mutex_spin_exit(&uvm_fpageqlock);
}

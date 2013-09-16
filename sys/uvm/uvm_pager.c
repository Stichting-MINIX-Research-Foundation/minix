/*	$NetBSD: uvm_pager.c,v 1.108 2012/01/27 19:48:42 para Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * from: Id: uvm_pager.c,v 1.1.2.23 1998/02/02 20:38:06 chuck Exp
 */

/*
 * uvm_pager.c: generic functions used to assist the pagers.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_pager.c,v 1.108 2012/01/27 19:48:42 para Exp $");

#include "opt_uvmhist.h"
#include "opt_readahead.h"
#include "opt_pagermap.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/buf.h>

#include <uvm/uvm.h>

/*
 * XXX
 * this is needed until the device strategy interface
 * is changed to do physically-addressed i/o.
 */

#ifndef PAGER_MAP_DEFAULT_SIZE
#define PAGER_MAP_DEFAULT_SIZE	(16 * 1024 * 1024)
#endif

#ifndef PAGER_MAP_SIZE
#define PAGER_MAP_SIZE	PAGER_MAP_DEFAULT_SIZE
#endif

size_t pager_map_size = PAGER_MAP_SIZE;

/*
 * list of uvm pagers in the system
 */

const struct uvm_pagerops * const uvmpagerops[] = {
	&aobj_pager,
	&uvm_deviceops,
	&uvm_vnodeops,
	&ubc_pager,
};

/*
 * the pager map: provides KVA for I/O
 */

struct vm_map *pager_map;		/* XXX */
kmutex_t pager_map_wanted_lock;
bool pager_map_wanted;	/* locked by pager map */
static vaddr_t emergva;
static int emerg_ncolors;
static bool emerginuse;

void
uvm_pager_realloc_emerg(void)
{
	vaddr_t new_emergva, old_emergva;
	int old_emerg_ncolors;

	if (__predict_true(emergva != 0 && emerg_ncolors >= uvmexp.ncolors))
		return;

	KASSERT(!emerginuse);

	new_emergva = uvm_km_alloc(kernel_map,
	    round_page(MAXPHYS) + ptoa(uvmexp.ncolors), ptoa(uvmexp.ncolors),
	    UVM_KMF_VAONLY);

	KASSERT(new_emergva != 0);

	old_emergva = emergva;
	old_emerg_ncolors = emerg_ncolors;

	/*
	 * don't support re-color in late boot anyway.
	 */
	if (0) /* XXX */
		mutex_enter(&pager_map_wanted_lock);

	emergva = new_emergva;
	emerg_ncolors = uvmexp.ncolors;
	wakeup(&old_emergva);

	if (0) /* XXX */
		mutex_exit(&pager_map_wanted_lock);

	if (old_emergva)
		uvm_km_free(kernel_map, old_emergva,
		    round_page(MAXPHYS) + ptoa(old_emerg_ncolors),
		    UVM_KMF_VAONLY);
}

/*
 * uvm_pager_init: init pagers (at boot time)
 */

void
uvm_pager_init(void)
{
	u_int lcv;
	vaddr_t sva, eva;

	/*
	 * init pager map
	 */

	sva = 0;
	pager_map = uvm_km_suballoc(kernel_map, &sva, &eva, pager_map_size, 0,
	    false, NULL);
	mutex_init(&pager_map_wanted_lock, MUTEX_DEFAULT, IPL_NONE);
	pager_map_wanted = false;

	uvm_pager_realloc_emerg();

	/*
	 * init ASYNC I/O queue
	 */

	TAILQ_INIT(&uvm.aio_done);

	/*
	 * call pager init functions
	 */
	for (lcv = 0 ; lcv < __arraycount(uvmpagerops); lcv++) {
		if (uvmpagerops[lcv]->pgo_init)
			uvmpagerops[lcv]->pgo_init();
	}
}

/*
 * uvm_pagermapin: map pages into KVA (pager_map) for I/O that needs mappings
 *
 * we basically just map in a blank map entry to reserve the space in the
 * map and then use pmap_enter() to put the mappings in by hand.
 */

vaddr_t
uvm_pagermapin(struct vm_page **pps, int npages, int flags)
{
	vsize_t size;
	vaddr_t kva;
	vaddr_t cva;
	struct vm_page *pp;
	vm_prot_t prot;
	const bool pdaemon = (curlwp == uvm.pagedaemon_lwp);
	const u_int first_color = VM_PGCOLOR_BUCKET(*pps);
	UVMHIST_FUNC("uvm_pagermapin"); UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist,"(pps=0x%x, npages=%d, first_color=%u)",
		pps, npages, first_color, 0);

	/*
	 * compute protection.  outgoing I/O only needs read
	 * access to the page, whereas incoming needs read/write.
	 */

	prot = VM_PROT_READ;
	if (flags & UVMPAGER_MAPIN_READ)
		prot |= VM_PROT_WRITE;

ReStart:
	size = ptoa(npages);
	kva = 0;			/* let system choose VA */

	if (uvm_map(pager_map, &kva, size, NULL, UVM_UNKNOWN_OFFSET,
	    first_color, UVM_FLAG_COLORMATCH | UVM_FLAG_NOMERGE
	    | (pdaemon ? UVM_FLAG_NOWAIT : 0)) != 0) {
		if (pdaemon) {
			mutex_enter(&pager_map_wanted_lock);
			if (emerginuse) {
				UVM_UNLOCK_AND_WAIT(&emergva,
				    &pager_map_wanted_lock, false,
				    "emergva", 0);
				goto ReStart;
			}
			emerginuse = true;
			mutex_exit(&pager_map_wanted_lock);
			kva = emergva + ptoa(first_color);
			/* The shift implicitly truncates to PAGE_SIZE */
			KASSERT(npages <= (MAXPHYS >> PAGE_SHIFT));
			goto enter;
		}
		if ((flags & UVMPAGER_MAPIN_WAITOK) == 0) {
			UVMHIST_LOG(maphist,"<- NOWAIT failed", 0,0,0,0);
			return(0);
		}
		mutex_enter(&pager_map_wanted_lock);
		pager_map_wanted = true;
		UVMHIST_LOG(maphist, "  SLEEPING on pager_map",0,0,0,0);
		UVM_UNLOCK_AND_WAIT(pager_map, &pager_map_wanted_lock, false,
		    "pager_map", 0);
		goto ReStart;
	}

enter:
	/* got it */
	for (cva = kva; npages != 0; npages--, cva += PAGE_SIZE) {
		pp = *pps++;
		KASSERT(pp);
		// KASSERT(!((VM_PAGE_TO_PHYS(pp) ^ cva) & uvmexp.colormask));
		KASSERT(pp->flags & PG_BUSY);
		pmap_kenter_pa(cva, VM_PAGE_TO_PHYS(pp), prot, 0);
	}
	pmap_update(vm_map_pmap(pager_map));

	UVMHIST_LOG(maphist, "<- done (KVA=0x%x)", kva,0,0,0);
	return(kva);
}

/*
 * uvm_pagermapout: remove pager_map mapping
 *
 * we remove our mappings by hand and then remove the mapping (waking
 * up anyone wanting space).
 */

void
uvm_pagermapout(vaddr_t kva, int npages)
{
	vsize_t size = ptoa(npages);
	struct vm_map_entry *entries;
	UVMHIST_FUNC("uvm_pagermapout"); UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist, " (kva=0x%x, npages=%d)", kva, npages,0,0);

	/*
	 * duplicate uvm_unmap, but add in pager_map_wanted handling.
	 */

	pmap_kremove(kva, size);
	pmap_update(pmap_kernel());

	if ((kva & ~ptoa(uvmexp.colormask)) == emergva) {
		mutex_enter(&pager_map_wanted_lock);
		KASSERT(emerginuse);
		emerginuse = false;
		wakeup(&emergva);
		mutex_exit(&pager_map_wanted_lock);
		return;
	}

	vm_map_lock(pager_map);
	uvm_unmap_remove(pager_map, kva, kva + size, &entries, 0);
	mutex_enter(&pager_map_wanted_lock);
	if (pager_map_wanted) {
		pager_map_wanted = false;
		wakeup(pager_map);
	}
	mutex_exit(&pager_map_wanted_lock);
	vm_map_unlock(pager_map);
	if (entries)
		uvm_unmap_detach(entries, 0);
	UVMHIST_LOG(maphist,"<- done",0,0,0,0);
}

/*
 * interrupt-context iodone handler for single-buf i/os
 * or the top-level buf of a nested-buf i/o.
 */

void
uvm_aio_biodone(struct buf *bp)
{
	/* reset b_iodone for when this is a single-buf i/o. */
	bp->b_iodone = uvm_aio_aiodone;

	workqueue_enqueue(uvm.aiodone_queue, &bp->b_work, NULL);
}

void
uvm_aio_aiodone_pages(struct vm_page **pgs, int npages, bool write, int error)
{
	struct uvm_object *uobj;
	struct vm_page *pg;
	kmutex_t *slock;
	int pageout_done;	/* number of PG_PAGEOUT pages processed */
	int swslot;
	int i;
	bool swap;
	UVMHIST_FUNC("uvm_aio_aiodone_pages"); UVMHIST_CALLED(ubchist);

	swslot = 0;
	pageout_done = 0;
	slock = NULL;
	uobj = NULL;
	pg = pgs[0];
	swap = (pg->uanon != NULL && pg->uobject == NULL) ||
		(pg->pqflags & PQ_AOBJ) != 0;
	if (!swap) {
		uobj = pg->uobject;
		slock = uobj->vmobjlock;
		mutex_enter(slock);
		mutex_enter(&uvm_pageqlock);
	} else {
#if defined(VMSWAP)
		if (error) {
			if (pg->uobject != NULL) {
				swslot = uao_find_swslot(pg->uobject,
				    pg->offset >> PAGE_SHIFT);
			} else {
				KASSERT(pg->uanon != NULL);
				swslot = pg->uanon->an_swslot;
			}
			KASSERT(swslot);
		}
#else /* defined(VMSWAP) */
		panic("%s: swap", __func__);
#endif /* defined(VMSWAP) */
	}
	for (i = 0; i < npages; i++) {
#if defined(VMSWAP)
		bool anon_disposed = false; /* XXX gcc */
#endif /* defined(VMSWAP) */

		pg = pgs[i];
		KASSERT(swap || pg->uobject == uobj);
		UVMHIST_LOG(ubchist, "pg %p", pg, 0,0,0);

#if defined(VMSWAP)
		/*
		 * for swap i/os, lock each page's object (or anon)
		 * individually since each page may need a different lock.
		 */

		if (swap) {
			if (pg->uobject != NULL) {
				slock = pg->uobject->vmobjlock;
			} else {
				slock = pg->uanon->an_lock;
			}
			mutex_enter(slock);
			mutex_enter(&uvm_pageqlock);
			anon_disposed = (pg->flags & PG_RELEASED) != 0;
			KASSERT(!anon_disposed || pg->uobject != NULL ||
			    pg->uanon->an_ref == 0);
		}
#endif /* defined(VMSWAP) */

		/*
		 * process errors.  for reads, just mark the page to be freed.
		 * for writes, if the error was ENOMEM, we assume this was
		 * a transient failure so we mark the page dirty so that
		 * we'll try to write it again later.  for all other write
		 * errors, we assume the error is permanent, thus the data
		 * in the page is lost.  bummer.
		 */

		if (error) {
			int slot;
			if (!write) {
				pg->flags |= PG_RELEASED;
				continue;
			} else if (error == ENOMEM) {
				if (pg->flags & PG_PAGEOUT) {
					pg->flags &= ~PG_PAGEOUT;
					pageout_done++;
				}
				pg->flags &= ~PG_CLEAN;
				uvm_pageactivate(pg);
				slot = 0;
			} else
				slot = SWSLOT_BAD;

#if defined(VMSWAP)
			if (swap) {
				if (pg->uobject != NULL) {
					int oldslot;
					oldslot = uao_set_swslot(pg->uobject,
						pg->offset >> PAGE_SHIFT, slot);
					KASSERT(oldslot == swslot + i);
				} else {
					KASSERT(pg->uanon->an_swslot ==
						swslot + i);
					pg->uanon->an_swslot = slot;
				}
			}
#endif /* defined(VMSWAP) */
		}

		/*
		 * if the page is PG_FAKE, this must have been a read to
		 * initialize the page.  clear PG_FAKE and activate the page.
		 * we must also clear the pmap "modified" flag since it may
		 * still be set from the page's previous identity.
		 */

		if (pg->flags & PG_FAKE) {
			KASSERT(!write);
			pg->flags &= ~PG_FAKE;
#if defined(READAHEAD_STATS)
			pg->pqflags |= PQ_READAHEAD;
			uvm_ra_total.ev_count++;
#endif /* defined(READAHEAD_STATS) */
			KASSERT((pg->flags & PG_CLEAN) != 0);
			uvm_pageenqueue(pg);
			pmap_clear_modify(pg);
		}

		/*
		 * do accounting for pagedaemon i/o and arrange to free
		 * the pages instead of just unbusying them.
		 */

		if (pg->flags & PG_PAGEOUT) {
			pg->flags &= ~PG_PAGEOUT;
			pageout_done++;
			uvmexp.pdfreed++;
			pg->flags |= PG_RELEASED;
		}

#if defined(VMSWAP)
		/*
		 * for swap pages, unlock everything for this page now.
		 */

		if (swap) {
			if (pg->uobject == NULL && anon_disposed) {
				mutex_exit(&uvm_pageqlock);
				uvm_anon_release(pg->uanon);
			} else {
				uvm_page_unbusy(&pg, 1);
				mutex_exit(&uvm_pageqlock);
				mutex_exit(slock);
			}
		}
#endif /* defined(VMSWAP) */
	}
	uvm_pageout_done(pageout_done);
	if (!swap) {
		uvm_page_unbusy(pgs, npages);
		mutex_exit(&uvm_pageqlock);
		mutex_exit(slock);
	} else {
#if defined(VMSWAP)
		KASSERT(write);

		/* these pages are now only in swap. */
		mutex_enter(&uvm_swap_data_lock);
		KASSERT(uvmexp.swpgonly + npages <= uvmexp.swpginuse);
		if (error != ENOMEM)
			uvmexp.swpgonly += npages;
		mutex_exit(&uvm_swap_data_lock);
		if (error) {
			if (error != ENOMEM)
				uvm_swap_markbad(swslot, npages);
			else
				uvm_swap_free(swslot, npages);
		}
		uvmexp.pdpending--;
#endif /* defined(VMSWAP) */
	}
}

/*
 * uvm_aio_aiodone: do iodone processing for async i/os.
 * this should be called in thread context, not interrupt context.
 */

void
uvm_aio_aiodone(struct buf *bp)
{
	int npages = bp->b_bufsize >> PAGE_SHIFT;
	struct vm_page *pgs[npages];
	int i, error;
	bool write;
	UVMHIST_FUNC("uvm_aio_aiodone"); UVMHIST_CALLED(ubchist);
	UVMHIST_LOG(ubchist, "bp %p", bp, 0,0,0);

	error = bp->b_error;
	write = (bp->b_flags & B_READ) == 0;

	for (i = 0; i < npages; i++) {
		pgs[i] = uvm_pageratop((vaddr_t)bp->b_data + (i << PAGE_SHIFT));
		UVMHIST_LOG(ubchist, "pgs[%d] = %p", i, pgs[i],0,0);
	}
	uvm_pagermapout((vaddr_t)bp->b_data, npages);

	uvm_aio_aiodone_pages(pgs, npages, write, error);

	if (write && (bp->b_cflags & BC_AGE) != 0) {
		mutex_enter(bp->b_objlock);
		vwakeup(bp);
		mutex_exit(bp->b_objlock);
	}
	putiobuf(bp);
}

/*
 * uvm_pageratop: convert KVAs in the pager map back to their page
 * structures.
 */

struct vm_page *
uvm_pageratop(vaddr_t kva)
{
	struct vm_page *pg;
	paddr_t pa;
	bool rv;

	rv = pmap_extract(pmap_kernel(), kva, &pa);
	KASSERT(rv);
	pg = PHYS_TO_VM_PAGE(pa);
	KASSERT(pg != NULL);
	return (pg);
}

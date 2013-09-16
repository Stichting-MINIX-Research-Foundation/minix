/*	$NetBSD: uvm_pdaemon.c,v 1.107 2012/07/30 23:56:48 matt Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * Copyright (c) 1991, 1993, The Regents of the University of California.
 *
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
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
 *	@(#)vm_pageout.c        8.5 (Berkeley) 2/14/94
 * from: Id: uvm_pdaemon.c,v 1.1.2.32 1998/02/06 05:26:30 chs Exp
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * uvm_pdaemon.c: the page daemon
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_pdaemon.c,v 1.107 2012/07/30 23:56:48 matt Exp $");

#include "opt_uvmhist.h"
#include "opt_readahead.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/pool.h>
#include <sys/buf.h>
#include <sys/module.h>
#include <sys/atomic.h>

#include <uvm/uvm.h>
#include <uvm/uvm_pdpolicy.h>

#ifdef UVMHIST
UVMHIST_DEFINE(pdhist);
#endif

/*
 * UVMPD_NUMDIRTYREACTS is how many dirty pages the pagedaemon will reactivate
 * in a pass thru the inactive list when swap is full.  the value should be
 * "small"... if it's too large we'll cycle the active pages thru the inactive
 * queue too quickly to for them to be referenced and avoid being freed.
 */

#define	UVMPD_NUMDIRTYREACTS	16

#define	UVMPD_NUMTRYLOCKOWNER	16

/*
 * local prototypes
 */

static void	uvmpd_scan(void);
static void	uvmpd_scan_queue(void);
static void	uvmpd_tune(void);

static unsigned int uvm_pagedaemon_waiters;

/*
 * XXX hack to avoid hangs when large processes fork.
 */
u_int uvm_extrapages;

/*
 * uvm_wait: wait (sleep) for the page daemon to free some pages
 *
 * => should be called with all locks released
 * => should _not_ be called by the page daemon (to avoid deadlock)
 */

void
uvm_wait(const char *wmsg)
{
	int timo = 0;

	mutex_spin_enter(&uvm_fpageqlock);

	/*
	 * check for page daemon going to sleep (waiting for itself)
	 */

	if (curlwp == uvm.pagedaemon_lwp && uvmexp.paging == 0) {
		/*
		 * now we have a problem: the pagedaemon wants to go to
		 * sleep until it frees more memory.   but how can it
		 * free more memory if it is asleep?  that is a deadlock.
		 * we have two options:
		 *  [1] panic now
		 *  [2] put a timeout on the sleep, thus causing the
		 *      pagedaemon to only pause (rather than sleep forever)
		 *
		 * note that option [2] will only help us if we get lucky
		 * and some other process on the system breaks the deadlock
		 * by exiting or freeing memory (thus allowing the pagedaemon
		 * to continue).  for now we panic if DEBUG is defined,
		 * otherwise we hope for the best with option [2] (better
		 * yet, this should never happen in the first place!).
		 */

		printf("pagedaemon: deadlock detected!\n");
		timo = hz >> 3;		/* set timeout */
#if defined(DEBUG)
		/* DEBUG: panic so we can debug it */
		panic("pagedaemon deadlock");
#endif
	}

	uvm_pagedaemon_waiters++;
	wakeup(&uvm.pagedaemon);		/* wake the daemon! */
	UVM_UNLOCK_AND_WAIT(&uvmexp.free, &uvm_fpageqlock, false, wmsg, timo);
}

/*
 * uvm_kick_pdaemon: perform checks to determine if we need to
 * give the pagedaemon a nudge, and do so if necessary.
 *
 * => called with uvm_fpageqlock held.
 */

void
uvm_kick_pdaemon(void)
{

	KASSERT(mutex_owned(&uvm_fpageqlock));

	if (uvmexp.free + uvmexp.paging < uvmexp.freemin ||
	    (uvmexp.free + uvmexp.paging < uvmexp.freetarg &&
	     uvmpdpol_needsscan_p()) ||
	     uvm_km_va_starved_p()) {
		wakeup(&uvm.pagedaemon);
	}
}

/*
 * uvmpd_tune: tune paging parameters
 *
 * => called when ever memory is added (or removed?) to the system
 * => caller must call with page queues locked
 */

static void
uvmpd_tune(void)
{
	int val;

	UVMHIST_FUNC("uvmpd_tune"); UVMHIST_CALLED(pdhist);

	/*
	 * try to keep 0.5% of available RAM free, but limit to between
	 * 128k and 1024k per-CPU.  XXX: what are these values good for?
	 */
	val = uvmexp.npages / 200;
	val = MAX(val, (128*1024) >> PAGE_SHIFT);
	val = MIN(val, (1024*1024) >> PAGE_SHIFT);
	val *= ncpu;

	/* Make sure there's always a user page free. */
	if (val < uvmexp.reserve_kernel + 1)
		val = uvmexp.reserve_kernel + 1;
	uvmexp.freemin = val;

	/* Calculate free target. */
	val = (uvmexp.freemin * 4) / 3;
	if (val <= uvmexp.freemin)
		val = uvmexp.freemin + 1;
	uvmexp.freetarg = val + atomic_swap_uint(&uvm_extrapages, 0);

	uvmexp.wiredmax = uvmexp.npages / 3;
	UVMHIST_LOG(pdhist, "<- done, freemin=%d, freetarg=%d, wiredmax=%d",
	      uvmexp.freemin, uvmexp.freetarg, uvmexp.wiredmax, 0);
}

/*
 * uvm_pageout: the main loop for the pagedaemon
 */

void
uvm_pageout(void *arg)
{
	int bufcnt, npages = 0;
	int extrapages = 0;
	struct pool *pp;
	
	UVMHIST_FUNC("uvm_pageout"); UVMHIST_CALLED(pdhist);

	UVMHIST_LOG(pdhist,"<starting uvm pagedaemon>", 0, 0, 0, 0);

	/*
	 * ensure correct priority and set paging parameters...
	 */

	uvm.pagedaemon_lwp = curlwp;
	mutex_enter(&uvm_pageqlock);
	npages = uvmexp.npages;
	uvmpd_tune();
	mutex_exit(&uvm_pageqlock);

	/*
	 * main loop
	 */

	for (;;) {
		bool needsscan, needsfree, kmem_va_starved;

		kmem_va_starved = uvm_km_va_starved_p();

		mutex_spin_enter(&uvm_fpageqlock);
		if ((uvm_pagedaemon_waiters == 0 || uvmexp.paging > 0) &&
		    !kmem_va_starved) {
			UVMHIST_LOG(pdhist,"  <<SLEEPING>>",0,0,0,0);
			UVM_UNLOCK_AND_WAIT(&uvm.pagedaemon,
			    &uvm_fpageqlock, false, "pgdaemon", 0);
			uvmexp.pdwoke++;
			UVMHIST_LOG(pdhist,"  <<WOKE UP>>",0,0,0,0);
		} else {
			mutex_spin_exit(&uvm_fpageqlock);
		}

		/*
		 * now lock page queues and recompute inactive count
		 */

		mutex_enter(&uvm_pageqlock);
		if (npages != uvmexp.npages || extrapages != uvm_extrapages) {
			npages = uvmexp.npages;
			extrapages = uvm_extrapages;
			mutex_spin_enter(&uvm_fpageqlock);
			uvmpd_tune();
			mutex_spin_exit(&uvm_fpageqlock);
		}

		uvmpdpol_tune();

		/*
		 * Estimate a hint.  Note that bufmem are returned to
		 * system only when entire pool page is empty.
		 */
		mutex_spin_enter(&uvm_fpageqlock);
		bufcnt = uvmexp.freetarg - uvmexp.free;
		if (bufcnt < 0)
			bufcnt = 0;

		UVMHIST_LOG(pdhist,"  free/ftarg=%d/%d",
		    uvmexp.free, uvmexp.freetarg, 0,0);

		needsfree = uvmexp.free + uvmexp.paging < uvmexp.freetarg;
		needsscan = needsfree || uvmpdpol_needsscan_p();

		/*
		 * scan if needed
		 */
		if (needsscan) {
			mutex_spin_exit(&uvm_fpageqlock);
			uvmpd_scan();
			mutex_spin_enter(&uvm_fpageqlock);
		}

		/*
		 * if there's any free memory to be had,
		 * wake up any waiters.
		 */
		if (uvmexp.free > uvmexp.reserve_kernel ||
		    uvmexp.paging == 0) {
			wakeup(&uvmexp.free);
			uvm_pagedaemon_waiters = 0;
		}
		mutex_spin_exit(&uvm_fpageqlock);

		/*
		 * scan done.  unlock page queues (the only lock we are holding)
		 */
		mutex_exit(&uvm_pageqlock);

		/*
		 * if we don't need free memory, we're done.
		 */

		if (!needsfree && !kmem_va_starved)
			continue;

		/*
		 * kill unused metadata buffers.
		 */
		mutex_enter(&bufcache_lock);
		buf_drain(bufcnt << PAGE_SHIFT);
		mutex_exit(&bufcache_lock);

		/*
		 * drain the pools.
		 */
		pool_drain(&pp);
	}
	/*NOTREACHED*/
}


/*
 * uvm_aiodone_worker: a workqueue callback for the aiodone daemon.
 */

void
uvm_aiodone_worker(struct work *wk, void *dummy)
{
	struct buf *bp = (void *)wk;

	KASSERT(&bp->b_work == wk);

	/*
	 * process an i/o that's done.
	 */

	(*bp->b_iodone)(bp);
}

void
uvm_pageout_start(int npages)
{

	mutex_spin_enter(&uvm_fpageqlock);
	uvmexp.paging += npages;
	mutex_spin_exit(&uvm_fpageqlock);
}

void
uvm_pageout_done(int npages)
{

	mutex_spin_enter(&uvm_fpageqlock);
	KASSERT(uvmexp.paging >= npages);
	uvmexp.paging -= npages;

	/*
	 * wake up either of pagedaemon or LWPs waiting for it.
	 */

	if (uvmexp.free <= uvmexp.reserve_kernel) {
		wakeup(&uvm.pagedaemon);
	} else {
		wakeup(&uvmexp.free);
		uvm_pagedaemon_waiters = 0;
	}
	mutex_spin_exit(&uvm_fpageqlock);
}

/*
 * uvmpd_trylockowner: trylock the page's owner.
 *
 * => called with pageq locked.
 * => resolve orphaned O->A loaned page.
 * => return the locked mutex on success.  otherwise, return NULL.
 */

kmutex_t *
uvmpd_trylockowner(struct vm_page *pg)
{
	struct uvm_object *uobj = pg->uobject;
	kmutex_t *slock;

	KASSERT(mutex_owned(&uvm_pageqlock));

	if (uobj != NULL) {
		slock = uobj->vmobjlock;
	} else {
		struct vm_anon *anon = pg->uanon;

		KASSERT(anon != NULL);
		slock = anon->an_lock;
	}

	if (!mutex_tryenter(slock)) {
		return NULL;
	}

	if (uobj == NULL) {

		/*
		 * set PQ_ANON if it isn't set already.
		 */

		if ((pg->pqflags & PQ_ANON) == 0) {
			KASSERT(pg->loan_count > 0);
			pg->loan_count--;
			pg->pqflags |= PQ_ANON;
			/* anon now owns it */
		}
	}

	return slock;
}

#if defined(VMSWAP)
struct swapcluster {
	int swc_slot;
	int swc_nallocated;
	int swc_nused;
	struct vm_page *swc_pages[howmany(MAXPHYS, MIN_PAGE_SIZE)];
};

static void
swapcluster_init(struct swapcluster *swc)
{

	swc->swc_slot = 0;
	swc->swc_nused = 0;
}

static int
swapcluster_allocslots(struct swapcluster *swc)
{
	int slot;
	int npages;

	if (swc->swc_slot != 0) {
		return 0;
	}

	/* Even with strange MAXPHYS, the shift
	   implicitly rounds down to a page. */
	npages = MAXPHYS >> PAGE_SHIFT;
	slot = uvm_swap_alloc(&npages, true);
	if (slot == 0) {
		return ENOMEM;
	}
	swc->swc_slot = slot;
	swc->swc_nallocated = npages;
	swc->swc_nused = 0;

	return 0;
}

static int
swapcluster_add(struct swapcluster *swc, struct vm_page *pg)
{
	int slot;
	struct uvm_object *uobj;

	KASSERT(swc->swc_slot != 0);
	KASSERT(swc->swc_nused < swc->swc_nallocated);
	KASSERT((pg->pqflags & PQ_SWAPBACKED) != 0);

	slot = swc->swc_slot + swc->swc_nused;
	uobj = pg->uobject;
	if (uobj == NULL) {
		KASSERT(mutex_owned(pg->uanon->an_lock));
		pg->uanon->an_swslot = slot;
	} else {
		int result;

		KASSERT(mutex_owned(uobj->vmobjlock));
		result = uao_set_swslot(uobj, pg->offset >> PAGE_SHIFT, slot);
		if (result == -1) {
			return ENOMEM;
		}
	}
	swc->swc_pages[swc->swc_nused] = pg;
	swc->swc_nused++;

	return 0;
}

static void
swapcluster_flush(struct swapcluster *swc, bool now)
{
	int slot;
	int nused;
	int nallocated;
	int error;

	if (swc->swc_slot == 0) {
		return;
	}
	KASSERT(swc->swc_nused <= swc->swc_nallocated);

	slot = swc->swc_slot;
	nused = swc->swc_nused;
	nallocated = swc->swc_nallocated;

	/*
	 * if this is the final pageout we could have a few
	 * unused swap blocks.  if so, free them now.
	 */

	if (nused < nallocated) {
		if (!now) {
			return;
		}
		uvm_swap_free(slot + nused, nallocated - nused);
	}

	/*
	 * now start the pageout.
	 */

	if (nused > 0) {
		uvmexp.pdpageouts++;
		uvm_pageout_start(nused);
		error = uvm_swap_put(slot, swc->swc_pages, nused, 0);
		KASSERT(error == 0 || error == ENOMEM);
	}

	/*
	 * zero swslot to indicate that we are
	 * no longer building a swap-backed cluster.
	 */

	swc->swc_slot = 0;
	swc->swc_nused = 0;
}

static int
swapcluster_nused(struct swapcluster *swc)
{

	return swc->swc_nused;
}

/*
 * uvmpd_dropswap: free any swap allocated to this page.
 *
 * => called with owner locked.
 * => return true if a page had an associated slot.
 */

static bool
uvmpd_dropswap(struct vm_page *pg)
{
	bool result = false;
	struct vm_anon *anon = pg->uanon;

	if ((pg->pqflags & PQ_ANON) && anon->an_swslot) {
		uvm_swap_free(anon->an_swslot, 1);
		anon->an_swslot = 0;
		pg->flags &= ~PG_CLEAN;
		result = true;
	} else if (pg->pqflags & PQ_AOBJ) {
		int slot = uao_set_swslot(pg->uobject,
		    pg->offset >> PAGE_SHIFT, 0);
		if (slot) {
			uvm_swap_free(slot, 1);
			pg->flags &= ~PG_CLEAN;
			result = true;
		}
	}

	return result;
}

/*
 * uvmpd_trydropswap: try to free any swap allocated to this page.
 *
 * => return true if a slot is successfully freed.
 */

bool
uvmpd_trydropswap(struct vm_page *pg)
{
	kmutex_t *slock;
	bool result;

	if ((pg->flags & PG_BUSY) != 0) {
		return false;
	}

	/*
	 * lock the page's owner.
	 */

	slock = uvmpd_trylockowner(pg);
	if (slock == NULL) {
		return false;
	}

	/*
	 * skip this page if it's busy.
	 */

	if ((pg->flags & PG_BUSY) != 0) {
		mutex_exit(slock);
		return false;
	}

	result = uvmpd_dropswap(pg);

	mutex_exit(slock);

	return result;
}

#endif /* defined(VMSWAP) */

/*
 * uvmpd_scan_queue: scan an replace candidate list for pages
 * to clean or free.
 *
 * => called with page queues locked
 * => we work on meeting our free target by converting inactive pages
 *    into free pages.
 * => we handle the building of swap-backed clusters
 */

static void
uvmpd_scan_queue(void)
{
	struct vm_page *p;
	struct uvm_object *uobj;
	struct vm_anon *anon;
#if defined(VMSWAP)
	struct swapcluster swc;
#endif /* defined(VMSWAP) */
	int dirtyreacts;
	int lockownerfail;
	kmutex_t *slock;
	UVMHIST_FUNC("uvmpd_scan_queue"); UVMHIST_CALLED(pdhist);

	/*
	 * swslot is non-zero if we are building a swap cluster.  we want
	 * to stay in the loop while we have a page to scan or we have
	 * a swap-cluster to build.
	 */

#if defined(VMSWAP)
	swapcluster_init(&swc);
#endif /* defined(VMSWAP) */

	dirtyreacts = 0;
	lockownerfail = 0;
	uvmpdpol_scaninit();

	while (/* CONSTCOND */ 1) {

		/*
		 * see if we've met the free target.
		 */

		if (uvmexp.free + uvmexp.paging
#if defined(VMSWAP)
		    + swapcluster_nused(&swc)
#endif /* defined(VMSWAP) */
		    >= uvmexp.freetarg << 2 ||
		    dirtyreacts == UVMPD_NUMDIRTYREACTS) {
			UVMHIST_LOG(pdhist,"  met free target: "
				    "exit loop", 0, 0, 0, 0);
			break;
		}

		p = uvmpdpol_selectvictim();
		if (p == NULL) {
			break;
		}
		KASSERT(uvmpdpol_pageisqueued_p(p));
		KASSERT(p->wire_count == 0);

		/*
		 * we are below target and have a new page to consider.
		 */

		anon = p->uanon;
		uobj = p->uobject;

		/*
		 * first we attempt to lock the object that this page
		 * belongs to.  if our attempt fails we skip on to
		 * the next page (no harm done).  it is important to
		 * "try" locking the object as we are locking in the
		 * wrong order (pageq -> object) and we don't want to
		 * deadlock.
		 *
		 * the only time we expect to see an ownerless page
		 * (i.e. a page with no uobject and !PQ_ANON) is if an
		 * anon has loaned a page from a uvm_object and the
		 * uvm_object has dropped the ownership.  in that
		 * case, the anon can "take over" the loaned page
		 * and make it its own.
		 */

		slock = uvmpd_trylockowner(p);
		if (slock == NULL) {
			/*
			 * yield cpu to make a chance for an LWP holding
			 * the lock run.  otherwise we can busy-loop too long
			 * if the page queue is filled with a lot of pages
			 * from few objects.
			 */
			lockownerfail++;
			if (lockownerfail > UVMPD_NUMTRYLOCKOWNER) {
				mutex_exit(&uvm_pageqlock);
				/* XXX Better than yielding but inadequate. */
				kpause("livelock", false, 1, NULL);
				mutex_enter(&uvm_pageqlock);
				lockownerfail = 0;
			}
			continue;
		}
		if (p->flags & PG_BUSY) {
			mutex_exit(slock);
			uvmexp.pdbusy++;
			continue;
		}

		/* does the page belong to an object? */
		if (uobj != NULL) {
			uvmexp.pdobscan++;
		} else {
#if defined(VMSWAP)
			KASSERT(anon != NULL);
			uvmexp.pdanscan++;
#else /* defined(VMSWAP) */
			panic("%s: anon", __func__);
#endif /* defined(VMSWAP) */
		}


		/*
		 * we now have the object and the page queues locked.
		 * if the page is not swap-backed, call the object's
		 * pager to flush and free the page.
		 */

#if defined(READAHEAD_STATS)
		if ((p->pqflags & PQ_READAHEAD) != 0) {
			p->pqflags &= ~PQ_READAHEAD;
			uvm_ra_miss.ev_count++;
		}
#endif /* defined(READAHEAD_STATS) */

		if ((p->pqflags & PQ_SWAPBACKED) == 0) {
			KASSERT(uobj != NULL);
			mutex_exit(&uvm_pageqlock);
			(void) (uobj->pgops->pgo_put)(uobj, p->offset,
			    p->offset + PAGE_SIZE, PGO_CLEANIT|PGO_FREE);
			mutex_enter(&uvm_pageqlock);
			continue;
		}

		/*
		 * the page is swap-backed.  remove all the permissions
		 * from the page so we can sync the modified info
		 * without any race conditions.  if the page is clean
		 * we can free it now and continue.
		 */

		pmap_page_protect(p, VM_PROT_NONE);
		if ((p->flags & PG_CLEAN) && pmap_clear_modify(p)) {
			p->flags &= ~(PG_CLEAN);
		}
		if (p->flags & PG_CLEAN) {
			int slot;
			int pageidx;

			pageidx = p->offset >> PAGE_SHIFT;
			uvm_pagefree(p);
			uvmexp.pdfreed++;

			/*
			 * for anons, we need to remove the page
			 * from the anon ourselves.  for aobjs,
			 * pagefree did that for us.
			 */

			if (anon) {
				KASSERT(anon->an_swslot != 0);
				anon->an_page = NULL;
				slot = anon->an_swslot;
			} else {
				slot = uao_find_swslot(uobj, pageidx);
			}
			mutex_exit(slock);

			if (slot > 0) {
				/* this page is now only in swap. */
				mutex_enter(&uvm_swap_data_lock);
				KASSERT(uvmexp.swpgonly < uvmexp.swpginuse);
				uvmexp.swpgonly++;
				mutex_exit(&uvm_swap_data_lock);
			}
			continue;
		}

#if defined(VMSWAP)
		/*
		 * this page is dirty, skip it if we'll have met our
		 * free target when all the current pageouts complete.
		 */

		if (uvmexp.free + uvmexp.paging > uvmexp.freetarg << 2) {
			mutex_exit(slock);
			continue;
		}

		/*
		 * free any swap space allocated to the page since
		 * we'll have to write it again with its new data.
		 */

		uvmpd_dropswap(p);

		/*
		 * start new swap pageout cluster (if necessary).
		 *
		 * if swap is full reactivate this page so that
		 * we eventually cycle all pages through the
		 * inactive queue.
		 */

		if (swapcluster_allocslots(&swc)) {
			dirtyreacts++;
			uvm_pageactivate(p);
			mutex_exit(slock);
			continue;
		}

		/*
		 * at this point, we're definitely going reuse this
		 * page.  mark the page busy and delayed-free.
		 * we should remove the page from the page queues
		 * so we don't ever look at it again.
		 * adjust counters and such.
		 */

		p->flags |= PG_BUSY;
		UVM_PAGE_OWN(p, "scan_queue");

		p->flags |= PG_PAGEOUT;
		uvm_pagedequeue(p);

		uvmexp.pgswapout++;
		mutex_exit(&uvm_pageqlock);

		/*
		 * add the new page to the cluster.
		 */

		if (swapcluster_add(&swc, p)) {
			p->flags &= ~(PG_BUSY|PG_PAGEOUT);
			UVM_PAGE_OWN(p, NULL);
			mutex_enter(&uvm_pageqlock);
			dirtyreacts++;
			uvm_pageactivate(p);
			mutex_exit(slock);
			continue;
		}
		mutex_exit(slock);

		swapcluster_flush(&swc, false);
		mutex_enter(&uvm_pageqlock);

		/*
		 * the pageout is in progress.  bump counters and set up
		 * for the next loop.
		 */

		uvmexp.pdpending++;

#else /* defined(VMSWAP) */
		uvm_pageactivate(p);
		mutex_exit(slock);
#endif /* defined(VMSWAP) */
	}

#if defined(VMSWAP)
	mutex_exit(&uvm_pageqlock);
	swapcluster_flush(&swc, true);
	mutex_enter(&uvm_pageqlock);
#endif /* defined(VMSWAP) */
}

/*
 * uvmpd_scan: scan the page queues and attempt to meet our targets.
 *
 * => called with pageq's locked
 */

static void
uvmpd_scan(void)
{
	int swap_shortage, pages_freed;
	UVMHIST_FUNC("uvmpd_scan"); UVMHIST_CALLED(pdhist);

	uvmexp.pdrevs++;

	/*
	 * work on meeting our targets.   first we work on our free target
	 * by converting inactive pages into free pages.  then we work on
	 * meeting our inactive target by converting active pages to
	 * inactive ones.
	 */

	UVMHIST_LOG(pdhist, "  starting 'free' loop",0,0,0,0);

	pages_freed = uvmexp.pdfreed;
	uvmpd_scan_queue();
	pages_freed = uvmexp.pdfreed - pages_freed;

	/*
	 * detect if we're not going to be able to page anything out
	 * until we free some swap resources from active pages.
	 */

	swap_shortage = 0;
	if (uvmexp.free < uvmexp.freetarg &&
	    uvmexp.swpginuse >= uvmexp.swpgavail &&
	    !uvm_swapisfull() &&
	    pages_freed == 0) {
		swap_shortage = uvmexp.freetarg - uvmexp.free;
	}

	uvmpdpol_balancequeue(swap_shortage);

	/*
	 * if still below the minimum target, try unloading kernel
	 * modules.
	 */

	if (uvmexp.free < uvmexp.freemin) {
		module_thread_kick();
	}
}

/*
 * uvm_reclaimable: decide whether to wait for pagedaemon.
 *
 * => return true if it seems to be worth to do uvm_wait.
 *
 * XXX should be tunable.
 * XXX should consider pools, etc?
 */

bool
uvm_reclaimable(void)
{
	int filepages;
	int active, inactive;

	/*
	 * if swap is not full, no problem.
	 */

	if (!uvm_swapisfull()) {
		return true;
	}

	/*
	 * file-backed pages can be reclaimed even when swap is full.
	 * if we have more than 1/16 of pageable memory or 5MB, try to reclaim.
	 *
	 * XXX assume the worst case, ie. all wired pages are file-backed.
	 *
	 * XXX should consider about other reclaimable memory.
	 * XXX ie. pools, traditional buffer cache.
	 */

	filepages = uvmexp.filepages + uvmexp.execpages - uvmexp.wired;
	uvm_estimatepageable(&active, &inactive);
	if (filepages >= MIN((active + inactive) >> 4,
	    5 * 1024 * 1024 >> PAGE_SHIFT)) {
		return true;
	}

	/*
	 * kill the process, fail allocation, etc..
	 */

	return false;
}

void
uvm_estimatepageable(int *active, int *inactive)
{

	uvmpdpol_estimatepageable(active, inactive);
}


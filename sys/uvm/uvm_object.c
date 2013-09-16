/*	$NetBSD: uvm_object.c,v 1.11 2011/08/27 09:11:53 christos Exp $	*/

/*
 * Copyright (c) 2006, 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Mindaugas Rasiukevicius.
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
 * uvm_object.c: operate with memory objects
 *
 * TODO:
 *  1. Support PG_RELEASED-using objects
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_object.c,v 1.11 2011/08/27 09:11:53 christos Exp $");

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/rbtree.h>

#include <uvm/uvm.h>
#include <uvm/uvm_ddb.h>

/* Page count to fetch per single step. */
#define	FETCH_PAGECOUNT			16

/*
 * uvm_obj_init: initialize UVM memory object.
 */
void
uvm_obj_init(struct uvm_object *uo, const struct uvm_pagerops *ops,
    bool alock, u_int refs)
{

	if (alock) {
		/* Allocate and assign a lock. */
		uo->vmobjlock = mutex_obj_alloc(MUTEX_DEFAULT, IPL_NONE);
	} else {
		/* The lock will need to be set via uvm_obj_setlock(). */
		uo->vmobjlock = NULL;
	}
	uo->pgops = ops;
	TAILQ_INIT(&uo->memq);
	LIST_INIT(&uo->uo_ubc);
	uo->uo_npages = 0;
	uo->uo_refs = refs;
	rb_tree_init(&uo->rb_tree, &uvm_page_tree_ops);
}

/*
 * uvm_obj_destroy: destroy UVM memory object.
 */
void
uvm_obj_destroy(struct uvm_object *uo, bool dlock)
{

	KASSERT(rb_tree_iterate(&uo->rb_tree, NULL, RB_DIR_LEFT) == NULL);

	/* Purge any UBC entries associated with this object. */
	ubc_purge(uo);

	/* Destroy the lock, if requested. */
	if (dlock) {
		mutex_obj_free(uo->vmobjlock);
	}
}

/*
 * uvm_obj_setlock: assign a vmobjlock to the UVM object.
 *
 * => Caller is responsible to ensure that UVM objects is not use.
 * => Only dynamic lock may be previously set.  We drop the reference then.
 */
void
uvm_obj_setlock(struct uvm_object *uo, kmutex_t *lockptr)
{
	kmutex_t *olockptr = uo->vmobjlock;

	if (olockptr) {
		/* Drop the reference on the old lock. */
		mutex_obj_free(olockptr);
	}
	if (lockptr == NULL) {
		/* If new lock is not passed - allocate default one. */
		lockptr = mutex_obj_alloc(MUTEX_DEFAULT, IPL_NONE);
	}
	uo->vmobjlock = lockptr;
}

/*
 * uvm_obj_wirepages: wire the pages of entire UVM object.
 *
 * => NOTE: this function should only be used for types of objects
 *  where PG_RELEASED flag is never set (aobj objects)
 * => caller must pass page-aligned start and end values
 */
int
uvm_obj_wirepages(struct uvm_object *uobj, off_t start, off_t end,
    struct pglist *list)
{
	int i, npages, error;
	struct vm_page *pgs[FETCH_PAGECOUNT], *pg = NULL;
	off_t offset = start, left;

	left = (end - start) >> PAGE_SHIFT;

	mutex_enter(uobj->vmobjlock);
	while (left) {

		npages = MIN(FETCH_PAGECOUNT, left);

		/* Get the pages */
		memset(pgs, 0, sizeof(pgs));
		error = (*uobj->pgops->pgo_get)(uobj, offset, pgs, &npages, 0,
			VM_PROT_READ | VM_PROT_WRITE, UVM_ADV_SEQUENTIAL,
			PGO_ALLPAGES | PGO_SYNCIO);

		if (error)
			goto error;

		mutex_enter(uobj->vmobjlock);
		for (i = 0; i < npages; i++) {

			KASSERT(pgs[i] != NULL);
			KASSERT(!(pgs[i]->flags & PG_RELEASED));

			/*
			 * Loan break
			 */
			if (pgs[i]->loan_count) {
				while (pgs[i]->loan_count) {
					pg = uvm_loanbreak(pgs[i]);
					if (!pg) {
						mutex_exit(uobj->vmobjlock);
						uvm_wait("uobjwirepg");
						mutex_enter(uobj->vmobjlock);
						continue;
					}
				}
				pgs[i] = pg;
			}

			if (pgs[i]->pqflags & PQ_AOBJ) {
				pgs[i]->flags &= ~(PG_CLEAN);
				uao_dropswap(uobj, i);
			}
		}

		/* Wire the pages */
		mutex_enter(&uvm_pageqlock);
		for (i = 0; i < npages; i++) {
			uvm_pagewire(pgs[i]);
			if (list != NULL)
				TAILQ_INSERT_TAIL(list, pgs[i], pageq.queue);
		}
		mutex_exit(&uvm_pageqlock);

		/* Unbusy the pages */
		uvm_page_unbusy(pgs, npages);

		left -= npages;
		offset += npages << PAGE_SHIFT;
	}
	mutex_exit(uobj->vmobjlock);

	return 0;

error:
	/* Unwire the pages which has been wired */
	uvm_obj_unwirepages(uobj, start, offset);

	return error;
}

/*
 * uvm_obj_unwirepages: unwire the pages of entire UVM object.
 *
 * => NOTE: this function should only be used for types of objects
 *  where PG_RELEASED flag is never set
 * => caller must pass page-aligned start and end values
 */
void
uvm_obj_unwirepages(struct uvm_object *uobj, off_t start, off_t end)
{
	struct vm_page *pg;
	off_t offset;

	mutex_enter(uobj->vmobjlock);
	mutex_enter(&uvm_pageqlock);
	for (offset = start; offset < end; offset += PAGE_SIZE) {
		pg = uvm_pagelookup(uobj, offset);

		KASSERT(pg != NULL);
		KASSERT(!(pg->flags & PG_RELEASED));

		uvm_pageunwire(pg);
	}
	mutex_exit(&uvm_pageqlock);
	mutex_exit(uobj->vmobjlock);
}

#if (defined(DDB) || defined(DEBUGPRINT)) && !defined(_RUMPKERNEL)

/*
 * uvm_object_printit: actually prints the object
 */
void
uvm_object_printit(struct uvm_object *uobj, bool full,
    void (*pr)(const char *, ...))
{
	struct vm_page *pg;
	int cnt = 0;

	(*pr)("OBJECT %p: locked=%d, pgops=%p, npages=%d, ",
	    uobj, mutex_owned(uobj->vmobjlock), uobj->pgops, uobj->uo_npages);
	if (UVM_OBJ_IS_KERN_OBJECT(uobj))
		(*pr)("refs=<SYSTEM>\n");
	else
		(*pr)("refs=%d\n", uobj->uo_refs);

	if (!full) {
		return;
	}
	(*pr)("  PAGES <pg,offset>:\n  ");
	TAILQ_FOREACH(pg, &uobj->memq, listq.queue) {
		cnt++;
		(*pr)("<%p,0x%llx> ", pg, (long long)pg->offset);
		if ((cnt % 3) == 0) {
			(*pr)("\n  ");
		}
	}
	if ((cnt % 3) != 0) {
		(*pr)("\n");
	}
}

#endif /* DDB || DEBUGPRINT */

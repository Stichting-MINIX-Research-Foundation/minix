/*	$NetBSD: uvm_aobj.c,v 1.119 2012/09/15 06:25:47 matt Exp $	*/

/*
 * Copyright (c) 1998 Chuck Silvers, Charles D. Cranor and
 *                    Washington University.
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
 * from: Id: uvm_aobj.c,v 1.1.2.5 1998/02/06 05:14:38 chs Exp
 */

/*
 * uvm_aobj.c: anonymous memory uvm_object pager
 *
 * author: Chuck Silvers <chuq@chuq.com>
 * started: Jan-1998
 *
 * - design mostly from Chuck Cranor
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_aobj.c,v 1.119 2012/09/15 06:25:47 matt Exp $");

#include "opt_uvmhist.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/pool.h>
#include <sys/atomic.h>

#include <uvm/uvm.h>

/*
 * An anonymous UVM object (aobj) manages anonymous-memory.  In addition to
 * keeping the list of resident pages, it may also keep a list of allocated
 * swap blocks.  Depending on the size of the object, this list is either
 * stored in an array (small objects) or in a hash table (large objects).
 *
 * Lock order
 *
 *	uao_list_lock ->
 *		uvm_object::vmobjlock
 */

/*
 * Note: for hash tables, we break the address space of the aobj into blocks
 * of UAO_SWHASH_CLUSTER_SIZE pages, which shall be a power of two.
 */

#define	UAO_SWHASH_CLUSTER_SHIFT	4
#define	UAO_SWHASH_CLUSTER_SIZE		(1 << UAO_SWHASH_CLUSTER_SHIFT)

/* Get the "tag" for this page index. */
#define	UAO_SWHASH_ELT_TAG(idx)		((idx) >> UAO_SWHASH_CLUSTER_SHIFT)
#define UAO_SWHASH_ELT_PAGESLOT_IDX(idx) \
    ((idx) & (UAO_SWHASH_CLUSTER_SIZE - 1))

/* Given an ELT and a page index, find the swap slot. */
#define	UAO_SWHASH_ELT_PAGESLOT(elt, idx) \
    ((elt)->slots[UAO_SWHASH_ELT_PAGESLOT_IDX(idx)])

/* Given an ELT, return its pageidx base. */
#define	UAO_SWHASH_ELT_PAGEIDX_BASE(ELT) \
    ((elt)->tag << UAO_SWHASH_CLUSTER_SHIFT)

/* The hash function. */
#define	UAO_SWHASH_HASH(aobj, idx) \
    (&(aobj)->u_swhash[(((idx) >> UAO_SWHASH_CLUSTER_SHIFT) \
    & (aobj)->u_swhashmask)])

/*
 * The threshold which determines whether we will use an array or a
 * hash table to store the list of allocated swap blocks.
 */
#define	UAO_SWHASH_THRESHOLD		(UAO_SWHASH_CLUSTER_SIZE * 4)
#define	UAO_USES_SWHASH(aobj) \
    ((aobj)->u_pages > UAO_SWHASH_THRESHOLD)

/* The number of buckets in a hash, with an upper bound. */
#define	UAO_SWHASH_MAXBUCKETS		256
#define	UAO_SWHASH_BUCKETS(aobj) \
    (MIN((aobj)->u_pages >> UAO_SWHASH_CLUSTER_SHIFT, UAO_SWHASH_MAXBUCKETS))

/*
 * uao_swhash_elt: when a hash table is being used, this structure defines
 * the format of an entry in the bucket list.
 */

struct uao_swhash_elt {
	LIST_ENTRY(uao_swhash_elt) list;	/* the hash list */
	voff_t tag;				/* our 'tag' */
	int count;				/* our number of active slots */
	int slots[UAO_SWHASH_CLUSTER_SIZE];	/* the slots */
};

/*
 * uao_swhash: the swap hash table structure
 */

LIST_HEAD(uao_swhash, uao_swhash_elt);

/*
 * uao_swhash_elt_pool: pool of uao_swhash_elt structures.
 * Note: pages for this pool must not come from a pageable kernel map.
 */
static struct pool	uao_swhash_elt_pool	__cacheline_aligned;

/*
 * uvm_aobj: the actual anon-backed uvm_object
 *
 * => the uvm_object is at the top of the structure, this allows
 *   (struct uvm_aobj *) == (struct uvm_object *)
 * => only one of u_swslots and u_swhash is used in any given aobj
 */

struct uvm_aobj {
	struct uvm_object u_obj; /* has: lock, pgops, memq, #pages, #refs */
	pgoff_t u_pages;	 /* number of pages in entire object */
	int u_flags;		 /* the flags (see uvm_aobj.h) */
	int *u_swslots;		 /* array of offset->swapslot mappings */
				 /*
				  * hashtable of offset->swapslot mappings
				  * (u_swhash is an array of bucket heads)
				  */
	struct uao_swhash *u_swhash;
	u_long u_swhashmask;		/* mask for hashtable */
	LIST_ENTRY(uvm_aobj) u_list;	/* global list of aobjs */
};

static void	uao_free(struct uvm_aobj *);
static int	uao_get(struct uvm_object *, voff_t, struct vm_page **,
		    int *, int, vm_prot_t, int, int);
static int	uao_put(struct uvm_object *, voff_t, voff_t, int);

#if defined(VMSWAP)
static struct uao_swhash_elt *uao_find_swhash_elt
    (struct uvm_aobj *, int, bool);

static bool uao_pagein(struct uvm_aobj *, int, int);
static bool uao_pagein_page(struct uvm_aobj *, int);
#endif /* defined(VMSWAP) */

/*
 * aobj_pager
 *
 * note that some functions (e.g. put) are handled elsewhere
 */

const struct uvm_pagerops aobj_pager = {
	.pgo_reference = uao_reference,
	.pgo_detach = uao_detach,
	.pgo_get = uao_get,
	.pgo_put = uao_put,
};

/*
 * uao_list: global list of active aobjs, locked by uao_list_lock
 */

static LIST_HEAD(aobjlist, uvm_aobj) uao_list	__cacheline_aligned;
static kmutex_t		uao_list_lock		__cacheline_aligned;

/*
 * hash table/array related functions
 */

#if defined(VMSWAP)

/*
 * uao_find_swhash_elt: find (or create) a hash table entry for a page
 * offset.
 *
 * => the object should be locked by the caller
 */

static struct uao_swhash_elt *
uao_find_swhash_elt(struct uvm_aobj *aobj, int pageidx, bool create)
{
	struct uao_swhash *swhash;
	struct uao_swhash_elt *elt;
	voff_t page_tag;

	swhash = UAO_SWHASH_HASH(aobj, pageidx);
	page_tag = UAO_SWHASH_ELT_TAG(pageidx);

	/*
	 * now search the bucket for the requested tag
	 */

	LIST_FOREACH(elt, swhash, list) {
		if (elt->tag == page_tag) {
			return elt;
		}
	}
	if (!create) {
		return NULL;
	}

	/*
	 * allocate a new entry for the bucket and init/insert it in
	 */

	elt = pool_get(&uao_swhash_elt_pool, PR_NOWAIT);
	if (elt == NULL) {
		return NULL;
	}
	LIST_INSERT_HEAD(swhash, elt, list);
	elt->tag = page_tag;
	elt->count = 0;
	memset(elt->slots, 0, sizeof(elt->slots));
	return elt;
}

/*
 * uao_find_swslot: find the swap slot number for an aobj/pageidx
 *
 * => object must be locked by caller
 */

int
uao_find_swslot(struct uvm_object *uobj, int pageidx)
{
	struct uvm_aobj *aobj = (struct uvm_aobj *)uobj;
	struct uao_swhash_elt *elt;

	/*
	 * if noswap flag is set, then we never return a slot
	 */

	if (aobj->u_flags & UAO_FLAG_NOSWAP)
		return 0;

	/*
	 * if hashing, look in hash table.
	 */

	if (UAO_USES_SWHASH(aobj)) {
		elt = uao_find_swhash_elt(aobj, pageidx, false);
		return elt ? UAO_SWHASH_ELT_PAGESLOT(elt, pageidx) : 0;
	}

	/*
	 * otherwise, look in the array
	 */

	return aobj->u_swslots[pageidx];
}

/*
 * uao_set_swslot: set the swap slot for a page in an aobj.
 *
 * => setting a slot to zero frees the slot
 * => object must be locked by caller
 * => we return the old slot number, or -1 if we failed to allocate
 *    memory to record the new slot number
 */

int
uao_set_swslot(struct uvm_object *uobj, int pageidx, int slot)
{
	struct uvm_aobj *aobj = (struct uvm_aobj *)uobj;
	struct uao_swhash_elt *elt;
	int oldslot;
	UVMHIST_FUNC("uao_set_swslot"); UVMHIST_CALLED(pdhist);
	UVMHIST_LOG(pdhist, "aobj %p pageidx %d slot %d",
	    aobj, pageidx, slot, 0);

	KASSERT(mutex_owned(uobj->vmobjlock) || uobj->uo_refs == 0);

	/*
	 * if noswap flag is set, then we can't set a non-zero slot.
	 */

	if (aobj->u_flags & UAO_FLAG_NOSWAP) {
		KASSERTMSG(slot == 0, "uao_set_swslot: no swap object");
		return 0;
	}

	/*
	 * are we using a hash table?  if so, add it in the hash.
	 */

	if (UAO_USES_SWHASH(aobj)) {

		/*
		 * Avoid allocating an entry just to free it again if
		 * the page had not swap slot in the first place, and
		 * we are freeing.
		 */

		elt = uao_find_swhash_elt(aobj, pageidx, slot != 0);
		if (elt == NULL) {
			return slot ? -1 : 0;
		}

		oldslot = UAO_SWHASH_ELT_PAGESLOT(elt, pageidx);
		UAO_SWHASH_ELT_PAGESLOT(elt, pageidx) = slot;

		/*
		 * now adjust the elt's reference counter and free it if we've
		 * dropped it to zero.
		 */

		if (slot) {
			if (oldslot == 0)
				elt->count++;
		} else {
			if (oldslot)
				elt->count--;

			if (elt->count == 0) {
				LIST_REMOVE(elt, list);
				pool_put(&uao_swhash_elt_pool, elt);
			}
		}
	} else {
		/* we are using an array */
		oldslot = aobj->u_swslots[pageidx];
		aobj->u_swslots[pageidx] = slot;
	}
	return oldslot;
}

#endif /* defined(VMSWAP) */

/*
 * end of hash/array functions
 */

/*
 * uao_free: free all resources held by an aobj, and then free the aobj
 *
 * => the aobj should be dead
 */

static void
uao_free(struct uvm_aobj *aobj)
{
	struct uvm_object *uobj = &aobj->u_obj;

	KASSERT(mutex_owned(uobj->vmobjlock));
	uao_dropswap_range(uobj, 0, 0);
	mutex_exit(uobj->vmobjlock);

#if defined(VMSWAP)
	if (UAO_USES_SWHASH(aobj)) {

		/*
		 * free the hash table itself.
		 */

		hashdone(aobj->u_swhash, HASH_LIST, aobj->u_swhashmask);
	} else {

		/*
		 * free the array itsself.
		 */

		kmem_free(aobj->u_swslots, aobj->u_pages * sizeof(int));
	}
#endif /* defined(VMSWAP) */

	/*
	 * finally free the aobj itself
	 */

	uvm_obj_destroy(uobj, true);
	kmem_free(aobj, sizeof(struct uvm_aobj));
}

/*
 * pager functions
 */

/*
 * uao_create: create an aobj of the given size and return its uvm_object.
 *
 * => for normal use, flags are always zero
 * => for the kernel object, the flags are:
 *	UAO_FLAG_KERNOBJ - allocate the kernel object (can only happen once)
 *	UAO_FLAG_KERNSWAP - enable swapping of kernel object ("           ")
 */

struct uvm_object *
uao_create(vsize_t size, int flags)
{
	static struct uvm_aobj kernel_object_store;
	static kmutex_t kernel_object_lock;
	static int kobj_alloced = 0;
	pgoff_t pages = round_page(size) >> PAGE_SHIFT;
	struct uvm_aobj *aobj;
	int refs;

	/*
	 * Allocate a new aobj, unless kernel object is requested.
	 */

	if (flags & UAO_FLAG_KERNOBJ) {
		KASSERT(!kobj_alloced);
		aobj = &kernel_object_store;
		aobj->u_pages = pages;
		aobj->u_flags = UAO_FLAG_NOSWAP;
		refs = UVM_OBJ_KERN;
		kobj_alloced = UAO_FLAG_KERNOBJ;
	} else if (flags & UAO_FLAG_KERNSWAP) {
		KASSERT(kobj_alloced == UAO_FLAG_KERNOBJ);
		aobj = &kernel_object_store;
		kobj_alloced = UAO_FLAG_KERNSWAP;
		refs = 0xdeadbeaf; /* XXX: gcc */
	} else {
		aobj = kmem_alloc(sizeof(struct uvm_aobj), KM_SLEEP);
		aobj->u_pages = pages;
		aobj->u_flags = 0;
		refs = 1;
	}

	/*
 	 * allocate hash/array if necessary
 	 *
 	 * note: in the KERNSWAP case no need to worry about locking since
 	 * we are still booting we should be the only thread around.
 	 */

	if (flags == 0 || (flags & UAO_FLAG_KERNSWAP) != 0) {
#if defined(VMSWAP)
		const int kernswap = (flags & UAO_FLAG_KERNSWAP) != 0;

		/* allocate hash table or array depending on object size */
		if (UAO_USES_SWHASH(aobj)) {
			aobj->u_swhash = hashinit(UAO_SWHASH_BUCKETS(aobj),
			    HASH_LIST, kernswap ? false : true,
			    &aobj->u_swhashmask);
			if (aobj->u_swhash == NULL)
				panic("uao_create: hashinit swhash failed");
		} else {
			aobj->u_swslots = kmem_zalloc(pages * sizeof(int),
			    kernswap ? KM_NOSLEEP : KM_SLEEP);
			if (aobj->u_swslots == NULL)
				panic("uao_create: swslots allocation failed");
		}
#endif /* defined(VMSWAP) */

		if (flags) {
			aobj->u_flags &= ~UAO_FLAG_NOSWAP; /* clear noswap */
			return &aobj->u_obj;
		}
	}

	/*
	 * Initialise UVM object.
	 */

	const bool kernobj = (flags & UAO_FLAG_KERNOBJ) != 0;
	uvm_obj_init(&aobj->u_obj, &aobj_pager, !kernobj, refs);
	if (__predict_false(kernobj)) {
		/* Initialisation only once, for UAO_FLAG_KERNOBJ. */
		mutex_init(&kernel_object_lock, MUTEX_DEFAULT, IPL_NONE);
		uvm_obj_setlock(&aobj->u_obj, &kernel_object_lock);
	}

	/*
 	 * now that aobj is ready, add it to the global list
 	 */

	mutex_enter(&uao_list_lock);
	LIST_INSERT_HEAD(&uao_list, aobj, u_list);
	mutex_exit(&uao_list_lock);
	return(&aobj->u_obj);
}

/*
 * uao_init: set up aobj pager subsystem
 *
 * => called at boot time from uvm_pager_init()
 */

void
uao_init(void)
{
	static int uao_initialized;

	if (uao_initialized)
		return;
	uao_initialized = true;
	LIST_INIT(&uao_list);
	mutex_init(&uao_list_lock, MUTEX_DEFAULT, IPL_NONE);
	pool_init(&uao_swhash_elt_pool, sizeof(struct uao_swhash_elt),
	    0, 0, 0, "uaoeltpl", NULL, IPL_VM);
}

/*
 * uao_reference: hold a reference to an anonymous UVM object.
 */
void
uao_reference(struct uvm_object *uobj)
{
	/* Kernel object is persistent. */
	if (UVM_OBJ_IS_KERN_OBJECT(uobj)) {
		return;
	}
	atomic_inc_uint(&uobj->uo_refs);
}

/*
 * uao_detach: drop a reference to an anonymous UVM object.
 */
void
uao_detach(struct uvm_object *uobj)
{
	struct uvm_aobj *aobj = (struct uvm_aobj *)uobj;
	struct vm_page *pg;

	UVMHIST_FUNC("uao_detach"); UVMHIST_CALLED(maphist);

	/*
	 * Detaching from kernel object is a NOP.
	 */

	if (UVM_OBJ_IS_KERN_OBJECT(uobj))
		return;

	/*
	 * Drop the reference.  If it was the last one, destroy the object.
	 */

	UVMHIST_LOG(maphist,"  (uobj=0x%x)  ref=%d", uobj,uobj->uo_refs,0,0);
	if (atomic_dec_uint_nv(&uobj->uo_refs) > 0) {
		UVMHIST_LOG(maphist, "<- done (rc>0)", 0,0,0,0);
		return;
	}

	/*
	 * Remove the aobj from the global list.
	 */

	mutex_enter(&uao_list_lock);
	LIST_REMOVE(aobj, u_list);
	mutex_exit(&uao_list_lock);

	/*
	 * Free all the pages left in the aobj.  For each page, when the
	 * page is no longer busy (and thus after any disk I/O that it is
	 * involved in is complete), release any swap resources and free
	 * the page itself.
	 */

	mutex_enter(uobj->vmobjlock);
	mutex_enter(&uvm_pageqlock);
	while ((pg = TAILQ_FIRST(&uobj->memq)) != NULL) {
		pmap_page_protect(pg, VM_PROT_NONE);
		if (pg->flags & PG_BUSY) {
			pg->flags |= PG_WANTED;
			mutex_exit(&uvm_pageqlock);
			UVM_UNLOCK_AND_WAIT(pg, uobj->vmobjlock, false,
			    "uao_det", 0);
			mutex_enter(uobj->vmobjlock);
			mutex_enter(&uvm_pageqlock);
			continue;
		}
		uao_dropswap(&aobj->u_obj, pg->offset >> PAGE_SHIFT);
		uvm_pagefree(pg);
	}
	mutex_exit(&uvm_pageqlock);

	/*
	 * Finally, free the anonymous UVM object itself.
	 */

	uao_free(aobj);
}

/*
 * uao_put: flush pages out of a uvm object
 *
 * => object should be locked by caller.  we may _unlock_ the object
 *	if (and only if) we need to clean a page (PGO_CLEANIT).
 *	XXXJRT Currently, however, we don't.  In the case of cleaning
 *	XXXJRT a page, we simply just deactivate it.  Should probably
 *	XXXJRT handle this better, in the future (although "flushing"
 *	XXXJRT anonymous memory isn't terribly important).
 * => if PGO_CLEANIT is not set, then we will neither unlock the object
 *	or block.
 * => if PGO_ALLPAGE is set, then all pages in the object are valid targets
 *	for flushing.
 * => NOTE: we rely on the fact that the object's memq is a TAILQ and
 *	that new pages are inserted on the tail end of the list.  thus,
 *	we can make a complete pass through the object in one go by starting
 *	at the head and working towards the tail (new pages are put in
 *	front of us).
 * => NOTE: we are allowed to lock the page queues, so the caller
 *	must not be holding the lock on them [e.g. pagedaemon had
 *	better not call us with the queues locked]
 * => we return 0 unless we encountered some sort of I/O error
 *	XXXJRT currently never happens, as we never directly initiate
 *	XXXJRT I/O
 *
 * note on page traversal:
 *	we can traverse the pages in an object either by going down the
 *	linked list in "uobj->memq", or we can go over the address range
 *	by page doing hash table lookups for each address.  depending
 *	on how many pages are in the object it may be cheaper to do one
 *	or the other.  we set "by_list" to true if we are using memq.
 *	if the cost of a hash lookup was equal to the cost of the list
 *	traversal we could compare the number of pages in the start->stop
 *	range to the total number of pages in the object.  however, it
 *	seems that a hash table lookup is more expensive than the linked
 *	list traversal, so we multiply the number of pages in the
 *	start->stop range by a penalty which we define below.
 */

static int
uao_put(struct uvm_object *uobj, voff_t start, voff_t stop, int flags)
{
	struct uvm_aobj *aobj = (struct uvm_aobj *)uobj;
	struct vm_page *pg, *nextpg, curmp, endmp;
	bool by_list;
	voff_t curoff;
	UVMHIST_FUNC("uao_put"); UVMHIST_CALLED(maphist);

	KASSERT(mutex_owned(uobj->vmobjlock));

	curoff = 0;
	if (flags & PGO_ALLPAGES) {
		start = 0;
		stop = aobj->u_pages << PAGE_SHIFT;
		by_list = true;		/* always go by the list */
	} else {
		start = trunc_page(start);
		if (stop == 0) {
			stop = aobj->u_pages << PAGE_SHIFT;
		} else {
			stop = round_page(stop);
		}
		if (stop > (aobj->u_pages << PAGE_SHIFT)) {
			printf("uao_flush: strange, got an out of range "
			    "flush (fixed)\n");
			stop = aobj->u_pages << PAGE_SHIFT;
		}
		by_list = (uobj->uo_npages <=
		    ((stop - start) >> PAGE_SHIFT) * UVM_PAGE_TREE_PENALTY);
	}
	UVMHIST_LOG(maphist,
	    " flush start=0x%lx, stop=0x%x, by_list=%d, flags=0x%x",
	    start, stop, by_list, flags);

	/*
	 * Don't need to do any work here if we're not freeing
	 * or deactivating pages.
	 */

	if ((flags & (PGO_DEACTIVATE|PGO_FREE)) == 0) {
		mutex_exit(uobj->vmobjlock);
		return 0;
	}

	/*
	 * Initialize the marker pages.  See the comment in
	 * genfs_putpages() also.
	 */

	curmp.flags = PG_MARKER;
	endmp.flags = PG_MARKER;

	/*
	 * now do it.  note: we must update nextpg in the body of loop or we
	 * will get stuck.  we need to use nextpg if we'll traverse the list
	 * because we may free "pg" before doing the next loop.
	 */

	if (by_list) {
		TAILQ_INSERT_TAIL(&uobj->memq, &endmp, listq.queue);
		nextpg = TAILQ_FIRST(&uobj->memq);
	} else {
		curoff = start;
		nextpg = NULL;	/* Quell compiler warning */
	}

	/* locked: uobj */
	for (;;) {
		if (by_list) {
			pg = nextpg;
			if (pg == &endmp)
				break;
			nextpg = TAILQ_NEXT(pg, listq.queue);
			if (pg->flags & PG_MARKER)
				continue;
			if (pg->offset < start || pg->offset >= stop)
				continue;
		} else {
			if (curoff < stop) {
				pg = uvm_pagelookup(uobj, curoff);
				curoff += PAGE_SIZE;
			} else
				break;
			if (pg == NULL)
				continue;
		}

		/*
		 * wait and try again if the page is busy.
		 */

		if (pg->flags & PG_BUSY) {
			if (by_list) {
				TAILQ_INSERT_BEFORE(pg, &curmp, listq.queue);
			}
			pg->flags |= PG_WANTED;
			UVM_UNLOCK_AND_WAIT(pg, uobj->vmobjlock, 0,
			    "uao_put", 0);
			mutex_enter(uobj->vmobjlock);
			if (by_list) {
				nextpg = TAILQ_NEXT(&curmp, listq.queue);
				TAILQ_REMOVE(&uobj->memq, &curmp,
				    listq.queue);
			} else
				curoff -= PAGE_SIZE;
			continue;
		}

		switch (flags & (PGO_CLEANIT|PGO_FREE|PGO_DEACTIVATE)) {

		/*
		 * XXX In these first 3 cases, we always just
		 * XXX deactivate the page.  We may want to
		 * XXX handle the different cases more specifically
		 * XXX in the future.
		 */

		case PGO_CLEANIT|PGO_FREE:
		case PGO_CLEANIT|PGO_DEACTIVATE:
		case PGO_DEACTIVATE:
 deactivate_it:
			mutex_enter(&uvm_pageqlock);
			/* skip the page if it's wired */
			if (pg->wire_count == 0) {
				uvm_pagedeactivate(pg);
			}
			mutex_exit(&uvm_pageqlock);
			break;

		case PGO_FREE:
			/*
			 * If there are multiple references to
			 * the object, just deactivate the page.
			 */

			if (uobj->uo_refs > 1)
				goto deactivate_it;

			/*
			 * free the swap slot and the page.
			 */

			pmap_page_protect(pg, VM_PROT_NONE);

			/*
			 * freeing swapslot here is not strictly necessary.
			 * however, leaving it here doesn't save much
			 * because we need to update swap accounting anyway.
			 */

			uao_dropswap(uobj, pg->offset >> PAGE_SHIFT);
			mutex_enter(&uvm_pageqlock);
			uvm_pagefree(pg);
			mutex_exit(&uvm_pageqlock);
			break;

		default:
			panic("%s: impossible", __func__);
		}
	}
	if (by_list) {
		TAILQ_REMOVE(&uobj->memq, &endmp, listq.queue);
	}
	mutex_exit(uobj->vmobjlock);
	return 0;
}

/*
 * uao_get: fetch me a page
 *
 * we have three cases:
 * 1: page is resident     -> just return the page.
 * 2: page is zero-fill    -> allocate a new page and zero it.
 * 3: page is swapped out  -> fetch the page from swap.
 *
 * cases 1 and 2 can be handled with PGO_LOCKED, case 3 cannot.
 * so, if the "center" page hits case 3 (or any page, with PGO_ALLPAGES),
 * then we will need to return EBUSY.
 *
 * => prefer map unlocked (not required)
 * => object must be locked!  we will _unlock_ it before starting any I/O.
 * => flags: PGO_ALLPAGES: get all of the pages
 *           PGO_LOCKED: fault data structures are locked
 * => NOTE: offset is the offset of pps[0], _NOT_ pps[centeridx]
 * => NOTE: caller must check for released pages!!
 */

static int
uao_get(struct uvm_object *uobj, voff_t offset, struct vm_page **pps,
    int *npagesp, int centeridx, vm_prot_t access_type, int advice, int flags)
{
	voff_t current_offset;
	struct vm_page *ptmp = NULL;	/* Quell compiler warning */
	int lcv, gotpages, maxpages, swslot, pageidx;
	bool done;
	UVMHIST_FUNC("uao_get"); UVMHIST_CALLED(pdhist);

	UVMHIST_LOG(pdhist, "aobj=%p offset=%d, flags=%d",
		    (struct uvm_aobj *)uobj, offset, flags,0);

	/*
 	 * get number of pages
 	 */

	maxpages = *npagesp;

	/*
 	 * step 1: handled the case where fault data structures are locked.
 	 */

	if (flags & PGO_LOCKED) {

		/*
 		 * step 1a: get pages that are already resident.   only do
		 * this if the data structures are locked (i.e. the first
		 * time through).
 		 */

		done = true;	/* be optimistic */
		gotpages = 0;	/* # of pages we got so far */
		for (lcv = 0, current_offset = offset ; lcv < maxpages ;
		    lcv++, current_offset += PAGE_SIZE) {
			/* do we care about this page?  if not, skip it */
			if (pps[lcv] == PGO_DONTCARE)
				continue;
			ptmp = uvm_pagelookup(uobj, current_offset);

			/*
 			 * if page is new, attempt to allocate the page,
			 * zero-fill'd.
 			 */

			if (ptmp == NULL && uao_find_swslot(uobj,
			    current_offset >> PAGE_SHIFT) == 0) {
				ptmp = uvm_pagealloc(uobj, current_offset,
				    NULL, UVM_FLAG_COLORMATCH|UVM_PGA_ZERO);
				if (ptmp) {
					/* new page */
					ptmp->flags &= ~(PG_FAKE);
					ptmp->pqflags |= PQ_AOBJ;
					goto gotpage;
				}
			}

			/*
			 * to be useful must get a non-busy page
			 */

			if (ptmp == NULL || (ptmp->flags & PG_BUSY) != 0) {
				if (lcv == centeridx ||
				    (flags & PGO_ALLPAGES) != 0)
					/* need to do a wait or I/O! */
					done = false;
					continue;
			}

			/*
			 * useful page: busy/lock it and plug it in our
			 * result array
			 */

			/* caller must un-busy this page */
			ptmp->flags |= PG_BUSY;
			UVM_PAGE_OWN(ptmp, "uao_get1");
gotpage:
			pps[lcv] = ptmp;
			gotpages++;
		}

		/*
 		 * step 1b: now we've either done everything needed or we
		 * to unlock and do some waiting or I/O.
 		 */

		UVMHIST_LOG(pdhist, "<- done (done=%d)", done, 0,0,0);
		*npagesp = gotpages;
		if (done)
			return 0;
		else
			return EBUSY;
	}

	/*
 	 * step 2: get non-resident or busy pages.
 	 * object is locked.   data structures are unlocked.
 	 */

	if ((flags & PGO_SYNCIO) == 0) {
		goto done;
	}

	for (lcv = 0, current_offset = offset ; lcv < maxpages ;
	    lcv++, current_offset += PAGE_SIZE) {

		/*
		 * - skip over pages we've already gotten or don't want
		 * - skip over pages we don't _have_ to get
		 */

		if (pps[lcv] != NULL ||
		    (lcv != centeridx && (flags & PGO_ALLPAGES) == 0))
			continue;

		pageidx = current_offset >> PAGE_SHIFT;

		/*
 		 * we have yet to locate the current page (pps[lcv]).   we
		 * first look for a page that is already at the current offset.
		 * if we find a page, we check to see if it is busy or
		 * released.  if that is the case, then we sleep on the page
		 * until it is no longer busy or released and repeat the lookup.
		 * if the page we found is neither busy nor released, then we
		 * busy it (so we own it) and plug it into pps[lcv].   this
		 * 'break's the following while loop and indicates we are
		 * ready to move on to the next page in the "lcv" loop above.
 		 *
 		 * if we exit the while loop with pps[lcv] still set to NULL,
		 * then it means that we allocated a new busy/fake/clean page
		 * ptmp in the object and we need to do I/O to fill in the data.
 		 */

		/* top of "pps" while loop */
		while (pps[lcv] == NULL) {
			/* look for a resident page */
			ptmp = uvm_pagelookup(uobj, current_offset);

			/* not resident?   allocate one now (if we can) */
			if (ptmp == NULL) {

				ptmp = uvm_pagealloc(uobj, current_offset,
				    NULL, 0);

				/* out of RAM? */
				if (ptmp == NULL) {
					mutex_exit(uobj->vmobjlock);
					UVMHIST_LOG(pdhist,
					    "sleeping, ptmp == NULL\n",0,0,0,0);
					uvm_wait("uao_getpage");
					mutex_enter(uobj->vmobjlock);
					continue;
				}

				/*
				 * safe with PQ's unlocked: because we just
				 * alloc'd the page
				 */

				ptmp->pqflags |= PQ_AOBJ;

				/*
				 * got new page ready for I/O.  break pps while
				 * loop.  pps[lcv] is still NULL.
				 */

				break;
			}

			/* page is there, see if we need to wait on it */
			if ((ptmp->flags & PG_BUSY) != 0) {
				ptmp->flags |= PG_WANTED;
				UVMHIST_LOG(pdhist,
				    "sleeping, ptmp->flags 0x%x\n",
				    ptmp->flags,0,0,0);
				UVM_UNLOCK_AND_WAIT(ptmp, uobj->vmobjlock,
				    false, "uao_get", 0);
				mutex_enter(uobj->vmobjlock);
				continue;
			}

			/*
 			 * if we get here then the page has become resident and
			 * unbusy between steps 1 and 2.  we busy it now (so we
			 * own it) and set pps[lcv] (so that we exit the while
			 * loop).
 			 */

			/* we own it, caller must un-busy */
			ptmp->flags |= PG_BUSY;
			UVM_PAGE_OWN(ptmp, "uao_get2");
			pps[lcv] = ptmp;
		}

		/*
 		 * if we own the valid page at the correct offset, pps[lcv] will
 		 * point to it.   nothing more to do except go to the next page.
 		 */

		if (pps[lcv])
			continue;			/* next lcv */

		/*
 		 * we have a "fake/busy/clean" page that we just allocated.
 		 * do the needed "i/o", either reading from swap or zeroing.
 		 */

		swslot = uao_find_swslot(uobj, pageidx);

		/*
 		 * just zero the page if there's nothing in swap.
 		 */

		if (swslot == 0) {

			/*
			 * page hasn't existed before, just zero it.
			 */

			uvm_pagezero(ptmp);
		} else {
#if defined(VMSWAP)
			int error;

			UVMHIST_LOG(pdhist, "pagein from swslot %d",
			     swslot, 0,0,0);

			/*
			 * page in the swapped-out page.
			 * unlock object for i/o, relock when done.
			 */

			mutex_exit(uobj->vmobjlock);
			error = uvm_swap_get(ptmp, swslot, PGO_SYNCIO);
			mutex_enter(uobj->vmobjlock);

			/*
			 * I/O done.  check for errors.
			 */

			if (error != 0) {
				UVMHIST_LOG(pdhist, "<- done (error=%d)",
				    error,0,0,0);
				if (ptmp->flags & PG_WANTED)
					wakeup(ptmp);

				/*
				 * remove the swap slot from the aobj
				 * and mark the aobj as having no real slot.
				 * don't free the swap slot, thus preventing
				 * it from being used again.
				 */

				swslot = uao_set_swslot(uobj, pageidx,
				    SWSLOT_BAD);
				if (swslot > 0) {
					uvm_swap_markbad(swslot, 1);
				}

				mutex_enter(&uvm_pageqlock);
				uvm_pagefree(ptmp);
				mutex_exit(&uvm_pageqlock);
				mutex_exit(uobj->vmobjlock);
				return error;
			}
#else /* defined(VMSWAP) */
			panic("%s: pagein", __func__);
#endif /* defined(VMSWAP) */
		}

		if ((access_type & VM_PROT_WRITE) == 0) {
			ptmp->flags |= PG_CLEAN;
			pmap_clear_modify(ptmp);
		}

		/*
 		 * we got the page!   clear the fake flag (indicates valid
		 * data now in page) and plug into our result array.   note
		 * that page is still busy.
 		 *
 		 * it is the callers job to:
 		 * => check if the page is released
 		 * => unbusy the page
 		 * => activate the page
 		 */

		ptmp->flags &= ~PG_FAKE;
		pps[lcv] = ptmp;
	}

	/*
 	 * finally, unlock object and return.
 	 */

done:
	mutex_exit(uobj->vmobjlock);
	UVMHIST_LOG(pdhist, "<- done (OK)",0,0,0,0);
	return 0;
}

#if defined(VMSWAP)

/*
 * uao_dropswap:  release any swap resources from this aobj page.
 *
 * => aobj must be locked or have a reference count of 0.
 */

void
uao_dropswap(struct uvm_object *uobj, int pageidx)
{
	int slot;

	slot = uao_set_swslot(uobj, pageidx, 0);
	if (slot) {
		uvm_swap_free(slot, 1);
	}
}

/*
 * page in every page in every aobj that is paged-out to a range of swslots.
 *
 * => nothing should be locked.
 * => returns true if pagein was aborted due to lack of memory.
 */

bool
uao_swap_off(int startslot, int endslot)
{
	struct uvm_aobj *aobj;

	/*
	 * Walk the list of all anonymous UVM objects.  Grab the first.
	 */
	mutex_enter(&uao_list_lock);
	if ((aobj = LIST_FIRST(&uao_list)) == NULL) {
		mutex_exit(&uao_list_lock);
		return false;
	}
	uao_reference(&aobj->u_obj);

	do {
		struct uvm_aobj *nextaobj;
		bool rv;

		/*
		 * Prefetch the next object and immediately hold a reference
		 * on it, so neither the current nor the next entry could
		 * disappear while we are iterating.
		 */
		if ((nextaobj = LIST_NEXT(aobj, u_list)) != NULL) {
			uao_reference(&nextaobj->u_obj);
		}
		mutex_exit(&uao_list_lock);

		/*
		 * Page in all pages in the swap slot range.
		 */
		mutex_enter(aobj->u_obj.vmobjlock);
		rv = uao_pagein(aobj, startslot, endslot);
		mutex_exit(aobj->u_obj.vmobjlock);

		/* Drop the reference of the current object. */
		uao_detach(&aobj->u_obj);
		if (rv) {
			if (nextaobj) {
				uao_detach(&nextaobj->u_obj);
			}
			return rv;
		}

		aobj = nextaobj;
		mutex_enter(&uao_list_lock);
	} while (aobj);

	mutex_exit(&uao_list_lock);
	return false;
}

/*
 * page in any pages from aobj in the given range.
 *
 * => aobj must be locked and is returned locked.
 * => returns true if pagein was aborted due to lack of memory.
 */
static bool
uao_pagein(struct uvm_aobj *aobj, int startslot, int endslot)
{
	bool rv;

	if (UAO_USES_SWHASH(aobj)) {
		struct uao_swhash_elt *elt;
		int buck;

restart:
		for (buck = aobj->u_swhashmask; buck >= 0; buck--) {
			for (elt = LIST_FIRST(&aobj->u_swhash[buck]);
			     elt != NULL;
			     elt = LIST_NEXT(elt, list)) {
				int i;

				for (i = 0; i < UAO_SWHASH_CLUSTER_SIZE; i++) {
					int slot = elt->slots[i];

					/*
					 * if the slot isn't in range, skip it.
					 */

					if (slot < startslot ||
					    slot >= endslot) {
						continue;
					}

					/*
					 * process the page,
					 * the start over on this object
					 * since the swhash elt
					 * may have been freed.
					 */

					rv = uao_pagein_page(aobj,
					  UAO_SWHASH_ELT_PAGEIDX_BASE(elt) + i);
					if (rv) {
						return rv;
					}
					goto restart;
				}
			}
		}
	} else {
		int i;

		for (i = 0; i < aobj->u_pages; i++) {
			int slot = aobj->u_swslots[i];

			/*
			 * if the slot isn't in range, skip it
			 */

			if (slot < startslot || slot >= endslot) {
				continue;
			}

			/*
			 * process the page.
			 */

			rv = uao_pagein_page(aobj, i);
			if (rv) {
				return rv;
			}
		}
	}

	return false;
}

/*
 * uao_pagein_page: page in a single page from an anonymous UVM object.
 *
 * => Returns true if pagein was aborted due to lack of memory.
 * => Object must be locked and is returned locked.
 */

static bool
uao_pagein_page(struct uvm_aobj *aobj, int pageidx)
{
	struct uvm_object *uobj = &aobj->u_obj;
	struct vm_page *pg;
	int rv, npages;

	pg = NULL;
	npages = 1;

	KASSERT(mutex_owned(uobj->vmobjlock));
	rv = uao_get(uobj, pageidx << PAGE_SHIFT, &pg, &npages,
	    0, VM_PROT_READ | VM_PROT_WRITE, 0, PGO_SYNCIO);

	/*
	 * relock and finish up.
	 */

	mutex_enter(uobj->vmobjlock);
	switch (rv) {
	case 0:
		break;

	case EIO:
	case ERESTART:

		/*
		 * nothing more to do on errors.
		 * ERESTART can only mean that the anon was freed,
		 * so again there's nothing to do.
		 */

		return false;

	default:
		return true;
	}

	/*
	 * ok, we've got the page now.
	 * mark it as dirty, clear its swslot and un-busy it.
	 */
	uao_dropswap(&aobj->u_obj, pageidx);

	/*
	 * make sure it's on a page queue.
	 */
	mutex_enter(&uvm_pageqlock);
	if (pg->wire_count == 0)
		uvm_pageenqueue(pg);
	mutex_exit(&uvm_pageqlock);

	if (pg->flags & PG_WANTED) {
		wakeup(pg);
	}
	pg->flags &= ~(PG_WANTED|PG_BUSY|PG_CLEAN|PG_FAKE);
	UVM_PAGE_OWN(pg, NULL);

	return false;
}

/*
 * uao_dropswap_range: drop swapslots in the range.
 *
 * => aobj must be locked and is returned locked.
 * => start is inclusive.  end is exclusive.
 */

void
uao_dropswap_range(struct uvm_object *uobj, voff_t start, voff_t end)
{
	struct uvm_aobj *aobj = (struct uvm_aobj *)uobj;
	int swpgonlydelta = 0;

	KASSERT(mutex_owned(uobj->vmobjlock));

	if (end == 0) {
		end = INT64_MAX;
	}

	if (UAO_USES_SWHASH(aobj)) {
		int i, hashbuckets = aobj->u_swhashmask + 1;
		voff_t taghi;
		voff_t taglo;

		taglo = UAO_SWHASH_ELT_TAG(start);
		taghi = UAO_SWHASH_ELT_TAG(end);

		for (i = 0; i < hashbuckets; i++) {
			struct uao_swhash_elt *elt, *next;

			for (elt = LIST_FIRST(&aobj->u_swhash[i]);
			     elt != NULL;
			     elt = next) {
				int startidx, endidx;
				int j;

				next = LIST_NEXT(elt, list);

				if (elt->tag < taglo || taghi < elt->tag) {
					continue;
				}

				if (elt->tag == taglo) {
					startidx =
					    UAO_SWHASH_ELT_PAGESLOT_IDX(start);
				} else {
					startidx = 0;
				}

				if (elt->tag == taghi) {
					endidx =
					    UAO_SWHASH_ELT_PAGESLOT_IDX(end);
				} else {
					endidx = UAO_SWHASH_CLUSTER_SIZE;
				}

				for (j = startidx; j < endidx; j++) {
					int slot = elt->slots[j];

					KASSERT(uvm_pagelookup(&aobj->u_obj,
					    (UAO_SWHASH_ELT_PAGEIDX_BASE(elt)
					    + j) << PAGE_SHIFT) == NULL);
					if (slot > 0) {
						uvm_swap_free(slot, 1);
						swpgonlydelta++;
						KASSERT(elt->count > 0);
						elt->slots[j] = 0;
						elt->count--;
					}
				}

				if (elt->count == 0) {
					LIST_REMOVE(elt, list);
					pool_put(&uao_swhash_elt_pool, elt);
				}
			}
		}
	} else {
		int i;

		if (aobj->u_pages < end) {
			end = aobj->u_pages;
		}
		for (i = start; i < end; i++) {
			int slot = aobj->u_swslots[i];

			if (slot > 0) {
				uvm_swap_free(slot, 1);
				swpgonlydelta++;
			}
		}
	}

	/*
	 * adjust the counter of pages only in swap for all
	 * the swap slots we've freed.
	 */

	if (swpgonlydelta > 0) {
		mutex_enter(&uvm_swap_data_lock);
		KASSERT(uvmexp.swpgonly >= swpgonlydelta);
		uvmexp.swpgonly -= swpgonlydelta;
		mutex_exit(&uvm_swap_data_lock);
	}
}

#endif /* defined(VMSWAP) */

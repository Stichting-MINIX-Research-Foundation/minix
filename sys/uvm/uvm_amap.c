/*	$NetBSD: uvm_amap.c,v 1.107 2012/04/08 20:47:10 chs Exp $	*/

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
 */

/*
 * uvm_amap.c: amap operations
 */

/*
 * this file contains functions that perform operations on amaps.  see
 * uvm_amap.h for a brief explanation of the role of amaps in uvm.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_amap.c,v 1.107 2012/04/08 20:47:10 chs Exp $");

#include "opt_uvmhist.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/pool.h>
#include <sys/atomic.h>

#include <uvm/uvm.h>
#include <uvm/uvm_swap.h>

/*
 * cache for allocation of vm_map structures.  note that in order to
 * avoid an endless loop, the amap cache's allocator cannot allocate
 * memory from an amap (it currently goes through the kernel uobj, so
 * we are ok).
 */
static struct pool_cache uvm_amap_cache;
static kmutex_t amap_list_lock;
static LIST_HEAD(, vm_amap) amap_list;

/*
 * local functions
 */

static inline void
amap_list_insert(struct vm_amap *amap)
{

	mutex_enter(&amap_list_lock);
	LIST_INSERT_HEAD(&amap_list, amap, am_list);
	mutex_exit(&amap_list_lock);
}

static inline void
amap_list_remove(struct vm_amap *amap)
{

	mutex_enter(&amap_list_lock);
	LIST_REMOVE(amap, am_list);
	mutex_exit(&amap_list_lock);
}

static int
amap_roundup_slots(int slots)
{

	return kmem_roundup_size(slots * sizeof(int)) / sizeof(int);
}

#ifdef UVM_AMAP_PPREF
/*
 * what is ppref?   ppref is an _optional_ amap feature which is used
 * to keep track of reference counts on a per-page basis.  it is enabled
 * when UVM_AMAP_PPREF is defined.
 *
 * when enabled, an array of ints is allocated for the pprefs.  this
 * array is allocated only when a partial reference is added to the
 * map (either by unmapping part of the amap, or gaining a reference
 * to only a part of an amap).  if the allocation of the array fails
 * (KM_NOSLEEP), then we set the array pointer to PPREF_NONE to indicate
 * that we tried to do ppref's but couldn't alloc the array so just
 * give up (after all, this is an optional feature!).
 *
 * the array is divided into page sized "chunks."   for chunks of length 1,
 * the chunk reference count plus one is stored in that chunk's slot.
 * for chunks of length > 1 the first slot contains (the reference count
 * plus one) * -1.    [the negative value indicates that the length is
 * greater than one.]   the second slot of the chunk contains the length
 * of the chunk.   here is an example:
 *
 * actual REFS:  2  2  2  2  3  1  1  0  0  0  4  4  0  1  1  1
 *       ppref: -3  4  x  x  4 -2  2 -1  3  x -5  2  1 -2  3  x
 *              <----------><-><----><-------><----><-><------->
 * (x = don't care)
 *
 * this allows us to allow one int to contain the ref count for the whole
 * chunk.    note that the "plus one" part is needed because a reference
 * count of zero is neither positive or negative (need a way to tell
 * if we've got one zero or a bunch of them).
 *
 * here are some in-line functions to help us.
 */

/*
 * pp_getreflen: get the reference and length for a specific offset
 *
 * => ppref's amap must be locked
 */
static inline void
pp_getreflen(int *ppref, int offset, int *refp, int *lenp)
{

	if (ppref[offset] > 0) {		/* chunk size must be 1 */
		*refp = ppref[offset] - 1;	/* don't forget to adjust */
		*lenp = 1;
	} else {
		*refp = (ppref[offset] * -1) - 1;
		*lenp = ppref[offset+1];
	}
}

/*
 * pp_setreflen: set the reference and length for a specific offset
 *
 * => ppref's amap must be locked
 */
static inline void
pp_setreflen(int *ppref, int offset, int ref, int len)
{
	if (len == 0)
		return;
	if (len == 1) {
		ppref[offset] = ref + 1;
	} else {
		ppref[offset] = (ref + 1) * -1;
		ppref[offset+1] = len;
	}
}
#endif /* UVM_AMAP_PPREF */

/*
 * amap_alloc1: allocate an amap, but do not initialise the overlay.
 *
 * => Note: lock is not set.
 */
static struct vm_amap *
amap_alloc1(int slots, int padslots, int flags)
{
	const bool nowait = (flags & UVM_FLAG_NOWAIT) != 0;
	const km_flag_t kmflags = nowait ? KM_NOSLEEP : KM_SLEEP;
	struct vm_amap *amap;
	int totalslots;

	amap = pool_cache_get(&uvm_amap_cache, nowait ? PR_NOWAIT : PR_WAITOK);
	if (amap == NULL) {
		return NULL;
	}
	totalslots = amap_roundup_slots(slots + padslots);
	amap->am_lock = NULL;
	amap->am_ref = 1;
	amap->am_flags = 0;
#ifdef UVM_AMAP_PPREF
	amap->am_ppref = NULL;
#endif
	amap->am_maxslot = totalslots;
	amap->am_nslot = slots;
	amap->am_nused = 0;

	/*
	 * Note: since allocations are likely big, we expect to reduce the
	 * memory fragmentation by allocating them in separate blocks.
	 */
	amap->am_slots = kmem_alloc(totalslots * sizeof(int), kmflags);
	if (amap->am_slots == NULL)
		goto fail1;

	amap->am_bckptr = kmem_alloc(totalslots * sizeof(int), kmflags);
	if (amap->am_bckptr == NULL)
		goto fail2;

	amap->am_anon = kmem_alloc(totalslots * sizeof(struct vm_anon *),
	    kmflags);
	if (amap->am_anon == NULL)
		goto fail3;

	return amap;

fail3:
	kmem_free(amap->am_bckptr, totalslots * sizeof(int));
fail2:
	kmem_free(amap->am_slots, totalslots * sizeof(int));
fail1:
	pool_cache_put(&uvm_amap_cache, amap);

	/*
	 * XXX hack to tell the pagedaemon how many pages we need,
	 * since we can need more than it would normally free.
	 */
	if (nowait) {
		extern u_int uvm_extrapages;
		atomic_add_int(&uvm_extrapages,
		    ((sizeof(int) * 2 + sizeof(struct vm_anon *)) *
		    totalslots) >> PAGE_SHIFT);
	}
	return NULL;
}

/*
 * amap_alloc: allocate an amap to manage "sz" bytes of anonymous VM
 *
 * => caller should ensure sz is a multiple of PAGE_SIZE
 * => reference count to new amap is set to one
 * => new amap is returned unlocked
 */

struct vm_amap *
amap_alloc(vaddr_t sz, vaddr_t padsz, int waitf)
{
	struct vm_amap *amap;
	int slots, padslots;
	UVMHIST_FUNC("amap_alloc"); UVMHIST_CALLED(maphist);

	AMAP_B2SLOT(slots, sz);
	AMAP_B2SLOT(padslots, padsz);

	amap = amap_alloc1(slots, padslots, waitf);
	if (amap) {
		memset(amap->am_anon, 0,
		    amap->am_maxslot * sizeof(struct vm_anon *));
		amap->am_lock = mutex_obj_alloc(MUTEX_DEFAULT, IPL_NONE);
		amap_list_insert(amap);
	}

	UVMHIST_LOG(maphist,"<- done, amap = 0x%x, sz=%d", amap, sz, 0, 0);
	return(amap);
}

/*
 * uvm_amap_init: initialize the amap system.
 */
void
uvm_amap_init(void)
{

	mutex_init(&amap_list_lock, MUTEX_DEFAULT, IPL_NONE);

	pool_cache_bootstrap(&uvm_amap_cache, sizeof(struct vm_amap), 0, 0, 0,
	    "amappl", NULL, IPL_NONE, NULL, NULL, NULL);
}

/*
 * amap_free: free an amap
 *
 * => the amap must be unlocked
 * => the amap should have a zero reference count and be empty
 */
void
amap_free(struct vm_amap *amap)
{
	int slots;

	UVMHIST_FUNC("amap_free"); UVMHIST_CALLED(maphist);

	KASSERT(amap->am_ref == 0 && amap->am_nused == 0);
	KASSERT((amap->am_flags & AMAP_SWAPOFF) == 0);
	if (amap->am_lock != NULL) {
		KASSERT(!mutex_owned(amap->am_lock));
		mutex_obj_free(amap->am_lock);
	}
	slots = amap->am_maxslot;
	kmem_free(amap->am_slots, slots * sizeof(*amap->am_slots));
	kmem_free(amap->am_bckptr, slots * sizeof(*amap->am_bckptr));
	kmem_free(amap->am_anon, slots * sizeof(*amap->am_anon));
#ifdef UVM_AMAP_PPREF
	if (amap->am_ppref && amap->am_ppref != PPREF_NONE)
		kmem_free(amap->am_ppref, slots * sizeof(*amap->am_ppref));
#endif
	pool_cache_put(&uvm_amap_cache, amap);
	UVMHIST_LOG(maphist,"<- done, freed amap = 0x%x", amap, 0, 0, 0);
}

/*
 * amap_extend: extend the size of an amap (if needed)
 *
 * => called from uvm_map when we want to extend an amap to cover
 *    a new mapping (rather than allocate a new one)
 * => amap should be unlocked (we will lock it)
 * => to safely extend an amap it should have a reference count of
 *    one (thus it can't be shared)
 */
int
amap_extend(struct vm_map_entry *entry, vsize_t addsize, int flags)
{
	struct vm_amap *amap = entry->aref.ar_amap;
	int slotoff = entry->aref.ar_pageoff;
	int slotmapped, slotadd, slotneed, slotadded, slotalloc;
	int slotadj, slotspace;
	int oldnslots;
#ifdef UVM_AMAP_PPREF
	int *newppref, *oldppref;
#endif
	int i, *newsl, *newbck, *oldsl, *oldbck;
	struct vm_anon **newover, **oldover, *tofree;
	const km_flag_t kmflags =
	    (flags & AMAP_EXTEND_NOWAIT) ? KM_NOSLEEP : KM_SLEEP;

	UVMHIST_FUNC("amap_extend"); UVMHIST_CALLED(maphist);

	UVMHIST_LOG(maphist, "  (entry=0x%x, addsize=0x%x, flags=0x%x)",
	    entry, addsize, flags, 0);

	/*
	 * first, determine how many slots we need in the amap.  don't
	 * forget that ar_pageoff could be non-zero: this means that
	 * there are some unused slots before us in the amap.
	 */

	amap_lock(amap);
	KASSERT(amap_refs(amap) == 1); /* amap can't be shared */
	AMAP_B2SLOT(slotmapped, entry->end - entry->start); /* slots mapped */
	AMAP_B2SLOT(slotadd, addsize);			/* slots to add */
	if (flags & AMAP_EXTEND_FORWARDS) {
		slotneed = slotoff + slotmapped + slotadd;
		slotadj = 0;
		slotspace = 0;
	}
	else {
		slotneed = slotadd + slotmapped;
		slotadj = slotadd - slotoff;
		slotspace = amap->am_maxslot - slotmapped;
	}
	tofree = NULL;

	/*
	 * case 1: we already have enough slots in the map and thus
	 * only need to bump the reference counts on the slots we are
	 * adding.
	 */

	if (flags & AMAP_EXTEND_FORWARDS) {
		if (amap->am_nslot >= slotneed) {
#ifdef UVM_AMAP_PPREF
			if (amap->am_ppref && amap->am_ppref != PPREF_NONE) {
				amap_pp_adjref(amap, slotoff + slotmapped,
				    slotadd, 1, &tofree);
			}
#endif
			uvm_anon_freelst(amap, tofree);
			UVMHIST_LOG(maphist,
			    "<- done (case 1f), amap = 0x%x, sltneed=%d",
			    amap, slotneed, 0, 0);
			return 0;
		}
	} else {
		if (slotadj <= 0) {
			slotoff -= slotadd;
			entry->aref.ar_pageoff = slotoff;
#ifdef UVM_AMAP_PPREF
			if (amap->am_ppref && amap->am_ppref != PPREF_NONE) {
				amap_pp_adjref(amap, slotoff, slotadd, 1,
				    &tofree);
			}
#endif
			uvm_anon_freelst(amap, tofree);
			UVMHIST_LOG(maphist,
			    "<- done (case 1b), amap = 0x%x, sltneed=%d",
			    amap, slotneed, 0, 0);
			return 0;
		}
	}

	/*
	 * case 2: we pre-allocated slots for use and we just need to
	 * bump nslot up to take account for these slots.
	 */

	if (amap->am_maxslot >= slotneed) {
		if (flags & AMAP_EXTEND_FORWARDS) {
#ifdef UVM_AMAP_PPREF
			if (amap->am_ppref && amap->am_ppref != PPREF_NONE) {
				if ((slotoff + slotmapped) < amap->am_nslot)
					amap_pp_adjref(amap,
					    slotoff + slotmapped,
					    (amap->am_nslot -
					    (slotoff + slotmapped)), 1,
					    &tofree);
				pp_setreflen(amap->am_ppref, amap->am_nslot, 1,
				    slotneed - amap->am_nslot);
			}
#endif
			amap->am_nslot = slotneed;
			uvm_anon_freelst(amap, tofree);

			/*
			 * no need to zero am_anon since that was done at
			 * alloc time and we never shrink an allocation.
			 */

			UVMHIST_LOG(maphist,"<- done (case 2f), amap = 0x%x, "
			    "slotneed=%d", amap, slotneed, 0, 0);
			return 0;
		} else {
#ifdef UVM_AMAP_PPREF
			if (amap->am_ppref && amap->am_ppref != PPREF_NONE) {
				/*
				 * Slide up the ref counts on the pages that
				 * are actually in use.
				 */
				memmove(amap->am_ppref + slotspace,
				    amap->am_ppref + slotoff,
				    slotmapped * sizeof(int));
				/*
				 * Mark the (adjusted) gap at the front as
				 * referenced/not referenced.
				 */
				pp_setreflen(amap->am_ppref,
				    0, 0, slotspace - slotadd);
				pp_setreflen(amap->am_ppref,
				    slotspace - slotadd, 1, slotadd);
			}
#endif

			/*
			 * Slide the anon pointers up and clear out
			 * the space we just made.
			 */
			memmove(amap->am_anon + slotspace,
			    amap->am_anon + slotoff,
			    slotmapped * sizeof(struct vm_anon*));
			memset(amap->am_anon + slotoff, 0,
			    (slotspace - slotoff) * sizeof(struct vm_anon *));

			/*
			 * Slide the backpointers up, but don't bother
			 * wiping out the old slots.
			 */
			memmove(amap->am_bckptr + slotspace,
			    amap->am_bckptr + slotoff,
			    slotmapped * sizeof(int));

			/*
			 * Adjust all the useful active slot numbers.
			 */
			for (i = 0; i < amap->am_nused; i++)
				amap->am_slots[i] += (slotspace - slotoff);

			/*
			 * We just filled all the empty space in the
			 * front of the amap by activating a few new
			 * slots.
			 */
			amap->am_nslot = amap->am_maxslot;
			entry->aref.ar_pageoff = slotspace - slotadd;
			amap_unlock(amap);

			UVMHIST_LOG(maphist,"<- done (case 2b), amap = 0x%x, "
			    "slotneed=%d", amap, slotneed, 0, 0);
			return 0;
		}
	}

	/*
	 * Case 3: we need to allocate a new amap and copy all the amap
	 * data over from old amap to the new one.  Drop the lock before
	 * performing allocation.
	 *
	 * Note: since allocations are likely big, we expect to reduce the
	 * memory fragmentation by allocating them in separate blocks.
	 */

	amap_unlock(amap);

	if (slotneed >= UVM_AMAP_LARGE) {
		return E2BIG;
	}

	slotalloc = amap_roundup_slots(slotneed);
#ifdef UVM_AMAP_PPREF
	newppref = NULL;
	if (amap->am_ppref && amap->am_ppref != PPREF_NONE) {
		/* Will be handled later if fails. */
		newppref = kmem_alloc(slotalloc * sizeof(*newppref), kmflags);
	}
#endif
	newsl = kmem_alloc(slotalloc * sizeof(*newsl), kmflags);
	newbck = kmem_alloc(slotalloc * sizeof(*newbck), kmflags);
	newover = kmem_alloc(slotalloc * sizeof(*newover), kmflags);
	if (newsl == NULL || newbck == NULL || newover == NULL) {
#ifdef UVM_AMAP_PPREF
		if (newppref != NULL) {
			kmem_free(newppref, slotalloc * sizeof(*newppref));
		}
#endif
		if (newsl != NULL) {
			kmem_free(newsl, slotalloc * sizeof(*newsl));
		}
		if (newbck != NULL) {
			kmem_free(newbck, slotalloc * sizeof(*newbck));
		}
		if (newover != NULL) {
			kmem_free(newover, slotalloc * sizeof(*newover));
		}
		return ENOMEM;
	}
	amap_lock(amap);
	KASSERT(amap->am_maxslot < slotneed);

	/*
	 * Copy everything over to new allocated areas.
	 */

	slotadded = slotalloc - amap->am_nslot;
	if (!(flags & AMAP_EXTEND_FORWARDS))
		slotspace = slotalloc - slotmapped;

	/* do am_slots */
	oldsl = amap->am_slots;
	if (flags & AMAP_EXTEND_FORWARDS)
		memcpy(newsl, oldsl, sizeof(int) * amap->am_nused);
	else
		for (i = 0; i < amap->am_nused; i++)
			newsl[i] = oldsl[i] + slotspace - slotoff;
	amap->am_slots = newsl;

	/* do am_anon */
	oldover = amap->am_anon;
	if (flags & AMAP_EXTEND_FORWARDS) {
		memcpy(newover, oldover,
		    sizeof(struct vm_anon *) * amap->am_nslot);
		memset(newover + amap->am_nslot, 0,
		    sizeof(struct vm_anon *) * slotadded);
	} else {
		memcpy(newover + slotspace, oldover + slotoff,
		    sizeof(struct vm_anon *) * slotmapped);
		memset(newover, 0,
		    sizeof(struct vm_anon *) * slotspace);
	}
	amap->am_anon = newover;

	/* do am_bckptr */
	oldbck = amap->am_bckptr;
	if (flags & AMAP_EXTEND_FORWARDS)
		memcpy(newbck, oldbck, sizeof(int) * amap->am_nslot);
	else
		memcpy(newbck + slotspace, oldbck + slotoff,
		    sizeof(int) * slotmapped);
	amap->am_bckptr = newbck;

#ifdef UVM_AMAP_PPREF
	/* do ppref */
	oldppref = amap->am_ppref;
	if (newppref) {
		if (flags & AMAP_EXTEND_FORWARDS) {
			memcpy(newppref, oldppref,
			    sizeof(int) * amap->am_nslot);
			memset(newppref + amap->am_nslot, 0,
			    sizeof(int) * slotadded);
		} else {
			memcpy(newppref + slotspace, oldppref + slotoff,
			    sizeof(int) * slotmapped);
		}
		amap->am_ppref = newppref;
		if ((flags & AMAP_EXTEND_FORWARDS) &&
		    (slotoff + slotmapped) < amap->am_nslot)
			amap_pp_adjref(amap, slotoff + slotmapped,
			    (amap->am_nslot - (slotoff + slotmapped)), 1,
			    &tofree);
		if (flags & AMAP_EXTEND_FORWARDS)
			pp_setreflen(newppref, amap->am_nslot, 1,
			    slotneed - amap->am_nslot);
		else {
			pp_setreflen(newppref, 0, 0,
			    slotalloc - slotneed);
			pp_setreflen(newppref, slotalloc - slotneed, 1,
			    slotneed - slotmapped);
		}
	} else {
		if (amap->am_ppref)
			amap->am_ppref = PPREF_NONE;
	}
#endif

	/* update master values */
	if (flags & AMAP_EXTEND_FORWARDS)
		amap->am_nslot = slotneed;
	else {
		entry->aref.ar_pageoff = slotspace - slotadd;
		amap->am_nslot = slotalloc;
	}
	oldnslots = amap->am_maxslot;
	amap->am_maxslot = slotalloc;

	uvm_anon_freelst(amap, tofree);

	kmem_free(oldsl, oldnslots * sizeof(*oldsl));
	kmem_free(oldbck, oldnslots * sizeof(*oldbck));
	kmem_free(oldover, oldnslots * sizeof(*oldover));
#ifdef UVM_AMAP_PPREF
	if (oldppref && oldppref != PPREF_NONE)
		kmem_free(oldppref, oldnslots * sizeof(*oldppref));
#endif
	UVMHIST_LOG(maphist,"<- done (case 3), amap = 0x%x, slotneed=%d",
	    amap, slotneed, 0, 0);
	return 0;
}

/*
 * amap_share_protect: change protection of anons in a shared amap
 *
 * for shared amaps, given the current data structure layout, it is
 * not possible for us to directly locate all maps referencing the
 * shared anon (to change the protection).  in order to protect data
 * in shared maps we use pmap_page_protect().  [this is useful for IPC
 * mechanisms like map entry passing that may want to write-protect
 * all mappings of a shared amap.]  we traverse am_anon or am_slots
 * depending on the current state of the amap.
 *
 * => entry's map and amap must be locked by the caller
 */
void
amap_share_protect(struct vm_map_entry *entry, vm_prot_t prot)
{
	struct vm_amap *amap = entry->aref.ar_amap;
	u_int slots, lcv, slot, stop;
	struct vm_anon *anon;

	KASSERT(mutex_owned(amap->am_lock));

	AMAP_B2SLOT(slots, (entry->end - entry->start));
	stop = entry->aref.ar_pageoff + slots;

	if (slots < amap->am_nused) {
		/*
		 * Cheaper to traverse am_anon.
		 */
		for (lcv = entry->aref.ar_pageoff ; lcv < stop ; lcv++) {
			anon = amap->am_anon[lcv];
			if (anon == NULL) {
				continue;
			}
			if (anon->an_page) {
				pmap_page_protect(anon->an_page, prot);
			}
		}
		return;
	}

	/*
	 * Cheaper to traverse am_slots.
	 */
	for (lcv = 0 ; lcv < amap->am_nused ; lcv++) {
		slot = amap->am_slots[lcv];
		if (slot < entry->aref.ar_pageoff || slot >= stop) {
			continue;
		}
		anon = amap->am_anon[slot];
		if (anon->an_page) {
			pmap_page_protect(anon->an_page, prot);
		}
	}
}

/*
 * amap_wipeout: wipeout all anon's in an amap; then free the amap!
 *
 * => Called from amap_unref(), when reference count drops to zero.
 * => amap must be locked.
 */

void
amap_wipeout(struct vm_amap *amap)
{
	struct vm_anon *tofree = NULL;
	u_int lcv;

	UVMHIST_FUNC("amap_wipeout"); UVMHIST_CALLED(maphist);
	UVMHIST_LOG(maphist,"(amap=0x%x)", amap, 0,0,0);

	KASSERT(mutex_owned(amap->am_lock));
	KASSERT(amap->am_ref == 0);

	if (__predict_false(amap->am_flags & AMAP_SWAPOFF)) {
		/*
		 * Note: amap_swap_off() will call us again.
		 */
		amap_unlock(amap);
		return;
	}
	amap_list_remove(amap);

	for (lcv = 0 ; lcv < amap->am_nused ; lcv++) {
		struct vm_anon *anon;
		u_int slot;

		slot = amap->am_slots[lcv];
		anon = amap->am_anon[slot];
		KASSERT(anon != NULL && anon->an_ref != 0);

		KASSERT(anon->an_lock == amap->am_lock);
		UVMHIST_LOG(maphist,"  processing anon 0x%x, ref=%d", anon,
		    anon->an_ref, 0, 0);

		/*
		 * Drop the reference.  Defer freeing.
		 */

		if (--anon->an_ref == 0) {
			anon->an_link = tofree;
			tofree = anon;
		}
		if (curlwp->l_cpu->ci_schedstate.spc_flags & SPCF_SHOULDYIELD) {
			preempt();
		}
	}

	/*
	 * Finally, destroy the amap.
	 */

	amap->am_nused = 0;
	uvm_anon_freelst(amap, tofree);
	amap_free(amap);
	UVMHIST_LOG(maphist,"<- done!", 0,0,0,0);
}

/*
 * amap_copy: ensure that a map entry's "needs_copy" flag is false
 *	by copying the amap if necessary.
 *
 * => an entry with a null amap pointer will get a new (blank) one.
 * => the map that the map entry belongs to must be locked by caller.
 * => the amap currently attached to "entry" (if any) must be unlocked.
 * => if canchunk is true, then we may clip the entry into a chunk
 * => "startva" and "endva" are used only if canchunk is true.  they are
 *     used to limit chunking (e.g. if you have a large space that you
 *     know you are going to need to allocate amaps for, there is no point
 *     in allowing that to be chunked)
 */

void
amap_copy(struct vm_map *map, struct vm_map_entry *entry, int flags,
    vaddr_t startva, vaddr_t endva)
{
	const int waitf = (flags & AMAP_COPY_NOWAIT) ? UVM_FLAG_NOWAIT : 0;
	struct vm_amap *amap, *srcamap;
	struct vm_anon *tofree;
	u_int slots, lcv;
	vsize_t len;

	UVMHIST_FUNC("amap_copy"); UVMHIST_CALLED(maphist);
	UVMHIST_LOG(maphist, "  (map=%p, entry=%p, flags=%d)",
		    map, entry, flags, 0);

	KASSERT(map != kernel_map);	/* we use nointr pool */

	srcamap = entry->aref.ar_amap;
	len = entry->end - entry->start;

	/*
	 * Is there an amap to copy?  If not, create one.
	 */

	if (srcamap == NULL) {
		const bool canchunk = (flags & AMAP_COPY_NOCHUNK) == 0;

		/*
		 * Check to see if we have a large amap that we can
		 * chunk.  We align startva/endva to chunk-sized
		 * boundaries and then clip to them.
		 */

		if (canchunk && atop(len) >= UVM_AMAP_LARGE) {
			vsize_t chunksize;

			/* Convert slots to bytes. */
			chunksize = UVM_AMAP_CHUNK << PAGE_SHIFT;
			startva = (startva / chunksize) * chunksize;
			endva = roundup(endva, chunksize);
			UVMHIST_LOG(maphist, "  chunk amap ==> clip 0x%x->0x%x"
			    "to 0x%x->0x%x", entry->start, entry->end, startva,
			    endva);
			UVM_MAP_CLIP_START(map, entry, startva);

			/* Watch out for endva wrap-around! */
			if (endva >= startva) {
				UVM_MAP_CLIP_END(map, entry, endva);
			}
		}

		if ((flags & AMAP_COPY_NOMERGE) == 0 &&
		    uvm_mapent_trymerge(map, entry, UVM_MERGE_COPYING)) {
			return;
		}

		UVMHIST_LOG(maphist, "<- done [creating new amap 0x%x->0x%x]",
		    entry->start, entry->end, 0, 0);

		/*
		 * Allocate an initialised amap and install it.
		 * Note: we must update the length after clipping.
		 */
		len = entry->end - entry->start;
		entry->aref.ar_pageoff = 0;
		entry->aref.ar_amap = amap_alloc(len, 0, waitf);
		if (entry->aref.ar_amap != NULL) {
			entry->etype &= ~UVM_ET_NEEDSCOPY;
		}
		return;
	}

	/*
	 * First check and see if we are the only map entry referencing 
	 * he amap we currently have.  If so, then just take it over instead
	 * of copying it.  Note that we are reading am_ref without lock held
	 * as the value value can only be one if we have the only reference
	 * to the amap (via our locked map).  If the value is greater than
	 * one, then allocate amap and re-check the value.
	 */

	if (srcamap->am_ref == 1) {
		entry->etype &= ~UVM_ET_NEEDSCOPY;
		UVMHIST_LOG(maphist, "<- done [ref cnt = 1, took it over]",
		    0, 0, 0, 0);
		return;
	}

	UVMHIST_LOG(maphist,"  amap=%p, ref=%d, must copy it",
	    srcamap, srcamap->am_ref, 0, 0);

	/*
	 * Allocate a new amap (note: not initialised, no lock set, etc).
	 */

	AMAP_B2SLOT(slots, len);
	amap = amap_alloc1(slots, 0, waitf);
	if (amap == NULL) {
		UVMHIST_LOG(maphist, "  amap_alloc1 failed", 0,0,0,0);
		return;
	}

	amap_lock(srcamap);

	/*
	 * Re-check the reference count with the lock held.  If it has
	 * dropped to one - we can take over the existing map.
	 */

	if (srcamap->am_ref == 1) {
		/* Just take over the existing amap. */
		entry->etype &= ~UVM_ET_NEEDSCOPY;
		amap_unlock(srcamap);
		/* Destroy the new (unused) amap. */
		amap->am_ref--;
		amap_free(amap);
		return;
	}

	/*
	 * Copy the slots.  Zero the padded part.
	 */

	UVMHIST_LOG(maphist, "  copying amap now",0, 0, 0, 0);
	for (lcv = 0 ; lcv < slots; lcv++) {
		amap->am_anon[lcv] =
		    srcamap->am_anon[entry->aref.ar_pageoff + lcv];
		if (amap->am_anon[lcv] == NULL)
			continue;
		KASSERT(amap->am_anon[lcv]->an_lock == srcamap->am_lock);
		KASSERT(amap->am_anon[lcv]->an_ref > 0);
		KASSERT(amap->am_nused < amap->am_maxslot);
		amap->am_anon[lcv]->an_ref++;
		amap->am_bckptr[lcv] = amap->am_nused;
		amap->am_slots[amap->am_nused] = lcv;
		amap->am_nused++;
	}
	memset(&amap->am_anon[lcv], 0,
	    (amap->am_maxslot - lcv) * sizeof(struct vm_anon *));

	/*
	 * Drop our reference to the old amap (srcamap) and unlock.
	 * Since the reference count on srcamap is greater than one,
	 * (we checked above), it cannot drop to zero while it is locked.
	 */

	srcamap->am_ref--;
	KASSERT(srcamap->am_ref > 0);

	if (srcamap->am_ref == 1 && (srcamap->am_flags & AMAP_SHARED) != 0) {
		srcamap->am_flags &= ~AMAP_SHARED;
	}
	tofree = NULL;
#ifdef UVM_AMAP_PPREF
	if (srcamap->am_ppref && srcamap->am_ppref != PPREF_NONE) {
		amap_pp_adjref(srcamap, entry->aref.ar_pageoff,
		    len >> PAGE_SHIFT, -1, &tofree);
	}
#endif

	/*
	 * If we referenced any anons, then share the source amap's lock.
	 * Otherwise, we have nothing in common, so allocate a new one.
	 */

	KASSERT(amap->am_lock == NULL);
	if (amap->am_nused != 0) {
		amap->am_lock = srcamap->am_lock;
		mutex_obj_hold(amap->am_lock);
	}
	uvm_anon_freelst(srcamap, tofree);

	if (amap->am_lock == NULL) {
		amap->am_lock = mutex_obj_alloc(MUTEX_DEFAULT, IPL_NONE);
	}
	amap_list_insert(amap);

	/*
	 * Install new amap.
	 */

	entry->aref.ar_pageoff = 0;
	entry->aref.ar_amap = amap;
	entry->etype &= ~UVM_ET_NEEDSCOPY;
	UVMHIST_LOG(maphist, "<- done",0, 0, 0, 0);
}

/*
 * amap_cow_now: resolve all copy-on-write faults in an amap now for fork(2)
 *
 *	called during fork(2) when the parent process has a wired map
 *	entry.   in that case we want to avoid write-protecting pages
 *	in the parent's map (e.g. like what you'd do for a COW page)
 *	so we resolve the COW here.
 *
 * => assume parent's entry was wired, thus all pages are resident.
 * => assume pages that are loaned out (loan_count) are already mapped
 *	read-only in all maps, and thus no need for us to worry about them
 * => assume both parent and child vm_map's are locked
 * => caller passes child's map/entry in to us
 * => if we run out of memory we will unlock the amap and sleep _with_ the
 *	parent and child vm_map's locked(!).    we have to do this since
 *	we are in the middle of a fork(2) and we can't let the parent
 *	map change until we are done copying all the map entrys.
 * => XXXCDC: out of memory should cause fork to fail, but there is
 *	currently no easy way to do this (needs fix)
 * => page queues must be unlocked (we may lock them)
 */

void
amap_cow_now(struct vm_map *map, struct vm_map_entry *entry)
{
	struct vm_amap *amap = entry->aref.ar_amap;
	struct vm_anon *anon, *nanon;
	struct vm_page *pg, *npg;
	u_int lcv, slot;

	/*
	 * note that if we unlock the amap then we must ReStart the "lcv" for
	 * loop because some other process could reorder the anon's in the
	 * am_anon[] array on us while the lock is dropped.
	 */

ReStart:
	amap_lock(amap);
	for (lcv = 0 ; lcv < amap->am_nused ; lcv++) {
		slot = amap->am_slots[lcv];
		anon = amap->am_anon[slot];
		KASSERT(anon->an_lock == amap->am_lock);

		/*
		 * If anon has only one reference - we must have already
		 * copied it.  This can happen if we needed to sleep waiting
		 * for memory in a previous run through this loop.  The new
		 * page might even have been paged out, since is not wired.
		 */

		if (anon->an_ref == 1) {
			KASSERT(anon->an_page != NULL || anon->an_swslot != 0);
			continue;
		}

		/*
		 * The old page must be resident since the parent is wired.
		 */

		pg = anon->an_page;
		KASSERT(pg != NULL);
		KASSERT(pg->wire_count > 0);

		/*
		 * If the page is loaned then it must already be mapped
		 * read-only and we don't need to copy it.
		 */

		if (pg->loan_count != 0) {
			continue;
		}
		KASSERT(pg->uanon == anon && pg->uobject == NULL);

		/*
		 * If the page is busy, then we have to unlock, wait for
		 * it and then restart.
		 */

		if (pg->flags & PG_BUSY) {
			pg->flags |= PG_WANTED;
			UVM_UNLOCK_AND_WAIT(pg, amap->am_lock, false,
			    "cownow", 0);
			goto ReStart;
		}

		/*
		 * Perform a copy-on-write.
		 * First - get a new anon and a page.
		 */

		nanon = uvm_analloc();
		if (nanon) {
			nanon->an_lock = amap->am_lock;
			npg = uvm_pagealloc(NULL, 0, nanon, 0);
		} else {
			npg = NULL;
		}
		if (nanon == NULL || npg == NULL) {
			amap_unlock(amap);
			if (nanon) {
				nanon->an_lock = NULL;
				nanon->an_ref--;
				KASSERT(nanon->an_ref == 0);
				uvm_anon_free(nanon);
			}
			uvm_wait("cownowpage");
			goto ReStart;
		}

		/*
		 * Copy the data and replace anon with the new one.
		 * Also, setup its lock (share the with amap's lock).
		 */

		uvm_pagecopy(pg, npg);
		anon->an_ref--;
		KASSERT(anon->an_ref > 0);
		amap->am_anon[slot] = nanon;

		/*
		 * Drop PG_BUSY on new page.  Since its owner was locked all
		 * this time - it cannot be PG_RELEASED or PG_WANTED.
		 */

		mutex_enter(&uvm_pageqlock);
		uvm_pageactivate(npg);
		mutex_exit(&uvm_pageqlock);
		npg->flags &= ~(PG_BUSY|PG_FAKE);
		UVM_PAGE_OWN(npg, NULL);
	}
	amap_unlock(amap);
}

/*
 * amap_splitref: split a single reference into two separate references
 *
 * => called from uvm_map's clip routines
 * => origref's map should be locked
 * => origref->ar_amap should be unlocked (we will lock)
 */
void
amap_splitref(struct vm_aref *origref, struct vm_aref *splitref, vaddr_t offset)
{
	struct vm_amap *amap = origref->ar_amap;
	u_int leftslots;

	KASSERT(splitref->ar_amap == origref->ar_amap);
	AMAP_B2SLOT(leftslots, offset);
	KASSERT(leftslots != 0);

	amap_lock(amap);
	KASSERT(amap->am_nslot - origref->ar_pageoff - leftslots > 0);

#ifdef UVM_AMAP_PPREF
	/* Establish ppref before we add a duplicate reference to the amap. */
	if (amap->am_ppref == NULL) {
		amap_pp_establish(amap, origref->ar_pageoff);
	}
#endif
	/* Note: not a share reference. */
	amap->am_ref++;
	splitref->ar_pageoff = origref->ar_pageoff + leftslots;
	amap_unlock(amap);
}

#ifdef UVM_AMAP_PPREF

/*
 * amap_pp_establish: add a ppref array to an amap, if possible.
 *
 * => amap should be locked by caller.
 */
void
amap_pp_establish(struct vm_amap *amap, vaddr_t offset)
{
	const size_t sz = amap->am_maxslot * sizeof(*amap->am_ppref);

	KASSERT(mutex_owned(amap->am_lock));

	amap->am_ppref = kmem_zalloc(sz, KM_NOSLEEP);
	if (amap->am_ppref == NULL) {
		/* Failure - just do not use ppref. */
		amap->am_ppref = PPREF_NONE;
		return;
	}
	pp_setreflen(amap->am_ppref, 0, 0, offset);
	pp_setreflen(amap->am_ppref, offset, amap->am_ref,
	    amap->am_nslot - offset);
}

/*
 * amap_pp_adjref: adjust reference count to a part of an amap using the
 * per-page reference count array.
 *
 * => caller must check that ppref != PPREF_NONE before calling.
 * => map and amap must be locked.
 */
void
amap_pp_adjref(struct vm_amap *amap, int curslot, vsize_t slotlen, int adjval,
    struct vm_anon **tofree)
{
	int stopslot, *ppref, lcv, prevlcv;
	int ref, len, prevref, prevlen;

	KASSERT(mutex_owned(amap->am_lock));

	stopslot = curslot + slotlen;
	ppref = amap->am_ppref;
	prevlcv = 0;

	/*
	 * Advance to the correct place in the array, fragment if needed.
	 */

	for (lcv = 0 ; lcv < curslot ; lcv += len) {
		pp_getreflen(ppref, lcv, &ref, &len);
		if (lcv + len > curslot) {     /* goes past start? */
			pp_setreflen(ppref, lcv, ref, curslot - lcv);
			pp_setreflen(ppref, curslot, ref, len - (curslot -lcv));
			len = curslot - lcv;   /* new length of entry @ lcv */
		}
		prevlcv = lcv;
	}
	if (lcv == 0) {
		/*
		 * Ensure that the "prevref == ref" test below always
		 * fails, since we are starting from the beginning of
		 * the ppref array; that is, there is no previous chunk.
		 */
		prevref = -1;
		prevlen = 0;
	} else {
		pp_getreflen(ppref, prevlcv, &prevref, &prevlen);
	}

	/*
	 * Now adjust reference counts in range.  Merge the first
	 * changed entry with the last unchanged entry if possible.
	 */
	KASSERT(lcv == curslot);
	for (/* lcv already set */; lcv < stopslot ; lcv += len) {
		pp_getreflen(ppref, lcv, &ref, &len);
		if (lcv + len > stopslot) {     /* goes past end? */
			pp_setreflen(ppref, lcv, ref, stopslot - lcv);
			pp_setreflen(ppref, stopslot, ref,
			    len - (stopslot - lcv));
			len = stopslot - lcv;
		}
		ref += adjval;
		KASSERT(ref >= 0);
		KASSERT(ref <= amap->am_ref);
		if (lcv == prevlcv + prevlen && ref == prevref) {
			pp_setreflen(ppref, prevlcv, ref, prevlen + len);
		} else {
			pp_setreflen(ppref, lcv, ref, len);
		}
		if (ref == 0) {
			amap_wiperange(amap, lcv, len, tofree);
		}
	}
}

/*
 * amap_wiperange: wipe out a range of an amap.
 * Note: different from amap_wipeout because the amap is kept intact.
 *
 * => Both map and amap must be locked by caller.
 */
void
amap_wiperange(struct vm_amap *amap, int slotoff, int slots,
    struct vm_anon **tofree)
{
	u_int lcv, stop, slotend;
	bool byanon;

	KASSERT(mutex_owned(amap->am_lock));

	/*
	 * We can either traverse the amap by am_anon or by am_slots.
	 * Determine which way is less expensive.
	 */

	if (slots < amap->am_nused) {
		byanon = true;
		lcv = slotoff;
		stop = slotoff + slots;
		slotend = 0;
	} else {
		byanon = false;
		lcv = 0;
		stop = amap->am_nused;
		slotend = slotoff + slots;
	}

	while (lcv < stop) {
		struct vm_anon *anon;
		u_int curslot, ptr, last;

		if (byanon) {
			curslot = lcv++;	/* lcv advances here */
			if (amap->am_anon[curslot] == NULL)
				continue;
		} else {
			curslot = amap->am_slots[lcv];
			if (curslot < slotoff || curslot >= slotend) {
				lcv++;		/* lcv advances here */
				continue;
			}
			stop--;	/* drop stop, since anon will be removed */
		}
		anon = amap->am_anon[curslot];
		KASSERT(anon->an_lock == amap->am_lock);

		/*
		 * Remove anon from the amap.
		 */

		amap->am_anon[curslot] = NULL;
		ptr = amap->am_bckptr[curslot];
		last = amap->am_nused - 1;
		if (ptr != last) {
			amap->am_slots[ptr] = amap->am_slots[last];
			amap->am_bckptr[amap->am_slots[ptr]] = ptr;
		}
		amap->am_nused--;

		/*
		 * Drop its reference count.
		 */

		KASSERT(anon->an_lock == amap->am_lock);
		if (--anon->an_ref == 0) {
			/*
			 * Eliminated the last reference to an anon - defer
			 * freeing as uvm_anon_freelst() will unlock the amap.
			 */
			anon->an_link = *tofree;
			*tofree = anon;
		}
	}
}

#endif

#if defined(VMSWAP)

/*
 * amap_swap_off: pagein anonymous pages in amaps and drop swap slots.
 *
 * => called with swap_syscall_lock held.
 * => note that we don't always traverse all anons.
 *    eg. amaps being wiped out, released anons.
 * => return true if failed.
 */

bool
amap_swap_off(int startslot, int endslot)
{
	struct vm_amap *am;
	struct vm_amap *am_next;
	struct vm_amap marker_prev;
	struct vm_amap marker_next;
	bool rv = false;

#if defined(DIAGNOSTIC)
	memset(&marker_prev, 0, sizeof(marker_prev));
	memset(&marker_next, 0, sizeof(marker_next));
#endif /* defined(DIAGNOSTIC) */

	mutex_enter(&amap_list_lock);
	for (am = LIST_FIRST(&amap_list); am != NULL && !rv; am = am_next) {
		int i;

		LIST_INSERT_BEFORE(am, &marker_prev, am_list);
		LIST_INSERT_AFTER(am, &marker_next, am_list);

		if (!amap_lock_try(am)) {
			mutex_exit(&amap_list_lock);
			preempt();
			mutex_enter(&amap_list_lock);
			am_next = LIST_NEXT(&marker_prev, am_list);
			if (am_next == &marker_next) {
				am_next = LIST_NEXT(am_next, am_list);
			} else {
				KASSERT(LIST_NEXT(am_next, am_list) ==
				    &marker_next);
			}
			LIST_REMOVE(&marker_prev, am_list);
			LIST_REMOVE(&marker_next, am_list);
			continue;
		}

		mutex_exit(&amap_list_lock);

		if (am->am_nused <= 0) {
			amap_unlock(am);
			goto next;
		}

		for (i = 0; i < am->am_nused; i++) {
			int slot;
			int swslot;
			struct vm_anon *anon;

			slot = am->am_slots[i];
			anon = am->am_anon[slot];
			KASSERT(anon->an_lock == am->am_lock);

			swslot = anon->an_swslot;
			if (swslot < startslot || endslot <= swslot) {
				continue;
			}

			am->am_flags |= AMAP_SWAPOFF;

			rv = uvm_anon_pagein(am, anon);
			amap_lock(am);

			am->am_flags &= ~AMAP_SWAPOFF;
			if (amap_refs(am) == 0) {
				amap_wipeout(am);
				am = NULL;
				break;
			}
			if (rv) {
				break;
			}
			i = 0;
		}

		if (am) {
			amap_unlock(am);
		}
		
next:
		mutex_enter(&amap_list_lock);
		KASSERT(LIST_NEXT(&marker_prev, am_list) == &marker_next ||
		    LIST_NEXT(LIST_NEXT(&marker_prev, am_list), am_list) ==
		    &marker_next);
		am_next = LIST_NEXT(&marker_next, am_list);
		LIST_REMOVE(&marker_prev, am_list);
		LIST_REMOVE(&marker_next, am_list);
	}
	mutex_exit(&amap_list_lock);

	return rv;
}

#endif /* defined(VMSWAP) */

/*
 * amap_lookup: look up a page in an amap.
 *
 * => amap should be locked by caller.
 */
struct vm_anon *
amap_lookup(struct vm_aref *aref, vaddr_t offset)
{
	struct vm_amap *amap = aref->ar_amap;
	struct vm_anon *an;
	u_int slot;

	UVMHIST_FUNC("amap_lookup"); UVMHIST_CALLED(maphist);
	KASSERT(mutex_owned(amap->am_lock));

	AMAP_B2SLOT(slot, offset);
	slot += aref->ar_pageoff;
	an = amap->am_anon[slot];

	UVMHIST_LOG(maphist, "<- done (amap=0x%x, offset=0x%x, result=0x%x)",
	    amap, offset, an, 0);

	KASSERT(slot < amap->am_nslot);
	KASSERT(an == NULL || an->an_ref != 0);
	KASSERT(an == NULL || an->an_lock == amap->am_lock);
	return an;
}

/*
 * amap_lookups: look up a range of pages in an amap.
 *
 * => amap should be locked by caller.
 */
void
amap_lookups(struct vm_aref *aref, vaddr_t offset, struct vm_anon **anons,
    int npages)
{
	struct vm_amap *amap = aref->ar_amap;
	u_int slot;

	UVMHIST_FUNC("amap_lookups"); UVMHIST_CALLED(maphist);
	KASSERT(mutex_owned(amap->am_lock));

	AMAP_B2SLOT(slot, offset);
	slot += aref->ar_pageoff;

	UVMHIST_LOG(maphist, "  slot=%u, npages=%d, nslot=%d",
	    slot, npages, amap->am_nslot, 0);

	KASSERT((slot + (npages - 1)) < amap->am_nslot);
	memcpy(anons, &amap->am_anon[slot], npages * sizeof(struct vm_anon *));

#if defined(DIAGNOSTIC)
	for (int i = 0; i < npages; i++) {
		struct vm_anon * const an = anons[i];
		if (an == NULL) {
			continue;
		}
		KASSERT(an->an_ref != 0);
		KASSERT(an->an_lock == amap->am_lock);
	}
#endif
	UVMHIST_LOG(maphist, "<- done", 0, 0, 0, 0);
}

/*
 * amap_add: add (or replace) a page to an amap.
 *
 * => amap should be locked by caller.
 * => anon must have the lock associated with this amap.
 */
void
amap_add(struct vm_aref *aref, vaddr_t offset, struct vm_anon *anon,
    bool replace)
{
	struct vm_amap *amap = aref->ar_amap;
	u_int slot;

	UVMHIST_FUNC("amap_add"); UVMHIST_CALLED(maphist);
	KASSERT(mutex_owned(amap->am_lock));
	KASSERT(anon->an_lock == amap->am_lock);

	AMAP_B2SLOT(slot, offset);
	slot += aref->ar_pageoff;
	KASSERT(slot < amap->am_nslot);

	if (replace) {
		struct vm_anon *oanon = amap->am_anon[slot];

		KASSERT(oanon != NULL);
		if (oanon->an_page && (amap->am_flags & AMAP_SHARED) != 0) {
			pmap_page_protect(oanon->an_page, VM_PROT_NONE);
			/*
			 * XXX: suppose page is supposed to be wired somewhere?
			 */
		}
	} else {
		KASSERT(amap->am_anon[slot] == NULL);
		KASSERT(amap->am_nused < amap->am_maxslot);
		amap->am_bckptr[slot] = amap->am_nused;
		amap->am_slots[amap->am_nused] = slot;
		amap->am_nused++;
	}
	amap->am_anon[slot] = anon;
	UVMHIST_LOG(maphist,
	    "<- done (amap=0x%x, offset=0x%x, anon=0x%x, rep=%d)",
	    amap, offset, anon, replace);
}

/*
 * amap_unadd: remove a page from an amap.
 *
 * => amap should be locked by caller.
 */
void
amap_unadd(struct vm_aref *aref, vaddr_t offset)
{
	struct vm_amap *amap = aref->ar_amap;
	u_int slot, ptr, last;

	UVMHIST_FUNC("amap_unadd"); UVMHIST_CALLED(maphist);
	KASSERT(mutex_owned(amap->am_lock));

	AMAP_B2SLOT(slot, offset);
	slot += aref->ar_pageoff;
	KASSERT(slot < amap->am_nslot);
	KASSERT(amap->am_anon[slot] != NULL);
	KASSERT(amap->am_anon[slot]->an_lock == amap->am_lock);

	amap->am_anon[slot] = NULL;
	ptr = amap->am_bckptr[slot];

	last = amap->am_nused - 1;
	if (ptr != last) {
		/* Move the last entry to keep the slots contiguous. */
		amap->am_slots[ptr] = amap->am_slots[last];
		amap->am_bckptr[amap->am_slots[ptr]] = ptr;
	}
	amap->am_nused--;
	UVMHIST_LOG(maphist, "<- done (amap=0x%x, slot=0x%x)", amap, slot,0, 0);
}

/*
 * amap_adjref_anons: adjust the reference count(s) on amap and its anons.
 */
static void
amap_adjref_anons(struct vm_amap *amap, vaddr_t offset, vsize_t len,
    int refv, bool all)
{
	struct vm_anon *tofree = NULL;

#ifdef UVM_AMAP_PPREF
	KASSERT(mutex_owned(amap->am_lock));

	/*
	 * We must establish the ppref array before changing am_ref
	 * so that the ppref values match the current amap refcount.
	 */

	if (amap->am_ppref == NULL && !all && len != amap->am_nslot) {
		amap_pp_establish(amap, offset);
	}
#endif

	amap->am_ref += refv;

#ifdef UVM_AMAP_PPREF
	if (amap->am_ppref && amap->am_ppref != PPREF_NONE) {
		if (all) {
			amap_pp_adjref(amap, 0, amap->am_nslot, refv, &tofree);
		} else {
			amap_pp_adjref(amap, offset, len, refv, &tofree);
		}
	}
#endif
	uvm_anon_freelst(amap, tofree);
}

/*
 * amap_ref: gain a reference to an amap.
 *
 * => amap must not be locked (we will lock).
 * => "offset" and "len" are in units of pages.
 * => Called at fork time to gain the child's reference.
 */
void
amap_ref(struct vm_amap *amap, vaddr_t offset, vsize_t len, int flags)
{
	UVMHIST_FUNC("amap_ref"); UVMHIST_CALLED(maphist);

	amap_lock(amap);
	if (flags & AMAP_SHARED) {
		amap->am_flags |= AMAP_SHARED;
	}
	amap_adjref_anons(amap, offset, len, 1, (flags & AMAP_REFALL) != 0);

	UVMHIST_LOG(maphist,"<- done!  amap=0x%x", amap, 0, 0, 0);
}

/*
 * amap_unref: remove a reference to an amap.
 *
 * => All pmap-level references to this amap must be already removed.
 * => Called from uvm_unmap_detach(); entry is already removed from the map.
 * => We will lock amap, so it must be unlocked.
 */
void
amap_unref(struct vm_amap *amap, vaddr_t offset, vsize_t len, bool all)
{
	UVMHIST_FUNC("amap_unref"); UVMHIST_CALLED(maphist);

	amap_lock(amap);

	UVMHIST_LOG(maphist,"  amap=0x%x  refs=%d, nused=%d",
	    amap, amap->am_ref, amap->am_nused, 0);
	KASSERT(amap->am_ref > 0);

	if (amap->am_ref == 1) {

		/*
		 * If the last reference - wipeout and destroy the amap.
		 */
		amap->am_ref--;
		amap_wipeout(amap);
		UVMHIST_LOG(maphist,"<- done (was last ref)!", 0, 0, 0, 0);
		return;
	}

	/*
	 * Otherwise, drop the reference count(s) on anons.
	 */

	if (amap->am_ref == 2 && (amap->am_flags & AMAP_SHARED) != 0) {
		amap->am_flags &= ~AMAP_SHARED;
	}
	amap_adjref_anons(amap, offset, len, -1, all);

	UVMHIST_LOG(maphist,"<- done!", 0, 0, 0, 0);
}

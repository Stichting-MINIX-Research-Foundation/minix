/*	$NetBSD: uvm_bio.c,v 1.83 2015/05/27 19:43:40 rmind Exp $	*/

/*
 * Copyright (c) 1998 Chuck Silvers.
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * uvm_bio.c: buffered i/o object mapping cache
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_bio.c,v 1.83 2015/05/27 19:43:40 rmind Exp $");

#include "opt_uvmhist.h"
#include "opt_ubc.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/vnode.h>

#include <uvm/uvm.h>

/*
 * global data structures
 */

/*
 * local functions
 */

static int	ubc_fault(struct uvm_faultinfo *, vaddr_t, struct vm_page **,
			  int, int, vm_prot_t, int);
static struct ubc_map *ubc_find_mapping(struct uvm_object *, voff_t);

/*
 * local data structues
 */

#define UBC_HASH(uobj, offset) 						\
	(((((u_long)(uobj)) >> 8) + (((u_long)(offset)) >> PAGE_SHIFT)) & \
				ubc_object.hashmask)

#define UBC_QUEUE(offset)						\
	(&ubc_object.inactive[(((u_long)(offset)) >> ubc_winshift) &	\
			     (UBC_NQUEUES - 1)])

#define UBC_UMAP_ADDR(u)						\
	(vaddr_t)(ubc_object.kva + (((u) - ubc_object.umap) << ubc_winshift))


#define UMAP_PAGES_LOCKED	0x0001
#define UMAP_MAPPING_CACHED	0x0002

struct ubc_map {
	struct uvm_object *	uobj;		/* mapped object */
	voff_t			offset;		/* offset into uobj */
	voff_t			writeoff;	/* write offset */
	vsize_t			writelen;	/* write len */
	int			refcount;	/* refcount on mapping */
	int			flags;		/* extra state */
	int			advice;

	LIST_ENTRY(ubc_map)	hash;		/* hash table */
	TAILQ_ENTRY(ubc_map)	inactive;	/* inactive queue */
	LIST_ENTRY(ubc_map)	list;		/* per-object list */
};

TAILQ_HEAD(ubc_inactive_head, ubc_map);
static struct ubc_object {
	struct uvm_object uobj;		/* glue for uvm_map() */
	char *kva;			/* where ubc_object is mapped */
	struct ubc_map *umap;		/* array of ubc_map's */

	LIST_HEAD(, ubc_map) *hash;	/* hashtable for cached ubc_map's */
	u_long hashmask;		/* mask for hashtable */

	struct ubc_inactive_head *inactive;
					/* inactive queues for ubc_map's */
} ubc_object;

const struct uvm_pagerops ubc_pager = {
	.pgo_fault = ubc_fault,
	/* ... rest are NULL */
};

int ubc_nwins = UBC_NWINS;
int ubc_winshift = UBC_WINSHIFT;
int ubc_winsize;
#if defined(PMAP_PREFER)
int ubc_nqueues;
#define UBC_NQUEUES ubc_nqueues
#else
#define UBC_NQUEUES 1
#endif

#if defined(UBC_STATS)

#define	UBC_EVCNT_DEFINE(name) \
struct evcnt ubc_evcnt_##name = \
EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "ubc", #name); \
EVCNT_ATTACH_STATIC(ubc_evcnt_##name);
#define	UBC_EVCNT_INCR(name) ubc_evcnt_##name.ev_count++

#else /* defined(UBC_STATS) */

#define	UBC_EVCNT_DEFINE(name)	/* nothing */
#define	UBC_EVCNT_INCR(name)	/* nothing */

#endif /* defined(UBC_STATS) */

UBC_EVCNT_DEFINE(wincachehit)
UBC_EVCNT_DEFINE(wincachemiss)
UBC_EVCNT_DEFINE(faultbusy)

/*
 * ubc_init
 *
 * init pager private data structures.
 */

void
ubc_init(void)
{
	struct ubc_map *umap;
	vaddr_t va;
	int i;

	/*
	 * Make sure ubc_winshift is sane.
	 */
	if (ubc_winshift < PAGE_SHIFT)
		ubc_winshift = PAGE_SHIFT;

	/*
	 * init ubc_object.
	 * alloc and init ubc_map's.
	 * init inactive queues.
	 * alloc and init hashtable.
	 * map in ubc_object.
	 */

	uvm_obj_init(&ubc_object.uobj, &ubc_pager, true, UVM_OBJ_KERN);

	ubc_object.umap = kmem_zalloc(ubc_nwins * sizeof(struct ubc_map),
	    KM_SLEEP);
	if (ubc_object.umap == NULL)
		panic("ubc_init: failed to allocate ubc_map");

	if (ubc_winshift < PAGE_SHIFT) {
		ubc_winshift = PAGE_SHIFT;
	}
	va = (vaddr_t)1L;
#ifdef PMAP_PREFER
	PMAP_PREFER(0, &va, 0, 0);	/* kernel is never topdown */
	ubc_nqueues = va >> ubc_winshift;
	if (ubc_nqueues == 0) {
		ubc_nqueues = 1;
	}
#endif
	ubc_winsize = 1 << ubc_winshift;
	ubc_object.inactive = kmem_alloc(UBC_NQUEUES *
	    sizeof(struct ubc_inactive_head), KM_SLEEP);
	if (ubc_object.inactive == NULL)
		panic("ubc_init: failed to allocate inactive queue heads");
	for (i = 0; i < UBC_NQUEUES; i++) {
		TAILQ_INIT(&ubc_object.inactive[i]);
	}
	for (i = 0; i < ubc_nwins; i++) {
		umap = &ubc_object.umap[i];
		TAILQ_INSERT_TAIL(&ubc_object.inactive[i & (UBC_NQUEUES - 1)],
				  umap, inactive);
	}

	ubc_object.hash = hashinit(ubc_nwins, HASH_LIST, true,
	    &ubc_object.hashmask);
	for (i = 0; i <= ubc_object.hashmask; i++) {
		LIST_INIT(&ubc_object.hash[i]);
	}

	if (uvm_map(kernel_map, (vaddr_t *)&ubc_object.kva,
		    ubc_nwins << ubc_winshift, &ubc_object.uobj, 0, (vsize_t)va,
		    UVM_MAPFLAG(UVM_PROT_ALL, UVM_PROT_ALL, UVM_INH_NONE,
				UVM_ADV_RANDOM, UVM_FLAG_NOMERGE)) != 0) {
		panic("ubc_init: failed to map ubc_object");
	}
}

void
ubchist_init(void)
{

	UVMHIST_INIT(ubchist, 300);
}

/*
 * ubc_fault_page: helper of ubc_fault to handle a single page.
 *
 * => Caller has UVM object locked.
 * => Caller will perform pmap_update().
 */

static inline int
ubc_fault_page(const struct uvm_faultinfo *ufi, const struct ubc_map *umap,
    struct vm_page *pg, vm_prot_t prot, vm_prot_t access_type, vaddr_t va)
{
	struct uvm_object *uobj;
	vm_prot_t mask;
	int error;
	bool rdonly;

	uobj = pg->uobject;
	KASSERT(mutex_owned(uobj->vmobjlock));

	if (pg->flags & PG_WANTED) {
		wakeup(pg);
	}
	KASSERT((pg->flags & PG_FAKE) == 0);
	if (pg->flags & PG_RELEASED) {
		mutex_enter(&uvm_pageqlock);
		uvm_pagefree(pg);
		mutex_exit(&uvm_pageqlock);
		return 0;
	}
	if (pg->loan_count != 0) {

		/*
		 * Avoid unneeded loan break, if possible.
		 */

		if ((access_type & VM_PROT_WRITE) == 0) {
			prot &= ~VM_PROT_WRITE;
		}
		if (prot & VM_PROT_WRITE) {
			struct vm_page *newpg;

			newpg = uvm_loanbreak(pg);
			if (newpg == NULL) {
				uvm_page_unbusy(&pg, 1);
				return ENOMEM;
			}
			pg = newpg;
		}
	}

	/*
	 * Note that a page whose backing store is partially allocated
	 * is marked as PG_RDONLY.
	 */

	KASSERT((pg->flags & PG_RDONLY) == 0 ||
	    (access_type & VM_PROT_WRITE) == 0 ||
	    pg->offset < umap->writeoff ||
	    pg->offset + PAGE_SIZE > umap->writeoff + umap->writelen);

	rdonly = ((access_type & VM_PROT_WRITE) == 0 &&
	    (pg->flags & PG_RDONLY) != 0) ||
	    UVM_OBJ_NEEDS_WRITEFAULT(uobj);
	mask = rdonly ? ~VM_PROT_WRITE : VM_PROT_ALL;

	error = pmap_enter(ufi->orig_map->pmap, va, VM_PAGE_TO_PHYS(pg),
	    prot & mask, PMAP_CANFAIL | (access_type & mask));

	mutex_enter(&uvm_pageqlock);
	uvm_pageactivate(pg);
	mutex_exit(&uvm_pageqlock);
	pg->flags &= ~(PG_BUSY|PG_WANTED);
	UVM_PAGE_OWN(pg, NULL);

	return error;
}

/*
 * ubc_fault: fault routine for ubc mapping
 */

static int
ubc_fault(struct uvm_faultinfo *ufi, vaddr_t ign1, struct vm_page **ign2,
    int ign3, int ign4, vm_prot_t access_type, int flags)
{
	struct uvm_object *uobj;
	struct ubc_map *umap;
	vaddr_t va, eva, ubc_offset, slot_offset;
	struct vm_page *pgs[ubc_winsize >> PAGE_SHIFT];
	int i, error, npages;
	vm_prot_t prot;

	UVMHIST_FUNC("ubc_fault"); UVMHIST_CALLED(ubchist);

	/*
	 * no need to try with PGO_LOCKED...
	 * we don't need to have the map locked since we know that
	 * no one will mess with it until our reference is released.
	 */

	if (flags & PGO_LOCKED) {
		uvmfault_unlockall(ufi, NULL, &ubc_object.uobj);
		flags &= ~PGO_LOCKED;
	}

	va = ufi->orig_rvaddr;
	ubc_offset = va - (vaddr_t)ubc_object.kva;
	umap = &ubc_object.umap[ubc_offset >> ubc_winshift];
	KASSERT(umap->refcount != 0);
	KASSERT((umap->flags & UMAP_PAGES_LOCKED) == 0);
	slot_offset = ubc_offset & (ubc_winsize - 1);

	/*
	 * some platforms cannot write to individual bytes atomically, so
	 * software has to do read/modify/write of larger quantities instead.
	 * this means that the access_type for "write" operations
	 * can be VM_PROT_READ, which confuses us mightily.
	 *
	 * deal with this by resetting access_type based on the info
	 * that ubc_alloc() stores for us.
	 */

	access_type = umap->writelen ? VM_PROT_WRITE : VM_PROT_READ;
	UVMHIST_LOG(ubchist, "va 0x%lx ubc_offset 0x%lx access_type %d",
	    va, ubc_offset, access_type, 0);

#ifdef DIAGNOSTIC
	if ((access_type & VM_PROT_WRITE) != 0) {
		if (slot_offset < trunc_page(umap->writeoff) ||
		    umap->writeoff + umap->writelen <= slot_offset) {
			panic("ubc_fault: out of range write");
		}
	}
#endif

	/* no umap locking needed since we have a ref on the umap */
	uobj = umap->uobj;

	if ((access_type & VM_PROT_WRITE) == 0) {
		npages = (ubc_winsize - slot_offset) >> PAGE_SHIFT;
	} else {
		npages = (round_page(umap->offset + umap->writeoff +
		    umap->writelen) - (umap->offset + slot_offset))
		    >> PAGE_SHIFT;
		flags |= PGO_PASTEOF;
	}

again:
	memset(pgs, 0, sizeof (pgs));
	mutex_enter(uobj->vmobjlock);

	UVMHIST_LOG(ubchist, "slot_offset 0x%x writeoff 0x%x writelen 0x%x ",
	    slot_offset, umap->writeoff, umap->writelen, 0);
	UVMHIST_LOG(ubchist, "getpages uobj %p offset 0x%x npages %d",
	    uobj, umap->offset + slot_offset, npages, 0);

	error = (*uobj->pgops->pgo_get)(uobj, umap->offset + slot_offset, pgs,
	    &npages, 0, access_type, umap->advice, flags | PGO_NOBLOCKALLOC |
	    PGO_NOTIMESTAMP);
	UVMHIST_LOG(ubchist, "getpages error %d npages %d", error, npages, 0,
	    0);

	if (error == EAGAIN) {
		kpause("ubc_fault", false, hz >> 2, NULL);
		goto again;
	}
	if (error) {
		return error;
	}

	/*
	 * For virtually-indexed, virtually-tagged caches we should avoid
	 * creating writable mappings when we do not absolutely need them,
	 * since the "compatible alias" trick does not work on such caches.
	 * Otherwise, we can always map the pages writable.
	 */

#ifdef PMAP_CACHE_VIVT
	prot = VM_PROT_READ | access_type;
#else
	prot = VM_PROT_READ | VM_PROT_WRITE;
#endif

	va = ufi->orig_rvaddr;
	eva = ufi->orig_rvaddr + (npages << PAGE_SHIFT);

	UVMHIST_LOG(ubchist, "va 0x%lx eva 0x%lx", va, eva, 0, 0);

	/*
	 * Note: normally all returned pages would have the same UVM object.
	 * However, layered file-systems and e.g. tmpfs, may return pages
	 * which belong to underlying UVM object.  In such case, lock is
	 * shared amongst the objects.
	 */
	mutex_enter(uobj->vmobjlock);
	for (i = 0; va < eva; i++, va += PAGE_SIZE) {
		struct vm_page *pg;

		UVMHIST_LOG(ubchist, "pgs[%d] = %p", i, pgs[i], 0, 0);
		pg = pgs[i];

		if (pg == NULL || pg == PGO_DONTCARE) {
			continue;
		}
		KASSERT(uobj->vmobjlock == pg->uobject->vmobjlock);
		error = ubc_fault_page(ufi, umap, pg, prot, access_type, va);
		if (error) {
			/*
			 * Flush (there might be pages entered), drop the lock,
			 * and perform uvm_wait().  Note: page will re-fault.
			 */
			pmap_update(ufi->orig_map->pmap);
			mutex_exit(uobj->vmobjlock);
			uvm_wait("ubc_fault");
			mutex_enter(uobj->vmobjlock);
		}
	}
	/* Must make VA visible before the unlock. */
	pmap_update(ufi->orig_map->pmap);
	mutex_exit(uobj->vmobjlock);

	return 0;
}

/*
 * local functions
 */

static struct ubc_map *
ubc_find_mapping(struct uvm_object *uobj, voff_t offset)
{
	struct ubc_map *umap;

	LIST_FOREACH(umap, &ubc_object.hash[UBC_HASH(uobj, offset)], hash) {
		if (umap->uobj == uobj && umap->offset == offset) {
			return umap;
		}
	}
	return NULL;
}


/*
 * ubc interface functions
 */

/*
 * ubc_alloc:  allocate a file mapping window
 */

void *
ubc_alloc(struct uvm_object *uobj, voff_t offset, vsize_t *lenp, int advice,
    int flags)
{
	vaddr_t slot_offset, va;
	struct ubc_map *umap;
	voff_t umap_offset;
	int error;
	UVMHIST_FUNC("ubc_alloc"); UVMHIST_CALLED(ubchist);

	UVMHIST_LOG(ubchist, "uobj %p offset 0x%lx len 0x%lx",
	    uobj, offset, *lenp, 0);

	KASSERT(*lenp > 0);
	umap_offset = (offset & ~((voff_t)ubc_winsize - 1));
	slot_offset = (vaddr_t)(offset & ((voff_t)ubc_winsize - 1));
	*lenp = MIN(*lenp, ubc_winsize - slot_offset);

	mutex_enter(ubc_object.uobj.vmobjlock);
again:
	/*
	 * The UVM object is already referenced.
	 * Lock order: UBC object -> ubc_map::uobj.
	 */
	umap = ubc_find_mapping(uobj, umap_offset);
	if (umap == NULL) {
		struct uvm_object *oobj;

		UBC_EVCNT_INCR(wincachemiss);
		umap = TAILQ_FIRST(UBC_QUEUE(offset));
		if (umap == NULL) {
			kpause("ubc_alloc", false, hz >> 2,
			    ubc_object.uobj.vmobjlock);
			goto again;
		}

		va = UBC_UMAP_ADDR(umap);
		oobj = umap->uobj;

		/*
		 * Remove from old hash (if any), add to new hash.
		 */

		if (oobj != NULL) {
			/*
			 * Mapping must be removed before the list entry,
			 * since there is a race with ubc_purge().
			 */
			if (umap->flags & UMAP_MAPPING_CACHED) {
				umap->flags &= ~UMAP_MAPPING_CACHED;
				mutex_enter(oobj->vmobjlock);
				pmap_remove(pmap_kernel(), va,
				    va + ubc_winsize);
				pmap_update(pmap_kernel());
				mutex_exit(oobj->vmobjlock);
			}
			LIST_REMOVE(umap, hash);
			LIST_REMOVE(umap, list);
		} else {
			KASSERT((umap->flags & UMAP_MAPPING_CACHED) == 0);
		}
		umap->uobj = uobj;
		umap->offset = umap_offset;
		LIST_INSERT_HEAD(&ubc_object.hash[UBC_HASH(uobj, umap_offset)],
		    umap, hash);
		LIST_INSERT_HEAD(&uobj->uo_ubc, umap, list);
	} else {
		UBC_EVCNT_INCR(wincachehit);
		va = UBC_UMAP_ADDR(umap);
	}

	if (umap->refcount == 0) {
		TAILQ_REMOVE(UBC_QUEUE(offset), umap, inactive);
	}

	if (flags & UBC_WRITE) {
		KASSERTMSG(umap->writeoff == 0 && umap->writelen == 0,
		    "ubc_alloc: concurrent writes to uobj %p", uobj);
		umap->writeoff = slot_offset;
		umap->writelen = *lenp;
	}

	umap->refcount++;
	umap->advice = advice;
	mutex_exit(ubc_object.uobj.vmobjlock);
	UVMHIST_LOG(ubchist, "umap %p refs %d va %p flags 0x%x",
	    umap, umap->refcount, va, flags);

	if (flags & UBC_FAULTBUSY) {
		int npages = (*lenp + PAGE_SIZE - 1) >> PAGE_SHIFT;
		struct vm_page *pgs[npages];
		int gpflags =
		    PGO_SYNCIO|PGO_OVERWRITE|PGO_PASTEOF|PGO_NOBLOCKALLOC|
		    PGO_NOTIMESTAMP;
		int i;
		KDASSERT(flags & UBC_WRITE);
		KASSERT(umap->refcount == 1);

		UBC_EVCNT_INCR(faultbusy);
again_faultbusy:
		mutex_enter(uobj->vmobjlock);
		if (umap->flags & UMAP_MAPPING_CACHED) {
			umap->flags &= ~UMAP_MAPPING_CACHED;
			pmap_remove(pmap_kernel(), va, va + ubc_winsize);
		}
		memset(pgs, 0, sizeof(pgs));

		error = (*uobj->pgops->pgo_get)(uobj, trunc_page(offset), pgs,
		    &npages, 0, VM_PROT_READ | VM_PROT_WRITE, advice, gpflags);
		UVMHIST_LOG(ubchist, "faultbusy getpages %d", error, 0, 0, 0);
		if (error) {
			/*
			 * Flush: the mapping above might have been removed.
			 */
			pmap_update(pmap_kernel());
			goto out;
		}
		for (i = 0; i < npages; i++) {
			struct vm_page *pg = pgs[i];

			KASSERT(pg->uobject == uobj);
			if (pg->loan_count != 0) {
				mutex_enter(uobj->vmobjlock);
				if (pg->loan_count != 0) {
					pg = uvm_loanbreak(pg);
				}
				if (pg == NULL) {
					pmap_kremove(va, ubc_winsize);
					pmap_update(pmap_kernel());
					uvm_page_unbusy(pgs, npages);
					mutex_exit(uobj->vmobjlock);
					uvm_wait("ubc_alloc");
					goto again_faultbusy;
				}
				mutex_exit(uobj->vmobjlock);
				pgs[i] = pg;
			}
			pmap_kenter_pa(va + slot_offset + (i << PAGE_SHIFT),
			    VM_PAGE_TO_PHYS(pg),
			    VM_PROT_READ | VM_PROT_WRITE, 0);
		}
		pmap_update(pmap_kernel());
		umap->flags |= UMAP_PAGES_LOCKED;
	} else {
		KASSERT((umap->flags & UMAP_PAGES_LOCKED) == 0);
	}

out:
	return (void *)(va + slot_offset);
}

/*
 * ubc_release:  free a file mapping window.
 */

void
ubc_release(void *va, int flags)
{
	struct ubc_map *umap;
	struct uvm_object *uobj;
	vaddr_t umapva;
	bool unmapped;
	UVMHIST_FUNC("ubc_release"); UVMHIST_CALLED(ubchist);

	UVMHIST_LOG(ubchist, "va %p", va, 0, 0, 0);
	umap = &ubc_object.umap[((char *)va - ubc_object.kva) >> ubc_winshift];
	umapva = UBC_UMAP_ADDR(umap);
	uobj = umap->uobj;
	KASSERT(uobj != NULL);

	if (umap->flags & UMAP_PAGES_LOCKED) {
		const voff_t slot_offset = umap->writeoff;
		const voff_t endoff = umap->writeoff + umap->writelen;
		const voff_t zerolen = round_page(endoff) - endoff;
		const u_int npages = (round_page(endoff) -
		    trunc_page(slot_offset)) >> PAGE_SHIFT;
		struct vm_page *pgs[npages];

		KASSERT((umap->flags & UMAP_MAPPING_CACHED) == 0);
		if (zerolen) {
			memset((char *)umapva + endoff, 0, zerolen);
		}
		umap->flags &= ~UMAP_PAGES_LOCKED;
		mutex_enter(uobj->vmobjlock);
		mutex_enter(&uvm_pageqlock);
		for (u_int i = 0; i < npages; i++) {
			paddr_t pa;
			bool rv __diagused;

			rv = pmap_extract(pmap_kernel(),
			    umapva + slot_offset + (i << PAGE_SHIFT), &pa);
			KASSERT(rv);
			pgs[i] = PHYS_TO_VM_PAGE(pa);
			pgs[i]->flags &= ~(PG_FAKE|PG_CLEAN);
			KASSERT(pgs[i]->loan_count == 0);
			uvm_pageactivate(pgs[i]);
		}
		mutex_exit(&uvm_pageqlock);
		pmap_kremove(umapva, ubc_winsize);
		pmap_update(pmap_kernel());
		uvm_page_unbusy(pgs, npages);
		mutex_exit(uobj->vmobjlock);
		unmapped = true;
	} else {
		unmapped = false;
	}

	mutex_enter(ubc_object.uobj.vmobjlock);
	umap->writeoff = 0;
	umap->writelen = 0;
	umap->refcount--;
	if (umap->refcount == 0) {
		if (flags & UBC_UNMAP) {
			/*
			 * Invalidate any cached mappings if requested.
			 * This is typically used to avoid leaving
			 * incompatible cache aliases around indefinitely.
			 */
			mutex_enter(uobj->vmobjlock);
			pmap_remove(pmap_kernel(), umapva,
				    umapva + ubc_winsize);
			pmap_update(pmap_kernel());
			mutex_exit(uobj->vmobjlock);

			umap->flags &= ~UMAP_MAPPING_CACHED;
			LIST_REMOVE(umap, hash);
			LIST_REMOVE(umap, list);
			umap->uobj = NULL;
			TAILQ_INSERT_HEAD(UBC_QUEUE(umap->offset), umap,
			    inactive);
		} else {
			if (!unmapped) {
				umap->flags |= UMAP_MAPPING_CACHED;
			}
			TAILQ_INSERT_TAIL(UBC_QUEUE(umap->offset), umap,
			    inactive);
		}
	}
	UVMHIST_LOG(ubchist, "umap %p refs %d", umap, umap->refcount, 0, 0);
	mutex_exit(ubc_object.uobj.vmobjlock);
}

/*
 * ubc_uiomove: move data to/from an object.
 */

int
ubc_uiomove(struct uvm_object *uobj, struct uio *uio, vsize_t todo, int advice,
    int flags)
{
	const bool overwrite = (flags & UBC_FAULTBUSY) != 0;
	voff_t off;
	int error;

	KASSERT(todo <= uio->uio_resid);
	KASSERT(((flags & UBC_WRITE) != 0 && uio->uio_rw == UIO_WRITE) ||
	    ((flags & UBC_READ) != 0 && uio->uio_rw == UIO_READ));

	off = uio->uio_offset;
	error = 0;
	while (todo > 0) {
		vsize_t bytelen = todo;
		void *win;

		win = ubc_alloc(uobj, off, &bytelen, advice, flags);
		if (error == 0) {
			error = uiomove(win, bytelen, uio);
		}
		if (error != 0 && overwrite) {
			/*
			 * if we haven't initialized the pages yet,
			 * do it now.  it's safe to use memset here
			 * because we just mapped the pages above.
			 */
			printf("%s: error=%d\n", __func__, error);
			memset(win, 0, bytelen);
		}
		ubc_release(win, flags);
		off += bytelen;
		todo -= bytelen;
		if (error != 0 && (flags & UBC_PARTIALOK) != 0) {
			break;
		}
	}

	return error;
}

/*
 * ubc_zerorange: set a range of bytes in an object to zero.
 */

void
ubc_zerorange(struct uvm_object *uobj, off_t off, size_t len, int flags)
{
	void *win;

	/*
	 * XXXUBC invent kzero() and use it
	 */

	while (len) {
		vsize_t bytelen = len;

		win = ubc_alloc(uobj, off, &bytelen, UVM_ADV_NORMAL, UBC_WRITE);
		memset(win, 0, bytelen);
		ubc_release(win, flags);

		off += bytelen;
		len -= bytelen;
	}
}

/*
 * ubc_purge: disassociate ubc_map structures from an empty uvm_object.
 */

void
ubc_purge(struct uvm_object *uobj)
{
	struct ubc_map *umap;
	vaddr_t va;

	KASSERT(uobj->uo_npages == 0);

	/*
	 * Safe to check without lock held, as ubc_alloc() removes
	 * the mapping and list entry in the correct order.
	 */
	if (__predict_true(LIST_EMPTY(&uobj->uo_ubc))) {
		return;
	}
	mutex_enter(ubc_object.uobj.vmobjlock);
	while ((umap = LIST_FIRST(&uobj->uo_ubc)) != NULL) {
		KASSERT(umap->refcount == 0);
		for (va = 0; va < ubc_winsize; va += PAGE_SIZE) {
			KASSERT(!pmap_extract(pmap_kernel(),
			    va + UBC_UMAP_ADDR(umap), NULL));
		}
		LIST_REMOVE(umap, list);
		LIST_REMOVE(umap, hash);
		umap->flags &= ~UMAP_MAPPING_CACHED;
		umap->uobj = NULL;
	}
	mutex_exit(ubc_object.uobj.vmobjlock);
}

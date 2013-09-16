/*	$NetBSD: uvm_page.c,v 1.182 2012/02/16 11:46:14 matt Exp $	*/

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
 *	@(#)vm_page.c   8.3 (Berkeley) 3/21/94
 * from: Id: uvm_page.c,v 1.1.2.18 1998/02/06 05:24:42 chs Exp
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
 * uvm_page.c: page ops.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_page.c,v 1.182 2012/02/16 11:46:14 matt Exp $");

#include "opt_ddb.h"
#include "opt_uvmhist.h"
#include "opt_readahead.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sched.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/atomic.h>
#include <sys/cpu.h>

#include <uvm/uvm.h>
#include <uvm/uvm_ddb.h>
#include <uvm/uvm_pdpolicy.h>

/*
 * global vars... XXXCDC: move to uvm. structure.
 */

/*
 * physical memory config is stored in vm_physmem.
 */

struct vm_physseg vm_physmem[VM_PHYSSEG_MAX];	/* XXXCDC: uvm.physmem */
int vm_nphysseg = 0;				/* XXXCDC: uvm.nphysseg */
#define	vm_nphysmem	vm_nphysseg

/*
 * Some supported CPUs in a given architecture don't support all
 * of the things necessary to do idle page zero'ing efficiently.
 * We therefore provide a way to enable it from machdep code here.
 */
bool vm_page_zero_enable = false;

/*
 * number of pages per-CPU to reserve for the kernel.
 */
int vm_page_reserve_kernel = 5;

/*
 * physical memory size;
 */
int physmem;

/*
 * local variables
 */

/*
 * these variables record the values returned by vm_page_bootstrap,
 * for debugging purposes.  The implementation of uvm_pageboot_alloc
 * and pmap_startup here also uses them internally.
 */

static vaddr_t      virtual_space_start;
static vaddr_t      virtual_space_end;

/*
 * we allocate an initial number of page colors in uvm_page_init(),
 * and remember them.  We may re-color pages as cache sizes are
 * discovered during the autoconfiguration phase.  But we can never
 * free the initial set of buckets, since they are allocated using
 * uvm_pageboot_alloc().
 */

static size_t recolored_pages_memsize /* = 0 */;

#ifdef DEBUG
vaddr_t uvm_zerocheckkva;
#endif /* DEBUG */

/*
 * local prototypes
 */

static void uvm_pageinsert(struct uvm_object *, struct vm_page *);
static void uvm_pageremove(struct uvm_object *, struct vm_page *);

/*
 * per-object tree of pages
 */

static signed int
uvm_page_compare_nodes(void *ctx, const void *n1, const void *n2)
{
	const struct vm_page *pg1 = n1;
	const struct vm_page *pg2 = n2;
	const voff_t a = pg1->offset;
	const voff_t b = pg2->offset;

	if (a < b)
		return -1;
	if (a > b)
		return 1;
	return 0;
}

static signed int
uvm_page_compare_key(void *ctx, const void *n, const void *key)
{
	const struct vm_page *pg = n;
	const voff_t a = pg->offset;
	const voff_t b = *(const voff_t *)key;

	if (a < b)
		return -1;
	if (a > b)
		return 1;
	return 0;
}

const rb_tree_ops_t uvm_page_tree_ops = {
	.rbto_compare_nodes = uvm_page_compare_nodes,
	.rbto_compare_key = uvm_page_compare_key,
	.rbto_node_offset = offsetof(struct vm_page, rb_node),
	.rbto_context = NULL
};

/*
 * inline functions
 */

/*
 * uvm_pageinsert: insert a page in the object.
 *
 * => caller must lock object
 * => caller must lock page queues
 * => call should have already set pg's object and offset pointers
 *    and bumped the version counter
 */

static inline void
uvm_pageinsert_list(struct uvm_object *uobj, struct vm_page *pg,
    struct vm_page *where)
{

	KASSERT(uobj == pg->uobject);
	KASSERT(mutex_owned(uobj->vmobjlock));
	KASSERT((pg->flags & PG_TABLED) == 0);
	KASSERT(where == NULL || (where->flags & PG_TABLED));
	KASSERT(where == NULL || (where->uobject == uobj));

	if (UVM_OBJ_IS_VNODE(uobj)) {
		if (uobj->uo_npages == 0) {
			struct vnode *vp = (struct vnode *)uobj;

			vholdl(vp);
		}
		if (UVM_OBJ_IS_VTEXT(uobj)) {
			atomic_inc_uint(&uvmexp.execpages);
		} else {
			atomic_inc_uint(&uvmexp.filepages);
		}
	} else if (UVM_OBJ_IS_AOBJ(uobj)) {
		atomic_inc_uint(&uvmexp.anonpages);
	}

	if (where)
		TAILQ_INSERT_AFTER(&uobj->memq, where, pg, listq.queue);
	else
		TAILQ_INSERT_TAIL(&uobj->memq, pg, listq.queue);
	pg->flags |= PG_TABLED;
	uobj->uo_npages++;
}


static inline void
uvm_pageinsert_tree(struct uvm_object *uobj, struct vm_page *pg)
{
	struct vm_page *ret;

	KASSERT(uobj == pg->uobject);
	ret = rb_tree_insert_node(&uobj->rb_tree, pg);
	KASSERT(ret == pg);
}

static inline void
uvm_pageinsert(struct uvm_object *uobj, struct vm_page *pg)
{

	KDASSERT(uobj != NULL);
	uvm_pageinsert_tree(uobj, pg);
	uvm_pageinsert_list(uobj, pg, NULL);
}

/*
 * uvm_page_remove: remove page from object.
 *
 * => caller must lock object
 * => caller must lock page queues
 */

static inline void
uvm_pageremove_list(struct uvm_object *uobj, struct vm_page *pg)
{

	KASSERT(uobj == pg->uobject);
	KASSERT(mutex_owned(uobj->vmobjlock));
	KASSERT(pg->flags & PG_TABLED);

	if (UVM_OBJ_IS_VNODE(uobj)) {
		if (uobj->uo_npages == 1) {
			struct vnode *vp = (struct vnode *)uobj;

			holdrelel(vp);
		}
		if (UVM_OBJ_IS_VTEXT(uobj)) {
			atomic_dec_uint(&uvmexp.execpages);
		} else {
			atomic_dec_uint(&uvmexp.filepages);
		}
	} else if (UVM_OBJ_IS_AOBJ(uobj)) {
		atomic_dec_uint(&uvmexp.anonpages);
	}

	/* object should be locked */
	uobj->uo_npages--;
	TAILQ_REMOVE(&uobj->memq, pg, listq.queue);
	pg->flags &= ~PG_TABLED;
	pg->uobject = NULL;
}

static inline void
uvm_pageremove_tree(struct uvm_object *uobj, struct vm_page *pg)
{

	KASSERT(uobj == pg->uobject);
	rb_tree_remove_node(&uobj->rb_tree, pg);
}

static inline void
uvm_pageremove(struct uvm_object *uobj, struct vm_page *pg)
{

	KDASSERT(uobj != NULL);
	uvm_pageremove_tree(uobj, pg);
	uvm_pageremove_list(uobj, pg);
}

static void
uvm_page_init_buckets(struct pgfreelist *pgfl)
{
	int color, i;

	for (color = 0; color < uvmexp.ncolors; color++) {
		for (i = 0; i < PGFL_NQUEUES; i++) {
			LIST_INIT(&pgfl->pgfl_buckets[color].pgfl_queues[i]);
		}
	}
}

/*
 * uvm_page_init: init the page system.   called from uvm_init().
 *
 * => we return the range of kernel virtual memory in kvm_startp/kvm_endp
 */

void
uvm_page_init(vaddr_t *kvm_startp, vaddr_t *kvm_endp)
{
	static struct uvm_cpu boot_cpu;
	psize_t freepages, pagecount, bucketcount, n;
	struct pgflbucket *bucketarray, *cpuarray;
	struct vm_physseg *seg;
	struct vm_page *pagearray;
	int lcv;
	u_int i;
	paddr_t paddr;

	KASSERT(ncpu <= 1);
	CTASSERT(sizeof(pagearray->offset) >= sizeof(struct uvm_cpu *));

	/*
	 * init the page queues and page queue locks, except the free
	 * list; we allocate that later (with the initial vm_page
	 * structures).
	 */

	uvm.cpus[0] = &boot_cpu;
	curcpu()->ci_data.cpu_uvm = &boot_cpu;
	uvmpdpol_init();
	mutex_init(&uvm_pageqlock, MUTEX_DRIVER, IPL_NONE);
	mutex_init(&uvm_fpageqlock, MUTEX_DRIVER, IPL_VM);

	/*
	 * allocate vm_page structures.
	 */

	/*
	 * sanity check:
	 * before calling this function the MD code is expected to register
	 * some free RAM with the uvm_page_physload() function.   our job
	 * now is to allocate vm_page structures for this memory.
	 */

	if (vm_nphysmem == 0)
		panic("uvm_page_bootstrap: no memory pre-allocated");

	/*
	 * first calculate the number of free pages...
	 *
	 * note that we use start/end rather than avail_start/avail_end.
	 * this allows us to allocate extra vm_page structures in case we
	 * want to return some memory to the pool after booting.
	 */

	freepages = 0;
	for (lcv = 0 ; lcv < vm_nphysmem ; lcv++) {
		seg = VM_PHYSMEM_PTR(lcv);
		freepages += (seg->end - seg->start);
	}

	/*
	 * Let MD code initialize the number of colors, or default
	 * to 1 color if MD code doesn't care.
	 */
	if (uvmexp.ncolors == 0)
		uvmexp.ncolors = 1;
	uvmexp.colormask = uvmexp.ncolors - 1;
	KASSERT((uvmexp.colormask & uvmexp.ncolors) == 0);

	/*
	 * we now know we have (PAGE_SIZE * freepages) bytes of memory we can
	 * use.   for each page of memory we use we need a vm_page structure.
	 * thus, the total number of pages we can use is the total size of
	 * the memory divided by the PAGE_SIZE plus the size of the vm_page
	 * structure.   we add one to freepages as a fudge factor to avoid
	 * truncation errors (since we can only allocate in terms of whole
	 * pages).
	 */

	bucketcount = uvmexp.ncolors * VM_NFREELIST;
	pagecount = ((freepages + 1) << PAGE_SHIFT) /
	    (PAGE_SIZE + sizeof(struct vm_page));

	bucketarray = (void *)uvm_pageboot_alloc((bucketcount *
	    sizeof(struct pgflbucket) * 2) + (pagecount *
	    sizeof(struct vm_page)));
	cpuarray = bucketarray + bucketcount;
	pagearray = (struct vm_page *)(bucketarray + bucketcount * 2);

	for (lcv = 0; lcv < VM_NFREELIST; lcv++) {
		uvm.page_free[lcv].pgfl_buckets =
		    (bucketarray + (lcv * uvmexp.ncolors));
		uvm_page_init_buckets(&uvm.page_free[lcv]);
		uvm.cpus[0]->page_free[lcv].pgfl_buckets =
		    (cpuarray + (lcv * uvmexp.ncolors));
		uvm_page_init_buckets(&uvm.cpus[0]->page_free[lcv]);
	}
	memset(pagearray, 0, pagecount * sizeof(struct vm_page));

	/*
	 * init the vm_page structures and put them in the correct place.
	 */

	for (lcv = 0 ; lcv < vm_nphysmem ; lcv++) {
		seg = VM_PHYSMEM_PTR(lcv);
		n = seg->end - seg->start;

		/* set up page array pointers */
		seg->pgs = pagearray;
		pagearray += n;
		pagecount -= n;
		seg->lastpg = seg->pgs + n;

		/* init and free vm_pages (we've already zeroed them) */
		paddr = ctob(seg->start);
		for (i = 0 ; i < n ; i++, paddr += PAGE_SIZE) {
			seg->pgs[i].phys_addr = paddr;
#ifdef __HAVE_VM_PAGE_MD
			VM_MDPAGE_INIT(&seg->pgs[i]);
#endif
			if (atop(paddr) >= seg->avail_start &&
			    atop(paddr) < seg->avail_end) {
				uvmexp.npages++;
				/* add page to free pool */
				uvm_pagefree(&seg->pgs[i]);
			}
		}
	}

	/*
	 * pass up the values of virtual_space_start and
	 * virtual_space_end (obtained by uvm_pageboot_alloc) to the upper
	 * layers of the VM.
	 */

	*kvm_startp = round_page(virtual_space_start);
	*kvm_endp = trunc_page(virtual_space_end);
#ifdef DEBUG
	/*
	 * steal kva for uvm_pagezerocheck().
	 */
	uvm_zerocheckkva = *kvm_startp;
	*kvm_startp += PAGE_SIZE;
#endif /* DEBUG */

	/*
	 * init various thresholds.
	 */

	uvmexp.reserve_pagedaemon = 1;
	uvmexp.reserve_kernel = vm_page_reserve_kernel;

	/*
	 * determine if we should zero pages in the idle loop.
	 */

	uvm.cpus[0]->page_idle_zero = vm_page_zero_enable;

	/*
	 * done!
	 */

	uvm.page_init_done = true;
}

/*
 * uvm_setpagesize: set the page size
 *
 * => sets page_shift and page_mask from uvmexp.pagesize.
 */

void
uvm_setpagesize(void)
{

	/*
	 * If uvmexp.pagesize is 0 at this point, we expect PAGE_SIZE
	 * to be a constant (indicated by being a non-zero value).
	 */
	if (uvmexp.pagesize == 0) {
		if (PAGE_SIZE == 0)
			panic("uvm_setpagesize: uvmexp.pagesize not set");
		uvmexp.pagesize = PAGE_SIZE;
	}
	uvmexp.pagemask = uvmexp.pagesize - 1;
	if ((uvmexp.pagemask & uvmexp.pagesize) != 0)
		panic("uvm_setpagesize: page size %u (%#x) not a power of two",
		    uvmexp.pagesize, uvmexp.pagesize);
	for (uvmexp.pageshift = 0; ; uvmexp.pageshift++)
		if ((1 << uvmexp.pageshift) == uvmexp.pagesize)
			break;
}

/*
 * uvm_pageboot_alloc: steal memory from physmem for bootstrapping
 */

vaddr_t
uvm_pageboot_alloc(vsize_t size)
{
	static bool initialized = false;
	vaddr_t addr;
#if !defined(PMAP_STEAL_MEMORY)
	vaddr_t vaddr;
	paddr_t paddr;
#endif

	/*
	 * on first call to this function, initialize ourselves.
	 */
	if (initialized == false) {
		pmap_virtual_space(&virtual_space_start, &virtual_space_end);

		/* round it the way we like it */
		virtual_space_start = round_page(virtual_space_start);
		virtual_space_end = trunc_page(virtual_space_end);

		initialized = true;
	}

	/* round to page size */
	size = round_page(size);

#if defined(PMAP_STEAL_MEMORY)

	/*
	 * defer bootstrap allocation to MD code (it may want to allocate
	 * from a direct-mapped segment).  pmap_steal_memory should adjust
	 * virtual_space_start/virtual_space_end if necessary.
	 */

	addr = pmap_steal_memory(size, &virtual_space_start,
	    &virtual_space_end);

	return(addr);

#else /* !PMAP_STEAL_MEMORY */

	/*
	 * allocate virtual memory for this request
	 */
	if (virtual_space_start == virtual_space_end ||
	    (virtual_space_end - virtual_space_start) < size)
		panic("uvm_pageboot_alloc: out of virtual space");

	addr = virtual_space_start;

#ifdef PMAP_GROWKERNEL
	/*
	 * If the kernel pmap can't map the requested space,
	 * then allocate more resources for it.
	 */
	if (uvm_maxkaddr < (addr + size)) {
		uvm_maxkaddr = pmap_growkernel(addr + size);
		if (uvm_maxkaddr < (addr + size))
			panic("uvm_pageboot_alloc: pmap_growkernel() failed");
	}
#endif

	virtual_space_start += size;

	/*
	 * allocate and mapin physical pages to back new virtual pages
	 */

	for (vaddr = round_page(addr) ; vaddr < addr + size ;
	    vaddr += PAGE_SIZE) {

		if (!uvm_page_physget(&paddr))
			panic("uvm_pageboot_alloc: out of memory");

		/*
		 * Note this memory is no longer managed, so using
		 * pmap_kenter is safe.
		 */
		pmap_kenter_pa(vaddr, paddr, VM_PROT_READ|VM_PROT_WRITE, 0);
	}
	pmap_update(pmap_kernel());
	return(addr);
#endif	/* PMAP_STEAL_MEMORY */
}

#if !defined(PMAP_STEAL_MEMORY)
/*
 * uvm_page_physget: "steal" one page from the vm_physmem structure.
 *
 * => attempt to allocate it off the end of a segment in which the "avail"
 *    values match the start/end values.   if we can't do that, then we
 *    will advance both values (making them equal, and removing some
 *    vm_page structures from the non-avail area).
 * => return false if out of memory.
 */

/* subroutine: try to allocate from memory chunks on the specified freelist */
static bool uvm_page_physget_freelist(paddr_t *, int);

static bool
uvm_page_physget_freelist(paddr_t *paddrp, int freelist)
{
	struct vm_physseg *seg;
	int lcv, x;

	/* pass 1: try allocating from a matching end */
#if (VM_PHYSSEG_STRAT == VM_PSTRAT_BIGFIRST)
	for (lcv = vm_nphysmem - 1 ; lcv >= 0 ; lcv--)
#else
	for (lcv = 0 ; lcv < vm_nphysmem ; lcv++)
#endif
	{
		seg = VM_PHYSMEM_PTR(lcv);

		if (uvm.page_init_done == true)
			panic("uvm_page_physget: called _after_ bootstrap");

		if (seg->free_list != freelist)
			continue;

		/* try from front */
		if (seg->avail_start == seg->start &&
		    seg->avail_start < seg->avail_end) {
			*paddrp = ctob(seg->avail_start);
			seg->avail_start++;
			seg->start++;
			/* nothing left?   nuke it */
			if (seg->avail_start == seg->end) {
				if (vm_nphysmem == 1)
				    panic("uvm_page_physget: out of memory!");
				vm_nphysmem--;
				for (x = lcv ; x < vm_nphysmem ; x++)
					/* structure copy */
					VM_PHYSMEM_PTR_SWAP(x, x + 1);
			}
			return (true);
		}

		/* try from rear */
		if (seg->avail_end == seg->end &&
		    seg->avail_start < seg->avail_end) {
			*paddrp = ctob(seg->avail_end - 1);
			seg->avail_end--;
			seg->end--;
			/* nothing left?   nuke it */
			if (seg->avail_end == seg->start) {
				if (vm_nphysmem == 1)
				    panic("uvm_page_physget: out of memory!");
				vm_nphysmem--;
				for (x = lcv ; x < vm_nphysmem ; x++)
					/* structure copy */
					VM_PHYSMEM_PTR_SWAP(x, x + 1);
			}
			return (true);
		}
	}

	/* pass2: forget about matching ends, just allocate something */
#if (VM_PHYSSEG_STRAT == VM_PSTRAT_BIGFIRST)
	for (lcv = vm_nphysmem - 1 ; lcv >= 0 ; lcv--)
#else
	for (lcv = 0 ; lcv < vm_nphysmem ; lcv++)
#endif
	{
		seg = VM_PHYSMEM_PTR(lcv);

		/* any room in this bank? */
		if (seg->avail_start >= seg->avail_end)
			continue;  /* nope */

		*paddrp = ctob(seg->avail_start);
		seg->avail_start++;
		/* truncate! */
		seg->start = seg->avail_start;

		/* nothing left?   nuke it */
		if (seg->avail_start == seg->end) {
			if (vm_nphysmem == 1)
				panic("uvm_page_physget: out of memory!");
			vm_nphysmem--;
			for (x = lcv ; x < vm_nphysmem ; x++)
				/* structure copy */
				VM_PHYSMEM_PTR_SWAP(x, x + 1);
		}
		return (true);
	}

	return (false);        /* whoops! */
}

bool
uvm_page_physget(paddr_t *paddrp)
{
	int i;

	/* try in the order of freelist preference */
	for (i = 0; i < VM_NFREELIST; i++)
		if (uvm_page_physget_freelist(paddrp, i) == true)
			return (true);
	return (false);
}
#endif /* PMAP_STEAL_MEMORY */

/*
 * uvm_page_physload: load physical memory into VM system
 *
 * => all args are PFs
 * => all pages in start/end get vm_page structures
 * => areas marked by avail_start/avail_end get added to the free page pool
 * => we are limited to VM_PHYSSEG_MAX physical memory segments
 */

void
uvm_page_physload(paddr_t start, paddr_t end, paddr_t avail_start,
    paddr_t avail_end, int free_list)
{
	int preload, lcv;
	psize_t npages;
	struct vm_page *pgs;
	struct vm_physseg *ps;

	if (uvmexp.pagesize == 0)
		panic("uvm_page_physload: page size not set!");
	if (free_list >= VM_NFREELIST || free_list < VM_FREELIST_DEFAULT)
		panic("uvm_page_physload: bad free list %d", free_list);
	if (start >= end)
		panic("uvm_page_physload: start >= end");

	/*
	 * do we have room?
	 */

	if (vm_nphysmem == VM_PHYSSEG_MAX) {
		printf("uvm_page_physload: unable to load physical memory "
		    "segment\n");
		printf("\t%d segments allocated, ignoring 0x%llx -> 0x%llx\n",
		    VM_PHYSSEG_MAX, (long long)start, (long long)end);
		printf("\tincrease VM_PHYSSEG_MAX\n");
		return;
	}

	/*
	 * check to see if this is a "preload" (i.e. uvm_page_init hasn't been
	 * called yet, so kmem is not available).
	 */

	for (lcv = 0 ; lcv < vm_nphysmem ; lcv++) {
		if (VM_PHYSMEM_PTR(lcv)->pgs)
			break;
	}
	preload = (lcv == vm_nphysmem);

	/*
	 * if VM is already running, attempt to kmem_alloc vm_page structures
	 */

	if (!preload) {
		panic("uvm_page_physload: tried to add RAM after vm_mem_init");
	} else {
		pgs = NULL;
		npages = 0;
	}

	/*
	 * now insert us in the proper place in vm_physmem[]
	 */

#if (VM_PHYSSEG_STRAT == VM_PSTRAT_RANDOM)
	/* random: put it at the end (easy!) */
	ps = VM_PHYSMEM_PTR(vm_nphysmem);
#elif (VM_PHYSSEG_STRAT == VM_PSTRAT_BSEARCH)
	{
		int x;
		/* sort by address for binary search */
		for (lcv = 0 ; lcv < vm_nphysmem ; lcv++)
			if (start < VM_PHYSMEM_PTR(lcv)->start)
				break;
		ps = VM_PHYSMEM_PTR(lcv);
		/* move back other entries, if necessary ... */
		for (x = vm_nphysmem ; x > lcv ; x--)
			/* structure copy */
			VM_PHYSMEM_PTR_SWAP(x, x - 1);
	}
#elif (VM_PHYSSEG_STRAT == VM_PSTRAT_BIGFIRST)
	{
		int x;
		/* sort by largest segment first */
		for (lcv = 0 ; lcv < vm_nphysmem ; lcv++)
			if ((end - start) >
			    (VM_PHYSMEM_PTR(lcv)->end - VM_PHYSMEM_PTR(lcv)->start))
				break;
		ps = VM_PHYSMEM_PTR(lcv);
		/* move back other entries, if necessary ... */
		for (x = vm_nphysmem ; x > lcv ; x--)
			/* structure copy */
			VM_PHYSMEM_PTR_SWAP(x, x - 1);
	}
#else
	panic("uvm_page_physload: unknown physseg strategy selected!");
#endif

	ps->start = start;
	ps->end = end;
	ps->avail_start = avail_start;
	ps->avail_end = avail_end;
	if (preload) {
		ps->pgs = NULL;
	} else {
		ps->pgs = pgs;
		ps->lastpg = pgs + npages;
	}
	ps->free_list = free_list;
	vm_nphysmem++;

	if (!preload) {
		uvmpdpol_reinit();
	}
}

/*
 * when VM_PHYSSEG_MAX is 1, we can simplify these functions
 */

#if VM_PHYSSEG_MAX == 1
static inline int vm_physseg_find_contig(struct vm_physseg *, int, paddr_t, int *);
#elif (VM_PHYSSEG_STRAT == VM_PSTRAT_BSEARCH)
static inline int vm_physseg_find_bsearch(struct vm_physseg *, int, paddr_t, int *);
#else
static inline int vm_physseg_find_linear(struct vm_physseg *, int, paddr_t, int *);
#endif

/*
 * vm_physseg_find: find vm_physseg structure that belongs to a PA
 */
int
vm_physseg_find(paddr_t pframe, int *offp)
{

#if VM_PHYSSEG_MAX == 1
	return vm_physseg_find_contig(vm_physmem, vm_nphysseg, pframe, offp);
#elif (VM_PHYSSEG_STRAT == VM_PSTRAT_BSEARCH)
	return vm_physseg_find_bsearch(vm_physmem, vm_nphysseg, pframe, offp);
#else
	return vm_physseg_find_linear(vm_physmem, vm_nphysseg, pframe, offp);
#endif
}

#if VM_PHYSSEG_MAX == 1
static inline int
vm_physseg_find_contig(struct vm_physseg *segs, int nsegs, paddr_t pframe, int *offp)
{

	/* 'contig' case */
	if (pframe >= segs[0].start && pframe < segs[0].end) {
		if (offp)
			*offp = pframe - segs[0].start;
		return(0);
	}
	return(-1);
}

#elif (VM_PHYSSEG_STRAT == VM_PSTRAT_BSEARCH)

static inline int
vm_physseg_find_bsearch(struct vm_physseg *segs, int nsegs, paddr_t pframe, int *offp)
{
	/* binary search for it */
	u_int	start, len, try;

	/*
	 * if try is too large (thus target is less than try) we reduce
	 * the length to trunc(len/2) [i.e. everything smaller than "try"]
	 *
	 * if the try is too small (thus target is greater than try) then
	 * we set the new start to be (try + 1).   this means we need to
	 * reduce the length to (round(len/2) - 1).
	 *
	 * note "adjust" below which takes advantage of the fact that
	 *  (round(len/2) - 1) == trunc((len - 1) / 2)
	 * for any value of len we may have
	 */

	for (start = 0, len = nsegs ; len != 0 ; len = len / 2) {
		try = start + (len / 2);	/* try in the middle */

		/* start past our try? */
		if (pframe >= segs[try].start) {
			/* was try correct? */
			if (pframe < segs[try].end) {
				if (offp)
					*offp = pframe - segs[try].start;
				return(try);            /* got it */
			}
			start = try + 1;	/* next time, start here */
			len--;			/* "adjust" */
		} else {
			/*
			 * pframe before try, just reduce length of
			 * region, done in "for" loop
			 */
		}
	}
	return(-1);
}

#else

static inline int
vm_physseg_find_linear(struct vm_physseg *segs, int nsegs, paddr_t pframe, int *offp)
{
	/* linear search for it */
	int	lcv;

	for (lcv = 0; lcv < nsegs; lcv++) {
		if (pframe >= segs[lcv].start &&
		    pframe < segs[lcv].end) {
			if (offp)
				*offp = pframe - segs[lcv].start;
			return(lcv);		   /* got it */
		}
	}
	return(-1);
}
#endif

/*
 * PHYS_TO_VM_PAGE: find vm_page for a PA.   used by MI code to get vm_pages
 * back from an I/O mapping (ugh!).   used in some MD code as well.
 */
struct vm_page *
uvm_phys_to_vm_page(paddr_t pa)
{
	paddr_t pf = atop(pa);
	int	off;
	int	psi;

	psi = vm_physseg_find(pf, &off);
	if (psi != -1)
		return(&VM_PHYSMEM_PTR(psi)->pgs[off]);
	return(NULL);
}

paddr_t
uvm_vm_page_to_phys(const struct vm_page *pg)
{

	return pg->phys_addr;
}

/*
 * uvm_page_recolor: Recolor the pages if the new bucket count is
 * larger than the old one.
 */

void
uvm_page_recolor(int newncolors)
{
	struct pgflbucket *bucketarray, *cpuarray, *oldbucketarray;
	struct pgfreelist gpgfl, pgfl;
	struct vm_page *pg;
	vsize_t bucketcount;
	size_t bucketmemsize, oldbucketmemsize;
	int lcv, color, i, ocolors;
	struct uvm_cpu *ucpu;

	KASSERT(((newncolors - 1) & newncolors) == 0);

	if (newncolors <= uvmexp.ncolors)
		return;

	if (uvm.page_init_done == false) {
		uvmexp.ncolors = newncolors;
		return;
	}

	bucketcount = newncolors * VM_NFREELIST;
	bucketmemsize = bucketcount * sizeof(struct pgflbucket) * 2;
	bucketarray = kmem_alloc(bucketmemsize, KM_SLEEP);
	cpuarray = bucketarray + bucketcount;
	if (bucketarray == NULL) {
		printf("WARNING: unable to allocate %ld page color buckets\n",
		    (long) bucketcount);
		return;
	}

	mutex_spin_enter(&uvm_fpageqlock);

	/* Make sure we should still do this. */
	if (newncolors <= uvmexp.ncolors) {
		mutex_spin_exit(&uvm_fpageqlock);
		kmem_free(bucketarray, bucketmemsize);
		return;
	}

	oldbucketarray = uvm.page_free[0].pgfl_buckets;
	ocolors = uvmexp.ncolors;

	uvmexp.ncolors = newncolors;
	uvmexp.colormask = uvmexp.ncolors - 1;

	ucpu = curcpu()->ci_data.cpu_uvm;
	for (lcv = 0; lcv < VM_NFREELIST; lcv++) {
		gpgfl.pgfl_buckets = (bucketarray + (lcv * newncolors));
		pgfl.pgfl_buckets = (cpuarray + (lcv * uvmexp.ncolors));
		uvm_page_init_buckets(&gpgfl);
		uvm_page_init_buckets(&pgfl);
		for (color = 0; color < ocolors; color++) {
			for (i = 0; i < PGFL_NQUEUES; i++) {
				while ((pg = LIST_FIRST(&uvm.page_free[
				    lcv].pgfl_buckets[color].pgfl_queues[i]))
				    != NULL) {
					LIST_REMOVE(pg, pageq.list); /* global */
					LIST_REMOVE(pg, listq.list); /* cpu */
					LIST_INSERT_HEAD(&gpgfl.pgfl_buckets[
					    VM_PGCOLOR_BUCKET(pg)].pgfl_queues[
					    i], pg, pageq.list);
					LIST_INSERT_HEAD(&pgfl.pgfl_buckets[
					    VM_PGCOLOR_BUCKET(pg)].pgfl_queues[
					    i], pg, listq.list);
				}
			}
		}
		uvm.page_free[lcv].pgfl_buckets = gpgfl.pgfl_buckets;
		ucpu->page_free[lcv].pgfl_buckets = pgfl.pgfl_buckets;
	}

	oldbucketmemsize = recolored_pages_memsize;

	recolored_pages_memsize = bucketmemsize;
	mutex_spin_exit(&uvm_fpageqlock);

	if (oldbucketmemsize) {
		kmem_free(oldbucketarray, recolored_pages_memsize);
	}

	/*
	 * this calls uvm_km_alloc() which may want to hold
	 * uvm_fpageqlock.
	 */
	uvm_pager_realloc_emerg();
}

/*
 * uvm_cpu_attach: initialize per-CPU data structures.
 */

void
uvm_cpu_attach(struct cpu_info *ci)
{
	struct pgflbucket *bucketarray;
	struct pgfreelist pgfl;
	struct uvm_cpu *ucpu;
	vsize_t bucketcount;
	int lcv;

	if (CPU_IS_PRIMARY(ci)) {
		/* Already done in uvm_page_init(). */
		goto attachrnd;
	}

	/* Add more reserve pages for this CPU. */
	uvmexp.reserve_kernel += vm_page_reserve_kernel;

	/* Configure this CPU's free lists. */
	bucketcount = uvmexp.ncolors * VM_NFREELIST;
	bucketarray = kmem_alloc(bucketcount * sizeof(struct pgflbucket),
	    KM_SLEEP);
	ucpu = kmem_zalloc(sizeof(*ucpu), KM_SLEEP);
	uvm.cpus[cpu_index(ci)] = ucpu;
	ci->ci_data.cpu_uvm = ucpu;
	for (lcv = 0; lcv < VM_NFREELIST; lcv++) {
		pgfl.pgfl_buckets = (bucketarray + (lcv * uvmexp.ncolors));
		uvm_page_init_buckets(&pgfl);
		ucpu->page_free[lcv].pgfl_buckets = pgfl.pgfl_buckets;
	}

attachrnd:
	/*
	 * Attach RNG source for this CPU's VM events
	 */
        rnd_attach_source(&uvm.cpus[cpu_index(ci)]->rs,
			  ci->ci_data.cpu_name, RND_TYPE_VM, 0);

}

/*
 * uvm_pagealloc_pgfl: helper routine for uvm_pagealloc_strat
 */

static struct vm_page *
uvm_pagealloc_pgfl(struct uvm_cpu *ucpu, int flist, int try1, int try2,
    int *trycolorp)
{
	struct pgflist *freeq;
	struct vm_page *pg;
	int color, trycolor = *trycolorp;
	struct pgfreelist *gpgfl, *pgfl;

	KASSERT(mutex_owned(&uvm_fpageqlock));

	color = trycolor;
	pgfl = &ucpu->page_free[flist];
	gpgfl = &uvm.page_free[flist];
	do {
		/* cpu, try1 */
		if ((pg = LIST_FIRST((freeq =
		    &pgfl->pgfl_buckets[color].pgfl_queues[try1]))) != NULL) {
			KASSERT(pg->pqflags & PQ_FREE);
			KASSERT(try1 == PGFL_ZEROS || !(pg->flags & PG_ZERO));
			KASSERT(try1 == PGFL_UNKNOWN || (pg->flags & PG_ZERO));
			KASSERT(ucpu == VM_FREE_PAGE_TO_CPU(pg));
			VM_FREE_PAGE_TO_CPU(pg)->pages[try1]--;
		    	uvmexp.cpuhit++;
			goto gotit;
		}
		/* global, try1 */
		if ((pg = LIST_FIRST((freeq =
		    &gpgfl->pgfl_buckets[color].pgfl_queues[try1]))) != NULL) {
			KASSERT(pg->pqflags & PQ_FREE);
			KASSERT(try1 == PGFL_ZEROS || !(pg->flags & PG_ZERO));
			KASSERT(try1 == PGFL_UNKNOWN || (pg->flags & PG_ZERO));
			KASSERT(ucpu != VM_FREE_PAGE_TO_CPU(pg));
			VM_FREE_PAGE_TO_CPU(pg)->pages[try1]--;
		    	uvmexp.cpumiss++;
			goto gotit;
		}
		/* cpu, try2 */
		if ((pg = LIST_FIRST((freeq =
		    &pgfl->pgfl_buckets[color].pgfl_queues[try2]))) != NULL) {
			KASSERT(pg->pqflags & PQ_FREE);
			KASSERT(try2 == PGFL_ZEROS || !(pg->flags & PG_ZERO));
			KASSERT(try2 == PGFL_UNKNOWN || (pg->flags & PG_ZERO));
			KASSERT(ucpu == VM_FREE_PAGE_TO_CPU(pg));
			VM_FREE_PAGE_TO_CPU(pg)->pages[try2]--;
		    	uvmexp.cpuhit++;
			goto gotit;
		}
		/* global, try2 */
		if ((pg = LIST_FIRST((freeq =
		    &gpgfl->pgfl_buckets[color].pgfl_queues[try2]))) != NULL) {
			KASSERT(pg->pqflags & PQ_FREE);
			KASSERT(try2 == PGFL_ZEROS || !(pg->flags & PG_ZERO));
			KASSERT(try2 == PGFL_UNKNOWN || (pg->flags & PG_ZERO));
			KASSERT(ucpu != VM_FREE_PAGE_TO_CPU(pg));
			VM_FREE_PAGE_TO_CPU(pg)->pages[try2]--;
		    	uvmexp.cpumiss++;
			goto gotit;
		}
		color = (color + 1) & uvmexp.colormask;
	} while (color != trycolor);

	return (NULL);

 gotit:
	LIST_REMOVE(pg, pageq.list);	/* global list */
	LIST_REMOVE(pg, listq.list);	/* per-cpu list */
	uvmexp.free--;

	/* update zero'd page count */
	if (pg->flags & PG_ZERO)
		uvmexp.zeropages--;

	if (color == trycolor)
		uvmexp.colorhit++;
	else {
		uvmexp.colormiss++;
		*trycolorp = color;
	}

	return (pg);
}

/*
 * uvm_pagealloc_strat: allocate vm_page from a particular free list.
 *
 * => return null if no pages free
 * => wake up pagedaemon if number of free pages drops below low water mark
 * => if obj != NULL, obj must be locked (to put in obj's tree)
 * => if anon != NULL, anon must be locked (to put in anon)
 * => only one of obj or anon can be non-null
 * => caller must activate/deactivate page if it is not wired.
 * => free_list is ignored if strat == UVM_PGA_STRAT_NORMAL.
 * => policy decision: it is more important to pull a page off of the
 *	appropriate priority free list than it is to get a zero'd or
 *	unknown contents page.  This is because we live with the
 *	consequences of a bad free list decision for the entire
 *	lifetime of the page, e.g. if the page comes from memory that
 *	is slower to access.
 */

struct vm_page *
uvm_pagealloc_strat(struct uvm_object *obj, voff_t off, struct vm_anon *anon,
    int flags, int strat, int free_list)
{
	int lcv, try1, try2, zeroit = 0, color;
	struct uvm_cpu *ucpu;
	struct vm_page *pg;
	lwp_t *l;

	KASSERT(obj == NULL || anon == NULL);
	KASSERT(anon == NULL || (flags & UVM_FLAG_COLORMATCH) || off == 0);
	KASSERT(off == trunc_page(off));
	KASSERT(obj == NULL || mutex_owned(obj->vmobjlock));
	KASSERT(anon == NULL || anon->an_lock == NULL ||
	    mutex_owned(anon->an_lock));

	mutex_spin_enter(&uvm_fpageqlock);

	/*
	 * This implements a global round-robin page coloring
	 * algorithm.
	 */

	ucpu = curcpu()->ci_data.cpu_uvm;
	if (flags & UVM_FLAG_COLORMATCH) {
		color = atop(off) & uvmexp.colormask;
	} else {
		color = ucpu->page_free_nextcolor;
	}

	/*
	 * check to see if we need to generate some free pages waking
	 * the pagedaemon.
	 */

	uvm_kick_pdaemon();

	/*
	 * fail if any of these conditions is true:
	 * [1]  there really are no free pages, or
	 * [2]  only kernel "reserved" pages remain and
	 *        reserved pages have not been requested.
	 * [3]  only pagedaemon "reserved" pages remain and
	 *        the requestor isn't the pagedaemon.
	 * we make kernel reserve pages available if called by a
	 * kernel thread or a realtime thread.
	 */
	l = curlwp;
	if (__predict_true(l != NULL) && lwp_eprio(l) >= PRI_KTHREAD) {
		flags |= UVM_PGA_USERESERVE;
	}
	if ((uvmexp.free <= uvmexp.reserve_kernel &&
	    (flags & UVM_PGA_USERESERVE) == 0) ||
	    (uvmexp.free <= uvmexp.reserve_pagedaemon &&
	     curlwp != uvm.pagedaemon_lwp))
		goto fail;

#if PGFL_NQUEUES != 2
#error uvm_pagealloc_strat needs to be updated
#endif

	/*
	 * If we want a zero'd page, try the ZEROS queue first, otherwise
	 * we try the UNKNOWN queue first.
	 */
	if (flags & UVM_PGA_ZERO) {
		try1 = PGFL_ZEROS;
		try2 = PGFL_UNKNOWN;
	} else {
		try1 = PGFL_UNKNOWN;
		try2 = PGFL_ZEROS;
	}

 again:
	switch (strat) {
	case UVM_PGA_STRAT_NORMAL:
		/* Check freelists: descending priority (ascending id) order */
		for (lcv = 0; lcv < VM_NFREELIST; lcv++) {
			pg = uvm_pagealloc_pgfl(ucpu, lcv,
			    try1, try2, &color);
			if (pg != NULL)
				goto gotit;
		}

		/* No pages free! */
		goto fail;

	case UVM_PGA_STRAT_ONLY:
	case UVM_PGA_STRAT_FALLBACK:
		/* Attempt to allocate from the specified free list. */
		KASSERT(free_list >= 0 && free_list < VM_NFREELIST);
		pg = uvm_pagealloc_pgfl(ucpu, free_list,
		    try1, try2, &color);
		if (pg != NULL)
			goto gotit;

		/* Fall back, if possible. */
		if (strat == UVM_PGA_STRAT_FALLBACK) {
			strat = UVM_PGA_STRAT_NORMAL;
			goto again;
		}

		/* No pages free! */
		goto fail;

	default:
		panic("uvm_pagealloc_strat: bad strat %d", strat);
		/* NOTREACHED */
	}

 gotit:
	/*
	 * We now know which color we actually allocated from; set
	 * the next color accordingly.
	 */

	ucpu->page_free_nextcolor = (color + 1) & uvmexp.colormask;

	/*
	 * update allocation statistics and remember if we have to
	 * zero the page
	 */

	if (flags & UVM_PGA_ZERO) {
		if (pg->flags & PG_ZERO) {
			uvmexp.pga_zerohit++;
			zeroit = 0;
		} else {
			uvmexp.pga_zeromiss++;
			zeroit = 1;
		}
		if (ucpu->pages[PGFL_ZEROS] < ucpu->pages[PGFL_UNKNOWN]) {
			ucpu->page_idle_zero = vm_page_zero_enable;
		}
	}
	KASSERT(pg->pqflags == PQ_FREE);

	pg->offset = off;
	pg->uobject = obj;
	pg->uanon = anon;
	pg->flags = PG_BUSY|PG_CLEAN|PG_FAKE;
	if (anon) {
		anon->an_page = pg;
		pg->pqflags = PQ_ANON;
		atomic_inc_uint(&uvmexp.anonpages);
	} else {
		if (obj) {
			uvm_pageinsert(obj, pg);
		}
		pg->pqflags = 0;
	}
	mutex_spin_exit(&uvm_fpageqlock);

#if defined(UVM_PAGE_TRKOWN)
	pg->owner_tag = NULL;
#endif
	UVM_PAGE_OWN(pg, "new alloc");

	if (flags & UVM_PGA_ZERO) {
		/*
		 * A zero'd page is not clean.  If we got a page not already
		 * zero'd, then we have to zero it ourselves.
		 */
		pg->flags &= ~PG_CLEAN;
		if (zeroit)
			pmap_zero_page(VM_PAGE_TO_PHYS(pg));
	}

	return(pg);

 fail:
	mutex_spin_exit(&uvm_fpageqlock);
	return (NULL);
}

/*
 * uvm_pagereplace: replace a page with another
 *
 * => object must be locked
 */

void
uvm_pagereplace(struct vm_page *oldpg, struct vm_page *newpg)
{
	struct uvm_object *uobj = oldpg->uobject;

	KASSERT((oldpg->flags & PG_TABLED) != 0);
	KASSERT(uobj != NULL);
	KASSERT((newpg->flags & PG_TABLED) == 0);
	KASSERT(newpg->uobject == NULL);
	KASSERT(mutex_owned(uobj->vmobjlock));

	newpg->uobject = uobj;
	newpg->offset = oldpg->offset;

	uvm_pageremove_tree(uobj, oldpg);
	uvm_pageinsert_tree(uobj, newpg);
	uvm_pageinsert_list(uobj, newpg, oldpg);
	uvm_pageremove_list(uobj, oldpg);
}

/*
 * uvm_pagerealloc: reallocate a page from one object to another
 *
 * => both objects must be locked
 */

void
uvm_pagerealloc(struct vm_page *pg, struct uvm_object *newobj, voff_t newoff)
{
	/*
	 * remove it from the old object
	 */

	if (pg->uobject) {
		uvm_pageremove(pg->uobject, pg);
	}

	/*
	 * put it in the new object
	 */

	if (newobj) {
		pg->uobject = newobj;
		pg->offset = newoff;
		uvm_pageinsert(newobj, pg);
	}
}

#ifdef DEBUG
/*
 * check if page is zero-filled
 *
 *  - called with free page queue lock held.
 */
void
uvm_pagezerocheck(struct vm_page *pg)
{
	int *p, *ep;

	KASSERT(uvm_zerocheckkva != 0);
	KASSERT(mutex_owned(&uvm_fpageqlock));

	/*
	 * XXX assuming pmap_kenter_pa and pmap_kremove never call
	 * uvm page allocator.
	 *
	 * it might be better to have "CPU-local temporary map" pmap interface.
	 */
	pmap_kenter_pa(uvm_zerocheckkva, VM_PAGE_TO_PHYS(pg), VM_PROT_READ, 0);
	p = (int *)uvm_zerocheckkva;
	ep = (int *)((char *)p + PAGE_SIZE);
	pmap_update(pmap_kernel());
	while (p < ep) {
		if (*p != 0)
			panic("PG_ZERO page isn't zero-filled");
		p++;
	}
	pmap_kremove(uvm_zerocheckkva, PAGE_SIZE);
	/*
	 * pmap_update() is not necessary here because no one except us
	 * uses this VA.
	 */
}
#endif /* DEBUG */

/*
 * uvm_pagefree: free page
 *
 * => erase page's identity (i.e. remove from object)
 * => put page on free list
 * => caller must lock owning object (either anon or uvm_object)
 * => caller must lock page queues
 * => assumes all valid mappings of pg are gone
 */

void
uvm_pagefree(struct vm_page *pg)
{
	struct pgflist *pgfl;
	struct uvm_cpu *ucpu;
	int index, color, queue;
	bool iszero;

#ifdef DEBUG
	if (pg->uobject == (void *)0xdeadbeef &&
	    pg->uanon == (void *)0xdeadbeef) {
		panic("uvm_pagefree: freeing free page %p", pg);
	}
#endif /* DEBUG */

	KASSERT((pg->flags & PG_PAGEOUT) == 0);
	KASSERT(!(pg->pqflags & PQ_FREE));
	//KASSERT(mutex_owned(&uvm_pageqlock) || !uvmpdpol_pageisqueued_p(pg));
	KASSERT(pg->uobject == NULL || mutex_owned(pg->uobject->vmobjlock));
	KASSERT(pg->uobject != NULL || pg->uanon == NULL ||
		mutex_owned(pg->uanon->an_lock));

	/*
	 * if the page is loaned, resolve the loan instead of freeing.
	 */

	if (pg->loan_count) {
		KASSERT(pg->wire_count == 0);

		/*
		 * if the page is owned by an anon then we just want to
		 * drop anon ownership.  the kernel will free the page when
		 * it is done with it.  if the page is owned by an object,
		 * remove it from the object and mark it dirty for the benefit
		 * of possible anon owners.
		 *
		 * regardless of previous ownership, wakeup any waiters,
		 * unbusy the page, and we're done.
		 */

		if (pg->uobject != NULL) {
			uvm_pageremove(pg->uobject, pg);
			pg->flags &= ~PG_CLEAN;
		} else if (pg->uanon != NULL) {
			if ((pg->pqflags & PQ_ANON) == 0) {
				pg->loan_count--;
			} else {
				pg->pqflags &= ~PQ_ANON;
				atomic_dec_uint(&uvmexp.anonpages);
			}
			pg->uanon->an_page = NULL;
			pg->uanon = NULL;
		}
		if (pg->flags & PG_WANTED) {
			wakeup(pg);
		}
		pg->flags &= ~(PG_WANTED|PG_BUSY|PG_RELEASED|PG_PAGER1);
#ifdef UVM_PAGE_TRKOWN
		pg->owner_tag = NULL;
#endif
		if (pg->loan_count) {
			KASSERT(pg->uobject == NULL);
			if (pg->uanon == NULL) {
				KASSERT(mutex_owned(&uvm_pageqlock));
				uvm_pagedequeue(pg);
			}
			return;
		}
	}

	/*
	 * remove page from its object or anon.
	 */

	if (pg->uobject != NULL) {
		uvm_pageremove(pg->uobject, pg);
	} else if (pg->uanon != NULL) {
		pg->uanon->an_page = NULL;
		atomic_dec_uint(&uvmexp.anonpages);
	}

	/*
	 * now remove the page from the queues.
	 */
	if (uvmpdpol_pageisqueued_p(pg)) {
		KASSERT(mutex_owned(&uvm_pageqlock));
		uvm_pagedequeue(pg);
	}

	/*
	 * if the page was wired, unwire it now.
	 */

	if (pg->wire_count) {
		pg->wire_count = 0;
		uvmexp.wired--;
	}

	/*
	 * and put on free queue
	 */

	iszero = (pg->flags & PG_ZERO);
	index = uvm_page_lookup_freelist(pg);
	color = VM_PGCOLOR_BUCKET(pg);
	queue = (iszero ? PGFL_ZEROS : PGFL_UNKNOWN);

#ifdef DEBUG
	pg->uobject = (void *)0xdeadbeef;
	pg->uanon = (void *)0xdeadbeef;
#endif

	mutex_spin_enter(&uvm_fpageqlock);
	pg->pqflags = PQ_FREE;

#ifdef DEBUG
	if (iszero)
		uvm_pagezerocheck(pg);
#endif /* DEBUG */


	/* global list */
	pgfl = &uvm.page_free[index].pgfl_buckets[color].pgfl_queues[queue];
	LIST_INSERT_HEAD(pgfl, pg, pageq.list);
	uvmexp.free++;
	if (iszero) {
		uvmexp.zeropages++;
	}

	/* per-cpu list */
	ucpu = curcpu()->ci_data.cpu_uvm;
	pg->offset = (uintptr_t)ucpu;
	pgfl = &ucpu->page_free[index].pgfl_buckets[color].pgfl_queues[queue];
	LIST_INSERT_HEAD(pgfl, pg, listq.list);
	ucpu->pages[queue]++;
	if (ucpu->pages[PGFL_ZEROS] < ucpu->pages[PGFL_UNKNOWN]) {
		ucpu->page_idle_zero = vm_page_zero_enable;
	}

	mutex_spin_exit(&uvm_fpageqlock);
}

/*
 * uvm_page_unbusy: unbusy an array of pages.
 *
 * => pages must either all belong to the same object, or all belong to anons.
 * => if pages are object-owned, object must be locked.
 * => if pages are anon-owned, anons must be locked.
 * => caller must lock page queues if pages may be released.
 * => caller must make sure that anon-owned pages are not PG_RELEASED.
 */

void
uvm_page_unbusy(struct vm_page **pgs, int npgs)
{
	struct vm_page *pg;
	int i;
	UVMHIST_FUNC("uvm_page_unbusy"); UVMHIST_CALLED(ubchist);

	for (i = 0; i < npgs; i++) {
		pg = pgs[i];
		if (pg == NULL || pg == PGO_DONTCARE) {
			continue;
		}

		KASSERT(uvm_page_locked_p(pg));
		KASSERT(pg->flags & PG_BUSY);
		KASSERT((pg->flags & PG_PAGEOUT) == 0);
		if (pg->flags & PG_WANTED) {
			wakeup(pg);
		}
		if (pg->flags & PG_RELEASED) {
			UVMHIST_LOG(ubchist, "releasing pg %p", pg,0,0,0);
			KASSERT(pg->uobject != NULL ||
			    (pg->uanon != NULL && pg->uanon->an_ref > 0));
			pg->flags &= ~PG_RELEASED;
			uvm_pagefree(pg);
		} else {
			UVMHIST_LOG(ubchist, "unbusying pg %p", pg,0,0,0);
			KASSERT((pg->flags & PG_FAKE) == 0);
			pg->flags &= ~(PG_WANTED|PG_BUSY);
			UVM_PAGE_OWN(pg, NULL);
		}
	}
}

#if defined(UVM_PAGE_TRKOWN)
/*
 * uvm_page_own: set or release page ownership
 *
 * => this is a debugging function that keeps track of who sets PG_BUSY
 *	and where they do it.   it can be used to track down problems
 *	such a process setting "PG_BUSY" and never releasing it.
 * => page's object [if any] must be locked
 * => if "tag" is NULL then we are releasing page ownership
 */
void
uvm_page_own(struct vm_page *pg, const char *tag)
{
	struct uvm_object *uobj;
	struct vm_anon *anon;

	KASSERT((pg->flags & (PG_PAGEOUT|PG_RELEASED)) == 0);

	uobj = pg->uobject;
	anon = pg->uanon;
	KASSERT(uvm_page_locked_p(pg));
	KASSERT((pg->flags & PG_WANTED) == 0);

	/* gain ownership? */
	if (tag) {
		KASSERT((pg->flags & PG_BUSY) != 0);
		if (pg->owner_tag) {
			printf("uvm_page_own: page %p already owned "
			    "by proc %d [%s]\n", pg,
			    pg->owner, pg->owner_tag);
			panic("uvm_page_own");
		}
		pg->owner = (curproc) ? curproc->p_pid :  (pid_t) -1;
		pg->lowner = (curlwp) ? curlwp->l_lid :  (lwpid_t) -1;
		pg->owner_tag = tag;
		return;
	}

	/* drop ownership */
	KASSERT((pg->flags & PG_BUSY) == 0);
	if (pg->owner_tag == NULL) {
		printf("uvm_page_own: dropping ownership of an non-owned "
		    "page (%p)\n", pg);
		panic("uvm_page_own");
	}
	if (!uvmpdpol_pageisqueued_p(pg)) {
		KASSERT((pg->uanon == NULL && pg->uobject == NULL) ||
		    pg->wire_count > 0);
	} else {
		KASSERT(pg->wire_count == 0);
	}
	pg->owner_tag = NULL;
}
#endif

/*
 * uvm_pageidlezero: zero free pages while the system is idle.
 *
 * => try to complete one color bucket at a time, to reduce our impact
 *	on the CPU cache.
 * => we loop until we either reach the target or there is a lwp ready
 *      to run, or MD code detects a reason to break early.
 */
void
uvm_pageidlezero(void)
{
	struct vm_page *pg;
	struct pgfreelist *pgfl, *gpgfl;
	struct uvm_cpu *ucpu;
	int free_list, firstbucket, nextbucket;
	bool lcont = false;

	ucpu = curcpu()->ci_data.cpu_uvm;
	if (!ucpu->page_idle_zero ||
	    ucpu->pages[PGFL_UNKNOWN] < uvmexp.ncolors) {
	    	ucpu->page_idle_zero = false;
		return;
	}
	if (!mutex_tryenter(&uvm_fpageqlock)) {
		/* Contention: let other CPUs to use the lock. */
		return;
	}
	firstbucket = ucpu->page_free_nextcolor;
	nextbucket = firstbucket;
	do {
		for (free_list = 0; free_list < VM_NFREELIST; free_list++) {
			if (sched_curcpu_runnable_p()) {
				goto quit;
			}
			pgfl = &ucpu->page_free[free_list];
			gpgfl = &uvm.page_free[free_list];
			while ((pg = LIST_FIRST(&pgfl->pgfl_buckets[
			    nextbucket].pgfl_queues[PGFL_UNKNOWN])) != NULL) {
				if (lcont || sched_curcpu_runnable_p()) {
					goto quit;
				}
				LIST_REMOVE(pg, pageq.list); /* global list */
				LIST_REMOVE(pg, listq.list); /* per-cpu list */
				ucpu->pages[PGFL_UNKNOWN]--;
				uvmexp.free--;
				KASSERT(pg->pqflags == PQ_FREE);
				pg->pqflags = 0;
				mutex_spin_exit(&uvm_fpageqlock);
#ifdef PMAP_PAGEIDLEZERO
				if (!PMAP_PAGEIDLEZERO(VM_PAGE_TO_PHYS(pg))) {

					/*
					 * The machine-dependent code detected
					 * some reason for us to abort zeroing
					 * pages, probably because there is a
					 * process now ready to run.
					 */

					mutex_spin_enter(&uvm_fpageqlock);
					pg->pqflags = PQ_FREE;
					LIST_INSERT_HEAD(&gpgfl->pgfl_buckets[
					    nextbucket].pgfl_queues[
					    PGFL_UNKNOWN], pg, pageq.list);
					LIST_INSERT_HEAD(&pgfl->pgfl_buckets[
					    nextbucket].pgfl_queues[
					    PGFL_UNKNOWN], pg, listq.list);
					ucpu->pages[PGFL_UNKNOWN]++;
					uvmexp.free++;
					uvmexp.zeroaborts++;
					goto quit;
				}
#else
				pmap_zero_page(VM_PAGE_TO_PHYS(pg));
#endif /* PMAP_PAGEIDLEZERO */
				pg->flags |= PG_ZERO;

				if (!mutex_tryenter(&uvm_fpageqlock)) {
					lcont = true;
					mutex_spin_enter(&uvm_fpageqlock);
				} else {
					lcont = false;
				}
				pg->pqflags = PQ_FREE;
				LIST_INSERT_HEAD(&gpgfl->pgfl_buckets[
				    nextbucket].pgfl_queues[PGFL_ZEROS],
				    pg, pageq.list);
				LIST_INSERT_HEAD(&pgfl->pgfl_buckets[
				    nextbucket].pgfl_queues[PGFL_ZEROS],
				    pg, listq.list);
				ucpu->pages[PGFL_ZEROS]++;
				uvmexp.free++;
				uvmexp.zeropages++;
			}
		}
		if (ucpu->pages[PGFL_UNKNOWN] < uvmexp.ncolors) {
			break;
		}
		nextbucket = (nextbucket + 1) & uvmexp.colormask;
	} while (nextbucket != firstbucket);
	ucpu->page_idle_zero = false;
 quit:
	mutex_spin_exit(&uvm_fpageqlock);
}

/*
 * uvm_pagelookup: look up a page
 *
 * => caller should lock object to keep someone from pulling the page
 *	out from under it
 */

struct vm_page *
uvm_pagelookup(struct uvm_object *obj, voff_t off)
{
	struct vm_page *pg;

	KASSERT(mutex_owned(obj->vmobjlock));

	pg = rb_tree_find_node(&obj->rb_tree, &off);

	KASSERT(pg == NULL || obj->uo_npages != 0);
	KASSERT(pg == NULL || (pg->flags & (PG_RELEASED|PG_PAGEOUT)) == 0 ||
		(pg->flags & PG_BUSY) != 0);
	return pg;
}

/*
 * uvm_pagewire: wire the page, thus removing it from the daemon's grasp
 *
 * => caller must lock page queues
 */

void
uvm_pagewire(struct vm_page *pg)
{
	KASSERT(mutex_owned(&uvm_pageqlock));
#if defined(READAHEAD_STATS)
	if ((pg->pqflags & PQ_READAHEAD) != 0) {
		uvm_ra_hit.ev_count++;
		pg->pqflags &= ~PQ_READAHEAD;
	}
#endif /* defined(READAHEAD_STATS) */
	if (pg->wire_count == 0) {
		uvm_pagedequeue(pg);
		uvmexp.wired++;
	}
	pg->wire_count++;
}

/*
 * uvm_pageunwire: unwire the page.
 *
 * => activate if wire count goes to zero.
 * => caller must lock page queues
 */

void
uvm_pageunwire(struct vm_page *pg)
{
	KASSERT(mutex_owned(&uvm_pageqlock));
	pg->wire_count--;
	if (pg->wire_count == 0) {
		uvm_pageactivate(pg);
		uvmexp.wired--;
	}
}

/*
 * uvm_pagedeactivate: deactivate page
 *
 * => caller must lock page queues
 * => caller must check to make sure page is not wired
 * => object that page belongs to must be locked (so we can adjust pg->flags)
 * => caller must clear the reference on the page before calling
 */

void
uvm_pagedeactivate(struct vm_page *pg)
{

	KASSERT(mutex_owned(&uvm_pageqlock));
	KASSERT(uvm_page_locked_p(pg));
	KASSERT(pg->wire_count != 0 || uvmpdpol_pageisqueued_p(pg));
	uvmpdpol_pagedeactivate(pg);
}

/*
 * uvm_pageactivate: activate page
 *
 * => caller must lock page queues
 */

void
uvm_pageactivate(struct vm_page *pg)
{

	KASSERT(mutex_owned(&uvm_pageqlock));
	KASSERT(uvm_page_locked_p(pg));
#if defined(READAHEAD_STATS)
	if ((pg->pqflags & PQ_READAHEAD) != 0) {
		uvm_ra_hit.ev_count++;
		pg->pqflags &= ~PQ_READAHEAD;
	}
#endif /* defined(READAHEAD_STATS) */
	if (pg->wire_count != 0) {
		return;
	}
	uvmpdpol_pageactivate(pg);
}

/*
 * uvm_pagedequeue: remove a page from any paging queue
 */

void
uvm_pagedequeue(struct vm_page *pg)
{

	if (uvmpdpol_pageisqueued_p(pg)) {
		KASSERT(mutex_owned(&uvm_pageqlock));
	}

	uvmpdpol_pagedequeue(pg);
}

/*
 * uvm_pageenqueue: add a page to a paging queue without activating.
 * used where a page is not really demanded (yet).  eg. read-ahead
 */

void
uvm_pageenqueue(struct vm_page *pg)
{

	KASSERT(mutex_owned(&uvm_pageqlock));
	if (pg->wire_count != 0) {
		return;
	}
	uvmpdpol_pageenqueue(pg);
}

/*
 * uvm_pagezero: zero fill a page
 *
 * => if page is part of an object then the object should be locked
 *	to protect pg->flags.
 */

void
uvm_pagezero(struct vm_page *pg)
{
	pg->flags &= ~PG_CLEAN;
	pmap_zero_page(VM_PAGE_TO_PHYS(pg));
}

/*
 * uvm_pagecopy: copy a page
 *
 * => if page is part of an object then the object should be locked
 *	to protect pg->flags.
 */

void
uvm_pagecopy(struct vm_page *src, struct vm_page *dst)
{

	dst->flags &= ~PG_CLEAN;
	pmap_copy_page(VM_PAGE_TO_PHYS(src), VM_PAGE_TO_PHYS(dst));
}

/*
 * uvm_pageismanaged: test it see that a page (specified by PA) is managed.
 */

bool
uvm_pageismanaged(paddr_t pa)
{

	return (vm_physseg_find(atop(pa), NULL) != -1);
}

/*
 * uvm_page_lookup_freelist: look up the free list for the specified page
 */

int
uvm_page_lookup_freelist(struct vm_page *pg)
{
	int lcv;

	lcv = vm_physseg_find(atop(VM_PAGE_TO_PHYS(pg)), NULL);
	KASSERT(lcv != -1);
	return (VM_PHYSMEM_PTR(lcv)->free_list);
}

/*
 * uvm_page_locked_p: return true if object associated with page is
 * locked.  this is a weak check for runtime assertions only.
 */

bool
uvm_page_locked_p(struct vm_page *pg)
{

	if (pg->uobject != NULL) {
		return mutex_owned(pg->uobject->vmobjlock);
	}
	if (pg->uanon != NULL) {
		return mutex_owned(pg->uanon->an_lock);
	}
	return true;
}

#if defined(DDB) || defined(DEBUGPRINT)

/*
 * uvm_page_printit: actually print the page
 */

static const char page_flagbits[] = UVM_PGFLAGBITS;
static const char page_pqflagbits[] = UVM_PQFLAGBITS;

void
uvm_page_printit(struct vm_page *pg, bool full,
    void (*pr)(const char *, ...))
{
	struct vm_page *tpg;
	struct uvm_object *uobj;
	struct pgflist *pgl;
	char pgbuf[128];
	char pqbuf[128];

	(*pr)("PAGE %p:\n", pg);
	snprintb(pgbuf, sizeof(pgbuf), page_flagbits, pg->flags);
	snprintb(pqbuf, sizeof(pqbuf), page_pqflagbits, pg->pqflags);
	(*pr)("  flags=%s, pqflags=%s, wire_count=%d, pa=0x%lx\n",
	    pgbuf, pqbuf, pg->wire_count, (long)VM_PAGE_TO_PHYS(pg));
	(*pr)("  uobject=%p, uanon=%p, offset=0x%llx loan_count=%d\n",
	    pg->uobject, pg->uanon, (long long)pg->offset, pg->loan_count);
#if defined(UVM_PAGE_TRKOWN)
	if (pg->flags & PG_BUSY)
		(*pr)("  owning process = %d, tag=%s\n",
		    pg->owner, pg->owner_tag);
	else
		(*pr)("  page not busy, no owner\n");
#else
	(*pr)("  [page ownership tracking disabled]\n");
#endif

	if (!full)
		return;

	/* cross-verify object/anon */
	if ((pg->pqflags & PQ_FREE) == 0) {
		if (pg->pqflags & PQ_ANON) {
			if (pg->uanon == NULL || pg->uanon->an_page != pg)
			    (*pr)("  >>> ANON DOES NOT POINT HERE <<< (%p)\n",
				(pg->uanon) ? pg->uanon->an_page : NULL);
			else
				(*pr)("  anon backpointer is OK\n");
		} else {
			uobj = pg->uobject;
			if (uobj) {
				(*pr)("  checking object list\n");
				TAILQ_FOREACH(tpg, &uobj->memq, listq.queue) {
					if (tpg == pg) {
						break;
					}
				}
				if (tpg)
					(*pr)("  page found on object list\n");
				else
			(*pr)("  >>> PAGE NOT FOUND ON OBJECT LIST! <<<\n");
			}
		}
	}

	/* cross-verify page queue */
	if (pg->pqflags & PQ_FREE) {
		int fl = uvm_page_lookup_freelist(pg);
		int color = VM_PGCOLOR_BUCKET(pg);
		pgl = &uvm.page_free[fl].pgfl_buckets[color].pgfl_queues[
		    ((pg)->flags & PG_ZERO) ? PGFL_ZEROS : PGFL_UNKNOWN];
	} else {
		pgl = NULL;
	}

	if (pgl) {
		(*pr)("  checking pageq list\n");
		LIST_FOREACH(tpg, pgl, pageq.list) {
			if (tpg == pg) {
				break;
			}
		}
		if (tpg)
			(*pr)("  page found on pageq list\n");
		else
			(*pr)("  >>> PAGE NOT FOUND ON PAGEQ LIST! <<<\n");
	}
}

/*
 * uvm_pages_printthem - print a summary of all managed pages
 */

void
uvm_page_printall(void (*pr)(const char *, ...))
{
	unsigned i;
	struct vm_page *pg;

	(*pr)("%18s %4s %4s %18s %18s"
#ifdef UVM_PAGE_TRKOWN
	    " OWNER"
#endif
	    "\n", "PAGE", "FLAG", "PQ", "UOBJECT", "UANON");
	for (i = 0; i < vm_nphysmem; i++) {
		for (pg = VM_PHYSMEM_PTR(i)->pgs; pg < VM_PHYSMEM_PTR(i)->lastpg; pg++) {
			(*pr)("%18p %04x %04x %18p %18p",
			    pg, pg->flags, pg->pqflags, pg->uobject,
			    pg->uanon);
#ifdef UVM_PAGE_TRKOWN
			if (pg->flags & PG_BUSY)
				(*pr)(" %d [%s]", pg->owner, pg->owner_tag);
#endif
			(*pr)("\n");
		}
	}
}

#endif /* DDB || DEBUGPRINT */

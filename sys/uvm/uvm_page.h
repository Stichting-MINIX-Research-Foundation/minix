/*	$NetBSD: uvm_page.h,v 1.80 2015/03/23 07:59:12 riastradh Exp $	*/

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
 *	@(#)vm_page.h   7.3 (Berkeley) 4/21/91
 * from: Id: uvm_page.h,v 1.1.2.6 1998/02/04 02:31:42 chuck Exp
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

#ifndef _UVM_UVM_PAGE_H_
#define _UVM_UVM_PAGE_H_

#include <uvm/uvm_extern.h>
#include <uvm/uvm_pglist.h>

#include <sys/rbtree.h>

/*
 * Management of resident (logical) pages.
 *
 * Each resident page has a vm_page structure, indexed by page number.
 * There are several lists in the structure:
 *
 * - A red-black tree rooted with the containing object is used to
 *   quickly perform object+offset lookups.
 * - A list of all pages for a given object, for a quick deactivation
 *   at a time of deallocation.
 * - An ordered list of pages due for pageout.
 *
 * In addition, the structure contains the object and offset to which
 * this page belongs (for pageout) and sundry status bits.
 *
 * Note that the page structure has no lock of its own.  The page is
 * generally protected by its owner's lock (UVM object or amap/anon).
 * It should be noted that UVM has to serialize pmap(9) operations on
 * the managed pages, e.g. for pmap_enter() calls.  Hence, the lock
 * order is as follows:
 *
 *	[vmpage-owner-lock] ->
 *		any pmap locks (e.g. PV hash lock)
 *
 * Since the kernel is always self-consistent, no serialization is
 * required for unmanaged mappings, e.g. for pmap_kenter_pa() calls.
 *
 * Field markings and the corresponding locks:
 *
 * f:	free page queue lock, uvm_fpageqlock
 * o:	page owner (uvm_object::vmobjlock, vm_amap::am_lock, vm_anon::an_lock)
 * p:	page queue lock, uvm_pageqlock
 * o,p:	o|p for read, o&p for write
 * w:	wired page queue or uvm_pglistalloc:
 *	  => wired page queue: o&p to change, stable from wire to unwire
 *		XXX What about concurrent or nested wire?
 *	  => uvm_pglistalloc: owned by caller
 * ?:	locked by pmap or assumed page owner's lock
 *
 * UVM and pmap(9) may use uvm_page_locked_p() to assert whether the
 * page owner's lock is acquired.
 *
 * A page can have one of four identities:
 *
 * o free
 *   => pageq.list is entry on global free page queue
 *   => listq.list is entry on per-CPU free page queue
 *   => uanon is unused (or (void *)0xdeadbeef for DEBUG)
 *   => uobject is unused (or (void *)0xdeadbeef for DEBUG)
 *   => PQ_FREE is set in pqflags
 * o owned by a uvm_object
 *   => pageq.queue is entry on wired page queue, if any
 *   => listq.queue is entry on list of pages in object
 *   => uanon is NULL or the vm_anon to which it has been O->A loaned
 *   => uobject is owner
 * o owned by a vm_anon
 *   => pageq is unused (XXX correct?)
 *   => listq is unused (XXX correct?)
 *   => uanon is owner
 *   => uobject is NULL
 *   => PQ_ANON is set in pqflags
 * o allocated by uvm_pglistalloc
 *   => pageq.queue is entry on resulting pglist, owned by caller
 *   => listq is unused (XXX correct?)
 *   => uanon is unused
 *   => uobject is unused
 *
 * The following transitions are allowed:
 *
 * - uvm_pagealloc: free -> owned by a uvm_object/vm_anon
 * - uvm_pagefree: owned by a uvm_object/vm_anon -> free
 * - uvm_pglistalloc: free -> allocated by uvm_pglistalloc
 * - uvm_pglistfree: allocated by uvm_pglistalloc -> free
 */

struct vm_page {
	struct rb_node		rb_node;	/* o: tree of pages in obj */

	union {
		TAILQ_ENTRY(vm_page) queue;	/* w: wired page queue
						 * or uvm_pglistalloc output */
		LIST_ENTRY(vm_page) list;	/* f: global free page queue */
	} pageq;

	union {
		TAILQ_ENTRY(vm_page) queue;	/* o: pages in same object */
		LIST_ENTRY(vm_page) list;	/* f: CPU free page queue */
	} listq;

	struct vm_anon		*uanon;		/* o,p: anon */
	struct uvm_object	*uobject;	/* o,p: object */
	voff_t			offset;		/* o,p: offset into object */
	uint16_t		flags;		/* o: object flags */
	uint16_t		loan_count;	/* o,p: num. active loans */
	uint16_t		wire_count;	/* p: wired down map refs */
	uint16_t		pqflags;	/* p: page queue flags */
	paddr_t			phys_addr;	/* physical address of page */

#ifdef __HAVE_VM_PAGE_MD
	struct vm_page_md	mdpage;		/* ?: pmap-specific data */
#endif

#if defined(UVM_PAGE_TRKOWN)
	/* debugging fields to track page ownership */
	pid_t			owner;		/* proc that set PG_BUSY */
	lwpid_t			lowner;		/* lwp that set PG_BUSY */
	const char		*owner_tag;	/* why it was set busy */
#endif
};

/*
 * Overview of UVM page flags.
 *
 * Locking notes:
 *
 * PG_, struct vm_page::flags	=> locked by owner
 * PQ_, struct vm_page::pqflags	=> locked by uvm_pageqlock
 * PQ_FREE			=> additionally locked by uvm_fpageqlock
 *
 * Flag descriptions:
 *
 * PG_BUSY:
 *	Page is long-term locked, usually because of I/O (transfer from the
 *	page memory to the backing store) is in progress.  LWP attempting
 *	to access the page shall set PG_WANTED and wait.
 *
 * PG_WANTED:
 *	Indicates that the page, which is currently PG_BUSY, is wanted by
 *	some other LWP.  The page owner (i.e. LWP which set PG_BUSY) is
 *	responsible to clear both flags and wake up any waiters once it has
 *	released the long-term lock (PG_BUSY).
 *
 * PG_RELEASED:
 *	Indicates that the page, which is currently PG_BUSY, should be freed
 *	after the release of long-term lock.  It is responsibility of the
 *	owning LWP (i.e. which set PG_BUSY) to do it.
 *
 * PG_CLEAN:
 *	Page has not been modified since it was loaded from the backing
 *	store.  If this flag is not set, page is considered "dirty".
 *	XXX: Currently it means that the page *might* be clean; will be
 *	fixed with yamt-pagecache merge.
 *
 * PG_FAKE:
 *	Page has been allocated, but not yet initialised.  The flag is used
 *	to avoid overwriting of valid data, e.g. to prevent read from the
 *	backing store when in-core data is newer.
 *
 * PG_TABLED:
 *	Indicates that the page is currently in the object's offset queue,
 *	and that it should be removed from it once the page is freed.  Used
 *	diagnostic purposes.
 *
 * PG_PAGEOUT:
 *	Indicates that the page is being paged-out in preparation for
 *	being freed.
 *
 * PG_RDONLY:
 *	Indicates that the page must be mapped read-only.
 *
 * PG_ZERO:
 *	Indicates that the page has been pre-zeroed.  This flag is only
 *	set when the page is not in the queues and is cleared when the
 *	page is placed on the free list.
 *
 * PG_MARKER:
 *	Dummy marker page.
 */

#define	PG_BUSY		0x0001
#define	PG_WANTED	0x0002
#define	PG_TABLED	0x0004
#define	PG_CLEAN	0x0008
#define	PG_PAGEOUT	0x0010
#define	PG_RELEASED	0x0020
#define	PG_FAKE		0x0040
#define	PG_RDONLY	0x0080
#define	PG_ZERO		0x0100
#define	PG_MARKER	0x0200

#define PG_PAGER1	0x1000		/* pager-specific flag */

#define	UVM_PGFLAGBITS \
	"\20\1BUSY\2WANTED\3TABLED\4CLEAN\5PAGEOUT\6RELEASED\7FAKE\10RDONLY" \
	"\11ZERO\12MARKER\15PAGER1"

#define PQ_FREE		0x0001		/* page is on free list */
#define PQ_ANON		0x0002		/* page is part of an anon, rather
					   than an uvm_object */
#define PQ_AOBJ		0x0004		/* page is part of an anonymous
					   uvm_object */
#define PQ_SWAPBACKED	(PQ_ANON|PQ_AOBJ)
#define PQ_READAHEAD	0x0008	/* read-ahead but has not been "hit" yet */

#define PQ_PRIVATE1	0x0100
#define PQ_PRIVATE2	0x0200
#define PQ_PRIVATE3	0x0400
#define PQ_PRIVATE4	0x0800
#define PQ_PRIVATE5	0x1000
#define PQ_PRIVATE6	0x2000
#define PQ_PRIVATE7	0x4000
#define PQ_PRIVATE8	0x8000

#define	UVM_PQFLAGBITS \
	"\20\1FREE\2ANON\3AOBJ\4READAHEAD" \
	"\11PRIVATE1\12PRIVATE2\13PRIVATE3\14PRIVATE4" \
	"\15PRIVATE5\16PRIVATE6\17PRIVATE7\20PRIVATE8"

/*
 * physical memory layout structure
 *
 * MD vmparam.h must #define:
 *   VM_PHYSEG_MAX = max number of physical memory segments we support
 *		   (if this is "1" then we revert to a "contig" case)
 *   VM_PHYSSEG_STRAT: memory sort/search options (for VM_PHYSEG_MAX > 1)
 * 	- VM_PSTRAT_RANDOM:   linear search (random order)
 *	- VM_PSTRAT_BSEARCH:  binary search (sorted by address)
 *	- VM_PSTRAT_BIGFIRST: linear search (sorted by largest segment first)
 *      - others?
 *   XXXCDC: eventually we should purge all left-over global variables...
 */
#define VM_PSTRAT_RANDOM	1
#define VM_PSTRAT_BSEARCH	2
#define VM_PSTRAT_BIGFIRST	3

/*
 * vm_physseg: describes one segment of physical memory
 */
struct vm_physseg {
	paddr_t	start;			/* PF# of first page in segment */
	paddr_t	end;			/* (PF# of last page in segment) + 1 */
	paddr_t	avail_start;		/* PF# of first free page in segment */
	paddr_t	avail_end;		/* (PF# of last free page in segment) +1  */
	struct	vm_page *pgs;		/* vm_page structures (from start) */
	struct	vm_page *lastpg;	/* vm_page structure for end */
	int	free_list;		/* which free list they belong on */
	u_int	start_hint;		/* start looking for free pages here */
					/* protected by uvm_fpageqlock */
#ifdef __HAVE_PMAP_PHYSSEG
	struct	pmap_physseg pmseg;	/* pmap specific (MD) data */
#endif
};

#ifdef _KERNEL

/*
 * globals
 */

extern bool vm_page_zero_enable;

/*
 * physical memory config is stored in vm_physmem.
 */

#define	VM_PHYSMEM_PTR(i)	(&vm_physmem[i])
#if VM_PHYSSEG_MAX == 1
#define VM_PHYSMEM_PTR_SWAP(i, j) /* impossible */
#else
#define VM_PHYSMEM_PTR_SWAP(i, j) \
	do { vm_physmem[(i)] = vm_physmem[(j)]; } while (0)
#endif

extern struct vm_physseg vm_physmem[VM_PHYSSEG_MAX];
extern int vm_nphysseg;

/*
 * prototypes: the following prototypes define the interface to pages
 */

void uvm_page_init(vaddr_t *, vaddr_t *);
#if defined(UVM_PAGE_TRKOWN)
void uvm_page_own(struct vm_page *, const char *);
#endif
#if !defined(PMAP_STEAL_MEMORY)
bool uvm_page_physget(paddr_t *);
#endif
void uvm_page_recolor(int);
void uvm_pageidlezero(void);

void uvm_pageactivate(struct vm_page *);
vaddr_t uvm_pageboot_alloc(vsize_t);
void uvm_pagecopy(struct vm_page *, struct vm_page *);
void uvm_pagedeactivate(struct vm_page *);
void uvm_pagedequeue(struct vm_page *);
void uvm_pageenqueue(struct vm_page *);
void uvm_pagefree(struct vm_page *);
void uvm_page_unbusy(struct vm_page **, int);
struct vm_page *uvm_pagelookup(struct uvm_object *, voff_t);
void uvm_pageunwire(struct vm_page *);
void uvm_pagewire(struct vm_page *);
void uvm_pagezero(struct vm_page *);
bool uvm_pageismanaged(paddr_t);
bool uvm_page_locked_p(struct vm_page *);

int uvm_page_lookup_freelist(struct vm_page *);

int vm_physseg_find(paddr_t, int *);
struct vm_page *uvm_phys_to_vm_page(paddr_t);
paddr_t uvm_vm_page_to_phys(const struct vm_page *);

/*
 * macros
 */

#define UVM_PAGE_TREE_PENALTY	4	/* XXX: a guess */

#define VM_PAGE_TO_PHYS(entry)	uvm_vm_page_to_phys(entry)

#ifdef __HAVE_VM_PAGE_MD
#define	VM_PAGE_TO_MD(pg)	(&(pg)->mdpage)
#endif

/*
 * Compute the page color bucket for a given page.
 */
#define	VM_PGCOLOR_BUCKET(pg) \
	(atop(VM_PAGE_TO_PHYS((pg))) & uvmexp.colormask)

#define	PHYS_TO_VM_PAGE(pa)	uvm_phys_to_vm_page(pa)

#define VM_PAGE_IS_FREE(entry)  ((entry)->pqflags & PQ_FREE)
#define	VM_FREE_PAGE_TO_CPU(pg)	((struct uvm_cpu *)((uintptr_t)pg->offset))

#ifdef DEBUG
void uvm_pagezerocheck(struct vm_page *);
#endif /* DEBUG */

#endif /* _KERNEL */

#endif /* _UVM_UVM_PAGE_H_ */

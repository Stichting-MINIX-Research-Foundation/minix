/*	$NetBSD: pmap.h,v 1.56 2015/04/03 01:04:23 riastradh Exp $	*/

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
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * pmap.h: see pmap.c for the history of this pmap module.
 */

#ifndef _X86_PMAP_H_
#define	_X86_PMAP_H_

/*
 * pl*_pi: index in the ptp page for a pde mapping a VA.
 * (pl*_i below is the index in the virtual array of all pdes per level)
 */
#define pl1_pi(VA)	(((VA_SIGN_POS(VA)) & L1_MASK) >> L1_SHIFT)
#define pl2_pi(VA)	(((VA_SIGN_POS(VA)) & L2_MASK) >> L2_SHIFT)
#define pl3_pi(VA)	(((VA_SIGN_POS(VA)) & L3_MASK) >> L3_SHIFT)
#define pl4_pi(VA)	(((VA_SIGN_POS(VA)) & L4_MASK) >> L4_SHIFT)

/*
 * pl*_i: generate index into pde/pte arrays in virtual space
 *
 * pl_i(va, X) == plX_i(va) <= pl_i_roundup(va, X)
 */
#define pl1_i(VA)	(((VA_SIGN_POS(VA)) & L1_FRAME) >> L1_SHIFT)
#define pl2_i(VA)	(((VA_SIGN_POS(VA)) & L2_FRAME) >> L2_SHIFT)
#define pl3_i(VA)	(((VA_SIGN_POS(VA)) & L3_FRAME) >> L3_SHIFT)
#define pl4_i(VA)	(((VA_SIGN_POS(VA)) & L4_FRAME) >> L4_SHIFT)
#define pl_i(va, lvl) \
        (((VA_SIGN_POS(va)) & ptp_masks[(lvl)-1]) >> ptp_shifts[(lvl)-1])

#define	pl_i_roundup(va, lvl)	pl_i((va)+ ~ptp_masks[(lvl)-1], (lvl))

/*
 * PTP macros:
 *   a PTP's index is the PD index of the PDE that points to it
 *   a PTP's offset is the byte-offset in the PTE space that this PTP is at
 *   a PTP's VA is the first VA mapped by that PTP
 */

#define ptp_va2o(va, lvl)	(pl_i(va, (lvl)+1) * PAGE_SIZE)

/* size of a PDP: usually one page, except for PAE */
#ifdef PAE
#define PDP_SIZE 4
#else
#define PDP_SIZE 1
#endif


#if defined(_KERNEL)
#include <sys/kcpuset.h>

/*
 * pmap data structures: see pmap.c for details of locking.
 */

/*
 * we maintain a list of all non-kernel pmaps
 */

LIST_HEAD(pmap_head, pmap); /* struct pmap_head: head of a pmap list */

/*
 * linked list of all non-kernel pmaps
 */
extern struct pmap_head pmaps;
extern kmutex_t pmaps_lock;    /* protects pmaps */

/*
 * pool_cache(9) that PDPs are allocated from 
 */
extern struct pool_cache pmap_pdp_cache;

/*
 * the pmap structure
 *
 * note that the pm_obj contains the lock pointer, the reference count,
 * page list, and number of PTPs within the pmap.
 *
 * pm_lock is the same as the lock for vm object 0.  Changes to
 * the other objects may only be made if that lock has been taken
 * (the other object locks are only used when uvm_pagealloc is called)
 */

struct pmap {
	struct uvm_object pm_obj[PTP_LEVELS-1]; /* objects for lvl >= 1) */
#define	pm_lock	pm_obj[0].vmobjlock
	kmutex_t pm_obj_lock[PTP_LEVELS-1];	/* locks for pm_objs */
	LIST_ENTRY(pmap) pm_list;	/* list (lck by pm_list lock) */
	pd_entry_t *pm_pdir;		/* VA of PD (lck by object lock) */
	paddr_t pm_pdirpa[PDP_SIZE];	/* PA of PDs (read-only after create) */
	struct vm_page *pm_ptphint[PTP_LEVELS-1];
					/* pointer to a PTP in our pmap */
	struct pmap_statistics pm_stats;  /* pmap stats (lck by object lock) */

#if !defined(__x86_64__)
	vaddr_t pm_hiexec;		/* highest executable mapping */
#endif /* !defined(__x86_64__) */
	int pm_flags;			/* see below */

	union descriptor *pm_ldt;	/* user-set LDT */
	size_t pm_ldt_len;		/* size of LDT in bytes */
	int pm_ldt_sel;			/* LDT selector */
	kcpuset_t *pm_cpus;		/* mask of CPUs using pmap */
	kcpuset_t *pm_kernel_cpus;	/* mask of CPUs using kernel part
					 of pmap */
	kcpuset_t *pm_xen_ptp_cpus;	/* mask of CPUs which have this pmap's
					 ptp mapped */
	uint64_t pm_ncsw;		/* for assertions */
	struct vm_page *pm_gc_ptp;	/* pages from pmap g/c */
};

/* macro to access pm_pdirpa slots */
#ifdef PAE
#define pmap_pdirpa(pmap, index) \
	((pmap)->pm_pdirpa[l2tol3(index)] + l2tol2(index) * sizeof(pd_entry_t))
#else
#define pmap_pdirpa(pmap, index) \
	((pmap)->pm_pdirpa[0] + (index) * sizeof(pd_entry_t))
#endif

/* 
 * flag to be used for kernel mappings: PG_u on Xen/amd64, 
 * 0 otherwise.
 */
#if defined(XEN) && defined(__x86_64__)
#define PG_k PG_u
#else
#define PG_k 0
#endif

/*
 * MD flags that we use for pmap_enter and pmap_kenter_pa:
 */

/*
 * global kernel variables
 */

/*
 * PDPpaddr is the physical address of the kernel's PDP.
 * - i386 non-PAE and amd64: PDPpaddr corresponds directly to the %cr3
 * value associated to the kernel process, proc0.
 * - i386 PAE: it still represents the PA of the kernel's PDP (L2). Due to
 * the L3 PD, it cannot be considered as the equivalent of a %cr3 any more.
 * - Xen: it corresponds to the PFN of the kernel's PDP.
 */
extern u_long PDPpaddr;

extern int pmap_pg_g;			/* do we support PG_G? */
extern long nkptp[PTP_LEVELS];

/*
 * macros
 */

#define	pmap_resident_count(pmap)	((pmap)->pm_stats.resident_count)
#define	pmap_wired_count(pmap)		((pmap)->pm_stats.wired_count)

#define pmap_clear_modify(pg)		pmap_clear_attrs(pg, PG_M)
#define pmap_clear_reference(pg)	pmap_clear_attrs(pg, PG_U)
#define pmap_copy(DP,SP,D,L,S)		__USE(L)
#define pmap_is_modified(pg)		pmap_test_attrs(pg, PG_M)
#define pmap_is_referenced(pg)		pmap_test_attrs(pg, PG_U)
#define pmap_move(DP,SP,D,L,S)
#define pmap_phys_address(ppn)		(x86_ptob(ppn) & ~X86_MMAP_FLAG_MASK)
#define pmap_mmap_flags(ppn)		x86_mmap_flags(ppn)
#define pmap_valid_entry(E) 		((E) & PG_V) /* is PDE or PTE valid? */

#if defined(__x86_64__) || defined(PAE)
#define X86_MMAP_FLAG_SHIFT	(64 - PGSHIFT)
#else
#define X86_MMAP_FLAG_SHIFT	(32 - PGSHIFT)
#endif

#define X86_MMAP_FLAG_MASK	0xf
#define X86_MMAP_FLAG_PREFETCH	0x1

/*
 * prototypes
 */

void		pmap_activate(struct lwp *);
void		pmap_bootstrap(vaddr_t);
bool		pmap_clear_attrs(struct vm_page *, unsigned);
bool		pmap_pv_clear_attrs(paddr_t, unsigned);
void		pmap_deactivate(struct lwp *);
void		pmap_page_remove(struct vm_page *);
void		pmap_pv_remove(paddr_t);
void		pmap_remove(struct pmap *, vaddr_t, vaddr_t);
bool		pmap_test_attrs(struct vm_page *, unsigned);
void		pmap_write_protect(struct pmap *, vaddr_t, vaddr_t, vm_prot_t);
void		pmap_load(void);
paddr_t		pmap_init_tmp_pgtbl(paddr_t);
void		pmap_remove_all(struct pmap *);
void		pmap_ldt_sync(struct pmap *);
void		pmap_kremove_local(vaddr_t, vsize_t);

void		pmap_emap_enter(vaddr_t, paddr_t, vm_prot_t);
void		pmap_emap_remove(vaddr_t, vsize_t);
void		pmap_emap_sync(bool);

#define	__HAVE_PMAP_PV_TRACK	1
void		pmap_pv_init(void);
void		pmap_pv_track(paddr_t, psize_t);
void		pmap_pv_untrack(paddr_t, psize_t);

void		pmap_map_ptes(struct pmap *, struct pmap **, pd_entry_t **,
		    pd_entry_t * const **);
void		pmap_unmap_ptes(struct pmap *, struct pmap *);

int		pmap_pdes_invalid(vaddr_t, pd_entry_t * const *, pd_entry_t *);

u_int		x86_mmap_flags(paddr_t);

bool		pmap_is_curpmap(struct pmap *);

vaddr_t reserve_dumppages(vaddr_t); /* XXX: not a pmap fn */

typedef enum tlbwhy {
	TLBSHOOT_APTE,
	TLBSHOOT_KENTER,
	TLBSHOOT_KREMOVE,
	TLBSHOOT_FREE_PTP1,
	TLBSHOOT_FREE_PTP2,
	TLBSHOOT_REMOVE_PTE,
	TLBSHOOT_REMOVE_PTES,
	TLBSHOOT_SYNC_PV1,
	TLBSHOOT_SYNC_PV2,
	TLBSHOOT_WRITE_PROTECT,
	TLBSHOOT_ENTER,
	TLBSHOOT_UPDATE,
	TLBSHOOT_BUS_DMA,
	TLBSHOOT_BUS_SPACE,
	TLBSHOOT__MAX,
} tlbwhy_t;

void		pmap_tlb_init(void);
void		pmap_tlb_cpu_init(struct cpu_info *);
void		pmap_tlb_shootdown(pmap_t, vaddr_t, pt_entry_t, tlbwhy_t);
void		pmap_tlb_shootnow(void);
void		pmap_tlb_intr(void);

#define	__HAVE_PMAP_EMAP

#define PMAP_GROWKERNEL		/* turn on pmap_growkernel interface */
#define PMAP_FORK		/* turn on pmap_fork interface */

/*
 * Do idle page zero'ing uncached to avoid polluting the cache.
 */
bool	pmap_pageidlezero(paddr_t);
#define	PMAP_PAGEIDLEZERO(pa)	pmap_pageidlezero((pa))

/*
 * inline functions
 */

__inline static bool __unused
pmap_pdes_valid(vaddr_t va, pd_entry_t * const *pdes, pd_entry_t *lastpde)
{
	return pmap_pdes_invalid(va, pdes, lastpde) == 0;
}

/*
 * pmap_update_pg: flush one page from the TLB (or flush the whole thing
 *	if hardware doesn't support one-page flushing)
 */

__inline static void __unused
pmap_update_pg(vaddr_t va)
{
	invlpg(va);
}

/*
 * pmap_update_2pg: flush two pages from the TLB
 */

__inline static void __unused
pmap_update_2pg(vaddr_t va, vaddr_t vb)
{
	invlpg(va);
	invlpg(vb);
}

/*
 * pmap_page_protect: change the protection of all recorded mappings
 *	of a managed page
 *
 * => this function is a frontend for pmap_page_remove/pmap_clear_attrs
 * => we only have to worry about making the page more protected.
 *	unprotecting a page is done on-demand at fault time.
 */

__inline static void __unused
pmap_page_protect(struct vm_page *pg, vm_prot_t prot)
{
	if ((prot & VM_PROT_WRITE) == 0) {
		if (prot & (VM_PROT_READ|VM_PROT_EXECUTE)) {
			(void) pmap_clear_attrs(pg, PG_RW);
		} else {
			pmap_page_remove(pg);
		}
	}
}

/*
 * pmap_pv_protect: change the protection of all recorded mappings
 *	of an unmanaged page
 */

__inline static void __unused
pmap_pv_protect(paddr_t pa, vm_prot_t prot)
{
	if ((prot & VM_PROT_WRITE) == 0) {
		if (prot & (VM_PROT_READ|VM_PROT_EXECUTE)) {
			(void) pmap_pv_clear_attrs(pa, PG_RW);
		} else {
			pmap_pv_remove(pa);
		}
	}
}

/*
 * pmap_protect: change the protection of pages in a pmap
 *
 * => this function is a frontend for pmap_remove/pmap_write_protect
 * => we only have to worry about making the page more protected.
 *	unprotecting a page is done on-demand at fault time.
 */

__inline static void __unused
pmap_protect(struct pmap *pmap, vaddr_t sva, vaddr_t eva, vm_prot_t prot)
{
	if ((prot & VM_PROT_WRITE) == 0) {
		if (prot & (VM_PROT_READ|VM_PROT_EXECUTE)) {
			pmap_write_protect(pmap, sva, eva, prot);
		} else {
			pmap_remove(pmap, sva, eva);
		}
	}
}

/*
 * various address inlines
 *
 *  vtopte: return a pointer to the PTE mapping a VA, works only for
 *  user and PT addresses
 *
 *  kvtopte: return a pointer to the PTE mapping a kernel VA
 */

#include <lib/libkern/libkern.h>

static __inline pt_entry_t * __unused
vtopte(vaddr_t va)
{

	KASSERT(va < VM_MIN_KERNEL_ADDRESS);

	return (PTE_BASE + pl1_i(va));
}

static __inline pt_entry_t * __unused
kvtopte(vaddr_t va)
{
	pd_entry_t *pde;

	KASSERT(va >= VM_MIN_KERNEL_ADDRESS);

	pde = L2_BASE + pl2_i(va);
	if (*pde & PG_PS)
		return ((pt_entry_t *)pde);

	return (PTE_BASE + pl1_i(va));
}

paddr_t vtophys(vaddr_t);
vaddr_t	pmap_map(vaddr_t, paddr_t, paddr_t, vm_prot_t);
void	pmap_cpu_init_late(struct cpu_info *);
bool	sse2_idlezero_page(void *);

#ifdef XEN
#include <sys/bitops.h>

#define XPTE_MASK	L1_FRAME
/* Selects the index of a PTE in (A)PTE_BASE */
#define XPTE_SHIFT	(L1_SHIFT - ilog2(sizeof(pt_entry_t)))

/* PTE access inline fuctions */

/*
 * Get the machine address of the pointed pte
 * We use hardware MMU to get value so works only for levels 1-3
 */

static __inline paddr_t
xpmap_ptetomach(pt_entry_t *pte)
{
	pt_entry_t *up_pte;
	vaddr_t va = (vaddr_t) pte;

	va = ((va & XPTE_MASK) >> XPTE_SHIFT) | (vaddr_t) PTE_BASE;
	up_pte = (pt_entry_t *) va;

	return (paddr_t) (((*up_pte) & PG_FRAME) + (((vaddr_t) pte) & (~PG_FRAME & ~VA_SIGN_MASK)));
}

/* Xen helpers to change bits of a pte */
#define XPMAP_UPDATE_DIRECT	1	/* Update direct map entry flags too */

paddr_t	vtomach(vaddr_t);
#define vtomfn(va) (vtomach(va) >> PAGE_SHIFT)
#endif	/* XEN */

/* pmap functions with machine addresses */
void	pmap_kenter_ma(vaddr_t, paddr_t, vm_prot_t, u_int);
int	pmap_enter_ma(struct pmap *, vaddr_t, paddr_t, paddr_t,
	    vm_prot_t, u_int, int);
bool	pmap_extract_ma(pmap_t, vaddr_t, paddr_t *);

/*
 * Hooks for the pool allocator.
 */
#define	POOL_VTOPHYS(va)	vtophys((vaddr_t) (va))

#ifdef __HAVE_DIRECT_MAP

#define L4_SLOT_DIRECT		509
#define PDIR_SLOT_DIRECT	L4_SLOT_DIRECT

#define PMAP_DIRECT_BASE	(VA_SIGN_NEG((L4_SLOT_DIRECT * NBPD_L4)))
#define PMAP_DIRECT_END		(VA_SIGN_NEG(((L4_SLOT_DIRECT + 1) * NBPD_L4)))

#define PMAP_DIRECT_MAP(pa)	((vaddr_t)PMAP_DIRECT_BASE + (pa))
#define PMAP_DIRECT_UNMAP(va)	((paddr_t)(va) - PMAP_DIRECT_BASE)

/*
 * Alternate mapping hooks for pool pages.
 */
#define PMAP_MAP_POOLPAGE(pa)	PMAP_DIRECT_MAP((pa))
#define PMAP_UNMAP_POOLPAGE(va)	PMAP_DIRECT_UNMAP((va))

void	pagezero(vaddr_t);

#endif /* __HAVE_DIRECT_MAP */

#endif /* _KERNEL */

#endif /* _X86_PMAP_H_ */

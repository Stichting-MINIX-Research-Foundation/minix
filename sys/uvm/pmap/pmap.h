/*	$NetBSD: pmap.h,v 1.1 2012/10/03 00:51:46 christos Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 *	@(#)pmap.h	8.1 (Berkeley) 6/10/93
 */

/*
 * Copyright (c) 1987 Carnegie-Mellon University
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)pmap.h	8.1 (Berkeley) 6/10/93
 */

#ifndef	_COMMON_PMAP_H_
#define	_COMMON_PMAP_H_

#include <uvm/uvm_stat.h>
#ifdef UVMHIST
UVMHIST_DECL(pmapexechist);
UVMHIST_DECL(pmaphist);
#endif

/*
 * The user address space is mapped using a two level structure where
 * virtual address bits 31..22 are used to index into a segment table which
 * points to a page worth of PTEs (4096 page can hold 1024 PTEs).
 * Bits 21..12 are then used to index a PTE which describes a page within
 * a segment.
 */

#define pmap_trunc_seg(x)	((vaddr_t)(x) & ~SEGOFSET)
#define pmap_round_seg(x)	(((vaddr_t)(x) + SEGOFSET) & ~SEGOFSET)

/*
 * Each seg_tab point an array of pt_entry [NPTEPG]
 */
typedef union pmap_segtab {
	union pmap_segtab *	seg_seg[PMAP_SEGTABSIZE];
	pt_entry_t *		seg_tab[PMAP_SEGTABSIZE];
} pmap_segtab_t;

#ifdef _KERNEL
struct pmap;
typedef bool (*pte_callback_t)(struct pmap *, vaddr_t, vaddr_t,
	pt_entry_t *, uintptr_t);
pt_entry_t *pmap_pte_lookup(struct pmap *, vaddr_t);
pt_entry_t *pmap_pte_reserve(struct pmap *, vaddr_t, int);
void pmap_pte_process(struct pmap *, vaddr_t, vaddr_t, pte_callback_t,
	uintptr_t);
void pmap_segtab_activate(struct pmap *, struct lwp *);
void pmap_segtab_init(struct pmap *);
void pmap_segtab_destroy(struct pmap *, pte_callback_t, uintptr_t);
extern kmutex_t pmap_segtab_lock;
#endif /* _KERNEL */

/*
 * Per TLB (normally same as CPU) asid info
 */
struct pmap_asid_info {
	LIST_ENTRY(pmap_asid_info) pai_link;
	uint32_t	pai_asid;	/* TLB address space tag */
};

#define	TLBINFO_LOCK(ti)		mutex_spin_enter((ti)->ti_lock)
#define	TLBINFO_UNLOCK(ti)		mutex_spin_exit((ti)->ti_lock)
#define	PMAP_PAI_ASIDVALID_P(pai, ti)	((pai)->pai_asid != 0)
#define	PMAP_PAI(pmap, ti)		(&(pmap)->pm_pai[tlbinfo_index(ti)])
#define	PAI_PMAP(pai, ti)	\
	((pmap_t)((intptr_t)(pai) \
	    - offsetof(struct pmap, pm_pai[tlbinfo_index(ti)])))

/*
 * Machine dependent pmap structure.
 */
struct pmap {
#ifdef MULTIPROCESSOR
	__cpuset_t		pm_active;	/* pmap was active on ... */
	__cpuset_t		pm_onproc;	/* pmap is active on ... */
	volatile u_int		pm_shootdown_pending;
#endif
	pmap_segtab_t *		pm_segtab;	/* pointers to pages of PTEs */
	u_int			pm_count;	/* pmap reference count */
	u_int			pm_flags;
#define	PMAP_DEFERRED_ACTIVATE	0x0001
	struct pmap_statistics	pm_stats;	/* pmap statistics */
	vaddr_t			pm_minaddr;
	vaddr_t			pm_maxaddr;
	struct pmap_asid_info	pm_pai[1];
};

enum tlb_invalidate_op {
	TLBINV_NOBODY=0,
	TLBINV_ONE=1,
	TLBINV_ALLUSER=2,
	TLBINV_ALLKERNEL=3,
	TLBINV_ALL=4
};

struct pmap_tlb_info {
	char ti_name[8];
	uint32_t ti_asid_hint;		/* probable next ASID to use */
	uint32_t ti_asids_free;		/* # of ASIDs free */
#define	tlbinfo_noasids_p(ti)	((ti)->ti_asids_free == 0)
	kmutex_t *ti_lock;
	u_int ti_wired;			/* # of wired TLB entries */
	uint32_t ti_asid_max;
	LIST_HEAD(, pmap_asid_info) ti_pais; /* list of active ASIDs */
#ifdef MULTIPROCESSOR
	pmap_t ti_victim;
	uint32_t ti_synci_page_bitmap;	/* page indices needing a syncicache */
	__cpuset_t ti_cpu_mask;		/* bitmask of CPUs sharing this TLB */
	enum tlb_invalidate_op ti_tlbinvop;
	u_int ti_index;
#define tlbinfo_index(ti)	((ti)->ti_index)
	struct evcnt ti_evcnt_synci_asts;
	struct evcnt ti_evcnt_synci_all;
	struct evcnt ti_evcnt_synci_pages;
	struct evcnt ti_evcnt_synci_deferred;
	struct evcnt ti_evcnt_synci_desired;
	struct evcnt ti_evcnt_synci_duplicate;
#else
#define tlbinfo_index(ti)	(0)
#endif
	struct evcnt ti_evcnt_asid_reinits;
	u_long ti_asid_bitmap[256 / (sizeof(u_long) * 8)];
};

#ifdef	_KERNEL

struct pmap_kernel {
	struct pmap kernel_pmap;
#ifdef MULTIPROCESSOR
	struct pmap_asid_info kernel_pai[MAXCPUS-1];
#endif
};

struct pmap_limits {
	paddr_t avail_start;
	paddr_t avail_end;
	vaddr_t virtual_end;
};

/* 
 * The pools from which pmap structures and sub-structures are allocated.
 */
extern struct pool pmap_pmap_pool; 
extern struct pool pmap_pv_pool;
extern struct pool_allocator pmap_pv_page_allocator;

extern struct pmap_kernel kernel_pmap_store;
extern struct pmap_tlb_info pmap_tlb0_info;
#ifdef MULTIPROCESSOR
extern struct pmap_tlb_info *pmap_tlbs[MAXCPUS];
extern u_int pmap_ntlbs;
#endif
extern struct pmap_limits pmap_limits;

#define	pmap_wired_count(pmap) 	((pmap)->pm_stats.wired_count)
#define pmap_resident_count(pmap) ((pmap)->pm_stats.resident_count)

/*
 *	Bootstrap the system enough to run with virtual memory.
 */
void	pmap_remove_all(pmap_t);
void	pmap_set_modified(paddr_t);
bool	pmap_page_clear_attributes(struct vm_page_md *, u_int);
void	pmap_page_set_attributes(struct vm_page_md *, u_int);
void	pmap_pvlist_lock_init(size_t);

#define	PMAP_WB		0
#define	PMAP_WBINV	1
#define	PMAP_INV	2

#ifdef MULTIPROCESSOR
void	pmap_tlb_shootdown_process(void);
bool	pmap_tlb_shootdown_bystanders(pmap_t pmap);
void	pmap_tlb_info_attach(struct pmap_tlb_info *, struct cpu_info *);
void	pmap_md_tlb_info_attach(struct pmap_tlb_info *, struct cpu_info *);
void	pmap_syncicache_ast(struct cpu_info *);
void	pmap_syncicache_wanted(struct cpu_info *);
void	pmap_syncicache(vaddr_t, uint32_t);
#endif
void	pmap_tlb_info_init(struct pmap_tlb_info *);
void	pmap_tlb_info_evcnt_attach(struct pmap_tlb_info *);
void	pmap_tlb_asid_acquire(pmap_t, struct lwp *l);
void	pmap_tlb_asid_deactivate(pmap_t);
void	pmap_tlb_asid_release_all(pmap_t);
int	pmap_tlb_update_addr(pmap_t, vaddr_t, uint32_t, u_int);
#define	PMAP_TLB_NEED_IPI	0x01
#define	PMAP_TLB_INSERT		0x02
void	pmap_tlb_invalidate_addr(pmap_t, vaddr_t);
void	pmap_tlb_check(pmap_t, bool (*)(void *, vaddr_t, tlb_asid_t, pt_entry_t));
void	pmap_tlb_asid_check(void);

uint16_t pmap_pvlist_lock(struct vm_page_md *, bool);

#define	PMAP_STEAL_MEMORY	/* enable pmap_steal_memory() */
#define	PMAP_GROWKERNEL		/* enable pmap_growkernel() */

/*
 * Alternate mapping hooks for pool pages.  Avoids thrashing the TLB.
 */
vaddr_t pmap_map_poolpage(paddr_t);
paddr_t pmap_unmap_poolpage(vaddr_t);
struct vm_page *pmap_md_alloc_poolpage(int);
#define	PMAP_ALLOC_POOLPAGE(flags)	pmap_md_alloc_poolpage(flags)
#define	PMAP_MAP_POOLPAGE(pa)		pmap_map_poolpage(pa)
#define	PMAP_UNMAP_POOLPAGE(va)		pmap_unmap_poolpage(va)

#endif	/* _KERNEL */
#endif	/* _COMMON_PMAP_H_ */

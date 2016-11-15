/*	$NetBSD: pmap.c,v 1.12 2015/06/11 05:27:07 matt Exp $	*/

/*-
 * Copyright (c) 1998, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center and by Chris G. Demetriou.
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
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 *	@(#)pmap.c	8.4 (Berkeley) 1/26/94
 */

#include <sys/cdefs.h>

__KERNEL_RCSID(0, "$NetBSD: pmap.c,v 1.12 2015/06/11 05:27:07 matt Exp $");

/*
 *	Manages physical address maps.
 *
 *	In addition to hardware address maps, this
 *	module is called upon to provide software-use-only
 *	maps which may or may not be stored in the same
 *	form as hardware maps.  These pseudo-maps are
 *	used to store intermediate results from copy
 *	operations to and from address spaces.
 *
 *	Since the information managed by this module is
 *	also stored by the logical address mapping module,
 *	this module may throw away valid virtual-to-physical
 *	mappings at almost any time.  However, invalidations
 *	of virtual-to-physical mappings must be done as
 *	requested.
 *
 *	In order to cope with hardware architectures which
 *	make virtual-to-physical map invalidates expensive,
 *	this module may delay invalidate or reduced protection
 *	operations until such time as they are actually
 *	necessary.  This module is given full information as
 *	to which processors are currently using which maps,
 *	and to when physical maps must be made correct.
 */

#include "opt_modular.h"
#include "opt_multiprocessor.h"
#include "opt_sysv.h"

#define __PMAP_PRIVATE

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/pool.h>
#include <sys/atomic.h>
#include <sys/mutex.h>
#include <sys/atomic.h>
#ifdef SYSVSHM
#include <sys/shm.h>
#endif
#include <sys/socketvar.h>	/* XXX: for sock_loan_thresh */

#include <uvm/uvm.h>

#define	PMAP_COUNT(name)	(pmap_evcnt_##name.ev_count++ + 0)
#define PMAP_COUNTER(name, desc) \
static struct evcnt pmap_evcnt_##name = \
	EVCNT_INITIALIZER(EVCNT_TYPE_MISC, NULL, "pmap", desc); \
EVCNT_ATTACH_STATIC(pmap_evcnt_##name)

PMAP_COUNTER(remove_kernel_calls, "remove kernel calls");
PMAP_COUNTER(remove_kernel_pages, "kernel pages unmapped");
PMAP_COUNTER(remove_user_calls, "remove user calls");
PMAP_COUNTER(remove_user_pages, "user pages unmapped");
PMAP_COUNTER(remove_flushes, "remove cache flushes");
PMAP_COUNTER(remove_tlb_ops, "remove tlb ops");
PMAP_COUNTER(remove_pvfirst, "remove pv first");
PMAP_COUNTER(remove_pvsearch, "remove pv search");

PMAP_COUNTER(prefer_requests, "prefer requests");
PMAP_COUNTER(prefer_adjustments, "prefer adjustments");

PMAP_COUNTER(idlezeroed_pages, "pages idle zeroed");
PMAP_COUNTER(zeroed_pages, "pages zeroed");
PMAP_COUNTER(copied_pages, "pages copied");

PMAP_COUNTER(kenter_pa, "kernel fast mapped pages");
PMAP_COUNTER(kenter_pa_bad, "kernel fast mapped pages (bad color)");
PMAP_COUNTER(kenter_pa_unmanaged, "kernel fast mapped unmanaged pages");
PMAP_COUNTER(kremove_pages, "kernel fast unmapped pages");

PMAP_COUNTER(page_cache_evictions, "pages changed to uncacheable");
PMAP_COUNTER(page_cache_restorations, "pages changed to cacheable");

PMAP_COUNTER(kernel_mappings_bad, "kernel pages mapped (bad color)");
PMAP_COUNTER(user_mappings_bad, "user pages mapped (bad color)");
PMAP_COUNTER(kernel_mappings, "kernel pages mapped");
PMAP_COUNTER(user_mappings, "user pages mapped");
PMAP_COUNTER(user_mappings_changed, "user mapping changed");
PMAP_COUNTER(kernel_mappings_changed, "kernel mapping changed");
PMAP_COUNTER(uncached_mappings, "uncached pages mapped");
PMAP_COUNTER(unmanaged_mappings, "unmanaged pages mapped");
PMAP_COUNTER(managed_mappings, "managed pages mapped");
PMAP_COUNTER(mappings, "pages mapped");
PMAP_COUNTER(remappings, "pages remapped");
PMAP_COUNTER(unmappings, "pages unmapped");
PMAP_COUNTER(primary_mappings, "page initial mappings");
PMAP_COUNTER(primary_unmappings, "page final unmappings");
PMAP_COUNTER(tlb_hit, "page mapping");

PMAP_COUNTER(exec_mappings, "exec pages mapped");
PMAP_COUNTER(exec_synced_mappings, "exec pages synced");
PMAP_COUNTER(exec_synced_remove, "exec pages synced (PR)");
PMAP_COUNTER(exec_synced_clear_modify, "exec pages synced (CM)");
PMAP_COUNTER(exec_synced_page_protect, "exec pages synced (PP)");
PMAP_COUNTER(exec_synced_protect, "exec pages synced (P)");
PMAP_COUNTER(exec_uncached_page_protect, "exec pages uncached (PP)");
PMAP_COUNTER(exec_uncached_clear_modify, "exec pages uncached (CM)");
PMAP_COUNTER(exec_uncached_zero_page, "exec pages uncached (ZP)");
PMAP_COUNTER(exec_uncached_copy_page, "exec pages uncached (CP)");
PMAP_COUNTER(exec_uncached_remove, "exec pages uncached (PR)");

PMAP_COUNTER(create, "creates");
PMAP_COUNTER(reference, "references");
PMAP_COUNTER(dereference, "dereferences");
PMAP_COUNTER(destroy, "destroyed");
PMAP_COUNTER(activate, "activations");
PMAP_COUNTER(deactivate, "deactivations");
PMAP_COUNTER(update, "updates");
#ifdef MULTIPROCESSOR
PMAP_COUNTER(shootdown_ipis, "shootdown IPIs");
#endif
PMAP_COUNTER(unwire, "unwires");
PMAP_COUNTER(copy, "copies");
PMAP_COUNTER(clear_modify, "clear_modifies");
PMAP_COUNTER(protect, "protects");
PMAP_COUNTER(page_protect, "page_protects");

#define PMAP_ASID_RESERVED 0
CTASSERT(PMAP_ASID_RESERVED == 0);

/*
 * Initialize the kernel pmap.
 */
#ifdef MULTIPROCESSOR
#define	PMAP_SIZE	offsetof(struct pmap, pm_pai[PMAP_TLB_MAX])
#else
#define	PMAP_SIZE	sizeof(struct pmap)
kmutex_t pmap_pvlist_mutex __aligned(COHERENCY_UNIT);
#endif

struct pmap_kernel kernel_pmap_store = {
	.kernel_pmap = {
		.pm_count = 1,
		.pm_segtab = PMAP_INVALID_SEGTAB_ADDRESS,
		.pm_minaddr = VM_MIN_KERNEL_ADDRESS,
		.pm_maxaddr = VM_MAX_KERNEL_ADDRESS,
	},
};

struct pmap * const kernel_pmap_ptr = &kernel_pmap_store.kernel_pmap;

struct pmap_limits pmap_limits = {
	.virtual_start = VM_MIN_KERNEL_ADDRESS,
};

#ifdef UVMHIST
static struct kern_history_ent pmapexechistbuf[10000];
static struct kern_history_ent pmaphistbuf[10000];
UVMHIST_DEFINE(pmapexechist);
UVMHIST_DEFINE(pmaphist);
#endif

/*
 * The pools from which pmap structures and sub-structures are allocated.
 */
struct pool pmap_pmap_pool;
struct pool pmap_pv_pool;

#ifndef PMAP_PV_LOWAT
#define	PMAP_PV_LOWAT	16
#endif
int		pmap_pv_lowat = PMAP_PV_LOWAT;

bool		pmap_initialized = false;
#define	PMAP_PAGE_COLOROK_P(a, b) \
		((((int)(a) ^ (int)(b)) & pmap_page_colormask) == 0)
u_int		pmap_page_colormask;

#define PAGE_IS_MANAGED(pa)	\
	(pmap_initialized == true && vm_physseg_find(atop(pa), NULL) != -1)

#define PMAP_IS_ACTIVE(pm)						\
	((pm) == pmap_kernel() || 					\
	 (pm) == curlwp->l_proc->p_vmspace->vm_map.pmap)

/* Forward function declarations */
void pmap_remove_pv(pmap_t, vaddr_t, struct vm_page *, bool);
void pmap_enter_pv(pmap_t, vaddr_t, struct vm_page *, u_int *);

/*
 * PV table management functions.
 */
void	*pmap_pv_page_alloc(struct pool *, int);
void	pmap_pv_page_free(struct pool *, void *);

struct pool_allocator pmap_pv_page_allocator = {
	pmap_pv_page_alloc, pmap_pv_page_free, 0,
};

#define	pmap_pv_alloc()		pool_get(&pmap_pv_pool, PR_NOWAIT)
#define	pmap_pv_free(pv)	pool_put(&pmap_pv_pool, (pv))

#if !defined(MULTIPROCESSOR) || !defined(PMAP_MD_NEED_TLB_MISS_LOCK)
#define	pmap_md_tlb_miss_lock_enter()	do { } while(/*CONSTCOND*/0)
#define	pmap_md_tlb_miss_lock_exit()	do { } while(/*CONSTCOND*/0)
#endif	/* !MULTIPROCESSOR || !PMAP_MD_NEED_TLB_MISS_LOCK */

/*
 * Misc. functions.
 */

bool
pmap_page_clear_attributes(struct vm_page_md *mdpg, u_int clear_attributes)
{
	volatile u_int * const attrp = &mdpg->mdpg_attrs;
#ifdef MULTIPROCESSOR
	for (;;) {
		u_int old_attr = *attrp;
		if ((old_attr & clear_attributes) == 0)
			return false;
		u_int new_attr = old_attr & ~clear_attributes;
		if (old_attr == atomic_cas_uint(attrp, old_attr, new_attr))
			return true;
	}
#else
	u_int old_attr = *attrp;
	if ((old_attr & clear_attributes) == 0)
		return false;
	*attrp &= ~clear_attributes;
	return true;
#endif
}

void
pmap_page_set_attributes(struct vm_page_md *mdpg, u_int set_attributes)
{
#ifdef MULTIPROCESSOR
	atomic_or_uint(&mdpg->mdpg_attrs, set_attributes);
#else
	mdpg->mdpg_attrs |= set_attributes;
#endif
}

static void
pmap_page_syncicache(struct vm_page *pg)
{
#ifndef MULTIPROCESSOR
	struct pmap * const curpmap = curcpu()->ci_curpm;
#endif
	struct vm_page_md * const mdpg = VM_PAGE_TO_MD(pg);
	pv_entry_t pv = &mdpg->mdpg_first;
	kcpuset_t *onproc;
#ifdef MULTIPROCESSOR
	kcpuset_create(&onproc, true);
#else
	onproc = NULL;
#endif
	(void)VM_PAGEMD_PVLIST_LOCK(mdpg, false);

	if (pv->pv_pmap != NULL) {
		for (; pv != NULL; pv = pv->pv_next) {
#ifdef MULTIPROCESSOR
			kcpuset_merge(onproc, pv->pv_pmap->pm_onproc);
			if (kcpuset_match(onproc, kcpuset_running)) {
				break;
			}
#else
			if (pv->pv_pmap == curpmap) {
				onproc = curcpu()->ci_data.cpu_kcpuset;
				break;
			}
#endif
		}
	}
	VM_PAGEMD_PVLIST_UNLOCK(mdpg);
	kpreempt_disable();
	pmap_md_page_syncicache(pg, onproc);
#ifdef MULTIPROCESSOR
	kcpuset_destroy(onproc);
#endif
	kpreempt_enable();
}

/*
 * Define the initial bounds of the kernel virtual address space.
 */
void
pmap_virtual_space(vaddr_t *vstartp, vaddr_t *vendp)
{

	*vstartp = pmap_limits.virtual_start;
	*vendp = pmap_limits.virtual_end;
}

vaddr_t
pmap_growkernel(vaddr_t maxkvaddr)
{
	vaddr_t virtual_end = pmap_limits.virtual_end; 
	maxkvaddr = pmap_round_seg(maxkvaddr) - 1;

	/*
	 * Reserve PTEs for the new KVA space.
	 */
	for (; virtual_end < maxkvaddr; virtual_end += NBSEG) {
		pmap_pte_reserve(pmap_kernel(), virtual_end, 0);
	}

	/*
	 * Don't exceed VM_MAX_KERNEL_ADDRESS!
	 */
	if (virtual_end == 0 || virtual_end > VM_MAX_KERNEL_ADDRESS)
		virtual_end = VM_MAX_KERNEL_ADDRESS;

	/*
	 * Update new end.
	 */
	pmap_limits.virtual_end = virtual_end;
	return virtual_end;
}

/*
 * Bootstrap memory allocator (alternative to vm_bootstrap_steal_memory()).
 * This function allows for early dynamic memory allocation until the virtual
 * memory system has been bootstrapped.  After that point, either kmem_alloc
 * or malloc should be used.  This function works by stealing pages from the
 * (to be) managed page pool, then implicitly mapping the pages (by using
 * their k0seg addresses) and zeroing them.
 *
 * It may be used once the physical memory segments have been pre-loaded
 * into the vm_physmem[] array.  Early memory allocation MUST use this
 * interface!  This cannot be used after vm_page_startup(), and will
 * generate a panic if tried.
 *
 * Note that this memory will never be freed, and in essence it is wired
 * down.
 *
 * We must adjust *vstartp and/or *vendp iff we use address space
 * from the kernel virtual address range defined by pmap_virtual_space().
 */
vaddr_t
pmap_steal_memory(vsize_t size, vaddr_t *vstartp, vaddr_t *vendp)
{
	u_int npgs;
	paddr_t pa;
	vaddr_t va;

	size = round_page(size);
	npgs = atop(size);

	for (u_int bank = 0; bank < vm_nphysseg; bank++) {
		struct vm_physseg * const seg = VM_PHYSMEM_PTR(bank);
		if (uvm.page_init_done == true)
			panic("pmap_steal_memory: called _after_ bootstrap");

		if (seg->avail_start != seg->start ||
		    seg->avail_start >= seg->avail_end)
			continue;

		if ((seg->avail_end - seg->avail_start) < npgs)
			continue;

		/*
		 * There are enough pages here; steal them!
		 */
		pa = ptoa(seg->avail_start);
		seg->avail_start += npgs;
		seg->start += npgs;

		/*
		 * Have we used up this segment?
		 */
		if (seg->avail_start == seg->end) {
			if (vm_nphysseg == 1)
				panic("pmap_steal_memory: out of memory!");

			/* Remove this segment from the list. */
			vm_nphysseg--;
			if (bank < vm_nphysseg)
				memmove(seg, seg+1,
				    sizeof(*seg) * (vm_nphysseg - bank));
		}

		va = pmap_md_map_poolpage(pa, size);
		memset((void *)va, 0, size);
		return va;
	}

	/*
	 * If we got here, there was no memory left.
	 */
	panic("pmap_steal_memory: no memory to steal");
}

/*
 *	Initialize the pmap module.
 *	Called by vm_init, to initialize any structures that the pmap
 *	system needs to map virtual memory.
 */
void
pmap_init(void)
{
	UVMHIST_INIT_STATIC(pmapexechist, pmapexechistbuf);
	UVMHIST_INIT_STATIC(pmaphist, pmaphistbuf);

	UVMHIST_FUNC(__func__); UVMHIST_CALLED(pmaphist);

	/*
	 * Initialize the segtab lock.
	 */
	mutex_init(&pmap_segtab_lock, MUTEX_DEFAULT, IPL_HIGH);

	/*
	 * Set a low water mark on the pv_entry pool, so that we are
	 * more likely to have these around even in extreme memory
	 * starvation.
	 */
	pool_setlowat(&pmap_pv_pool, pmap_pv_lowat);

	pmap_md_init();

	/*
	 * Now it is safe to enable pv entry recording.
	 */
	pmap_initialized = true;
}

/*
 *	Create and return a physical map.
 *
 *	If the size specified for the map
 *	is zero, the map is an actual physical
 *	map, and may be referenced by the
 *	hardware.
 *
 *	If the size specified is non-zero,
 *	the map will be used in software only, and
 *	is bounded by that size.
 */
pmap_t
pmap_create(void)
{
	pmap_t pmap;

	UVMHIST_FUNC(__func__); UVMHIST_CALLED(pmaphist);
	PMAP_COUNT(create);

	pmap = pool_get(&pmap_pmap_pool, PR_WAITOK);
	memset(pmap, 0, PMAP_SIZE);

	KASSERT(pmap->pm_pai[0].pai_link.le_prev == NULL);

	pmap->pm_count = 1;
	pmap->pm_minaddr = VM_MIN_ADDRESS;
	pmap->pm_maxaddr = VM_MAXUSER_ADDRESS;

	pmap_segtab_init(pmap);

#ifdef MULTIPROCESSOR
	kcpuset_create(&pmap->pm_active, true);
	kcpuset_create(&pmap->pm_onproc, true);
#endif

	UVMHIST_LOG(pmaphist, "<- pmap %p", pmap,0,0,0);
	return pmap;
}

/*
 *	Retire the given physical map from service.
 *	Should only be called if the map contains
 *	no valid mappings.
 */
void
pmap_destroy(pmap_t pmap)
{
	UVMHIST_FUNC(__func__); UVMHIST_CALLED(pmaphist);
	UVMHIST_LOG(pmaphist, "(pmap=%p)", pmap, 0,0,0);

	if (atomic_dec_uint_nv(&pmap->pm_count) > 0) {
		PMAP_COUNT(dereference);
		return;
	}

	KASSERT(pmap->pm_count == 0);
	PMAP_COUNT(destroy);
	kpreempt_disable();
	pmap_md_tlb_miss_lock_enter();
	pmap_tlb_asid_release_all(pmap);
	pmap_segtab_destroy(pmap, NULL, 0);
	pmap_md_tlb_miss_lock_exit();

#ifdef MULTIPROCESSOR
	kcpuset_destroy(pmap->pm_active);
	kcpuset_destroy(pmap->pm_onproc);
#endif

	pool_put(&pmap_pmap_pool, pmap);
	kpreempt_enable();

	UVMHIST_LOG(pmaphist, "<- done", 0,0,0,0);
}

/*
 *	Add a reference to the specified pmap.
 */
void
pmap_reference(pmap_t pmap)
{

	UVMHIST_FUNC(__func__); UVMHIST_CALLED(pmaphist);
	UVMHIST_LOG(pmaphist, "(pmap=%p)", pmap, 0,0,0);
	PMAP_COUNT(reference);

	if (pmap != NULL) {
		atomic_inc_uint(&pmap->pm_count);
	}

	UVMHIST_LOG(pmaphist, "<- done", 0,0,0,0);
}

/*
 *	Make a new pmap (vmspace) active for the given process.
 */
void
pmap_activate(struct lwp *l)
{
	pmap_t pmap = l->l_proc->p_vmspace->vm_map.pmap;

	UVMHIST_FUNC(__func__); UVMHIST_CALLED(pmaphist);
	UVMHIST_LOG(pmaphist, "(l=%p (pmap=%p))", l, pmap, 0,0);
	PMAP_COUNT(activate);

	kpreempt_disable();
	pmap_md_tlb_miss_lock_enter();
	pmap_tlb_asid_acquire(pmap, l);
	if (l == curlwp) {
		pmap_segtab_activate(pmap, l);
	}
	pmap_md_tlb_miss_lock_exit();
	kpreempt_enable();

	UVMHIST_LOG(pmaphist, "<- done", 0,0,0,0);
}

/*
 *	Make a previously active pmap (vmspace) inactive.
 */
void
pmap_deactivate(struct lwp *l)
{
	pmap_t pmap = l->l_proc->p_vmspace->vm_map.pmap;

	UVMHIST_FUNC(__func__); UVMHIST_CALLED(pmaphist);
	UVMHIST_LOG(pmaphist, "(l=%p (pmap=%p))", l, pmap, 0,0);
	PMAP_COUNT(deactivate);

	kpreempt_disable();
	pmap_md_tlb_miss_lock_enter();
	curcpu()->ci_pmap_user_segtab = PMAP_INVALID_SEGTAB_ADDRESS;
	pmap_tlb_asid_deactivate(pmap);
	pmap_md_tlb_miss_lock_exit();
	kpreempt_enable();

	UVMHIST_LOG(pmaphist, "<- done", 0,0,0,0);
}

void
pmap_update(struct pmap *pmap)
{

	UVMHIST_FUNC(__func__); UVMHIST_CALLED(pmaphist);
	UVMHIST_LOG(pmaphist, "(pmap=%p)", pmap, 0,0,0);
	PMAP_COUNT(update);

	kpreempt_disable();
#if defined(MULTIPROCESSOR) && defined(PMAP_NEED_TLB_SHOOTDOWN)
	u_int pending = atomic_swap_uint(&pmap->pm_shootdown_pending, 0);
	if (pending && pmap_tlb_shootdown_bystanders(pmap))
		PMAP_COUNT(shootdown_ipis);
#endif
	pmap_md_tlb_miss_lock_enter();
#if defined(DEBUG) && !defined(MULTIPROCESSOR)
	pmap_tlb_check(pmap, pmap_md_tlb_check_entry);
#endif /* DEBUG */

	/*
	 * If pmap_remove_all was called, we deactivated ourselves and nuked
	 * our ASID.  Now we have to reactivate ourselves.
	 */
	if (__predict_false(pmap->pm_flags & PMAP_DEFERRED_ACTIVATE)) {
		pmap->pm_flags ^= PMAP_DEFERRED_ACTIVATE;
		pmap_tlb_asid_acquire(pmap, curlwp);
		pmap_segtab_activate(pmap, curlwp);
	}
	pmap_md_tlb_miss_lock_exit();
	kpreempt_enable();

	UVMHIST_LOG(pmaphist, "<- done", 0,0,0,0);
}

/*
 *	Remove the given range of addresses from the specified map.
 *
 *	It is assumed that the start and end are properly
 *	rounded to the page size.
 */

static bool
pmap_pte_remove(pmap_t pmap, vaddr_t sva, vaddr_t eva, pt_entry_t *ptep,
	uintptr_t flags)
{
	const pt_entry_t npte = flags;
	const bool is_kernel_pmap_p = (pmap == pmap_kernel());

	UVMHIST_FUNC(__func__); UVMHIST_CALLED(pmaphist);
	UVMHIST_LOG(pmaphist, "(pmap=%p %sva=%"PRIxVADDR"..%"PRIxVADDR,
	    pmap, (is_kernel_pmap_p ? "(kernel) " : ""), sva, eva);
	UVMHIST_LOG(pmaphist, "ptep=%p, flags(npte)=%#"PRIxPTR")",
	    ptep, flags, 0, 0);

	KASSERT(kpreempt_disabled());

	for (; sva < eva; sva += NBPG, ptep++) {
		pt_entry_t pt_entry = *ptep;
		if (!pte_valid_p(pt_entry))
			continue;
		if (is_kernel_pmap_p)
			PMAP_COUNT(remove_kernel_calls);
		else
			PMAP_COUNT(remove_user_pages);
		if (pte_wired_p(pt_entry))
			pmap->pm_stats.wired_count--;
		pmap->pm_stats.resident_count--;
		struct vm_page *pg = PHYS_TO_VM_PAGE(pte_to_paddr(pt_entry));
		if (__predict_true(pg != NULL)) {
			pmap_remove_pv(pmap, sva, pg,
			   pte_modified_p(pt_entry));
		}
		pmap_md_tlb_miss_lock_enter();
		*ptep = npte;
		/*
		 * Flush the TLB for the given address.
		 */
		pmap_tlb_invalidate_addr(pmap, sva);
		pmap_md_tlb_miss_lock_exit();
	}
	return false;
}

void
pmap_remove(pmap_t pmap, vaddr_t sva, vaddr_t eva)
{
	const bool is_kernel_pmap_p = (pmap == pmap_kernel());
	const pt_entry_t npte = pte_nv_entry(is_kernel_pmap_p);

	UVMHIST_FUNC(__func__); UVMHIST_CALLED(pmaphist);
	UVMHIST_LOG(pmaphist, "(pmap=%p, va=%#"PRIxVADDR"..%#"PRIxVADDR")",
	    pmap, sva, eva, 0);

	if (is_kernel_pmap_p)
		PMAP_COUNT(remove_kernel_calls);
	else
		PMAP_COUNT(remove_user_calls);
#ifdef PARANOIADIAG
	if (sva < pm->pm_minaddr || eva > pm->pm_maxaddr)
		panic("%s: va range %#"PRIxVADDR"-%#"PRIxVADDR" not in range",
		    __func__, sva, eva - 1);
	if (PMAP_IS_ACTIVE(pmap)) {
		struct pmap_asid_info * const pai = PMAP_PAI(pmap, curcpu());
		uint32_t asid = tlb_get_asid();
		if (asid != pai->pai_asid) {
			panic("%s: inconsistency for active TLB flush"
			    ": %d <-> %d", __func__, asid, pai->pai_asid);
		}
	}
#endif
	kpreempt_disable();
	pmap_pte_process(pmap, sva, eva, pmap_pte_remove, npte);
	kpreempt_enable();

	UVMHIST_LOG(pmaphist, "<- done", 0,0,0,0);
}

/*
 *	pmap_page_protect:
 *
 *	Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(struct vm_page *pg, vm_prot_t prot)
{
	struct vm_page_md * const mdpg = VM_PAGE_TO_MD(pg);
	pv_entry_t pv;
	vaddr_t va;

	UVMHIST_FUNC(__func__); UVMHIST_CALLED(pmaphist);
	UVMHIST_LOG(pmaphist, "(pg=%p (pa %#"PRIxPADDR") prot=%#x)",
	    pg, VM_PAGE_TO_PHYS(pg), prot, 0);
	PMAP_COUNT(page_protect);

	switch (prot) {
	case VM_PROT_READ|VM_PROT_WRITE:
	case VM_PROT_ALL:
		break;

	/* copy_on_write */
	case VM_PROT_READ:
	case VM_PROT_READ|VM_PROT_EXECUTE:
		(void)VM_PAGEMD_PVLIST_LOCK(mdpg, false);
		pv = &mdpg->mdpg_first;
		/*
		 * Loop over all current mappings setting/clearing as appropriate.
		 */
		if (pv->pv_pmap != NULL) {
			while (pv != NULL) {
				const pmap_t pmap = pv->pv_pmap;
				const uint16_t gen = VM_PAGEMD_PVLIST_GEN(mdpg);
				va = pv->pv_va;
				VM_PAGEMD_PVLIST_UNLOCK(mdpg);
				pmap_protect(pmap, va, va + PAGE_SIZE, prot);
				KASSERT(pv->pv_pmap == pmap);
				pmap_update(pmap);
				if (gen != VM_PAGEMD_PVLIST_LOCK(mdpg, false)) {
					pv = &mdpg->mdpg_first;
				} else {
					pv = pv->pv_next;
				}
			}
		}
		VM_PAGEMD_PVLIST_UNLOCK(mdpg);
		break;

	/* remove_all */
	default:
		/*
		 * Do this first so that for each unmapping, pmap_remove_pv
		 * won't try to sync the icache.
		 */
		if (pmap_page_clear_attributes(mdpg, VM_PAGEMD_EXECPAGE)) {
			UVMHIST_LOG(pmapexechist, "pg %p (pa %#"PRIxPADDR
			    "): execpage cleared", pg, VM_PAGE_TO_PHYS(pg),0,0);
			PMAP_COUNT(exec_uncached_page_protect);
		}
		(void)VM_PAGEMD_PVLIST_LOCK(mdpg, false);
		pv = &mdpg->mdpg_first;
		while (pv->pv_pmap != NULL) {
			const pmap_t pmap = pv->pv_pmap;
			va = pv->pv_va;
			VM_PAGEMD_PVLIST_UNLOCK(mdpg);
			pmap_remove(pmap, va, va + PAGE_SIZE);
			pmap_update(pmap);
			(void)VM_PAGEMD_PVLIST_LOCK(mdpg, false);
		}
		VM_PAGEMD_PVLIST_UNLOCK(mdpg);
	}

	UVMHIST_LOG(pmaphist, "<- done", 0,0,0,0);
}

static bool
pmap_pte_protect(pmap_t pmap, vaddr_t sva, vaddr_t eva, pt_entry_t *ptep,
	uintptr_t flags)
{
	const vm_prot_t prot = (flags & VM_PROT_ALL);

	UVMHIST_FUNC(__func__); UVMHIST_CALLED(pmaphist);
	UVMHIST_LOG(pmaphist, "(pmap=%p %sva=%"PRIxVADDR"..%"PRIxVADDR,
	    pmap, (pmap == pmap_kernel() ? "(kernel) " : ""), sva, eva);
	UVMHIST_LOG(pmaphist, "ptep=%p, flags(npte)=%#"PRIxPTR")",
	    ptep, flags, 0, 0);

	KASSERT(kpreempt_disabled());
	/*
	 * Change protection on every valid mapping within this segment.
	 */
	for (; sva < eva; sva += NBPG, ptep++) {
		pt_entry_t pt_entry = *ptep;
		if (!pte_valid_p(pt_entry))
			continue;
		struct vm_page * const pg =
		    PHYS_TO_VM_PAGE(pte_to_paddr(pt_entry));
		if (pg != NULL && pte_modified_p(pt_entry)) {
			struct vm_page_md * const mdpg = VM_PAGE_TO_MD(pg);
			pmap_md_vca_clean(pg, sva, PMAP_WBINV);
			if (VM_PAGEMD_EXECPAGE_P(mdpg)) {
				KASSERT(mdpg->mdpg_first.pv_pmap != NULL);
				if (pte_cached_p(pt_entry)) {
					UVMHIST_LOG(pmapexechist,
					    "pg %p (pa %#"PRIxPADDR"): %s",
					    pg, VM_PAGE_TO_PHYS(pg),
					    "syncicached performed", 0);
					pmap_page_syncicache(pg);
					PMAP_COUNT(exec_synced_protect);
				}
			}
		}
		pt_entry = pte_prot_downgrade(pt_entry, prot);
		if (*ptep != pt_entry) {
			pmap_md_tlb_miss_lock_enter();
			*ptep = pt_entry;
			/*
			 * Update the TLB if needed.
			 */
			pmap_tlb_update_addr(pmap, sva, pt_entry,
			    PMAP_TLB_NEED_IPI);
			pmap_md_tlb_miss_lock_exit();
		}
	}
	return false;
}

/*
 *	Set the physical protection on the
 *	specified range of this map as requested.
 */
void
pmap_protect(pmap_t pmap, vaddr_t sva, vaddr_t eva, vm_prot_t prot)
{

	UVMHIST_FUNC(__func__); UVMHIST_CALLED(pmaphist);
	UVMHIST_LOG(pmaphist,
	    "  pmap=%p, va=%#"PRIxVADDR"..%#"PRIxVADDR" port=%#x)",
	    pmap, sva, eva, prot);
	PMAP_COUNT(protect);

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		UVMHIST_LOG(pmaphist, "<- done", 0,0,0,0);
		return;
	}

#ifdef PARANOIADIAG
	if (sva < pm->pm_minaddr || eva > pm->pm_maxaddr)
		panic("%s: va range %#"PRIxVADDR"-%#"PRIxVADDR" not in range",
		    __func__, sva, eva - 1);
	if (PMAP_IS_ACTIVE(pmap)) {
		struct pmap_asid_info * const pai = PMAP_PAI(pmap, curcpu());
		uint32_t asid = tlb_get_asid();
		if (asid != pai->pai_asid) {
			panic("%s: inconsistency for active TLB update"
			    ": %d <-> %d", __func__, asid, pai->pai_asid);
		}
	}
#endif

	/*
	 * Change protection on every valid mapping within this segment.
	 */
	kpreempt_disable();
	pmap_pte_process(pmap, sva, eva, pmap_pte_protect, prot);
	kpreempt_enable();

	UVMHIST_LOG(pmaphist, "<- done", 0,0,0,0);
}

#if defined(__PMAP_VIRTUAL_CACHE_ALIASES)
/*
 *	pmap_page_cache:
 *
 *	Change all mappings of a managed page to cached/uncached.
 */
static void
pmap_page_cache(struct vm_page *pg, bool cached)
{
	struct vm_page_md * const mdpg = VM_PAGE_TO_MD(pg);
	UVMHIST_FUNC(__func__); UVMHIST_CALLED(pmaphist);
	UVMHIST_LOG(pmaphist, "(pg=%p (pa %#"PRIxPADDR") cached=%s)",
	    pg, VM_PAGE_TO_PHYS(pg), cached ? "true" : "false", 0);
	KASSERT(kpreempt_disabled());

	if (cached) {
		pmap_page_clear_attributes(mdpg, VM_PAGEMD_UNCACHED);
		PMAP_COUNT(page_cache_restorations);
	} else {
		pmap_page_set_attributes(mdpg, VM_PAGEMD_UNCACHED);
		PMAP_COUNT(page_cache_evictions);
	}

	KASSERT(VM_PAGEMD_PVLIST_LOCKED_P(mdpg));
	KASSERT(kpreempt_disabled());
	for (pv_entry_t pv = &mdpg->mdpg_first;
	     pv != NULL;
	     pv = pv->pv_next) {
		pmap_t pmap = pv->pv_pmap;
		vaddr_t va = pv->pv_va;

		KASSERT(pmap != NULL);
		KASSERT(pmap != pmap_kernel() || !pmap_md_direct_mapped_vaddr_p(va));
		pt_entry_t * const ptep = pmap_pte_lookup(pmap, va);
		if (ptep == NULL)
			continue;
		pt_entry_t pt_entry = *ptep;
		if (pte_valid_p(pt_entry)) {
			pt_entry = pte_cached_change(pt_entry, cached);
			pmap_md_tlb_miss_lock_enter();
			*ptep = pt_entry;
			pmap_tlb_update_addr(pmap, va, pt_entry,
			    PMAP_TLB_NEED_IPI);
			pmap_md_tlb_miss_lock_exit();
		}
	}
	UVMHIST_LOG(pmaphist, "<- done", 0,0,0,0);
}
#endif	/* __PMAP_VIRTUAL_CACHE_ALIASES */

/*
 *	Insert the given physical page (p) at
 *	the specified virtual address (v) in the
 *	target physical map with the protection requested.
 *
 *	If specified, the page will be wired down, meaning
 *	that the related pte can not be reclaimed.
 *
 *	NB:  This is the only routine which MAY NOT lazy-evaluate
 *	or lose information.  That is, this routine must actually
 *	insert this page into the given map NOW.
 */
int
pmap_enter(pmap_t pmap, vaddr_t va, paddr_t pa, vm_prot_t prot, u_int flags)
{
	pt_entry_t npte;
	const bool wired = (flags & PMAP_WIRED) != 0;
	const bool is_kernel_pmap_p = (pmap == pmap_kernel());
#ifdef UVMHIST
	struct kern_history * const histp = 
	    ((prot & VM_PROT_EXECUTE) ? &pmapexechist : &pmaphist);
#endif

	UVMHIST_FUNC(__func__);
#define VM_PROT_STRING(prot) \
	&"\0    (R)\0  (W)\0  (RW)\0 (X)\0  (RX)\0 (WX)\0 (RWX)\0"[UVM_PROTECTION(prot)*6]
	UVMHIST_CALLED(*histp);
	UVMHIST_LOG(*histp, "(pmap=%p, va=%#"PRIxVADDR", pa=%#"PRIxPADDR,
	    pmap, va, pa, 0);
	UVMHIST_LOG(*histp, "prot=%#x%s flags=%#x%s)",
	    prot, VM_PROT_STRING(prot), flags, VM_PROT_STRING(flags));

	const bool good_color = PMAP_PAGE_COLOROK_P(pa, va);
	if (is_kernel_pmap_p) {
		PMAP_COUNT(kernel_mappings);
		if (!good_color)
			PMAP_COUNT(kernel_mappings_bad);
	} else {
		PMAP_COUNT(user_mappings);
		if (!good_color)
			PMAP_COUNT(user_mappings_bad);
	}
#if defined(DEBUG) || defined(DIAGNOSTIC) || defined(PARANOIADIAG)
	if (va < pmap->pm_minaddr || va >= pmap->pm_maxaddr)
		panic("%s: %s %#"PRIxVADDR" too big",
		    __func__, is_kernel_pmap_p ? "kva" : "uva", va);
#endif

	KASSERTMSG(prot & VM_PROT_READ,
	    "%s: no READ (%#x) in prot %#x", __func__, VM_PROT_READ, prot);

	struct vm_page * const pg = PHYS_TO_VM_PAGE(pa);
	struct vm_page_md *mdpg;

	if (pg) {
		mdpg = VM_PAGE_TO_MD(pg);
		/* Set page referenced/modified status based on flags */
		if (flags & VM_PROT_WRITE)
			pmap_page_set_attributes(mdpg, VM_PAGEMD_MODIFIED|VM_PAGEMD_REFERENCED);
		else if (flags & VM_PROT_ALL)
			pmap_page_set_attributes(mdpg, VM_PAGEMD_REFERENCED);

#ifdef __PMAP_VIRTUAL_CACHE_ALIASES
		if (!VM_PAGEMD_CACHED(pg))
			flags |= PMAP_NOCACHE;
#endif

		PMAP_COUNT(managed_mappings);
	} else {
		/*
		 * Assumption: if it is not part of our managed memory
		 * then it must be device memory which may be volatile.
		 */
		mdpg = NULL;
		flags |= PMAP_NOCACHE;
		PMAP_COUNT(unmanaged_mappings);
	}

	npte = pte_make_enter(pa, mdpg, prot, flags, is_kernel_pmap_p);

	kpreempt_disable();
	pt_entry_t * const ptep = pmap_pte_reserve(pmap, va, flags);
	if (__predict_false(ptep == NULL)) {
		kpreempt_enable();
		UVMHIST_LOG(*histp, "<- ENOMEM", 0,0,0,0);
		return ENOMEM;
	}
	pt_entry_t opte = *ptep;

	/* Done after case that may sleep/return. */
	if (pg)
		pmap_enter_pv(pmap, va, pg, &npte);

	/*
	 * Now validate mapping with desired protection/wiring.
	 * Assume uniform modified and referenced status for all
	 * MIPS pages in a MACH page.
	 */
	if (wired) {
		pmap->pm_stats.wired_count++;
		npte = pte_wire_entry(npte);
	}

	UVMHIST_LOG(*histp, "new pte %#x (pa %#"PRIxPADDR")", npte, pa, 0,0);

	if (pte_valid_p(opte) && pte_to_paddr(opte) != pa) {
		pmap_remove(pmap, va, va + NBPG);
		PMAP_COUNT(user_mappings_changed);
	}

	KASSERT(pte_valid_p(npte));
	bool resident = pte_valid_p(opte);
	if (!resident)
		pmap->pm_stats.resident_count++;
	pmap_md_tlb_miss_lock_enter();
	*ptep = npte;

	pmap_tlb_update_addr(pmap, va, npte,
	    ((flags & VM_PROT_ALL) ? PMAP_TLB_INSERT : 0)
	    | (resident ? PMAP_TLB_NEED_IPI : 0));
	pmap_md_tlb_miss_lock_exit();
	kpreempt_enable();

	if (pg != NULL && (prot == (VM_PROT_READ | VM_PROT_EXECUTE))) {
		KASSERT(mdpg != NULL);
		PMAP_COUNT(exec_mappings);
		if (!VM_PAGEMD_EXECPAGE_P(mdpg) && pte_cached_p(npte)) {
			if (!pte_deferred_exec_p(npte)) {
				UVMHIST_LOG(*histp,
				    "va=%#"PRIxVADDR" pg %p: %s syncicache%s",
				    va, pg, "immediate", "");
				pmap_page_syncicache(pg);
				pmap_page_set_attributes(mdpg,
				    VM_PAGEMD_EXECPAGE);
				PMAP_COUNT(exec_synced_mappings);
			} else {
				UVMHIST_LOG(*histp, "va=%#"PRIxVADDR
				    " pg %p: %s syncicache: pte %#x",
				    va, pg, "defer", npte);
			}
		} else {
			UVMHIST_LOG(*histp,
			    "va=%#"PRIxVADDR" pg %p: %s syncicache%s",
			    va, pg, "no",
			    (pte_cached_p(npte)
				? " (already exec)"
				: " (uncached)"));
		}
	} else if (pg != NULL && (prot & VM_PROT_EXECUTE)) {
		KASSERT(mdpg != NULL);
		KASSERT(prot & VM_PROT_WRITE);
		PMAP_COUNT(exec_mappings);
		pmap_page_syncicache(pg);
		pmap_page_clear_attributes(mdpg, VM_PAGEMD_EXECPAGE);
		UVMHIST_LOG(pmapexechist,
		    "va=%#"PRIxVADDR" pg %p: %s syncicache%s",
		    va, pg, "immediate", " (writeable)");
	}

	if (prot & VM_PROT_EXECUTE) {
		UVMHIST_LOG(pmapexechist, "<- 0 (OK)", 0,0,0,0);
	} else {
		UVMHIST_LOG(pmaphist, "<- 0 (OK)", 0,0,0,0);
	}
	return 0;
}

void
pmap_kenter_pa(vaddr_t va, paddr_t pa, vm_prot_t prot, u_int flags)
{
	struct vm_page * const pg = PHYS_TO_VM_PAGE(pa);
	struct vm_page_md *mdpg;

	UVMHIST_FUNC(__func__); UVMHIST_CALLED(pmaphist);
	UVMHIST_LOG(pmaphist, "(va=%#"PRIxVADDR" pa=%#"PRIxPADDR
	    ", prot=%#x, flags=%#x)", va, pa, prot, flags);
	PMAP_COUNT(kenter_pa);

	if (pg == NULL) {
		mdpg = NULL;
		PMAP_COUNT(kenter_pa_unmanaged);
		flags |= PMAP_NOCACHE;
	} else {
		mdpg = VM_PAGE_TO_MD(pg);
	}

	if ((flags & PMAP_NOCACHE) == 0 && !PMAP_PAGE_COLOROK_P(pa, va))
		PMAP_COUNT(kenter_pa_bad);

	const pt_entry_t npte = pte_make_kenter_pa(pa, mdpg, prot, flags);
	kpreempt_disable();
	pt_entry_t * const ptep = pmap_pte_reserve(pmap_kernel(), va, 0);
	KASSERT(ptep != NULL);
	KASSERT(!pte_valid_p(*ptep));
	pmap_md_tlb_miss_lock_enter();
	*ptep = npte;
	/*
	 * We have the option to force this mapping into the TLB but we
	 * don't.  Instead let the next reference to the page do it.
	 */
	pmap_tlb_update_addr(pmap_kernel(), va, npte, 0);
	pmap_md_tlb_miss_lock_exit();
	kpreempt_enable();
#if DEBUG > 1
	for (u_int i = 0; i < PAGE_SIZE / sizeof(long); i++) {
		if (((long *)va)[i] != ((long *)pa)[i])
			panic("%s: contents (%lx) of va %#"PRIxVADDR
			    " != contents (%lx) of pa %#"PRIxPADDR, __func__,
			    ((long *)va)[i], va, ((long *)pa)[i], pa);
	}
#endif
	UVMHIST_LOG(pmaphist, "<- done", 0,0,0,0);
}

static bool
pmap_pte_kremove(pmap_t pmap, vaddr_t sva, vaddr_t eva, pt_entry_t *ptep,
	uintptr_t flags)
{
	const pt_entry_t new_pt_entry = pte_nv_entry(true);

	KASSERT(kpreempt_disabled());

	/*
	 * Set every pt on every valid mapping within this segment.
	 */
	for (; sva < eva; sva += NBPG, ptep++) {
		pt_entry_t pt_entry = *ptep;
		if (!pte_valid_p(pt_entry)) {
			continue;
		}

		PMAP_COUNT(kremove_pages);
		struct vm_page * const pg =
		    PHYS_TO_VM_PAGE(pte_to_paddr(pt_entry));
		if (pg != NULL)
			pmap_md_vca_clean(pg, sva, PMAP_WBINV);

		pmap_md_tlb_miss_lock_enter();
		*ptep = new_pt_entry;
		pmap_tlb_invalidate_addr(pmap_kernel(), sva);
		pmap_md_tlb_miss_lock_exit();
	}

	return false;
}

void
pmap_kremove(vaddr_t va, vsize_t len)
{
	const vaddr_t sva = trunc_page(va);
	const vaddr_t eva = round_page(va + len);

	UVMHIST_FUNC(__func__); UVMHIST_CALLED(pmaphist);
	UVMHIST_LOG(pmaphist, "(va=%#"PRIxVADDR" len=%#"PRIxVSIZE")",
	    va, len, 0,0);

	kpreempt_disable();
	pmap_pte_process(pmap_kernel(), sva, eva, pmap_pte_kremove, 0);
	kpreempt_enable();

	UVMHIST_LOG(pmaphist, "<- done", 0,0,0,0);
}

void
pmap_remove_all(struct pmap *pmap)
{
	KASSERT(pmap != pmap_kernel());

	kpreempt_disable();
	/*
	 * Free all of our ASIDs which means we can skip doing all the
	 * tlb_invalidate_addrs().
	 */
	pmap_md_tlb_miss_lock_enter();
	pmap_tlb_asid_deactivate(pmap);
	pmap_tlb_asid_release_all(pmap);
	pmap_md_tlb_miss_lock_exit();
	pmap->pm_flags |= PMAP_DEFERRED_ACTIVATE;

	kpreempt_enable();
}

/*
 *	Routine:	pmap_unwire
 *	Function:	Clear the wired attribute for a map/virtual-address
 *			pair.
 *	In/out conditions:
 *			The mapping must already exist in the pmap.
 */
void
pmap_unwire(pmap_t pmap, vaddr_t va)
{

	UVMHIST_FUNC(__func__); UVMHIST_CALLED(pmaphist);
	UVMHIST_LOG(pmaphist, "(pmap=%p va=%#"PRIxVADDR")", pmap, va, 0,0);
	PMAP_COUNT(unwire);

	/*
	 * Don't need to flush the TLB since PG_WIRED is only in software.
	 */
#ifdef PARANOIADIAG
	if (va < pmap->pm_minaddr || pmap->pm_maxaddr <= va)
		panic("pmap_unwire");
#endif
	kpreempt_disable();
	pt_entry_t * const ptep = pmap_pte_lookup(pmap, va);
	pt_entry_t pt_entry = *ptep;
#ifdef DIAGNOSTIC
	if (ptep == NULL)
		panic("%s: pmap %p va %#"PRIxVADDR" invalid STE",
		    __func__, pmap, va);
#endif

#ifdef DIAGNOSTIC
	if (!pte_valid_p(pt_entry))
		panic("pmap_unwire: pmap %p va %#"PRIxVADDR" invalid PTE",
		    pmap, va);
#endif

	if (pte_wired_p(pt_entry)) {
		pmap_md_tlb_miss_lock_enter();
		*ptep = pte_unwire_entry(*ptep);
		pmap_md_tlb_miss_lock_exit();
		pmap->pm_stats.wired_count--;
	}
#ifdef DIAGNOSTIC
	else {
		printf("%s: wiring for pmap %p va %#"PRIxVADDR" unchanged!\n",
		    __func__, pmap, va);
	}
#endif
	kpreempt_enable();
}

/*
 *	Routine:	pmap_extract
 *	Function:
 *		Extract the physical page address associated
 *		with the given map/virtual_address pair.
 */
bool
pmap_extract(pmap_t pmap, vaddr_t va, paddr_t *pap)
{
	paddr_t pa;

	//UVMHIST_FUNC(__func__); UVMHIST_CALLED(pmaphist);
	//UVMHIST_LOG(pmaphist, "(pmap=%p va=%#"PRIxVADDR")", pmap, va, 0,0);
	if (pmap == pmap_kernel()) {
		if (pmap_md_direct_mapped_vaddr_p(va)) {
			pa = pmap_md_direct_mapped_vaddr_to_paddr(va);
			goto done;
		}
		if (pmap_md_io_vaddr_p(va))
			panic("pmap_extract: io address %#"PRIxVADDR"", va);
	}
	kpreempt_disable();
	pt_entry_t * const ptep = pmap_pte_lookup(pmap, va);
	if (ptep == NULL) {
		//UVMHIST_LOG(pmaphist, "<- false (not in segmap)", 0,0,0,0);
		kpreempt_enable();
		return false;
	}
	if (!pte_valid_p(*ptep)) {
		//UVMHIST_LOG(pmaphist, "<- false (PTE not valid)", 0,0,0,0);
		kpreempt_enable();
		return false;
	}
	pa = pte_to_paddr(*ptep) | (va & PGOFSET);
	kpreempt_enable();
done:
	if (pap != NULL) {
		*pap = pa;
	}
	//UVMHIST_LOG(pmaphist, "<- true (pa %#"PRIxPADDR")", pa, 0,0,0);
	return true;
}

/*
 *	Copy the range specified by src_addr/len
 *	from the source map to the range dst_addr/len
 *	in the destination map.
 *
 *	This routine is only advisory and need not do anything.
 */
void
pmap_copy(pmap_t dst_pmap, pmap_t src_pmap, vaddr_t dst_addr, vsize_t len,
    vaddr_t src_addr)
{

	UVMHIST_FUNC(__func__); UVMHIST_CALLED(pmaphist);
	PMAP_COUNT(copy);
}

/*
 *	pmap_clear_reference:
 *
 *	Clear the reference bit on the specified physical page.
 */
bool
pmap_clear_reference(struct vm_page *pg)
{
	struct vm_page_md * const mdpg = VM_PAGE_TO_MD(pg);

	UVMHIST_FUNC(__func__); UVMHIST_CALLED(pmaphist);
	UVMHIST_LOG(pmaphist, "(pg=%p (pa %#"PRIxPADDR"))",
	   pg, VM_PAGE_TO_PHYS(pg), 0,0);

	bool rv = pmap_page_clear_attributes(mdpg, VM_PAGEMD_REFERENCED);

	UVMHIST_LOG(pmaphist, "<- %s", rv ? "true" : "false", 0,0,0);

	return rv;
}

/*
 *	pmap_is_referenced:
 *
 *	Return whether or not the specified physical page is referenced
 *	by any physical maps.
 */
bool
pmap_is_referenced(struct vm_page *pg)
{

	return VM_PAGEMD_REFERENCED_P(VM_PAGE_TO_MD(pg));
}

/*
 *	Clear the modify bits on the specified physical page.
 */
bool
pmap_clear_modify(struct vm_page *pg)
{
	struct vm_page_md * const mdpg = VM_PAGE_TO_MD(pg);
	pv_entry_t pv = &mdpg->mdpg_first;
	pv_entry_t pv_next;
	uint16_t gen;

	UVMHIST_FUNC(__func__); UVMHIST_CALLED(pmaphist);
	UVMHIST_LOG(pmaphist, "(pg=%p (%#"PRIxPADDR"))",
	    pg, VM_PAGE_TO_PHYS(pg), 0,0);
	PMAP_COUNT(clear_modify);

	if (VM_PAGEMD_EXECPAGE_P(mdpg)) {
		if (pv->pv_pmap == NULL) {
			UVMHIST_LOG(pmapexechist,
			    "pg %p (pa %#"PRIxPADDR"): %s",
			    pg, VM_PAGE_TO_PHYS(pg), "execpage cleared", 0);
			pmap_page_clear_attributes(mdpg, VM_PAGEMD_EXECPAGE);
			PMAP_COUNT(exec_uncached_clear_modify);
		} else {
			UVMHIST_LOG(pmapexechist,
			    "pg %p (pa %#"PRIxPADDR"): %s",
			    pg, VM_PAGE_TO_PHYS(pg), "syncicache performed", 0);
			pmap_page_syncicache(pg);
			PMAP_COUNT(exec_synced_clear_modify);
		}
	}
	if (!pmap_page_clear_attributes(mdpg, VM_PAGEMD_MODIFIED)) {
		UVMHIST_LOG(pmaphist, "<- false", 0,0,0,0);
		return false;
	}
	if (pv->pv_pmap == NULL) {
		UVMHIST_LOG(pmaphist, "<- true (no mappings)", 0,0,0,0);
		return true;
	}

	/*
	 * remove write access from any pages that are dirty
	 * so we can tell if they are written to again later.
	 * flush the VAC first if there is one.
	 */
	kpreempt_disable();
	gen = VM_PAGEMD_PVLIST_LOCK(mdpg, false);
	for (; pv != NULL; pv = pv_next) {
		pmap_t pmap = pv->pv_pmap;
		vaddr_t va = pv->pv_va;
		pt_entry_t * const ptep = pmap_pte_lookup(pmap, va);
		KASSERT(ptep);
		pv_next = pv->pv_next;
		pt_entry_t pt_entry = pte_prot_nowrite(*ptep);
		if (*ptep == pt_entry) {
			continue;
		}
		pmap_md_vca_clean(pg, va, PMAP_WBINV);
		pmap_md_tlb_miss_lock_enter();
		*ptep = pt_entry;
		VM_PAGEMD_PVLIST_UNLOCK(mdpg);
		pmap_tlb_invalidate_addr(pmap, va);
		pmap_md_tlb_miss_lock_exit();
		pmap_update(pmap);
		if (__predict_false(gen != VM_PAGEMD_PVLIST_LOCK(mdpg, false))) {
			/*
			 * The list changed!  So restart from the beginning.
			 */
			pv_next = &mdpg->mdpg_first;
		}
	}
	VM_PAGEMD_PVLIST_UNLOCK(mdpg);
	kpreempt_enable();

	UVMHIST_LOG(pmaphist, "<- true (mappings changed)", 0,0,0,0);
	return true;
}

/*
 *	pmap_is_modified:
 *
 *	Return whether or not the specified physical page is modified
 *	by any physical maps.
 */
bool
pmap_is_modified(struct vm_page *pg)
{

	return VM_PAGEMD_MODIFIED_P(VM_PAGE_TO_MD(pg));
}

/*
 *	pmap_set_modified:
 *
 *	Sets the page modified reference bit for the specified page.
 */
void
pmap_set_modified(paddr_t pa)
{
	struct vm_page * const pg = PHYS_TO_VM_PAGE(pa);
	struct vm_page_md * const mdpg = VM_PAGE_TO_MD(pg);
	pmap_page_set_attributes(mdpg, VM_PAGEMD_MODIFIED|VM_PAGEMD_REFERENCED);
}

/******************** pv_entry management ********************/

static void
pmap_check_pvlist(struct vm_page *pg)
{
#ifdef PARANOIADIAG
	struct vm_page_md * const mdpg = VM_PAGE_TO_MD(pg);
	pt_entry_t pv = &mdpg->mdpg_first;
	if (pv->pv_pmap != NULL) {
		for (; pv != NULL; pv = pv->pv_next) {
			KASSERT(!pmap_md_direct_mapped_vaddr_p(pv->pv_va));
		}
	}
#endif /* PARANOIADIAG */
}

/*
 * Enter the pmap and virtual address into the
 * physical to virtual map table.
 */
void
pmap_enter_pv(pmap_t pmap, vaddr_t va, struct vm_page *pg, u_int *npte)
{
	struct vm_page_md * const mdpg = VM_PAGE_TO_MD(pg);
	pv_entry_t pv, npv, apv;
	int16_t gen;
	bool first __unused = false;

	UVMHIST_FUNC(__func__); UVMHIST_CALLED(pmaphist);
	UVMHIST_LOG(pmaphist,
	    "(pmap=%p va=%#"PRIxVADDR" pg=%p (%#"PRIxPADDR")",
	    pmap, va, pg, VM_PAGE_TO_PHYS(pg));
	UVMHIST_LOG(pmaphist, "nptep=%p (%#x))", npte, *npte, 0, 0);

	KASSERT(kpreempt_disabled());
	KASSERT(pmap != pmap_kernel() || !pmap_md_direct_mapped_vaddr_p(va));

	apv = NULL;
	pv = &mdpg->mdpg_first;
	gen = VM_PAGEMD_PVLIST_LOCK(mdpg, true);
	pmap_check_pvlist(pg);
again:
	if (pv->pv_pmap == NULL) {
		KASSERT(pv->pv_next == NULL);
		/*
		 * No entries yet, use header as the first entry
		 */
		PMAP_COUNT(primary_mappings);
		PMAP_COUNT(mappings);
		first = true;
#ifdef __PMAP_VIRTUAL_CACHE_ALIASES
		pmap_page_clear_attributes(pg, VM_PAGEMD_UNCACHED);
#endif
		pv->pv_pmap = pmap;
		pv->pv_va = va;
	} else {
		if (pmap_md_vca_add(pg, va, npte))
			goto again;

		/*
		 * There is at least one other VA mapping this page.
		 * Place this entry after the header.
		 *
		 * Note: the entry may already be in the table if
		 * we are only changing the protection bits.
		 */

#ifdef PARANOIADIAG
		const paddr_t pa = VM_PAGE_TO_PHYS(pg);
#endif
		for (npv = pv; npv; npv = npv->pv_next) {
			if (pmap == npv->pv_pmap && va == npv->pv_va) {
#ifdef PARANOIADIAG
				pt_entry_t *ptep = pmap_pte_lookup(pmap, va);
				pt_entry_t pt_entry = (ptep ? *ptep : 0);
				if (!pte_valid_p(pt_entry)
				    || pte_to_paddr(pt_entry) != pa)
					printf(
		"pmap_enter_pv: found va %#"PRIxVADDR" pa %#"PRIxPADDR" in pv_table but != %x\n",
					    va, pa, pt_entry);
#endif
				PMAP_COUNT(remappings);
				VM_PAGEMD_PVLIST_UNLOCK(mdpg);
				if (__predict_false(apv != NULL))
					pmap_pv_free(apv);
				return;
			}
		}
		if (__predict_true(apv == NULL)) {
			/*
			 * To allocate a PV, we have to release the PVLIST lock
			 * so get the page generation.  We allocate the PV, and
			 * then reacquire the lock.  
			 */
			VM_PAGEMD_PVLIST_UNLOCK(mdpg);

			apv = (pv_entry_t)pmap_pv_alloc();
			if (apv == NULL)
				panic("pmap_enter_pv: pmap_pv_alloc() failed");

			/*
			 * If the generation has changed, then someone else
			 * tinkered with this page so we should
			 * start over.
			 */
			uint16_t oldgen = gen;
			gen = VM_PAGEMD_PVLIST_LOCK(mdpg, true);
			if (gen != oldgen)
				goto again;
		}
		npv = apv;
		apv = NULL;
		npv->pv_va = va;
		npv->pv_pmap = pmap;
		npv->pv_next = pv->pv_next;
		pv->pv_next = npv;
		PMAP_COUNT(mappings);
	}
	pmap_check_pvlist(pg);
	VM_PAGEMD_PVLIST_UNLOCK(mdpg);
	if (__predict_false(apv != NULL))
		pmap_pv_free(apv);

	UVMHIST_LOG(pmaphist, "<- done pv=%p%s",
	    pv, first ? " (first pv)" : "",0,0);
}

/*
 * Remove a physical to virtual address translation.
 * If cache was inhibited on this page, and there are no more cache
 * conflicts, restore caching.
 * Flush the cache if the last page is removed (should always be cached
 * at this point).
 */
void
pmap_remove_pv(pmap_t pmap, vaddr_t va, struct vm_page *pg, bool dirty)
{
	struct vm_page_md * const mdpg = VM_PAGE_TO_MD(pg);
	pv_entry_t pv, npv;
	bool last;

	UVMHIST_FUNC(__func__); UVMHIST_CALLED(pmaphist);
	UVMHIST_LOG(pmaphist,
	    "(pmap=%p va=%#"PRIxVADDR" pg=%p (pa %#"PRIxPADDR")\n",
	    pmap, va, pg, VM_PAGE_TO_PHYS(pg));
	UVMHIST_LOG(pmaphist, "dirty=%s)", dirty ? "true" : "false", 0,0,0);

	KASSERT(kpreempt_disabled());
	pv = &mdpg->mdpg_first;

	(void)VM_PAGEMD_PVLIST_LOCK(mdpg, true);
	pmap_check_pvlist(pg);

	/*
	 * If it is the first entry on the list, it is actually
	 * in the header and we must copy the following entry up
	 * to the header.  Otherwise we must search the list for
	 * the entry.  In either case we free the now unused entry.
	 */

	last = false;
	if (pmap == pv->pv_pmap && va == pv->pv_va) {
		npv = pv->pv_next;
		if (npv) {
			*pv = *npv;
			KASSERT(pv->pv_pmap != NULL);
		} else {
#ifdef __PMAP_VIRTUAL_CACHE_ALIASES
			pmap_page_clear_attributes(pg, VM_PAGEMD_UNCACHED);
#endif
			pv->pv_pmap = NULL;
			last = true;	/* Last mapping removed */
		}
		PMAP_COUNT(remove_pvfirst);
	} else {
		for (npv = pv->pv_next; npv; pv = npv, npv = npv->pv_next) {
			PMAP_COUNT(remove_pvsearch);
			if (pmap == npv->pv_pmap && va == npv->pv_va)
				break;
		}
		if (npv) {
			pv->pv_next = npv->pv_next;
		}
	}
	pmap_md_vca_remove(pg, va);

	pmap_check_pvlist(pg);
	VM_PAGEMD_PVLIST_UNLOCK(mdpg);

	/*
	 * Free the pv_entry if needed.
	 */
	if (npv)
		pmap_pv_free(npv);
	if (VM_PAGEMD_EXECPAGE_P(mdpg) && dirty) {
		if (last) {
			/*
			 * If this was the page's last mapping, we no longer
			 * care about its execness.
			 */
			UVMHIST_LOG(pmapexechist,
			    "pg %p (pa %#"PRIxPADDR")%s: %s",
			    pg, VM_PAGE_TO_PHYS(pg),
			    last ? " [last mapping]" : "",
			    "execpage cleared");
			pmap_page_clear_attributes(mdpg, VM_PAGEMD_EXECPAGE);
			PMAP_COUNT(exec_uncached_remove);
		} else {
			/*
			 * Someone still has it mapped as an executable page
			 * so we must sync it.
			 */
			UVMHIST_LOG(pmapexechist,
			    "pg %p (pa %#"PRIxPADDR")%s: %s",
			    pg, VM_PAGE_TO_PHYS(pg),
			    last ? " [last mapping]" : "",
			    "performed syncicache");
			pmap_page_syncicache(pg);
			PMAP_COUNT(exec_synced_remove);
		}
	}
	UVMHIST_LOG(pmaphist, "<- done", 0,0,0,0);
}

#if defined(MULTIPROCESSOR)
struct pmap_pvlist_info {
	kmutex_t *pli_locks[PAGE_SIZE / 32];
	volatile u_int pli_lock_refs[PAGE_SIZE / 32];
	volatile u_int pli_lock_index;
	u_int pli_lock_mask;
} pmap_pvlist_info;

void
pmap_pvlist_lock_init(size_t cache_line_size)
{
	struct pmap_pvlist_info * const pli = &pmap_pvlist_info;
	const vaddr_t lock_page = uvm_pageboot_alloc(PAGE_SIZE);
	vaddr_t lock_va = lock_page;
	if (sizeof(kmutex_t) > cache_line_size) {
		cache_line_size = roundup2(sizeof(kmutex_t), cache_line_size);
	}
	const size_t nlocks = PAGE_SIZE / cache_line_size;
	KASSERT((nlocks & (nlocks - 1)) == 0);
	/*
	 * Now divide the page into a number of mutexes, one per cacheline.
	 */
	for (size_t i = 0; i < nlocks; lock_va += cache_line_size, i++) {
		kmutex_t * const lock = (kmutex_t *)lock_va;
		mutex_init(lock, MUTEX_DEFAULT, IPL_VM);
		pli->pli_locks[i] = lock;
	}
	pli->pli_lock_mask = nlocks - 1;
}

uint16_t
pmap_pvlist_lock(struct vm_page_md *mdpg, bool list_change)
{
	struct pmap_pvlist_info * const pli = &pmap_pvlist_info;
	kmutex_t *lock = mdpg->mdpg_lock;
	int16_t gen;

	/*
	 * Allocate a lock on an as-needed basis.  This will hopefully give us
	 * semi-random distribution not based on page color.
	 */
	if (__predict_false(lock == NULL)) {
		size_t locknum = atomic_add_int_nv(&pli->pli_lock_index, 37);
		size_t lockid = locknum & pli->pli_lock_mask;
		kmutex_t * const new_lock = pli->pli_locks[lockid];
		/*
		 * Set the lock.  If some other thread already did, just use
		 * the one they assigned.
		 */
		lock = atomic_cas_ptr(&mdpg->mdpg_lock, NULL, new_lock);
		if (lock == NULL) {
			lock = new_lock;
			atomic_inc_uint(&pli->pli_lock_refs[lockid]);
		}
	}

	/*
	 * Now finally lock the pvlists.
	 */
	mutex_spin_enter(lock);

	/*
	 * If the locker will be changing the list, increment the high 16 bits
	 * of attrs so we use that as a generation number.
	 */
	gen = VM_PAGEMD_PVLIST_GEN(mdpg);		/* get old value */
	if (list_change)
		atomic_add_int(&mdpg->mdpg_attrs, 0x10000);

	/*
	 * Return the generation number.
	 */
	return gen;
}
#else /* !MULTIPROCESSOR */
void
pmap_pvlist_lock_init(size_t cache_line_size)
{
	mutex_init(&pmap_pvlist_mutex, MUTEX_DEFAULT, IPL_VM);
}

#ifdef MODULAR
uint16_t
pmap_pvlist_lock(struct vm_page_md *mdpg, bool list_change)
{
	/*
	 * We just use a global lock.
	 */
	if (__predict_false(mdpg->mdpg_lock == NULL)) {
		mdpg->mdpg_lock = &pmap_pvlist_mutex;
	}

	/*
	 * Now finally lock the pvlists.
	 */
	mutex_spin_enter(mdpg->mdpg_lock);

	return 0;
}
#endif /* MODULAR */
#endif /* !MULTIPROCESSOR */

/*
 * pmap_pv_page_alloc:
 *
 *	Allocate a page for the pv_entry pool.
 */
void *
pmap_pv_page_alloc(struct pool *pp, int flags)
{
	struct vm_page *pg = PMAP_ALLOC_POOLPAGE(UVM_PGA_USERESERVE);
	if (pg == NULL)
		return NULL;

	return (void *)pmap_map_poolpage(VM_PAGE_TO_PHYS(pg));
}

/*
 * pmap_pv_page_free:
 *
 *	Free a pv_entry pool page.
 */
void
pmap_pv_page_free(struct pool *pp, void *v)
{
	vaddr_t va = (vaddr_t)v;

	KASSERT(pmap_md_direct_mapped_vaddr_p(va));
	const paddr_t pa = pmap_md_direct_mapped_vaddr_to_paddr(va);
	struct vm_page * const pg = PHYS_TO_VM_PAGE(pa);
	struct vm_page_md * const mdpg = VM_PAGE_TO_MD(pg);
	pmap_md_vca_remove(pg, va);
	pmap_page_clear_attributes(mdpg, VM_PAGEMD_POOLPAGE);
	uvm_pagefree(pg);
}

#ifdef PMAP_PREFER
/*
 * Find first virtual address >= *vap that doesn't cause
 * a cache alias conflict.
 */
void
pmap_prefer(vaddr_t foff, vaddr_t *vap, vsize_t sz, int td)
{
	vaddr_t	va;
	vsize_t d;
	vsize_t prefer_mask = ptoa(uvmexp.colormask);

	PMAP_COUNT(prefer_requests);

	prefer_mask |= pmap_md_cache_prefer_mask();

	if (prefer_mask) {
		va = *vap;

		d = foff - va;
		d &= prefer_mask;
		if (d) {
			if (td)
				*vap = trunc_page(va -((-d) & prefer_mask));
			else
				*vap = round_page(va + d);
			PMAP_COUNT(prefer_adjustments);
		}
	}
}
#endif /* PMAP_PREFER */

#ifdef PMAP_MAP_POOLPAGE
vaddr_t
pmap_map_poolpage(paddr_t pa)
{

	struct vm_page * const pg = PHYS_TO_VM_PAGE(pa);
	KASSERT(pg);
	struct vm_page_md * const mdpg = VM_PAGE_TO_MD(pg);
	pmap_page_set_attributes(mdpg, VM_PAGEMD_POOLPAGE);

	const vaddr_t va = pmap_md_map_poolpage(pa, NBPG);
	pmap_md_vca_add(pg, va, NULL);
	return va;
}

paddr_t
pmap_unmap_poolpage(vaddr_t va)
{

	KASSERT(pmap_md_direct_mapped_vaddr_p(va));
	paddr_t pa = pmap_md_direct_mapped_vaddr_to_paddr(va);

	struct vm_page * const pg = PHYS_TO_VM_PAGE(pa);
	KASSERT(pg);
	struct vm_page_md * const mdpg = VM_PAGE_TO_MD(pg);
	pmap_page_clear_attributes(mdpg, VM_PAGEMD_POOLPAGE);
	pmap_md_unmap_poolpage(va, NBPG);
	pmap_md_vca_remove(pg, va);

	return pa;
}
#endif /* PMAP_MAP_POOLPAGE */

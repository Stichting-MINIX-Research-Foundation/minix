/*	$NetBSD: subr_physmap.c,v 1.2 2013/01/19 01:04:51 rmind Exp $	*/

/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(1, "$NetBSD: subr_physmap.c,v 1.2 2013/01/19 01:04:51 rmind Exp $");

#include <sys/param.h>
#include <sys/physmap.h>
#include <sys/kmem.h>

#include <dev/mm.h>

/*
 * This file contain support routines used to create and destroy lists of
 * physical pages from lists of pages or ranges of virtual address.  By using
 * these physical maps, the kernel can avoid mapping physical I/O in the 
 * kernel's address space in most cases.
 */

typedef struct {
	physmap_t *pc_physmap;
	physmap_segment_t *pc_segs;
	vsize_t pc_offset;
	vsize_t pc_klen;
	vaddr_t pc_kva;
	u_int pc_nsegs;
	vm_prot_t pc_prot;
	bool pc_direct_mapped;
} physmap_cookie_t;

/*
 * Allocate a physmap structure that requires "maxsegs" segments.
 */
static physmap_t *
physmap_alloc(size_t maxsegs)
{
	const size_t mapsize = offsetof(physmap_t, pm_segs[maxsegs]);

	KASSERT(maxsegs > 0);

	physmap_t * const map = kmem_zalloc(mapsize, KM_SLEEP);
	map->pm_maxsegs = maxsegs;

	return map;
}

static int
physmap_fill(physmap_t *map, pmap_t pmap, vaddr_t va, vsize_t len)
{
	size_t nsegs = map->pm_nsegs;
	physmap_segment_t *ps = &map->pm_segs[nsegs];
	vsize_t offset = va - trunc_page(va);

	if (nsegs == 0) {
		if (!pmap_extract(pmap, va, &ps->ps_addr)) {
			return EFAULT;
		}
		ps->ps_len = MIN(len, PAGE_SIZE - offset);
		if (ps->ps_len == len) {
			map->pm_nsegs = 1;
			return 0;
		}
		offset = 0;
	} else {
		/*
		 * Backup to the last segment since we have to see if we can
		 * merge virtual addresses that are physically contiguous into
		 * as few segments as possible.
		 */
		ps--;
		nsegs--;
	}

	paddr_t lastaddr = ps->ps_addr + ps->ps_len;
	for (;;) {
		paddr_t curaddr;
		if (!pmap_extract(pmap, va, &curaddr)) {
			return EFAULT;
		}
		if (curaddr != lastaddr) {
			ps++;
			nsegs++;
			KASSERT(nsegs < map->pm_maxsegs);
			ps->ps_addr = curaddr;
			lastaddr = curaddr;
		}
		if (offset + len > PAGE_SIZE) {
			ps->ps_len += PAGE_SIZE - offset;
			lastaddr = ps->ps_addr + ps->ps_len;
			len -= PAGE_SIZE - offset;
			lastaddr += PAGE_SIZE - offset;
			offset = 0;
		} else {
			ps->ps_len += len;
			map->pm_nsegs = nsegs + 1;
			return 0;
		}
	}
}

/*
 * Create a physmap and populate it with the pages that are used to mapped
 * linear range of virtual addresses.  It is assumed that uvm_vslock has been
 * called to lock these pages into memory.
 */
int
physmap_create_linear(physmap_t **map_p, const struct vmspace *vs, vaddr_t va,
	vsize_t len)
{
	const size_t maxsegs = atop(round_page(va + len) - trunc_page(va));
	physmap_t * const map = physmap_alloc(maxsegs);
	int error = physmap_fill(map, vs->vm_map.pmap, va, len);
	if (error) {
		physmap_destroy(map);
		*map_p = NULL;
		return error;
	}
	*map_p = map;
	return 0;
}

/*
 * Create a physmap and populate it with the pages that are contained in an
 * iovec array.  It is assumed that uvm_vslock has been called to lock these
 * pages into memory.
 */
int
physmap_create_iov(physmap_t **map_p, const struct vmspace *vs,
	struct iovec *iov, size_t iovlen)
{
	size_t maxsegs = 0;
	for (size_t i = 0; i < iovlen; i++) {
		const vaddr_t start = (vaddr_t) iov[i].iov_base;
		const vaddr_t end = start + iov[i].iov_len;
		maxsegs += atop(round_page(end) - trunc_page(start));
	}
	physmap_t * const map = physmap_alloc(maxsegs);

	for (size_t i = 0; i < iovlen; i++) {
		int error = physmap_fill(map, vs->vm_map.pmap,
		    (vaddr_t) iov[i].iov_base, iov[i].iov_len);
		if (error) {
			physmap_destroy(map);
			*map_p = NULL;
			return error;
		}
	}
	*map_p = map;
	return 0;
}

/*
 * This uses a list of vm_page structure to create a physmap.
 */
physmap_t *
physmap_create_pagelist(struct vm_page **pgs, size_t npgs)
{
	physmap_t * const map = physmap_alloc(npgs);

	physmap_segment_t *ps = map->pm_segs;

	/*
	 * Initialize the first segment.
	 */
	paddr_t lastaddr = VM_PAGE_TO_PHYS(pgs[0]);
	ps->ps_addr = lastaddr;
	ps->ps_len = PAGE_SIZE;

	for (pgs++; npgs-- > 1; pgs++) {
		/*
		 * lastaddr needs to be increased by a page.
		 */
		lastaddr += PAGE_SIZE;
		paddr_t curaddr = VM_PAGE_TO_PHYS(*pgs);
		if (curaddr != lastaddr) {
			/*
			 * If the addresses are not the same, we need to use
			 * a new segemnt.  Set its address and update lastaddr.
			 */
			ps++;
			ps->ps_addr = curaddr;
			lastaddr = curaddr;
		}
		/*
		 * Increase this segment's length by a page
		 */
		ps->ps_len += PAGE_SIZE;
	}

	map->pm_nsegs = ps + 1 - map->pm_segs;
	return map;
}

void
physmap_destroy(physmap_t *map)
{
	const size_t mapsize = offsetof(physmap_t, pm_segs[map->pm_maxsegs]);

	kmem_free(map, mapsize);
}

void *
physmap_map_init(physmap_t *map, size_t offset, vm_prot_t prot)
{
	physmap_cookie_t * const pc = kmem_zalloc(sizeof(*pc), KM_SLEEP);

	KASSERT(prot == VM_PROT_READ || prot == (VM_PROT_READ|VM_PROT_WRITE));

	pc->pc_physmap = map;
	pc->pc_segs = map->pm_segs;
	pc->pc_nsegs = map->pm_nsegs;
	pc->pc_prot = prot;
	pc->pc_klen = 0;
	pc->pc_kva = 0;
	pc->pc_direct_mapped = false;

	/*
	 * Skip to the first segment we are interested in.
	 */
	while (offset >= pc->pc_segs->ps_len) {
		offset -= pc->pc_segs->ps_len;
		pc->pc_segs++;
		pc->pc_nsegs--;
	}

	pc->pc_offset = offset;

	return pc;
}

size_t
physmap_map(void *cookie, vaddr_t *kvap)
{
	physmap_cookie_t * const pc = cookie;

	/*
	 * If there is currently a non-direct mapped KVA region allocated,
	 * free it now.
	 */
	if (pc->pc_kva != 0 && !pc->pc_direct_mapped) {
		pmap_kremove(pc->pc_kva, pc->pc_klen);
		pmap_update(pmap_kernel());
		uvm_km_free(kernel_map, pc->pc_kva, pc->pc_klen,
		    UVM_KMF_VAONLY);
	}

	/*
	 * If there are no more segments to process, return 0 indicating
	 * we are done.
	 */
	if (pc->pc_nsegs == 0) {
		return 0;
	}

	/*
	 * Get starting physical address of this segment and its length.
	 */
	paddr_t pa = pc->pc_segs->ps_addr + pc->pc_offset;
	const size_t koff = pa & PAGE_MASK;
	const size_t len = pc->pc_segs->ps_len - pc->pc_offset;

	/*
	 * Now that we have the starting offset in the page, reset to the
	 * beginning of the page.
	 */
	pa = trunc_page(pa);

	/*
	 * We are now done with this segment; advance to the next one.
	 */
	pc->pc_segs++;
	pc->pc_nsegs--;
	pc->pc_offset = 0;

	/*
	 * Find out how many pages we are mapping.
	 */
	pc->pc_klen = round_page(len);
#ifdef __HAVE_MM_MD_DIRECT_MAPPED_PHYS
	/*
	 * Always try to direct map it since that's nearly zero cost.
	 */
	pc->pc_direct_mapped = mm_md_direct_mapped_phys(pa, &pc->pc_kva);
#endif
	if (!pc->pc_direct_mapped) {
		/*
		 * If we can't direct map it, we have to allocate some KVA
		 * so we map it via the kernel_map.
		 */
		pc->pc_kva = uvm_km_alloc(kernel_map, pc->pc_klen,
		    atop(pa) & uvmexp.ncolors,
		    UVM_KMF_VAONLY | UVM_KMF_WAITVA | UVM_KMF_COLORMATCH);
		KASSERT(pc->pc_kva != 0);

		/*
		 * Setup mappings for this segment.
		 */
		for (size_t poff = 0; poff < pc->pc_klen; poff += PAGE_SIZE) {
			pmap_kenter_pa(pc->pc_kva + poff, pa + poff,
			    pc->pc_prot, 0);
		}
		/*
		 * Make them real.
		 */
		pmap_update(pmap_kernel());
	}
	/*
	 * Return the starting KVA (including offset into the page) and
	 * the length of this segment.
	 */
	*kvap = pc->pc_kva + koff;
	return len;
}

void
physmap_map_fini(void *cookie)
{
	physmap_cookie_t * const pc = cookie;

	/*
	 * If there is currently a non-direct mapped KVA region allocated,
	 * free it now.
	 */
	if (pc->pc_kva != 0 && !pc->pc_direct_mapped) {
		pmap_kremove(pc->pc_kva, pc->pc_klen);
		pmap_update(pmap_kernel());
		uvm_km_free(kernel_map, pc->pc_kva, pc->pc_klen,
		    UVM_KMF_VAONLY);
	}

	/*
	 * Free the cookie.
	 */
	kmem_free(pc, sizeof(*pc));
}

/*
 * genio needs to zero pages past the EOF or without backing storage (think
 * sparse files).  But since we are using physmaps, there is no kva to use with
 * memset so we need a helper to obtain a kva and memset the desired memory.
 */
void
physmap_zero(physmap_t *map, size_t offset, size_t len)
{
	void * const cookie = physmap_map_init(map, offset,
	    VM_PROT_READ|VM_PROT_WRITE);

	for (;;) {
		vaddr_t kva;
		size_t seglen = physmap_map(cookie, &kva);
		KASSERT(seglen != 0);
		if (seglen > len)
			seglen = len;
		memset((void *)kva, 0, seglen);
		if (seglen == len)
			break;
	}

	physmap_map_fini(cookie);
}

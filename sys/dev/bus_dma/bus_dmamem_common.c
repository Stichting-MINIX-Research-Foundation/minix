/*	$NetBSD: bus_dmamem_common.c,v 1.2 2012/10/02 23:49:19 christos Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: bus_dmamem_common.c,v 1.2 2012/10/02 23:49:19 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/bus.h>

#include <uvm/uvm.h>

#include <dev/bus_dma/bus_dmamem_common.h>

/*
 * _bus_dmamem_alloc_range_common --
 *	Allocate physical memory from the specified physical address range.
 */
int
_bus_dmamem_alloc_range_common(bus_dma_tag_t t,
			       bus_size_t size,
			       bus_size_t alignment,
			       bus_size_t boundary,
			       bus_dma_segment_t *segs,
			       int nsegs,
			       int *rsegs,
			       int flags,
			       paddr_t low,
			       paddr_t high)
{
	paddr_t curaddr, lastaddr;
	struct vm_page *m;
	struct pglist mlist;
	int curseg, error;

	/* Always round the size. */
	size = round_page(size);

	/* Allocate pages from the VM system. */
	error = uvm_pglistalloc(size, low, high, alignment, boundary,
				&mlist, nsegs, (flags & BUS_DMA_NOWAIT) == 0);
	if (__predict_false(error != 0))
		return (error);
	
	/*
	 * Compute the location, size, and number of segments actually
	 * returned by the VM system.
	 */
	m = TAILQ_FIRST(&mlist);
	curseg = 0;
	lastaddr = segs[curseg].ds_addr = VM_PAGE_TO_PHYS(m);
	segs[curseg].ds_len = PAGE_SIZE;
	m = TAILQ_NEXT(m, pageq.queue);

	for (; m != NULL; m = TAILQ_NEXT(m, pageq.queue)) {
		curaddr = VM_PAGE_TO_PHYS(m);
		KASSERT(curaddr >= low);
		KASSERT(curaddr < high);
		if (curaddr == (lastaddr + PAGE_SIZE))
			segs[curseg].ds_len += PAGE_SIZE;
		else {
			curseg++;
			segs[curseg].ds_addr = curaddr;
			segs[curseg].ds_len = PAGE_SIZE;
		}
		lastaddr = curaddr;
	}

	*rsegs = curseg + 1;

	return (0);
}

/*
 * _bus_dmamem_free_common --
 *	Free memory allocated with _bus_dmamem_alloc_range_common()
 *	back to the VM system.
 */
void
_bus_dmamem_free_common(bus_dma_tag_t t,
			bus_dma_segment_t *segs,
			int nsegs)
{
	struct vm_page *m;
	bus_addr_t addr;
	struct pglist mlist;
	int curseg;

	TAILQ_INIT(&mlist);
	for (curseg = 0; curseg < nsegs; curseg++) {
		for (addr = segs[curseg].ds_addr;
		     addr < (segs[curseg].ds_addr + segs[curseg].ds_len);
		     addr += PAGE_SIZE) {
			m = PHYS_TO_VM_PAGE(addr);
			TAILQ_INSERT_TAIL(&mlist, m, pageq.queue);
		}
	}

	uvm_pglistfree(&mlist);
}

/*
 * _bus_dmamem_map_common --
 *	Map memory allocated with _bus_dmamem_alloc_range_common() into
 *	the kernel virtual address space.
 */
int
_bus_dmamem_map_common(bus_dma_tag_t t,
		       bus_dma_segment_t *segs,
		       int nsegs,
		       size_t size,
		       void **kvap,
		       int flags,
		       int pmapflags)
{
	vaddr_t va;
	bus_addr_t addr;
	int curseg;
	const uvm_flag_t kmflags =
	    (flags & BUS_DMA_NOWAIT) != 0 ? UVM_KMF_NOWAIT : 0;

	size = round_page(size);

	va = uvm_km_alloc(kernel_map, size, 0, UVM_KMF_VAONLY | kmflags);
	if (__predict_false(va == 0))
		return (ENOMEM);
	
	*kvap = (void *)va;

	for (curseg = 0; curseg < nsegs; curseg++) {
		for (addr = segs[curseg].ds_addr;
		     addr < (segs[curseg].ds_addr + segs[curseg].ds_len);
		     addr += PAGE_SIZE, va += PAGE_SIZE, size -= PAGE_SIZE) {
			KASSERT(size != 0);
			/* XXX pmap_kenter_pa()? */
			pmap_enter(pmap_kernel(), va, addr,
			    VM_PROT_READ | VM_PROT_WRITE,
			    pmapflags | PMAP_WIRED |
			    	VM_PROT_READ | VM_PROT_WRITE);
		}
	}
	pmap_update(pmap_kernel());

	return (0);
}

/*
 * _bus_dmamem_unmap_common --
 *	Remove a mapping created with _bus_dmamem_map_common().
 */
void
_bus_dmamem_unmap_common(bus_dma_tag_t t,
			 void *kva,
			 size_t size)
{

	KASSERT(((vaddr_t)kva & PAGE_MASK) == 0);

	size = round_page(size);
	/* XXX pmap_kremove()?  See above... */
	pmap_remove(pmap_kernel(), (vaddr_t)kva, (vaddr_t)kva + size);
	pmap_update(pmap_kernel());
	uvm_km_free(kernel_map, (vaddr_t)kva, size, UVM_KMF_VAONLY);
}

/*
 * _bus_dmamem_mmap_common --
 *	Mmap support for memory allocated with _bus_dmamem_alloc_range_common().
 */
bus_addr_t
_bus_dmamem_mmap_common(bus_dma_tag_t t,
			bus_dma_segment_t *segs,
			int nsegs,
			off_t off,
			int prot,
			int flags)
{
	int i;

	for (i = 0; i < nsegs; i++) {
		KASSERT((off & PAGE_MASK) == 0);
		KASSERT((segs[i].ds_addr & PAGE_MASK) == 0);
		KASSERT((segs[i].ds_len & PAGE_MASK) == 0);
		if (off >= segs[i].ds_len) {
			off -= segs[i].ds_len;
			continue;
		}

		/* XXX BUS_DMA_COHERENT */

		return (segs[i].ds_addr + off);
	}

	/* Page not found. */
	return ((bus_addr_t)-1);
}

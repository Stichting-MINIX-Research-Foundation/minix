/*	$NetBSD: rumpdev_bus_dma.c,v 1.5 2015/06/15 15:38:52 pooka Exp $	*/

/*-
 * Copyright (c) 2013 Antti Kantee
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS
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

/*-
 * Copyright (c) 1996, 1997, 1998 The NetBSD Foundation, Inc.
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

/*
 * bus_dma(9) implementation which runs on top of rump kernel hypercalls.
 * It's essentially the same as the PowerPC implementation its based on,
 * except with some indirection and PowerPC MD features removed.
 * This should/could be expected to run on x86, other archs may need
 * some cache flushing hooks.
 *
 * From sys/arch/powerpc/powerpc/bus_dma.c:
 *	NetBSD: bus_dma.c,v 1.46 2012/02/01 09:54:03 matt Exp
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/kmem.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <uvm/uvm.h>

#include "pci_user.h"

#define	EIEIO	membar_sync()

int	_bus_dmamap_load_buffer (bus_dma_tag_t, bus_dmamap_t, void *,
	    bus_size_t, struct vmspace *, int, paddr_t *, int *, int);

/*
 * Common function for DMA map creation.  May be called by bus-specific
 * DMA map creation functions.
 */
int
bus_dmamap_create(bus_dma_tag_t t, bus_size_t size, int nsegments,
	bus_size_t maxsegsz, bus_size_t boundary, int flags,
	bus_dmamap_t *dmamp)
{
	bus_dmamap_t map;
	void *mapstore;
	size_t mapsize;

	/*
	 * Allocate and initialize the DMA map.  The end of the map
	 * is a variable-sized array of segments, so we allocate enough
	 * room for them in one shot.
	 *
	 * Note we don't preserve the WAITOK or NOWAIT flags.  Preservation
	 * of ALLOCNOW notifies others that we've reserved these resources,
	 * and they are not to be freed.
	 *
	 * The bus_dmamap_t includes one bus_dma_segment_t, hence
	 * the (nsegments - 1).
	 */
	mapsize = sizeof(*map) + sizeof(bus_dma_segment_t [nsegments - 1]);
	if ((mapstore = kmem_intr_alloc(mapsize,
	    (flags & BUS_DMA_NOWAIT) ? KM_NOSLEEP : KM_SLEEP)) == NULL)
		return (ENOMEM);

	memset(mapstore, 0, mapsize);
	map = (void *)mapstore;
	map->_dm_size = size;
	map->_dm_segcnt = nsegments;
	map->_dm_maxmaxsegsz = maxsegsz;
	map->_dm_boundary = boundary;
	map->_dm_bounce_thresh = 0;
	map->_dm_flags = flags & ~(BUS_DMA_WAITOK|BUS_DMA_NOWAIT);
	map->dm_maxsegsz = maxsegsz;
	map->dm_mapsize = 0;		/* no valid mappings */
	map->dm_nsegs = 0;

	*dmamp = map;
	return (0);
}

/*
 * Common function for DMA map destruction.  May be called by bus-specific
 * DMA map destruction functions.
 */
void
bus_dmamap_destroy(bus_dma_tag_t t, bus_dmamap_t map)
{

	size_t mapsize = sizeof(*map)
	    + sizeof(bus_dma_segment_t [map->_dm_segcnt - 1]);
	kmem_intr_free(map, mapsize);
}

/*
 * Utility function to load a linear buffer.  lastaddrp holds state
 * between invocations (for multiple-buffer loads).  segp contains
 * the starting segment on entrance, and the ending segment on exit.
 * first indicates if this is the first invocation of this function.
 */
int
_bus_dmamap_load_buffer(bus_dma_tag_t t, bus_dmamap_t map,
	void *buf, bus_size_t buflen, struct vmspace *vm, int flags,
	paddr_t *lastaddrp, int *segp, int first)
{
	bus_size_t sgsize;
	bus_addr_t curaddr, lastaddr, baddr, bmask;
	vaddr_t vaddr = (vaddr_t)buf;
	int seg;

//	printf("%s(%p,%p,%p,%u,%p,%#x,%p,%p,%u)\n", __func__,
//	    t, map, buf, buflen, vm, flags, lastaddrp, segp, first);

	lastaddr = *lastaddrp;
	bmask = ~(map->_dm_boundary - 1);

	for (seg = *segp; buflen > 0 ; ) {
		/*
		 * Get the physical address for this segment.
		 */
		if (!VMSPACE_IS_KERNEL_P(vm))
			(void) pmap_extract(vm_map_pmap(&vm->vm_map),
			    vaddr, (void *)&curaddr);
		else
			curaddr = vtophys(vaddr);

		/*
		 * If we're beyond the bounce threshold, notify
		 * the caller.
		 */
		if (map->_dm_bounce_thresh != 0 &&
		    curaddr >= map->_dm_bounce_thresh)
			return (EINVAL);

		/*
		 * Compute the segment size, and adjust counts.
		 */
		sgsize = PAGE_SIZE - ((u_long)vaddr & PGOFSET);
		if (buflen < sgsize)
			sgsize = buflen;
		sgsize = min(sgsize, map->dm_maxsegsz);

		/*
		 * Make sure we don't cross any boundaries.
		 */
		if (map->_dm_boundary > 0) {
			baddr = (curaddr + map->_dm_boundary) & bmask;
			if (sgsize > (baddr - curaddr))
				sgsize = (baddr - curaddr);
		}

		/*
		 * Insert chunk into a segment, coalescing with
		 * the previous segment if possible.
		 */
		if (first) {
			map->dm_segs[seg].ds_addr
			    = rumpcomp_pci_virt_to_mach((void *)curaddr);
			map->dm_segs[seg].ds_len = sgsize;
			first = 0;
		} else {
			if (curaddr == lastaddr &&
			    (map->dm_segs[seg].ds_len + sgsize) <=
			     map->dm_maxsegsz &&
			    (map->_dm_boundary == 0 ||
			     (map->dm_segs[seg].ds_addr & bmask) ==
			     (rumpcomp_pci_virt_to_mach((void*)curaddr)&bmask)))
				map->dm_segs[seg].ds_len += sgsize;
			else {
				if (++seg >= map->_dm_segcnt)
					break;
				map->dm_segs[seg].ds_addr =
				    rumpcomp_pci_virt_to_mach((void *)curaddr);
				map->dm_segs[seg].ds_len = sgsize;
			}
		}

		lastaddr = curaddr + sgsize;
		vaddr += sgsize;
		buflen -= sgsize;
	}

	*segp = seg;
	*lastaddrp = lastaddr;

	/*
	 * Did we fit?
	 */
	if (buflen != 0)
		return (EFBIG);		/* XXX better return value here? */

	return (0);
}

/*
 * Common function for loading a DMA map with a linear buffer.  May
 * be called by bus-specific DMA map load functions.
 */
int
bus_dmamap_load(bus_dma_tag_t t, bus_dmamap_t map,
	void *buf, bus_size_t buflen, struct proc *p, int flags)
{
	paddr_t lastaddr = 0;
	int seg, error;
	struct vmspace *vm;

	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;
	KASSERT(map->dm_maxsegsz <= map->_dm_maxmaxsegsz);

	if (buflen > map->_dm_size)
		return (EINVAL);

	if (p != NULL) {
		vm = p->p_vmspace;
	} else {
		vm = vmspace_kernel();
	}

	seg = 0;
	error = _bus_dmamap_load_buffer(t, map, buf, buflen, vm, flags,
		&lastaddr, &seg, 1);
	if (error == 0) {
		map->dm_mapsize = buflen;
		map->dm_nsegs = seg + 1;
	}
	return (error);
}

/*
 * Like _bus_dmamap_load(), but for mbufs.
 */
int
bus_dmamap_load_mbuf(bus_dma_tag_t t, bus_dmamap_t map,
	struct mbuf *m0, int flags)
{
	paddr_t lastaddr = 0;
	int seg, error, first;
	struct mbuf *m;

	/*
	 * Make sure that on error condition we return "no valid mappings."
	 */
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;
	KASSERT(map->dm_maxsegsz <= map->_dm_maxmaxsegsz);

#ifdef DIAGNOSTIC
	if ((m0->m_flags & M_PKTHDR) == 0)
		panic("_bus_dmamap_load_mbuf: no packet header");
#endif

	if (m0->m_pkthdr.len > map->_dm_size)
		return (EINVAL);

	first = 1;
	seg = 0;
	error = 0;
	for (m = m0; m != NULL && error == 0; m = m->m_next, first = 0) {
		if (m->m_len == 0)
			continue;
#ifdef POOL_VTOPHYS
		/* XXX Could be better about coalescing. */
		/* XXX Doesn't check boundaries. */
		switch (m->m_flags & (M_EXT|M_CLUSTER)) {
		case M_EXT|M_CLUSTER:
			/* XXX KDASSERT */
			KASSERT(m->m_ext.ext_paddr != M_PADDR_INVALID);
			lastaddr = m->m_ext.ext_paddr +
			    (m->m_data - m->m_ext.ext_buf);
 have_addr:
			if (first == 0 && ++seg >= map->_dm_segcnt) {
				error = EFBIG;
				continue;
			}
			map->dm_segs[seg].ds_addr =
			    rumpcomp_pci_virt_to_mach((void *)lastaddr);
			map->dm_segs[seg].ds_len = m->m_len;
			lastaddr += m->m_len;
			continue;

		case 0:
			lastaddr = m->m_paddr + M_BUFOFFSET(m) +
			    (m->m_data - M_BUFADDR(m));
			goto have_addr;

		default:
			break;
		}
#endif
		error = _bus_dmamap_load_buffer(t, map, m->m_data,
		    m->m_len, vmspace_kernel(), flags, &lastaddr, &seg, first);
	}
	if (error == 0) {
		map->dm_mapsize = m0->m_pkthdr.len;
		map->dm_nsegs = seg + 1;
	}
	return (error);
}

/*
 * Like _bus_dmamap_load(), but for uios.
 */
int
bus_dmamap_load_uio(bus_dma_tag_t t, bus_dmamap_t map,
	struct uio *uio, int flags)
{
	paddr_t lastaddr = 0;
	int seg, i, error, first;
	bus_size_t minlen, resid;
	struct iovec *iov;
	void *addr;

	/*
	 * Make sure that on error condition we return "no valid mappings."
	 */
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;
	KASSERT(map->dm_maxsegsz <= map->_dm_maxmaxsegsz);

	resid = uio->uio_resid;
	iov = uio->uio_iov;

	first = 1;
	seg = 0;
	error = 0;
	for (i = 0; i < uio->uio_iovcnt && resid != 0 && error == 0; i++) {
		/*
		 * Now at the first iovec to load.  Load each iovec
		 * until we have exhausted the residual count.
		 */
		minlen = resid < iov[i].iov_len ? resid : iov[i].iov_len;
		addr = (void *)iov[i].iov_base;

		error = _bus_dmamap_load_buffer(t, map, addr, minlen,
		    uio->uio_vmspace, flags, &lastaddr, &seg, first);
		first = 0;

		resid -= minlen;
	}
	if (error == 0) {
		map->dm_mapsize = uio->uio_resid;
		map->dm_nsegs = seg + 1;
	}
	return (error);
}

/*
 * Like _bus_dmamap_load(), but for raw memory allocated with
 * bus_dmamem_alloc().
 */
int
bus_dmamap_load_raw(bus_dma_tag_t t, bus_dmamap_t map,
	bus_dma_segment_t *segs, int nsegs, bus_size_t size, int flags)
{

	panic("_bus_dmamap_load_raw: not implemented");
}

/*
 * Common function for unloading a DMA map.  May be called by
 * chipset-specific DMA map unload functions.
 */
void
bus_dmamap_unload(bus_dma_tag_t t, bus_dmamap_t map)
{

	/*
	 * No resources to free; just mark the mappings as
	 * invalid.
	 */
	map->dm_maxsegsz = map->_dm_maxmaxsegsz;
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;
}

void
bus_dmamap_sync(bus_dma_tag_t t, bus_dmamap_t map,
	bus_addr_t offset, bus_size_t len, int ops)
{

	/* XXX: this might need some MD tweaks */
	membar_sync();
}

/*
 * Common function for freeing DMA-safe memory.  May be called by
 * bus-specific DMA memory free functions.
 */
void
bus_dmamem_free(bus_dma_tag_t t, bus_dma_segment_t *segs, int nsegs)
{
#ifdef RUMPCOMP_USERFEATURE_PCI_DMAFREE
	vaddr_t vacookie = segs[0]._ds_vacookie;
	bus_size_t sizecookie = segs[0]._ds_sizecookie;

	rumpcomp_pci_dmafree(vacookie, sizecookie);
#else
	panic("bus_dmamem_free not implemented");
#endif
}

/*
 * Don't have hypercall for mapping scatter-gather memory.
 * So just simply fail if there's more than one segment to map
 */
int
bus_dmamem_map(bus_dma_tag_t t, bus_dma_segment_t *segs, int nsegs,
	size_t size, void **kvap, int flags)
{
	struct rumpcomp_pci_dmaseg *dss;
	size_t allocsize = nsegs * sizeof(*dss);
	int rv, i;

	/*
	 * Though rumpcomp_pci_dmaseg "accidentally" matches the
	 * bus_dma segment descriptor (at least for now), act
	 * proper and actually translate it.
	 */
	dss = kmem_alloc(allocsize, KM_SLEEP);
	for (i = 0; i < nsegs; i++) {
		dss[i].ds_pa = segs[i].ds_addr;
		dss[i].ds_len = segs[i].ds_len;
		dss[i].ds_vacookie = segs[i]._ds_vacookie;
	}
	rv = rumpcomp_pci_dmamem_map(dss, nsegs, size, kvap);
	kmem_free(dss, allocsize);

	return rv;
}

/*
 * Common function for unmapping DMA-safe memory.  May be called by
 * bus-specific DMA memory unmapping functions.
 */
void
bus_dmamem_unmap(bus_dma_tag_t t, void *kva, size_t size)
{

	/* nothing to do as long as bus_dmamem_map() is what it is */
}

paddr_t
bus_dmamem_mmap(bus_dma_tag_t t, bus_dma_segment_t *segs, int nsegs,
	off_t off, int prot, int flags)
{

	panic("bus_dmamem_mmap not supported");
}

/*
 * Allocate physical memory from the given physical address range.
 * Called by DMA-safe memory allocation methods.
 */
int
bus_dmamem_alloc(bus_dma_tag_t t, bus_size_t size, bus_size_t alignment,
	bus_size_t boundary, bus_dma_segment_t *segs, int nsegs, int *rsegs,
	int flags)
{
	paddr_t curaddr, lastaddr, pa;
	vaddr_t vacookie;
	size_t sizecookie;
	int curseg, error;

	/* Always round the size. */
	size = round_page(size);

	sizecookie = size;

	/*
	 * Allocate pages from the VM system.
	 */
#if 0
	error = uvm_pglistalloc(size, low, high, alignment, boundary,
	    &mlist, nsegs, (flags & BUS_DMA_NOWAIT) == 0);
#else
	/* XXX: ignores boundary, nsegs, etc. */
	//printf("dma allocation %lx %lx %d\n", alignment, boundary, nsegs);
	error = rumpcomp_pci_dmalloc(size, alignment, &pa, &vacookie);
#endif
	if (error)
		return (error);

	/*
	 * Compute the location, size, and number of segments actually
	 * returned by the VM code.
	 */
	curseg = 0;
	lastaddr = segs[curseg].ds_addr = pa;
	segs[curseg].ds_len = PAGE_SIZE;
	segs[curseg]._ds_vacookie = vacookie;
	segs[curseg]._ds_sizecookie = sizecookie;
	size -= PAGE_SIZE;
	pa += PAGE_SIZE;
	vacookie += PAGE_SIZE;

	for (; size;
	    pa += PAGE_SIZE, vacookie += PAGE_SIZE, size -= PAGE_SIZE) {
		curaddr = pa;
		if (curaddr == (lastaddr + PAGE_SIZE) &&
		    (lastaddr & boundary) == (curaddr & boundary)) {
			segs[curseg].ds_len += PAGE_SIZE;
		} else {
			curseg++;
			if (curseg >= nsegs)
				return EFBIG;
			segs[curseg].ds_addr = curaddr;
			segs[curseg].ds_len = PAGE_SIZE;
			segs[curseg]._ds_vacookie = vacookie;
			segs[curseg]._ds_sizecookie = sizecookie;
		}
		lastaddr = curaddr;
	}
	*rsegs = curseg + 1;

	return (0);
}

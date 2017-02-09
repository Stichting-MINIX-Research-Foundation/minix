/*	$NetBSD: fwdma.c,v 1.16 2010/05/23 18:56:58 christos Exp $	*/
/*-
 * Copyright (c) 2003
 * 	Hidetoshi Shimokawa. All rights reserved.
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
 *
 *	This product includes software developed by Hidetoshi Shimokawa.
 *
 * 4. Neither the name of the author nor the names of its contributors
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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: fwdma.c,v 1.16 2010/05/23 18:56:58 christos Exp $");
#if defined(__FreeBSD__)
__FBSDID("$FreeBSD: src/sys/dev/firewire/fwdma.c,v 1.9 2007/06/06 14:31:36 simokawa Exp $");
#endif

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/select.h>

#include <machine/vmparam.h>

#include <dev/ieee1394/firewire.h>
#include <dev/ieee1394/firewirereg.h>
#include <dev/ieee1394/fwdma.h>

#define BUS_SPACE_MAXSIZE_32BIT		0xffffffff


void *
fwdma_malloc(device_t dev, bus_dma_tag_t dmat, bus_dmamap_t *dmamap,
	     bus_size_t size, int alignment, int flags)
{
	bus_dma_segment_t segs;
	int nsegs;
	int err;
	void *v_addr;

	err = bus_dmamem_alloc(dmat, size, alignment, 0, &segs, 1,
	    &nsegs, flags);
	if (err) {
		aprint_error_dev(dev, "DMA memory allocation failed %d\n", err);
		return NULL;
	}

	err = bus_dmamem_map(dmat, &segs, nsegs, size, &v_addr, flags);
	if (err) {
		aprint_error_dev(dev, "DMA memory map failed %d\n", err);
		bus_dmamem_free(dmat, &segs, nsegs);
		return NULL;
	}

	if (*dmamap == NULL) {
		err = bus_dmamap_create(dmat, size, nsegs,
		    BUS_SPACE_MAXSIZE_32BIT, 0, flags, dmamap);
		if (err) {
			aprint_error_dev(dev,
			    "DMA map create failed %d\n", err);
			bus_dmamem_unmap(dmat, v_addr, size);
			bus_dmamem_free(dmat, &segs, nsegs);
			return NULL;
		}
	}

	err = bus_dmamap_load(dmat, *dmamap, v_addr, size, NULL, flags);
	if (err != 0) {
		aprint_error_dev(dev, "DMA map load failed %d\n", err);
		bus_dmamap_destroy(dmat, *dmamap);
		bus_dmamem_unmap(dmat, v_addr, size);
		bus_dmamem_free(dmat, &segs, nsegs);
		return NULL;
	}

	return v_addr;
}

void
fwdma_free(bus_dma_tag_t dmat, bus_dmamap_t dmamap, void *vaddr)
{

	bus_dmamap_unload(dmat, dmamap);
	bus_dmamem_unmap(dmat, vaddr, dmamap->dm_mapsize);
	bus_dmamem_free(dmat, dmamap->dm_segs, dmamap->dm_nsegs);
	bus_dmamap_destroy(dmat, dmamap);
}


void *
fwdma_alloc_setup(device_t dev, bus_dma_tag_t dmat, bus_size_t size,
		  struct fwdma_alloc *dma, int alignment, int flags)
{

	dma->v_addr =
	    fwdma_malloc(dev, dmat, &dma->dma_map, size, alignment, flags);
	if (dma->v_addr != NULL) {
		dma->dma_tag = dmat;
		dma->bus_addr = dma->dma_map->dm_segs[0].ds_addr;
	}
	return dma->v_addr;
}

/*
 * Allocate multisegment dma buffers
 * each segment size is eqaul to ssize except last segment.
 */
struct fwdma_alloc_multi *
fwdma_malloc_multiseg(struct firewire_comm *fc, int alignment, int esize, int n,
		      int flags)
{
	struct fwdma_alloc_multi *am;
	struct fwdma_seg *seg;
	bus_size_t ssize;
	size_t size;
	int nseg;

	if (esize > PAGE_SIZE) {
		/* round up to PAGE_SIZE */
		esize = ssize = roundup2(esize, PAGE_SIZE);
		nseg = n;
	} else {
		/* allocate PAGE_SIZE segment for small elements */
		ssize = rounddown(PAGE_SIZE, esize);
		nseg = howmany(n, ssize / esize);
	}
	size = sizeof(struct fwdma_alloc_multi) +
	    sizeof(struct fwdma_seg) * nseg;
	am = (struct fwdma_alloc_multi *)malloc(size, M_FW, M_WAITOK | M_ZERO);
	if (am == NULL) {
		aprint_error_dev(fc->dev, "malloc failed\n");
		return NULL;
	}
	am->ssize = ssize;
	am->esize = esize;
	am->nseg = 0;
	am->dma_tag = fc->dmat;

	for (seg = am->seg; nseg--; seg++) {
		seg->v_addr = fwdma_malloc(fc->dev, am->dma_tag, &seg->dma_map,
		    ssize, alignment, flags);
		if (seg->v_addr == NULL) {
			aprint_error_dev(fc->dev, "malloc_size failed %d\n",
			    am->nseg);
			fwdma_free_multiseg(am);
			return NULL;
		}
		seg->bus_addr = seg->dma_map->dm_segs[0].ds_addr;
		am->nseg++;
	}
	return am;
}

void
fwdma_free_multiseg(struct fwdma_alloc_multi *am)
{
	struct fwdma_seg *seg;

	for (seg = am->seg; am->nseg--; seg++)
		fwdma_free(am->dma_tag, seg->dma_map, seg->v_addr);
	free(am, M_FW);
}

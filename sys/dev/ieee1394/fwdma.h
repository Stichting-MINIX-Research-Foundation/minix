/*	$NetBSD: fwdma.h,v 1.6 2010/03/29 03:05:27 kiyohara Exp $	*/
/*-
 * Copyright (C) 2003
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
 * $FreeBSD: /repoman/r/ncvs/src/sys/dev/firewire/fwdma.h,v 1.3 2005/01/06 01:42:41 imp Exp $
 */

#ifndef _FWDMA_H_
#define _FWDMA_H_

struct fwdma_alloc {
	bus_dma_tag_t	dma_tag;
	bus_dmamap_t	dma_map;
	void *		v_addr;
	bus_addr_t	bus_addr;
};

struct fwdma_seg {
	bus_dmamap_t	dma_map;
	void *		v_addr;
	bus_addr_t	bus_addr;
};

struct fwdma_alloc_multi {
	bus_size_t	ssize;
	bus_size_t	esize;
	int		nseg;
	bus_dma_tag_t	dma_tag;
	struct fwdma_seg seg[0];
};

static __inline void *
fwdma_v_addr(struct fwdma_alloc_multi *am, int index)
{
	bus_size_t ssize = am->ssize;
	int offset = am->esize * index;

	return (char *)am->seg[offset / ssize].v_addr + (offset % ssize);
}

static __inline bus_addr_t
fwdma_bus_addr(struct fwdma_alloc_multi *am, int index)
{
	bus_size_t ssize = am->ssize;
	int offset = am->esize * index;

	return am->seg[offset / ssize].bus_addr + (offset % ssize);
}

static __inline void
fwdma_sync_multiseg(struct fwdma_alloc_multi *am, int start, int end, int op)
{
	struct fwdma_seg *seg, *eseg;
	bus_addr_t off, eoff;

	off = (am->esize * start) % am->ssize;
	eoff = (am->esize * end) % am->ssize;
	seg = &am->seg[am->esize * start / am->ssize];
	eseg = &am->seg[am->esize * end / am->ssize];

	if (start > end) {
		for (; seg < &am->seg[am->nseg]; seg++) {
			bus_dmamap_sync(am->dma_tag, seg->dma_map,
			    off, seg->dma_map->dm_mapsize - off, op);
			off = 0;
		}
		seg = am->seg;
	}
	for (; seg < eseg; seg++) {
		bus_dmamap_sync(am->dma_tag, seg->dma_map,
		    off, seg->dma_map->dm_mapsize - off, op);
		off = 0;
	}
	bus_dmamap_sync(am->dma_tag, seg->dma_map,
	    off, eoff - off + am->esize, op);
}

static __inline void
fwdma_sync_multiseg_all(struct fwdma_alloc_multi *am, int op)
{
	struct fwdma_seg *seg;
	int i;

	seg = am->seg;
	for (i = 0; i < am->nseg; i++, seg++)
		bus_dmamap_sync(am->dma_tag, seg->dma_map,
		    0, seg->dma_map->dm_mapsize, op);
}

void *fwdma_malloc(device_t, bus_dma_tag_t, bus_dmamap_t *, bus_size_t, int,
		   int);
void fwdma_free(bus_dma_tag_t, bus_dmamap_t, void *);
void *fwdma_alloc_setup(device_t, bus_dma_tag_t, bus_size_t,
			struct fwdma_alloc *, int, int);
struct fwdma_alloc_multi *fwdma_malloc_multiseg(struct firewire_comm *,
						int, int, int, int);
void fwdma_free_multiseg(struct fwdma_alloc_multi *);

#endif	/* _FWDMA_H_ */

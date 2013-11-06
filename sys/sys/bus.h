/*	$NetBSD: bus.h,v 1.11 2012/05/07 18:16:38 tsutsui Exp $	*/

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
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

#ifndef _SYS_BUS_H_
#define	_SYS_BUS_H_

#include <sys/types.h>

#ifdef __HAVE_NEW_STYLE_BUS_H

#include <machine/bus_defs.h>

struct bus_space_reservation {
	bus_addr_t _bsr_start;
	bus_size_t _bsr_size;
};

typedef struct bus_space_reservation bus_space_reservation_t;

static inline bus_size_t
bus_space_reservation_size(bus_space_reservation_t *bsr)
{
	return bsr->_bsr_size;
}

static inline bus_space_reservation_t *
bus_space_reservation_init(bus_space_reservation_t *bsr,
    bus_addr_t addr, bus_size_t size)
{
	bsr->_bsr_start = addr;
	bsr->_bsr_size = size;
	return bsr;
}

static inline bus_addr_t
bus_space_reservation_addr(bus_space_reservation_t *bsr)
{
	return bsr->_bsr_start;
}

enum bus_space_override_idx {
	  BUS_SPACE_OVERRIDE_MAP		= __BIT(0)
	, BUS_SPACE_OVERRIDE_UNMAP		= __BIT(1)
	, BUS_SPACE_OVERRIDE_ALLOC		= __BIT(2)
	, BUS_SPACE_OVERRIDE_FREE		= __BIT(3)
	, BUS_SPACE_OVERRIDE_RESERVE		= __BIT(4)
	, BUS_SPACE_OVERRIDE_RELEASE		= __BIT(5)
	, BUS_SPACE_OVERRIDE_RESERVATION_MAP	= __BIT(6)
	, BUS_SPACE_OVERRIDE_RESERVATION_UNMAP	= __BIT(7)
	, BUS_SPACE_OVERRIDE_RESERVE_SUBREGION	= __BIT(8)
#if 0
	, BUS_SPACE_OVERRIDE_EXTEND	= __BIT(9)
	, BUS_SPACE_OVERRIDE_TRIM	= __BIT(10)
#endif
};

enum bus_dma_override_idx {
	  BUS_DMAMAP_OVERRIDE_CREATE	= __BIT(0)
	, BUS_DMAMAP_OVERRIDE_DESTROY	= __BIT(1)
	, BUS_DMAMAP_OVERRIDE_LOAD	= __BIT(2)
	, BUS_DMAMAP_OVERRIDE_LOAD_MBUF	= __BIT(3)
	, BUS_DMAMAP_OVERRIDE_LOAD_UIO	= __BIT(4)
	, BUS_DMAMAP_OVERRIDE_LOAD_RAW	= __BIT(5)
	, BUS_DMAMAP_OVERRIDE_UNLOAD	= __BIT(6)
	, BUS_DMAMAP_OVERRIDE_SYNC	= __BIT(7)
	, BUS_DMAMEM_OVERRIDE_ALLOC	= __BIT(8)
	, BUS_DMAMEM_OVERRIDE_FREE	= __BIT(9)
	, BUS_DMAMEM_OVERRIDE_MAP	= __BIT(10)
	, BUS_DMAMEM_OVERRIDE_UNMAP	= __BIT(11)
	, BUS_DMAMEM_OVERRIDE_MMAP	= __BIT(12)
	, BUS_DMATAG_OVERRIDE_SUBREGION	= __BIT(13)
	, BUS_DMATAG_OVERRIDE_DESTROY	= __BIT(14)
};

/* Only add new members at the end of this struct! */
struct bus_space_overrides {
	int (*ov_space_map)(void *, bus_space_tag_t, bus_addr_t, bus_size_t,
	    int, bus_space_handle_t *);

	void (*ov_space_unmap)(void *, bus_space_tag_t, bus_space_handle_t,
	    bus_size_t);

	int (*ov_space_alloc)(void *, bus_space_tag_t, bus_addr_t, bus_addr_t,
	    bus_size_t, bus_size_t, bus_size_t, int, bus_addr_t *,
	    bus_space_handle_t *);

	void (*ov_space_free)(void *, bus_space_tag_t, bus_space_handle_t,
	    bus_size_t);

	int (*ov_space_reserve)(void *, bus_space_tag_t, bus_addr_t, bus_size_t,
	    int, bus_space_reservation_t *);

	void (*ov_space_release)(void *, bus_space_tag_t,
	    bus_space_reservation_t *);

	int (*ov_space_reservation_map)(void *, bus_space_tag_t,
	    bus_space_reservation_t *, int, bus_space_handle_t *);

	void (*ov_space_reservation_unmap)(void *, bus_space_tag_t,
	    bus_space_handle_t, bus_size_t);

	int (*ov_space_reserve_subregion)(void *, bus_space_tag_t,
	    bus_addr_t, bus_addr_t, bus_size_t, bus_size_t, bus_size_t,
	    int, bus_space_reservation_t *);

#if 0
	int (*ov_space_extend)(void *, bus_space_tag_t,
	    bus_space_reservation_t *, bus_size_t, bus_size_t);

	void (*ov_space_trim)(void *, bus_space_tag_t,
	    bus_space_reservation_t *, bus_size_t, bus_size_t);
#endif
};

struct mbuf;
struct uio;

/* Only add new members at the end of this struct! */
struct bus_dma_overrides {
	int (*ov_dmamap_create)(void *, bus_dma_tag_t, bus_size_t, int,
	    bus_size_t, bus_size_t, int, bus_dmamap_t *);
	void (*ov_dmamap_destroy)(void *, bus_dma_tag_t, bus_dmamap_t);
	int (*ov_dmamap_load)(void *, bus_dma_tag_t, bus_dmamap_t, void *,
	    bus_size_t, struct proc *, int);
	int (*ov_dmamap_load_mbuf)(void *, bus_dma_tag_t, bus_dmamap_t,
	    struct mbuf *, int);
	int (*ov_dmamap_load_uio)(void *, bus_dma_tag_t, bus_dmamap_t,
	    struct uio *, int);
	int (*ov_dmamap_load_raw)(void *, bus_dma_tag_t, bus_dmamap_t,
	    bus_dma_segment_t *, int, bus_size_t, int);
	void (*ov_dmamap_unload)(void *, bus_dma_tag_t, bus_dmamap_t);
	void (*ov_dmamap_sync)(void *, bus_dma_tag_t, bus_dmamap_t, bus_addr_t,
	    bus_size_t, int);
	int (*ov_dmamem_alloc)(void *, bus_dma_tag_t, bus_size_t, bus_size_t,
	    bus_size_t, bus_dma_segment_t *, int, int *, int);
	void (*ov_dmamem_free)(void *, bus_dma_tag_t, bus_dma_segment_t *, int);
	int (*ov_dmamem_map)(void *, bus_dma_tag_t, bus_dma_segment_t *, int,
	    size_t, void **, int);
	void (*ov_dmamem_unmap)(void *, bus_dma_tag_t, void *, size_t);
	paddr_t (*ov_dmamem_mmap)(void *, bus_dma_tag_t, bus_dma_segment_t *,
	    int, off_t, int, int);
	int (*ov_dmatag_subregion)(void *, bus_dma_tag_t, bus_addr_t,
	    bus_addr_t, bus_dma_tag_t *, int);
	void (*ov_dmatag_destroy)(void *, bus_dma_tag_t);
};

int	bus_space_tag_create(bus_space_tag_t, uint64_t, uint64_t,
	                     const struct bus_space_overrides *, void *,
	                     bus_space_tag_t *);
void	bus_space_tag_destroy(bus_space_tag_t);

int bus_dma_tag_create(bus_dma_tag_t, uint64_t,
    const struct bus_dma_overrides *, void *, bus_dma_tag_t *);
void bus_dma_tag_destroy(bus_dma_tag_t);

/* Reserve a region of bus space.  Reserved bus space cannot be allocated
 * with bus_space_alloc().  Reserved space has not been bus_space_map()'d.
 */
int	bus_space_reserve(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	                  bus_space_reservation_t *);

int
bus_space_reserve_subregion(bus_space_tag_t,
    bus_addr_t, bus_addr_t, bus_size_t, bus_size_t, bus_size_t,
    int, bus_space_reservation_t *);

/* Cancel a reservation. */
void	bus_space_release(bus_space_tag_t, bus_space_reservation_t *);

int bus_space_reservation_map(bus_space_tag_t, bus_space_reservation_t *,
    int, bus_space_handle_t *);

void bus_space_reservation_unmap(bus_space_tag_t, bus_space_handle_t,
    bus_size_t);

#if 0
/* Extend a reservation to the left and/or to the right.  The extension
 * has not been bus_space_map()'d.
 */
int	bus_space_extend(bus_space_tag_t, bus_space_reservation_t *, bus_size_t,
	                 bus_size_t);

/* Trim bus space from a reservation on the left and/or on the right. */
void	bus_space_trim(bus_space_tag_t, bus_space_reservation_t *, bus_size_t,
	               bus_size_t);
#endif

#include <sys/bus_proto.h>

#include <machine/bus_funcs.h>

#else /* !__HAVE_NEW_STYLE_BUS_H */

#include <machine/bus.h>

bool	bus_space_is_equal(bus_space_tag_t, bus_space_tag_t);
bool	bus_space_handle_is_equal(bus_space_tag_t, bus_space_handle_t,
    bus_space_handle_t);

#endif /* __HAVE_NEW_STYLE_BUS_H */

#ifdef __HAVE_NO_BUS_DMA
/*
 * XXX
 * Dummy bus_dma(9) stuff for ports which don't bother to have
 * unnecessary bus_dma(9) implementation to appease MI driver modules etc.
 */
typedef void *bus_dma_tag_t;

typedef struct bus_dma_segment {
	bus_addr_t ds_addr;
	bus_size_t ds_len;
} bus_dma_segment_t;

typedef struct bus_dmamap {
	bus_size_t dm_maxsegsz;
	bus_size_t dm_mapsize;
	int dm_nsegs;
	bus_dma_segment_t *dm_segs;
} *bus_dmamap_t;
#endif /* __HAVE_NO_BUS_DMA */

#endif	/* _SYS_BUS_H_ */

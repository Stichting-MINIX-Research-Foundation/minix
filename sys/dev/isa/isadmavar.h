/*	$NetBSD: isadmavar.h,v 1.26 2012/04/29 21:13:56 dsl Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 2000 The NetBSD Foundation, Inc.
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

#ifndef _DEV_ISA_ISADMAVAR_H_
#define	_DEV_ISA_ISADMAVAR_H_

#define	DMAMODE_WRITE		0x00
#define	DMAMODE_READ		0x01
#define	DMAMODE_SINGLE		0x00
#define	DMAMODE_DEMAND		0x02
#define	DMAMODE_LOOP		0x04
#define	DMAMODE_LOOPDEMAND	(DMAMODE_LOOP | DMAMODE_DEMAND)

/*
 * ISA DMA state.  This structure is provided by the ISA chipset
 * DMA entry points to the generic back-end functions that actually
 * frob the controller.
 */
struct isa_dma_state {
	device_t ids_dev;		/* associated device (for dv_xname) */
	bus_space_tag_t ids_bst;	/* bus space tag for DMA controller */
	bus_space_handle_t ids_dma1h;	/* handle for DMA controller #1 */
	bus_space_handle_t ids_dma2h;	/* handle for DMA controller #2 */
	bus_space_handle_t ids_dmapgh;	/* handle for DMA page registers */
	bus_dma_tag_t ids_dmat;		/* DMA tag for DMA controller */
	bus_dmamap_t ids_dmamaps[8];	/* DMA maps for each channel */
	bus_size_t ids_dmalength[8];	/* size of DMA transfer per channel */
	bus_size_t ids_maxsize[8];	/* max size per channel */
	int	ids_drqmap;		/* available DRQs (bitmap) */
	int	ids_dmareads;		/* state for isa_dmadone() (bitmap) */
	int	ids_dmafinished;	/* DMA completion state (bitmap) */
	int	ids_masked;		/* masked channels (bitmap) */
	int	ids_frozen;		/* `frozen' count */
	int	ids_initialized;	/* only initialize once... */
};

#define	ISA_DMA_DRQ_ISFREE(state, drq)					\
	(((state)->ids_drqmap & (1 << (drq))) == 0)

#define	ISA_DMA_DRQ_ALLOC(state, drq)					\
	(state)->ids_drqmap |= (1 << (drq))

#define	ISA_DMA_DRQ_FREE(state, drq)					\
	(state)->ids_drqmap &= ~(1 << (drq))

#define	ISA_DMA_MASK_SET(state, drq)					\
	(state)->ids_masked |= (1 << (drq))

#define	ISA_DMA_MASK_CLR(state, drq)					\
	(state)->ids_masked &= ~(1 << (drq))

/*
 * Memory list used by _isa_malloc().
 */
struct isa_mem {
	struct isa_dma_state *ids;
	int chan;
	bus_size_t size;
	bus_addr_t addr;
	void *kva;
	struct isa_mem *next;
};

#ifdef _KERNEL
struct proc;

void	   _isa_dmainit(struct isa_dma_state *, bus_space_tag_t,
	       bus_dma_tag_t, device_t);

void	   _isa_dmadestroy(struct isa_dma_state *);

int	   _isa_dmacascade(struct isa_dma_state *, int);
int	   _isa_dmacascade_stop(struct isa_dma_state *, int);

bus_size_t _isa_dmamaxsize(struct isa_dma_state *, int);

int	   _isa_dmamap_create(struct isa_dma_state *, int, bus_size_t, int);
void	   _isa_dmamap_destroy(struct isa_dma_state *, int);

int	   _isa_dmastart(struct isa_dma_state *, int, void *, bus_size_t,
	       struct proc *, int, int);
void	   _isa_dmaabort(struct isa_dma_state *, int);
bus_size_t _isa_dmacount(struct isa_dma_state *, int);
int	   _isa_dmafinished(struct isa_dma_state *, int);
void	   _isa_dmadone(struct isa_dma_state *, int);

void	   _isa_dmafreeze(struct isa_dma_state *);
void	   _isa_dmathaw(struct isa_dma_state *);

int	   _isa_dmamem_alloc(struct isa_dma_state *, int, bus_size_t,
	       bus_addr_t *, int);
void	   _isa_dmamem_free(struct isa_dma_state *, int, bus_addr_t,
	       bus_size_t);
int	   _isa_dmamem_map(struct isa_dma_state *, int, bus_addr_t,
	       bus_size_t, void **, int);
void	   _isa_dmamem_unmap(struct isa_dma_state *, int, void *,
	       size_t);
paddr_t	   _isa_dmamem_mmap(struct isa_dma_state *, int, bus_addr_t,
	       bus_size_t, off_t, int, int);

int	   _isa_drq_alloc(struct isa_dma_state *, int);
int	   _isa_drq_free(struct isa_dma_state *, int);
int	   _isa_drq_isfree(struct isa_dma_state *, int);

#define _isa_malloc(dma_state, c, s, p, f) \
    _isa_malloc(dma_state, c, s, f)
#define _isa_free(v, p) _isa_free(v)

void      *_isa_malloc(struct isa_dma_state *, int, size_t,
		struct malloc_type *, int);
void	   _isa_free(void *, struct malloc_type *);
paddr_t	   _isa_mappage(void *, off_t, int);
#endif /* _KERNEL */

#endif /* _DEV_ISA_ISADMAVAR_H_ */

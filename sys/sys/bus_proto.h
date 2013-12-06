/*	$NetBSD: bus_proto.h,v 1.7 2013/02/04 13:18:35 macallan Exp $	*/

/*-
 * Copyright (c) 1996, 1997, 1998, 2001, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center, and by Andrew Doran.
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
 * Copyright (c) 1996 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#ifndef _SYS_BUS_PROTO_H_
#define _SYS_BUS_PROTO_H_

/*
 * Forwards needed by prototypes below.
 */
struct mbuf;
struct uio;

/*
 * bus_space(9)
 */

/* Map types. */
#define	BUS_SPACE_MAP_CACHEABLE		0x01
#define	BUS_SPACE_MAP_LINEAR		0x02
#define	BUS_SPACE_MAP_PREFETCHABLE	0x04

/* Bus read/write barrier methods. */
#define	BUS_SPACE_BARRIER_READ	0x01		/* force read barrier */
#define	BUS_SPACE_BARRIER_WRITE	0x02		/* force write barrier */

int	bus_space_map(bus_space_tag_t, bus_addr_t, bus_size_t, int,
		      bus_space_handle_t *);

void	bus_space_unmap(bus_space_tag_t, bus_space_handle_t, bus_size_t);

int	bus_space_subregion(bus_space_tag_t, bus_space_handle_t,
			    bus_size_t, bus_size_t, bus_space_handle_t *);

int	bus_space_alloc(bus_space_tag_t, bus_addr_t, bus_addr_t,
			bus_size_t, bus_size_t, bus_size_t,
			int, bus_addr_t *, bus_space_handle_t *);

void	bus_space_free(bus_space_tag_t, bus_space_handle_t, bus_size_t);

paddr_t	bus_space_mmap(bus_space_tag_t, bus_addr_t, off_t, int, int);

void	*bus_space_vaddr(bus_space_tag_t, bus_space_handle_t);

void	bus_space_barrier(bus_space_tag_t tag, bus_space_handle_t bsh,
			  bus_size_t offset, bus_size_t len, int flags);

/*
 * bus_space(9) accessors
 */

uint8_t	bus_space_read_1(bus_space_tag_t, bus_space_handle_t,
			 bus_size_t);
uint8_t	bus_space_read_stream_1(bus_space_tag_t, bus_space_handle_t,
				bus_size_t);

uint16_t bus_space_read_2(bus_space_tag_t, bus_space_handle_t,
			  bus_size_t);
uint16_t bus_space_read_stream_2(bus_space_tag_t, bus_space_handle_t,
				 bus_size_t);

uint32_t bus_space_read_4(bus_space_tag_t, bus_space_handle_t,
			  bus_size_t);
uint32_t bus_space_read_stream_4(bus_space_tag_t, bus_space_handle_t,
				 bus_size_t);

uint64_t bus_space_read_8(bus_space_tag_t, bus_space_handle_t,
			  bus_size_t);
uint64_t bus_space_read_stream_8(bus_space_tag_t, bus_space_handle_t,
				 bus_size_t);

void	bus_space_read_multi_1(bus_space_tag_t, bus_space_handle_t,
			       bus_size_t, uint8_t *, bus_size_t);
void	bus_space_read_multi_stream_1(bus_space_tag_t, bus_space_handle_t,
				      bus_size_t, uint8_t *, bus_size_t);
void	bus_space_read_region_1(bus_space_tag_t, bus_space_handle_t,
			        bus_size_t, uint8_t *, bus_size_t);
void	bus_space_read_region_stream_1(bus_space_tag_t, bus_space_handle_t,
				       bus_size_t, uint8_t *, bus_size_t);

void	bus_space_read_multi_2(bus_space_tag_t, bus_space_handle_t,
			       bus_size_t, uint16_t *, bus_size_t);
void	bus_space_read_multi_stream_2(bus_space_tag_t, bus_space_handle_t,
				      bus_size_t, uint16_t *, bus_size_t);
void	bus_space_read_region_2(bus_space_tag_t, bus_space_handle_t,
			        bus_size_t, uint16_t *, bus_size_t);
void	bus_space_read_region_stream_2(bus_space_tag_t, bus_space_handle_t,
				       bus_size_t, uint16_t *, bus_size_t);

void	bus_space_read_multi_4(bus_space_tag_t, bus_space_handle_t,
			       bus_size_t, uint32_t *, bus_size_t);
void	bus_space_read_multi_stream_4(bus_space_tag_t, bus_space_handle_t,
				      bus_size_t, uint32_t *, bus_size_t);
void	bus_space_read_region_4(bus_space_tag_t, bus_space_handle_t,
			        bus_size_t, uint32_t *, bus_size_t);
void	bus_space_read_region_stream_4(bus_space_tag_t, bus_space_handle_t,
				       bus_size_t, uint32_t *, bus_size_t);

void	bus_space_read_multi_8(bus_space_tag_t, bus_space_handle_t,
			       bus_size_t, uint64_t *, bus_size_t);
void	bus_space_read_multi_stream_8(bus_space_tag_t, bus_space_handle_t,
				      bus_size_t, uint64_t *, bus_size_t);
void	bus_space_read_region_8(bus_space_tag_t, bus_space_handle_t,
			        bus_size_t, uint64_t *, bus_size_t);
void	bus_space_read_region_stream_8(bus_space_tag_t, bus_space_handle_t,
				       bus_size_t, uint64_t *, bus_size_t);

void	bus_space_write_1(bus_space_tag_t, bus_space_handle_t,
			  bus_size_t, uint8_t);
void	bus_space_write_stream_1(bus_space_tag_t, bus_space_handle_t,
				 bus_size_t, uint8_t);

void	bus_space_write_2(bus_space_tag_t, bus_space_handle_t,
			  bus_size_t, uint16_t);
void	bus_space_write_stream_2(bus_space_tag_t, bus_space_handle_t,
		  		 bus_size_t, uint16_t);

void	bus_space_write_4(bus_space_tag_t, bus_space_handle_t,
			  bus_size_t, uint32_t);
void	bus_space_write_stream_4(bus_space_tag_t, bus_space_handle_t,
		  		 bus_size_t, uint32_t);

void	bus_space_write_8(bus_space_tag_t, bus_space_handle_t,
			  bus_size_t, uint64_t);
void	bus_space_write_stream_8(bus_space_tag_t, bus_space_handle_t,
		  		 bus_size_t, uint64_t);

void	bus_space_write_multi_1(bus_space_tag_t, bus_space_handle_t,
			        bus_size_t, const uint8_t *,
			        bus_size_t);
void	bus_space_write_multi_stream_1(bus_space_tag_t, bus_space_handle_t,
				       bus_size_t, const uint8_t *,
				       bus_size_t);
void	bus_space_write_region_1(bus_space_tag_t, bus_space_handle_t,
			         bus_size_t, const uint8_t *,
			         bus_size_t);
void	bus_space_write_region_stream_1(bus_space_tag_t, bus_space_handle_t,
				        bus_size_t, const uint8_t *,
					bus_size_t);

void	bus_space_write_multi_2(bus_space_tag_t, bus_space_handle_t,
			        bus_size_t, const uint16_t *,
			        bus_size_t);
void	bus_space_write_multi_stream_2(bus_space_tag_t, bus_space_handle_t,
				       bus_size_t, const uint16_t *,
				       bus_size_t);
void	bus_space_write_region_2(bus_space_tag_t, bus_space_handle_t,
			         bus_size_t, const uint16_t *,
			         bus_size_t);
void	bus_space_write_region_stream_2(bus_space_tag_t, bus_space_handle_t,
				        bus_size_t, const uint16_t *,
				        bus_size_t);

void	bus_space_write_multi_4(bus_space_tag_t, bus_space_handle_t,
			        bus_size_t, const uint32_t *,
			        bus_size_t);
void	bus_space_write_multi_stream_4(bus_space_tag_t, bus_space_handle_t,
				       bus_size_t, const uint32_t *,
				       bus_size_t);
void	bus_space_write_region_4(bus_space_tag_t, bus_space_handle_t,
			         bus_size_t, const uint32_t *,
			         bus_size_t);
void	bus_space_write_region_stream_4(bus_space_tag_t, bus_space_handle_t,
				        bus_size_t, const uint32_t *,
				        bus_size_t);

void	bus_space_write_multi_8(bus_space_tag_t, bus_space_handle_t,
			        bus_size_t, const uint64_t *,
			        bus_size_t);
void	bus_space_write_multi_stream_8(bus_space_tag_t, bus_space_handle_t,
				       bus_size_t, const uint64_t *,
				       bus_size_t);
void	bus_space_write_region_8(bus_space_tag_t, bus_space_handle_t,
			         bus_size_t, const uint64_t *,
			         bus_size_t);
void	bus_space_write_region_stream_8(bus_space_tag_t, bus_space_handle_t,
				        bus_size_t, const uint64_t *,
				        bus_size_t);

void	bus_space_set_multi_1(bus_space_tag_t, bus_space_handle_t,
			      bus_size_t, u_int8_t, bus_size_t);
void	bus_space_set_multi_2(bus_space_tag_t, bus_space_handle_t,
			      bus_size_t, u_int16_t, bus_size_t);
void	bus_space_set_multi_4(bus_space_tag_t, bus_space_handle_t,
			      bus_size_t, u_int32_t, bus_size_t);
void	bus_space_set_multi_8(bus_space_tag_t, bus_space_handle_t,
			      bus_size_t, u_int64_t, bus_size_t);

void	bus_space_set_multi_stream_1(bus_space_tag_t, bus_space_handle_t,
			      bus_size_t, u_int8_t, bus_size_t);
void	bus_space_set_multi_stream_2(bus_space_tag_t, bus_space_handle_t,
			      bus_size_t, u_int16_t, bus_size_t);
void	bus_space_set_multi_stream_4(bus_space_tag_t, bus_space_handle_t,
			      bus_size_t, u_int32_t, bus_size_t);
void	bus_space_set_multi_stream_8(bus_space_tag_t, bus_space_handle_t,
			      bus_size_t, u_int64_t, bus_size_t);

void	bus_space_set_region_1(bus_space_tag_t, bus_space_handle_t,
			       bus_size_t, u_int8_t, bus_size_t);
void	bus_space_set_region_2(bus_space_tag_t, bus_space_handle_t,
			       bus_size_t, u_int16_t, bus_size_t);
void	bus_space_set_region_4(bus_space_tag_t, bus_space_handle_t,
			       bus_size_t, u_int32_t, bus_size_t);
void	bus_space_set_region_8(bus_space_tag_t, bus_space_handle_t,
			       bus_size_t, u_int64_t, bus_size_t);

void	bus_space_set_region_stream_1(bus_space_tag_t, bus_space_handle_t,
			       bus_size_t, u_int8_t, bus_size_t);
void	bus_space_set_region_stream_2(bus_space_tag_t, bus_space_handle_t,
			       bus_size_t, u_int16_t, bus_size_t);
void	bus_space_set_region_stream_4(bus_space_tag_t, bus_space_handle_t,
			       bus_size_t, u_int32_t, bus_size_t);
void	bus_space_set_region_stream_8(bus_space_tag_t, bus_space_handle_t,
			       bus_size_t, u_int64_t, bus_size_t);

void	bus_space_copy_region_1(bus_space_tag_t, bus_space_handle_t,
				bus_size_t, bus_space_handle_t,
				bus_size_t, bus_size_t);
void	bus_space_copy_region_2(bus_space_tag_t, bus_space_handle_t,
				bus_size_t, bus_space_handle_t,
				bus_size_t, bus_size_t);
void	bus_space_copy_region_4(bus_space_tag_t, bus_space_handle_t,
				bus_size_t, bus_space_handle_t,
				bus_size_t, bus_size_t);
void	bus_space_copy_region_8(bus_space_tag_t, bus_space_handle_t,
				bus_size_t, bus_space_handle_t,
				bus_size_t, bus_size_t);

void	bus_space_copy_region_stream_1(bus_space_tag_t, bus_space_handle_t,
				bus_size_t, bus_space_handle_t,
				bus_size_t, bus_size_t);
void	bus_space_copy_region_stream_2(bus_space_tag_t, bus_space_handle_t,
				bus_size_t, bus_space_handle_t,
				bus_size_t, bus_size_t);
void	bus_space_copy_region_stream_4(bus_space_tag_t, bus_space_handle_t,
				bus_size_t, bus_space_handle_t,
				bus_size_t, bus_size_t);
void	bus_space_copy_region_stream_8(bus_space_tag_t, bus_space_handle_t,
				bus_size_t, bus_space_handle_t,
				bus_size_t, bus_size_t);

bool	bus_space_is_equal(bus_space_tag_t, bus_space_tag_t);
bool	bus_space_handle_is_equal(bus_space_tag_t, bus_space_handle_t,
    bus_space_handle_t);

/*
 * bus_dma(9)
 */

/* Flags used in various bus DMA methods. */
#define	BUS_DMA_WAITOK		0x000	/* safe to sleep (pseudo-flag) */
#define	BUS_DMA_NOWAIT		0x001	/* not safe to sleep */
#define	BUS_DMA_ALLOCNOW	0x002	/* perform resource allocation now */
#define	BUS_DMA_COHERENT	0x004	/* hint: map memory DMA coherent */
#define	BUS_DMA_STREAMING	0x008	/* hint: sequential, unidirectional */
#define	BUS_DMA_BUS1		0x010	/* placeholders for bus functions... */
#define	BUS_DMA_BUS2		0x020
#define	BUS_DMA_BUS3		0x040
#define	BUS_DMA_BUS4		0x080
#define	BUS_DMA_READ		0x100	/* mapping is device -> memory only */
#define	BUS_DMA_WRITE		0x200	/* mapping is memory -> device only */
#define	BUS_DMA_NOCACHE		0x400	/* hint: map non-cached memory */
#define	BUS_DMA_PREFETCHABLE	0x800	/* hint: map non-cached but allow 
					 * things like write combining */

/* Operations performed by bus_dmamap_sync(). */
#define	BUS_DMASYNC_PREREAD	0x01	/* pre-read synchronization */
#define	BUS_DMASYNC_POSTREAD	0x02	/* post-read synchronization */
#define	BUS_DMASYNC_PREWRITE	0x04	/* pre-write synchronization */
#define	BUS_DMASYNC_POSTWRITE	0x08	/* post-write synchronization */

int	bus_dmamap_create(bus_dma_tag_t, bus_size_t, int, bus_size_t,
			  bus_size_t, int, bus_dmamap_t *);
void	bus_dmamap_destroy(bus_dma_tag_t, bus_dmamap_t);
int	bus_dmamap_load(bus_dma_tag_t, bus_dmamap_t, void *, bus_size_t,
			struct proc *, int);
int	bus_dmamap_load_mbuf(bus_dma_tag_t, bus_dmamap_t,
			     struct mbuf *, int);
int	bus_dmamap_load_uio(bus_dma_tag_t, bus_dmamap_t,
			    struct uio *, int);
int	bus_dmamap_load_raw(bus_dma_tag_t, bus_dmamap_t,
			    bus_dma_segment_t *, int, bus_size_t, int);
void	bus_dmamap_unload(bus_dma_tag_t, bus_dmamap_t);
void	bus_dmamap_sync(bus_dma_tag_t, bus_dmamap_t, bus_addr_t,
			bus_size_t, int);

int	bus_dmamem_alloc(bus_dma_tag_t, bus_size_t, bus_size_t,
			 bus_size_t, bus_dma_segment_t *,
			 int, int *, int);
void	bus_dmamem_free(bus_dma_tag_t, bus_dma_segment_t *, int);
int	bus_dmamem_map(bus_dma_tag_t, bus_dma_segment_t *, int,
		       size_t, void **, int);
void	bus_dmamem_unmap(bus_dma_tag_t, void *, size_t);
paddr_t	bus_dmamem_mmap(bus_dma_tag_t, bus_dma_segment_t *, int,
			off_t, int, int);

int	bus_dmatag_subregion(bus_dma_tag_t, bus_addr_t, bus_addr_t,
			     bus_dma_tag_t *, int);
void	bus_dmatag_destroy(bus_dma_tag_t);

#endif	/* _SYS_BUS_PROTO_H_ */

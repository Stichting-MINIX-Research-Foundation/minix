/*$NetBSD: ixgbe_netbsd.h,v 1.3 2015/04/24 07:00:51 msaitoh Exp $*/
/*
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Coyote Point Systems, Inc.
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

#ifndef _IXGBE_NETBSD_H
#define _IXGBE_NETBSD_H

#define	IXGBE_LEGACY_TX	1

#define	ETHERCAP_VLAN_HWFILTER	0
#define	ETHERCAP_VLAN_HWCSUM	0
#define	MJUM9BYTES	(9 * 1024)
#define	MJUM16BYTES	(16 * 1024)
#define	MJUMPAGESIZE	PAGE_SIZE

#define IFCAP_RXCSUM	\
	(IFCAP_CSUM_IPv4_Rx|IFCAP_CSUM_TCPv4_Rx|IFCAP_CSUM_UDPv4_Rx|\
	IFCAP_CSUM_TCPv6_Rx|IFCAP_CSUM_UDPv6_Rx)

#define IFCAP_TXCSUM	\
	(IFCAP_CSUM_IPv4_Tx|IFCAP_CSUM_TCPv4_Tx|IFCAP_CSUM_UDPv4_Tx|\
	IFCAP_CSUM_TCPv6_Tx|IFCAP_CSUM_UDPv6_Tx)

#define IFCAP_HWCSUM	(IFCAP_RXCSUM|IFCAP_TXCSUM)

#define	ETHER_ALIGN		2

struct ixgbe_dma_tag {
	bus_dma_tag_t	dt_dmat;
	bus_size_t	dt_alignment;
	bus_size_t	dt_boundary;
	bus_size_t	dt_maxsize;
	int		dt_nsegments;
	bus_size_t	dt_maxsegsize;
	int		dt_flags;
};

typedef struct ixgbe_dma_tag ixgbe_dma_tag_t;

struct ixgbe_extmem_head;
typedef struct ixgbe_extmem_head ixgbe_extmem_head_t;

struct ixgbe_extmem {
	ixgbe_extmem_head_t		*em_head;
	bus_dma_tag_t			em_dmat;
	bus_size_t			em_size;
	bus_dma_segment_t		em_seg;
	void				*em_vaddr;
	TAILQ_ENTRY(ixgbe_extmem)	em_link;
};

typedef struct ixgbe_extmem ixgbe_extmem_t;

struct ixgbe_extmem_head {
	TAILQ_HEAD(, ixgbe_extmem)	eh_freelist;
	kmutex_t			eh_mtx;
	bool				eh_initialized;
};

int ixgbe_dma_tag_create(bus_dma_tag_t, bus_size_t, bus_size_t, bus_size_t, int,
    bus_size_t, int, ixgbe_dma_tag_t **);
void ixgbe_dma_tag_destroy(ixgbe_dma_tag_t *);
int ixgbe_dmamap_create(ixgbe_dma_tag_t *, int, bus_dmamap_t *);
void ixgbe_dmamap_destroy(ixgbe_dma_tag_t *, bus_dmamap_t);
void ixgbe_dmamap_sync(ixgbe_dma_tag_t *, bus_dmamap_t, int);
void ixgbe_dmamap_unload(ixgbe_dma_tag_t *, bus_dmamap_t);

void ixgbe_jcl_reinit(ixgbe_extmem_head_t *, bus_dma_tag_t, int, size_t);
struct mbuf *ixgbe_getjcl(ixgbe_extmem_head_t *, int, int, int, size_t);

#endif /* _IXGBE_NETBSD_H */

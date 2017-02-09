/*	$NetBSD: if_gfevar.h,v 1.13 2015/04/14 20:32:36 riastradh Exp $	*/

/*
 * Copyright (c) 2002 Allegro Networks, Inc., Wasabi Systems, Inc.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Allegro Networks, Inc., and Wasabi Systems, Inc.
 * 4. The name of Allegro Networks, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 * 5. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ALLEGRO NETWORKS, INC. AND
 * WASABI SYSTEMS, INC. ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL EITHER ALLEGRO NETWORKS, INC. OR WASABI SYSTEMS, INC.
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _IF_GFEVAR_H_
#define _IF_GFEVAR_H_

#include <sys/rndsource.h>

#define	GE_RXDESC_MEMSIZE		(1 * PAGE_SIZE)
#define	GE_RXDESC_MAX			64
#define	GE_RXBUF_SIZE			2048
#define	GE_RXBUF_MEMSIZE		(GE_RXDESC_MAX*GE_RXBUF_SIZE)
#define	GE_RXBUF_NSEGS			((GE_RXBUF_MEMSIZE/PAGE_SIZE)+1)
#define	GE_DMSEG_MAX			(GE_RXBUF_NSEGS)

struct gfe_dmamem {
	bus_dmamap_t gdm_map;		/* dmamem'ed memory */
	void *gdm_kva;		/* kva of tx memory */
	int gdm_nsegs;			/* # of segment in gdm_segs */
	int gdm_maxsegs;		/* maximum # of segments allowed */
	size_t gdm_size;		/* size of memory region */
	bus_dma_segment_t gdm_segs[GE_DMSEG_MAX]; /* dma segment of tx memory */
};

/* With a 4096 page size, we get 256 descriptors per page.
 */
#define	GE_TXDESC_MEMSIZE		(1 * PAGE_SIZE)
#define	GE_TXDESC_MAX			(GE_TXDESC_MEMSIZE / 16)
#define	GE_TXBUF_SIZE			(4 * PAGE_SIZE)

struct gfe_txqueue {
	struct ifqueue txq_pendq;	/* these are ready to go to the GT */
	struct gfe_dmamem txq_desc_mem;	/* transmit descriptor memory */
	struct gfe_dmamem txq_buf_mem;	/* transmit buffer memory */
	unsigned int txq_lo;		/* next to be given to GT */
	unsigned int txq_fi; 		/* next to be returned to CPU */
	unsigned int txq_ei_gapcount;	/* counter until next EI */
	unsigned int txq_nactive;	/* number of active descriptors */
	unsigned int txq_outptr;	/* where to put next transmit packet */
	unsigned int txq_inptr;		/* start of 1st queued tx packet */
	uint32_t txq_intrbits;		/* bits to write to EIMR */
	uint32_t txq_esdcmrbits;	/* bits to write to ESDCMR */
	uint32_t txq_epsrbits;		/* bits to test with EPSR */
	volatile struct gt_eth_desc *txq_descs; /* ptr to tx descriptors */
	bus_addr_t txq_ectdp;		/* offset to cur. tx desc ptr reg */
	bus_addr_t txq_desc_busaddr;	/* bus addr of tx descriptors */
	bus_addr_t txq_buf_busaddr;	/* bus addr of tx buffers */
};

/* With a 4096 page size, we get 256 descriptors per page.  We want 1024
 * which will give us about 8ms of 64 byte packets (2ms for each priority
 * queue).
 */

struct gfe_rxbuf {
	uint8_t	rxb_data[GE_RXBUF_SIZE];
};

struct gfe_rxqueue {
	struct gfe_dmamem rxq_desc_mem;	/* receive descriptor memory */
	struct gfe_dmamem rxq_buf_mem;	/* receive buffer memory */
	struct mbuf *rxq_curpkt;	/* mbuf for current packet */
	volatile struct gt_eth_desc *rxq_descs;
	struct gfe_rxbuf *rxq_bufs;
	unsigned int rxq_fi; 		/* next to be returned to CPU */
	unsigned int rxq_active;	/* # of descriptors given to GT */
	uint32_t rxq_intrbits;		/* bits to write to EIMR */
	bus_addr_t rxq_desc_busaddr;	/* bus addr of rx descriptors */
	uint32_t rxq_cmdsts;		/* save cmdsts from first descriptor */
	bus_size_t rxq_efrdp;
	bus_size_t rxq_ecrdp;
};

enum gfe_txprio {
	GE_TXPRIO_HI=1,
	GE_TXPRIO_LO=0,
	GE_TXPRIO_NONE=2
};
enum gfe_rxprio {
	GE_RXPRIO_HI=3,
	GE_RXPRIO_MEDHI=2,
	GE_RXPRIO_MEDLO=1,
	GE_RXPRIO_LO=0
};

struct gfec_softc {
	device_t sc_dev;		/* must be first */

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;	/* subregion for ethernet */

	kmutex_t sc_mtx;
};

struct gfe_softc {
	device_t sc_dev;		/* must be first */
	struct ethercom sc_ec;		/* common ethernet glue */
	struct callout sc_co;		/* resource recovery */
	mii_data_t sc_mii;		/* mii interface */

	bus_space_tag_t sc_memt;
	bus_space_handle_t sc_memh;	/* subregion for ethernet */
	bus_dma_tag_t sc_dmat;
	int sc_macno;			/* which mac? 0, 1, or 2 */

	unsigned int sc_tickflags;
#define	GE_TICK_TX_IFSTART	0x0001
#define	GE_TICK_RX_RESTART	0x0002
	unsigned int sc_flags;
#define	GE_ALLMULTI	0x0001
#define	GE_PHYSTSCHG	0x0002
#define	GE_RXACTIVE	0x0004
#define	GE_NOFREE	0x0008		/* Don't free on disable */
	uint32_t sc_pcr;		/* current EPCR value */
	uint32_t sc_pcxr;		/* current EPCXR value */
	uint32_t sc_intrmask;		/* current EIMR value */
	uint32_t sc_idlemask;		/* suspended EIMR bits */
	size_t sc_max_frame_length;	/* maximum frame length */

	/*
	 * Hash table related members
	 */
	struct gfe_dmamem sc_hash_mem;	/* dma'ble hash table */
	uint64_t *sc_hashtable;
	unsigned int sc_hashmask;	/* 0x1ff or 0x1fff */

	/*
	 * Transmit related members
	 */
	struct gfe_txqueue sc_txq[2];	/* High & Low transmit queues */

	/*
	 * Receive related members
	 */
	struct gfe_rxqueue sc_rxq[4];	/* Hi/MedHi/MedLo/Lo receive queues */

	krndsource_t sc_rnd_source;
};
#endif	/* _IF_GFEVAR_H_ */

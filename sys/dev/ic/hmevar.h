/*	$NetBSD: hmevar.h,v 1.24 2015/04/13 16:33:24 riastradh Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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

#include <sys/callout.h>
#include <sys/rndsource.h>

struct hme_ring {
	/* Ring Descriptors */
	void *		rb_membase;	/* Packet buffer: CPU address */
	bus_addr_t	rb_dmabase;	/* Packet buffer: DMA address */
	void *		rb_txd;		/* Transmit descriptors */
	bus_addr_t	rb_txddma;	/* DMA address of same */
	void *		rb_rxd;		/* Receive descriptors */
	bus_addr_t	rb_rxddma;	/* DMA address of same */
	void *		rb_txbuf;	/* Transmit buffers */
	void *		rb_rxbuf;	/* Receive buffers */
	int		rb_ntbuf;	/* # of transmit buffers */
	int		rb_nrbuf;	/* # of receive buffers */

	/* Ring Descriptor state */
	int	rb_tdhead, rb_tdtail;
	int	rb_rdtail;
	int	rb_td_nbusy;
};

struct hme_softc {
	device_t	sc_dev;		/* boilerplate device view */
	struct ethercom	sc_ethercom;	/* Ethernet common part */
	struct mii_data	sc_mii;		/* MII media control */
	struct callout	sc_tick_ch;	/* tick callout */

	/* The following bus handles are to be provided by the bus front-end */
	bus_space_tag_t	sc_bustag;	/* bus tag */
	bus_dma_tag_t	sc_dmatag;	/* bus dma tag */
	bus_dmamap_t	sc_dmamap;	/* bus dma handle */
	bus_space_handle_t sc_seb;	/* HME Global registers */
	bus_space_handle_t sc_erx;	/* HME ERX registers */
	bus_space_handle_t sc_etx;	/* HME ETX registers */
	bus_space_handle_t sc_mac;	/* HME MAC registers */
	bus_space_handle_t sc_mif;	/* HME MIF registers */
	int		sc_burst;	/* DVMA burst size in effect */
	int		sc_phys[2];	/* MII instance -> PHY map */

	int		sc_pci;		/* XXXXX -- PCI buses are LE. */

	/* Ring descriptor */
	struct hme_ring		sc_rb;
#if notused
	void		(*sc_copytobuf)(struct hme_softc *,
					     void *, void *, size_t);
	void		(*sc_copyfrombuf)(struct hme_softc *,
					      void *, void *, size_t);
#endif

	int			sc_debug;
	int			sc_ec_capenable;
	short			sc_if_flags;
	uint8_t			sc_enaddr[ETHER_ADDR_LEN]; /* MAC address */

	/* Special hardware hooks */
	void	(*sc_hwreset)(struct hme_softc *);
	void	(*sc_hwinit)(struct hme_softc *);

	krndsource_t	rnd_source;
};


void	hme_config(struct hme_softc *);
int	hme_intr(void *);

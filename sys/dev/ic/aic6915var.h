/*	$NetBSD: aic6915var.h,v 1.4 2012/10/27 17:18:19 chs Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

#ifndef _DEV_IC_AIC6915VAR_H_
#define	_DEV_IC_AIC6915VAR_H_

#include <sys/callout.h>

/*
 * Data structure definitions for the Adaptec AIC-6915 (``Starfire'')
 * PCI 10/100 Ethernet controller driver.
 */

/*
 * Transmit descriptor list size.
 */
#define	SF_NTXDESC		256
#define	SF_NTXDESC_MASK		(SF_NTXDESC - 1)
#define	SF_NEXTTX(x)		((x + 1) & SF_NTXDESC_MASK)

/*
 * Transmit completion queue size.  1024 is a hardware requirement.
 */
#define	SF_NTCD			1024
#define	SF_NTCD_MASK		(SF_NTCD - 1)
#define	SF_NEXTTCD(x)		((x + 1) & SF_NTCD_MASK)

/*
 * Receive descriptor list size.
 */
#define	SF_NRXDESC		256
#define	SF_NRXDESC_MASK		(SF_NRXDESC - 1)
#define	SF_NEXTRX(x)		((x + 1) & SF_NRXDESC_MASK)

/*
 * Receive completion queue size.  1024 is a hardware requirement.
 */
#define	SF_NRCD			1024
#define	SF_NRCD_MASK		(SF_NRCD - 1)
#define	SF_NEXTRCD(x)		((x + 1) & SF_NRCD_MASK)

/*
 * Control structures are DMA to the Starfire chip.  We allocate them in
 * a single clump that maps to a single DMA segment to make several things
 * easier.
 */
struct sf_control_data {
	/*
	 * The transmit descriptors.
	 */
	struct sf_txdesc0 scd_txdescs[SF_NTXDESC];

	/*
	 * The transmit completion queue entires.
	 */
	struct sf_tcd scd_txcomp[SF_NTCD];

	/*
	 * The receive buffer descriptors.
	 */
	struct sf_rbd32 scd_rxbufdescs[SF_NRXDESC];

	/*
	 * The receive completion queue entries.
	 */
	struct sf_rcd_full scd_rxcomp[SF_NRCD];
};

#define	SF_CDOFF(x)		offsetof(struct sf_control_data, x)
#define	SF_CDTXDOFF(x)		SF_CDOFF(scd_txdescs[(x)])
#define	SF_CDTXCOFF(x)		SF_CDOFF(scd_txcomp[(x)])
#define	SF_CDRXDOFF(x)		SF_CDOFF(scd_rxbufdescs[(x)])
#define	SF_CDRXCOFF(x)		SF_CDOFF(scd_rxcomp[(x)])

/*
 * Software state for transmit and receive descriptors.
 */
struct sf_descsoft {
	struct mbuf *ds_mbuf;		/* head of mbuf chain */
	bus_dmamap_t ds_dmamap;		/* our DMA map */
};

/*
 * Software state per device.
 */
struct sf_softc {
	device_t sc_dev;		/* generic device information */
	bus_space_tag_t sc_st;		/* bus space tag */
	bus_space_handle_t sc_sh;	/* bus space handle */
	bus_space_handle_t sc_sh_func;	/* sub-handle for func regs */
	bus_dma_tag_t sc_dmat;		/* bus DMA tag */
	struct ethercom sc_ethercom;	/* ethernet common data */
	int sc_iomapped;		/* are we I/O mapped? */

	struct mii_data sc_mii;		/* MII/media information */
	struct callout sc_tick_callout;	/* MII callout */

	bus_dmamap_t sc_cddmamap;	/* control data DMA map */
#define	sc_cddma	sc_cddmamap->dm_segs[0].ds_addr

	/*
	 * Software state for transmit and receive descriptors.
	 */
	struct sf_descsoft sc_txsoft[SF_NTXDESC];
	struct sf_descsoft sc_rxsoft[SF_NRXDESC];

	/*
	 * Control data structures.
	 */
	struct sf_control_data *sc_control_data;
#define	sc_txdescs	sc_control_data->scd_txdescs
#define	sc_txcomp	sc_control_data->scd_txcomp
#define	sc_rxbufdescs	sc_control_data->scd_rxbufdescs
#define	sc_rxcomp	sc_control_data->scd_rxcomp

	int	sc_txpending;		/* number of Tx requests pending */

	uint32_t sc_InterruptEn;	/* prototype InterruptEn register */

	uint32_t sc_TransmitFrameCSR;	/* prototype TransmitFrameCSR reg */
	uint32_t sc_TxDescQueueCtrl;	/* prototype TxDescQueueCtrl reg */
	int	sc_txthresh;		/* current Tx threshold */

	uint32_t sc_MacConfig1;		/* prototype MacConfig1 register */

	uint32_t sc_RxAddressFilteringCtl;
};

#define	SF_CDTXDADDR(sc, x)	((sc)->sc_cddma + SF_CDTXDOFF((x)))
#define	SF_CDTXCADDR(sc, x)	((sc)->sc_cddma + SF_CDTXCOFF((x)))
#define	SF_CDRXDADDR(sc, x)	((sc)->sc_cddma + SF_CDRXDOFF((x)))
#define	SF_CDRXCADDR(sc, x)	((sc)->sc_cddma + SF_CDRXCOFF((x)))

#define	SF_CDTXDSYNC(sc, x, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    SF_CDTXDOFF((x)), sizeof(struct sf_txdesc0), (ops))

#define	SF_CDTXCSYNC(sc, x, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    SF_CDTXCOFF((x)), sizeof(struct sf_tcd), (ops))

#define	SF_CDRXDSYNC(sc, x, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    SF_CDRXDOFF((x)), sizeof(struct sf_rbd32), (ops))

#define	SF_CDRXCSYNC(sc, x, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    SF_CDRXCOFF((x)), sizeof(struct sf_rcd_full), (ops))

#define	SF_INIT_RXDESC(sc, x)						\
do {									\
	struct sf_descsoft *__ds = &sc->sc_rxsoft[(x)];			\
									\
	(sc)->sc_rxbufdescs[(x)].rbd32_addr =				\
	    __ds->ds_dmamap->dm_segs[0].ds_addr | RBD_V;		\
	SF_CDRXDSYNC((sc), (x), BUS_DMASYNC_PREWRITE);			\
} while (/*CONSTCOND*/0)

#ifdef _KERNEL
void	sf_attach(struct sf_softc *);
int	sf_intr(void *);
#endif /* _KERNEL */

#endif /* _DEV_IC_AIC6915VAR_H_ */

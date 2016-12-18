/*	$NetBSD: if_casvar.h,v 1.5 2015/04/14 20:32:36 riastradh Exp $ */
/*	$OpenBSD: if_casvar.h,v 1.6 2009/06/13 12:18:58 kettenis Exp $	*/

/*
 *
 * Copyright (C) 2007 Mark Kettenis.
 * Copyright (C) 2001 Eduardo Horvath.
 * All rights reserved.
 *
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR  ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR  BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef	_IF_CASVAR_H
#define	_IF_CASVAR_H

#include <sys/queue.h>
#include <sys/callout.h>
#include <sys/rndsource.h>

/*
 * Misc. definitions for Sun Cassini ethernet controllers.
 */

/*
 * Preferred page size.  Cassini has a configurable page size, but
 * needs at least 8k to handle jumbo frames.  This happens to be the
 * default anyway.
 */
#define	CAS_PAGE_SIZE		8192

/*
 * Transmit descriptor ring size.  This is arbitrary, but allocate
 * enough descriptors for 64 pending transmissions and 16 segments
 * per packet.
 */
#define	CAS_NTXSEGS		16

#define	CAS_TXQUEUELEN		64
#define	CAS_NTXDESC		(CAS_TXQUEUELEN * CAS_NTXSEGS)
#define	CAS_NTXDESC_MASK	(CAS_NTXDESC - 1)
#define	CAS_NEXTTX(x)		((x + 1) & CAS_NTXDESC_MASK)

struct cas_sxd {
	struct mbuf *sd_mbuf;
	bus_dmamap_t sd_map;
};

/*
 * Receive descriptor ring size.  We have one Rx buffer per incoming
 * packet, so this logic is a little simpler.
 */
#define	CAS_NRXDESC		128
#define	CAS_NRXDESC_MASK	(CAS_NRXDESC - 1)

/*
 * Receive completion ring size.
 */
#define	CAS_NRXCOMP		256
#define	CAS_NRXCOMP_MASK	(CAS_NRXCOMP - 1)
#define	CAS_NEXTRX(x)		((x + 1) & CAS_NRXCOMP_MASK)

/*
 * Control structures are DMA'd to the Cassini chip.  We allocate them in
 * a single clump that maps to a single DMA segment to make several things
 * easier.
 */
struct cas_control_data {
	/*
	 * The transmit descriptors.
	 */
	struct cas_desc ccd_txdescs[CAS_NTXDESC];

	/*
	 * The receive completions.
	 */
	struct cas_comp ccd_rxcomps[CAS_NRXCOMP];

	/*
	 * The receive descriptors.
	 */
	struct cas_desc ccd_rxdescs[CAS_NRXDESC];
	char ccd_unused[CAS_PAGE_SIZE - CAS_NRXDESC * 16];
	struct cas_desc ccd_rxdescs2[CAS_NRXDESC];
};

#define	CAS_CDOFF(x)		offsetof(struct cas_control_data, x)
#define	CAS_CDTXOFF(x)		CAS_CDOFF(ccd_txdescs[(x)])
#define	CAS_CDRXOFF(x)		CAS_CDOFF(ccd_rxdescs[(x)])
#define	CAS_CDRXOFF2(x)		CAS_CDOFF(ccd_rxdescs2[(x)])
#define	CAS_CDRXCOFF(x)		CAS_CDOFF(ccd_rxcomps[(x)])

/*
 * Software state for receive jobs.
 */
struct cas_rxsoft {
	bus_dmamap_t rxs_dmamap;	/* our DMA map */
	bus_dma_segment_t rxs_dmaseg;	/* our DMA segment */
	char *rxs_kva;
};

enum cas_attach_stage {
	  CAS_ATT_BACKEND_2 = 0
	, CAS_ATT_BACKEND_1
	, CAS_ATT_FINISHED
	, CAS_ATT_MII
	, CAS_ATT_7
	, CAS_ATT_6
	, CAS_ATT_5
	, CAS_ATT_4
	, CAS_ATT_3
	, CAS_ATT_2
	, CAS_ATT_1
	, CAS_ATT_0
	, CAS_ATT_BACKEND_0
};

/*
 * Software state per device.
 */
struct cas_softc {
	device_t	sc_dev;		/* generic device information */
	struct ethercom	sc_ethercom;	/* ethernet common data */
	struct mii_data	sc_mii;		/* MII media control */
#define sc_media	sc_mii.mii_media/* shorthand */
	struct callout	sc_tick_ch;	/* tick callout */

	bus_space_tag_t	sc_memt;
	bus_space_handle_t sc_memh;
	bus_size_t	sc_size;

	void		*sc_ih;
	pci_chipset_tag_t	sc_pc;
	pci_intr_handle_t	sc_handle;

	bus_dma_tag_t	sc_dmatag;	/* bus dma tag */
	bus_dmamap_t	sc_dmamap;	/* bus dma handle */
	int		sc_burst;	/* DVMA burst size in effect */
	int		sc_phys[2];	/* MII instance -> PHY map */

	int		sc_mif_config;	/* Selected MII reg setting */

	/*
	 * Ring buffer DMA stuff.
	 */
	bus_dma_segment_t sc_cdseg;	/* control data memory */
	int		sc_cdnseg;	/* number of segments */
	bus_dmamap_t sc_cddmamap;	/* control data DMA map */
#define	sc_cddma	sc_cddmamap->dm_segs[0].ds_addr

	/*
	 * Software state for transmit and receive descriptors.
	 */
	struct cas_sxd sc_txd[CAS_NTXDESC];
	u_int32_t sc_tx_cnt, sc_tx_prod, sc_tx_cons;

	struct cas_rxsoft sc_rxsoft[CAS_NRXDESC];
	struct cas_rxsoft sc_rxsoft2[CAS_NRXDESC];

	/*
	 * Control data structures.
	 */
	struct cas_control_data *sc_control_data;
#define	sc_txdescs	sc_control_data->ccd_txdescs
#define	sc_rxdescs	sc_control_data->ccd_rxdescs
#define	sc_rxdescs2	sc_control_data->ccd_rxdescs2
#define	sc_rxcomps	sc_control_data->ccd_rxcomps

	int			sc_rxptr;		/* next ready RX descriptor/descsoft */
	int			sc_rxfifosize;
	int			sc_rxdptr;

	int			sc_rev;
	int			sc_inited;
	int			sc_debug;
	void			*sc_sh;		/* shutdownhook cookie */

	krndsource_t	rnd_source;

	struct evcnt		sc_ev_intr;
	enum cas_attach_stage	sc_att_stage;
};

/*
 * This maccro determines whether we have a Cassini+.
 */
#define	CAS_PLUS(sc)	(sc->sc_rev > 0x10)

#define	CAS_DMA_READ(v)		le64toh(v)
#define	CAS_DMA_WRITE(v)	htole64(v)

#define	CAS_CDTXADDR(sc, x)	((sc)->sc_cddma + CAS_CDTXOFF((x)))
#define	CAS_CDRXADDR(sc, x)	((sc)->sc_cddma + CAS_CDRXOFF((x)))
#define	CAS_CDRXADDR2(sc, x)	((sc)->sc_cddma + CAS_CDRXOFF2((x)))
#define	CAS_CDRXCADDR(sc, x)	((sc)->sc_cddma + CAS_CDRXCOFF((x)))

#define	CAS_CDTXSYNC(sc, x, n, ops)					\
do {									\
	int __x, __n;							\
									\
	__x = (x);							\
	__n = (n);							\
									\
	/* If it will wrap around, sync to the end of the ring. */	\
	if ((__x + __n) > CAS_NTXDESC) {				\
		bus_dmamap_sync((sc)->sc_dmatag, (sc)->sc_cddmamap,	\
		    CAS_CDTXOFF(__x), sizeof(struct cas_desc) *		\
		    (CAS_NTXDESC - __x), (ops));			\
		__n -= (CAS_NTXDESC - __x);				\
		__x = 0;						\
	}								\
									\
	/* Now sync whatever is left. */				\
	bus_dmamap_sync((sc)->sc_dmatag, (sc)->sc_cddmamap,		\
	    CAS_CDTXOFF(__x), sizeof(struct cas_desc) * __n, (ops));	\
} while (0)

#define	CAS_CDRXSYNC(sc, x, ops)					\
	bus_dmamap_sync((sc)->sc_dmatag, (sc)->sc_cddmamap,		\
	    CAS_CDRXOFF((x)), sizeof(struct cas_desc), (ops))

#define	CAS_CDRXCSYNC(sc, x, ops)					\
	bus_dmamap_sync((sc)->sc_dmatag, (sc)->sc_cddmamap,		\
	    CAS_CDRXCOFF((x)), sizeof(struct cas_desc), (ops))

#define	CAS_INIT_RXDESC(sc, d, s)					\
do {									\
	struct cas_rxsoft *__rxs = &sc->sc_rxsoft[(s)];			\
	struct cas_desc *__rxd = &sc->sc_rxdescs[(d)];			\
									\
	__rxd->cd_addr =						\
	    CAS_DMA_WRITE(__rxs->rxs_dmamap->dm_segs[0].ds_addr);	\
	__rxd->cd_flags =						\
	    CAS_DMA_WRITE((s));						\
	CAS_CDRXSYNC((sc), (d), BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE); \
} while (0)

#define CAS_INTR_PCI	1
#define CAS_INTR_REG	2

#define ETHER_ALIGN	2

#endif

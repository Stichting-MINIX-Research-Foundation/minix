/*	$NetBSD: dp83932var.h,v 1.12 2009/09/01 15:20:53 tsutsui Exp $	*/

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

#ifndef _DEV_IC_DP83932VAR_H_
#define	_DEV_IC_DP83932VAR_H_

/*
 * Data structure definitions for the National Semiconductor DP83932
 * Systems-Oriented Network Interface Controller (SONIC).
 */

/*
 * NOTE: The control data for the SONIC must not cross a 64k boundary,
 * so we have to be careful about how we size things.
 *
 * Also, since the SONIC is only a 10Mb/s chip, and systems on which
 * it is present tend to be low on memory, we try to keep the data
 * structure sizes small.
 */

/*
 * Transmit descriptor list size.
 */
#define	SONIC_NTXDESC		32
#define	SONIC_NTXDESC_MASK	(SONIC_NTXDESC - 1)
#define	SONIC_NEXTTX(x)		(((x) + 1) & SONIC_NTXDESC_MASK)

/*
 * Receive descriptor list size.
 */
#define	SONIC_NRXDESC		32
#define	SONIC_NRXDESC_MASK	(SONIC_NRXDESC - 1)
#define	SONIC_NEXTRX(x)		(((x) + 1) & SONIC_NRXDESC_MASK)
#define	SONIC_PREVRX(x)		(((x) - 1) & SONIC_NRXDESC_MASK)
#define	SONIC_RXSEQ_TO_DESC(x)	((x) & SONIC_NRXDESC_MASK)

/*
 * Number of CAM entries.
 */
#define	SONIC_NCAMENT		16

/*
 * Control structures are DMA'd to the SONIC chip.  We allocate them in
 * a single clump that maps to a single DMA segment to make several things
 * easier.
 */
struct sonic_control_data16 {
	/*
	 * The transmit descriptors.
	 */
	struct sonic_tda16 scd_txdescs[SONIC_NTXDESC];

	/*
	 * The receive descriptors.
	 */
	struct sonic_rda16 scd_rxdescs[SONIC_NRXDESC];

	/*
	 * The receive resource descriptors.
	 */
	struct sonic_rra16 scd_rxbufs[SONIC_NRXDESC];

	/*
	 * The CAM descriptors.
	 */
	struct sonic_cda16 scd_cam[SONIC_NCAMENT];
	uint16_t scd_camenable;
};

#define	SONIC_CDOFF16(x)	offsetof(struct sonic_control_data16, x)
#define	SONIC_CDTXOFF16(x)	SONIC_CDOFF16(scd_txdescs[(x)])
#define	SONIC_CDRXOFF16(x)	SONIC_CDOFF16(scd_rxdescs[(x)])
#define	SONIC_CDRROFF16(x)	SONIC_CDOFF16(scd_rxbufs[(x)])
#define	SONIC_CDCAMOFF16	SONIC_CDOFF16(scd_cam)
#define	SONIC_CDCAMSIZE16	\
	(sizeof(struct sonic_cda16) * SONIC_NCAMENT + sizeof(uint16_t))

struct sonic_control_data32 {
	/*
	 * The transmit descriptors.
	 */
	struct sonic_tda32 scd_txdescs[SONIC_NTXDESC];

	/*
	 * The receive descriptors.
	 */
	struct sonic_rda32 scd_rxdescs[SONIC_NRXDESC];

	/*
	 * The receive resource descriptors.
	 */
	struct sonic_rra32 scd_rxbufs[SONIC_NRXDESC];

	/*
	 * The CAM descriptors.
	 */
	struct sonic_cda32 scd_cam[SONIC_NCAMENT];
	uint32_t scd_camenable;
};

#define	SONIC_CDOFF32(x)	offsetof(struct sonic_control_data32, x)
#define	SONIC_CDTXOFF32(x)	SONIC_CDOFF32(scd_txdescs[(x)])
#define	SONIC_CDRXOFF32(x)	SONIC_CDOFF32(scd_rxdescs[(x)])
#define	SONIC_CDRROFF32(x)	SONIC_CDOFF32(scd_rxbufs[(x)])
#define	SONIC_CDCAMOFF32	SONIC_CDOFF32(scd_cam)
#define	SONIC_CDCAMSIZE32	\
	(sizeof(struct sonic_cda32) * SONIC_NCAMENT + sizeof(uint32_t))

/*
 * Software state for transmit and receive descriptors.
 */
struct sonic_descsoft {
	struct mbuf *ds_mbuf;		/* head of mbuf chain */
	bus_dmamap_t ds_dmamap;		/* our DMA map */
};

/*
 * Software state per device.
 */
struct sonic_softc {
	device_t sc_dev;		/* generic device information */
	bus_space_tag_t sc_st;		/* bus space tag */
	bus_space_handle_t sc_sh;	/* bus space handle */
	bus_dma_tag_t sc_dmat;		/* bus DMA tag */
	struct ethercom sc_ethercom;	/* ethernet common data */

	int sc_32bit;			/* use 32-bit mode */
	int sc_bigendian;		/* BMODE -> Vcc */

	/* Our register map. */
	bus_addr_t sc_regmap[SONIC_NREGS];

	bus_dmamap_t sc_cddmamap;	/* control data DMA map */
#define	sc_cddma	sc_cddmamap->dm_segs[0].ds_addr
	bus_dmamap_t sc_nulldmamap;	/* DMA map for the pad buffer */
#define sc_nulldma     sc_nulldmamap->dm_segs[0].ds_addr

	/*
	 * Software state for transmit and receive descriptors.
	 */
	struct sonic_descsoft sc_txsoft[SONIC_NTXDESC];
	struct sonic_descsoft sc_rxsoft[SONIC_NRXDESC];

	/*
	 * Control data structures.
	 */
	union {
		struct sonic_control_data16 *cdun_16;
		struct sonic_control_data32 *cdun_32;
	} sc_cdun;
#define	sc_cdata16	sc_cdun.cdun_16
#define	sc_cdata32	sc_cdun.cdun_32

#define	sc_tda16	sc_cdun.cdun_16->scd_txdescs
#define	sc_rda16	sc_cdun.cdun_16->scd_rxdescs
#define	sc_rra16	sc_cdun.cdun_16->scd_rxbufs
#define	sc_cda16	sc_cdun.cdun_16->scd_cam
#define	sc_cdaenable16	sc_cdun.cdun_16->scd_camenable

#define	sc_tda32	sc_cdun.cdun_32->scd_txdescs
#define	sc_rda32	sc_cdun.cdun_32->scd_rxdescs
#define	sc_rra32	sc_cdun.cdun_32->scd_rxbufs
#define	sc_cda32	sc_cdun.cdun_32->scd_cam
#define	sc_cdaenable32	sc_cdun.cdun_32->scd_camenable

	int	sc_txpending;		/* number of Tx requests pending */
	int	sc_txdirty;		/* first dirty Tx descriptor */
	int	sc_txlast;		/* last used Tx descriptor */

	int	sc_rxptr;		/* next ready Rx descriptor */

	uint16_t sc_imr;		/* prototype IMR */
	uint16_t sc_dcr;		/* prototype DCR */
	uint16_t sc_dcr2;		/* prototype DCR2 */
};

#define	CSR_READ(sc, reg)						\
	bus_space_read_2((sc)->sc_st, (sc)->sc_sh,			\
	    (sc)->sc_regmap[(reg)])

#define	CSR_WRITE(sc, reg, val)						\
	bus_space_write_2((sc)->sc_st, (sc)->sc_sh,			\
	    (sc)->sc_regmap[(reg)], (val))

#define	SONIC_CDTXADDR16(sc, x)						\
	((sc)->sc_cddma + SONIC_CDTXOFF16((x)))

#define	SONIC_CDTXADDR32(sc, x)						\
	((sc)->sc_cddma + SONIC_CDTXOFF32((x)))

#define	SONIC_CDTXADDR(sc, x)						\
	((sc)->sc_32bit ? SONIC_CDTXADDR32((sc), (x)) :			\
	    SONIC_CDTXADDR16((sc), (x)))

#define	SONIC_CDRXADDR16(sc, x)						\
	((sc)->sc_cddma + SONIC_CDRXOFF16((x)))

#define	SONIC_CDRXADDR32(sc, x)						\
	((sc)->sc_cddma + SONIC_CDRXOFF32((x)))

#define	SONIC_CDRXADDR(sc, x)						\
	((sc)->sc_32bit ? SONIC_CDRXADDR32((sc), (x)) :			\
	    SONIC_CDRXADDR16((sc), (x)))

#define	SONIC_CDRRADDR(sc, x)						\
	((sc)->sc_cddma +						\
	 ((sc)->sc_32bit ? SONIC_CDRROFF32((x)) : SONIC_CDRROFF16((x))))

#define	SONIC_CDCAMADDR(sc)						\
	((sc)->sc_cddma +						\
	 ((sc)->sc_32bit ? SONIC_CDCAMOFF32 : SONIC_CDCAMOFF16))

#define	SONIC_CDTXSYNC16(sc, x, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    SONIC_CDTXOFF16((x)), sizeof(struct sonic_tda16), (ops))

#define	SONIC_CDTXSYNC32(sc, x, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    SONIC_CDTXOFF32((x)), sizeof(struct sonic_tda32), (ops))

#define	SONIC_CDRXSYNC16(sc, x, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    SONIC_CDRXOFF16((x)), sizeof(struct sonic_rda16), (ops))

#define	SONIC_CDRXSYNC32(sc, x, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    SONIC_CDRXOFF32((x)), sizeof(struct sonic_rda32), (ops))

#define	SONIC_CDRRSYNC16(sc, x, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    SONIC_CDRROFF16((x)), sizeof(struct sonic_rra16), (ops))

#define	SONIC_CDRRSYNC32(sc, x, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    SONIC_CDRROFF32((x)), sizeof(struct sonic_rra32), (ops))

#define	SONIC_CDCAMSYNC(sc, ops)					\
do {									\
	if ((sc)->sc_32bit)						\
		bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,	\
		    SONIC_CDCAMOFF32, SONIC_CDCAMSIZE32,		\
		    (ops));						\
	else								\
		bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,	\
		    SONIC_CDCAMOFF16, SONIC_CDCAMSIZE16,		\
		    (ops));						\
} while (/*CONSTCOND*/0)

#define	SONIC_INIT_RXDESC(sc, x)					\
do {									\
	struct sonic_descsoft *__ds = &(sc)->sc_rxsoft[(x)];		\
	struct mbuf *__m = __ds->ds_mbuf;				\
									\
	if ((sc)->sc_32bit) {						\
		/*							\
		 * Unfortuantely, in 32-bit mode, the Rx buffer must	\
		 * be 32-bit aligned.					\
		 */							\
		struct sonic_rda32 *__rda = &(sc)->sc_rda32[(x)];	\
		struct sonic_rda32 *__prda =				\
		    &(sc)->sc_rda32[SONIC_PREVRX((x))];			\
		struct sonic_rra32 *__rra = &(sc)->sc_rra32[(x)];	\
									\
		__m->m_data = __m->m_ext.ext_buf;			\
									\
		__rra->rra_ptr1 =					\
		    __ds->ds_dmamap->dm_segs[0].ds_addr >> 16;		\
		__rra->rra_ptr0 =					\
		    __ds->ds_dmamap->dm_segs[0].ds_addr & 0xffff;	\
		__rra->rra_wc1 = 0;					\
		__rra->rra_wc0 = (ETHER_MAX_LEN + 6) / 2;		\
									\
		__rda->rda_link =					\
		    (SONIC_CDRXADDR32((sc), SONIC_NEXTRX((x))) & 0xffff) |\
		    RDA_LINK_EOL;					\
		__rda->rda_inuse = 1;					\
									\
		__prda->rda_link = SONIC_CDRXADDR32((sc), (x));		\
									\
		SONIC_CDRRSYNC32((sc), (x), BUS_DMASYNC_PREWRITE);	\
		SONIC_CDRXSYNC32((sc), (x),				\
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);		\
		SONIC_CDRXSYNC32((sc), SONIC_PREVRX(x),			\
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);		\
	} else {							\
		/*							\
		 * In 16-bit mode, we scoot the packet forward 2 bytes	\
		 * so that the payload after the Ethernet header is	\
		 * suitably aligned.					\
		 */							\
		struct sonic_rda16 *__rda = &(sc)->sc_rda16[(x)];	\
		struct sonic_rda16 *__prda =				\
		    &(sc)->sc_rda16[SONIC_PREVRX((x))];			\
		struct sonic_rra16 *__rra = &(sc)->sc_rra16[(x)];	\
									\
		__m->m_data = __m->m_ext.ext_buf + 2;			\
									\
		__rra->rra_ptr1 =					\
		    __ds->ds_dmamap->dm_segs[0].ds_addr >> 16;		\
		__rra->rra_ptr0 =					\
		    (__ds->ds_dmamap->dm_segs[0].ds_addr + 2) & 0xffff;	\
		__rra->rra_wc1 = 0;					\
		__rra->rra_wc0 = (ETHER_MAX_LEN + 2) / 2;		\
									\
		__rda->rda_link =					\
		    (SONIC_CDRXADDR16((sc), SONIC_NEXTRX((x))) & 0xffff) |\
		    RDA_LINK_EOL;					\
		__rda->rda_inuse = 1;					\
									\
		__prda->rda_link = SONIC_CDRXADDR16((sc), (x));		\
									\
		SONIC_CDRRSYNC16((sc), (x), BUS_DMASYNC_PREWRITE);	\
		SONIC_CDRXSYNC16((sc), (x),				\
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);		\
		SONIC_CDRXSYNC16((sc), SONIC_PREVRX(x),			\
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);		\
	}								\
} while (/*CONSTCOND*/0)

static __inline uint16_t __unused
htosonic16(struct sonic_softc *sc, uint16_t val)
{

	if (sc->sc_bigendian)
		return (htobe16(val));
	return (htole16(val));
}

static __inline uint16_t __unused
sonic16toh(struct sonic_softc *sc, uint16_t val)
{

	if (sc->sc_bigendian)
		return (be16toh(val));
	return (le16toh(val));
}

static __inline uint32_t __unused
htosonic32(struct sonic_softc *sc, uint32_t val)
{

	if (sc->sc_bigendian)
		return (htobe32(val));
	return (htole32(val));
}

static __inline uint32_t __unused
sonic32toh(struct sonic_softc *sc, uint32_t val)
{

	if (sc->sc_bigendian)
		return (be32toh(val));
	return (le32toh(val));
}

#ifdef _KERNEL
void	sonic_attach(struct sonic_softc *, const uint8_t *);
int	sonic_intr(void *);
#endif /* _KERNEL */

#endif /* _DEV_IC_DP83932VAR_H_ */

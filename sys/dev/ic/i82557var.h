/*	$NetBSD: i82557var.h,v 1.52 2015/04/13 16:33:24 riastradh Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 1999, 2001 The NetBSD Foundation, Inc.
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

/*
 * Copyright (c) 1995, David Greenman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	Id: if_fxpvar.h,v 1.4 1997/11/29 08:11:01 davidg Exp
 */

#include <sys/callout.h>
#include <sys/rndsource.h>

/*
 * Misc. definitions for the Intel i82557 fast Ethernet controller
 * driver.
 */

/*
 * Transmit descriptor list size.
 */
#define	FXP_NTXCB		256
#define	FXP_NTXCB_MASK		(FXP_NTXCB - 1)
#define	FXP_NEXTTX(x)		((x + 1) & FXP_NTXCB_MASK)
#define	FXP_NTXSEG		16
#define	FXP_IPCB_NTXSEG		(FXP_NTXSEG - 1)

/*
 * Number of receive frame area buffers.  These are large, so
 * choose wisely.
 */
#define	FXP_NRFABUFS		128

/*
 * Maximum number of seconds that the receiver can be idle before we
 * assume it's dead and attempt to reset it by reprogramming the
 * multicast filter.  This is part of a work-around for a bug in the
 * NIC.  See fxp_stats_update().
 */
#define	FXP_MAX_RX_IDLE	15

/*
 * Misc. DMA'd data structures are allocated in a single clump, that
 * maps to a single DMA segment, to make several things easier (computing
 * offsets, setting up DMA maps, etc.)
 */
struct fxp_control_data {
	/*
	 * The transmit control blocks and transmit buffer descriptors.
	 * We arrange them like this so that everything is all lined
	 * up to use the extended TxCB feature.
	 */
	struct fxp_txdesc {
		struct fxp_cb_tx txd_txcb;
		union {
			struct fxp_ipcb txdu_ipcb;
			struct fxp_tbd txdu_tbd[FXP_NTXSEG];
		} txd_u;
	} fcd_txdescs[FXP_NTXCB];

	/*
	 * The configuration CB.
	 */
	struct fxp_cb_config fcd_configcb;

	/*
	 * The Individual Address CB.
	 */
	struct fxp_cb_ias fcd_iascb;

	/*
	 * The multicast setup CB.
	 */
	struct fxp_cb_mcs fcd_mcscb;

	/*
	 * The microcode setup CB.
	 */
	struct fxp_cb_ucode fcd_ucode;

	/*
	 * The NIC statistics.
	 */
	struct fxp_stats fcd_stats;

	/*
	 * TX pad buffer for ip4csum-tx bug workaround.
	 */
	uint8_t fcd_txpad[FXP_IP4CSUMTX_PADLEN];
};

#define	txd_tbd	txd_u.txdu_tbd

#define	FXP_CDOFF(x)	offsetof(struct fxp_control_data, x)
#define	FXP_CDTXOFF(x)	FXP_CDOFF(fcd_txdescs[(x)].txd_txcb)
#define	FXP_CDTBDOFF(x)	FXP_CDOFF(fcd_txdescs[(x)].txd_tbd)
#define	FXP_CDCONFIGOFF	FXP_CDOFF(fcd_configcb)
#define	FXP_CDIASOFF	FXP_CDOFF(fcd_iascb)
#define	FXP_CDMCSOFF	FXP_CDOFF(fcd_mcscb)
#define	FXP_CDUCODEOFF	FXP_CDOFF(fcd_ucode)
#define	FXP_CDSTATSOFF	FXP_CDOFF(fcd_stats)
#define	FXP_CDTXPADOFF	FXP_CDOFF(fcd_txpad)

/*
 * Software state for transmit descriptors.
 */
struct fxp_txsoft {
	struct mbuf *txs_mbuf;		/* head of mbuf chain */
	bus_dmamap_t txs_dmamap;	/* our DMA map */
};

/*
 * Software state per device.
 */
struct fxp_softc {
	device_t sc_dev;
	bus_space_tag_t sc_st;		/* bus space tag */
	bus_space_handle_t sc_sh;	/* bus space handle */
	bus_size_t sc_size;		/* bus space size */
	bus_dma_tag_t sc_dmat;		/* bus dma tag */
	struct ethercom sc_ethercom;	/* ethernet common part */
	void *sc_ih;			/* interrupt handler cookie */

	struct mii_data sc_mii;		/* MII/media information */
	struct callout sc_callout;	/* MII callout */

	/*
	 * We create a single DMA map that maps all data structure
	 * overhead, except for RFAs, which are mapped by the
	 * fxp_rxdesc DMA map on a per-mbuf basis.
	 */
	bus_dmamap_t sc_dmamap;
#define	sc_cddma	sc_dmamap->dm_segs[0].ds_addr

	/*
	 * Software state for transmit descriptors.
	 */
	struct fxp_txsoft sc_txsoft[FXP_NTXCB];

	int	sc_rfa_size;		/* size of the RFA structure */
	struct ifqueue sc_rxq;		/* receive buffer queue */
	bus_dmamap_t sc_rxmaps[FXP_NRFABUFS]; /* free receive buffer DMA maps */
	int	sc_rxfree;		/* free map index */
	int	sc_rxidle;		/* # of seconds RX has been idle */
	uint16_t sc_txcmd;		/* transmit command (LITTLE ENDIAN) */

	/*
	 * Control data structures.
	 */
	struct fxp_control_data *sc_control_data;

#ifdef FXP_EVENT_COUNTERS
	struct evcnt sc_ev_txstall;	/* Tx stalled */
	struct evcnt sc_ev_txintr;	/* Tx interrupts */
	struct evcnt sc_ev_rxintr;	/* Rx interrupts */
	struct evcnt sc_ev_txpause;	/* Tx PAUSE frames */
	struct evcnt sc_ev_rxpause;	/* Rx PAUSE frames */
#endif /* FXP_EVENT_COUNTERS */

	bus_dma_segment_t sc_cdseg;	/* control dma segment */
	int	sc_cdnseg;

	int	sc_rev;			/* chip revision */
	int	sc_flags;		/* misc. flags */

#define	FXPF_MII		0x0001	/* device uses MII */
#define	FXPF_ATTACHED		0x0002	/* attach has succeeded */
#define	FXPF_WANTINIT		0x0004	/* want a re-init */
#define	FXPF_HAS_RESUME_BUG	0x0008	/* has the resume bug */
#define	FXPF_MWI		0x0010	/* enable PCI MWI */
#define	FXPF_READ_ALIGN		0x0020	/* align read access w/ cacheline */
#define	FXPF_WRITE_ALIGN	0x0040	/* end write on cacheline */
#define	FXPF_EXT_TXCB		0x0080	/* has extended TxCB */
#define	FXPF_UCODE_LOADED	0x0100	/* microcode is loaded */
#define	FXPF_EXT_RFA		0x0200	/* has extended RFD and IPCB (82550) */
#define	FXPF_RECV_WORKAROUND	0x0800	/* receiver lock-up workaround */
#define	FXPF_FC			0x1000	/* has flow control */
#define	FXPF_82559_RXCSUM	0x2000	/* has 82559 compat RX checksum */

	int	sc_int_delay;		/* interrupt delay */
	int	sc_bundle_max;		/* max packet bundle */

	int	sc_txpending;		/* number of TX requests pending */
	int	sc_txdirty;		/* first dirty TX descriptor */
	int	sc_txlast;		/* last used TX descriptor */

	int phy_primary_device;		/* device type of primary PHY */

	int	sc_enabled;	/* boolean; power enabled on interface */
	int	(*sc_enable)(struct fxp_softc *);
	void	(*sc_disable)(struct fxp_softc *);

	int	sc_eeprom_size;		/* log2 size of EEPROM */
	krndsource_t rnd_source;	/* random source */
};

#ifdef FXP_EVENT_COUNTERS
#define	FXP_EVCNT_INCR(ev)	(ev)->ev_count++
#else
#define	FXP_EVCNT_INCR(ev)	/* nothing */
#endif

#define	FXP_RXMAP_GET(sc)	((sc)->sc_rxmaps[(sc)->sc_rxfree++])
#define	FXP_RXMAP_PUT(sc, map)	(sc)->sc_rxmaps[--(sc)->sc_rxfree] = (map)

#define	FXP_CDTXADDR(sc, x)	((sc)->sc_cddma + FXP_CDTXOFF((x)))
#define	FXP_CDTBDADDR(sc, x)	((sc)->sc_cddma + FXP_CDTBDOFF((x)))
#define	FXP_CDTXPADADDR(sc)	((sc)->sc_cddma + FXP_CDTXPADOFF)

#define	FXP_CDTX(sc, x)		(&(sc)->sc_control_data->fcd_txdescs[(x)])

#define	FXP_DSTX(sc, x)		(&(sc)->sc_txsoft[(x)])

#define	FXP_CDTXSYNC(sc, x, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_dmamap,			\
	    FXP_CDTXOFF((x)), sizeof(struct fxp_txdesc), (ops))

#define	FXP_CDCONFIGSYNC(sc, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_dmamap,			\
	    FXP_CDCONFIGOFF, sizeof(struct fxp_cb_config), (ops))

#define	FXP_CDIASSYNC(sc, ops)						\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_dmamap,			\
	    FXP_CDIASOFF, sizeof(struct fxp_cb_ias), (ops))

#define	FXP_CDMCSSYNC(sc, ops)						\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_dmamap,			\
	    FXP_CDMCSOFF, sizeof(struct fxp_cb_mcs), (ops))

#define	FXP_CDUCODESYNC(sc, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_dmamap,			\
	    FXP_CDUCODEOFF, sizeof(struct fxp_cb_ucode), (ops))

#define	FXP_CDSTATSSYNC(sc, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_dmamap,			\
	    FXP_CDSTATSOFF, sizeof(struct fxp_stats), (ops))

#define	FXP_RXBUFSIZE(sc, m)	((m)->m_ext.ext_size -			\
				 (sc->sc_rfa_size +			\
				  RFA_ALIGNMENT_FUDGE))

#define	FXP_RFASYNC(sc, m, ops)						\
	bus_dmamap_sync((sc)->sc_dmat, M_GETCTX((m), bus_dmamap_t),	\
	    RFA_ALIGNMENT_FUDGE, (sc)->sc_rfa_size, (ops))

#define	FXP_RXBUFSYNC(sc, m, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, M_GETCTX((m), bus_dmamap_t),	\
	    RFA_ALIGNMENT_FUDGE + (sc)->sc_rfa_size,			\
	    FXP_RXBUFSIZE((sc), (m)), (ops))

#define	FXP_MTORFA(m)	(struct fxp_rfa *)((m)->m_ext.ext_buf +		\
					   RFA_ALIGNMENT_FUDGE)

#define	FXP_INIT_RFABUF(sc, m)						\
do {									\
	bus_dmamap_t __rxmap = M_GETCTX((m), bus_dmamap_t);		\
	struct mbuf *__p_m;						\
	struct fxp_rfa *__rfa, *__p_rfa;				\
	uint32_t __v;							\
									\
	(m)->m_data = (m)->m_ext.ext_buf + (sc)->sc_rfa_size +		\
	    RFA_ALIGNMENT_FUDGE;					\
									\
	__rfa = FXP_MTORFA((m));					\
	__rfa->size = htole16(FXP_RXBUFSIZE((sc), (m)));		\
	/* BIG_ENDIAN: no need to swap to store 0 */			\
	__rfa->rfa_status = 0;						\
	__rfa->rfa_control =						\
	    htole16(FXP_RFA_CONTROL_EL | FXP_RFA_CONTROL_S);		\
	/* BIG_ENDIAN: no need to swap to store 0 */			\
	__rfa->actual_size = 0;						\
									\
	/* NOTE: the RFA is misaligned, so we must copy. */		\
	/* BIG_ENDIAN: no need to swap to store 0xffffffff */		\
	__v = 0xffffffff;						\
	memcpy(__UNVOLATILE(&__rfa->link_addr), &__v, sizeof(__v));	\
	memcpy(__UNVOLATILE(&__rfa->rbd_addr), &__v, sizeof(__v));	\
									\
	FXP_RFASYNC((sc), (m),						\
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);			\
									\
	FXP_RXBUFSYNC((sc), (m), BUS_DMASYNC_PREREAD);			\
									\
	if ((__p_m = (sc)->sc_rxq.ifq_tail) != NULL) {			\
		__p_rfa = FXP_MTORFA(__p_m);				\
		__v = htole32(__rxmap->dm_segs[0].ds_addr +		\
		    RFA_ALIGNMENT_FUDGE);				\
		FXP_RFASYNC((sc), __p_m,				\
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);	\
		memcpy(__UNVOLATILE(&__p_rfa->link_addr), &__v,		\
		    sizeof(__v));					\
		__p_rfa->rfa_control &= htole16(~(FXP_RFA_CONTROL_EL|	\
		    FXP_RFA_CONTROL_S));				\
		FXP_RFASYNC((sc), __p_m,				\
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);		\
	}								\
	IF_ENQUEUE(&(sc)->sc_rxq, (m));					\
} while (0)

/* Macros to ease CSR access. */
#define	CSR_READ_1(sc, reg)						\
	bus_space_read_1((sc)->sc_st, (sc)->sc_sh, (reg))
#define	CSR_READ_2(sc, reg)						\
	bus_space_read_2((sc)->sc_st, (sc)->sc_sh, (reg))
#define	CSR_READ_4(sc, reg)						\
	bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (reg))
#define	CSR_WRITE_1(sc, reg, val)					\
	bus_space_write_1((sc)->sc_st, (sc)->sc_sh, (reg), (val))
#define	CSR_WRITE_2(sc, reg, val)					\
	bus_space_write_2((sc)->sc_st, (sc)->sc_sh, (reg), (val))
#define	CSR_WRITE_4(sc, reg, val)					\
	bus_space_write_4((sc)->sc_st, (sc)->sc_sh, (reg), (val))

void	fxp_attach(struct fxp_softc *);
int	fxp_activate(device_t, enum devact);
int	fxp_detach(struct fxp_softc *, int);
int	fxp_intr(void *);

int	fxp_enable(struct fxp_softc*);
void	fxp_disable(struct fxp_softc*);

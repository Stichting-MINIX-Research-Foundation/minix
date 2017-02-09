/*	$NetBSD: if_sip.c,v 1.159 2015/04/13 16:33:25 riastradh Exp $	*/

/*-
 * Copyright (c) 2001, 2002 The NetBSD Foundation, Inc.
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

/*-
 * Copyright (c) 1999 Network Computer, Inc.
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
 * 3. Neither the name of Network Computer, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY NETWORK COMPUTER, INC. AND CONTRIBUTORS
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
 * Device driver for the Silicon Integrated Systems SiS 900,
 * SiS 7016 10/100, National Semiconductor DP83815 10/100, and
 * National Semiconductor DP83820 10/100/1000 PCI Ethernet
 * controllers.
 *
 * Originally written to support the SiS 900 by Jason R. Thorpe for
 * Network Computer, Inc.
 *
 * TODO:
 *
 *	- Reduce the Rx interrupt load.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_sip.c,v 1.159 2015/04/13 16:33:25 riastradh Exp $");



#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/queue.h>

#include <sys/rndsource.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <net/bpf.h>

#include <sys/bus.h>
#include <sys/intr.h>
#include <machine/endian.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/mii_bitbang.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/if_sipreg.h>

/*
 * Transmit descriptor list size.  This is arbitrary, but allocate
 * enough descriptors for 128 pending transmissions, and 8 segments
 * per packet (64 for DP83820 for jumbo frames).
 *
 * This MUST work out to a power of 2.
 */
#define	GSIP_NTXSEGS_ALLOC 16
#define	SIP_NTXSEGS_ALLOC 8

#define	SIP_TXQUEUELEN		256
#define	MAX_SIP_NTXDESC	\
    (SIP_TXQUEUELEN * MAX(SIP_NTXSEGS_ALLOC, GSIP_NTXSEGS_ALLOC))

/*
 * Receive descriptor list size.  We have one Rx buffer per incoming
 * packet, so this logic is a little simpler.
 *
 * Actually, on the DP83820, we allow the packet to consume more than
 * one buffer, in order to support jumbo Ethernet frames.  In that
 * case, a packet may consume up to 5 buffers (assuming a 2048 byte
 * mbuf cluster).  256 receive buffers is only 51 maximum size packets,
 * so we'd better be quick about handling receive interrupts.
 */
#define	GSIP_NRXDESC		256
#define	SIP_NRXDESC		128

#define	MAX_SIP_NRXDESC	MAX(GSIP_NRXDESC, SIP_NRXDESC)

/*
 * Control structures are DMA'd to the SiS900 chip.  We allocate them in
 * a single clump that maps to a single DMA segment to make several things
 * easier.
 */
struct sip_control_data {
	/*
	 * The transmit descriptors.
	 */
	struct sip_desc scd_txdescs[MAX_SIP_NTXDESC];

	/*
	 * The receive descriptors.
	 */
	struct sip_desc scd_rxdescs[MAX_SIP_NRXDESC];
};

#define	SIP_CDOFF(x)	offsetof(struct sip_control_data, x)
#define	SIP_CDTXOFF(x)	SIP_CDOFF(scd_txdescs[(x)])
#define	SIP_CDRXOFF(x)	SIP_CDOFF(scd_rxdescs[(x)])

/*
 * Software state for transmit jobs.
 */
struct sip_txsoft {
	struct mbuf *txs_mbuf;		/* head of our mbuf chain */
	bus_dmamap_t txs_dmamap;	/* our DMA map */
	int txs_firstdesc;		/* first descriptor in packet */
	int txs_lastdesc;		/* last descriptor in packet */
	SIMPLEQ_ENTRY(sip_txsoft) txs_q;
};

SIMPLEQ_HEAD(sip_txsq, sip_txsoft);

/*
 * Software state for receive jobs.
 */
struct sip_rxsoft {
	struct mbuf *rxs_mbuf;		/* head of our mbuf chain */
	bus_dmamap_t rxs_dmamap;	/* our DMA map */
};

enum sip_attach_stage {
	  SIP_ATTACH_FIN = 0
	, SIP_ATTACH_CREATE_RXMAP
	, SIP_ATTACH_CREATE_TXMAP
	, SIP_ATTACH_LOAD_MAP
	, SIP_ATTACH_CREATE_MAP
	, SIP_ATTACH_MAP_MEM
	, SIP_ATTACH_ALLOC_MEM
	, SIP_ATTACH_INTR
	, SIP_ATTACH_MAP
};

/*
 * Software state per device.
 */
struct sip_softc {
	device_t sc_dev;		/* generic device information */
	device_suspensor_t		sc_suspensor;
	pmf_qual_t			sc_qual;

	bus_space_tag_t sc_st;		/* bus space tag */
	bus_space_handle_t sc_sh;	/* bus space handle */
	bus_size_t sc_sz;		/* bus space size */
	bus_dma_tag_t sc_dmat;		/* bus DMA tag */
	pci_chipset_tag_t sc_pc;
	bus_dma_segment_t sc_seg;
	struct ethercom sc_ethercom;	/* ethernet common data */

	const struct sip_product *sc_model; /* which model are we? */
	int sc_gigabit;			/* 1: 83820, 0: other */
	int sc_rev;			/* chip revision */

	void *sc_ih;			/* interrupt cookie */

	struct mii_data sc_mii;		/* MII/media information */

	callout_t sc_tick_ch;		/* tick callout */

	bus_dmamap_t sc_cddmamap;	/* control data DMA map */
#define	sc_cddma	sc_cddmamap->dm_segs[0].ds_addr

	/*
	 * Software state for transmit and receive descriptors.
	 */
	struct sip_txsoft sc_txsoft[SIP_TXQUEUELEN];
	struct sip_rxsoft sc_rxsoft[MAX_SIP_NRXDESC];

	/*
	 * Control data structures.
	 */
	struct sip_control_data *sc_control_data;
#define	sc_txdescs	sc_control_data->scd_txdescs
#define	sc_rxdescs	sc_control_data->scd_rxdescs

#ifdef SIP_EVENT_COUNTERS
	/*
	 * Event counters.
	 */
	struct evcnt sc_ev_txsstall;	/* Tx stalled due to no txs */
	struct evcnt sc_ev_txdstall;	/* Tx stalled due to no txd */
	struct evcnt sc_ev_txforceintr;	/* Tx interrupts forced */
	struct evcnt sc_ev_txdintr;	/* Tx descriptor interrupts */
	struct evcnt sc_ev_txiintr;	/* Tx idle interrupts */
	struct evcnt sc_ev_rxintr;	/* Rx interrupts */
	struct evcnt sc_ev_hiberr;	/* HIBERR interrupts */
	struct evcnt sc_ev_rxpause;	/* PAUSE received */
	/* DP83820 only */
	struct evcnt sc_ev_txpause;	/* PAUSE transmitted */
	struct evcnt sc_ev_rxipsum;	/* IP checksums checked in-bound */
	struct evcnt sc_ev_rxtcpsum;	/* TCP checksums checked in-bound */
	struct evcnt sc_ev_rxudpsum;	/* UDP checksums checked in-boudn */
	struct evcnt sc_ev_txipsum;	/* IP checksums comp. out-bound */
	struct evcnt sc_ev_txtcpsum;	/* TCP checksums comp. out-bound */
	struct evcnt sc_ev_txudpsum;	/* UDP checksums comp. out-bound */
#endif /* SIP_EVENT_COUNTERS */

	u_int32_t sc_txcfg;		/* prototype TXCFG register */
	u_int32_t sc_rxcfg;		/* prototype RXCFG register */
	u_int32_t sc_imr;		/* prototype IMR register */
	u_int32_t sc_rfcr;		/* prototype RFCR register */

	u_int32_t sc_cfg;		/* prototype CFG register */

	u_int32_t sc_gpior;		/* prototype GPIOR register */

	u_int32_t sc_tx_fill_thresh;	/* transmit fill threshold */
	u_int32_t sc_tx_drain_thresh;	/* transmit drain threshold */

	u_int32_t sc_rx_drain_thresh;	/* receive drain threshold */

	int	sc_flowflags;		/* 802.3x flow control flags */
	int	sc_rx_flow_thresh;	/* Rx FIFO threshold for flow control */
	int	sc_paused;		/* paused indication */

	int	sc_txfree;		/* number of free Tx descriptors */
	int	sc_txnext;		/* next ready Tx descriptor */
	int	sc_txwin;		/* Tx descriptors since last intr */

	struct sip_txsq sc_txfreeq;	/* free Tx descsofts */
	struct sip_txsq sc_txdirtyq;	/* dirty Tx descsofts */

	/* values of interface state at last init */
	struct {
		/* if_capenable */
		uint64_t	if_capenable;
		/* ec_capenable */
		int		ec_capenable;
		/* VLAN_ATTACHED */
		int		is_vlan;
	}	sc_prev;
	
	short	sc_if_flags;

	int	sc_rxptr;		/* next ready Rx descriptor/descsoft */
	int	sc_rxdiscard;
	int	sc_rxlen;
	struct mbuf *sc_rxhead;
	struct mbuf *sc_rxtail;
	struct mbuf **sc_rxtailp;

	int sc_ntxdesc;
	int sc_ntxdesc_mask;

	int sc_nrxdesc_mask;

	const struct sip_parm {
		const struct sip_regs {
			int r_rxcfg;
			int r_txcfg;
		} p_regs;

		const struct sip_bits {
			uint32_t b_txcfg_mxdma_8;
			uint32_t b_txcfg_mxdma_16;
			uint32_t b_txcfg_mxdma_32;
			uint32_t b_txcfg_mxdma_64;
			uint32_t b_txcfg_mxdma_128;
			uint32_t b_txcfg_mxdma_256;
			uint32_t b_txcfg_mxdma_512;
			uint32_t b_txcfg_flth_mask;
			uint32_t b_txcfg_drth_mask;

			uint32_t b_rxcfg_mxdma_8;
			uint32_t b_rxcfg_mxdma_16;
			uint32_t b_rxcfg_mxdma_32;
			uint32_t b_rxcfg_mxdma_64;
			uint32_t b_rxcfg_mxdma_128;
			uint32_t b_rxcfg_mxdma_256;
			uint32_t b_rxcfg_mxdma_512;

			uint32_t b_isr_txrcmp;
			uint32_t b_isr_rxrcmp;
			uint32_t b_isr_dperr;
			uint32_t b_isr_sserr;
			uint32_t b_isr_rmabt;
			uint32_t b_isr_rtabt;

			uint32_t b_cmdsts_size_mask;
		} p_bits;
		int		p_filtmem;
		int		p_rxbuf_len;
		bus_size_t	p_tx_dmamap_size;
		int		p_ntxsegs;
		int		p_ntxsegs_alloc;
		int		p_nrxdesc;
	} *sc_parm;

	void (*sc_rxintr)(struct sip_softc *);

	krndsource_t rnd_source;	/* random source */
};

#define	sc_bits	sc_parm->p_bits
#define	sc_regs	sc_parm->p_regs

static const struct sip_parm sip_parm = {
	  .p_filtmem = OTHER_RFCR_NS_RFADDR_FILTMEM
	, .p_rxbuf_len = MCLBYTES - 1	/* field width */
	, .p_tx_dmamap_size = MCLBYTES
	, .p_ntxsegs = 16
	, .p_ntxsegs_alloc = SIP_NTXSEGS_ALLOC
	, .p_nrxdesc = SIP_NRXDESC
	, .p_bits = {
		  .b_txcfg_mxdma_8	= 0x00200000	/*       8 bytes */
		, .b_txcfg_mxdma_16	= 0x00300000	/*      16 bytes */
		, .b_txcfg_mxdma_32	= 0x00400000	/*      32 bytes */
		, .b_txcfg_mxdma_64	= 0x00500000	/*      64 bytes */
		, .b_txcfg_mxdma_128	= 0x00600000	/*     128 bytes */
		, .b_txcfg_mxdma_256	= 0x00700000	/*     256 bytes */
		, .b_txcfg_mxdma_512	= 0x00000000	/*     512 bytes */
		, .b_txcfg_flth_mask	= 0x00003f00	/* Tx fill threshold */
		, .b_txcfg_drth_mask	= 0x0000003f	/* Tx drain threshold */

		, .b_rxcfg_mxdma_8	= 0x00200000	/*       8 bytes */
		, .b_rxcfg_mxdma_16	= 0x00300000	/*      16 bytes */
		, .b_rxcfg_mxdma_32	= 0x00400000	/*      32 bytes */
		, .b_rxcfg_mxdma_64	= 0x00500000	/*      64 bytes */
		, .b_rxcfg_mxdma_128	= 0x00600000	/*     128 bytes */
		, .b_rxcfg_mxdma_256	= 0x00700000	/*     256 bytes */
		, .b_rxcfg_mxdma_512	= 0x00000000	/*     512 bytes */

		, .b_isr_txrcmp	= 0x02000000	/* transmit reset complete */
		, .b_isr_rxrcmp	= 0x01000000	/* receive reset complete */
		, .b_isr_dperr	= 0x00800000	/* detected parity error */
		, .b_isr_sserr	= 0x00400000	/* signalled system error */
		, .b_isr_rmabt	= 0x00200000	/* received master abort */
		, .b_isr_rtabt	= 0x00100000	/* received target abort */
		, .b_cmdsts_size_mask = OTHER_CMDSTS_SIZE_MASK
	}
	, .p_regs = {
		.r_rxcfg = OTHER_SIP_RXCFG,
		.r_txcfg = OTHER_SIP_TXCFG
	}
}, gsip_parm = {
	  .p_filtmem = DP83820_RFCR_NS_RFADDR_FILTMEM
	, .p_rxbuf_len = MCLBYTES - 8
	, .p_tx_dmamap_size = ETHER_MAX_LEN_JUMBO
	, .p_ntxsegs = 64
	, .p_ntxsegs_alloc = GSIP_NTXSEGS_ALLOC
	, .p_nrxdesc = GSIP_NRXDESC
	, .p_bits = {
		  .b_txcfg_mxdma_8	= 0x00100000	/*       8 bytes */
		, .b_txcfg_mxdma_16	= 0x00200000	/*      16 bytes */
		, .b_txcfg_mxdma_32	= 0x00300000	/*      32 bytes */
		, .b_txcfg_mxdma_64	= 0x00400000	/*      64 bytes */
		, .b_txcfg_mxdma_128	= 0x00500000	/*     128 bytes */
		, .b_txcfg_mxdma_256	= 0x00600000	/*     256 bytes */
		, .b_txcfg_mxdma_512	= 0x00700000	/*     512 bytes */
		, .b_txcfg_flth_mask	= 0x0000ff00	/* Fx fill threshold */
		, .b_txcfg_drth_mask	= 0x000000ff	/* Tx drain threshold */

		, .b_rxcfg_mxdma_8	= 0x00100000	/*       8 bytes */
		, .b_rxcfg_mxdma_16	= 0x00200000	/*      16 bytes */
		, .b_rxcfg_mxdma_32	= 0x00300000	/*      32 bytes */
		, .b_rxcfg_mxdma_64	= 0x00400000	/*      64 bytes */
		, .b_rxcfg_mxdma_128	= 0x00500000	/*     128 bytes */
		, .b_rxcfg_mxdma_256	= 0x00600000	/*     256 bytes */
		, .b_rxcfg_mxdma_512	= 0x00700000	/*     512 bytes */

		, .b_isr_txrcmp	= 0x00400000	/* transmit reset complete */
		, .b_isr_rxrcmp	= 0x00200000	/* receive reset complete */
		, .b_isr_dperr	= 0x00100000	/* detected parity error */
		, .b_isr_sserr	= 0x00080000	/* signalled system error */
		, .b_isr_rmabt	= 0x00040000	/* received master abort */
		, .b_isr_rtabt	= 0x00020000	/* received target abort */
		, .b_cmdsts_size_mask = DP83820_CMDSTS_SIZE_MASK
	}
	, .p_regs = {
		.r_rxcfg = DP83820_SIP_RXCFG,
		.r_txcfg = DP83820_SIP_TXCFG
	}
};

static inline int
sip_nexttx(const struct sip_softc *sc, int x)
{
	return (x + 1) & sc->sc_ntxdesc_mask;
}

static inline int
sip_nextrx(const struct sip_softc *sc, int x)
{
	return (x + 1) & sc->sc_nrxdesc_mask;
}

/* 83820 only */
static inline void
sip_rxchain_reset(struct sip_softc *sc)
{
	sc->sc_rxtailp = &sc->sc_rxhead;
	*sc->sc_rxtailp = NULL;
	sc->sc_rxlen = 0;
}

/* 83820 only */
static inline void
sip_rxchain_link(struct sip_softc *sc, struct mbuf *m)
{
	*sc->sc_rxtailp = sc->sc_rxtail = m;
	sc->sc_rxtailp = &m->m_next;
}

#ifdef SIP_EVENT_COUNTERS
#define	SIP_EVCNT_INCR(ev)	(ev)->ev_count++
#else
#define	SIP_EVCNT_INCR(ev)	/* nothing */
#endif

#define	SIP_CDTXADDR(sc, x)	((sc)->sc_cddma + SIP_CDTXOFF((x)))
#define	SIP_CDRXADDR(sc, x)	((sc)->sc_cddma + SIP_CDRXOFF((x)))

static inline void
sip_cdtxsync(struct sip_softc *sc, const int x0, const int n0, const int ops)
{
	int x, n;

	x = x0;
	n = n0;

	/* If it will wrap around, sync to the end of the ring. */
	if (x + n > sc->sc_ntxdesc) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_cddmamap,
		    SIP_CDTXOFF(x), sizeof(struct sip_desc) *
		    (sc->sc_ntxdesc - x), ops);
		n -= (sc->sc_ntxdesc - x);
		x = 0;
	}

	/* Now sync whatever is left. */
	bus_dmamap_sync(sc->sc_dmat, sc->sc_cddmamap,
	    SIP_CDTXOFF(x), sizeof(struct sip_desc) * n, ops);
}

static inline void
sip_cdrxsync(struct sip_softc *sc, int x, int ops)
{
	bus_dmamap_sync(sc->sc_dmat, sc->sc_cddmamap,
	    SIP_CDRXOFF(x), sizeof(struct sip_desc), ops);
}

#if 0
#ifdef DP83820
	u_int32_t	sipd_bufptr;	/* pointer to DMA segment */
	u_int32_t	sipd_cmdsts;	/* command/status word */
#else
	u_int32_t	sipd_cmdsts;	/* command/status word */
	u_int32_t	sipd_bufptr;	/* pointer to DMA segment */
#endif /* DP83820 */
#endif /* 0 */

static inline volatile uint32_t *
sipd_cmdsts(struct sip_softc *sc, struct sip_desc *sipd)
{
	return &sipd->sipd_cbs[(sc->sc_gigabit) ? 1 : 0];
}

static inline volatile uint32_t *
sipd_bufptr(struct sip_softc *sc, struct sip_desc *sipd)
{
	return &sipd->sipd_cbs[(sc->sc_gigabit) ? 0 : 1];
}

static inline void
sip_init_rxdesc(struct sip_softc *sc, int x)
{
	struct sip_rxsoft *rxs = &sc->sc_rxsoft[x];
	struct sip_desc *sipd = &sc->sc_rxdescs[x];

	sipd->sipd_link = htole32(SIP_CDRXADDR(sc, sip_nextrx(sc, x)));
	*sipd_bufptr(sc, sipd) = htole32(rxs->rxs_dmamap->dm_segs[0].ds_addr);
	*sipd_cmdsts(sc, sipd) = htole32(CMDSTS_INTR |
	    (sc->sc_parm->p_rxbuf_len & sc->sc_bits.b_cmdsts_size_mask));
	sipd->sipd_extsts = 0;
	sip_cdrxsync(sc, x, BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
}

#define	SIP_CHIP_VERS(sc, v, p, r)					\
	((sc)->sc_model->sip_vendor == (v) &&				\
	 (sc)->sc_model->sip_product == (p) &&				\
	 (sc)->sc_rev == (r))

#define	SIP_CHIP_MODEL(sc, v, p)					\
	((sc)->sc_model->sip_vendor == (v) &&				\
	 (sc)->sc_model->sip_product == (p))

#define	SIP_SIS900_REV(sc, rev)						\
	SIP_CHIP_VERS((sc), PCI_VENDOR_SIS, PCI_PRODUCT_SIS_900, (rev))

#define SIP_TIMEOUT 1000

static int	sip_ifflags_cb(struct ethercom *);
static void	sipcom_start(struct ifnet *);
static void	sipcom_watchdog(struct ifnet *);
static int	sipcom_ioctl(struct ifnet *, u_long, void *);
static int	sipcom_init(struct ifnet *);
static void	sipcom_stop(struct ifnet *, int);

static bool	sipcom_reset(struct sip_softc *);
static void	sipcom_rxdrain(struct sip_softc *);
static int	sipcom_add_rxbuf(struct sip_softc *, int);
static void	sipcom_read_eeprom(struct sip_softc *, int, int,
				      u_int16_t *);
static void	sipcom_tick(void *);

static void	sipcom_sis900_set_filter(struct sip_softc *);
static void	sipcom_dp83815_set_filter(struct sip_softc *);

static void	sipcom_dp83820_read_macaddr(struct sip_softc *,
		    const struct pci_attach_args *, u_int8_t *);
static void	sipcom_sis900_eeprom_delay(struct sip_softc *sc);
static void	sipcom_sis900_read_macaddr(struct sip_softc *,
		    const struct pci_attach_args *, u_int8_t *);
static void	sipcom_dp83815_read_macaddr(struct sip_softc *,
		    const struct pci_attach_args *, u_int8_t *);

static int	sipcom_intr(void *);
static void	sipcom_txintr(struct sip_softc *);
static void	sip_rxintr(struct sip_softc *);
static void	gsip_rxintr(struct sip_softc *);

static int	sipcom_dp83820_mii_readreg(device_t, int, int);
static void	sipcom_dp83820_mii_writereg(device_t, int, int, int);
static void	sipcom_dp83820_mii_statchg(struct ifnet *);

static int	sipcom_sis900_mii_readreg(device_t, int, int);
static void	sipcom_sis900_mii_writereg(device_t, int, int, int);
static void	sipcom_sis900_mii_statchg(struct ifnet *);

static int	sipcom_dp83815_mii_readreg(device_t, int, int);
static void	sipcom_dp83815_mii_writereg(device_t, int, int, int);
static void	sipcom_dp83815_mii_statchg(struct ifnet *);

static void	sipcom_mediastatus(struct ifnet *, struct ifmediareq *);

static int	sipcom_match(device_t, cfdata_t, void *);
static void	sipcom_attach(device_t, device_t, void *);
static void	sipcom_do_detach(device_t, enum sip_attach_stage);
static int	sipcom_detach(device_t, int);
static bool	sipcom_resume(device_t, const pmf_qual_t *);
static bool	sipcom_suspend(device_t, const pmf_qual_t *);

int	gsip_copy_small = 0;
int	sip_copy_small = 0;

CFATTACH_DECL3_NEW(gsip, sizeof(struct sip_softc),
    sipcom_match, sipcom_attach, sipcom_detach, NULL, NULL, NULL,
    DVF_DETACH_SHUTDOWN);
CFATTACH_DECL3_NEW(sip, sizeof(struct sip_softc),
    sipcom_match, sipcom_attach, sipcom_detach, NULL, NULL, NULL,
    DVF_DETACH_SHUTDOWN);

/*
 * Descriptions of the variants of the SiS900.
 */
struct sip_variant {
	int	(*sipv_mii_readreg)(device_t, int, int);
	void	(*sipv_mii_writereg)(device_t, int, int, int);
	void	(*sipv_mii_statchg)(struct ifnet *);
	void	(*sipv_set_filter)(struct sip_softc *);
	void	(*sipv_read_macaddr)(struct sip_softc *,
		    const struct pci_attach_args *, u_int8_t *);
};

static u_int32_t sipcom_mii_bitbang_read(device_t);
static void	sipcom_mii_bitbang_write(device_t, u_int32_t);

static const struct mii_bitbang_ops sipcom_mii_bitbang_ops = {
	sipcom_mii_bitbang_read,
	sipcom_mii_bitbang_write,
	{
		EROMAR_MDIO,		/* MII_BIT_MDO */
		EROMAR_MDIO,		/* MII_BIT_MDI */
		EROMAR_MDC,		/* MII_BIT_MDC */
		EROMAR_MDDIR,		/* MII_BIT_DIR_HOST_PHY */
		0,			/* MII_BIT_DIR_PHY_HOST */
	}
};

static const struct sip_variant sipcom_variant_dp83820 = {
	sipcom_dp83820_mii_readreg,
	sipcom_dp83820_mii_writereg,
	sipcom_dp83820_mii_statchg,
	sipcom_dp83815_set_filter,
	sipcom_dp83820_read_macaddr,
};

static const struct sip_variant sipcom_variant_sis900 = {
	sipcom_sis900_mii_readreg,
	sipcom_sis900_mii_writereg,
	sipcom_sis900_mii_statchg,
	sipcom_sis900_set_filter,
	sipcom_sis900_read_macaddr,
};

static const struct sip_variant sipcom_variant_dp83815 = {
	sipcom_dp83815_mii_readreg,
	sipcom_dp83815_mii_writereg,
	sipcom_dp83815_mii_statchg,
	sipcom_dp83815_set_filter,
	sipcom_dp83815_read_macaddr,
};


/*
 * Devices supported by this driver.
 */
static const struct sip_product {
	pci_vendor_id_t		sip_vendor;
	pci_product_id_t	sip_product;
	const char		*sip_name;
	const struct sip_variant *sip_variant;
	int			sip_gigabit;
} sipcom_products[] = {
	{ PCI_VENDOR_NS,	PCI_PRODUCT_NS_DP83820,
	  "NatSemi DP83820 Gigabit Ethernet",
	  &sipcom_variant_dp83820, 1 },
	{ PCI_VENDOR_SIS,	PCI_PRODUCT_SIS_900,
	  "SiS 900 10/100 Ethernet",
	  &sipcom_variant_sis900, 0 },
	{ PCI_VENDOR_SIS,	PCI_PRODUCT_SIS_7016,
	  "SiS 7016 10/100 Ethernet",
	  &sipcom_variant_sis900, 0 },

	{ PCI_VENDOR_NS,	PCI_PRODUCT_NS_DP83815,
	  "NatSemi DP83815 10/100 Ethernet",
	  &sipcom_variant_dp83815, 0 },

	{ 0,			0,
	  NULL,
	  NULL, 0 },
};

static const struct sip_product *
sipcom_lookup(const struct pci_attach_args *pa, bool gigabit)
{
	const struct sip_product *sip;

	for (sip = sipcom_products; sip->sip_name != NULL; sip++) {
		if (PCI_VENDOR(pa->pa_id) == sip->sip_vendor &&
		    PCI_PRODUCT(pa->pa_id) == sip->sip_product &&
		    sip->sip_gigabit == gigabit)
			return sip;
	}
	return NULL;
}

/*
 * I really hate stupid hardware vendors.  There's a bit in the EEPROM
 * which indicates if the card can do 64-bit data transfers.  Unfortunately,
 * several vendors of 32-bit cards fail to clear this bit in the EEPROM,
 * which means we try to use 64-bit data transfers on those cards if we
 * happen to be plugged into a 32-bit slot.
 *
 * What we do is use this table of cards known to be 64-bit cards.  If
 * you have a 64-bit card who's subsystem ID is not listed in this table,
 * send the output of "pcictl dump ..." of the device to me so that your
 * card will use the 64-bit data path when plugged into a 64-bit slot.
 *
 *	-- Jason R. Thorpe <thorpej@NetBSD.org>
 *	   June 30, 2002
 */
static int
sipcom_check_64bit(const struct pci_attach_args *pa)
{
	static const struct {
		pci_vendor_id_t c64_vendor;
		pci_product_id_t c64_product;
	} card64[] = {
		/* Asante GigaNIX */
		{ 0x128a,	0x0002 },

		/* Accton EN1407-T, Planex GN-1000TE */
		{ 0x1113,	0x1407 },

		/* Netgear GA621 */
		{ 0x1385,	0x621a },

		/* Netgear GA622 */
		{ 0x1385,	0x622a },

		/* SMC EZ Card 1000 (9462TX) */
		{ 0x10b8,	0x9462 },

		{ 0, 0}
	};
	pcireg_t subsys;
	int i;

	subsys = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);

	for (i = 0; card64[i].c64_vendor != 0; i++) {
		if (PCI_VENDOR(subsys) == card64[i].c64_vendor &&
		    PCI_PRODUCT(subsys) == card64[i].c64_product)
			return (1);
	}

	return (0);
}

static int
sipcom_match(device_t parent, cfdata_t cf, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (sipcom_lookup(pa, strcmp(cf->cf_name, "gsip") == 0) != NULL)
		return 1;

	return 0;
}

static void
sipcom_dp83820_attach(struct sip_softc *sc, struct pci_attach_args *pa)
{
	u_int32_t reg;
	int i;

	/*
	 * Cause the chip to load configuration data from the EEPROM.
	 */
	bus_space_write_4(sc->sc_st, sc->sc_sh, SIP_PTSCR, PTSCR_EELOAD_EN);
	for (i = 0; i < 10000; i++) {
		delay(10);
		if ((bus_space_read_4(sc->sc_st, sc->sc_sh, SIP_PTSCR) &
		    PTSCR_EELOAD_EN) == 0)
			break;
	}
	if (bus_space_read_4(sc->sc_st, sc->sc_sh, SIP_PTSCR) &
	    PTSCR_EELOAD_EN) {
		printf("%s: timeout loading configuration from EEPROM\n",
		    device_xname(sc->sc_dev));
		return;
	}

	sc->sc_gpior = bus_space_read_4(sc->sc_st, sc->sc_sh, SIP_GPIOR);

	reg = bus_space_read_4(sc->sc_st, sc->sc_sh, SIP_CFG);
	if (reg & CFG_PCI64_DET) {
		printf("%s: 64-bit PCI slot detected", device_xname(sc->sc_dev));
		/*
		 * Check to see if this card is 64-bit.  If so, enable 64-bit
		 * data transfers.
		 *
		 * We can't use the DATA64_EN bit in the EEPROM, because
		 * vendors of 32-bit cards fail to clear that bit in many
		 * cases (yet the card still detects that it's in a 64-bit
		 * slot; go figure).
		 */
		if (sipcom_check_64bit(pa)) {
			sc->sc_cfg |= CFG_DATA64_EN;
			printf(", using 64-bit data transfers");
		}
		printf("\n");
	}

	/*
	 * XXX Need some PCI flags indicating support for
	 * XXX 64-bit addressing.
	 */
#if 0
	if (reg & CFG_M64ADDR)
		sc->sc_cfg |= CFG_M64ADDR;
	if (reg & CFG_T64ADDR)
		sc->sc_cfg |= CFG_T64ADDR;
#endif

	if (reg & (CFG_TBI_EN|CFG_EXT_125)) {
		const char *sep = "";
		printf("%s: using ", device_xname(sc->sc_dev));
		if (reg & CFG_EXT_125) {
			sc->sc_cfg |= CFG_EXT_125;
			printf("%s125MHz clock", sep);
			sep = ", ";
		}
		if (reg & CFG_TBI_EN) {
			sc->sc_cfg |= CFG_TBI_EN;
			printf("%sten-bit interface", sep);
			sep = ", ";
		}
		printf("\n");
	}
	if ((pa->pa_flags & PCI_FLAGS_MRM_OKAY) == 0 ||
	    (reg & CFG_MRM_DIS) != 0)
		sc->sc_cfg |= CFG_MRM_DIS;
	if ((pa->pa_flags & PCI_FLAGS_MWI_OKAY) == 0 ||
	    (reg & CFG_MWI_DIS) != 0)
		sc->sc_cfg |= CFG_MWI_DIS;

	/*
	 * Use the extended descriptor format on the DP83820.  This
	 * gives us an interface to VLAN tagging and IPv4/TCP/UDP
	 * checksumming.
	 */
	sc->sc_cfg |= CFG_EXTSTS_EN;
}

static int
sipcom_detach(device_t self, int flags)
{
	int s;

	s = splnet();
	sipcom_do_detach(self, SIP_ATTACH_FIN);
	splx(s);

	return 0;
}

static void
sipcom_do_detach(device_t self, enum sip_attach_stage stage)
{
	int i;
	struct sip_softc *sc = device_private(self);
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	/*
	 * Free any resources we've allocated during attach.
	 * Do this in reverse order and fall through.
	 */
	switch (stage) {
	case SIP_ATTACH_FIN:
		sipcom_stop(ifp, 1);
		pmf_device_deregister(self);
#ifdef SIP_EVENT_COUNTERS
		/*
		 * Attach event counters.
		 */
		evcnt_detach(&sc->sc_ev_txforceintr);
		evcnt_detach(&sc->sc_ev_txdstall);
		evcnt_detach(&sc->sc_ev_txsstall);
		evcnt_detach(&sc->sc_ev_hiberr);
		evcnt_detach(&sc->sc_ev_rxintr);
		evcnt_detach(&sc->sc_ev_txiintr);
		evcnt_detach(&sc->sc_ev_txdintr);
		if (!sc->sc_gigabit) {
			evcnt_detach(&sc->sc_ev_rxpause);
		} else {
			evcnt_detach(&sc->sc_ev_txudpsum);
			evcnt_detach(&sc->sc_ev_txtcpsum);
			evcnt_detach(&sc->sc_ev_txipsum);
			evcnt_detach(&sc->sc_ev_rxudpsum);
			evcnt_detach(&sc->sc_ev_rxtcpsum);
			evcnt_detach(&sc->sc_ev_rxipsum);
			evcnt_detach(&sc->sc_ev_txpause);
			evcnt_detach(&sc->sc_ev_rxpause);
		}
#endif /* SIP_EVENT_COUNTERS */

		rnd_detach_source(&sc->rnd_source);

		ether_ifdetach(ifp);
		if_detach(ifp);
		mii_detach(&sc->sc_mii, MII_PHY_ANY, MII_OFFSET_ANY);

		/*FALLTHROUGH*/
	case SIP_ATTACH_CREATE_RXMAP:
		for (i = 0; i < sc->sc_parm->p_nrxdesc; i++) {
			if (sc->sc_rxsoft[i].rxs_dmamap != NULL)
				bus_dmamap_destroy(sc->sc_dmat,
				    sc->sc_rxsoft[i].rxs_dmamap);
		}
		/*FALLTHROUGH*/
	case SIP_ATTACH_CREATE_TXMAP:
		for (i = 0; i < SIP_TXQUEUELEN; i++) {
			if (sc->sc_txsoft[i].txs_dmamap != NULL)
				bus_dmamap_destroy(sc->sc_dmat,
				    sc->sc_txsoft[i].txs_dmamap);
		}
		/*FALLTHROUGH*/
	case SIP_ATTACH_LOAD_MAP:
		bus_dmamap_unload(sc->sc_dmat, sc->sc_cddmamap);
		/*FALLTHROUGH*/
	case SIP_ATTACH_CREATE_MAP:
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_cddmamap);
		/*FALLTHROUGH*/
	case SIP_ATTACH_MAP_MEM:
		bus_dmamem_unmap(sc->sc_dmat, (void *)sc->sc_control_data,
		    sizeof(struct sip_control_data));
		/*FALLTHROUGH*/
	case SIP_ATTACH_ALLOC_MEM:
		bus_dmamem_free(sc->sc_dmat, &sc->sc_seg, 1);
		/* FALLTHROUGH*/
	case SIP_ATTACH_INTR:
		pci_intr_disestablish(sc->sc_pc, sc->sc_ih);
		/* FALLTHROUGH*/
	case SIP_ATTACH_MAP:
		bus_space_unmap(sc->sc_st, sc->sc_sh, sc->sc_sz);
		break;
	default:
		break;
	}
	return;
}

static bool
sipcom_resume(device_t self, const pmf_qual_t *qual)
{
	struct sip_softc *sc = device_private(self);

	return sipcom_reset(sc);
}

static bool
sipcom_suspend(device_t self, const pmf_qual_t *qual)
{
	struct sip_softc *sc = device_private(self);

	sipcom_rxdrain(sc);
	return true;
}

static void
sipcom_attach(device_t parent, device_t self, void *aux)
{
	struct sip_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	bus_space_tag_t iot, memt;
	bus_space_handle_t ioh, memh;
	bus_size_t iosz, memsz;
	int ioh_valid, memh_valid;
	int i, rseg, error;
	const struct sip_product *sip;
	u_int8_t enaddr[ETHER_ADDR_LEN];
	pcireg_t csr;
	pcireg_t memtype;
	bus_size_t tx_dmamap_size;
	int ntxsegs_alloc;
	cfdata_t cf = device_cfdata(self);
	char intrbuf[PCI_INTRSTR_LEN];

	callout_init(&sc->sc_tick_ch, 0);

	sip = sipcom_lookup(pa, strcmp(cf->cf_name, "gsip") == 0);
	if (sip == NULL) {
		printf("\n");
		panic("%s: impossible", __func__);
	}
	sc->sc_dev = self;
	sc->sc_gigabit = sip->sip_gigabit;
	pmf_self_suspensor_init(self, &sc->sc_suspensor, &sc->sc_qual);
	sc->sc_pc = pc;

	if (sc->sc_gigabit) {
		sc->sc_rxintr = gsip_rxintr;
		sc->sc_parm = &gsip_parm;
	} else {
		sc->sc_rxintr = sip_rxintr;
		sc->sc_parm = &sip_parm;
	}
	tx_dmamap_size = sc->sc_parm->p_tx_dmamap_size;
	ntxsegs_alloc = sc->sc_parm->p_ntxsegs_alloc;
	sc->sc_ntxdesc = SIP_TXQUEUELEN * ntxsegs_alloc;
	sc->sc_ntxdesc_mask = sc->sc_ntxdesc - 1;
	sc->sc_nrxdesc_mask = sc->sc_parm->p_nrxdesc - 1;

	sc->sc_rev = PCI_REVISION(pa->pa_class);

	printf(": %s, rev %#02x\n", sip->sip_name, sc->sc_rev);

	sc->sc_model = sip;

	/*
	 * XXX Work-around broken PXE firmware on some boards.
	 *
	 * The DP83815 shares an address decoder with the MEM BAR
	 * and the ROM BAR.  Make sure the ROM BAR is disabled,
	 * so that memory mapped access works.
	 */
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_MAPREG_ROM,
	    pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_MAPREG_ROM) &
	    ~PCI_MAPREG_ROM_ENABLE);

	/*
	 * Map the device.
	 */
	ioh_valid = (pci_mapreg_map(pa, SIP_PCI_CFGIOA,
	    PCI_MAPREG_TYPE_IO, 0,
	    &iot, &ioh, NULL, &iosz) == 0);
	if (sc->sc_gigabit) {
		memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, SIP_PCI_CFGMA);
		switch (memtype) {
		case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
		case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
			memh_valid = (pci_mapreg_map(pa, SIP_PCI_CFGMA,
			    memtype, 0, &memt, &memh, NULL, &memsz) == 0);
			break;
		default:
			memh_valid = 0;
		}
	} else {
		memh_valid = (pci_mapreg_map(pa, SIP_PCI_CFGMA,
		    PCI_MAPREG_TYPE_MEM|PCI_MAPREG_MEM_TYPE_32BIT, 0,
		    &memt, &memh, NULL, &memsz) == 0);
	}

	if (memh_valid) {
		sc->sc_st = memt;
		sc->sc_sh = memh;
		sc->sc_sz = memsz;
	} else if (ioh_valid) {
		sc->sc_st = iot;
		sc->sc_sh = ioh;
		sc->sc_sz = iosz;
	} else {
		printf("%s: unable to map device registers\n",
		    device_xname(sc->sc_dev));
		return;
	}

	sc->sc_dmat = pa->pa_dmat;

	/*
	 * Make sure bus mastering is enabled.  Also make sure
	 * Write/Invalidate is enabled if we're allowed to use it.
	 */
	csr = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	if (pa->pa_flags & PCI_FLAGS_MWI_OKAY)
		csr |= PCI_COMMAND_INVALIDATE_ENABLE;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    csr | PCI_COMMAND_MASTER_ENABLE);

	/* power up chip */
	error = pci_activate(pa->pa_pc, pa->pa_tag, self, pci_activate_null);
	if (error != 0 && error != EOPNOTSUPP) {
		aprint_error_dev(sc->sc_dev, "cannot activate %d\n", error);
		return;
	}

	/*
	 * Map and establish our interrupt.
	 */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(sc->sc_dev, "unable to map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, sipcom_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "unable to establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		sipcom_do_detach(self, SIP_ATTACH_MAP);
		return;
	}
	aprint_normal_dev(sc->sc_dev, "interrupting at %s\n", intrstr);

	SIMPLEQ_INIT(&sc->sc_txfreeq);
	SIMPLEQ_INIT(&sc->sc_txdirtyq);

	/*
	 * Allocate the control data structures, and create and load the
	 * DMA map for it.
	 */
	if ((error = bus_dmamem_alloc(sc->sc_dmat,
	    sizeof(struct sip_control_data), PAGE_SIZE, 0, &sc->sc_seg, 1,
	    &rseg, 0)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to allocate control data, error = %d\n",
		    error);
		sipcom_do_detach(self, SIP_ATTACH_INTR);
		return;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, &sc->sc_seg, rseg,
	    sizeof(struct sip_control_data), (void **)&sc->sc_control_data,
	    BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to map control data, error = %d\n",
		    error);
		sipcom_do_detach(self, SIP_ATTACH_ALLOC_MEM);
	}

	if ((error = bus_dmamap_create(sc->sc_dmat,
	    sizeof(struct sip_control_data), 1,
	    sizeof(struct sip_control_data), 0, 0, &sc->sc_cddmamap)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to create control data DMA map, "
		    "error = %d\n", error);
		sipcom_do_detach(self, SIP_ATTACH_MAP_MEM);
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_cddmamap,
	    sc->sc_control_data, sizeof(struct sip_control_data), NULL,
	    0)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to load control data DMA map, error = %d\n",
		    error);
		sipcom_do_detach(self, SIP_ATTACH_CREATE_MAP);
	}

	/*
	 * Create the transmit buffer DMA maps.
	 */
	for (i = 0; i < SIP_TXQUEUELEN; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, tx_dmamap_size,
		    sc->sc_parm->p_ntxsegs, MCLBYTES, 0, 0,
		    &sc->sc_txsoft[i].txs_dmamap)) != 0) {
			aprint_error_dev(sc->sc_dev, "unable to create tx DMA map %d, "
			    "error = %d\n", i, error);
			sipcom_do_detach(self, SIP_ATTACH_CREATE_TXMAP);
		}
	}

	/*
	 * Create the receive buffer DMA maps.
	 */
	for (i = 0; i < sc->sc_parm->p_nrxdesc; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1,
		    MCLBYTES, 0, 0, &sc->sc_rxsoft[i].rxs_dmamap)) != 0) {
			aprint_error_dev(sc->sc_dev, "unable to create rx DMA map %d, "
			    "error = %d\n", i, error);
			sipcom_do_detach(self, SIP_ATTACH_CREATE_RXMAP);
		}
		sc->sc_rxsoft[i].rxs_mbuf = NULL;
	}

	/*
	 * Reset the chip to a known state.
	 */
	sipcom_reset(sc);

	/*
	 * Read the Ethernet address from the EEPROM.  This might
	 * also fetch other stuff from the EEPROM and stash it
	 * in the softc.
	 */
	sc->sc_cfg = 0;
	if (!sc->sc_gigabit) {
		if (SIP_SIS900_REV(sc,SIS_REV_635) ||
		    SIP_SIS900_REV(sc,SIS_REV_900B))
			sc->sc_cfg |= (CFG_PESEL | CFG_RNDCNT);

		if (SIP_SIS900_REV(sc,SIS_REV_635) ||
		    SIP_SIS900_REV(sc,SIS_REV_960) ||
		    SIP_SIS900_REV(sc,SIS_REV_900B))
			sc->sc_cfg |=
			    (bus_space_read_4(sc->sc_st, sc->sc_sh, SIP_CFG) &
			     CFG_EDBMASTEN);
	}

	(*sip->sip_variant->sipv_read_macaddr)(sc, pa, enaddr);

	printf("%s: Ethernet address %s\n", device_xname(sc->sc_dev),
	    ether_sprintf(enaddr));

	/*
	 * Initialize the configuration register: aggressive PCI
	 * bus request algorithm, default backoff, default OW timer,
	 * default parity error detection.
	 *
	 * NOTE: "Big endian mode" is useless on the SiS900 and
	 * friends -- it affects packet data, not descriptors.
	 */
	if (sc->sc_gigabit)
		sipcom_dp83820_attach(sc, pa);

	/*
	 * Initialize our media structures and probe the MII.
	 */
	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = sip->sip_variant->sipv_mii_readreg;
	sc->sc_mii.mii_writereg = sip->sip_variant->sipv_mii_writereg;
	sc->sc_mii.mii_statchg = sip->sip_variant->sipv_mii_statchg;
	sc->sc_ethercom.ec_mii = &sc->sc_mii;
	ifmedia_init(&sc->sc_mii.mii_media, IFM_IMASK, ether_mediachange,
	    sipcom_mediastatus);

	/*
	 * XXX We cannot handle flow control on the DP83815.
	 */
	if (SIP_CHIP_MODEL(sc, PCI_VENDOR_NS, PCI_PRODUCT_NS_DP83815))
		mii_attach(sc->sc_dev, &sc->sc_mii, 0xffffffff, MII_PHY_ANY,
			   MII_OFFSET_ANY, 0);
	else
		mii_attach(sc->sc_dev, &sc->sc_mii, 0xffffffff, MII_PHY_ANY,
			   MII_OFFSET_ANY, MIIF_DOPAUSE);
	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE, 0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE);
	} else
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);

	ifp = &sc->sc_ethercom.ec_if;
	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	sc->sc_if_flags = ifp->if_flags;
	ifp->if_ioctl = sipcom_ioctl;
	ifp->if_start = sipcom_start;
	ifp->if_watchdog = sipcom_watchdog;
	ifp->if_init = sipcom_init;
	ifp->if_stop = sipcom_stop;
	IFQ_SET_READY(&ifp->if_snd);

	/*
	 * We can support 802.1Q VLAN-sized frames.
	 */
	sc->sc_ethercom.ec_capabilities |= ETHERCAP_VLAN_MTU;

	if (sc->sc_gigabit) {
		/*
		 * And the DP83820 can do VLAN tagging in hardware, and
		 * support the jumbo Ethernet MTU.
		 */
		sc->sc_ethercom.ec_capabilities |=
		    ETHERCAP_VLAN_HWTAGGING | ETHERCAP_JUMBO_MTU;

		/*
		 * The DP83820 can do IPv4, TCPv4, and UDPv4 checksums
		 * in hardware.
		 */
		ifp->if_capabilities |=
		    IFCAP_CSUM_IPv4_Tx | IFCAP_CSUM_IPv4_Rx |
		    IFCAP_CSUM_TCPv4_Tx | IFCAP_CSUM_TCPv4_Rx |
		    IFCAP_CSUM_UDPv4_Tx | IFCAP_CSUM_UDPv4_Rx;
	}

	/*
	 * Attach the interface.
	 */
	if_attach(ifp);
	ether_ifattach(ifp, enaddr);
	ether_set_ifflags_cb(&sc->sc_ethercom, sip_ifflags_cb);
	sc->sc_prev.ec_capenable = sc->sc_ethercom.ec_capenable;
	sc->sc_prev.is_vlan = VLAN_ATTACHED(&(sc)->sc_ethercom);
	sc->sc_prev.if_capenable = ifp->if_capenable;
	rnd_attach_source(&sc->rnd_source, device_xname(sc->sc_dev),
	    RND_TYPE_NET, RND_FLAG_DEFAULT);

	/*
	 * The number of bytes that must be available in
	 * the Tx FIFO before the bus master can DMA more
	 * data into the FIFO.
	 */
	sc->sc_tx_fill_thresh = 64 / 32;

	/*
	 * Start at a drain threshold of 512 bytes.  We will
	 * increase it if a DMA underrun occurs.
	 *
	 * XXX The minimum value of this variable should be
	 * tuned.  We may be able to improve performance
	 * by starting with a lower value.  That, however,
	 * may trash the first few outgoing packets if the
	 * PCI bus is saturated.
	 */
	if (sc->sc_gigabit)
		sc->sc_tx_drain_thresh = 6400 / 32; /* from FreeBSD nge(4) */
	else
		sc->sc_tx_drain_thresh = 1504 / 32;

	/*
	 * Initialize the Rx FIFO drain threshold.
	 *
	 * This is in units of 8 bytes.
	 *
	 * We should never set this value lower than 2; 14 bytes are
	 * required to filter the packet.
	 */
	sc->sc_rx_drain_thresh = 128 / 8;

#ifdef SIP_EVENT_COUNTERS
	/*
	 * Attach event counters.
	 */
	evcnt_attach_dynamic(&sc->sc_ev_txsstall, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "txsstall");
	evcnt_attach_dynamic(&sc->sc_ev_txdstall, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "txdstall");
	evcnt_attach_dynamic(&sc->sc_ev_txforceintr, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "txforceintr");
	evcnt_attach_dynamic(&sc->sc_ev_txdintr, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "txdintr");
	evcnt_attach_dynamic(&sc->sc_ev_txiintr, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "txiintr");
	evcnt_attach_dynamic(&sc->sc_ev_rxintr, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "rxintr");
	evcnt_attach_dynamic(&sc->sc_ev_hiberr, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "hiberr");
	if (!sc->sc_gigabit) {
		evcnt_attach_dynamic(&sc->sc_ev_rxpause, EVCNT_TYPE_INTR,
		    NULL, device_xname(sc->sc_dev), "rxpause");
	} else {
		evcnt_attach_dynamic(&sc->sc_ev_rxpause, EVCNT_TYPE_MISC,
		    NULL, device_xname(sc->sc_dev), "rxpause");
		evcnt_attach_dynamic(&sc->sc_ev_txpause, EVCNT_TYPE_MISC,
		    NULL, device_xname(sc->sc_dev), "txpause");
		evcnt_attach_dynamic(&sc->sc_ev_rxipsum, EVCNT_TYPE_MISC,
		    NULL, device_xname(sc->sc_dev), "rxipsum");
		evcnt_attach_dynamic(&sc->sc_ev_rxtcpsum, EVCNT_TYPE_MISC,
		    NULL, device_xname(sc->sc_dev), "rxtcpsum");
		evcnt_attach_dynamic(&sc->sc_ev_rxudpsum, EVCNT_TYPE_MISC,
		    NULL, device_xname(sc->sc_dev), "rxudpsum");
		evcnt_attach_dynamic(&sc->sc_ev_txipsum, EVCNT_TYPE_MISC,
		    NULL, device_xname(sc->sc_dev), "txipsum");
		evcnt_attach_dynamic(&sc->sc_ev_txtcpsum, EVCNT_TYPE_MISC,
		    NULL, device_xname(sc->sc_dev), "txtcpsum");
		evcnt_attach_dynamic(&sc->sc_ev_txudpsum, EVCNT_TYPE_MISC,
		    NULL, device_xname(sc->sc_dev), "txudpsum");
	}
#endif /* SIP_EVENT_COUNTERS */

	if (pmf_device_register(self, sipcom_suspend, sipcom_resume))
		pmf_class_network_register(self, ifp);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");
}

static inline void
sipcom_set_extsts(struct sip_softc *sc, int lasttx, struct mbuf *m0,
    uint64_t capenable)
{
	struct m_tag *mtag;
	u_int32_t extsts;
#ifdef DEBUG
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
#endif
	/*
	 * If VLANs are enabled and the packet has a VLAN tag, set
	 * up the descriptor to encapsulate the packet for us.
	 *
	 * This apparently has to be on the last descriptor of
	 * the packet.
	 */

	/*
	 * Byte swapping is tricky. We need to provide the tag
	 * in a network byte order. On a big-endian machine,
	 * the byteorder is correct, but we need to swap it
	 * anyway, because this will be undone by the outside
	 * htole32(). That's why there must be an
	 * unconditional swap instead of htons() inside.
	 */
	if ((mtag = VLAN_OUTPUT_TAG(&sc->sc_ethercom, m0)) != NULL) {
		sc->sc_txdescs[lasttx].sipd_extsts |=
		    htole32(EXTSTS_VPKT |
				(bswap16(VLAN_TAG_VALUE(mtag)) &
				 EXTSTS_VTCI));
	}

	/*
	 * If the upper-layer has requested IPv4/TCPv4/UDPv4
	 * checksumming, set up the descriptor to do this work
	 * for us.
	 *
	 * This apparently has to be on the first descriptor of
	 * the packet.
	 *
	 * Byte-swap constants so the compiler can optimize.
	 */
	extsts = 0;
	if (m0->m_pkthdr.csum_flags & M_CSUM_IPv4) {
		KDASSERT(ifp->if_capenable & IFCAP_CSUM_IPv4_Tx);
		SIP_EVCNT_INCR(&sc->sc_ev_txipsum);
		extsts |= htole32(EXTSTS_IPPKT);
	}
	if (m0->m_pkthdr.csum_flags & M_CSUM_TCPv4) {
		KDASSERT(ifp->if_capenable & IFCAP_CSUM_TCPv4_Tx);
		SIP_EVCNT_INCR(&sc->sc_ev_txtcpsum);
		extsts |= htole32(EXTSTS_TCPPKT);
	} else if (m0->m_pkthdr.csum_flags & M_CSUM_UDPv4) {
		KDASSERT(ifp->if_capenable & IFCAP_CSUM_UDPv4_Tx);
		SIP_EVCNT_INCR(&sc->sc_ev_txudpsum);
		extsts |= htole32(EXTSTS_UDPPKT);
	}
	sc->sc_txdescs[sc->sc_txnext].sipd_extsts |= extsts;
}

/*
 * sip_start:		[ifnet interface function]
 *
 *	Start packet transmission on the interface.
 */
static void
sipcom_start(struct ifnet *ifp)
{
	struct sip_softc *sc = ifp->if_softc;
	struct mbuf *m0;
	struct mbuf *m;
	struct sip_txsoft *txs;
	bus_dmamap_t dmamap;
	int error, nexttx, lasttx, seg;
	int ofree = sc->sc_txfree;
#if 0
	int firsttx = sc->sc_txnext;
#endif

	/*
	 * If we've been told to pause, don't transmit any more packets.
	 */
	if (!sc->sc_gigabit && sc->sc_paused)
		ifp->if_flags |= IFF_OACTIVE;

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

	/*
	 * Loop through the send queue, setting up transmit descriptors
	 * until we drain the queue, or use up all available transmit
	 * descriptors.
	 */
	for (;;) {
		/* Get a work queue entry. */
		if ((txs = SIMPLEQ_FIRST(&sc->sc_txfreeq)) == NULL) {
			SIP_EVCNT_INCR(&sc->sc_ev_txsstall);
			break;
		}

		/*
		 * Grab a packet off the queue.
		 */
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;
		m = NULL;

		dmamap = txs->txs_dmamap;

		/*
		 * Load the DMA map.  If this fails, the packet either
		 * didn't fit in the alloted number of segments, or we
		 * were short on resources.
		 */
		error = bus_dmamap_load_mbuf(sc->sc_dmat, dmamap, m0,
		    BUS_DMA_WRITE|BUS_DMA_NOWAIT);
		/* In the non-gigabit case, we'll copy and try again. */
		if (error != 0 && !sc->sc_gigabit) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				printf("%s: unable to allocate Tx mbuf\n",
				    device_xname(sc->sc_dev));
				break;
			}
			MCLAIM(m, &sc->sc_ethercom.ec_tx_mowner);
			if (m0->m_pkthdr.len > MHLEN) {
				MCLGET(m, M_DONTWAIT);
				if ((m->m_flags & M_EXT) == 0) {
					printf("%s: unable to allocate Tx "
					    "cluster\n", device_xname(sc->sc_dev));
					m_freem(m);
					break;
				}
			}
			m_copydata(m0, 0, m0->m_pkthdr.len, mtod(m, void *));
			m->m_pkthdr.len = m->m_len = m0->m_pkthdr.len;
			error = bus_dmamap_load_mbuf(sc->sc_dmat, dmamap,
			    m, BUS_DMA_WRITE|BUS_DMA_NOWAIT);
			if (error) {
				printf("%s: unable to load Tx buffer, "
				    "error = %d\n", device_xname(sc->sc_dev), error);
				break;
			}
		} else if (error == EFBIG) {
			/*
			 * For the too-many-segments case, we simply
			 * report an error and drop the packet,
			 * since we can't sanely copy a jumbo packet
			 * to a single buffer.
			 */
			printf("%s: Tx packet consumes too many "
			    "DMA segments, dropping...\n", device_xname(sc->sc_dev));
			IFQ_DEQUEUE(&ifp->if_snd, m0);
			m_freem(m0);
			continue;
		} else if (error != 0) {
			/*
			 * Short on resources, just stop for now.
			 */
			break;
		}

		/*
		 * Ensure we have enough descriptors free to describe
		 * the packet.  Note, we always reserve one descriptor
		 * at the end of the ring as a termination point, to
		 * prevent wrap-around.
		 */
		if (dmamap->dm_nsegs > (sc->sc_txfree - 1)) {
			/*
			 * Not enough free descriptors to transmit this
			 * packet.  We haven't committed anything yet,
			 * so just unload the DMA map, put the packet
			 * back on the queue, and punt.  Notify the upper
			 * layer that there are not more slots left.
			 *
			 * XXX We could allocate an mbuf and copy, but
			 * XXX is it worth it?
			 */
			ifp->if_flags |= IFF_OACTIVE;
			bus_dmamap_unload(sc->sc_dmat, dmamap);
			if (m != NULL)
				m_freem(m);
			SIP_EVCNT_INCR(&sc->sc_ev_txdstall);
			break;
		}

		IFQ_DEQUEUE(&ifp->if_snd, m0);
		if (m != NULL) {
			m_freem(m0);
			m0 = m;
		}

		/*
		 * WE ARE NOW COMMITTED TO TRANSMITTING THE PACKET.
		 */

		/* Sync the DMA map. */
		bus_dmamap_sync(sc->sc_dmat, dmamap, 0, dmamap->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		/*
		 * Initialize the transmit descriptors.
		 */
		for (nexttx = lasttx = sc->sc_txnext, seg = 0;
		     seg < dmamap->dm_nsegs;
		     seg++, nexttx = sip_nexttx(sc, nexttx)) {
			/*
			 * If this is the first descriptor we're
			 * enqueueing, don't set the OWN bit just
			 * yet.  That could cause a race condition.
			 * We'll do it below.
			 */
			*sipd_bufptr(sc, &sc->sc_txdescs[nexttx]) =
			    htole32(dmamap->dm_segs[seg].ds_addr);
			*sipd_cmdsts(sc, &sc->sc_txdescs[nexttx]) =
			    htole32((nexttx == sc->sc_txnext ? 0 : CMDSTS_OWN) |
			    CMDSTS_MORE | dmamap->dm_segs[seg].ds_len);
			sc->sc_txdescs[nexttx].sipd_extsts = 0;
			lasttx = nexttx;
		}

		/* Clear the MORE bit on the last segment. */
		*sipd_cmdsts(sc, &sc->sc_txdescs[lasttx]) &=
		    htole32(~CMDSTS_MORE);

		/*
		 * If we're in the interrupt delay window, delay the
		 * interrupt.
		 */
		if (++sc->sc_txwin >= (SIP_TXQUEUELEN * 2 / 3)) {
			SIP_EVCNT_INCR(&sc->sc_ev_txforceintr);
			*sipd_cmdsts(sc, &sc->sc_txdescs[lasttx]) |=
			    htole32(CMDSTS_INTR);
			sc->sc_txwin = 0;
		}

		if (sc->sc_gigabit)
			sipcom_set_extsts(sc, lasttx, m0, ifp->if_capenable);

		/* Sync the descriptors we're using. */
		sip_cdtxsync(sc, sc->sc_txnext, dmamap->dm_nsegs,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/*
		 * The entire packet is set up.  Give the first descrptor
		 * to the chip now.
		 */
		*sipd_cmdsts(sc, &sc->sc_txdescs[sc->sc_txnext]) |=
		    htole32(CMDSTS_OWN);
		sip_cdtxsync(sc, sc->sc_txnext, 1,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/*
		 * Store a pointer to the packet so we can free it later,
		 * and remember what txdirty will be once the packet is
		 * done.
		 */
		txs->txs_mbuf = m0;
		txs->txs_firstdesc = sc->sc_txnext;
		txs->txs_lastdesc = lasttx;

		/* Advance the tx pointer. */
		sc->sc_txfree -= dmamap->dm_nsegs;
		sc->sc_txnext = nexttx;

		SIMPLEQ_REMOVE_HEAD(&sc->sc_txfreeq, txs_q);
		SIMPLEQ_INSERT_TAIL(&sc->sc_txdirtyq, txs, txs_q);

		/*
		 * Pass the packet to any BPF listeners.
		 */
		bpf_mtap(ifp, m0);
	}

	if (txs == NULL || sc->sc_txfree == 0) {
		/* No more slots left; notify upper layer. */
		ifp->if_flags |= IFF_OACTIVE;
	}

	if (sc->sc_txfree != ofree) {
		/*
		 * Start the transmit process.  Note, the manual says
		 * that if there are no pending transmissions in the
		 * chip's internal queue (indicated by TXE being clear),
		 * then the driver software must set the TXDP to the
		 * first descriptor to be transmitted.  However, if we
		 * do this, it causes serious performance degredation on
		 * the DP83820 under load, not setting TXDP doesn't seem
		 * to adversely affect the SiS 900 or DP83815.
		 *
		 * Well, I guess it wouldn't be the first time a manual
		 * has lied -- and they could be speaking of the NULL-
		 * terminated descriptor list case, rather than OWN-
		 * terminated rings.
		 */
#if 0
		if ((bus_space_read_4(sc->sc_st, sc->sc_sh, SIP_CR) &
		     CR_TXE) == 0) {
			bus_space_write_4(sc->sc_st, sc->sc_sh, SIP_TXDP,
			    SIP_CDTXADDR(sc, firsttx));
			bus_space_write_4(sc->sc_st, sc->sc_sh, SIP_CR, CR_TXE);
		}
#else
		bus_space_write_4(sc->sc_st, sc->sc_sh, SIP_CR, CR_TXE);
#endif

		/* Set a watchdog timer in case the chip flakes out. */
		/* Gigabit autonegotiation takes 5 seconds. */
		ifp->if_timer = (sc->sc_gigabit) ? 10 : 5;
	}
}

/*
 * sip_watchdog:	[ifnet interface function]
 *
 *	Watchdog timer handler.
 */
static void
sipcom_watchdog(struct ifnet *ifp)
{
	struct sip_softc *sc = ifp->if_softc;

	/*
	 * The chip seems to ignore the CMDSTS_INTR bit sometimes!
	 * If we get a timeout, try and sweep up transmit descriptors.
	 * If we manage to sweep them all up, ignore the lack of
	 * interrupt.
	 */
	sipcom_txintr(sc);

	if (sc->sc_txfree != sc->sc_ntxdesc) {
		printf("%s: device timeout\n", device_xname(sc->sc_dev));
		ifp->if_oerrors++;

		/* Reset the interface. */
		(void) sipcom_init(ifp);
	} else if (ifp->if_flags & IFF_DEBUG)
		printf("%s: recovered from device timeout\n",
		    device_xname(sc->sc_dev));

	/* Try to get more packets going. */
	sipcom_start(ifp);
}

/* If the interface is up and running, only modify the receive
 * filter when setting promiscuous or debug mode.  Otherwise fall
 * through to ether_ioctl, which will reset the chip.
 */
static int
sip_ifflags_cb(struct ethercom *ec)
{
#define COMPARE_EC(sc) (((sc)->sc_prev.ec_capenable			\
			 == (sc)->sc_ethercom.ec_capenable)		\
			&& ((sc)->sc_prev.is_vlan ==			\
			    VLAN_ATTACHED(&(sc)->sc_ethercom) ))
#define COMPARE_IC(sc, ifp) ((sc)->sc_prev.if_capenable == (ifp)->if_capenable)
	struct ifnet *ifp = &ec->ec_if;
	struct sip_softc *sc = ifp->if_softc;
	int change = ifp->if_flags ^ sc->sc_if_flags;

	if ((change & ~(IFF_CANTCHANGE|IFF_DEBUG)) != 0 || !COMPARE_EC(sc) ||
	    !COMPARE_IC(sc, ifp))
		return ENETRESET;
	/* Set up the receive filter. */
	(*sc->sc_model->sip_variant->sipv_set_filter)(sc);
	return 0;
}

/*
 * sip_ioctl:		[ifnet interface function]
 *
 *	Handle control requests from the operator.
 */
static int
sipcom_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct sip_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error;

	s = splnet();

	switch (cmd) {
	case SIOCSIFMEDIA:
		/* Flow control requires full-duplex mode. */
		if (IFM_SUBTYPE(ifr->ifr_media) == IFM_AUTO ||
		    (ifr->ifr_media & IFM_FDX) == 0)
		    	ifr->ifr_media &= ~IFM_ETH_FMASK;

		/* XXX */
		if (SIP_CHIP_MODEL(sc, PCI_VENDOR_NS, PCI_PRODUCT_NS_DP83815))
			ifr->ifr_media &= ~IFM_ETH_FMASK;
		if (IFM_SUBTYPE(ifr->ifr_media) != IFM_AUTO) {
			if (sc->sc_gigabit &&
			    (ifr->ifr_media & IFM_ETH_FMASK) == IFM_FLOW) {
				/* We can do both TXPAUSE and RXPAUSE. */
				ifr->ifr_media |=
				    IFM_ETH_TXPAUSE | IFM_ETH_RXPAUSE;
			} else if (ifr->ifr_media & IFM_FLOW) {
				/*
				 * Both TXPAUSE and RXPAUSE must be set.
				 * (SiS900 and DP83815 don't have PAUSE_ASYM
				 * feature.)
				 *
				 * XXX Can SiS900 and DP83815 send PAUSE?
				 */
				ifr->ifr_media |=
				    IFM_ETH_TXPAUSE | IFM_ETH_RXPAUSE;
			}
			sc->sc_flowflags = ifr->ifr_media & IFM_ETH_FMASK;
		}
		/*FALLTHROUGH*/
	default:
		if ((error = ether_ioctl(ifp, cmd, data)) != ENETRESET)
			break;

		error = 0;

		if (cmd == SIOCSIFCAP)
			error = (*ifp->if_init)(ifp);
		else if (cmd != SIOCADDMULTI && cmd != SIOCDELMULTI)
			;
		else if (ifp->if_flags & IFF_RUNNING) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			(*sc->sc_model->sip_variant->sipv_set_filter)(sc);
		}
		break;
	}

	/* Try to get more packets going. */
	sipcom_start(ifp);

	sc->sc_if_flags = ifp->if_flags;
	splx(s);
	return (error);
}

/*
 * sip_intr:
 *
 *	Interrupt service routine.
 */
static int
sipcom_intr(void *arg)
{
	struct sip_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	u_int32_t isr;
	int handled = 0;

	if (!device_activation(sc->sc_dev, DEVACT_LEVEL_DRIVER))
		return 0;

	/* Disable interrupts. */
	bus_space_write_4(sc->sc_st, sc->sc_sh, SIP_IER, 0);

	for (;;) {
		/* Reading clears interrupt. */
		isr = bus_space_read_4(sc->sc_st, sc->sc_sh, SIP_ISR);
		if ((isr & sc->sc_imr) == 0)
			break;

		rnd_add_uint32(&sc->rnd_source, isr);

		handled = 1;

		if ((ifp->if_flags & IFF_RUNNING) == 0)
			break;

		if (isr & (ISR_RXORN|ISR_RXIDLE|ISR_RXDESC)) {
			SIP_EVCNT_INCR(&sc->sc_ev_rxintr);

			/* Grab any new packets. */
			(*sc->sc_rxintr)(sc);

			if (isr & ISR_RXORN) {
				printf("%s: receive FIFO overrun\n",
				    device_xname(sc->sc_dev));

				/* XXX adjust rx_drain_thresh? */
			}

			if (isr & ISR_RXIDLE) {
				printf("%s: receive ring overrun\n",
				    device_xname(sc->sc_dev));

				/* Get the receive process going again. */
				bus_space_write_4(sc->sc_st, sc->sc_sh,
				    SIP_RXDP, SIP_CDRXADDR(sc, sc->sc_rxptr));
				bus_space_write_4(sc->sc_st, sc->sc_sh,
				    SIP_CR, CR_RXE);
			}
		}

		if (isr & (ISR_TXURN|ISR_TXDESC|ISR_TXIDLE)) {
#ifdef SIP_EVENT_COUNTERS
			if (isr & ISR_TXDESC)
				SIP_EVCNT_INCR(&sc->sc_ev_txdintr);
			else if (isr & ISR_TXIDLE)
				SIP_EVCNT_INCR(&sc->sc_ev_txiintr);
#endif

			/* Sweep up transmit descriptors. */
			sipcom_txintr(sc);

			if (isr & ISR_TXURN) {
				u_int32_t thresh;
				int txfifo_size = (sc->sc_gigabit)
				    ? DP83820_SIP_TXFIFO_SIZE
				    : OTHER_SIP_TXFIFO_SIZE;

				printf("%s: transmit FIFO underrun",
				    device_xname(sc->sc_dev));
				thresh = sc->sc_tx_drain_thresh + 1;
				if (thresh <= __SHIFTOUT_MASK(sc->sc_bits.b_txcfg_drth_mask)
				&& (thresh * 32) <= (txfifo_size -
				     (sc->sc_tx_fill_thresh * 32))) {
					printf("; increasing Tx drain "
					    "threshold to %u bytes\n",
					    thresh * 32);
					sc->sc_tx_drain_thresh = thresh;
					(void) sipcom_init(ifp);
				} else {
					(void) sipcom_init(ifp);
					printf("\n");
				}
			}
		}

		if (sc->sc_imr & (ISR_PAUSE_END|ISR_PAUSE_ST)) {
			if (isr & ISR_PAUSE_ST) {
				sc->sc_paused = 1;
				SIP_EVCNT_INCR(&sc->sc_ev_rxpause);
				ifp->if_flags |= IFF_OACTIVE;
			}
			if (isr & ISR_PAUSE_END) {
				sc->sc_paused = 0;
				ifp->if_flags &= ~IFF_OACTIVE;
			}
		}

		if (isr & ISR_HIBERR) {
			int want_init = 0;

			SIP_EVCNT_INCR(&sc->sc_ev_hiberr);

#define	PRINTERR(bit, str)						\
			do {						\
				if ((isr & (bit)) != 0) {		\
					if ((ifp->if_flags & IFF_DEBUG) != 0) \
						printf("%s: %s\n",	\
						    device_xname(sc->sc_dev), str); \
					want_init = 1;			\
				}					\
			} while (/*CONSTCOND*/0)

			PRINTERR(sc->sc_bits.b_isr_dperr, "parity error");
			PRINTERR(sc->sc_bits.b_isr_sserr, "system error");
			PRINTERR(sc->sc_bits.b_isr_rmabt, "master abort");
			PRINTERR(sc->sc_bits.b_isr_rtabt, "target abort");
			PRINTERR(ISR_RXSOVR, "receive status FIFO overrun");
			/*
			 * Ignore:
			 *	Tx reset complete
			 *	Rx reset complete
			 */
			if (want_init)
				(void) sipcom_init(ifp);
#undef PRINTERR
		}
	}

	/* Re-enable interrupts. */
	bus_space_write_4(sc->sc_st, sc->sc_sh, SIP_IER, IER_IE);

	/* Try to get more packets going. */
	sipcom_start(ifp);

	return (handled);
}

/*
 * sip_txintr:
 *
 *	Helper; handle transmit interrupts.
 */
static void
sipcom_txintr(struct sip_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct sip_txsoft *txs;
	u_int32_t cmdsts;

	if (sc->sc_paused == 0)
		ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * Go through our Tx list and free mbufs for those
	 * frames which have been transmitted.
	 */
	while ((txs = SIMPLEQ_FIRST(&sc->sc_txdirtyq)) != NULL) {
		sip_cdtxsync(sc, txs->txs_firstdesc, txs->txs_dmamap->dm_nsegs,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		cmdsts = le32toh(*sipd_cmdsts(sc, &sc->sc_txdescs[txs->txs_lastdesc]));
		if (cmdsts & CMDSTS_OWN)
			break;

		SIMPLEQ_REMOVE_HEAD(&sc->sc_txdirtyq, txs_q);

		sc->sc_txfree += txs->txs_dmamap->dm_nsegs;

		bus_dmamap_sync(sc->sc_dmat, txs->txs_dmamap,
		    0, txs->txs_dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, txs->txs_dmamap);
		m_freem(txs->txs_mbuf);
		txs->txs_mbuf = NULL;

		SIMPLEQ_INSERT_TAIL(&sc->sc_txfreeq, txs, txs_q);

		/*
		 * Check for errors and collisions.
		 */
		if (cmdsts &
		    (CMDSTS_Tx_TXA|CMDSTS_Tx_TFU|CMDSTS_Tx_ED|CMDSTS_Tx_EC)) {
			ifp->if_oerrors++;
			if (cmdsts & CMDSTS_Tx_EC)
				ifp->if_collisions += 16;
			if (ifp->if_flags & IFF_DEBUG) {
				if (cmdsts & CMDSTS_Tx_ED)
					printf("%s: excessive deferral\n",
					    device_xname(sc->sc_dev));
				if (cmdsts & CMDSTS_Tx_EC)
					printf("%s: excessive collisions\n",
					    device_xname(sc->sc_dev));
			}
		} else {
			/* Packet was transmitted successfully. */
			ifp->if_opackets++;
			ifp->if_collisions += CMDSTS_COLLISIONS(cmdsts);
		}
	}

	/*
	 * If there are no more pending transmissions, cancel the watchdog
	 * timer.
	 */
	if (txs == NULL) {
		ifp->if_timer = 0;
		sc->sc_txwin = 0;
	}
}

/*
 * gsip_rxintr:
 *
 *	Helper; handle receive interrupts on gigabit parts.
 */
static void
gsip_rxintr(struct sip_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct sip_rxsoft *rxs;
	struct mbuf *m;
	u_int32_t cmdsts, extsts;
	int i, len;

	for (i = sc->sc_rxptr;; i = sip_nextrx(sc, i)) {
		rxs = &sc->sc_rxsoft[i];

		sip_cdrxsync(sc, i, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		cmdsts = le32toh(*sipd_cmdsts(sc, &sc->sc_rxdescs[i]));
		extsts = le32toh(sc->sc_rxdescs[i].sipd_extsts);
		len = CMDSTS_SIZE(sc, cmdsts);

		/*
		 * NOTE: OWN is set if owned by _consumer_.  We're the
		 * consumer of the receive ring, so if the bit is clear,
		 * we have processed all of the packets.
		 */
		if ((cmdsts & CMDSTS_OWN) == 0) {
			/*
			 * We have processed all of the receive buffers.
			 */
			break;
		}

		if (__predict_false(sc->sc_rxdiscard)) {
			sip_init_rxdesc(sc, i);
			if ((cmdsts & CMDSTS_MORE) == 0) {
				/* Reset our state. */
				sc->sc_rxdiscard = 0;
			}
			continue;
		}

		bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
		    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);

		m = rxs->rxs_mbuf;

		/*
		 * Add a new receive buffer to the ring.
		 */
		if (sipcom_add_rxbuf(sc, i) != 0) {
			/*
			 * Failed, throw away what we've done so
			 * far, and discard the rest of the packet.
			 */
			ifp->if_ierrors++;
			bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
			    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);
			sip_init_rxdesc(sc, i);
			if (cmdsts & CMDSTS_MORE)
				sc->sc_rxdiscard = 1;
			if (sc->sc_rxhead != NULL)
				m_freem(sc->sc_rxhead);
			sip_rxchain_reset(sc);
			continue;
		}

		sip_rxchain_link(sc, m);

		m->m_len = len;

		/*
		 * If this is not the end of the packet, keep
		 * looking.
		 */
		if (cmdsts & CMDSTS_MORE) {
			sc->sc_rxlen += len;
			continue;
		}

		/*
		 * Okay, we have the entire packet now.  The chip includes
		 * the FCS, so we need to trim it.
		 */
		m->m_len -= ETHER_CRC_LEN;

		*sc->sc_rxtailp = NULL;
		len = m->m_len + sc->sc_rxlen;
		m = sc->sc_rxhead;

		sip_rxchain_reset(sc);

		/*
		 * If an error occurred, update stats and drop the packet.
		 */
		if (cmdsts & (CMDSTS_Rx_RXA|CMDSTS_Rx_RUNT|
		    CMDSTS_Rx_ISE|CMDSTS_Rx_CRCE|CMDSTS_Rx_FAE)) {
			ifp->if_ierrors++;
			if ((cmdsts & CMDSTS_Rx_RXA) != 0 &&
			    (cmdsts & CMDSTS_Rx_RXO) == 0) {
				/* Receive overrun handled elsewhere. */
				printf("%s: receive descriptor error\n",
				    device_xname(sc->sc_dev));
			}
#define	PRINTERR(bit, str)						\
			if ((ifp->if_flags & IFF_DEBUG) != 0 &&		\
			    (cmdsts & (bit)) != 0)			\
				printf("%s: %s\n", device_xname(sc->sc_dev), str)
			PRINTERR(CMDSTS_Rx_RUNT, "runt packet");
			PRINTERR(CMDSTS_Rx_ISE, "invalid symbol error");
			PRINTERR(CMDSTS_Rx_CRCE, "CRC error");
			PRINTERR(CMDSTS_Rx_FAE, "frame alignment error");
#undef PRINTERR
			m_freem(m);
			continue;
		}

		/*
		 * If the packet is small enough to fit in a
		 * single header mbuf, allocate one and copy
		 * the data into it.  This greatly reduces
		 * memory consumption when we receive lots
		 * of small packets.
		 */
		if (gsip_copy_small != 0 && len <= (MHLEN - 2)) {
			struct mbuf *nm;
			MGETHDR(nm, M_DONTWAIT, MT_DATA);
			if (nm == NULL) {
				ifp->if_ierrors++;
				m_freem(m);
				continue;
			}
			MCLAIM(m, &sc->sc_ethercom.ec_rx_mowner);
			nm->m_data += 2;
			nm->m_pkthdr.len = nm->m_len = len;
			m_copydata(m, 0, len, mtod(nm, void *));
			m_freem(m);
			m = nm;
		}
#ifndef __NO_STRICT_ALIGNMENT
		else {
			/*
			 * The DP83820's receive buffers must be 4-byte
			 * aligned.  But this means that the data after
			 * the Ethernet header is misaligned.  To compensate,
			 * we have artificially shortened the buffer size
			 * in the descriptor, and we do an overlapping copy
			 * of the data two bytes further in (in the first
			 * buffer of the chain only).
			 */
			memmove(mtod(m, char *) + 2, mtod(m, void *),
			    m->m_len);
			m->m_data += 2;
		}
#endif /* ! __NO_STRICT_ALIGNMENT */

		/*
		 * If VLANs are enabled, VLAN packets have been unwrapped
		 * for us.  Associate the tag with the packet.
		 */

		/*
		 * Again, byte swapping is tricky. Hardware provided
		 * the tag in the network byte order, but extsts was
		 * passed through le32toh() in the meantime. On a
		 * big-endian machine, we need to swap it again. On a
		 * little-endian machine, we need to convert from the
		 * network to host byte order. This means that we must
		 * swap it in any case, so unconditional swap instead
		 * of htons() is used.
		 */
		if ((extsts & EXTSTS_VPKT) != 0) {
			VLAN_INPUT_TAG(ifp, m, bswap16(extsts & EXTSTS_VTCI),
			    continue);
		}

		/*
		 * Set the incoming checksum information for the
		 * packet.
		 */
		if ((extsts & EXTSTS_IPPKT) != 0) {
			SIP_EVCNT_INCR(&sc->sc_ev_rxipsum);
			m->m_pkthdr.csum_flags |= M_CSUM_IPv4;
			if (extsts & EXTSTS_Rx_IPERR)
				m->m_pkthdr.csum_flags |= M_CSUM_IPv4_BAD;
			if (extsts & EXTSTS_TCPPKT) {
				SIP_EVCNT_INCR(&sc->sc_ev_rxtcpsum);
				m->m_pkthdr.csum_flags |= M_CSUM_TCPv4;
				if (extsts & EXTSTS_Rx_TCPERR)
					m->m_pkthdr.csum_flags |=
					    M_CSUM_TCP_UDP_BAD;
			} else if (extsts & EXTSTS_UDPPKT) {
				SIP_EVCNT_INCR(&sc->sc_ev_rxudpsum);
				m->m_pkthdr.csum_flags |= M_CSUM_UDPv4;
				if (extsts & EXTSTS_Rx_UDPERR)
					m->m_pkthdr.csum_flags |=
					    M_CSUM_TCP_UDP_BAD;
			}
		}

		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = len;

		/*
		 * Pass this up to any BPF listeners, but only
		 * pass if up the stack if it's for us.
		 */
		bpf_mtap(ifp, m);

		/* Pass it on. */
		(*ifp->if_input)(ifp, m);
	}

	/* Update the receive pointer. */
	sc->sc_rxptr = i;
}

/*
 * sip_rxintr:
 *
 *	Helper; handle receive interrupts on 10/100 parts.
 */
static void
sip_rxintr(struct sip_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct sip_rxsoft *rxs;
	struct mbuf *m;
	u_int32_t cmdsts;
	int i, len;

	for (i = sc->sc_rxptr;; i = sip_nextrx(sc, i)) {
		rxs = &sc->sc_rxsoft[i];

		sip_cdrxsync(sc, i, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		cmdsts = le32toh(*sipd_cmdsts(sc, &sc->sc_rxdescs[i]));

		/*
		 * NOTE: OWN is set if owned by _consumer_.  We're the
		 * consumer of the receive ring, so if the bit is clear,
		 * we have processed all of the packets.
		 */
		if ((cmdsts & CMDSTS_OWN) == 0) {
			/*
			 * We have processed all of the receive buffers.
			 */
			break;
		}

		/*
		 * If any collisions were seen on the wire, count one.
		 */
		if (cmdsts & CMDSTS_Rx_COL)
			ifp->if_collisions++;

		/*
		 * If an error occurred, update stats, clear the status
		 * word, and leave the packet buffer in place.  It will
		 * simply be reused the next time the ring comes around.
		 */
		if (cmdsts & (CMDSTS_Rx_RXA|CMDSTS_Rx_RUNT|
		    CMDSTS_Rx_ISE|CMDSTS_Rx_CRCE|CMDSTS_Rx_FAE)) {
			ifp->if_ierrors++;
			if ((cmdsts & CMDSTS_Rx_RXA) != 0 &&
			    (cmdsts & CMDSTS_Rx_RXO) == 0) {
				/* Receive overrun handled elsewhere. */
				printf("%s: receive descriptor error\n",
				    device_xname(sc->sc_dev));
			}
#define	PRINTERR(bit, str)						\
			if ((ifp->if_flags & IFF_DEBUG) != 0 &&		\
			    (cmdsts & (bit)) != 0)			\
				printf("%s: %s\n", device_xname(sc->sc_dev), str)
			PRINTERR(CMDSTS_Rx_RUNT, "runt packet");
			PRINTERR(CMDSTS_Rx_ISE, "invalid symbol error");
			PRINTERR(CMDSTS_Rx_CRCE, "CRC error");
			PRINTERR(CMDSTS_Rx_FAE, "frame alignment error");
#undef PRINTERR
			sip_init_rxdesc(sc, i);
			continue;
		}

		bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
		    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);

		/*
		 * No errors; receive the packet.  Note, the SiS 900
		 * includes the CRC with every packet.
		 */
		len = CMDSTS_SIZE(sc, cmdsts) - ETHER_CRC_LEN;

#ifdef __NO_STRICT_ALIGNMENT
		/*
		 * If the packet is small enough to fit in a
		 * single header mbuf, allocate one and copy
		 * the data into it.  This greatly reduces
		 * memory consumption when we receive lots
		 * of small packets.
		 *
		 * Otherwise, we add a new buffer to the receive
		 * chain.  If this fails, we drop the packet and
		 * recycle the old buffer.
		 */
		if (sip_copy_small != 0 && len <= MHLEN) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL)
				goto dropit;
			MCLAIM(m, &sc->sc_ethercom.ec_rx_mowner);
			memcpy(mtod(m, void *),
			    mtod(rxs->rxs_mbuf, void *), len);
			sip_init_rxdesc(sc, i);
			bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
			    rxs->rxs_dmamap->dm_mapsize,
			    BUS_DMASYNC_PREREAD);
		} else {
			m = rxs->rxs_mbuf;
			if (sipcom_add_rxbuf(sc, i) != 0) {
 dropit:
				ifp->if_ierrors++;
				sip_init_rxdesc(sc, i);
				bus_dmamap_sync(sc->sc_dmat,
				    rxs->rxs_dmamap, 0,
				    rxs->rxs_dmamap->dm_mapsize,
				    BUS_DMASYNC_PREREAD);
				continue;
			}
		}
#else
		/*
		 * The SiS 900's receive buffers must be 4-byte aligned.
		 * But this means that the data after the Ethernet header
		 * is misaligned.  We must allocate a new buffer and
		 * copy the data, shifted forward 2 bytes.
		 */
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
 dropit:
			ifp->if_ierrors++;
			sip_init_rxdesc(sc, i);
			bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
			    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);
			continue;
		}
		MCLAIM(m, &sc->sc_ethercom.ec_rx_mowner);
		if (len > (MHLEN - 2)) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				m_freem(m);
				goto dropit;
			}
		}
		m->m_data += 2;

		/*
		 * Note that we use clusters for incoming frames, so the
		 * buffer is virtually contiguous.
		 */
		memcpy(mtod(m, void *), mtod(rxs->rxs_mbuf, void *), len);

		/* Allow the receive descriptor to continue using its mbuf. */
		sip_init_rxdesc(sc, i);
		bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
		    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);
#endif /* __NO_STRICT_ALIGNMENT */

		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = len;

		/*
		 * Pass this up to any BPF listeners, but only
		 * pass if up the stack if it's for us.
		 */
		bpf_mtap(ifp, m);

		/* Pass it on. */
		(*ifp->if_input)(ifp, m);
	}

	/* Update the receive pointer. */
	sc->sc_rxptr = i;
}

/*
 * sip_tick:
 *
 *	One second timer, used to tick the MII.
 */
static void
sipcom_tick(void *arg)
{
	struct sip_softc *sc = arg;
	int s;

	s = splnet();
#ifdef SIP_EVENT_COUNTERS
	if (sc->sc_gigabit) {
		/* Read PAUSE related counts from MIB registers. */
		sc->sc_ev_rxpause.ev_count +=
		    bus_space_read_4(sc->sc_st, sc->sc_sh,
				     SIP_NS_MIB(MIB_RXPauseFrames)) & 0xffff;
		sc->sc_ev_txpause.ev_count +=
		    bus_space_read_4(sc->sc_st, sc->sc_sh,
				     SIP_NS_MIB(MIB_TXPauseFrames)) & 0xffff;
		bus_space_write_4(sc->sc_st, sc->sc_sh, SIP_NS_MIBC, MIBC_ACLR);
	}
#endif /* SIP_EVENT_COUNTERS */
	mii_tick(&sc->sc_mii);
	splx(s);

	callout_reset(&sc->sc_tick_ch, hz, sipcom_tick, sc);
}

/*
 * sip_reset:
 *
 *	Perform a soft reset on the SiS 900.
 */
static bool
sipcom_reset(struct sip_softc *sc)
{
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	int i;

	bus_space_write_4(st, sh, SIP_IER, 0);
	bus_space_write_4(st, sh, SIP_IMR, 0);
	bus_space_write_4(st, sh, SIP_RFCR, 0);
	bus_space_write_4(st, sh, SIP_CR, CR_RST);

	for (i = 0; i < SIP_TIMEOUT; i++) {
		if ((bus_space_read_4(st, sh, SIP_CR) & CR_RST) == 0)
			break;
		delay(2);
	}

	if (i == SIP_TIMEOUT) {
		printf("%s: reset failed to complete\n", device_xname(sc->sc_dev));
		return false;
	}

	delay(1000);

	if (sc->sc_gigabit) {
		/*
		 * Set the general purpose I/O bits.  Do it here in case we
		 * need to have GPIO set up to talk to the media interface.
		 */
		bus_space_write_4(st, sh, SIP_GPIOR, sc->sc_gpior);
		delay(1000);
	}
	return true;
}

static void
sipcom_dp83820_init(struct sip_softc *sc, uint64_t capenable)
{
	u_int32_t reg;
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	/*
	 * Initialize the VLAN/IP receive control register.
	 * We enable checksum computation on all incoming
	 * packets, and do not reject packets w/ bad checksums.
	 */
	reg = 0;
	if (capenable &
	    (IFCAP_CSUM_IPv4_Rx|IFCAP_CSUM_TCPv4_Rx|IFCAP_CSUM_UDPv4_Rx))
		reg |= VRCR_IPEN;
	if (VLAN_ATTACHED(&sc->sc_ethercom))
		reg |= VRCR_VTDEN|VRCR_VTREN;
	bus_space_write_4(st, sh, SIP_VRCR, reg);

	/*
	 * Initialize the VLAN/IP transmit control register.
	 * We enable outgoing checksum computation on a
	 * per-packet basis.
	 */
	reg = 0;
	if (capenable &
	    (IFCAP_CSUM_IPv4_Tx|IFCAP_CSUM_TCPv4_Tx|IFCAP_CSUM_UDPv4_Tx))
		reg |= VTCR_PPCHK;
	if (VLAN_ATTACHED(&sc->sc_ethercom))
		reg |= VTCR_VPPTI;
	bus_space_write_4(st, sh, SIP_VTCR, reg);

	/*
	 * If we're using VLANs, initialize the VLAN data register.
	 * To understand why we bswap the VLAN Ethertype, see section
	 * 4.2.36 of the DP83820 manual.
	 */
	if (VLAN_ATTACHED(&sc->sc_ethercom))
		bus_space_write_4(st, sh, SIP_VDR, bswap16(ETHERTYPE_VLAN));
}

/*
 * sip_init:		[ ifnet interface function ]
 *
 *	Initialize the interface.  Must be called at splnet().
 */
static int
sipcom_init(struct ifnet *ifp)
{
	struct sip_softc *sc = ifp->if_softc;
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	struct sip_txsoft *txs;
	struct sip_rxsoft *rxs;
	struct sip_desc *sipd;
	int i, error = 0;

	if (device_is_active(sc->sc_dev)) {
		/*
		 * Cancel any pending I/O.
		 */
		sipcom_stop(ifp, 0);
	} else if (!pmf_device_subtree_resume(sc->sc_dev, &sc->sc_qual) ||
	           !device_is_active(sc->sc_dev))
		return 0;

	/*
	 * Reset the chip to a known state.
	 */
	if (!sipcom_reset(sc))
		return EBUSY;

	if (SIP_CHIP_MODEL(sc, PCI_VENDOR_NS, PCI_PRODUCT_NS_DP83815)) {
		/*
		 * DP83815 manual, page 78:
		 *    4.4 Recommended Registers Configuration
		 *    For optimum performance of the DP83815, version noted
		 *    as DP83815CVNG (SRR = 203h), the listed register
		 *    modifications must be followed in sequence...
		 *
		 * It's not clear if this should be 302h or 203h because that
		 * chip name is listed as SRR 302h in the description of the
		 * SRR register.  However, my revision 302h DP83815 on the
		 * Netgear FA311 purchased in 02/2001 needs these settings
		 * to avoid tons of errors in AcceptPerfectMatch (non-
		 * IFF_PROMISC) mode.  I do not know if other revisions need
		 * this set or not.  [briggs -- 09 March 2001]
		 *
		 * Note that only the low-order 12 bits of 0xe4 are documented
		 * and that this sets reserved bits in that register.
		 */
		bus_space_write_4(st, sh, 0x00cc, 0x0001);

		bus_space_write_4(st, sh, 0x00e4, 0x189C);
		bus_space_write_4(st, sh, 0x00fc, 0x0000);
		bus_space_write_4(st, sh, 0x00f4, 0x5040);
		bus_space_write_4(st, sh, 0x00f8, 0x008c);

		bus_space_write_4(st, sh, 0x00cc, 0x0000);
	}

	/*
	 * Initialize the transmit descriptor ring.
	 */
	for (i = 0; i < sc->sc_ntxdesc; i++) {
		sipd = &sc->sc_txdescs[i];
		memset(sipd, 0, sizeof(struct sip_desc));
		sipd->sipd_link = htole32(SIP_CDTXADDR(sc, sip_nexttx(sc, i)));
	}
	sip_cdtxsync(sc, 0, sc->sc_ntxdesc,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	sc->sc_txfree = sc->sc_ntxdesc;
	sc->sc_txnext = 0;
	sc->sc_txwin = 0;

	/*
	 * Initialize the transmit job descriptors.
	 */
	SIMPLEQ_INIT(&sc->sc_txfreeq);
	SIMPLEQ_INIT(&sc->sc_txdirtyq);
	for (i = 0; i < SIP_TXQUEUELEN; i++) {
		txs = &sc->sc_txsoft[i];
		txs->txs_mbuf = NULL;
		SIMPLEQ_INSERT_TAIL(&sc->sc_txfreeq, txs, txs_q);
	}

	/*
	 * Initialize the receive descriptor and receive job
	 * descriptor rings.
	 */
	for (i = 0; i < sc->sc_parm->p_nrxdesc; i++) {
		rxs = &sc->sc_rxsoft[i];
		if (rxs->rxs_mbuf == NULL) {
			if ((error = sipcom_add_rxbuf(sc, i)) != 0) {
				printf("%s: unable to allocate or map rx "
				    "buffer %d, error = %d\n",
				    device_xname(sc->sc_dev), i, error);
				/*
				 * XXX Should attempt to run with fewer receive
				 * XXX buffers instead of just failing.
				 */
				sipcom_rxdrain(sc);
				goto out;
			}
		} else
			sip_init_rxdesc(sc, i);
	}
	sc->sc_rxptr = 0;
	sc->sc_rxdiscard = 0;
	sip_rxchain_reset(sc);

	/*
	 * Set the configuration register; it's already initialized
	 * in sip_attach().
	 */
	bus_space_write_4(st, sh, SIP_CFG, sc->sc_cfg);

	/*
	 * Initialize the prototype TXCFG register.
	 */
	if (sc->sc_gigabit) {
		sc->sc_txcfg = sc->sc_bits.b_txcfg_mxdma_512;
		sc->sc_rxcfg = sc->sc_bits.b_rxcfg_mxdma_512;
	} else if ((SIP_SIS900_REV(sc, SIS_REV_635) ||
	     SIP_SIS900_REV(sc, SIS_REV_960) ||
	     SIP_SIS900_REV(sc, SIS_REV_900B)) &&
	    (sc->sc_cfg & CFG_EDBMASTEN)) {
		sc->sc_txcfg = sc->sc_bits.b_txcfg_mxdma_64;
		sc->sc_rxcfg = sc->sc_bits.b_rxcfg_mxdma_64;
	} else {
		sc->sc_txcfg = sc->sc_bits.b_txcfg_mxdma_512;
		sc->sc_rxcfg = sc->sc_bits.b_rxcfg_mxdma_512;
	}

	sc->sc_txcfg |= TXCFG_ATP |
	    __SHIFTIN(sc->sc_tx_fill_thresh, sc->sc_bits.b_txcfg_flth_mask) |
	    sc->sc_tx_drain_thresh;
	bus_space_write_4(st, sh, sc->sc_regs.r_txcfg, sc->sc_txcfg);

	/*
	 * Initialize the receive drain threshold if we have never
	 * done so.
	 */
	if (sc->sc_rx_drain_thresh == 0) {
		/*
		 * XXX This value should be tuned.  This is set to the
		 * maximum of 248 bytes, and we may be able to improve
		 * performance by decreasing it (although we should never
		 * set this value lower than 2; 14 bytes are required to
		 * filter the packet).
		 */
		sc->sc_rx_drain_thresh = __SHIFTOUT_MASK(RXCFG_DRTH_MASK);
	}

	/*
	 * Initialize the prototype RXCFG register.
	 */
	sc->sc_rxcfg |= __SHIFTIN(sc->sc_rx_drain_thresh, RXCFG_DRTH_MASK);
	/*
	 * Accept long packets (including FCS) so we can handle
	 * 802.1q-tagged frames and jumbo frames properly.
	 */
	if ((sc->sc_gigabit && ifp->if_mtu > ETHERMTU) ||
	    (sc->sc_ethercom.ec_capenable & ETHERCAP_VLAN_MTU))
		sc->sc_rxcfg |= RXCFG_ALP;

	/*
	 * Checksum offloading is disabled if the user selects an MTU
	 * larger than 8109.  (FreeBSD says 8152, but there is emperical
	 * evidence that >8109 does not work on some boards, such as the
	 * Planex GN-1000TE).
	 */
	if (sc->sc_gigabit && ifp->if_mtu > 8109 &&
	    (ifp->if_capenable &
	     (IFCAP_CSUM_IPv4_Tx|IFCAP_CSUM_IPv4_Rx|
	      IFCAP_CSUM_TCPv4_Tx|IFCAP_CSUM_TCPv4_Rx|
	      IFCAP_CSUM_UDPv4_Tx|IFCAP_CSUM_UDPv4_Rx))) {
		printf("%s: Checksum offloading does not work if MTU > 8109 - "
		       "disabled.\n", device_xname(sc->sc_dev));
		ifp->if_capenable &=
		    ~(IFCAP_CSUM_IPv4_Tx|IFCAP_CSUM_IPv4_Rx|
		     IFCAP_CSUM_TCPv4_Tx|IFCAP_CSUM_TCPv4_Rx|
		     IFCAP_CSUM_UDPv4_Tx|IFCAP_CSUM_UDPv4_Rx);
		ifp->if_csum_flags_tx = 0;
		ifp->if_csum_flags_rx = 0;
	}

	bus_space_write_4(st, sh, sc->sc_regs.r_rxcfg, sc->sc_rxcfg);

	if (sc->sc_gigabit)
		sipcom_dp83820_init(sc, ifp->if_capenable);

	/*
	 * Give the transmit and receive rings to the chip.
	 */
	bus_space_write_4(st, sh, SIP_TXDP, SIP_CDTXADDR(sc, sc->sc_txnext));
	bus_space_write_4(st, sh, SIP_RXDP, SIP_CDRXADDR(sc, sc->sc_rxptr));

	/*
	 * Initialize the interrupt mask.
	 */
	sc->sc_imr = sc->sc_bits.b_isr_dperr |
	             sc->sc_bits.b_isr_sserr |
		     sc->sc_bits.b_isr_rmabt |
		     sc->sc_bits.b_isr_rtabt | ISR_RXSOVR |
	    ISR_TXURN|ISR_TXDESC|ISR_TXIDLE|ISR_RXORN|ISR_RXIDLE|ISR_RXDESC;
	bus_space_write_4(st, sh, SIP_IMR, sc->sc_imr);

	/* Set up the receive filter. */
	(*sc->sc_model->sip_variant->sipv_set_filter)(sc);

	/*
	 * Tune sc_rx_flow_thresh.
	 * XXX "More than 8KB" is too short for jumbo frames.
	 * XXX TODO: Threshold value should be user-settable.
	 */
	sc->sc_rx_flow_thresh = (PCR_PS_STHI_8 | PCR_PS_STLO_4 |
				 PCR_PS_FFHI_8 | PCR_PS_FFLO_4 |
				 (PCR_PAUSE_CNT & PCR_PAUSE_CNT_MASK));

	/*
	 * Set the current media.  Do this after initializing the prototype
	 * IMR, since sip_mii_statchg() modifies the IMR for 802.3x flow
	 * control.
	 */
	if ((error = ether_mediachange(ifp)) != 0)
		goto out;

	/*
	 * Set the interrupt hold-off timer to 100us.
	 */
	if (sc->sc_gigabit)
		bus_space_write_4(st, sh, SIP_IHR, 0x01);

	/*
	 * Enable interrupts.
	 */
	bus_space_write_4(st, sh, SIP_IER, IER_IE);

	/*
	 * Start the transmit and receive processes.
	 */
	bus_space_write_4(st, sh, SIP_CR, CR_RXE | CR_TXE);

	/*
	 * Start the one second MII clock.
	 */
	callout_reset(&sc->sc_tick_ch, hz, sipcom_tick, sc);

	/*
	 * ...all done!
	 */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	sc->sc_if_flags = ifp->if_flags;
	sc->sc_prev.ec_capenable = sc->sc_ethercom.ec_capenable;
	sc->sc_prev.is_vlan = VLAN_ATTACHED(&(sc)->sc_ethercom);
	sc->sc_prev.if_capenable = ifp->if_capenable;

 out:
	if (error)
		printf("%s: interface not running\n", device_xname(sc->sc_dev));
	return (error);
}

/*
 * sip_drain:
 *
 *	Drain the receive queue.
 */
static void
sipcom_rxdrain(struct sip_softc *sc)
{
	struct sip_rxsoft *rxs;
	int i;

	for (i = 0; i < sc->sc_parm->p_nrxdesc; i++) {
		rxs = &sc->sc_rxsoft[i];
		if (rxs->rxs_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, rxs->rxs_dmamap);
			m_freem(rxs->rxs_mbuf);
			rxs->rxs_mbuf = NULL;
		}
	}
}

/*
 * sip_stop:		[ ifnet interface function ]
 *
 *	Stop transmission on the interface.
 */
static void
sipcom_stop(struct ifnet *ifp, int disable)
{
	struct sip_softc *sc = ifp->if_softc;
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	struct sip_txsoft *txs;
	u_int32_t cmdsts = 0;		/* DEBUG */

	/*
	 * Stop the one second clock.
	 */
	callout_stop(&sc->sc_tick_ch);

	/* Down the MII. */
	mii_down(&sc->sc_mii);

	if (device_is_active(sc->sc_dev)) {
		/*
		 * Disable interrupts.
		 */
		bus_space_write_4(st, sh, SIP_IER, 0);

		/*
		 * Stop receiver and transmitter.
		 */
		bus_space_write_4(st, sh, SIP_CR, CR_RXD | CR_TXD);
	}

	/*
	 * Release any queued transmit buffers.
	 */
	while ((txs = SIMPLEQ_FIRST(&sc->sc_txdirtyq)) != NULL) {
		if ((ifp->if_flags & IFF_DEBUG) != 0 &&
		    SIMPLEQ_NEXT(txs, txs_q) == NULL &&
		    (le32toh(*sipd_cmdsts(sc, &sc->sc_txdescs[txs->txs_lastdesc])) &
		     CMDSTS_INTR) == 0)
			printf("%s: sip_stop: last descriptor does not "
			    "have INTR bit set\n", device_xname(sc->sc_dev));
		SIMPLEQ_REMOVE_HEAD(&sc->sc_txdirtyq, txs_q);
#ifdef DIAGNOSTIC
		if (txs->txs_mbuf == NULL) {
			printf("%s: dirty txsoft with no mbuf chain\n",
			    device_xname(sc->sc_dev));
			panic("sip_stop");
		}
#endif
		cmdsts |=		/* DEBUG */
		    le32toh(*sipd_cmdsts(sc, &sc->sc_txdescs[txs->txs_lastdesc]));
		bus_dmamap_unload(sc->sc_dmat, txs->txs_dmamap);
		m_freem(txs->txs_mbuf);
		txs->txs_mbuf = NULL;
		SIMPLEQ_INSERT_TAIL(&sc->sc_txfreeq, txs, txs_q);
	}

	/*
	 * Mark the interface down and cancel the watchdog timer.
	 */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	if (disable)
		pmf_device_recursive_suspend(sc->sc_dev, &sc->sc_qual);

	if ((ifp->if_flags & IFF_DEBUG) != 0 &&
	    (cmdsts & CMDSTS_INTR) == 0 && sc->sc_txfree != sc->sc_ntxdesc)
		printf("%s: sip_stop: no INTR bits set in dirty tx "
		    "descriptors\n", device_xname(sc->sc_dev));
}

/*
 * sip_read_eeprom:
 *
 *	Read data from the serial EEPROM.
 */
static void
sipcom_read_eeprom(struct sip_softc *sc, int word, int wordcnt,
    u_int16_t *data)
{
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	u_int16_t reg;
	int i, x;

	for (i = 0; i < wordcnt; i++) {
		/* Send CHIP SELECT. */
		reg = EROMAR_EECS;
		bus_space_write_4(st, sh, SIP_EROMAR, reg);

		/* Shift in the READ opcode. */
		for (x = 3; x > 0; x--) {
			if (SIP_EEPROM_OPC_READ & (1 << (x - 1)))
				reg |= EROMAR_EEDI;
			else
				reg &= ~EROMAR_EEDI;
			bus_space_write_4(st, sh, SIP_EROMAR, reg);
			bus_space_write_4(st, sh, SIP_EROMAR,
			    reg | EROMAR_EESK);
			delay(4);
			bus_space_write_4(st, sh, SIP_EROMAR, reg);
			delay(4);
		}

		/* Shift in address. */
		for (x = 6; x > 0; x--) {
			if ((word + i) & (1 << (x - 1)))
				reg |= EROMAR_EEDI;
			else
				reg &= ~EROMAR_EEDI;
			bus_space_write_4(st, sh, SIP_EROMAR, reg);
			bus_space_write_4(st, sh, SIP_EROMAR,
			    reg | EROMAR_EESK);
			delay(4);
			bus_space_write_4(st, sh, SIP_EROMAR, reg);
			delay(4);
		}

		/* Shift out data. */
		reg = EROMAR_EECS;
		data[i] = 0;
		for (x = 16; x > 0; x--) {
			bus_space_write_4(st, sh, SIP_EROMAR,
			    reg | EROMAR_EESK);
			delay(4);
			if (bus_space_read_4(st, sh, SIP_EROMAR) & EROMAR_EEDO)
				data[i] |= (1 << (x - 1));
			bus_space_write_4(st, sh, SIP_EROMAR, reg);
			delay(4);
		}

		/* Clear CHIP SELECT. */
		bus_space_write_4(st, sh, SIP_EROMAR, 0);
		delay(4);
	}
}

/*
 * sipcom_add_rxbuf:
 *
 *	Add a receive buffer to the indicated descriptor.
 */
static int
sipcom_add_rxbuf(struct sip_softc *sc, int idx)
{
	struct sip_rxsoft *rxs = &sc->sc_rxsoft[idx];
	struct mbuf *m;
	int error;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);
	MCLAIM(m, &sc->sc_ethercom.ec_rx_mowner);

	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return (ENOBUFS);
	}

	/* XXX I don't believe this is necessary. --dyoung */
	if (sc->sc_gigabit)
		m->m_len = sc->sc_parm->p_rxbuf_len;

	if (rxs->rxs_mbuf != NULL)
		bus_dmamap_unload(sc->sc_dmat, rxs->rxs_dmamap);

	rxs->rxs_mbuf = m;

	error = bus_dmamap_load(sc->sc_dmat, rxs->rxs_dmamap,
	    m->m_ext.ext_buf, m->m_ext.ext_size, NULL,
	    BUS_DMA_READ|BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: can't load rx DMA map %d, error = %d\n",
		    device_xname(sc->sc_dev), idx, error);
		panic("%s", __func__);		/* XXX */
	}

	bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
	    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);

	sip_init_rxdesc(sc, idx);

	return (0);
}

/*
 * sip_sis900_set_filter:
 *
 *	Set up the receive filter.
 */
static void
sipcom_sis900_set_filter(struct sip_softc *sc)
{
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	struct ethercom *ec = &sc->sc_ethercom;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct ether_multi *enm;
	const u_int8_t *cp;
	struct ether_multistep step;
	u_int32_t crc, mchash[16];

	/*
	 * Initialize the prototype RFCR.
	 */
	sc->sc_rfcr = RFCR_RFEN;
	if (ifp->if_flags & IFF_BROADCAST)
		sc->sc_rfcr |= RFCR_AAB;
	if (ifp->if_flags & IFF_PROMISC) {
		sc->sc_rfcr |= RFCR_AAP;
		goto allmulti;
	}

	/*
	 * Set up the multicast address filter by passing all multicast
	 * addresses through a CRC generator, and then using the high-order
	 * 6 bits as an index into the 128 bit multicast hash table (only
	 * the lower 16 bits of each 32 bit multicast hash register are
	 * valid).  The high order bits select the register, while the
	 * rest of the bits select the bit within the register.
	 */

	memset(mchash, 0, sizeof(mchash));

	/*
	 * SiS900 (at least SiS963) requires us to register the address of
	 * the PAUSE packet (01:80:c2:00:00:01) into the address filter.
	 */
	crc = 0x0ed423f9;

	if (SIP_SIS900_REV(sc, SIS_REV_635) ||
	    SIP_SIS900_REV(sc, SIS_REV_960) ||
	    SIP_SIS900_REV(sc, SIS_REV_900B)) {
		/* Just want the 8 most significant bits. */
		crc >>= 24;
	} else {
		/* Just want the 7 most significant bits. */
		crc >>= 25;
	}

	/* Set the corresponding bit in the hash table. */
	mchash[crc >> 4] |= 1 << (crc & 0xf);

	ETHER_FIRST_MULTI(step, ec, enm);
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
			/*
			 * We must listen to a range of multicast addresses.
			 * For now, just accept all multicasts, rather than
			 * trying to set only those filter bits needed to match
			 * the range.  (At this time, the only use of address
			 * ranges is for IP multicast routing, for which the
			 * range is big enough to require all bits set.)
			 */
			goto allmulti;
		}

		crc = ether_crc32_be(enm->enm_addrlo, ETHER_ADDR_LEN);

		if (SIP_SIS900_REV(sc, SIS_REV_635) ||
		    SIP_SIS900_REV(sc, SIS_REV_960) ||
		    SIP_SIS900_REV(sc, SIS_REV_900B)) {
			/* Just want the 8 most significant bits. */
			crc >>= 24;
		} else {
			/* Just want the 7 most significant bits. */
			crc >>= 25;
		}

		/* Set the corresponding bit in the hash table. */
		mchash[crc >> 4] |= 1 << (crc & 0xf);

		ETHER_NEXT_MULTI(step, enm);
	}

	ifp->if_flags &= ~IFF_ALLMULTI;
	goto setit;

 allmulti:
	ifp->if_flags |= IFF_ALLMULTI;
	sc->sc_rfcr |= RFCR_AAM;

 setit:
#define	FILTER_EMIT(addr, data)						\
	bus_space_write_4(st, sh, SIP_RFCR, (addr));			\
	delay(1);							\
	bus_space_write_4(st, sh, SIP_RFDR, (data));			\
	delay(1)

	/*
	 * Disable receive filter, and program the node address.
	 */
	cp = CLLADDR(ifp->if_sadl);
	FILTER_EMIT(RFCR_RFADDR_NODE0, (cp[1] << 8) | cp[0]);
	FILTER_EMIT(RFCR_RFADDR_NODE2, (cp[3] << 8) | cp[2]);
	FILTER_EMIT(RFCR_RFADDR_NODE4, (cp[5] << 8) | cp[4]);

	if ((ifp->if_flags & IFF_ALLMULTI) == 0) {
		/*
		 * Program the multicast hash table.
		 */
		FILTER_EMIT(RFCR_RFADDR_MC0, mchash[0]);
		FILTER_EMIT(RFCR_RFADDR_MC1, mchash[1]);
		FILTER_EMIT(RFCR_RFADDR_MC2, mchash[2]);
		FILTER_EMIT(RFCR_RFADDR_MC3, mchash[3]);
		FILTER_EMIT(RFCR_RFADDR_MC4, mchash[4]);
		FILTER_EMIT(RFCR_RFADDR_MC5, mchash[5]);
		FILTER_EMIT(RFCR_RFADDR_MC6, mchash[6]);
		FILTER_EMIT(RFCR_RFADDR_MC7, mchash[7]);
		if (SIP_SIS900_REV(sc, SIS_REV_635) ||
		    SIP_SIS900_REV(sc, SIS_REV_960) ||
		    SIP_SIS900_REV(sc, SIS_REV_900B)) {
			FILTER_EMIT(RFCR_RFADDR_MC8, mchash[8]);
			FILTER_EMIT(RFCR_RFADDR_MC9, mchash[9]);
			FILTER_EMIT(RFCR_RFADDR_MC10, mchash[10]);
			FILTER_EMIT(RFCR_RFADDR_MC11, mchash[11]);
			FILTER_EMIT(RFCR_RFADDR_MC12, mchash[12]);
			FILTER_EMIT(RFCR_RFADDR_MC13, mchash[13]);
			FILTER_EMIT(RFCR_RFADDR_MC14, mchash[14]);
			FILTER_EMIT(RFCR_RFADDR_MC15, mchash[15]);
		}
	}
#undef FILTER_EMIT

	/*
	 * Re-enable the receiver filter.
	 */
	bus_space_write_4(st, sh, SIP_RFCR, sc->sc_rfcr);
}

/*
 * sip_dp83815_set_filter:
 *
 *	Set up the receive filter.
 */
static void
sipcom_dp83815_set_filter(struct sip_softc *sc)
{
	bus_space_tag_t st = sc->sc_st;
	bus_space_handle_t sh = sc->sc_sh;
	struct ethercom *ec = &sc->sc_ethercom;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct ether_multi *enm;
	const u_int8_t *cp;
	struct ether_multistep step;
	u_int32_t crc, hash, slot, bit;
#define	MCHASH_NWORDS_83820	128
#define	MCHASH_NWORDS_83815	32
#define	MCHASH_NWORDS	MAX(MCHASH_NWORDS_83820, MCHASH_NWORDS_83815)
	u_int16_t mchash[MCHASH_NWORDS];
	int i;

	/*
	 * Initialize the prototype RFCR.
	 * Enable the receive filter, and accept on
	 *    Perfect (destination address) Match
	 * If IFF_BROADCAST, also accept all broadcast packets.
	 * If IFF_PROMISC, accept all unicast packets (and later, set
	 *    IFF_ALLMULTI and accept all multicast, too).
	 */
	sc->sc_rfcr = RFCR_RFEN | RFCR_APM;
	if (ifp->if_flags & IFF_BROADCAST)
		sc->sc_rfcr |= RFCR_AAB;
	if (ifp->if_flags & IFF_PROMISC) {
		sc->sc_rfcr |= RFCR_AAP;
		goto allmulti;
	}

	/*
         * Set up the DP83820/DP83815 multicast address filter by
         * passing all multicast addresses through a CRC generator,
         * and then using the high-order 11/9 bits as an index into
         * the 2048/512 bit multicast hash table.  The high-order
         * 7/5 bits select the slot, while the low-order 4 bits
         * select the bit within the slot.  Note that only the low
         * 16-bits of each filter word are used, and there are
         * 128/32 filter words.
	 */

	memset(mchash, 0, sizeof(mchash));

	ifp->if_flags &= ~IFF_ALLMULTI;
	ETHER_FIRST_MULTI(step, ec, enm);
	if (enm == NULL)
		goto setit;
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
			/*
			 * We must listen to a range of multicast addresses.
			 * For now, just accept all multicasts, rather than
			 * trying to set only those filter bits needed to match
			 * the range.  (At this time, the only use of address
			 * ranges is for IP multicast routing, for which the
			 * range is big enough to require all bits set.)
			 */
			goto allmulti;
		}

		crc = ether_crc32_be(enm->enm_addrlo, ETHER_ADDR_LEN);

		if (sc->sc_gigabit) {
			/* Just want the 11 most significant bits. */
			hash = crc >> 21;
		} else {
			/* Just want the 9 most significant bits. */
			hash = crc >> 23;
		}

		slot = hash >> 4;
		bit = hash & 0xf;

		/* Set the corresponding bit in the hash table. */
		mchash[slot] |= 1 << bit;

		ETHER_NEXT_MULTI(step, enm);
	}
	sc->sc_rfcr |= RFCR_MHEN;
	goto setit;

 allmulti:
	ifp->if_flags |= IFF_ALLMULTI;
	sc->sc_rfcr |= RFCR_AAM;

 setit:
#define	FILTER_EMIT(addr, data)						\
	bus_space_write_4(st, sh, SIP_RFCR, (addr));			\
	delay(1);							\
	bus_space_write_4(st, sh, SIP_RFDR, (data));			\
	delay(1)

	/*
	 * Disable receive filter, and program the node address.
	 */
	cp = CLLADDR(ifp->if_sadl);
	FILTER_EMIT(RFCR_NS_RFADDR_PMATCH0, (cp[1] << 8) | cp[0]);
	FILTER_EMIT(RFCR_NS_RFADDR_PMATCH2, (cp[3] << 8) | cp[2]);
	FILTER_EMIT(RFCR_NS_RFADDR_PMATCH4, (cp[5] << 8) | cp[4]);

	if ((ifp->if_flags & IFF_ALLMULTI) == 0) {
		int nwords =
		    sc->sc_gigabit ? MCHASH_NWORDS_83820 : MCHASH_NWORDS_83815;
		/*
		 * Program the multicast hash table.
		 */
		for (i = 0; i < nwords; i++) {
			FILTER_EMIT(sc->sc_parm->p_filtmem + (i * 2), mchash[i]);
		}
	}
#undef FILTER_EMIT
#undef MCHASH_NWORDS
#undef MCHASH_NWORDS_83815
#undef MCHASH_NWORDS_83820

	/*
	 * Re-enable the receiver filter.
	 */
	bus_space_write_4(st, sh, SIP_RFCR, sc->sc_rfcr);
}

/*
 * sip_dp83820_mii_readreg:	[mii interface function]
 *
 *	Read a PHY register on the MII of the DP83820.
 */
static int
sipcom_dp83820_mii_readreg(device_t self, int phy, int reg)
{
	struct sip_softc *sc = device_private(self);

	if (sc->sc_cfg & CFG_TBI_EN) {
		bus_addr_t tbireg;
		int rv;

		if (phy != 0)
			return (0);

		switch (reg) {
		case MII_BMCR:		tbireg = SIP_TBICR; break;
		case MII_BMSR:		tbireg = SIP_TBISR; break;
		case MII_ANAR:		tbireg = SIP_TANAR; break;
		case MII_ANLPAR:	tbireg = SIP_TANLPAR; break;
		case MII_ANER:		tbireg = SIP_TANER; break;
		case MII_EXTSR:
			/*
			 * Don't even bother reading the TESR register.
			 * The manual documents that the device has
			 * 1000baseX full/half capability, but the
			 * register itself seems read back 0 on some
			 * boards.  Just hard-code the result.
			 */
			return (EXTSR_1000XFDX|EXTSR_1000XHDX);

		default:
			return (0);
		}

		rv = bus_space_read_4(sc->sc_st, sc->sc_sh, tbireg) & 0xffff;
		if (tbireg == SIP_TBISR) {
			/* LINK and ACOMP are switched! */
			int val = rv;

			rv = 0;
			if (val & TBISR_MR_LINK_STATUS)
				rv |= BMSR_LINK;
			if (val & TBISR_MR_AN_COMPLETE)
				rv |= BMSR_ACOMP;

			/*
			 * The manual claims this register reads back 0
			 * on hard and soft reset.  But we want to let
			 * the gentbi driver know that we support auto-
			 * negotiation, so hard-code this bit in the
			 * result.
			 */
			rv |= BMSR_ANEG | BMSR_EXTSTAT;
		}

		return (rv);
	}

	return mii_bitbang_readreg(self, &sipcom_mii_bitbang_ops, phy, reg);
}

/*
 * sip_dp83820_mii_writereg:	[mii interface function]
 *
 *	Write a PHY register on the MII of the DP83820.
 */
static void
sipcom_dp83820_mii_writereg(device_t self, int phy, int reg, int val)
{
	struct sip_softc *sc = device_private(self);

	if (sc->sc_cfg & CFG_TBI_EN) {
		bus_addr_t tbireg;

		if (phy != 0)
			return;

		switch (reg) {
		case MII_BMCR:		tbireg = SIP_TBICR; break;
		case MII_ANAR:		tbireg = SIP_TANAR; break;
		case MII_ANLPAR:	tbireg = SIP_TANLPAR; break;
		default:
			return;
		}

		bus_space_write_4(sc->sc_st, sc->sc_sh, tbireg, val);
		return;
	}

	mii_bitbang_writereg(self, &sipcom_mii_bitbang_ops, phy, reg, val);
}

/*
 * sip_dp83820_mii_statchg:	[mii interface function]
 *
 *	Callback from MII layer when media changes.
 */
static void
sipcom_dp83820_mii_statchg(struct ifnet *ifp)
{
	struct sip_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->sc_mii;
	u_int32_t cfg, pcr;

	/*
	 * Get flow control negotiation result.
	 */
	if (IFM_SUBTYPE(mii->mii_media.ifm_cur->ifm_media) == IFM_AUTO &&
	    (mii->mii_media_active & IFM_ETH_FMASK) != sc->sc_flowflags) {
		sc->sc_flowflags = mii->mii_media_active & IFM_ETH_FMASK;
		mii->mii_media_active &= ~IFM_ETH_FMASK;
	}

	/*
	 * Update TXCFG for full-duplex operation.
	 */
	if ((mii->mii_media_active & IFM_FDX) != 0)
		sc->sc_txcfg |= (TXCFG_CSI | TXCFG_HBI);
	else
		sc->sc_txcfg &= ~(TXCFG_CSI | TXCFG_HBI);

	/*
	 * Update RXCFG for full-duplex or loopback.
	 */
	if ((mii->mii_media_active & IFM_FDX) != 0 ||
	    IFM_SUBTYPE(mii->mii_media_active) == IFM_LOOP)
		sc->sc_rxcfg |= RXCFG_ATX;
	else
		sc->sc_rxcfg &= ~RXCFG_ATX;

	/*
	 * Update CFG for MII/GMII.
	 */
	if (sc->sc_ethercom.ec_if.if_baudrate == IF_Mbps(1000))
		cfg = sc->sc_cfg | CFG_MODE_1000;
	else
		cfg = sc->sc_cfg;

	/*
	 * 802.3x flow control.
	 */
	pcr = 0;
	if (sc->sc_flowflags & IFM_FLOW) {
		if (sc->sc_flowflags & IFM_ETH_TXPAUSE)
			pcr |= sc->sc_rx_flow_thresh;
		if (sc->sc_flowflags & IFM_ETH_RXPAUSE)
			pcr |= PCR_PSEN | PCR_PS_MCAST;
	}

	bus_space_write_4(sc->sc_st, sc->sc_sh, SIP_CFG, cfg);
	bus_space_write_4(sc->sc_st, sc->sc_sh, sc->sc_regs.r_txcfg,
	    sc->sc_txcfg);
	bus_space_write_4(sc->sc_st, sc->sc_sh, sc->sc_regs.r_rxcfg,
	    sc->sc_rxcfg);
	bus_space_write_4(sc->sc_st, sc->sc_sh, SIP_NS_PCR, pcr);
}

/*
 * sip_mii_bitbang_read: [mii bit-bang interface function]
 *
 *	Read the MII serial port for the MII bit-bang module.
 */
static u_int32_t
sipcom_mii_bitbang_read(device_t self)
{
	struct sip_softc *sc = device_private(self);

	return (bus_space_read_4(sc->sc_st, sc->sc_sh, SIP_EROMAR));
}

/*
 * sip_mii_bitbang_write: [mii big-bang interface function]
 *
 *	Write the MII serial port for the MII bit-bang module.
 */
static void
sipcom_mii_bitbang_write(device_t self, u_int32_t val)
{
	struct sip_softc *sc = device_private(self);

	bus_space_write_4(sc->sc_st, sc->sc_sh, SIP_EROMAR, val);
}

/*
 * sip_sis900_mii_readreg:	[mii interface function]
 *
 *	Read a PHY register on the MII.
 */
static int
sipcom_sis900_mii_readreg(device_t self, int phy, int reg)
{
	struct sip_softc *sc = device_private(self);
	u_int32_t enphy;

	/*
	 * The PHY of recent SiS chipsets is accessed through bitbang
	 * operations.
	 */
	if (sc->sc_model->sip_product == PCI_PRODUCT_SIS_900)
		return mii_bitbang_readreg(self, &sipcom_mii_bitbang_ops,
		    phy, reg);

#ifndef SIS900_MII_RESTRICT
	/*
	 * The SiS 900 has only an internal PHY on the MII.  Only allow
	 * MII address 0.
	 */
	if (sc->sc_model->sip_product == PCI_PRODUCT_SIS_900 && phy != 0)
		return (0);
#endif

	bus_space_write_4(sc->sc_st, sc->sc_sh, SIP_ENPHY,
	    (phy << ENPHY_PHYADDR_SHIFT) | (reg << ENPHY_REGADDR_SHIFT) |
	    ENPHY_RWCMD | ENPHY_ACCESS);
	do {
		enphy = bus_space_read_4(sc->sc_st, sc->sc_sh, SIP_ENPHY);
	} while (enphy & ENPHY_ACCESS);
	return ((enphy & ENPHY_PHYDATA) >> ENPHY_DATA_SHIFT);
}

/*
 * sip_sis900_mii_writereg:	[mii interface function]
 *
 *	Write a PHY register on the MII.
 */
static void
sipcom_sis900_mii_writereg(device_t self, int phy, int reg, int val)
{
	struct sip_softc *sc = device_private(self);
	u_int32_t enphy;

	if (sc->sc_model->sip_product == PCI_PRODUCT_SIS_900) {
		mii_bitbang_writereg(self, &sipcom_mii_bitbang_ops,
		    phy, reg, val);
		return;
	}

#ifndef SIS900_MII_RESTRICT
	/*
	 * The SiS 900 has only an internal PHY on the MII.  Only allow
	 * MII address 0.
	 */
	if (sc->sc_model->sip_product == PCI_PRODUCT_SIS_900 && phy != 0)
		return;
#endif

	bus_space_write_4(sc->sc_st, sc->sc_sh, SIP_ENPHY,
	    (val << ENPHY_DATA_SHIFT) | (phy << ENPHY_PHYADDR_SHIFT) |
	    (reg << ENPHY_REGADDR_SHIFT) | ENPHY_ACCESS);
	do {
		enphy = bus_space_read_4(sc->sc_st, sc->sc_sh, SIP_ENPHY);
	} while (enphy & ENPHY_ACCESS);
}

/*
 * sip_sis900_mii_statchg:	[mii interface function]
 *
 *	Callback from MII layer when media changes.
 */
static void
sipcom_sis900_mii_statchg(struct ifnet *ifp)
{
	struct sip_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->sc_mii;
	u_int32_t flowctl;

	/*
	 * Get flow control negotiation result.
	 */
	if (IFM_SUBTYPE(mii->mii_media.ifm_cur->ifm_media) == IFM_AUTO &&
	    (mii->mii_media_active & IFM_ETH_FMASK) != sc->sc_flowflags) {
		sc->sc_flowflags = mii->mii_media_active & IFM_ETH_FMASK;
		mii->mii_media_active &= ~IFM_ETH_FMASK;
	}

	/*
	 * Update TXCFG for full-duplex operation.
	 */
	if ((mii->mii_media_active & IFM_FDX) != 0)
		sc->sc_txcfg |= (TXCFG_CSI | TXCFG_HBI);
	else
		sc->sc_txcfg &= ~(TXCFG_CSI | TXCFG_HBI);

	/*
	 * Update RXCFG for full-duplex or loopback.
	 */
	if ((mii->mii_media_active & IFM_FDX) != 0 ||
	    IFM_SUBTYPE(mii->mii_media_active) == IFM_LOOP)
		sc->sc_rxcfg |= RXCFG_ATX;
	else
		sc->sc_rxcfg &= ~RXCFG_ATX;

	/*
	 * Update IMR for use of 802.3x flow control.
	 */
	if (sc->sc_flowflags & IFM_FLOW) {
		sc->sc_imr |= (ISR_PAUSE_END|ISR_PAUSE_ST);
		flowctl = FLOWCTL_FLOWEN;
	} else {
		sc->sc_imr &= ~(ISR_PAUSE_END|ISR_PAUSE_ST);
		flowctl = 0;
	}

	bus_space_write_4(sc->sc_st, sc->sc_sh, sc->sc_regs.r_txcfg,
	    sc->sc_txcfg);
	bus_space_write_4(sc->sc_st, sc->sc_sh, sc->sc_regs.r_rxcfg,
	    sc->sc_rxcfg);
	bus_space_write_4(sc->sc_st, sc->sc_sh, SIP_IMR, sc->sc_imr);
	bus_space_write_4(sc->sc_st, sc->sc_sh, SIP_FLOWCTL, flowctl);
}

/*
 * sip_dp83815_mii_readreg:	[mii interface function]
 *
 *	Read a PHY register on the MII.
 */
static int
sipcom_dp83815_mii_readreg(device_t self, int phy, int reg)
{
	struct sip_softc *sc = device_private(self);
	u_int32_t val;

	/*
	 * The DP83815 only has an internal PHY.  Only allow
	 * MII address 0.
	 */
	if (phy != 0)
		return (0);

	/*
	 * Apparently, after a reset, the DP83815 can take a while
	 * to respond.  During this recovery period, the BMSR returns
	 * a value of 0.  Catch this -- it's not supposed to happen
	 * (the BMSR has some hardcoded-to-1 bits), and wait for the
	 * PHY to come back to life.
	 *
	 * This works out because the BMSR is the first register
	 * read during the PHY probe process.
	 */
	do {
		val = bus_space_read_4(sc->sc_st, sc->sc_sh, SIP_NS_PHY(reg));
	} while (reg == MII_BMSR && val == 0);

	return (val & 0xffff);
}

/*
 * sip_dp83815_mii_writereg:	[mii interface function]
 *
 *	Write a PHY register to the MII.
 */
static void
sipcom_dp83815_mii_writereg(device_t self, int phy, int reg, int val)
{
	struct sip_softc *sc = device_private(self);

	/*
	 * The DP83815 only has an internal PHY.  Only allow
	 * MII address 0.
	 */
	if (phy != 0)
		return;

	bus_space_write_4(sc->sc_st, sc->sc_sh, SIP_NS_PHY(reg), val);
}

/*
 * sip_dp83815_mii_statchg:	[mii interface function]
 *
 *	Callback from MII layer when media changes.
 */
static void
sipcom_dp83815_mii_statchg(struct ifnet *ifp)
{
	struct sip_softc *sc = ifp->if_softc;

	/*
	 * Update TXCFG for full-duplex operation.
	 */
	if ((sc->sc_mii.mii_media_active & IFM_FDX) != 0)
		sc->sc_txcfg |= (TXCFG_CSI | TXCFG_HBI);
	else
		sc->sc_txcfg &= ~(TXCFG_CSI | TXCFG_HBI);

	/*
	 * Update RXCFG for full-duplex or loopback.
	 */
	if ((sc->sc_mii.mii_media_active & IFM_FDX) != 0 ||
	    IFM_SUBTYPE(sc->sc_mii.mii_media_active) == IFM_LOOP)
		sc->sc_rxcfg |= RXCFG_ATX;
	else
		sc->sc_rxcfg &= ~RXCFG_ATX;

	/*
	 * XXX 802.3x flow control.
	 */

	bus_space_write_4(sc->sc_st, sc->sc_sh, sc->sc_regs.r_txcfg,
	    sc->sc_txcfg);
	bus_space_write_4(sc->sc_st, sc->sc_sh, sc->sc_regs.r_rxcfg,
	    sc->sc_rxcfg);

	/*
	 * Some DP83815s experience problems when used with short
	 * (< 30m/100ft) Ethernet cables in 100BaseTX mode.  This
	 * sequence adjusts the DSP's signal attenuation to fix the
	 * problem.
	 */
	if (IFM_SUBTYPE(sc->sc_mii.mii_media_active) == IFM_100_TX) {
		uint32_t reg;

		bus_space_write_4(sc->sc_st, sc->sc_sh, 0x00cc, 0x0001);

		reg = bus_space_read_4(sc->sc_st, sc->sc_sh, 0x00f4);
		reg &= 0x0fff;
		bus_space_write_4(sc->sc_st, sc->sc_sh, 0x00f4, reg | 0x1000);
		delay(100);
		reg = bus_space_read_4(sc->sc_st, sc->sc_sh, 0x00fc);
		reg &= 0x00ff;
		if ((reg & 0x0080) == 0 || (reg >= 0x00d8)) {
			bus_space_write_4(sc->sc_st, sc->sc_sh, 0x00fc,
			    0x00e8);
			reg = bus_space_read_4(sc->sc_st, sc->sc_sh, 0x00f4);
			bus_space_write_4(sc->sc_st, sc->sc_sh, 0x00f4,
			    reg | 0x20);
		}

		bus_space_write_4(sc->sc_st, sc->sc_sh, 0x00cc, 0);
	}
}

static void
sipcom_dp83820_read_macaddr(struct sip_softc *sc,
    const struct pci_attach_args *pa, u_int8_t *enaddr)
{
	u_int16_t eeprom_data[SIP_DP83820_EEPROM_LENGTH / 2];
	u_int8_t cksum, *e, match;
	int i;

	/*
	 * EEPROM data format for the DP83820 can be found in
	 * the DP83820 manual, section 4.2.4.
	 */

	sipcom_read_eeprom(sc, 0, __arraycount(eeprom_data), eeprom_data);

	match = eeprom_data[SIP_DP83820_EEPROM_CHECKSUM / 2] >> 8;
	match = ~(match - 1);

	cksum = 0x55;
	e = (u_int8_t *) eeprom_data;
	for (i = 0; i < SIP_DP83820_EEPROM_CHECKSUM; i++)
		cksum += *e++;

	if (cksum != match)
		printf("%s: Checksum (%x) mismatch (%x)",
		    device_xname(sc->sc_dev), cksum, match);

	enaddr[0] = eeprom_data[SIP_DP83820_EEPROM_PMATCH2 / 2] & 0xff;
	enaddr[1] = eeprom_data[SIP_DP83820_EEPROM_PMATCH2 / 2] >> 8;
	enaddr[2] = eeprom_data[SIP_DP83820_EEPROM_PMATCH1 / 2] & 0xff;
	enaddr[3] = eeprom_data[SIP_DP83820_EEPROM_PMATCH1 / 2] >> 8;
	enaddr[4] = eeprom_data[SIP_DP83820_EEPROM_PMATCH0 / 2] & 0xff;
	enaddr[5] = eeprom_data[SIP_DP83820_EEPROM_PMATCH0 / 2] >> 8;
}

static void
sipcom_sis900_eeprom_delay(struct sip_softc *sc)
{
	int i;

	/*
	 * FreeBSD goes from (300/33)+1 [10] to 0.  There must be
	 * a reason, but I don't know it.
	 */
	for (i = 0; i < 10; i++)
		bus_space_read_4(sc->sc_st, sc->sc_sh, SIP_CR);
}

static void
sipcom_sis900_read_macaddr(struct sip_softc *sc,
    const struct pci_attach_args *pa, u_int8_t *enaddr)
{
	u_int16_t myea[ETHER_ADDR_LEN / 2];

	switch (sc->sc_rev) {
	case SIS_REV_630S:
	case SIS_REV_630E:
	case SIS_REV_630EA1:
	case SIS_REV_630ET:
	case SIS_REV_635:
		/*
		 * The MAC address for the on-board Ethernet of
		 * the SiS 630 chipset is in the NVRAM.  Kick
		 * the chip into re-loading it from NVRAM, and
		 * read the MAC address out of the filter registers.
		 */
		bus_space_write_4(sc->sc_st, sc->sc_sh, SIP_CR, CR_RLD);

		bus_space_write_4(sc->sc_st, sc->sc_sh, SIP_RFCR,
		    RFCR_RFADDR_NODE0);
		myea[0] = bus_space_read_4(sc->sc_st, sc->sc_sh, SIP_RFDR) &
		    0xffff;

		bus_space_write_4(sc->sc_st, sc->sc_sh, SIP_RFCR,
		    RFCR_RFADDR_NODE2);
		myea[1] = bus_space_read_4(sc->sc_st, sc->sc_sh, SIP_RFDR) &
		    0xffff;

		bus_space_write_4(sc->sc_st, sc->sc_sh, SIP_RFCR,
		    RFCR_RFADDR_NODE4);
		myea[2] = bus_space_read_4(sc->sc_st, sc->sc_sh, SIP_RFDR) &
		    0xffff;
		break;

	case SIS_REV_960:
		{
#define	SIS_SET_EROMAR(x,y)	bus_space_write_4(x->sc_st, x->sc_sh, SIP_EROMAR,	\
				    bus_space_read_4(x->sc_st, x->sc_sh, SIP_EROMAR) | (y))

#define	SIS_CLR_EROMAR(x,y)	bus_space_write_4(x->sc_st, x->sc_sh, SIP_EROMAR,	\
				    bus_space_read_4(x->sc_st, x->sc_sh, SIP_EROMAR) & ~(y))

			int waittime, i;

			/* Allow to read EEPROM from LAN. It is shared
			 * between a 1394 controller and the NIC and each
			 * time we access it, we need to set SIS_EECMD_REQ.
			 */
			SIS_SET_EROMAR(sc, EROMAR_REQ);

			for (waittime = 0; waittime < 1000; waittime++) { /* 1 ms max */
				/* Force EEPROM to idle state. */

				/*
				 * XXX-cube This is ugly.  I'll look for docs about it.
				 */
				SIS_SET_EROMAR(sc, EROMAR_EECS);
				sipcom_sis900_eeprom_delay(sc);
				for (i = 0; i <= 25; i++) { /* Yes, 26 times. */
					SIS_SET_EROMAR(sc, EROMAR_EESK);
					sipcom_sis900_eeprom_delay(sc);
					SIS_CLR_EROMAR(sc, EROMAR_EESK);
					sipcom_sis900_eeprom_delay(sc);
				}
				SIS_CLR_EROMAR(sc, EROMAR_EECS);
				sipcom_sis900_eeprom_delay(sc);
				bus_space_write_4(sc->sc_st, sc->sc_sh, SIP_EROMAR, 0);

				if (bus_space_read_4(sc->sc_st, sc->sc_sh, SIP_EROMAR) & EROMAR_GNT) {
					sipcom_read_eeprom(sc, SIP_EEPROM_ETHERNET_ID0 >> 1,
					    sizeof(myea) / sizeof(myea[0]), myea);
					break;
				}
				DELAY(1);
			}

			/*
			 * Set SIS_EECTL_CLK to high, so a other master
			 * can operate on the i2c bus.
			 */
			SIS_SET_EROMAR(sc, EROMAR_EESK);

			/* Refuse EEPROM access by LAN */
			SIS_SET_EROMAR(sc, EROMAR_DONE);
		} break;

	default:
		sipcom_read_eeprom(sc, SIP_EEPROM_ETHERNET_ID0 >> 1,
		    sizeof(myea) / sizeof(myea[0]), myea);
	}

	enaddr[0] = myea[0] & 0xff;
	enaddr[1] = myea[0] >> 8;
	enaddr[2] = myea[1] & 0xff;
	enaddr[3] = myea[1] >> 8;
	enaddr[4] = myea[2] & 0xff;
	enaddr[5] = myea[2] >> 8;
}

/* Table and macro to bit-reverse an octet. */
static const u_int8_t bbr4[] = {0,8,4,12,2,10,6,14,1,9,5,13,3,11,7,15};
#define bbr(v)	((bbr4[(v)&0xf] << 4) | bbr4[((v)>>4) & 0xf])

static void
sipcom_dp83815_read_macaddr(struct sip_softc *sc,
    const struct pci_attach_args *pa, u_int8_t *enaddr)
{
	u_int16_t eeprom_data[SIP_DP83815_EEPROM_LENGTH / 2], *ea;
	u_int8_t cksum, *e, match;
	int i;

	sipcom_read_eeprom(sc, 0, sizeof(eeprom_data) /
	    sizeof(eeprom_data[0]), eeprom_data);

	match = eeprom_data[SIP_DP83815_EEPROM_CHECKSUM/2] >> 8;
	match = ~(match - 1);

	cksum = 0x55;
	e = (u_int8_t *) eeprom_data;
	for (i=0 ; i<SIP_DP83815_EEPROM_CHECKSUM ; i++) {
		cksum += *e++;
	}
	if (cksum != match) {
		printf("%s: Checksum (%x) mismatch (%x)",
		    device_xname(sc->sc_dev), cksum, match);
	}

	/*
	 * Unrolled because it makes slightly more sense this way.
	 * The DP83815 stores the MAC address in bit 0 of word 6
	 * through bit 15 of word 8.
	 */
	ea = &eeprom_data[6];
	enaddr[0] = ((*ea & 0x1) << 7);
	ea++;
	enaddr[0] |= ((*ea & 0xFE00) >> 9);
	enaddr[1] = ((*ea & 0x1FE) >> 1);
	enaddr[2] = ((*ea & 0x1) << 7);
	ea++;
	enaddr[2] |= ((*ea & 0xFE00) >> 9);
	enaddr[3] = ((*ea & 0x1FE) >> 1);
	enaddr[4] = ((*ea & 0x1) << 7);
	ea++;
	enaddr[4] |= ((*ea & 0xFE00) >> 9);
	enaddr[5] = ((*ea & 0x1FE) >> 1);

	/*
	 * In case that's not weird enough, we also need to reverse
	 * the bits in each byte.  This all actually makes more sense
	 * if you think about the EEPROM storage as an array of bits
	 * being shifted into bytes, but that's not how we're looking
	 * at it here...
	 */
	for (i = 0; i < 6 ;i++)
		enaddr[i] = bbr(enaddr[i]);
}

/*
 * sip_mediastatus:	[ifmedia interface function]
 *
 *	Get the current interface media status.
 */
static void
sipcom_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct sip_softc *sc = ifp->if_softc;

	if (!device_is_active(sc->sc_dev)) {
		ifmr->ifm_active = IFM_ETHER | IFM_NONE;
		ifmr->ifm_status = 0;
		return;
	}
	ether_mediastatus(ifp, ifmr);
	ifmr->ifm_active = (ifmr->ifm_active & ~IFM_ETH_FMASK) |
			   sc->sc_flowflags;
}

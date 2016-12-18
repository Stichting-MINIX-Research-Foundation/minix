/*	$NetBSD: atwvar.h,v 1.37 2010/03/14 21:25:59 dyoung Exp $	*/

/*
 * Copyright (c) 2003, 2004 The NetBSD Foundation, Inc.  All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by David Young.
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

#ifndef _DEV_IC_ATWVAR_H_
#define	_DEV_IC_ATWVAR_H_

#include <sys/queue.h>
#include <sys/callout.h>
#include <sys/time.h>

/*
 * Transmit descriptor list size.  This is arbitrary, but allocate
 * enough descriptors for 64 pending transmissions and 16 segments
 * per packet.  Since a descriptor holds 2 buffer addresses, that's
 * 8 descriptors per packet.  This MUST work out to a power of 2.
 */
#define	ATW_NTXSEGS		16

#define	ATW_TXQUEUELEN	64
#define	ATW_NTXDESC		(ATW_TXQUEUELEN * ATW_NTXSEGS)
#define	ATW_NTXDESC_MASK	(ATW_NTXDESC - 1)
#define	ATW_NEXTTX(x)		((x + 1) & ATW_NTXDESC_MASK)

/*
 * Receive descriptor list size.  We have one Rx buffer per incoming
 * packet, so this logic is a little simpler.
 */
#define	ATW_NRXDESC		64
#define	ATW_NRXDESC_MASK	(ATW_NRXDESC - 1)
#define	ATW_NEXTRX(x)		((x + 1) & ATW_NRXDESC_MASK)

/*
 * Control structures are DMA'd to the ADM8211 chip.  We allocate them in
 * a single clump that maps to a single DMA segment to make several things
 * easier.
 */
struct atw_control_data {
	/*
	 * The transmit descriptors.
	 */
	struct atw_txdesc acd_txdescs[ATW_NTXDESC];

	/*
	 * The receive descriptors.
	 */
	struct atw_rxdesc acd_rxdescs[ATW_NRXDESC];
};

#define	ATW_CDOFF(x)		offsetof(struct atw_control_data, x)
#define	ATW_CDTXOFF(x)	ATW_CDOFF(acd_txdescs[(x)])
#define	ATW_CDRXOFF(x)	ATW_CDOFF(acd_rxdescs[(x)])
/*
 * Software state for transmit jobs.
 */
struct atw_txsoft {
	struct mbuf *txs_mbuf;		/* head of our mbuf chain */
	bus_dmamap_t txs_dmamap;	/* our DMA map */
	int txs_firstdesc;		/* first descriptor in packet */
	int txs_lastdesc;		/* last descriptor in packet */
	int txs_ndescs;			/* number of descriptors */
	struct ieee80211_duration	txs_d0;
	struct ieee80211_duration	txs_dn;
	SIMPLEQ_ENTRY(atw_txsoft) txs_q;
};

SIMPLEQ_HEAD(atw_txsq, atw_txsoft);

/*
 * Software state for receive jobs.
 */
struct atw_rxsoft {
	struct mbuf *rxs_mbuf;		/* head of our mbuf chain */
	bus_dmamap_t rxs_dmamap;	/* our DMA map */
};

/*
 * Table which describes the transmit threshold mode.  We generally
 * start at index 0.  Whenever we get a transmit underrun, we increment
 * our index, falling back if we encounter the NULL terminator.
 */
struct atw_txthresh_tab {
	u_int32_t txth_opmode;		/* OPMODE bits */
	const char *txth_name;		/* name of mode */
};

#define	ATW_TXTHRESH_TAB_LO_RATE {					\
	{ ATW_NAR_TR_L64,	"64 bytes" },				\
	{ ATW_NAR_TR_L160,	"160 bytes" },				\
	{ ATW_NAR_TR_L192,	"192 bytes" },				\
	{ ATW_NAR_SF,		"store and forward" },			\
	{ 0,			NULL },					\
}

#define	ATW_TXTHRESH_TAB_HI_RATE {					\
	{ ATW_NAR_TR_H96,	"96 bytes" },				\
	{ ATW_NAR_TR_H288,	"288 bytes" },				\
	{ ATW_NAR_TR_H544,	"544 bytes" },				\
	{ ATW_NAR_SF,		"store and forward" },			\
	{ 0,			NULL },					\
}

enum atw_rftype { ATW_RFTYPE_INTERSIL = 0, ATW_RFTYPE_RFMD  = 1,
       ATW_RFTYPE_MARVEL = 2 };

enum atw_bbptype { ATW_BBPTYPE_INTERSIL = 0, ATW_BBPTYPE_RFMD  = 1,
       ATW_BBPTYPE_MARVEL = 2, ATW_C_BBPTYPE_RFMD  = 5 };

/* Radio capture format for ADMtek. */

#define ATW_RX_RADIOTAP_PRESENT	\
	((1 << IEEE80211_RADIOTAP_FLAGS) | (1 << IEEE80211_RADIOTAP_RATE) | \
	 (1 << IEEE80211_RADIOTAP_CHANNEL) | \
	 (1 << IEEE80211_RADIOTAP_DB_ANTSIGNAL))

struct atw_rx_radiotap_header {
	struct ieee80211_radiotap_header	ar_ihdr;
	uint8_t					ar_flags;
	uint8_t					ar_rate;
	uint16_t				ar_chan_freq;
	uint16_t				ar_chan_flags;
	uint8_t					ar_antsignal;
} __packed;

#define ATW_TX_RADIOTAP_PRESENT	((1 << IEEE80211_RADIOTAP_RATE) | \
				 (1 << IEEE80211_RADIOTAP_CHANNEL))

struct atw_tx_radiotap_header {
	struct ieee80211_radiotap_header	at_ihdr;
	uint8_t					at_rate;
	uint8_t					at_pad;
	uint16_t				at_chan_freq;
	uint16_t				at_chan_flags;
} __packed;

enum atw_revision {
	ATW_REVISION_AB = 0x11,	/* ADM8211A */
	ATW_REVISION_AF = 0x15,	/* ADM8211A? */
	ATW_REVISION_BA = 0x20,	/* ADM8211B */
	ATW_REVISION_CA = 0x30	/* ADM8211C/CR */
};

struct atw_softc {
	device_t		sc_dev;
	device_suspensor_t	sc_suspensor;
	pmf_qual_t		sc_qual;

	struct ethercom		sc_ec;
	struct ieee80211com	sc_ic;
	int			(*sc_newstate)(struct ieee80211com *,
					enum ieee80211_state, int);
	void			(*sc_recv_mgmt)(struct ieee80211com *,
				    struct mbuf *, struct ieee80211_node *,
				    int, int, u_int32_t);
	struct ieee80211_node	*(*sc_node_alloc)(struct ieee80211_node_table*);
	void			(*sc_node_free)(struct ieee80211_node *);

	int			sc_tx_timer;
	int			sc_rescan_timer;

	bus_space_tag_t		sc_st;		/* bus space tag */
	bus_space_handle_t	sc_sh;		/* bus space handle */
	bus_dma_tag_t		sc_dmat;	/* bus dma tag */
	u_int32_t		sc_cacheline;	/* cache line size */
	u_int32_t		sc_maxburst;	/* maximum burst length */

	const struct atw_txthresh_tab	*sc_txth;
	int				sc_txthresh; /* current tx threshold */

	u_int			sc_cur_chan;	/* current channel */

	int			sc_flags;

	u_int16_t		*sc_srom;
	u_int16_t		sc_sromsz;

	struct bpf_if *		sc_radiobpf;

	bus_dma_segment_t	sc_cdseg;	/* control data memory */
	int			sc_cdnseg;	/* number of segments */
	bus_dmamap_t		sc_cddmamap;	/* control data DMA map */
#define	sc_cddma	sc_cddmamap->dm_segs[0].ds_addr

	/*
	 * Software state for transmit and receive descriptors.
	 */
	struct atw_txsoft sc_txsoft[ATW_TXQUEUELEN];
	struct atw_rxsoft sc_rxsoft[ATW_NRXDESC];

	/*
	 * Control data structures.
	 */
	struct atw_control_data *sc_control_data;
#define	sc_txdescs	sc_control_data->acd_txdescs
#define	sc_rxdescs	sc_control_data->acd_rxdescs
#define	sc_setup_desc	sc_control_data->acd_setup_desc

	int	sc_txfree;		/* number of free Tx descriptors */
	int	sc_txnext;		/* next ready Tx descriptor */
	int	sc_ntxsegs;		/* number of transmit segs per pkt */

	struct atw_txsq sc_txfreeq;	/* free Tx descsofts */
	struct atw_txsq sc_txdirtyq;	/* dirty Tx descsofts */

	int	sc_rxptr;		/* next ready RX descriptor/descsoft */

	u_int32_t	sc_busmode;	/* copy of ATW_PAR */
	u_int32_t	sc_opmode;	/* copy of ATW_NAR */
	u_int32_t	sc_inten;	/* copy of ATW_IER */
	u_int32_t	sc_wepctl;	/* copy of ATW_WEPCTL */

	u_int32_t	sc_rxint_mask;	/* mask of Rx interrupts we want */
	u_int32_t	sc_txint_mask;	/* mask of Tx interrupts we want */
	u_int32_t	sc_linkint_mask;/* link-state interrupts mask */

	enum atw_rftype		sc_rftype;
	enum atw_bbptype	sc_bbptype;
	u_int32_t	sc_synctl_rd;
	u_int32_t	sc_synctl_wr;
	u_int32_t	sc_bbpctl_rd;
	u_int32_t	sc_bbpctl_wr;

	void		(*sc_recv_beacon)(struct ieee80211com *, struct mbuf *,
			    int, u_int32_t);
	void		(*sc_recv_prresp)(struct ieee80211com *, struct mbuf *,
			    int, u_int32_t);

	/* ADM8211 state variables. */
	u_int8_t	sc_sram[ATW_SRAM_MAXSIZE];
	u_int		sc_sramlen;
	u_int8_t	sc_bssid[IEEE80211_ADDR_LEN];
	uint8_t		sc_rev;
	uint8_t		sc_rf3000_options1;
	uint8_t		sc_rf3000_options2;

	struct evcnt	sc_misc_ev;
	struct evcnt	sc_workaround1_ev;
	struct evcnt	sc_rxamatch_ev;
	struct evcnt	sc_rxpkt1in_ev;

	struct evcnt	sc_xmit_ev;
	struct evcnt	sc_tuf_ev;	/* transmit underflow errors */
	struct evcnt	sc_tro_ev;	/* transmit overrun */
	struct evcnt	sc_trt_ev;	/* retry count exceeded */
	struct evcnt	sc_tlt_ev;	/* lifetime exceeded */
	struct evcnt	sc_sofbr_ev;	/* packet size mismatch */

	struct evcnt	sc_recv_ev;
	struct evcnt	sc_crc16e_ev;
	struct evcnt	sc_crc32e_ev;
	struct evcnt	sc_icve_ev;
	struct evcnt	sc_sfde_ev;
	struct evcnt	sc_sige_ev;

	struct callout	sc_scan_ch;
	union {
		struct atw_rx_radiotap_header	tap;
		u_int8_t			pad[64];
	} sc_rxtapu;
	union {
		struct atw_tx_radiotap_header	tap;
		u_int8_t			pad[64];
	} sc_txtapu;
};

#define	sc_if		sc_ec.ec_if
#define sc_rxtap	sc_rxtapu.tap
#define sc_txtap	sc_txtapu.tap

/* XXX this is fragile. try not to introduce any u_int32_t's. */
struct atw_frame {
/*00*/	u_int8_t			atw_dst[IEEE80211_ADDR_LEN];
/*06*/	u_int8_t			atw_rate;	/* TX rate in 100Kbps */
/*07*/	u_int8_t			atw_service;	/* 0 */
/*08*/	u_int16_t			atw_paylen;	/* payload length */
/*0a*/	u_int8_t			atw_fc[2];	/* 802.11 Frame
							 * Control
							 */
	/* 802.11 PLCP Length for first & last fragment */
/*0c*/	u_int16_t			atw_tail_plcplen;
/*0e*/	u_int16_t			atw_head_plcplen;
	/* 802.11 Duration for first & last fragment */
/*10*/	u_int16_t			atw_tail_dur;
/*12*/	u_int16_t			atw_head_dur;
/*14*/	u_int8_t			atw_addr4[IEEE80211_ADDR_LEN];
	union {
		struct {
/*1a*/			u_int16_t	hdrctl;	/*transmission control*/
/*1c*/			u_int16_t	fragthr;/* fragmentation threshold
						 * [0:11], zero [12:15].
						 */
/*1e*/			u_int8_t	fragnum;/* fragment number [4:7],
						 * zero [0:3].
						 */
/*1f*/			u_int8_t	rtylmt;	/* retry limit */
/*20*/			u_int8_t	wepkey0[4];/* ??? */
/*24*/			u_int8_t	wepkey1[4];/* ??? */
/*28*/			u_int8_t	wepkey2[4];/* ??? */
/*2c*/			u_int8_t	wepkey3[4];/* ??? */
/*30*/			u_int8_t	keyid;
/*31*/			u_int8_t	reserved0[7];
		} s1;
		struct {
			u_int8_t		pad[6];
			struct ieee80211_frame	ihdr;
		} s2;
	} u;
} __packed;

#define atw_hdrctl	u.s1.hdrctl
#define atw_fragthr	u.s1.fragthr
#define atw_fragnum	u.s1.fragnum
#define atw_rtylmt	u.s1.rtylmt
#define atw_keyid	u.s1.keyid
#define atw_ihdr	u.s2.ihdr

#define ATW_HDRCTL_SHORT_PREAMBLE	__BIT(0)	/* use short preamble */
#define ATW_HDRCTL_MORE_FRAG		__BIT(1)	/* ??? from Linux */
#define ATW_HDRCTL_MORE_DATA		__BIT(2)	/* ??? from Linux */
#define ATW_HDRCTL_FRAG_NUM		__BIT(3)	/* ??? from Linux */
#define ATW_HDRCTL_RTSCTS		__BIT(4)	/* send RTS */
#define ATW_HDRCTL_WEP			__BIT(5)
/* MAC adds FCS?  Linux calls this "enable extended header" */
#define ATW_HDRCTL_UNKNOWN1		__BIT(15)
#define ATW_HDRCTL_UNKNOWN2		__BIT(8)

#define ATW_FRAGTHR_FRAGTHR_MASK	__BITS(0, 11)
#define ATW_FRAGNUM_FRAGNUM_MASK	__BITS(4, 7)

/* Values for sc_flags. */
#define	ATWF_MRL		0x00000001	/* memory read line okay */
#define	ATWF_MRM		0x00000002	/* memory read multi okay */
#define	ATWF_MWI		0x00000004	/* memory write inval okay */
#define	ATWF_SHORT_PREAMBLE	0x00000008	/* short preamble enabled */
#define	ATWF_ATTACHED		0x00000010	/* attach has succeeded */
#define	ATWF_ENABLED		0x00000020	/* chip is enabled */
#define	ATWF_WEP_SRAM_VALID	0x00000040	/* SRAM matches s/w state */

#define	ATW_CDTXADDR(sc, x)	((sc)->sc_cddma + ATW_CDTXOFF((x)))
#define	ATW_CDRXADDR(sc, x)	((sc)->sc_cddma + ATW_CDRXOFF((x)))

#define	ATW_CDTXSYNC(sc, x, n, ops)					\
do {									\
	int __x, __n;							\
									\
	__x = (x);							\
	__n = (n);							\
									\
	/* If it will wrap around, sync to the end of the ring. */	\
	if ((__x + __n) > ATW_NTXDESC) {				\
		bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,	\
		    ATW_CDTXOFF(__x), sizeof(struct atw_txdesc) *	\
		    (ATW_NTXDESC - __x), (ops));			\
		__n -= (ATW_NTXDESC - __x);				\
		__x = 0;						\
	}								\
									\
	/* Now sync whatever is left. */				\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    ATW_CDTXOFF(__x), sizeof(struct atw_txdesc) * __n, (ops)); \
} while (0)

#define	ATW_CDRXSYNC(sc, x, ops)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    ATW_CDRXOFF((x)), sizeof(struct atw_rxdesc), (ops))

/*
 * Note we rely on MCLBYTES being a power of two.  Because the `length'
 * field is only 11 bits, we must subtract 1 from the length to avoid
 * having it truncated to 0!
 */
static inline void
atw_init_rxdesc(struct atw_softc *sc, int x)
{
	struct atw_rxsoft *rxs = &sc->sc_rxsoft[x];
	struct atw_rxdesc *rxd = &sc->sc_rxdescs[x];
	struct mbuf *m = rxs->rxs_mbuf;

	rxd->ar_buf1 =
	    htole32(rxs->rxs_dmamap->dm_segs[0].ds_addr);
	rxd->ar_buf2 =	/* for descriptor chaining */
	    htole32(ATW_CDRXADDR((sc), ATW_NEXTRX(x)));
	rxd->ar_ctlrssi =
	    htole32(__SHIFTIN(((m->m_ext.ext_size - 1) & ~0x3U),
	                   ATW_RXCTL_RBS1_MASK) |
		    0 /* ATW_RXCTL_RCH */ |
	    (x == (ATW_NRXDESC - 1) ? ATW_RXCTL_RER : 0));
	rxd->ar_stat = htole32(ATW_RXSTAT_OWN);

	ATW_CDRXSYNC((sc), x, BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
}

/* country codes from ADM8211 SROM */
#define	ATW_COUNTRY_FCC 0		/* USA 1-11 */
#define	ATW_COUNTRY_IC 1		/* Canada 1-11 */
#define	ATW_COUNTRY_ETSI 2		/* European Union (?) 1-13 */
#define	ATW_COUNTRY_SPAIN 3		/* 10-11 */
#define	ATW_COUNTRY_FRANCE 4		/* 10-13 */
#define	ATW_COUNTRY_MKK 5		/* Japan: 14 */
#define	ATW_COUNTRY_MKK2 6		/* Japan: 1-14 */

/*
 * register space access macros
 */
#define	ATW_READ(sc, reg)						\
	bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (reg))

#define	ATW_WRITE(sc, reg, val)					\
	bus_space_write_4((sc)->sc_st, (sc)->sc_sh, (reg), (val))

#define	ATW_SET(sc, reg, mask)					\
	ATW_WRITE((sc), (reg), ATW_READ((sc), (reg)) | (mask))

#define	ATW_CLR(sc, reg, mask)					\
	ATW_WRITE((sc), (reg), ATW_READ((sc), (reg)) & ~(mask))

#define	ATW_ISSET(sc, reg, mask)					\
	(ATW_READ((sc), (reg)) & (mask))

void	atw_attach(struct atw_softc *);
int	atw_detach(struct atw_softc *);
int	atw_activate(device_t, enum devact);
int	atw_intr(void *arg);
bool	atw_shutdown(device_t, int);
bool	atw_suspend(device_t, const pmf_qual_t *);

#endif /* _DEV_IC_ATWVAR_H_ */

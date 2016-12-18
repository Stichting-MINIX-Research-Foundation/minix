/*	$NetBSD: athvar.h,v 1.36 2013/01/27 12:48:56 jmcneill Exp $	*/

/*-
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 *
 * $FreeBSD: src/sys/dev/ath/if_athvar.h,v 1.29 2005/08/08 18:46:36 sam Exp $
 */

/*
 * Defintions for the Atheros Wireless LAN controller driver.
 */
#ifndef _DEV_ATH_ATHVAR_H
#define _DEV_ATH_ATHVAR_H

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <net80211/ieee80211_netbsd.h>
#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <external/isc/atheros_hal/dist/ah.h>

#include <dev/ic/ath_netbsd.h>
#include <dev/ic/athioctl.h>
#include <dev/ic/athrate.h>

#define	ATH_TIMEOUT		1000

#ifndef ATH_RXBUF
#define	ATH_RXBUF	40		/* number of RX buffers */
#endif
#ifndef ATH_TXBUF
#define	ATH_TXBUF	200		/* number of TX buffers */
#endif
#define	ATH_TXDESC	10		/* number of descriptors per buffer */
#define	ATH_TXMAXTRY	11		/* max number of transmit attempts */
#define	ATH_TXMGTTRY	4		/* xmit attempts for mgt/ctl frames */
#define	ATH_TXINTR_PERIOD 5		/* max number of batched tx descriptors */

#define	ATH_BEACON_AIFS_DEFAULT	 0	/* default aifs for ap beacon q */
#define	ATH_BEACON_CWMIN_DEFAULT 0	/* default cwmin for ap beacon q */
#define	ATH_BEACON_CWMAX_DEFAULT 0	/* default cwmax for ap beacon q */

/*
 * The key cache is used for h/w cipher state and also for
 * tracking station state such as the current tx antenna.
 * We also setup a mapping table between key cache slot indices
 * and station state to short-circuit node lookups on rx.
 * Different parts have different size key caches.  We handle
 * up to ATH_KEYMAX entries (could dynamically allocate state).
 */
#define	ATH_KEYMAX	128		/* max key cache size we handle */
#define	ATH_KEYBYTES	(ATH_KEYMAX/NBBY)	/* storage space in bytes */
/*
 * Convert from net80211 layer values to Ath layer values. Hopefully this will
 * be optimised away when the two constants are the same.
 */
typedef unsigned int ath_keyix_t;
#define ATH_KEY(_keyix) ((_keyix == IEEE80211_KEYIX_NONE) ? HAL_TXKEYIX_INVALID : _keyix)

/* driver-specific node state */
struct ath_node {
	struct ieee80211_node an_node;	/* base class */
	u_int32_t	an_avgrssi;	/* average rssi over all rx frames */
	/* variable-length rate control state follows */
};
#define	ATH_NODE(ni)	((struct ath_node *)(ni))
#define	ATH_NODE_CONST(ni)	((const struct ath_node *)(ni))

#define ATH_RSSI_LPF_LEN	10
#define ATH_RSSI_DUMMY_MARKER	0x127
#define ATH_EP_MUL(x, mul)	((x) * (mul))
#define ATH_RSSI_IN(x)		(ATH_EP_MUL((x), HAL_RSSI_EP_MULTIPLIER))
#define ATH_LPF_RSSI(x, y, len) \
    ((x != ATH_RSSI_DUMMY_MARKER) ? (((x) * ((len) - 1) + (y)) / (len)) : (y))
#define ATH_RSSI_LPF(x, y) do {						\
    if ((y) >= -20)							\
    	x = ATH_LPF_RSSI((x), ATH_RSSI_IN((y)), ATH_RSSI_LPF_LEN);	\
} while (0)

struct ath_buf {
	STAILQ_ENTRY(ath_buf)	bf_list;
#define bf_nseg		bf_dmamap->dm_nsegs
	int			bf_flags;	/* tx descriptor flags */
	struct ath_desc		*bf_desc;	/* virtual addr of desc */
	bus_addr_t		bf_daddr;	/* physical addr of desc */
	bus_dmamap_t		bf_dmamap;	/* DMA map for mbuf chain */
	struct mbuf		*bf_m;		/* mbuf for buf */
	struct ieee80211_node	*bf_node;	/* pointer to the node */
#define bf_mapsize	bf_dmamap->dm_mapsize
#define	ATH_MAX_SCATTER		ATH_TXDESC	/* max(tx,rx,beacon) desc's */
#define bf_segs		bf_dmamap->dm_segs
};
typedef STAILQ_HEAD(, ath_buf) ath_bufhead;

/*
 * DMA state for tx/rx descriptors.
 */
struct ath_descdma {
	const char*		dd_name;
	struct ath_desc		*dd_desc;	/* descriptors */
	bus_addr_t		dd_desc_paddr;	/* physical addr of dd_desc */
	bus_size_t		dd_desc_len;	/* size of dd_desc */
	bus_dma_segment_t	dd_dseg;
	int			dd_dnseg;	/* number of segments */
	bus_dma_tag_t		dd_dmat;	/* bus DMA tag */
	bus_dmamap_t		dd_dmamap;	/* DMA map for descriptors */
	struct ath_buf		*dd_bufptr;	/* associated buffers */
};

/*
 * Data transmit queue state.  One of these exists for each
 * hardware transmit queue.  Packets sent to us from above
 * are assigned to queues based on their priority.  Not all
 * devices support a complete set of hardware transmit queues.
 * For those devices the array sc_ac2q will map multiple
 * priorities to fewer hardware queues (typically all to one
 * hardware queue).
 */
struct ath_txq {
	u_int			axq_qnum;	/* hardware q number */
	u_int			axq_depth;	/* queue depth (stat only) */
	u_int			axq_intrcnt;	/* interrupt count */
	u_int32_t		*axq_link;	/* link ptr in last TX desc */
	STAILQ_HEAD(, ath_buf)	axq_q;		/* transmit queue */
	ath_txq_lock_t		axq_lock;	/* lock on q and link */
	/*
	 * State for patching up CTS when bursting.
	 */
	struct	ath_buf		*axq_linkbuf;	/* va of last buffer */
	u_int			axq_timer;	/* transmit timeout */
};

#define ATH_TXQ_INSERT_TAIL(_tq, _elm, _field) do { \
	STAILQ_INSERT_TAIL(&(_tq)->axq_q, (_elm), _field); \
	(_tq)->axq_depth++; \
	(_tq)->axq_timer = 5; \
} while (0)
#define ATH_TXQ_REMOVE_HEAD(_tq, _field) do { \
	STAILQ_REMOVE_HEAD(&(_tq)->axq_q, _field); \
	if (--(_tq)->axq_depth == 0) \
		(_tq)->axq_timer = 0; \
} while (0)

struct taskqueue;
struct ath_tx99;

struct ath_softc {
	device_t 		sc_dev;
	device_suspensor_t	sc_suspensor;
	pmf_qual_t		sc_qual;
	struct ethercom		sc_ec;		/* interface common */
	struct ath_stats	sc_stats;	/* interface statistics */
	struct ieee80211com	sc_ic;		/* IEEE 802.11 common */
	void			(*sc_power)(struct ath_softc *, int);
	int			sc_regdomain;
	int			sc_countrycode;
	int			sc_debug;
	struct sysctllog	*sc_sysctllog;
	void			(*sc_recv_mgmt)(struct ieee80211com *,
					struct mbuf *,
					struct ieee80211_node *,
					int, int, u_int32_t);
	int			(*sc_newstate)(struct ieee80211com *,
					enum ieee80211_state, int);
	void 			(*sc_node_free)(struct ieee80211_node *);
	HAL_BUS_TAG		sc_st;		/* bus space tag */
	HAL_BUS_HANDLE		sc_sh;		/* bus space handle */
	bus_dma_tag_t		sc_dmat;	/* bus DMA tag */
	struct ath_hal		*sc_ah;		/* Atheros HAL */
	struct ath_ratectrl	*sc_rc;		/* tx rate control support */
	struct ath_tx99		*sc_tx99;	/* tx99 adjunct state */
	void			(*sc_setdefantenna)(struct ath_softc *, u_int);
	unsigned int		sc_mrretry : 1,	/* multi-rate retry support */
				sc_softled : 1,	/* enable LED gpio status */
				sc_splitmic: 1,	/* split TKIP MIC keys */
				sc_needmib : 1,	/* enable MIB stats intr */
				sc_diversity : 1,/* enable rx diversity */
				sc_hasveol : 1,	/* tx VEOL support */
				sc_ledstate: 1,	/* LED on/off state */
				sc_blinking: 1,	/* LED blink operation active */
				sc_mcastkey: 1,	/* mcast key cache search */
				sc_syncbeacon:1,/* sync/resync beacon timers */
				sc_hasclrkey:1;	/* CLR key supported */
						/* rate tables */
	const HAL_RATE_TABLE	*sc_rates[IEEE80211_MODE_MAX];
	const HAL_RATE_TABLE	*sc_currates;	/* current rate table */
	enum ieee80211_phymode	sc_curmode;	/* current phy mode */
	u_int16_t		sc_curtxpow;	/* current tx power limit */
	HAL_CHANNEL		sc_curchan;	/* current h/w channel */
	u_int8_t		sc_rixmap[256];	/* IEEE to h/w rate table ix */
	struct {
		u_int8_t	ieeerate;	/* IEEE rate */
		u_int8_t	rxflags;	/* radiotap rx flags */
		u_int8_t	txflags;	/* radiotap tx flags */
		u_int16_t	ledon;		/* softled on time */
		u_int16_t	ledoff;		/* softled off time */
	} sc_hwmap[32];				/* h/w rate ix mappings */
	u_int8_t		sc_minrateix;	/* min h/w rate index */
	u_int8_t		sc_mcastrix;	/* mcast h/w rate index */
	u_int8_t		sc_protrix;	/* protection rate index */
	u_int			sc_mcastrate;	/* ieee rate for mcastrateix */
	u_int			sc_txantenna;	/* tx antenna (fixed or auto) */
	HAL_INT			sc_imask;	/* interrupt mask copy */
	u_int			sc_keymax;	/* size of key cache */
	u_int8_t		sc_keymap[ATH_KEYBYTES];/* key use bit map */

	u_int			sc_ledpin;	/* GPIO pin for driving LED */
	u_int			sc_ledon;	/* pin setting for LED on */
	u_int			sc_ledidle;	/* idle polling interval */
	int			sc_ledevent;	/* time of last LED event */
	u_int8_t		sc_rxrate;	/* current rx rate for LED */
	u_int8_t		sc_txrate;	/* current tx rate for LED */
	u_int16_t		sc_ledoff;	/* off time for current blink */
	struct callout		sc_ledtimer;	/* led off timer */

	struct bpf_if *		sc_drvbpf;
	union {
		struct ath_tx_radiotap_header th;
		u_int8_t	pad[64];
	} u_tx_rt;
	int			sc_tx_th_len;
	union {
		struct ath_rx_radiotap_header th;
		u_int8_t	pad[64];
	} u_rx_rt;
	int			sc_rx_th_len;

	ath_task_t		sc_fataltask;	/* fatal int processing */

	struct ath_descdma	sc_rxdma;	/* RX descriptos */
	ath_bufhead		sc_rxbuf;	/* receive buffer */
	u_int32_t		*sc_rxlink;	/* link ptr in last RX desc */
	ath_task_t		sc_rxtask;	/* rx int processing */
	ath_task_t		sc_rxorntask;	/* rxorn int processing */
	ath_task_t		sc_radartask;	/* radar processing */
	u_int8_t		sc_defant;	/* current default antenna */
	u_int8_t		sc_rxotherant;	/* rx's on non-default antenna*/
	u_int64_t		sc_lastrx;	/* tsf of last rx'd frame */

	struct ath_descdma	sc_txdma;	/* TX descriptors */
	ath_bufhead		sc_txbuf;	/* transmit buffer */
	ath_txbuf_lock_t	sc_txbuflock;	/* txbuf lock */
	u_int			sc_txqsetup;	/* h/w queues setup */
	u_int			sc_txintrperiod;/* tx interrupt batching */
	struct ath_txq		sc_txq[HAL_NUM_TX_QUEUES];
	struct ath_txq		*sc_ac2q[5];	/* WME AC -> h/w q map */
	ath_task_t		sc_txtask;	/* tx int processing */

	struct ath_descdma	sc_bdma;	/* beacon descriptors */
	ath_bufhead		sc_bbuf;	/* beacon buffers */
	u_int			sc_bhalq;	/* HAL q for outgoing beacons */
	u_int			sc_bmisscount;	/* missed beacon transmits */
	u_int32_t		sc_ant_tx[8];	/* recent tx frames/antenna */
	struct ath_txq		*sc_cabq;	/* tx q for cab frames */
	struct ieee80211_beacon_offsets sc_boff;/* dynamic update state */
	ath_task_t		sc_bmisstask;	/* bmiss int processing */
	ath_task_t		sc_bstucktask;	/* stuck beacon processing */
	enum {
		OK,				/* no change needed */
		UPDATE,				/* update pending */
		COMMIT				/* beacon sent, commit change */
	} sc_updateslot;			/* slot time update fsm */

	struct callout		sc_cal_ch;	/* callout handle for cals */
	int			sc_calinterval;	/* current polling interval */
	int			sc_caltries;	/* cals at current interval */
	HAL_NODE_STATS		sc_halstats;	/* station-mode rssi stats */
	struct callout		sc_scan_ch;	/* callout handle for scan */
	struct callout		sc_dfs_ch;	/* callout handle for dfs */
	u_int			sc_flags;	/* misc flags */
};
#define	sc_if			sc_ec.ec_if
#define	sc_tx_th		u_tx_rt.th
#define	sc_rx_th		u_rx_rt.th

#define	ATH_ATTACHED		0x0001		/* attach has succeeded */
#define	ATH_KEY_UPDATING	0x0002		/* key change in progress */

#define	ATH_TXQ_SETUP(sc, i)	((sc)->sc_txqsetup & (1<<i))

int	ath_attach(u_int16_t, struct ath_softc *);
int	ath_detach(struct ath_softc *);
int	ath_activate(device_t, enum devact);
bool	ath_resume(struct ath_softc *);
void	ath_suspend(struct ath_softc *);
int	ath_intr(void *);
int	ath_reset(struct ifnet *);
void	ath_sysctlattach(struct ath_softc *);

extern int ath_dwelltime;
extern int ath_calinterval;
extern int ath_outdoor;
extern int ath_xchanmode;
extern int ath_countrycode;
extern int ath_regdomain;
extern int ath_debug;
extern int ath_rxbuf;
extern int ath_txbuf;

/*
 * HAL definitions to comply with local coding convention.
 */
#define	ath_hal_detach(_ah) \
	((*(_ah)->ah_detach)((_ah)))
#define	ath_hal_reset(_ah, _opmode, _chan, _outdoor, _pstatus) \
	((*(_ah)->ah_reset)((_ah), (_opmode), (_chan), (_outdoor), (_pstatus)))
#define	ath_hal_getratetable(_ah, _mode) \
	((*(_ah)->ah_getRateTable)((_ah), (_mode)))
#define	ath_hal_getmac(_ah, _mac) \
	((*(_ah)->ah_getMacAddress)((_ah), (_mac)))
#define	ath_hal_setmac(_ah, _mac) \
	((*(_ah)->ah_setMacAddress)((_ah), (_mac)))
#define	ath_hal_intrset(_ah, _mask) \
	((*(_ah)->ah_setInterrupts)((_ah), (_mask)))
#define	ath_hal_intrget(_ah) \
	((*(_ah)->ah_getInterrupts)((_ah)))
#define	ath_hal_intrpend(_ah) \
	((*(_ah)->ah_isInterruptPending)((_ah)))
#define	ath_hal_getisr(_ah, _pmask) \
	((*(_ah)->ah_getPendingInterrupts)((_ah), (_pmask)))
#define	ath_hal_updatetxtriglevel(_ah, _inc) \
	((*(_ah)->ah_updateTxTrigLevel)((_ah), (_inc)))
#define	ath_hal_setpower(_ah, _mode) \
	((*(_ah)->ah_setPowerMode)((_ah), (_mode), AH_TRUE))
#define	ath_hal_keycachesize(_ah) \
	((*(_ah)->ah_getKeyCacheSize)((_ah)))
#define	ath_hal_keyreset(_ah, _ix) \
	((*(_ah)->ah_resetKeyCacheEntry)((_ah), (_ix)))
#define	ath_hal_keyset(_ah, _ix, _pk, _mac) \
	((*(_ah)->ah_setKeyCacheEntry)((_ah), (_ix), (_pk), (_mac), AH_FALSE))
#define	ath_hal_keyisvalid(_ah, _ix) \
	(((*(_ah)->ah_isKeyCacheEntryValid)((_ah), (_ix))))
#define	ath_hal_keysetmac(_ah, _ix, _mac) \
	((*(_ah)->ah_setKeyCacheEntryMac)((_ah), (_ix), (_mac)))
#define	ath_hal_getrxfilter(_ah) \
	((*(_ah)->ah_getRxFilter)((_ah)))
#define	ath_hal_setrxfilter(_ah, _filter) \
	((*(_ah)->ah_setRxFilter)((_ah), (_filter)))
#define	ath_hal_setmcastfilter(_ah, _mfilt0, _mfilt1) \
	((*(_ah)->ah_setMulticastFilter)((_ah), (_mfilt0), (_mfilt1)))
#define	ath_hal_waitforbeacon(_ah, _bf) \
	((*(_ah)->ah_waitForBeaconDone)((_ah), (_bf)->bf_daddr))
#define	ath_hal_putrxbuf(_ah, _bufaddr) \
	((*(_ah)->ah_setRxDP)((_ah), (_bufaddr)))
#define	ath_hal_gettsf32(_ah) \
	((*(_ah)->ah_getTsf32)((_ah)))
#define	ath_hal_gettsf64(_ah) \
	((*(_ah)->ah_getTsf64)((_ah)))
#define	ath_hal_resettsf(_ah) \
	((*(_ah)->ah_resetTsf)((_ah)))
#define	ath_hal_rxena(_ah) \
	((*(_ah)->ah_enableReceive)((_ah)))
#define	ath_hal_puttxbuf(_ah, _q, _bufaddr) \
	((*(_ah)->ah_setTxDP)((_ah), (_q), (_bufaddr)))
#define	ath_hal_gettxbuf(_ah, _q) \
	((*(_ah)->ah_getTxDP)((_ah), (_q)))
#define	ath_hal_numtxpending(_ah, _q) \
	((*(_ah)->ah_numTxPending)((_ah), (_q)))
#define	ath_hal_getrxbuf(_ah) \
	((*(_ah)->ah_getRxDP)((_ah)))
#define	ath_hal_txstart(_ah, _q) \
	((*(_ah)->ah_startTxDma)((_ah), (_q)))
#define	ath_hal_setchannel(_ah, _chan) \
	((*(_ah)->ah_setChannel)((_ah), (_chan)))
#define	ath_hal_calibrate(_ah, _chan, _iqcal) \
	((*(_ah)->ah_perCalibration)((_ah), (_chan), (_iqcal)))
#define	ath_hal_setledstate(_ah, _state) \
	((*(_ah)->ah_setLedState)((_ah), (_state)))
#define	ath_hal_beaconinit(_ah, _nextb, _bperiod) \
	((*(_ah)->ah_beaconInit)((_ah), (_nextb), (_bperiod)))
#define	ath_hal_beaconreset(_ah) \
	((*(_ah)->ah_resetStationBeaconTimers)((_ah)))
#define	ath_hal_beacontimers(_ah, _bs) \
	((*(_ah)->ah_setStationBeaconTimers)((_ah), (_bs)))
#define	ath_hal_setassocid(_ah, _bss, _associd) \
	((*(_ah)->ah_writeAssocid)((_ah), (_bss), (_associd)))
#define	ath_hal_phydisable(_ah) \
	((*(_ah)->ah_phyDisable)((_ah)))
#define	ath_hal_setopmode(_ah) \
	((*(_ah)->ah_setPCUConfig)((_ah)))
#define	ath_hal_stoptxdma(_ah, _qnum) \
	((*(_ah)->ah_stopTxDma)((_ah), (_qnum)))
#define	ath_hal_stoppcurecv(_ah) \
	((*(_ah)->ah_stopPcuReceive)((_ah)))
#define	ath_hal_startpcurecv(_ah) \
	((*(_ah)->ah_startPcuReceive)((_ah)))
#define	ath_hal_stopdmarecv(_ah) \
	((*(_ah)->ah_stopDmaReceive)((_ah)))
#define	ath_hal_getdiagstate(_ah, _id, _indata, _insize, _outdata, _outsize) \
	((*(_ah)->ah_getDiagState)((_ah), (_id), \
		(_indata), (_insize), (_outdata), (_outsize)))
#define	ath_hal_setuptxqueue(_ah, _type, _irq) \
	((*(_ah)->ah_setupTxQueue)((_ah), (_type), (_irq)))
#define	ath_hal_resettxqueue(_ah, _q) \
	((*(_ah)->ah_resetTxQueue)((_ah), (_q)))
#define	ath_hal_releasetxqueue(_ah, _q) \
	((*(_ah)->ah_releaseTxQueue)((_ah), (_q)))
#define	ath_hal_gettxqueueprops(_ah, _q, _qi) \
	((*(_ah)->ah_getTxQueueProps)((_ah), (_q), (_qi)))
#define	ath_hal_settxqueueprops(_ah, _q, _qi) \
	((*(_ah)->ah_setTxQueueProps)((_ah), (_q), (_qi)))
#define	ath_hal_getrfgain(_ah) \
	((*(_ah)->ah_getRfGain)((_ah)))
#define	ath_hal_getdefantenna(_ah) \
	((*(_ah)->ah_getDefAntenna)((_ah)))
#define	ath_hal_setdefantenna(_ah, _ant) \
	((*(_ah)->ah_setDefAntenna)((_ah), (_ant)))
#define	ath_hal_rxmonitor(_ah, _arg, _chan) \
	((*(_ah)->ah_rxMonitor)((_ah), (_arg), (_chan)))
#define	ath_hal_mibevent(_ah, _stats) \
	((*(_ah)->ah_procMibEvent)((_ah), (_stats)))
#define	ath_hal_setslottime(_ah, _us) \
	((*(_ah)->ah_setSlotTime)((_ah), (_us)))
#define	ath_hal_getslottime(_ah) \
	((*(_ah)->ah_getSlotTime)((_ah)))
#define	ath_hal_setacktimeout(_ah, _us) \
	((*(_ah)->ah_setAckTimeout)((_ah), (_us)))
#define	ath_hal_getacktimeout(_ah) \
	((*(_ah)->ah_getAckTimeout)((_ah)))
#define	ath_hal_setctstimeout(_ah, _us) \
	((*(_ah)->ah_setCTSTimeout)((_ah), (_us)))
#define	ath_hal_getctstimeout(_ah) \
	((*(_ah)->ah_getCTSTimeout)((_ah)))
#define	ath_hal_getcapability(_ah, _cap, _param, _result) \
	((*(_ah)->ah_getCapability)((_ah), (_cap), (_param), (_result)))
#define	ath_hal_setcapability(_ah, _cap, _param, _v, _status) \
	((*(_ah)->ah_setCapability)((_ah), (_cap), (_param), (_v), (_status)))
#define	ath_hal_ciphersupported(_ah, _cipher) \
	(ath_hal_getcapability(_ah, HAL_CAP_CIPHER, _cipher, NULL) == HAL_OK)
#define	ath_hal_getregdomain(_ah, _prd) \
	(ath_hal_getcapability(_ah, HAL_CAP_REG_DMN, 0, (_prd)) == HAL_OK)
#define	ath_hal_setregdomain(_ah, _rd) \
	ath_hal_setcapability(_ah, HAL_CAP_REG_DMN, 0, _rd, NULL)
#define	ath_hal_getcountrycode(_ah, _pcc) \
	(*(_pcc) = (_ah)->ah_countryCode)
#define	ath_hal_gettkipmic(_ah) \
	(ath_hal_getcapability(_ah, HAL_CAP_TKIP_MIC, 1, NULL) == HAL_OK)
#define	ath_hal_settkipmic(_ah, _v) \
	ath_hal_setcapability(_ah, HAL_CAP_TKIP_MIC, 1, _v, NULL)
#define	ath_hal_hastkipsplit(_ah) \
	(ath_hal_getcapability(_ah, HAL_CAP_TKIP_SPLIT, 0, NULL) == HAL_OK)
#define	ath_hal_gettkipsplit(_ah) \
	(ath_hal_getcapability(_ah, HAL_CAP_TKIP_SPLIT, 1, NULL) == HAL_OK)
#define	ath_hal_settkipsplit(_ah, _v) \
	ath_hal_setcapability(_ah, HAL_CAP_TKIP_SPLIT, 1, _v, NULL)
#define	ath_hal_haswmetkipmic(_ah) \
	(ath_hal_getcapability(_ah, HAL_CAP_WME_TKIPMIC, 0, NULL) == HAL_OK)
#define	ath_hal_hwphycounters(_ah) \
	(ath_hal_getcapability(_ah, HAL_CAP_PHYCOUNTERS, 0, NULL) == HAL_OK)
#define	ath_hal_hasdiversity(_ah) \
	(ath_hal_getcapability(_ah, HAL_CAP_DIVERSITY, 0, NULL) == HAL_OK)
#define	ath_hal_getdiversity(_ah) \
	(ath_hal_getcapability(_ah, HAL_CAP_DIVERSITY, 1, NULL) == HAL_OK)
#define	ath_hal_setdiversity(_ah, _v) \
	ath_hal_setcapability(_ah, HAL_CAP_DIVERSITY, 1, _v, NULL)
#define	ath_hal_getdiag(_ah, _pv) \
	(ath_hal_getcapability(_ah, HAL_CAP_DIAG, 0, _pv) == HAL_OK)
#define	ath_hal_setdiag(_ah, _v) \
	ath_hal_setcapability(_ah, HAL_CAP_DIAG, 0, _v, NULL)
#define	ath_hal_getnumtxqueues(_ah, _pv) \
	(ath_hal_getcapability(_ah, HAL_CAP_NUM_TXQUEUES, 0, _pv) == HAL_OK)
#define	ath_hal_hasveol(_ah) \
	(ath_hal_getcapability(_ah, HAL_CAP_VEOL, 0, NULL) == HAL_OK)
#define	ath_hal_hastxpowlimit(_ah) \
	(ath_hal_getcapability(_ah, HAL_CAP_TXPOW, 0, NULL) == HAL_OK)
#define	ath_hal_settxpowlimit(_ah, _pow) \
	((*(_ah)->ah_setTxPowerLimit)((_ah), (_pow)))
#define	ath_hal_gettxpowlimit(_ah, _ppow) \
	(ath_hal_getcapability(_ah, HAL_CAP_TXPOW, 1, _ppow) == HAL_OK)
#define	ath_hal_getmaxtxpow(_ah, _ppow) \
	(ath_hal_getcapability(_ah, HAL_CAP_TXPOW, 2, _ppow) == HAL_OK)
#define	ath_hal_gettpscale(_ah, _scale) \
	(ath_hal_getcapability(_ah, HAL_CAP_TXPOW, 3, _scale) == HAL_OK)
#define	ath_hal_settpscale(_ah, _v) \
	ath_hal_setcapability(_ah, HAL_CAP_TXPOW, 3, _v, NULL)
#define	ath_hal_hastpc(_ah) \
	(ath_hal_getcapability(_ah, HAL_CAP_TPC, 0, NULL) == HAL_OK)
#define	ath_hal_gettpc(_ah) \
	(ath_hal_getcapability(_ah, HAL_CAP_TPC, 1, NULL) == HAL_OK)
#define	ath_hal_settpc(_ah, _v) \
	ath_hal_setcapability(_ah, HAL_CAP_TPC, 1, _v, NULL)
#define	ath_hal_hasbursting(_ah) \
	(ath_hal_getcapability(_ah, HAL_CAP_BURST, 0, NULL) == HAL_OK)
#ifdef notyet
#define	ath_hal_hasmcastkeysearch(_ah) \
	(ath_hal_getcapability(_ah, HAL_CAP_MCAST_KEYSRCH, 0, NULL) == HAL_OK)
#define	ath_hal_getmcastkeysearch(_ah) \
	(ath_hal_getcapability(_ah, HAL_CAP_MCAST_KEYSRCH, 1, NULL) == HAL_OK)
#else
#define	ath_hal_getmcastkeysearch(_ah)	0
#endif
#define	ath_hal_hasrfsilent(_ah) \
	(ath_hal_getcapability(_ah, HAL_CAP_RFSILENT, 0, NULL) == HAL_OK)
#define	ath_hal_getrfkill(_ah) \
	(ath_hal_getcapability(_ah, HAL_CAP_RFSILENT, 1, NULL) == HAL_OK)
#define	ath_hal_setrfkill(_ah, _onoff) \
	ath_hal_setcapability(_ah, HAL_CAP_RFSILENT, 1, _onoff, NULL)
#define	ath_hal_getrfsilent(_ah, _prfsilent) \
	(ath_hal_getcapability(_ah, HAL_CAP_RFSILENT, 2, _prfsilent) == HAL_OK)
#define	ath_hal_setrfsilent(_ah, _rfsilent) \
	ath_hal_setcapability(_ah, HAL_CAP_RFSILENT, 2, _rfsilent, NULL)
#define	ath_hal_gettpack(_ah, _ptpack) \
	(ath_hal_getcapability(_ah, HAL_CAP_TPC_ACK, 0, _ptpack) == HAL_OK)
#define	ath_hal_settpack(_ah, _tpack) \
	ath_hal_setcapability(_ah, HAL_CAP_TPC_ACK, 0, _tpack, NULL)
#define	ath_hal_gettpcts(_ah, _ptpcts) \
	(ath_hal_getcapability(_ah, HAL_CAP_TPC_CTS, 0, _ptpcts) == HAL_OK)
#define	ath_hal_settpcts(_ah, _tpcts) \
	ath_hal_setcapability(_ah, HAL_CAP_TPC_CTS, 0, _tpcts, NULL)
#define	ath_hal_getchannoise(_ah, _c) \
	((*(_ah)->ah_getChanNoise)((_ah), (_c)))

#define	ath_hal_setuprxdesc(_ah, _ds, _size, _intreq) \
	((*(_ah)->ah_setupRxDesc)((_ah), (_ds), (_size), (_intreq)))
#if 0
#define	ath_hal_rxprocdesc(_ah, _ds, _dspa, _dsnext, tsf, a5) \
	((*(_ah)->ah_procRxDesc)((_ah), (_ds), (_dspa), (_dsnext), (tsf), (a5)))
#else
#define	ath_hal_rxprocdesc(_ah, _ds, _dspa, _dsnext, _rs) \
	((*(_ah)->ah_procRxDesc)((_ah), (_ds), (_dspa), (_dsnext), 0, (_rs)))
#endif
#define	ath_hal_setuptxdesc(_ah, _ds, _plen, _hlen, _atype, _txpow, \
		_txr0, _txtr0, _keyix, _ant, _flags, \
		_rtsrate, _rtsdura) \
	((*(_ah)->ah_setupTxDesc)((_ah), (_ds), (_plen), (_hlen), (_atype), \
		(_txpow), (_txr0), (_txtr0), (_keyix), (_ant), \
		(_flags), (_rtsrate), (_rtsdura), 0, 0, 0))
#define	ath_hal_setupxtxdesc(_ah, _ds, \
		_txr1, _txtr1, _txr2, _txtr2, _txr3, _txtr3) \
	((*(_ah)->ah_setupXTxDesc)((_ah), (_ds), \
		(_txr1), (_txtr1), (_txr2), (_txtr2), (_txr3), (_txtr3)))
#define	ath_hal_filltxdesc(_ah, _ds, _l, _first, _last, _ds0) \
	((*(_ah)->ah_fillTxDesc)((_ah), (_ds), (_l), (_first), (_last), (_ds0)))
#define	ath_hal_txprocdesc(_ah, _ds, _ts) \
	((*(_ah)->ah_procTxDesc)((_ah), (_ds), (_ts)))
#define	ath_hal_gettxintrtxqs(_ah, _txqs) \
	((*(_ah)->ah_getTxIntrQueue)((_ah), (_txqs)))

#define ath_hal_gpioCfgOutput(_ah, _gpio, _type) \
        ((*(_ah)->ah_gpioCfgOutput)((_ah), (_gpio), (_type)))
#define ath_hal_gpioset(_ah, _gpio, _b) \
        ((*(_ah)->ah_gpioSet)((_ah), (_gpio), (_b)))

#define ath_hal_radar_event(_ah) \
	((*(_ah)->ah_radarHaveEvent)((_ah)))
#define ath_hal_procdfs(_ah, _chan) \
	((*(_ah)->ah_processDfs)((_ah), (_chan)))
#define ath_hal_checknol(_ah, _chan, _nchans) \
	((*(_ah)->ah_dfsNolCheck)((_ah), (_chan), (_nchans)))

#endif /* _DEV_ATH_ATHVAR_H */

/*	$NetBSD: ath.c,v 1.117 2014/10/18 08:33:27 snj Exp $	*/

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
 */

#include <sys/cdefs.h>
#ifdef __FreeBSD__
__FBSDID("$FreeBSD: src/sys/dev/ath/if_ath.c,v 1.104 2005/09/16 10:09:23 ru Exp $");
#endif
#ifdef __NetBSD__
__KERNEL_RCSID(0, "$NetBSD: ath.c,v 1.117 2014/10/18 08:33:27 snj Exp $");
#endif

/*
 * Driver for the Atheros Wireless LAN controller.
 *
 * This software is derived from work of Atsushi Onoe; his contribution
 * is greatly appreciated.
 */

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#endif

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/errno.h>
#include <sys/callout.h>
#include <sys/bus.h>
#include <sys/endian.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_arp.h>
#include <net/if_ether.h>
#include <net/if_llc.h>

#include <net80211/ieee80211_netbsd.h>
#include <net80211/ieee80211_var.h>

#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h>
#endif

#include <sys/device.h>
#include <dev/ic/ath_netbsd.h>

#define	AR_DEBUG
#include <dev/ic/athvar.h>
#include "ah_desc.h"
#include "ah_devid.h"	/* XXX for softled */
#include "opt_ah.h"

#ifdef ATH_TX99_DIAG
#include <dev/ath/ath_tx99/ath_tx99.h>
#endif

/* unaligned little endian access */
#define LE_READ_2(p)							\
	((u_int16_t)							\
	 ((((u_int8_t *)(p))[0]      ) | (((u_int8_t *)(p))[1] <<  8)))
#define LE_READ_4(p)							\
	((u_int32_t)							\
	 ((((u_int8_t *)(p))[0]      ) | (((u_int8_t *)(p))[1] <<  8) |	\
	  (((u_int8_t *)(p))[2] << 16) | (((u_int8_t *)(p))[3] << 24)))

enum {
	ATH_LED_TX,
	ATH_LED_RX,
	ATH_LED_POLL,
};

#ifdef	AH_NEED_DESC_SWAP
#define	HTOAH32(x)	htole32(x)
#else
#define	HTOAH32(x)	(x)
#endif

static int	ath_ifinit(struct ifnet *);
static int	ath_init(struct ath_softc *);
static void	ath_stop_locked(struct ifnet *, int);
static void	ath_stop(struct ifnet *, int);
static void	ath_start(struct ifnet *);
static int	ath_media_change(struct ifnet *);
static void	ath_watchdog(struct ifnet *);
static int	ath_ioctl(struct ifnet *, u_long, void *);
static void	ath_fatal_proc(void *, int);
static void	ath_rxorn_proc(void *, int);
static void	ath_bmiss_proc(void *, int);
static void	ath_radar_proc(void *, int);
static int	ath_key_alloc(struct ieee80211com *,
			const struct ieee80211_key *,
			ieee80211_keyix *, ieee80211_keyix *);
static int	ath_key_delete(struct ieee80211com *,
			const struct ieee80211_key *);
static int	ath_key_set(struct ieee80211com *, const struct ieee80211_key *,
			const u_int8_t mac[IEEE80211_ADDR_LEN]);
static void	ath_key_update_begin(struct ieee80211com *);
static void	ath_key_update_end(struct ieee80211com *);
static void	ath_mode_init(struct ath_softc *);
static void	ath_setslottime(struct ath_softc *);
static void	ath_updateslot(struct ifnet *);
static int	ath_beaconq_setup(struct ath_hal *);
static int	ath_beacon_alloc(struct ath_softc *, struct ieee80211_node *);
static void	ath_beacon_setup(struct ath_softc *, struct ath_buf *);
static void	ath_beacon_proc(void *, int);
static void	ath_bstuck_proc(void *, int);
static void	ath_beacon_free(struct ath_softc *);
static void	ath_beacon_config(struct ath_softc *);
static void	ath_descdma_cleanup(struct ath_softc *sc,
			struct ath_descdma *, ath_bufhead *);
static int	ath_desc_alloc(struct ath_softc *);
static void	ath_desc_free(struct ath_softc *);
static struct ieee80211_node *ath_node_alloc(struct ieee80211_node_table *);
static void	ath_node_free(struct ieee80211_node *);
static u_int8_t	ath_node_getrssi(const struct ieee80211_node *);
static int	ath_rxbuf_init(struct ath_softc *, struct ath_buf *);
static void	ath_recv_mgmt(struct ieee80211com *ic, struct mbuf *m,
			struct ieee80211_node *ni,
			int subtype, int rssi, u_int32_t rstamp);
static void	ath_setdefantenna(struct ath_softc *, u_int);
static void	ath_rx_proc(void *, int);
static struct ath_txq *ath_txq_setup(struct ath_softc*, int qtype, int subtype);
static int	ath_tx_setup(struct ath_softc *, int, int);
static int	ath_wme_update(struct ieee80211com *);
static void	ath_tx_cleanupq(struct ath_softc *, struct ath_txq *);
static void	ath_tx_cleanup(struct ath_softc *);
static int	ath_tx_start(struct ath_softc *, struct ieee80211_node *,
			     struct ath_buf *, struct mbuf *);
static void	ath_tx_proc_q0(void *, int);
static void	ath_tx_proc_q0123(void *, int);
static void	ath_tx_proc(void *, int);
static int	ath_chan_set(struct ath_softc *, struct ieee80211_channel *);
static void	ath_draintxq(struct ath_softc *);
static void	ath_stoprecv(struct ath_softc *);
static int	ath_startrecv(struct ath_softc *);
static void	ath_chan_change(struct ath_softc *, struct ieee80211_channel *);
static void	ath_next_scan(void *);
static void	ath_calibrate(void *);
static int	ath_newstate(struct ieee80211com *, enum ieee80211_state, int);
static void	ath_setup_stationkey(struct ieee80211_node *);
static void	ath_newassoc(struct ieee80211_node *, int);
static int	ath_getchannels(struct ath_softc *, u_int cc,
			HAL_BOOL outdoor, HAL_BOOL xchanmode);
static void	ath_led_event(struct ath_softc *, int);
static void	ath_update_txpow(struct ath_softc *);
static void	ath_freetx(struct mbuf *);
static void	ath_restore_diversity(struct ath_softc *);

static int	ath_rate_setup(struct ath_softc *, u_int mode);
static void	ath_setcurmode(struct ath_softc *, enum ieee80211_phymode);

static void	ath_bpfattach(struct ath_softc *);
static void	ath_announce(struct ath_softc *);

int ath_dwelltime = 200;		/* 5 channels/second */
int ath_calinterval = 30;		/* calibrate every 30 secs */
int ath_outdoor = AH_TRUE;		/* outdoor operation */
int ath_xchanmode = AH_TRUE;		/* enable extended channels */
int ath_countrycode = CTRY_DEFAULT;	/* country code */
int ath_regdomain = 0;			/* regulatory domain */
int ath_debug = 0;
int ath_rxbuf = ATH_RXBUF;		/* # rx buffers to allocate */
int ath_txbuf = ATH_TXBUF;		/* # tx buffers to allocate */

#ifdef AR_DEBUG
enum {
	ATH_DEBUG_XMIT		= 0x00000001,	/* basic xmit operation */
	ATH_DEBUG_XMIT_DESC	= 0x00000002,	/* xmit descriptors */
	ATH_DEBUG_RECV		= 0x00000004,	/* basic recv operation */
	ATH_DEBUG_RECV_DESC	= 0x00000008,	/* recv descriptors */
	ATH_DEBUG_RATE		= 0x00000010,	/* rate control */
	ATH_DEBUG_RESET		= 0x00000020,	/* reset processing */
	ATH_DEBUG_MODE		= 0x00000040,	/* mode init/setup */
	ATH_DEBUG_BEACON 	= 0x00000080,	/* beacon handling */
	ATH_DEBUG_WATCHDOG 	= 0x00000100,	/* watchdog timeout */
	ATH_DEBUG_INTR		= 0x00001000,	/* ISR */
	ATH_DEBUG_TX_PROC	= 0x00002000,	/* tx ISR proc */
	ATH_DEBUG_RX_PROC	= 0x00004000,	/* rx ISR proc */
	ATH_DEBUG_BEACON_PROC	= 0x00008000,	/* beacon ISR proc */
	ATH_DEBUG_CALIBRATE	= 0x00010000,	/* periodic calibration */
	ATH_DEBUG_KEYCACHE	= 0x00020000,	/* key cache management */
	ATH_DEBUG_STATE		= 0x00040000,	/* 802.11 state transitions */
	ATH_DEBUG_NODE		= 0x00080000,	/* node management */
	ATH_DEBUG_LED		= 0x00100000,	/* led management */
	ATH_DEBUG_FF		= 0x00200000,	/* fast frames */
	ATH_DEBUG_DFS		= 0x00400000,	/* DFS processing */
	ATH_DEBUG_FATAL		= 0x80000000,	/* fatal errors */
	ATH_DEBUG_ANY		= 0xffffffff
};
#define	IFF_DUMPPKTS(sc, m) \
	((sc->sc_debug & (m)) || \
	    (sc->sc_if.if_flags & (IFF_DEBUG|IFF_LINK2)) == (IFF_DEBUG|IFF_LINK2))
#define	DPRINTF(sc, m, fmt, ...) do {				\
	if (sc->sc_debug & (m))					\
		printf(fmt, __VA_ARGS__);			\
} while (0)
#define	KEYPRINTF(sc, ix, hk, mac) do {				\
	if (sc->sc_debug & ATH_DEBUG_KEYCACHE)			\
		ath_keyprint(__func__, ix, hk, mac);		\
} while (0)
static	void ath_printrxbuf(struct ath_buf *bf, int);
static	void ath_printtxbuf(struct ath_buf *bf, int);
#else
#define        IFF_DUMPPKTS(sc, m) \
	((sc->sc_if.if_flags & (IFF_DEBUG|IFF_LINK2)) == (IFF_DEBUG|IFF_LINK2))
#define        DPRINTF(m, fmt, ...)
#define        KEYPRINTF(sc, k, ix, mac)
#endif

MALLOC_DEFINE(M_ATHDEV, "athdev", "ath driver dma buffers");

int
ath_attach(u_int16_t devid, struct ath_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = NULL;
	HAL_STATUS status;
	int error = 0, i;

	DPRINTF(sc, ATH_DEBUG_ANY, "%s: devid 0x%x\n", __func__, devid);

	pmf_self_suspensor_init(sc->sc_dev, &sc->sc_suspensor, &sc->sc_qual);

	memcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);

	ah = ath_hal_attach(devid, sc, sc->sc_st, sc->sc_sh, &status);
	if (ah == NULL) {
		if_printf(ifp, "unable to attach hardware; HAL status %u\n",
			status);
		error = ENXIO;
		goto bad;
	}
	if (ah->ah_abi != HAL_ABI_VERSION) {
		if_printf(ifp, "HAL ABI mismatch detected "
			"(HAL:0x%x != driver:0x%x)\n",
			ah->ah_abi, HAL_ABI_VERSION);
		error = ENXIO;
		goto bad;
	}
	sc->sc_ah = ah;

	if (!prop_dictionary_set_bool(device_properties(sc->sc_dev),
	    "pmf-powerdown", false))
		goto bad;

	/*
	 * Check if the MAC has multi-rate retry support.
	 * We do this by trying to setup a fake extended
	 * descriptor.  MAC's that don't have support will
	 * return false w/o doing anything.  MAC's that do
	 * support it will return true w/o doing anything.
	 */
	sc->sc_mrretry = ath_hal_setupxtxdesc(ah, NULL, 0,0, 0,0, 0,0);

	/*
	 * Check if the device has hardware counters for PHY
	 * errors.  If so we need to enable the MIB interrupt
	 * so we can act on stat triggers.
	 */
	if (ath_hal_hwphycounters(ah))
		sc->sc_needmib = 1;

	/*
	 * Get the hardware key cache size.
	 */
	sc->sc_keymax = ath_hal_keycachesize(ah);
	if (sc->sc_keymax > ATH_KEYMAX) {
		if_printf(ifp, "Warning, using only %u of %u key cache slots\n",
			ATH_KEYMAX, sc->sc_keymax);
		sc->sc_keymax = ATH_KEYMAX;
	}
	/*
	 * Reset the key cache since some parts do not
	 * reset the contents on initial power up.
	 */
	for (i = 0; i < sc->sc_keymax; i++)
		ath_hal_keyreset(ah, i);
	/*
	 * Mark key cache slots associated with global keys
	 * as in use.  If we knew TKIP was not to be used we
	 * could leave the +32, +64, and +32+64 slots free.
	 * XXX only for splitmic.
	 */
	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		setbit(sc->sc_keymap, i);
		setbit(sc->sc_keymap, i+32);
		setbit(sc->sc_keymap, i+64);
		setbit(sc->sc_keymap, i+32+64);
	}

	/*
	 * Collect the channel list using the default country
	 * code and including outdoor channels.  The 802.11 layer
	 * is resposible for filtering this list based on settings
	 * like the phy mode.
	 */
	error = ath_getchannels(sc, ath_countrycode,
			ath_outdoor, ath_xchanmode);
	if (error != 0)
		goto bad;

	/*
	 * Setup rate tables for all potential media types.
	 */
	ath_rate_setup(sc, IEEE80211_MODE_11A);
	ath_rate_setup(sc, IEEE80211_MODE_11B);
	ath_rate_setup(sc, IEEE80211_MODE_11G);
	ath_rate_setup(sc, IEEE80211_MODE_TURBO_A);
	ath_rate_setup(sc, IEEE80211_MODE_TURBO_G);
	/* NB: setup here so ath_rate_update is happy */
	ath_setcurmode(sc, IEEE80211_MODE_11A);

	/*
	 * Allocate tx+rx descriptors and populate the lists.
	 */
	error = ath_desc_alloc(sc);
	if (error != 0) {
		if_printf(ifp, "failed to allocate descriptors: %d\n", error);
		goto bad;
	}
	ATH_CALLOUT_INIT(&sc->sc_scan_ch, debug_mpsafenet ? CALLOUT_MPSAFE : 0);
	ATH_CALLOUT_INIT(&sc->sc_cal_ch, CALLOUT_MPSAFE);
#if 0
	ATH_CALLOUT_INIT(&sc->sc_dfs_ch, CALLOUT_MPSAFE);
#endif

	ATH_TXBUF_LOCK_INIT(sc);

	TASK_INIT(&sc->sc_rxtask, 0, ath_rx_proc, sc);
	TASK_INIT(&sc->sc_rxorntask, 0, ath_rxorn_proc, sc);
	TASK_INIT(&sc->sc_fataltask, 0, ath_fatal_proc, sc);
	TASK_INIT(&sc->sc_bmisstask, 0, ath_bmiss_proc, sc);
	TASK_INIT(&sc->sc_bstucktask,0, ath_bstuck_proc, sc);
	TASK_INIT(&sc->sc_radartask, 0, ath_radar_proc, sc);

	/*
	 * Allocate hardware transmit queues: one queue for
	 * beacon frames and one data queue for each QoS
	 * priority.  Note that the hal handles reseting
	 * these queues at the needed time.
	 *
	 * XXX PS-Poll
	 */
	sc->sc_bhalq = ath_beaconq_setup(ah);
	if (sc->sc_bhalq == (u_int) -1) {
		if_printf(ifp, "unable to setup a beacon xmit queue!\n");
		error = EIO;
		goto bad2;
	}
	sc->sc_cabq = ath_txq_setup(sc, HAL_TX_QUEUE_CAB, 0);
	if (sc->sc_cabq == NULL) {
		if_printf(ifp, "unable to setup CAB xmit queue!\n");
		error = EIO;
		goto bad2;
	}
	/* NB: insure BK queue is the lowest priority h/w queue */
	if (!ath_tx_setup(sc, WME_AC_BK, HAL_WME_AC_BK)) {
		if_printf(ifp, "unable to setup xmit queue for %s traffic!\n",
			ieee80211_wme_acnames[WME_AC_BK]);
		error = EIO;
		goto bad2;
	}
	if (!ath_tx_setup(sc, WME_AC_BE, HAL_WME_AC_BE) ||
	    !ath_tx_setup(sc, WME_AC_VI, HAL_WME_AC_VI) ||
	    !ath_tx_setup(sc, WME_AC_VO, HAL_WME_AC_VO)) {
		/*
		 * Not enough hardware tx queues to properly do WME;
		 * just punt and assign them all to the same h/w queue.
		 * We could do a better job of this if, for example,
		 * we allocate queues when we switch from station to
		 * AP mode.
		 */
		if (sc->sc_ac2q[WME_AC_VI] != NULL)
			ath_tx_cleanupq(sc, sc->sc_ac2q[WME_AC_VI]);
		if (sc->sc_ac2q[WME_AC_BE] != NULL)
			ath_tx_cleanupq(sc, sc->sc_ac2q[WME_AC_BE]);
		sc->sc_ac2q[WME_AC_BE] = sc->sc_ac2q[WME_AC_BK];
		sc->sc_ac2q[WME_AC_VI] = sc->sc_ac2q[WME_AC_BK];
		sc->sc_ac2q[WME_AC_VO] = sc->sc_ac2q[WME_AC_BK];
	}

	/*
	 * Special case certain configurations.  Note the
	 * CAB queue is handled by these specially so don't
	 * include them when checking the txq setup mask.
	 */
	switch (sc->sc_txqsetup &~ (1<<sc->sc_cabq->axq_qnum)) {
	case 0x01:
		TASK_INIT(&sc->sc_txtask, 0, ath_tx_proc_q0, sc);
		break;
	case 0x0f:
		TASK_INIT(&sc->sc_txtask, 0, ath_tx_proc_q0123, sc);
		break;
	default:
		TASK_INIT(&sc->sc_txtask, 0, ath_tx_proc, sc);
		break;
	}

	/*
	 * Setup rate control.  Some rate control modules
	 * call back to change the anntena state so expose
	 * the necessary entry points.
	 * XXX maybe belongs in struct ath_ratectrl?
	 */
	sc->sc_setdefantenna = ath_setdefantenna;
	sc->sc_rc = ath_rate_attach(sc);
	if (sc->sc_rc == NULL) {
		error = EIO;
		goto bad2;
	}

	sc->sc_blinking = 0;
	sc->sc_ledstate = 1;
	sc->sc_ledon = 0;			/* low true */
	sc->sc_ledidle = (2700*hz)/1000;	/* 2.7sec */
	ATH_CALLOUT_INIT(&sc->sc_ledtimer, CALLOUT_MPSAFE);
	/*
	 * Auto-enable soft led processing for IBM cards and for
	 * 5211 minipci cards.  Users can also manually enable/disable
	 * support with a sysctl.
	 */
	sc->sc_softled = (devid == AR5212_DEVID_IBM || devid == AR5211_DEVID);
	if (sc->sc_softled) {
		ath_hal_gpioCfgOutput(ah, sc->sc_ledpin,
		    HAL_GPIO_MUX_MAC_NETWORK_LED);
		ath_hal_gpioset(ah, sc->sc_ledpin, !sc->sc_ledon);
	}

	ifp->if_softc = sc;
	ifp->if_flags = IFF_SIMPLEX | IFF_BROADCAST | IFF_MULTICAST;
	ifp->if_start = ath_start;
	ifp->if_stop = ath_stop;
	ifp->if_watchdog = ath_watchdog;
	ifp->if_ioctl = ath_ioctl;
	ifp->if_init = ath_ifinit;
	IFQ_SET_READY(&ifp->if_snd);

	ic->ic_ifp = ifp;
	ic->ic_reset = ath_reset;
	ic->ic_newassoc = ath_newassoc;
	ic->ic_updateslot = ath_updateslot;
	ic->ic_wme.wme_update = ath_wme_update;
	/* XXX not right but it's not used anywhere important */
	ic->ic_phytype = IEEE80211_T_OFDM;
	ic->ic_opmode = IEEE80211_M_STA;
	ic->ic_caps =
		  IEEE80211_C_IBSS		/* ibss, nee adhoc, mode */
		| IEEE80211_C_HOSTAP		/* hostap mode */
		| IEEE80211_C_MONITOR		/* monitor mode */
		| IEEE80211_C_SHPREAMBLE	/* short preamble supported */
		| IEEE80211_C_SHSLOT		/* short slot time supported */
		| IEEE80211_C_WPA		/* capable of WPA1+WPA2 */
		| IEEE80211_C_TXFRAG		/* handle tx frags */
		;
	/*
	 * Query the hal to figure out h/w crypto support.
	 */
	if (ath_hal_ciphersupported(ah, HAL_CIPHER_WEP))
		ic->ic_caps |= IEEE80211_C_WEP;
	if (ath_hal_ciphersupported(ah, HAL_CIPHER_AES_OCB))
		ic->ic_caps |= IEEE80211_C_AES;
	if (ath_hal_ciphersupported(ah, HAL_CIPHER_AES_CCM))
		ic->ic_caps |= IEEE80211_C_AES_CCM;
	if (ath_hal_ciphersupported(ah, HAL_CIPHER_CKIP))
		ic->ic_caps |= IEEE80211_C_CKIP;
	if (ath_hal_ciphersupported(ah, HAL_CIPHER_TKIP)) {
		ic->ic_caps |= IEEE80211_C_TKIP;
		/*
		 * Check if h/w does the MIC and/or whether the
		 * separate key cache entries are required to
		 * handle both tx+rx MIC keys.
		 */
		if (ath_hal_ciphersupported(ah, HAL_CIPHER_MIC))
			ic->ic_caps |= IEEE80211_C_TKIPMIC;

		/*
		 * If the h/w supports storing tx+rx MIC keys
		 * in one cache slot automatically enable use.
		 */
		if (ath_hal_hastkipsplit(ah) ||
		    !ath_hal_settkipsplit(ah, AH_FALSE))
			sc->sc_splitmic = 1;

		/*
		 * If the h/w can do TKIP MIC together with WME then
		 * we use it; otherwise we force the MIC to be done
		 * in software by the net80211 layer.
		 */
		if (ath_hal_haswmetkipmic(ah))
			ic->ic_caps |= IEEE80211_C_WME_TKIPMIC;
	}
	sc->sc_hasclrkey = ath_hal_ciphersupported(ah, HAL_CIPHER_CLR);
	sc->sc_mcastkey = ath_hal_getmcastkeysearch(ah);
	/*
	 * Mark key cache slots associated with global keys
	 * as in use.  If we knew TKIP was not to be used we
	 * could leave the +32, +64, and +32+64 slots free.
	 */
	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		setbit(sc->sc_keymap, i);
		setbit(sc->sc_keymap, i+64);
		if (sc->sc_splitmic) {
			setbit(sc->sc_keymap, i+32);
			setbit(sc->sc_keymap, i+32+64);
		}
	}
	/*
	 * TPC support can be done either with a global cap or
	 * per-packet support.  The latter is not available on
	 * all parts.  We're a bit pedantic here as all parts
	 * support a global cap.
	 */
	if (ath_hal_hastpc(ah) || ath_hal_hastxpowlimit(ah))
		ic->ic_caps |= IEEE80211_C_TXPMGT;

	/*
	 * Mark WME capability only if we have sufficient
	 * hardware queues to do proper priority scheduling.
	 */
	if (sc->sc_ac2q[WME_AC_BE] != sc->sc_ac2q[WME_AC_BK])
		ic->ic_caps |= IEEE80211_C_WME;
	/*
	 * Check for misc other capabilities.
	 */
	if (ath_hal_hasbursting(ah))
		ic->ic_caps |= IEEE80211_C_BURST;

	/*
	 * Indicate we need the 802.11 header padded to a
	 * 32-bit boundary for 4-address and QoS frames.
	 */
	ic->ic_flags |= IEEE80211_F_DATAPAD;

	/*
	 * Query the hal about antenna support.
	 */
	sc->sc_defant = ath_hal_getdefantenna(ah);

	/*
	 * Not all chips have the VEOL support we want to
	 * use with IBSS beacons; check here for it.
	 */
	sc->sc_hasveol = ath_hal_hasveol(ah);

	/* get mac address from hardware */
	ath_hal_getmac(ah, ic->ic_myaddr);

	if_attach(ifp);
	/* call MI attach routine. */
	ieee80211_ifattach(ic);
	/* override default methods */
	ic->ic_node_alloc = ath_node_alloc;
	sc->sc_node_free = ic->ic_node_free;
	ic->ic_node_free = ath_node_free;
	ic->ic_node_getrssi = ath_node_getrssi;
	sc->sc_recv_mgmt = ic->ic_recv_mgmt;
	ic->ic_recv_mgmt = ath_recv_mgmt;
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = ath_newstate;
	ic->ic_crypto.cs_max_keyix = sc->sc_keymax;
	ic->ic_crypto.cs_key_alloc = ath_key_alloc;
	ic->ic_crypto.cs_key_delete = ath_key_delete;
	ic->ic_crypto.cs_key_set = ath_key_set;
	ic->ic_crypto.cs_key_update_begin = ath_key_update_begin;
	ic->ic_crypto.cs_key_update_end = ath_key_update_end;
	/* complete initialization */
	ieee80211_media_init(ic, ath_media_change, ieee80211_media_status);

	ath_bpfattach(sc);

	sc->sc_flags |= ATH_ATTACHED;

	/*
	 * Setup dynamic sysctl's now that country code and
	 * regdomain are available from the hal.
	 */
	ath_sysctlattach(sc);

	ieee80211_announce(ic);
	ath_announce(sc);
	return 0;
bad2:
	ath_tx_cleanup(sc);
	ath_desc_free(sc);
bad:
	if (ah)
		ath_hal_detach(ah);
	/* XXX don't get under the abstraction like this */
	sc->sc_dev->dv_flags &= ~DVF_ACTIVE;
	return error;
}

int
ath_detach(struct ath_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;
	int s;

	if ((sc->sc_flags & ATH_ATTACHED) == 0)
		return (0);

	DPRINTF(sc, ATH_DEBUG_ANY, "%s: if_flags %x\n",
		__func__, ifp->if_flags);

	s = splnet();
	ath_stop(ifp, 1);
	bpf_detach(ifp);
	/*
	 * NB: the order of these is important:
	 * o call the 802.11 layer before detaching the hal to
	 *   insure callbacks into the driver to delete global
	 *   key cache entries can be handled
	 * o reclaim the tx queue data structures after calling
	 *   the 802.11 layer as we'll get called back to reclaim
	 *   node state and potentially want to use them
	 * o to cleanup the tx queues the hal is called, so detach
	 *   it last
	 * Other than that, it's straightforward...
	 */
	ieee80211_ifdetach(&sc->sc_ic);
#ifdef ATH_TX99_DIAG
	if (sc->sc_tx99 != NULL)
		sc->sc_tx99->detach(sc->sc_tx99);
#endif
	ath_rate_detach(sc->sc_rc);
	ath_desc_free(sc);
	ath_tx_cleanup(sc);
	sysctl_teardown(&sc->sc_sysctllog);
	ath_hal_detach(sc->sc_ah);
	if_detach(ifp);
	splx(s);

	return 0;
}

void
ath_suspend(struct ath_softc *sc)
{
#if notyet
	/*
	 * Set the chip in full sleep mode.  Note that we are
	 * careful to do this only when bringing the interface
	 * completely to a stop.  When the chip is in this state
	 * it must be carefully woken up or references to
	 * registers in the PCI clock domain may freeze the bus
	 * (and system).  This varies by chip and is mostly an
	 * issue with newer parts that go to sleep more quickly.
	 */
	ath_hal_setpower(sc->sc_ah, HAL_PM_FULL_SLEEP);
#endif
}

bool
ath_resume(struct ath_softc *sc)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211com *ic = &sc->sc_ic;
	HAL_STATUS status;
	int i;

#if notyet
	ath_hal_setpower(ah, HAL_PM_AWAKE);
#else
	ath_hal_reset(ah, ic->ic_opmode, &sc->sc_curchan, AH_FALSE, &status);
#endif

	/*
	 * Reset the key cache since some parts do not
	 * reset the contents on initial power up.
	 */
	for (i = 0; i < sc->sc_keymax; i++)
		ath_hal_keyreset(ah, i);

	ath_hal_resettxqueue(ah, sc->sc_bhalq);
	for (i = 0; i < HAL_NUM_TX_QUEUES; i++)
		if (ATH_TXQ_SETUP(sc, i))
			ath_hal_resettxqueue(ah, i);

	if (sc->sc_softled) {
		ath_hal_gpioCfgOutput(sc->sc_ah, sc->sc_ledpin,
		    HAL_GPIO_MUX_MAC_NETWORK_LED);
		ath_hal_gpioset(sc->sc_ah, sc->sc_ledpin, !sc->sc_ledon);
	}
	return true;
}

/*
 * Interrupt handler.  Most of the actual processing is deferred.
 */
int
ath_intr(void *arg)
{
	struct ath_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_if;
	struct ath_hal *ah = sc->sc_ah;
	HAL_INT status = 0;

	if (!device_activation(sc->sc_dev, DEVACT_LEVEL_DRIVER)) {
		/*
		 * The hardware is not ready/present, don't touch anything.
		 * Note this can happen early on if the IRQ is shared.
		 */
		DPRINTF(sc, ATH_DEBUG_ANY, "%s: invalid; ignored\n", __func__);
		return 0;
	}

	if (!ath_hal_intrpend(ah))		/* shared irq, not for us */
		return 0;

	if ((ifp->if_flags & (IFF_RUNNING|IFF_UP)) != (IFF_RUNNING|IFF_UP)) {
		DPRINTF(sc, ATH_DEBUG_ANY, "%s: if_flags 0x%x\n",
			__func__, ifp->if_flags);
		ath_hal_getisr(ah, &status);	/* clear ISR */
		ath_hal_intrset(ah, 0);		/* disable further intr's */
		return 1; /* XXX */
	}
	/*
	 * Figure out the reason(s) for the interrupt.  Note
	 * that the hal returns a pseudo-ISR that may include
	 * bits we haven't explicitly enabled so we mask the
	 * value to insure we only process bits we requested.
	 */
	ath_hal_getisr(ah, &status);		/* NB: clears ISR too */
	DPRINTF(sc, ATH_DEBUG_INTR, "%s: status 0x%x\n", __func__, status);
	status &= sc->sc_imask;			/* discard unasked for bits */
	if (status & HAL_INT_FATAL) {
		/*
		 * Fatal errors are unrecoverable.  Typically
		 * these are caused by DMA errors.  Unfortunately
		 * the exact reason is not (presently) returned
		 * by the hal.
		 */
		sc->sc_stats.ast_hardware++;
		ath_hal_intrset(ah, 0);		/* disable intr's until reset */
		TASK_RUN_OR_ENQUEUE(&sc->sc_fataltask);
	} else if (status & HAL_INT_RXORN) {
		sc->sc_stats.ast_rxorn++;
		ath_hal_intrset(ah, 0);		/* disable intr's until reset */
		TASK_RUN_OR_ENQUEUE(&sc->sc_rxorntask);
	} else {
		if (status & HAL_INT_SWBA) {
			/*
			 * Software beacon alert--time to send a beacon.
			 * Handle beacon transmission directly; deferring
			 * this is too slow to meet timing constraints
			 * under load.
			 */
			ath_beacon_proc(sc, 0);
		}
		if (status & HAL_INT_RXEOL) {
			/*
			 * NB: the hardware should re-read the link when
			 *     RXE bit is written, but it doesn't work at
			 *     least on older hardware revs.
			 */
			sc->sc_stats.ast_rxeol++;
			sc->sc_rxlink = NULL;
		}
		if (status & HAL_INT_TXURN) {
			sc->sc_stats.ast_txurn++;
			/* bump tx trigger level */
			ath_hal_updatetxtriglevel(ah, AH_TRUE);
		}
		if (status & HAL_INT_RX)
			TASK_RUN_OR_ENQUEUE(&sc->sc_rxtask);
		if (status & HAL_INT_TX)
			TASK_RUN_OR_ENQUEUE(&sc->sc_txtask);
		if (status & HAL_INT_BMISS) {
			sc->sc_stats.ast_bmiss++;
			TASK_RUN_OR_ENQUEUE(&sc->sc_bmisstask);
		}
		if (status & HAL_INT_MIB) {
			sc->sc_stats.ast_mib++;
			/*
			 * Disable interrupts until we service the MIB
			 * interrupt; otherwise it will continue to fire.
			 */
			ath_hal_intrset(ah, 0);
			/*
			 * Let the hal handle the event.  We assume it will
			 * clear whatever condition caused the interrupt.
			 */
			ath_hal_mibevent(ah, &sc->sc_halstats);
			ath_hal_intrset(ah, sc->sc_imask);
		}
	}
	return 1;
}

/* Swap transmit descriptor.
 * if AH_NEED_DESC_SWAP flag is not defined this becomes a "null"
 * function.
 */
static inline void
ath_desc_swap(struct ath_desc *ds)
{
#ifdef AH_NEED_DESC_SWAP
	ds->ds_link = htole32(ds->ds_link);
	ds->ds_data = htole32(ds->ds_data);
	ds->ds_ctl0 = htole32(ds->ds_ctl0);
	ds->ds_ctl1 = htole32(ds->ds_ctl1);
	ds->ds_hw[0] = htole32(ds->ds_hw[0]);
	ds->ds_hw[1] = htole32(ds->ds_hw[1]);
#endif
}

static void
ath_fatal_proc(void *arg, int pending)
{
	struct ath_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_if;

	if_printf(ifp, "hardware error; resetting\n");
	ath_reset(ifp);
}

static void
ath_rxorn_proc(void *arg, int pending)
{
	struct ath_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_if;

	if_printf(ifp, "rx FIFO overrun; resetting\n");
	ath_reset(ifp);
}

static void
ath_bmiss_proc(void *arg, int pending)
{
	struct ath_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;

	DPRINTF(sc, ATH_DEBUG_ANY, "%s: pending %u\n", __func__, pending);
	KASSERTMSG(ic->ic_opmode == IEEE80211_M_STA,
		"unexpect operating mode %u", ic->ic_opmode);
	if (ic->ic_state == IEEE80211_S_RUN) {
		u_int64_t lastrx = sc->sc_lastrx;
		u_int64_t tsf = ath_hal_gettsf64(sc->sc_ah);

		DPRINTF(sc, ATH_DEBUG_BEACON,
		    "%s: tsf %" PRIu64 " lastrx %" PRId64
		    " (%" PRIu64 ") bmiss %u\n",
		    __func__, tsf, tsf - lastrx, lastrx,
		    ic->ic_bmisstimeout*1024);
		/*
		 * Workaround phantom bmiss interrupts by sanity-checking
		 * the time of our last rx'd frame.  If it is within the
		 * beacon miss interval then ignore the interrupt.  If it's
		 * truly a bmiss we'll get another interrupt soon and that'll
		 * be dispatched up for processing.
		 */
		if (tsf - lastrx > ic->ic_bmisstimeout*1024) {
			NET_LOCK_GIANT();
			ieee80211_beacon_miss(ic);
			NET_UNLOCK_GIANT();
		} else
			sc->sc_stats.ast_bmiss_phantom++;
	}
}

static void
ath_radar_proc(void *arg, int pending)
{
#if 0
	struct ath_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_if;
	struct ath_hal *ah = sc->sc_ah;
	HAL_CHANNEL hchan;

	if (ath_hal_procdfs(ah, &hchan)) {
		if_printf(ifp, "radar detected on channel %u/0x%x/0x%x\n",
			hchan.channel, hchan.channelFlags, hchan.privFlags);
		/*
		 * Initiate channel change.
		 */
		/* XXX not yet */
	}
#endif
}

static u_int
ath_chan2flags(struct ieee80211com *ic, struct ieee80211_channel *chan)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	static const u_int modeflags[] = {
		0,			/* IEEE80211_MODE_AUTO */
		CHANNEL_A,		/* IEEE80211_MODE_11A */
		CHANNEL_B,		/* IEEE80211_MODE_11B */
		CHANNEL_PUREG,		/* IEEE80211_MODE_11G */
		0,			/* IEEE80211_MODE_FH */
		CHANNEL_ST,		/* IEEE80211_MODE_TURBO_A */
		CHANNEL_108G		/* IEEE80211_MODE_TURBO_G */
	};
	enum ieee80211_phymode mode = ieee80211_chan2mode(ic, chan);

	KASSERTMSG(mode < N(modeflags), "unexpected phy mode %u", mode);
	KASSERTMSG(modeflags[mode] != 0, "mode %u undefined", mode);
	return modeflags[mode];
#undef N
}

static int
ath_ifinit(struct ifnet *ifp)
{
	struct ath_softc *sc = (struct ath_softc *)ifp->if_softc;

	return ath_init(sc);
}

static void
ath_settkipmic(struct ath_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;

	if ((ic->ic_caps & IEEE80211_C_TKIP) &&
	    !(ic->ic_caps & IEEE80211_C_WME_TKIPMIC)) {
		if (ic->ic_flags & IEEE80211_F_WME) {
			(void)ath_hal_settkipmic(ah, AH_FALSE);
			ic->ic_caps &= ~IEEE80211_C_TKIPMIC;
		} else {
			(void)ath_hal_settkipmic(ah, AH_TRUE);
			ic->ic_caps |= IEEE80211_C_TKIPMIC;
		}
	}
}

static int
ath_init(struct ath_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	HAL_STATUS status;
	int error = 0, s;

	DPRINTF(sc, ATH_DEBUG_ANY, "%s: if_flags 0x%x\n",
		__func__, ifp->if_flags);

	if (device_is_active(sc->sc_dev)) {
		s = splnet();
	} else if (!pmf_device_subtree_resume(sc->sc_dev, &sc->sc_qual) ||
	           !device_is_active(sc->sc_dev))
		return 0;
	else
		s = splnet();

	/*
	 * Stop anything previously setup.  This is safe
	 * whether this is the first time through or not.
	 */
	ath_stop_locked(ifp, 0);

	/*
	 * The basic interface to setting the hardware in a good
	 * state is ``reset''.  On return the hardware is known to
	 * be powered up and with interrupts disabled.  This must
	 * be followed by initialization of the appropriate bits
	 * and then setup of the interrupt mask.
	 */
	ath_settkipmic(sc);
	sc->sc_curchan.channel = ic->ic_curchan->ic_freq;
	sc->sc_curchan.channelFlags = ath_chan2flags(ic, ic->ic_curchan);
	if (!ath_hal_reset(ah, ic->ic_opmode, &sc->sc_curchan, AH_FALSE, &status)) {
		if_printf(ifp, "unable to reset hardware; hal status %u\n",
			status);
		error = EIO;
		goto done;
	}

	/*
	 * This is needed only to setup initial state
	 * but it's best done after a reset.
	 */
	ath_update_txpow(sc);
	/*
	 * Likewise this is set during reset so update
	 * state cached in the driver.
	 */
	ath_restore_diversity(sc);
	sc->sc_calinterval = 1;
	sc->sc_caltries = 0;

	/*
	 * Setup the hardware after reset: the key cache
	 * is filled as needed and the receive engine is
	 * set going.  Frame transmit is handled entirely
	 * in the frame output path; there's nothing to do
	 * here except setup the interrupt mask.
	 */
	if ((error = ath_startrecv(sc)) != 0) {
		if_printf(ifp, "unable to start recv logic\n");
		goto done;
	}

	/*
	 * Enable interrupts.
	 */
	sc->sc_imask = HAL_INT_RX | HAL_INT_TX
		  | HAL_INT_RXEOL | HAL_INT_RXORN
		  | HAL_INT_FATAL | HAL_INT_GLOBAL;
	/*
	 * Enable MIB interrupts when there are hardware phy counters.
	 * Note we only do this (at the moment) for station mode.
	 */
	if (sc->sc_needmib && ic->ic_opmode == IEEE80211_M_STA)
		sc->sc_imask |= HAL_INT_MIB;
	ath_hal_intrset(ah, sc->sc_imask);

	ifp->if_flags |= IFF_RUNNING;
	ic->ic_state = IEEE80211_S_INIT;

	/*
	 * The hardware should be ready to go now so it's safe
	 * to kick the 802.11 state machine as it's likely to
	 * immediately call back to us to send mgmt frames.
	 */
	ath_chan_change(sc, ic->ic_curchan);
#ifdef ATH_TX99_DIAG
	if (sc->sc_tx99 != NULL)
		sc->sc_tx99->start(sc->sc_tx99);
	else
#endif
	if (ic->ic_opmode != IEEE80211_M_MONITOR) {
		if (ic->ic_roaming != IEEE80211_ROAMING_MANUAL)
			ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
	} else
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
done:
	splx(s);
	return error;
}

static void
ath_stop_locked(struct ifnet *ifp, int disable)
{
	struct ath_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;

	DPRINTF(sc, ATH_DEBUG_ANY, "%s: invalid %d if_flags 0x%x\n",
		__func__, !device_is_enabled(sc->sc_dev), ifp->if_flags);

	/* KASSERT() IPL_NET */
	if (ifp->if_flags & IFF_RUNNING) {
		/*
		 * Shutdown the hardware and driver:
		 *    reset 802.11 state machine
		 *    turn off timers
		 *    disable interrupts
		 *    turn off the radio
		 *    clear transmit machinery
		 *    clear receive machinery
		 *    drain and release tx queues
		 *    reclaim beacon resources
		 *    power down hardware
		 *
		 * Note that some of this work is not possible if the
		 * hardware is gone (invalid).
		 */
#ifdef ATH_TX99_DIAG
		if (sc->sc_tx99 != NULL)
			sc->sc_tx99->stop(sc->sc_tx99);
#endif
		ieee80211_new_state(ic, IEEE80211_S_INIT, -1);
		ifp->if_flags &= ~IFF_RUNNING;
		ifp->if_timer = 0;
		if (device_is_enabled(sc->sc_dev)) {
			if (sc->sc_softled) {
				callout_stop(&sc->sc_ledtimer);
				ath_hal_gpioset(ah, sc->sc_ledpin,
					!sc->sc_ledon);
				sc->sc_blinking = 0;
			}
			ath_hal_intrset(ah, 0);
		}
		ath_draintxq(sc);
		if (device_is_enabled(sc->sc_dev)) {
			ath_stoprecv(sc);
			ath_hal_phydisable(ah);
		} else
			sc->sc_rxlink = NULL;
		IF_PURGE(&ifp->if_snd);
		ath_beacon_free(sc);
	}
	if (disable)
		pmf_device_suspend(sc->sc_dev, &sc->sc_qual);
}

static void
ath_stop(struct ifnet *ifp, int disable)
{
	int s;

	s = splnet();
	ath_stop_locked(ifp, disable);
	splx(s);
}

static void
ath_restore_diversity(struct ath_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;
	struct ath_hal *ah = sc->sc_ah;

	if (!ath_hal_setdiversity(sc->sc_ah, sc->sc_diversity) ||
	    sc->sc_diversity != ath_hal_getdiversity(ah)) {
		if_printf(ifp, "could not restore diversity setting %d\n",
		    sc->sc_diversity);
		sc->sc_diversity = ath_hal_getdiversity(ah);
	}
}

/*
 * Reset the hardware w/o losing operational state.  This is
 * basically a more efficient way of doing ath_stop, ath_init,
 * followed by state transitions to the current 802.11
 * operational state.  Used to recover from various errors and
 * to reset or reload hardware state.
 */
int
ath_reset(struct ifnet *ifp)
{
	struct ath_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211_channel *c;
	HAL_STATUS status;

	/*
	 * Convert to a HAL channel description with the flags
	 * constrained to reflect the current operating mode.
	 */
	c = ic->ic_curchan;
	sc->sc_curchan.channel = c->ic_freq;
	sc->sc_curchan.channelFlags = ath_chan2flags(ic, c);

	ath_hal_intrset(ah, 0);		/* disable interrupts */
	ath_draintxq(sc);		/* stop xmit side */
	ath_stoprecv(sc);		/* stop recv side */
	ath_settkipmic(sc);		/* configure TKIP MIC handling */
	/* NB: indicate channel change so we do a full reset */
	if (!ath_hal_reset(ah, ic->ic_opmode, &sc->sc_curchan, AH_TRUE, &status))
		if_printf(ifp, "%s: unable to reset hardware; hal status %u\n",
			__func__, status);
	ath_update_txpow(sc);		/* update tx power state */
	ath_restore_diversity(sc);
	sc->sc_calinterval = 1;
	sc->sc_caltries = 0;
	if (ath_startrecv(sc) != 0)	/* restart recv */
		if_printf(ifp, "%s: unable to start recv logic\n", __func__);
	/*
	 * We may be doing a reset in response to an ioctl
	 * that changes the channel so update any state that
	 * might change as a result.
	 */
	ath_chan_change(sc, c);
	if (ic->ic_state == IEEE80211_S_RUN)
		ath_beacon_config(sc);	/* restart beacons */
	ath_hal_intrset(ah, sc->sc_imask);

	ath_start(ifp);			/* restart xmit */
	return 0;
}

/*
 * Cleanup driver resources when we run out of buffers
 * while processing fragments; return the tx buffers
 * allocated and drop node references.
 */
static void
ath_txfrag_cleanup(struct ath_softc *sc,
	ath_bufhead *frags, struct ieee80211_node *ni)
{
	struct ath_buf *bf;

	ATH_TXBUF_LOCK_ASSERT(sc);

	while ((bf = STAILQ_FIRST(frags)) != NULL) {
		STAILQ_REMOVE_HEAD(frags, bf_list);
		STAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
		sc->sc_if.if_flags &= ~IFF_OACTIVE;
		ieee80211_node_decref(ni);
	}
}

/*
 * Setup xmit of a fragmented frame.  Allocate a buffer
 * for each frag and bump the node reference count to
 * reflect the held reference to be setup by ath_tx_start.
 */
static int
ath_txfrag_setup(struct ath_softc *sc, ath_bufhead *frags,
	struct mbuf *m0, struct ieee80211_node *ni)
{
	struct mbuf *m;
	struct ath_buf *bf;

	ATH_TXBUF_LOCK(sc);
	for (m = m0->m_nextpkt; m != NULL; m = m->m_nextpkt) {
		bf = STAILQ_FIRST(&sc->sc_txbuf);
		if (bf == NULL) {       /* out of buffers, cleanup */
			DPRINTF(sc, ATH_DEBUG_XMIT, "%s: out of xmit buffers\n",
				__func__);
			sc->sc_if.if_flags |= IFF_OACTIVE;
			ath_txfrag_cleanup(sc, frags, ni);
			break;
		}
		STAILQ_REMOVE_HEAD(&sc->sc_txbuf, bf_list);
		ieee80211_node_incref(ni);
		STAILQ_INSERT_TAIL(frags, bf, bf_list);
	}
	ATH_TXBUF_UNLOCK(sc);

	return !STAILQ_EMPTY(frags);
}

static void
ath_start(struct ifnet *ifp)
{
	struct ath_softc *sc = ifp->if_softc;
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct ath_buf *bf;
	struct mbuf *m, *next;
	struct ieee80211_frame *wh;
	struct ether_header *eh;
	ath_bufhead frags;

	if ((ifp->if_flags & IFF_RUNNING) == 0 ||
	    !device_is_active(sc->sc_dev))
		return;

	if (sc->sc_flags & ATH_KEY_UPDATING)
		return;

	for (;;) {
		/*
		 * Grab a TX buffer and associated resources.
		 */
		ATH_TXBUF_LOCK(sc);
		bf = STAILQ_FIRST(&sc->sc_txbuf);
		if (bf != NULL)
			STAILQ_REMOVE_HEAD(&sc->sc_txbuf, bf_list);
		ATH_TXBUF_UNLOCK(sc);
		if (bf == NULL) {
			DPRINTF(sc, ATH_DEBUG_XMIT, "%s: out of xmit buffers\n",
				__func__);
			sc->sc_stats.ast_tx_qstop++;
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
		/*
		 * Poll the management queue for frames; they
		 * have priority over normal data frames.
		 */
		IF_DEQUEUE(&ic->ic_mgtq, m);
		if (m == NULL) {
			/*
			 * No data frames go out unless we're associated.
			 */
			if (ic->ic_state != IEEE80211_S_RUN) {
				DPRINTF(sc, ATH_DEBUG_XMIT,
				    "%s: discard data packet, state %s\n",
				    __func__,
				    ieee80211_state_name[ic->ic_state]);
				sc->sc_stats.ast_tx_discard++;
				ATH_TXBUF_LOCK(sc);
				STAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
				ATH_TXBUF_UNLOCK(sc);
				break;
			}
			IFQ_DEQUEUE(&ifp->if_snd, m);	/* XXX: LOCK */
			if (m == NULL) {
				ATH_TXBUF_LOCK(sc);
				STAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
				ATH_TXBUF_UNLOCK(sc);
				break;
			}
			STAILQ_INIT(&frags);
			/*
			 * Find the node for the destination so we can do
			 * things like power save and fast frames aggregation.
			 */
			if (m->m_len < sizeof(struct ether_header) &&
			   (m = m_pullup(m, sizeof(struct ether_header))) == NULL) {
				ic->ic_stats.is_tx_nobuf++;	/* XXX */
				ni = NULL;
				goto bad;
			}
			eh = mtod(m, struct ether_header *);
			ni = ieee80211_find_txnode(ic, eh->ether_dhost);
			if (ni == NULL) {
				/* NB: ieee80211_find_txnode does stat+msg */
				m_freem(m);
				goto bad;
			}
			if ((ni->ni_flags & IEEE80211_NODE_PWR_MGT) &&
			    (m->m_flags & M_PWR_SAV) == 0) {
				/*
				 * Station in power save mode; pass the frame
				 * to the 802.11 layer and continue.  We'll get
				 * the frame back when the time is right.
				 */
				ieee80211_pwrsave(ic, ni, m);
				goto reclaim;
			}
			/* calculate priority so we can find the tx queue */
			if (ieee80211_classify(ic, m, ni)) {
				DPRINTF(sc, ATH_DEBUG_XMIT,
					"%s: discard, classification failure\n",
					__func__);
				m_freem(m);
				goto bad;
			}
			ifp->if_opackets++;

			bpf_mtap(ifp, m);
			/*
			 * Encapsulate the packet in prep for transmission.
			 */
			m = ieee80211_encap(ic, m, ni);
			if (m == NULL) {
				DPRINTF(sc, ATH_DEBUG_XMIT,
					"%s: encapsulation failure\n",
					__func__);
				sc->sc_stats.ast_tx_encap++;
				goto bad;
			}
			/*
			 * Check for fragmentation.  If this has frame
			 * has been broken up verify we have enough
			 * buffers to send all the fragments so all
			 * go out or none...
			 */
			if ((m->m_flags & M_FRAG) &&
			    !ath_txfrag_setup(sc, &frags, m, ni)) {
				DPRINTF(sc, ATH_DEBUG_ANY,
				    "%s: out of txfrag buffers\n", __func__);
				ic->ic_stats.is_tx_nobuf++;     /* XXX */
				ath_freetx(m);
				goto bad;
			}
		} else {
			/*
			 * Hack!  The referenced node pointer is in the
			 * rcvif field of the packet header.  This is
			 * placed there by ieee80211_mgmt_output because
			 * we need to hold the reference with the frame
			 * and there's no other way (other than packet
			 * tags which we consider too expensive to use)
			 * to pass it along.
			 */
			ni = (struct ieee80211_node *) m->m_pkthdr.rcvif;
			m->m_pkthdr.rcvif = NULL;

			wh = mtod(m, struct ieee80211_frame *);
			if ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
			    IEEE80211_FC0_SUBTYPE_PROBE_RESP) {
				/* fill time stamp */
				u_int64_t tsf;
				u_int32_t *tstamp;

				tsf = ath_hal_gettsf64(ah);
				/* XXX: adjust 100us delay to xmit */
				tsf += 100;
				tstamp = (u_int32_t *)&wh[1];
				tstamp[0] = htole32(tsf & 0xffffffff);
				tstamp[1] = htole32(tsf >> 32);
			}
			sc->sc_stats.ast_tx_mgmt++;
		}

	nextfrag:
		next = m->m_nextpkt;
		if (ath_tx_start(sc, ni, bf, m)) {
	bad:
			ifp->if_oerrors++;
	reclaim:
			ATH_TXBUF_LOCK(sc);
			STAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
			ath_txfrag_cleanup(sc, &frags, ni);
			ATH_TXBUF_UNLOCK(sc);
			if (ni != NULL)
				ieee80211_free_node(ni);
			continue;
		}
		if (next != NULL) {
			m = next;
			bf = STAILQ_FIRST(&frags);
			KASSERTMSG(bf != NULL, "no buf for txfrag");
			STAILQ_REMOVE_HEAD(&frags, bf_list);
			goto nextfrag;
		}

		ifp->if_timer = 1;
	}
}

static int
ath_media_change(struct ifnet *ifp)
{
#define	IS_UP(ifp) \
	((ifp->if_flags & IFF_UP) && (ifp->if_flags & IFF_RUNNING))
	int error;

	error = ieee80211_media_change(ifp);
	if (error == ENETRESET) {
		if (IS_UP(ifp))
			ath_init(ifp->if_softc);	/* XXX lose error */
		error = 0;
	}
	return error;
#undef IS_UP
}

#ifdef AR_DEBUG
static void
ath_keyprint(const char *tag, u_int ix,
	const HAL_KEYVAL *hk, const u_int8_t mac[IEEE80211_ADDR_LEN])
{
	static const char *ciphers[] = {
		"WEP",
		"AES-OCB",
		"AES-CCM",
		"CKIP",
		"TKIP",
		"CLR",
	};
	int i, n;

	printf("%s: [%02u] %-7s ", tag, ix, ciphers[hk->kv_type]);
	for (i = 0, n = hk->kv_len; i < n; i++)
		printf("%02x", hk->kv_val[i]);
	printf(" mac %s", ether_sprintf(mac));
	if (hk->kv_type == HAL_CIPHER_TKIP) {
		printf(" mic ");
		for (i = 0; i < sizeof(hk->kv_mic); i++)
			printf("%02x", hk->kv_mic[i]);
	}
	printf("\n");
}
#endif

/*
 * Set a TKIP key into the hardware.  This handles the
 * potential distribution of key state to multiple key
 * cache slots for TKIP.
 */
static int
ath_keyset_tkip(struct ath_softc *sc, const struct ieee80211_key *k,
	HAL_KEYVAL *hk, const u_int8_t mac[IEEE80211_ADDR_LEN])
{
#define	IEEE80211_KEY_XR	(IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV)
	static const u_int8_t zerobssid[IEEE80211_ADDR_LEN];
	struct ath_hal *ah = sc->sc_ah;

	KASSERTMSG(k->wk_cipher->ic_cipher == IEEE80211_CIPHER_TKIP,
		"got a non-TKIP key, cipher %u", k->wk_cipher->ic_cipher);
	if ((k->wk_flags & IEEE80211_KEY_XR) == IEEE80211_KEY_XR) {
		if (sc->sc_splitmic) {
			/*
			 * TX key goes at first index, RX key at the rx index.
			 * The hal handles the MIC keys at index+64.
			 */
			memcpy(hk->kv_mic, k->wk_txmic, sizeof(hk->kv_mic));
			KEYPRINTF(sc, k->wk_keyix, hk, zerobssid);
			if (!ath_hal_keyset(ah, ATH_KEY(k->wk_keyix), hk,
						zerobssid))
				return 0;

			memcpy(hk->kv_mic, k->wk_rxmic, sizeof(hk->kv_mic));
			KEYPRINTF(sc, k->wk_keyix+32, hk, mac);
			/* XXX delete tx key on failure? */
			return ath_hal_keyset(ah, ATH_KEY(k->wk_keyix+32),
					hk, mac);
		} else {
			/*
			 * Room for both TX+RX MIC keys in one key cache
			 * slot, just set key at the first index; the HAL
			 * will handle the reset.
			 */
			memcpy(hk->kv_mic, k->wk_rxmic, sizeof(hk->kv_mic));
			memcpy(hk->kv_txmic, k->wk_txmic, sizeof(hk->kv_txmic));
			KEYPRINTF(sc, k->wk_keyix, hk, mac);
			return ath_hal_keyset(ah, ATH_KEY(k->wk_keyix), hk, mac);
		}
	} else if (k->wk_flags & IEEE80211_KEY_XMIT) {
		if (sc->sc_splitmic) {
			/*
			 * NB: must pass MIC key in expected location when
			 * the keycache only holds one MIC key per entry.
			 */
			memcpy(hk->kv_mic, k->wk_txmic, sizeof(hk->kv_txmic));
		} else
			memcpy(hk->kv_txmic, k->wk_txmic, sizeof(hk->kv_txmic));
		KEYPRINTF(sc, k->wk_keyix, hk, mac);
		return ath_hal_keyset(ah, ATH_KEY(k->wk_keyix), hk, mac);
	} else if (k->wk_flags & IEEE80211_KEY_RECV) {
		memcpy(hk->kv_mic, k->wk_rxmic, sizeof(hk->kv_mic));
		KEYPRINTF(sc, k->wk_keyix, hk, mac);
		return ath_hal_keyset(ah, k->wk_keyix, hk, mac);
	}
	return 0;
#undef IEEE80211_KEY_XR
}

/*
 * Set a net80211 key into the hardware.  This handles the
 * potential distribution of key state to multiple key
 * cache slots for TKIP with hardware MIC support.
 */
static int
ath_keyset(struct ath_softc *sc, const struct ieee80211_key *k,
	const u_int8_t mac0[IEEE80211_ADDR_LEN],
	struct ieee80211_node *bss)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	static const u_int8_t ciphermap[] = {
		HAL_CIPHER_WEP,		/* IEEE80211_CIPHER_WEP */
		HAL_CIPHER_TKIP,	/* IEEE80211_CIPHER_TKIP */
		HAL_CIPHER_AES_OCB,	/* IEEE80211_CIPHER_AES_OCB */
		HAL_CIPHER_AES_CCM,	/* IEEE80211_CIPHER_AES_CCM */
		(u_int8_t) -1,		/* 4 is not allocated */
		HAL_CIPHER_CKIP,	/* IEEE80211_CIPHER_CKIP */
		HAL_CIPHER_CLR,		/* IEEE80211_CIPHER_NONE */
	};
	struct ath_hal *ah = sc->sc_ah;
	const struct ieee80211_cipher *cip = k->wk_cipher;
	u_int8_t gmac[IEEE80211_ADDR_LEN];
	const u_int8_t *mac;
	HAL_KEYVAL hk;

	memset(&hk, 0, sizeof(hk));
	/*
	 * Software crypto uses a "clear key" so non-crypto
	 * state kept in the key cache are maintained and
	 * so that rx frames have an entry to match.
	 */
	if ((k->wk_flags & IEEE80211_KEY_SWCRYPT) == 0) {
		KASSERTMSG(cip->ic_cipher < N(ciphermap),
			"invalid cipher type %u", cip->ic_cipher);
		hk.kv_type = ciphermap[cip->ic_cipher];
		hk.kv_len = k->wk_keylen;
		memcpy(hk.kv_val, k->wk_key, k->wk_keylen);
	} else
		hk.kv_type = HAL_CIPHER_CLR;

	if ((k->wk_flags & IEEE80211_KEY_GROUP) && sc->sc_mcastkey) {
		/*
		 * Group keys on hardware that supports multicast frame
		 * key search use a mac that is the sender's address with
		 * the high bit set instead of the app-specified address.
		 */
		IEEE80211_ADDR_COPY(gmac, bss->ni_macaddr);
		gmac[0] |= 0x80;
		mac = gmac;
	} else
		mac = mac0;

	if ((hk.kv_type == HAL_CIPHER_TKIP &&
	    (k->wk_flags & IEEE80211_KEY_SWMIC) == 0)) {
		return ath_keyset_tkip(sc, k, &hk, mac);
	} else {
		KEYPRINTF(sc, k->wk_keyix, &hk, mac);
		return ath_hal_keyset(ah, ATH_KEY(k->wk_keyix), &hk, mac);
	}
#undef N
}

/*
 * Allocate tx/rx key slots for TKIP.  We allocate two slots for
 * each key, one for decrypt/encrypt and the other for the MIC.
 */
static u_int16_t
key_alloc_2pair(struct ath_softc *sc,
	ieee80211_keyix *txkeyix, ieee80211_keyix *rxkeyix)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	u_int i, keyix;

	KASSERTMSG(sc->sc_splitmic, "key cache !split");
	/* XXX could optimize */
	for (i = 0; i < N(sc->sc_keymap)/4; i++) {
		u_int8_t b = sc->sc_keymap[i];
		if (b != 0xff) {
			/*
			 * One or more slots in this byte are free.
			 */
			keyix = i*NBBY;
			while (b & 1) {
		again:
				keyix++;
				b >>= 1;
			}
			/* XXX IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV */
			if (isset(sc->sc_keymap, keyix+32) ||
			    isset(sc->sc_keymap, keyix+64) ||
			    isset(sc->sc_keymap, keyix+32+64)) {
				/* full pair unavailable */
				/* XXX statistic */
				if (keyix == (i+1)*NBBY) {
					/* no slots were appropriate, advance */
					continue;
				}
				goto again;
			}
			setbit(sc->sc_keymap, keyix);
			setbit(sc->sc_keymap, keyix+64);
			setbit(sc->sc_keymap, keyix+32);
			setbit(sc->sc_keymap, keyix+32+64);
			DPRINTF(sc, ATH_DEBUG_KEYCACHE,
				"%s: key pair %u,%u %u,%u\n",
				__func__, keyix, keyix+64,
				keyix+32, keyix+32+64);
			*txkeyix = keyix;
			*rxkeyix = keyix+32;
			return keyix;
		}
	}
	DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s: out of pair space\n", __func__);
	return IEEE80211_KEYIX_NONE;
#undef N
}

/*
 * Allocate tx/rx key slots for TKIP.  We allocate two slots for
 * each key, one for decrypt/encrypt and the other for the MIC.
 */
static int
key_alloc_pair(struct ath_softc *sc, ieee80211_keyix *txkeyix,
    ieee80211_keyix *rxkeyix)
{
#define N(a)	(sizeof(a)/sizeof(a[0]))
	u_int i, keyix;

	KASSERTMSG(!sc->sc_splitmic, "key cache split");
	/* XXX could optimize */
	for (i = 0; i < N(sc->sc_keymap)/4; i++) {
		uint8_t b = sc->sc_keymap[i];
		if (b != 0xff) {
			/*
			 * One or more slots in this byte are free.
			 */
			keyix = i*NBBY;
			while (b & 1) {
		again:
				keyix++;
				b >>= 1;
			}
			if (isset(sc->sc_keymap, keyix+64)) {
				/* full pair unavailable */
				/* XXX statistic */
				if (keyix == (i+1)*NBBY) {
					/* no slots were appropriate, advance */
					continue;
				}
				goto again;
			}
			setbit(sc->sc_keymap, keyix);
			setbit(sc->sc_keymap, keyix+64);
			DPRINTF(sc, ATH_DEBUG_KEYCACHE,
				"%s: key pair %u,%u\n",
				__func__, keyix, keyix+64);
			*txkeyix = *rxkeyix = keyix;
			return 1;
		}
	}
	DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s: out of pair space\n", __func__);
	return 0;
#undef N
}

/*
 * Allocate a single key cache slot.
 */
static int
key_alloc_single(struct ath_softc *sc,
	ieee80211_keyix *txkeyix, ieee80211_keyix *rxkeyix)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	u_int i, keyix;

	/* XXX try i,i+32,i+64,i+32+64 to minimize key pair conflicts */
	for (i = 0; i < N(sc->sc_keymap); i++) {
		u_int8_t b = sc->sc_keymap[i];
		if (b != 0xff) {
			/*
			 * One or more slots are free.
			 */
			keyix = i*NBBY;
			while (b & 1)
				keyix++, b >>= 1;
			setbit(sc->sc_keymap, keyix);
			DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s: key %u\n",
				__func__, keyix);
			*txkeyix = *rxkeyix = keyix;
			return 1;
		}
	}
	DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s: out of space\n", __func__);
	return 0;
#undef N
}

/*
 * Allocate one or more key cache slots for a uniacst key.  The
 * key itself is needed only to identify the cipher.  For hardware
 * TKIP with split cipher+MIC keys we allocate two key cache slot
 * pairs so that we can setup separate TX and RX MIC keys.  Note
 * that the MIC key for a TKIP key at slot i is assumed by the
 * hardware to be at slot i+64.  This limits TKIP keys to the first
 * 64 entries.
 */
static int
ath_key_alloc(struct ieee80211com *ic, const struct ieee80211_key *k,
	ieee80211_keyix *keyix, ieee80211_keyix *rxkeyix)
{
	struct ath_softc *sc = ic->ic_ifp->if_softc;

	/*
	 * Group key allocation must be handled specially for
	 * parts that do not support multicast key cache search
	 * functionality.  For those parts the key id must match
	 * the h/w key index so lookups find the right key.  On
	 * parts w/ the key search facility we install the sender's
	 * mac address (with the high bit set) and let the hardware
	 * find the key w/o using the key id.  This is preferred as
	 * it permits us to support multiple users for adhoc and/or
	 * multi-station operation.
	 */
	if ((k->wk_flags & IEEE80211_KEY_GROUP) && !sc->sc_mcastkey) {
		if (!(&ic->ic_nw_keys[0] <= k &&
		      k < &ic->ic_nw_keys[IEEE80211_WEP_NKID])) {
			/* should not happen */
			DPRINTF(sc, ATH_DEBUG_KEYCACHE,
				"%s: bogus group key\n", __func__);
			return 0;
		}
		/*
		 * XXX we pre-allocate the global keys so
		 * have no way to check if they've already been allocated.
		 */
		*keyix = *rxkeyix = k - ic->ic_nw_keys;
		return 1;
	}

	/*
	 * We allocate two pair for TKIP when using the h/w to do
	 * the MIC.  For everything else, including software crypto,
	 * we allocate a single entry.  Note that s/w crypto requires
	 * a pass-through slot on the 5211 and 5212.  The 5210 does
	 * not support pass-through cache entries and we map all
	 * those requests to slot 0.
	 */
	if (k->wk_flags & IEEE80211_KEY_SWCRYPT) {
		return key_alloc_single(sc, keyix, rxkeyix);
	} else if (k->wk_cipher->ic_cipher == IEEE80211_CIPHER_TKIP &&
	    (k->wk_flags & IEEE80211_KEY_SWMIC) == 0) {
		if (sc->sc_splitmic)
			return key_alloc_2pair(sc, keyix, rxkeyix);
		else
			return key_alloc_pair(sc, keyix, rxkeyix);
	} else {
		return key_alloc_single(sc, keyix, rxkeyix);
	}
}

/*
 * Delete an entry in the key cache allocated by ath_key_alloc.
 */
static int
ath_key_delete(struct ieee80211com *ic, const struct ieee80211_key *k)
{
	struct ath_softc *sc = ic->ic_ifp->if_softc;
	struct ath_hal *ah = sc->sc_ah;
	const struct ieee80211_cipher *cip = k->wk_cipher;
	u_int keyix = k->wk_keyix;

	DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s: delete key %u\n", __func__, keyix);

	if (!device_has_power(sc->sc_dev)) {
		aprint_error_dev(sc->sc_dev, "deleting keyix %d w/o power\n",
		    k->wk_keyix);
	}

	ath_hal_keyreset(ah, keyix);
	/*
	 * Handle split tx/rx keying required for TKIP with h/w MIC.
	 */
	if (cip->ic_cipher == IEEE80211_CIPHER_TKIP &&
	    (k->wk_flags & IEEE80211_KEY_SWMIC) == 0 && sc->sc_splitmic)
		ath_hal_keyreset(ah, keyix+32);		/* RX key */
	if (keyix >= IEEE80211_WEP_NKID) {
		/*
		 * Don't touch keymap entries for global keys so
		 * they are never considered for dynamic allocation.
		 */
		clrbit(sc->sc_keymap, keyix);
		if (cip->ic_cipher == IEEE80211_CIPHER_TKIP &&
		    (k->wk_flags & IEEE80211_KEY_SWMIC) == 0) {
			clrbit(sc->sc_keymap, keyix+64);	/* TX key MIC */
			if (sc->sc_splitmic) {
				/* +32 for RX key, +32+64 for RX key MIC */
				clrbit(sc->sc_keymap, keyix+32);
				clrbit(sc->sc_keymap, keyix+32+64);
			}
		}
	}
	return 1;
}

/*
 * Set the key cache contents for the specified key.  Key cache
 * slot(s) must already have been allocated by ath_key_alloc.
 */
static int
ath_key_set(struct ieee80211com *ic, const struct ieee80211_key *k,
	const u_int8_t mac[IEEE80211_ADDR_LEN])
{
	struct ath_softc *sc = ic->ic_ifp->if_softc;

	if (!device_has_power(sc->sc_dev)) {
		aprint_error_dev(sc->sc_dev, "setting keyix %d w/o power\n",
		    k->wk_keyix);
	}
	return ath_keyset(sc, k, mac, ic->ic_bss);
}

/*
 * Block/unblock tx+rx processing while a key change is done.
 * We assume the caller serializes key management operations
 * so we only need to worry about synchronization with other
 * uses that originate in the driver.
 */
static void
ath_key_update_begin(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct ath_softc *sc = ifp->if_softc;

	DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s:\n", __func__);
#if 0
	tasklet_disable(&sc->sc_rxtq);
#endif
	sc->sc_flags |= ATH_KEY_UPDATING;
}

static void
ath_key_update_end(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct ath_softc *sc = ifp->if_softc;

	DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s:\n", __func__);
	sc->sc_flags &= ~ATH_KEY_UPDATING;
#if 0
	tasklet_enable(&sc->sc_rxtq);
#endif
}

/*
 * Calculate the receive filter according to the
 * operating mode and state:
 *
 * o always accept unicast, broadcast, and multicast traffic
 * o maintain current state of phy error reception (the hal
 *   may enable phy error frames for noise immunity work)
 * o probe request frames are accepted only when operating in
 *   hostap, adhoc, or monitor modes
 * o enable promiscuous mode according to the interface state
 * o accept beacons:
 *   - when operating in adhoc mode so the 802.11 layer creates
 *     node table entries for peers,
 *   - when operating in station mode for collecting rssi data when
 *     the station is otherwise quiet, or
 *   - when scanning
 */
static u_int32_t
ath_calcrxfilter(struct ath_softc *sc, enum ieee80211_state state)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	struct ifnet *ifp = &sc->sc_if;
	u_int32_t rfilt;

	rfilt = (ath_hal_getrxfilter(ah) & HAL_RX_FILTER_PHYERR)
	      | HAL_RX_FILTER_UCAST | HAL_RX_FILTER_BCAST | HAL_RX_FILTER_MCAST;
	if (ic->ic_opmode != IEEE80211_M_STA)
		rfilt |= HAL_RX_FILTER_PROBEREQ;
	if (ic->ic_opmode != IEEE80211_M_HOSTAP &&
	    (ifp->if_flags & IFF_PROMISC))
		rfilt |= HAL_RX_FILTER_PROM;
	if (ifp->if_flags & IFF_PROMISC)
		rfilt |= HAL_RX_FILTER_CONTROL | HAL_RX_FILTER_PROBEREQ;
	if (ic->ic_opmode == IEEE80211_M_STA ||
	    ic->ic_opmode == IEEE80211_M_IBSS ||
	    state == IEEE80211_S_SCAN)
		rfilt |= HAL_RX_FILTER_BEACON;
	return rfilt;
}

static void
ath_mode_init(struct ath_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	struct ether_multi *enm;
	struct ether_multistep estep;
	u_int32_t rfilt, mfilt[2], val;
	int i;
	uint8_t pos;

	/* configure rx filter */
	rfilt = ath_calcrxfilter(sc, ic->ic_state);
	ath_hal_setrxfilter(ah, rfilt);

	/* configure operational mode */
	ath_hal_setopmode(ah);

	/* Write keys to hardware; it may have been powered down. */
	ath_key_update_begin(ic);
	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		ath_key_set(ic,
			    &ic->ic_crypto.cs_nw_keys[i],
			    ic->ic_myaddr);
	}
	ath_key_update_end(ic);

	/*
	 * Handle any link-level address change.  Note that we only
	 * need to force ic_myaddr; any other addresses are handled
	 * as a byproduct of the ifnet code marking the interface
	 * down then up.
	 *
	 * XXX should get from lladdr instead of arpcom but that's more work
	 */
	IEEE80211_ADDR_COPY(ic->ic_myaddr, CLLADDR(sc->sc_if.if_sadl));
	ath_hal_setmac(ah, ic->ic_myaddr);

	/* calculate and install multicast filter */
	ifp->if_flags &= ~IFF_ALLMULTI;
	mfilt[0] = mfilt[1] = 0;
	ETHER_FIRST_MULTI(estep, &sc->sc_ec, enm);
	while (enm != NULL) {
		void *dl;
		/* XXX Punt on ranges. */
		if (!IEEE80211_ADDR_EQ(enm->enm_addrlo, enm->enm_addrhi)) {
			mfilt[0] = mfilt[1] = 0xffffffff;
			ifp->if_flags |= IFF_ALLMULTI;
			break;
		}
		dl = enm->enm_addrlo;
		val = LE_READ_4((char *)dl + 0);
		pos = (val >> 18) ^ (val >> 12) ^ (val >> 6) ^ val;
		val = LE_READ_4((char *)dl + 3);
		pos ^= (val >> 18) ^ (val >> 12) ^ (val >> 6) ^ val;
		pos &= 0x3f;
		mfilt[pos / 32] |= (1 << (pos % 32));

		ETHER_NEXT_MULTI(estep, enm);
	}

	ath_hal_setmcastfilter(ah, mfilt[0], mfilt[1]);
	DPRINTF(sc, ATH_DEBUG_MODE, "%s: RX filter 0x%x, MC filter %08x:%08x\n",
		__func__, rfilt, mfilt[0], mfilt[1]);
}

/*
 * Set the slot time based on the current setting.
 */
static void
ath_setslottime(struct ath_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;

	if (ic->ic_flags & IEEE80211_F_SHSLOT)
		ath_hal_setslottime(ah, HAL_SLOT_TIME_9);
	else
		ath_hal_setslottime(ah, HAL_SLOT_TIME_20);
	sc->sc_updateslot = OK;
}

/*
 * Callback from the 802.11 layer to update the
 * slot time based on the current setting.
 */
static void
ath_updateslot(struct ifnet *ifp)
{
	struct ath_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	/*
	 * When not coordinating the BSS, change the hardware
	 * immediately.  For other operation we defer the change
	 * until beacon updates have propagated to the stations.
	 */
	if (ic->ic_opmode == IEEE80211_M_HOSTAP)
		sc->sc_updateslot = UPDATE;
	else
		ath_setslottime(sc);
}

/*
 * Setup a h/w transmit queue for beacons.
 */
static int
ath_beaconq_setup(struct ath_hal *ah)
{
	HAL_TXQ_INFO qi;

	memset(&qi, 0, sizeof(qi));
	qi.tqi_aifs = HAL_TXQ_USEDEFAULT;
	qi.tqi_cwmin = HAL_TXQ_USEDEFAULT;
	qi.tqi_cwmax = HAL_TXQ_USEDEFAULT;
	/* NB: for dynamic turbo, don't enable any other interrupts */
	qi.tqi_qflags = HAL_TXQ_TXDESCINT_ENABLE;
	return ath_hal_setuptxqueue(ah, HAL_TX_QUEUE_BEACON, &qi);
}

/*
 * Setup the transmit queue parameters for the beacon queue.
 */
static int
ath_beaconq_config(struct ath_softc *sc)
{
#define	ATH_EXPONENT_TO_VALUE(v)	((1<<(v))-1)
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	HAL_TXQ_INFO qi;

	ath_hal_gettxqueueprops(ah, sc->sc_bhalq, &qi);
	if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
		/*
		 * Always burst out beacon and CAB traffic.
		 */
		qi.tqi_aifs = ATH_BEACON_AIFS_DEFAULT;
		qi.tqi_cwmin = ATH_BEACON_CWMIN_DEFAULT;
		qi.tqi_cwmax = ATH_BEACON_CWMAX_DEFAULT;
	} else {
		struct wmeParams *wmep =
			&ic->ic_wme.wme_chanParams.cap_wmeParams[WME_AC_BE];
		/*
		 * Adhoc mode; important thing is to use 2x cwmin.
		 */
		qi.tqi_aifs = wmep->wmep_aifsn;
		qi.tqi_cwmin = 2*ATH_EXPONENT_TO_VALUE(wmep->wmep_logcwmin);
		qi.tqi_cwmax = ATH_EXPONENT_TO_VALUE(wmep->wmep_logcwmax);
	}

	if (!ath_hal_settxqueueprops(ah, sc->sc_bhalq, &qi)) {
		device_printf(sc->sc_dev, "unable to update parameters for "
			"beacon hardware queue!\n");
		return 0;
	} else {
		ath_hal_resettxqueue(ah, sc->sc_bhalq); /* push to h/w */
		return 1;
	}
#undef ATH_EXPONENT_TO_VALUE
}

/*
 * Allocate and setup an initial beacon frame.
 */
static int
ath_beacon_alloc(struct ath_softc *sc, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ath_buf *bf;
	struct mbuf *m;
	int error;

	bf = STAILQ_FIRST(&sc->sc_bbuf);
	if (bf == NULL) {
		DPRINTF(sc, ATH_DEBUG_BEACON, "%s: no dma buffers\n", __func__);
		sc->sc_stats.ast_be_nombuf++;	/* XXX */
		return ENOMEM;			/* XXX */
	}
	/*
	 * NB: the beacon data buffer must be 32-bit aligned;
	 * we assume the mbuf routines will return us something
	 * with this alignment (perhaps should assert).
	 */
	m = ieee80211_beacon_alloc(ic, ni, &sc->sc_boff);
	if (m == NULL) {
		DPRINTF(sc, ATH_DEBUG_BEACON, "%s: cannot get mbuf\n",
			__func__);
		sc->sc_stats.ast_be_nombuf++;
		return ENOMEM;
	}
	error = bus_dmamap_load_mbuf(sc->sc_dmat, bf->bf_dmamap, m,
				     BUS_DMA_NOWAIT);
	if (error == 0) {
		bf->bf_m = m;
		bf->bf_node = ieee80211_ref_node(ni);
	} else {
		m_freem(m);
	}
	return error;
}

/*
 * Setup the beacon frame for transmit.
 */
static void
ath_beacon_setup(struct ath_softc *sc, struct ath_buf *bf)
{
#define	USE_SHPREAMBLE(_ic) \
	(((_ic)->ic_flags & (IEEE80211_F_SHPREAMBLE | IEEE80211_F_USEBARKER))\
		== IEEE80211_F_SHPREAMBLE)
	struct ieee80211_node *ni = bf->bf_node;
	struct ieee80211com *ic = ni->ni_ic;
	struct mbuf *m = bf->bf_m;
	struct ath_hal *ah = sc->sc_ah;
	struct ath_desc *ds;
	int flags, antenna;
	const HAL_RATE_TABLE *rt;
	u_int8_t rix, rate;

	DPRINTF(sc, ATH_DEBUG_BEACON, "%s: m %p len %u\n",
		__func__, m, m->m_len);

	/* setup descriptors */
	ds = bf->bf_desc;

	flags = HAL_TXDESC_NOACK;
	if (ic->ic_opmode == IEEE80211_M_IBSS && sc->sc_hasveol) {
		ds->ds_link = HTOAH32(bf->bf_daddr);	/* self-linked */
		flags |= HAL_TXDESC_VEOL;
		/*
		 * Let hardware handle antenna switching unless
		 * the user has selected a transmit antenna
		 * (sc_txantenna is not 0).
		 */
		antenna = sc->sc_txantenna;
	} else {
		ds->ds_link = 0;
		/*
		 * Switch antenna every 4 beacons, unless the user
		 * has selected a transmit antenna (sc_txantenna
		 * is not 0).
		 *
		 * XXX assumes two antenna
		 */
		if (sc->sc_txantenna == 0)
			antenna = (sc->sc_stats.ast_be_xmit & 4 ? 2 : 1);
		else
			antenna = sc->sc_txantenna;
	}

	KASSERTMSG(bf->bf_nseg == 1,
		"multi-segment beacon frame; nseg %u", bf->bf_nseg);
	ds->ds_data = bf->bf_segs[0].ds_addr;
	/*
	 * Calculate rate code.
	 * XXX everything at min xmit rate
	 */
	rix = sc->sc_minrateix;
	rt = sc->sc_currates;
	rate = rt->info[rix].rateCode;
	if (USE_SHPREAMBLE(ic))
		rate |= rt->info[rix].shortPreamble;
	ath_hal_setuptxdesc(ah, ds
		, m->m_len + IEEE80211_CRC_LEN	/* frame length */
		, sizeof(struct ieee80211_frame)/* header length */
		, HAL_PKT_TYPE_BEACON		/* Atheros packet type */
		, ni->ni_txpower		/* txpower XXX */
		, rate, 1			/* series 0 rate/tries */
		, HAL_TXKEYIX_INVALID		/* no encryption */
		, antenna			/* antenna mode */
		, flags				/* no ack, veol for beacons */
		, 0				/* rts/cts rate */
		, 0				/* rts/cts duration */
	);
	/* NB: beacon's BufLen must be a multiple of 4 bytes */
	ath_hal_filltxdesc(ah, ds
		, roundup(m->m_len, 4)		/* buffer length */
		, AH_TRUE			/* first segment */
		, AH_TRUE			/* last segment */
		, ds				/* first descriptor */
	);

	/* NB: The desc swap function becomes void, if descriptor swapping
	 * is not enabled
	 */
	ath_desc_swap(ds);

#undef USE_SHPREAMBLE
}

/*
 * Transmit a beacon frame at SWBA.  Dynamic updates to the
 * frame contents are done as needed and the slot time is
 * also adjusted based on current state.
 */
static void
ath_beacon_proc(void *arg, int pending)
{
	struct ath_softc *sc = arg;
	struct ath_buf *bf = STAILQ_FIRST(&sc->sc_bbuf);
	struct ieee80211_node *ni = bf->bf_node;
	struct ieee80211com *ic = ni->ni_ic;
	struct ath_hal *ah = sc->sc_ah;
	struct mbuf *m;
	int ncabq, error, otherant;

	DPRINTF(sc, ATH_DEBUG_BEACON_PROC, "%s: pending %u\n",
		__func__, pending);

	if (ic->ic_opmode == IEEE80211_M_STA ||
	    ic->ic_opmode == IEEE80211_M_MONITOR ||
	    bf == NULL || bf->bf_m == NULL) {
		DPRINTF(sc, ATH_DEBUG_ANY, "%s: ic_flags=%x bf=%p bf_m=%p\n",
			__func__, ic->ic_flags, bf, bf ? bf->bf_m : NULL);
		return;
	}
	/*
	 * Check if the previous beacon has gone out.  If
	 * not don't try to post another, skip this period
	 * and wait for the next.  Missed beacons indicate
	 * a problem and should not occur.  If we miss too
	 * many consecutive beacons reset the device.
	 */
	if (ath_hal_numtxpending(ah, sc->sc_bhalq) != 0) {
		sc->sc_bmisscount++;
		DPRINTF(sc, ATH_DEBUG_BEACON_PROC,
			"%s: missed %u consecutive beacons\n",
			__func__, sc->sc_bmisscount);
		if (sc->sc_bmisscount > 3)		/* NB: 3 is a guess */
			TASK_RUN_OR_ENQUEUE(&sc->sc_bstucktask);
		return;
	}
	if (sc->sc_bmisscount != 0) {
		DPRINTF(sc, ATH_DEBUG_BEACON,
			"%s: resume beacon xmit after %u misses\n",
			__func__, sc->sc_bmisscount);
		sc->sc_bmisscount = 0;
	}

	/*
	 * Update dynamic beacon contents.  If this returns
	 * non-zero then we need to remap the memory because
	 * the beacon frame changed size (probably because
	 * of the TIM bitmap).
	 */
	m = bf->bf_m;
	ncabq = ath_hal_numtxpending(ah, sc->sc_cabq->axq_qnum);
	if (ieee80211_beacon_update(ic, bf->bf_node, &sc->sc_boff, m, ncabq)) {
		/* XXX too conservative? */
		bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
		error = bus_dmamap_load_mbuf(sc->sc_dmat, bf->bf_dmamap, m,
					     BUS_DMA_NOWAIT);
		if (error != 0) {
			if_printf(&sc->sc_if,
			    "%s: bus_dmamap_load_mbuf failed, error %u\n",
			    __func__, error);
			return;
		}
	}

	/*
	 * Handle slot time change when a non-ERP station joins/leaves
	 * an 11g network.  The 802.11 layer notifies us via callback,
	 * we mark updateslot, then wait one beacon before effecting
	 * the change.  This gives associated stations at least one
	 * beacon interval to note the state change.
	 */
	/* XXX locking */
	if (sc->sc_updateslot == UPDATE)
		sc->sc_updateslot = COMMIT;	/* commit next beacon */
	else if (sc->sc_updateslot == COMMIT)
		ath_setslottime(sc);		/* commit change to h/w */

	/*
	 * Check recent per-antenna transmit statistics and flip
	 * the default antenna if noticeably more frames went out
	 * on the non-default antenna.
	 * XXX assumes 2 anntenae
	 */
	otherant = sc->sc_defant & 1 ? 2 : 1;
	if (sc->sc_ant_tx[otherant] > sc->sc_ant_tx[sc->sc_defant] + 2)
		ath_setdefantenna(sc, otherant);
	sc->sc_ant_tx[1] = sc->sc_ant_tx[2] = 0;

	/*
	 * Construct tx descriptor.
	 */
	ath_beacon_setup(sc, bf);

	/*
	 * Stop any current dma and put the new frame on the queue.
	 * This should never fail since we check above that no frames
	 * are still pending on the queue.
	 */
	if (!ath_hal_stoptxdma(ah, sc->sc_bhalq)) {
		DPRINTF(sc, ATH_DEBUG_ANY,
			"%s: beacon queue %u did not stop?\n",
			__func__, sc->sc_bhalq);
	}
	bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap, 0,
	    bf->bf_dmamap->dm_mapsize, BUS_DMASYNC_PREWRITE);

	/*
	 * Enable the CAB queue before the beacon queue to
	 * insure cab frames are triggered by this beacon.
	 */
	if (ncabq != 0 && (sc->sc_boff.bo_tim[4] & 1))	/* NB: only at DTIM */
		ath_hal_txstart(ah, sc->sc_cabq->axq_qnum);
	ath_hal_puttxbuf(ah, sc->sc_bhalq, bf->bf_daddr);
	ath_hal_txstart(ah, sc->sc_bhalq);
	DPRINTF(sc, ATH_DEBUG_BEACON_PROC,
	    "%s: TXDP[%u] = %" PRIx64 " (%p)\n", __func__,
	    sc->sc_bhalq, (uint64_t)bf->bf_daddr, bf->bf_desc);

	sc->sc_stats.ast_be_xmit++;
}

/*
 * Reset the hardware after detecting beacons have stopped.
 */
static void
ath_bstuck_proc(void *arg, int pending)
{
	struct ath_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_if;

	if_printf(ifp, "stuck beacon; resetting (bmiss count %u)\n",
		sc->sc_bmisscount);
	ath_reset(ifp);
}

/*
 * Reclaim beacon resources.
 */
static void
ath_beacon_free(struct ath_softc *sc)
{
	struct ath_buf *bf;

	STAILQ_FOREACH(bf, &sc->sc_bbuf, bf_list) {
		if (bf->bf_m != NULL) {
			bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
			m_freem(bf->bf_m);
			bf->bf_m = NULL;
		}
		if (bf->bf_node != NULL) {
			ieee80211_free_node(bf->bf_node);
			bf->bf_node = NULL;
		}
	}
}

/*
 * Configure the beacon and sleep timers.
 *
 * When operating as an AP this resets the TSF and sets
 * up the hardware to notify us when we need to issue beacons.
 *
 * When operating in station mode this sets up the beacon
 * timers according to the timestamp of the last received
 * beacon and the current TSF, configures PCF and DTIM
 * handling, programs the sleep registers so the hardware
 * will wakeup in time to receive beacons, and configures
 * the beacon miss handling so we'll receive a BMISS
 * interrupt when we stop seeing beacons from the AP
 * we've associated with.
 */
static void
ath_beacon_config(struct ath_softc *sc)
{
#define	TSF_TO_TU(_h,_l) \
	((((u_int32_t)(_h)) << 22) | (((u_int32_t)(_l)) >> 10))
#define	FUDGE	2
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = ic->ic_bss;
	u_int32_t nexttbtt, intval, tsftu;
	u_int64_t tsf;

	/* extract tstamp from last beacon and convert to TU */
	nexttbtt = TSF_TO_TU(LE_READ_4(ni->ni_tstamp.data + 4),
			     LE_READ_4(ni->ni_tstamp.data));
	/* NB: the beacon interval is kept internally in TU's */
	intval = ni->ni_intval & HAL_BEACON_PERIOD;
	if (nexttbtt == 0)		/* e.g. for ap mode */
		nexttbtt = intval;
	else if (intval)		/* NB: can be 0 for monitor mode */
		nexttbtt = roundup(nexttbtt, intval);
	DPRINTF(sc, ATH_DEBUG_BEACON, "%s: nexttbtt %u intval %u (%u)\n",
		__func__, nexttbtt, intval, ni->ni_intval);
	if (ic->ic_opmode == IEEE80211_M_STA) {
		HAL_BEACON_STATE bs;
		int dtimperiod, dtimcount;
		int cfpperiod, cfpcount;

		/*
		 * Setup dtim and cfp parameters according to
		 * last beacon we received (which may be none).
		 */
		dtimperiod = ni->ni_dtim_period;
		if (dtimperiod <= 0)		/* NB: 0 if not known */
			dtimperiod = 1;
		dtimcount = ni->ni_dtim_count;
		if (dtimcount >= dtimperiod)	/* NB: sanity check */
			dtimcount = 0;		/* XXX? */
		cfpperiod = 1;			/* NB: no PCF support yet */
		cfpcount = 0;
		/*
		 * Pull nexttbtt forward to reflect the current
		 * TSF and calculate dtim+cfp state for the result.
		 */
		tsf = ath_hal_gettsf64(ah);
		tsftu = TSF_TO_TU(tsf>>32, tsf) + FUDGE;
		do {
			nexttbtt += intval;
			if (--dtimcount < 0) {
				dtimcount = dtimperiod - 1;
				if (--cfpcount < 0)
					cfpcount = cfpperiod - 1;
			}
		} while (nexttbtt < tsftu);
		memset(&bs, 0, sizeof(bs));
		bs.bs_intval = intval;
		bs.bs_nexttbtt = nexttbtt;
		bs.bs_dtimperiod = dtimperiod*intval;
		bs.bs_nextdtim = bs.bs_nexttbtt + dtimcount*intval;
		bs.bs_cfpperiod = cfpperiod*bs.bs_dtimperiod;
		bs.bs_cfpnext = bs.bs_nextdtim + cfpcount*bs.bs_dtimperiod;
		bs.bs_cfpmaxduration = 0;
#if 0
		/*
		 * The 802.11 layer records the offset to the DTIM
		 * bitmap while receiving beacons; use it here to
		 * enable h/w detection of our AID being marked in
		 * the bitmap vector (to indicate frames for us are
		 * pending at the AP).
		 * XXX do DTIM handling in s/w to WAR old h/w bugs
		 * XXX enable based on h/w rev for newer chips
		 */
		bs.bs_timoffset = ni->ni_timoff;
#endif
		/*
		 * Calculate the number of consecutive beacons to miss
		 * before taking a BMISS interrupt.  The configuration
		 * is specified in ms, so we need to convert that to
		 * TU's and then calculate based on the beacon interval.
		 * Note that we clamp the result to at most 10 beacons.
		 */
		bs.bs_bmissthreshold = howmany(ic->ic_bmisstimeout, intval);
		if (bs.bs_bmissthreshold > 10)
			bs.bs_bmissthreshold = 10;
		else if (bs.bs_bmissthreshold <= 0)
			bs.bs_bmissthreshold = 1;

		/*
		 * Calculate sleep duration.  The configuration is
		 * given in ms.  We insure a multiple of the beacon
		 * period is used.  Also, if the sleep duration is
		 * greater than the DTIM period then it makes senses
		 * to make it a multiple of that.
		 *
		 * XXX fixed at 100ms
		 */
		bs.bs_sleepduration =
			roundup(IEEE80211_MS_TO_TU(100), bs.bs_intval);
		if (bs.bs_sleepduration > bs.bs_dtimperiod)
			bs.bs_sleepduration = roundup(bs.bs_sleepduration, bs.bs_dtimperiod);

		DPRINTF(sc, ATH_DEBUG_BEACON,
			"%s: tsf %ju tsf:tu %u intval %u nexttbtt %u dtim %u nextdtim %u bmiss %u sleep %u cfp:period %u maxdur %u next %u timoffset %u\n"
			, __func__
			, tsf, tsftu
			, bs.bs_intval
			, bs.bs_nexttbtt
			, bs.bs_dtimperiod
			, bs.bs_nextdtim
			, bs.bs_bmissthreshold
			, bs.bs_sleepduration
			, bs.bs_cfpperiod
			, bs.bs_cfpmaxduration
			, bs.bs_cfpnext
			, bs.bs_timoffset
		);
		ath_hal_intrset(ah, 0);
		ath_hal_beacontimers(ah, &bs);
		sc->sc_imask |= HAL_INT_BMISS;
		ath_hal_intrset(ah, sc->sc_imask);
	} else {
		ath_hal_intrset(ah, 0);
		if (nexttbtt == intval)
			intval |= HAL_BEACON_RESET_TSF;
		if (ic->ic_opmode == IEEE80211_M_IBSS) {
			/*
			 * In IBSS mode enable the beacon timers but only
			 * enable SWBA interrupts if we need to manually
			 * prepare beacon frames.  Otherwise we use a
			 * self-linked tx descriptor and let the hardware
			 * deal with things.
			 */
			intval |= HAL_BEACON_ENA;
			if (!sc->sc_hasveol)
				sc->sc_imask |= HAL_INT_SWBA;
			if ((intval & HAL_BEACON_RESET_TSF) == 0) {
				/*
				 * Pull nexttbtt forward to reflect
				 * the current TSF.
				 */
				tsf = ath_hal_gettsf64(ah);
				tsftu = TSF_TO_TU(tsf>>32, tsf) + FUDGE;
				do {
					nexttbtt += intval;
				} while (nexttbtt < tsftu);
			}
			ath_beaconq_config(sc);
		} else if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
			/*
			 * In AP mode we enable the beacon timers and
			 * SWBA interrupts to prepare beacon frames.
			 */
			intval |= HAL_BEACON_ENA;
			sc->sc_imask |= HAL_INT_SWBA;	/* beacon prepare */
			ath_beaconq_config(sc);
		}
		ath_hal_beaconinit(ah, nexttbtt, intval);
		sc->sc_bmisscount = 0;
		ath_hal_intrset(ah, sc->sc_imask);
		/*
		 * When using a self-linked beacon descriptor in
		 * ibss mode load it once here.
		 */
		if (ic->ic_opmode == IEEE80211_M_IBSS && sc->sc_hasveol)
			ath_beacon_proc(sc, 0);
	}
	sc->sc_syncbeacon = 0;
#undef UNDEF
#undef TSF_TO_TU
}

static int
ath_descdma_setup(struct ath_softc *sc,
	struct ath_descdma *dd, ath_bufhead *head,
	const char *name, int nbuf, int ndesc)
{
#define	DS2PHYS(_dd, _ds) \
	((_dd)->dd_desc_paddr + ((char *)(_ds) - (char *)(_dd)->dd_desc))
	struct ifnet *ifp = &sc->sc_if;
	struct ath_desc *ds;
	struct ath_buf *bf;
	int i, bsize, error;

	DPRINTF(sc, ATH_DEBUG_RESET, "%s: %s DMA: %u buffers %u desc/buf\n",
	    __func__, name, nbuf, ndesc);

	dd->dd_name = name;
	dd->dd_desc_len = sizeof(struct ath_desc) * nbuf * ndesc;

	/*
	 * Setup DMA descriptor area.
	 */
	dd->dd_dmat = sc->sc_dmat;

	error = bus_dmamem_alloc(dd->dd_dmat, dd->dd_desc_len, PAGE_SIZE,
	    0, &dd->dd_dseg, 1, &dd->dd_dnseg, 0);

	if (error != 0) {
		if_printf(ifp, "unable to alloc memory for %u %s descriptors, "
			"error %u\n", nbuf * ndesc, dd->dd_name, error);
		goto fail0;
	}

	error = bus_dmamem_map(dd->dd_dmat, &dd->dd_dseg, dd->dd_dnseg,
	    dd->dd_desc_len, (void **)&dd->dd_desc, BUS_DMA_COHERENT);
	if (error != 0) {
		if_printf(ifp, "unable to map %u %s descriptors, error = %u\n",
		    nbuf * ndesc, dd->dd_name, error);
		goto fail1;
	}

	/* allocate descriptors */
	error = bus_dmamap_create(dd->dd_dmat, dd->dd_desc_len, 1,
	    dd->dd_desc_len, 0, BUS_DMA_NOWAIT, &dd->dd_dmamap);
	if (error != 0) {
		if_printf(ifp, "unable to create dmamap for %s descriptors, "
			"error %u\n", dd->dd_name, error);
		goto fail2;
	}

	error = bus_dmamap_load(dd->dd_dmat, dd->dd_dmamap, dd->dd_desc,
	    dd->dd_desc_len, NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		if_printf(ifp, "unable to map %s descriptors, error %u\n",
			dd->dd_name, error);
		goto fail3;
	}

	ds = dd->dd_desc;
	dd->dd_desc_paddr = dd->dd_dmamap->dm_segs[0].ds_addr;
	DPRINTF(sc, ATH_DEBUG_RESET,
	    "%s: %s DMA map: %p (%lu) -> %" PRIx64 " (%lu)\n",
	    __func__, dd->dd_name, ds, (u_long) dd->dd_desc_len,
	    (uint64_t) dd->dd_desc_paddr, /*XXX*/ (u_long) dd->dd_desc_len);

	/* allocate rx buffers */
	bsize = sizeof(struct ath_buf) * nbuf;
	bf = malloc(bsize, M_ATHDEV, M_NOWAIT | M_ZERO);
	if (bf == NULL) {
		if_printf(ifp, "malloc of %s buffers failed, size %u\n",
			dd->dd_name, bsize);
		goto fail4;
	}
	dd->dd_bufptr = bf;

	STAILQ_INIT(head);
	for (i = 0; i < nbuf; i++, bf++, ds += ndesc) {
		bf->bf_desc = ds;
		bf->bf_daddr = DS2PHYS(dd, ds);
		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, ndesc,
				MCLBYTES, 0, BUS_DMA_NOWAIT, &bf->bf_dmamap);
		if (error != 0) {
			if_printf(ifp, "unable to create dmamap for %s "
				"buffer %u, error %u\n", dd->dd_name, i, error);
			ath_descdma_cleanup(sc, dd, head);
			return error;
		}
		STAILQ_INSERT_TAIL(head, bf, bf_list);
	}
	return 0;
fail4:
	bus_dmamap_unload(dd->dd_dmat, dd->dd_dmamap);
fail3:
	bus_dmamap_destroy(dd->dd_dmat, dd->dd_dmamap);
fail2:
	bus_dmamem_unmap(dd->dd_dmat, (void *)dd->dd_desc, dd->dd_desc_len);
fail1:
	bus_dmamem_free(dd->dd_dmat, &dd->dd_dseg, dd->dd_dnseg);
fail0:
	memset(dd, 0, sizeof(*dd));
	return error;
#undef DS2PHYS
}

static void
ath_descdma_cleanup(struct ath_softc *sc,
	struct ath_descdma *dd, ath_bufhead *head)
{
	struct ath_buf *bf;
	struct ieee80211_node *ni;

	bus_dmamap_unload(dd->dd_dmat, dd->dd_dmamap);
	bus_dmamap_destroy(dd->dd_dmat, dd->dd_dmamap);
	bus_dmamem_unmap(dd->dd_dmat, (void *)dd->dd_desc, dd->dd_desc_len);
	bus_dmamem_free(dd->dd_dmat, &dd->dd_dseg, dd->dd_dnseg);

	STAILQ_FOREACH(bf, head, bf_list) {
		if (bf->bf_m) {
			m_freem(bf->bf_m);
			bf->bf_m = NULL;
		}
		if (bf->bf_dmamap != NULL) {
			bus_dmamap_destroy(sc->sc_dmat, bf->bf_dmamap);
			bf->bf_dmamap = NULL;
		}
		ni = bf->bf_node;
		bf->bf_node = NULL;
		if (ni != NULL) {
			/*
			 * Reclaim node reference.
			 */
			ieee80211_free_node(ni);
		}
	}

	STAILQ_INIT(head);
	free(dd->dd_bufptr, M_ATHDEV);
	memset(dd, 0, sizeof(*dd));
}

static int
ath_desc_alloc(struct ath_softc *sc)
{
	int error;

	error = ath_descdma_setup(sc, &sc->sc_rxdma, &sc->sc_rxbuf,
			"rx", ath_rxbuf, 1);
	if (error != 0)
		return error;

	error = ath_descdma_setup(sc, &sc->sc_txdma, &sc->sc_txbuf,
			"tx", ath_txbuf, ATH_TXDESC);
	if (error != 0) {
		ath_descdma_cleanup(sc, &sc->sc_rxdma, &sc->sc_rxbuf);
		return error;
	}

	error = ath_descdma_setup(sc, &sc->sc_bdma, &sc->sc_bbuf,
			"beacon", 1, 1);
	if (error != 0) {
		ath_descdma_cleanup(sc, &sc->sc_txdma, &sc->sc_txbuf);
		ath_descdma_cleanup(sc, &sc->sc_rxdma, &sc->sc_rxbuf);
		return error;
	}
	return 0;
}

static void
ath_desc_free(struct ath_softc *sc)
{

	if (sc->sc_bdma.dd_desc_len != 0)
		ath_descdma_cleanup(sc, &sc->sc_bdma, &sc->sc_bbuf);
	if (sc->sc_txdma.dd_desc_len != 0)
		ath_descdma_cleanup(sc, &sc->sc_txdma, &sc->sc_txbuf);
	if (sc->sc_rxdma.dd_desc_len != 0)
		ath_descdma_cleanup(sc, &sc->sc_rxdma, &sc->sc_rxbuf);
}

static struct ieee80211_node *
ath_node_alloc(struct ieee80211_node_table *nt)
{
	struct ieee80211com *ic = nt->nt_ic;
	struct ath_softc *sc = ic->ic_ifp->if_softc;
	const size_t space = sizeof(struct ath_node) + sc->sc_rc->arc_space;
	struct ath_node *an;

	an = malloc(space, M_80211_NODE, M_NOWAIT|M_ZERO);
	if (an == NULL) {
		/* XXX stat+msg */
		return NULL;
	}
	an->an_avgrssi = ATH_RSSI_DUMMY_MARKER;
	ath_rate_node_init(sc, an);

	DPRINTF(sc, ATH_DEBUG_NODE, "%s: an %p\n", __func__, an);
	return &an->an_node;
}

static void
ath_node_free(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
        struct ath_softc *sc = ic->ic_ifp->if_softc;

	DPRINTF(sc, ATH_DEBUG_NODE, "%s: ni %p\n", __func__, ni);

	ath_rate_node_cleanup(sc, ATH_NODE(ni));
	sc->sc_node_free(ni);
}

static u_int8_t
ath_node_getrssi(const struct ieee80211_node *ni)
{
#define	HAL_EP_RND(x, mul) \
	((((x)%(mul)) >= ((mul)/2)) ? ((x) + ((mul) - 1)) / (mul) : (x)/(mul))
	u_int32_t avgrssi = ATH_NODE_CONST(ni)->an_avgrssi;
	int32_t rssi;

	/*
	 * When only one frame is received there will be no state in
	 * avgrssi so fallback on the value recorded by the 802.11 layer.
	 */
	if (avgrssi != ATH_RSSI_DUMMY_MARKER)
		rssi = HAL_EP_RND(avgrssi, HAL_RSSI_EP_MULTIPLIER);
	else
		rssi = ni->ni_rssi;
	return rssi < 0 ? 0 : rssi > 127 ? 127 : rssi;
#undef HAL_EP_RND
}

static int
ath_rxbuf_init(struct ath_softc *sc, struct ath_buf *bf)
{
	struct ath_hal *ah = sc->sc_ah;
	int error;
	struct mbuf *m;
	struct ath_desc *ds;

	m = bf->bf_m;
	if (m == NULL) {
		/*
		 * NB: by assigning a page to the rx dma buffer we
		 * implicitly satisfy the Atheros requirement that
		 * this buffer be cache-line-aligned and sized to be
		 * multiple of the cache line size.  Not doing this
		 * causes weird stuff to happen (for the 5210 at least).
		 */
		m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		if (m == NULL) {
			DPRINTF(sc, ATH_DEBUG_ANY,
				"%s: no mbuf/cluster\n", __func__);
			sc->sc_stats.ast_rx_nombuf++;
			return ENOMEM;
		}
		bf->bf_m = m;
		m->m_pkthdr.len = m->m_len = m->m_ext.ext_size;

		error = bus_dmamap_load_mbuf(sc->sc_dmat,
					     bf->bf_dmamap, m,
					     BUS_DMA_NOWAIT);
		if (error != 0) {
			DPRINTF(sc, ATH_DEBUG_ANY,
			    "%s: bus_dmamap_load_mbuf failed; error %d\n",
			    __func__, error);
			sc->sc_stats.ast_rx_busdma++;
			return error;
		}
		KASSERTMSG(bf->bf_nseg == 1,
			"multi-segment packet; nseg %u", bf->bf_nseg);
	}
	bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap, 0,
	    bf->bf_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);

	/*
	 * Setup descriptors.  For receive we always terminate
	 * the descriptor list with a self-linked entry so we'll
	 * not get overrun under high load (as can happen with a
	 * 5212 when ANI processing enables PHY error frames).
	 *
	 * To insure the last descriptor is self-linked we create
	 * each descriptor as self-linked and add it to the end.  As
	 * each additional descriptor is added the previous self-linked
	 * entry is ``fixed'' naturally.  This should be safe even
	 * if DMA is happening.  When processing RX interrupts we
	 * never remove/process the last, self-linked, entry on the
	 * descriptor list.  This insures the hardware always has
	 * someplace to write a new frame.
	 */
	ds = bf->bf_desc;
	ds->ds_link = HTOAH32(bf->bf_daddr);	/* link to self */
	ds->ds_data = bf->bf_segs[0].ds_addr;
	/* ds->ds_vdata = mtod(m, void *);	for radar */
	ath_hal_setuprxdesc(ah, ds
		, m->m_len		/* buffer size */
		, 0
	);

	if (sc->sc_rxlink != NULL)
		*sc->sc_rxlink = bf->bf_daddr;
	sc->sc_rxlink = &ds->ds_link;
	return 0;
}

/*
 * Extend 15-bit time stamp from rx descriptor to
 * a full 64-bit TSF using the specified TSF.
 */
static inline u_int64_t
ath_extend_tsf(u_int32_t rstamp, u_int64_t tsf)
{
	if ((tsf & 0x7fff) < rstamp)
		tsf -= 0x8000;
	return ((tsf &~ 0x7fff) | rstamp);
}

/*
 * Intercept management frames to collect beacon rssi data
 * and to do ibss merges.
 */
static void
ath_recv_mgmt(struct ieee80211com *ic, struct mbuf *m,
	struct ieee80211_node *ni,
	int subtype, int rssi, u_int32_t rstamp)
{
	struct ath_softc *sc = ic->ic_ifp->if_softc;

	/*
	 * Call up first so subsequent work can use information
	 * potentially stored in the node (e.g. for ibss merge).
	 */
	sc->sc_recv_mgmt(ic, m, ni, subtype, rssi, rstamp);
	switch (subtype) {
	case IEEE80211_FC0_SUBTYPE_BEACON:
		/* update rssi statistics for use by the hal */
		ATH_RSSI_LPF(sc->sc_halstats.ns_avgbrssi, rssi);
		if (sc->sc_syncbeacon &&
		    ni == ic->ic_bss && ic->ic_state == IEEE80211_S_RUN) {
			/*
			 * Resync beacon timers using the tsf of the beacon
			 * frame we just received.
			 */
			ath_beacon_config(sc);
		}
		/* fall thru... */
	case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
		if (ic->ic_opmode == IEEE80211_M_IBSS &&
		    ic->ic_state == IEEE80211_S_RUN) {
			u_int64_t tsf = ath_extend_tsf(rstamp,
				ath_hal_gettsf64(sc->sc_ah));

			/*
			 * Handle ibss merge as needed; check the tsf on the
			 * frame before attempting the merge.  The 802.11 spec
			 * says the station should change its bssid to match
			 * the oldest station with the same ssid, where oldest
			 * is determined by the tsf.  Note that hardware
			 * reconfiguration happens through callback to
			 * ath_newstate as the state machine will go from
			 * RUN -> RUN when this happens.
			 */
			if (le64toh(ni->ni_tstamp.tsf) >= tsf) {
				DPRINTF(sc, ATH_DEBUG_STATE,
				    "ibss merge, rstamp %u tsf %ju "
				    "tstamp %ju\n", rstamp, (uintmax_t)tsf,
				    (uintmax_t)ni->ni_tstamp.tsf);
				(void) ieee80211_ibss_merge(ni);
			}
		}
		break;
	}
}

/*
 * Set the default antenna.
 */
static void
ath_setdefantenna(struct ath_softc *sc, u_int antenna)
{
	struct ath_hal *ah = sc->sc_ah;

	/* XXX block beacon interrupts */
	ath_hal_setdefantenna(ah, antenna);
	if (sc->sc_defant != antenna)
		sc->sc_stats.ast_ant_defswitch++;
	sc->sc_defant = antenna;
	sc->sc_rxotherant = 0;
}

static void
ath_handle_micerror(struct ieee80211com *ic,
	struct ieee80211_frame *wh, int keyix)
{
	struct ieee80211_node *ni;

	/* XXX recheck MIC to deal w/ chips that lie */
	/* XXX discard MIC errors on !data frames */
	ni = ieee80211_find_rxnode_withkey(ic, (const struct ieee80211_frame_min *) wh, keyix);
	if (ni != NULL) {
		ieee80211_notify_michael_failure(ic, wh, keyix);
		ieee80211_free_node(ni);
	}
}

static void
ath_rx_proc(void *arg, int npending)
{
#define	PA2DESC(_sc, _pa) \
	((struct ath_desc *)((char *)(_sc)->sc_rxdma.dd_desc + \
		((_pa) - (_sc)->sc_rxdma.dd_desc_paddr)))
	struct ath_softc *sc = arg;
	struct ath_buf *bf;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	struct ath_hal *ah = sc->sc_ah;
	struct ath_desc *ds;
	struct mbuf *m;
	struct ieee80211_node *ni;
	struct ath_node *an;
	int len, ngood, type;
	u_int phyerr;
	HAL_STATUS status;
	int16_t nf;
	u_int64_t tsf;
	uint8_t rxerr_tap, rxerr_mon;

	NET_LOCK_GIANT();		/* XXX */

	rxerr_tap =
	    (ifp->if_flags & IFF_PROMISC) ? HAL_RXERR_CRC|HAL_RXERR_PHY : 0;

	if (sc->sc_ic.ic_opmode == IEEE80211_M_MONITOR)
		rxerr_mon = HAL_RXERR_DECRYPT|HAL_RXERR_MIC;
	else if (ifp->if_flags & IFF_PROMISC)
		rxerr_tap |= HAL_RXERR_DECRYPT|HAL_RXERR_MIC;

	DPRINTF(sc, ATH_DEBUG_RX_PROC, "%s: pending %u\n", __func__, npending);
	ngood = 0;
	nf = ath_hal_getchannoise(ah, &sc->sc_curchan);
	tsf = ath_hal_gettsf64(ah);
	do {
		bf = STAILQ_FIRST(&sc->sc_rxbuf);
		if (bf == NULL) {		/* NB: shouldn't happen */
			if_printf(ifp, "%s: no buffer!\n", __func__);
			break;
		}
		ds = bf->bf_desc;
		if (ds->ds_link == bf->bf_daddr) {
			/* NB: never process the self-linked entry at the end */
			break;
		}
		m = bf->bf_m;
		if (m == NULL) {		/* NB: shouldn't happen */
			if_printf(ifp, "%s: no mbuf!\n", __func__);
			break;
		}
		/* XXX sync descriptor memory */
		/*
		 * Must provide the virtual address of the current
		 * descriptor, the physical address, and the virtual
		 * address of the next descriptor in the h/w chain.
		 * This allows the HAL to look ahead to see if the
		 * hardware is done with a descriptor by checking the
		 * done bit in the following descriptor and the address
		 * of the current descriptor the DMA engine is working
		 * on.  All this is necessary because of our use of
		 * a self-linked list to avoid rx overruns.
		 */
		status = ath_hal_rxprocdesc(ah, ds,
				bf->bf_daddr, PA2DESC(sc, ds->ds_link),
				&ds->ds_rxstat);
#ifdef AR_DEBUG
		if (sc->sc_debug & ATH_DEBUG_RECV_DESC)
			ath_printrxbuf(bf, status == HAL_OK);
#endif
		if (status == HAL_EINPROGRESS)
			break;
		STAILQ_REMOVE_HEAD(&sc->sc_rxbuf, bf_list);
		if (ds->ds_rxstat.rs_more) {
			/*
			 * Frame spans multiple descriptors; this
			 * cannot happen yet as we don't support
			 * jumbograms.  If not in monitor mode,
			 * discard the frame.
			 */
			if (ic->ic_opmode != IEEE80211_M_MONITOR) {
				sc->sc_stats.ast_rx_toobig++;
				goto rx_next;
			}
			/* fall thru for monitor mode handling... */
		} else if (ds->ds_rxstat.rs_status != 0) {
			if (ds->ds_rxstat.rs_status & HAL_RXERR_CRC)
				sc->sc_stats.ast_rx_crcerr++;
			if (ds->ds_rxstat.rs_status & HAL_RXERR_FIFO)
				sc->sc_stats.ast_rx_fifoerr++;
			if (ds->ds_rxstat.rs_status & HAL_RXERR_PHY) {
				sc->sc_stats.ast_rx_phyerr++;
				phyerr = ds->ds_rxstat.rs_phyerr & 0x1f;
				sc->sc_stats.ast_rx_phy[phyerr]++;
				goto rx_next;
			}
			if (ds->ds_rxstat.rs_status & HAL_RXERR_DECRYPT) {
				/*
				 * Decrypt error.  If the error occurred
				 * because there was no hardware key, then
				 * let the frame through so the upper layers
				 * can process it.  This is necessary for 5210
				 * parts which have no way to setup a ``clear''
				 * key cache entry.
				 *
				 * XXX do key cache faulting
				 */
				if (ds->ds_rxstat.rs_keyix == HAL_RXKEYIX_INVALID)
					goto rx_accept;
				sc->sc_stats.ast_rx_badcrypt++;
			}
			if (ds->ds_rxstat.rs_status & HAL_RXERR_MIC) {
				sc->sc_stats.ast_rx_badmic++;
				/*
				 * Do minimal work required to hand off
				 * the 802.11 header for notifcation.
				 */
				/* XXX frag's and qos frames */
				len = ds->ds_rxstat.rs_datalen;
				if (len >= sizeof (struct ieee80211_frame)) {
					bus_dmamap_sync(sc->sc_dmat,
					    bf->bf_dmamap,
					    0, bf->bf_dmamap->dm_mapsize,
					    BUS_DMASYNC_POSTREAD);
					ath_handle_micerror(ic,
					    mtod(m, struct ieee80211_frame *),
					    sc->sc_splitmic ?
						ds->ds_rxstat.rs_keyix-32 : ds->ds_rxstat.rs_keyix);
				}
			}
			ifp->if_ierrors++;
			/*
			 * Reject error frames, we normally don't want
			 * to see them in monitor mode (in monitor mode
			 * allow through packets that have crypto problems).
			 */

			if (ds->ds_rxstat.rs_status &~ (rxerr_tap|rxerr_mon))
				goto rx_next;
		}
rx_accept:
		/*
		 * Sync and unmap the frame.  At this point we're
		 * committed to passing the mbuf somewhere so clear
		 * bf_m; this means a new sk_buff must be allocated
		 * when the rx descriptor is setup again to receive
		 * another frame.
		 */
		bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap,
		    0, bf->bf_dmamap->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
		bf->bf_m = NULL;

		m->m_pkthdr.rcvif = ifp;
		len = ds->ds_rxstat.rs_datalen;
		m->m_pkthdr.len = m->m_len = len;

		sc->sc_stats.ast_ant_rx[ds->ds_rxstat.rs_antenna]++;

		if (sc->sc_drvbpf) {
			u_int8_t rix;

			/*
			 * Discard anything shorter than an ack or cts.
			 */
			if (len < IEEE80211_ACK_LEN) {
				DPRINTF(sc, ATH_DEBUG_RECV,
					"%s: runt packet %d\n",
					__func__, len);
				sc->sc_stats.ast_rx_tooshort++;
				m_freem(m);
				goto rx_next;
			}
			rix = ds->ds_rxstat.rs_rate;
			sc->sc_rx_th.wr_tsf = htole64(
				ath_extend_tsf(ds->ds_rxstat.rs_tstamp, tsf));
			sc->sc_rx_th.wr_flags = sc->sc_hwmap[rix].rxflags;
			if (ds->ds_rxstat.rs_status &
			    (HAL_RXERR_CRC|HAL_RXERR_PHY)) {
				sc->sc_rx_th.wr_flags |=
				    IEEE80211_RADIOTAP_F_BADFCS;
			}
			sc->sc_rx_th.wr_rate = sc->sc_hwmap[rix].ieeerate;
			sc->sc_rx_th.wr_antsignal = ds->ds_rxstat.rs_rssi + nf;
			sc->sc_rx_th.wr_antnoise = nf;
			sc->sc_rx_th.wr_antenna = ds->ds_rxstat.rs_antenna;

			bpf_mtap2(sc->sc_drvbpf, &sc->sc_rx_th,
			    sc->sc_rx_th_len, m);
		}

		if (ds->ds_rxstat.rs_status & rxerr_tap) {
			m_freem(m);
			goto rx_next;
		}
		/*
		 * From this point on we assume the frame is at least
		 * as large as ieee80211_frame_min; verify that.
		 */
		if (len < IEEE80211_MIN_LEN) {
			DPRINTF(sc, ATH_DEBUG_RECV, "%s: short packet %d\n",
				__func__, len);
			sc->sc_stats.ast_rx_tooshort++;
			m_freem(m);
			goto rx_next;
		}

		if (IFF_DUMPPKTS(sc, ATH_DEBUG_RECV)) {
			ieee80211_dump_pkt(mtod(m, void *), len,
				   sc->sc_hwmap[ds->ds_rxstat.rs_rate].ieeerate,
				   ds->ds_rxstat.rs_rssi);
		}

		m_adj(m, -IEEE80211_CRC_LEN);

		/*
		 * Locate the node for sender, track state, and then
		 * pass the (referenced) node up to the 802.11 layer
		 * for its use.
		 */
		ni = ieee80211_find_rxnode_withkey(ic,
			mtod(m, const struct ieee80211_frame_min *),
			ds->ds_rxstat.rs_keyix == HAL_RXKEYIX_INVALID ?
				IEEE80211_KEYIX_NONE : ds->ds_rxstat.rs_keyix);
		/*
		 * Track rx rssi and do any rx antenna management.
		 */
		an = ATH_NODE(ni);
		ATH_RSSI_LPF(an->an_avgrssi, ds->ds_rxstat.rs_rssi);
		ATH_RSSI_LPF(sc->sc_halstats.ns_avgrssi, ds->ds_rxstat.rs_rssi);
		/*
		 * Send frame up for processing.
		 */
		type = ieee80211_input(ic, m, ni,
			ds->ds_rxstat.rs_rssi, ds->ds_rxstat.rs_tstamp);
		ieee80211_free_node(ni);
		if (sc->sc_diversity) {
			/*
			 * When using fast diversity, change the default rx
			 * antenna if diversity chooses the other antenna 3
			 * times in a row.
			 */
			if (sc->sc_defant != ds->ds_rxstat.rs_antenna) {
				if (++sc->sc_rxotherant >= 3)
					ath_setdefantenna(sc,
						ds->ds_rxstat.rs_antenna);
			} else
				sc->sc_rxotherant = 0;
		}
		if (sc->sc_softled) {
			/*
			 * Blink for any data frame.  Otherwise do a
			 * heartbeat-style blink when idle.  The latter
			 * is mainly for station mode where we depend on
			 * periodic beacon frames to trigger the poll event.
			 */
			if (type == IEEE80211_FC0_TYPE_DATA) {
				sc->sc_rxrate = ds->ds_rxstat.rs_rate;
				ath_led_event(sc, ATH_LED_RX);
			} else if (ticks - sc->sc_ledevent >= sc->sc_ledidle)
				ath_led_event(sc, ATH_LED_POLL);
		}
		/*
		 * Arrange to update the last rx timestamp only for
		 * frames from our ap when operating in station mode.
		 * This assumes the rx key is always setup when associated.
		 */
		if (ic->ic_opmode == IEEE80211_M_STA &&
		    ds->ds_rxstat.rs_keyix != HAL_RXKEYIX_INVALID)
			ngood++;
rx_next:
		STAILQ_INSERT_TAIL(&sc->sc_rxbuf, bf, bf_list);
	} while (ath_rxbuf_init(sc, bf) == 0);

	/* rx signal state monitoring */
	ath_hal_rxmonitor(ah, &sc->sc_halstats, &sc->sc_curchan);
#if 0
	if (ath_hal_radar_event(ah))
		TASK_RUN_OR_ENQUEUE(&sc->sc_radartask);
#endif
	if (ngood)
		sc->sc_lastrx = tsf;

#ifdef __NetBSD__
	/* XXX Why isn't this necessary in FreeBSD? */
	if ((ifp->if_flags & IFF_OACTIVE) == 0 && !IFQ_IS_EMPTY(&ifp->if_snd))
		ath_start(ifp);
#endif /* __NetBSD__ */

	NET_UNLOCK_GIANT();		/* XXX */
#undef PA2DESC
}

/*
 * Setup a h/w transmit queue.
 */
static struct ath_txq *
ath_txq_setup(struct ath_softc *sc, int qtype, int subtype)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	struct ath_hal *ah = sc->sc_ah;
	HAL_TXQ_INFO qi;
	int qnum;

	memset(&qi, 0, sizeof(qi));
	qi.tqi_subtype = subtype;
	qi.tqi_aifs = HAL_TXQ_USEDEFAULT;
	qi.tqi_cwmin = HAL_TXQ_USEDEFAULT;
	qi.tqi_cwmax = HAL_TXQ_USEDEFAULT;
	/*
	 * Enable interrupts only for EOL and DESC conditions.
	 * We mark tx descriptors to receive a DESC interrupt
	 * when a tx queue gets deep; otherwise waiting for the
	 * EOL to reap descriptors.  Note that this is done to
	 * reduce interrupt load and this only defers reaping
	 * descriptors, never transmitting frames.  Aside from
	 * reducing interrupts this also permits more concurrency.
	 * The only potential downside is if the tx queue backs
	 * up in which case the top half of the kernel may backup
	 * due to a lack of tx descriptors.
	 */
	qi.tqi_qflags = HAL_TXQ_TXEOLINT_ENABLE | HAL_TXQ_TXDESCINT_ENABLE;
	qnum = ath_hal_setuptxqueue(ah, qtype, &qi);
	if (qnum == -1) {
		/*
		 * NB: don't print a message, this happens
		 * normally on parts with too few tx queues
		 */
		return NULL;
	}
	if (qnum >= N(sc->sc_txq)) {
		device_printf(sc->sc_dev,
			"hal qnum %u out of range, max %zu!\n",
			qnum, N(sc->sc_txq));
		ath_hal_releasetxqueue(ah, qnum);
		return NULL;
	}
	if (!ATH_TXQ_SETUP(sc, qnum)) {
		struct ath_txq *txq = &sc->sc_txq[qnum];

		txq->axq_qnum = qnum;
		txq->axq_depth = 0;
		txq->axq_intrcnt = 0;
		txq->axq_link = NULL;
		STAILQ_INIT(&txq->axq_q);
		ATH_TXQ_LOCK_INIT(sc, txq);
		sc->sc_txqsetup |= 1<<qnum;
	}
	return &sc->sc_txq[qnum];
#undef N
}

/*
 * Setup a hardware data transmit queue for the specified
 * access control.  The hal may not support all requested
 * queues in which case it will return a reference to a
 * previously setup queue.  We record the mapping from ac's
 * to h/w queues for use by ath_tx_start and also track
 * the set of h/w queues being used to optimize work in the
 * transmit interrupt handler and related routines.
 */
static int
ath_tx_setup(struct ath_softc *sc, int ac, int haltype)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	struct ath_txq *txq;

	if (ac >= N(sc->sc_ac2q)) {
		device_printf(sc->sc_dev, "AC %u out of range, max %zu!\n",
			ac, N(sc->sc_ac2q));
		return 0;
	}
	txq = ath_txq_setup(sc, HAL_TX_QUEUE_DATA, haltype);
	if (txq != NULL) {
		sc->sc_ac2q[ac] = txq;
		return 1;
	} else
		return 0;
#undef N
}

/*
 * Update WME parameters for a transmit queue.
 */
static int
ath_txq_update(struct ath_softc *sc, int ac)
{
#define	ATH_EXPONENT_TO_VALUE(v)	((1<<v)-1)
#define	ATH_TXOP_TO_US(v)		(v<<5)
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_txq *txq = sc->sc_ac2q[ac];
	struct wmeParams *wmep = &ic->ic_wme.wme_chanParams.cap_wmeParams[ac];
	struct ath_hal *ah = sc->sc_ah;
	HAL_TXQ_INFO qi;

	ath_hal_gettxqueueprops(ah, txq->axq_qnum, &qi);
	qi.tqi_aifs = wmep->wmep_aifsn;
	qi.tqi_cwmin = ATH_EXPONENT_TO_VALUE(wmep->wmep_logcwmin);
	qi.tqi_cwmax = ATH_EXPONENT_TO_VALUE(wmep->wmep_logcwmax);
	qi.tqi_burstTime = ATH_TXOP_TO_US(wmep->wmep_txopLimit);

	if (!ath_hal_settxqueueprops(ah, txq->axq_qnum, &qi)) {
		device_printf(sc->sc_dev, "unable to update hardware queue "
			"parameters for %s traffic!\n",
			ieee80211_wme_acnames[ac]);
		return 0;
	} else {
		ath_hal_resettxqueue(ah, txq->axq_qnum); /* push to h/w */
		return 1;
	}
#undef ATH_TXOP_TO_US
#undef ATH_EXPONENT_TO_VALUE
}

/*
 * Callback from the 802.11 layer to update WME parameters.
 */
static int
ath_wme_update(struct ieee80211com *ic)
{
	struct ath_softc *sc = ic->ic_ifp->if_softc;

	return !ath_txq_update(sc, WME_AC_BE) ||
	    !ath_txq_update(sc, WME_AC_BK) ||
	    !ath_txq_update(sc, WME_AC_VI) ||
	    !ath_txq_update(sc, WME_AC_VO) ? EIO : 0;
}

/*
 * Reclaim resources for a setup queue.
 */
static void
ath_tx_cleanupq(struct ath_softc *sc, struct ath_txq *txq)
{

	ath_hal_releasetxqueue(sc->sc_ah, txq->axq_qnum);
	ATH_TXQ_LOCK_DESTROY(txq);
	sc->sc_txqsetup &= ~(1<<txq->axq_qnum);
}

/*
 * Reclaim all tx queue resources.
 */
static void
ath_tx_cleanup(struct ath_softc *sc)
{
	int i;

	ATH_TXBUF_LOCK_DESTROY(sc);
	for (i = 0; i < HAL_NUM_TX_QUEUES; i++)
		if (ATH_TXQ_SETUP(sc, i))
			ath_tx_cleanupq(sc, &sc->sc_txq[i]);
}

/*
 * Defragment an mbuf chain, returning at most maxfrags separate
 * mbufs+clusters.  If this is not possible NULL is returned and
 * the original mbuf chain is left in its present (potentially
 * modified) state.  We use two techniques: collapsing consecutive
 * mbufs and replacing consecutive mbufs by a cluster.
 */
static struct mbuf *
ath_defrag(struct mbuf *m0, int how, int maxfrags)
{
	struct mbuf *m, *n, *n2, **prev;
	u_int curfrags;

	/*
	 * Calculate the current number of frags.
	 */
	curfrags = 0;
	for (m = m0; m != NULL; m = m->m_next)
		curfrags++;
	/*
	 * First, try to collapse mbufs.  Note that we always collapse
	 * towards the front so we don't need to deal with moving the
	 * pkthdr.  This may be suboptimal if the first mbuf has much
	 * less data than the following.
	 */
	m = m0;
again:
	for (;;) {
		n = m->m_next;
		if (n == NULL)
			break;
		if (n->m_len < M_TRAILINGSPACE(m)) {
			memcpy(mtod(m, char *) + m->m_len, mtod(n, void *),
				n->m_len);
			m->m_len += n->m_len;
			m->m_next = n->m_next;
			m_free(n);
			if (--curfrags <= maxfrags)
				return m0;
		} else
			m = n;
	}
	KASSERTMSG(maxfrags > 1,
		"maxfrags %u, but normal collapse failed", maxfrags);
	/*
	 * Collapse consecutive mbufs to a cluster.
	 */
	prev = &m0->m_next;		/* NB: not the first mbuf */
	while ((n = *prev) != NULL) {
		if ((n2 = n->m_next) != NULL &&
		    n->m_len + n2->m_len < MCLBYTES) {
			m = m_getcl(how, MT_DATA, 0);
			if (m == NULL)
				goto bad;
			bcopy(mtod(n, void *), mtod(m, void *), n->m_len);
			bcopy(mtod(n2, void *), mtod(m, char *) + n->m_len,
				n2->m_len);
			m->m_len = n->m_len + n2->m_len;
			m->m_next = n2->m_next;
			*prev = m;
			m_free(n);
			m_free(n2);
			if (--curfrags <= maxfrags)	/* +1 cl -2 mbufs */
				return m0;
			/*
			 * Still not there, try the normal collapse
			 * again before we allocate another cluster.
			 */
			goto again;
		}
		prev = &n->m_next;
	}
	/*
	 * No place where we can collapse to a cluster; punt.
	 * This can occur if, for example, you request 2 frags
	 * but the packet requires that both be clusters (we
	 * never reallocate the first mbuf to avoid moving the
	 * packet header).
	 */
bad:
	return NULL;
}

/*
 * Return h/w rate index for an IEEE rate (w/o basic rate bit).
 */
static int
ath_tx_findrix(const HAL_RATE_TABLE *rt, int rate)
{
	int i;

	for (i = 0; i < rt->rateCount; i++)
		if ((rt->info[i].dot11Rate & IEEE80211_RATE_VAL) == rate)
			return i;
	return 0;		/* NB: lowest rate */
}

static void
ath_freetx(struct mbuf *m)
{
	struct mbuf *next;

	do {
		next = m->m_nextpkt;
		m->m_nextpkt = NULL;
		m_freem(m);
	} while ((m = next) != NULL);
}

static int
deduct_pad_bytes(int len, int hdrlen)
{
	/* XXX I am suspicious that this code, which I extracted
	 * XXX from ath_tx_start() for reuse, does the right thing.
	 */
	return len - (hdrlen & 3);
}

static int
ath_tx_start(struct ath_softc *sc, struct ieee80211_node *ni, struct ath_buf *bf,
    struct mbuf *m0)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	struct ifnet *ifp = &sc->sc_if;
	const struct chanAccParams *cap = &ic->ic_wme.wme_chanParams;
	int i, error, iswep, ismcast, isfrag, ismrr;
	int keyix, hdrlen, pktlen, try0;
	u_int8_t rix, txrate, ctsrate;
	u_int8_t cix = 0xff;		/* NB: silence compiler */
	struct ath_desc *ds, *ds0;
	struct ath_txq *txq;
	struct ieee80211_frame *wh;
	u_int subtype, flags, ctsduration;
	HAL_PKT_TYPE atype;
	const HAL_RATE_TABLE *rt;
	HAL_BOOL shortPreamble;
	struct ath_node *an;
	struct mbuf *m;
	u_int pri;

	wh = mtod(m0, struct ieee80211_frame *);
	iswep = wh->i_fc[1] & IEEE80211_FC1_WEP;
	ismcast = IEEE80211_IS_MULTICAST(wh->i_addr1);
	isfrag = m0->m_flags & M_FRAG;
	hdrlen = ieee80211_anyhdrsize(wh);
	/*
	 * Packet length must not include any
	 * pad bytes; deduct them here.
	 */
	pktlen = deduct_pad_bytes(m0->m_pkthdr.len, hdrlen);

	if (iswep) {
		const struct ieee80211_cipher *cip;
		struct ieee80211_key *k;

		/*
		 * Construct the 802.11 header+trailer for an encrypted
		 * frame. The only reason this can fail is because of an
		 * unknown or unsupported cipher/key type.
		 */
		k = ieee80211_crypto_encap(ic, ni, m0);
		if (k == NULL) {
			/*
			 * This can happen when the key is yanked after the
			 * frame was queued.  Just discard the frame; the
			 * 802.11 layer counts failures and provides
			 * debugging/diagnostics.
			 */
			ath_freetx(m0);
			return EIO;
		}
		/*
		 * Adjust the packet + header lengths for the crypto
		 * additions and calculate the h/w key index.  When
		 * a s/w mic is done the frame will have had any mic
		 * added to it prior to entry so m0->m_pkthdr.len above will
		 * account for it. Otherwise we need to add it to the
		 * packet length.
		 */
		cip = k->wk_cipher;
		hdrlen += cip->ic_header;
		pktlen += cip->ic_header + cip->ic_trailer;
		/* NB: frags always have any TKIP MIC done in s/w */
		if ((k->wk_flags & IEEE80211_KEY_SWMIC) == 0 && !isfrag)
			pktlen += cip->ic_miclen;
		keyix = k->wk_keyix;

		/* packet header may have moved, reset our local pointer */
		wh = mtod(m0, struct ieee80211_frame *);
	} else if (ni->ni_ucastkey.wk_cipher == &ieee80211_cipher_none) {
		/*
		 * Use station key cache slot, if assigned.
		 */
		keyix = ni->ni_ucastkey.wk_keyix;
		if (keyix == IEEE80211_KEYIX_NONE)
			keyix = HAL_TXKEYIX_INVALID;
	} else
		keyix = HAL_TXKEYIX_INVALID;

	pktlen += IEEE80211_CRC_LEN;

	/*
	 * Load the DMA map so any coalescing is done.  This
	 * also calculates the number of descriptors we need.
	 */
	error = bus_dmamap_load_mbuf(sc->sc_dmat, bf->bf_dmamap, m0,
				     BUS_DMA_NOWAIT);
	if (error == EFBIG) {
		/* XXX packet requires too many descriptors */
		bf->bf_nseg = ATH_TXDESC+1;
	} else if (error != 0) {
		sc->sc_stats.ast_tx_busdma++;
		ath_freetx(m0);
		return error;
	}
	/*
	 * Discard null packets and check for packets that
	 * require too many TX descriptors.  We try to convert
	 * the latter to a cluster.
	 */
	if (error == EFBIG) {		/* too many desc's, linearize */
		sc->sc_stats.ast_tx_linear++;
		m = ath_defrag(m0, M_DONTWAIT, ATH_TXDESC);
		if (m == NULL) {
			ath_freetx(m0);
			sc->sc_stats.ast_tx_nombuf++;
			return ENOMEM;
		}
		m0 = m;
		error = bus_dmamap_load_mbuf(sc->sc_dmat, bf->bf_dmamap, m0,
					     BUS_DMA_NOWAIT);
		if (error != 0) {
			sc->sc_stats.ast_tx_busdma++;
			ath_freetx(m0);
			return error;
		}
		KASSERTMSG(bf->bf_nseg <= ATH_TXDESC,
		    "too many segments after defrag; nseg %u", bf->bf_nseg);
	} else if (bf->bf_nseg == 0) {		/* null packet, discard */
		sc->sc_stats.ast_tx_nodata++;
		ath_freetx(m0);
		return EIO;
	}
	DPRINTF(sc, ATH_DEBUG_XMIT, "%s: m %p len %u\n", __func__, m0, pktlen);
	bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap, 0,
            bf->bf_dmamap->dm_mapsize, BUS_DMASYNC_PREWRITE);
	bf->bf_m = m0;
	bf->bf_node = ni;			/* NB: held reference */

	/* setup descriptors */
	ds = bf->bf_desc;
	rt = sc->sc_currates;
	KASSERTMSG(rt != NULL, "no rate table, mode %u", sc->sc_curmode);

	/*
	 * NB: the 802.11 layer marks whether or not we should
	 * use short preamble based on the current mode and
	 * negotiated parameters.
	 */
	if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
	    (ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_PREAMBLE) && !ismcast) {
		shortPreamble = AH_TRUE;
		sc->sc_stats.ast_tx_shortpre++;
	} else {
		shortPreamble = AH_FALSE;
	}

	an = ATH_NODE(ni);
	flags = HAL_TXDESC_CLRDMASK;		/* XXX needed for crypto errs */
	ismrr = 0;				/* default no multi-rate retry*/
	/*
	 * Calculate Atheros packet type from IEEE80211 packet header,
	 * setup for rate calculations, and select h/w transmit queue.
	 */
	switch (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) {
	case IEEE80211_FC0_TYPE_MGT:
		subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
		if (subtype == IEEE80211_FC0_SUBTYPE_BEACON)
			atype = HAL_PKT_TYPE_BEACON;
		else if (subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP)
			atype = HAL_PKT_TYPE_PROBE_RESP;
		else if (subtype == IEEE80211_FC0_SUBTYPE_ATIM)
			atype = HAL_PKT_TYPE_ATIM;
		else
			atype = HAL_PKT_TYPE_NORMAL;	/* XXX */
		rix = sc->sc_minrateix;
		txrate = rt->info[rix].rateCode;
		if (shortPreamble)
			txrate |= rt->info[rix].shortPreamble;
		try0 = ATH_TXMGTTRY;
		/* NB: force all management frames to highest queue */
		if (ni->ni_flags & IEEE80211_NODE_QOS) {
			/* NB: force all management frames to highest queue */
			pri = WME_AC_VO;
		} else
			pri = WME_AC_BE;
		flags |= HAL_TXDESC_INTREQ;	/* force interrupt */
		break;
	case IEEE80211_FC0_TYPE_CTL:
		atype = HAL_PKT_TYPE_PSPOLL;	/* stop setting of duration */
		rix = sc->sc_minrateix;
		txrate = rt->info[rix].rateCode;
		if (shortPreamble)
			txrate |= rt->info[rix].shortPreamble;
		try0 = ATH_TXMGTTRY;
		/* NB: force all ctl frames to highest queue */
		if (ni->ni_flags & IEEE80211_NODE_QOS) {
			/* NB: force all ctl frames to highest queue */
			pri = WME_AC_VO;
		} else
			pri = WME_AC_BE;
		flags |= HAL_TXDESC_INTREQ;	/* force interrupt */
		break;
	case IEEE80211_FC0_TYPE_DATA:
		atype = HAL_PKT_TYPE_NORMAL;		/* default */
		/*
		 * Data frames: multicast frames go out at a fixed rate,
		 * otherwise consult the rate control module for the
		 * rate to use.
		 */
		if (ismcast) {
			/*
			 * Check mcast rate setting in case it's changed.
			 * XXX move out of fastpath
			 */
			if (ic->ic_mcast_rate != sc->sc_mcastrate) {
				sc->sc_mcastrix =
					ath_tx_findrix(rt, ic->ic_mcast_rate);
				sc->sc_mcastrate = ic->ic_mcast_rate;
			}
			rix = sc->sc_mcastrix;
			txrate = rt->info[rix].rateCode;
			try0 = 1;
		} else {
			ath_rate_findrate(sc, an, shortPreamble, pktlen,
				&rix, &try0, &txrate);
			sc->sc_txrate = txrate;		/* for LED blinking */
			if (try0 != ATH_TXMAXTRY)
				ismrr = 1;
		}
		pri = M_WME_GETAC(m0);
		if (cap->cap_wmeParams[pri].wmep_noackPolicy)
			flags |= HAL_TXDESC_NOACK;
		break;
	default:
		if_printf(ifp, "bogus frame type 0x%x (%s)\n",
			wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK, __func__);
		/* XXX statistic */
		ath_freetx(m0);
		return EIO;
	}
	txq = sc->sc_ac2q[pri];

	/*
	 * When servicing one or more stations in power-save mode
	 * multicast frames must be buffered until after the beacon.
	 * We use the CAB queue for that.
	 */
	if (ismcast && ic->ic_ps_sta) {
		txq = sc->sc_cabq;
		/* XXX? more bit in 802.11 frame header */
	}

	/*
	 * Calculate miscellaneous flags.
	 */
	if (ismcast) {
		flags |= HAL_TXDESC_NOACK;	/* no ack on broad/multicast */
	} else if (pktlen > ic->ic_rtsthreshold) {
		flags |= HAL_TXDESC_RTSENA;	/* RTS based on frame length */
		cix = rt->info[rix].controlRate;
		sc->sc_stats.ast_tx_rts++;
	}
	if (flags & HAL_TXDESC_NOACK)		/* NB: avoid double counting */
		sc->sc_stats.ast_tx_noack++;

	/*
	 * If 802.11g protection is enabled, determine whether
	 * to use RTS/CTS or just CTS.  Note that this is only
	 * done for OFDM unicast frames.
	 */
	if ((ic->ic_flags & IEEE80211_F_USEPROT) &&
	    rt->info[rix].phy == IEEE80211_T_OFDM &&
	    (flags & HAL_TXDESC_NOACK) == 0) {
		/* XXX fragments must use CCK rates w/ protection */
		if (ic->ic_protmode == IEEE80211_PROT_RTSCTS)
			flags |= HAL_TXDESC_RTSENA;
		else if (ic->ic_protmode == IEEE80211_PROT_CTSONLY)
			flags |= HAL_TXDESC_CTSENA;
		if (isfrag) {
			/*
			 * For frags it would be desirable to use the
			 * highest CCK rate for RTS/CTS.  But stations
			 * farther away may detect it at a lower CCK rate
			 * so use the configured protection rate instead
			 * (for now).
			 */
			cix = rt->info[sc->sc_protrix].controlRate;
		} else
			cix = rt->info[sc->sc_protrix].controlRate;
		sc->sc_stats.ast_tx_protect++;
	}

	/*
	 * Calculate duration.  This logically belongs in the 802.11
	 * layer but it lacks sufficient information to calculate it.
	 */
	if ((flags & HAL_TXDESC_NOACK) == 0 &&
	    (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) != IEEE80211_FC0_TYPE_CTL) {
		u_int16_t dur;
		/*
		 * XXX not right with fragmentation.
		 */
		if (shortPreamble)
			dur = rt->info[rix].spAckDuration;
		else
			dur = rt->info[rix].lpAckDuration;
		if (wh->i_fc[1] & IEEE80211_FC1_MORE_FRAG) {
			dur += dur;             /* additional SIFS+ACK */
			KASSERTMSG(m0->m_nextpkt != NULL, "no fragment");
			/*
			 * Include the size of next fragment so NAV is
			 * updated properly.  The last fragment uses only
			 * the ACK duration
			 */
			dur += ath_hal_computetxtime(ah, rt,
			    deduct_pad_bytes(m0->m_nextpkt->m_pkthdr.len,
			        hdrlen) -
			    deduct_pad_bytes(m0->m_pkthdr.len, hdrlen) + pktlen,
			    rix, shortPreamble);
		}
		if (isfrag) {
			/*
			 * Force hardware to use computed duration for next
			 * fragment by disabling multi-rate retry which updates
			 * duration based on the multi-rate duration table.
			 */
			try0 = ATH_TXMAXTRY;
		}
		*(u_int16_t *)wh->i_dur = htole16(dur);
	}

	/*
	 * Calculate RTS/CTS rate and duration if needed.
	 */
	ctsduration = 0;
	if (flags & (HAL_TXDESC_RTSENA|HAL_TXDESC_CTSENA)) {
		/*
		 * CTS transmit rate is derived from the transmit rate
		 * by looking in the h/w rate table.  We must also factor
		 * in whether or not a short preamble is to be used.
		 */
		/* NB: cix is set above where RTS/CTS is enabled */
		KASSERTMSG(cix != 0xff, "cix not setup");
		ctsrate = rt->info[cix].rateCode;
		/*
		 * Compute the transmit duration based on the frame
		 * size and the size of an ACK frame.  We call into the
		 * HAL to do the computation since it depends on the
		 * characteristics of the actual PHY being used.
		 *
		 * NB: CTS is assumed the same size as an ACK so we can
		 *     use the precalculated ACK durations.
		 */
		if (shortPreamble) {
			ctsrate |= rt->info[cix].shortPreamble;
			if (flags & HAL_TXDESC_RTSENA)		/* SIFS + CTS */
				ctsduration += rt->info[cix].spAckDuration;
			ctsduration += ath_hal_computetxtime(ah,
				rt, pktlen, rix, AH_TRUE);
			if ((flags & HAL_TXDESC_NOACK) == 0)	/* SIFS + ACK */
				ctsduration += rt->info[rix].spAckDuration;
		} else {
			if (flags & HAL_TXDESC_RTSENA)		/* SIFS + CTS */
				ctsduration += rt->info[cix].lpAckDuration;
			ctsduration += ath_hal_computetxtime(ah,
				rt, pktlen, rix, AH_FALSE);
			if ((flags & HAL_TXDESC_NOACK) == 0)	/* SIFS + ACK */
				ctsduration += rt->info[rix].lpAckDuration;
		}
		/*
		 * Must disable multi-rate retry when using RTS/CTS.
		 */
		ismrr = 0;
		try0 = ATH_TXMGTTRY;		/* XXX */
	} else
		ctsrate = 0;

	if (IFF_DUMPPKTS(sc, ATH_DEBUG_XMIT))
		ieee80211_dump_pkt(mtod(m0, void *), m0->m_len,
			sc->sc_hwmap[txrate].ieeerate, -1);
	bpf_mtap3(ic->ic_rawbpf, m0);
	if (sc->sc_drvbpf) {
		u_int64_t tsf = ath_hal_gettsf64(ah);

		sc->sc_tx_th.wt_tsf = htole64(tsf);
		sc->sc_tx_th.wt_flags = sc->sc_hwmap[txrate].txflags;
		if (iswep)
			sc->sc_tx_th.wt_flags |= IEEE80211_RADIOTAP_F_WEP;
		if (isfrag)
			sc->sc_tx_th.wt_flags |= IEEE80211_RADIOTAP_F_FRAG;
		sc->sc_tx_th.wt_rate = sc->sc_hwmap[txrate].ieeerate;
		sc->sc_tx_th.wt_txpower = ni->ni_txpower;
		sc->sc_tx_th.wt_antenna = sc->sc_txantenna;

		bpf_mtap2(sc->sc_drvbpf, &sc->sc_tx_th, sc->sc_tx_th_len, m0);
	}

	/*
	 * Determine if a tx interrupt should be generated for
	 * this descriptor.  We take a tx interrupt to reap
	 * descriptors when the h/w hits an EOL condition or
	 * when the descriptor is specifically marked to generate
	 * an interrupt.  We periodically mark descriptors in this
	 * way to insure timely replenishing of the supply needed
	 * for sending frames.  Defering interrupts reduces system
	 * load and potentially allows more concurrent work to be
	 * done but if done to aggressively can cause senders to
	 * backup.
	 *
	 * NB: use >= to deal with sc_txintrperiod changing
	 *     dynamically through sysctl.
	 */
	if (flags & HAL_TXDESC_INTREQ) {
		txq->axq_intrcnt = 0;
	} else if (++txq->axq_intrcnt >= sc->sc_txintrperiod) {
		flags |= HAL_TXDESC_INTREQ;
		txq->axq_intrcnt = 0;
	}

	/*
	 * Formulate first tx descriptor with tx controls.
	 */
	/* XXX check return value? */
	ath_hal_setuptxdesc(ah, ds
		, pktlen		/* packet length */
		, hdrlen		/* header length */
		, atype			/* Atheros packet type */
		, ni->ni_txpower	/* txpower */
		, txrate, try0		/* series 0 rate/tries */
		, keyix			/* key cache index */
		, sc->sc_txantenna	/* antenna mode */
		, flags			/* flags */
		, ctsrate		/* rts/cts rate */
		, ctsduration		/* rts/cts duration */
	);
	bf->bf_flags = flags;
	/*
	 * Setup the multi-rate retry state only when we're
	 * going to use it.  This assumes ath_hal_setuptxdesc
	 * initializes the descriptors (so we don't have to)
	 * when the hardware supports multi-rate retry and
	 * we don't use it.
	 */
	if (ismrr)
		ath_rate_setupxtxdesc(sc, an, ds, shortPreamble, rix);

	/*
	 * Fillin the remainder of the descriptor info.
	 */
	ds0 = ds;
	for (i = 0; i < bf->bf_nseg; i++, ds++) {
		ds->ds_data = bf->bf_segs[i].ds_addr;
		if (i == bf->bf_nseg - 1)
			ds->ds_link = 0;
		else
			ds->ds_link = bf->bf_daddr + sizeof(*ds) * (i + 1);
		ath_hal_filltxdesc(ah, ds
			, bf->bf_segs[i].ds_len	/* segment length */
			, i == 0		/* first segment */
			, i == bf->bf_nseg - 1	/* last segment */
			, ds0			/* first descriptor */
		);

		/* NB: The desc swap function becomes void,
		 * if descriptor swapping is not enabled
		 */
		ath_desc_swap(ds);

		DPRINTF(sc, ATH_DEBUG_XMIT,
			"%s: %d: %08x %08x %08x %08x %08x %08x\n",
			__func__, i, ds->ds_link, ds->ds_data,
			ds->ds_ctl0, ds->ds_ctl1, ds->ds_hw[0], ds->ds_hw[1]);
	}
	/*
	 * Insert the frame on the outbound list and
	 * pass it on to the hardware.
	 */
	ATH_TXQ_LOCK(txq);
	ATH_TXQ_INSERT_TAIL(txq, bf, bf_list);
	if (txq->axq_link == NULL) {
		ath_hal_puttxbuf(ah, txq->axq_qnum, bf->bf_daddr);
		DPRINTF(sc, ATH_DEBUG_XMIT,
		    "%s: TXDP[%u] = %" PRIx64 " (%p) depth %d\n", __func__,
		    txq->axq_qnum, (uint64_t)bf->bf_daddr, bf->bf_desc,
		    txq->axq_depth);
	} else {
		*txq->axq_link = HTOAH32(bf->bf_daddr);
		DPRINTF(sc, ATH_DEBUG_XMIT,
		    "%s: link[%u](%p)=%" PRIx64 " (%p) depth %d\n",
		    __func__, txq->axq_qnum, txq->axq_link,
		    (uint64_t)bf->bf_daddr, bf->bf_desc, txq->axq_depth);
	}
	txq->axq_link = &bf->bf_desc[bf->bf_nseg - 1].ds_link;
	/*
	 * The CAB queue is started from the SWBA handler since
	 * frames only go out on DTIM and to avoid possible races.
	 */
	if (txq != sc->sc_cabq)
		ath_hal_txstart(ah, txq->axq_qnum);
	ATH_TXQ_UNLOCK(txq);

	return 0;
}

/*
 * Process completed xmit descriptors from the specified queue.
 */
static int
ath_tx_processq(struct ath_softc *sc, struct ath_txq *txq)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_buf *bf;
	struct ath_desc *ds, *ds0;
	struct ieee80211_node *ni;
	struct ath_node *an;
	int sr, lr, pri, nacked;
	HAL_STATUS status;

	DPRINTF(sc, ATH_DEBUG_TX_PROC, "%s: tx queue %u head %p link %p\n",
		__func__, txq->axq_qnum,
		(void *)(uintptr_t) ath_hal_gettxbuf(sc->sc_ah, txq->axq_qnum),
		txq->axq_link);
	nacked = 0;
	for (;;) {
		ATH_TXQ_LOCK(txq);
		txq->axq_intrcnt = 0;	/* reset periodic desc intr count */
		bf = STAILQ_FIRST(&txq->axq_q);
		if (bf == NULL) {
			txq->axq_link = NULL;
			ATH_TXQ_UNLOCK(txq);
			break;
		}
		ds0 = &bf->bf_desc[0];
		ds = &bf->bf_desc[bf->bf_nseg - 1];
		status = ath_hal_txprocdesc(ah, ds, &ds->ds_txstat);
		if (sc->sc_debug & ATH_DEBUG_XMIT_DESC)
			ath_printtxbuf(bf, status == HAL_OK);
		if (status == HAL_EINPROGRESS) {
			ATH_TXQ_UNLOCK(txq);
			break;
		}
		ATH_TXQ_REMOVE_HEAD(txq, bf_list);
		ATH_TXQ_UNLOCK(txq);

		ni = bf->bf_node;
		if (ni != NULL) {
			an = ATH_NODE(ni);
			if (ds->ds_txstat.ts_status == 0) {
				u_int8_t txant = ds->ds_txstat.ts_antenna;
				sc->sc_stats.ast_ant_tx[txant]++;
				sc->sc_ant_tx[txant]++;
				if (ds->ds_txstat.ts_rate & HAL_TXSTAT_ALTRATE)
					sc->sc_stats.ast_tx_altrate++;
				sc->sc_stats.ast_tx_rssi =
					ds->ds_txstat.ts_rssi;
				ATH_RSSI_LPF(sc->sc_halstats.ns_avgtxrssi,
					ds->ds_txstat.ts_rssi);
				pri = M_WME_GETAC(bf->bf_m);
				if (pri >= WME_AC_VO)
					ic->ic_wme.wme_hipri_traffic++;
				ni->ni_inact = ni->ni_inact_reload;
			} else {
				if (ds->ds_txstat.ts_status & HAL_TXERR_XRETRY)
					sc->sc_stats.ast_tx_xretries++;
				if (ds->ds_txstat.ts_status & HAL_TXERR_FIFO)
					sc->sc_stats.ast_tx_fifoerr++;
				if (ds->ds_txstat.ts_status & HAL_TXERR_FILT)
					sc->sc_stats.ast_tx_filtered++;
			}
			sr = ds->ds_txstat.ts_shortretry;
			lr = ds->ds_txstat.ts_longretry;
			sc->sc_stats.ast_tx_shortretry += sr;
			sc->sc_stats.ast_tx_longretry += lr;
			/*
			 * Hand the descriptor to the rate control algorithm.
			 */
			if ((ds->ds_txstat.ts_status & HAL_TXERR_FILT) == 0 &&
			    (bf->bf_flags & HAL_TXDESC_NOACK) == 0) {
				/*
				 * If frame was ack'd update the last rx time
				 * used to workaround phantom bmiss interrupts.
				 */
				if (ds->ds_txstat.ts_status == 0)
					nacked++;
				ath_rate_tx_complete(sc, an, ds, ds0);
			}
			/*
			 * Reclaim reference to node.
			 *
			 * NB: the node may be reclaimed here if, for example
			 *     this is a DEAUTH message that was sent and the
			 *     node was timed out due to inactivity.
			 */
			ieee80211_free_node(ni);
		}
		bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap, 0,
		    bf->bf_dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
		m_freem(bf->bf_m);
		bf->bf_m = NULL;
		bf->bf_node = NULL;

		ATH_TXBUF_LOCK(sc);
		STAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
		sc->sc_if.if_flags &= ~IFF_OACTIVE;
		ATH_TXBUF_UNLOCK(sc);
	}
	return nacked;
}

static inline int
txqactive(struct ath_hal *ah, int qnum)
{
	u_int32_t txqs = 1<<qnum;
	ath_hal_gettxintrtxqs(ah, &txqs);
	return (txqs & (1<<qnum));
}

/*
 * Deferred processing of transmit interrupt; special-cased
 * for a single hardware transmit queue (e.g. 5210 and 5211).
 */
static void
ath_tx_proc_q0(void *arg, int npending)
{
	struct ath_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_if;

	if (txqactive(sc->sc_ah, 0) && ath_tx_processq(sc, &sc->sc_txq[0]) > 0){
		sc->sc_lastrx = ath_hal_gettsf64(sc->sc_ah);
	}
	if (txqactive(sc->sc_ah, sc->sc_cabq->axq_qnum))
		ath_tx_processq(sc, sc->sc_cabq);

	if (sc->sc_softled)
		ath_led_event(sc, ATH_LED_TX);

	ath_start(ifp);
}

/*
 * Deferred processing of transmit interrupt; special-cased
 * for four hardware queues, 0-3 (e.g. 5212 w/ WME support).
 */
static void
ath_tx_proc_q0123(void *arg, int npending)
{
	struct ath_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_if;
	int nacked;

	/*
	 * Process each active queue.
	 */
	nacked = 0;
	if (txqactive(sc->sc_ah, 0))
		nacked += ath_tx_processq(sc, &sc->sc_txq[0]);
	if (txqactive(sc->sc_ah, 1))
		nacked += ath_tx_processq(sc, &sc->sc_txq[1]);
	if (txqactive(sc->sc_ah, 2))
		nacked += ath_tx_processq(sc, &sc->sc_txq[2]);
	if (txqactive(sc->sc_ah, 3))
		nacked += ath_tx_processq(sc, &sc->sc_txq[3]);
	if (txqactive(sc->sc_ah, sc->sc_cabq->axq_qnum))
		ath_tx_processq(sc, sc->sc_cabq);
	if (nacked) {
		sc->sc_lastrx = ath_hal_gettsf64(sc->sc_ah);
	}

	if (sc->sc_softled)
		ath_led_event(sc, ATH_LED_TX);

	ath_start(ifp);
}

/*
 * Deferred processing of transmit interrupt.
 */
static void
ath_tx_proc(void *arg, int npending)
{
	struct ath_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_if;
	int i, nacked;

	/*
	 * Process each active queue.
	 */
	nacked = 0;
	for (i = 0; i < HAL_NUM_TX_QUEUES; i++)
		if (ATH_TXQ_SETUP(sc, i) && txqactive(sc->sc_ah, i))
			nacked += ath_tx_processq(sc, &sc->sc_txq[i]);
	if (nacked) {
		sc->sc_lastrx = ath_hal_gettsf64(sc->sc_ah);
	}

	if (sc->sc_softled)
		ath_led_event(sc, ATH_LED_TX);

	ath_start(ifp);
}

static void
ath_tx_draintxq(struct ath_softc *sc, struct ath_txq *txq)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211_node *ni;
	struct ath_buf *bf;
	struct ath_desc *ds;

	/*
	 * NB: this assumes output has been stopped and
	 *     we do not need to block ath_tx_tasklet
	 */
	for (;;) {
		ATH_TXQ_LOCK(txq);
		bf = STAILQ_FIRST(&txq->axq_q);
		if (bf == NULL) {
			txq->axq_link = NULL;
			ATH_TXQ_UNLOCK(txq);
			break;
		}
		ATH_TXQ_REMOVE_HEAD(txq, bf_list);
		ATH_TXQ_UNLOCK(txq);
		ds = &bf->bf_desc[bf->bf_nseg - 1];
		if (sc->sc_debug & ATH_DEBUG_RESET)
			ath_printtxbuf(bf,
				ath_hal_txprocdesc(ah, bf->bf_desc,
					&ds->ds_txstat) == HAL_OK);
		bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
		m_freem(bf->bf_m);
		bf->bf_m = NULL;
		ni = bf->bf_node;
		bf->bf_node = NULL;
		if (ni != NULL) {
			/*
			 * Reclaim node reference.
			 */
			ieee80211_free_node(ni);
		}
		ATH_TXBUF_LOCK(sc);
		STAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
		sc->sc_if.if_flags &= ~IFF_OACTIVE;
		ATH_TXBUF_UNLOCK(sc);
	}
}

static void
ath_tx_stopdma(struct ath_softc *sc, struct ath_txq *txq)
{
	struct ath_hal *ah = sc->sc_ah;

	(void) ath_hal_stoptxdma(ah, txq->axq_qnum);
	DPRINTF(sc, ATH_DEBUG_RESET, "%s: tx queue [%u] %p, link %p\n",
	    __func__, txq->axq_qnum,
	    (void *)(uintptr_t) ath_hal_gettxbuf(ah, txq->axq_qnum),
	    txq->axq_link);
}

/*
 * Drain the transmit queues and reclaim resources.
 */
static void
ath_draintxq(struct ath_softc *sc)
{
	struct ath_hal *ah = sc->sc_ah;
	int i;

	/* XXX return value */
	if (device_is_active(sc->sc_dev)) {
		/* don't touch the hardware if marked invalid */
		(void) ath_hal_stoptxdma(ah, sc->sc_bhalq);
		DPRINTF(sc, ATH_DEBUG_RESET,
		    "%s: beacon queue %p\n", __func__,
		    (void *)(uintptr_t) ath_hal_gettxbuf(ah, sc->sc_bhalq));
		for (i = 0; i < HAL_NUM_TX_QUEUES; i++)
			if (ATH_TXQ_SETUP(sc, i))
				ath_tx_stopdma(sc, &sc->sc_txq[i]);
	}
	for (i = 0; i < HAL_NUM_TX_QUEUES; i++)
		if (ATH_TXQ_SETUP(sc, i))
			ath_tx_draintxq(sc, &sc->sc_txq[i]);
}

/*
 * Disable the receive h/w in preparation for a reset.
 */
static void
ath_stoprecv(struct ath_softc *sc)
{
#define	PA2DESC(_sc, _pa) \
	((struct ath_desc *)((char *)(_sc)->sc_rxdma.dd_desc + \
		((_pa) - (_sc)->sc_rxdma.dd_desc_paddr)))
	struct ath_hal *ah = sc->sc_ah;

	ath_hal_stoppcurecv(ah);	/* disable PCU */
	ath_hal_setrxfilter(ah, 0);	/* clear recv filter */
	ath_hal_stopdmarecv(ah);	/* disable DMA engine */
	DELAY(3000);			/* 3ms is long enough for 1 frame */
	if (sc->sc_debug & (ATH_DEBUG_RESET | ATH_DEBUG_FATAL)) {
		struct ath_buf *bf;

		printf("%s: rx queue %p, link %p\n", __func__,
			(void *)(uintptr_t) ath_hal_getrxbuf(ah), sc->sc_rxlink);
		STAILQ_FOREACH(bf, &sc->sc_rxbuf, bf_list) {
			struct ath_desc *ds = bf->bf_desc;
			HAL_STATUS status = ath_hal_rxprocdesc(ah, ds,
				bf->bf_daddr, PA2DESC(sc, ds->ds_link),
				&ds->ds_rxstat);
			if (status == HAL_OK || (sc->sc_debug & ATH_DEBUG_FATAL))
				ath_printrxbuf(bf, status == HAL_OK);
		}
	}
	sc->sc_rxlink = NULL;		/* just in case */
#undef PA2DESC
}

/*
 * Enable the receive h/w following a reset.
 */
static int
ath_startrecv(struct ath_softc *sc)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ath_buf *bf;

	sc->sc_rxlink = NULL;
	STAILQ_FOREACH(bf, &sc->sc_rxbuf, bf_list) {
		int error = ath_rxbuf_init(sc, bf);
		if (error != 0) {
			DPRINTF(sc, ATH_DEBUG_RECV,
				"%s: ath_rxbuf_init failed %d\n",
				__func__, error);
			return error;
		}
	}

	bf = STAILQ_FIRST(&sc->sc_rxbuf);
	ath_hal_putrxbuf(ah, bf->bf_daddr);
	ath_hal_rxena(ah);		/* enable recv descriptors */
	ath_mode_init(sc);		/* set filters, etc. */
	ath_hal_startpcurecv(ah);	/* re-enable PCU/DMA engine */
	return 0;
}

/*
 * Update internal state after a channel change.
 */
static void
ath_chan_change(struct ath_softc *sc, struct ieee80211_channel *chan)
{
	struct ieee80211com *ic = &sc->sc_ic;
	enum ieee80211_phymode mode;
	u_int16_t flags;

	/*
	 * Change channels and update the h/w rate map
	 * if we're switching; e.g. 11a to 11b/g.
	 */
	mode = ieee80211_chan2mode(ic, chan);
	if (mode != sc->sc_curmode)
		ath_setcurmode(sc, mode);
	/*
	 * Update BPF state.  NB: ethereal et. al. don't handle
	 * merged flags well so pick a unique mode for their use.
	 */
	if (IEEE80211_IS_CHAN_A(chan))
		flags = IEEE80211_CHAN_A;
	/* XXX 11g schizophrenia */
	else if (IEEE80211_IS_CHAN_G(chan) ||
	    IEEE80211_IS_CHAN_PUREG(chan))
		flags = IEEE80211_CHAN_G;
	else
		flags = IEEE80211_CHAN_B;
	if (IEEE80211_IS_CHAN_T(chan))
		flags |= IEEE80211_CHAN_TURBO;
	sc->sc_tx_th.wt_chan_freq = sc->sc_rx_th.wr_chan_freq =
		htole16(chan->ic_freq);
	sc->sc_tx_th.wt_chan_flags = sc->sc_rx_th.wr_chan_flags =
		htole16(flags);
}

#if 0
/*
 * Poll for a channel clear indication; this is required
 * for channels requiring DFS and not previously visited
 * and/or with a recent radar detection.
 */
static void
ath_dfswait(void *arg)
{
	struct ath_softc *sc = arg;
	struct ath_hal *ah = sc->sc_ah;
	HAL_CHANNEL hchan;

	ath_hal_radar_wait(ah, &hchan);
	if (hchan.privFlags & CHANNEL_INTERFERENCE) {
		if_printf(&sc->sc_if,
		    "channel %u/0x%x/0x%x has interference\n",
		    hchan.channel, hchan.channelFlags, hchan.privFlags);
		return;
	}
	if ((hchan.privFlags & CHANNEL_DFS) == 0) {
		/* XXX should not happen */
		return;
	}
	if (hchan.privFlags & CHANNEL_DFS_CLEAR) {
		sc->sc_curchan.privFlags |= CHANNEL_DFS_CLEAR;
		sc->sc_if.if_flags &= ~IFF_OACTIVE;
		if_printf(&sc->sc_if,
		    "channel %u/0x%x/0x%x marked clear\n",
		    hchan.channel, hchan.channelFlags, hchan.privFlags);
	} else
		callout_reset(&sc->sc_dfs_ch, 2 * hz, ath_dfswait, sc);
}
#endif

/*
 * Set/change channels.  If the channel is really being changed,
 * it's done by reseting the chip.  To accomplish this we must
 * first cleanup any pending DMA, then restart stuff after a la
 * ath_init.
 */
static int
ath_chan_set(struct ath_softc *sc, struct ieee80211_channel *chan)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211com *ic = &sc->sc_ic;
	HAL_CHANNEL hchan;

	/*
	 * Convert to a HAL channel description with
	 * the flags constrained to reflect the current
	 * operating mode.
	 */
	hchan.channel = chan->ic_freq;
	hchan.channelFlags = ath_chan2flags(ic, chan);

	DPRINTF(sc, ATH_DEBUG_RESET,
	    "%s: %u (%u MHz, hal flags 0x%x) -> %u (%u MHz, hal flags 0x%x)\n",
	    __func__,
	    ath_hal_mhz2ieee(ah, sc->sc_curchan.channel,
		sc->sc_curchan.channelFlags),
	    	sc->sc_curchan.channel, sc->sc_curchan.channelFlags,
	    ath_hal_mhz2ieee(ah, hchan.channel, hchan.channelFlags),
	        hchan.channel, hchan.channelFlags);
	if (hchan.channel != sc->sc_curchan.channel ||
	    hchan.channelFlags != sc->sc_curchan.channelFlags) {
		HAL_STATUS status;

		/*
		 * To switch channels clear any pending DMA operations;
		 * wait long enough for the RX fifo to drain, reset the
		 * hardware at the new frequency, and then re-enable
		 * the relevant bits of the h/w.
		 */
		ath_hal_intrset(ah, 0);		/* disable interrupts */
		ath_draintxq(sc);		/* clear pending tx frames */
		ath_stoprecv(sc);		/* turn off frame recv */
		if (!ath_hal_reset(ah, ic->ic_opmode, &hchan, AH_TRUE, &status)) {
			if_printf(ic->ic_ifp, "%s: unable to reset "
			    "channel %u (%u MHz, flags 0x%x hal flags 0x%x)\n",
			    __func__, ieee80211_chan2ieee(ic, chan),
			    chan->ic_freq, chan->ic_flags, hchan.channelFlags);
			return EIO;
		}
		sc->sc_curchan = hchan;
		ath_update_txpow(sc);		/* update tx power state */
		ath_restore_diversity(sc);
		sc->sc_calinterval = 1;
		sc->sc_caltries = 0;

		/*
		 * Re-enable rx framework.
		 */
		if (ath_startrecv(sc) != 0) {
			if_printf(&sc->sc_if,
				"%s: unable to restart recv logic\n", __func__);
			return EIO;
		}

		/*
		 * Change channels and update the h/w rate map
		 * if we're switching; e.g. 11a to 11b/g.
		 */
		ic->ic_ibss_chan = chan;
		ath_chan_change(sc, chan);

#if 0
		/*
		 * Handle DFS required waiting period to determine
		 * if channel is clear of radar traffic.
		 */
		if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
#define	DFS_AND_NOT_CLEAR(_c) \
	(((_c)->privFlags & (CHANNEL_DFS | CHANNEL_DFS_CLEAR)) == CHANNEL_DFS)
			if (DFS_AND_NOT_CLEAR(&sc->sc_curchan)) {
				if_printf(&sc->sc_if,
					"wait for DFS clear channel signal\n");
				/* XXX stop sndq */
				sc->sc_if.if_flags |= IFF_OACTIVE;
				callout_reset(&sc->sc_dfs_ch,
					2 * hz, ath_dfswait, sc);
			} else
				callout_stop(&sc->sc_dfs_ch);
#undef DFS_NOT_CLEAR
		}
#endif

		/*
		 * Re-enable interrupts.
		 */
		ath_hal_intrset(ah, sc->sc_imask);
	}
	return 0;
}

static void
ath_next_scan(void *arg)
{
	struct ath_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	int s;

	/* don't call ath_start w/o network interrupts blocked */
	s = splnet();

	if (ic->ic_state == IEEE80211_S_SCAN)
		ieee80211_next_scan(ic);
	splx(s);
}

/*
 * Periodically recalibrate the PHY to account
 * for temperature/environment changes.
 */
static void
ath_calibrate(void *arg)
{
	struct ath_softc *sc = arg;
	struct ath_hal *ah = sc->sc_ah;
	HAL_BOOL iqCalDone;
	int s;

	sc->sc_stats.ast_per_cal++;

	 s = splnet();

	if (ath_hal_getrfgain(ah) == HAL_RFGAIN_NEED_CHANGE) {
		/*
		 * Rfgain is out of bounds, reset the chip
		 * to load new gain values.
		 */
		DPRINTF(sc, ATH_DEBUG_CALIBRATE,
			"%s: rfgain change\n", __func__);
		sc->sc_stats.ast_per_rfgain++;
		ath_reset(&sc->sc_if);
	}
	if (!ath_hal_calibrate(ah, &sc->sc_curchan, &iqCalDone)) {
		DPRINTF(sc, ATH_DEBUG_ANY,
			"%s: calibration of channel %u failed\n",
			__func__, sc->sc_curchan.channel);
		sc->sc_stats.ast_per_calfail++;
	}
	/*
	 * Calibrate noise floor data again in case of change.
	 */
	ath_hal_process_noisefloor(ah);
	/*
	 * Poll more frequently when the IQ calibration is in
	 * progress to speedup loading the final settings.
	 * We temper this aggressive polling with an exponential
	 * back off after 4 tries up to ath_calinterval.
	 */
	if (iqCalDone || sc->sc_calinterval >= ath_calinterval) {
		sc->sc_caltries = 0;
		sc->sc_calinterval = ath_calinterval;
	} else if (sc->sc_caltries > 4) {
		sc->sc_caltries = 0;
		sc->sc_calinterval <<= 1;
		if (sc->sc_calinterval > ath_calinterval)
			sc->sc_calinterval = ath_calinterval;
	}
	KASSERTMSG(0 < sc->sc_calinterval &&
	           sc->sc_calinterval <= ath_calinterval,
		   "bad calibration interval %u", sc->sc_calinterval);

	DPRINTF(sc, ATH_DEBUG_CALIBRATE,
		"%s: next +%u (%siqCalDone tries %u)\n", __func__,
		sc->sc_calinterval, iqCalDone ? "" : "!", sc->sc_caltries);
	sc->sc_caltries++;
	callout_reset(&sc->sc_cal_ch, sc->sc_calinterval * hz,
		ath_calibrate, sc);
	splx(s);
}

static int
ath_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct ath_softc *sc = ifp->if_softc;
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211_node *ni;
	int i, error;
	const u_int8_t *bssid;
	u_int32_t rfilt;
	static const HAL_LED_STATE leds[] = {
	    HAL_LED_INIT,	/* IEEE80211_S_INIT */
	    HAL_LED_SCAN,	/* IEEE80211_S_SCAN */
	    HAL_LED_AUTH,	/* IEEE80211_S_AUTH */
	    HAL_LED_ASSOC, 	/* IEEE80211_S_ASSOC */
	    HAL_LED_RUN, 	/* IEEE80211_S_RUN */
	};

	DPRINTF(sc, ATH_DEBUG_STATE, "%s: %s -> %s\n", __func__,
		ieee80211_state_name[ic->ic_state],
		ieee80211_state_name[nstate]);

	callout_stop(&sc->sc_scan_ch);
	callout_stop(&sc->sc_cal_ch);
#if 0	
	callout_stop(&sc->sc_dfs_ch);
#endif
	ath_hal_setledstate(ah, leds[nstate]);	/* set LED */

	if (nstate == IEEE80211_S_INIT) {
		sc->sc_imask &= ~(HAL_INT_SWBA | HAL_INT_BMISS);
		/*
		 * NB: disable interrupts so we don't rx frames.
		 */
		ath_hal_intrset(ah, sc->sc_imask &~ HAL_INT_GLOBAL);
		/*
		 * Notify the rate control algorithm.
		 */
		ath_rate_newstate(sc, nstate);
		goto done;
	}
	ni = ic->ic_bss;
	error = ath_chan_set(sc, ic->ic_curchan);
	if (error != 0)
		goto bad;
	rfilt = ath_calcrxfilter(sc, nstate);
	if (nstate == IEEE80211_S_SCAN)
		bssid = ifp->if_broadcastaddr;
	else
		bssid = ni->ni_bssid;
	ath_hal_setrxfilter(ah, rfilt);
	DPRINTF(sc, ATH_DEBUG_STATE, "%s: RX filter 0x%x bssid %s\n",
		 __func__, rfilt, ether_sprintf(bssid));

	if (nstate == IEEE80211_S_RUN && ic->ic_opmode == IEEE80211_M_STA)
		ath_hal_setassocid(ah, bssid, ni->ni_associd);
	else
		ath_hal_setassocid(ah, bssid, 0);
	if (ic->ic_flags & IEEE80211_F_PRIVACY) {
		for (i = 0; i < IEEE80211_WEP_NKID; i++)
			if (ath_hal_keyisvalid(ah, i))
				ath_hal_keysetmac(ah, i, bssid);
	}

	/*
	 * Notify the rate control algorithm so rates
	 * are setup should ath_beacon_alloc be called.
	 */
	ath_rate_newstate(sc, nstate);

	if (ic->ic_opmode == IEEE80211_M_MONITOR) {
		/* nothing to do */;
	} else if (nstate == IEEE80211_S_RUN) {
		DPRINTF(sc, ATH_DEBUG_STATE,
			"%s(RUN): ic_flags=0x%08x iv=%d bssid=%s "
			"capinfo=0x%04x chan=%d\n"
			 , __func__
			 , ic->ic_flags
			 , ni->ni_intval
			 , ether_sprintf(ni->ni_bssid)
			 , ni->ni_capinfo
			 , ieee80211_chan2ieee(ic, ic->ic_curchan));

		switch (ic->ic_opmode) {
		case IEEE80211_M_HOSTAP:
		case IEEE80211_M_IBSS:
			/*
			 * Allocate and setup the beacon frame.
			 *
			 * Stop any previous beacon DMA.  This may be
			 * necessary, for example, when an ibss merge
			 * causes reconfiguration; there will be a state
			 * transition from RUN->RUN that means we may
			 * be called with beacon transmission active.
			 */
			ath_hal_stoptxdma(ah, sc->sc_bhalq);
			ath_beacon_free(sc);
			error = ath_beacon_alloc(sc, ni);
			if (error != 0)
				goto bad;
			/*
			 * If joining an adhoc network defer beacon timer
			 * configuration to the next beacon frame so we
			 * have a current TSF to use.  Otherwise we're
			 * starting an ibss/bss so there's no need to delay.
			 */
			if (ic->ic_opmode == IEEE80211_M_IBSS &&
			    ic->ic_bss->ni_tstamp.tsf != 0)
				sc->sc_syncbeacon = 1;
			else
				ath_beacon_config(sc);
			break;
		case IEEE80211_M_STA:
			/*
			 * Allocate a key cache slot to the station.
			 */
			if ((ic->ic_flags & IEEE80211_F_PRIVACY) == 0 &&
			    sc->sc_hasclrkey &&
			    ni->ni_ucastkey.wk_keyix == IEEE80211_KEYIX_NONE)
				ath_setup_stationkey(ni);
			/*
			 * Defer beacon timer configuration to the next
			 * beacon frame so we have a current TSF to use
			 * (any TSF collected when scanning is likely old).
			 */
			sc->sc_syncbeacon = 1;
			break;
		default:
			break;
		}
		/*
		 * Let the hal process statistics collected during a
		 * scan so it can provide calibrated noise floor data.
		 */
		ath_hal_process_noisefloor(ah);
		/*
		 * Reset rssi stats; maybe not the best place...
		 */
		sc->sc_halstats.ns_avgbrssi = ATH_RSSI_DUMMY_MARKER;
		sc->sc_halstats.ns_avgrssi = ATH_RSSI_DUMMY_MARKER;
		sc->sc_halstats.ns_avgtxrssi = ATH_RSSI_DUMMY_MARKER;
	} else {
		ath_hal_intrset(ah,
			sc->sc_imask &~ (HAL_INT_SWBA | HAL_INT_BMISS));
		sc->sc_imask &= ~(HAL_INT_SWBA | HAL_INT_BMISS);
	}
done:
	/*
	 * Invoke the parent method to complete the work.
	 */
	error = sc->sc_newstate(ic, nstate, arg);
	/*
	 * Finally, start any timers.
	 */
	if (nstate == IEEE80211_S_RUN) {
		/* start periodic recalibration timer */
		callout_reset(&sc->sc_cal_ch, sc->sc_calinterval * hz,
			ath_calibrate, sc);
	} else if (nstate == IEEE80211_S_SCAN) {
		/* start ap/neighbor scan timer */
		callout_reset(&sc->sc_scan_ch, (ath_dwelltime * hz) / 1000,
			ath_next_scan, sc);
	}
bad:
	return error;
}

/*
 * Allocate a key cache slot to the station so we can
 * setup a mapping from key index to node. The key cache
 * slot is needed for managing antenna state and for
 * compression when stations do not use crypto.  We do
 * it uniliaterally here; if crypto is employed this slot
 * will be reassigned.
 */
static void
ath_setup_stationkey(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ath_softc *sc = ic->ic_ifp->if_softc;
	ieee80211_keyix keyix, rxkeyix;

	if (!ath_key_alloc(ic, &ni->ni_ucastkey, &keyix, &rxkeyix)) {
		/*
		 * Key cache is full; we'll fall back to doing
		 * the more expensive lookup in software.  Note
		 * this also means no h/w compression.
		 */
		/* XXX msg+statistic */
	} else {
		/* XXX locking? */
		ni->ni_ucastkey.wk_keyix = keyix;
		ni->ni_ucastkey.wk_rxkeyix = rxkeyix;
		/* NB: this will create a pass-thru key entry */
		ath_keyset(sc, &ni->ni_ucastkey, ni->ni_macaddr, ic->ic_bss);
	}
}

/*
 * Setup driver-specific state for a newly associated node.
 * Note that we're called also on a re-associate, the isnew
 * param tells us if this is the first time or not.
 */
static void
ath_newassoc(struct ieee80211_node *ni, int isnew)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ath_softc *sc = ic->ic_ifp->if_softc;

	ath_rate_newassoc(sc, ATH_NODE(ni), isnew);
	if (isnew &&
	    (ic->ic_flags & IEEE80211_F_PRIVACY) == 0 && sc->sc_hasclrkey) {
		KASSERTMSG(ni->ni_ucastkey.wk_keyix == IEEE80211_KEYIX_NONE,
		    "new assoc with a unicast key already setup (keyix %u)",
		    ni->ni_ucastkey.wk_keyix);
		ath_setup_stationkey(ni);
	}
}

static int
ath_getchannels(struct ath_softc *sc, u_int cc,
	HAL_BOOL outdoor, HAL_BOOL xchanmode)
{
#define	COMPAT	(CHANNEL_ALL_NOTURBO|CHANNEL_PASSIVE)
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	struct ath_hal *ah = sc->sc_ah;
	HAL_CHANNEL *chans;
	int i, ix, nchan;

	chans = malloc(IEEE80211_CHAN_MAX * sizeof(HAL_CHANNEL),
			M_TEMP, M_NOWAIT);
	if (chans == NULL) {
		if_printf(ifp, "unable to allocate channel table\n");
		return ENOMEM;
	}
	if (!ath_hal_init_channels(ah, chans, IEEE80211_CHAN_MAX, &nchan,
	    NULL, 0, NULL,
	    cc, HAL_MODE_ALL, outdoor, xchanmode)) {
		u_int32_t rd;

		(void)ath_hal_getregdomain(ah, &rd);
		if_printf(ifp, "unable to collect channel list from hal; "
			"regdomain likely %u country code %u\n", rd, cc);
		free(chans, M_TEMP);
		return EINVAL;
	}

	/*
	 * Convert HAL channels to ieee80211 ones and insert
	 * them in the table according to their channel number.
	 */
	for (i = 0; i < nchan; i++) {
		HAL_CHANNEL *c = &chans[i];
		u_int16_t flags;

		ix = ath_hal_mhz2ieee(ah, c->channel, c->channelFlags);
		if (ix > IEEE80211_CHAN_MAX) {
			if_printf(ifp, "bad hal channel %d (%u/%x) ignored\n",
				ix, c->channel, c->channelFlags);
			continue;
		}
		if (ix < 0) {
			/* XXX can't handle stuff <2400 right now */
			if (bootverbose)
				if_printf(ifp, "hal channel %d (%u/%x) "
				    "cannot be handled; ignored\n",
				    ix, c->channel, c->channelFlags);
			continue;
		}
		/*
		 * Calculate net80211 flags; most are compatible
		 * but some need massaging.  Note the static turbo
		 * conversion can be removed once net80211 is updated
		 * to understand static vs. dynamic turbo.
		 */
		flags = c->channelFlags & COMPAT;
		if (c->channelFlags & CHANNEL_STURBO)
			flags |= IEEE80211_CHAN_TURBO;
		if (ic->ic_channels[ix].ic_freq == 0) {
			ic->ic_channels[ix].ic_freq = c->channel;
			ic->ic_channels[ix].ic_flags = flags;
		} else {
			/* channels overlap; e.g. 11g and 11b */
			ic->ic_channels[ix].ic_flags |= flags;
		}
	}
	free(chans, M_TEMP);
	return 0;
#undef COMPAT
}

static void
ath_led_done(void *arg)
{
	struct ath_softc *sc = arg;

	sc->sc_blinking = 0;
}

/*
 * Turn the LED off: flip the pin and then set a timer so no
 * update will happen for the specified duration.
 */
static void
ath_led_off(void *arg)
{
	struct ath_softc *sc = arg;

	ath_hal_gpioset(sc->sc_ah, sc->sc_ledpin, !sc->sc_ledon);
	callout_reset(&sc->sc_ledtimer, sc->sc_ledoff, ath_led_done, sc);
}

/*
 * Blink the LED according to the specified on/off times.
 */
static void
ath_led_blink(struct ath_softc *sc, int on, int off)
{
	DPRINTF(sc, ATH_DEBUG_LED, "%s: on %u off %u\n", __func__, on, off);
	ath_hal_gpioset(sc->sc_ah, sc->sc_ledpin, sc->sc_ledon);
	sc->sc_blinking = 1;
	sc->sc_ledoff = off;
	callout_reset(&sc->sc_ledtimer, on, ath_led_off, sc);
}

static void
ath_led_event(struct ath_softc *sc, int event)
{

	sc->sc_ledevent = ticks;	/* time of last event */
	if (sc->sc_blinking)		/* don't interrupt active blink */
		return;
	switch (event) {
	case ATH_LED_POLL:
		ath_led_blink(sc, sc->sc_hwmap[0].ledon,
			sc->sc_hwmap[0].ledoff);
		break;
	case ATH_LED_TX:
		ath_led_blink(sc, sc->sc_hwmap[sc->sc_txrate].ledon,
			sc->sc_hwmap[sc->sc_txrate].ledoff);
		break;
	case ATH_LED_RX:
		ath_led_blink(sc, sc->sc_hwmap[sc->sc_rxrate].ledon,
			sc->sc_hwmap[sc->sc_rxrate].ledoff);
		break;
	}
}

static void
ath_update_txpow(struct ath_softc *sc)
{
#define	COMPAT	(CHANNEL_ALL_NOTURBO|CHANNEL_PASSIVE)
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	u_int32_t txpow;

	if (sc->sc_curtxpow != ic->ic_txpowlimit) {
		ath_hal_settxpowlimit(ah, ic->ic_txpowlimit);
		/* read back in case value is clamped */
		(void)ath_hal_gettxpowlimit(ah, &txpow);
		ic->ic_txpowlimit = sc->sc_curtxpow = txpow;
	}
	/*
	 * Fetch max tx power level for status requests.
	 */
	(void)ath_hal_getmaxtxpow(sc->sc_ah, &txpow);
	ic->ic_bss->ni_txpower = txpow;
}

static void
rate_setup(struct ath_softc *sc,
	const HAL_RATE_TABLE *rt, struct ieee80211_rateset *rs)
{
	int i, maxrates;

	if (rt->rateCount > IEEE80211_RATE_MAXSIZE) {
		DPRINTF(sc, ATH_DEBUG_ANY,
			"%s: rate table too small (%u > %u)\n",
		       __func__, rt->rateCount, IEEE80211_RATE_MAXSIZE);
		maxrates = IEEE80211_RATE_MAXSIZE;
	} else
		maxrates = rt->rateCount;
	for (i = 0; i < maxrates; i++)
		rs->rs_rates[i] = rt->info[i].dot11Rate;
	rs->rs_nrates = maxrates;
}

static int
ath_rate_setup(struct ath_softc *sc, u_int mode)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211com *ic = &sc->sc_ic;
	const HAL_RATE_TABLE *rt;

	switch (mode) {
	case IEEE80211_MODE_11A:
		rt = ath_hal_getratetable(ah, HAL_MODE_11A);
		break;
	case IEEE80211_MODE_11B:
		rt = ath_hal_getratetable(ah, HAL_MODE_11B);
		break;
	case IEEE80211_MODE_11G:
		rt = ath_hal_getratetable(ah, HAL_MODE_11G);
		break;
	case IEEE80211_MODE_TURBO_A:
		/* XXX until static/dynamic turbo is fixed */
		rt = ath_hal_getratetable(ah, HAL_MODE_TURBO);
		break;
	case IEEE80211_MODE_TURBO_G:
		rt = ath_hal_getratetable(ah, HAL_MODE_108G);
		break;
	default:
		DPRINTF(sc, ATH_DEBUG_ANY, "%s: invalid mode %u\n",
			__func__, mode);
		return 0;
	}
	sc->sc_rates[mode] = rt;
	if (rt != NULL) {
		rate_setup(sc, rt, &ic->ic_sup_rates[mode]);
		return 1;
	} else
		return 0;
}

static void
ath_setcurmode(struct ath_softc *sc, enum ieee80211_phymode mode)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	/* NB: on/off times from the Atheros NDIS driver, w/ permission */
	static const struct {
		u_int		rate;		/* tx/rx 802.11 rate */
		u_int16_t	timeOn;		/* LED on time (ms) */
		u_int16_t	timeOff;	/* LED off time (ms) */
	} blinkrates[] = {
		{ 108,  40,  10 },
		{  96,  44,  11 },
		{  72,  50,  13 },
		{  48,  57,  14 },
		{  36,  67,  16 },
		{  24,  80,  20 },
		{  22, 100,  25 },
		{  18, 133,  34 },
		{  12, 160,  40 },
		{  10, 200,  50 },
		{   6, 240,  58 },
		{   4, 267,  66 },
		{   2, 400, 100 },
		{   0, 500, 130 },
	};
	const HAL_RATE_TABLE *rt;
	int i, j;

	memset(sc->sc_rixmap, 0xff, sizeof(sc->sc_rixmap));
	rt = sc->sc_rates[mode];
	KASSERTMSG(rt != NULL, "no h/w rate set for phy mode %u", mode);
	for (i = 0; i < rt->rateCount; i++)
		sc->sc_rixmap[rt->info[i].dot11Rate & IEEE80211_RATE_VAL] = i;
	memset(sc->sc_hwmap, 0, sizeof(sc->sc_hwmap));
	for (i = 0; i < 32; i++) {
		u_int8_t ix = rt->rateCodeToIndex[i];
		if (ix == 0xff) {
			sc->sc_hwmap[i].ledon = (500 * hz) / 1000;
			sc->sc_hwmap[i].ledoff = (130 * hz) / 1000;
			continue;
		}
		sc->sc_hwmap[i].ieeerate =
			rt->info[ix].dot11Rate & IEEE80211_RATE_VAL;
		sc->sc_hwmap[i].txflags = IEEE80211_RADIOTAP_F_DATAPAD;
		if (rt->info[ix].shortPreamble ||
		    rt->info[ix].phy == IEEE80211_T_OFDM)
			sc->sc_hwmap[i].txflags |= IEEE80211_RADIOTAP_F_SHORTPRE;
		/* NB: receive frames include FCS */
		sc->sc_hwmap[i].rxflags = sc->sc_hwmap[i].txflags |
			IEEE80211_RADIOTAP_F_FCS;
		/* setup blink rate table to avoid per-packet lookup */
		for (j = 0; j < N(blinkrates)-1; j++)
			if (blinkrates[j].rate == sc->sc_hwmap[i].ieeerate)
				break;
		/* NB: this uses the last entry if the rate isn't found */
		/* XXX beware of overlow */
		sc->sc_hwmap[i].ledon = (blinkrates[j].timeOn * hz) / 1000;
		sc->sc_hwmap[i].ledoff = (blinkrates[j].timeOff * hz) / 1000;
	}
	sc->sc_currates = rt;
	sc->sc_curmode = mode;
	/*
	 * All protection frames are transmited at 2Mb/s for
	 * 11g, otherwise at 1Mb/s.
	 */
	if (mode == IEEE80211_MODE_11G)
		sc->sc_protrix = ath_tx_findrix(rt, 2*2);
	else
		sc->sc_protrix = ath_tx_findrix(rt, 2*1);
	/* rate index used to send management frames */
	sc->sc_minrateix = 0;
	/*
	 * Setup multicast rate state.
	 */
	/* XXX layering violation */
	sc->sc_mcastrix = ath_tx_findrix(rt, sc->sc_ic.ic_mcast_rate);
	sc->sc_mcastrate = sc->sc_ic.ic_mcast_rate;
	/* NB: caller is responsible for reseting rate control state */
#undef N
}

#ifdef AR_DEBUG
static void
ath_printrxbuf(struct ath_buf *bf, int done)
{
	struct ath_desc *ds;
	int i;

	for (i = 0, ds = bf->bf_desc; i < bf->bf_nseg; i++, ds++) {
		printf("R%d (%p %" PRIx64
		    ") %08x %08x %08x %08x %08x %08x %02x %02x %c\n", i, ds,
		    (uint64_t)bf->bf_daddr + sizeof (struct ath_desc) * i,
		    ds->ds_link, ds->ds_data,
		    ds->ds_ctl0, ds->ds_ctl1,
		    ds->ds_hw[0], ds->ds_hw[1],
		    ds->ds_rxstat.rs_status, ds->ds_rxstat.rs_keyix,
		    !done ? ' ' : (ds->ds_rxstat.rs_status == 0) ? '*' : '!');
	}
}

static void
ath_printtxbuf(struct ath_buf *bf, int done)
{
	struct ath_desc *ds;
	int i;

	for (i = 0, ds = bf->bf_desc; i < bf->bf_nseg; i++, ds++) {
		printf("T%d (%p %" PRIx64
		    ") %08x %08x %08x %08x %08x %08x %08x %08x %c\n",
		    i, ds,
		    (uint64_t)bf->bf_daddr + sizeof (struct ath_desc) * i,
		    ds->ds_link, ds->ds_data,
		    ds->ds_ctl0, ds->ds_ctl1,
		    ds->ds_hw[0], ds->ds_hw[1], ds->ds_hw[2], ds->ds_hw[3],
		    !done ? ' ' : (ds->ds_txstat.ts_status == 0) ? '*' : '!');
	}
}
#endif	/* AR_DEBUG */

static void
ath_watchdog(struct ifnet *ifp)
{
	struct ath_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_txq *axq;
	int i;

	ifp->if_timer = 0;
	if ((ifp->if_flags & IFF_RUNNING) == 0 ||
	    !device_is_active(sc->sc_dev))
		return;
	for (i = 0; i < HAL_NUM_TX_QUEUES; i++) {
		if (!ATH_TXQ_SETUP(sc, i))
			continue;
		axq = &sc->sc_txq[i];
		ATH_TXQ_LOCK(axq);
		if (axq->axq_timer == 0)
			;
		else if (--axq->axq_timer == 0) {
			ATH_TXQ_UNLOCK(axq);
			if_printf(ifp, "device timeout (txq %d, "
			    "txintrperiod %d)\n", i, sc->sc_txintrperiod);
			if (sc->sc_txintrperiod > 1)
				sc->sc_txintrperiod--;
			ath_reset(ifp);
			ifp->if_oerrors++;
			sc->sc_stats.ast_watchdog++;
			break;
		} else
			ifp->if_timer = 1;
		ATH_TXQ_UNLOCK(axq);
	}
	ieee80211_watchdog(ic);
}

/*
 * Diagnostic interface to the HAL.  This is used by various
 * tools to do things like retrieve register contents for
 * debugging.  The mechanism is intentionally opaque so that
 * it can change frequently w/o concern for compatiblity.
 */
static int
ath_ioctl_diag(struct ath_softc *sc, struct ath_diag *ad)
{
	struct ath_hal *ah = sc->sc_ah;
	u_int id = ad->ad_id & ATH_DIAG_ID;
	void *indata = NULL;
	void *outdata = NULL;
	u_int32_t insize = ad->ad_in_size;
	u_int32_t outsize = ad->ad_out_size;
	int error = 0;

	if (ad->ad_id & ATH_DIAG_IN) {
		/*
		 * Copy in data.
		 */
		indata = malloc(insize, M_TEMP, M_NOWAIT);
		if (indata == NULL) {
			error = ENOMEM;
			goto bad;
		}
		error = copyin(ad->ad_in_data, indata, insize);
		if (error)
			goto bad;
	}
	if (ad->ad_id & ATH_DIAG_DYN) {
		/*
		 * Allocate a buffer for the results (otherwise the HAL
		 * returns a pointer to a buffer where we can read the
		 * results).  Note that we depend on the HAL leaving this
		 * pointer for us to use below in reclaiming the buffer;
		 * may want to be more defensive.
		 */
		outdata = malloc(outsize, M_TEMP, M_NOWAIT);
		if (outdata == NULL) {
			error = ENOMEM;
			goto bad;
		}
	}
	if (ath_hal_getdiagstate(ah, id, indata, insize, &outdata, &outsize)) {
		if (outsize < ad->ad_out_size)
			ad->ad_out_size = outsize;
		if (outdata != NULL)
			error = copyout(outdata, ad->ad_out_data,
					ad->ad_out_size);
	} else {
		error = EINVAL;
	}
bad:
	if ((ad->ad_id & ATH_DIAG_IN) && indata != NULL)
		free(indata, M_TEMP);
	if ((ad->ad_id & ATH_DIAG_DYN) && outdata != NULL)
		free(outdata, M_TEMP);
	return error;
}

static int
ath_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
#define	IS_RUNNING(ifp) \
	((ifp->if_flags & IFF_UP) && (ifp->if_flags & IFF_RUNNING))
	struct ath_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifreq *ifr = (struct ifreq *)data;
	int error = 0, s;

	s = splnet();
	switch (cmd) {
	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		switch (ifp->if_flags & (IFF_UP|IFF_RUNNING)) {
		case IFF_UP|IFF_RUNNING:
			/*
			 * To avoid rescanning another access point,
			 * do not call ath_init() here.  Instead,
			 * only reflect promisc mode settings.
			 */
			ath_mode_init(sc);
			break;
		case IFF_UP:
			/*
			 * Beware of being called during attach/detach
			 * to reset promiscuous mode.  In that case we
			 * will still be marked UP but not RUNNING.
			 * However trying to re-init the interface
			 * is the wrong thing to do as we've already
			 * torn down much of our state.  There's
			 * probably a better way to deal with this.
			 */
			error = ath_init(sc);
			break;
		case IFF_RUNNING:
			ath_stop_locked(ifp, 1);
			break;
		case 0:
			break;
		}
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if ((error = ether_ioctl(ifp, cmd, data)) == ENETRESET) {
			if (ifp->if_flags & IFF_RUNNING)
				ath_mode_init(sc);
			error = 0;
		}
		break;
	case SIOCGATHSTATS:
		/* NB: embed these numbers to get a consistent view */
		sc->sc_stats.ast_tx_packets = ifp->if_opackets;
		sc->sc_stats.ast_rx_packets = ifp->if_ipackets;
		sc->sc_stats.ast_rx_rssi = ieee80211_getrssi(ic);
		splx(s);
		/*
		 * NB: Drop the softc lock in case of a page fault;
		 * we'll accept any potential inconsisentcy in the
		 * statistics.  The alternative is to copy the data
		 * to a local structure.
		 */
		return copyout(&sc->sc_stats,
				ifr->ifr_data, sizeof (sc->sc_stats));
	case SIOCGATHDIAG:
		error = ath_ioctl_diag(sc, (struct ath_diag *) ifr);
		break;
	default:
		error = ieee80211_ioctl(ic, cmd, data);
		if (error != ENETRESET)
			;
		else if (IS_RUNNING(ifp) &&
		         ic->ic_roaming != IEEE80211_ROAMING_MANUAL)
			error = ath_init(sc);
		else
			error = 0;
		break;
	}
	splx(s);
	return error;
#undef IS_RUNNING
}

static void
ath_bpfattach(struct ath_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;

	bpf_attach2(ifp, DLT_IEEE802_11_RADIO,
	    sizeof(struct ieee80211_frame) + sizeof(sc->sc_tx_th),
	    &sc->sc_drvbpf);

	/*
	 * Initialize constant fields.
	 * XXX make header lengths a multiple of 32-bits so subsequent
	 *     headers are properly aligned; this is a kludge to keep
	 *     certain applications happy.
	 *
	 * NB: the channel is setup each time we transition to the
	 *     RUN state to avoid filling it in for each frame.
	 */
	sc->sc_tx_th_len = roundup(sizeof(sc->sc_tx_th), sizeof(u_int32_t));
	sc->sc_tx_th.wt_ihdr.it_len = htole16(sc->sc_tx_th_len);
	sc->sc_tx_th.wt_ihdr.it_present = htole32(ATH_TX_RADIOTAP_PRESENT);

	sc->sc_rx_th_len = roundup(sizeof(sc->sc_rx_th), sizeof(u_int32_t));
	sc->sc_rx_th.wr_ihdr.it_len = htole16(sc->sc_rx_th_len);
	sc->sc_rx_th.wr_ihdr.it_present = htole32(ATH_RX_RADIOTAP_PRESENT);
}

/*
 * Announce various information on device/driver attach.
 */
static void
ath_announce(struct ath_softc *sc)
{
#define	HAL_MODE_DUALBAND	(HAL_MODE_11A|HAL_MODE_11B)
	struct ifnet *ifp = &sc->sc_if;
	struct ath_hal *ah = sc->sc_ah;
	u_int modes, cc;

	if_printf(ifp, "mac %d.%d phy %d.%d",
		ah->ah_macVersion, ah->ah_macRev,
		ah->ah_phyRev >> 4, ah->ah_phyRev & 0xf);
	/*
	 * Print radio revision(s).  We check the wireless modes
	 * to avoid falsely printing revs for inoperable parts.
	 * Dual-band radio revs are returned in the 5 GHz rev number.
	 */
	ath_hal_getcountrycode(ah, &cc);
	modes = ath_hal_getwirelessmodes(ah, cc);
	if ((modes & HAL_MODE_DUALBAND) == HAL_MODE_DUALBAND) {
		if (ah->ah_analog5GhzRev && ah->ah_analog2GhzRev)
			printf(" 5 GHz radio %d.%d 2 GHz radio %d.%d",
				ah->ah_analog5GhzRev >> 4,
				ah->ah_analog5GhzRev & 0xf,
				ah->ah_analog2GhzRev >> 4,
				ah->ah_analog2GhzRev & 0xf);
		else
			printf(" radio %d.%d", ah->ah_analog5GhzRev >> 4,
				ah->ah_analog5GhzRev & 0xf);
	} else
		printf(" radio %d.%d", ah->ah_analog5GhzRev >> 4,
			ah->ah_analog5GhzRev & 0xf);
	printf("\n");
	if (bootverbose) {
		int i;
		for (i = 0; i <= WME_AC_VO; i++) {
			struct ath_txq *txq = sc->sc_ac2q[i];
			if_printf(ifp, "Use hw queue %u for %s traffic\n",
				txq->axq_qnum, ieee80211_wme_acnames[i]);
		}
		if_printf(ifp, "Use hw queue %u for CAB traffic\n",
			sc->sc_cabq->axq_qnum);
		if_printf(ifp, "Use hw queue %u for beacons\n", sc->sc_bhalq);
	}
	if (ath_rxbuf != ATH_RXBUF)
		if_printf(ifp, "using %u rx buffers\n", ath_rxbuf);
	if (ath_txbuf != ATH_TXBUF)
		if_printf(ifp, "using %u tx buffers\n", ath_txbuf);
#undef HAL_MODE_DUALBAND
}

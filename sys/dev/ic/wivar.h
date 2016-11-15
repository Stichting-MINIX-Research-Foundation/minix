/*	$NetBSD: wivar.h,v 1.65 2011/08/15 18:24:34 dyoung Exp $	*/

/*
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/lwp.h>

/* Radio capture format for Prism. */

#define WI_RX_RADIOTAP_PRESENT	((1 << IEEE80211_RADIOTAP_FLAGS) | \
				 (1 << IEEE80211_RADIOTAP_RATE) | \
				 (1 << IEEE80211_RADIOTAP_CHANNEL) | \
				 (1 << IEEE80211_RADIOTAP_DB_ANTSIGNAL) | \
				 (1 << IEEE80211_RADIOTAP_DB_ANTNOISE))

struct wi_rx_radiotap_header {
	struct ieee80211_radiotap_header	wr_ihdr;
	u_int8_t				wr_flags;
	u_int8_t				wr_rate;
	u_int16_t				wr_chan_freq;
	u_int16_t				wr_chan_flags;
	int8_t					wr_antsignal;
	int8_t					wr_antnoise;
} __packed;

#define WI_TX_RADIOTAP_PRESENT	((1 << IEEE80211_RADIOTAP_FLAGS) | \
				 (1 << IEEE80211_RADIOTAP_RATE) | \
				 (1 << IEEE80211_RADIOTAP_CHANNEL))

struct wi_tx_radiotap_header {
	struct ieee80211_radiotap_header	wt_ihdr;
	u_int8_t				wt_flags;
	u_int8_t				wt_rate;
	u_int16_t				wt_chan_freq;
	u_int16_t				wt_chan_flags;
} __packed;

struct wi_rssdesc {
	struct ieee80211_rssdesc	rd_desc;
	SLIST_ENTRY(wi_rssdesc)		rd_next;
};

typedef SLIST_HEAD(,wi_rssdesc) wi_rssdescq_t;

/*
 * FreeBSD driver ported to NetBSD by Bill Sommerfeld in the back of the
 * Oslo IETF plenary meeting.
 */
struct wi_softc	{
	device_t		sc_dev;
	struct ethercom		sc_ec;
	struct ieee80211com	sc_ic;
	u_int32_t		sc_ic_flags;	/* backup of ic->ic_flags */
	void			*sc_ih;		/* interrupt handler */
	int			(*sc_enable)(device_t, int);
	void			(*sc_reset)(struct wi_softc *);

	int			(*sc_newstate)(struct ieee80211com *,
				    enum ieee80211_state, int);
	void			(*sc_set_tim)(struct ieee80211_node *, int);

	int			sc_attached;
	int			sc_enabled;
	int			sc_invalid;
	int			sc_firmware_type;
#define	WI_NOTYPE	0
#define	WI_LUCENT	1
#define	WI_INTERSIL	2
#define	WI_SYMBOL	3
	int			sc_pri_firmware_ver;	/* Primary firm vers */
	int			sc_sta_firmware_ver;	/* Station firm vers */
	int			sc_pci;			/* attach to PCI-Bus */

	bus_space_tag_t		sc_iot;			/* bus cookie */
	bus_space_handle_t	sc_ioh;			/* bus i/o handle */

	struct bpf_if *		sc_drvbpf;
	int			sc_flags;
	int			sc_bap_id;
	int			sc_bap_off;

	u_int16_t		sc_portnum;

	/* RSSI interpretation */
	u_int16_t		sc_dbm_offset;	/* dBm ~ RSSI - sc_dbm_offset */
	u_int16_t		sc_max_datalen;
	u_int16_t		sc_frag_thresh;
	u_int16_t		sc_rts_thresh;
	u_int16_t		sc_system_scale;
	u_int16_t		sc_tx_rate;
	u_int16_t		sc_cnfauthmode;
	u_int16_t		sc_roaming_mode;
	u_int16_t		sc_microwave_oven;

	int			sc_nodelen;
	char			sc_nodename[IEEE80211_NWID_LEN];

	int			sc_buflen;
#define	WI_NTXBUF	3
#define	WI_NTXRSS	10
	struct {
		int				d_fid;
	}			sc_txd[WI_NTXBUF];
	int			sc_txalloc;	/* next FID to allocate */
	int			sc_txalloced;	/* FIDs currently allocated */
	int			sc_txqueue;	/* next FID to queue */
	int			sc_txqueued;	/* FIDs currently queued */
	int			sc_txstart;	/* next FID to start */
	int			sc_txstarted;	/* FIDs currently started */
	int			sc_txcmds;

	int			sc_status;

	struct wi_rssdesc 	sc_rssd[WI_NTXRSS];
	wi_rssdescq_t		sc_rssdfree;
	int			sc_tx_timer;
	int			sc_scan_timer;
	int			sc_syn_timer;

	struct wi_counters	sc_stats;
	u_int16_t		sc_ibss_port;

	struct wi_apinfo	sc_aps[MAXAPINFO];
	int 			sc_naps;

	struct timeval		sc_last_syn;
	int			sc_false_syns;
	int			sc_alt_retry;

	union {
		struct wi_rx_radiotap_header	tap;
		u_int8_t			pad[64];
	} sc_rxtapu;
	union {
		struct wi_tx_radiotap_header	tap;
		u_int8_t			pad[64];
	} sc_txtapu;
	u_int16_t		sc_txbuf[IEEE80211_MAX_LEN/2];
	/* number of transmissions pending at each data rate */
	u_int8_t		sc_txpending[IEEE80211_RATE_MAXSIZE];
	struct callout		sc_rssadapt_ch;
	kmutex_t		sc_ioctl_mtx;
	kcondvar_t		sc_ioctl_cv;
	bool			sc_ioctl_gone;
	unsigned int		sc_ioctl_nwait;
	unsigned int		sc_ioctl_depth;
	lwp_t			*sc_ioctl_lwp;
};

#define	sc_if		sc_ec.ec_if
#define sc_rxtap	sc_rxtapu.tap
#define sc_txtap	sc_txtapu.tap

struct wi_node {
	struct ieee80211_node		wn_node;
	struct ieee80211_rssadapt	wn_rssadapt;
};

/* maximum false change-of-BSSID indications per second */
#define	WI_MAX_FALSE_SYNS		10

#define	WI_PRISM_DBM_OFFSET	100	/* XXX */

#define	WI_LUCENT_DBM_OFFSET	149

#define	WI_SCAN_INQWAIT			3	/* wait sec before inquire */
#define	WI_SCAN_WAIT			5	/* maximum scan wait */

/* Values for wi_flags. */
#define	WI_FLAGS_ATTACHED		0x0001
#define	WI_FLAGS_INITIALIZED		0x0002
#define	WI_FLAGS_OUTRANGE		0x0004
#define	WI_FLAGS_RSSADAPTSTA		0x0008
#define	WI_FLAGS_HAS_MOR		0x0010
#define	WI_FLAGS_HAS_ROAMING		0x0020
#define	WI_FLAGS_HAS_DIVERSITY		0x0040
#define	WI_FLAGS_HAS_SYSSCALE		0x0080
#define	WI_FLAGS_BUG_AUTOINC		0x0100
#define	WI_FLAGS_HAS_FRAGTHR		0x0200
#define	WI_FLAGS_HAS_DBMADJUST		0x0400
#define	WI_FLAGS_WEP_VALID		0x0800

struct wi_card_ident {
	u_int16_t	card_id;
	const char	*card_name;
	u_int8_t	firm_type;
};

/*
 * register space access macros
 */
#ifdef WI_AT_BIGENDIAN_BUS_HACK
	/*
	 * XXX - ugly hack for sparc bus_space_* macro deficiencies:
	 *       assume the bus we are accessing is big endian.
	 */

#define CSR_WRITE_4(sc, reg, val)	\
	bus_space_write_4(sc->sc_iot, sc->sc_ioh,	\
			(sc->sc_pci? reg * 2: reg) , htole32(val))
#define CSR_WRITE_2(sc, reg, val)	\
	bus_space_write_2(sc->sc_iot, sc->sc_ioh,	\
			(sc->sc_pci? reg * 2: reg), htole16(val))
#define CSR_WRITE_1(sc, reg, val)	\
	bus_space_write_1(sc->sc_iot, sc->sc_ioh,	\
			(sc->sc_pci? reg * 2: reg), val)

#define CSR_READ_4(sc, reg)		\
	le32toh(bus_space_read_4(sc->sc_iot, sc->sc_ioh,	\
			(sc->sc_pci? reg * 2: reg)))
#define CSR_READ_2(sc, reg)		\
	le16toh(bus_space_read_2(sc->sc_iot, sc->sc_ioh,	\
			(sc->sc_pci? reg * 2: reg)))
#define CSR_READ_1(sc, reg)		\
	bus_space_read_1(sc->sc_iot, sc->sc_ioh,	\
			(sc->sc_pci? reg * 2: reg))

#else

#define CSR_WRITE_4(sc, reg, val)	\
	bus_space_write_4(sc->sc_iot, sc->sc_ioh,	\
			(sc->sc_pci? reg * 2: reg) , val)
#define CSR_WRITE_2(sc, reg, val)	\
	bus_space_write_2(sc->sc_iot, sc->sc_ioh,	\
			(sc->sc_pci? reg * 2: reg), val)
#define CSR_WRITE_1(sc, reg, val)	\
	bus_space_write_1(sc->sc_iot, sc->sc_ioh,	\
			(sc->sc_pci? reg * 2: reg), val)

#define CSR_READ_4(sc, reg)		\
	bus_space_read_4(sc->sc_iot, sc->sc_ioh,	\
			(sc->sc_pci? reg * 2: reg))
#define CSR_READ_2(sc, reg)		\
	bus_space_read_2(sc->sc_iot, sc->sc_ioh,	\
			(sc->sc_pci? reg * 2: reg))
#define CSR_READ_1(sc, reg)		\
	bus_space_read_1(sc->sc_iot, sc->sc_ioh,	\
			(sc->sc_pci? reg * 2: reg))
#endif

#ifndef __BUS_SPACE_HAS_STREAM_METHODS
#define bus_space_write_stream_2	bus_space_write_2
#define bus_space_write_multi_stream_2	bus_space_write_multi_2
#define bus_space_read_stream_2		bus_space_read_2
#define bus_space_read_multi_stream_2		bus_space_read_multi_2
#endif

#define CSR_WRITE_STREAM_2(sc, reg, val)	\
	bus_space_write_stream_2(sc->sc_iot, sc->sc_ioh,	\
			(sc->sc_pci? reg * 2: reg), val)
#define CSR_WRITE_MULTI_STREAM_2(sc, reg, val, count)	\
	bus_space_write_multi_stream_2(sc->sc_iot, sc->sc_ioh,	\
			(sc->sc_pci? reg * 2: reg), val, count)
#define CSR_READ_STREAM_2(sc, reg)		\
	bus_space_read_stream_2(sc->sc_iot, sc->sc_ioh,	\
			(sc->sc_pci? reg * 2: reg))
#define CSR_READ_MULTI_STREAM_2(sc, reg, buf, count)		\
	bus_space_read_multi_stream_2(sc->sc_iot, sc->sc_ioh,	\
			(sc->sc_pci? reg * 2: reg), buf, count)


int	wi_attach(struct wi_softc *, const u_int8_t *);
int	wi_detach(struct wi_softc *);
int	wi_activate(device_t, enum devact);
int	wi_intr(void *arg);

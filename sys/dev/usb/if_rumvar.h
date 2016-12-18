/*	$NetBSD: if_rumvar.h,v 1.9 2011/02/21 23:50:42 jmcneill Exp $	*/
/*	$OpenBSD: if_rumvar.h,v 1.7 2006/11/13 20:06:38 damien Exp $	*/

/*-
 * Copyright (c) 2005, 2006 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2006 Niall O'Higgins <niallo@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define RUM_RX_LIST_COUNT	1
#define RUM_TX_LIST_COUNT	8

struct rum_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
	uint8_t		wr_flags;
	uint8_t		wr_rate;
	uint16_t	wr_chan_freq;
	uint16_t	wr_chan_flags;
	uint8_t		wr_antenna;
	uint8_t		wr_antsignal;
} __packed;

#define RT2573_RX_RADIOTAP_PRESENT					\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_RATE) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL) |				\
	 (1 << IEEE80211_RADIOTAP_ANTENNA) |				\
	 (1 << IEEE80211_RADIOTAP_DB_ANTSIGNAL))

struct rum_tx_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	uint8_t		wt_flags;
	uint8_t		wt_rate;
	uint16_t	wt_chan_freq;
	uint16_t	wt_chan_flags;
	uint8_t		wt_antenna;
} __packed;

#define RT2573_TX_RADIOTAP_PRESENT						\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_RATE) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL) |				\
	 (1 << IEEE80211_RADIOTAP_ANTENNA))

struct rum_softc;

struct rum_tx_data {
	struct rum_softc	*sc;
	usbd_xfer_handle	xfer;
	uint8_t			*buf;
	struct mbuf		*m;
	struct ieee80211_node	*ni;
};

struct rum_rx_data {
	struct rum_softc	*sc;
	usbd_xfer_handle	xfer;
	uint8_t			*buf;
	struct mbuf		*m;
};

struct rum_softc {
	device_t			sc_dev;
	struct ethercom			sc_ec;
#define sc_if	sc_ec.ec_if
	struct ieee80211com		sc_ic;
	int				(*sc_newstate)(struct ieee80211com *,
					    enum ieee80211_state, int);

	usbd_device_handle		sc_udev;
	usbd_interface_handle		sc_iface;
	int				sc_flags;
#define	RT2573_FWLOADED	(1 << 0)

	struct ieee80211_channel	*sc_curchan;

	int				sc_rx_no;
	int				sc_tx_no;

	uint16_t			macbbp_rev;
	uint8_t				rf_rev;
	uint8_t				rffreq;

	usbd_xfer_handle		amrr_xfer;

	usbd_pipe_handle		sc_rx_pipeh;
	usbd_pipe_handle		sc_tx_pipeh;

	enum ieee80211_state		sc_state;
	int				sc_arg;
	struct usb_task			sc_task;

	struct ieee80211_amrr		amrr;
	struct ieee80211_amrr_node	amn;

	struct rum_rx_data		rx_data[RUM_RX_LIST_COUNT];
	struct rum_tx_data		tx_data[RUM_TX_LIST_COUNT];
	int				tx_queued;
	int				tx_cur;

	struct ieee80211_beacon_offsets	sc_bo;

	struct callout			sc_scan_ch;
	struct callout			sc_amrr_ch;

	int				sc_tx_timer;

	uint32_t			sta[6];
	uint32_t			rf_regs[4];
	uint8_t				txpow[44];

	struct {
		uint8_t	val;
		uint8_t	reg;
	} __packed			bbp_prom[16];

	int				hw_radio;
	int				rx_ant;
	int				tx_ant;
	int				nb_ant;
	int				ext_2ghz_lna;
	int				ext_5ghz_lna;
	int				rssi_2ghz_corr;
	int				rssi_5ghz_corr;
	int				sifs;
	uint8_t				bbp17;

	struct bpf_if *			sc_drvbpf;

	union {
		struct rum_rx_radiotap_header th;
		uint8_t	pad[64];
	}				sc_rxtapu;
#define sc_rxtap	sc_rxtapu.th
	int				sc_rxtap_len;

	union {
		struct rum_tx_radiotap_header th;
		uint8_t	pad[64];
	}				sc_txtapu;
#define sc_txtap	sc_txtapu.th
	int				sc_txtap_len;
};

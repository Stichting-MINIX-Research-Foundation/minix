/*	$NetBSD: if_otusvar.h,v 1.7 2013/03/30 01:10:00 christos Exp $	*/
/*	$OpenBSD: if_otusreg.h,v 1.6 2009/04/06 18:17:01 damien Exp $	*/

/*-
 * Copyright (c) 2009 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
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
#ifndef _IF_OTUSVAR_H_
#define _IF_OTUSVAR_H_

#ifndef HAVE_EDCA
/************************************************************
 * XXX: This block belongs in sys/net80211/ieee80211_var.h.
 */
/*
 * EDCA AC parameters.
 */
struct ieee80211_edca_ac_params {
	u_int8_t	ac_ecwmin;	/* CWmin = 2^ECWmin - 1 */
	u_int8_t	ac_ecwmax;	/* CWmax = 2^ECWmax - 1 */
	u_int8_t	ac_aifsn;
	u_int16_t	ac_txoplimit;	/* 32TU */
	u_int8_t	ac_acm;
};
/************************************************************/
#endif /* ! HAVE_EDCA */

/* Default EDCA parameters for when QoS is disabled. */
static const struct ieee80211_edca_ac_params otus_edca_def[] = {
	{ 4, 10, 3,  0, 0 },
	{ 4, 10, 7,  0, 0 },
	{ 3,  4, 2, 94, 0 },
	{ 2,  3, 2, 47, 0 }
};

#define OTUS_TX_DATA_LIST_COUNT	8
#define OTUS_RX_DATA_LIST_COUNT	1

#define OTUS_CMD_TIMEOUT	1000
#define OTUS_TX_TIMEOUT		1000

#define OTUS_UID(aid)		(IEEE80211_AID(aid) + 4)

#define OTUS_MAX_TXCMDSZ	64
#define OTUS_RXBUFSZ		(8 * 1024)
#define OTUS_TXBUFSZ		(4 * 1024)

#define OTUS_RIDX_CCK1		 0
#define OTUS_RIDX_OFDM6		 4
#define OTUS_RIDX_OFDM24	 8
#define OTUS_RIDX_MAX		11
static const struct otus_rate {
	uint8_t	rate;
	uint8_t	mcs;
} otus_rates[] = {
	{   2, 0x0 },
	{   4, 0x1 },
	{  11, 0x2 },
	{  22, 0x3 },
	{  12, 0xb },
	{  18, 0xf },
	{  24, 0xa },
	{  36, 0xe },
	{  48, 0x9 },
	{  72, 0xd },
	{  96, 0x8 },
	{ 108, 0xc }
};

struct otus_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
	uint8_t		wr_flags;
	uint8_t		wr_rate;
	uint16_t	wr_chan_freq;
	uint16_t	wr_chan_flags;
	uint8_t		wr_antsignal;
} __packed;

#define OTUS_RX_RADIOTAP_PRESENT			\
	(1 << IEEE80211_RADIOTAP_FLAGS |		\
	 1 << IEEE80211_RADIOTAP_RATE |			\
	 1 << IEEE80211_RADIOTAP_CHANNEL |		\
	 1 << IEEE80211_RADIOTAP_DB_ANTSIGNAL)

struct otus_tx_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	uint8_t		wt_flags;
	uint8_t		wt_rate;
	uint16_t	wt_chan_freq;
	uint16_t	wt_chan_flags;
} __packed;

#define OTUS_TX_RADIOTAP_PRESENT			\
	(1 << IEEE80211_RADIOTAP_FLAGS |		\
	 1 << IEEE80211_RADIOTAP_RATE |			\
	 1 << IEEE80211_RADIOTAP_CHANNEL)


struct otus_softc;

struct otus_tx_cmd {
	usbd_xfer_handle	xfer;
	uint8_t			*buf;
	void			*odata;
	uint16_t		token;
	uint8_t			done;
};

struct otus_rx_data {
	struct otus_softc	*sc;
	usbd_xfer_handle	xfer;
	uint8_t			*buf;
};

struct otus_tx_data {
	struct otus_softc		*sc;
	usbd_xfer_handle		xfer;
	uint8_t				*buf;
	TAILQ_ENTRY(otus_tx_data)	next;
};

struct otus_host_cmd {
	void	(*cb)(struct otus_softc *, void *);
	uint8_t	data[256];
};

#define OTUS_HOST_CMD_RING_COUNT	32
struct otus_host_cmd_ring {
	struct otus_host_cmd	cmd[OTUS_HOST_CMD_RING_COUNT];
	int			cur;
	int			next;
	int			queued;
};

struct otus_node {
	struct ieee80211_node		ni;	/* must be first */
	struct ieee80211_amrr_node	amn;
	uint8_t				ridx[IEEE80211_RATE_MAXSIZE];
};

struct otus_cmd_newstate {
	enum ieee80211_state	state;
	int			arg;
};

struct otus_cmd_key {
	struct ieee80211_key	key;
	uint16_t		associd;
};

struct otus_softc {
	device_t			sc_dev;
	struct ieee80211com		sc_ic;
	struct ethercom			sc_ec;
#define sc_if	sc_ec.ec_if
	int				(*sc_newstate)(struct ieee80211com *,
					    enum ieee80211_state, int);
	void				(*sc_led_newstate)(struct otus_softc *);

	usbd_device_handle		sc_udev;
	usbd_interface_handle		sc_iface;

	struct ar5416eeprom		sc_eeprom;
	uint8_t				sc_capflags;
	uint8_t				sc_rxmask;
	uint8_t				sc_txmask;

	usbd_pipe_handle		sc_data_tx_pipe;
	usbd_pipe_handle		sc_data_rx_pipe;
	usbd_pipe_handle		sc_cmd_tx_pipe;
	usbd_pipe_handle		sc_cmd_rx_pipe;
	uint8_t 			*sc_ibuf;

	int				sc_if_flags;
	int				sc_tx_timer;
	int				sc_fixed_ridx;
	int				sc_bb_reset;

	struct ieee80211_channel	*sc_curchan;

	struct usb_task			sc_task;
	callout_t			sc_scan_to;
	callout_t			sc_calib_to;
	struct ieee80211_amrr		sc_amrr;

	unsigned int			sc_write_idx;
	uint32_t			sc_led_state;

	kmutex_t			sc_cmd_mtx;
	kmutex_t			sc_task_mtx;
	kmutex_t			sc_write_mtx;
	kmutex_t			sc_tx_mtx;

	const uint32_t			*sc_phy_vals;

	struct {
		uint32_t	reg;
		uint32_t	val;
	} __packed			sc_write_buf[AR_FW_MAX_WRITES];

	struct otus_host_cmd_ring	sc_cmdq;
	struct otus_tx_cmd		sc_tx_cmd;
	struct otus_tx_data		sc_tx_data[OTUS_TX_DATA_LIST_COUNT];
	TAILQ_HEAD(, otus_tx_data)	sc_tx_free_list;
	struct otus_rx_data		sc_rx_data[OTUS_RX_DATA_LIST_COUNT];

	struct bpf_if *			sc_drvbpf;
	union {
		struct otus_rx_radiotap_header th;
		uint8_t	pad[64];
	}				sc_rxtapu;
#define sc_rxtap	sc_rxtapu.th
	int				sc_rxtap_len;
	union {
		struct otus_tx_radiotap_header th;
		uint8_t	pad[64];
	}				sc_txtapu;
#define sc_txtap	sc_txtapu.th
	int				sc_txtap_len;

	uint8_t				sc_rx_error_msk;
	int				sc_dying;
};

#endif /* _IF_OTUSVAR_H_ */

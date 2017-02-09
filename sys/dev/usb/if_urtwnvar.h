/*	$NetBSD: if_urtwnvar.h,v 1.8 2014/07/20 13:25:23 nonaka Exp $	*/
/*	$OpenBSD: if_urtwnreg.h,v 1.3 2010/11/16 18:02:59 damien Exp $	*/

/*-
 * Copyright (c) 2010 Damien Bergamini <damien.bergamini@free.fr>
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
#ifndef _IF_URTWNVAR_H_
#define _IF_URTWNVAR_H_

/*
 * Driver definitions.
 */
#define URTWN_RX_LIST_COUNT		1
#define URTWN_TX_LIST_COUNT		8

#define URTWN_HOST_CMD_RING_COUNT	32

#define URTWN_RXBUFSZ	(16 * 1024)
#define URTWN_TXBUFSZ	(sizeof(struct r92c_tx_desc) + IEEE80211_MAX_LEN + 8)

#define URTWN_RIDX_COUNT	28

#define URTWN_TX_TIMEOUT	5000	/* ms */

#define URTWN_LED_LINK	0
#define URTWN_LED_DATA	1

struct urtwn_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
	uint8_t		wr_flags;
	uint8_t		wr_rate;
	uint16_t	wr_chan_freq;
	uint16_t	wr_chan_flags;
	uint8_t		wr_dbm_antsignal;
} __packed;

#define URTWN_RX_RADIOTAP_PRESENT			\
	(1 << IEEE80211_RADIOTAP_FLAGS |		\
	 1 << IEEE80211_RADIOTAP_RATE |			\
	 1 << IEEE80211_RADIOTAP_CHANNEL |		\
	 1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL)

struct urtwn_tx_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	uint8_t		wt_flags;
	uint16_t	wt_chan_freq;
	uint16_t	wt_chan_flags;
} __packed;

#define URTWN_TX_RADIOTAP_PRESENT			\
	(1 << IEEE80211_RADIOTAP_FLAGS |		\
	 1 << IEEE80211_RADIOTAP_CHANNEL)

struct urtwn_softc;

struct urtwn_rx_data {
	struct urtwn_softc	*sc;
	usbd_xfer_handle	xfer;
	uint8_t			*buf;
};

struct urtwn_tx_data {
	struct urtwn_softc		*sc;
	usbd_pipe_handle		pipe;
	usbd_xfer_handle		xfer;
	uint8_t				*buf;
	TAILQ_ENTRY(urtwn_tx_data)	next;
};

struct urtwn_host_cmd {
	void	(*cb)(struct urtwn_softc *, void *);
	uint8_t	data[256];
};

struct urtwn_cmd_newstate {
	enum ieee80211_state	state;
	int			arg;
};

struct urtwn_host_cmd_ring {
	struct urtwn_host_cmd	cmd[URTWN_HOST_CMD_RING_COUNT];
	int			cur;
	int			next;
	int			queued;
};

#if 1	/* XXX: sys/net80211/ieee80211.h */

#define	IEEE80211_HTINFO_2NDCHAN	0x03	/* secondary/ext chan offset */
#define	IEEE80211_HTINFO_2NDCHAN_S	0
#define	IEEE80211_HTINFO_2NDCHAN_NONE	0x00	/* no secondary/ext channel */
#define	IEEE80211_HTINFO_2NDCHAN_ABOVE	0x01	/* above private channel */
/* NB: 2 is reserved */
#define	IEEE80211_HTINFO_2NDCHAN_BELOW	0x03	/* below primary channel */
#endif	/* XXX: 1 */

struct urtwn_softc {
	device_t			sc_dev;
	struct ieee80211com		sc_ic;
	struct ethercom			sc_ec;
#define sc_if   sc_ec.ec_if
	int				(*sc_newstate)(struct ieee80211com *,
					    enum ieee80211_state, int);

	usbd_device_handle		sc_udev;
	usbd_interface_handle		sc_iface;
	u_int				sc_flags;
#define URTWN_FLAG_CCK_HIPWR	__BIT(0)
#define	URTWN_FLAG_ATTACHED	__BIT(1)
#define	URTWN_FLAG_FWREADY	__BIT(2)
	int				sc_dying;

	struct usb_task			sc_task;
	callout_t			sc_scan_to;
	callout_t			sc_calib_to;

	kmutex_t			sc_task_mtx;
	kmutex_t			sc_fwcmd_mtx;
	kmutex_t			sc_tx_mtx;
	kmutex_t			sc_write_mtx;

	usbd_pipe_handle		rx_pipe;
	int				rx_npipe;
	usbd_pipe_handle		tx_pipe[R92C_MAX_EPOUT];
	int				tx_npipe;
	int				ac2idx[WME_NUM_AC];

	u_int				chip;
#define URTWN_CHIP_92C		0x01
#define URTWN_CHIP_92C_1T2R	0x02
#define URTWN_CHIP_UMC		0x04
#define URTWN_CHIP_UMC_A_CUT	0x08
#define URTWN_CHIP_88E		0x10

	void				(*sc_rf_write)(struct urtwn_softc *,
					    int, uint8_t, uint32_t);
	int				(*sc_power_on)(struct urtwn_softc *);
	int				(*sc_dma_init)(struct urtwn_softc *);

	uint8_t				board_type;
	uint8_t				regulatory;
	uint8_t				pa_setting;
	int				avg_pwdb;
	int				thcal_state;
	int				thcal_lctemp;
	size_t				ntxchains;
	size_t				nrxchains;
	int				ledlink;
	bool				iqk_inited;

	int				tx_timer;

	struct urtwn_host_cmd_ring	cmdq;
	int				fwcur;
	struct urtwn_rx_data		rx_data[URTWN_RX_LIST_COUNT];
	struct urtwn_tx_data		tx_data[URTWN_TX_LIST_COUNT];
	TAILQ_HEAD(, urtwn_tx_data)	tx_free_list;

	struct r92c_rom			rom;
	uint8_t				r88e_rom[512];
	uint8_t				cck_tx_pwr[6];
	uint8_t				ht40_tx_pwr[5];
	int8_t				bw20_tx_pwr_diff;
	int8_t				ofdm_tx_pwr_diff;

	uint32_t			rf_chnlbw[R92C_MAX_CHAINS];

	struct bpf_if *			sc_drvbpf;
	union {
		struct urtwn_rx_radiotap_header th;
		uint8_t	pad[64];
	}				sc_rxtapu;
#define sc_rxtap	sc_rxtapu.th
	int				sc_rxtap_len;
	union {
		struct urtwn_tx_radiotap_header th;
		uint8_t	pad[64];
	}				sc_txtapu;
#define sc_txtap	sc_txtapu.th
	int				sc_txtap_len;
};

#endif /* _IF_URTWNVAR_H_ */

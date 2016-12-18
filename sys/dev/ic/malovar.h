/*	$OpenBSD: malo.h,v 1.10 2010/08/08 16:36:33 deraadt Exp $ */

/*
 * Copyright (c) 2006 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2006 Marcus Glocker <mglocker@openbsd.org>
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

//#define MALO_DEBUG

struct malo_rx_desc;
struct malo_rx_data;

struct malo_rx_ring {
	bus_dmamap_t		map;
	bus_dma_segment_t	seg;
	bus_addr_t		physaddr;
	struct malo_rx_desc	*desc;
	struct malo_rx_data	*data;
	int			count;
	int			cur;
	int			next;
};

struct malo_tx_desc;
struct malo_tx_data;

struct malo_tx_ring {
	bus_dmamap_t		map;
	bus_dma_segment_t	seg;
	bus_addr_t		physaddr;
	struct malo_tx_desc	*desc;
	struct malo_tx_data	*data;
	int			count;
	int			queued;
	int			cur;
	int			next;
	int			stat;
};

// XXX Support for RSSI ?
#define MALO_RX_RADIOTAP_PRESENT					\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL))

struct malo_rx_radiotap_hdr {
	struct ieee80211_radiotap_header	wr_ihdr;
	uint8_t					wr_flags;
	uint16_t				wr_chan_freq;
	uint16_t				wr_chan_flags;
} __packed;

#define MALO_TX_RADIOTAP_PRESENT					\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_RATE) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL))

struct malo_tx_radiotap_hdr {
	struct ieee80211_radiotap_header	wt_ihdr;
	uint8_t					wt_flags;
	uint8_t					wt_rate;
	uint16_t				wt_chan_freq;
	uint16_t				wt_chan_flags;
} __packed;

struct malo_softc {
	device_t		sc_dev;
	struct ethercom		 sc_ec;
	struct ieee80211com	sc_ic;
#define sc_if sc_ec.ec_if
	struct malo_rx_ring	sc_rxring;
	struct malo_tx_ring	sc_txring;

	bus_dma_tag_t		sc_dmat;
	bus_space_tag_t		sc_mem1_bt;
	bus_space_tag_t		sc_mem2_bt;
	bus_space_handle_t	sc_mem1_bh;
	bus_space_handle_t	sc_mem2_bh;

	bus_dmamap_t		sc_cmd_dmam;
	bus_dma_segment_t	sc_cmd_dmas;
	void				*sc_cmd_mem;
	bus_addr_t		sc_cmd_dmaaddr;
	uint32_t		*sc_cookie;
	bus_addr_t		sc_cookie_dmaaddr;

	uint32_t		sc_RxPdWrPtr;
	uint32_t		sc_RxPdRdPtr;

	int			(*sc_newstate)
				(struct ieee80211com *,
				 enum ieee80211_state, int);

	int			(*sc_enable)(struct malo_softc *);
	void			(*sc_disable)(struct malo_softc *);

	struct callout	sc_scan_to;
	int			sc_tx_timer;
	int			sc_last_txrate;

	struct bpf_if *		sc_drvbpf;

	union {
		struct malo_rx_radiotap_hdr th;
		uint8_t pad[IEEE80211_RADIOTAP_HDRLEN];
	}		sc_rxtapu;
#define sc_rxtap	sc_rxtapu.th
	int		sc_rxtap_len;

	union {
		struct malo_tx_radiotap_hdr th;
		uint8_t	pad[IEEE80211_RADIOTAP_HDRLEN];
	}		sc_txtapu;
#define sc_txtap	sc_txtapu.th
	int		sc_txtap_len;
};

int malo_intr(void *arg);
int malo_attach(struct malo_softc *sc);
int malo_detach(void *arg);
int malo_init(struct ifnet *);
void malo_stop(struct ifnet *, int disable);

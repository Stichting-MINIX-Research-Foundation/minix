/*	$NetBSD: if_iwivar.h,v 1.18 2015/09/06 06:01:00 dholland Exp $ */

/*-
 * Copyright (c) 2004, 2005
 *      Damien Bergamini <damien.bergamini@free.fr>. All rights reserved.
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
 */

#include <sys/ioccom.h>

struct iwi_firmware {
	void	*boot;
	int	boot_size;
	void	*ucode;
	int	ucode_size;
	void	*main;
	int	main_size;
};

struct iwi_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
	uint8_t		wr_flags;
	uint8_t		wr_rate;
	uint16_t	wr_chan_freq;
	uint16_t	wr_chan_flags;
	uint8_t		wr_antsignal;
	uint8_t		wr_antenna;
};

#define IWI_RX_RADIOTAP_PRESENT						\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_RATE) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL) |				\
	 (1 << IEEE80211_RADIOTAP_DB_ANTSIGNAL) |			\
	 (1 << IEEE80211_RADIOTAP_ANTENNA))

struct iwi_tx_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	uint8_t		wt_flags;
	uint16_t	wt_chan_freq;
	uint16_t	wt_chan_flags;
};

#define IWI_TX_RADIOTAP_PRESENT						\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL))

struct iwi_cmd_ring {
	bus_dmamap_t		desc_map;
	bus_dma_segment_t	desc_seg;
	struct iwi_cmd_desc	*desc;
	int			count;
	int			queued;
	int			cur;
	int			next;
};

struct iwi_tx_data {
	bus_dmamap_t		map;
	struct mbuf		*m;
	struct ieee80211_node	*ni;
};

struct iwi_tx_ring {
	bus_dmamap_t		desc_map;
	bus_dma_segment_t	desc_seg;
	bus_size_t		csr_ridx;
	bus_size_t		csr_widx;
	struct iwi_tx_desc	*desc;
	struct iwi_tx_data	*data;
	int			count;
	int			queued;
	int			cur;
	int			next;
};

struct iwi_rx_data {
	bus_dmamap_t	map;
	struct mbuf	*m;
};

struct iwi_rx_ring {
	struct iwi_rx_data	*data;
	int			count;
	int			cur;
};

struct iwi_node {
	struct ieee80211_node	in_node;
	int			in_station;
#define IWI_MAX_IBSSNODE	32
};

struct iwi_softc {
	device_t		sc_dev;
	struct ethercom		sc_ec;
	struct ieee80211com	sc_ic;
	int			(*sc_newstate)(struct ieee80211com *,
				    enum ieee80211_state, int);
	void			(*sc_node_free)(struct ieee80211_node *);

	uint32_t		sc_unr;

	struct iwi_firmware	fw;
	const char		*sc_fwname;
	char			*sc_blob;
	size_t			sc_blobsize;

	uint32_t		flags;
#define IWI_FLAG_FW_CACHED	(1 << 0)
#define IWI_FLAG_FW_INITED	(1 << 1)
#define IWI_FLAG_SCANNING	(1 << 3)

	bus_dma_tag_t		sc_dmat;

	struct iwi_cmd_ring	cmdq;
	struct iwi_tx_ring	txq[WME_NUM_AC];
	struct iwi_rx_ring	rxq;

	struct resource		*irq;
	struct resource		*mem;
	bus_space_tag_t		sc_st;
	bus_space_handle_t	sc_sh;
	void 			*sc_ih;
	pci_chipset_tag_t	sc_pct;
	pcitag_t		sc_pcitag;
	bus_size_t		sc_sz;

	struct sysctllog	*sc_sysctllog;

	int			antenna;
	int			dwelltime;
	int			bluetooth;
	int			nictype;

	int			sc_tx_timer;

	struct bpf_if		*sc_drvbpf;

	union {
		struct iwi_rx_radiotap_header th;
		uint8_t	pad[64];
	} sc_rxtapu;
#define sc_rxtap	sc_rxtapu.th
	int			sc_rxtap_len;

	union {
		struct iwi_tx_radiotap_header th;
		uint8_t	pad[64];
	} sc_txtapu;
#define sc_txtap	sc_txtapu.th
	int			sc_txtap_len;
};

#define	sc_if	sc_ec.ec_if

#define SIOCGRADIO	_IOWR('i', 139, struct ifreq)
#define SIOCGTABLE0	_IOWR('i', 140, struct ifreq)

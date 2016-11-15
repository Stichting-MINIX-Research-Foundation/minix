/*	$NetBSD: if_vtevar.h,v 1.4 2015/04/14 20:32:36 riastradh Exp $	*/

/*-
 * Copyright (c) 2010, Pyun YongHyeon <yongari@FreeBSD.org>
 * All rights reserved.
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
 *
 * FreeBSD: src/sys/dev/vte/if_vtevar.h,v 1.1 2010/12/31 00:21:41 yongari Exp
 */

#ifndef	_IF_VTEVAR_H
#define	_IF_VTEVAR_H

#include <sys/rndsource.h>

#define	VTE_TX_RING_CNT		64
#define	VTE_TX_RING_ALIGN	16
/*
 * The TX/RX descriptor format has no limitation for number of
 * descriptors in TX/RX ring.  However, the maximum number of
 * descriptors that could be set as RX descriptor ring residue
 * counter is 255.  This effectively limits number of RX
 * descriptors available to be less than or equal to 255.
 */
#define	VTE_RX_RING_CNT		128
#define	VTE_RX_RING_ALIGN	16
#define	VTE_RX_BUF_ALIGN	4

#define	VTE_DESC_INC(x, y)	((x) = ((x) + 1) % (y))

#define	VTE_TX_RING_SZ		\
	(sizeof(struct vte_tx_desc) * VTE_TX_RING_CNT)
#define	VTE_RX_RING_SZ		\
	(sizeof(struct vte_rx_desc) * VTE_RX_RING_CNT)

#define	VTE_RX_BUF_SIZE_MAX	(MCLBYTES - sizeof(uint32_t))

#define	VTE_MIN_FRAMELEN	(ETHER_MIN_LEN - ETHER_CRC_LEN)

struct vte_rxdesc {
	struct mbuf		*rx_m;
	bus_dma_segment_t	rx_seg;
	bus_dmamap_t		rx_dmamap;
	struct vte_rx_desc	*rx_desc;
};

struct vte_txdesc {
	struct mbuf		*tx_m;
	bus_dma_segment_t	*tx_seg;
	bus_dmamap_t		tx_dmamap;
	struct vte_tx_desc	*tx_desc;
	int			tx_flags;
#define	VTE_TXMBUF		0x0001
};

struct vte_chain_data {
	struct vte_txdesc	vte_txdesc[VTE_TX_RING_CNT];
	struct mbuf		*vte_txmbufs[VTE_TX_RING_CNT];
	struct vte_rxdesc	vte_rxdesc[VTE_RX_RING_CNT];
	bus_dmamap_t		vte_tx_ring_map;
	bus_dma_segment_t	vte_tx_ring_seg[1];
	bus_dmamap_t		vte_rx_ring_map;
	bus_dma_segment_t	vte_rx_ring_seg[1];
	bus_dmamap_t		vte_rr_ring_map;
	bus_dma_segment_t	vte_rr_ring_seg[1];
	bus_dmamap_t		vte_rx_sparemap;
	bus_dmamap_t		vte_cmb_map;
	bus_dmamap_t		vte_smb_map;
	struct vte_tx_desc	*vte_tx_ring;
	struct vte_rx_desc	*vte_rx_ring;

	int			vte_tx_prod;
	int			vte_tx_cons;
	int			vte_tx_cnt;
	int			vte_rx_cons;
};

struct vte_hw_stats {
	/* RX stats. */
	uint32_t rx_frames;
	uint32_t rx_bcast_frames;
	uint32_t rx_mcast_frames;
	uint32_t rx_runts;
	uint32_t rx_crcerrs;
	uint32_t rx_long_frames;
	uint32_t rx_fifo_full;
	uint32_t rx_desc_unavail;
	uint32_t rx_pause_frames;

	/* TX stats. */
	uint32_t tx_frames;
	uint32_t tx_underruns;
	uint32_t tx_late_colls;
	uint32_t tx_pause_frames;
};

/*
 * Software state per device.
 */
struct vte_softc {
	device_t		vte_dev;
	bus_space_tag_t		vte_bustag;
	bus_space_handle_t	vte_bushandle;
	bus_dma_tag_t		vte_dmatag;
	void*			vte_ih;
	struct ethercom		vte_ec;
	mii_data_t		vte_mii;
	device_t		vte_miibus;
	uint8_t			vte_eaddr[ETHER_ADDR_LEN];
	int			vte_flags;
#define	VTE_FLAG_LINK		0x8000

	struct callout		vte_tick_ch;
	struct vte_hw_stats	vte_stats;
	struct vte_chain_data	vte_cdata;
	int			vte_if_flags;
	int			vte_watchdog_timer;
	struct sysctllog	*vte_clog;
	int			vte_int_rx_mod;
	int			vte_int_tx_mod;

	krndsource_t	rnd_source;
};

#define vte_if	vte_ec.ec_if
#define vte_bpf	vte_if.if_bpf           

/* Register access macros. */
#define	CSR_WRITE_2(_sc, reg, val)	\
	bus_space_write_2((_sc)->vte_bustag,  (_sc)->vte_bushandle, \
	(reg), (val))
#define	CSR_READ_2(_sc, reg)		\
	bus_space_read_2((_sc)->vte_bustag,  (_sc)->vte_bushandle, (reg))

#define	VTE_TX_TIMEOUT		5
#define	VTE_RESET_TIMEOUT	100
#define	VTE_TIMEOUT		1000
#define	VTE_PHY_TIMEOUT		1000

#endif	/* _IF_VTEVAR_H */

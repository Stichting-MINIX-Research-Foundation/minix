/* $NetBSD */
/*-
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jean-Yves Migeon <jym@NetBSD.org>
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
 *
 * $FreeBSD: src/sys/dev/bce/if_bcereg.h,v 1.4 2006/05/04 00:34:07 mjacob Exp $
 */

#ifndef	_DEV_PCI_IF_BNXVAR_H_
#define _DEV_PCI_IF_BNXVAR_H_

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/workqueue.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_inarp.h>
#endif

#include <net/if_vlanvar.h>

#include <net/bpf.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/miidevs.h>
#include <dev/mii/brgphyreg.h>

/*
 * PCI registers defined in the PCI 2.2 spec.
 */
#define BNX_PCI_BAR0			0x10
#define BNX_PCI_PCIX_CMD		0x40

/****************************************************************************/
/* Convenience definitions.                                                 */
/****************************************************************************/
#define REG_WR(sc, reg, val)		bus_space_write_4(sc->bnx_btag, sc->bnx_bhandle, reg, val)
#define REG_WR16(sc, reg, val)		bus_space_write_2(sc->bnx_btag, sc->bnx_bhandle, reg, val)
#define REG_RD(sc, reg)			bus_space_read_4(sc->bnx_btag, sc->bnx_bhandle, reg)
#define REG_RD_IND(sc, offset)		bnx_reg_rd_ind(sc, offset)
#define REG_WR_IND(sc, offset, val)	bnx_reg_wr_ind(sc, offset, val)
#define CTX_WR(sc, cid_addr, offset, val)	bnx_ctx_wr(sc, cid_addr, offset, val)
#define BNX_SETBIT(sc, reg, x)		REG_WR(sc, reg, (REG_RD(sc, reg) | (x)))
#define BNX_CLRBIT(sc, reg, x)		REG_WR(sc, reg, (REG_RD(sc, reg) & ~(x)))
#define	PCI_SETBIT(pc, tag, reg, x)	pci_conf_write(pc, tag, reg, (pci_conf_read(pc, tag, reg) | (x)))
#define PCI_CLRBIT(pc, tag, reg, x)	pci_conf_write(pc, tag, reg, (pci_conf_read(pc, tag, reg) & ~(x)))

/****************************************************************************/
/* BNX Device State Data Structure                                          */
/****************************************************************************/

#define BNX_STATUS_BLK_SZ		sizeof(struct status_block)
#define BNX_STATS_BLK_SZ		sizeof(struct statistics_block)
#define BNX_TX_CHAIN_PAGE_SZ	BCM_PAGE_SIZE
#define BNX_RX_CHAIN_PAGE_SZ	BCM_PAGE_SIZE

struct bnx_pkt {
	TAILQ_ENTRY(bnx_pkt)     pkt_entry;
	bus_dmamap_t             pkt_dmamap;
	struct mbuf             *pkt_mbuf;
	uint16_t                pkt_end_desc;
};

TAILQ_HEAD(bnx_pkt_list, bnx_pkt);

struct bnx_softc
{
	device_t bnx_dev;
	struct ethercom			bnx_ec;
	struct pci_attach_args		bnx_pa;

	struct ifmedia		bnx_ifmedia;		/* TBI media info */

	bus_space_tag_t		bnx_btag;		/* Device bus tag */
	bus_space_handle_t	bnx_bhandle;		/* Device bus handle */
	bus_size_t		bnx_size;

	void			*bnx_intrhand;		/* Interrupt handler */

	/* packet allocation workqueue */
	struct workqueue	*bnx_wq;

	/* ASIC Chip ID. */
	uint32_t		bnx_chipid;

	/* General controller flags. */
	uint32_t		bnx_flags;

	/* Controller capability flags. */
	uint32_t		bnx_cap_flags;
#define BNX_MSI_CAPABLE_FLAG			0x00000001
#define BNX_MSIX_CAPABLE_FLAG			0x00000002
#define BNX_PCIE_CAPABLE_FLAG			0x00000004
#define BNX_PCIX_CAPABLE_FLAG			0x00000008

	/* PHY specific flags. */
	uint32_t		bnx_phy_flags;

	/* Values that need to be shared with the PHY driver. */
	uint32_t		bnx_shared_hw_cfg;
	uint32_t		bnx_port_hw_cfg;

	uint16_t		bus_speed_mhz;		/* PCI bus speed */
	uint16_t		link_width;		/* PCIe link width */
	uint16_t		link_speed;		/* PCIe link speed */
	struct flash_spec	*bnx_flash_info;     /* Flash NVRAM settings */
	uint32_t		bnx_flash_size;		/* Flash NVRAM size */
	uint32_t		bnx_shmem_base;/* Shared Memory base address */
	char *			bnx_name;		/* Name string */

	/* Tracks the version of bootcode firmware. */
	uint32_t		bnx_fw_ver;

	/* Tracks the state of the firmware.  0 = Running while any     */
	/* other value indicates that the firmware is not responding.   */
	uint16_t		bnx_fw_timed_out;

	/* An incrementing sequence used to coordinate messages passed   */
	/* from the driver to the firmware.                              */
	uint16_t		bnx_fw_wr_seq;

	/* An incrementing sequence used to let the firmware know that   */
	/* the driver is still operating.  Without the pulse, management */
	/* firmware such as IPMI or UMP will operate in OS absent state. */
	uint16_t		bnx_fw_drv_pulse_wr_seq;

	/* Ethernet MAC address. */
	u_char			eaddr[6];

	/* These setting are used by the host coalescing (HC) block to   */
	/* to control how often the status block, statistics block and   */
	/* interrupts are generated.                                     */
	uint16_t		bnx_tx_quick_cons_trip_int;
	uint16_t		bnx_tx_quick_cons_trip;
	uint16_t		bnx_rx_quick_cons_trip_int;
	uint16_t		bnx_rx_quick_cons_trip;
	uint16_t		bnx_comp_prod_trip_int;
	uint16_t		bnx_comp_prod_trip;
	uint16_t		bnx_tx_ticks_int;
	uint16_t		bnx_tx_ticks;
	uint16_t		bnx_rx_ticks_int;
	uint16_t		bnx_rx_ticks;
	uint16_t		bnx_com_ticks_int;
	uint16_t		bnx_com_ticks;
	uint16_t		bnx_cmd_ticks_int;
	uint16_t		bnx_cmd_ticks;
	uint32_t		bnx_stats_ticks;

	/* The address of the integrated PHY on the MII bus. */
	int			bnx_phy_addr;

	/* The device handle for the MII bus child device. */
	struct mii_data		bnx_mii;

	/* Driver maintained TX chain pointers and byte counter. */
	uint16_t		rx_prod;
	uint16_t		rx_cons;
	uint32_t		rx_prod_bseq;	/* Counts the bytes used.  */
	uint16_t		tx_prod;
	uint16_t		tx_cons;
	uint32_t		tx_prod_bseq;	/* Counts the bytes used.  */

	struct callout		bnx_timeout;

	/* Frame size and mbuf allocation size for RX frames. */
	uint32_t		max_frame_size;
	int			mbuf_alloc_size;

	/* Receive mode settings (i.e promiscuous, multicast, etc.). */
	uint32_t		rx_mode;

	/* Bus tag for the bnx controller. */
	bus_dma_tag_t		bnx_dmatag;

	/* H/W maintained TX buffer descriptor chain structure. */
	bus_dma_segment_t	tx_bd_chain_seg[TX_PAGES];
	int			tx_bd_chain_rseg[TX_PAGES];
	bus_dmamap_t		tx_bd_chain_map[TX_PAGES];
	struct tx_bd		*tx_bd_chain[TX_PAGES];
	bus_addr_t		tx_bd_chain_paddr[TX_PAGES];

	/* H/W maintained RX buffer descriptor chain structure. */
	bus_dma_segment_t	rx_bd_chain_seg[RX_PAGES];
	int			rx_bd_chain_rseg[RX_PAGES];
	bus_dmamap_t		rx_bd_chain_map[RX_PAGES];
	struct rx_bd		*rx_bd_chain[RX_PAGES];
	bus_addr_t		rx_bd_chain_paddr[RX_PAGES];

	/* H/W maintained status block. */
	bus_dma_segment_t	status_seg;
	int			status_rseg;
	bus_dmamap_t		status_map;
	struct status_block	*status_block;		/* virtual address */
	bus_addr_t		status_block_paddr;	/* Physical address */

	/* H/W maintained context block */
	int			ctx_pages;
	bus_dma_segment_t	ctx_segs[4];
	int			ctx_rsegs[4];
	bus_dmamap_t		ctx_map[4];
	void			*ctx_block[4];

	/* Driver maintained status block values. */
	uint16_t		last_status_idx;
	uint16_t		hw_rx_cons;
	uint16_t		hw_tx_cons;

	/* H/W maintained statistics block. */
	bus_dma_segment_t	stats_seg;
	int			stats_rseg;
	bus_dmamap_t		stats_map;
	struct statistics_block *stats_block;		/* Virtual address */
	bus_addr_t		stats_block_paddr;	/* Physical address */

	/* Bus tag for RX/TX mbufs. */
	bus_dma_segment_t	rx_mbuf_seg;
	int			rx_mbuf_rseg;
	bus_dma_segment_t	tx_mbuf_seg;
	int			tx_mbuf_rseg;

	/* S/W maintained mbuf TX chain structure. */
	kmutex_t		tx_pkt_mtx;
	uint			tx_pkt_count;
	struct bnx_pkt_list	tx_free_pkts;
	struct bnx_pkt_list	tx_used_pkts;

	/* S/W maintained mbuf RX chain structure. */
	bus_dmamap_t		rx_mbuf_map[TOTAL_RX_BD];
	struct mbuf		*rx_mbuf_ptr[TOTAL_RX_BD];

	/* Track the number of rx_bd and tx_bd's in use. */
	uint16_t 		free_rx_bd;
	uint16_t		max_rx_bd;
	uint16_t		used_tx_bd;
	uint16_t		max_tx_bd;

	/* Provides access to hardware statistics through sysctl. */
	uint64_t 	stat_IfHCInOctets;
	uint64_t 	stat_IfHCInBadOctets;
	uint64_t 	stat_IfHCOutOctets;
	uint64_t 	stat_IfHCOutBadOctets;
	uint64_t 	stat_IfHCInUcastPkts;
	uint64_t 	stat_IfHCInMulticastPkts;
	uint64_t 	stat_IfHCInBroadcastPkts;
	uint64_t 	stat_IfHCOutUcastPkts;
	uint64_t 	stat_IfHCOutMulticastPkts;
	uint64_t 	stat_IfHCOutBroadcastPkts;

	uint32_t	stat_emac_tx_stat_dot3statsinternalmactransmiterrors;
	uint32_t	stat_Dot3StatsCarrierSenseErrors;
	uint32_t	stat_Dot3StatsFCSErrors;
	uint32_t	stat_Dot3StatsAlignmentErrors;
	uint32_t	stat_Dot3StatsSingleCollisionFrames;
	uint32_t	stat_Dot3StatsMultipleCollisionFrames;
	uint32_t	stat_Dot3StatsDeferredTransmissions;
	uint32_t	stat_Dot3StatsExcessiveCollisions;
	uint32_t	stat_Dot3StatsLateCollisions;
	uint32_t	stat_EtherStatsCollisions;
	uint32_t	stat_EtherStatsFragments;
	uint32_t	stat_EtherStatsJabbers;
	uint32_t	stat_EtherStatsUndersizePkts;
	uint32_t	stat_EtherStatsOverrsizePkts;
	uint32_t	stat_EtherStatsPktsRx64Octets;
	uint32_t	stat_EtherStatsPktsRx65Octetsto127Octets;
	uint32_t	stat_EtherStatsPktsRx128Octetsto255Octets;
	uint32_t	stat_EtherStatsPktsRx256Octetsto511Octets;
	uint32_t	stat_EtherStatsPktsRx512Octetsto1023Octets;
	uint32_t	stat_EtherStatsPktsRx1024Octetsto1522Octets;
	uint32_t	stat_EtherStatsPktsRx1523Octetsto9022Octets;
	uint32_t	stat_EtherStatsPktsTx64Octets;
	uint32_t	stat_EtherStatsPktsTx65Octetsto127Octets;
	uint32_t	stat_EtherStatsPktsTx128Octetsto255Octets;
	uint32_t	stat_EtherStatsPktsTx256Octetsto511Octets;
	uint32_t	stat_EtherStatsPktsTx512Octetsto1023Octets;
	uint32_t	stat_EtherStatsPktsTx1024Octetsto1522Octets;
	uint32_t	stat_EtherStatsPktsTx1523Octetsto9022Octets;
	uint32_t	stat_XonPauseFramesReceived;
	uint32_t	stat_XoffPauseFramesReceived;
	uint32_t	stat_OutXonSent;
	uint32_t	stat_OutXoffSent;
	uint32_t	stat_FlowControlDone;
	uint32_t	stat_MacControlFramesReceived;
	uint32_t	stat_XoffStateEntered;
	uint32_t	stat_IfInFramesL2FilterDiscards;
	uint32_t	stat_IfInRuleCheckerDiscards;
	uint32_t	stat_IfInFTQDiscards;
	uint32_t	stat_IfInMBUFDiscards;
	uint32_t	stat_IfInRuleCheckerP4Hit;
	uint32_t	stat_CatchupInRuleCheckerDiscards;
	uint32_t	stat_CatchupInFTQDiscards;
	uint32_t	stat_CatchupInMBUFDiscards;
	uint32_t	stat_CatchupInRuleCheckerP4Hit;

	/* Mbuf allocation failure counter. */
	uint32_t		mbuf_alloc_failed;

	/* TX DMA mapping failure counter. */
	uint32_t		tx_dma_map_failures;

#ifdef BNX_DEBUG
	/* Track the number of enqueued mbufs. */
	int			tx_mbuf_alloc;
	int			rx_mbuf_alloc;

	/* Track the distribution buffer segments. */
	uint32_t		rx_mbuf_segs[BNX_MAX_SEGMENTS+1];

	/* Track how many and what type of interrupts are generated. */
	uint32_t		interrupts_generated;
	uint32_t		interrupts_handled;
	uint32_t		rx_interrupts;
	uint32_t		tx_interrupts;

	uint32_t rx_low_watermark;	/* Lowest number of rx_bd's free. */
	uint32_t rx_empty_count;	/* Number of times the RX chain was empty. */
	uint32_t tx_hi_watermark;	/* Greatest number of tx_bd's used. */
	uint32_t tx_full_count;	/* Number of times the TX chain was full. */
	uint32_t mbuf_sim_alloc_failed;/* Mbuf simulated allocation failure counter. */
	uint32_t l2fhdr_status_errors;
	uint32_t unexpected_attentions;
	uint32_t lost_status_block_updates;
#endif
};

struct bnx_firmware_header {
	int		bnx_COM_FwReleaseMajor;
	int		bnx_COM_FwReleaseMinor;
	int		bnx_COM_FwReleaseFix;
	uint32_t	bnx_COM_FwStartAddr;
	uint32_t	bnx_COM_FwTextAddr;
	int		bnx_COM_FwTextLen;
	uint32_t	bnx_COM_FwDataAddr;
	int		bnx_COM_FwDataLen;
	uint32_t	bnx_COM_FwRodataAddr;
	int		bnx_COM_FwRodataLen;
	uint32_t	bnx_COM_FwBssAddr;
	int		bnx_COM_FwBssLen;
	uint32_t	bnx_COM_FwSbssAddr;
	int		bnx_COM_FwSbssLen;

	int		bnx_RXP_FwReleaseMajor;
	int		bnx_RXP_FwReleaseMinor;
	int		bnx_RXP_FwReleaseFix;
	uint32_t	bnx_RXP_FwStartAddr;
	uint32_t	bnx_RXP_FwTextAddr;
	int		bnx_RXP_FwTextLen;
	uint32_t	bnx_RXP_FwDataAddr;
	int		bnx_RXP_FwDataLen;
	uint32_t	bnx_RXP_FwRodataAddr;
	int		bnx_RXP_FwRodataLen;
	uint32_t	bnx_RXP_FwBssAddr;
	int		bnx_RXP_FwBssLen;
	uint32_t	bnx_RXP_FwSbssAddr;
	int		bnx_RXP_FwSbssLen;

	int		bnx_TPAT_FwReleaseMajor;
	int		bnx_TPAT_FwReleaseMinor;
	int		bnx_TPAT_FwReleaseFix;
	uint32_t	bnx_TPAT_FwStartAddr;
	uint32_t	bnx_TPAT_FwTextAddr;
	int		bnx_TPAT_FwTextLen;
	uint32_t	bnx_TPAT_FwDataAddr;
	int		bnx_TPAT_FwDataLen;
	uint32_t	bnx_TPAT_FwRodataAddr;
	int		bnx_TPAT_FwRodataLen;
	uint32_t	bnx_TPAT_FwBssAddr;
	int		bnx_TPAT_FwBssLen;
	uint32_t	bnx_TPAT_FwSbssAddr;
	int		bnx_TPAT_FwSbssLen;

	int		bnx_TXP_FwReleaseMajor;
	int		bnx_TXP_FwReleaseMinor;
	int		bnx_TXP_FwReleaseFix;
	uint32_t	bnx_TXP_FwStartAddr;
	uint32_t	bnx_TXP_FwTextAddr;
	int		bnx_TXP_FwTextLen;
	uint32_t	bnx_TXP_FwDataAddr;
	int		bnx_TXP_FwDataLen;
	uint32_t	bnx_TXP_FwRodataAddr;
	int		bnx_TXP_FwRodataLen;
	uint32_t	bnx_TXP_FwBssAddr;
	int		bnx_TXP_FwBssLen;
	uint32_t	bnx_TXP_FwSbssAddr;
	int		bnx_TXP_FwSbssLen;

	/* Followed by blocks of data, each sized according to
	 * the (rather obvious) block length stated above.
	 *
	 * bnx_COM_FwText, bnx_COM_FwData, bnx_COM_FwRodata,
	 * bnx_COM_FwBss, bnx_COM_FwSbss,
	 *
	 * bnx_RXP_FwText, bnx_RXP_FwData, bnx_RXP_FwRodata,
	 * bnx_RXP_FwBss, bnx_RXP_FwSbss,
	 *
	 * bnx_TPAT_FwText, bnx_TPAT_FwData, bnx_TPAT_FwRodata,
	 * bnx_TPAT_FwBss, bnx_TPAT_FwSbss,
	 *
	 * bnx_TXP_FwText, bnx_TXP_FwData, bnx_TXP_FwRodata,
	 * bnx_TXP_FwBss, bnx_TXP_FwSbss,
	 */
};

#endif /* _DEV_PCI_IF_BNXVAR_H_ */

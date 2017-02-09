/*	$NetBSD: if_wm.c,v 1.351 2015/10/08 09:28:13 msaitoh Exp $	*/

/*
 * Copyright (c) 2001, 2002, 2003, 2004 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*******************************************************************************

  Copyright (c) 2001-2005, Intel Corporation
  All rights reserved.
 
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
 
   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
 
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
 
   3. Neither the name of the Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.
 
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/
/*
 * Device driver for the Intel i8254x family of Gigabit Ethernet chips.
 *
 * TODO (in order of importance):
 *
 *	- Check XXX'ed comments
 *	- EEE (Energy Efficiency Ethernet)
 *	- Multi queue
 *	- Image Unique ID
 *	- LPLU other than PCH*
 *	- Virtual Function
 *	- Set LED correctly (based on contents in EEPROM)
 *	- Rework how parameters are loaded from the EEPROM.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_wm.c,v 1.351 2015/10/08 09:28:13 msaitoh Exp $");

#ifdef _KERNEL_OPT
#include "opt_net_mpsafe.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/syslog.h>
#include <sys/interrupt.h>

#include <sys/rndsource.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <net/bpf.h>

#include <netinet/in.h>			/* XXX for struct ip */
#include <netinet/in_systm.h>		/* XXX for struct ip */
#include <netinet/ip.h>			/* XXX for struct ip */
#include <netinet/ip6.h>		/* XXX for struct ip6_hdr */
#include <netinet/tcp.h>		/* XXX for struct tcphdr */

#include <sys/bus.h>
#include <sys/intr.h>
#include <machine/endian.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/miidevs.h>
#include <dev/mii/mii_bitbang.h>
#include <dev/mii/ikphyreg.h>
#include <dev/mii/igphyreg.h>
#include <dev/mii/igphyvar.h>
#include <dev/mii/inbmphyreg.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/if_wmreg.h>
#include <dev/pci/if_wmvar.h>

#ifdef WM_DEBUG
#define	WM_DEBUG_LINK		0x01
#define	WM_DEBUG_TX		0x02
#define	WM_DEBUG_RX		0x04
#define	WM_DEBUG_GMII		0x08
#define	WM_DEBUG_MANAGE		0x10
#define	WM_DEBUG_NVM		0x20
int	wm_debug = WM_DEBUG_TX | WM_DEBUG_RX | WM_DEBUG_LINK | WM_DEBUG_GMII
    | WM_DEBUG_MANAGE | WM_DEBUG_NVM;

#define	DPRINTF(x, y)	if (wm_debug & (x)) printf y
#else
#define	DPRINTF(x, y)	/* nothing */
#endif /* WM_DEBUG */

#ifdef NET_MPSAFE
#define WM_MPSAFE	1
#endif

#ifdef __HAVE_PCI_MSI_MSIX
#define WM_MSI_MSIX	1 /* Enable by default */
#endif

/*
 * This device driver divides interrupt to TX, RX and link state.
 * Each MSI-X vector indexes are below.
 */
#define WM_MSIX_NINTR		3
#define WM_MSIX_TXINTR_IDX	0
#define WM_MSIX_RXINTR_IDX	1
#define WM_MSIX_LINKINTR_IDX	2
#define WM_MAX_NINTR		WM_MSIX_NINTR

/*
 * This device driver set affinity to each interrupts like below (round-robin).
 * If the number CPUs is less than the number of interrupts, this driver usase
 * the same CPU for multiple interrupts.
 */
#define WM_MSIX_TXINTR_CPUID	0
#define WM_MSIX_RXINTR_CPUID	1
#define WM_MSIX_LINKINTR_CPUID	2

/*
 * Transmit descriptor list size.  Due to errata, we can only have
 * 256 hardware descriptors in the ring on < 82544, but we use 4096
 * on >= 82544.  We tell the upper layers that they can queue a lot
 * of packets, and we go ahead and manage up to 64 (16 for the i82547)
 * of them at a time.
 *
 * We allow up to 256 (!) DMA segments per packet.  Pathological packet
 * chains containing many small mbufs have been observed in zero-copy
 * situations with jumbo frames.
 */
#define	WM_NTXSEGS		256
#define	WM_IFQUEUELEN		256
#define	WM_TXQUEUELEN_MAX	64
#define	WM_TXQUEUELEN_MAX_82547	16
#define	WM_TXQUEUELEN(sc)	((sc)->sc_txnum)
#define	WM_TXQUEUELEN_MASK(sc)	(WM_TXQUEUELEN(sc) - 1)
#define	WM_TXQUEUE_GC(sc)	(WM_TXQUEUELEN(sc) / 8)
#define	WM_NTXDESC_82542	256
#define	WM_NTXDESC_82544	4096
#define	WM_NTXDESC(sc)		((sc)->sc_ntxdesc)
#define	WM_NTXDESC_MASK(sc)	(WM_NTXDESC(sc) - 1)
#define	WM_TXDESCSIZE(sc)	(WM_NTXDESC(sc) * sizeof(wiseman_txdesc_t))
#define	WM_NEXTTX(sc, x)	(((x) + 1) & WM_NTXDESC_MASK(sc))
#define	WM_NEXTTXS(sc, x)	(((x) + 1) & WM_TXQUEUELEN_MASK(sc))

#define	WM_MAXTXDMA		 (2 * round_page(IP_MAXPACKET)) /* for TSO */

/*
 * Receive descriptor list size.  We have one Rx buffer for normal
 * sized packets.  Jumbo packets consume 5 Rx buffers for a full-sized
 * packet.  We allocate 256 receive descriptors, each with a 2k
 * buffer (MCLBYTES), which gives us room for 50 jumbo packets.
 */
#define	WM_NRXDESC		256
#define	WM_NRXDESC_MASK		(WM_NRXDESC - 1)
#define	WM_NEXTRX(x)		(((x) + 1) & WM_NRXDESC_MASK)
#define	WM_PREVRX(x)		(((x) - 1) & WM_NRXDESC_MASK)

/*
 * Control structures are DMA'd to the i82542 chip.  We allocate them in
 * a single clump that maps to a single DMA segment to make several things
 * easier.
 */
struct wm_control_data_82544 {
	/*
	 * The receive descriptors.
	 */
	wiseman_rxdesc_t wcd_rxdescs[WM_NRXDESC];

	/*
	 * The transmit descriptors.  Put these at the end, because
	 * we might use a smaller number of them.
	 */
	union {
		wiseman_txdesc_t wcdu_txdescs[WM_NTXDESC_82544];
		nq_txdesc_t      wcdu_nq_txdescs[WM_NTXDESC_82544];
	} wdc_u;
};

struct wm_control_data_82542 {
	wiseman_rxdesc_t wcd_rxdescs[WM_NRXDESC];
	wiseman_txdesc_t wcd_txdescs[WM_NTXDESC_82542];
};

#define	WM_CDOFF(x)	offsetof(struct wm_control_data_82544, x)
#define	WM_CDTXOFF(x)	WM_CDOFF(wdc_u.wcdu_txdescs[(x)])
#define	WM_CDRXOFF(x)	WM_CDOFF(wcd_rxdescs[(x)])

/*
 * Software state for transmit jobs.
 */
struct wm_txsoft {
	struct mbuf *txs_mbuf;		/* head of our mbuf chain */
	bus_dmamap_t txs_dmamap;	/* our DMA map */
	int txs_firstdesc;		/* first descriptor in packet */
	int txs_lastdesc;		/* last descriptor in packet */
	int txs_ndesc;			/* # of descriptors used */
};

/*
 * Software state for receive buffers.  Each descriptor gets a
 * 2k (MCLBYTES) buffer and a DMA map.  For packets which fill
 * more than one buffer, we chain them together.
 */
struct wm_rxsoft {
	struct mbuf *rxs_mbuf;		/* head of our mbuf chain */
	bus_dmamap_t rxs_dmamap;	/* our DMA map */
};

#define WM_LINKUP_TIMEOUT	50

static uint16_t swfwphysem[] = {
	SWFW_PHY0_SM,
	SWFW_PHY1_SM,
	SWFW_PHY2_SM,
	SWFW_PHY3_SM
};

static const uint32_t wm_82580_rxpbs_table[] = {
	36, 72, 144, 1, 2, 4, 8, 16, 35, 70, 140
};

/*
 * Software state per device.
 */
struct wm_softc {
	device_t sc_dev;		/* generic device information */
	bus_space_tag_t sc_st;		/* bus space tag */
	bus_space_handle_t sc_sh;	/* bus space handle */
	bus_size_t sc_ss;		/* bus space size */
	bus_space_tag_t sc_iot;		/* I/O space tag */
	bus_space_handle_t sc_ioh;	/* I/O space handle */
	bus_size_t sc_ios;		/* I/O space size */
	bus_space_tag_t sc_flasht;	/* flash registers space tag */
	bus_space_handle_t sc_flashh;	/* flash registers space handle */
	bus_size_t sc_flashs;		/* flash registers space size */
	bus_dma_tag_t sc_dmat;		/* bus DMA tag */

	struct ethercom sc_ethercom;	/* ethernet common data */
	struct mii_data sc_mii;		/* MII/media information */

	pci_chipset_tag_t sc_pc;
	pcitag_t sc_pcitag;
	int sc_bus_speed;		/* PCI/PCIX bus speed */
	int sc_pcixe_capoff;		/* PCI[Xe] capability reg offset */

	uint16_t sc_pcidevid;		/* PCI device ID */
	wm_chip_type sc_type;		/* MAC type */
	int sc_rev;			/* MAC revision */
	wm_phy_type sc_phytype;		/* PHY type */
	uint32_t sc_mediatype;		/* Media type (Copper, Fiber, SERDES)*/
#define	WM_MEDIATYPE_UNKNOWN		0x00
#define	WM_MEDIATYPE_FIBER		0x01
#define	WM_MEDIATYPE_COPPER		0x02
#define	WM_MEDIATYPE_SERDES		0x03 /* Internal SERDES */
	int sc_funcid;			/* unit number of the chip (0 to 3) */
	int sc_flags;			/* flags; see below */
	int sc_if_flags;		/* last if_flags */
	int sc_flowflags;		/* 802.3x flow control flags */
	int sc_align_tweak;

	void *sc_ihs[WM_MAX_NINTR];	/*
					 * interrupt cookie.
					 * legacy and msi use sc_ihs[0].
					 */
	pci_intr_handle_t *sc_intrs;	/* legacy and msi use sc_intrs[0] */
	int sc_nintrs;			/* number of interrupts */

	callout_t sc_tick_ch;		/* tick callout */
	bool sc_stopping;

	int sc_nvm_ver_major;
	int sc_nvm_ver_minor;
	int sc_nvm_ver_build;
	int sc_nvm_addrbits;		/* NVM address bits */
	unsigned int sc_nvm_wordsize;	/* NVM word size */
	int sc_ich8_flash_base;
	int sc_ich8_flash_bank_size;
	int sc_nvm_k1_enabled;

	/* Software state for the transmit and receive descriptors. */
	int sc_txnum;			/* must be a power of two */
	struct wm_txsoft sc_txsoft[WM_TXQUEUELEN_MAX];
	struct wm_rxsoft sc_rxsoft[WM_NRXDESC];

	/* Control data structures. */
	int sc_ntxdesc;			/* must be a power of two */
	struct wm_control_data_82544 *sc_control_data;
	bus_dmamap_t sc_cddmamap;	/* control data DMA map */
	bus_dma_segment_t sc_cd_seg;	/* control data segment */
	int sc_cd_rseg;			/* real number of control segment */
	size_t sc_cd_size;		/* control data size */
#define	sc_cddma	sc_cddmamap->dm_segs[0].ds_addr
#define	sc_txdescs	sc_control_data->wdc_u.wcdu_txdescs
#define	sc_nq_txdescs	sc_control_data->wdc_u.wcdu_nq_txdescs
#define	sc_rxdescs	sc_control_data->wcd_rxdescs

#ifdef WM_EVENT_COUNTERS
	/* Event counters. */
	struct evcnt sc_ev_txsstall;	/* Tx stalled due to no txs */
	struct evcnt sc_ev_txdstall;	/* Tx stalled due to no txd */
	struct evcnt sc_ev_txfifo_stall;/* Tx FIFO stalls (82547) */
	struct evcnt sc_ev_txdw;	/* Tx descriptor interrupts */
	struct evcnt sc_ev_txqe;	/* Tx queue empty interrupts */
	struct evcnt sc_ev_rxintr;	/* Rx interrupts */
	struct evcnt sc_ev_linkintr;	/* Link interrupts */

	struct evcnt sc_ev_rxipsum;	/* IP checksums checked in-bound */
	struct evcnt sc_ev_rxtusum;	/* TCP/UDP cksums checked in-bound */
	struct evcnt sc_ev_txipsum;	/* IP checksums comp. out-bound */
	struct evcnt sc_ev_txtusum;	/* TCP/UDP cksums comp. out-bound */
	struct evcnt sc_ev_txtusum6;	/* TCP/UDP v6 cksums comp. out-bound */
	struct evcnt sc_ev_txtso;	/* TCP seg offload out-bound (IPv4) */
	struct evcnt sc_ev_txtso6;	/* TCP seg offload out-bound (IPv6) */
	struct evcnt sc_ev_txtsopain;	/* painful header manip. for TSO */

	struct evcnt sc_ev_txseg[WM_NTXSEGS]; /* Tx packets w/ N segments */
	struct evcnt sc_ev_txdrop;	/* Tx packets dropped (too many segs) */

	struct evcnt sc_ev_tu;		/* Tx underrun */

	struct evcnt sc_ev_tx_xoff;	/* Tx PAUSE(!0) frames */
	struct evcnt sc_ev_tx_xon;	/* Tx PAUSE(0) frames */
	struct evcnt sc_ev_rx_xoff;	/* Rx PAUSE(!0) frames */
	struct evcnt sc_ev_rx_xon;	/* Rx PAUSE(0) frames */
	struct evcnt sc_ev_rx_macctl;	/* Rx Unsupported */
#endif /* WM_EVENT_COUNTERS */

	bus_addr_t sc_tdt_reg;		/* offset of TDT register */

	int	sc_txfree;		/* number of free Tx descriptors */
	int	sc_txnext;		/* next ready Tx descriptor */

	int	sc_txsfree;		/* number of free Tx jobs */
	int	sc_txsnext;		/* next free Tx job */
	int	sc_txsdirty;		/* dirty Tx jobs */

	/* These 5 variables are used only on the 82547. */
	int	sc_txfifo_size;		/* Tx FIFO size */
	int	sc_txfifo_head;		/* current head of FIFO */
	uint32_t sc_txfifo_addr;	/* internal address of start of FIFO */
	int	sc_txfifo_stall;	/* Tx FIFO is stalled */
	callout_t sc_txfifo_ch;		/* Tx FIFO stall work-around timer */

	bus_addr_t sc_rdt_reg;		/* offset of RDT register */

	int	sc_rxptr;		/* next ready Rx descriptor/queue ent */
	int	sc_rxdiscard;
	int	sc_rxlen;
	struct mbuf *sc_rxhead;
	struct mbuf *sc_rxtail;
	struct mbuf **sc_rxtailp;

	uint32_t sc_ctrl;		/* prototype CTRL register */
#if 0
	uint32_t sc_ctrl_ext;		/* prototype CTRL_EXT register */
#endif
	uint32_t sc_icr;		/* prototype interrupt bits */
	uint32_t sc_itr;		/* prototype intr throttling reg */
	uint32_t sc_tctl;		/* prototype TCTL register */
	uint32_t sc_rctl;		/* prototype RCTL register */
	uint32_t sc_txcw;		/* prototype TXCW register */
	uint32_t sc_tipg;		/* prototype TIPG register */
	uint32_t sc_fcrtl;		/* prototype FCRTL register */
	uint32_t sc_pba;		/* prototype PBA register */

	int sc_tbi_linkup;		/* TBI link status */
	int sc_tbi_serdes_anegticks;	/* autonegotiation ticks */
	int sc_tbi_serdes_ticks;	/* tbi ticks */

	int sc_mchash_type;		/* multicast filter offset */

	krndsource_t rnd_source;	/* random source */

	kmutex_t *sc_tx_lock;		/* lock for tx operations */
	kmutex_t *sc_rx_lock;		/* lock for rx operations */
};

#define WM_TX_LOCK(_sc)		if ((_sc)->sc_tx_lock) mutex_enter((_sc)->sc_tx_lock)
#define WM_TX_UNLOCK(_sc)	if ((_sc)->sc_tx_lock) mutex_exit((_sc)->sc_tx_lock)
#define WM_TX_LOCKED(_sc)	(!(_sc)->sc_tx_lock || mutex_owned((_sc)->sc_tx_lock))
#define WM_RX_LOCK(_sc)		if ((_sc)->sc_rx_lock) mutex_enter((_sc)->sc_rx_lock)
#define WM_RX_UNLOCK(_sc)	if ((_sc)->sc_rx_lock) mutex_exit((_sc)->sc_rx_lock)
#define WM_RX_LOCKED(_sc)	(!(_sc)->sc_rx_lock || mutex_owned((_sc)->sc_rx_lock))
#define WM_BOTH_LOCK(_sc)	do {WM_TX_LOCK(_sc); WM_RX_LOCK(_sc);} while (0)
#define WM_BOTH_UNLOCK(_sc)	do {WM_RX_UNLOCK(_sc); WM_TX_UNLOCK(_sc);} while (0)
#define WM_BOTH_LOCKED(_sc)	(WM_TX_LOCKED(_sc) && WM_RX_LOCKED(_sc))

#ifdef WM_MPSAFE
#define CALLOUT_FLAGS	CALLOUT_MPSAFE
#else
#define CALLOUT_FLAGS	0
#endif

#define	WM_RXCHAIN_RESET(sc)						\
do {									\
	(sc)->sc_rxtailp = &(sc)->sc_rxhead;				\
	*(sc)->sc_rxtailp = NULL;					\
	(sc)->sc_rxlen = 0;						\
} while (/*CONSTCOND*/0)

#define	WM_RXCHAIN_LINK(sc, m)						\
do {									\
	*(sc)->sc_rxtailp = (sc)->sc_rxtail = (m);			\
	(sc)->sc_rxtailp = &(m)->m_next;				\
} while (/*CONSTCOND*/0)

#ifdef WM_EVENT_COUNTERS
#define	WM_EVCNT_INCR(ev)	(ev)->ev_count++
#define	WM_EVCNT_ADD(ev, val)	(ev)->ev_count += (val)
#else
#define	WM_EVCNT_INCR(ev)	/* nothing */
#define	WM_EVCNT_ADD(ev, val)	/* nothing */
#endif

#define	CSR_READ(sc, reg)						\
	bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (reg))
#define	CSR_WRITE(sc, reg, val)						\
	bus_space_write_4((sc)->sc_st, (sc)->sc_sh, (reg), (val))
#define	CSR_WRITE_FLUSH(sc)						\
	(void) CSR_READ((sc), WMREG_STATUS)

#define ICH8_FLASH_READ32(sc, reg) \
	bus_space_read_4((sc)->sc_flasht, (sc)->sc_flashh, (reg))
#define ICH8_FLASH_WRITE32(sc, reg, data) \
	bus_space_write_4((sc)->sc_flasht, (sc)->sc_flashh, (reg), (data))

#define ICH8_FLASH_READ16(sc, reg) \
	bus_space_read_2((sc)->sc_flasht, (sc)->sc_flashh, (reg))
#define ICH8_FLASH_WRITE16(sc, reg, data) \
	bus_space_write_2((sc)->sc_flasht, (sc)->sc_flashh, (reg), (data))

#define	WM_CDTXADDR(sc, x)	((sc)->sc_cddma + WM_CDTXOFF((x)))
#define	WM_CDRXADDR(sc, x)	((sc)->sc_cddma + WM_CDRXOFF((x)))

#define	WM_CDTXADDR_LO(sc, x)	(WM_CDTXADDR((sc), (x)) & 0xffffffffU)
#define	WM_CDTXADDR_HI(sc, x)						\
	(sizeof(bus_addr_t) == 8 ?					\
	 (uint64_t)WM_CDTXADDR((sc), (x)) >> 32 : 0)

#define	WM_CDRXADDR_LO(sc, x)	(WM_CDRXADDR((sc), (x)) & 0xffffffffU)
#define	WM_CDRXADDR_HI(sc, x)						\
	(sizeof(bus_addr_t) == 8 ?					\
	 (uint64_t)WM_CDRXADDR((sc), (x)) >> 32 : 0)

#define	WM_CDTXSYNC(sc, x, n, ops)					\
do {									\
	int __x, __n;							\
									\
	__x = (x);							\
	__n = (n);							\
									\
	/* If it will wrap around, sync to the end of the ring. */	\
	if ((__x + __n) > WM_NTXDESC(sc)) {				\
		bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,	\
		    WM_CDTXOFF(__x), sizeof(wiseman_txdesc_t) *		\
		    (WM_NTXDESC(sc) - __x), (ops));			\
		__n -= (WM_NTXDESC(sc) - __x);				\
		__x = 0;						\
	}								\
									\
	/* Now sync whatever is left. */				\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	    WM_CDTXOFF(__x), sizeof(wiseman_txdesc_t) * __n, (ops));	\
} while (/*CONSTCOND*/0)

#define	WM_CDRXSYNC(sc, x, ops)						\
do {									\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_cddmamap,		\
	   WM_CDRXOFF((x)), sizeof(wiseman_rxdesc_t), (ops));		\
} while (/*CONSTCOND*/0)

#define	WM_INIT_RXDESC(sc, x)						\
do {									\
	struct wm_rxsoft *__rxs = &(sc)->sc_rxsoft[(x)];		\
	wiseman_rxdesc_t *__rxd = &(sc)->sc_rxdescs[(x)];		\
	struct mbuf *__m = __rxs->rxs_mbuf;				\
									\
	/*								\
	 * Note: We scoot the packet forward 2 bytes in the buffer	\
	 * so that the payload after the Ethernet header is aligned	\
	 * to a 4-byte boundary.					\
	 *								\
	 * XXX BRAINDAMAGE ALERT!					\
	 * The stupid chip uses the same size for every buffer, which	\
	 * is set in the Receive Control register.  We are using the 2K	\
	 * size option, but what we REALLY want is (2K - 2)!  For this	\
	 * reason, we can't "scoot" packets longer than the standard	\
	 * Ethernet MTU.  On strict-alignment platforms, if the total	\
	 * size exceeds (2K - 2) we set align_tweak to 0 and let	\
	 * the upper layer copy the headers.				\
	 */								\
	__m->m_data = __m->m_ext.ext_buf + (sc)->sc_align_tweak;	\
									\
	wm_set_dma_addr(&__rxd->wrx_addr,				\
	    __rxs->rxs_dmamap->dm_segs[0].ds_addr + (sc)->sc_align_tweak); \
	__rxd->wrx_len = 0;						\
	__rxd->wrx_cksum = 0;						\
	__rxd->wrx_status = 0;						\
	__rxd->wrx_errors = 0;						\
	__rxd->wrx_special = 0;						\
	WM_CDRXSYNC((sc), (x), BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE); \
									\
	CSR_WRITE((sc), (sc)->sc_rdt_reg, (x));				\
} while (/*CONSTCOND*/0)

/*
 * Register read/write functions.
 * Other than CSR_{READ|WRITE}().
 */
#if 0
static inline uint32_t wm_io_read(struct wm_softc *, int);
#endif
static inline void wm_io_write(struct wm_softc *, int, uint32_t);
static inline void wm_82575_write_8bit_ctlr_reg(struct wm_softc *, uint32_t,
	uint32_t, uint32_t);
static inline void wm_set_dma_addr(volatile wiseman_addr_t *, bus_addr_t);

/*
 * Device driver interface functions and commonly used functions.
 * match, attach, detach, init, start, stop, ioctl, watchdog and so on.
 */
static const struct wm_product *wm_lookup(const struct pci_attach_args *);
static int	wm_match(device_t, cfdata_t, void *);
static void	wm_attach(device_t, device_t, void *);
static int	wm_detach(device_t, int);
static bool	wm_suspend(device_t, const pmf_qual_t *);
static bool	wm_resume(device_t, const pmf_qual_t *);
static void	wm_watchdog(struct ifnet *);
static void	wm_tick(void *);
static int	wm_ifflags_cb(struct ethercom *);
static int	wm_ioctl(struct ifnet *, u_long, void *);
/* MAC address related */
static uint16_t	wm_check_alt_mac_addr(struct wm_softc *);
static int	wm_read_mac_addr(struct wm_softc *, uint8_t *);
static void	wm_set_ral(struct wm_softc *, const uint8_t *, int);
static uint32_t	wm_mchash(struct wm_softc *, const uint8_t *);
static void	wm_set_filter(struct wm_softc *);
/* Reset and init related */
static void	wm_set_vlan(struct wm_softc *);
static void	wm_set_pcie_completion_timeout(struct wm_softc *);
static void	wm_get_auto_rd_done(struct wm_softc *);
static void	wm_lan_init_done(struct wm_softc *);
static void	wm_get_cfg_done(struct wm_softc *);
static void	wm_initialize_hardware_bits(struct wm_softc *);
static uint32_t	wm_rxpbs_adjust_82580(uint32_t);
static void	wm_reset(struct wm_softc *);
static int	wm_add_rxbuf(struct wm_softc *, int);
static void	wm_rxdrain(struct wm_softc *);
static int	wm_init(struct ifnet *);
static int	wm_init_locked(struct ifnet *);
static void	wm_stop(struct ifnet *, int);
static void	wm_stop_locked(struct ifnet *, int);
static int	wm_tx_offload(struct wm_softc *, struct wm_txsoft *,
    uint32_t *, uint8_t *);
static void	wm_dump_mbuf_chain(struct wm_softc *, struct mbuf *);
static void	wm_82547_txfifo_stall(void *);
static int	wm_82547_txfifo_bugchk(struct wm_softc *, struct mbuf *);
/* Start */
static void	wm_start(struct ifnet *);
static void	wm_start_locked(struct ifnet *);
static int	wm_nq_tx_offload(struct wm_softc *, struct wm_txsoft *,
    uint32_t *, uint32_t *, bool *);
static void	wm_nq_start(struct ifnet *);
static void	wm_nq_start_locked(struct ifnet *);
/* Interrupt */
static int	wm_txeof(struct wm_softc *);
static void	wm_rxeof(struct wm_softc *);
static void	wm_linkintr_gmii(struct wm_softc *, uint32_t);
static void	wm_linkintr_tbi(struct wm_softc *, uint32_t);
static void	wm_linkintr_serdes(struct wm_softc *, uint32_t);
static void	wm_linkintr(struct wm_softc *, uint32_t);
static int	wm_intr_legacy(void *);
#ifdef WM_MSI_MSIX
static int	wm_txintr_msix(void *);
static int	wm_rxintr_msix(void *);
static int	wm_linkintr_msix(void *);
#endif

/*
 * Media related.
 * GMII, SGMII, TBI, SERDES and SFP.
 */
/* Common */
static void	wm_tbi_serdes_set_linkled(struct wm_softc *);
/* GMII related */
static void	wm_gmii_reset(struct wm_softc *);
static int	wm_get_phy_id_82575(struct wm_softc *);
static void	wm_gmii_mediainit(struct wm_softc *, pci_product_id_t);
static int	wm_gmii_mediachange(struct ifnet *);
static void	wm_gmii_mediastatus(struct ifnet *, struct ifmediareq *);
static void	wm_i82543_mii_sendbits(struct wm_softc *, uint32_t, int);
static uint32_t	wm_i82543_mii_recvbits(struct wm_softc *);
static int	wm_gmii_i82543_readreg(device_t, int, int);
static void	wm_gmii_i82543_writereg(device_t, int, int, int);
static int	wm_gmii_i82544_readreg(device_t, int, int);
static void	wm_gmii_i82544_writereg(device_t, int, int, int);
static int	wm_gmii_i80003_readreg(device_t, int, int);
static void	wm_gmii_i80003_writereg(device_t, int, int, int);
static int	wm_gmii_bm_readreg(device_t, int, int);
static void	wm_gmii_bm_writereg(device_t, int, int, int);
static void	wm_access_phy_wakeup_reg_bm(device_t, int, int16_t *, int);
static int	wm_gmii_hv_readreg(device_t, int, int);
static void	wm_gmii_hv_writereg(device_t, int, int, int);
static int	wm_gmii_82580_readreg(device_t, int, int);
static void	wm_gmii_82580_writereg(device_t, int, int, int);
static int	wm_gmii_gs40g_readreg(device_t, int, int);
static void	wm_gmii_gs40g_writereg(device_t, int, int, int);
static void	wm_gmii_statchg(struct ifnet *);
static int	wm_kmrn_readreg(struct wm_softc *, int);
static void	wm_kmrn_writereg(struct wm_softc *, int, int);
/* SGMII */
static bool	wm_sgmii_uses_mdio(struct wm_softc *);
static int	wm_sgmii_readreg(device_t, int, int);
static void	wm_sgmii_writereg(device_t, int, int, int);
/* TBI related */
static void	wm_tbi_mediainit(struct wm_softc *);
static int	wm_tbi_mediachange(struct ifnet *);
static void	wm_tbi_mediastatus(struct ifnet *, struct ifmediareq *);
static int	wm_check_for_link(struct wm_softc *);
static void	wm_tbi_tick(struct wm_softc *);
/* SERDES related */
static void	wm_serdes_power_up_link_82575(struct wm_softc *);
static int	wm_serdes_mediachange(struct ifnet *);
static void	wm_serdes_mediastatus(struct ifnet *, struct ifmediareq *);
static void	wm_serdes_tick(struct wm_softc *);
/* SFP related */
static int	wm_sfp_read_data_byte(struct wm_softc *, uint16_t, uint8_t *);
static uint32_t	wm_sfp_get_media_type(struct wm_softc *);

/*
 * NVM related.
 * Microwire, SPI (w/wo EERD) and Flash.
 */
/* Misc functions */
static void	wm_eeprom_sendbits(struct wm_softc *, uint32_t, int);
static void	wm_eeprom_recvbits(struct wm_softc *, uint32_t *, int);
static int	wm_nvm_set_addrbits_size_eecd(struct wm_softc *);
/* Microwire */
static int	wm_nvm_read_uwire(struct wm_softc *, int, int, uint16_t *);
/* SPI */
static int	wm_nvm_ready_spi(struct wm_softc *);
static int	wm_nvm_read_spi(struct wm_softc *, int, int, uint16_t *);
/* Using with EERD */
static int	wm_poll_eerd_eewr_done(struct wm_softc *, int);
static int	wm_nvm_read_eerd(struct wm_softc *, int, int, uint16_t *);
/* Flash */
static int	wm_nvm_valid_bank_detect_ich8lan(struct wm_softc *,
    unsigned int *);
static int32_t	wm_ich8_cycle_init(struct wm_softc *);
static int32_t	wm_ich8_flash_cycle(struct wm_softc *, uint32_t);
static int32_t	wm_read_ich8_data(struct wm_softc *, uint32_t, uint32_t,
	uint16_t *);
static int32_t	wm_read_ich8_byte(struct wm_softc *, uint32_t, uint8_t *);
static int32_t	wm_read_ich8_word(struct wm_softc *, uint32_t, uint16_t *);
static int	wm_nvm_read_ich8(struct wm_softc *, int, int, uint16_t *);
/* iNVM */
static int	wm_nvm_read_word_invm(struct wm_softc *, uint16_t, uint16_t *);
static int	wm_nvm_read_invm(struct wm_softc *, int, int, uint16_t *);
/* Lock, detecting NVM type, validate checksum and read */
static int	wm_nvm_acquire(struct wm_softc *);
static void	wm_nvm_release(struct wm_softc *);
static int	wm_nvm_is_onboard_eeprom(struct wm_softc *);
static int	wm_nvm_get_flash_presence_i210(struct wm_softc *);
static int	wm_nvm_validate_checksum(struct wm_softc *);
static void	wm_nvm_version_invm(struct wm_softc *);
static void	wm_nvm_version(struct wm_softc *);
static int	wm_nvm_read(struct wm_softc *, int, int, uint16_t *);

/*
 * Hardware semaphores.
 * Very complexed...
 */
static int	wm_get_swsm_semaphore(struct wm_softc *);
static void	wm_put_swsm_semaphore(struct wm_softc *);
static int	wm_get_swfw_semaphore(struct wm_softc *, uint16_t);
static void	wm_put_swfw_semaphore(struct wm_softc *, uint16_t);
static int	wm_get_swfwhw_semaphore(struct wm_softc *);
static void	wm_put_swfwhw_semaphore(struct wm_softc *);
static int	wm_get_hw_semaphore_82573(struct wm_softc *);
static void	wm_put_hw_semaphore_82573(struct wm_softc *);

/*
 * Management mode and power management related subroutines.
 * BMC, AMT, suspend/resume and EEE.
 */
static int	wm_check_mng_mode(struct wm_softc *);
static int	wm_check_mng_mode_ich8lan(struct wm_softc *);
static int	wm_check_mng_mode_82574(struct wm_softc *);
static int	wm_check_mng_mode_generic(struct wm_softc *);
static int	wm_enable_mng_pass_thru(struct wm_softc *);
static int	wm_check_reset_block(struct wm_softc *);
static void	wm_get_hw_control(struct wm_softc *);
static void	wm_release_hw_control(struct wm_softc *);
static void	wm_gate_hw_phy_config_ich8lan(struct wm_softc *, int);
static void	wm_smbustopci(struct wm_softc *);
static void	wm_init_manageability(struct wm_softc *);
static void	wm_release_manageability(struct wm_softc *);
static void	wm_get_wakeup(struct wm_softc *);
#ifdef WM_WOL
static void	wm_enable_phy_wakeup(struct wm_softc *);
static void	wm_igp3_phy_powerdown_workaround_ich8lan(struct wm_softc *);
static void	wm_enable_wakeup(struct wm_softc *);
#endif
/* EEE */
static void	wm_set_eee_i350(struct wm_softc *);

/*
 * Workarounds (mainly PHY related).
 * Basically, PHY's workarounds are in the PHY drivers.
 */
static void	wm_kmrn_lock_loss_workaround_ich8lan(struct wm_softc *);
static void	wm_gig_downshift_workaround_ich8lan(struct wm_softc *);
static void	wm_hv_phy_workaround_ich8lan(struct wm_softc *);
static void	wm_lv_phy_workaround_ich8lan(struct wm_softc *);
static void	wm_k1_gig_workaround_hv(struct wm_softc *, int);
static void	wm_set_mdio_slow_mode_hv(struct wm_softc *);
static void	wm_configure_k1_ich8lan(struct wm_softc *, int);
static void	wm_reset_init_script_82575(struct wm_softc *);
static void	wm_reset_mdicnfg_82580(struct wm_softc *);
static void	wm_pll_workaround_i210(struct wm_softc *);

#ifdef WM_MSI_MSIX
struct _msix_matrix {
	const char *intrname;
	int(*func)(void *);
	int intridx;
	int cpuid;
} msix_matrix[WM_MSIX_NINTR] = {
	{ "TX", wm_txintr_msix, WM_MSIX_TXINTR_IDX, WM_MSIX_TXINTR_CPUID },
	{ "RX", wm_rxintr_msix, WM_MSIX_RXINTR_IDX, WM_MSIX_RXINTR_CPUID },
	{ "LINK", wm_linkintr_msix, WM_MSIX_LINKINTR_IDX,
	  WM_MSIX_LINKINTR_CPUID },
};
#endif

CFATTACH_DECL3_NEW(wm, sizeof(struct wm_softc),
    wm_match, wm_attach, wm_detach, NULL, NULL, NULL, DVF_DETACH_SHUTDOWN);

/*
 * Devices supported by this driver.
 */
static const struct wm_product {
	pci_vendor_id_t		wmp_vendor;
	pci_product_id_t	wmp_product;
	const char		*wmp_name;
	wm_chip_type		wmp_type;
	uint32_t		wmp_flags;
#define	WMP_F_UNKNOWN		WM_MEDIATYPE_UNKNOWN
#define	WMP_F_FIBER		WM_MEDIATYPE_FIBER
#define	WMP_F_COPPER		WM_MEDIATYPE_COPPER
#define	WMP_F_SERDES		WM_MEDIATYPE_SERDES
#define WMP_MEDIATYPE(x)	((x) & 0x03)
} wm_products[] = {
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82542,
	  "Intel i82542 1000BASE-X Ethernet",
	  WM_T_82542_2_1,	WMP_F_FIBER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82543GC_FIBER,
	  "Intel i82543GC 1000BASE-X Ethernet",
	  WM_T_82543,		WMP_F_FIBER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82543GC_COPPER,
	  "Intel i82543GC 1000BASE-T Ethernet",
	  WM_T_82543,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82544EI_COPPER,
	  "Intel i82544EI 1000BASE-T Ethernet",
	  WM_T_82544,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82544EI_FIBER,
	  "Intel i82544EI 1000BASE-X Ethernet",
	  WM_T_82544,		WMP_F_FIBER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82544GC_COPPER,
	  "Intel i82544GC 1000BASE-T Ethernet",
	  WM_T_82544,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82544GC_LOM,
	  "Intel i82544GC (LOM) 1000BASE-T Ethernet",
	  WM_T_82544,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82540EM,
	  "Intel i82540EM 1000BASE-T Ethernet",
	  WM_T_82540,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82540EM_LOM,
	  "Intel i82540EM (LOM) 1000BASE-T Ethernet",
	  WM_T_82540,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82540EP_LOM,
	  "Intel i82540EP 1000BASE-T Ethernet",
	  WM_T_82540,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82540EP,
	  "Intel i82540EP 1000BASE-T Ethernet",
	  WM_T_82540,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82540EP_LP,
	  "Intel i82540EP 1000BASE-T Ethernet",
	  WM_T_82540,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82545EM_COPPER,
	  "Intel i82545EM 1000BASE-T Ethernet",
	  WM_T_82545,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82545GM_COPPER,
	  "Intel i82545GM 1000BASE-T Ethernet",
	  WM_T_82545_3,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82545GM_FIBER,
	  "Intel i82545GM 1000BASE-X Ethernet",
	  WM_T_82545_3,		WMP_F_FIBER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82545GM_SERDES,
	  "Intel i82545GM Gigabit Ethernet (SERDES)",
	  WM_T_82545_3,		WMP_F_SERDES },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82546EB_COPPER,
	  "Intel i82546EB 1000BASE-T Ethernet",
	  WM_T_82546,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82546EB_QUAD,
	  "Intel i82546EB 1000BASE-T Ethernet",
	  WM_T_82546,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82545EM_FIBER,
	  "Intel i82545EM 1000BASE-X Ethernet",
	  WM_T_82545,		WMP_F_FIBER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82546EB_FIBER,
	  "Intel i82546EB 1000BASE-X Ethernet",
	  WM_T_82546,		WMP_F_FIBER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82546GB_COPPER,
	  "Intel i82546GB 1000BASE-T Ethernet",
	  WM_T_82546_3,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82546GB_FIBER,
	  "Intel i82546GB 1000BASE-X Ethernet",
	  WM_T_82546_3,		WMP_F_FIBER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82546GB_SERDES,
	  "Intel i82546GB Gigabit Ethernet (SERDES)",
	  WM_T_82546_3,		WMP_F_SERDES },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82546GB_QUAD_COPPER,
	  "i82546GB quad-port Gigabit Ethernet",
	  WM_T_82546_3,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82546GB_QUAD_COPPER_KSP3,
	  "i82546GB quad-port Gigabit Ethernet (KSP3)",
	  WM_T_82546_3,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82546GB_PCIE,
	  "Intel PRO/1000MT (82546GB)",
	  WM_T_82546_3,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82541EI,
	  "Intel i82541EI 1000BASE-T Ethernet",
	  WM_T_82541,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82541ER_LOM,
	  "Intel i82541ER (LOM) 1000BASE-T Ethernet",
	  WM_T_82541,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82541EI_MOBILE,
	  "Intel i82541EI Mobile 1000BASE-T Ethernet",
	  WM_T_82541,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82541ER,
	  "Intel i82541ER 1000BASE-T Ethernet",
	  WM_T_82541_2,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82541GI,
	  "Intel i82541GI 1000BASE-T Ethernet",
	  WM_T_82541_2,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82541GI_MOBILE,
	  "Intel i82541GI Mobile 1000BASE-T Ethernet",
	  WM_T_82541_2,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82541PI,
	  "Intel i82541PI 1000BASE-T Ethernet",
	  WM_T_82541_2,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82547EI,
	  "Intel i82547EI 1000BASE-T Ethernet",
	  WM_T_82547,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82547EI_MOBILE,
	  "Intel i82547EI Mobile 1000BASE-T Ethernet",
	  WM_T_82547,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82547GI,
	  "Intel i82547GI 1000BASE-T Ethernet",
	  WM_T_82547_2,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82571EB_COPPER,
	  "Intel PRO/1000 PT (82571EB)",
	  WM_T_82571,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82571EB_FIBER,
	  "Intel PRO/1000 PF (82571EB)",
	  WM_T_82571,		WMP_F_FIBER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82571EB_SERDES,
	  "Intel PRO/1000 PB (82571EB)",
	  WM_T_82571,		WMP_F_SERDES },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82571EB_QUAD_COPPER,
	  "Intel PRO/1000 QT (82571EB)",
	  WM_T_82571,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82571GB_QUAD_COPPER,
	  "Intel PRO/1000 PT Quad Port Server Adapter",
	  WM_T_82571,		WMP_F_COPPER, },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82571PT_QUAD_COPPER,
	  "Intel Gigabit PT Quad Port Server ExpressModule",
	  WM_T_82571,		WMP_F_COPPER, },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82571EB_DUAL_SERDES,
	  "Intel 82571EB Dual Gigabit Ethernet (SERDES)",
	  WM_T_82571,		WMP_F_SERDES, },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82571EB_QUAD_SERDES,
	  "Intel 82571EB Quad Gigabit Ethernet (SERDES)",
	  WM_T_82571,		WMP_F_SERDES, },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82571EB_QUAD_FIBER,
	  "Intel 82571EB Quad 1000baseX Ethernet",
	  WM_T_82571,		WMP_F_FIBER, },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82572EI_COPPER,
	  "Intel i82572EI 1000baseT Ethernet",
	  WM_T_82572,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82572EI_FIBER,
	  "Intel i82572EI 1000baseX Ethernet",
	  WM_T_82572,		WMP_F_FIBER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82572EI_SERDES,
	  "Intel i82572EI Gigabit Ethernet (SERDES)",
	  WM_T_82572,		WMP_F_SERDES },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82572EI,
	  "Intel i82572EI 1000baseT Ethernet",
	  WM_T_82572,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82573E,
	  "Intel i82573E",
	  WM_T_82573,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82573E_IAMT,
	  "Intel i82573E IAMT",
	  WM_T_82573,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82573L,
	  "Intel i82573L Gigabit Ethernet",
	  WM_T_82573,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82574L,
	  "Intel i82574L",
	  WM_T_82574,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82574LA,
	  "Intel i82574L",
	  WM_T_82574,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82583V,
	  "Intel i82583V",
	  WM_T_82583,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_80K3LAN_CPR_DPT,
	  "i80003 dual 1000baseT Ethernet",
	  WM_T_80003,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_80K3LAN_FIB_DPT,
	  "i80003 dual 1000baseX Ethernet",
	  WM_T_80003,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_80K3LAN_SDS_DPT,
	  "Intel i80003ES2 dual Gigabit Ethernet (SERDES)",
	  WM_T_80003,		WMP_F_SERDES },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_80K3LAN_CPR_SPT,
	  "Intel i80003 1000baseT Ethernet",
	  WM_T_80003,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_80K3LAN_SDS_SPT,
	  "Intel i80003 Gigabit Ethernet (SERDES)",
	  WM_T_80003,		WMP_F_SERDES },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801H_M_AMT,
	  "Intel i82801H (M_AMT) LAN Controller",
	  WM_T_ICH8,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801H_AMT,
	  "Intel i82801H (AMT) LAN Controller",
	  WM_T_ICH8,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801H_LAN,
	  "Intel i82801H LAN Controller",
	  WM_T_ICH8,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801H_IFE_LAN,
	  "Intel i82801H (IFE) LAN Controller",
	  WM_T_ICH8,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801H_M_LAN,
	  "Intel i82801H (M) LAN Controller",
	  WM_T_ICH8,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801H_IFE_GT,
	  "Intel i82801H IFE (GT) LAN Controller",
	  WM_T_ICH8,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801H_IFE_G,
	  "Intel i82801H IFE (G) LAN Controller",
	  WM_T_ICH8,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801I_IGP_AMT,
	  "82801I (AMT) LAN Controller",
	  WM_T_ICH9,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801I_IFE,
	  "82801I LAN Controller",
	  WM_T_ICH9,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801I_IFE_G,
	  "82801I (G) LAN Controller",
	  WM_T_ICH9,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801I_IFE_GT,
	  "82801I (GT) LAN Controller",
	  WM_T_ICH9,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801I_IGP_C,
	  "82801I (C) LAN Controller",
	  WM_T_ICH9,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801I_IGP_M,
	  "82801I mobile LAN Controller",
	  WM_T_ICH9,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801H_IGP_M_V,
	  "82801I mobile (V) LAN Controller",
	  WM_T_ICH9,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801I_IGP_M_AMT,
	  "82801I mobile (AMT) LAN Controller",
	  WM_T_ICH9,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801I_BM,
	  "82567LM-4 LAN Controller",
	  WM_T_ICH9,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801I_82567V_3,
	  "82567V-3 LAN Controller",
	  WM_T_ICH9,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801J_R_BM_LM,
	  "82567LM-2 LAN Controller",
	  WM_T_ICH10,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801J_R_BM_LF,
	  "82567LF-2 LAN Controller",
	  WM_T_ICH10,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801J_D_BM_LM,
	  "82567LM-3 LAN Controller",
	  WM_T_ICH10,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801J_D_BM_LF,
	  "82567LF-3 LAN Controller",
	  WM_T_ICH10,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801J_R_BM_V,
	  "82567V-2 LAN Controller",
	  WM_T_ICH10,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801J_D_BM_V,
	  "82567V-3? LAN Controller",
	  WM_T_ICH10,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_HANKSVILLE,
	  "HANKSVILLE LAN Controller",
	  WM_T_ICH10,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_PCH_M_LM,
	  "PCH LAN (82577LM) Controller",
	  WM_T_PCH,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_PCH_M_LC,
	  "PCH LAN (82577LC) Controller",
	  WM_T_PCH,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_PCH_D_DM,
	  "PCH LAN (82578DM) Controller",
	  WM_T_PCH,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_PCH_D_DC,
	  "PCH LAN (82578DC) Controller",
	  WM_T_PCH,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_PCH2_LV_LM,
	  "PCH2 LAN (82579LM) Controller",
	  WM_T_PCH2,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_PCH2_LV_V,
	  "PCH2 LAN (82579V) Controller",
	  WM_T_PCH2,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82575EB_COPPER,
	  "82575EB dual-1000baseT Ethernet",
	  WM_T_82575,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82575EB_FIBER_SERDES,
	  "82575EB dual-1000baseX Ethernet (SERDES)",
	  WM_T_82575,		WMP_F_SERDES },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82575GB_QUAD_COPPER,
	  "82575GB quad-1000baseT Ethernet",
	  WM_T_82575,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82575GB_QUAD_COPPER_PM,
	  "82575GB quad-1000baseT Ethernet (PM)",
	  WM_T_82575,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82576_COPPER,
	  "82576 1000BaseT Ethernet",
	  WM_T_82576,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82576_FIBER,
	  "82576 1000BaseX Ethernet",
	  WM_T_82576,		WMP_F_FIBER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82576_SERDES,
	  "82576 gigabit Ethernet (SERDES)",
	  WM_T_82576,		WMP_F_SERDES },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82576_QUAD_COPPER,
	  "82576 quad-1000BaseT Ethernet",
	  WM_T_82576,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82576_QUAD_COPPER_ET2,
	  "82576 Gigabit ET2 Quad Port Server Adapter",
	  WM_T_82576,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82576_NS,
	  "82576 gigabit Ethernet",
	  WM_T_82576,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82576_NS_SERDES,
	  "82576 gigabit Ethernet (SERDES)",
	  WM_T_82576,		WMP_F_SERDES },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82576_SERDES_QUAD,
	  "82576 quad-gigabit Ethernet (SERDES)",
	  WM_T_82576,		WMP_F_SERDES },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82580_COPPER,
	  "82580 1000BaseT Ethernet",
	  WM_T_82580,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82580_FIBER,
	  "82580 1000BaseX Ethernet",
	  WM_T_82580,		WMP_F_FIBER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82580_SERDES,
	  "82580 1000BaseT Ethernet (SERDES)",
	  WM_T_82580,		WMP_F_SERDES },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82580_SGMII,
	  "82580 gigabit Ethernet (SGMII)",
	  WM_T_82580,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82580_COPPER_DUAL,
	  "82580 dual-1000BaseT Ethernet",
	  WM_T_82580,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82580_QUAD_FIBER,
	  "82580 quad-1000BaseX Ethernet",
	  WM_T_82580,		WMP_F_FIBER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_DH89XXCC_SGMII,
	  "DH89XXCC Gigabit Ethernet (SGMII)",
	  WM_T_82580,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_DH89XXCC_SERDES,
	  "DH89XXCC Gigabit Ethernet (SERDES)",
	  WM_T_82580,		WMP_F_SERDES },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_DH89XXCC_BPLANE,
	  "DH89XXCC 1000BASE-KX Ethernet",
	  WM_T_82580,		WMP_F_SERDES },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_DH89XXCC_SFP,
	  "DH89XXCC Gigabit Ethernet (SFP)",
	  WM_T_82580,		WMP_F_SERDES },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_I350_COPPER,
	  "I350 Gigabit Network Connection",
	  WM_T_I350,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_I350_FIBER,
	  "I350 Gigabit Fiber Network Connection",
	  WM_T_I350,		WMP_F_FIBER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_I350_SERDES,
	  "I350 Gigabit Backplane Connection",
	  WM_T_I350,		WMP_F_SERDES },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_I350_DA4,
	  "I350 Quad Port Gigabit Ethernet",
	  WM_T_I350,		WMP_F_SERDES },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_I350_SGMII,
	  "I350 Gigabit Connection",
	  WM_T_I350,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_C2000_1000KX,
	  "I354 Gigabit Ethernet (KX)",
	  WM_T_I354,		WMP_F_SERDES },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_C2000_SGMII,
	  "I354 Gigabit Ethernet (SGMII)",
	  WM_T_I354,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_C2000_25GBE,
	  "I354 Gigabit Ethernet (2.5G)",
	  WM_T_I354,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_I210_T1,
	  "I210-T1 Ethernet Server Adapter",
	  WM_T_I210,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_I210_COPPER_OEM1,
	  "I210 Ethernet (Copper OEM)",
	  WM_T_I210,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_I210_COPPER_IT,
	  "I210 Ethernet (Copper IT)",
	  WM_T_I210,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_I210_COPPER_WOF,
	  "I210 Ethernet (FLASH less)",
	  WM_T_I210,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_I210_FIBER,
	  "I210 Gigabit Ethernet (Fiber)",
	  WM_T_I210,		WMP_F_FIBER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_I210_SERDES,
	  "I210 Gigabit Ethernet (SERDES)",
	  WM_T_I210,		WMP_F_SERDES },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_I210_SERDES_WOF,
	  "I210 Gigabit Ethernet (FLASH less)",
	  WM_T_I210,		WMP_F_SERDES },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_I210_SGMII,
	  "I210 Gigabit Ethernet (SGMII)",
	  WM_T_I210,		WMP_F_COPPER },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_I211_COPPER,
	  "I211 Ethernet (COPPER)",
	  WM_T_I211,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_I217_V,
	  "I217 V Ethernet Connection",
	  WM_T_PCH_LPT,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_I217_LM,
	  "I217 LM Ethernet Connection",
	  WM_T_PCH_LPT,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_I218_V,
	  "I218 V Ethernet Connection",
	  WM_T_PCH_LPT,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_I218_V2,
	  "I218 V Ethernet Connection",
	  WM_T_PCH_LPT,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_I218_V3,
	  "I218 V Ethernet Connection",
	  WM_T_PCH_LPT,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_I218_LM,
	  "I218 LM Ethernet Connection",
	  WM_T_PCH_LPT,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_I218_LM2,
	  "I218 LM Ethernet Connection",
	  WM_T_PCH_LPT,		WMP_F_COPPER },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_I218_LM3,
	  "I218 LM Ethernet Connection",
	  WM_T_PCH_LPT,		WMP_F_COPPER },
	{ 0,			0,
	  NULL,
	  0,			0 },
};

#ifdef WM_EVENT_COUNTERS
static char wm_txseg_evcnt_names[WM_NTXSEGS][sizeof("txsegXXX")];
#endif /* WM_EVENT_COUNTERS */


/*
 * Register read/write functions.
 * Other than CSR_{READ|WRITE}().
 */

#if 0 /* Not currently used */
static inline uint32_t
wm_io_read(struct wm_softc *sc, int reg)
{

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, 0, reg);
	return (bus_space_read_4(sc->sc_iot, sc->sc_ioh, 4));
}
#endif

static inline void
wm_io_write(struct wm_softc *sc, int reg, uint32_t val)
{

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, 0, reg);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, 4, val);
}

static inline void
wm_82575_write_8bit_ctlr_reg(struct wm_softc *sc, uint32_t reg, uint32_t off,
    uint32_t data)
{
	uint32_t regval;
	int i;

	regval = (data & SCTL_CTL_DATA_MASK) | (off << SCTL_CTL_ADDR_SHIFT);

	CSR_WRITE(sc, reg, regval);

	for (i = 0; i < SCTL_CTL_POLL_TIMEOUT; i++) {
		delay(5);
		if (CSR_READ(sc, reg) & SCTL_CTL_READY)
			break;
	}
	if (i == SCTL_CTL_POLL_TIMEOUT) {
		aprint_error("%s: WARNING:"
		    " i82575 reg 0x%08x setup did not indicate ready\n",
		    device_xname(sc->sc_dev), reg);
	}
}

static inline void
wm_set_dma_addr(volatile wiseman_addr_t *wa, bus_addr_t v)
{
	wa->wa_low = htole32(v & 0xffffffffU);
	if (sizeof(bus_addr_t) == 8)
		wa->wa_high = htole32((uint64_t) v >> 32);
	else
		wa->wa_high = 0;
}

/*
 * Device driver interface functions and commonly used functions.
 * match, attach, detach, init, start, stop, ioctl, watchdog and so on.
 */

/* Lookup supported device table */
static const struct wm_product *
wm_lookup(const struct pci_attach_args *pa)
{
	const struct wm_product *wmp;

	for (wmp = wm_products; wmp->wmp_name != NULL; wmp++) {
		if (PCI_VENDOR(pa->pa_id) == wmp->wmp_vendor &&
		    PCI_PRODUCT(pa->pa_id) == wmp->wmp_product)
			return wmp;
	}
	return NULL;
}

/* The match function (ca_match) */
static int
wm_match(device_t parent, cfdata_t cf, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (wm_lookup(pa) != NULL)
		return 1;

	return 0;
}

/* The attach function (ca_attach) */
static void
wm_attach(device_t parent, device_t self, void *aux)
{
	struct wm_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	prop_dictionary_t dict;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	pci_chipset_tag_t pc = pa->pa_pc;
#ifndef WM_MSI_MSIX
	pci_intr_handle_t ih;
#else
	int counts[PCI_INTR_TYPE_SIZE];
	pci_intr_type_t max_type;
#endif
	const char *intrstr = NULL;
	const char *eetype, *xname;
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	bus_size_t memsize;
	int memh_valid;
	int i, error;
	const struct wm_product *wmp;
	prop_data_t ea;
	prop_number_t pn;
	uint8_t enaddr[ETHER_ADDR_LEN];
	uint16_t cfg1, cfg2, swdpin, nvmword;
	pcireg_t preg, memtype;
	uint16_t eeprom_data, apme_mask;
	bool force_clear_smbi;
	uint32_t link_mode;
	uint32_t reg;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;
	callout_init(&sc->sc_tick_ch, CALLOUT_FLAGS);
	sc->sc_stopping = false;

	wmp = wm_lookup(pa);
#ifdef DIAGNOSTIC
	if (wmp == NULL) {
		printf("\n");
		panic("wm_attach: impossible");
	}
#endif
	sc->sc_mediatype = WMP_MEDIATYPE(wmp->wmp_flags);

	sc->sc_pc = pa->pa_pc;
	sc->sc_pcitag = pa->pa_tag;

	if (pci_dma64_available(pa))
		sc->sc_dmat = pa->pa_dmat64;
	else
		sc->sc_dmat = pa->pa_dmat;

	sc->sc_pcidevid = PCI_PRODUCT(pa->pa_id);
	sc->sc_rev = PCI_REVISION(pci_conf_read(pc, pa->pa_tag, PCI_CLASS_REG));
	pci_aprint_devinfo_fancy(pa, "Ethernet controller", wmp->wmp_name, 1);

	sc->sc_type = wmp->wmp_type;
	if (sc->sc_type < WM_T_82543) {
		if (sc->sc_rev < 2) {
			aprint_error_dev(sc->sc_dev,
			    "i82542 must be at least rev. 2\n");
			return;
		}
		if (sc->sc_rev < 3)
			sc->sc_type = WM_T_82542_2_0;
	}

	/*
	 * Disable MSI for Errata:
	 * "Message Signaled Interrupt Feature May Corrupt Write Transactions"
	 * 
	 *  82544: Errata 25
	 *  82540: Errata  6 (easy to reproduce device timeout)
	 *  82545: Errata  4 (easy to reproduce device timeout)
	 *  82546: Errata 26 (easy to reproduce device timeout)
	 *  82541: Errata  7 (easy to reproduce device timeout)
	 *
	 * "Byte Enables 2 and 3 are not set on MSI writes"
	 *
	 *  82571 & 82572: Errata 63
	 */
	if ((sc->sc_type <= WM_T_82541_2) || (sc->sc_type == WM_T_82571)
	    || (sc->sc_type == WM_T_82572))
		pa->pa_flags &= ~PCI_FLAGS_MSI_OKAY;

	if ((sc->sc_type == WM_T_82575) || (sc->sc_type == WM_T_82576)
	    || (sc->sc_type == WM_T_82580)
	    || (sc->sc_type == WM_T_I350) || (sc->sc_type == WM_T_I354)
	    || (sc->sc_type == WM_T_I210) || (sc->sc_type == WM_T_I211))
		sc->sc_flags |= WM_F_NEWQUEUE;

	/* Set device properties (mactype) */
	dict = device_properties(sc->sc_dev);
	prop_dictionary_set_uint32(dict, "mactype", sc->sc_type);

	/*
	 * Map the device.  All devices support memory-mapped acccess,
	 * and it is really required for normal operation.
	 */
	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, WM_PCI_MMBA);
	switch (memtype) {
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
		memh_valid = (pci_mapreg_map(pa, WM_PCI_MMBA,
		    memtype, 0, &memt, &memh, NULL, &memsize) == 0);
		break;
	default:
		memh_valid = 0;
		break;
	}

	if (memh_valid) {
		sc->sc_st = memt;
		sc->sc_sh = memh;
		sc->sc_ss = memsize;
	} else {
		aprint_error_dev(sc->sc_dev,
		    "unable to map device registers\n");
		return;
	}

	/*
	 * In addition, i82544 and later support I/O mapped indirect
	 * register access.  It is not desirable (nor supported in
	 * this driver) to use it for normal operation, though it is
	 * required to work around bugs in some chip versions.
	 */
	if (sc->sc_type >= WM_T_82544) {
		/* First we have to find the I/O BAR. */
		for (i = PCI_MAPREG_START; i < PCI_MAPREG_END; i += 4) {
			memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, i);
			if (memtype == PCI_MAPREG_TYPE_IO)
				break;
			if (PCI_MAPREG_MEM_TYPE(memtype) ==
			    PCI_MAPREG_MEM_TYPE_64BIT)
				i += 4;	/* skip high bits, too */
		}
		if (i < PCI_MAPREG_END) {
			/*
			 * We found PCI_MAPREG_TYPE_IO. Note that 82580
			 * (and newer?) chip has no PCI_MAPREG_TYPE_IO.
			 * It's no problem because newer chips has no this
			 * bug.
			 *
			 * The i8254x doesn't apparently respond when the
			 * I/O BAR is 0, which looks somewhat like it's not
			 * been configured.
			 */
			preg = pci_conf_read(pc, pa->pa_tag, i);
			if (PCI_MAPREG_MEM_ADDR(preg) == 0) {
				aprint_error_dev(sc->sc_dev,
				    "WARNING: I/O BAR at zero.\n");
			} else if (pci_mapreg_map(pa, i, PCI_MAPREG_TYPE_IO,
					0, &sc->sc_iot, &sc->sc_ioh,
					NULL, &sc->sc_ios) == 0) {
				sc->sc_flags |= WM_F_IOH_VALID;
			} else {
				aprint_error_dev(sc->sc_dev,
				    "WARNING: unable to map I/O space\n");
			}
		}

	}

	/* Enable bus mastering.  Disable MWI on the i82542 2.0. */
	preg = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	preg |= PCI_COMMAND_MASTER_ENABLE;
	if (sc->sc_type < WM_T_82542_2_1)
		preg &= ~PCI_COMMAND_INVALIDATE_ENABLE;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, preg);

	/* power up chip */
	if ((error = pci_activate(pa->pa_pc, pa->pa_tag, self,
	    NULL)) && error != EOPNOTSUPP) {
		aprint_error_dev(sc->sc_dev, "cannot activate %d\n", error);
		return;
	}

#ifndef WM_MSI_MSIX
	/*
	 * Map and establish our interrupt.
	 */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(sc->sc_dev, "unable to map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
#ifdef WM_MPSAFE
	pci_intr_setattr(pc, &ih, PCI_INTR_MPSAFE, true);
#endif
	sc->sc_ihs[0] = pci_intr_establish_xname(pc, ih, IPL_NET,
	    wm_intr_legacy, sc, device_xname(sc->sc_dev));
	if (sc->sc_ihs[0] == NULL) {
		aprint_error_dev(sc->sc_dev, "unable to establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(sc->sc_dev, "interrupting at %s\n", intrstr);
	sc->sc_nintrs = 1;
#else /* WM_MSI_MSIX */
	/* Allocation settings */
	max_type = PCI_INTR_TYPE_MSIX;
	counts[PCI_INTR_TYPE_MSIX] = WM_MAX_NINTR;
	counts[PCI_INTR_TYPE_MSI] = 1;
	counts[PCI_INTR_TYPE_INTX] = 1;

alloc_retry:
	if (pci_intr_alloc(pa, &sc->sc_intrs, counts, max_type) != 0) {
		aprint_error_dev(sc->sc_dev, "failed to allocate interrupt\n");
		return;
	}

	if (pci_intr_type(sc->sc_intrs[0]) == PCI_INTR_TYPE_MSIX) {
		void *vih;
		kcpuset_t *affinity;
		char intr_xname[INTRDEVNAMEBUF];

		kcpuset_create(&affinity, false);

		for (i = 0; i < WM_MSIX_NINTR; i++) {
			intrstr = pci_intr_string(pc,
			    sc->sc_intrs[msix_matrix[i].intridx], intrbuf,
			    sizeof(intrbuf));
#ifdef WM_MPSAFE
			pci_intr_setattr(pc,
			    &sc->sc_intrs[msix_matrix[i].intridx],
			    PCI_INTR_MPSAFE, true);
#endif
			memset(intr_xname, 0, sizeof(intr_xname));
			strlcat(intr_xname, device_xname(sc->sc_dev),
			    sizeof(intr_xname));
			strlcat(intr_xname, msix_matrix[i].intrname,
			    sizeof(intr_xname));
			vih = pci_intr_establish_xname(pc,
			    sc->sc_intrs[msix_matrix[i].intridx], IPL_NET,
			    msix_matrix[i].func, sc, intr_xname);
			if (vih == NULL) {
				aprint_error_dev(sc->sc_dev,
				    "unable to establish MSI-X(for %s)%s%s\n",
				    msix_matrix[i].intrname,
				    intrstr ? " at " : "",
				    intrstr ? intrstr : "");
				pci_intr_release(sc->sc_pc, sc->sc_intrs,
				    WM_MSIX_NINTR);
				kcpuset_destroy(affinity);

				/* Setup for MSI: Disable MSI-X */
				max_type = PCI_INTR_TYPE_MSI;
				counts[PCI_INTR_TYPE_MSI] = 1;
				counts[PCI_INTR_TYPE_INTX] = 1;
				goto alloc_retry;
			}
			kcpuset_zero(affinity);
			/* Round-robin affinity */
			kcpuset_set(affinity, msix_matrix[i].cpuid % ncpu);
			error = interrupt_distribute(vih, affinity, NULL);
			if (error == 0) {
				aprint_normal_dev(sc->sc_dev,
				    "for %s interrupting at %s affinity to %u\n",
				    msix_matrix[i].intrname, intrstr,
				    msix_matrix[i].cpuid % ncpu);
			} else {
				aprint_normal_dev(sc->sc_dev,
				    "for %s interrupting at %s\n",
				    msix_matrix[i].intrname, intrstr);
			}
			sc->sc_ihs[msix_matrix[i].intridx] = vih;
		}

		sc->sc_nintrs = WM_MSIX_NINTR;
		kcpuset_destroy(affinity);
	} else {
		/* MSI or INTx */
		intrstr = pci_intr_string(pc, sc->sc_intrs[0], intrbuf,
		    sizeof(intrbuf));
#ifdef WM_MPSAFE
		pci_intr_setattr(pc, &sc->sc_intrs[0], PCI_INTR_MPSAFE, true);
#endif
		sc->sc_ihs[0] = pci_intr_establish_xname(pc, sc->sc_intrs[0],
		    IPL_NET, wm_intr_legacy, sc, device_xname(sc->sc_dev));
		if (sc->sc_ihs[0] == NULL) {
			aprint_error_dev(sc->sc_dev,"unable to establish %s\n",
			    (pci_intr_type(sc->sc_intrs[0])
				== PCI_INTR_TYPE_MSI) ? "MSI" : "INTx");
			pci_intr_release(sc->sc_pc, sc->sc_intrs, 1);
			switch (pci_intr_type(sc->sc_intrs[0])) {
			case PCI_INTR_TYPE_MSI:
				/* The next try is for INTx: Disable MSI */
				max_type = PCI_INTR_TYPE_INTX;
				counts[PCI_INTR_TYPE_INTX] = 1;
				goto alloc_retry;
			case PCI_INTR_TYPE_INTX:
			default:
				return;
			}
		}
		aprint_normal_dev(sc->sc_dev, "%s at %s\n",
		    (pci_intr_type(sc->sc_intrs[0]) == PCI_INTR_TYPE_MSI)
			? "MSI" : "interrupting", intrstr);

		sc->sc_nintrs = 1;
	}
#endif /* WM_MSI_MSIX */

	/*
	 * Check the function ID (unit number of the chip).
	 */
	if ((sc->sc_type == WM_T_82546) || (sc->sc_type == WM_T_82546_3)
	    || (sc->sc_type ==  WM_T_82571) || (sc->sc_type == WM_T_80003)
	    || (sc->sc_type == WM_T_82575) || (sc->sc_type == WM_T_82576)
	    || (sc->sc_type == WM_T_82580)
	    || (sc->sc_type == WM_T_I350) || (sc->sc_type == WM_T_I354))
		sc->sc_funcid = (CSR_READ(sc, WMREG_STATUS)
		    >> STATUS_FUNCID_SHIFT) & STATUS_FUNCID_MASK;
	else
		sc->sc_funcid = 0;

	/*
	 * Determine a few things about the bus we're connected to.
	 */
	if (sc->sc_type < WM_T_82543) {
		/* We don't really know the bus characteristics here. */
		sc->sc_bus_speed = 33;
	} else if (sc->sc_type == WM_T_82547 || sc->sc_type == WM_T_82547_2) {
		/*
		 * CSA (Communication Streaming Architecture) is about as fast
		 * a 32-bit 66MHz PCI Bus.
		 */
		sc->sc_flags |= WM_F_CSA;
		sc->sc_bus_speed = 66;
		aprint_verbose_dev(sc->sc_dev,
		    "Communication Streaming Architecture\n");
		if (sc->sc_type == WM_T_82547) {
			callout_init(&sc->sc_txfifo_ch, CALLOUT_FLAGS);
			callout_setfunc(&sc->sc_txfifo_ch,
					wm_82547_txfifo_stall, sc);
			aprint_verbose_dev(sc->sc_dev,
			    "using 82547 Tx FIFO stall work-around\n");
		}
	} else if (sc->sc_type >= WM_T_82571) {
		sc->sc_flags |= WM_F_PCIE;
		if ((sc->sc_type != WM_T_ICH8) && (sc->sc_type != WM_T_ICH9)
		    && (sc->sc_type != WM_T_ICH10)
		    && (sc->sc_type != WM_T_PCH)
		    && (sc->sc_type != WM_T_PCH2)
		    && (sc->sc_type != WM_T_PCH_LPT)) {
			/* ICH* and PCH* have no PCIe capability registers */
			if (pci_get_capability(pa->pa_pc, pa->pa_tag,
				PCI_CAP_PCIEXPRESS, &sc->sc_pcixe_capoff,
				NULL) == 0)
				aprint_error_dev(sc->sc_dev,
				    "unable to find PCIe capability\n");
		}
		aprint_verbose_dev(sc->sc_dev, "PCI-Express bus\n");
	} else {
		reg = CSR_READ(sc, WMREG_STATUS);
		if (reg & STATUS_BUS64)
			sc->sc_flags |= WM_F_BUS64;
		if ((reg & STATUS_PCIX_MODE) != 0) {
			pcireg_t pcix_cmd, pcix_sts, bytecnt, maxb;

			sc->sc_flags |= WM_F_PCIX;
			if (pci_get_capability(pa->pa_pc, pa->pa_tag,
				PCI_CAP_PCIX, &sc->sc_pcixe_capoff, NULL) == 0)
				aprint_error_dev(sc->sc_dev,
				    "unable to find PCIX capability\n");
			else if (sc->sc_type != WM_T_82545_3 &&
				 sc->sc_type != WM_T_82546_3) {
				/*
				 * Work around a problem caused by the BIOS
				 * setting the max memory read byte count
				 * incorrectly.
				 */
				pcix_cmd = pci_conf_read(pa->pa_pc, pa->pa_tag,
				    sc->sc_pcixe_capoff + PCIX_CMD);
				pcix_sts = pci_conf_read(pa->pa_pc, pa->pa_tag,
				    sc->sc_pcixe_capoff + PCIX_STATUS);

				bytecnt =
				    (pcix_cmd & PCIX_CMD_BYTECNT_MASK) >>
				    PCIX_CMD_BYTECNT_SHIFT;
				maxb =
				    (pcix_sts & PCIX_STATUS_MAXB_MASK) >>
				    PCIX_STATUS_MAXB_SHIFT;
				if (bytecnt > maxb) {
					aprint_verbose_dev(sc->sc_dev,
					    "resetting PCI-X MMRBC: %d -> %d\n",
					    512 << bytecnt, 512 << maxb);
					pcix_cmd = (pcix_cmd &
					    ~PCIX_CMD_BYTECNT_MASK) |
					   (maxb << PCIX_CMD_BYTECNT_SHIFT);
					pci_conf_write(pa->pa_pc, pa->pa_tag,
					    sc->sc_pcixe_capoff + PCIX_CMD,
					    pcix_cmd);
				}
			}
		}
		/*
		 * The quad port adapter is special; it has a PCIX-PCIX
		 * bridge on the board, and can run the secondary bus at
		 * a higher speed.
		 */
		if (wmp->wmp_product == PCI_PRODUCT_INTEL_82546EB_QUAD) {
			sc->sc_bus_speed = (sc->sc_flags & WM_F_PCIX) ? 120
								      : 66;
		} else if (sc->sc_flags & WM_F_PCIX) {
			switch (reg & STATUS_PCIXSPD_MASK) {
			case STATUS_PCIXSPD_50_66:
				sc->sc_bus_speed = 66;
				break;
			case STATUS_PCIXSPD_66_100:
				sc->sc_bus_speed = 100;
				break;
			case STATUS_PCIXSPD_100_133:
				sc->sc_bus_speed = 133;
				break;
			default:
				aprint_error_dev(sc->sc_dev,
				    "unknown PCIXSPD %d; assuming 66MHz\n",
				    reg & STATUS_PCIXSPD_MASK);
				sc->sc_bus_speed = 66;
				break;
			}
		} else
			sc->sc_bus_speed = (reg & STATUS_PCI66) ? 66 : 33;
		aprint_verbose_dev(sc->sc_dev, "%d-bit %dMHz %s bus\n",
		    (sc->sc_flags & WM_F_BUS64) ? 64 : 32, sc->sc_bus_speed,
		    (sc->sc_flags & WM_F_PCIX) ? "PCIX" : "PCI");
	}

	/*
	 * Allocate the control data structures, and create and load the
	 * DMA map for it.
	 *
	 * NOTE: All Tx descriptors must be in the same 4G segment of
	 * memory.  So must Rx descriptors.  We simplify by allocating
	 * both sets within the same 4G segment.
	 */
	WM_NTXDESC(sc) = sc->sc_type < WM_T_82544 ?
	    WM_NTXDESC_82542 : WM_NTXDESC_82544;
	sc->sc_cd_size = sc->sc_type < WM_T_82544 ?
	    sizeof(struct wm_control_data_82542) :
	    sizeof(struct wm_control_data_82544);
	if ((error = bus_dmamem_alloc(sc->sc_dmat, sc->sc_cd_size, PAGE_SIZE,
		    (bus_size_t) 0x100000000ULL, &sc->sc_cd_seg, 1,
		    &sc->sc_cd_rseg, 0)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to allocate control data, error = %d\n",
		    error);
		goto fail_0;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, &sc->sc_cd_seg,
		    sc->sc_cd_rseg, sc->sc_cd_size,
		    (void **)&sc->sc_control_data, BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to map control data, error = %d\n", error);
		goto fail_1;
	}

	if ((error = bus_dmamap_create(sc->sc_dmat, sc->sc_cd_size, 1,
		    sc->sc_cd_size, 0, 0, &sc->sc_cddmamap)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to create control data DMA map, error = %d\n",
		    error);
		goto fail_2;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_cddmamap,
		    sc->sc_control_data, sc->sc_cd_size, NULL, 0)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to load control data DMA map, error = %d\n",
		    error);
		goto fail_3;
	}

	/* Create the transmit buffer DMA maps. */
	WM_TXQUEUELEN(sc) =
	    (sc->sc_type == WM_T_82547 || sc->sc_type == WM_T_82547_2) ?
	    WM_TXQUEUELEN_MAX_82547 : WM_TXQUEUELEN_MAX;
	for (i = 0; i < WM_TXQUEUELEN(sc); i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, WM_MAXTXDMA,
			    WM_NTXSEGS, WTX_MAX_LEN, 0, 0,
			    &sc->sc_txsoft[i].txs_dmamap)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "unable to create Tx DMA map %d, error = %d\n",
			    i, error);
			goto fail_4;
		}
	}

	/* Create the receive buffer DMA maps. */
	for (i = 0; i < WM_NRXDESC; i++) {
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1,
			    MCLBYTES, 0, 0,
			    &sc->sc_rxsoft[i].rxs_dmamap)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "unable to create Rx DMA map %d error = %d\n",
			    i, error);
			goto fail_5;
		}
		sc->sc_rxsoft[i].rxs_mbuf = NULL;
	}

	/* clear interesting stat counters */
	CSR_READ(sc, WMREG_COLC);
	CSR_READ(sc, WMREG_RXERRC);

	/* get PHY control from SMBus to PCIe */
	if ((sc->sc_type == WM_T_PCH) || (sc->sc_type == WM_T_PCH2)
	    || (sc->sc_type == WM_T_PCH_LPT))
		wm_smbustopci(sc);

	/* Reset the chip to a known state. */
	wm_reset(sc);

	/* Get some information about the EEPROM. */
	switch (sc->sc_type) {
	case WM_T_82542_2_0:
	case WM_T_82542_2_1:
	case WM_T_82543:
	case WM_T_82544:
		/* Microwire */
		sc->sc_nvm_wordsize = 64;
		sc->sc_nvm_addrbits = 6;
		break;
	case WM_T_82540:
	case WM_T_82545:
	case WM_T_82545_3:
	case WM_T_82546:
	case WM_T_82546_3:
		/* Microwire */
		reg = CSR_READ(sc, WMREG_EECD);
		if (reg & EECD_EE_SIZE) {
			sc->sc_nvm_wordsize = 256;
			sc->sc_nvm_addrbits = 8;
		} else {
			sc->sc_nvm_wordsize = 64;
			sc->sc_nvm_addrbits = 6;
		}
		sc->sc_flags |= WM_F_LOCK_EECD;
		break;
	case WM_T_82541:
	case WM_T_82541_2:
	case WM_T_82547:
	case WM_T_82547_2:
		sc->sc_flags |= WM_F_LOCK_EECD;
		reg = CSR_READ(sc, WMREG_EECD);
		if (reg & EECD_EE_TYPE) {
			/* SPI */
			sc->sc_flags |= WM_F_EEPROM_SPI;
			wm_nvm_set_addrbits_size_eecd(sc);
		} else {
			/* Microwire */
			if ((reg & EECD_EE_ABITS) != 0) {
				sc->sc_nvm_wordsize = 256;
				sc->sc_nvm_addrbits = 8;
			} else {
				sc->sc_nvm_wordsize = 64;
				sc->sc_nvm_addrbits = 6;
			}
		}
		break;
	case WM_T_82571:
	case WM_T_82572:
		/* SPI */
		sc->sc_flags |= WM_F_EEPROM_SPI;
		wm_nvm_set_addrbits_size_eecd(sc);
		sc->sc_flags |= WM_F_LOCK_EECD | WM_F_LOCK_SWSM;
		break;
	case WM_T_82573:
		sc->sc_flags |= WM_F_LOCK_SWSM;
		/* FALLTHROUGH */
	case WM_T_82574:
	case WM_T_82583:
		if (wm_nvm_is_onboard_eeprom(sc) == 0) {
			sc->sc_flags |= WM_F_EEPROM_FLASH;
			sc->sc_nvm_wordsize = 2048;
		} else {
			/* SPI */
			sc->sc_flags |= WM_F_EEPROM_SPI;
			wm_nvm_set_addrbits_size_eecd(sc);
		}
		sc->sc_flags |= WM_F_EEPROM_EERDEEWR;
		break;
	case WM_T_82575:
	case WM_T_82576:
	case WM_T_82580:
	case WM_T_I350:
	case WM_T_I354:
	case WM_T_80003:
		/* SPI */
		sc->sc_flags |= WM_F_EEPROM_SPI;
		wm_nvm_set_addrbits_size_eecd(sc);
		sc->sc_flags |= WM_F_EEPROM_EERDEEWR | WM_F_LOCK_SWFW
		    | WM_F_LOCK_SWSM;
		break;
	case WM_T_ICH8:
	case WM_T_ICH9:
	case WM_T_ICH10:
	case WM_T_PCH:
	case WM_T_PCH2:
	case WM_T_PCH_LPT:
		/* FLASH */
		sc->sc_flags |= WM_F_EEPROM_FLASH | WM_F_LOCK_EXTCNF;
		sc->sc_nvm_wordsize = 2048;
		memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, WM_ICH8_FLASH);
		if (pci_mapreg_map(pa, WM_ICH8_FLASH, memtype, 0,
		    &sc->sc_flasht, &sc->sc_flashh, NULL, &sc->sc_flashs)) {
			aprint_error_dev(sc->sc_dev,
			    "can't map FLASH registers\n");
			goto fail_5;
		}
		reg = ICH8_FLASH_READ32(sc, ICH_FLASH_GFPREG);
		sc->sc_ich8_flash_base = (reg & ICH_GFPREG_BASE_MASK) *
						ICH_FLASH_SECTOR_SIZE;
		sc->sc_ich8_flash_bank_size =
		    ((reg >> 16) & ICH_GFPREG_BASE_MASK) + 1;
		sc->sc_ich8_flash_bank_size -=
		    (reg & ICH_GFPREG_BASE_MASK);
		sc->sc_ich8_flash_bank_size *= ICH_FLASH_SECTOR_SIZE;
		sc->sc_ich8_flash_bank_size /= 2 * sizeof(uint16_t);
		break;
	case WM_T_I210:
	case WM_T_I211:
		if (wm_nvm_get_flash_presence_i210(sc)) {
			wm_nvm_set_addrbits_size_eecd(sc);
			sc->sc_flags |= WM_F_EEPROM_FLASH_HW;
			sc->sc_flags |= WM_F_EEPROM_EERDEEWR | WM_F_LOCK_SWFW;
		} else {
			sc->sc_nvm_wordsize = INVM_SIZE;
			sc->sc_flags |= WM_F_EEPROM_INVM;
			sc->sc_flags |= WM_F_LOCK_SWFW;
		}
		break;
	default:
		break;
	}

	/* Ensure the SMBI bit is clear before first NVM or PHY access */
	switch (sc->sc_type) {
	case WM_T_82571:
	case WM_T_82572:
		reg = CSR_READ(sc, WMREG_SWSM2);
		if ((reg & SWSM2_LOCK) == 0) {
			CSR_WRITE(sc, WMREG_SWSM2, reg | SWSM2_LOCK);
			force_clear_smbi = true;
		} else
			force_clear_smbi = false;
		break;
	case WM_T_82573:
	case WM_T_82574:
	case WM_T_82583:
		force_clear_smbi = true;
		break;
	default:
		force_clear_smbi = false;
		break;
	}
	if (force_clear_smbi) {
		reg = CSR_READ(sc, WMREG_SWSM);
		if ((reg & SWSM_SMBI) != 0)
			aprint_error_dev(sc->sc_dev,
			    "Please update the Bootagent\n");
		CSR_WRITE(sc, WMREG_SWSM, reg & ~SWSM_SMBI);
	}

	/*
	 * Defer printing the EEPROM type until after verifying the checksum
	 * This allows the EEPROM type to be printed correctly in the case
	 * that no EEPROM is attached.
	 */
	/*
	 * Validate the EEPROM checksum. If the checksum fails, flag
	 * this for later, so we can fail future reads from the EEPROM.
	 */
	if (wm_nvm_validate_checksum(sc)) {
		/*
		 * Read twice again because some PCI-e parts fail the
		 * first check due to the link being in sleep state.
		 */
		if (wm_nvm_validate_checksum(sc))
			sc->sc_flags |= WM_F_EEPROM_INVALID;
	}

	/* Set device properties (macflags) */
	prop_dictionary_set_uint32(dict, "macflags", sc->sc_flags);

	if (sc->sc_flags & WM_F_EEPROM_INVALID)
		aprint_verbose_dev(sc->sc_dev, "No EEPROM");
	else {
		aprint_verbose_dev(sc->sc_dev, "%u words ",
		    sc->sc_nvm_wordsize);
		if (sc->sc_flags & WM_F_EEPROM_INVM)
			aprint_verbose("iNVM");
		else if (sc->sc_flags & WM_F_EEPROM_FLASH_HW)
			aprint_verbose("FLASH(HW)");
		else if (sc->sc_flags & WM_F_EEPROM_FLASH)
			aprint_verbose("FLASH");
		else {
			if (sc->sc_flags & WM_F_EEPROM_SPI)
				eetype = "SPI";
			else
				eetype = "MicroWire";
			aprint_verbose("(%d address bits) %s EEPROM",
			    sc->sc_nvm_addrbits, eetype);
		}
	}
	wm_nvm_version(sc);
	aprint_verbose("\n");

	/* Check for I21[01] PLL workaround */
	if (sc->sc_type == WM_T_I210)
		sc->sc_flags |= WM_F_PLL_WA_I210;
	if ((sc->sc_type == WM_T_I210) && wm_nvm_get_flash_presence_i210(sc)) {
		/* NVM image release 3.25 has a workaround */
		if ((sc->sc_nvm_ver_major < 3)
		    || ((sc->sc_nvm_ver_major == 3)
			&& (sc->sc_nvm_ver_minor < 25))) {
			aprint_verbose_dev(sc->sc_dev,
			    "ROM image version %d.%d is older than 3.25\n",
			    sc->sc_nvm_ver_major, sc->sc_nvm_ver_minor);
			sc->sc_flags |= WM_F_PLL_WA_I210;
		}
	}
	if ((sc->sc_flags & WM_F_PLL_WA_I210) != 0)
		wm_pll_workaround_i210(sc);

	switch (sc->sc_type) {
	case WM_T_82571:
	case WM_T_82572:
	case WM_T_82573:
	case WM_T_82574:
	case WM_T_82583:
	case WM_T_80003:
	case WM_T_ICH8:
	case WM_T_ICH9:
	case WM_T_ICH10:
	case WM_T_PCH:
	case WM_T_PCH2:
	case WM_T_PCH_LPT:
		if (wm_check_mng_mode(sc) != 0)
			wm_get_hw_control(sc);
		break;
	default:
		break;
	}
	wm_get_wakeup(sc);
	/*
	 * Read the Ethernet address from the EEPROM, if not first found
	 * in device properties.
	 */
	ea = prop_dictionary_get(dict, "mac-address");
	if (ea != NULL) {
		KASSERT(prop_object_type(ea) == PROP_TYPE_DATA);
		KASSERT(prop_data_size(ea) == ETHER_ADDR_LEN);
		memcpy(enaddr, prop_data_data_nocopy(ea), ETHER_ADDR_LEN);
	} else {
		if (wm_read_mac_addr(sc, enaddr) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "unable to read Ethernet address\n");
			goto fail_5;
		}
	}

	aprint_normal_dev(sc->sc_dev, "Ethernet address %s\n",
	    ether_sprintf(enaddr));

	/*
	 * Read the config info from the EEPROM, and set up various
	 * bits in the control registers based on their contents.
	 */
	pn = prop_dictionary_get(dict, "i82543-cfg1");
	if (pn != NULL) {
		KASSERT(prop_object_type(pn) == PROP_TYPE_NUMBER);
		cfg1 = (uint16_t) prop_number_integer_value(pn);
	} else {
		if (wm_nvm_read(sc, NVM_OFF_CFG1, 1, &cfg1)) {
			aprint_error_dev(sc->sc_dev, "unable to read CFG1\n");
			goto fail_5;
		}
	}

	pn = prop_dictionary_get(dict, "i82543-cfg2");
	if (pn != NULL) {
		KASSERT(prop_object_type(pn) == PROP_TYPE_NUMBER);
		cfg2 = (uint16_t) prop_number_integer_value(pn);
	} else {
		if (wm_nvm_read(sc, NVM_OFF_CFG2, 1, &cfg2)) {
			aprint_error_dev(sc->sc_dev, "unable to read CFG2\n");
			goto fail_5;
		}
	}

	/* check for WM_F_WOL */
	switch (sc->sc_type) {
	case WM_T_82542_2_0:
	case WM_T_82542_2_1:
	case WM_T_82543:
		/* dummy? */
		eeprom_data = 0;
		apme_mask = NVM_CFG3_APME;
		break;
	case WM_T_82544:
		apme_mask = NVM_CFG2_82544_APM_EN;
		eeprom_data = cfg2;
		break;
	case WM_T_82546:
	case WM_T_82546_3:
	case WM_T_82571:
	case WM_T_82572:
	case WM_T_82573:
	case WM_T_82574:
	case WM_T_82583:
	case WM_T_80003:
	default:
		apme_mask = NVM_CFG3_APME;
		wm_nvm_read(sc, (sc->sc_funcid == 1) ? NVM_OFF_CFG3_PORTB
		    : NVM_OFF_CFG3_PORTA, 1, &eeprom_data);
		break;
	case WM_T_82575:
	case WM_T_82576:
	case WM_T_82580:
	case WM_T_I350:
	case WM_T_I354: /* XXX ok? */
	case WM_T_ICH8:
	case WM_T_ICH9:
	case WM_T_ICH10:
	case WM_T_PCH:
	case WM_T_PCH2:
	case WM_T_PCH_LPT:
		/* XXX The funcid should be checked on some devices */
		apme_mask = WUC_APME;
		eeprom_data = CSR_READ(sc, WMREG_WUC);
		break;
	}

	/* Check for WM_F_WOL flag after the setting of the EEPROM stuff */
	if ((eeprom_data & apme_mask) != 0)
		sc->sc_flags |= WM_F_WOL;
#ifdef WM_DEBUG
	if ((sc->sc_flags & WM_F_WOL) != 0)
		printf("WOL\n");
#endif

	if ((sc->sc_type == WM_T_82575) || (sc->sc_type == WM_T_82576)) {
		/* Check NVM for autonegotiation */
		if (wm_nvm_read(sc, NVM_OFF_COMPAT, 1, &nvmword) == 0) {
			if ((nvmword & NVM_COMPAT_SERDES_FORCE_MODE) != 0)
				sc->sc_flags |= WM_F_PCS_DIS_AUTONEGO;
		}
	}

	/*
	 * XXX need special handling for some multiple port cards
	 * to disable a paticular port.
	 */

	if (sc->sc_type >= WM_T_82544) {
		pn = prop_dictionary_get(dict, "i82543-swdpin");
		if (pn != NULL) {
			KASSERT(prop_object_type(pn) == PROP_TYPE_NUMBER);
			swdpin = (uint16_t) prop_number_integer_value(pn);
		} else {
			if (wm_nvm_read(sc, NVM_OFF_SWDPIN, 1, &swdpin)) {
				aprint_error_dev(sc->sc_dev,
				    "unable to read SWDPIN\n");
				goto fail_5;
			}
		}
	}

	if (cfg1 & NVM_CFG1_ILOS)
		sc->sc_ctrl |= CTRL_ILOS;

	/*
	 * XXX
	 * This code isn't correct because pin 2 and 3 are located
	 * in different position on newer chips. Check all datasheet.
	 *
	 * Until resolve this problem, check if a chip < 82580
	 */
	if (sc->sc_type <= WM_T_82580) {
		if (sc->sc_type >= WM_T_82544) {
			sc->sc_ctrl |=
			    ((swdpin >> NVM_SWDPIN_SWDPIO_SHIFT) & 0xf) <<
			    CTRL_SWDPIO_SHIFT;
			sc->sc_ctrl |=
			    ((swdpin >> NVM_SWDPIN_SWDPIN_SHIFT) & 0xf) <<
			    CTRL_SWDPINS_SHIFT;
		} else {
			sc->sc_ctrl |=
			    ((cfg1 >> NVM_CFG1_SWDPIO_SHIFT) & 0xf) <<
			    CTRL_SWDPIO_SHIFT;
		}
	}

	/* XXX For other than 82580? */
	if (sc->sc_type == WM_T_82580) {
		wm_nvm_read(sc, NVM_OFF_CFG3_PORTA, 1, &nvmword);
		printf("CFG3 = %08x\n", (uint32_t)nvmword);
		if (nvmword & __BIT(13)) {
			printf("SET ILOS\n");
			sc->sc_ctrl |= CTRL_ILOS;
		}
	}

#if 0
	if (sc->sc_type >= WM_T_82544) {
		if (cfg1 & NVM_CFG1_IPS0)
			sc->sc_ctrl_ext |= CTRL_EXT_IPS;
		if (cfg1 & NVM_CFG1_IPS1)
			sc->sc_ctrl_ext |= CTRL_EXT_IPS1;
		sc->sc_ctrl_ext |=
		    ((swdpin >> (NVM_SWDPIN_SWDPIO_SHIFT + 4)) & 0xd) <<
		    CTRL_EXT_SWDPIO_SHIFT;
		sc->sc_ctrl_ext |=
		    ((swdpin >> (NVM_SWDPIN_SWDPIN_SHIFT + 4)) & 0xd) <<
		    CTRL_EXT_SWDPINS_SHIFT;
	} else {
		sc->sc_ctrl_ext |=
		    ((cfg2 >> NVM_CFG2_SWDPIO_SHIFT) & 0xf) <<
		    CTRL_EXT_SWDPIO_SHIFT;
	}
#endif

	CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl);
#if 0
	CSR_WRITE(sc, WMREG_CTRL_EXT, sc->sc_ctrl_ext);
#endif

	/*
	 * Set up some register offsets that are different between
	 * the i82542 and the i82543 and later chips.
	 */
	if (sc->sc_type < WM_T_82543) {
		sc->sc_rdt_reg = WMREG_OLD_RDT0;
		sc->sc_tdt_reg = WMREG_OLD_TDT;
	} else {
		sc->sc_rdt_reg = WMREG_RDT;
		sc->sc_tdt_reg = WMREG_TDT;
	}

	if (sc->sc_type == WM_T_PCH) {
		uint16_t val;

		/* Save the NVM K1 bit setting */
		wm_nvm_read(sc, NVM_OFF_K1_CONFIG, 1, &val);

		if ((val & NVM_K1_CONFIG_ENABLE) != 0)
			sc->sc_nvm_k1_enabled = 1;
		else
			sc->sc_nvm_k1_enabled = 0;
	}

	/*
	 * Determine if we're TBI,GMII or SGMII mode, and initialize the
	 * media structures accordingly.
	 */
	if (sc->sc_type == WM_T_ICH8 || sc->sc_type == WM_T_ICH9
	    || sc->sc_type == WM_T_ICH10 || sc->sc_type == WM_T_PCH
	    || sc->sc_type == WM_T_PCH2 || sc->sc_type == WM_T_PCH_LPT
	    || sc->sc_type == WM_T_82573
	    || sc->sc_type == WM_T_82574 || sc->sc_type == WM_T_82583) {
		/* STATUS_TBIMODE reserved/reused, can't rely on it */
		wm_gmii_mediainit(sc, wmp->wmp_product);
	} else if (sc->sc_type < WM_T_82543 ||
	    (CSR_READ(sc, WMREG_STATUS) & STATUS_TBIMODE) != 0) {
		if (sc->sc_mediatype == WM_MEDIATYPE_COPPER) {
			aprint_error_dev(sc->sc_dev,
			    "WARNING: TBIMODE set on 1000BASE-T product!\n");
			sc->sc_mediatype = WM_MEDIATYPE_FIBER;
		}
		wm_tbi_mediainit(sc);
	} else {
		switch (sc->sc_type) {
		case WM_T_82575:
		case WM_T_82576:
		case WM_T_82580:
		case WM_T_I350:
		case WM_T_I354:
		case WM_T_I210:
		case WM_T_I211:
			reg = CSR_READ(sc, WMREG_CTRL_EXT);
			link_mode = reg & CTRL_EXT_LINK_MODE_MASK;
			switch (link_mode) {
			case CTRL_EXT_LINK_MODE_1000KX:
				aprint_verbose_dev(sc->sc_dev, "1000KX\n");
				sc->sc_mediatype = WM_MEDIATYPE_SERDES;
				break;
			case CTRL_EXT_LINK_MODE_SGMII:
				if (wm_sgmii_uses_mdio(sc)) {
					aprint_verbose_dev(sc->sc_dev,
					    "SGMII(MDIO)\n");
					sc->sc_flags |= WM_F_SGMII;
					sc->sc_mediatype = WM_MEDIATYPE_COPPER;
					break;
				}
				aprint_verbose_dev(sc->sc_dev, "SGMII(I2C)\n");
				/*FALLTHROUGH*/
			case CTRL_EXT_LINK_MODE_PCIE_SERDES:
				sc->sc_mediatype = wm_sfp_get_media_type(sc);
				if (sc->sc_mediatype == WM_MEDIATYPE_UNKNOWN) {
					if (link_mode
					    == CTRL_EXT_LINK_MODE_SGMII) {
						sc->sc_mediatype
						    = WM_MEDIATYPE_COPPER;
						sc->sc_flags |= WM_F_SGMII;
					} else {
						sc->sc_mediatype
						    = WM_MEDIATYPE_SERDES;
						aprint_verbose_dev(sc->sc_dev,
						    "SERDES\n");
					}
					break;
				}
				if (sc->sc_mediatype == WM_MEDIATYPE_SERDES)
					aprint_verbose_dev(sc->sc_dev,
					    "SERDES\n");

				/* Change current link mode setting */
				reg &= ~CTRL_EXT_LINK_MODE_MASK;
				switch (sc->sc_mediatype) {
				case WM_MEDIATYPE_COPPER:
					reg |= CTRL_EXT_LINK_MODE_SGMII;
					break;
				case WM_MEDIATYPE_SERDES:
					reg |= CTRL_EXT_LINK_MODE_PCIE_SERDES;
					break;
				default:
					break;
				}
				CSR_WRITE(sc, WMREG_CTRL_EXT, reg);
				break;
			case CTRL_EXT_LINK_MODE_GMII:
			default:
				aprint_verbose_dev(sc->sc_dev, "Copper\n");
				sc->sc_mediatype = WM_MEDIATYPE_COPPER;
				break;
			}

			reg &= ~CTRL_EXT_I2C_ENA;
			if ((sc->sc_flags & WM_F_SGMII) != 0)
				reg |= CTRL_EXT_I2C_ENA;
			else
				reg &= ~CTRL_EXT_I2C_ENA;
			CSR_WRITE(sc, WMREG_CTRL_EXT, reg);

			if (sc->sc_mediatype == WM_MEDIATYPE_COPPER)
				wm_gmii_mediainit(sc, wmp->wmp_product);
			else
				wm_tbi_mediainit(sc);
			break;
		default:
			if (sc->sc_mediatype == WM_MEDIATYPE_FIBER)
				aprint_error_dev(sc->sc_dev,
				    "WARNING: TBIMODE clear on 1000BASE-X product!\n");
			sc->sc_mediatype = WM_MEDIATYPE_COPPER;
			wm_gmii_mediainit(sc, wmp->wmp_product);
		}
	}

	ifp = &sc->sc_ethercom.ec_if;
	xname = device_xname(sc->sc_dev);
	strlcpy(ifp->if_xname, xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = wm_ioctl;
	if ((sc->sc_flags & WM_F_NEWQUEUE) != 0)
		ifp->if_start = wm_nq_start;
	else
		ifp->if_start = wm_start;
	ifp->if_watchdog = wm_watchdog;
	ifp->if_init = wm_init;
	ifp->if_stop = wm_stop;
	IFQ_SET_MAXLEN(&ifp->if_snd, max(WM_IFQUEUELEN, IFQ_MAXLEN));
	IFQ_SET_READY(&ifp->if_snd);

	/* Check for jumbo frame */
	switch (sc->sc_type) {
	case WM_T_82573:
		/* XXX limited to 9234 if ASPM is disabled */
		wm_nvm_read(sc, NVM_OFF_INIT_3GIO_3, 1, &nvmword);
		if ((nvmword & NVM_3GIO_3_ASPM_MASK) != 0)
			sc->sc_ethercom.ec_capabilities |= ETHERCAP_JUMBO_MTU;
		break;
	case WM_T_82571:
	case WM_T_82572:
	case WM_T_82574:
	case WM_T_82575:
	case WM_T_82576:
	case WM_T_82580:
	case WM_T_I350:
	case WM_T_I354: /* XXXX ok? */
	case WM_T_I210:
	case WM_T_I211:
	case WM_T_80003:
	case WM_T_ICH9:
	case WM_T_ICH10:
	case WM_T_PCH2:	/* PCH2 supports 9K frame size */
	case WM_T_PCH_LPT:
		/* XXX limited to 9234 */
		sc->sc_ethercom.ec_capabilities |= ETHERCAP_JUMBO_MTU;
		break;
	case WM_T_PCH:
		/* XXX limited to 4096 */
		sc->sc_ethercom.ec_capabilities |= ETHERCAP_JUMBO_MTU;
		break;
	case WM_T_82542_2_0:
	case WM_T_82542_2_1:
	case WM_T_82583:
	case WM_T_ICH8:
		/* No support for jumbo frame */
		break;
	default:
		/* ETHER_MAX_LEN_JUMBO */
		sc->sc_ethercom.ec_capabilities |= ETHERCAP_JUMBO_MTU;
		break;
	}

	/* If we're a i82543 or greater, we can support VLANs. */
	if (sc->sc_type >= WM_T_82543)
		sc->sc_ethercom.ec_capabilities |=
		    ETHERCAP_VLAN_MTU | ETHERCAP_VLAN_HWTAGGING;

	/*
	 * We can perform TCPv4 and UDPv4 checkums in-bound.  Only
	 * on i82543 and later.
	 */
	if (sc->sc_type >= WM_T_82543) {
		ifp->if_capabilities |=
		    IFCAP_CSUM_IPv4_Tx | IFCAP_CSUM_IPv4_Rx |
		    IFCAP_CSUM_TCPv4_Tx | IFCAP_CSUM_TCPv4_Rx |
		    IFCAP_CSUM_UDPv4_Tx | IFCAP_CSUM_UDPv4_Rx |
		    IFCAP_CSUM_TCPv6_Tx |
		    IFCAP_CSUM_UDPv6_Tx;
	}

	/*
	 * XXXyamt: i'm not sure which chips support RXCSUM_IPV6OFL.
	 *
	 *	82541GI (8086:1076) ... no
	 *	82572EI (8086:10b9) ... yes
	 */
	if (sc->sc_type >= WM_T_82571) {
		ifp->if_capabilities |=
		    IFCAP_CSUM_TCPv6_Rx | IFCAP_CSUM_UDPv6_Rx;
	}

	/*
	 * If we're a i82544 or greater (except i82547), we can do
	 * TCP segmentation offload.
	 */
	if (sc->sc_type >= WM_T_82544 && sc->sc_type != WM_T_82547) {
		ifp->if_capabilities |= IFCAP_TSOv4;
	}

	if (sc->sc_type >= WM_T_82571) {
		ifp->if_capabilities |= IFCAP_TSOv6;
	}

#ifdef WM_MPSAFE
	sc->sc_tx_lock = mutex_obj_alloc(MUTEX_DEFAULT, IPL_NET);
	sc->sc_rx_lock = mutex_obj_alloc(MUTEX_DEFAULT, IPL_NET);
#else
	sc->sc_tx_lock = NULL;
	sc->sc_rx_lock = NULL;
#endif

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp, enaddr);
	ether_set_ifflags_cb(&sc->sc_ethercom, wm_ifflags_cb);
	rnd_attach_source(&sc->rnd_source, xname, RND_TYPE_NET,
			  RND_FLAG_DEFAULT);

#ifdef WM_EVENT_COUNTERS
	/* Attach event counters. */
	evcnt_attach_dynamic(&sc->sc_ev_txsstall, EVCNT_TYPE_MISC,
	    NULL, xname, "txsstall");
	evcnt_attach_dynamic(&sc->sc_ev_txdstall, EVCNT_TYPE_MISC,
	    NULL, xname, "txdstall");
	evcnt_attach_dynamic(&sc->sc_ev_txfifo_stall, EVCNT_TYPE_MISC,
	    NULL, xname, "txfifo_stall");
	evcnt_attach_dynamic(&sc->sc_ev_txdw, EVCNT_TYPE_INTR,
	    NULL, xname, "txdw");
	evcnt_attach_dynamic(&sc->sc_ev_txqe, EVCNT_TYPE_INTR,
	    NULL, xname, "txqe");
	evcnt_attach_dynamic(&sc->sc_ev_rxintr, EVCNT_TYPE_INTR,
	    NULL, xname, "rxintr");
	evcnt_attach_dynamic(&sc->sc_ev_linkintr, EVCNT_TYPE_INTR,
	    NULL, xname, "linkintr");

	evcnt_attach_dynamic(&sc->sc_ev_rxipsum, EVCNT_TYPE_MISC,
	    NULL, xname, "rxipsum");
	evcnt_attach_dynamic(&sc->sc_ev_rxtusum, EVCNT_TYPE_MISC,
	    NULL, xname, "rxtusum");
	evcnt_attach_dynamic(&sc->sc_ev_txipsum, EVCNT_TYPE_MISC,
	    NULL, xname, "txipsum");
	evcnt_attach_dynamic(&sc->sc_ev_txtusum, EVCNT_TYPE_MISC,
	    NULL, xname, "txtusum");
	evcnt_attach_dynamic(&sc->sc_ev_txtusum6, EVCNT_TYPE_MISC,
	    NULL, xname, "txtusum6");

	evcnt_attach_dynamic(&sc->sc_ev_txtso, EVCNT_TYPE_MISC,
	    NULL, xname, "txtso");
	evcnt_attach_dynamic(&sc->sc_ev_txtso6, EVCNT_TYPE_MISC,
	    NULL, xname, "txtso6");
	evcnt_attach_dynamic(&sc->sc_ev_txtsopain, EVCNT_TYPE_MISC,
	    NULL, xname, "txtsopain");

	for (i = 0; i < WM_NTXSEGS; i++) {
		snprintf(wm_txseg_evcnt_names[i],
		    sizeof(wm_txseg_evcnt_names[i]), "txseg%d", i);
		evcnt_attach_dynamic(&sc->sc_ev_txseg[i], EVCNT_TYPE_MISC,
		    NULL, xname, wm_txseg_evcnt_names[i]);
	}

	evcnt_attach_dynamic(&sc->sc_ev_txdrop, EVCNT_TYPE_MISC,
	    NULL, xname, "txdrop");

	evcnt_attach_dynamic(&sc->sc_ev_tu, EVCNT_TYPE_MISC,
	    NULL, xname, "tu");

	evcnt_attach_dynamic(&sc->sc_ev_tx_xoff, EVCNT_TYPE_MISC,
	    NULL, xname, "tx_xoff");
	evcnt_attach_dynamic(&sc->sc_ev_tx_xon, EVCNT_TYPE_MISC,
	    NULL, xname, "tx_xon");
	evcnt_attach_dynamic(&sc->sc_ev_rx_xoff, EVCNT_TYPE_MISC,
	    NULL, xname, "rx_xoff");
	evcnt_attach_dynamic(&sc->sc_ev_rx_xon, EVCNT_TYPE_MISC,
	    NULL, xname, "rx_xon");
	evcnt_attach_dynamic(&sc->sc_ev_rx_macctl, EVCNT_TYPE_MISC,
	    NULL, xname, "rx_macctl");
#endif /* WM_EVENT_COUNTERS */

	if (pmf_device_register(self, wm_suspend, wm_resume))
		pmf_class_network_register(self, ifp);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

	sc->sc_flags |= WM_F_ATTACHED;
	return;

	/*
	 * Free any resources we've allocated during the failed attach
	 * attempt.  Do this in reverse order and fall through.
	 */
 fail_5:
	for (i = 0; i < WM_NRXDESC; i++) {
		if (sc->sc_rxsoft[i].rxs_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->sc_rxsoft[i].rxs_dmamap);
	}
 fail_4:
	for (i = 0; i < WM_TXQUEUELEN(sc); i++) {
		if (sc->sc_txsoft[i].txs_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->sc_txsoft[i].txs_dmamap);
	}
	bus_dmamap_unload(sc->sc_dmat, sc->sc_cddmamap);
 fail_3:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_cddmamap);
 fail_2:
	bus_dmamem_unmap(sc->sc_dmat, (void *)sc->sc_control_data,
	    sc->sc_cd_size);
 fail_1:
	bus_dmamem_free(sc->sc_dmat, &sc->sc_cd_seg, sc->sc_cd_rseg);
 fail_0:
	return;
}

/* The detach function (ca_detach) */
static int
wm_detach(device_t self, int flags __unused)
{
	struct wm_softc *sc = device_private(self);
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	int i;
#ifndef WM_MPSAFE
	int s;
#endif

	if ((sc->sc_flags & WM_F_ATTACHED) == 0)
		return 0;

#ifndef WM_MPSAFE
	s = splnet();
#endif
	/* Stop the interface. Callouts are stopped in it. */
	wm_stop(ifp, 1);

#ifndef WM_MPSAFE
	splx(s);
#endif

	pmf_device_deregister(self);

	/* Tell the firmware about the release */
	WM_BOTH_LOCK(sc);
	wm_release_manageability(sc);
	wm_release_hw_control(sc);
	WM_BOTH_UNLOCK(sc);

	mii_detach(&sc->sc_mii, MII_PHY_ANY, MII_OFFSET_ANY);

	/* Delete all remaining media. */
	ifmedia_delete_instance(&sc->sc_mii.mii_media, IFM_INST_ANY);

	ether_ifdetach(ifp);
	if_detach(ifp);


	/* Unload RX dmamaps and free mbufs */
	WM_RX_LOCK(sc);
	wm_rxdrain(sc);
	WM_RX_UNLOCK(sc);
	/* Must unlock here */

	/* Free dmamap. It's the same as the end of the wm_attach() function */
	for (i = 0; i < WM_NRXDESC; i++) {
		if (sc->sc_rxsoft[i].rxs_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->sc_rxsoft[i].rxs_dmamap);
	}
	for (i = 0; i < WM_TXQUEUELEN(sc); i++) {
		if (sc->sc_txsoft[i].txs_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->sc_txsoft[i].txs_dmamap);
	}
	bus_dmamap_unload(sc->sc_dmat, sc->sc_cddmamap);
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_cddmamap);
	bus_dmamem_unmap(sc->sc_dmat, (void *)sc->sc_control_data,
	    sc->sc_cd_size);
	bus_dmamem_free(sc->sc_dmat, &sc->sc_cd_seg, sc->sc_cd_rseg);

	/* Disestablish the interrupt handler */
	for (i = 0; i < sc->sc_nintrs; i++) {
		if (sc->sc_ihs[i] != NULL) {
			pci_intr_disestablish(sc->sc_pc, sc->sc_ihs[i]);
			sc->sc_ihs[i] = NULL;
		}
	}
#ifdef WM_MSI_MSIX
	pci_intr_release(sc->sc_pc, sc->sc_intrs, sc->sc_nintrs);
#endif /* WM_MSI_MSIX */

	/* Unmap the registers */
	if (sc->sc_ss) {
		bus_space_unmap(sc->sc_st, sc->sc_sh, sc->sc_ss);
		sc->sc_ss = 0;
	}
	if (sc->sc_ios) {
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);
		sc->sc_ios = 0;
	}
	if (sc->sc_flashs) {
		bus_space_unmap(sc->sc_flasht, sc->sc_flashh, sc->sc_flashs);
		sc->sc_flashs = 0;
	}

	if (sc->sc_tx_lock)
		mutex_obj_free(sc->sc_tx_lock);
	if (sc->sc_rx_lock)
		mutex_obj_free(sc->sc_rx_lock);

	return 0;
}

static bool
wm_suspend(device_t self, const pmf_qual_t *qual)
{
	struct wm_softc *sc = device_private(self);

	wm_release_manageability(sc);
	wm_release_hw_control(sc);
#ifdef WM_WOL
	wm_enable_wakeup(sc);
#endif

	return true;
}

static bool
wm_resume(device_t self, const pmf_qual_t *qual)
{
	struct wm_softc *sc = device_private(self);

	wm_init_manageability(sc);

	return true;
}

/*
 * wm_watchdog:		[ifnet interface function]
 *
 *	Watchdog timer handler.
 */
static void
wm_watchdog(struct ifnet *ifp)
{
	struct wm_softc *sc = ifp->if_softc;

	/*
	 * Since we're using delayed interrupts, sweep up
	 * before we report an error.
	 */
	WM_TX_LOCK(sc);
	wm_txeof(sc);
	WM_TX_UNLOCK(sc);

	if (sc->sc_txfree != WM_NTXDESC(sc)) {
#ifdef WM_DEBUG
		int i, j;
		struct wm_txsoft *txs;
#endif
		log(LOG_ERR,
		    "%s: device timeout (txfree %d txsfree %d txnext %d)\n",
		    device_xname(sc->sc_dev), sc->sc_txfree, sc->sc_txsfree,
		    sc->sc_txnext);
		ifp->if_oerrors++;
#ifdef WM_DEBUG
		for (i = sc->sc_txsdirty; i != sc->sc_txsnext ;
		    i = WM_NEXTTXS(sc, i)) {
		    txs = &sc->sc_txsoft[i];
		    printf("txs %d tx %d -> %d\n",
			i, txs->txs_firstdesc, txs->txs_lastdesc);
		    for (j = txs->txs_firstdesc; ;
			j = WM_NEXTTX(sc, j)) {
			printf("\tdesc %d: 0x%" PRIx64 "\n", j,
			    sc->sc_nq_txdescs[j].nqtx_data.nqtxd_addr);
			printf("\t %#08x%08x\n",
			    sc->sc_nq_txdescs[j].nqtx_data.nqtxd_fields,
			    sc->sc_nq_txdescs[j].nqtx_data.nqtxd_cmdlen);
			if (j == txs->txs_lastdesc)
				break;
			}
		}
#endif
		/* Reset the interface. */
		(void) wm_init(ifp);
	}

	/* Try to get more packets going. */
	ifp->if_start(ifp);
}

/*
 * wm_tick:
 *
 *	One second timer, used to check link status, sweep up
 *	completed transmit jobs, etc.
 */
static void
wm_tick(void *arg)
{
	struct wm_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
#ifndef WM_MPSAFE
	int s;

	s = splnet();
#endif

	WM_TX_LOCK(sc);

	if (sc->sc_stopping)
		goto out;

	if (sc->sc_type >= WM_T_82542_2_1) {
		WM_EVCNT_ADD(&sc->sc_ev_rx_xon, CSR_READ(sc, WMREG_XONRXC));
		WM_EVCNT_ADD(&sc->sc_ev_tx_xon, CSR_READ(sc, WMREG_XONTXC));
		WM_EVCNT_ADD(&sc->sc_ev_rx_xoff, CSR_READ(sc, WMREG_XOFFRXC));
		WM_EVCNT_ADD(&sc->sc_ev_tx_xoff, CSR_READ(sc, WMREG_XOFFTXC));
		WM_EVCNT_ADD(&sc->sc_ev_rx_macctl, CSR_READ(sc, WMREG_FCRUC));
	}

	ifp->if_collisions += CSR_READ(sc, WMREG_COLC);
	ifp->if_ierrors += 0ULL + /* ensure quad_t */
	    + CSR_READ(sc, WMREG_CRCERRS)
	    + CSR_READ(sc, WMREG_ALGNERRC)
	    + CSR_READ(sc, WMREG_SYMERRC)
	    + CSR_READ(sc, WMREG_RXERRC)
	    + CSR_READ(sc, WMREG_SEC)
	    + CSR_READ(sc, WMREG_CEXTERR)
	    + CSR_READ(sc, WMREG_RLEC);
	ifp->if_iqdrops += CSR_READ(sc, WMREG_MPC) + CSR_READ(sc, WMREG_RNBC);

	if (sc->sc_flags & WM_F_HAS_MII)
		mii_tick(&sc->sc_mii);
	else if ((sc->sc_type >= WM_T_82575)
	    && (sc->sc_mediatype == WM_MEDIATYPE_SERDES))
		wm_serdes_tick(sc);
	else
		wm_tbi_tick(sc);

out:
	WM_TX_UNLOCK(sc);
#ifndef WM_MPSAFE
	splx(s);
#endif

	if (!sc->sc_stopping)
		callout_reset(&sc->sc_tick_ch, hz, wm_tick, sc);
}

static int
wm_ifflags_cb(struct ethercom *ec)
{
	struct ifnet *ifp = &ec->ec_if;
	struct wm_softc *sc = ifp->if_softc;
	int change = ifp->if_flags ^ sc->sc_if_flags;
	int rc = 0;

	WM_BOTH_LOCK(sc);

	if (change != 0)
		sc->sc_if_flags = ifp->if_flags;

	if ((change & ~(IFF_CANTCHANGE|IFF_DEBUG)) != 0) {
		rc = ENETRESET;
		goto out;
	}

	if ((change & (IFF_PROMISC | IFF_ALLMULTI)) != 0)
		wm_set_filter(sc);

	wm_set_vlan(sc);

out:
	WM_BOTH_UNLOCK(sc);

	return rc;
}

/*
 * wm_ioctl:		[ifnet interface function]
 *
 *	Handle control requests from the operator.
 */
static int
wm_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct wm_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *) data;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct sockaddr_dl *sdl;
	int s, error;

#ifndef WM_MPSAFE
	s = splnet();
#endif
	switch (cmd) {
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		WM_BOTH_LOCK(sc);
		/* Flow control requires full-duplex mode. */
		if (IFM_SUBTYPE(ifr->ifr_media) == IFM_AUTO ||
		    (ifr->ifr_media & IFM_FDX) == 0)
			ifr->ifr_media &= ~IFM_ETH_FMASK;
		if (IFM_SUBTYPE(ifr->ifr_media) != IFM_AUTO) {
			if ((ifr->ifr_media & IFM_ETH_FMASK) == IFM_FLOW) {
				/* We can do both TXPAUSE and RXPAUSE. */
				ifr->ifr_media |=
				    IFM_ETH_TXPAUSE | IFM_ETH_RXPAUSE;
			}
			sc->sc_flowflags = ifr->ifr_media & IFM_ETH_FMASK;
		}
		WM_BOTH_UNLOCK(sc);
#ifdef WM_MPSAFE
		s = splnet();
#endif
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_mii.mii_media, cmd);
#ifdef WM_MPSAFE
		splx(s);
#endif
		break;
	case SIOCINITIFADDR:
		WM_BOTH_LOCK(sc);
		if (ifa->ifa_addr->sa_family == AF_LINK) {
			sdl = satosdl(ifp->if_dl->ifa_addr);
			(void)sockaddr_dl_setaddr(sdl, sdl->sdl_len,
			    LLADDR(satosdl(ifa->ifa_addr)), ifp->if_addrlen);
			/* unicast address is first multicast entry */
			wm_set_filter(sc);
			error = 0;
			WM_BOTH_UNLOCK(sc);
			break;
		}
		WM_BOTH_UNLOCK(sc);
		/*FALLTHROUGH*/
	default:
#ifdef WM_MPSAFE
		s = splnet();
#endif
		/* It may call wm_start, so unlock here */
		error = ether_ioctl(ifp, cmd, data);
#ifdef WM_MPSAFE
		splx(s);
#endif
		if (error != ENETRESET)
			break;

		error = 0;

		if (cmd == SIOCSIFCAP) {
			error = (*ifp->if_init)(ifp);
		} else if (cmd != SIOCADDMULTI && cmd != SIOCDELMULTI)
			;
		else if (ifp->if_flags & IFF_RUNNING) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			WM_BOTH_LOCK(sc);
			wm_set_filter(sc);
			WM_BOTH_UNLOCK(sc);
		}
		break;
	}

#ifndef WM_MPSAFE
	splx(s);
#endif
	return error;
}

/* MAC address related */

/*
 * Get the offset of MAC address and return it.
 * If error occured, use offset 0.
 */
static uint16_t
wm_check_alt_mac_addr(struct wm_softc *sc)
{
	uint16_t myea[ETHER_ADDR_LEN / 2];
	uint16_t offset = NVM_OFF_MACADDR;

	/* Try to read alternative MAC address pointer */
	if (wm_nvm_read(sc, NVM_OFF_ALT_MAC_ADDR_PTR, 1, &offset) != 0)
		return 0;

	/* Check pointer if it's valid or not. */
	if ((offset == 0x0000) || (offset == 0xffff))
		return 0;

	offset += NVM_OFF_MACADDR_82571(sc->sc_funcid);
	/*
	 * Check whether alternative MAC address is valid or not.
	 * Some cards have non 0xffff pointer but those don't use
	 * alternative MAC address in reality.
	 *
	 * Check whether the broadcast bit is set or not.
	 */
	if (wm_nvm_read(sc, offset, 1, myea) == 0)
		if (((myea[0] & 0xff) & 0x01) == 0)
			return offset; /* Found */

	/* Not found */
	return 0;
}

static int
wm_read_mac_addr(struct wm_softc *sc, uint8_t *enaddr)
{
	uint16_t myea[ETHER_ADDR_LEN / 2];
	uint16_t offset = NVM_OFF_MACADDR;
	int do_invert = 0;

	switch (sc->sc_type) {
	case WM_T_82580:
	case WM_T_I350:
	case WM_T_I354:
		/* EEPROM Top Level Partitioning */
		offset = NVM_OFF_LAN_FUNC_82580(sc->sc_funcid) + 0;
		break;
	case WM_T_82571:
	case WM_T_82575:
	case WM_T_82576:
	case WM_T_80003:
	case WM_T_I210:
	case WM_T_I211:
		offset = wm_check_alt_mac_addr(sc);
		if (offset == 0)
			if ((sc->sc_funcid & 0x01) == 1)
				do_invert = 1;
		break;
	default:
		if ((sc->sc_funcid & 0x01) == 1)
			do_invert = 1;
		break;
	}

	if (wm_nvm_read(sc, offset, sizeof(myea) / sizeof(myea[0]),
		myea) != 0)
		goto bad;

	enaddr[0] = myea[0] & 0xff;
	enaddr[1] = myea[0] >> 8;
	enaddr[2] = myea[1] & 0xff;
	enaddr[3] = myea[1] >> 8;
	enaddr[4] = myea[2] & 0xff;
	enaddr[5] = myea[2] >> 8;

	/*
	 * Toggle the LSB of the MAC address on the second port
	 * of some dual port cards.
	 */
	if (do_invert != 0)
		enaddr[5] ^= 1;

	return 0;

 bad:
	return -1;
}

/*
 * wm_set_ral:
 *
 *	Set an entery in the receive address list.
 */
static void
wm_set_ral(struct wm_softc *sc, const uint8_t *enaddr, int idx)
{
	uint32_t ral_lo, ral_hi;

	if (enaddr != NULL) {
		ral_lo = enaddr[0] | (enaddr[1] << 8) | (enaddr[2] << 16) |
		    (enaddr[3] << 24);
		ral_hi = enaddr[4] | (enaddr[5] << 8);
		ral_hi |= RAL_AV;
	} else {
		ral_lo = 0;
		ral_hi = 0;
	}

	if (sc->sc_type >= WM_T_82544) {
		CSR_WRITE(sc, WMREG_RAL_LO(WMREG_CORDOVA_RAL_BASE, idx),
		    ral_lo);
		CSR_WRITE(sc, WMREG_RAL_HI(WMREG_CORDOVA_RAL_BASE, idx),
		    ral_hi);
	} else {
		CSR_WRITE(sc, WMREG_RAL_LO(WMREG_RAL_BASE, idx), ral_lo);
		CSR_WRITE(sc, WMREG_RAL_HI(WMREG_RAL_BASE, idx), ral_hi);
	}
}

/*
 * wm_mchash:
 *
 *	Compute the hash of the multicast address for the 4096-bit
 *	multicast filter.
 */
static uint32_t
wm_mchash(struct wm_softc *sc, const uint8_t *enaddr)
{
	static const int lo_shift[4] = { 4, 3, 2, 0 };
	static const int hi_shift[4] = { 4, 5, 6, 8 };
	static const int ich8_lo_shift[4] = { 6, 5, 4, 2 };
	static const int ich8_hi_shift[4] = { 2, 3, 4, 6 };
	uint32_t hash;

	if ((sc->sc_type == WM_T_ICH8) || (sc->sc_type == WM_T_ICH9)
	    || (sc->sc_type == WM_T_ICH10) || (sc->sc_type == WM_T_PCH)
	    || (sc->sc_type == WM_T_PCH2) || (sc->sc_type == WM_T_PCH_LPT)) {
		hash = (enaddr[4] >> ich8_lo_shift[sc->sc_mchash_type]) |
		    (((uint16_t) enaddr[5]) << ich8_hi_shift[sc->sc_mchash_type]);
		return (hash & 0x3ff);
	}
	hash = (enaddr[4] >> lo_shift[sc->sc_mchash_type]) |
	    (((uint16_t) enaddr[5]) << hi_shift[sc->sc_mchash_type]);

	return (hash & 0xfff);
}

/*
 * wm_set_filter:
 *
 *	Set up the receive filter.
 */
static void
wm_set_filter(struct wm_softc *sc)
{
	struct ethercom *ec = &sc->sc_ethercom;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	bus_addr_t mta_reg;
	uint32_t hash, reg, bit;
	int i, size;

	if (sc->sc_type >= WM_T_82544)
		mta_reg = WMREG_CORDOVA_MTA;
	else
		mta_reg = WMREG_MTA;

	sc->sc_rctl &= ~(RCTL_BAM | RCTL_UPE | RCTL_MPE);

	if (ifp->if_flags & IFF_BROADCAST)
		sc->sc_rctl |= RCTL_BAM;
	if (ifp->if_flags & IFF_PROMISC) {
		sc->sc_rctl |= RCTL_UPE;
		goto allmulti;
	}

	/*
	 * Set the station address in the first RAL slot, and
	 * clear the remaining slots.
	 */
	if (sc->sc_type == WM_T_ICH8)
		size = WM_RAL_TABSIZE_ICH8 -1;
	else if ((sc->sc_type == WM_T_ICH9) || (sc->sc_type == WM_T_ICH10)
	    || (sc->sc_type == WM_T_PCH) || (sc->sc_type == WM_T_PCH2)
	    || (sc->sc_type == WM_T_PCH_LPT))
		size = WM_RAL_TABSIZE_ICH8;
	else if (sc->sc_type == WM_T_82575)
		size = WM_RAL_TABSIZE_82575;
	else if ((sc->sc_type == WM_T_82576) || (sc->sc_type == WM_T_82580))
		size = WM_RAL_TABSIZE_82576;
	else if ((sc->sc_type == WM_T_I350) || (sc->sc_type == WM_T_I354))
		size = WM_RAL_TABSIZE_I350;
	else
		size = WM_RAL_TABSIZE;
	wm_set_ral(sc, CLLADDR(ifp->if_sadl), 0);
	for (i = 1; i < size; i++)
		wm_set_ral(sc, NULL, i);

	if ((sc->sc_type == WM_T_ICH8) || (sc->sc_type == WM_T_ICH9)
	    || (sc->sc_type == WM_T_ICH10) || (sc->sc_type == WM_T_PCH)
	    || (sc->sc_type == WM_T_PCH2) || (sc->sc_type == WM_T_PCH_LPT))
		size = WM_ICH8_MC_TABSIZE;
	else
		size = WM_MC_TABSIZE;
	/* Clear out the multicast table. */
	for (i = 0; i < size; i++)
		CSR_WRITE(sc, mta_reg + (i << 2), 0);

	ETHER_FIRST_MULTI(step, ec, enm);
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
			/*
			 * We must listen to a range of multicast addresses.
			 * For now, just accept all multicasts, rather than
			 * trying to set only those filter bits needed to match
			 * the range.  (At this time, the only use of address
			 * ranges is for IP multicast routing, for which the
			 * range is big enough to require all bits set.)
			 */
			goto allmulti;
		}

		hash = wm_mchash(sc, enm->enm_addrlo);

		reg = (hash >> 5);
		if ((sc->sc_type == WM_T_ICH8) || (sc->sc_type == WM_T_ICH9)
		    || (sc->sc_type == WM_T_ICH10) || (sc->sc_type == WM_T_PCH)
		    || (sc->sc_type == WM_T_PCH2)
		    || (sc->sc_type == WM_T_PCH_LPT))
			reg &= 0x1f;
		else
			reg &= 0x7f;
		bit = hash & 0x1f;

		hash = CSR_READ(sc, mta_reg + (reg << 2));
		hash |= 1U << bit;

		/* XXX Hardware bug?? */
		if (sc->sc_type == WM_T_82544 && (reg & 0xe) == 1) {
			bit = CSR_READ(sc, mta_reg + ((reg - 1) << 2));
			CSR_WRITE(sc, mta_reg + (reg << 2), hash);
			CSR_WRITE(sc, mta_reg + ((reg - 1) << 2), bit);
		} else
			CSR_WRITE(sc, mta_reg + (reg << 2), hash);

		ETHER_NEXT_MULTI(step, enm);
	}

	ifp->if_flags &= ~IFF_ALLMULTI;
	goto setit;

 allmulti:
	ifp->if_flags |= IFF_ALLMULTI;
	sc->sc_rctl |= RCTL_MPE;

 setit:
	CSR_WRITE(sc, WMREG_RCTL, sc->sc_rctl);
}

/* Reset and init related */

static void
wm_set_vlan(struct wm_softc *sc)
{
	/* Deal with VLAN enables. */
	if (VLAN_ATTACHED(&sc->sc_ethercom))
		sc->sc_ctrl |= CTRL_VME;
	else
		sc->sc_ctrl &= ~CTRL_VME;

	/* Write the control registers. */
	CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl);
}

static void
wm_set_pcie_completion_timeout(struct wm_softc *sc)
{
	uint32_t gcr;
	pcireg_t ctrl2;

	gcr = CSR_READ(sc, WMREG_GCR);

	/* Only take action if timeout value is defaulted to 0 */
	if ((gcr & GCR_CMPL_TMOUT_MASK) != 0)
		goto out;

	if ((gcr & GCR_CAP_VER2) == 0) {
		gcr |= GCR_CMPL_TMOUT_10MS;
		goto out;
	}

	ctrl2 = pci_conf_read(sc->sc_pc, sc->sc_pcitag,
	    sc->sc_pcixe_capoff + PCIE_DCSR2);
	ctrl2 |= WM_PCIE_DCSR2_16MS;
	pci_conf_write(sc->sc_pc, sc->sc_pcitag,
	    sc->sc_pcixe_capoff + PCIE_DCSR2, ctrl2);

out:
	/* Disable completion timeout resend */
	gcr &= ~GCR_CMPL_TMOUT_RESEND;

	CSR_WRITE(sc, WMREG_GCR, gcr);
}

void
wm_get_auto_rd_done(struct wm_softc *sc)
{
	int i;

	/* wait for eeprom to reload */
	switch (sc->sc_type) {
	case WM_T_82571:
	case WM_T_82572:
	case WM_T_82573:
	case WM_T_82574:
	case WM_T_82583:
	case WM_T_82575:
	case WM_T_82576:
	case WM_T_82580:
	case WM_T_I350:
	case WM_T_I354:
	case WM_T_I210:
	case WM_T_I211:
	case WM_T_80003:
	case WM_T_ICH8:
	case WM_T_ICH9:
		for (i = 0; i < 10; i++) {
			if (CSR_READ(sc, WMREG_EECD) & EECD_EE_AUTORD)
				break;
			delay(1000);
		}
		if (i == 10) {
			log(LOG_ERR, "%s: auto read from eeprom failed to "
			    "complete\n", device_xname(sc->sc_dev));
		}
		break;
	default:
		break;
	}
}

void
wm_lan_init_done(struct wm_softc *sc)
{
	uint32_t reg = 0;
	int i;

	/* wait for eeprom to reload */
	switch (sc->sc_type) {
	case WM_T_ICH10:
	case WM_T_PCH:
	case WM_T_PCH2:
	case WM_T_PCH_LPT:
		for (i = 0; i < WM_ICH8_LAN_INIT_TIMEOUT; i++) {
			reg = CSR_READ(sc, WMREG_STATUS);
			if ((reg & STATUS_LAN_INIT_DONE) != 0)
				break;
			delay(100);
		}
		if (i >= WM_ICH8_LAN_INIT_TIMEOUT) {
			log(LOG_ERR, "%s: %s: lan_init_done failed to "
			    "complete\n", device_xname(sc->sc_dev), __func__);
		}
		break;
	default:
		panic("%s: %s: unknown type\n", device_xname(sc->sc_dev),
		    __func__);
		break;
	}

	reg &= ~STATUS_LAN_INIT_DONE;
	CSR_WRITE(sc, WMREG_STATUS, reg);
}

void
wm_get_cfg_done(struct wm_softc *sc)
{
	int mask;
	uint32_t reg;
	int i;

	/* wait for eeprom to reload */
	switch (sc->sc_type) {
	case WM_T_82542_2_0:
	case WM_T_82542_2_1:
		/* null */
		break;
	case WM_T_82543:
	case WM_T_82544:
	case WM_T_82540:
	case WM_T_82545:
	case WM_T_82545_3:
	case WM_T_82546:
	case WM_T_82546_3:
	case WM_T_82541:
	case WM_T_82541_2:
	case WM_T_82547:
	case WM_T_82547_2:
	case WM_T_82573:
	case WM_T_82574:
	case WM_T_82583:
		/* generic */
		delay(10*1000);
		break;
	case WM_T_80003:
	case WM_T_82571:
	case WM_T_82572:
	case WM_T_82575:
	case WM_T_82576:
	case WM_T_82580:
	case WM_T_I350:
	case WM_T_I354:
	case WM_T_I210:
	case WM_T_I211:
		if (sc->sc_type == WM_T_82571) {
			/* Only 82571 shares port 0 */
			mask = EEMNGCTL_CFGDONE_0;
		} else
			mask = EEMNGCTL_CFGDONE_0 << sc->sc_funcid;
		for (i = 0; i < WM_PHY_CFG_TIMEOUT; i++) {
			if (CSR_READ(sc, WMREG_EEMNGCTL) & mask)
				break;
			delay(1000);
		}
		if (i >= WM_PHY_CFG_TIMEOUT) {
			DPRINTF(WM_DEBUG_GMII, ("%s: %s failed\n",
				device_xname(sc->sc_dev), __func__));
		}
		break;
	case WM_T_ICH8:
	case WM_T_ICH9:
	case WM_T_ICH10:
	case WM_T_PCH:
	case WM_T_PCH2:
	case WM_T_PCH_LPT:
		delay(10*1000);
		if (sc->sc_type >= WM_T_ICH10)
			wm_lan_init_done(sc);
		else
			wm_get_auto_rd_done(sc);

		reg = CSR_READ(sc, WMREG_STATUS);
		if ((reg & STATUS_PHYRA) != 0)
			CSR_WRITE(sc, WMREG_STATUS, reg & ~STATUS_PHYRA);
		break;
	default:
		panic("%s: %s: unknown type\n", device_xname(sc->sc_dev),
		    __func__);
		break;
	}
}

/* Init hardware bits */
void
wm_initialize_hardware_bits(struct wm_softc *sc)
{
	uint32_t tarc0, tarc1, reg;

	/* For 82571 variant, 80003 and ICHs */
	if (((sc->sc_type >= WM_T_82571) && (sc->sc_type <= WM_T_82583))
	    || (sc->sc_type >= WM_T_80003)) {

		/* Transmit Descriptor Control 0 */
		reg = CSR_READ(sc, WMREG_TXDCTL(0));
		reg |= TXDCTL_COUNT_DESC;
		CSR_WRITE(sc, WMREG_TXDCTL(0), reg);

		/* Transmit Descriptor Control 1 */
		reg = CSR_READ(sc, WMREG_TXDCTL(1));
		reg |= TXDCTL_COUNT_DESC;
		CSR_WRITE(sc, WMREG_TXDCTL(1), reg);

		/* TARC0 */
		tarc0 = CSR_READ(sc, WMREG_TARC0);
		switch (sc->sc_type) {
		case WM_T_82571:
		case WM_T_82572:
		case WM_T_82573:
		case WM_T_82574:
		case WM_T_82583:
		case WM_T_80003:
			/* Clear bits 30..27 */
			tarc0 &= ~__BITS(30, 27);
			break;
		default:
			break;
		}

		switch (sc->sc_type) {
		case WM_T_82571:
		case WM_T_82572:
			tarc0 |= __BITS(26, 23); /* TARC0 bits 23-26 */

			tarc1 = CSR_READ(sc, WMREG_TARC1);
			tarc1 &= ~__BITS(30, 29); /* Clear bits 30 and 29 */
			tarc1 |= __BITS(26, 24); /* TARC1 bits 26-24 */
			/* 8257[12] Errata No.7 */
			tarc1 |= __BIT(22); /* TARC1 bits 22 */

			/* TARC1 bit 28 */
			if ((CSR_READ(sc, WMREG_TCTL) & TCTL_MULR) != 0)
				tarc1 &= ~__BIT(28);
			else
				tarc1 |= __BIT(28);
			CSR_WRITE(sc, WMREG_TARC1, tarc1);

			/*
			 * 8257[12] Errata No.13
			 * Disable Dyamic Clock Gating.
			 */
			reg = CSR_READ(sc, WMREG_CTRL_EXT);
			reg &= ~CTRL_EXT_DMA_DYN_CLK;
			CSR_WRITE(sc, WMREG_CTRL_EXT, reg);
			break;
		case WM_T_82573:
		case WM_T_82574:
		case WM_T_82583:
			if ((sc->sc_type == WM_T_82574)
			    || (sc->sc_type == WM_T_82583))
				tarc0 |= __BIT(26); /* TARC0 bit 26 */

			/* Extended Device Control */
			reg = CSR_READ(sc, WMREG_CTRL_EXT);
			reg &= ~__BIT(23);	/* Clear bit 23 */
			reg |= __BIT(22);	/* Set bit 22 */
			CSR_WRITE(sc, WMREG_CTRL_EXT, reg);

			/* Device Control */
			sc->sc_ctrl &= ~__BIT(29);	/* Clear bit 29 */
			CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl);

			/* PCIe Control Register */
			/*
			 * 82573 Errata (unknown).
			 *
			 * 82574 Errata 25 and 82583 Errata 12
			 * "Dropped Rx Packets":
			 *   NVM Image Version 2.1.4 and newer has no this bug.
			 */
			reg = CSR_READ(sc, WMREG_GCR);
			reg |= GCR_L1_ACT_WITHOUT_L0S_RX;
			CSR_WRITE(sc, WMREG_GCR, reg);

			if ((sc->sc_type == WM_T_82574)
			    || (sc->sc_type == WM_T_82583)) {
				/*
				 * Document says this bit must be set for
				 * proper operation.
				 */
				reg = CSR_READ(sc, WMREG_GCR);
				reg |= __BIT(22);
				CSR_WRITE(sc, WMREG_GCR, reg);

				/*
				 * Apply workaround for hardware errata
				 * documented in errata docs Fixes issue where
				 * some error prone or unreliable PCIe
				 * completions are occurring, particularly
				 * with ASPM enabled. Without fix, issue can
				 * cause Tx timeouts.
				 */
				reg = CSR_READ(sc, WMREG_GCR2);
				reg |= __BIT(0);
				CSR_WRITE(sc, WMREG_GCR2, reg);
			}
			break;
		case WM_T_80003:
			/* TARC0 */
			if ((sc->sc_mediatype == WM_MEDIATYPE_FIBER)
			    || (sc->sc_mediatype == WM_MEDIATYPE_SERDES))
				tarc0 &= ~__BIT(20); /* Clear bits 20 */

			/* TARC1 bit 28 */
			tarc1 = CSR_READ(sc, WMREG_TARC1);
			if ((CSR_READ(sc, WMREG_TCTL) & TCTL_MULR) != 0)
				tarc1 &= ~__BIT(28);
			else
				tarc1 |= __BIT(28);
			CSR_WRITE(sc, WMREG_TARC1, tarc1);
			break;
		case WM_T_ICH8:
		case WM_T_ICH9:
		case WM_T_ICH10:
		case WM_T_PCH:
		case WM_T_PCH2:
		case WM_T_PCH_LPT:
			/* TARC 0 */
			if (sc->sc_type == WM_T_ICH8) {
				/* Set TARC0 bits 29 and 28 */
				tarc0 |= __BITS(29, 28);
			}
			/* Set TARC0 bits 23,24,26,27 */
			tarc0 |= __BITS(27, 26) | __BITS(24, 23);

			/* CTRL_EXT */
			reg = CSR_READ(sc, WMREG_CTRL_EXT);
			reg |= __BIT(22);	/* Set bit 22 */
			/*
			 * Enable PHY low-power state when MAC is at D3
			 * w/o WoL
			 */
			if (sc->sc_type >= WM_T_PCH)
				reg |= CTRL_EXT_PHYPDEN;
			CSR_WRITE(sc, WMREG_CTRL_EXT, reg);

			/* TARC1 */
			tarc1 = CSR_READ(sc, WMREG_TARC1);
			/* bit 28 */
			if ((CSR_READ(sc, WMREG_TCTL) & TCTL_MULR) != 0)
				tarc1 &= ~__BIT(28);
			else
				tarc1 |= __BIT(28);
			tarc1 |= __BIT(24) | __BIT(26) | __BIT(30);
			CSR_WRITE(sc, WMREG_TARC1, tarc1);

			/* Device Status */
			if (sc->sc_type == WM_T_ICH8) {
				reg = CSR_READ(sc, WMREG_STATUS);
				reg &= ~__BIT(31);
				CSR_WRITE(sc, WMREG_STATUS, reg);

			}

			/*
			 * Work-around descriptor data corruption issue during
			 * NFS v2 UDP traffic, just disable the NFS filtering
			 * capability.
			 */
			reg = CSR_READ(sc, WMREG_RFCTL);
			reg |= WMREG_RFCTL_NFSWDIS | WMREG_RFCTL_NFSRDIS;
			CSR_WRITE(sc, WMREG_RFCTL, reg);
			break;
		default:
			break;
		}
		CSR_WRITE(sc, WMREG_TARC0, tarc0);

		/*
		 * 8257[12] Errata No.52 and some others.
		 * Avoid RSS Hash Value bug.
		 */
		switch (sc->sc_type) {
		case WM_T_82571:
		case WM_T_82572:
		case WM_T_82573:
		case WM_T_80003:
		case WM_T_ICH8:
			reg = CSR_READ(sc, WMREG_RFCTL);
			reg |= WMREG_RFCTL_NEWIPV6EXDIS |WMREG_RFCTL_IPV6EXDIS;
			CSR_WRITE(sc, WMREG_RFCTL, reg);
			break;
		default:
			break;
		}
	}
}

static uint32_t
wm_rxpbs_adjust_82580(uint32_t val)
{
	uint32_t rv = 0;

	if (val < __arraycount(wm_82580_rxpbs_table))
		rv = wm_82580_rxpbs_table[val];

	return rv;
}

/*
 * wm_reset:
 *
 *	Reset the i82542 chip.
 */
static void
wm_reset(struct wm_softc *sc)
{
	int phy_reset = 0;
	int error = 0;
	uint32_t reg, mask;

	/*
	 * Allocate on-chip memory according to the MTU size.
	 * The Packet Buffer Allocation register must be written
	 * before the chip is reset.
	 */
	switch (sc->sc_type) {
	case WM_T_82547:
	case WM_T_82547_2:
		sc->sc_pba = sc->sc_ethercom.ec_if.if_mtu > 8192 ?
		    PBA_22K : PBA_30K;
		sc->sc_txfifo_head = 0;
		sc->sc_txfifo_addr = sc->sc_pba << PBA_ADDR_SHIFT;
		sc->sc_txfifo_size =
		    (PBA_40K - sc->sc_pba) << PBA_BYTE_SHIFT;
		sc->sc_txfifo_stall = 0;
		break;
	case WM_T_82571:
	case WM_T_82572:
	case WM_T_82575:	/* XXX need special handing for jumbo frames */
	case WM_T_80003:
		sc->sc_pba = PBA_32K;
		break;
	case WM_T_82573:
		sc->sc_pba = PBA_12K;
		break;
	case WM_T_82574:
	case WM_T_82583:
		sc->sc_pba = PBA_20K;
		break;
	case WM_T_82576:
		sc->sc_pba = CSR_READ(sc, WMREG_RXPBS);
		sc->sc_pba &= RXPBS_SIZE_MASK_82576;
		break;
	case WM_T_82580:
	case WM_T_I350:
	case WM_T_I354:
		sc->sc_pba = wm_rxpbs_adjust_82580(CSR_READ(sc, WMREG_RXPBS));
		break;
	case WM_T_I210:
	case WM_T_I211:
		sc->sc_pba = PBA_34K;
		break;
	case WM_T_ICH8:
		/* Workaround for a bit corruption issue in FIFO memory */
		sc->sc_pba = PBA_8K;
		CSR_WRITE(sc, WMREG_PBS, PBA_16K);
		break;
	case WM_T_ICH9:
	case WM_T_ICH10:
		sc->sc_pba = sc->sc_ethercom.ec_if.if_mtu > 4096 ?
		    PBA_14K : PBA_10K;
		break;
	case WM_T_PCH:
	case WM_T_PCH2:
	case WM_T_PCH_LPT:
		sc->sc_pba = PBA_26K;
		break;
	default:
		sc->sc_pba = sc->sc_ethercom.ec_if.if_mtu > 8192 ?
		    PBA_40K : PBA_48K;
		break;
	}
	/*
	 * Only old or non-multiqueue devices have the PBA register
	 * XXX Need special handling for 82575.
	 */
	if (((sc->sc_flags & WM_F_NEWQUEUE) == 0)
	    || (sc->sc_type == WM_T_82575))
		CSR_WRITE(sc, WMREG_PBA, sc->sc_pba);

	/* Prevent the PCI-E bus from sticking */
	if (sc->sc_flags & WM_F_PCIE) {
		int timeout = 800;

		sc->sc_ctrl |= CTRL_GIO_M_DIS;
		CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl);

		while (timeout--) {
			if ((CSR_READ(sc, WMREG_STATUS) & STATUS_GIO_M_ENA)
			    == 0)
				break;
			delay(100);
		}
	}

	/* Set the completion timeout for interface */
	if ((sc->sc_type == WM_T_82575) || (sc->sc_type == WM_T_82576)
	    || (sc->sc_type == WM_T_82580)
	    || (sc->sc_type == WM_T_I350) || (sc->sc_type == WM_T_I354)
	    || (sc->sc_type == WM_T_I210) || (sc->sc_type == WM_T_I211))
		wm_set_pcie_completion_timeout(sc);

	/* Clear interrupt */
	CSR_WRITE(sc, WMREG_IMC, 0xffffffffU);
	if (sc->sc_nintrs > 1) {
		if (sc->sc_type != WM_T_82574) {
			CSR_WRITE(sc, WMREG_EIMC, 0xffffffffU);
			CSR_WRITE(sc, WMREG_EIAC, 0);
		} else {
			CSR_WRITE(sc, WMREG_EIAC_82574, 0);
		}
	}

	/* Stop the transmit and receive processes. */
	CSR_WRITE(sc, WMREG_RCTL, 0);
	sc->sc_rctl &= ~RCTL_EN;
	CSR_WRITE(sc, WMREG_TCTL, TCTL_PSP);
	CSR_WRITE_FLUSH(sc);

	/* XXX set_tbi_sbp_82543() */

	delay(10*1000);

	/* Must acquire the MDIO ownership before MAC reset */
	switch (sc->sc_type) {
	case WM_T_82573:
	case WM_T_82574:
	case WM_T_82583:
		error = wm_get_hw_semaphore_82573(sc);
		break;
	default:
		break;
	}

	/*
	 * 82541 Errata 29? & 82547 Errata 28?
	 * See also the description about PHY_RST bit in CTRL register
	 * in 8254x_GBe_SDM.pdf.
	 */
	if ((sc->sc_type == WM_T_82541) || (sc->sc_type == WM_T_82547)) {
		CSR_WRITE(sc, WMREG_CTRL,
		    CSR_READ(sc, WMREG_CTRL) | CTRL_PHY_RESET);
		CSR_WRITE_FLUSH(sc);
		delay(5000);
	}

	switch (sc->sc_type) {
	case WM_T_82544: /* XXX check whether WM_F_IOH_VALID is set */
	case WM_T_82541:
	case WM_T_82541_2:
	case WM_T_82547:
	case WM_T_82547_2:
		/*
		 * On some chipsets, a reset through a memory-mapped write
		 * cycle can cause the chip to reset before completing the
		 * write cycle.  This causes major headache that can be
		 * avoided by issuing the reset via indirect register writes
		 * through I/O space.
		 *
		 * So, if we successfully mapped the I/O BAR at attach time,
		 * use that.  Otherwise, try our luck with a memory-mapped
		 * reset.
		 */
		if (sc->sc_flags & WM_F_IOH_VALID)
			wm_io_write(sc, WMREG_CTRL, CTRL_RST);
		else
			CSR_WRITE(sc, WMREG_CTRL, CTRL_RST);
		break;
	case WM_T_82545_3:
	case WM_T_82546_3:
		/* Use the shadow control register on these chips. */
		CSR_WRITE(sc, WMREG_CTRL_SHADOW, CTRL_RST);
		break;
	case WM_T_80003:
		mask = swfwphysem[sc->sc_funcid];
		reg = CSR_READ(sc, WMREG_CTRL) | CTRL_RST;
		wm_get_swfw_semaphore(sc, mask);
		CSR_WRITE(sc, WMREG_CTRL, reg);
		wm_put_swfw_semaphore(sc, mask);
		break;
	case WM_T_ICH8:
	case WM_T_ICH9:
	case WM_T_ICH10:
	case WM_T_PCH:
	case WM_T_PCH2:
	case WM_T_PCH_LPT:
		reg = CSR_READ(sc, WMREG_CTRL) | CTRL_RST;
		if (wm_check_reset_block(sc) == 0) {
			/*
			 * Gate automatic PHY configuration by hardware on
			 * non-managed 82579
			 */
			if ((sc->sc_type == WM_T_PCH2)
			    && ((CSR_READ(sc, WMREG_FWSM) & FWSM_FW_VALID)
				!= 0))
				wm_gate_hw_phy_config_ich8lan(sc, 1);


			reg |= CTRL_PHY_RESET;
			phy_reset = 1;
		}
		wm_get_swfwhw_semaphore(sc);
		CSR_WRITE(sc, WMREG_CTRL, reg);
		/* Don't insert a completion barrier when reset */
		delay(20*1000);
		wm_put_swfwhw_semaphore(sc);
		break;
	case WM_T_82580:
	case WM_T_I350:
	case WM_T_I354:
	case WM_T_I210:
	case WM_T_I211:
		CSR_WRITE(sc, WMREG_CTRL, CSR_READ(sc, WMREG_CTRL) | CTRL_RST);
		if (sc->sc_pcidevid != PCI_PRODUCT_INTEL_DH89XXCC_SGMII)
			CSR_WRITE_FLUSH(sc);
		delay(5000);
		break;
	case WM_T_82542_2_0:
	case WM_T_82542_2_1:
	case WM_T_82543:
	case WM_T_82540:
	case WM_T_82545:
	case WM_T_82546:
	case WM_T_82571:
	case WM_T_82572:
	case WM_T_82573:
	case WM_T_82574:
	case WM_T_82575:
	case WM_T_82576:
	case WM_T_82583:
	default:
		/* Everything else can safely use the documented method. */
		CSR_WRITE(sc, WMREG_CTRL, CSR_READ(sc, WMREG_CTRL) | CTRL_RST);
		break;
	}

	/* Must release the MDIO ownership after MAC reset */
	switch (sc->sc_type) {
	case WM_T_82573:
	case WM_T_82574:
	case WM_T_82583:
		if (error == 0)
			wm_put_hw_semaphore_82573(sc);
		break;
	default:
		break;
	}

	if (phy_reset != 0)
		wm_get_cfg_done(sc);

	/* reload EEPROM */
	switch (sc->sc_type) {
	case WM_T_82542_2_0:
	case WM_T_82542_2_1:
	case WM_T_82543:
	case WM_T_82544:
		delay(10);
		reg = CSR_READ(sc, WMREG_CTRL_EXT) | CTRL_EXT_EE_RST;
		CSR_WRITE(sc, WMREG_CTRL_EXT, reg);
		CSR_WRITE_FLUSH(sc);
		delay(2000);
		break;
	case WM_T_82540:
	case WM_T_82545:
	case WM_T_82545_3:
	case WM_T_82546:
	case WM_T_82546_3:
		delay(5*1000);
		/* XXX Disable HW ARPs on ASF enabled adapters */
		break;
	case WM_T_82541:
	case WM_T_82541_2:
	case WM_T_82547:
	case WM_T_82547_2:
		delay(20000);
		/* XXX Disable HW ARPs on ASF enabled adapters */
		break;
	case WM_T_82571:
	case WM_T_82572:
	case WM_T_82573:
	case WM_T_82574:
	case WM_T_82583:
		if (sc->sc_flags & WM_F_EEPROM_FLASH) {
			delay(10);
			reg = CSR_READ(sc, WMREG_CTRL_EXT) | CTRL_EXT_EE_RST;
			CSR_WRITE(sc, WMREG_CTRL_EXT, reg);
			CSR_WRITE_FLUSH(sc);
		}
		/* check EECD_EE_AUTORD */
		wm_get_auto_rd_done(sc);
		/*
		 * Phy configuration from NVM just starts after EECD_AUTO_RD
		 * is set.
		 */
		if ((sc->sc_type == WM_T_82573) || (sc->sc_type == WM_T_82574)
		    || (sc->sc_type == WM_T_82583))
			delay(25*1000);
		break;
	case WM_T_82575:
	case WM_T_82576:
	case WM_T_82580:
	case WM_T_I350:
	case WM_T_I354:
	case WM_T_I210:
	case WM_T_I211:
	case WM_T_80003:
		/* check EECD_EE_AUTORD */
		wm_get_auto_rd_done(sc);
		break;
	case WM_T_ICH8:
	case WM_T_ICH9:
	case WM_T_ICH10:
	case WM_T_PCH:
	case WM_T_PCH2:
	case WM_T_PCH_LPT:
		break;
	default:
		panic("%s: unknown type\n", __func__);
	}

	/* Check whether EEPROM is present or not */
	switch (sc->sc_type) {
	case WM_T_82575:
	case WM_T_82576:
	case WM_T_82580:
	case WM_T_I350:
	case WM_T_I354:
	case WM_T_ICH8:
	case WM_T_ICH9:
		if ((CSR_READ(sc, WMREG_EECD) & EECD_EE_PRES) == 0) {
			/* Not found */
			sc->sc_flags |= WM_F_EEPROM_INVALID;
			if (sc->sc_type == WM_T_82575)
				wm_reset_init_script_82575(sc);
		}
		break;
	default:
		break;
	}

	if ((sc->sc_type == WM_T_82580)
	    || (sc->sc_type == WM_T_I350) || (sc->sc_type == WM_T_I354)) {
		/* clear global device reset status bit */
		CSR_WRITE(sc, WMREG_STATUS, STATUS_DEV_RST_SET);
	}

	/* Clear any pending interrupt events. */
	CSR_WRITE(sc, WMREG_IMC, 0xffffffffU);
	reg = CSR_READ(sc, WMREG_ICR);
	if (sc->sc_nintrs > 1) {
		if (sc->sc_type != WM_T_82574) {
			CSR_WRITE(sc, WMREG_EIMC, 0xffffffffU);
			CSR_WRITE(sc, WMREG_EIAC, 0);
		} else
			CSR_WRITE(sc, WMREG_EIAC_82574, 0);
	}

	/* reload sc_ctrl */
	sc->sc_ctrl = CSR_READ(sc, WMREG_CTRL);

	if ((sc->sc_type >= WM_T_I350) && (sc->sc_type <= WM_T_I211))
		wm_set_eee_i350(sc);

	/* dummy read from WUC */
	if (sc->sc_type == WM_T_PCH)
		reg = wm_gmii_hv_readreg(sc->sc_dev, 1, BM_WUC);
	/*
	 * For PCH, this write will make sure that any noise will be detected
	 * as a CRC error and be dropped rather than show up as a bad packet
	 * to the DMA engine
	 */
	if (sc->sc_type == WM_T_PCH)
		CSR_WRITE(sc, WMREG_CRC_OFFSET, 0x65656565);

	if ((sc->sc_flags & WM_F_NEWQUEUE) != 0)
		CSR_WRITE(sc, WMREG_WUC, 0);

	wm_reset_mdicnfg_82580(sc);

	if ((sc->sc_flags & WM_F_PLL_WA_I210) != 0)
		wm_pll_workaround_i210(sc);
}

/*
 * wm_add_rxbuf:
 *
 *	Add a receive buffer to the indiciated descriptor.
 */
static int
wm_add_rxbuf(struct wm_softc *sc, int idx)
{
	struct wm_rxsoft *rxs = &sc->sc_rxsoft[idx];
	struct mbuf *m;
	int error;

	KASSERT(WM_RX_LOCKED(sc));

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return ENOBUFS;

	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return ENOBUFS;
	}

	if (rxs->rxs_mbuf != NULL)
		bus_dmamap_unload(sc->sc_dmat, rxs->rxs_dmamap);

	rxs->rxs_mbuf = m;

	m->m_len = m->m_pkthdr.len = m->m_ext.ext_size;
	error = bus_dmamap_load_mbuf(sc->sc_dmat, rxs->rxs_dmamap, m,
	    BUS_DMA_READ|BUS_DMA_NOWAIT);
	if (error) {
		/* XXX XXX XXX */
		aprint_error_dev(sc->sc_dev,
		    "unable to load rx DMA map %d, error = %d\n",
		    idx, error);
		panic("wm_add_rxbuf");
	}

	bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
	    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);

	if ((sc->sc_flags & WM_F_NEWQUEUE) != 0) {
		if ((sc->sc_rctl & RCTL_EN) != 0)
			WM_INIT_RXDESC(sc, idx);
	} else
		WM_INIT_RXDESC(sc, idx);

	return 0;
}

/*
 * wm_rxdrain:
 *
 *	Drain the receive queue.
 */
static void
wm_rxdrain(struct wm_softc *sc)
{
	struct wm_rxsoft *rxs;
	int i;

	KASSERT(WM_RX_LOCKED(sc));

	for (i = 0; i < WM_NRXDESC; i++) {
		rxs = &sc->sc_rxsoft[i];
		if (rxs->rxs_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, rxs->rxs_dmamap);
			m_freem(rxs->rxs_mbuf);
			rxs->rxs_mbuf = NULL;
		}
	}
}

/*
 * wm_init:		[ifnet interface function]
 *
 *	Initialize the interface.
 */
static int
wm_init(struct ifnet *ifp)
{
	struct wm_softc *sc = ifp->if_softc;
	int ret;

	WM_BOTH_LOCK(sc);
	ret = wm_init_locked(ifp);
	WM_BOTH_UNLOCK(sc);

	return ret;
}

static int
wm_init_locked(struct ifnet *ifp)
{
	struct wm_softc *sc = ifp->if_softc;
	struct wm_rxsoft *rxs;
	int i, j, trynum, error = 0;
	uint32_t reg;

	KASSERT(WM_BOTH_LOCKED(sc));
	/*
	 * *_HDR_ALIGNED_P is constant 1 if __NO_STRICT_ALIGMENT is set.
	 * There is a small but measurable benefit to avoiding the adjusment
	 * of the descriptor so that the headers are aligned, for normal mtu,
	 * on such platforms.  One possibility is that the DMA itself is
	 * slightly more efficient if the front of the entire packet (instead
	 * of the front of the headers) is aligned.
	 *
	 * Note we must always set align_tweak to 0 if we are using
	 * jumbo frames.
	 */
#ifdef __NO_STRICT_ALIGNMENT
	sc->sc_align_tweak = 0;
#else
	if ((ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN) > (MCLBYTES - 2))
		sc->sc_align_tweak = 0;
	else
		sc->sc_align_tweak = 2;
#endif /* __NO_STRICT_ALIGNMENT */

	/* Cancel any pending I/O. */
	wm_stop_locked(ifp, 0);

	/* update statistics before reset */
	ifp->if_collisions += CSR_READ(sc, WMREG_COLC);
	ifp->if_ierrors += CSR_READ(sc, WMREG_RXERRC);

	/* Reset the chip to a known state. */
	wm_reset(sc);

	switch (sc->sc_type) {
	case WM_T_82571:
	case WM_T_82572:
	case WM_T_82573:
	case WM_T_82574:
	case WM_T_82583:
	case WM_T_80003:
	case WM_T_ICH8:
	case WM_T_ICH9:
	case WM_T_ICH10:
	case WM_T_PCH:
	case WM_T_PCH2:
	case WM_T_PCH_LPT:
		if (wm_check_mng_mode(sc) != 0)
			wm_get_hw_control(sc);
		break;
	default:
		break;
	}

	/* Init hardware bits */
	wm_initialize_hardware_bits(sc);

	/* Reset the PHY. */
	if (sc->sc_flags & WM_F_HAS_MII)
		wm_gmii_reset(sc);

	/* Calculate (E)ITR value */
	if ((sc->sc_flags & WM_F_NEWQUEUE) != 0) {
		sc->sc_itr = 450;	/* For EITR */
	} else if (sc->sc_type >= WM_T_82543) {
		/*
		 * Set up the interrupt throttling register (units of 256ns)
		 * Note that a footnote in Intel's documentation says this
		 * ticker runs at 1/4 the rate when the chip is in 100Mbit
		 * or 10Mbit mode.  Empirically, it appears to be the case
		 * that that is also true for the 1024ns units of the other
		 * interrupt-related timer registers -- so, really, we ought
		 * to divide this value by 4 when the link speed is low.
		 *
		 * XXX implement this division at link speed change!
		 */

		/*
		 * For N interrupts/sec, set this value to:
		 * 1000000000 / (N * 256).  Note that we set the
		 * absolute and packet timer values to this value
		 * divided by 4 to get "simple timer" behavior.
		 */

		sc->sc_itr = 1500;		/* 2604 ints/sec */
	}

	/* Initialize the transmit descriptor ring. */
	memset(sc->sc_txdescs, 0, WM_TXDESCSIZE(sc));
	WM_CDTXSYNC(sc, 0, WM_NTXDESC(sc),
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	sc->sc_txfree = WM_NTXDESC(sc);
	sc->sc_txnext = 0;

	if (sc->sc_type < WM_T_82543) {
		CSR_WRITE(sc, WMREG_OLD_TDBAH, WM_CDTXADDR_HI(sc, 0));
		CSR_WRITE(sc, WMREG_OLD_TDBAL, WM_CDTXADDR_LO(sc, 0));
		CSR_WRITE(sc, WMREG_OLD_TDLEN, WM_TXDESCSIZE(sc));
		CSR_WRITE(sc, WMREG_OLD_TDH, 0);
		CSR_WRITE(sc, WMREG_OLD_TDT, 0);
		CSR_WRITE(sc, WMREG_OLD_TIDV, 128);
	} else {
		CSR_WRITE(sc, WMREG_TDBAH, WM_CDTXADDR_HI(sc, 0));
		CSR_WRITE(sc, WMREG_TDBAL, WM_CDTXADDR_LO(sc, 0));
		CSR_WRITE(sc, WMREG_TDLEN, WM_TXDESCSIZE(sc));
		CSR_WRITE(sc, WMREG_TDH, 0);

		if ((sc->sc_flags & WM_F_NEWQUEUE) != 0)
			/*
			 * Don't write TDT before TCTL.EN is set.
			 * See the document.
			 */
			CSR_WRITE(sc, WMREG_TXDCTL(0), TXDCTL_QUEUE_ENABLE
			    | TXDCTL_PTHRESH(0) | TXDCTL_HTHRESH(0)
			    | TXDCTL_WTHRESH(0));
		else {
			/* ITR / 4 */
			CSR_WRITE(sc, WMREG_TIDV, sc->sc_itr / 4);
			if (sc->sc_type >= WM_T_82540) {
				/* should be same */
				CSR_WRITE(sc, WMREG_TADV, sc->sc_itr / 4);
			}

			CSR_WRITE(sc, WMREG_TDT, 0);
			CSR_WRITE(sc, WMREG_TXDCTL(0), TXDCTL_PTHRESH(0) |
			    TXDCTL_HTHRESH(0) | TXDCTL_WTHRESH(0));
			CSR_WRITE(sc, WMREG_RXDCTL, RXDCTL_PTHRESH(0) |
			    RXDCTL_HTHRESH(0) | RXDCTL_WTHRESH(1));
		}
	}

	/* Initialize the transmit job descriptors. */
	for (i = 0; i < WM_TXQUEUELEN(sc); i++)
		sc->sc_txsoft[i].txs_mbuf = NULL;
	sc->sc_txsfree = WM_TXQUEUELEN(sc);
	sc->sc_txsnext = 0;
	sc->sc_txsdirty = 0;

	/*
	 * Initialize the receive descriptor and receive job
	 * descriptor rings.
	 */
	if (sc->sc_type < WM_T_82543) {
		CSR_WRITE(sc, WMREG_OLD_RDBAH0, WM_CDRXADDR_HI(sc, 0));
		CSR_WRITE(sc, WMREG_OLD_RDBAL0, WM_CDRXADDR_LO(sc, 0));
		CSR_WRITE(sc, WMREG_OLD_RDLEN0, sizeof(sc->sc_rxdescs));
		CSR_WRITE(sc, WMREG_OLD_RDH0, 0);
		CSR_WRITE(sc, WMREG_OLD_RDT0, 0);
		CSR_WRITE(sc, WMREG_OLD_RDTR0, 28 | RDTR_FPD);

		CSR_WRITE(sc, WMREG_OLD_RDBA1_HI, 0);
		CSR_WRITE(sc, WMREG_OLD_RDBA1_LO, 0);
		CSR_WRITE(sc, WMREG_OLD_RDLEN1, 0);
		CSR_WRITE(sc, WMREG_OLD_RDH1, 0);
		CSR_WRITE(sc, WMREG_OLD_RDT1, 0);
		CSR_WRITE(sc, WMREG_OLD_RDTR1, 0);
	} else {
		CSR_WRITE(sc, WMREG_RDBAH, WM_CDRXADDR_HI(sc, 0));
		CSR_WRITE(sc, WMREG_RDBAL, WM_CDRXADDR_LO(sc, 0));
		CSR_WRITE(sc, WMREG_RDLEN, sizeof(sc->sc_rxdescs));

		if ((sc->sc_flags & WM_F_NEWQUEUE) != 0) {
			if (MCLBYTES & ((1 << SRRCTL_BSIZEPKT_SHIFT) - 1))
				panic("%s: MCLBYTES %d unsupported for i2575 or higher\n", __func__, MCLBYTES);
			CSR_WRITE(sc, WMREG_SRRCTL, SRRCTL_DESCTYPE_LEGACY
			    | (MCLBYTES >> SRRCTL_BSIZEPKT_SHIFT));
			CSR_WRITE(sc, WMREG_RXDCTL, RXDCTL_QUEUE_ENABLE
			    | RXDCTL_PTHRESH(16) | RXDCTL_HTHRESH(8)
			    | RXDCTL_WTHRESH(1));
		} else {
			CSR_WRITE(sc, WMREG_RDH, 0);
			CSR_WRITE(sc, WMREG_RDT, 0);
			/* ITR/4 */
			CSR_WRITE(sc, WMREG_RDTR, (sc->sc_itr / 4) | RDTR_FPD);
			/* MUST be same */
			CSR_WRITE(sc, WMREG_RADV, sc->sc_itr);
		}
	}
	for (i = 0; i < WM_NRXDESC; i++) {
		rxs = &sc->sc_rxsoft[i];
		if (rxs->rxs_mbuf == NULL) {
			if ((error = wm_add_rxbuf(sc, i)) != 0) {
				log(LOG_ERR, "%s: unable to allocate or map "
				    "rx buffer %d, error = %d\n",
				    device_xname(sc->sc_dev), i, error);
				/*
				 * XXX Should attempt to run with fewer receive
				 * XXX buffers instead of just failing.
				 */
				wm_rxdrain(sc);
				goto out;
			}
		} else {
			if ((sc->sc_flags & WM_F_NEWQUEUE) == 0)
				WM_INIT_RXDESC(sc, i);
			/*
			 * For 82575 and newer device, the RX descriptors
			 * must be initialized after the setting of RCTL.EN in
			 * wm_set_filter()
			 */
		}
	}
	sc->sc_rxptr = 0;
	sc->sc_rxdiscard = 0;
	WM_RXCHAIN_RESET(sc);

	/*
	 * Clear out the VLAN table -- we don't use it (yet).
	 */
	CSR_WRITE(sc, WMREG_VET, 0);
	if ((sc->sc_type == WM_T_I350) || (sc->sc_type == WM_T_I354))
		trynum = 10; /* Due to hw errata */
	else
		trynum = 1;
	for (i = 0; i < WM_VLAN_TABSIZE; i++)
		for (j = 0; j < trynum; j++)
			CSR_WRITE(sc, WMREG_VFTA + (i << 2), 0);

	/*
	 * Set up flow-control parameters.
	 *
	 * XXX Values could probably stand some tuning.
	 */
	if ((sc->sc_type != WM_T_ICH8) && (sc->sc_type != WM_T_ICH9)
	    && (sc->sc_type != WM_T_ICH10) && (sc->sc_type != WM_T_PCH)
	    && (sc->sc_type != WM_T_PCH2) && (sc->sc_type != WM_T_PCH_LPT)) {
		CSR_WRITE(sc, WMREG_FCAL, FCAL_CONST);
		CSR_WRITE(sc, WMREG_FCAH, FCAH_CONST);
		CSR_WRITE(sc, WMREG_FCT, ETHERTYPE_FLOWCONTROL);
	}

	sc->sc_fcrtl = FCRTL_DFLT;
	if (sc->sc_type < WM_T_82543) {
		CSR_WRITE(sc, WMREG_OLD_FCRTH, FCRTH_DFLT);
		CSR_WRITE(sc, WMREG_OLD_FCRTL, sc->sc_fcrtl);
	} else {
		CSR_WRITE(sc, WMREG_FCRTH, FCRTH_DFLT);
		CSR_WRITE(sc, WMREG_FCRTL, sc->sc_fcrtl);
	}

	if (sc->sc_type == WM_T_80003)
		CSR_WRITE(sc, WMREG_FCTTV, 0xffff);
	else
		CSR_WRITE(sc, WMREG_FCTTV, FCTTV_DFLT);

	/* Writes the control register. */
	wm_set_vlan(sc);

	if (sc->sc_flags & WM_F_HAS_MII) {
		int val;

		switch (sc->sc_type) {
		case WM_T_80003:
		case WM_T_ICH8:
		case WM_T_ICH9:
		case WM_T_ICH10:
		case WM_T_PCH:
		case WM_T_PCH2:
		case WM_T_PCH_LPT:
			/*
			 * Set the mac to wait the maximum time between each
			 * iteration and increase the max iterations when
			 * polling the phy; this fixes erroneous timeouts at
			 * 10Mbps.
			 */
			wm_kmrn_writereg(sc, KUMCTRLSTA_OFFSET_TIMEOUTS,
			    0xFFFF);
			val = wm_kmrn_readreg(sc,
			    KUMCTRLSTA_OFFSET_INB_PARAM);
			val |= 0x3F;
			wm_kmrn_writereg(sc,
			    KUMCTRLSTA_OFFSET_INB_PARAM, val);
			break;
		default:
			break;
		}

		if (sc->sc_type == WM_T_80003) {
			val = CSR_READ(sc, WMREG_CTRL_EXT);
			val &= ~CTRL_EXT_LINK_MODE_MASK;
			CSR_WRITE(sc, WMREG_CTRL_EXT, val);

			/* Bypass RX and TX FIFO's */
			wm_kmrn_writereg(sc, KUMCTRLSTA_OFFSET_FIFO_CTRL,
			    KUMCTRLSTA_FIFO_CTRL_RX_BYPASS
			    | KUMCTRLSTA_FIFO_CTRL_TX_BYPASS);
			wm_kmrn_writereg(sc, KUMCTRLSTA_OFFSET_INB_CTRL,
			    KUMCTRLSTA_INB_CTRL_DIS_PADDING |
			    KUMCTRLSTA_INB_CTRL_LINK_TMOUT_DFLT);
		}
	}
#if 0
	CSR_WRITE(sc, WMREG_CTRL_EXT, sc->sc_ctrl_ext);
#endif

	/* Set up checksum offload parameters. */
	reg = CSR_READ(sc, WMREG_RXCSUM);
	reg &= ~(RXCSUM_IPOFL | RXCSUM_IPV6OFL | RXCSUM_TUOFL);
	if (ifp->if_capenable & IFCAP_CSUM_IPv4_Rx)
		reg |= RXCSUM_IPOFL;
	if (ifp->if_capenable & (IFCAP_CSUM_TCPv4_Rx | IFCAP_CSUM_UDPv4_Rx))
		reg |= RXCSUM_IPOFL | RXCSUM_TUOFL;
	if (ifp->if_capenable & (IFCAP_CSUM_TCPv6_Rx | IFCAP_CSUM_UDPv6_Rx))
		reg |= RXCSUM_IPV6OFL | RXCSUM_TUOFL;
	CSR_WRITE(sc, WMREG_RXCSUM, reg);

	/* Set up MSI-X */
	if (sc->sc_nintrs > 1) {
		uint32_t ivar;

		if (sc->sc_type == WM_T_82575) {
			/* Interrupt control */
			reg = CSR_READ(sc, WMREG_CTRL_EXT);
			reg |= CTRL_EXT_PBA | CTRL_EXT_EIAME | CTRL_EXT_NSICR;
			CSR_WRITE(sc, WMREG_CTRL_EXT, reg);

			/* TX */
			CSR_WRITE(sc, WMREG_MSIXBM(WM_MSIX_TXINTR_IDX),
			    EITR_TX_QUEUE0);
			/* RX */
			CSR_WRITE(sc, WMREG_MSIXBM(WM_MSIX_RXINTR_IDX),
			    EITR_RX_QUEUE0);
			/* Link status */
			CSR_WRITE(sc, WMREG_MSIXBM(WM_MSIX_LINKINTR_IDX),
			    EITR_OTHER);
		} else if (sc->sc_type == WM_T_82574) {
			/* Interrupt control */
			reg = CSR_READ(sc, WMREG_CTRL_EXT);
			reg |= CTRL_EXT_PBA | CTRL_EXT_EIAME;
			CSR_WRITE(sc, WMREG_CTRL_EXT, reg);

			/* TX, RX and Link status */
			ivar = __SHIFTIN((IVAR_VALID_82574|WM_MSIX_TXINTR_IDX),
			    IVAR_TX_MASK_Q_82574(0));
			ivar |= __SHIFTIN((IVAR_VALID_82574
				| WM_MSIX_RXINTR_IDX),
			    IVAR_RX_MASK_Q_82574(0));
			ivar |=__SHIFTIN((IVAR_VALID_82574|WM_MSIX_LINKINTR_IDX),
			    IVAR_OTHER_MASK);
			CSR_WRITE(sc, WMREG_IVAR, ivar | IVAR_INT_ON_ALL_WB);
		} else {
			/* Interrupt control */
			CSR_WRITE(sc, WMREG_GPIE, GPIE_NSICR
			    | GPIE_MULTI_MSIX | GPIE_EIAME
			    | GPIE_PBA);

			switch (sc->sc_type) {
			case WM_T_82580:
			case WM_T_I350:
			case WM_T_I354:
			case WM_T_I210:
			case WM_T_I211:
				/* TX */
				ivar = CSR_READ(sc, WMREG_IVAR_Q(0));
				ivar &= ~IVAR_TX_MASK_Q(0);
				ivar |= __SHIFTIN(
					(WM_MSIX_TXINTR_IDX | IVAR_VALID),
					IVAR_TX_MASK_Q(0));
				CSR_WRITE(sc, WMREG_IVAR_Q(0), ivar);

				/* RX */
				ivar = CSR_READ(sc, WMREG_IVAR_Q(0));
				ivar &= ~IVAR_RX_MASK_Q(0);
				ivar |= __SHIFTIN(
					(WM_MSIX_RXINTR_IDX | IVAR_VALID),
					IVAR_RX_MASK_Q(0));
				CSR_WRITE(sc, WMREG_IVAR_Q(0), ivar);
				break;
			case WM_T_82576:
				/* TX */
				ivar = CSR_READ(sc, WMREG_IVAR_Q_82576(0));
				ivar &= ~IVAR_TX_MASK_Q_82576(0);
				ivar |= __SHIFTIN(
					(WM_MSIX_TXINTR_IDX | IVAR_VALID),
					IVAR_TX_MASK_Q_82576(0));
				CSR_WRITE(sc, WMREG_IVAR_Q_82576(0), ivar);

				/* RX */
				ivar = CSR_READ(sc, WMREG_IVAR_Q_82576(0));
				ivar &= ~IVAR_RX_MASK_Q_82576(0);
				ivar |= __SHIFTIN(
					(WM_MSIX_RXINTR_IDX | IVAR_VALID),
					IVAR_RX_MASK_Q_82576(0));
				CSR_WRITE(sc, WMREG_IVAR_Q_82576(0), ivar);
				break;
			default:
				break;
			}

			/* Link status */
			ivar = __SHIFTIN((WM_MSIX_LINKINTR_IDX | IVAR_VALID),
			    IVAR_MISC_OTHER);
			CSR_WRITE(sc, WMREG_IVAR_MISC, ivar);
		}
	}

	/* Set up the interrupt registers. */
	CSR_WRITE(sc, WMREG_IMC, 0xffffffffU);
	sc->sc_icr = ICR_TXDW | ICR_LSC | ICR_RXSEQ | ICR_RXDMT0 |
	    ICR_RXO | ICR_RXT0;
	if (sc->sc_nintrs > 1) {
		uint32_t mask;
		switch (sc->sc_type) {
		case WM_T_82574:
			CSR_WRITE(sc, WMREG_EIAC_82574,
			    WMREG_EIAC_82574_MSIX_MASK);
			sc->sc_icr |= WMREG_EIAC_82574_MSIX_MASK;
			CSR_WRITE(sc, WMREG_IMS, sc->sc_icr);
			break;
		default:
			if (sc->sc_type == WM_T_82575)
				mask = EITR_RX_QUEUE0 |EITR_TX_QUEUE0
				    | EITR_OTHER;
			else
				mask = (1 << WM_MSIX_RXINTR_IDX)
				    | (1 << WM_MSIX_TXINTR_IDX)
				    | (1 << WM_MSIX_LINKINTR_IDX);
			CSR_WRITE(sc, WMREG_EIAC, mask);
			CSR_WRITE(sc, WMREG_EIAM, mask);
			CSR_WRITE(sc, WMREG_EIMS, mask);
			CSR_WRITE(sc, WMREG_IMS, ICR_LSC);
			break;
		}
	} else
		CSR_WRITE(sc, WMREG_IMS, sc->sc_icr);

	if ((sc->sc_type == WM_T_ICH8) || (sc->sc_type == WM_T_ICH9)
	    || (sc->sc_type == WM_T_ICH10) || (sc->sc_type == WM_T_PCH)
	    || (sc->sc_type == WM_T_PCH2) || (sc->sc_type == WM_T_PCH_LPT)) {
		reg = CSR_READ(sc, WMREG_KABGTXD);
		reg |= KABGTXD_BGSQLBIAS;
		CSR_WRITE(sc, WMREG_KABGTXD, reg);
	}

	/* Set up the inter-packet gap. */
	CSR_WRITE(sc, WMREG_TIPG, sc->sc_tipg);

	if (sc->sc_type >= WM_T_82543) {
		/*
		 * XXX 82574 has both ITR and EITR. SET EITR when we use
		 * the multi queue function with MSI-X.
		 */
		if ((sc->sc_flags & WM_F_NEWQUEUE) != 0) {
			if (sc->sc_nintrs > 1) {
				CSR_WRITE(sc, WMREG_EITR(WM_MSIX_RXINTR_IDX),
				    sc->sc_itr);
				CSR_WRITE(sc, WMREG_EITR(WM_MSIX_TXINTR_IDX),
				    sc->sc_itr);
				/*
				 * Link interrupts occur much less than TX
				 * interrupts and RX interrupts. So, we don't
				 * tune EINTR(WM_MSIX_LINKINTR_IDX) value like
				 * FreeBSD's if_igb.
				 */
			} else
				CSR_WRITE(sc, WMREG_EITR(0), sc->sc_itr);
		} else
			CSR_WRITE(sc, WMREG_ITR, sc->sc_itr);
	}

	/* Set the VLAN ethernetype. */
	CSR_WRITE(sc, WMREG_VET, ETHERTYPE_VLAN);

	/*
	 * Set up the transmit control register; we start out with
	 * a collision distance suitable for FDX, but update it whe
	 * we resolve the media type.
	 */
	sc->sc_tctl = TCTL_EN | TCTL_PSP | TCTL_RTLC
	    | TCTL_CT(TX_COLLISION_THRESHOLD)
	    | TCTL_COLD(TX_COLLISION_DISTANCE_FDX);
	if (sc->sc_type >= WM_T_82571)
		sc->sc_tctl |= TCTL_MULR;
	CSR_WRITE(sc, WMREG_TCTL, sc->sc_tctl);

	if ((sc->sc_flags & WM_F_NEWQUEUE) != 0) {
		/* Write TDT after TCTL.EN is set. See the document. */
		CSR_WRITE(sc, WMREG_TDT, 0);
	}

	if (sc->sc_type == WM_T_80003) {
		reg = CSR_READ(sc, WMREG_TCTL_EXT);
		reg &= ~TCTL_EXT_GCEX_MASK;
		reg |= DEFAULT_80003ES2LAN_TCTL_EXT_GCEX;
		CSR_WRITE(sc, WMREG_TCTL_EXT, reg);
	}

	/* Set the media. */
	if ((error = mii_ifmedia_change(&sc->sc_mii)) != 0)
		goto out;

	/* Configure for OS presence */
	wm_init_manageability(sc);

	/*
	 * Set up the receive control register; we actually program
	 * the register when we set the receive filter.  Use multicast
	 * address offset type 0.
	 *
	 * Only the i82544 has the ability to strip the incoming
	 * CRC, so we don't enable that feature.
	 */
	sc->sc_mchash_type = 0;
	sc->sc_rctl = RCTL_EN | RCTL_LBM_NONE | RCTL_RDMTS_1_2 | RCTL_DPF
	    | RCTL_MO(sc->sc_mchash_type);

	/*
	 * The I350 has a bug where it always strips the CRC whether
	 * asked to or not. So ask for stripped CRC here and cope in rxeof
	 */
	if ((sc->sc_type == WM_T_I350) || (sc->sc_type == WM_T_I354)
	    || (sc->sc_type == WM_T_I210))
		sc->sc_rctl |= RCTL_SECRC;

	if (((sc->sc_ethercom.ec_capabilities & ETHERCAP_JUMBO_MTU) != 0)
	    && (ifp->if_mtu > ETHERMTU)) {
		sc->sc_rctl |= RCTL_LPE;
		if ((sc->sc_flags & WM_F_NEWQUEUE) != 0)
			CSR_WRITE(sc, WMREG_RLPML, ETHER_MAX_LEN_JUMBO);
	}

	if (MCLBYTES == 2048) {
		sc->sc_rctl |= RCTL_2k;
	} else {
		if (sc->sc_type >= WM_T_82543) {
			switch (MCLBYTES) {
			case 4096:
				sc->sc_rctl |= RCTL_BSEX | RCTL_BSEX_4k;
				break;
			case 8192:
				sc->sc_rctl |= RCTL_BSEX | RCTL_BSEX_8k;
				break;
			case 16384:
				sc->sc_rctl |= RCTL_BSEX | RCTL_BSEX_16k;
				break;
			default:
				panic("wm_init: MCLBYTES %d unsupported",
				    MCLBYTES);
				break;
			}
		} else panic("wm_init: i82542 requires MCLBYTES = 2048");
	}

	/* Set the receive filter. */
	wm_set_filter(sc);

	/* Enable ECC */
	switch (sc->sc_type) {
	case WM_T_82571:
		reg = CSR_READ(sc, WMREG_PBA_ECC);
		reg |= PBA_ECC_CORR_EN;
		CSR_WRITE(sc, WMREG_PBA_ECC, reg);
		break;
	case WM_T_PCH_LPT:
		reg = CSR_READ(sc, WMREG_PBECCSTS);
		reg |= PBECCSTS_UNCORR_ECC_ENABLE;
		CSR_WRITE(sc, WMREG_PBECCSTS, reg);

		reg = CSR_READ(sc, WMREG_CTRL);
		reg |= CTRL_MEHE;
		CSR_WRITE(sc, WMREG_CTRL, reg);
		break;
	default:
		break;
	}

	/* On 575 and later set RDT only if RX enabled */
	if ((sc->sc_flags & WM_F_NEWQUEUE) != 0)
		for (i = 0; i < WM_NRXDESC; i++)
			WM_INIT_RXDESC(sc, i);

	sc->sc_stopping = false;

	/* Start the one second link check clock. */
	callout_reset(&sc->sc_tick_ch, hz, wm_tick, sc);

	/* ...all done! */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

 out:
	sc->sc_if_flags = ifp->if_flags;
	if (error)
		log(LOG_ERR, "%s: interface not running\n",
		    device_xname(sc->sc_dev));
	return error;
}

/*
 * wm_stop:		[ifnet interface function]
 *
 *	Stop transmission on the interface.
 */
static void
wm_stop(struct ifnet *ifp, int disable)
{
	struct wm_softc *sc = ifp->if_softc;

	WM_BOTH_LOCK(sc);
	wm_stop_locked(ifp, disable);
	WM_BOTH_UNLOCK(sc);
}

static void
wm_stop_locked(struct ifnet *ifp, int disable)
{
	struct wm_softc *sc = ifp->if_softc;
	struct wm_txsoft *txs;
	int i;

	KASSERT(WM_BOTH_LOCKED(sc));

	sc->sc_stopping = true;

	/* Stop the one second clock. */
	callout_stop(&sc->sc_tick_ch);

	/* Stop the 82547 Tx FIFO stall check timer. */
	if (sc->sc_type == WM_T_82547)
		callout_stop(&sc->sc_txfifo_ch);

	if (sc->sc_flags & WM_F_HAS_MII) {
		/* Down the MII. */
		mii_down(&sc->sc_mii);
	} else {
#if 0
		/* Should we clear PHY's status properly? */
		wm_reset(sc);
#endif
	}

	/* Stop the transmit and receive processes. */
	CSR_WRITE(sc, WMREG_TCTL, 0);
	CSR_WRITE(sc, WMREG_RCTL, 0);
	sc->sc_rctl &= ~RCTL_EN;

	/*
	 * Clear the interrupt mask to ensure the device cannot assert its
	 * interrupt line.
	 * Clear sc->sc_icr to ensure wm_intr_legacy() makes no attempt to
	 * service any currently pending or shared interrupt.
	 */
	CSR_WRITE(sc, WMREG_IMC, 0xffffffffU);
	sc->sc_icr = 0;
	if (sc->sc_nintrs > 1) {
		if (sc->sc_type != WM_T_82574) {
			CSR_WRITE(sc, WMREG_EIMC, 0xffffffffU);
			CSR_WRITE(sc, WMREG_EIAC, 0);
		} else
			CSR_WRITE(sc, WMREG_EIAC_82574, 0);
	}

	/* Release any queued transmit buffers. */
	for (i = 0; i < WM_TXQUEUELEN(sc); i++) {
		txs = &sc->sc_txsoft[i];
		if (txs->txs_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat, txs->txs_dmamap);
			m_freem(txs->txs_mbuf);
			txs->txs_mbuf = NULL;
		}
	}

	/* Mark the interface as down and cancel the watchdog timer. */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	if (disable)
		wm_rxdrain(sc);

#if 0 /* notyet */
	if (sc->sc_type >= WM_T_82544)
		CSR_WRITE(sc, WMREG_WUC, 0);
#endif
}

/*
 * wm_tx_offload:
 *
 *	Set up TCP/IP checksumming parameters for the
 *	specified packet.
 */
static int
wm_tx_offload(struct wm_softc *sc, struct wm_txsoft *txs, uint32_t *cmdp,
    uint8_t *fieldsp)
{
	struct mbuf *m0 = txs->txs_mbuf;
	struct livengood_tcpip_ctxdesc *t;
	uint32_t ipcs, tucs, cmd, cmdlen, seg;
	uint32_t ipcse;
	struct ether_header *eh;
	int offset, iphl;
	uint8_t fields;

	/*
	 * XXX It would be nice if the mbuf pkthdr had offset
	 * fields for the protocol headers.
	 */

	eh = mtod(m0, struct ether_header *);
	switch (htons(eh->ether_type)) {
	case ETHERTYPE_IP:
	case ETHERTYPE_IPV6:
		offset = ETHER_HDR_LEN;
		break;

	case ETHERTYPE_VLAN:
		offset = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
		break;

	default:
		/*
		 * Don't support this protocol or encapsulation.
		 */
		*fieldsp = 0;
		*cmdp = 0;
		return 0;
	}

	if ((m0->m_pkthdr.csum_flags &
	    (M_CSUM_TSOv4|M_CSUM_UDPv4|M_CSUM_TCPv4)) != 0) {
		iphl = M_CSUM_DATA_IPv4_IPHL(m0->m_pkthdr.csum_data);
	} else {
		iphl = M_CSUM_DATA_IPv6_HL(m0->m_pkthdr.csum_data);
	}
	ipcse = offset + iphl - 1;

	cmd = WTX_CMD_DEXT | WTX_DTYP_D;
	cmdlen = WTX_CMD_DEXT | WTX_DTYP_C | WTX_CMD_IDE;
	seg = 0;
	fields = 0;

	if ((m0->m_pkthdr.csum_flags & (M_CSUM_TSOv4 | M_CSUM_TSOv6)) != 0) {
		int hlen = offset + iphl;
		bool v4 = (m0->m_pkthdr.csum_flags & M_CSUM_TSOv4) != 0;

		if (__predict_false(m0->m_len <
				    (hlen + sizeof(struct tcphdr)))) {
			/*
			 * TCP/IP headers are not in the first mbuf; we need
			 * to do this the slow and painful way.  Let's just
			 * hope this doesn't happen very often.
			 */
			struct tcphdr th;

			WM_EVCNT_INCR(&sc->sc_ev_txtsopain);

			m_copydata(m0, hlen, sizeof(th), &th);
			if (v4) {
				struct ip ip;

				m_copydata(m0, offset, sizeof(ip), &ip);
				ip.ip_len = 0;
				m_copyback(m0,
				    offset + offsetof(struct ip, ip_len),
				    sizeof(ip.ip_len), &ip.ip_len);
				th.th_sum = in_cksum_phdr(ip.ip_src.s_addr,
				    ip.ip_dst.s_addr, htons(IPPROTO_TCP));
			} else {
				struct ip6_hdr ip6;

				m_copydata(m0, offset, sizeof(ip6), &ip6);
				ip6.ip6_plen = 0;
				m_copyback(m0,
				    offset + offsetof(struct ip6_hdr, ip6_plen),
				    sizeof(ip6.ip6_plen), &ip6.ip6_plen);
				th.th_sum = in6_cksum_phdr(&ip6.ip6_src,
				    &ip6.ip6_dst, 0, htonl(IPPROTO_TCP));
			}
			m_copyback(m0, hlen + offsetof(struct tcphdr, th_sum),
			    sizeof(th.th_sum), &th.th_sum);

			hlen += th.th_off << 2;
		} else {
			/*
			 * TCP/IP headers are in the first mbuf; we can do
			 * this the easy way.
			 */
			struct tcphdr *th;

			if (v4) {
				struct ip *ip =
				    (void *)(mtod(m0, char *) + offset);
				th = (void *)(mtod(m0, char *) + hlen);

				ip->ip_len = 0;
				th->th_sum = in_cksum_phdr(ip->ip_src.s_addr,
				    ip->ip_dst.s_addr, htons(IPPROTO_TCP));
			} else {
				struct ip6_hdr *ip6 =
				    (void *)(mtod(m0, char *) + offset);
				th = (void *)(mtod(m0, char *) + hlen);

				ip6->ip6_plen = 0;
				th->th_sum = in6_cksum_phdr(&ip6->ip6_src,
				    &ip6->ip6_dst, 0, htonl(IPPROTO_TCP));
			}
			hlen += th->th_off << 2;
		}

		if (v4) {
			WM_EVCNT_INCR(&sc->sc_ev_txtso);
			cmdlen |= WTX_TCPIP_CMD_IP;
		} else {
			WM_EVCNT_INCR(&sc->sc_ev_txtso6);
			ipcse = 0;
		}
		cmd |= WTX_TCPIP_CMD_TSE;
		cmdlen |= WTX_TCPIP_CMD_TSE |
		    WTX_TCPIP_CMD_TCP | (m0->m_pkthdr.len - hlen);
		seg = WTX_TCPIP_SEG_HDRLEN(hlen) |
		    WTX_TCPIP_SEG_MSS(m0->m_pkthdr.segsz);
	}

	/*
	 * NOTE: Even if we're not using the IP or TCP/UDP checksum
	 * offload feature, if we load the context descriptor, we
	 * MUST provide valid values for IPCSS and TUCSS fields.
	 */

	ipcs = WTX_TCPIP_IPCSS(offset) |
	    WTX_TCPIP_IPCSO(offset + offsetof(struct ip, ip_sum)) |
	    WTX_TCPIP_IPCSE(ipcse);
	if (m0->m_pkthdr.csum_flags & (M_CSUM_IPv4|M_CSUM_TSOv4)) {
		WM_EVCNT_INCR(&sc->sc_ev_txipsum);
		fields |= WTX_IXSM;
	}

	offset += iphl;

	if (m0->m_pkthdr.csum_flags &
	    (M_CSUM_TCPv4|M_CSUM_UDPv4|M_CSUM_TSOv4)) {
		WM_EVCNT_INCR(&sc->sc_ev_txtusum);
		fields |= WTX_TXSM;
		tucs = WTX_TCPIP_TUCSS(offset) |
		    WTX_TCPIP_TUCSO(offset +
		    M_CSUM_DATA_IPv4_OFFSET(m0->m_pkthdr.csum_data)) |
		    WTX_TCPIP_TUCSE(0) /* rest of packet */;
	} else if ((m0->m_pkthdr.csum_flags &
	    (M_CSUM_TCPv6|M_CSUM_UDPv6|M_CSUM_TSOv6)) != 0) {
		WM_EVCNT_INCR(&sc->sc_ev_txtusum6);
		fields |= WTX_TXSM;
		tucs = WTX_TCPIP_TUCSS(offset) |
		    WTX_TCPIP_TUCSO(offset +
		    M_CSUM_DATA_IPv6_OFFSET(m0->m_pkthdr.csum_data)) |
		    WTX_TCPIP_TUCSE(0) /* rest of packet */;
	} else {
		/* Just initialize it to a valid TCP context. */
		tucs = WTX_TCPIP_TUCSS(offset) |
		    WTX_TCPIP_TUCSO(offset + offsetof(struct tcphdr, th_sum)) |
		    WTX_TCPIP_TUCSE(0) /* rest of packet */;
	}

	/* Fill in the context descriptor. */
	t = (struct livengood_tcpip_ctxdesc *)
	    &sc->sc_txdescs[sc->sc_txnext];
	t->tcpip_ipcs = htole32(ipcs);
	t->tcpip_tucs = htole32(tucs);
	t->tcpip_cmdlen = htole32(cmdlen);
	t->tcpip_seg = htole32(seg);
	WM_CDTXSYNC(sc, sc->sc_txnext, 1, BUS_DMASYNC_PREWRITE);

	sc->sc_txnext = WM_NEXTTX(sc, sc->sc_txnext);
	txs->txs_ndesc++;

	*cmdp = cmd;
	*fieldsp = fields;

	return 0;
}

static void
wm_dump_mbuf_chain(struct wm_softc *sc, struct mbuf *m0)
{
	struct mbuf *m;
	int i;

	log(LOG_DEBUG, "%s: mbuf chain:\n", device_xname(sc->sc_dev));
	for (m = m0, i = 0; m != NULL; m = m->m_next, i++)
		log(LOG_DEBUG, "%s:\tm_data = %p, m_len = %d, "
		    "m_flags = 0x%08x\n", device_xname(sc->sc_dev),
		    m->m_data, m->m_len, m->m_flags);
	log(LOG_DEBUG, "%s:\t%d mbuf%s in chain\n", device_xname(sc->sc_dev),
	    i, i == 1 ? "" : "s");
}

/*
 * wm_82547_txfifo_stall:
 *
 *	Callout used to wait for the 82547 Tx FIFO to drain,
 *	reset the FIFO pointers, and restart packet transmission.
 */
static void
wm_82547_txfifo_stall(void *arg)
{
	struct wm_softc *sc = arg;
#ifndef WM_MPSAFE
	int s;

	s = splnet();
#endif
	WM_TX_LOCK(sc);

	if (sc->sc_stopping)
		goto out;

	if (sc->sc_txfifo_stall) {
		if (CSR_READ(sc, WMREG_TDT) == CSR_READ(sc, WMREG_TDH) &&
		    CSR_READ(sc, WMREG_TDFT) == CSR_READ(sc, WMREG_TDFH) &&
		    CSR_READ(sc, WMREG_TDFTS) == CSR_READ(sc, WMREG_TDFHS)) {
			/*
			 * Packets have drained.  Stop transmitter, reset
			 * FIFO pointers, restart transmitter, and kick
			 * the packet queue.
			 */
			uint32_t tctl = CSR_READ(sc, WMREG_TCTL);
			CSR_WRITE(sc, WMREG_TCTL, tctl & ~TCTL_EN);
			CSR_WRITE(sc, WMREG_TDFT, sc->sc_txfifo_addr);
			CSR_WRITE(sc, WMREG_TDFH, sc->sc_txfifo_addr);
			CSR_WRITE(sc, WMREG_TDFTS, sc->sc_txfifo_addr);
			CSR_WRITE(sc, WMREG_TDFHS, sc->sc_txfifo_addr);
			CSR_WRITE(sc, WMREG_TCTL, tctl);
			CSR_WRITE_FLUSH(sc);

			sc->sc_txfifo_head = 0;
			sc->sc_txfifo_stall = 0;
			wm_start_locked(&sc->sc_ethercom.ec_if);
		} else {
			/*
			 * Still waiting for packets to drain; try again in
			 * another tick.
			 */
			callout_schedule(&sc->sc_txfifo_ch, 1);
		}
	}

out:
	WM_TX_UNLOCK(sc);
#ifndef WM_MPSAFE
	splx(s);
#endif
}

/*
 * wm_82547_txfifo_bugchk:
 *
 *	Check for bug condition in the 82547 Tx FIFO.  We need to
 *	prevent enqueueing a packet that would wrap around the end
 *	if the Tx FIFO ring buffer, otherwise the chip will croak.
 *
 *	We do this by checking the amount of space before the end
 *	of the Tx FIFO buffer.  If the packet will not fit, we "stall"
 *	the Tx FIFO, wait for all remaining packets to drain, reset
 *	the internal FIFO pointers to the beginning, and restart
 *	transmission on the interface.
 */
#define	WM_FIFO_HDR		0x10
#define	WM_82547_PAD_LEN	0x3e0
static int
wm_82547_txfifo_bugchk(struct wm_softc *sc, struct mbuf *m0)
{
	int space = sc->sc_txfifo_size - sc->sc_txfifo_head;
	int len = roundup(m0->m_pkthdr.len + WM_FIFO_HDR, WM_FIFO_HDR);

	/* Just return if already stalled. */
	if (sc->sc_txfifo_stall)
		return 1;

	if (sc->sc_mii.mii_media_active & IFM_FDX) {
		/* Stall only occurs in half-duplex mode. */
		goto send_packet;
	}

	if (len >= WM_82547_PAD_LEN + space) {
		sc->sc_txfifo_stall = 1;
		callout_schedule(&sc->sc_txfifo_ch, 1);
		return 1;
	}

 send_packet:
	sc->sc_txfifo_head += len;
	if (sc->sc_txfifo_head >= sc->sc_txfifo_size)
		sc->sc_txfifo_head -= sc->sc_txfifo_size;

	return 0;
}

/*
 * wm_start:		[ifnet interface function]
 *
 *	Start packet transmission on the interface.
 */
static void
wm_start(struct ifnet *ifp)
{
	struct wm_softc *sc = ifp->if_softc;

	WM_TX_LOCK(sc);
	if (!sc->sc_stopping)
		wm_start_locked(ifp);
	WM_TX_UNLOCK(sc);
}

static void
wm_start_locked(struct ifnet *ifp)
{
	struct wm_softc *sc = ifp->if_softc;
	struct mbuf *m0;
	struct m_tag *mtag;
	struct wm_txsoft *txs;
	bus_dmamap_t dmamap;
	int error, nexttx, lasttx = -1, ofree, seg, segs_needed, use_tso;
	bus_addr_t curaddr;
	bus_size_t seglen, curlen;
	uint32_t cksumcmd;
	uint8_t cksumfields;

	KASSERT(WM_TX_LOCKED(sc));

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

	/* Remember the previous number of free descriptors. */
	ofree = sc->sc_txfree;

	/*
	 * Loop through the send queue, setting up transmit descriptors
	 * until we drain the queue, or use up all available transmit
	 * descriptors.
	 */
	for (;;) {
		m0 = NULL;

		/* Get a work queue entry. */
		if (sc->sc_txsfree < WM_TXQUEUE_GC(sc)) {
			wm_txeof(sc);
			if (sc->sc_txsfree == 0) {
				DPRINTF(WM_DEBUG_TX,
				    ("%s: TX: no free job descriptors\n",
					device_xname(sc->sc_dev)));
				WM_EVCNT_INCR(&sc->sc_ev_txsstall);
				break;
			}
		}

		/* Grab a packet off the queue. */
		IFQ_DEQUEUE(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		DPRINTF(WM_DEBUG_TX,
		    ("%s: TX: have packet to transmit: %p\n",
		    device_xname(sc->sc_dev), m0));

		txs = &sc->sc_txsoft[sc->sc_txsnext];
		dmamap = txs->txs_dmamap;

		use_tso = (m0->m_pkthdr.csum_flags &
		    (M_CSUM_TSOv4 | M_CSUM_TSOv6)) != 0;

		/*
		 * So says the Linux driver:
		 * The controller does a simple calculation to make sure
		 * there is enough room in the FIFO before initiating the
		 * DMA for each buffer.  The calc is:
		 *	4 = ceil(buffer len / MSS)
		 * To make sure we don't overrun the FIFO, adjust the max
		 * buffer len if the MSS drops.
		 */
		dmamap->dm_maxsegsz =
		    (use_tso && (m0->m_pkthdr.segsz << 2) < WTX_MAX_LEN)
		    ? m0->m_pkthdr.segsz << 2
		    : WTX_MAX_LEN;

		/*
		 * Load the DMA map.  If this fails, the packet either
		 * didn't fit in the allotted number of segments, or we
		 * were short on resources.  For the too-many-segments
		 * case, we simply report an error and drop the packet,
		 * since we can't sanely copy a jumbo packet to a single
		 * buffer.
		 */
		error = bus_dmamap_load_mbuf(sc->sc_dmat, dmamap, m0,
		    BUS_DMA_WRITE|BUS_DMA_NOWAIT);
		if (error) {
			if (error == EFBIG) {
				WM_EVCNT_INCR(&sc->sc_ev_txdrop);
				log(LOG_ERR, "%s: Tx packet consumes too many "
				    "DMA segments, dropping...\n",
				    device_xname(sc->sc_dev));
				wm_dump_mbuf_chain(sc, m0);
				m_freem(m0);
				continue;
			}
			/*  Short on resources, just stop for now. */
			DPRINTF(WM_DEBUG_TX,
			    ("%s: TX: dmamap load failed: %d\n",
			    device_xname(sc->sc_dev), error));
			break;
		}

		segs_needed = dmamap->dm_nsegs;
		if (use_tso) {
			/* For sentinel descriptor; see below. */
			segs_needed++;
		}

		/*
		 * Ensure we have enough descriptors free to describe
		 * the packet.  Note, we always reserve one descriptor
		 * at the end of the ring due to the semantics of the
		 * TDT register, plus one more in the event we need
		 * to load offload context.
		 */
		if (segs_needed > sc->sc_txfree - 2) {
			/*
			 * Not enough free descriptors to transmit this
			 * packet.  We haven't committed anything yet,
			 * so just unload the DMA map, put the packet
			 * pack on the queue, and punt.  Notify the upper
			 * layer that there are no more slots left.
			 */
			DPRINTF(WM_DEBUG_TX,
			    ("%s: TX: need %d (%d) descriptors, have %d\n",
			    device_xname(sc->sc_dev), dmamap->dm_nsegs,
			    segs_needed, sc->sc_txfree - 1));
			ifp->if_flags |= IFF_OACTIVE;
			bus_dmamap_unload(sc->sc_dmat, dmamap);
			WM_EVCNT_INCR(&sc->sc_ev_txdstall);
			break;
		}

		/*
		 * Check for 82547 Tx FIFO bug.  We need to do this
		 * once we know we can transmit the packet, since we
		 * do some internal FIFO space accounting here.
		 */
		if (sc->sc_type == WM_T_82547 &&
		    wm_82547_txfifo_bugchk(sc, m0)) {
			DPRINTF(WM_DEBUG_TX,
			    ("%s: TX: 82547 Tx FIFO bug detected\n",
			    device_xname(sc->sc_dev)));
			ifp->if_flags |= IFF_OACTIVE;
			bus_dmamap_unload(sc->sc_dmat, dmamap);
			WM_EVCNT_INCR(&sc->sc_ev_txfifo_stall);
			break;
		}

		/* WE ARE NOW COMMITTED TO TRANSMITTING THE PACKET. */

		DPRINTF(WM_DEBUG_TX,
		    ("%s: TX: packet has %d (%d) DMA segments\n",
		    device_xname(sc->sc_dev), dmamap->dm_nsegs, segs_needed));

		WM_EVCNT_INCR(&sc->sc_ev_txseg[dmamap->dm_nsegs - 1]);

		/*
		 * Store a pointer to the packet so that we can free it
		 * later.
		 *
		 * Initially, we consider the number of descriptors the
		 * packet uses the number of DMA segments.  This may be
		 * incremented by 1 if we do checksum offload (a descriptor
		 * is used to set the checksum context).
		 */
		txs->txs_mbuf = m0;
		txs->txs_firstdesc = sc->sc_txnext;
		txs->txs_ndesc = segs_needed;

		/* Set up offload parameters for this packet. */
		if (m0->m_pkthdr.csum_flags &
		    (M_CSUM_TSOv4|M_CSUM_TSOv6|
		    M_CSUM_IPv4|M_CSUM_TCPv4|M_CSUM_UDPv4|
		    M_CSUM_TCPv6|M_CSUM_UDPv6)) {
			if (wm_tx_offload(sc, txs, &cksumcmd,
					  &cksumfields) != 0) {
				/* Error message already displayed. */
				bus_dmamap_unload(sc->sc_dmat, dmamap);
				continue;
			}
		} else {
			cksumcmd = 0;
			cksumfields = 0;
		}

		cksumcmd |= WTX_CMD_IDE | WTX_CMD_IFCS;

		/* Sync the DMA map. */
		bus_dmamap_sync(sc->sc_dmat, dmamap, 0, dmamap->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		/* Initialize the transmit descriptor. */
		for (nexttx = sc->sc_txnext, seg = 0;
		     seg < dmamap->dm_nsegs; seg++) {
			for (seglen = dmamap->dm_segs[seg].ds_len,
			     curaddr = dmamap->dm_segs[seg].ds_addr;
			     seglen != 0;
			     curaddr += curlen, seglen -= curlen,
			     nexttx = WM_NEXTTX(sc, nexttx)) {
				curlen = seglen;

				/*
				 * So says the Linux driver:
				 * Work around for premature descriptor
				 * write-backs in TSO mode.  Append a
				 * 4-byte sentinel descriptor.
				 */
				if (use_tso &&
				    seg == dmamap->dm_nsegs - 1 &&
				    curlen > 8)
					curlen -= 4;

				wm_set_dma_addr(
				    &sc->sc_txdescs[nexttx].wtx_addr,
				    curaddr);
				sc->sc_txdescs[nexttx].wtx_cmdlen =
				    htole32(cksumcmd | curlen);
				sc->sc_txdescs[nexttx].wtx_fields.wtxu_status =
				    0;
				sc->sc_txdescs[nexttx].wtx_fields.wtxu_options =
				    cksumfields;
				sc->sc_txdescs[nexttx].wtx_fields.wtxu_vlan = 0;
				lasttx = nexttx;

				DPRINTF(WM_DEBUG_TX,
				    ("%s: TX: desc %d: low %#" PRIx64 ", "
				     "len %#04zx\n",
				    device_xname(sc->sc_dev), nexttx,
				    (uint64_t)curaddr, curlen));
			}
		}

		KASSERT(lasttx != -1);

		/*
		 * Set up the command byte on the last descriptor of
		 * the packet.  If we're in the interrupt delay window,
		 * delay the interrupt.
		 */
		sc->sc_txdescs[lasttx].wtx_cmdlen |=
		    htole32(WTX_CMD_EOP | WTX_CMD_RS);

		/*
		 * If VLANs are enabled and the packet has a VLAN tag, set
		 * up the descriptor to encapsulate the packet for us.
		 *
		 * This is only valid on the last descriptor of the packet.
		 */
		if ((mtag = VLAN_OUTPUT_TAG(&sc->sc_ethercom, m0)) != NULL) {
			sc->sc_txdescs[lasttx].wtx_cmdlen |=
			    htole32(WTX_CMD_VLE);
			sc->sc_txdescs[lasttx].wtx_fields.wtxu_vlan
			    = htole16(VLAN_TAG_VALUE(mtag) & 0xffff);
		}

		txs->txs_lastdesc = lasttx;

		DPRINTF(WM_DEBUG_TX,
		    ("%s: TX: desc %d: cmdlen 0x%08x\n",
		    device_xname(sc->sc_dev),
		    lasttx, le32toh(sc->sc_txdescs[lasttx].wtx_cmdlen)));

		/* Sync the descriptors we're using. */
		WM_CDTXSYNC(sc, sc->sc_txnext, txs->txs_ndesc,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/* Give the packet to the chip. */
		CSR_WRITE(sc, sc->sc_tdt_reg, nexttx);

		DPRINTF(WM_DEBUG_TX,
		    ("%s: TX: TDT -> %d\n", device_xname(sc->sc_dev), nexttx));

		DPRINTF(WM_DEBUG_TX,
		    ("%s: TX: finished transmitting packet, job %d\n",
		    device_xname(sc->sc_dev), sc->sc_txsnext));

		/* Advance the tx pointer. */
		sc->sc_txfree -= txs->txs_ndesc;
		sc->sc_txnext = nexttx;

		sc->sc_txsfree--;
		sc->sc_txsnext = WM_NEXTTXS(sc, sc->sc_txsnext);

		/* Pass the packet to any BPF listeners. */
		bpf_mtap(ifp, m0);
	}

	if (m0 != NULL) {
		ifp->if_flags |= IFF_OACTIVE;
		WM_EVCNT_INCR(&sc->sc_ev_txdrop);
		DPRINTF(WM_DEBUG_TX, ("%s: TX: error after IFQ_DEQUEUE\n", __func__));
		m_freem(m0);
	}

	if (sc->sc_txsfree == 0 || sc->sc_txfree <= 2) {
		/* No more slots; notify upper layer. */
		ifp->if_flags |= IFF_OACTIVE;
	}

	if (sc->sc_txfree != ofree) {
		/* Set a watchdog timer in case the chip flakes out. */
		ifp->if_timer = 5;
	}
}

/*
 * wm_nq_tx_offload:
 *
 *	Set up TCP/IP checksumming parameters for the
 *	specified packet, for NEWQUEUE devices
 */
static int
wm_nq_tx_offload(struct wm_softc *sc, struct wm_txsoft *txs,
    uint32_t *cmdlenp, uint32_t *fieldsp, bool *do_csum)
{
	struct mbuf *m0 = txs->txs_mbuf;
	struct m_tag *mtag;
	uint32_t vl_len, mssidx, cmdc;
	struct ether_header *eh;
	int offset, iphl;

	/*
	 * XXX It would be nice if the mbuf pkthdr had offset
	 * fields for the protocol headers.
	 */
	*cmdlenp = 0;
	*fieldsp = 0;

	eh = mtod(m0, struct ether_header *);
	switch (htons(eh->ether_type)) {
	case ETHERTYPE_IP:
	case ETHERTYPE_IPV6:
		offset = ETHER_HDR_LEN;
		break;

	case ETHERTYPE_VLAN:
		offset = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
		break;

	default:
		/* Don't support this protocol or encapsulation. */
		*do_csum = false;
		return 0;
	}
	*do_csum = true;
	*cmdlenp = NQTX_DTYP_D | NQTX_CMD_DEXT | NQTX_CMD_IFCS;
	cmdc = NQTX_DTYP_C | NQTX_CMD_DEXT;

	vl_len = (offset << NQTXC_VLLEN_MACLEN_SHIFT);
	KASSERT((offset & ~NQTXC_VLLEN_MACLEN_MASK) == 0);

	if ((m0->m_pkthdr.csum_flags &
	    (M_CSUM_TSOv4|M_CSUM_UDPv4|M_CSUM_TCPv4|M_CSUM_IPv4)) != 0) {
		iphl = M_CSUM_DATA_IPv4_IPHL(m0->m_pkthdr.csum_data);
	} else {
		iphl = M_CSUM_DATA_IPv6_HL(m0->m_pkthdr.csum_data);
	}
	vl_len |= (iphl << NQTXC_VLLEN_IPLEN_SHIFT);
	KASSERT((iphl & ~NQTXC_VLLEN_IPLEN_MASK) == 0);

	if ((mtag = VLAN_OUTPUT_TAG(&sc->sc_ethercom, m0)) != NULL) {
		vl_len |= ((VLAN_TAG_VALUE(mtag) & NQTXC_VLLEN_VLAN_MASK)
		     << NQTXC_VLLEN_VLAN_SHIFT);
		*cmdlenp |= NQTX_CMD_VLE;
	}

	mssidx = 0;

	if ((m0->m_pkthdr.csum_flags & (M_CSUM_TSOv4 | M_CSUM_TSOv6)) != 0) {
		int hlen = offset + iphl;
		int tcp_hlen;
		bool v4 = (m0->m_pkthdr.csum_flags & M_CSUM_TSOv4) != 0;

		if (__predict_false(m0->m_len <
				    (hlen + sizeof(struct tcphdr)))) {
			/*
			 * TCP/IP headers are not in the first mbuf; we need
			 * to do this the slow and painful way.  Let's just
			 * hope this doesn't happen very often.
			 */
			struct tcphdr th;

			WM_EVCNT_INCR(&sc->sc_ev_txtsopain);

			m_copydata(m0, hlen, sizeof(th), &th);
			if (v4) {
				struct ip ip;

				m_copydata(m0, offset, sizeof(ip), &ip);
				ip.ip_len = 0;
				m_copyback(m0,
				    offset + offsetof(struct ip, ip_len),
				    sizeof(ip.ip_len), &ip.ip_len);
				th.th_sum = in_cksum_phdr(ip.ip_src.s_addr,
				    ip.ip_dst.s_addr, htons(IPPROTO_TCP));
			} else {
				struct ip6_hdr ip6;

				m_copydata(m0, offset, sizeof(ip6), &ip6);
				ip6.ip6_plen = 0;
				m_copyback(m0,
				    offset + offsetof(struct ip6_hdr, ip6_plen),
				    sizeof(ip6.ip6_plen), &ip6.ip6_plen);
				th.th_sum = in6_cksum_phdr(&ip6.ip6_src,
				    &ip6.ip6_dst, 0, htonl(IPPROTO_TCP));
			}
			m_copyback(m0, hlen + offsetof(struct tcphdr, th_sum),
			    sizeof(th.th_sum), &th.th_sum);

			tcp_hlen = th.th_off << 2;
		} else {
			/*
			 * TCP/IP headers are in the first mbuf; we can do
			 * this the easy way.
			 */
			struct tcphdr *th;

			if (v4) {
				struct ip *ip =
				    (void *)(mtod(m0, char *) + offset);
				th = (void *)(mtod(m0, char *) + hlen);

				ip->ip_len = 0;
				th->th_sum = in_cksum_phdr(ip->ip_src.s_addr,
				    ip->ip_dst.s_addr, htons(IPPROTO_TCP));
			} else {
				struct ip6_hdr *ip6 =
				    (void *)(mtod(m0, char *) + offset);
				th = (void *)(mtod(m0, char *) + hlen);

				ip6->ip6_plen = 0;
				th->th_sum = in6_cksum_phdr(&ip6->ip6_src,
				    &ip6->ip6_dst, 0, htonl(IPPROTO_TCP));
			}
			tcp_hlen = th->th_off << 2;
		}
		hlen += tcp_hlen;
		*cmdlenp |= NQTX_CMD_TSE;

		if (v4) {
			WM_EVCNT_INCR(&sc->sc_ev_txtso);
			*fieldsp |= NQTXD_FIELDS_IXSM | NQTXD_FIELDS_TUXSM;
		} else {
			WM_EVCNT_INCR(&sc->sc_ev_txtso6);
			*fieldsp |= NQTXD_FIELDS_TUXSM;
		}
		*fieldsp |= ((m0->m_pkthdr.len - hlen) << NQTXD_FIELDS_PAYLEN_SHIFT);
		KASSERT(((m0->m_pkthdr.len - hlen) & ~NQTXD_FIELDS_PAYLEN_MASK) == 0);
		mssidx |= (m0->m_pkthdr.segsz << NQTXC_MSSIDX_MSS_SHIFT);
		KASSERT((m0->m_pkthdr.segsz & ~NQTXC_MSSIDX_MSS_MASK) == 0);
		mssidx |= (tcp_hlen << NQTXC_MSSIDX_L4LEN_SHIFT);
		KASSERT((tcp_hlen & ~NQTXC_MSSIDX_L4LEN_MASK) == 0);
	} else {
		*fieldsp |= (m0->m_pkthdr.len << NQTXD_FIELDS_PAYLEN_SHIFT);
		KASSERT((m0->m_pkthdr.len & ~NQTXD_FIELDS_PAYLEN_MASK) == 0);
	}

	if (m0->m_pkthdr.csum_flags & M_CSUM_IPv4) {
		*fieldsp |= NQTXD_FIELDS_IXSM;
		cmdc |= NQTXC_CMD_IP4;
	}

	if (m0->m_pkthdr.csum_flags &
	    (M_CSUM_UDPv4 | M_CSUM_TCPv4 | M_CSUM_TSOv4)) {
		WM_EVCNT_INCR(&sc->sc_ev_txtusum);
		if (m0->m_pkthdr.csum_flags & (M_CSUM_TCPv4 | M_CSUM_TSOv4)) {
			cmdc |= NQTXC_CMD_TCP;
		} else {
			cmdc |= NQTXC_CMD_UDP;
		}
		cmdc |= NQTXC_CMD_IP4;
		*fieldsp |= NQTXD_FIELDS_TUXSM;
	}
	if (m0->m_pkthdr.csum_flags &
	    (M_CSUM_UDPv6 | M_CSUM_TCPv6 | M_CSUM_TSOv6)) {
		WM_EVCNT_INCR(&sc->sc_ev_txtusum6);
		if (m0->m_pkthdr.csum_flags & (M_CSUM_TCPv6 | M_CSUM_TSOv6)) {
			cmdc |= NQTXC_CMD_TCP;
		} else {
			cmdc |= NQTXC_CMD_UDP;
		}
		cmdc |= NQTXC_CMD_IP6;
		*fieldsp |= NQTXD_FIELDS_TUXSM;
	}

	/* Fill in the context descriptor. */
	sc->sc_nq_txdescs[sc->sc_txnext].nqrx_ctx.nqtxc_vl_len =
	    htole32(vl_len);
	sc->sc_nq_txdescs[sc->sc_txnext].nqrx_ctx.nqtxc_sn = 0;
	sc->sc_nq_txdescs[sc->sc_txnext].nqrx_ctx.nqtxc_cmd =
	    htole32(cmdc);
	sc->sc_nq_txdescs[sc->sc_txnext].nqrx_ctx.nqtxc_mssidx =
	    htole32(mssidx);
	WM_CDTXSYNC(sc, sc->sc_txnext, 1, BUS_DMASYNC_PREWRITE);
	DPRINTF(WM_DEBUG_TX,
	    ("%s: TX: context desc %d 0x%08x%08x\n", device_xname(sc->sc_dev),
	    sc->sc_txnext, 0, vl_len));
	DPRINTF(WM_DEBUG_TX, ("\t0x%08x%08x\n", mssidx, cmdc));
	sc->sc_txnext = WM_NEXTTX(sc, sc->sc_txnext);
	txs->txs_ndesc++;
	return 0;
}

/*
 * wm_nq_start:		[ifnet interface function]
 *
 *	Start packet transmission on the interface for NEWQUEUE devices
 */
static void
wm_nq_start(struct ifnet *ifp)
{
	struct wm_softc *sc = ifp->if_softc;

	WM_TX_LOCK(sc);
	if (!sc->sc_stopping)
		wm_nq_start_locked(ifp);
	WM_TX_UNLOCK(sc);
}

static void
wm_nq_start_locked(struct ifnet *ifp)
{
	struct wm_softc *sc = ifp->if_softc;
	struct mbuf *m0;
	struct m_tag *mtag;
	struct wm_txsoft *txs;
	bus_dmamap_t dmamap;
	int error, nexttx, lasttx = -1, seg, segs_needed;
	bool do_csum, sent;

	KASSERT(WM_TX_LOCKED(sc));

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

	sent = false;

	/*
	 * Loop through the send queue, setting up transmit descriptors
	 * until we drain the queue, or use up all available transmit
	 * descriptors.
	 */
	for (;;) {
		m0 = NULL;

		/* Get a work queue entry. */
		if (sc->sc_txsfree < WM_TXQUEUE_GC(sc)) {
			wm_txeof(sc);
			if (sc->sc_txsfree == 0) {
				DPRINTF(WM_DEBUG_TX,
				    ("%s: TX: no free job descriptors\n",
					device_xname(sc->sc_dev)));
				WM_EVCNT_INCR(&sc->sc_ev_txsstall);
				break;
			}
		}

		/* Grab a packet off the queue. */
		IFQ_DEQUEUE(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		DPRINTF(WM_DEBUG_TX,
		    ("%s: TX: have packet to transmit: %p\n",
		    device_xname(sc->sc_dev), m0));

		txs = &sc->sc_txsoft[sc->sc_txsnext];
		dmamap = txs->txs_dmamap;

		/*
		 * Load the DMA map.  If this fails, the packet either
		 * didn't fit in the allotted number of segments, or we
		 * were short on resources.  For the too-many-segments
		 * case, we simply report an error and drop the packet,
		 * since we can't sanely copy a jumbo packet to a single
		 * buffer.
		 */
		error = bus_dmamap_load_mbuf(sc->sc_dmat, dmamap, m0,
		    BUS_DMA_WRITE|BUS_DMA_NOWAIT);
		if (error) {
			if (error == EFBIG) {
				WM_EVCNT_INCR(&sc->sc_ev_txdrop);
				log(LOG_ERR, "%s: Tx packet consumes too many "
				    "DMA segments, dropping...\n",
				    device_xname(sc->sc_dev));
				wm_dump_mbuf_chain(sc, m0);
				m_freem(m0);
				continue;
			}
			/* Short on resources, just stop for now. */
			DPRINTF(WM_DEBUG_TX,
			    ("%s: TX: dmamap load failed: %d\n",
			    device_xname(sc->sc_dev), error));
			break;
		}

		segs_needed = dmamap->dm_nsegs;

		/*
		 * Ensure we have enough descriptors free to describe
		 * the packet.  Note, we always reserve one descriptor
		 * at the end of the ring due to the semantics of the
		 * TDT register, plus one more in the event we need
		 * to load offload context.
		 */
		if (segs_needed > sc->sc_txfree - 2) {
			/*
			 * Not enough free descriptors to transmit this
			 * packet.  We haven't committed anything yet,
			 * so just unload the DMA map, put the packet
			 * pack on the queue, and punt.  Notify the upper
			 * layer that there are no more slots left.
			 */
			DPRINTF(WM_DEBUG_TX,
			    ("%s: TX: need %d (%d) descriptors, have %d\n",
			    device_xname(sc->sc_dev), dmamap->dm_nsegs,
			    segs_needed, sc->sc_txfree - 1));
			ifp->if_flags |= IFF_OACTIVE;
			bus_dmamap_unload(sc->sc_dmat, dmamap);
			WM_EVCNT_INCR(&sc->sc_ev_txdstall);
			break;
		}

		/* WE ARE NOW COMMITTED TO TRANSMITTING THE PACKET. */

		DPRINTF(WM_DEBUG_TX,
		    ("%s: TX: packet has %d (%d) DMA segments\n",
		    device_xname(sc->sc_dev), dmamap->dm_nsegs, segs_needed));

		WM_EVCNT_INCR(&sc->sc_ev_txseg[dmamap->dm_nsegs - 1]);

		/*
		 * Store a pointer to the packet so that we can free it
		 * later.
		 *
		 * Initially, we consider the number of descriptors the
		 * packet uses the number of DMA segments.  This may be
		 * incremented by 1 if we do checksum offload (a descriptor
		 * is used to set the checksum context).
		 */
		txs->txs_mbuf = m0;
		txs->txs_firstdesc = sc->sc_txnext;
		txs->txs_ndesc = segs_needed;

		/* Set up offload parameters for this packet. */
		uint32_t cmdlen, fields, dcmdlen;
		if (m0->m_pkthdr.csum_flags &
		    (M_CSUM_TSOv4|M_CSUM_TSOv6|
		    M_CSUM_IPv4|M_CSUM_TCPv4|M_CSUM_UDPv4|
		    M_CSUM_TCPv6|M_CSUM_UDPv6)) {
			if (wm_nq_tx_offload(sc, txs, &cmdlen, &fields,
			    &do_csum) != 0) {
				/* Error message already displayed. */
				bus_dmamap_unload(sc->sc_dmat, dmamap);
				continue;
			}
		} else {
			do_csum = false;
			cmdlen = 0;
			fields = 0;
		}

		/* Sync the DMA map. */
		bus_dmamap_sync(sc->sc_dmat, dmamap, 0, dmamap->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		/* Initialize the first transmit descriptor. */
		nexttx = sc->sc_txnext;
		if (!do_csum) {
			/* setup a legacy descriptor */
			wm_set_dma_addr(
			    &sc->sc_txdescs[nexttx].wtx_addr,
			    dmamap->dm_segs[0].ds_addr);
			sc->sc_txdescs[nexttx].wtx_cmdlen =
			    htole32(WTX_CMD_IFCS | dmamap->dm_segs[0].ds_len);
			sc->sc_txdescs[nexttx].wtx_fields.wtxu_status = 0;
			sc->sc_txdescs[nexttx].wtx_fields.wtxu_options = 0;
			if ((mtag = VLAN_OUTPUT_TAG(&sc->sc_ethercom, m0)) !=
			    NULL) {
				sc->sc_txdescs[nexttx].wtx_cmdlen |=
				    htole32(WTX_CMD_VLE);
				sc->sc_txdescs[nexttx].wtx_fields.wtxu_vlan =
				    htole16(VLAN_TAG_VALUE(mtag) & 0xffff);
			} else {
				sc->sc_txdescs[nexttx].wtx_fields.wtxu_vlan =0;
			}
			dcmdlen = 0;
		} else {
			/* setup an advanced data descriptor */
			sc->sc_nq_txdescs[nexttx].nqtx_data.nqtxd_addr =
			    htole64(dmamap->dm_segs[0].ds_addr);
			KASSERT((dmamap->dm_segs[0].ds_len & cmdlen) == 0);
			sc->sc_nq_txdescs[nexttx].nqtx_data.nqtxd_cmdlen =
			    htole32(dmamap->dm_segs[0].ds_len | cmdlen );
			sc->sc_nq_txdescs[nexttx].nqtx_data.nqtxd_fields =
			    htole32(fields);
			DPRINTF(WM_DEBUG_TX,
			    ("%s: TX: adv data desc %d 0x%" PRIx64 "\n",
			    device_xname(sc->sc_dev), nexttx,
			    (uint64_t)dmamap->dm_segs[0].ds_addr));
			DPRINTF(WM_DEBUG_TX,
			    ("\t 0x%08x%08x\n", fields,
			    (uint32_t)dmamap->dm_segs[0].ds_len | cmdlen));
			dcmdlen = NQTX_DTYP_D | NQTX_CMD_DEXT;
		}

		lasttx = nexttx;
		nexttx = WM_NEXTTX(sc, nexttx);
		/*
		 * fill in the next descriptors. legacy or adcanced format
		 * is the same here
		 */
		for (seg = 1; seg < dmamap->dm_nsegs;
		    seg++, nexttx = WM_NEXTTX(sc, nexttx)) {
			sc->sc_nq_txdescs[nexttx].nqtx_data.nqtxd_addr =
			    htole64(dmamap->dm_segs[seg].ds_addr);
			sc->sc_nq_txdescs[nexttx].nqtx_data.nqtxd_cmdlen =
			    htole32(dcmdlen | dmamap->dm_segs[seg].ds_len);
			KASSERT((dcmdlen & dmamap->dm_segs[seg].ds_len) == 0);
			sc->sc_nq_txdescs[nexttx].nqtx_data.nqtxd_fields = 0;
			lasttx = nexttx;

			DPRINTF(WM_DEBUG_TX,
			    ("%s: TX: desc %d: %#" PRIx64 ", "
			     "len %#04zx\n",
			    device_xname(sc->sc_dev), nexttx,
			    (uint64_t)dmamap->dm_segs[seg].ds_addr,
			    dmamap->dm_segs[seg].ds_len));
		}

		KASSERT(lasttx != -1);

		/*
		 * Set up the command byte on the last descriptor of
		 * the packet.  If we're in the interrupt delay window,
		 * delay the interrupt.
		 */
		KASSERT((WTX_CMD_EOP | WTX_CMD_RS) ==
		    (NQTX_CMD_EOP | NQTX_CMD_RS));
		sc->sc_txdescs[lasttx].wtx_cmdlen |=
		    htole32(WTX_CMD_EOP | WTX_CMD_RS);

		txs->txs_lastdesc = lasttx;

		DPRINTF(WM_DEBUG_TX,
		    ("%s: TX: desc %d: cmdlen 0x%08x\n",
		    device_xname(sc->sc_dev),
		    lasttx, le32toh(sc->sc_txdescs[lasttx].wtx_cmdlen)));

		/* Sync the descriptors we're using. */
		WM_CDTXSYNC(sc, sc->sc_txnext, txs->txs_ndesc,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/* Give the packet to the chip. */
		CSR_WRITE(sc, sc->sc_tdt_reg, nexttx);
		sent = true;

		DPRINTF(WM_DEBUG_TX,
		    ("%s: TX: TDT -> %d\n", device_xname(sc->sc_dev), nexttx));

		DPRINTF(WM_DEBUG_TX,
		    ("%s: TX: finished transmitting packet, job %d\n",
		    device_xname(sc->sc_dev), sc->sc_txsnext));

		/* Advance the tx pointer. */
		sc->sc_txfree -= txs->txs_ndesc;
		sc->sc_txnext = nexttx;

		sc->sc_txsfree--;
		sc->sc_txsnext = WM_NEXTTXS(sc, sc->sc_txsnext);

		/* Pass the packet to any BPF listeners. */
		bpf_mtap(ifp, m0);
	}

	if (m0 != NULL) {
		ifp->if_flags |= IFF_OACTIVE;
		WM_EVCNT_INCR(&sc->sc_ev_txdrop);
		DPRINTF(WM_DEBUG_TX, ("%s: TX: error after IFQ_DEQUEUE\n", __func__));
		m_freem(m0);
	}

	if (sc->sc_txsfree == 0 || sc->sc_txfree <= 2) {
		/* No more slots; notify upper layer. */
		ifp->if_flags |= IFF_OACTIVE;
	}

	if (sent) {
		/* Set a watchdog timer in case the chip flakes out. */
		ifp->if_timer = 5;
	}
}

/* Interrupt */

/*
 * wm_txeof:
 *
 *	Helper; handle transmit interrupts.
 */
static int
wm_txeof(struct wm_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct wm_txsoft *txs;
	bool processed = false;
	int count = 0;
	int i;
	uint8_t status;

	if (sc->sc_stopping)
		return 0;

	ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * Go through the Tx list and free mbufs for those
	 * frames which have been transmitted.
	 */
	for (i = sc->sc_txsdirty; sc->sc_txsfree != WM_TXQUEUELEN(sc);
	     i = WM_NEXTTXS(sc, i), sc->sc_txsfree++) {
		txs = &sc->sc_txsoft[i];

		DPRINTF(WM_DEBUG_TX,
		    ("%s: TX: checking job %d\n", device_xname(sc->sc_dev), i));

		WM_CDTXSYNC(sc, txs->txs_firstdesc, txs->txs_ndesc,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		status =
		    sc->sc_txdescs[txs->txs_lastdesc].wtx_fields.wtxu_status;
		if ((status & WTX_ST_DD) == 0) {
			WM_CDTXSYNC(sc, txs->txs_lastdesc, 1,
			    BUS_DMASYNC_PREREAD);
			break;
		}

		processed = true;
		count++;
		DPRINTF(WM_DEBUG_TX,
		    ("%s: TX: job %d done: descs %d..%d\n",
		    device_xname(sc->sc_dev), i, txs->txs_firstdesc,
		    txs->txs_lastdesc));

		/*
		 * XXX We should probably be using the statistics
		 * XXX registers, but I don't know if they exist
		 * XXX on chips before the i82544.
		 */

#ifdef WM_EVENT_COUNTERS
		if (status & WTX_ST_TU)
			WM_EVCNT_INCR(&sc->sc_ev_tu);
#endif /* WM_EVENT_COUNTERS */

		if (status & (WTX_ST_EC|WTX_ST_LC)) {
			ifp->if_oerrors++;
			if (status & WTX_ST_LC)
				log(LOG_WARNING, "%s: late collision\n",
				    device_xname(sc->sc_dev));
			else if (status & WTX_ST_EC) {
				ifp->if_collisions += 16;
				log(LOG_WARNING, "%s: excessive collisions\n",
				    device_xname(sc->sc_dev));
			}
		} else
			ifp->if_opackets++;

		sc->sc_txfree += txs->txs_ndesc;
		bus_dmamap_sync(sc->sc_dmat, txs->txs_dmamap,
		    0, txs->txs_dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, txs->txs_dmamap);
		m_freem(txs->txs_mbuf);
		txs->txs_mbuf = NULL;
	}

	/* Update the dirty transmit buffer pointer. */
	sc->sc_txsdirty = i;
	DPRINTF(WM_DEBUG_TX,
	    ("%s: TX: txsdirty -> %d\n", device_xname(sc->sc_dev), i));

	if (count != 0)
		rnd_add_uint32(&sc->rnd_source, count);

	/*
	 * If there are no more pending transmissions, cancel the watchdog
	 * timer.
	 */
	if (sc->sc_txsfree == WM_TXQUEUELEN(sc))
		ifp->if_timer = 0;

	return processed;
}

/*
 * wm_rxeof:
 *
 *	Helper; handle receive interrupts.
 */
static void
wm_rxeof(struct wm_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct wm_rxsoft *rxs;
	struct mbuf *m;
	int i, len;
	int count = 0;
	uint8_t status, errors;
	uint16_t vlantag;

	for (i = sc->sc_rxptr;; i = WM_NEXTRX(i)) {
		rxs = &sc->sc_rxsoft[i];

		DPRINTF(WM_DEBUG_RX,
		    ("%s: RX: checking descriptor %d\n",
		    device_xname(sc->sc_dev), i));

		WM_CDRXSYNC(sc, i, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		status = sc->sc_rxdescs[i].wrx_status;
		errors = sc->sc_rxdescs[i].wrx_errors;
		len = le16toh(sc->sc_rxdescs[i].wrx_len);
		vlantag = sc->sc_rxdescs[i].wrx_special;

		if ((status & WRX_ST_DD) == 0) {
			/* We have processed all of the receive descriptors. */
			WM_CDRXSYNC(sc, i, BUS_DMASYNC_PREREAD);
			break;
		}

		count++;
		if (__predict_false(sc->sc_rxdiscard)) {
			DPRINTF(WM_DEBUG_RX,
			    ("%s: RX: discarding contents of descriptor %d\n",
			    device_xname(sc->sc_dev), i));
			WM_INIT_RXDESC(sc, i);
			if (status & WRX_ST_EOP) {
				/* Reset our state. */
				DPRINTF(WM_DEBUG_RX,
				    ("%s: RX: resetting rxdiscard -> 0\n",
				    device_xname(sc->sc_dev)));
				sc->sc_rxdiscard = 0;
			}
			continue;
		}

		bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
		    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);

		m = rxs->rxs_mbuf;

		/*
		 * Add a new receive buffer to the ring, unless of
		 * course the length is zero. Treat the latter as a
		 * failed mapping.
		 */
		if ((len == 0) || (wm_add_rxbuf(sc, i) != 0)) {
			/*
			 * Failed, throw away what we've done so
			 * far, and discard the rest of the packet.
			 */
			ifp->if_ierrors++;
			bus_dmamap_sync(sc->sc_dmat, rxs->rxs_dmamap, 0,
			    rxs->rxs_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);
			WM_INIT_RXDESC(sc, i);
			if ((status & WRX_ST_EOP) == 0)
				sc->sc_rxdiscard = 1;
			if (sc->sc_rxhead != NULL)
				m_freem(sc->sc_rxhead);
			WM_RXCHAIN_RESET(sc);
			DPRINTF(WM_DEBUG_RX,
			    ("%s: RX: Rx buffer allocation failed, "
			    "dropping packet%s\n", device_xname(sc->sc_dev),
			    sc->sc_rxdiscard ? " (discard)" : ""));
			continue;
		}

		m->m_len = len;
		sc->sc_rxlen += len;
		DPRINTF(WM_DEBUG_RX,
		    ("%s: RX: buffer at %p len %d\n",
		    device_xname(sc->sc_dev), m->m_data, len));

		/* If this is not the end of the packet, keep looking. */
		if ((status & WRX_ST_EOP) == 0) {
			WM_RXCHAIN_LINK(sc, m);
			DPRINTF(WM_DEBUG_RX,
			    ("%s: RX: not yet EOP, rxlen -> %d\n",
			    device_xname(sc->sc_dev), sc->sc_rxlen));
			continue;
		}

		/*
		 * Okay, we have the entire packet now.  The chip is
		 * configured to include the FCS except I350 and I21[01]
		 * (not all chips can be configured to strip it),
		 * so we need to trim it.
		 * May need to adjust length of previous mbuf in the
		 * chain if the current mbuf is too short.
		 * For an eratta, the RCTL_SECRC bit in RCTL register
		 * is always set in I350, so we don't trim it.
		 */
		if ((sc->sc_type != WM_T_I350) && (sc->sc_type != WM_T_I354)
		    && (sc->sc_type != WM_T_I210)
		    && (sc->sc_type != WM_T_I211)) {
			if (m->m_len < ETHER_CRC_LEN) {
				sc->sc_rxtail->m_len
				    -= (ETHER_CRC_LEN - m->m_len);
				m->m_len = 0;
			} else
				m->m_len -= ETHER_CRC_LEN;
			len = sc->sc_rxlen - ETHER_CRC_LEN;
		} else
			len = sc->sc_rxlen;

		WM_RXCHAIN_LINK(sc, m);

		*sc->sc_rxtailp = NULL;
		m = sc->sc_rxhead;

		WM_RXCHAIN_RESET(sc);

		DPRINTF(WM_DEBUG_RX,
		    ("%s: RX: have entire packet, len -> %d\n",
		    device_xname(sc->sc_dev), len));

		/* If an error occurred, update stats and drop the packet. */
		if (errors &
		     (WRX_ER_CE|WRX_ER_SE|WRX_ER_SEQ|WRX_ER_CXE|WRX_ER_RXE)) {
			if (errors & WRX_ER_SE)
				log(LOG_WARNING, "%s: symbol error\n",
				    device_xname(sc->sc_dev));
			else if (errors & WRX_ER_SEQ)
				log(LOG_WARNING, "%s: receive sequence error\n",
				    device_xname(sc->sc_dev));
			else if (errors & WRX_ER_CE)
				log(LOG_WARNING, "%s: CRC error\n",
				    device_xname(sc->sc_dev));
			m_freem(m);
			continue;
		}

		/* No errors.  Receive the packet. */
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = len;

		/*
		 * If VLANs are enabled, VLAN packets have been unwrapped
		 * for us.  Associate the tag with the packet.
		 */
		/* XXXX should check for i350 and i354 */
		if ((status & WRX_ST_VP) != 0) {
			VLAN_INPUT_TAG(ifp, m,
			    le16toh(vlantag),
			    continue);
		}

		/* Set up checksum info for this packet. */
		if ((status & WRX_ST_IXSM) == 0) {
			if (status & WRX_ST_IPCS) {
				WM_EVCNT_INCR(&sc->sc_ev_rxipsum);
				m->m_pkthdr.csum_flags |= M_CSUM_IPv4;
				if (errors & WRX_ER_IPE)
					m->m_pkthdr.csum_flags |=
					    M_CSUM_IPv4_BAD;
			}
			if (status & WRX_ST_TCPCS) {
				/*
				 * Note: we don't know if this was TCP or UDP,
				 * so we just set both bits, and expect the
				 * upper layers to deal.
				 */
				WM_EVCNT_INCR(&sc->sc_ev_rxtusum);
				m->m_pkthdr.csum_flags |=
				    M_CSUM_TCPv4 | M_CSUM_UDPv4 |
				    M_CSUM_TCPv6 | M_CSUM_UDPv6;
				if (errors & WRX_ER_TCPE)
					m->m_pkthdr.csum_flags |=
					    M_CSUM_TCP_UDP_BAD;
			}
		}

		ifp->if_ipackets++;

		WM_RX_UNLOCK(sc);

		/* Pass this up to any BPF listeners. */
		bpf_mtap(ifp, m);

		/* Pass it on. */
		(*ifp->if_input)(ifp, m);

		WM_RX_LOCK(sc);

		if (sc->sc_stopping)
			break;
	}

	/* Update the receive pointer. */
	sc->sc_rxptr = i;
	if (count != 0)
		rnd_add_uint32(&sc->rnd_source, count);

	DPRINTF(WM_DEBUG_RX,
	    ("%s: RX: rxptr -> %d\n", device_xname(sc->sc_dev), i));
}

/*
 * wm_linkintr_gmii:
 *
 *	Helper; handle link interrupts for GMII.
 */
static void
wm_linkintr_gmii(struct wm_softc *sc, uint32_t icr)
{

	KASSERT(WM_TX_LOCKED(sc));

	DPRINTF(WM_DEBUG_LINK, ("%s: %s:\n", device_xname(sc->sc_dev),
		__func__));

	if (icr & ICR_LSC) {
		DPRINTF(WM_DEBUG_LINK,
		    ("%s: LINK: LSC -> mii_pollstat\n",
			device_xname(sc->sc_dev)));
		mii_pollstat(&sc->sc_mii);
		if (sc->sc_type == WM_T_82543) {
			int miistatus, active;

			/*
			 * With 82543, we need to force speed and
			 * duplex on the MAC equal to what the PHY
			 * speed and duplex configuration is.
			 */
			miistatus = sc->sc_mii.mii_media_status;

			if (miistatus & IFM_ACTIVE) {
				active = sc->sc_mii.mii_media_active;
				sc->sc_ctrl &= ~(CTRL_SPEED_MASK | CTRL_FD);
				switch (IFM_SUBTYPE(active)) {
				case IFM_10_T:
					sc->sc_ctrl |= CTRL_SPEED_10;
					break;
				case IFM_100_TX:
					sc->sc_ctrl |= CTRL_SPEED_100;
					break;
				case IFM_1000_T:
					sc->sc_ctrl |= CTRL_SPEED_1000;
					break;
				default:
					/*
					 * fiber?
					 * Shoud not enter here.
					 */
					printf("unknown media (%x)\n",
					    active);
					break;
				}
				if (active & IFM_FDX)
					sc->sc_ctrl |= CTRL_FD;
				CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl);
			}
		} else if ((sc->sc_type == WM_T_ICH8)
		    && (sc->sc_phytype == WMPHY_IGP_3)) {
			wm_kmrn_lock_loss_workaround_ich8lan(sc);
		} else if (sc->sc_type == WM_T_PCH) {
			wm_k1_gig_workaround_hv(sc,
			    ((sc->sc_mii.mii_media_status & IFM_ACTIVE) != 0));
		}

		if ((sc->sc_phytype == WMPHY_82578)
		    && (IFM_SUBTYPE(sc->sc_mii.mii_media_active)
			== IFM_1000_T)) {

			if ((sc->sc_mii.mii_media_status & IFM_ACTIVE) != 0) {
				delay(200*1000); /* XXX too big */

				/* Link stall fix for link up */
				wm_gmii_hv_writereg(sc->sc_dev, 1,
				    HV_MUX_DATA_CTRL,
				    HV_MUX_DATA_CTRL_GEN_TO_MAC
				    | HV_MUX_DATA_CTRL_FORCE_SPEED);
				wm_gmii_hv_writereg(sc->sc_dev, 1,
				    HV_MUX_DATA_CTRL,
				    HV_MUX_DATA_CTRL_GEN_TO_MAC);
			}
		}
	} else if (icr & ICR_RXSEQ) {
		DPRINTF(WM_DEBUG_LINK,
		    ("%s: LINK Receive sequence error\n",
			device_xname(sc->sc_dev)));
	}
}

/*
 * wm_linkintr_tbi:
 *
 *	Helper; handle link interrupts for TBI mode.
 */
static void
wm_linkintr_tbi(struct wm_softc *sc, uint32_t icr)
{
	uint32_t status;

	DPRINTF(WM_DEBUG_LINK, ("%s: %s:\n", device_xname(sc->sc_dev),
		__func__));

	status = CSR_READ(sc, WMREG_STATUS);
	if (icr & ICR_LSC) {
		if (status & STATUS_LU) {
			DPRINTF(WM_DEBUG_LINK, ("%s: LINK: LSC -> up %s\n",
			    device_xname(sc->sc_dev),
			    (status & STATUS_FD) ? "FDX" : "HDX"));
			/*
			 * NOTE: CTRL will update TFCE and RFCE automatically,
			 * so we should update sc->sc_ctrl
			 */

			sc->sc_ctrl = CSR_READ(sc, WMREG_CTRL);
			sc->sc_tctl &= ~TCTL_COLD(0x3ff);
			sc->sc_fcrtl &= ~FCRTL_XONE;
			if (status & STATUS_FD)
				sc->sc_tctl |=
				    TCTL_COLD(TX_COLLISION_DISTANCE_FDX);
			else
				sc->sc_tctl |=
				    TCTL_COLD(TX_COLLISION_DISTANCE_HDX);
			if (sc->sc_ctrl & CTRL_TFCE)
				sc->sc_fcrtl |= FCRTL_XONE;
			CSR_WRITE(sc, WMREG_TCTL, sc->sc_tctl);
			CSR_WRITE(sc, (sc->sc_type < WM_T_82543) ?
				      WMREG_OLD_FCRTL : WMREG_FCRTL,
				      sc->sc_fcrtl);
			sc->sc_tbi_linkup = 1;
		} else {
			DPRINTF(WM_DEBUG_LINK, ("%s: LINK: LSC -> down\n",
			    device_xname(sc->sc_dev)));
			sc->sc_tbi_linkup = 0;
		}
		/* Update LED */
		wm_tbi_serdes_set_linkled(sc);
	} else if (icr & ICR_RXSEQ) {
		DPRINTF(WM_DEBUG_LINK,
		    ("%s: LINK: Receive sequence error\n",
		    device_xname(sc->sc_dev)));
	}
}

/*
 * wm_linkintr_serdes:
 *
 *	Helper; handle link interrupts for TBI mode.
 */
static void
wm_linkintr_serdes(struct wm_softc *sc, uint32_t icr)
{
	struct mii_data *mii = &sc->sc_mii;
	struct ifmedia_entry *ife = sc->sc_mii.mii_media.ifm_cur;
	uint32_t pcs_adv, pcs_lpab, reg;

	DPRINTF(WM_DEBUG_LINK, ("%s: %s:\n", device_xname(sc->sc_dev),
		__func__));

	if (icr & ICR_LSC) {
		/* Check PCS */
		reg = CSR_READ(sc, WMREG_PCS_LSTS);
		if ((reg & PCS_LSTS_LINKOK) != 0) {
			mii->mii_media_status |= IFM_ACTIVE;
			sc->sc_tbi_linkup = 1;
		} else {
			mii->mii_media_status |= IFM_NONE;
			sc->sc_tbi_linkup = 0;
			wm_tbi_serdes_set_linkled(sc);
			return;
		}
		mii->mii_media_active |= IFM_1000_SX;
		if ((reg & PCS_LSTS_FDX) != 0)
			mii->mii_media_active |= IFM_FDX;
		else
			mii->mii_media_active |= IFM_HDX;
		if (IFM_SUBTYPE(ife->ifm_media) == IFM_AUTO) {
			/* Check flow */
			reg = CSR_READ(sc, WMREG_PCS_LSTS);
			if ((reg & PCS_LSTS_AN_COMP) == 0) {
				DPRINTF(WM_DEBUG_LINK,
				    ("XXX LINKOK but not ACOMP\n"));
				return;
			}
			pcs_adv = CSR_READ(sc, WMREG_PCS_ANADV);
			pcs_lpab = CSR_READ(sc, WMREG_PCS_LPAB);
			DPRINTF(WM_DEBUG_LINK,
			    ("XXX AN result %08x, %08x\n", pcs_adv, pcs_lpab));
			if ((pcs_adv & TXCW_SYM_PAUSE)
			    && (pcs_lpab & TXCW_SYM_PAUSE)) {
				mii->mii_media_active |= IFM_FLOW
				    | IFM_ETH_TXPAUSE | IFM_ETH_RXPAUSE;
			} else if (((pcs_adv & TXCW_SYM_PAUSE) == 0)
			    && (pcs_adv & TXCW_ASYM_PAUSE)
			    && (pcs_lpab & TXCW_SYM_PAUSE)
			    && (pcs_lpab & TXCW_ASYM_PAUSE))
				mii->mii_media_active |= IFM_FLOW
				    | IFM_ETH_TXPAUSE;
			else if ((pcs_adv & TXCW_SYM_PAUSE)
			    && (pcs_adv & TXCW_ASYM_PAUSE)
			    && ((pcs_lpab & TXCW_SYM_PAUSE) == 0)
			    && (pcs_lpab & TXCW_ASYM_PAUSE))
				mii->mii_media_active |= IFM_FLOW
				    | IFM_ETH_RXPAUSE;
		}
		/* Update LED */
		wm_tbi_serdes_set_linkled(sc);
	} else {
		DPRINTF(WM_DEBUG_LINK,
		    ("%s: LINK: Receive sequence error\n",
		    device_xname(sc->sc_dev)));
	}
}

/*
 * wm_linkintr:
 *
 *	Helper; handle link interrupts.
 */
static void
wm_linkintr(struct wm_softc *sc, uint32_t icr)
{

	if (sc->sc_flags & WM_F_HAS_MII)
		wm_linkintr_gmii(sc, icr);
	else if ((sc->sc_mediatype == WM_MEDIATYPE_SERDES)
	    && (sc->sc_type >= WM_T_82575))
		wm_linkintr_serdes(sc, icr);
	else
		wm_linkintr_tbi(sc, icr);
}

/*
 * wm_intr_legacy:
 *
 *	Interrupt service routine for INTx and MSI.
 */
static int
wm_intr_legacy(void *arg)
{
	struct wm_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	uint32_t icr, rndval = 0;
	int handled = 0;

	DPRINTF(WM_DEBUG_TX,
	    ("%s: INTx: got intr\n", device_xname(sc->sc_dev)));
	while (1 /* CONSTCOND */) {
		icr = CSR_READ(sc, WMREG_ICR);
		if ((icr & sc->sc_icr) == 0)
			break;
		if (rndval == 0)
			rndval = icr;

		WM_RX_LOCK(sc);

		if (sc->sc_stopping) {
			WM_RX_UNLOCK(sc);
			break;
		}

		handled = 1;

#if defined(WM_DEBUG) || defined(WM_EVENT_COUNTERS)
		if (icr & (ICR_RXDMT0|ICR_RXT0)) {
			DPRINTF(WM_DEBUG_RX,
			    ("%s: RX: got Rx intr 0x%08x\n",
			    device_xname(sc->sc_dev),
			    icr & (ICR_RXDMT0|ICR_RXT0)));
			WM_EVCNT_INCR(&sc->sc_ev_rxintr);
		}
#endif
		wm_rxeof(sc);

		WM_RX_UNLOCK(sc);
		WM_TX_LOCK(sc);

#if defined(WM_DEBUG) || defined(WM_EVENT_COUNTERS)
		if (icr & ICR_TXDW) {
			DPRINTF(WM_DEBUG_TX,
			    ("%s: TX: got TXDW interrupt\n",
			    device_xname(sc->sc_dev)));
			WM_EVCNT_INCR(&sc->sc_ev_txdw);
		}
#endif
		wm_txeof(sc);

		if (icr & (ICR_LSC|ICR_RXSEQ)) {
			WM_EVCNT_INCR(&sc->sc_ev_linkintr);
			wm_linkintr(sc, icr);
		}

		WM_TX_UNLOCK(sc);

		if (icr & ICR_RXO) {
#if defined(WM_DEBUG)
			log(LOG_WARNING, "%s: Receive overrun\n",
			    device_xname(sc->sc_dev));
#endif /* defined(WM_DEBUG) */
		}
	}

	rnd_add_uint32(&sc->rnd_source, rndval);

	if (handled) {
		/* Try to get more packets going. */
		ifp->if_start(ifp);
	}

	return handled;
}

#ifdef WM_MSI_MSIX
/*
 * wm_txintr_msix:
 *
 *	Interrupt service routine for TX complete interrupt for MSI-X.
 */
static int
wm_txintr_msix(void *arg)
{
	struct wm_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	int handled = 0;

	DPRINTF(WM_DEBUG_TX,
	    ("%s: TX: got Tx intr\n", device_xname(sc->sc_dev)));

	if (sc->sc_type == WM_T_82574)
		CSR_WRITE(sc, WMREG_IMC, ICR_TXQ0); /* 82574 only */
	else if (sc->sc_type == WM_T_82575)
		CSR_WRITE(sc, WMREG_EIMC, EITR_TX_QUEUE0);
	else
		CSR_WRITE(sc, WMREG_EIMC, 1 << WM_MSIX_TXINTR_IDX);

	WM_TX_LOCK(sc);

	if (sc->sc_stopping)
		goto out;

	WM_EVCNT_INCR(&sc->sc_ev_txdw);
	handled = wm_txeof(sc);

out:
	WM_TX_UNLOCK(sc);

	if (sc->sc_type == WM_T_82574)
		CSR_WRITE(sc, WMREG_IMS, ICR_TXQ0); /* 82574 only */
	else if (sc->sc_type == WM_T_82575)
		CSR_WRITE(sc, WMREG_EIMS, EITR_TX_QUEUE0);
	else
		CSR_WRITE(sc, WMREG_EIMS, 1 << WM_MSIX_TXINTR_IDX);

	if (handled) {
		/* Try to get more packets going. */
		ifp->if_start(ifp);
	}

	return handled;
}

/*
 * wm_rxintr_msix:
 *
 *	Interrupt service routine for RX interrupt for MSI-X.
 */
static int
wm_rxintr_msix(void *arg)
{
	struct wm_softc *sc = arg;

	DPRINTF(WM_DEBUG_TX,
	    ("%s: RX: got Rx intr\n", device_xname(sc->sc_dev)));

	if (sc->sc_type == WM_T_82574)
		CSR_WRITE(sc, WMREG_IMC, ICR_RXQ0); /* 82574 only */
	else if (sc->sc_type == WM_T_82575)
		CSR_WRITE(sc, WMREG_EIMC, EITR_RX_QUEUE0);
	else
		CSR_WRITE(sc, WMREG_EIMC, 1 << WM_MSIX_RXINTR_IDX);

	WM_RX_LOCK(sc);

	if (sc->sc_stopping)
		goto out;

	WM_EVCNT_INCR(&sc->sc_ev_rxintr);
	wm_rxeof(sc);

out:
	WM_RX_UNLOCK(sc);

	if (sc->sc_type == WM_T_82574)
		CSR_WRITE(sc, WMREG_IMS, ICR_RXQ0);
	else if (sc->sc_type == WM_T_82575)
		CSR_WRITE(sc, WMREG_EIMS, EITR_RX_QUEUE0);
	else
		CSR_WRITE(sc, WMREG_EIMS, 1 << WM_MSIX_RXINTR_IDX);

	return 1;
}

/*
 * wm_linkintr_msix:
 *
 *	Interrupt service routine for link status change for MSI-X.
 */
static int
wm_linkintr_msix(void *arg)
{
	struct wm_softc *sc = arg;
	uint32_t reg;

	DPRINTF(WM_DEBUG_TX,
	    ("%s: LINK: got link intr\n", device_xname(sc->sc_dev)));

	reg = CSR_READ(sc, WMREG_ICR);
	WM_TX_LOCK(sc);
	if ((sc->sc_stopping) || ((reg & ICR_LSC) == 0))
		goto out;

	WM_EVCNT_INCR(&sc->sc_ev_linkintr);
	wm_linkintr(sc, ICR_LSC);

out:
	WM_TX_UNLOCK(sc);
	
	if (sc->sc_type == WM_T_82574)
		CSR_WRITE(sc, WMREG_IMS, ICR_OTHER | ICR_LSC); /* 82574 only */
	else if (sc->sc_type == WM_T_82575)
		CSR_WRITE(sc, WMREG_EIMS, EITR_OTHER);
	else
		CSR_WRITE(sc, WMREG_EIMS, 1 << WM_MSIX_LINKINTR_IDX);

	return 1;
}
#endif /* WM_MSI_MSIX */

/*
 * Media related.
 * GMII, SGMII, TBI (and SERDES)
 */

/* Common */

/*
 * wm_tbi_serdes_set_linkled:
 *
 *	Update the link LED on TBI and SERDES devices.
 */
static void
wm_tbi_serdes_set_linkled(struct wm_softc *sc)
{

	if (sc->sc_tbi_linkup)
		sc->sc_ctrl |= CTRL_SWDPIN(0);
	else
		sc->sc_ctrl &= ~CTRL_SWDPIN(0);

	/* 82540 or newer devices are active low */
	sc->sc_ctrl ^= (sc->sc_type >= WM_T_82540) ? CTRL_SWDPIN(0) : 0;

	CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl);
}

/* GMII related */

/*
 * wm_gmii_reset:
 *
 *	Reset the PHY.
 */
static void
wm_gmii_reset(struct wm_softc *sc)
{
	uint32_t reg;
	int rv;

	/* get phy semaphore */
	switch (sc->sc_type) {
	case WM_T_82571:
	case WM_T_82572:
	case WM_T_82573:
	case WM_T_82574:
	case WM_T_82583:
		 /* XXX should get sw semaphore, too */
		rv = wm_get_swsm_semaphore(sc);
		break;
	case WM_T_82575:
	case WM_T_82576:
	case WM_T_82580:
	case WM_T_I350:
	case WM_T_I354:
	case WM_T_I210:
	case WM_T_I211:
	case WM_T_80003:
		rv = wm_get_swfw_semaphore(sc, swfwphysem[sc->sc_funcid]);
		break;
	case WM_T_ICH8:
	case WM_T_ICH9:
	case WM_T_ICH10:
	case WM_T_PCH:
	case WM_T_PCH2:
	case WM_T_PCH_LPT:
		rv = wm_get_swfwhw_semaphore(sc);
		break;
	default:
		/* nothing to do*/
		rv = 0;
		break;
	}
	if (rv != 0) {
		aprint_error_dev(sc->sc_dev, "%s: failed to get semaphore\n",
		    __func__);
		return;
	}

	switch (sc->sc_type) {
	case WM_T_82542_2_0:
	case WM_T_82542_2_1:
		/* null */
		break;
	case WM_T_82543:
		/*
		 * With 82543, we need to force speed and duplex on the MAC
		 * equal to what the PHY speed and duplex configuration is.
		 * In addition, we need to perform a hardware reset on the PHY
		 * to take it out of reset.
		 */
		sc->sc_ctrl |= CTRL_FRCSPD | CTRL_FRCFDX;
		CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl);

		/* The PHY reset pin is active-low. */
		reg = CSR_READ(sc, WMREG_CTRL_EXT);
		reg &= ~((CTRL_EXT_SWDPIO_MASK << CTRL_EXT_SWDPIO_SHIFT) |
		    CTRL_EXT_SWDPIN(4));
		reg |= CTRL_EXT_SWDPIO(4);

		CSR_WRITE(sc, WMREG_CTRL_EXT, reg);
		CSR_WRITE_FLUSH(sc);
		delay(10*1000);

		CSR_WRITE(sc, WMREG_CTRL_EXT, reg | CTRL_EXT_SWDPIN(4));
		CSR_WRITE_FLUSH(sc);
		delay(150);
#if 0
		sc->sc_ctrl_ext = reg | CTRL_EXT_SWDPIN(4);
#endif
		delay(20*1000);	/* XXX extra delay to get PHY ID? */
		break;
	case WM_T_82544:	/* reset 10000us */
	case WM_T_82540:
	case WM_T_82545:
	case WM_T_82545_3:
	case WM_T_82546:
	case WM_T_82546_3:
	case WM_T_82541:
	case WM_T_82541_2:
	case WM_T_82547:
	case WM_T_82547_2:
	case WM_T_82571:	/* reset 100us */
	case WM_T_82572:
	case WM_T_82573:
	case WM_T_82574:
	case WM_T_82575:
	case WM_T_82576:
	case WM_T_82580:
	case WM_T_I350:
	case WM_T_I354:
	case WM_T_I210:
	case WM_T_I211:
	case WM_T_82583:
	case WM_T_80003:
		/* generic reset */
		CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl | CTRL_PHY_RESET);
		CSR_WRITE_FLUSH(sc);
		delay(20000);
		CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl);
		CSR_WRITE_FLUSH(sc);
		delay(20000);

		if ((sc->sc_type == WM_T_82541)
		    || (sc->sc_type == WM_T_82541_2)
		    || (sc->sc_type == WM_T_82547)
		    || (sc->sc_type == WM_T_82547_2)) {
			/* workaround for igp are done in igp_reset() */
			/* XXX add code to set LED after phy reset */
		}
		break;
	case WM_T_ICH8:
	case WM_T_ICH9:
	case WM_T_ICH10:
	case WM_T_PCH:
	case WM_T_PCH2:
	case WM_T_PCH_LPT:
		/* generic reset */
		CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl | CTRL_PHY_RESET);
		CSR_WRITE_FLUSH(sc);
		delay(100);
		CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl);
		CSR_WRITE_FLUSH(sc);
		delay(150);
		break;
	default:
		panic("%s: %s: unknown type\n", device_xname(sc->sc_dev),
		    __func__);
		break;
	}

	/* release PHY semaphore */
	switch (sc->sc_type) {
	case WM_T_82571:
	case WM_T_82572:
	case WM_T_82573:
	case WM_T_82574:
	case WM_T_82583:
		 /* XXX should put sw semaphore, too */
		wm_put_swsm_semaphore(sc);
		break;
	case WM_T_82575:
	case WM_T_82576:
	case WM_T_82580:
	case WM_T_I350:
	case WM_T_I354:
	case WM_T_I210:
	case WM_T_I211:
	case WM_T_80003:
		wm_put_swfw_semaphore(sc, swfwphysem[sc->sc_funcid]);
		break;
	case WM_T_ICH8:
	case WM_T_ICH9:
	case WM_T_ICH10:
	case WM_T_PCH:
	case WM_T_PCH2:
	case WM_T_PCH_LPT:
		wm_put_swfwhw_semaphore(sc);
		break;
	default:
		/* nothing to do*/
		rv = 0;
		break;
	}

	/* get_cfg_done */
	wm_get_cfg_done(sc);

	/* extra setup */
	switch (sc->sc_type) {
	case WM_T_82542_2_0:
	case WM_T_82542_2_1:
	case WM_T_82543:
	case WM_T_82544:
	case WM_T_82540:
	case WM_T_82545:
	case WM_T_82545_3:
	case WM_T_82546:
	case WM_T_82546_3:
	case WM_T_82541_2:
	case WM_T_82547_2:
	case WM_T_82571:
	case WM_T_82572:
	case WM_T_82573:
	case WM_T_82574:
	case WM_T_82575:
	case WM_T_82576:
	case WM_T_82580:
	case WM_T_I350:
	case WM_T_I354:
	case WM_T_I210:
	case WM_T_I211:
	case WM_T_82583:
	case WM_T_80003:
		/* null */
		break;
	case WM_T_82541:
	case WM_T_82547:
		/* XXX Configure actively LED after PHY reset */
		break;
	case WM_T_ICH8:
	case WM_T_ICH9:
	case WM_T_ICH10:
	case WM_T_PCH:
	case WM_T_PCH2:
	case WM_T_PCH_LPT:
		/* Allow time for h/w to get to a quiescent state afer reset */
		delay(10*1000);

		if (sc->sc_type == WM_T_PCH)
			wm_hv_phy_workaround_ich8lan(sc);

		if (sc->sc_type == WM_T_PCH2)
			wm_lv_phy_workaround_ich8lan(sc);

		if ((sc->sc_type == WM_T_PCH) || (sc->sc_type == WM_T_PCH2)) {
			/*
			 * dummy read to clear the phy wakeup bit after lcd
			 * reset
			 */
			reg = wm_gmii_hv_readreg(sc->sc_dev, 1, BM_WUC);
		}

		/*
		 * XXX Configure the LCD with th extended configuration region
		 * in NVM
		 */

		/* Configure the LCD with the OEM bits in NVM */
		if ((sc->sc_type == WM_T_PCH) || (sc->sc_type == WM_T_PCH2)
		    || (sc->sc_type == WM_T_PCH_LPT)) {
			/*
			 * Disable LPLU.
			 * XXX It seems that 82567 has LPLU, too.
			 */
			reg = wm_gmii_hv_readreg(sc->sc_dev, 1, HV_OEM_BITS);
			reg &= ~(HV_OEM_BITS_A1KDIS| HV_OEM_BITS_LPLU);
			reg |= HV_OEM_BITS_ANEGNOW;
			wm_gmii_hv_writereg(sc->sc_dev, 1, HV_OEM_BITS, reg);
		}
		break;
	default:
		panic("%s: unknown type\n", __func__);
		break;
	}
}

/*
 * wm_get_phy_id_82575:
 *
 * Return PHY ID. Return -1 if it failed.
 */
static int
wm_get_phy_id_82575(struct wm_softc *sc)
{
	uint32_t reg;
	int phyid = -1;

	/* XXX */
	if ((sc->sc_flags & WM_F_SGMII) == 0)
		return -1;

	if (wm_sgmii_uses_mdio(sc)) {
		switch (sc->sc_type) {
		case WM_T_82575:
		case WM_T_82576:
			reg = CSR_READ(sc, WMREG_MDIC);
			phyid = (reg & MDIC_PHY_MASK) >> MDIC_PHY_SHIFT;
			break;
		case WM_T_82580:
		case WM_T_I350:
		case WM_T_I354:
		case WM_T_I210:
		case WM_T_I211:
			reg = CSR_READ(sc, WMREG_MDICNFG);
			phyid = (reg & MDICNFG_PHY_MASK) >> MDICNFG_PHY_SHIFT;
			break;
		default:
			return -1;
		}
	}

	return phyid;
}


/*
 * wm_gmii_mediainit:
 *
 *	Initialize media for use on 1000BASE-T devices.
 */
static void
wm_gmii_mediainit(struct wm_softc *sc, pci_product_id_t prodid)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct mii_data *mii = &sc->sc_mii;
	uint32_t reg;

	/* We have GMII. */
	sc->sc_flags |= WM_F_HAS_MII;

	if (sc->sc_type == WM_T_80003)
		sc->sc_tipg =  TIPG_1000T_80003_DFLT;
	else
		sc->sc_tipg = TIPG_1000T_DFLT;

	/* XXX Not for I354? FreeBSD's e1000_82575.c doesn't include it */
	if ((sc->sc_type == WM_T_82580)
	    || (sc->sc_type == WM_T_I350) || (sc->sc_type == WM_T_I210)
	    || (sc->sc_type == WM_T_I211)) {
		reg = CSR_READ(sc, WMREG_PHPM);
		reg &= ~PHPM_GO_LINK_D;
		CSR_WRITE(sc, WMREG_PHPM, reg);
	}

	/*
	 * Let the chip set speed/duplex on its own based on
	 * signals from the PHY.
	 * XXXbouyer - I'm not sure this is right for the 80003,
	 * the em driver only sets CTRL_SLU here - but it seems to work.
	 */
	sc->sc_ctrl |= CTRL_SLU;
	CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl);

	/* Initialize our media structures and probe the GMII. */
	mii->mii_ifp = ifp;

	/*
	 * Determine the PHY access method.
	 *
	 *  For SGMII, use SGMII specific method.
	 *
	 *  For some devices, we can determine the PHY access method
	 * from sc_type.
	 *
	 *  For ICH and PCH variants, it's difficult to determine the PHY
	 * access  method by sc_type, so use the PCI product ID for some
	 * devices.
	 * For other ICH8 variants, try to use igp's method. If the PHY
	 * can't detect, then use bm's method.
	 */
	switch (prodid) {
	case PCI_PRODUCT_INTEL_PCH_M_LM:
	case PCI_PRODUCT_INTEL_PCH_M_LC:
		/* 82577 */
		sc->sc_phytype = WMPHY_82577;
		break;
	case PCI_PRODUCT_INTEL_PCH_D_DM:
	case PCI_PRODUCT_INTEL_PCH_D_DC:
		/* 82578 */
		sc->sc_phytype = WMPHY_82578;
		break;
	case PCI_PRODUCT_INTEL_PCH2_LV_LM:
	case PCI_PRODUCT_INTEL_PCH2_LV_V:
		/* 82579 */
		sc->sc_phytype = WMPHY_82579;
		break;
	case PCI_PRODUCT_INTEL_82801I_BM:
	case PCI_PRODUCT_INTEL_82801J_R_BM_LM:
	case PCI_PRODUCT_INTEL_82801J_R_BM_LF:
	case PCI_PRODUCT_INTEL_82801J_D_BM_LM:
	case PCI_PRODUCT_INTEL_82801J_D_BM_LF:
	case PCI_PRODUCT_INTEL_82801J_R_BM_V:
		/* 82567 */
		sc->sc_phytype = WMPHY_BM;
		mii->mii_readreg = wm_gmii_bm_readreg;
		mii->mii_writereg = wm_gmii_bm_writereg;
		break;
	default:
		if (((sc->sc_flags & WM_F_SGMII) != 0)
		    && !wm_sgmii_uses_mdio(sc)){
			/* SGMII */
			mii->mii_readreg = wm_sgmii_readreg;
			mii->mii_writereg = wm_sgmii_writereg;
		} else if (sc->sc_type >= WM_T_80003) {
			/* 80003 */
			mii->mii_readreg = wm_gmii_i80003_readreg;
			mii->mii_writereg = wm_gmii_i80003_writereg;
		} else if (sc->sc_type >= WM_T_I210) {
			/* I210 and I211 */
			mii->mii_readreg = wm_gmii_gs40g_readreg;
			mii->mii_writereg = wm_gmii_gs40g_writereg;
		} else if (sc->sc_type >= WM_T_82580) {
			/* 82580, I350 and I354 */
			sc->sc_phytype = WMPHY_82580;
			mii->mii_readreg = wm_gmii_82580_readreg;
			mii->mii_writereg = wm_gmii_82580_writereg;
		} else if (sc->sc_type >= WM_T_82544) {
			/* 82544, 0, [56], [17], 8257[1234] and 82583 */
			mii->mii_readreg = wm_gmii_i82544_readreg;
			mii->mii_writereg = wm_gmii_i82544_writereg;
		} else {
			mii->mii_readreg = wm_gmii_i82543_readreg;
			mii->mii_writereg = wm_gmii_i82543_writereg;
		}
		break;
	}
	if ((sc->sc_type >= WM_T_PCH) && (sc->sc_type <= WM_T_PCH_LPT)) {
		/* All PCH* use _hv_ */
		mii->mii_readreg = wm_gmii_hv_readreg;
		mii->mii_writereg = wm_gmii_hv_writereg;
	}
	mii->mii_statchg = wm_gmii_statchg;

	wm_gmii_reset(sc);

	sc->sc_ethercom.ec_mii = &sc->sc_mii;
	ifmedia_init(&mii->mii_media, IFM_IMASK, wm_gmii_mediachange,
	    wm_gmii_mediastatus);

	if ((sc->sc_type == WM_T_82575) || (sc->sc_type == WM_T_82576)
	    || (sc->sc_type == WM_T_82580)
	    || (sc->sc_type == WM_T_I350) || (sc->sc_type == WM_T_I354)
	    || (sc->sc_type == WM_T_I210) || (sc->sc_type == WM_T_I211)) {
		if ((sc->sc_flags & WM_F_SGMII) == 0) {
			/* Attach only one port */
			mii_attach(sc->sc_dev, &sc->sc_mii, 0xffffffff, 1,
			    MII_OFFSET_ANY, MIIF_DOPAUSE);
		} else {
			int i, id;
			uint32_t ctrl_ext;

			id = wm_get_phy_id_82575(sc);
			if (id != -1) {
				mii_attach(sc->sc_dev, &sc->sc_mii, 0xffffffff,
				    id, MII_OFFSET_ANY, MIIF_DOPAUSE);
			}
			if ((id == -1)
			    || (LIST_FIRST(&mii->mii_phys) == NULL)) {
				/* Power on sgmii phy if it is disabled */
				ctrl_ext = CSR_READ(sc, WMREG_CTRL_EXT);
				CSR_WRITE(sc, WMREG_CTRL_EXT,
				    ctrl_ext &~ CTRL_EXT_SWDPIN(3));
				CSR_WRITE_FLUSH(sc);
				delay(300*1000); /* XXX too long */

				/* from 1 to 8 */
				for (i = 1; i < 8; i++)
					mii_attach(sc->sc_dev, &sc->sc_mii,
					    0xffffffff, i, MII_OFFSET_ANY,
					    MIIF_DOPAUSE);

				/* restore previous sfp cage power state */
				CSR_WRITE(sc, WMREG_CTRL_EXT, ctrl_ext);
			}
		}
	} else {
		mii_attach(sc->sc_dev, &sc->sc_mii, 0xffffffff, MII_PHY_ANY,
		    MII_OFFSET_ANY, MIIF_DOPAUSE);
	}

	/*
	 * If the MAC is PCH2 or PCH_LPT and failed to detect MII PHY, call
	 * wm_set_mdio_slow_mode_hv() for a workaround and retry.
	 */
	if (((sc->sc_type == WM_T_PCH2) || (sc->sc_type == WM_T_PCH_LPT)) &&
	    (LIST_FIRST(&mii->mii_phys) == NULL)) {
		wm_set_mdio_slow_mode_hv(sc);
		mii_attach(sc->sc_dev, &sc->sc_mii, 0xffffffff, MII_PHY_ANY,
		    MII_OFFSET_ANY, MIIF_DOPAUSE);
	}

	/*
	 * (For ICH8 variants)
	 * If PHY detection failed, use BM's r/w function and retry.
	 */
	if (LIST_FIRST(&mii->mii_phys) == NULL) {
		/* if failed, retry with *_bm_* */
		mii->mii_readreg = wm_gmii_bm_readreg;
		mii->mii_writereg = wm_gmii_bm_writereg;

		mii_attach(sc->sc_dev, &sc->sc_mii, 0xffffffff, MII_PHY_ANY,
		    MII_OFFSET_ANY, MIIF_DOPAUSE);
	}

	if (LIST_FIRST(&mii->mii_phys) == NULL) {
		/* Any PHY wasn't find */
		ifmedia_add(&mii->mii_media, IFM_ETHER|IFM_NONE, 0, NULL);
		ifmedia_set(&mii->mii_media, IFM_ETHER|IFM_NONE);
		sc->sc_phytype = WMPHY_NONE;
	} else {
		/*
		 * PHY Found!
		 * Check PHY type.
		 */
		uint32_t model;
		struct mii_softc *child;

		child = LIST_FIRST(&mii->mii_phys);
		if (device_is_a(child->mii_dev, "igphy")) {
			struct igphy_softc *isc = (struct igphy_softc *)child;

			model = isc->sc_mii.mii_mpd_model;
			if (model == MII_MODEL_yyINTEL_I82566)
				sc->sc_phytype = WMPHY_IGP_3;
		}

		ifmedia_set(&mii->mii_media, IFM_ETHER | IFM_AUTO);
	}
}

/*
 * wm_gmii_mediachange:	[ifmedia interface function]
 *
 *	Set hardware to newly-selected media on a 1000BASE-T device.
 */
static int
wm_gmii_mediachange(struct ifnet *ifp)
{
	struct wm_softc *sc = ifp->if_softc;
	struct ifmedia_entry *ife = sc->sc_mii.mii_media.ifm_cur;
	int rc;

	if ((ifp->if_flags & IFF_UP) == 0)
		return 0;

	sc->sc_ctrl &= ~(CTRL_SPEED_MASK | CTRL_FD);
	sc->sc_ctrl |= CTRL_SLU;
	if ((IFM_SUBTYPE(ife->ifm_media) == IFM_AUTO)
	    || (sc->sc_type > WM_T_82543)) {
		sc->sc_ctrl &= ~(CTRL_FRCSPD | CTRL_FRCFDX);
	} else {
		sc->sc_ctrl &= ~CTRL_ASDE;
		sc->sc_ctrl |= CTRL_FRCSPD | CTRL_FRCFDX;
		if (ife->ifm_media & IFM_FDX)
			sc->sc_ctrl |= CTRL_FD;
		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_10_T:
			sc->sc_ctrl |= CTRL_SPEED_10;
			break;
		case IFM_100_TX:
			sc->sc_ctrl |= CTRL_SPEED_100;
			break;
		case IFM_1000_T:
			sc->sc_ctrl |= CTRL_SPEED_1000;
			break;
		default:
			panic("wm_gmii_mediachange: bad media 0x%x",
			    ife->ifm_media);
		}
	}
	CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl);
	if (sc->sc_type <= WM_T_82543)
		wm_gmii_reset(sc);

	if ((rc = mii_mediachg(&sc->sc_mii)) == ENXIO)
		return 0;
	return rc;
}

/*
 * wm_gmii_mediastatus:	[ifmedia interface function]
 *
 *	Get the current interface media status on a 1000BASE-T device.
 */
static void
wm_gmii_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct wm_softc *sc = ifp->if_softc;

	ether_mediastatus(ifp, ifmr);
	ifmr->ifm_active = (ifmr->ifm_active & ~IFM_ETH_FMASK)
	    | sc->sc_flowflags;
}

#define	MDI_IO		CTRL_SWDPIN(2)
#define	MDI_DIR		CTRL_SWDPIO(2)	/* host -> PHY */
#define	MDI_CLK		CTRL_SWDPIN(3)

static void
wm_i82543_mii_sendbits(struct wm_softc *sc, uint32_t data, int nbits)
{
	uint32_t i, v;

	v = CSR_READ(sc, WMREG_CTRL);
	v &= ~(MDI_IO|MDI_CLK|(CTRL_SWDPIO_MASK << CTRL_SWDPIO_SHIFT));
	v |= MDI_DIR | CTRL_SWDPIO(3);

	for (i = 1 << (nbits - 1); i != 0; i >>= 1) {
		if (data & i)
			v |= MDI_IO;
		else
			v &= ~MDI_IO;
		CSR_WRITE(sc, WMREG_CTRL, v);
		CSR_WRITE_FLUSH(sc);
		delay(10);
		CSR_WRITE(sc, WMREG_CTRL, v | MDI_CLK);
		CSR_WRITE_FLUSH(sc);
		delay(10);
		CSR_WRITE(sc, WMREG_CTRL, v);
		CSR_WRITE_FLUSH(sc);
		delay(10);
	}
}

static uint32_t
wm_i82543_mii_recvbits(struct wm_softc *sc)
{
	uint32_t v, i, data = 0;

	v = CSR_READ(sc, WMREG_CTRL);
	v &= ~(MDI_IO|MDI_CLK|(CTRL_SWDPIO_MASK << CTRL_SWDPIO_SHIFT));
	v |= CTRL_SWDPIO(3);

	CSR_WRITE(sc, WMREG_CTRL, v);
	CSR_WRITE_FLUSH(sc);
	delay(10);
	CSR_WRITE(sc, WMREG_CTRL, v | MDI_CLK);
	CSR_WRITE_FLUSH(sc);
	delay(10);
	CSR_WRITE(sc, WMREG_CTRL, v);
	CSR_WRITE_FLUSH(sc);
	delay(10);

	for (i = 0; i < 16; i++) {
		data <<= 1;
		CSR_WRITE(sc, WMREG_CTRL, v | MDI_CLK);
		CSR_WRITE_FLUSH(sc);
		delay(10);
		if (CSR_READ(sc, WMREG_CTRL) & MDI_IO)
			data |= 1;
		CSR_WRITE(sc, WMREG_CTRL, v);
		CSR_WRITE_FLUSH(sc);
		delay(10);
	}

	CSR_WRITE(sc, WMREG_CTRL, v | MDI_CLK);
	CSR_WRITE_FLUSH(sc);
	delay(10);
	CSR_WRITE(sc, WMREG_CTRL, v);
	CSR_WRITE_FLUSH(sc);
	delay(10);

	return data;
}

#undef MDI_IO
#undef MDI_DIR
#undef MDI_CLK

/*
 * wm_gmii_i82543_readreg:	[mii interface function]
 *
 *	Read a PHY register on the GMII (i82543 version).
 */
static int
wm_gmii_i82543_readreg(device_t self, int phy, int reg)
{
	struct wm_softc *sc = device_private(self);
	int rv;

	wm_i82543_mii_sendbits(sc, 0xffffffffU, 32);
	wm_i82543_mii_sendbits(sc, reg | (phy << 5) |
	    (MII_COMMAND_READ << 10) | (MII_COMMAND_START << 12), 14);
	rv = wm_i82543_mii_recvbits(sc) & 0xffff;

	DPRINTF(WM_DEBUG_GMII,
	    ("%s: GMII: read phy %d reg %d -> 0x%04x\n",
	    device_xname(sc->sc_dev), phy, reg, rv));

	return rv;
}

/*
 * wm_gmii_i82543_writereg:	[mii interface function]
 *
 *	Write a PHY register on the GMII (i82543 version).
 */
static void
wm_gmii_i82543_writereg(device_t self, int phy, int reg, int val)
{
	struct wm_softc *sc = device_private(self);

	wm_i82543_mii_sendbits(sc, 0xffffffffU, 32);
	wm_i82543_mii_sendbits(sc, val | (MII_COMMAND_ACK << 16) |
	    (reg << 18) | (phy << 23) | (MII_COMMAND_WRITE << 28) |
	    (MII_COMMAND_START << 30), 32);
}

/*
 * wm_gmii_i82544_readreg:	[mii interface function]
 *
 *	Read a PHY register on the GMII.
 */
static int
wm_gmii_i82544_readreg(device_t self, int phy, int reg)
{
	struct wm_softc *sc = device_private(self);
	uint32_t mdic = 0;
	int i, rv;

	CSR_WRITE(sc, WMREG_MDIC, MDIC_OP_READ | MDIC_PHYADD(phy) |
	    MDIC_REGADD(reg));

	for (i = 0; i < WM_GEN_POLL_TIMEOUT * 3; i++) {
		mdic = CSR_READ(sc, WMREG_MDIC);
		if (mdic & MDIC_READY)
			break;
		delay(50);
	}

	if ((mdic & MDIC_READY) == 0) {
		log(LOG_WARNING, "%s: MDIC read timed out: phy %d reg %d\n",
		    device_xname(sc->sc_dev), phy, reg);
		rv = 0;
	} else if (mdic & MDIC_E) {
#if 0 /* This is normal if no PHY is present. */
		log(LOG_WARNING, "%s: MDIC read error: phy %d reg %d\n",
		    device_xname(sc->sc_dev), phy, reg);
#endif
		rv = 0;
	} else {
		rv = MDIC_DATA(mdic);
		if (rv == 0xffff)
			rv = 0;
	}

	return rv;
}

/*
 * wm_gmii_i82544_writereg:	[mii interface function]
 *
 *	Write a PHY register on the GMII.
 */
static void
wm_gmii_i82544_writereg(device_t self, int phy, int reg, int val)
{
	struct wm_softc *sc = device_private(self);
	uint32_t mdic = 0;
	int i;

	CSR_WRITE(sc, WMREG_MDIC, MDIC_OP_WRITE | MDIC_PHYADD(phy) |
	    MDIC_REGADD(reg) | MDIC_DATA(val));

	for (i = 0; i < WM_GEN_POLL_TIMEOUT * 3; i++) {
		mdic = CSR_READ(sc, WMREG_MDIC);
		if (mdic & MDIC_READY)
			break;
		delay(50);
	}

	if ((mdic & MDIC_READY) == 0)
		log(LOG_WARNING, "%s: MDIC write timed out: phy %d reg %d\n",
		    device_xname(sc->sc_dev), phy, reg);
	else if (mdic & MDIC_E)
		log(LOG_WARNING, "%s: MDIC write error: phy %d reg %d\n",
		    device_xname(sc->sc_dev), phy, reg);
}

/*
 * wm_gmii_i80003_readreg:	[mii interface function]
 *
 *	Read a PHY register on the kumeran
 * This could be handled by the PHY layer if we didn't have to lock the
 * ressource ...
 */
static int
wm_gmii_i80003_readreg(device_t self, int phy, int reg)
{
	struct wm_softc *sc = device_private(self);
	int sem;
	int rv;

	if (phy != 1) /* only one PHY on kumeran bus */
		return 0;

	sem = swfwphysem[sc->sc_funcid];
	if (wm_get_swfw_semaphore(sc, sem)) {
		aprint_error_dev(sc->sc_dev, "%s: failed to get semaphore\n",
		    __func__);
		return 0;
	}

	if ((reg & GG82563_MAX_REG_ADDRESS) < GG82563_MIN_ALT_REG) {
		wm_gmii_i82544_writereg(self, phy, GG82563_PHY_PAGE_SELECT,
		    reg >> GG82563_PAGE_SHIFT);
	} else {
		wm_gmii_i82544_writereg(self, phy, GG82563_PHY_PAGE_SELECT_ALT,
		    reg >> GG82563_PAGE_SHIFT);
	}
	/* Wait more 200us for a bug of the ready bit in the MDIC register */
	delay(200);
	rv = wm_gmii_i82544_readreg(self, phy, reg & GG82563_MAX_REG_ADDRESS);
	delay(200);

	wm_put_swfw_semaphore(sc, sem);
	return rv;
}

/*
 * wm_gmii_i80003_writereg:	[mii interface function]
 *
 *	Write a PHY register on the kumeran.
 * This could be handled by the PHY layer if we didn't have to lock the
 * ressource ...
 */
static void
wm_gmii_i80003_writereg(device_t self, int phy, int reg, int val)
{
	struct wm_softc *sc = device_private(self);
	int sem;

	if (phy != 1) /* only one PHY on kumeran bus */
		return;

	sem = swfwphysem[sc->sc_funcid];
	if (wm_get_swfw_semaphore(sc, sem)) {
		aprint_error_dev(sc->sc_dev, "%s: failed to get semaphore\n",
		    __func__);
		return;
	}

	if ((reg & GG82563_MAX_REG_ADDRESS) < GG82563_MIN_ALT_REG) {
		wm_gmii_i82544_writereg(self, phy, GG82563_PHY_PAGE_SELECT,
		    reg >> GG82563_PAGE_SHIFT);
	} else {
		wm_gmii_i82544_writereg(self, phy, GG82563_PHY_PAGE_SELECT_ALT,
		    reg >> GG82563_PAGE_SHIFT);
	}
	/* Wait more 200us for a bug of the ready bit in the MDIC register */
	delay(200);
	wm_gmii_i82544_writereg(self, phy, reg & GG82563_MAX_REG_ADDRESS, val);
	delay(200);

	wm_put_swfw_semaphore(sc, sem);
}

/*
 * wm_gmii_bm_readreg:	[mii interface function]
 *
 *	Read a PHY register on the kumeran
 * This could be handled by the PHY layer if we didn't have to lock the
 * ressource ...
 */
static int
wm_gmii_bm_readreg(device_t self, int phy, int reg)
{
	struct wm_softc *sc = device_private(self);
	int sem;
	int rv;

	sem = swfwphysem[sc->sc_funcid];
	if (wm_get_swfw_semaphore(sc, sem)) {
		aprint_error_dev(sc->sc_dev, "%s: failed to get semaphore\n",
		    __func__);
		return 0;
	}

	if (reg > BME1000_MAX_MULTI_PAGE_REG) {
		if (phy == 1)
			wm_gmii_i82544_writereg(self, phy, MII_IGPHY_PAGE_SELECT,
			    reg);
		else
			wm_gmii_i82544_writereg(self, phy,
			    GG82563_PHY_PAGE_SELECT,
			    reg >> GG82563_PAGE_SHIFT);
	}

	rv = wm_gmii_i82544_readreg(self, phy, reg & GG82563_MAX_REG_ADDRESS);
	wm_put_swfw_semaphore(sc, sem);
	return rv;
}

/*
 * wm_gmii_bm_writereg:	[mii interface function]
 *
 *	Write a PHY register on the kumeran.
 * This could be handled by the PHY layer if we didn't have to lock the
 * ressource ...
 */
static void
wm_gmii_bm_writereg(device_t self, int phy, int reg, int val)
{
	struct wm_softc *sc = device_private(self);
	int sem;

	sem = swfwphysem[sc->sc_funcid];
	if (wm_get_swfw_semaphore(sc, sem)) {
		aprint_error_dev(sc->sc_dev, "%s: failed to get semaphore\n",
		    __func__);
		return;
	}

	if (reg > BME1000_MAX_MULTI_PAGE_REG) {
		if (phy == 1)
			wm_gmii_i82544_writereg(self, phy, MII_IGPHY_PAGE_SELECT,
			    reg);
		else
			wm_gmii_i82544_writereg(self, phy,
			    GG82563_PHY_PAGE_SELECT,
			    reg >> GG82563_PAGE_SHIFT);
	}

	wm_gmii_i82544_writereg(self, phy, reg & GG82563_MAX_REG_ADDRESS, val);
	wm_put_swfw_semaphore(sc, sem);
}

static void
wm_access_phy_wakeup_reg_bm(device_t self, int offset, int16_t *val, int rd)
{
	struct wm_softc *sc = device_private(self);
	uint16_t regnum = BM_PHY_REG_NUM(offset);
	uint16_t wuce;

	/* XXX Gig must be disabled for MDIO accesses to page 800 */
	if (sc->sc_type == WM_T_PCH) {
		/* XXX e1000 driver do nothing... why? */
	}

	/* Set page 769 */
	wm_gmii_i82544_writereg(self, 1, MII_IGPHY_PAGE_SELECT,
	    BM_WUC_ENABLE_PAGE << BME1000_PAGE_SHIFT);

	wuce = wm_gmii_i82544_readreg(self, 1, BM_WUC_ENABLE_REG);

	wuce &= ~BM_WUC_HOST_WU_BIT;
	wm_gmii_i82544_writereg(self, 1, BM_WUC_ENABLE_REG,
	    wuce | BM_WUC_ENABLE_BIT);

	/* Select page 800 */
	wm_gmii_i82544_writereg(self, 1, MII_IGPHY_PAGE_SELECT,
	    BM_WUC_PAGE << BME1000_PAGE_SHIFT);

	/* Write page 800 */
	wm_gmii_i82544_writereg(self, 1, BM_WUC_ADDRESS_OPCODE, regnum);

	if (rd)
		*val = wm_gmii_i82544_readreg(self, 1, BM_WUC_DATA_OPCODE);
	else
		wm_gmii_i82544_writereg(self, 1, BM_WUC_DATA_OPCODE, *val);

	/* Set page 769 */
	wm_gmii_i82544_writereg(self, 1, MII_IGPHY_PAGE_SELECT,
	    BM_WUC_ENABLE_PAGE << BME1000_PAGE_SHIFT);

	wm_gmii_i82544_writereg(self, 1, BM_WUC_ENABLE_REG, wuce);
}

/*
 * wm_gmii_hv_readreg:	[mii interface function]
 *
 *	Read a PHY register on the kumeran
 * This could be handled by the PHY layer if we didn't have to lock the
 * ressource ...
 */
static int
wm_gmii_hv_readreg(device_t self, int phy, int reg)
{
	struct wm_softc *sc = device_private(self);
	uint16_t page = BM_PHY_REG_PAGE(reg);
	uint16_t regnum = BM_PHY_REG_NUM(reg);
	uint16_t val;
	int rv;

	if (wm_get_swfwhw_semaphore(sc)) {
		aprint_error_dev(sc->sc_dev, "%s: failed to get semaphore\n",
		    __func__);
		return 0;
	}

	/* XXX Workaround failure in MDIO access while cable is disconnected */
	if (sc->sc_phytype == WMPHY_82577) {
		/* XXX must write */
	}

	/* Page 800 works differently than the rest so it has its own func */
	if (page == BM_WUC_PAGE) {
		wm_access_phy_wakeup_reg_bm(self, reg, &val, 1);
		return val;
	}

	/*
	 * Lower than page 768 works differently than the rest so it has its
	 * own func
	 */
	if ((page > 0) && (page < HV_INTC_FC_PAGE_START)) {
		printf("gmii_hv_readreg!!!\n");
		return 0;
	}

	if (regnum > BME1000_MAX_MULTI_PAGE_REG) {
		wm_gmii_i82544_writereg(self, 1, MII_IGPHY_PAGE_SELECT,
		    page << BME1000_PAGE_SHIFT);
	}

	rv = wm_gmii_i82544_readreg(self, phy, regnum & IGPHY_MAXREGADDR);
	wm_put_swfwhw_semaphore(sc);
	return rv;
}

/*
 * wm_gmii_hv_writereg:	[mii interface function]
 *
 *	Write a PHY register on the kumeran.
 * This could be handled by the PHY layer if we didn't have to lock the
 * ressource ...
 */
static void
wm_gmii_hv_writereg(device_t self, int phy, int reg, int val)
{
	struct wm_softc *sc = device_private(self);
	uint16_t page = BM_PHY_REG_PAGE(reg);
	uint16_t regnum = BM_PHY_REG_NUM(reg);

	if (wm_get_swfwhw_semaphore(sc)) {
		aprint_error_dev(sc->sc_dev, "%s: failed to get semaphore\n",
		    __func__);
		return;
	}

	/* XXX Workaround failure in MDIO access while cable is disconnected */

	/* Page 800 works differently than the rest so it has its own func */
	if (page == BM_WUC_PAGE) {
		uint16_t tmp;

		tmp = val;
		wm_access_phy_wakeup_reg_bm(self, reg, &tmp, 0);
		return;
	}

	/*
	 * Lower than page 768 works differently than the rest so it has its
	 * own func
	 */
	if ((page > 0) && (page < HV_INTC_FC_PAGE_START)) {
		printf("gmii_hv_writereg!!!\n");
		return;
	}

	/*
	 * XXX Workaround MDIO accesses being disabled after entering IEEE
	 * Power Down (whenever bit 11 of the PHY control register is set)
	 */

	if (regnum > BME1000_MAX_MULTI_PAGE_REG) {
		wm_gmii_i82544_writereg(self, 1, MII_IGPHY_PAGE_SELECT,
		    page << BME1000_PAGE_SHIFT);
	}

	wm_gmii_i82544_writereg(self, phy, regnum & IGPHY_MAXREGADDR, val);
	wm_put_swfwhw_semaphore(sc);
}

/*
 * wm_gmii_82580_readreg:	[mii interface function]
 *
 *	Read a PHY register on the 82580 and I350.
 * This could be handled by the PHY layer if we didn't have to lock the
 * ressource ...
 */
static int
wm_gmii_82580_readreg(device_t self, int phy, int reg)
{
	struct wm_softc *sc = device_private(self);
	int sem;
	int rv;

	sem = swfwphysem[sc->sc_funcid];
	if (wm_get_swfw_semaphore(sc, sem)) {
		aprint_error_dev(sc->sc_dev, "%s: failed to get semaphore\n",
		    __func__);
		return 0;
	}

	rv = wm_gmii_i82544_readreg(self, phy, reg);

	wm_put_swfw_semaphore(sc, sem);
	return rv;
}

/*
 * wm_gmii_82580_writereg:	[mii interface function]
 *
 *	Write a PHY register on the 82580 and I350.
 * This could be handled by the PHY layer if we didn't have to lock the
 * ressource ...
 */
static void
wm_gmii_82580_writereg(device_t self, int phy, int reg, int val)
{
	struct wm_softc *sc = device_private(self);
	int sem;

	sem = swfwphysem[sc->sc_funcid];
	if (wm_get_swfw_semaphore(sc, sem)) {
		aprint_error_dev(sc->sc_dev, "%s: failed to get semaphore\n",
		    __func__);
		return;
	}

	wm_gmii_i82544_writereg(self, phy, reg, val);

	wm_put_swfw_semaphore(sc, sem);
}

/*
 * wm_gmii_gs40g_readreg:	[mii interface function]
 *
 *	Read a PHY register on the I2100 and I211.
 * This could be handled by the PHY layer if we didn't have to lock the
 * ressource ...
 */
static int
wm_gmii_gs40g_readreg(device_t self, int phy, int reg)
{
	struct wm_softc *sc = device_private(self);
	int sem;
	int page, offset;
	int rv;

	/* Acquire semaphore */
	sem = swfwphysem[sc->sc_funcid];
	if (wm_get_swfw_semaphore(sc, sem)) {
		aprint_error_dev(sc->sc_dev, "%s: failed to get semaphore\n",
		    __func__);
		return 0;
	}

	/* Page select */
	page = reg >> GS40G_PAGE_SHIFT;
	wm_gmii_i82544_writereg(self, phy, GS40G_PAGE_SELECT, page);

	/* Read reg */
	offset = reg & GS40G_OFFSET_MASK;
	rv = wm_gmii_i82544_readreg(self, phy, offset);

	wm_put_swfw_semaphore(sc, sem);
	return rv;
}

/*
 * wm_gmii_gs40g_writereg:	[mii interface function]
 *
 *	Write a PHY register on the I210 and I211.
 * This could be handled by the PHY layer if we didn't have to lock the
 * ressource ...
 */
static void
wm_gmii_gs40g_writereg(device_t self, int phy, int reg, int val)
{
	struct wm_softc *sc = device_private(self);
	int sem;
	int page, offset;

	/* Acquire semaphore */
	sem = swfwphysem[sc->sc_funcid];
	if (wm_get_swfw_semaphore(sc, sem)) {
		aprint_error_dev(sc->sc_dev, "%s: failed to get semaphore\n",
		    __func__);
		return;
	}

	/* Page select */
	page = reg >> GS40G_PAGE_SHIFT;
	wm_gmii_i82544_writereg(self, phy, GS40G_PAGE_SELECT, page);

	/* Write reg */
	offset = reg & GS40G_OFFSET_MASK;
	wm_gmii_i82544_writereg(self, phy, offset, val);

	/* Release semaphore */
	wm_put_swfw_semaphore(sc, sem);
}

/*
 * wm_gmii_statchg:	[mii interface function]
 *
 *	Callback from MII layer when media changes.
 */
static void
wm_gmii_statchg(struct ifnet *ifp)
{
	struct wm_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->sc_mii;

	sc->sc_ctrl &= ~(CTRL_TFCE | CTRL_RFCE);
	sc->sc_tctl &= ~TCTL_COLD(0x3ff);
	sc->sc_fcrtl &= ~FCRTL_XONE;

	/*
	 * Get flow control negotiation result.
	 */
	if (IFM_SUBTYPE(mii->mii_media.ifm_cur->ifm_media) == IFM_AUTO &&
	    (mii->mii_media_active & IFM_ETH_FMASK) != sc->sc_flowflags) {
		sc->sc_flowflags = mii->mii_media_active & IFM_ETH_FMASK;
		mii->mii_media_active &= ~IFM_ETH_FMASK;
	}

	if (sc->sc_flowflags & IFM_FLOW) {
		if (sc->sc_flowflags & IFM_ETH_TXPAUSE) {
			sc->sc_ctrl |= CTRL_TFCE;
			sc->sc_fcrtl |= FCRTL_XONE;
		}
		if (sc->sc_flowflags & IFM_ETH_RXPAUSE)
			sc->sc_ctrl |= CTRL_RFCE;
	}

	if (sc->sc_mii.mii_media_active & IFM_FDX) {
		DPRINTF(WM_DEBUG_LINK,
		    ("%s: LINK: statchg: FDX\n", ifp->if_xname));
		sc->sc_tctl |= TCTL_COLD(TX_COLLISION_DISTANCE_FDX);
	} else {
		DPRINTF(WM_DEBUG_LINK,
		    ("%s: LINK: statchg: HDX\n", ifp->if_xname));
		sc->sc_tctl |= TCTL_COLD(TX_COLLISION_DISTANCE_HDX);
	}

	CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl);
	CSR_WRITE(sc, WMREG_TCTL, sc->sc_tctl);
	CSR_WRITE(sc, (sc->sc_type < WM_T_82543) ? WMREG_OLD_FCRTL
						 : WMREG_FCRTL, sc->sc_fcrtl);
	if (sc->sc_type == WM_T_80003) {
		switch (IFM_SUBTYPE(sc->sc_mii.mii_media_active)) {
		case IFM_1000_T:
			wm_kmrn_writereg(sc, KUMCTRLSTA_OFFSET_HD_CTRL,
			    KUMCTRLSTA_HD_CTRL_1000_DEFAULT);
			sc->sc_tipg =  TIPG_1000T_80003_DFLT;
			break;
		default:
			wm_kmrn_writereg(sc, KUMCTRLSTA_OFFSET_HD_CTRL,
			    KUMCTRLSTA_HD_CTRL_10_100_DEFAULT);
			sc->sc_tipg =  TIPG_10_100_80003_DFLT;
			break;
		}
		CSR_WRITE(sc, WMREG_TIPG, sc->sc_tipg);
	}
}

/*
 * wm_kmrn_readreg:
 *
 *	Read a kumeran register
 */
static int
wm_kmrn_readreg(struct wm_softc *sc, int reg)
{
	int rv;

	if (sc->sc_flags & WM_F_LOCK_SWFW) {
		if (wm_get_swfw_semaphore(sc, SWFW_MAC_CSR_SM)) {
			aprint_error_dev(sc->sc_dev,
			    "%s: failed to get semaphore\n", __func__);
			return 0;
		}
	} else if (sc->sc_flags & WM_F_LOCK_EXTCNF) {
		if (wm_get_swfwhw_semaphore(sc)) {
			aprint_error_dev(sc->sc_dev,
			    "%s: failed to get semaphore\n", __func__);
			return 0;
		}
	}

	CSR_WRITE(sc, WMREG_KUMCTRLSTA,
	    ((reg << KUMCTRLSTA_OFFSET_SHIFT) & KUMCTRLSTA_OFFSET) |
	    KUMCTRLSTA_REN);
	CSR_WRITE_FLUSH(sc);
	delay(2);

	rv = CSR_READ(sc, WMREG_KUMCTRLSTA) & KUMCTRLSTA_MASK;

	if (sc->sc_flags & WM_F_LOCK_SWFW)
		wm_put_swfw_semaphore(sc, SWFW_MAC_CSR_SM);
	else if (sc->sc_flags & WM_F_LOCK_EXTCNF)
		wm_put_swfwhw_semaphore(sc);

	return rv;
}

/*
 * wm_kmrn_writereg:
 *
 *	Write a kumeran register
 */
static void
wm_kmrn_writereg(struct wm_softc *sc, int reg, int val)
{

	if (sc->sc_flags & WM_F_LOCK_SWFW) {
		if (wm_get_swfw_semaphore(sc, SWFW_MAC_CSR_SM)) {
			aprint_error_dev(sc->sc_dev,
			    "%s: failed to get semaphore\n", __func__);
			return;
		}
	} else if (sc->sc_flags & WM_F_LOCK_EXTCNF) {
		if (wm_get_swfwhw_semaphore(sc)) {
			aprint_error_dev(sc->sc_dev,
			    "%s: failed to get semaphore\n", __func__);
			return;
		}
	}

	CSR_WRITE(sc, WMREG_KUMCTRLSTA,
	    ((reg << KUMCTRLSTA_OFFSET_SHIFT) & KUMCTRLSTA_OFFSET) |
	    (val & KUMCTRLSTA_MASK));

	if (sc->sc_flags & WM_F_LOCK_SWFW)
		wm_put_swfw_semaphore(sc, SWFW_MAC_CSR_SM);
	else if (sc->sc_flags & WM_F_LOCK_EXTCNF)
		wm_put_swfwhw_semaphore(sc);
}

/* SGMII related */

/*
 * wm_sgmii_uses_mdio
 *
 * Check whether the transaction is to the internal PHY or the external
 * MDIO interface. Return true if it's MDIO.
 */
static bool
wm_sgmii_uses_mdio(struct wm_softc *sc)
{
	uint32_t reg;
	bool ismdio = false;

	switch (sc->sc_type) {
	case WM_T_82575:
	case WM_T_82576:
		reg = CSR_READ(sc, WMREG_MDIC);
		ismdio = ((reg & MDIC_DEST) != 0);
		break;
	case WM_T_82580:
	case WM_T_I350:
	case WM_T_I354:
	case WM_T_I210:
	case WM_T_I211:
		reg = CSR_READ(sc, WMREG_MDICNFG);
		ismdio = ((reg & MDICNFG_DEST) != 0);
		break;
	default:
		break;
	}

	return ismdio;
}

/*
 * wm_sgmii_readreg:	[mii interface function]
 *
 *	Read a PHY register on the SGMII
 * This could be handled by the PHY layer if we didn't have to lock the
 * ressource ...
 */
static int
wm_sgmii_readreg(device_t self, int phy, int reg)
{
	struct wm_softc *sc = device_private(self);
	uint32_t i2ccmd;
	int i, rv;

	if (wm_get_swfw_semaphore(sc, swfwphysem[sc->sc_funcid])) {
		aprint_error_dev(sc->sc_dev, "%s: failed to get semaphore\n",
		    __func__);
		return 0;
	}

	i2ccmd = (reg << I2CCMD_REG_ADDR_SHIFT)
	    | (phy << I2CCMD_PHY_ADDR_SHIFT)
	    | I2CCMD_OPCODE_READ;
	CSR_WRITE(sc, WMREG_I2CCMD, i2ccmd);

	/* Poll the ready bit */
	for (i = 0; i < I2CCMD_PHY_TIMEOUT; i++) {
		delay(50);
		i2ccmd = CSR_READ(sc, WMREG_I2CCMD);
		if (i2ccmd & I2CCMD_READY)
			break;
	}
	if ((i2ccmd & I2CCMD_READY) == 0)
		aprint_error_dev(sc->sc_dev, "I2CCMD Read did not complete\n");
	if ((i2ccmd & I2CCMD_ERROR) != 0)
		aprint_error_dev(sc->sc_dev, "I2CCMD Error bit set\n");

	rv = ((i2ccmd >> 8) & 0x00ff) | ((i2ccmd << 8) & 0xff00);

	wm_put_swfw_semaphore(sc, swfwphysem[sc->sc_funcid]);
	return rv;
}

/*
 * wm_sgmii_writereg:	[mii interface function]
 *
 *	Write a PHY register on the SGMII.
 * This could be handled by the PHY layer if we didn't have to lock the
 * ressource ...
 */
static void
wm_sgmii_writereg(device_t self, int phy, int reg, int val)
{
	struct wm_softc *sc = device_private(self);
	uint32_t i2ccmd;
	int i;
	int val_swapped;

	if (wm_get_swfw_semaphore(sc, swfwphysem[sc->sc_funcid])) {
		aprint_error_dev(sc->sc_dev, "%s: failed to get semaphore\n",
		    __func__);
		return;
	}
	/* Swap the data bytes for the I2C interface */
	val_swapped = ((val >> 8) & 0x00FF) | ((val << 8) & 0xFF00);
	i2ccmd = (reg << I2CCMD_REG_ADDR_SHIFT)
	    | (phy << I2CCMD_PHY_ADDR_SHIFT)
	    | I2CCMD_OPCODE_WRITE | val_swapped;
	CSR_WRITE(sc, WMREG_I2CCMD, i2ccmd);

	/* Poll the ready bit */
	for (i = 0; i < I2CCMD_PHY_TIMEOUT; i++) {
		delay(50);
		i2ccmd = CSR_READ(sc, WMREG_I2CCMD);
		if (i2ccmd & I2CCMD_READY)
			break;
	}
	if ((i2ccmd & I2CCMD_READY) == 0)
		aprint_error_dev(sc->sc_dev, "I2CCMD Write did not complete\n");
	if ((i2ccmd & I2CCMD_ERROR) != 0)
		aprint_error_dev(sc->sc_dev, "I2CCMD Error bit set\n");

	wm_put_swfw_semaphore(sc, SWFW_PHY0_SM);
}

/* TBI related */

/*
 * wm_tbi_mediainit:
 *
 *	Initialize media for use on 1000BASE-X devices.
 */
static void
wm_tbi_mediainit(struct wm_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	const char *sep = "";

	if (sc->sc_type < WM_T_82543)
		sc->sc_tipg = TIPG_WM_DFLT;
	else
		sc->sc_tipg = TIPG_LG_DFLT;

	sc->sc_tbi_serdes_anegticks = 5;

	/* Initialize our media structures */
	sc->sc_mii.mii_ifp = ifp;
	sc->sc_ethercom.ec_mii = &sc->sc_mii;

	if ((sc->sc_type >= WM_T_82575)
	    && (sc->sc_mediatype == WM_MEDIATYPE_SERDES))
		ifmedia_init(&sc->sc_mii.mii_media, IFM_IMASK,
		    wm_serdes_mediachange, wm_serdes_mediastatus);
	else
		ifmedia_init(&sc->sc_mii.mii_media, IFM_IMASK,
		    wm_tbi_mediachange, wm_tbi_mediastatus);

	/*
	 * SWD Pins:
	 *
	 *	0 = Link LED (output)
	 *	1 = Loss Of Signal (input)
	 */
	sc->sc_ctrl |= CTRL_SWDPIO(0);

	/* XXX Perhaps this is only for TBI */
	if (sc->sc_mediatype != WM_MEDIATYPE_SERDES)
		sc->sc_ctrl &= ~CTRL_SWDPIO(1);

	if (sc->sc_mediatype == WM_MEDIATYPE_SERDES)
		sc->sc_ctrl &= ~CTRL_LRST;

	CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl);

#define	ADD(ss, mm, dd)							\
do {									\
	aprint_normal("%s%s", sep, ss);					\
	ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|(mm), (dd), NULL);	\
	sep = ", ";							\
} while (/*CONSTCOND*/0)

	aprint_normal_dev(sc->sc_dev, "");

	/* Only 82545 is LX */
	if (sc->sc_type == WM_T_82545) {
		ADD("1000baseLX", IFM_1000_LX, ANAR_X_HD);
		ADD("1000baseLX-FDX", IFM_1000_LX|IFM_FDX, ANAR_X_FD);
	} else {
		ADD("1000baseSX", IFM_1000_SX, ANAR_X_HD);
		ADD("1000baseSX-FDX", IFM_1000_SX|IFM_FDX, ANAR_X_FD);
	}
	ADD("auto", IFM_AUTO, ANAR_X_FD|ANAR_X_HD);
	aprint_normal("\n");

#undef ADD

	ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER | IFM_AUTO);
}

/*
 * wm_tbi_mediachange:	[ifmedia interface function]
 *
 *	Set hardware to newly-selected media on a 1000BASE-X device.
 */
static int
wm_tbi_mediachange(struct ifnet *ifp)
{
	struct wm_softc *sc = ifp->if_softc;
	struct ifmedia_entry *ife = sc->sc_mii.mii_media.ifm_cur;
	uint32_t status;
	int i;

	if (sc->sc_mediatype == WM_MEDIATYPE_SERDES) {
		/* XXX need some work for >= 82571 and < 82575 */
		if (sc->sc_type < WM_T_82575)
			return 0;
	}

	if ((sc->sc_type == WM_T_82571) || (sc->sc_type == WM_T_82572)
	    || (sc->sc_type >= WM_T_82575))
		CSR_WRITE(sc, WMREG_SCTL, SCTL_DISABLE_SERDES_LOOPBACK);

	sc->sc_ctrl &= ~CTRL_LRST;
	sc->sc_txcw = TXCW_ANE;
	if (IFM_SUBTYPE(ife->ifm_media) == IFM_AUTO)
		sc->sc_txcw |= TXCW_FD | TXCW_HD;
	else if (ife->ifm_media & IFM_FDX)
		sc->sc_txcw |= TXCW_FD;
	else
		sc->sc_txcw |= TXCW_HD;

	if ((sc->sc_mii.mii_media.ifm_media & IFM_FLOW) != 0)
		sc->sc_txcw |= TXCW_SYM_PAUSE | TXCW_ASYM_PAUSE;

	DPRINTF(WM_DEBUG_LINK,("%s: sc_txcw = 0x%x after autoneg check\n",
		    device_xname(sc->sc_dev), sc->sc_txcw));
	CSR_WRITE(sc, WMREG_TXCW, sc->sc_txcw);
	CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl);
	CSR_WRITE_FLUSH(sc);
	delay(1000);

	i = CSR_READ(sc, WMREG_CTRL) & CTRL_SWDPIN(1);
	DPRINTF(WM_DEBUG_LINK,("%s: i = 0x%x\n", device_xname(sc->sc_dev),i));

	/*
	 * On 82544 chips and later, the CTRL_SWDPIN(1) bit will be set if the
	 * optics detect a signal, 0 if they don't.
	 */
	if (((i != 0) && (sc->sc_type > WM_T_82544)) || (i == 0)) {
		/* Have signal; wait for the link to come up. */
		for (i = 0; i < WM_LINKUP_TIMEOUT; i++) {
			delay(10000);
			if (CSR_READ(sc, WMREG_STATUS) & STATUS_LU)
				break;
		}

		DPRINTF(WM_DEBUG_LINK,("%s: i = %d after waiting for link\n",
			    device_xname(sc->sc_dev),i));

		status = CSR_READ(sc, WMREG_STATUS);
		DPRINTF(WM_DEBUG_LINK,
		    ("%s: status after final read = 0x%x, STATUS_LU = 0x%x\n",
			device_xname(sc->sc_dev),status, STATUS_LU));
		if (status & STATUS_LU) {
			/* Link is up. */
			DPRINTF(WM_DEBUG_LINK,
			    ("%s: LINK: set media -> link up %s\n",
			    device_xname(sc->sc_dev),
			    (status & STATUS_FD) ? "FDX" : "HDX"));

			/*
			 * NOTE: CTRL will update TFCE and RFCE automatically,
			 * so we should update sc->sc_ctrl
			 */
			sc->sc_ctrl = CSR_READ(sc, WMREG_CTRL);
			sc->sc_tctl &= ~TCTL_COLD(0x3ff);
			sc->sc_fcrtl &= ~FCRTL_XONE;
			if (status & STATUS_FD)
				sc->sc_tctl |=
				    TCTL_COLD(TX_COLLISION_DISTANCE_FDX);
			else
				sc->sc_tctl |=
				    TCTL_COLD(TX_COLLISION_DISTANCE_HDX);
			if (CSR_READ(sc, WMREG_CTRL) & CTRL_TFCE)
				sc->sc_fcrtl |= FCRTL_XONE;
			CSR_WRITE(sc, WMREG_TCTL, sc->sc_tctl);
			CSR_WRITE(sc, (sc->sc_type < WM_T_82543) ?
				      WMREG_OLD_FCRTL : WMREG_FCRTL,
				      sc->sc_fcrtl);
			sc->sc_tbi_linkup = 1;
		} else {
			if (i == WM_LINKUP_TIMEOUT)
				wm_check_for_link(sc);
			/* Link is down. */
			DPRINTF(WM_DEBUG_LINK,
			    ("%s: LINK: set media -> link down\n",
			    device_xname(sc->sc_dev)));
			sc->sc_tbi_linkup = 0;
		}
	} else {
		DPRINTF(WM_DEBUG_LINK, ("%s: LINK: set media -> no signal\n",
		    device_xname(sc->sc_dev)));
		sc->sc_tbi_linkup = 0;
	}

	wm_tbi_serdes_set_linkled(sc);

	return 0;
}

/*
 * wm_tbi_mediastatus:	[ifmedia interface function]
 *
 *	Get the current interface media status on a 1000BASE-X device.
 */
static void
wm_tbi_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct wm_softc *sc = ifp->if_softc;
	uint32_t ctrl, status;

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	status = CSR_READ(sc, WMREG_STATUS);
	if ((status & STATUS_LU) == 0) {
		ifmr->ifm_active |= IFM_NONE;
		return;
	}

	ifmr->ifm_status |= IFM_ACTIVE;
	/* Only 82545 is LX */
	if (sc->sc_type == WM_T_82545)
		ifmr->ifm_active |= IFM_1000_LX;
	else
		ifmr->ifm_active |= IFM_1000_SX;
	if (CSR_READ(sc, WMREG_STATUS) & STATUS_FD)
		ifmr->ifm_active |= IFM_FDX;
	else
		ifmr->ifm_active |= IFM_HDX;
	ctrl = CSR_READ(sc, WMREG_CTRL);
	if (ctrl & CTRL_RFCE)
		ifmr->ifm_active |= IFM_FLOW | IFM_ETH_RXPAUSE;
	if (ctrl & CTRL_TFCE)
		ifmr->ifm_active |= IFM_FLOW | IFM_ETH_TXPAUSE;
}

/* XXX TBI only */
static int
wm_check_for_link(struct wm_softc *sc)
{
	struct ifmedia_entry *ife = sc->sc_mii.mii_media.ifm_cur;
	uint32_t rxcw;
	uint32_t ctrl;
	uint32_t status;
	uint32_t sig;

	if (sc->sc_mediatype == WM_MEDIATYPE_SERDES) {
		/* XXX need some work for >= 82571 */
		if (sc->sc_type >= WM_T_82571) {
			sc->sc_tbi_linkup = 1;
			return 0;
		}
	}

	rxcw = CSR_READ(sc, WMREG_RXCW);
	ctrl = CSR_READ(sc, WMREG_CTRL);
	status = CSR_READ(sc, WMREG_STATUS);

	sig = (sc->sc_type > WM_T_82544) ? CTRL_SWDPIN(1) : 0;

	DPRINTF(WM_DEBUG_LINK, ("%s: %s: sig = %d, status_lu = %d, rxcw_c = %d\n",
		device_xname(sc->sc_dev), __func__,
		((ctrl & CTRL_SWDPIN(1)) == sig),
		((status & STATUS_LU) != 0),
		((rxcw & RXCW_C) != 0)
		    ));

	/*
	 * SWDPIN   LU RXCW
	 *      0    0    0
	 *      0    0    1	(should not happen)
	 *      0    1    0	(should not happen)
	 *      0    1    1	(should not happen)
	 *      1    0    0	Disable autonego and force linkup
	 *      1    0    1	got /C/ but not linkup yet
	 *      1    1    0	(linkup)
	 *      1    1    1	If IFM_AUTO, back to autonego
	 *
	 */
	if (((ctrl & CTRL_SWDPIN(1)) == sig)
	    && ((status & STATUS_LU) == 0)
	    && ((rxcw & RXCW_C) == 0)) {
		DPRINTF(WM_DEBUG_LINK, ("%s: force linkup and fullduplex\n",
			__func__));
		sc->sc_tbi_linkup = 0;
		/* Disable auto-negotiation in the TXCW register */
		CSR_WRITE(sc, WMREG_TXCW, (sc->sc_txcw & ~TXCW_ANE));

		/*
		 * Force link-up and also force full-duplex.
		 *
		 * NOTE: CTRL was updated TFCE and RFCE automatically,
		 * so we should update sc->sc_ctrl
		 */
		sc->sc_ctrl = ctrl | CTRL_SLU | CTRL_FD;
		CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl);
	} else if (((status & STATUS_LU) != 0)
	    && ((rxcw & RXCW_C) != 0)
	    && (IFM_SUBTYPE(ife->ifm_media) == IFM_AUTO)) {
		sc->sc_tbi_linkup = 1;
		DPRINTF(WM_DEBUG_LINK, ("%s: go back to autonego\n",
			__func__));
		CSR_WRITE(sc, WMREG_TXCW, sc->sc_txcw);
		CSR_WRITE(sc, WMREG_CTRL, (ctrl & ~CTRL_SLU));
	} else if (((ctrl & CTRL_SWDPIN(1)) == sig)
	    && ((rxcw & RXCW_C) != 0)) {
		DPRINTF(WM_DEBUG_LINK, ("/C/"));
	} else {
		DPRINTF(WM_DEBUG_LINK, ("%s: %x,%x,%x\n", __func__, rxcw, ctrl,
			status));
	}

	return 0;
}

/*
 * wm_tbi_tick:
 *
 *	Check the link on TBI devices.
 *	This function acts as mii_tick().
 */
static void
wm_tbi_tick(struct wm_softc *sc)
{
	struct mii_data *mii = &sc->sc_mii;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	uint32_t status;

	KASSERT(WM_TX_LOCKED(sc));

	status = CSR_READ(sc, WMREG_STATUS);

	/* XXX is this needed? */
	(void)CSR_READ(sc, WMREG_RXCW);
	(void)CSR_READ(sc, WMREG_CTRL);

	/* set link status */
	if ((status & STATUS_LU) == 0) {
		DPRINTF(WM_DEBUG_LINK,
		    ("%s: LINK: checklink -> down\n",
			device_xname(sc->sc_dev)));
		sc->sc_tbi_linkup = 0;
	} else if (sc->sc_tbi_linkup == 0) {
		DPRINTF(WM_DEBUG_LINK,
		    ("%s: LINK: checklink -> up %s\n",
			device_xname(sc->sc_dev),
			(status & STATUS_FD) ? "FDX" : "HDX"));
		sc->sc_tbi_linkup = 1;
		sc->sc_tbi_serdes_ticks = 0;
	}

	if ((sc->sc_ethercom.ec_if.if_flags & IFF_UP) == 0)
		goto setled;

	if ((status & STATUS_LU) == 0) {
		sc->sc_tbi_linkup = 0;
		/* If the timer expired, retry autonegotiation */
		if ((IFM_SUBTYPE(ife->ifm_media) == IFM_AUTO)
		    && (++sc->sc_tbi_serdes_ticks
			>= sc->sc_tbi_serdes_anegticks)) {
			DPRINTF(WM_DEBUG_LINK, ("EXPIRE\n"));
			sc->sc_tbi_serdes_ticks = 0;
			/*
			 * Reset the link, and let autonegotiation do
			 * its thing
			 */
			sc->sc_ctrl |= CTRL_LRST;
			CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl);
			CSR_WRITE_FLUSH(sc);
			delay(1000);
			sc->sc_ctrl &= ~CTRL_LRST;
			CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl);
			CSR_WRITE_FLUSH(sc);
			delay(1000);
			CSR_WRITE(sc, WMREG_TXCW,
			    sc->sc_txcw & ~TXCW_ANE);
			CSR_WRITE(sc, WMREG_TXCW, sc->sc_txcw);
		}
	}

setled:
	wm_tbi_serdes_set_linkled(sc);
}

/* SERDES related */
static void
wm_serdes_power_up_link_82575(struct wm_softc *sc)
{
	uint32_t reg;

	if ((sc->sc_mediatype != WM_MEDIATYPE_SERDES)
	    && ((sc->sc_flags & WM_F_SGMII) == 0))
		return;

	reg = CSR_READ(sc, WMREG_PCS_CFG);
	reg |= PCS_CFG_PCS_EN;
	CSR_WRITE(sc, WMREG_PCS_CFG, reg);

	reg = CSR_READ(sc, WMREG_CTRL_EXT);
	reg &= ~CTRL_EXT_SWDPIN(3);
	CSR_WRITE(sc, WMREG_CTRL_EXT, reg);
	CSR_WRITE_FLUSH(sc);
}

static int
wm_serdes_mediachange(struct ifnet *ifp)
{
	struct wm_softc *sc = ifp->if_softc;
	bool pcs_autoneg = true; /* XXX */
	uint32_t ctrl_ext, pcs_lctl, reg;

	/* XXX Currently, this function is not called on 8257[12] */
	if ((sc->sc_type == WM_T_82571) || (sc->sc_type == WM_T_82572)
	    || (sc->sc_type >= WM_T_82575))
		CSR_WRITE(sc, WMREG_SCTL, SCTL_DISABLE_SERDES_LOOPBACK);

	wm_serdes_power_up_link_82575(sc);

	sc->sc_ctrl |= CTRL_SLU;

	if ((sc->sc_type == WM_T_82575) || (sc->sc_type == WM_T_82576))
		sc->sc_ctrl |= CTRL_SWDPIN(0) | CTRL_SWDPIN(1);

	ctrl_ext = CSR_READ(sc, WMREG_CTRL_EXT);
	pcs_lctl = CSR_READ(sc, WMREG_PCS_LCTL);
	switch (ctrl_ext & CTRL_EXT_LINK_MODE_MASK) {
	case CTRL_EXT_LINK_MODE_SGMII:
		pcs_autoneg = true;
		pcs_lctl &= ~PCS_LCTL_AN_TIMEOUT;
		break;
	case CTRL_EXT_LINK_MODE_1000KX:
		pcs_autoneg = false;
		/* FALLTHROUGH */
	default:
		if ((sc->sc_type == WM_T_82575) || (sc->sc_type == WM_T_82576)){
			if ((sc->sc_flags & WM_F_PCS_DIS_AUTONEGO) != 0)
				pcs_autoneg = false;
		}
		sc->sc_ctrl |= CTRL_SPEED_1000 | CTRL_FRCSPD | CTRL_FD
		    | CTRL_FRCFDX;
		pcs_lctl |= PCS_LCTL_FSV_1000 | PCS_LCTL_FDV_FULL;
	}
	CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl);

	if (pcs_autoneg) {
		pcs_lctl |= PCS_LCTL_AN_ENABLE | PCS_LCTL_AN_RESTART;
		pcs_lctl &= ~PCS_LCTL_FORCE_FC;

		reg = CSR_READ(sc, WMREG_PCS_ANADV);
		reg &= ~(TXCW_ASYM_PAUSE | TXCW_SYM_PAUSE);
		reg |= TXCW_ASYM_PAUSE | TXCW_SYM_PAUSE;
		CSR_WRITE(sc, WMREG_PCS_ANADV, reg);
	} else
		pcs_lctl |= PCS_LCTL_FSD | PCS_LCTL_FORCE_FC;

	CSR_WRITE(sc, WMREG_PCS_LCTL, pcs_lctl);


	return 0;
}

static void
wm_serdes_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct wm_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->sc_mii;
	struct ifmedia_entry *ife = sc->sc_mii.mii_media.ifm_cur;
	uint32_t pcs_adv, pcs_lpab, reg;

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	/* Check PCS */
	reg = CSR_READ(sc, WMREG_PCS_LSTS);
	if ((reg & PCS_LSTS_LINKOK) == 0) {
		ifmr->ifm_active |= IFM_NONE;
		sc->sc_tbi_linkup = 0;
		goto setled;
	}

	sc->sc_tbi_linkup = 1;
	ifmr->ifm_status |= IFM_ACTIVE;
	ifmr->ifm_active |= IFM_1000_SX; /* XXX */
	if ((reg & PCS_LSTS_FDX) != 0)
		ifmr->ifm_active |= IFM_FDX;
	else
		ifmr->ifm_active |= IFM_HDX;
	mii->mii_media_active &= ~IFM_ETH_FMASK;
	if (IFM_SUBTYPE(ife->ifm_media) == IFM_AUTO) {
		/* Check flow */
		reg = CSR_READ(sc, WMREG_PCS_LSTS);
		if ((reg & PCS_LSTS_AN_COMP) == 0) {
			printf("XXX LINKOK but not ACOMP\n");
			goto setled;
		}
		pcs_adv = CSR_READ(sc, WMREG_PCS_ANADV);
		pcs_lpab = CSR_READ(sc, WMREG_PCS_LPAB);
			printf("XXX AN result(2) %08x, %08x\n", pcs_adv, pcs_lpab);
		if ((pcs_adv & TXCW_SYM_PAUSE)
		    && (pcs_lpab & TXCW_SYM_PAUSE)) {
			mii->mii_media_active |= IFM_FLOW
			    | IFM_ETH_TXPAUSE | IFM_ETH_RXPAUSE;
		} else if (((pcs_adv & TXCW_SYM_PAUSE) == 0)
		    && (pcs_adv & TXCW_ASYM_PAUSE)
		    && (pcs_lpab & TXCW_SYM_PAUSE)
		    && (pcs_lpab & TXCW_ASYM_PAUSE)) {
			mii->mii_media_active |= IFM_FLOW
			    | IFM_ETH_TXPAUSE;
		} else if ((pcs_adv & TXCW_SYM_PAUSE)
		    && (pcs_adv & TXCW_ASYM_PAUSE)
		    && ((pcs_lpab & TXCW_SYM_PAUSE) == 0)
		    && (pcs_lpab & TXCW_ASYM_PAUSE)) {
			mii->mii_media_active |= IFM_FLOW
			    | IFM_ETH_RXPAUSE;
		} else {
		}
	}
	ifmr->ifm_active = (ifmr->ifm_active & ~IFM_ETH_FMASK)
	    | (mii->mii_media_active & IFM_ETH_FMASK);
setled:
	wm_tbi_serdes_set_linkled(sc);
}

/*
 * wm_serdes_tick:
 *
 *	Check the link on serdes devices.
 */
static void
wm_serdes_tick(struct wm_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	struct mii_data *mii = &sc->sc_mii;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	uint32_t reg;

	KASSERT(WM_TX_LOCKED(sc));

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	/* Check PCS */
	reg = CSR_READ(sc, WMREG_PCS_LSTS);
	if ((reg & PCS_LSTS_LINKOK) != 0) {
		mii->mii_media_status |= IFM_ACTIVE;
		sc->sc_tbi_linkup = 1;
		sc->sc_tbi_serdes_ticks = 0;
		mii->mii_media_active |= IFM_1000_SX; /* XXX */
		if ((reg & PCS_LSTS_FDX) != 0)
			mii->mii_media_active |= IFM_FDX;
		else
			mii->mii_media_active |= IFM_HDX;
	} else {
		mii->mii_media_status |= IFM_NONE;
		sc->sc_tbi_linkup = 0;
		    /* If the timer expired, retry autonegotiation */
		if ((IFM_SUBTYPE(ife->ifm_media) == IFM_AUTO)
		    && (++sc->sc_tbi_serdes_ticks
			>= sc->sc_tbi_serdes_anegticks)) {
			DPRINTF(WM_DEBUG_LINK, ("EXPIRE\n"));
			sc->sc_tbi_serdes_ticks = 0;
			/* XXX */
			wm_serdes_mediachange(ifp);
		}
	}

	wm_tbi_serdes_set_linkled(sc);
}

/* SFP related */

static int
wm_sfp_read_data_byte(struct wm_softc *sc, uint16_t offset, uint8_t *data)
{
	uint32_t i2ccmd;
	int i;

	i2ccmd = (offset << I2CCMD_REG_ADDR_SHIFT) | I2CCMD_OPCODE_READ;
	CSR_WRITE(sc, WMREG_I2CCMD, i2ccmd);

	/* Poll the ready bit */
	for (i = 0; i < I2CCMD_PHY_TIMEOUT; i++) {
		delay(50);
		i2ccmd = CSR_READ(sc, WMREG_I2CCMD);
		if (i2ccmd & I2CCMD_READY)
			break;
	}
	if ((i2ccmd & I2CCMD_READY) == 0)
		return -1;
	if ((i2ccmd & I2CCMD_ERROR) != 0)
		return -1;

	*data = i2ccmd & 0x00ff;

	return 0;
}

static uint32_t
wm_sfp_get_media_type(struct wm_softc *sc)
{
	uint32_t ctrl_ext;
	uint8_t val = 0;
	int timeout = 3;
	uint32_t mediatype = WM_MEDIATYPE_UNKNOWN;
	int rv = -1;

	ctrl_ext = CSR_READ(sc, WMREG_CTRL_EXT);
	ctrl_ext &= ~CTRL_EXT_SWDPIN(3);
	CSR_WRITE(sc, WMREG_CTRL_EXT, ctrl_ext | CTRL_EXT_I2C_ENA);
	CSR_WRITE_FLUSH(sc);

	/* Read SFP module data */
	while (timeout) {
		rv = wm_sfp_read_data_byte(sc, SFF_SFP_ID_OFF, &val);
		if (rv == 0)
			break;
		delay(100*1000); /* XXX too big */
		timeout--;
	}
	if (rv != 0)
		goto out;
	switch (val) {
	case SFF_SFP_ID_SFF:
		aprint_normal_dev(sc->sc_dev,
		    "Module/Connector soldered to board\n");
		break;
	case SFF_SFP_ID_SFP:
		aprint_normal_dev(sc->sc_dev, "SFP\n");
		break;
	case SFF_SFP_ID_UNKNOWN:
		goto out;
	default:
		break;
	}

	rv = wm_sfp_read_data_byte(sc, SFF_SFP_ETH_FLAGS_OFF, &val);
	if (rv != 0) {
		goto out;
	}

	if ((val & (SFF_SFP_ETH_FLAGS_1000SX | SFF_SFP_ETH_FLAGS_1000LX)) != 0)
		mediatype = WM_MEDIATYPE_SERDES;
	else if ((val & SFF_SFP_ETH_FLAGS_1000T) != 0){
		sc->sc_flags |= WM_F_SGMII;
		mediatype = WM_MEDIATYPE_COPPER;
	} else if ((val & SFF_SFP_ETH_FLAGS_100FX) != 0){
		sc->sc_flags |= WM_F_SGMII;
		mediatype = WM_MEDIATYPE_SERDES;
	}

out:
	/* Restore I2C interface setting */
	CSR_WRITE(sc, WMREG_CTRL_EXT, ctrl_ext);

	return mediatype;
}
/*
 * NVM related.
 * Microwire, SPI (w/wo EERD) and Flash.
 */

/* Both spi and uwire */

/*
 * wm_eeprom_sendbits:
 *
 *	Send a series of bits to the EEPROM.
 */
static void
wm_eeprom_sendbits(struct wm_softc *sc, uint32_t bits, int nbits)
{
	uint32_t reg;
	int x;

	reg = CSR_READ(sc, WMREG_EECD);

	for (x = nbits; x > 0; x--) {
		if (bits & (1U << (x - 1)))
			reg |= EECD_DI;
		else
			reg &= ~EECD_DI;
		CSR_WRITE(sc, WMREG_EECD, reg);
		CSR_WRITE_FLUSH(sc);
		delay(2);
		CSR_WRITE(sc, WMREG_EECD, reg | EECD_SK);
		CSR_WRITE_FLUSH(sc);
		delay(2);
		CSR_WRITE(sc, WMREG_EECD, reg);
		CSR_WRITE_FLUSH(sc);
		delay(2);
	}
}

/*
 * wm_eeprom_recvbits:
 *
 *	Receive a series of bits from the EEPROM.
 */
static void
wm_eeprom_recvbits(struct wm_softc *sc, uint32_t *valp, int nbits)
{
	uint32_t reg, val;
	int x;

	reg = CSR_READ(sc, WMREG_EECD) & ~EECD_DI;

	val = 0;
	for (x = nbits; x > 0; x--) {
		CSR_WRITE(sc, WMREG_EECD, reg | EECD_SK);
		CSR_WRITE_FLUSH(sc);
		delay(2);
		if (CSR_READ(sc, WMREG_EECD) & EECD_DO)
			val |= (1U << (x - 1));
		CSR_WRITE(sc, WMREG_EECD, reg);
		CSR_WRITE_FLUSH(sc);
		delay(2);
	}
	*valp = val;
}

/* Microwire */

/*
 * wm_nvm_read_uwire:
 *
 *	Read a word from the EEPROM using the MicroWire protocol.
 */
static int
wm_nvm_read_uwire(struct wm_softc *sc, int word, int wordcnt, uint16_t *data)
{
	uint32_t reg, val;
	int i;

	for (i = 0; i < wordcnt; i++) {
		/* Clear SK and DI. */
		reg = CSR_READ(sc, WMREG_EECD) & ~(EECD_SK | EECD_DI);
		CSR_WRITE(sc, WMREG_EECD, reg);

		/*
		 * XXX: workaround for a bug in qemu-0.12.x and prior
		 * and Xen.
		 *
		 * We use this workaround only for 82540 because qemu's
		 * e1000 act as 82540.
		 */
		if (sc->sc_type == WM_T_82540) {
			reg |= EECD_SK;
			CSR_WRITE(sc, WMREG_EECD, reg);
			reg &= ~EECD_SK;
			CSR_WRITE(sc, WMREG_EECD, reg);
			CSR_WRITE_FLUSH(sc);
			delay(2);
		}
		/* XXX: end of workaround */

		/* Set CHIP SELECT. */
		reg |= EECD_CS;
		CSR_WRITE(sc, WMREG_EECD, reg);
		CSR_WRITE_FLUSH(sc);
		delay(2);

		/* Shift in the READ command. */
		wm_eeprom_sendbits(sc, UWIRE_OPC_READ, 3);

		/* Shift in address. */
		wm_eeprom_sendbits(sc, word + i, sc->sc_nvm_addrbits);

		/* Shift out the data. */
		wm_eeprom_recvbits(sc, &val, 16);
		data[i] = val & 0xffff;

		/* Clear CHIP SELECT. */
		reg = CSR_READ(sc, WMREG_EECD) & ~EECD_CS;
		CSR_WRITE(sc, WMREG_EECD, reg);
		CSR_WRITE_FLUSH(sc);
		delay(2);
	}

	return 0;
}

/* SPI */

/*
 * Set SPI and FLASH related information from the EECD register.
 * For 82541 and 82547, the word size is taken from EEPROM.
 */
static int
wm_nvm_set_addrbits_size_eecd(struct wm_softc *sc)
{
	int size;
	uint32_t reg;
	uint16_t data;

	reg = CSR_READ(sc, WMREG_EECD);
	sc->sc_nvm_addrbits = (reg & EECD_EE_ABITS) ? 16 : 8;

	/* Read the size of NVM from EECD by default */
	size = __SHIFTOUT(reg, EECD_EE_SIZE_EX_MASK);
	switch (sc->sc_type) {
	case WM_T_82541:
	case WM_T_82541_2:
	case WM_T_82547:
	case WM_T_82547_2:
		/* Set dummy value to access EEPROM */
		sc->sc_nvm_wordsize = 64;
		wm_nvm_read(sc, NVM_OFF_EEPROM_SIZE, 1, &data);
		reg = data;
		size = __SHIFTOUT(reg, EECD_EE_SIZE_EX_MASK);
		if (size == 0)
			size = 6; /* 64 word size */
		else
			size += NVM_WORD_SIZE_BASE_SHIFT + 1;
		break;
	case WM_T_80003:
	case WM_T_82571:
	case WM_T_82572:
	case WM_T_82573: /* SPI case */
	case WM_T_82574: /* SPI case */
	case WM_T_82583: /* SPI case */
		size += NVM_WORD_SIZE_BASE_SHIFT;
		if (size > 14)
			size = 14;
		break;
	case WM_T_82575:
	case WM_T_82576:
	case WM_T_82580:
	case WM_T_I350:
	case WM_T_I354:
	case WM_T_I210:
	case WM_T_I211:
		size += NVM_WORD_SIZE_BASE_SHIFT;
		if (size > 15)
			size = 15;
		break;
	default:
		aprint_error_dev(sc->sc_dev,
		    "%s: unknown device(%d)?\n", __func__, sc->sc_type);
		return -1;
		break;
	}

	sc->sc_nvm_wordsize = 1 << size;

	return 0;
}

/*
 * wm_nvm_ready_spi:
 *
 *	Wait for a SPI EEPROM to be ready for commands.
 */
static int
wm_nvm_ready_spi(struct wm_softc *sc)
{
	uint32_t val;
	int usec;

	for (usec = 0; usec < SPI_MAX_RETRIES; delay(5), usec += 5) {
		wm_eeprom_sendbits(sc, SPI_OPC_RDSR, 8);
		wm_eeprom_recvbits(sc, &val, 8);
		if ((val & SPI_SR_RDY) == 0)
			break;
	}
	if (usec >= SPI_MAX_RETRIES) {
		aprint_error_dev(sc->sc_dev, "EEPROM failed to become ready\n");
		return 1;
	}
	return 0;
}

/*
 * wm_nvm_read_spi:
 *
 *	Read a work from the EEPROM using the SPI protocol.
 */
static int
wm_nvm_read_spi(struct wm_softc *sc, int word, int wordcnt, uint16_t *data)
{
	uint32_t reg, val;
	int i;
	uint8_t opc;

	/* Clear SK and CS. */
	reg = CSR_READ(sc, WMREG_EECD) & ~(EECD_SK | EECD_CS);
	CSR_WRITE(sc, WMREG_EECD, reg);
	CSR_WRITE_FLUSH(sc);
	delay(2);

	if (wm_nvm_ready_spi(sc))
		return 1;

	/* Toggle CS to flush commands. */
	CSR_WRITE(sc, WMREG_EECD, reg | EECD_CS);
	CSR_WRITE_FLUSH(sc);
	delay(2);
	CSR_WRITE(sc, WMREG_EECD, reg);
	CSR_WRITE_FLUSH(sc);
	delay(2);

	opc = SPI_OPC_READ;
	if (sc->sc_nvm_addrbits == 8 && word >= 128)
		opc |= SPI_OPC_A8;

	wm_eeprom_sendbits(sc, opc, 8);
	wm_eeprom_sendbits(sc, word << 1, sc->sc_nvm_addrbits);

	for (i = 0; i < wordcnt; i++) {
		wm_eeprom_recvbits(sc, &val, 16);
		data[i] = ((val >> 8) & 0xff) | ((val & 0xff) << 8);
	}

	/* Raise CS and clear SK. */
	reg = (CSR_READ(sc, WMREG_EECD) & ~EECD_SK) | EECD_CS;
	CSR_WRITE(sc, WMREG_EECD, reg);
	CSR_WRITE_FLUSH(sc);
	delay(2);

	return 0;
}

/* Using with EERD */

static int
wm_poll_eerd_eewr_done(struct wm_softc *sc, int rw)
{
	uint32_t attempts = 100000;
	uint32_t i, reg = 0;
	int32_t done = -1;

	for (i = 0; i < attempts; i++) {
		reg = CSR_READ(sc, rw);

		if (reg & EERD_DONE) {
			done = 0;
			break;
		}
		delay(5);
	}

	return done;
}

static int
wm_nvm_read_eerd(struct wm_softc *sc, int offset, int wordcnt,
    uint16_t *data)
{
	int i, eerd = 0;
	int error = 0;

	for (i = 0; i < wordcnt; i++) {
		eerd = ((offset + i) << EERD_ADDR_SHIFT) | EERD_START;

		CSR_WRITE(sc, WMREG_EERD, eerd);
		error = wm_poll_eerd_eewr_done(sc, WMREG_EERD);
		if (error != 0)
			break;

		data[i] = (CSR_READ(sc, WMREG_EERD) >> EERD_DATA_SHIFT);
	}

	return error;
}

/* Flash */

static int
wm_nvm_valid_bank_detect_ich8lan(struct wm_softc *sc, unsigned int *bank)
{
	uint32_t eecd;
	uint32_t act_offset = ICH_NVM_SIG_WORD * 2 + 1;
	uint32_t bank1_offset = sc->sc_ich8_flash_bank_size * sizeof(uint16_t);
	uint8_t sig_byte = 0;

	switch (sc->sc_type) {
	case WM_T_ICH8:
	case WM_T_ICH9:
		eecd = CSR_READ(sc, WMREG_EECD);
		if ((eecd & EECD_SEC1VAL_VALMASK) == EECD_SEC1VAL_VALMASK) {
			*bank = ((eecd & EECD_SEC1VAL) != 0) ? 1 : 0;
			return 0;
		}
		/* FALLTHROUGH */
	default:
		/* Default to 0 */
		*bank = 0;

		/* Check bank 0 */
		wm_read_ich8_byte(sc, act_offset, &sig_byte);
		if ((sig_byte & ICH_NVM_VALID_SIG_MASK) == ICH_NVM_SIG_VALUE) {
			*bank = 0;
			return 0;
		}

		/* Check bank 1 */
		wm_read_ich8_byte(sc, act_offset + bank1_offset,
		    &sig_byte);
		if ((sig_byte & ICH_NVM_VALID_SIG_MASK) == ICH_NVM_SIG_VALUE) {
			*bank = 1;
			return 0;
		}
	}

	DPRINTF(WM_DEBUG_NVM, ("%s: No valid NVM bank present\n",
		device_xname(sc->sc_dev)));
	return -1;
}

/******************************************************************************
 * This function does initial flash setup so that a new read/write/erase cycle
 * can be started.
 *
 * sc - The pointer to the hw structure
 ****************************************************************************/
static int32_t
wm_ich8_cycle_init(struct wm_softc *sc)
{
	uint16_t hsfsts;
	int32_t error = 1;
	int32_t i     = 0;

	hsfsts = ICH8_FLASH_READ16(sc, ICH_FLASH_HSFSTS);

	/* May be check the Flash Des Valid bit in Hw status */
	if ((hsfsts & HSFSTS_FLDVAL) == 0) {
		return error;
	}

	/* Clear FCERR in Hw status by writing 1 */
	/* Clear DAEL in Hw status by writing a 1 */
	hsfsts |= HSFSTS_ERR | HSFSTS_DAEL;

	ICH8_FLASH_WRITE16(sc, ICH_FLASH_HSFSTS, hsfsts);

	/*
	 * Either we should have a hardware SPI cycle in progress bit to check
	 * against, in order to start a new cycle or FDONE bit should be
	 * changed in the hardware so that it is 1 after harware reset, which
	 * can then be used as an indication whether a cycle is in progress or
	 * has been completed .. we should also have some software semaphore
	 * mechanism to guard FDONE or the cycle in progress bit so that two
	 * threads access to those bits can be sequentiallized or a way so that
	 * 2 threads dont start the cycle at the same time
	 */

	if ((hsfsts & HSFSTS_FLINPRO) == 0) {
		/*
		 * There is no cycle running at present, so we can start a
		 * cycle
		 */

		/* Begin by setting Flash Cycle Done. */
		hsfsts |= HSFSTS_DONE;
		ICH8_FLASH_WRITE16(sc, ICH_FLASH_HSFSTS, hsfsts);
		error = 0;
	} else {
		/*
		 * otherwise poll for sometime so the current cycle has a
		 * chance to end before giving up.
		 */
		for (i = 0; i < ICH_FLASH_COMMAND_TIMEOUT; i++) {
			hsfsts = ICH8_FLASH_READ16(sc, ICH_FLASH_HSFSTS);
			if ((hsfsts & HSFSTS_FLINPRO) == 0) {
				error = 0;
				break;
			}
			delay(1);
		}
		if (error == 0) {
			/*
			 * Successful in waiting for previous cycle to timeout,
			 * now set the Flash Cycle Done.
			 */
			hsfsts |= HSFSTS_DONE;
			ICH8_FLASH_WRITE16(sc, ICH_FLASH_HSFSTS, hsfsts);
		}
	}
	return error;
}

/******************************************************************************
 * This function starts a flash cycle and waits for its completion
 *
 * sc - The pointer to the hw structure
 ****************************************************************************/
static int32_t
wm_ich8_flash_cycle(struct wm_softc *sc, uint32_t timeout)
{
	uint16_t hsflctl;
	uint16_t hsfsts;
	int32_t error = 1;
	uint32_t i = 0;

	/* Start a cycle by writing 1 in Flash Cycle Go in Hw Flash Control */
	hsflctl = ICH8_FLASH_READ16(sc, ICH_FLASH_HSFCTL);
	hsflctl |= HSFCTL_GO;
	ICH8_FLASH_WRITE16(sc, ICH_FLASH_HSFCTL, hsflctl);

	/* Wait till FDONE bit is set to 1 */
	do {
		hsfsts = ICH8_FLASH_READ16(sc, ICH_FLASH_HSFSTS);
		if (hsfsts & HSFSTS_DONE)
			break;
		delay(1);
		i++;
	} while (i < timeout);
	if ((hsfsts & HSFSTS_DONE) == 1 && (hsfsts & HSFSTS_ERR) == 0)
		error = 0;

	return error;
}

/******************************************************************************
 * Reads a byte or word from the NVM using the ICH8 flash access registers.
 *
 * sc - The pointer to the hw structure
 * index - The index of the byte or word to read.
 * size - Size of data to read, 1=byte 2=word
 * data - Pointer to the word to store the value read.
 *****************************************************************************/
static int32_t
wm_read_ich8_data(struct wm_softc *sc, uint32_t index,
    uint32_t size, uint16_t *data)
{
	uint16_t hsfsts;
	uint16_t hsflctl;
	uint32_t flash_linear_address;
	uint32_t flash_data = 0;
	int32_t error = 1;
	int32_t count = 0;

	if (size < 1  || size > 2 || data == 0x0 ||
	    index > ICH_FLASH_LINEAR_ADDR_MASK)
		return error;

	flash_linear_address = (ICH_FLASH_LINEAR_ADDR_MASK & index) +
	    sc->sc_ich8_flash_base;

	do {
		delay(1);
		/* Steps */
		error = wm_ich8_cycle_init(sc);
		if (error)
			break;

		hsflctl = ICH8_FLASH_READ16(sc, ICH_FLASH_HSFCTL);
		/* 0b/1b corresponds to 1 or 2 byte size, respectively. */
		hsflctl |=  ((size - 1) << HSFCTL_BCOUNT_SHIFT)
		    & HSFCTL_BCOUNT_MASK;
		hsflctl |= ICH_CYCLE_READ << HSFCTL_CYCLE_SHIFT;
		ICH8_FLASH_WRITE16(sc, ICH_FLASH_HSFCTL, hsflctl);

		/*
		 * Write the last 24 bits of index into Flash Linear address
		 * field in Flash Address
		 */
		/* TODO: TBD maybe check the index against the size of flash */

		ICH8_FLASH_WRITE32(sc, ICH_FLASH_FADDR, flash_linear_address);

		error = wm_ich8_flash_cycle(sc, ICH_FLASH_COMMAND_TIMEOUT);

		/*
		 * Check if FCERR is set to 1, if set to 1, clear it and try
		 * the whole sequence a few more times, else read in (shift in)
		 * the Flash Data0, the order is least significant byte first
		 * msb to lsb
		 */
		if (error == 0) {
			flash_data = ICH8_FLASH_READ32(sc, ICH_FLASH_FDATA0);
			if (size == 1)
				*data = (uint8_t)(flash_data & 0x000000FF);
			else if (size == 2)
				*data = (uint16_t)(flash_data & 0x0000FFFF);
			break;
		} else {
			/*
			 * If we've gotten here, then things are probably
			 * completely hosed, but if the error condition is
			 * detected, it won't hurt to give it another try...
			 * ICH_FLASH_CYCLE_REPEAT_COUNT times.
			 */
			hsfsts = ICH8_FLASH_READ16(sc, ICH_FLASH_HSFSTS);
			if (hsfsts & HSFSTS_ERR) {
				/* Repeat for some time before giving up. */
				continue;
			} else if ((hsfsts & HSFSTS_DONE) == 0)
				break;
		}
	} while (count++ < ICH_FLASH_CYCLE_REPEAT_COUNT);

	return error;
}

/******************************************************************************
 * Reads a single byte from the NVM using the ICH8 flash access registers.
 *
 * sc - pointer to wm_hw structure
 * index - The index of the byte to read.
 * data - Pointer to a byte to store the value read.
 *****************************************************************************/
static int32_t
wm_read_ich8_byte(struct wm_softc *sc, uint32_t index, uint8_t* data)
{
	int32_t status;
	uint16_t word = 0;

	status = wm_read_ich8_data(sc, index, 1, &word);
	if (status == 0)
		*data = (uint8_t)word;
	else
		*data = 0;

	return status;
}

/******************************************************************************
 * Reads a word from the NVM using the ICH8 flash access registers.
 *
 * sc - pointer to wm_hw structure
 * index - The starting byte index of the word to read.
 * data - Pointer to a word to store the value read.
 *****************************************************************************/
static int32_t
wm_read_ich8_word(struct wm_softc *sc, uint32_t index, uint16_t *data)
{
	int32_t status;

	status = wm_read_ich8_data(sc, index, 2, data);
	return status;
}

/******************************************************************************
 * Reads a 16 bit word or words from the EEPROM using the ICH8's flash access
 * register.
 *
 * sc - Struct containing variables accessed by shared code
 * offset - offset of word in the EEPROM to read
 * data - word read from the EEPROM
 * words - number of words to read
 *****************************************************************************/
static int
wm_nvm_read_ich8(struct wm_softc *sc, int offset, int words, uint16_t *data)
{
	int32_t  error = 0;
	uint32_t flash_bank = 0;
	uint32_t act_offset = 0;
	uint32_t bank_offset = 0;
	uint16_t word = 0;
	uint16_t i = 0;

	/*
	 * We need to know which is the valid flash bank.  In the event
	 * that we didn't allocate eeprom_shadow_ram, we may not be
	 * managing flash_bank.  So it cannot be trusted and needs
	 * to be updated with each read.
	 */
	error = wm_nvm_valid_bank_detect_ich8lan(sc, &flash_bank);
	if (error) {
		DPRINTF(WM_DEBUG_NVM, ("%s: failed to detect NVM bank\n",
			device_xname(sc->sc_dev)));
		flash_bank = 0;
	}

	/*
	 * Adjust offset appropriately if we're on bank 1 - adjust for word
	 * size
	 */
	bank_offset = flash_bank * (sc->sc_ich8_flash_bank_size * 2);

	error = wm_get_swfwhw_semaphore(sc);
	if (error) {
		aprint_error_dev(sc->sc_dev, "%s: failed to get semaphore\n",
		    __func__);
		return error;
	}

	for (i = 0; i < words; i++) {
		/* The NVM part needs a byte offset, hence * 2 */
		act_offset = bank_offset + ((offset + i) * 2);
		error = wm_read_ich8_word(sc, act_offset, &word);
		if (error) {
			aprint_error_dev(sc->sc_dev,
			    "%s: failed to read NVM\n", __func__);
			break;
		}
		data[i] = word;
	}

	wm_put_swfwhw_semaphore(sc);
	return error;
}

/* iNVM */

static int
wm_nvm_read_word_invm(struct wm_softc *sc, uint16_t address, uint16_t *data)
{
	int32_t  rv = 0;
	uint32_t invm_dword;
	uint16_t i;
	uint8_t record_type, word_address;

	for (i = 0; i < INVM_SIZE; i++) {
		invm_dword = CSR_READ(sc, WM_INVM_DATA_REG(i));
		/* Get record type */
		record_type = INVM_DWORD_TO_RECORD_TYPE(invm_dword);
		if (record_type == INVM_UNINITIALIZED_STRUCTURE)
			break;
		if (record_type == INVM_CSR_AUTOLOAD_STRUCTURE)
			i += INVM_CSR_AUTOLOAD_DATA_SIZE_IN_DWORDS;
		if (record_type == INVM_RSA_KEY_SHA256_STRUCTURE)
			i += INVM_RSA_KEY_SHA256_DATA_SIZE_IN_DWORDS;
		if (record_type == INVM_WORD_AUTOLOAD_STRUCTURE) {
			word_address = INVM_DWORD_TO_WORD_ADDRESS(invm_dword);
			if (word_address == address) {
				*data = INVM_DWORD_TO_WORD_DATA(invm_dword);
				rv = 0;
				break;
			}
		}
	}

	return rv;
}

static int
wm_nvm_read_invm(struct wm_softc *sc, int offset, int words, uint16_t *data)
{
	int rv = 0;
	int i;

	for (i = 0; i < words; i++) {
		switch (offset + i) {
		case NVM_OFF_MACADDR:
		case NVM_OFF_MACADDR1:
		case NVM_OFF_MACADDR2:
			rv = wm_nvm_read_word_invm(sc, offset + i, &data[i]);
			if (rv != 0) {
				data[i] = 0xffff;
				rv = -1;
			}
			break;
		case NVM_OFF_CFG2:
			rv = wm_nvm_read_word_invm(sc, offset, data);
			if (rv != 0) {
				*data = NVM_INIT_CTRL_2_DEFAULT_I211;
				rv = 0;
			}
			break;
		case NVM_OFF_CFG4:
			rv = wm_nvm_read_word_invm(sc, offset, data);
			if (rv != 0) {
				*data = NVM_INIT_CTRL_4_DEFAULT_I211;
				rv = 0;
			}
			break;
		case NVM_OFF_LED_1_CFG:
			rv = wm_nvm_read_word_invm(sc, offset, data);
			if (rv != 0) {
				*data = NVM_LED_1_CFG_DEFAULT_I211;
				rv = 0;
			}
			break;
		case NVM_OFF_LED_0_2_CFG:
			rv = wm_nvm_read_word_invm(sc, offset, data);
			if (rv != 0) {
				*data = NVM_LED_0_2_CFG_DEFAULT_I211;
				rv = 0;
			}
			break;
		case NVM_OFF_ID_LED_SETTINGS:
			rv = wm_nvm_read_word_invm(sc, offset, data);
			if (rv != 0) {
				*data = ID_LED_RESERVED_FFFF;
				rv = 0;
			}
			break;
		default:
			DPRINTF(WM_DEBUG_NVM,
			    ("NVM word 0x%02x is not mapped.\n", offset));
			*data = NVM_RESERVED_WORD;
			break;
		}
	}

	return rv;
}

/* Lock, detecting NVM type, validate checksum, version and read */

/*
 * wm_nvm_acquire:
 *
 *	Perform the EEPROM handshake required on some chips.
 */
static int
wm_nvm_acquire(struct wm_softc *sc)
{
	uint32_t reg;
	int x;
	int ret = 0;

	/* always success */
	if ((sc->sc_flags & WM_F_EEPROM_FLASH) != 0)
		return 0;

	if (sc->sc_flags & WM_F_LOCK_EXTCNF) {
		ret = wm_get_swfwhw_semaphore(sc);
	} else if (sc->sc_flags & WM_F_LOCK_SWFW) {
		/* This will also do wm_get_swsm_semaphore() if needed */
		ret = wm_get_swfw_semaphore(sc, SWFW_EEP_SM);
	} else if (sc->sc_flags & WM_F_LOCK_SWSM) {
		ret = wm_get_swsm_semaphore(sc);
	}

	if (ret) {
		aprint_error_dev(sc->sc_dev, "%s: failed to get semaphore\n",
			__func__);
		return 1;
	}

	if (sc->sc_flags & WM_F_LOCK_EECD) {
		reg = CSR_READ(sc, WMREG_EECD);

		/* Request EEPROM access. */
		reg |= EECD_EE_REQ;
		CSR_WRITE(sc, WMREG_EECD, reg);

		/* ..and wait for it to be granted. */
		for (x = 0; x < 1000; x++) {
			reg = CSR_READ(sc, WMREG_EECD);
			if (reg & EECD_EE_GNT)
				break;
			delay(5);
		}
		if ((reg & EECD_EE_GNT) == 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not acquire EEPROM GNT\n");
			reg &= ~EECD_EE_REQ;
			CSR_WRITE(sc, WMREG_EECD, reg);
			if (sc->sc_flags & WM_F_LOCK_EXTCNF)
				wm_put_swfwhw_semaphore(sc);
			if (sc->sc_flags & WM_F_LOCK_SWFW)
				wm_put_swfw_semaphore(sc, SWFW_EEP_SM);
			else if (sc->sc_flags & WM_F_LOCK_SWSM)
				wm_put_swsm_semaphore(sc);
			return 1;
		}
	}

	return 0;
}

/*
 * wm_nvm_release:
 *
 *	Release the EEPROM mutex.
 */
static void
wm_nvm_release(struct wm_softc *sc)
{
	uint32_t reg;

	/* always success */
	if ((sc->sc_flags & WM_F_EEPROM_FLASH) != 0)
		return;

	if (sc->sc_flags & WM_F_LOCK_EECD) {
		reg = CSR_READ(sc, WMREG_EECD);
		reg &= ~EECD_EE_REQ;
		CSR_WRITE(sc, WMREG_EECD, reg);
	}

	if (sc->sc_flags & WM_F_LOCK_EXTCNF)
		wm_put_swfwhw_semaphore(sc);
	if (sc->sc_flags & WM_F_LOCK_SWFW)
		wm_put_swfw_semaphore(sc, SWFW_EEP_SM);
	else if (sc->sc_flags & WM_F_LOCK_SWSM)
		wm_put_swsm_semaphore(sc);
}

static int
wm_nvm_is_onboard_eeprom(struct wm_softc *sc)
{
	uint32_t eecd = 0;

	if (sc->sc_type == WM_T_82573 || sc->sc_type == WM_T_82574
	    || sc->sc_type == WM_T_82583) {
		eecd = CSR_READ(sc, WMREG_EECD);

		/* Isolate bits 15 & 16 */
		eecd = ((eecd >> 15) & 0x03);

		/* If both bits are set, device is Flash type */
		if (eecd == 0x03)
			return 0;
	}
	return 1;
}

static int
wm_nvm_get_flash_presence_i210(struct wm_softc *sc)
{
	uint32_t eec;

	eec = CSR_READ(sc, WMREG_EEC);
	if ((eec & EEC_FLASH_DETECTED) != 0)
		return 1;

	return 0;
}

/*
 * wm_nvm_validate_checksum
 *
 * The checksum is defined as the sum of the first 64 (16 bit) words.
 */
static int
wm_nvm_validate_checksum(struct wm_softc *sc)
{
	uint16_t checksum;
	uint16_t eeprom_data;
#ifdef WM_DEBUG
	uint16_t csum_wordaddr, valid_checksum;
#endif
	int i;

	checksum = 0;

	/* Don't check for I211 */
	if (sc->sc_type == WM_T_I211)
		return 0;

#ifdef WM_DEBUG
	if (sc->sc_type == WM_T_PCH_LPT) {
		csum_wordaddr = NVM_OFF_COMPAT;
		valid_checksum = NVM_COMPAT_VALID_CHECKSUM;
	} else {
		csum_wordaddr = NVM_OFF_FUTURE_INIT_WORD1;
		valid_checksum = NVM_FUTURE_INIT_WORD1_VALID_CHECKSUM;
	}

	/* Dump EEPROM image for debug */
	if ((sc->sc_type == WM_T_ICH8) || (sc->sc_type == WM_T_ICH9)
	    || (sc->sc_type == WM_T_ICH10) || (sc->sc_type == WM_T_PCH)
	    || (sc->sc_type == WM_T_PCH2) || (sc->sc_type == WM_T_PCH_LPT)) {
		wm_nvm_read(sc, csum_wordaddr, 1, &eeprom_data);
		if ((eeprom_data & valid_checksum) == 0) {
			DPRINTF(WM_DEBUG_NVM,
			    ("%s: NVM need to be updated (%04x != %04x)\n",
				device_xname(sc->sc_dev), eeprom_data,
				    valid_checksum));
		}
	}

	if ((wm_debug & WM_DEBUG_NVM) != 0) {
		printf("%s: NVM dump:\n", device_xname(sc->sc_dev));
		for (i = 0; i < NVM_SIZE; i++) {
			if (wm_nvm_read(sc, i, 1, &eeprom_data))
				printf("XXXX ");
			else
				printf("%04hx ", eeprom_data);
			if (i % 8 == 7)
				printf("\n");
		}
	}

#endif /* WM_DEBUG */

	for (i = 0; i < NVM_SIZE; i++) {
		if (wm_nvm_read(sc, i, 1, &eeprom_data))
			return 1;
		checksum += eeprom_data;
	}

	if (checksum != (uint16_t) NVM_CHECKSUM) {
#ifdef WM_DEBUG
		printf("%s: NVM checksum mismatch (%04x != %04x)\n",
		    device_xname(sc->sc_dev), checksum, NVM_CHECKSUM);
#endif
	}

	return 0;
}

static void
wm_nvm_version_invm(struct wm_softc *sc)
{
	uint32_t dword;

	/*
	 * Linux's code to decode version is very strange, so we don't
	 * obey that algorithm and just use word 61 as the document.
	 * Perhaps it's not perfect though...
	 *
	 * Example:
	 *
	 *   Word61: 00800030 -> Version 0.6 (I211 spec update notes about 0.6)
	 */
	dword = CSR_READ(sc, WM_INVM_DATA_REG(61));
	dword = __SHIFTOUT(dword, INVM_VER_1);
	sc->sc_nvm_ver_major = __SHIFTOUT(dword, INVM_MAJOR);
	sc->sc_nvm_ver_minor = __SHIFTOUT(dword, INVM_MINOR);
}

static void
wm_nvm_version(struct wm_softc *sc)
{
	uint16_t major, minor, build, patch;
	uint16_t uid0, uid1;
	uint16_t nvm_data;
	uint16_t off;
	bool check_version = false;
	bool check_optionrom = false;
	bool have_build = false;

	/*
	 * Version format:
	 *
	 * XYYZ
	 * X0YZ
	 * X0YY
	 *
	 * Example:
	 *
	 *	82571	0x50a2	5.10.2?	(the spec update notes about 5.6-5.10)
	 *	82571	0x50a6	5.10.6?
	 *	82572	0x506a	5.6.10?
	 *	82572EI	0x5069	5.6.9?
	 *	82574L	0x1080	1.8.0?	(the spec update notes about 2.1.4)
	 *		0x2013	2.1.3?
	 *	82583	0x10a0	1.10.0? (document says it's default vaule)
	 */
	wm_nvm_read(sc, NVM_OFF_IMAGE_UID1, 1, &uid1);
	switch (sc->sc_type) {
	case WM_T_82571:
	case WM_T_82572:
	case WM_T_82574:
	case WM_T_82583:
		check_version = true;
		check_optionrom = true;
		have_build = true;
		break;
	case WM_T_82575:
	case WM_T_82576:
	case WM_T_82580:
		if ((uid1 & NVM_MAJOR_MASK) != NVM_UID_VALID)
			check_version = true;
		break;
	case WM_T_I211:
		wm_nvm_version_invm(sc);
		goto printver;
	case WM_T_I210:
		if (!wm_nvm_get_flash_presence_i210(sc)) {
			wm_nvm_version_invm(sc);
			goto printver;
		}
		/* FALLTHROUGH */
	case WM_T_I350:
	case WM_T_I354:
		check_version = true;
		check_optionrom = true;
		break;
	default:
		return;
	}
	if (check_version) {
		wm_nvm_read(sc, NVM_OFF_VERSION, 1, &nvm_data);
		major = (nvm_data & NVM_MAJOR_MASK) >> NVM_MAJOR_SHIFT;
		if (have_build || ((nvm_data & 0x0f00) != 0x0000)) {
			minor = (nvm_data & NVM_MINOR_MASK) >> NVM_MINOR_SHIFT;
			build = nvm_data & NVM_BUILD_MASK;
			have_build = true;
		} else
			minor = nvm_data & 0x00ff;

		/* Decimal */
		minor = (minor / 16) * 10 + (minor % 16);
		sc->sc_nvm_ver_major = major;
		sc->sc_nvm_ver_minor = minor;

printver:
		aprint_verbose(", version %d.%d", sc->sc_nvm_ver_major,
		    sc->sc_nvm_ver_minor);
		if (have_build) {
			sc->sc_nvm_ver_build = build;
			aprint_verbose(".%d", build);
		}
	}
	if (check_optionrom) {
		wm_nvm_read(sc, NVM_OFF_COMB_VER_PTR, 1, &off);
		/* Option ROM Version */
		if ((off != 0x0000) && (off != 0xffff)) {
			off += NVM_COMBO_VER_OFF;
			wm_nvm_read(sc, off + 1, 1, &uid1);
			wm_nvm_read(sc, off, 1, &uid0);
			if ((uid0 != 0) && (uid0 != 0xffff)
			    && (uid1 != 0) && (uid1 != 0xffff)) {
				/* 16bits */
				major = uid0 >> 8;
				build = (uid0 << 8) | (uid1 >> 8);
				patch = uid1 & 0x00ff;
				aprint_verbose(", option ROM Version %d.%d.%d",
				    major, build, patch);
			}
		}
	}

	wm_nvm_read(sc, NVM_OFF_IMAGE_UID0, 1, &uid0);
	aprint_verbose(", Image Unique ID %08x", (uid1 << 16) | uid0);
}

/*
 * wm_nvm_read:
 *
 *	Read data from the serial EEPROM.
 */
static int
wm_nvm_read(struct wm_softc *sc, int word, int wordcnt, uint16_t *data)
{
	int rv;

	if (sc->sc_flags & WM_F_EEPROM_INVALID)
		return 1;

	if (wm_nvm_acquire(sc))
		return 1;

	if ((sc->sc_type == WM_T_ICH8) || (sc->sc_type == WM_T_ICH9)
	    || (sc->sc_type == WM_T_ICH10) || (sc->sc_type == WM_T_PCH)
	    || (sc->sc_type == WM_T_PCH2) || (sc->sc_type == WM_T_PCH_LPT))
		rv = wm_nvm_read_ich8(sc, word, wordcnt, data);
	else if (sc->sc_flags & WM_F_EEPROM_INVM)
		rv = wm_nvm_read_invm(sc, word, wordcnt, data);
	else if (sc->sc_flags & WM_F_EEPROM_EERDEEWR)
		rv = wm_nvm_read_eerd(sc, word, wordcnt, data);
	else if (sc->sc_flags & WM_F_EEPROM_SPI)
		rv = wm_nvm_read_spi(sc, word, wordcnt, data);
	else
		rv = wm_nvm_read_uwire(sc, word, wordcnt, data);

	wm_nvm_release(sc);
	return rv;
}

/*
 * Hardware semaphores.
 * Very complexed...
 */

static int
wm_get_swsm_semaphore(struct wm_softc *sc)
{
	int32_t timeout;
	uint32_t swsm;

	if (sc->sc_flags & WM_F_LOCK_SWSM) {
		/* Get the SW semaphore. */
		timeout = sc->sc_nvm_wordsize + 1;
		while (timeout) {
			swsm = CSR_READ(sc, WMREG_SWSM);

			if ((swsm & SWSM_SMBI) == 0)
				break;

			delay(50);
			timeout--;
		}

		if (timeout == 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not acquire SWSM SMBI\n");
			return 1;
		}
	}

	/* Get the FW semaphore. */
	timeout = sc->sc_nvm_wordsize + 1;
	while (timeout) {
		swsm = CSR_READ(sc, WMREG_SWSM);
		swsm |= SWSM_SWESMBI;
		CSR_WRITE(sc, WMREG_SWSM, swsm);
		/* If we managed to set the bit we got the semaphore. */
		swsm = CSR_READ(sc, WMREG_SWSM);
		if (swsm & SWSM_SWESMBI)
			break;

		delay(50);
		timeout--;
	}

	if (timeout == 0) {
		aprint_error_dev(sc->sc_dev, "could not acquire SWSM SWESMBI\n");
		/* Release semaphores */
		wm_put_swsm_semaphore(sc);
		return 1;
	}
	return 0;
}

static void
wm_put_swsm_semaphore(struct wm_softc *sc)
{
	uint32_t swsm;

	swsm = CSR_READ(sc, WMREG_SWSM);
	swsm &= ~(SWSM_SMBI | SWSM_SWESMBI);
	CSR_WRITE(sc, WMREG_SWSM, swsm);
}

static int
wm_get_swfw_semaphore(struct wm_softc *sc, uint16_t mask)
{
	uint32_t swfw_sync;
	uint32_t swmask = mask << SWFW_SOFT_SHIFT;
	uint32_t fwmask = mask << SWFW_FIRM_SHIFT;
	int timeout = 200;

	for (timeout = 0; timeout < 200; timeout++) {
		if (sc->sc_flags & WM_F_LOCK_SWSM) {
			if (wm_get_swsm_semaphore(sc)) {
				aprint_error_dev(sc->sc_dev,
				    "%s: failed to get semaphore\n",
				    __func__);
				return 1;
			}
		}
		swfw_sync = CSR_READ(sc, WMREG_SW_FW_SYNC);
		if ((swfw_sync & (swmask | fwmask)) == 0) {
			swfw_sync |= swmask;
			CSR_WRITE(sc, WMREG_SW_FW_SYNC, swfw_sync);
			if (sc->sc_flags & WM_F_LOCK_SWSM)
				wm_put_swsm_semaphore(sc);
			return 0;
		}
		if (sc->sc_flags & WM_F_LOCK_SWSM)
			wm_put_swsm_semaphore(sc);
		delay(5000);
	}
	printf("%s: failed to get swfw semaphore mask 0x%x swfw 0x%x\n",
	    device_xname(sc->sc_dev), mask, swfw_sync);
	return 1;
}

static void
wm_put_swfw_semaphore(struct wm_softc *sc, uint16_t mask)
{
	uint32_t swfw_sync;

	if (sc->sc_flags & WM_F_LOCK_SWSM) {
		while (wm_get_swsm_semaphore(sc) != 0)
			continue;
	}
	swfw_sync = CSR_READ(sc, WMREG_SW_FW_SYNC);
	swfw_sync &= ~(mask << SWFW_SOFT_SHIFT);
	CSR_WRITE(sc, WMREG_SW_FW_SYNC, swfw_sync);
	if (sc->sc_flags & WM_F_LOCK_SWSM)
		wm_put_swsm_semaphore(sc);
}

static int
wm_get_swfwhw_semaphore(struct wm_softc *sc)
{
	uint32_t ext_ctrl;
	int timeout = 200;

	for (timeout = 0; timeout < 200; timeout++) {
		ext_ctrl = CSR_READ(sc, WMREG_EXTCNFCTR);
		ext_ctrl |= EXTCNFCTR_MDIO_SW_OWNERSHIP;
		CSR_WRITE(sc, WMREG_EXTCNFCTR, ext_ctrl);

		ext_ctrl = CSR_READ(sc, WMREG_EXTCNFCTR);
		if (ext_ctrl & EXTCNFCTR_MDIO_SW_OWNERSHIP)
			return 0;
		delay(5000);
	}
	printf("%s: failed to get swfwhw semaphore ext_ctrl 0x%x\n",
	    device_xname(sc->sc_dev), ext_ctrl);
	return 1;
}

static void
wm_put_swfwhw_semaphore(struct wm_softc *sc)
{
	uint32_t ext_ctrl;
	ext_ctrl = CSR_READ(sc, WMREG_EXTCNFCTR);
	ext_ctrl &= ~EXTCNFCTR_MDIO_SW_OWNERSHIP;
	CSR_WRITE(sc, WMREG_EXTCNFCTR, ext_ctrl);
}

static int
wm_get_hw_semaphore_82573(struct wm_softc *sc)
{
	int i = 0;
	uint32_t reg;

	reg = CSR_READ(sc, WMREG_EXTCNFCTR);
	do {
		CSR_WRITE(sc, WMREG_EXTCNFCTR,
		    reg | EXTCNFCTR_MDIO_SW_OWNERSHIP);
		reg = CSR_READ(sc, WMREG_EXTCNFCTR);
		if ((reg & EXTCNFCTR_MDIO_SW_OWNERSHIP) != 0)
			break;
		delay(2*1000);
		i++;
	} while (i < WM_MDIO_OWNERSHIP_TIMEOUT);

	if (i == WM_MDIO_OWNERSHIP_TIMEOUT) {
		wm_put_hw_semaphore_82573(sc);
		log(LOG_ERR, "%s: Driver can't access the PHY\n",
		    device_xname(sc->sc_dev));
		return -1;
	}

	return 0;
}

static void
wm_put_hw_semaphore_82573(struct wm_softc *sc)
{
	uint32_t reg;

	reg = CSR_READ(sc, WMREG_EXTCNFCTR);
	reg &= ~EXTCNFCTR_MDIO_SW_OWNERSHIP;
	CSR_WRITE(sc, WMREG_EXTCNFCTR, reg);
}

/*
 * Management mode and power management related subroutines.
 * BMC, AMT, suspend/resume and EEE.
 */

static int
wm_check_mng_mode(struct wm_softc *sc)
{
	int rv;

	switch (sc->sc_type) {
	case WM_T_ICH8:
	case WM_T_ICH9:
	case WM_T_ICH10:
	case WM_T_PCH:
	case WM_T_PCH2:
	case WM_T_PCH_LPT:
		rv = wm_check_mng_mode_ich8lan(sc);
		break;
	case WM_T_82574:
	case WM_T_82583:
		rv = wm_check_mng_mode_82574(sc);
		break;
	case WM_T_82571:
	case WM_T_82572:
	case WM_T_82573:
	case WM_T_80003:
		rv = wm_check_mng_mode_generic(sc);
		break;
	default:
		/* noting to do */
		rv = 0;
		break;
	}

	return rv;
}

static int
wm_check_mng_mode_ich8lan(struct wm_softc *sc)
{
	uint32_t fwsm;

	fwsm = CSR_READ(sc, WMREG_FWSM);

	if ((fwsm & FWSM_MODE_MASK) == (MNG_ICH_IAMT_MODE << FWSM_MODE_SHIFT))
		return 1;

	return 0;
}

static int
wm_check_mng_mode_82574(struct wm_softc *sc)
{
	uint16_t data;

	wm_nvm_read(sc, NVM_OFF_CFG2, 1, &data);

	if ((data & NVM_CFG2_MNGM_MASK) != 0)
		return 1;

	return 0;
}

static int
wm_check_mng_mode_generic(struct wm_softc *sc)
{
	uint32_t fwsm;

	fwsm = CSR_READ(sc, WMREG_FWSM);

	if ((fwsm & FWSM_MODE_MASK) == (MNG_IAMT_MODE << FWSM_MODE_SHIFT))
		return 1;

	return 0;
}

static int
wm_enable_mng_pass_thru(struct wm_softc *sc)
{
	uint32_t manc, fwsm, factps;

	if ((sc->sc_flags & WM_F_ASF_FIRMWARE_PRES) == 0)
		return 0;

	manc = CSR_READ(sc, WMREG_MANC);

	DPRINTF(WM_DEBUG_MANAGE, ("%s: MANC (%08x)\n",
		device_xname(sc->sc_dev), manc));
	if ((manc & MANC_RECV_TCO_EN) == 0)
		return 0;

	if ((sc->sc_flags & WM_F_ARC_SUBSYS_VALID) != 0) {
		fwsm = CSR_READ(sc, WMREG_FWSM);
		factps = CSR_READ(sc, WMREG_FACTPS);
		if (((factps & FACTPS_MNGCG) == 0)
		    && ((fwsm & FWSM_MODE_MASK)
			== (MNG_ICH_IAMT_MODE << FWSM_MODE_SHIFT)))
			return 1;
	} else if ((sc->sc_type == WM_T_82574) || (sc->sc_type == WM_T_82583)){
		uint16_t data;

		factps = CSR_READ(sc, WMREG_FACTPS);
		wm_nvm_read(sc, NVM_OFF_CFG2, 1, &data);
		DPRINTF(WM_DEBUG_MANAGE, ("%s: FACTPS = %08x, CFG2=%04x\n",
			device_xname(sc->sc_dev), factps, data));
		if (((factps & FACTPS_MNGCG) == 0)
		    && ((data & NVM_CFG2_MNGM_MASK)
			== (NVM_CFG2_MNGM_PT << NVM_CFG2_MNGM_SHIFT)))
			return 1;
	} else if (((manc & MANC_SMBUS_EN) != 0)
	    && ((manc & MANC_ASF_EN) == 0))
		return 1;

	return 0;
}

static int
wm_check_reset_block(struct wm_softc *sc)
{
	uint32_t reg;

	switch (sc->sc_type) {
	case WM_T_ICH8:
	case WM_T_ICH9:
	case WM_T_ICH10:
	case WM_T_PCH:
	case WM_T_PCH2:
	case WM_T_PCH_LPT:
		reg = CSR_READ(sc, WMREG_FWSM);
		if ((reg & FWSM_RSPCIPHY) != 0)
			return 0;
		else
			return -1;
		break;
	case WM_T_82571:
	case WM_T_82572:
	case WM_T_82573:
	case WM_T_82574:
	case WM_T_82583:
	case WM_T_80003:
		reg = CSR_READ(sc, WMREG_MANC);
		if ((reg & MANC_BLK_PHY_RST_ON_IDE) != 0)
			return -1;
		else
			return 0;
		break;
	default:
		/* no problem */
		break;
	}

	return 0;
}

static void
wm_get_hw_control(struct wm_softc *sc)
{
	uint32_t reg;

	switch (sc->sc_type) {
	case WM_T_82573:
		reg = CSR_READ(sc, WMREG_SWSM);
		CSR_WRITE(sc, WMREG_SWSM, reg | SWSM_DRV_LOAD);
		break;
	case WM_T_82571:
	case WM_T_82572:
	case WM_T_82574:
	case WM_T_82583:
	case WM_T_80003:
	case WM_T_ICH8:
	case WM_T_ICH9:
	case WM_T_ICH10:
	case WM_T_PCH:
	case WM_T_PCH2:
	case WM_T_PCH_LPT:
		reg = CSR_READ(sc, WMREG_CTRL_EXT);
		CSR_WRITE(sc, WMREG_CTRL_EXT, reg | CTRL_EXT_DRV_LOAD);
		break;
	default:
		break;
	}
}

static void
wm_release_hw_control(struct wm_softc *sc)
{
	uint32_t reg;

	if ((sc->sc_flags & WM_F_HAS_MANAGE) == 0)
		return;

	if (sc->sc_type == WM_T_82573) {
		reg = CSR_READ(sc, WMREG_SWSM);
		reg &= ~SWSM_DRV_LOAD;
		CSR_WRITE(sc, WMREG_SWSM, reg & ~SWSM_DRV_LOAD);
	} else {
		reg = CSR_READ(sc, WMREG_CTRL_EXT);
		CSR_WRITE(sc, WMREG_CTRL_EXT, reg & ~CTRL_EXT_DRV_LOAD);
	}
}

static void
wm_gate_hw_phy_config_ich8lan(struct wm_softc *sc, int on)
{
	uint32_t reg;

	reg = CSR_READ(sc, WMREG_EXTCNFCTR);

	if (on != 0)
		reg |= EXTCNFCTR_GATE_PHY_CFG;
	else
		reg &= ~EXTCNFCTR_GATE_PHY_CFG;

	CSR_WRITE(sc, WMREG_EXTCNFCTR, reg);
}

static void
wm_smbustopci(struct wm_softc *sc)
{
	uint32_t fwsm;

	fwsm = CSR_READ(sc, WMREG_FWSM);
	if (((fwsm & FWSM_FW_VALID) == 0)
	    && ((wm_check_reset_block(sc) == 0))) {
		sc->sc_ctrl |= CTRL_LANPHYPC_OVERRIDE;
		sc->sc_ctrl &= ~CTRL_LANPHYPC_VALUE;
		CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl);
		CSR_WRITE_FLUSH(sc);
		delay(10);
		sc->sc_ctrl &= ~CTRL_LANPHYPC_OVERRIDE;
		CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl);
		CSR_WRITE_FLUSH(sc);
		delay(50*1000);

		/*
		 * Gate automatic PHY configuration by hardware on non-managed
		 * 82579
		 */
		if (sc->sc_type == WM_T_PCH2)
			wm_gate_hw_phy_config_ich8lan(sc, 1);
	}
}

static void
wm_init_manageability(struct wm_softc *sc)
{

	if (sc->sc_flags & WM_F_HAS_MANAGE) {
		uint32_t manc2h = CSR_READ(sc, WMREG_MANC2H);
		uint32_t manc = CSR_READ(sc, WMREG_MANC);

		/* Disable hardware interception of ARP */
		manc &= ~MANC_ARP_EN;

		/* Enable receiving management packets to the host */
		if (sc->sc_type >= WM_T_82571) {
			manc |= MANC_EN_MNG2HOST;
			manc2h |= MANC2H_PORT_623| MANC2H_PORT_624;
			CSR_WRITE(sc, WMREG_MANC2H, manc2h);
		}

		CSR_WRITE(sc, WMREG_MANC, manc);
	}
}

static void
wm_release_manageability(struct wm_softc *sc)
{

	if (sc->sc_flags & WM_F_HAS_MANAGE) {
		uint32_t manc = CSR_READ(sc, WMREG_MANC);

		manc |= MANC_ARP_EN;
		if (sc->sc_type >= WM_T_82571)
			manc &= ~MANC_EN_MNG2HOST;

		CSR_WRITE(sc, WMREG_MANC, manc);
	}
}

static void
wm_get_wakeup(struct wm_softc *sc)
{

	/* 0: HAS_AMT, ARC_SUBSYS_VALID, ASF_FIRMWARE_PRES */
	switch (sc->sc_type) {
	case WM_T_82573:
	case WM_T_82583:
		sc->sc_flags |= WM_F_HAS_AMT;
		/* FALLTHROUGH */
	case WM_T_80003:
	case WM_T_82541:
	case WM_T_82547:
	case WM_T_82571:
	case WM_T_82572:
	case WM_T_82574:
	case WM_T_82575:
	case WM_T_82576:
	case WM_T_82580:
	case WM_T_I350:
	case WM_T_I354:
		if ((CSR_READ(sc, WMREG_FWSM) & FWSM_MODE_MASK) != 0)
			sc->sc_flags |= WM_F_ARC_SUBSYS_VALID;
		sc->sc_flags |= WM_F_ASF_FIRMWARE_PRES;
		break;
	case WM_T_ICH8:
	case WM_T_ICH9:
	case WM_T_ICH10:
	case WM_T_PCH:
	case WM_T_PCH2:
	case WM_T_PCH_LPT:
		sc->sc_flags |= WM_F_HAS_AMT;
		sc->sc_flags |= WM_F_ASF_FIRMWARE_PRES;
		break;
	default:
		break;
	}

	/* 1: HAS_MANAGE */
	if (wm_enable_mng_pass_thru(sc) != 0)
		sc->sc_flags |= WM_F_HAS_MANAGE;

#ifdef WM_DEBUG
	printf("\n");
	if ((sc->sc_flags & WM_F_HAS_AMT) != 0)
		printf("HAS_AMT,");
	if ((sc->sc_flags & WM_F_ARC_SUBSYS_VALID) != 0)
		printf("ARC_SUBSYS_VALID,");
	if ((sc->sc_flags & WM_F_ASF_FIRMWARE_PRES) != 0)
		printf("ASF_FIRMWARE_PRES,");
	if ((sc->sc_flags & WM_F_HAS_MANAGE) != 0)
		printf("HAS_MANAGE,");
	printf("\n");
#endif
	/*
	 * Note that the WOL flags is set after the resetting of the eeprom
	 * stuff
	 */
}

#ifdef WM_WOL
/* WOL in the newer chipset interfaces (pchlan) */
static void
wm_enable_phy_wakeup(struct wm_softc *sc)
{
#if 0
	uint16_t preg;

	/* Copy MAC RARs to PHY RARs */

	/* Copy MAC MTA to PHY MTA */

	/* Configure PHY Rx Control register */

	/* Enable PHY wakeup in MAC register */

	/* Configure and enable PHY wakeup in PHY registers */

	/* Activate PHY wakeup */

	/* XXX */
#endif
}

/* Power down workaround on D3 */
static void
wm_igp3_phy_powerdown_workaround_ich8lan(struct wm_softc *sc)
{
	uint32_t reg;
	int i;

	for (i = 0; i < 2; i++) {
		/* Disable link */
		reg = CSR_READ(sc, WMREG_PHY_CTRL);
		reg |= PHY_CTRL_GBE_DIS | PHY_CTRL_NOND0A_GBE_DIS;
		CSR_WRITE(sc, WMREG_PHY_CTRL, reg);

		/*
		 * Call gig speed drop workaround on Gig disable before
		 * accessing any PHY registers
		 */
		if (sc->sc_type == WM_T_ICH8)
			wm_gig_downshift_workaround_ich8lan(sc);

		/* Write VR power-down enable */
		reg = sc->sc_mii.mii_readreg(sc->sc_dev, 1, IGP3_VR_CTRL);
		reg &= ~IGP3_VR_CTRL_DEV_POWERDOWN_MODE_MASK;
		reg |= IGP3_VR_CTRL_MODE_SHUTDOWN;
		sc->sc_mii.mii_writereg(sc->sc_dev, 1, IGP3_VR_CTRL, reg);

		/* Read it back and test */
		reg = sc->sc_mii.mii_readreg(sc->sc_dev, 1, IGP3_VR_CTRL);
		reg &= IGP3_VR_CTRL_DEV_POWERDOWN_MODE_MASK;
		if ((reg == IGP3_VR_CTRL_MODE_SHUTDOWN) || (i != 0))
			break;

		/* Issue PHY reset and repeat at most one more time */
		CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl | CTRL_PHY_RESET);
	}
}

static void
wm_enable_wakeup(struct wm_softc *sc)
{
	uint32_t reg, pmreg;
	pcireg_t pmode;

	if (pci_get_capability(sc->sc_pc, sc->sc_pcitag, PCI_CAP_PWRMGMT,
		&pmreg, NULL) == 0)
		return;

	/* Advertise the wakeup capability */
	CSR_WRITE(sc, WMREG_CTRL, sc->sc_ctrl | CTRL_SWDPIN(2)
	    | CTRL_SWDPIN(3));
	CSR_WRITE(sc, WMREG_WUC, WUC_APME);

	/* ICH workaround */
	switch (sc->sc_type) {
	case WM_T_ICH8:
	case WM_T_ICH9:
	case WM_T_ICH10:
	case WM_T_PCH:
	case WM_T_PCH2:
	case WM_T_PCH_LPT:
		/* Disable gig during WOL */
		reg = CSR_READ(sc, WMREG_PHY_CTRL);
		reg |= PHY_CTRL_D0A_LPLU | PHY_CTRL_GBE_DIS;
		CSR_WRITE(sc, WMREG_PHY_CTRL, reg);
		if (sc->sc_type == WM_T_PCH)
			wm_gmii_reset(sc);

		/* Power down workaround */
		if (sc->sc_phytype == WMPHY_82577) {
			struct mii_softc *child;

			/* Assume that the PHY is copper */
			child = LIST_FIRST(&sc->sc_mii.mii_phys);
			if (child->mii_mpd_rev <= 2)
				sc->sc_mii.mii_writereg(sc->sc_dev, 1,
				    (768 << 5) | 25, 0x0444); /* magic num */
		}
		break;
	default:
		break;
	}

	/* Keep the laser running on fiber adapters */
	if ((sc->sc_mediatype == WM_MEDIATYPE_FIBER)
	    || (sc->sc_mediatype == WM_MEDIATYPE_SERDES)) {
		reg = CSR_READ(sc, WMREG_CTRL_EXT);
		reg |= CTRL_EXT_SWDPIN(3);
		CSR_WRITE(sc, WMREG_CTRL_EXT, reg);
	}

	reg = CSR_READ(sc, WMREG_WUFC) | WUFC_MAG;
#if 0	/* for the multicast packet */
	reg |= WUFC_MC;
	CSR_WRITE(sc, WMREG_RCTL, CSR_READ(sc, WMREG_RCTL) | RCTL_MPE);
#endif

	if (sc->sc_type == WM_T_PCH) {
		wm_enable_phy_wakeup(sc);
	} else {
		CSR_WRITE(sc, WMREG_WUC, WUC_PME_EN);
		CSR_WRITE(sc, WMREG_WUFC, reg);
	}

	if (((sc->sc_type == WM_T_ICH8) || (sc->sc_type == WM_T_ICH9)
		|| (sc->sc_type == WM_T_ICH10) || (sc->sc_type == WM_T_PCH)
		|| (sc->sc_type == WM_T_PCH2))
		    && (sc->sc_phytype == WMPHY_IGP_3))
			wm_igp3_phy_powerdown_workaround_ich8lan(sc);

	/* Request PME */
	pmode = pci_conf_read(sc->sc_pc, sc->sc_pcitag, pmreg + PCI_PMCSR);
#if 0
	/* Disable WOL */
	pmode &= ~(PCI_PMCSR_PME_STS | PCI_PMCSR_PME_EN);
#else
	/* For WOL */
	pmode |= PCI_PMCSR_PME_STS | PCI_PMCSR_PME_EN;
#endif
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, pmreg + PCI_PMCSR, pmode);
}
#endif /* WM_WOL */

/* EEE */

static void
wm_set_eee_i350(struct wm_softc *sc)
{
	uint32_t ipcnfg, eeer;

	ipcnfg = CSR_READ(sc, WMREG_IPCNFG);
	eeer = CSR_READ(sc, WMREG_EEER);

	if ((sc->sc_flags & WM_F_EEE) != 0) {
		ipcnfg |= (IPCNFG_EEE_1G_AN | IPCNFG_EEE_100M_AN);
		eeer |= (EEER_TX_LPI_EN | EEER_RX_LPI_EN
		    | EEER_LPI_FC);
	} else {
		ipcnfg &= ~(IPCNFG_EEE_1G_AN | IPCNFG_EEE_100M_AN);
		ipcnfg &= ~IPCNFG_10BASE_TE;
		eeer &= ~(EEER_TX_LPI_EN | EEER_RX_LPI_EN
		    | EEER_LPI_FC);
	}

	CSR_WRITE(sc, WMREG_IPCNFG, ipcnfg);
	CSR_WRITE(sc, WMREG_EEER, eeer);
	CSR_READ(sc, WMREG_IPCNFG); /* XXX flush? */
	CSR_READ(sc, WMREG_EEER); /* XXX flush? */
}

/*
 * Workarounds (mainly PHY related).
 * Basically, PHY's workarounds are in the PHY drivers.
 */

/* Work-around for 82566 Kumeran PCS lock loss */
static void
wm_kmrn_lock_loss_workaround_ich8lan(struct wm_softc *sc)
{
	int miistatus, active, i;
	int reg;

	miistatus = sc->sc_mii.mii_media_status;

	/* If the link is not up, do nothing */
	if ((miistatus & IFM_ACTIVE) != 0)
		return;

	active = sc->sc_mii.mii_media_active;

	/* Nothing to do if the link is other than 1Gbps */
	if (IFM_SUBTYPE(active) != IFM_1000_T)
		return;

	for (i = 0; i < 10; i++) {
		/* read twice */
		reg = wm_gmii_i80003_readreg(sc->sc_dev, 1, IGP3_KMRN_DIAG);
		reg = wm_gmii_i80003_readreg(sc->sc_dev, 1, IGP3_KMRN_DIAG);
		if ((reg & IGP3_KMRN_DIAG_PCS_LOCK_LOSS) != 0)
			goto out;	/* GOOD! */

		/* Reset the PHY */
		wm_gmii_reset(sc);
		delay(5*1000);
	}

	/* Disable GigE link negotiation */
	reg = CSR_READ(sc, WMREG_PHY_CTRL);
	reg |= PHY_CTRL_GBE_DIS | PHY_CTRL_NOND0A_GBE_DIS;
	CSR_WRITE(sc, WMREG_PHY_CTRL, reg);

	/*
	 * Call gig speed drop workaround on Gig disable before accessing
	 * any PHY registers.
	 */
	wm_gig_downshift_workaround_ich8lan(sc);

out:
	return;
}

/* WOL from S5 stops working */
static void
wm_gig_downshift_workaround_ich8lan(struct wm_softc *sc)
{
	uint16_t kmrn_reg;

	/* Only for igp3 */
	if (sc->sc_phytype == WMPHY_IGP_3) {
		kmrn_reg = wm_kmrn_readreg(sc, KUMCTRLSTA_OFFSET_DIAG);
		kmrn_reg |= KUMCTRLSTA_DIAG_NELPBK;
		wm_kmrn_writereg(sc, KUMCTRLSTA_OFFSET_DIAG, kmrn_reg);
		kmrn_reg &= ~KUMCTRLSTA_DIAG_NELPBK;
		wm_kmrn_writereg(sc, KUMCTRLSTA_OFFSET_DIAG, kmrn_reg);
	}
}

/*
 * Workaround for pch's PHYs
 * XXX should be moved to new PHY driver?
 */
static void
wm_hv_phy_workaround_ich8lan(struct wm_softc *sc)
{
	if (sc->sc_phytype == WMPHY_82577)
		wm_set_mdio_slow_mode_hv(sc);

	/* (PCH rev.2) && (82577 && (phy rev 2 or 3)) */

	/* (82577 && (phy rev 1 or 2)) || (82578 & phy rev 1)*/

	/* 82578 */
	if (sc->sc_phytype == WMPHY_82578) {
		/* PCH rev. < 3 */
		if (sc->sc_rev < 3) {
			/* XXX 6 bit shift? Why? Is it page2? */
			wm_gmii_hv_writereg(sc->sc_dev, 1, ((1 << 6) | 0x29),
			    0x66c0);
			wm_gmii_hv_writereg(sc->sc_dev, 1, ((1 << 6) | 0x1e),
			    0xffff);
		}

		/* XXX phy rev. < 2 */
	}

	/* Select page 0 */

	/* XXX acquire semaphore */
	wm_gmii_i82544_writereg(sc->sc_dev, 1, MII_IGPHY_PAGE_SELECT, 0);
	/* XXX release semaphore */

	/*
	 * Configure the K1 Si workaround during phy reset assuming there is
	 * link so that it disables K1 if link is in 1Gbps.
	 */
	wm_k1_gig_workaround_hv(sc, 1);
}

static void
wm_lv_phy_workaround_ich8lan(struct wm_softc *sc)
{

	wm_set_mdio_slow_mode_hv(sc);
}

static void
wm_k1_gig_workaround_hv(struct wm_softc *sc, int link)
{
	int k1_enable = sc->sc_nvm_k1_enabled;

	/* XXX acquire semaphore */

	if (link) {
		k1_enable = 0;

		/* Link stall fix for link up */
		wm_gmii_hv_writereg(sc->sc_dev, 1, IGP3_KMRN_DIAG, 0x0100);
	} else {
		/* Link stall fix for link down */
		wm_gmii_hv_writereg(sc->sc_dev, 1, IGP3_KMRN_DIAG, 0x4100);
	}

	wm_configure_k1_ich8lan(sc, k1_enable);

	/* XXX release semaphore */
}

static void
wm_set_mdio_slow_mode_hv(struct wm_softc *sc)
{
	uint32_t reg;

	reg = wm_gmii_hv_readreg(sc->sc_dev, 1, HV_KMRN_MODE_CTRL);
	wm_gmii_hv_writereg(sc->sc_dev, 1, HV_KMRN_MODE_CTRL,
	    reg | HV_KMRN_MDIO_SLOW);
}

static void
wm_configure_k1_ich8lan(struct wm_softc *sc, int k1_enable)
{
	uint32_t ctrl, ctrl_ext, tmp;
	uint16_t kmrn_reg;

	kmrn_reg = wm_kmrn_readreg(sc, KUMCTRLSTA_OFFSET_K1_CONFIG);

	if (k1_enable)
		kmrn_reg |= KUMCTRLSTA_K1_ENABLE;
	else
		kmrn_reg &= ~KUMCTRLSTA_K1_ENABLE;

	wm_kmrn_writereg(sc, KUMCTRLSTA_OFFSET_K1_CONFIG, kmrn_reg);

	delay(20);

	ctrl = CSR_READ(sc, WMREG_CTRL);
	ctrl_ext = CSR_READ(sc, WMREG_CTRL_EXT);

	tmp = ctrl & ~(CTRL_SPEED_1000 | CTRL_SPEED_100);
	tmp |= CTRL_FRCSPD;

	CSR_WRITE(sc, WMREG_CTRL, tmp);
	CSR_WRITE(sc, WMREG_CTRL_EXT, ctrl_ext | CTRL_EXT_SPD_BYPS);
	CSR_WRITE_FLUSH(sc);
	delay(20);

	CSR_WRITE(sc, WMREG_CTRL, ctrl);
	CSR_WRITE(sc, WMREG_CTRL_EXT, ctrl_ext);
	CSR_WRITE_FLUSH(sc);
	delay(20);
}

/* special case - for 82575 - need to do manual init ... */
static void
wm_reset_init_script_82575(struct wm_softc *sc)
{
	/*
	 * remark: this is untested code - we have no board without EEPROM
	 *  same setup as mentioned int the FreeBSD driver for the i82575
	 */

	/* SerDes configuration via SERDESCTRL */
	wm_82575_write_8bit_ctlr_reg(sc, WMREG_SCTL, 0x00, 0x0c);
	wm_82575_write_8bit_ctlr_reg(sc, WMREG_SCTL, 0x01, 0x78);
	wm_82575_write_8bit_ctlr_reg(sc, WMREG_SCTL, 0x1b, 0x23);
	wm_82575_write_8bit_ctlr_reg(sc, WMREG_SCTL, 0x23, 0x15);

	/* CCM configuration via CCMCTL register */
	wm_82575_write_8bit_ctlr_reg(sc, WMREG_CCMCTL, 0x14, 0x00);
	wm_82575_write_8bit_ctlr_reg(sc, WMREG_CCMCTL, 0x10, 0x00);

	/* PCIe lanes configuration */
	wm_82575_write_8bit_ctlr_reg(sc, WMREG_GIOCTL, 0x00, 0xec);
	wm_82575_write_8bit_ctlr_reg(sc, WMREG_GIOCTL, 0x61, 0xdf);
	wm_82575_write_8bit_ctlr_reg(sc, WMREG_GIOCTL, 0x34, 0x05);
	wm_82575_write_8bit_ctlr_reg(sc, WMREG_GIOCTL, 0x2f, 0x81);

	/* PCIe PLL Configuration */
	wm_82575_write_8bit_ctlr_reg(sc, WMREG_SCCTL, 0x02, 0x47);
	wm_82575_write_8bit_ctlr_reg(sc, WMREG_SCCTL, 0x14, 0x00);
	wm_82575_write_8bit_ctlr_reg(sc, WMREG_SCCTL, 0x10, 0x00);
}

static void
wm_reset_mdicnfg_82580(struct wm_softc *sc)
{
	uint32_t reg;
	uint16_t nvmword;
	int rv;

	if ((sc->sc_flags & WM_F_SGMII) == 0)
		return;

	rv = wm_nvm_read(sc, NVM_OFF_LAN_FUNC_82580(sc->sc_funcid)
	    + NVM_OFF_CFG3_PORTA, 1, &nvmword);
	if (rv != 0) {
		aprint_error_dev(sc->sc_dev, "%s: failed to read NVM\n",
		    __func__);
		return;
	}

	reg = CSR_READ(sc, WMREG_MDICNFG);
	if (nvmword & NVM_CFG3_PORTA_EXT_MDIO)
		reg |= MDICNFG_DEST;
	if (nvmword & NVM_CFG3_PORTA_COM_MDIO)
		reg |= MDICNFG_COM_MDIO;
	CSR_WRITE(sc, WMREG_MDICNFG, reg);
}

/*
 * I210 Errata 25 and I211 Errata 10
 * Slow System Clock.
 */
static void
wm_pll_workaround_i210(struct wm_softc *sc)
{
	uint32_t mdicnfg, wuc;
	uint32_t reg;
	pcireg_t pcireg;
	uint32_t pmreg;
	uint16_t nvmword, tmp_nvmword;
	int phyval;
	bool wa_done = false;
	int i;

	/* Save WUC and MDICNFG registers */
	wuc = CSR_READ(sc, WMREG_WUC);
	mdicnfg = CSR_READ(sc, WMREG_MDICNFG);

	reg = mdicnfg & ~MDICNFG_DEST;
	CSR_WRITE(sc, WMREG_MDICNFG, reg);

	if (wm_nvm_read(sc, INVM_AUTOLOAD, 1, &nvmword) != 0)
		nvmword = INVM_DEFAULT_AL;
	tmp_nvmword = nvmword | INVM_PLL_WO_VAL;

	/* Get Power Management cap offset */
	if (pci_get_capability(sc->sc_pc, sc->sc_pcitag, PCI_CAP_PWRMGMT,
		&pmreg, NULL) == 0)
		return;
	for (i = 0; i < WM_MAX_PLL_TRIES; i++) {
		phyval = wm_gmii_gs40g_readreg(sc->sc_dev, 1,
		    GS40G_PHY_PLL_FREQ_PAGE | GS40G_PHY_PLL_FREQ_REG);

		if ((phyval & GS40G_PHY_PLL_UNCONF) != GS40G_PHY_PLL_UNCONF) {
			break; /* OK */
		}

		wa_done = true;
		/* Directly reset the internal PHY */
		reg = CSR_READ(sc, WMREG_CTRL);
		CSR_WRITE(sc, WMREG_CTRL, reg | CTRL_PHY_RESET);

		reg = CSR_READ(sc, WMREG_CTRL_EXT);
		reg |= CTRL_EXT_PHYPDEN | CTRL_EXT_SDLPE;
		CSR_WRITE(sc, WMREG_CTRL_EXT, reg);

		CSR_WRITE(sc, WMREG_WUC, 0);
		reg = (INVM_AUTOLOAD << 4) | (tmp_nvmword << 16);
		CSR_WRITE(sc, WMREG_EEARBC_I210, reg);

		pcireg = pci_conf_read(sc->sc_pc, sc->sc_pcitag,
		    pmreg + PCI_PMCSR);
		pcireg |= PCI_PMCSR_STATE_D3;
		pci_conf_write(sc->sc_pc, sc->sc_pcitag,
		    pmreg + PCI_PMCSR, pcireg);
		delay(1000);
		pcireg &= ~PCI_PMCSR_STATE_D3;
		pci_conf_write(sc->sc_pc, sc->sc_pcitag,
		    pmreg + PCI_PMCSR, pcireg);

		reg = (INVM_AUTOLOAD << 4) | (nvmword << 16);
		CSR_WRITE(sc, WMREG_EEARBC_I210, reg);

		/* Restore WUC register */
		CSR_WRITE(sc, WMREG_WUC, wuc);
	}

	/* Restore MDICNFG setting */
	CSR_WRITE(sc, WMREG_MDICNFG, mdicnfg);
	if (wa_done)
		aprint_verbose_dev(sc->sc_dev, "I210 workaround done\n");
}

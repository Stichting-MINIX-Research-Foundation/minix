/******************************************************************************

  Copyright (c) 2001-2013, Intel Corporation 
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

******************************************************************************/
/*
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Coyote Point Systems, Inc.
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
 */
/*$FreeBSD: head/sys/dev/ixgbe/ixgbe.h 279393 2015-02-28 14:57:57Z ngie $*/
/*$NetBSD: ixgbe.h,v 1.9 2015/08/17 06:16:03 knakahara Exp $*/


#ifndef _IXGBE_H_
#define _IXGBE_H_


#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/systm.h>
#include <sys/time.h>
#if __FreeBSD_version >= 800000
#include <sys/buf_ring.h>
#endif
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/bpf.h>
#include <net/if_ether.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>
#include <net/if_types.h>
#include <net/if_vlanvar.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <sys/bus.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/endian.h>
#include <sys/workqueue.h>
#include <sys/cpu.h>
#include <sys/interrupt.h>

#include "ixgbe_netbsd.h"
#include "ixgbe_api.h"

/* Tunables */

/*
 * TxDescriptors Valid Range: 64-4096 Default Value: 256 This value is the
 * number of transmit descriptors allocated by the driver. Increasing this
 * value allows the driver to queue more transmits. Each descriptor is 16
 * bytes. Performance tests have show the 2K value to be optimal for top
 * performance.
 */
#define DEFAULT_TXD	1024
#define PERFORM_TXD	2048
#define MAX_TXD		4096
#define MIN_TXD		64

/*
 * RxDescriptors Valid Range: 64-4096 Default Value: 256 This value is the
 * number of receive descriptors allocated for each RX queue. Increasing this
 * value allows the driver to buffer more incoming packets. Each descriptor
 * is 16 bytes.  A receive buffer is also allocated for each descriptor. 
 * 
 * Note: with 8 rings and a dual port card, it is possible to bump up 
 *	against the system mbuf pool limit, you can tune nmbclusters
 *	to adjust for this.
 */
#define DEFAULT_RXD	1024
#define PERFORM_RXD	2048
#define MAX_RXD		4096
#define MIN_RXD		64

/* Alignment for rings */
#define DBA_ALIGN	128

/*
 * This parameter controls the maximum no of times the driver will loop in
 * the isr. Minimum Value = 1
 */
#define MAX_LOOP	10

/*
 * This is the max watchdog interval, ie. the time that can
 * pass between any two TX clean operations, such only happening
 * when the TX hardware is functioning.
 */
#define IXGBE_WATCHDOG                   (10 * hz)

/*
 * This parameters control when the driver calls the routine to reclaim
 * transmit descriptors.
 */
#define IXGBE_TX_CLEANUP_THRESHOLD	(adapter->num_tx_desc / 8)
#define IXGBE_TX_OP_THRESHOLD		(adapter->num_tx_desc / 32)

#define IXGBE_MAX_FRAME_SIZE	0x3F00

/* Flow control constants */
#define IXGBE_FC_PAUSE		0xFFFF
#define IXGBE_FC_HI		0x20000
#define IXGBE_FC_LO		0x10000

/*
 * Used for optimizing small rx mbufs.  Effort is made to keep the copy
 * small and aligned for the CPU L1 cache.
 * 
 * MHLEN is typically 168 bytes, giving us 8-byte alignment.  Getting
 * 32 byte alignment needed for the fast bcopy results in 8 bytes being
 * wasted.  Getting 64 byte alignment, which _should_ be ideal for
 * modern Intel CPUs, results in 40 bytes wasted and a significant drop
 * in observed efficiency of the optimization, 97.9% -> 81.8%.
 */
#define	MPKTHSIZE		(offsetof(struct _mbuf_dummy, m_pktdat))
#define IXGBE_RX_COPY_HDR_PADDED	((((MPKTHSIZE - 1) / 32) + 1) * 32)
#define IXGBE_RX_COPY_LEN		(MSIZE - IXGBE_RX_COPY_HDR_PADDED)
#define IXGBE_RX_COPY_ALIGN		(IXGBE_RX_COPY_HDR_PADDED - MPKTHSIZE)

/* Keep older OS drivers building... */
#if !defined(SYSCTL_ADD_UQUAD)
#define SYSCTL_ADD_UQUAD SYSCTL_ADD_QUAD
#endif

/* Defines for printing debug information */
#define DEBUG_INIT  0
#define DEBUG_IOCTL 0
#define DEBUG_HW    0

#define INIT_DEBUGOUT(S)            if (DEBUG_INIT)  printf(S "\n")
#define INIT_DEBUGOUT1(S, A)        if (DEBUG_INIT)  printf(S "\n", A)
#define INIT_DEBUGOUT2(S, A, B)     if (DEBUG_INIT)  printf(S "\n", A, B)
#define IOCTL_DEBUGOUT(S)           if (DEBUG_IOCTL) printf(S "\n")
#define IOCTL_DEBUGOUT1(S, A)       if (DEBUG_IOCTL) printf(S "\n", A)
#define IOCTL_DEBUGOUT2(S, A, B)    if (DEBUG_IOCTL) printf(S "\n", A, B)
#define HW_DEBUGOUT(S)              if (DEBUG_HW) printf(S "\n")
#define HW_DEBUGOUT1(S, A)          if (DEBUG_HW) printf(S "\n", A)
#define HW_DEBUGOUT2(S, A, B)       if (DEBUG_HW) printf(S "\n", A, B)

#define MAX_NUM_MULTICAST_ADDRESSES     128
#define IXGBE_82598_SCATTER		100
#define IXGBE_82599_SCATTER		32
#define MSIX_82598_BAR			3
#define MSIX_82599_BAR			4
#define IXGBE_TSO_SIZE			262140
#define IXGBE_TX_BUFFER_SIZE		((u32) 1514)
#define IXGBE_RX_HDR			128
#define IXGBE_VFTA_SIZE			128
#define IXGBE_BR_SIZE			4096
#define IXGBE_QUEUE_MIN_FREE		32

/* IOCTL define to gather SFP+ Diagnostic data */
#define SIOCGI2C	SIOCGIFGENERIC

/* Offload bits in mbuf flag */
#define	M_CSUM_OFFLOAD	\
    (M_CSUM_IPv4|M_CSUM_UDPv4|M_CSUM_TCPv4|M_CSUM_UDPv6|M_CSUM_TCPv6)

/*
 * Interrupt Moderation parameters 
 */
#define IXGBE_LOW_LATENCY	128
#define IXGBE_AVE_LATENCY	400
#define IXGBE_BULK_LATENCY	1200
#define IXGBE_LINK_ITR		2000


/*
 *****************************************************************************
 * vendor_info_array
 * 
 * This array contains the list of Subvendor/Subdevice IDs on which the driver
 * should load.
 * 
 *****************************************************************************
 */
typedef struct _ixgbe_vendor_info_t {
	unsigned int    vendor_id;
	unsigned int    device_id;
	unsigned int    subvendor_id;
	unsigned int    subdevice_id;
	unsigned int    index;
} ixgbe_vendor_info_t;

/* This is used to get SFP+ module data */
struct ixgbe_i2c_req {
        u8 dev_addr;
        u8 offset;
        u8 len;
        u8 data[8];
};

struct ixgbe_tx_buf {
	union ixgbe_adv_tx_desc	*eop;
	struct mbuf	*m_head;
	bus_dmamap_t	map;
};

struct ixgbe_rx_buf {
	struct mbuf	*buf;
	struct mbuf	*fmp;
	bus_dmamap_t	pmap;
	u_int		flags;
#define IXGBE_RX_COPY	0x01
	uint64_t	addr;
};

/*
 * Bus dma allocation structure used by ixgbe_dma_malloc and ixgbe_dma_free.
 */
struct ixgbe_dma_alloc {
	bus_addr_t		dma_paddr;
	void			*dma_vaddr;
	ixgbe_dma_tag_t		*dma_tag;
	bus_dmamap_t		dma_map;
	bus_dma_segment_t	dma_seg;
	bus_size_t		dma_size;
};

/*
** Driver queue struct: this is the interrupt container
**  for the associated tx and rx ring.
*/
struct ix_queue {
	struct adapter		*adapter;
	u32			msix;           /* This queue's MSIX vector */
	u32			eims;           /* This queue's EIMS bit */
	u32			eitr_setting;
	struct resource		*res;
	void			*tag;
	struct tx_ring		*txr;
	struct rx_ring		*rxr;
	void			*que_si;
	u64			irqs;
	char			namebuf[32];
	char			evnamebuf[32];
};

/*
 * The transmit ring, one per queue
 */
struct tx_ring {
        struct adapter		*adapter;
	kmutex_t		tx_mtx;
	u32			me;
	struct timeval		watchdog_time;
	union ixgbe_adv_tx_desc	*tx_base;
	struct ixgbe_tx_buf	*tx_buffers;
	struct ixgbe_dma_alloc	txdma;
	volatile u16		tx_avail;
	u16			next_avail_desc;
	u16			next_to_clean;
	u32			process_limit;
	u16			num_desc;
	enum {
	    IXGBE_QUEUE_IDLE,
	    IXGBE_QUEUE_WORKING,
	    IXGBE_QUEUE_HUNG,
	}			queue_status;
	u32			txd_cmd;
	ixgbe_dma_tag_t		*txtag;
	char			mtx_name[16];
#ifndef IXGBE_LEGACY_TX
	struct buf_ring		*br;
	void			*txq_si;
#endif
#ifdef IXGBE_FDIR
	u16			atr_sample;
	u16			atr_count;
#endif
	u32			bytes;  /* used for AIM */
	u32			packets;
	/* Soft Stats */
	struct evcnt	   	tso_tx;
	struct evcnt	   	no_tx_map_avail;
	struct evcnt		no_desc_avail;
	struct evcnt		total_packets;
};


/*
 * The Receive ring, one per rx queue
 */
struct rx_ring {
        struct adapter		*adapter;
	kmutex_t		rx_mtx;
	u32			me;
	union ixgbe_adv_rx_desc	*rx_base;
	struct ixgbe_dma_alloc	rxdma;
#ifdef LRO
	struct lro_ctrl		lro;
#endif /* LRO */
	bool			lro_enabled;
	bool			hw_rsc;
	bool			vtag_strip;
        u16			next_to_refresh;
        u16 			next_to_check;
	u16			num_desc;
	u16			mbuf_sz;
	u32			process_limit;
	char			mtx_name[16];
	struct ixgbe_rx_buf	*rx_buffers;
	ixgbe_dma_tag_t		*ptag;

	u32			bytes; /* Used for AIM calc */
	u32			packets;

	/* Soft stats */
	struct evcnt		rx_irq;
	struct evcnt		rx_copies;
	struct evcnt		rx_packets;
	struct evcnt 		rx_bytes;
	struct evcnt 		rx_discarded;
	struct evcnt 		no_jmbuf;
	u64 			rsc_num;
#ifdef IXGBE_FDIR
	u64			flm;
#endif
};

/* Our adapter structure */
struct adapter {
	struct ifnet		*ifp;
	struct ixgbe_hw		hw;

	struct ixgbe_osdep	osdep;
	device_t		dev;

	struct resource		*pci_mem;
	struct resource		*msix_mem;

	/*
	 * Interrupt resources: this set is
	 * either used for legacy, or for Link
	 * when doing MSIX
	 */
	void			*tag;
	struct resource 	*res;

	struct ifmedia		media;
	callout_t		timer;
	int			msix;
	int			if_flags;

	kmutex_t		core_mtx;

	unsigned int		num_queues;

	/*
	** Shadow VFTA table, this is needed because
	** the real vlan filter table gets cleared during
	** a soft reset and the driver needs to be able
	** to repopulate it.
	*/
	u32			shadow_vfta[IXGBE_VFTA_SIZE];

	/* Info about the interface */
	u32			optics;
	u32			fc; /* local flow ctrl setting */
	int			advertise;  /* link speeds */
	bool			link_active;
	u16			max_frame_size;
	u16			num_segs;
	u32			link_speed;
	bool			link_up;
	u32 			linkvec;

	/* Mbuf cluster size */
	u32			rx_mbuf_sz;

	/* Support for pluggable optics */
	bool			sfp_probe;
	void			*link_si;  /* Link tasklet */
	void			*mod_si;   /* SFP tasklet */
	void			*msf_si;   /* Multispeed Fiber */
#ifdef IXGBE_FDIR
	int			fdir_reinit;
	void			*fdir_si;
#endif

	/*
	** Queues: 
	**   This is the irq holder, it has
	**   and RX/TX pair or rings associated
	**   with it.
	*/
	struct ix_queue		*queues;

	/*
	 * Transmit rings:
	 *	Allocated at run time, an array of rings.
	 */
	struct tx_ring		*tx_rings;
	u32			num_tx_desc;

	/*
	 * Receive rings:
	 *	Allocated at run time, an array of rings.
	 */
	struct rx_ring		*rx_rings;
	u64			que_mask;
	u32			num_rx_desc;

	/* Multicast array memory */
	u8			*mta;


	/* Misc stats maintained by the driver */
	struct evcnt   		dropped_pkts;
	struct evcnt   		mbuf_defrag_failed;
	struct evcnt	   	mbuf_header_failed;
	struct evcnt	   	mbuf_packet_failed;
	struct evcnt	   	efbig_tx_dma_setup;
	struct evcnt	   	efbig2_tx_dma_setup;
	struct evcnt	   	m_defrag_failed;
	struct evcnt	   	einval_tx_dma_setup;
	struct evcnt	   	other_tx_dma_setup;
	struct evcnt	   	eagain_tx_dma_setup;
	struct evcnt	   	enomem_tx_dma_setup;
	struct evcnt	   	watchdog_events;
	struct evcnt	   	tso_err;
	struct evcnt		link_irq;
	struct evcnt		morerx;
	struct evcnt		moretx;
	struct evcnt		txloops;
	struct evcnt		handleq;
	struct evcnt		req;

	struct ixgbe_hw_stats 	stats;
	struct sysctllog	*sysctllog;
	ixgbe_extmem_head_t jcl_head;
};


/* Precision Time Sync (IEEE 1588) defines */
#define ETHERTYPE_IEEE1588      0x88F7
#define PICOSECS_PER_TICK       20833
#define TSYNC_UDP_PORT          319 /* UDP port for the protocol */
#define IXGBE_ADVTXD_TSTAMP	0x00080000


#define IXGBE_CORE_LOCK_INIT(_sc, _name) \
        mutex_init(&(_sc)->core_mtx, MUTEX_DEFAULT, IPL_SOFTNET)
#define IXGBE_CORE_LOCK_DESTROY(_sc)      mutex_destroy(&(_sc)->core_mtx)
#define IXGBE_TX_LOCK_DESTROY(_sc)        mutex_destroy(&(_sc)->tx_mtx)
#define IXGBE_RX_LOCK_DESTROY(_sc)        mutex_destroy(&(_sc)->rx_mtx)
#define IXGBE_CORE_LOCK(_sc)              mutex_enter(&(_sc)->core_mtx)
#define IXGBE_TX_LOCK(_sc)                mutex_enter(&(_sc)->tx_mtx)
#define IXGBE_TX_TRYLOCK(_sc)             mutex_tryenter(&(_sc)->tx_mtx)
#define IXGBE_RX_LOCK(_sc)                mutex_enter(&(_sc)->rx_mtx)
#define IXGBE_CORE_UNLOCK(_sc)            mutex_exit(&(_sc)->core_mtx)
#define IXGBE_TX_UNLOCK(_sc)              mutex_exit(&(_sc)->tx_mtx)
#define IXGBE_RX_UNLOCK(_sc)              mutex_exit(&(_sc)->rx_mtx)
#define IXGBE_CORE_LOCK_ASSERT(_sc)       KASSERT(mutex_owned(&(_sc)->core_mtx))
#define IXGBE_TX_LOCK_ASSERT(_sc)         KASSERT(mutex_owned(&(_sc)->tx_mtx))

static inline bool
ixgbe_is_sfp(struct ixgbe_hw *hw)
{
	switch (hw->phy.type) {
	case ixgbe_phy_sfp_avago:
	case ixgbe_phy_sfp_ftl:
	case ixgbe_phy_sfp_intel:
	case ixgbe_phy_sfp_unknown:
	case ixgbe_phy_sfp_passive_tyco:
	case ixgbe_phy_sfp_passive_unknown:
		return TRUE;
	default:
		return FALSE;
	}
}

/* Workaround to make 8.0 buildable */
#if __FreeBSD_version >= 800000 && __FreeBSD_version < 800504
static __inline int
drbr_needs_enqueue(struct ifnet *ifp, struct buf_ring *br)
{
#ifdef ALTQ
        if (ALTQ_IS_ENABLED(&ifp->if_snd))
                return (1);
#endif
        return (!buf_ring_empty(br));
}
#endif

/*
** Find the number of unrefreshed RX descriptors
*/
static inline u16
ixgbe_rx_unrefreshed(struct rx_ring *rxr)
{       
	if (rxr->next_to_check > rxr->next_to_refresh)
		return (rxr->next_to_check - rxr->next_to_refresh - 1);
	else
		return ((rxr->num_desc + rxr->next_to_check) -
		    rxr->next_to_refresh - 1);
}       

#endif /* _IXGBE_H_ */

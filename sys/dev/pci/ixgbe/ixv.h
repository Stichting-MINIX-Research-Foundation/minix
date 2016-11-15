/******************************************************************************

  Copyright (c) 2001-2012, Intel Corporation 
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
/*$FreeBSD: head/sys/dev/ixgbe/ixv.h 257176 2013-10-26 17:58:36Z glebius $*/
/*$NetBSD: ixv.h,v 1.7 2015/08/17 06:16:03 knakahara Exp $*/


#ifndef _IXV_H_
#define _IXV_H_


#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
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
#include <sys/interrupt.h>

#include "ixgbe_netbsd.h"
#include "ixgbe_api.h"
#include "ixgbe_vf.h"

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
#define IXV_WATCHDOG                   (10 * hz)

/*
 * This parameters control when the driver calls the routine to reclaim
 * transmit descriptors.
 */
#define IXV_TX_CLEANUP_THRESHOLD	(adapter->num_tx_desc / 8)
#define IXV_TX_OP_THRESHOLD		(adapter->num_tx_desc / 32)

#define IXV_MAX_FRAME_SIZE	0x3F00

/* Flow control constants */
#define IXV_FC_PAUSE		0xFFFF
#define IXV_FC_HI		0x20000
#define IXV_FC_LO		0x10000

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
#define IXV_EITR_DEFAULT		128
#define IXV_SCATTER			32
#define IXV_RX_HDR			128
#define MSIX_BAR			3
#define IXV_TSO_SIZE			65535
#define IXV_BR_SIZE			4096
#define IXV_LINK_ITR			2000
#define TX_BUFFER_SIZE		((u32) 1514)
#define VFTA_SIZE			128

/* Offload bits in mbuf flag */
#define	M_CSUM_OFFLOAD	\
    (M_CSUM_IPv4|M_CSUM_UDPv4|M_CSUM_TCPv4|M_CSUM_UDPv6|M_CSUM_TCPv6)

/*
 *****************************************************************************
 * vendor_info_array
 * 
 * This array contains the list of Subvendor/Subdevice IDs on which the driver
 * should load.
 * 
 *****************************************************************************
 */
typedef struct _ixv_vendor_info_t {
	unsigned int    vendor_id;
	unsigned int    device_id;
	unsigned int    subvendor_id;
	unsigned int    subdevice_id;
	unsigned int    index;
} ixv_vendor_info_t;


struct ixv_tx_buf {
	u32		eop_index;
	struct mbuf	*m_head;
	bus_dmamap_t	map;
};

struct ixv_rx_buf {
	struct mbuf	*m_head;
	struct mbuf	*m_pack;
	struct mbuf	*fmp;
	bus_dmamap_t	hmap;
	bus_dmamap_t	pmap;
};

/*
 * Bus dma allocation structure used by ixv_dma_malloc and ixv_dma_free.
 */
struct ixv_dma_alloc {
	bus_addr_t		dma_paddr;
	void			*dma_vaddr;
	ixgbe_dma_tag_t		*dma_tag; /* XXX s/ixgbe/ixv/ --msaitoh */
	bus_dmamap_t		dma_map;
	bus_dma_segment_t	dma_seg;
	bus_size_t		dma_size;
	int			dma_nseg;
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
	u32			eitr;		/* cached reg */
	struct resource		*res;
	void			*tag;
	struct tx_ring		*txr;
	struct rx_ring		*rxr;
	void			*que_si;
	u64			irqs;
};

/*
 * The transmit ring, one per queue
 */
struct tx_ring {
        struct adapter		*adapter;
	kmutex_t		tx_mtx;
	u32			me;
	bool			watchdog_check;
	struct timeval		watchdog_time;
	union ixgbe_adv_tx_desc	*tx_base;
	struct ixv_dma_alloc	txdma;
	u32			next_avail_desc;
	u32			next_to_clean;
	struct ixv_tx_buf	*tx_buffers;
	volatile u16		tx_avail;
	u32			txd_cmd;
	ixgbe_dma_tag_t		*txtag;
	char			mtx_name[16];
	struct buf_ring		*br;
	/* Soft Stats */
	u32			bytes;
	u32			packets;
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
	struct ixv_dma_alloc	rxdma;
#ifdef LRO
	struct lro_ctrl		lro;
#endif /* LRO */
	bool			lro_enabled;
	bool			hdr_split;
	bool			discard;
        u32			next_to_refresh;
        u32 			next_to_check;
	char			mtx_name[16];
	struct ixv_rx_buf	*rx_buffers;
	ixgbe_dma_tag_t		*htag;
	ixgbe_dma_tag_t		*ptag;

	u32			bytes; /* Used for AIM calc */
	u32			packets;

	/* Soft stats */
	struct evcnt		rx_irq;
	struct evcnt		rx_split_packets;
	struct evcnt		rx_packets;
	struct evcnt		rx_bytes;
	struct evcnt		rx_discarded;
	struct evcnt 		no_jmbuf;
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
	struct callout		timer;
	int			msix;
	int			if_flags;

	kmutex_t		core_mtx;

#if 0
	eventhandler_tag 	vlan_attach;
	eventhandler_tag 	vlan_detach;
#endif

	u16			num_vlans;
	u16			num_queues;

	/* Info about the board itself */
	bool			link_active;
	u16			max_frame_size;
	u32			link_speed;
	bool			link_up;
	u32 			mbxvec;

	/* Mbuf cluster size */
	u32			rx_mbuf_sz;

	/* Support for pluggable optics */
	void     		*mbx_si;  /* Mailbox tasklet */

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
	int			num_tx_desc;

	/*
	 * Receive rings:
	 *	Allocated at run time, an array of rings.
	 */
	struct rx_ring		*rx_rings;
	int			num_rx_desc;
	u64			que_mask;
	u32			rx_process_limit;

	/* Misc stats maintained by the driver */
	struct evcnt	   	dropped_pkts;
	struct evcnt   		mbuf_defrag_failed;
	struct evcnt   		mbuf_header_failed;
	struct evcnt   		mbuf_packet_failed;
	struct evcnt   		no_tx_map_avail;
	struct evcnt   		no_tx_dma_setup;

	struct evcnt	   	efbig_tx_dma_setup;
	struct evcnt	   	efbig2_tx_dma_setup;
	struct evcnt	   	m_defrag_failed;
	struct evcnt	   	einval_tx_dma_setup;
	struct evcnt	   	other_tx_dma_setup;
	struct evcnt	   	eagain_tx_dma_setup;
	struct evcnt	   	enomem_tx_dma_setup;
	struct evcnt   		watchdog_events;
	struct evcnt	   	tso_err;
	struct evcnt   		tso_tx;
	struct evcnt		mbx_irq;
	struct evcnt		req;

	struct ixgbevf_hw_stats	stats;
	struct sysctllog	*sysctllog;
	ixgbe_extmem_head_t jcl_head;
};


#define IXV_CORE_LOCK_INIT(_sc, _name) \
        mutex_init(&(_sc)->core_mtx, MUTEX_DEFAULT, IPL_SOFTNET)
#define IXV_CORE_LOCK_DESTROY(_sc)      mutex_destroy(&(_sc)->core_mtx)
#define IXV_TX_LOCK_DESTROY(_sc)        mutex_destroy(&(_sc)->tx_mtx)
#define IXV_RX_LOCK_DESTROY(_sc)        mutex_destroy(&(_sc)->rx_mtx)
#define IXV_CORE_LOCK(_sc)              mutex_enter(&(_sc)->core_mtx)
#define IXV_TX_LOCK(_sc)                mutex_enter(&(_sc)->tx_mtx)
#define IXV_TX_TRYLOCK(_sc)             mutex_tryenter(&(_sc)->tx_mtx)
#define IXV_RX_LOCK(_sc)                mutex_enter(&(_sc)->rx_mtx)
#define IXV_CORE_UNLOCK(_sc)            mutex_exit(&(_sc)->core_mtx)
#define IXV_TX_UNLOCK(_sc)              mutex_exit(&(_sc)->tx_mtx)
#define IXV_RX_UNLOCK(_sc)              mutex_exit(&(_sc)->rx_mtx)
#define IXV_CORE_LOCK_ASSERT(_sc)       KASSERT(mutex_owned(&(_sc)->core_mtx))
#define IXV_TX_LOCK_ASSERT(_sc)         KASSERT(mutex_owned(&(_sc)->tx_mtx))

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
ixv_rx_unrefreshed(struct rx_ring *rxr)
{       
	struct adapter  *adapter = rxr->adapter;
        
	if (rxr->next_to_check > rxr->next_to_refresh)
		return (rxr->next_to_check - rxr->next_to_refresh - 1);
	else
		return ((adapter->num_rx_desc + rxr->next_to_check) -
		    rxr->next_to_refresh - 1);
}       

#endif /* _IXV_H_ */

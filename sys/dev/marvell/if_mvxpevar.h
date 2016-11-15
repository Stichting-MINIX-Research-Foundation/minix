/*	$NetBSD: if_mvxpevar.h,v 1.2 2015/06/03 03:55:47 hsuenaga Exp $	*/
/*
 * Copyright (c) 2015 Internet Initiative Japan Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _IF_MVXPEVAR_H_
#define _IF_MVXPEVAR_H_
#include <net/if.h>
#include <dev/marvell/mvxpbmvar.h>

/*
 * Limit of packet sizes.
 */
#define MVXPE_HWHEADER_SIZE	2		/* Marvell Header */
#define MVXPE_MRU		2000		/* Max Receive Unit */
#define MVXPE_MTU		MVXPE_MRU	/* Max Transmit Unit */

/*
 * Default limit of queue length
 *
 * queue 0 is lowest priority and queue 7 is highest priority.
 * IP packet is received on queue 7 by default.
 *
 * XXX: packet classifier is not implement yet
 */
#define MVXPE_RX_QUEUE_LIMIT_0	8
#define MVXPE_RX_QUEUE_LIMIT_1	8
#define MVXPE_RX_QUEUE_LIMIT_2	8
#define MVXPE_RX_QUEUE_LIMIT_3	8
#define MVXPE_RX_QUEUE_LIMIT_4	8
#define MVXPE_RX_QUEUE_LIMIT_5	8
#define MVXPE_RX_QUEUE_LIMIT_6	8
#define MVXPE_RX_QUEUE_LIMIT_7	IFQ_MAXLEN

#define MVXPE_TX_QUEUE_LIMIT_0	IFQ_MAXLEN
#define MVXPE_TX_QUEUE_LIMIT_1	8
#define MVXPE_TX_QUEUE_LIMIT_2	8
#define MVXPE_TX_QUEUE_LIMIT_3	8
#define MVXPE_TX_QUEUE_LIMIT_4	8
#define MVXPE_TX_QUEUE_LIMIT_5	8
#define MVXPE_TX_QUEUE_LIMIT_6	8
#define MVXPE_TX_QUEUE_LIMIT_7	8

/* interrupt is triggered when corossing (queuelen / RATIO) */
#define MVXPE_RXTH_RATIO	8
#define MVXPE_RXTH_REFILL_RATIO	2
#define MVXPE_TXTH_RATIO	8

/*
 * Device Register access
 */
#define MVXPE_READ(sc, reg) \
	bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg))
#define MVXPE_WRITE(sc, reg, val) \
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))

#define MVXPE_READ_REGION(sc, reg, val, c) \
	bus_space_read_region_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val), (c))
#define MVXPE_WRITE_REGION(sc, reg, val, c) \
	bus_space_write_region_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val), (c))

#define MVXPE_READ_MIB(sc, reg) \
	bus_space_read_4((sc)->sc_iot, (sc)->sc_mibh, (reg))

#define MVXPE_IS_LINKUP(sc) \
	(MVXPE_READ((sc), MVXPE_PSR) & MVXPE_PSR_LINKUP)

#define MVXPE_IS_QUEUE_BUSY(queues, q) \
	((((queues) >> (q)) & 0x1))

/*
 * EEE: Lower Power Idle config
 * Default timer is duration of MTU sized frame transmission.
 * The timer can be negotiated by LLDP protocol, but we have no
 * support.
 */
#define MVXPE_LPI_TS		(MVXPE_MRU * 8 / 1000) /* [us] */
#define MVXPE_LPI_TW		(MVXPE_MRU * 8 / 1000) /* [us] */
#define MVXPE_LPI_LI		(MVXPE_MRU * 8 / 1000) /* [us] */

/*
 * DMA Descriptor
 *
 * the ethernet device has 8 rx/tx DMA queues. each of queue has its own
 * decriptor list. descriptors are simply index by counter inside the device.
 */
#define MVXPE_TX_RING_CNT	IFQ_MAXLEN
#define MVXPE_TX_RING_MSK	(MVXPE_TX_RING_CNT - 1)
#define MVXPE_TX_RING_NEXT(x)	(((x) + 1) & MVXPE_TX_RING_MSK)
#define MVXPE_RX_RING_CNT	IFQ_MAXLEN
#define MVXPE_RX_RING_MSK	(MVXPE_RX_RING_CNT - 1)
#define MVXPE_RX_RING_NEXT(x)	(((x) + 1) & MVXPE_RX_RING_MSK)
#define MVXPE_TX_SEGLIMIT	32

struct mvxpe_rx_ring {
	/* Real descriptors array. shared by RxDMA */
	struct mvxpe_rx_desc		*rx_descriptors;
	bus_dmamap_t			rx_descriptors_map;

	/* Managment entries for each of descritors */
	struct mvxpe_rx_handle {
		struct mvxpe_rx_desc	*rxdesc_va;
		off_t			rxdesc_off; /* from rx_descriptors[0] */
		struct mvxpbm_chunk	*chunk;
	} rx_handle[MVXPE_RX_RING_CNT];

	/* locks */
	kmutex_t			rx_ring_mtx;

	/* Index */
	int				rx_dma;
	int				rx_cpu;

	/* Limit */
	int				rx_queue_len;
	int				rx_queue_th_received;
	int				rx_queue_th_free;
	int				rx_queue_th_time; /* [Tclk] */
};

struct mvxpe_tx_ring {
	/* Real descriptors array. shared by TxDMA */
	struct mvxpe_tx_desc		*tx_descriptors;
	bus_dmamap_t			tx_descriptors_map;

	/* Managment entries for each of descritors */
	struct mvxpe_tx_handle {
		struct mvxpe_tx_desc	*txdesc_va;
		off_t			txdesc_off; /* from tx_descriptors[0] */
		struct mbuf		*txdesc_mbuf;
		bus_dmamap_t		txdesc_mbuf_map;
	} tx_handle[MVXPE_TX_RING_CNT];

	/* locks */
	kmutex_t			tx_ring_mtx;

	/* Index */
	int				tx_used;
	int				tx_dma;
	int				tx_cpu;

	/* Limit */
	int				tx_queue_len;
	int				tx_queue_th_free;
};

static inline int
tx_counter_adv(int ctr, int n)
{
	/* XXX: lock or atomic */
	ctr += n;
	while (ctr >= MVXPE_TX_RING_CNT)
		ctr -= MVXPE_TX_RING_CNT;

	return ctr;
}

static inline int
rx_counter_adv(int ctr, int n)
{
	/* XXX: lock or atomic */
	ctr += n;
	while (ctr >= MVXPE_TX_RING_CNT)
		ctr -= MVXPE_TX_RING_CNT;

	return ctr;
}

/*
 * Timeout control
 */
#define MVXPE_PHY_TIMEOUT	10000	/* msec */
#define RX_DISABLE_TIMEOUT	0x1000000 /* times */
#define TX_DISABLE_TIMEOUT	0x1000000 /* times */
#define TX_FIFO_EMPTY_TIMEOUT	0x1000000 /* times */

/*
 * Event counter
 */
#ifdef MVXPE_EVENT_COUNTERS
#define	MVXPE_EVCNT_INCR(ev)		(ev)->ev_count++
#define	MVXPE_EVCNT_ADD(ev, val)	(ev)->ev_count += (val)
#else
#define	MVXPE_EVCNT_INCR(ev)		/* nothing */
#define	MVXPE_EVCNT_ADD(ev, val)	/* nothing */
#endif
struct mvxpe_evcnt {
	/*
	 * Master Interrupt Handler
	 */
	struct evcnt ev_i_rxtxth;
	struct evcnt ev_i_rxtx;
	struct evcnt ev_i_misc;

	/*
	 * RXTXTH Interrupt
	 */
	struct evcnt ev_rxtxth_txerr;

	/*
	 * MISC Interrupt
	 */
	struct evcnt ev_misc_phystatuschng;
	struct evcnt ev_misc_linkchange;
	struct evcnt ev_misc_iae;
	struct evcnt ev_misc_rxoverrun;
	struct evcnt ev_misc_rxcrc;
	struct evcnt ev_misc_rxlargepacket;
	struct evcnt ev_misc_txunderrun;
	struct evcnt ev_misc_prbserr;
	struct evcnt ev_misc_srse;
	struct evcnt ev_misc_txreq;

	/*
	 * RxTx Interrupt
	 */
	struct evcnt ev_rxtx_rreq;
	struct evcnt ev_rxtx_rpq;
	struct evcnt ev_rxtx_tbrq;
	struct evcnt ev_rxtx_rxtxth;
	struct evcnt ev_rxtx_txerr;
	struct evcnt ev_rxtx_misc;

	/*
	 * Link
	 */
	struct evcnt ev_link_up;
	struct evcnt ev_link_down;

	/*
	 * Rx Descriptor
	 */
	struct evcnt ev_rxd_ce;
	struct evcnt ev_rxd_or;
	struct evcnt ev_rxd_mf;
	struct evcnt ev_rxd_re;
	struct evcnt ev_rxd_scat;

	/*
	 * Tx Descriptor
	 */
	struct evcnt ev_txd_lc;
	struct evcnt ev_txd_ur;
	struct evcnt ev_txd_rl;
	struct evcnt ev_txd_oth;

	/*
	 * Status Registers
	 */
	struct evcnt ev_reg_pdfc;	/* Rx Port Discard Frame Counter */
	struct evcnt ev_reg_pofc;	/* Rx Port Overrun Frame Counter */
	struct evcnt ev_reg_txbadfcs;	/* Tx BAD FCS Counter */
	struct evcnt ev_reg_txdropped;	/* Tx Dropped Counter */
	struct evcnt ev_reg_lpic;


	/* Device Driver Errors */
	struct evcnt ev_drv_wdogsoft;
	struct evcnt ev_drv_txerr;
	struct evcnt ev_drv_rxq[MVXPE_QUEUE_SIZE];
	struct evcnt ev_drv_rxqe[MVXPE_QUEUE_SIZE];
	struct evcnt ev_drv_txq[MVXPE_QUEUE_SIZE];
	struct evcnt ev_drv_txqe[MVXPE_QUEUE_SIZE];
};

/*
 * Debug
 */
#ifdef MVXPE_DEBUG
#define DPRINTF(fmt, ...) \
	do { \
		if (mvxpe_debug >= 1) { \
			printf("%s: ", __func__); \
			printf((fmt), ##__VA_ARGS__); \
		} \
	} while (/*CONSTCOND*/0)
#define DPRINTFN(level , fmt, ...) \
	do { \
		if (mvxpe_debug >= (level)) { \
			printf("%s: ", __func__); \
			printf((fmt), ##__VA_ARGS__); \
		} \
	} while (/*CONSTCOND*/0)
#define DPRINTDEV(dev, level, fmt, ...) \
	do { \
		if (mvxpe_debug >= (level)) { \
			device_printf((dev), \
			    "%s: "fmt , __func__, ##__VA_ARGS__); \
		} \
	} while (/*CONSTCOND*/0)
#define DPRINTSC(sc, level, fmt, ...) \
	do { \
		device_t dev = (sc)->sc_dev; \
		if (mvxpe_debug >= (level)) { \
			device_printf(dev, \
			    "%s: " fmt, __func__, ##__VA_ARGS__); \
		} \
	} while (/*CONSTCOND*/0)
#define DPRINTIFNET(ifp, level, fmt, ...) \
	do { \
		const char *xname = (ifp)->if_xname; \
		if (mvxpe_debug >= (level)) { \
			printf("%s: %s: " fmt, xname, __func__, ##__VA_ARGS__);\
		} \
	} while (/*CONSTCOND*/0)
#define DPRINTIFNET(ifp, level, fmt, ...) \
	do { \
		const char *xname = (ifp)->if_xname; \
		if (mvxpe_debug >= (level)) { \
			printf("%s: %s: " fmt, xname, __func__, ##__VA_ARGS__);\
		} \
	} while (/*CONSTCOND*/0)
#define DPRINTPRXS(level, q) \
	do { \
		uint32_t _reg = MVXPE_READ(sc, MVXPE_PRXS(q)); \
		if (mvxpe_debug >= (level)) { \
		   printf("PRXS(queue %d) %#x: Occupied %d, NoOccupied %d.\n", \
		    q, _reg, MVXPE_PRXS_GET_ODC(_reg), \
		    MVXPE_PRXS_GET_NODC(_reg)); \
		} \
	} while (/*CONSTCOND*/0)
#else
#define DPRINTF(fmt, ...)
#define DPRINTFN(level, fmt, ...)
#define DPRINTDEV(dev, level, fmt, ...)
#define DPRINTSC(sc, level, fmt, ...)
#define DPRINTIFNET(ifp, level, fmt, ...)
#define DPRINTPRXS(level, reg)
#endif

#define KASSERT_SC_MTX(sc) \
    KASSERT(mutex_owned(&(sc)->sc_mtx))
#define KASSERT_BM_MTX(sc) \
    KASSERT(mutex_owned(&(sc)->sc_bm.bm_mtx))
#define KASSERT_RX_MTX(sc, q) \
    KASSERT(mutex_owned(&(sc)->sc_rx_ring[(q)].rx_ring_mtx))
#define KASSERT_TX_MTX(sc, q) \
    KASSERT(mutex_owned(&(sc)->sc_tx_ring[(q)].tx_ring_mtx))

/*
 * Configuration parameters
 */
struct mvxpe_conf {
	int cf_lpi;		/* EEE Low Power IDLE enable */
	int cf_fc;		/* Flow Control enable */
};

/*
 * sysctl(9) parameters
 */
struct mvxpe_softc;
struct mvxpe_sysctl_queue {
	struct mvxpe_softc	*sc;
	int			rxtx;
	int			queue;
};
#define MVXPE_SYSCTL_RX		0
#define MVXPE_SYSCTL_TX		1

struct mvxpe_sysctl_mib {
	struct mvxpe_softc	*sc;
	int			index;
	uint64_t		counter;
};

/*
 * Ethernet Device main context
 */
struct mvxpe_softc {
	device_t sc_dev;
	int sc_port;
	uint32_t sc_version;

	/*
	 * sc_mtx must be held by interface functions to/from
	 * other frameworks. interrupt hander, sysctl hander,
	 * ioctl hander, and so on.
	 */
	kmutex_t sc_mtx;

	/*
	 * Ethernet facilities
	 */
	struct ethercom sc_ethercom;
	struct mii_data sc_mii;
	u_int8_t sc_enaddr[ETHER_ADDR_LEN];	/* station addr */
	int sc_if_flags;
	int sc_wdogsoft;

	/*
	 * Configuration Parameters
	 */
	struct mvxpe_conf sc_cf;

	/*
	 * I/O Spaces
	 */
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;	/* all registers handle */
	bus_space_handle_t sc_mibh;	/* mib counter handle */

	/*
	 * DMA Spaces 
	 */
	bus_dma_tag_t sc_dmat;
	struct mvxpe_rx_ring		sc_rx_ring[MVXPE_QUEUE_SIZE];
	struct mvxpe_tx_ring		sc_tx_ring[MVXPE_QUEUE_SIZE];
	int sc_tx_pending;		/* total number of tx pkt */

	/*
	 * Software Buffer Manager
	 */
	struct mvxpbm_softc *sc_bm;

	/*
	 * Maintance clock
	 */
	callout_t sc_tick_ch;		/* tick callout */

	/*
	 * Link State control
	 */
	uint32_t sc_linkstate;

	/*
	 * Act as Rndom source
	 */
	krndsource_t sc_rnd_source;

	/*
	 * Sysctl interfaces
	 */
	struct sysctllog *sc_mvxpe_clog;
	struct mvxpe_sysctl_queue sc_sysctl_rx_queue[MVXPE_QUEUE_SIZE];
	struct mvxpe_sysctl_queue sc_sysctl_tx_queue[MVXPE_QUEUE_SIZE];

	/*
	 * MIB counter
	 */
	size_t sc_sysctl_mib_size;
	struct mvxpe_sysctl_mib *sc_sysctl_mib;

#ifdef MVXPE_EVENT_COUNTERS
	/*
	 * Event counter
	 */
	struct mvxpe_evcnt sc_ev;
#endif
};
#define MVXPE_RX_RING_MEM_VA(sc, q) \
    ((sc)->sc_rx_ring[(q)].rx_descriptors)
#define MVXPE_RX_RING_MEM_PA(sc, q) \
    ((sc)->sc_rx_ring[(q)].rx_descriptors_map->dm_segs[0].ds_addr)
#define MVXPE_RX_RING_MEM_MAP(sc, q) \
    ((sc)->sc_rx_ring[(q)].rx_descriptors_map)
#define MVXPE_RX_RING(sc, q) \
    (&(sc)->sc_rx_ring[(q)])
#define MVXPE_RX_HANDLE(sc, q, i) \
    (&(sc)->sc_rx_ring[(q)].rx_handle[(i)])
#define MVXPE_RX_DESC(sc, q, i) \
    ((sc)->sc_rx_ring[(q)].rx_handle[(i)].rxdesc_va)
#define MVXPE_RX_DESC_OFF(sc, q, i) \
    ((sc)->sc_rx_ring[(q)].rx_handle[(i)].rxdesc_off)
#define MVXPE_RX_PKTBUF(sc, q, i) \
    ((sc)->sc_rx_ring[(q)].rx_handle[(i)].chunk)

#define MVXPE_TX_RING_MEM_VA(sc, q) \
    ((sc)->sc_tx_ring[(q)].tx_descriptors)
#define MVXPE_TX_RING_MEM_PA(sc, q) \
    ((sc)->sc_tx_ring[(q)].tx_descriptors_map->dm_segs[0].ds_addr)
#define MVXPE_TX_RING_MEM_MAP(sc, q) \
    ((sc)->sc_tx_ring[(q)].tx_descriptors_map)
#define MVXPE_TX_RING(sc, q) \
    (&(sc)->sc_tx_ring[(q)])
#define MVXPE_TX_HANDLE(sc, q, i) \
    (&(sc)->sc_tx_ring[(q)].tx_handle[(i)])
#define MVXPE_TX_DESC(sc, q, i) \
    ((sc)->sc_tx_ring[(q)].tx_handle[(i)].txdesc_va)
#define MVXPE_TX_DESC_OFF(sc, q, i) \
    ((sc)->sc_tx_ring[(q)].tx_handle[(i)].txdesc_off)
#define MVXPE_TX_MBUF(sc, q, i) \
    ((sc)->sc_tx_ring[(q)].tx_handle[(i)].txdesc_mbuf)
#define MVXPE_TX_MAP(sc, q, i) \
    ((sc)->sc_tx_ring[(q)].tx_handle[(i)].txdesc_mbuf_map)

#endif /* _IF_MVXPEVAR_H_ */

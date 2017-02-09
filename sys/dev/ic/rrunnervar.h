/*	$NetBSD: rrunnervar.h,v 1.14 2012/10/27 17:18:22 chs Exp $	*/

/* Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code contributed to The NetBSD Foundation by Kevin M. Lahey
 * of the Numerical Aerospace Simulation Facility, NASA Ames Research
 * Center.
 *
 * Partially based on a HIPPI driver written by Essential Communications
 * Corporation.  Thanks to Jason Thorpe, Matt Jacob, and Fred Templin
 * for invaluable advice and encouragement!
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

/* RoadRunner software status per interface */

struct rr_tuning {

	/* Performance tuning registers: */

	u_int32_t		rt_mode_and_status;
	u_int32_t		rt_conn_retry_count;
	u_int32_t		rt_conn_retry_timer;
	u_int32_t		rt_conn_timeout;
	u_int32_t		rt_stats_timer;
	u_int32_t		rt_interrupt_timer;
	u_int32_t		rt_tx_timeout;
	u_int32_t		rt_rx_timeout;

	/* DMA performance tuning registers: */

	u_int32_t		rt_pci_state;
	u_int32_t		rt_dma_write_state;
	u_int32_t		rt_dma_read_state;
	u_int32_t		rt_driver_param;
};



struct rr_eeprom {
	u_int32_t	ifr_offset;	/* initial offset in bytes */
	u_int32_t	ifr_length;	/* length in bytes to write */
	u_int32_t	*ifr_buffer;	/* data to be written */
};

#define EIOCGTUNE   1	/* retrieve tuning */
#define EIOCSTUNE   2	/* set tuning */
#define EIOCGEEPROM 3	/* get eeprom */
#define EIOCSEEPROM 4	/* set eeprom */
#define EIOCGSTATS  5	/* get statistics */
#define EIOCRESET   6   /* reset the card */

#ifdef _KERNEL

/* Per-ring information for the SNAP (network) receive ring */

struct esh_snap_ring_ctl {
	bus_dmamap_t ec_dma[RR_MAX_DESCR];
	struct mbuf *ec_m[RR_MAX_DESCR];
	struct mbuf *ec_cur_pkt;	/* current packet being processed */
	struct mbuf *ec_cur_mbuf;	/* current mbuf being processed */
	int ec_error;			/* encountered error? */
	u_int16_t ec_producer;		/* latest buffer driver produced */
	u_int16_t ec_consumer;		/* latest buffer runcode consumed */
	struct rr_descr *ec_descr;	/* array of descriptors for ring */
};

TAILQ_HEAD(esh_dmainfo_list, esh_dmainfo);

struct esh_dmainfo {
	u_int32_t ed_flags;
#define ESH_DI_BUSY	0x1
#define ESH_DI_READING	0x2
	bus_dmamap_t ed_dma;
	struct buf *ed_buf;
	int ed_read_len;
	int ed_error;
	TAILQ_ENTRY(esh_dmainfo) ed_list;
};

struct esh_send_ring_ctl {
	bus_dmamap_t ec_dma;		/* dmamap for data to transmit */
	int ec_offset;			/* offset in dmamap to send next */
	size_t ec_len;			/* total length of current buf */
	struct mbuf *ec_cur_mbuf;	/* current mbuf being processed */
	struct buf *ec_cur_buf;		/* current buf being processed */
	struct esh_dmainfo *ec_cur_dmainfo;
					/* current dmainfo being processed */
	struct bufq_state *ec_buf_queue;/* queue of bufs to send */
	int ec_error;			/* encountered error? */
	u_int16_t ec_producer;		/* latest buffer driver produced */
	u_int16_t ec_consumer;		/* latest buffer runcode consumed */
	struct rr_descr *ec_descr;	/* array of descriptors for ring */
	struct esh_dmainfo_list ec_di_queue;
};

struct esh_fp_ring_ctl {
	struct esh_dmainfo *ec_dmainfo[RR_MAX_DESCR];
	struct esh_dmainfo *ec_cur_dmainfo;
	int ec_offset;			/* offset of current buf */
	int ec_error;			/* encountered error? */
	int ec_seen_end;		/* seen the end of the buffer? */
	int ec_dmainfo_count;		/* dmainfo buffers in use count */
	u_int16_t ec_producer;		/* latest buffer driver produced */
	u_int16_t ec_consumer;		/* latest buffer runcode consumed */
	u_int32_t ec_read_len;		/* length of packet being read in */
	struct rr_descr *ec_descr;	/* array of descriptors for ring */
	struct esh_dmainfo_list ec_queue;
	u_int ec_ulp;			/* ULP for this ring */
	int ec_index;			/* index into list of active rings */
	bus_dmamap_t ec_dma;
	bus_dma_segment_t ec_dmaseg;
};


struct esh_softc {
	device_t		sc_dev;
	struct ifnet		sc_if;
	struct ifmedia		sc_media;

	volatile int		sc_flags;
#define ESH_FL_INITIALIZING	0x001
#define ESH_FL_INITIALIZED	0x002
#define ESH_FL_RUNCODE_UP	0x004
#define ESH_FL_LINK_UP		0x008
#define ESH_FL_SNAP_RING_UP	0x010
#define ESH_FL_FP_RING_UP	0x020
#define ESH_FL_EEPROM_BUSY	0x040
#define ESH_FL_FP_OPEN		0x080
#define ESH_FL_CRASHED		0x100
#define ESH_FL_CLOSING_SNAP	0x200

	void			*sc_ih;

	bus_space_tag_t		sc_iot;	     /* bus cookie      */
	bus_space_handle_t	sc_ioh;      /* bus i/o handle  */

	bus_dma_tag_t		sc_dmat;     /* dma tag */

	bus_dma_segment_t	sc_dmaseg;   /* segment holding the various
					        data structures in host memory
					        that are DMA'ed to the NIC */
	bus_dmamap_t		sc_dma;	     /* dma map for the segment */
	char 		        *sc_dma_addr; /* address in kernel of DMA mem */
	bus_size_t		sc_dma_size; /* size of dma-able region */

	u_int8_t	(*sc_bist_read)(struct esh_softc *);
	void		(*sc_bist_write)(struct esh_softc *, u_int8_t);

	/*
	 * Definitions for the various driver structures that sit in host
	 * memory and are read by the NIC via DMA:
	 */

	struct rr_gen_info	*sc_gen_info;	/* gen info block pointer */
	bus_addr_t		sc_gen_info_dma;

	struct rr_ring_ctl	*sc_recv_ring_table;
	bus_addr_t		sc_recv_ring_table_dma;

	struct rr_event		*sc_event_ring;
	bus_addr_t		sc_event_ring_dma;

	struct rr_descr		*sc_send_ring;
	struct rr2_descr	*sc2_send_ring;
	bus_addr_t		sc_send_ring_dma;

	struct rr_descr		*sc_snap_recv_ring;
	struct rr2_descr	*sc2_snap_recv_ring;
	bus_addr_t		sc_snap_recv_ring_dma;

	/*
	 * Control structures for the various rings that we definitely
	 * know we want to keep track of.
	 */

	struct esh_send_ring_ctl
				sc_send;
	struct esh_snap_ring_ctl
				sc_snap_recv;
	struct esh_fp_ring_ctl	*sc_fp_recv[RR_ULP_COUNT];
	struct esh_fp_ring_ctl	*sc_fp_recv_index[RR_MAX_RECV_RING];
	int			sc_event_consumer;
	int			sc_event_producer;
	int			sc_cmd_consumer;
	int			sc_cmd_producer;

	/*
	 * Various maintainance values we need
	 */

	int			sc_watchdog;

	/*
	 * Various hardware parameters we need to keep track of.
	 */

	u_int32_t		sc_sram_size;
	u_int32_t		sc_runcode_start;
	u_int32_t		sc_runcode_version;
	u_int32_t		sc_version; /* interface of runcode (1 or 2) */
	u_int16_t		sc_options; /* options in current RunCode */
	u_int			sc_max_rings;

	u_int32_t		sc_pci_latency;
	u_int32_t		sc_pci_lat_gnt;
	u_int32_t		sc_pci_cache_line;

	/* ULA assigned to hardware */

	u_int8_t		sc_ula[6];

	/* Tuning parameters */

	struct rr_tuning	sc_tune;

	/* Measure of how ugly this is. */

	u_int32_t		sc_misaligned_bufs;
	u_int32_t		sc_bad_lens;

	struct esh_dmainfo_list sc_dmainfo_freelist;
	u_int			sc_dmainfo_freelist_count;
	u_int			sc_fp_rings;
};

void	eshconfig(struct esh_softc *);
int	eshintr(void *);
#endif /* _KERNEL */

/* Define a few constants for future use */

#define ESH_MAX_NSEGS			512     /* room for 2MB of data */
#define ESH_STATS_TIMER_DEFAULT		1030900
	/* 1000000 usecs / 0.97 usecs/tick */

#define NEXT_EVENT(i)  (((i) + 1) & (RR_EVENT_RING_SIZE - 1))
#define NEXT_SEND(i)  (((i) + 1) & (RR_SEND_RING_SIZE - 1))
#define NEXT_RECV(i)  (((i) + 1) & (RR_SNAP_RECV_RING_SIZE - 1))

#define PREV_SEND(i)  (((i) + RR_SEND_RING_SIZE - 1) & (RR_SEND_RING_SIZE - 1))
#define PREV_RECV(i)  \
	(((i) + RR_SNAP_RECV_RING_SIZE - 1) & (RR_SNAP_RECV_RING_SIZE - 1))


/* $NetBSD: i82596var.h,v 1.15 2009/12/01 23:16:01 skrll Exp $ */

/*
 * Copyright (c) 2003 Jochen Kunz.
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
 * 3. The name of Jochen Kunz may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOCHEN KUNZ
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL JOCHEN KUNZ
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* All definitions are for a Intel 82596 DX/SX / CA in linear 32 bit mode. */



/* Supported chip variants */
extern const char *i82596_typenames[];
enum i82596_types { I82596_UNKNOWN, I82596_DX, I82596_CA };



/* System Configuration Pointer */
struct iee_scp {
	volatile uint16_t scp_pad1;
	volatile uint16_t scp_sysbus;		/* Sysbus Byte */
	volatile uint32_t scp_pad2;
	volatile uint32_t scp_iscp_addr;	/* Int. Sys. Conf. Pointer */
} __packed;



/* Intermediate System Configuration Pointer */
struct iee_iscp {
	volatile uint16_t iscp_busy;		/* Even Word, bits 0..15 */
	volatile uint16_t iscp_pad;		/* Odd Word, bits 16..32 */
	volatile uint32_t iscp_scb_addr;	/* address of SCB */
} __packed;



/* System Control Block */
struct iee_scb {
	volatile uint16_t scb_status;		/* Status Bits */
	volatile uint16_t scb_cmd;		/* Command Bits */
	volatile uint32_t scb_cmd_blk_addr;	/* Command Block Address */
	volatile uint32_t scb_rfa_addr;		/* Receive Frame Area Address */
	volatile uint32_t scb_crc_err;		/* CRC Errors */
	volatile uint32_t scb_align_err;	/* Alignment Errors */
	volatile uint32_t scb_resource_err;	/* Resource Errors [1] */
	volatile uint32_t scb_overrun_err;	/* Overrun Errors [1] */
	volatile uint32_t scb_rcvcdt_err;	/* RCVCDT Errors [1] */
	volatile uint32_t scb_short_fr_err;	/* Short Frame Errors */
	volatile uint16_t scb_tt_off;		/* Bus Throtle Off Timer */
	volatile uint16_t scb_tt_on;		/* Bus Throtle On Timer */
} __packed;
/* [1] In MONITOR mode these counters change function. */



/* Command Block */
struct iee_cb {
	volatile uint16_t cb_status;		/* Status Bits */
	volatile uint16_t cb_cmd;		/* Command Bits */
	volatile uint32_t cb_link_addr;		/* Link Address to next CMD */
	union {
		volatile uint8_t cb_ind_addr[8];/* Individual Address */
		volatile uint8_t cb_cf[16];	/* Configuration Bytes */
		struct {
			volatile uint16_t mc_size;/* Num bytes of Mcast Addr.*/
			volatile uint8_t mc_addrs[6]; /* List of Mcast Addr. */
		} cb_mcast;
		struct {
			volatile uint32_t tx_tbd_addr;/* TX Buf. Descr. Addr.*/
			volatile uint16_t tx_tcb_count; /* Len. of opt. data */
			volatile uint16_t tx_pad;
			volatile uint8_t tx_dest_addr[6]; /* Dest. Addr. */
			volatile uint16_t tx_length; /* Length of data */
			/* uint8_t data;	 Data to send, optional */
		} cb_transmit;
		volatile uint32_t cb_tdr;	/* Time & Flags from TDR CMD */
		volatile uint32_t cb_dump_addr;	/* Address of Dump buffer */
	};
} __packed;



/* Transmit Buffer Descriptor */
struct iee_tbd {
	volatile uint16_t tbd_size;		/* Size of buffer & Flags */
	volatile uint16_t tbd_pad;
	volatile uint32_t tbd_link_addr;	/* Link Address to next RFD */
	volatile uint32_t tbd_tb_addr;		/* Transmit Buffer Address */
} __packed;



/* Receive Frame Descriptor */
struct iee_rfd {
	volatile uint16_t rfd_status;		/* Status Bits */
	volatile uint16_t rfd_cmd;		/* Command Bits */
	volatile uint32_t rfd_link_addr;	/* Link Address to next RFD */
	volatile uint32_t rfd_rbd_addr;		/* Address of first free RBD */
	volatile uint16_t rfd_count;		/* Actual Count */
	volatile uint16_t rfd_size;		/* Size */
	volatile uint8_t rfd_dest_addr[6];	/* Destination Address */
	volatile uint8_t rfd_src_addr[6];	/* Source Address */
	volatile uint16_t rfd_length;		/* Length Field */
	volatile uint16_t rfd_pad;		/* Optional Data */
} __packed;



/* Receive Buffer Descriptor */
struct iee_rbd {
	volatile uint16_t rbd_count;		/* Actual Cont of bytes */
	volatile uint16_t rbd_pad1;
	volatile uint32_t rbd_next_rbd;		/* Address of Next RBD */
	volatile uint32_t rbd_rb_addr;		/* Receive Buffer Address */
	volatile uint16_t rbd_size;		/* Size of Receive Buffer */
	volatile uint16_t rbd_pad2;
} __packed;



#define IEE_NRFD	32	/* Number of RFDs == length of receive queue */
#define IEE_NCB		32	/* Number of Command Blocks == transmit queue */
#define IEE_NTBD	16	/* Number of TBDs per CB */



struct iee_softc {
	device_t sc_dev;		/* common device data */
	struct ifmedia sc_ifmedia;	/* media interface */
	struct ethercom sc_ethercom;	/* ethernet specific stuff */
	enum i82596_types sc_type;
	bus_dma_tag_t sc_dmat;
	bus_dmamap_t sc_shmem_map;
	bus_dma_segment_t sc_dma_segs;
	int sc_dma_rsegs;
	bus_dmamap_t sc_rx_map[IEE_NRFD];
	bus_dmamap_t sc_tx_map[IEE_NCB];
	struct mbuf *sc_rx_mbuf[IEE_NRFD];
	struct mbuf *sc_tx_mbuf[IEE_NCB];
	uint8_t *sc_shmem_addr;
	int sc_next_cb;
	int sc_next_tbd;
	int sc_rx_done;
	uint8_t sc_cf[14];
	int sc_flags;
	int sc_cl_align;
	int sc_scp_off;
	int sc_scp_sz;
	int sc_iscp_off;
	int sc_iscp_sz;
	int sc_scb_off;
	int sc_scb_sz;
	int sc_rfd_off;
	int sc_rfd_sz;
	int sc_rbd_off;
	int sc_rbd_sz;
	int sc_cb_off;
	int sc_cb_sz;
	int sc_tbd_off;
	int sc_tbd_sz;
	int sc_shmem_sz;
	uint32_t sc_sysbus;
	uint32_t sc_crc_err;
	uint32_t sc_align_err;
	uint32_t sc_resource_err;
	uint32_t sc_overrun_err;
	uint32_t sc_rcvcdt_err;
	uint32_t sc_short_fr_err;
	uint32_t sc_receive_err;
	uint32_t sc_tx_col;
	uint32_t sc_rx_err;
	uint32_t sc_cmd_err;
	uint32_t sc_tx_timeout;
	uint32_t sc_setup_timeout;
	int (*sc_iee_cmd)(struct iee_softc *, uint32_t);
	int (*sc_iee_reset)(struct iee_softc *);
	void (*sc_mediastatus)(struct ifnet *, struct ifmediareq *);
	int (*sc_mediachange)(struct ifnet *);
};



/* Flags */
#define IEE_NEED_SWAP	0x01
#define IEE_WANT_MCAST	0x02
#define IEE_REV_A	0x04

/*
 * Rev A1 chip doesn't have 32-bit BE mode and all 32 bit pointers are
 * treated as two 16-bit big endian entities.
 */
#define IEE_SWAPA32(x)	((sc->sc_flags & (IEE_NEED_SWAP|IEE_REV_A)) ==	\
			    (IEE_NEED_SWAP|IEE_REV_A) ?			\
			    (((x) << 16) | ((x) >> 16)) : (x))
/*
 * The SCB absolute address and statistical counters are
 * always treated as two 16-bit big endian entities
 * even in 32-bit BE mode supported by Rev B and C chips.
 */
#define IEE_SWAP32(x)	((sc->sc_flags & IEE_NEED_SWAP) != 0 ? 		\
			    (((x) << 16) | ((x) >> 16)) : (x))
#define IEE_PHYS_SHMEM(x) ((uint32_t) (sc->sc_shmem_map->dm_segs[0].ds_addr \
			+ (x)))

#define SC_SCP(sc)	((struct iee_scp *)((sc)->sc_shmem_addr +	\
			    (sc)->sc_scp_off))
#define SC_ISCP(sc)	((struct iee_iscp *)((sc)->sc_shmem_addr +	\
			    (sc)->sc_iscp_off))
#define SC_SCB(sc)	((struct iee_scb *)((sc)->sc_shmem_addr +	\
			    (sc)->sc_scb_off))
#define SC_RFD(sc, n)	((struct iee_rfd *)((sc)->sc_shmem_addr +	\
			    (sc)->sc_rfd_off +				\
			    (n) * (sc)->sc_rfd_sz))
#define SC_RBD(sc, n)	((struct iee_rbd *)((sc)->sc_shmem_addr +	\
			    (sc)->sc_rbd_off +				\
			    (n) * (sc)->sc_rbd_sz))
#define SC_CB(sc, n)	((struct iee_cb *)((sc)->sc_shmem_addr +	\
			    (sc)->sc_cb_off +				\
			    (n) * (sc)->sc_cb_sz))
#define SC_TBD(sc, n)	((struct iee_tbd *)((sc)->sc_shmem_addr +	\
			    (sc)->sc_tbd_off +				\
			    (n) * (sc)->sc_tbd_sz))

#define IEE_SCPSYNC(sc, ops)						\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_shmem_map,		\
	    (sc)->sc_scb_off, (sc)->sc_scp_sz, (ops))
#define IEE_ISCPSYNC(sc, ops)						\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_shmem_map,		\
	    (sc)->sc_iscp_off, (sc)->sc_iscp_sz, (ops))
#define IEE_SCBSYNC(sc, ops)						\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_shmem_map,		\
	    (sc)->sc_scb_off, (sc)->sc_scb_sz, (ops))
#define IEE_RFDSYNC(sc, n, ops)						\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_shmem_map,		\
	    (sc)->sc_rfd_off + (n) * (sc)->sc_rfd_sz,			\
	    (sc)->sc_rfd_sz, (ops))
#define IEE_RBDSYNC(sc, n, ops)						\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_shmem_map,		\
	    (sc)->sc_rbd_off + (n) * (sc)->sc_rbd_sz,			\
	    (sc)->sc_rbd_sz, (ops))
#define IEE_CBSYNC(sc, n, ops)						\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_shmem_map,		\
	    (sc)->sc_cb_off + (n) * (sc)->sc_cb_sz,			\
	    (sc)->sc_cb_sz, (ops))
#define IEE_TBDSYNC(sc, n, ops)						\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_shmem_map,		\
	    (sc)->sc_tbd_off + (n) * (sc)->sc_tbd_sz,			\
	    (sc)->sc_tbd_sz, (ops))

void iee_attach(struct iee_softc *, uint8_t *, int *, int, int);
void iee_detach(struct iee_softc *, int);
int iee_intr(void *);





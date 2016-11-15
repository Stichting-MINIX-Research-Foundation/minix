/*	$NetBSD: mvxpsecvar.h,v 1.1 2015/06/03 04:20:02 hsuenaga Exp $	*/
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

/*
 * Cryptographic Engine and Security Accelerator(CESA)
 */
#ifndef __MVXPSECVAR_H__
#define __MVXPSECVAR_H__
#include <sys/device.h>
#include <dev/marvell/mvxpsecreg.h>

/*
 * Compile time options
 */
/* use multi-packet chained mode */
#define MVXPSEC_MULTI_PACKET
#define MVXPSEC_EVENT_COUNTERS

/*
 * Memory management
 */
struct mvxpsec_devmem {
	bus_dmamap_t map;
	void *kva;
	int size;
};
#define dm_paddr dm_segs[0].ds_addr
#define devmem_va(x) ((x)->kva)
#define devmem_nseg(x) ((x)->map->dm_nsegs)
#define devmem_pa(x, s) ((x)->map->dm_segs[(s)].ds_addr)
#define devmem_palen(x, s) ((x)->map->dm_segs[(s)].ds_len)
#define devmem_size(x) ((x)->size)
#define devmem_map(x) ((x)->map)

/*
 * DMA Descriptors
 */
struct mvxpsec_descriptor {
	uint32_t tdma_word0;
	uint32_t tdma_src;
	uint32_t tdma_dst;
	uint32_t tdma_nxt;
} __attribute__((__packed__));

struct mvxpsec_descriptor_handle {
	bus_dmamap_t map;
	paddr_t phys_addr;
	int off;

	void *_desc;

	SIMPLEQ_ENTRY(mvxpsec_descriptor_handle) chain;
};
SIMPLEQ_HEAD(mvxpsec_descriptor_list, mvxpsec_descriptor_handle);

struct mvxpsec_descriptor_ring {
	struct mvxpsec_descriptor_handle *dma_head;
	struct mvxpsec_descriptor_handle *dma_last;
	int				dma_size;
};

#define MVXPSEC_SYNC_DESC(sc, x, f) \
    do { \
	bus_dmamap_sync((sc)->sc_dmat, (x)->map, \
            (x)->off, sizeof(struct mvxpsec_descriptor), (f)); \
    } while (0);

typedef struct mvxpsec_descriptor_ring mvxpsec_dma_ring;

#define MV_TDMA_DEFAULT_CONTROL \
	( MV_TDMA_CONTROL_DST_BURST_32 | \
	  MV_TDMA_CONTROL_SRC_BURST_32 | \
	  MV_TDMA_CONTROL_OUTS_EN | \
	  MV_TDMA_CONTROL_OUTS_MODE_4OUTS | \
	  MV_TDMA_CONTROL_BSWAP_DIS )

/*
 * Security Accelerator Descriptors
 */
struct mvxpsec_acc_descriptor {
	uint32_t acc_config;
	uint32_t acc_encdata;
	uint32_t acc_enclen;
	uint32_t acc_enckey;
	uint32_t acc_enciv;
	uint32_t acc_macsrc;
	uint32_t acc_macdst;
	uint32_t acc_maciv;
#define acc_desc_dword0 acc_config
#define acc_desc_dword1 acc_encdata
#define acc_desc_dword2 acc_enclen
#define acc_desc_dword3 acc_enckey
#define acc_desc_dword4 acc_enciv
#define acc_desc_dword5 acc_macsrc
#define acc_desc_dword6 acc_macdst
#define acc_desc_dword7 acc_maciv
} __attribute__((aligned(4)));

struct mvxpsec_crp_key {
	uint32_t crp_key32[8];
} __attribute__((aligned(4)));

struct mvxpsec_crp_iv {
	uint32_t crp_iv32[4];
} __attribute__((aligned(4)));

struct mvxpsec_mac_iv {
	uint32_t mac_iv32[5];
	uint32_t mac_ivpad[1]; /* bit[2:0] = 0 */
} __attribute__((aligned(8)));

/* many pointer in the desc has a limitation of bit[2:0] = 0. */
struct mvxpsec_packet_header {
	struct mvxpsec_acc_descriptor	desc;		/* 32 oct. */
	struct mvxpsec_crp_iv		crp_iv_work;	/* 16 oct. */
	struct mvxpsec_crp_iv		crp_iv_ext;	/* 16 oct. */
} __attribute__((aligned(4))); /* 64 oct. */

struct mvxpsec_session_header {
	struct mvxpsec_crp_key		crp_key;	/* 32 oct. */
	struct mvxpsec_crp_key		crp_key_d;	/* 32 oct. */
	struct mvxpsec_mac_iv		miv_in;		/* 24 oct. */
	struct mvxpsec_mac_iv		miv_out;	/* 24 oct. */
	uint8_t				pad[16];	/* 16 oct. */
} __attribute__((aligned(4))); /* 128 oct. */

/*
 * Usage of CESA internal SRAM
 *
 * +---------------+ MVXPSEC_SRAM_PKT_HDR_OFF(0)
 * |Packet Header  |   contains per packet information (IV, ACC descriptor)
 * |               |
 * |               |
 * +---------------+ MVXPSEC_SRAM_SESS_HDR_OFF
 * |Session Header |   contains per session information (Key, HMAC-iPad/oPad)  
 * |               |   may not DMA transfered if session is not changed.
 * |               |
 * +---------------+ MVXPSEC_SRAM_PAYLOAD_OFF
 * |Payload        | 
 * |               |
 * .               .
 * .               .
 * .               .
 * |               |
 * +---------------+ MV_ACC_SRAM_SIZE(2048)
 * 
 * The input data is transfered to SRAM from system DRAM using TDMA,
 * and ACC is working on the SRAM. When ACC finished the work,
 * TDMA returns the payload of SRAM to system DRAM.
 *
 * CPU can also access the SRAM via Mbus interface directly. This driver
 * access the SRAM only for debugging.
 *
 */
#define SRAM_PAYLOAD_SIZE \
    (MV_ACC_SRAM_SIZE \
     - sizeof(struct mvxpsec_packet_header) \
     - sizeof(struct mvxpsec_session_header))
struct mvxpsec_crypt_sram {
	struct mvxpsec_packet_header	packet_header;	/* 64 oct. */
	struct mvxpsec_session_header	session_header; /* 128 oct. */
	uint8_t				payload[SRAM_PAYLOAD_SIZE];
} __attribute__((aligned(8))); /* Max. 2048 oct. */
#define MVXPSEC_SRAM_PKT_HDR_OFF \
    (offsetof(struct mvxpsec_crypt_sram, packet_header))
#define MVXPSEC_SRAM_DESC_OFF (MVXPSEC_SRAM_PKT_HDR_OFF + \
    offsetof(struct mvxpsec_packet_header, desc))
#define MVXPSEC_SRAM_IV_WORK_OFF (MVXPSEC_SRAM_PKT_HDR_OFF + \
    offsetof(struct mvxpsec_packet_header, crp_iv_work))
#define MVXPSEC_SRAM_IV_EXT_OFF (MVXPSEC_SRAM_PKT_HDR_OFF + \
    offsetof(struct mvxpsec_packet_header, crp_iv_ext))

#define MVXPSEC_SRAM_SESS_HDR_OFF \
    (offsetof(struct mvxpsec_crypt_sram, session_header))
#define MVXPSEC_SRAM_KEY_OFF (MVXPSEC_SRAM_SESS_HDR_OFF + \
    offsetof(struct mvxpsec_session_header, crp_key))
#define MVXPSEC_SRAM_KEY_D_OFF (MVXPSEC_SRAM_SESS_HDR_OFF + \
    offsetof(struct mvxpsec_session_header, crp_key_d))
#define MVXPSEC_SRAM_MIV_IN_OFF (MVXPSEC_SRAM_SESS_HDR_OFF + \
    offsetof(struct mvxpsec_session_header, miv_in))
#define MVXPSEC_SRAM_MIV_OUT_OFF (MVXPSEC_SRAM_SESS_HDR_OFF + \
    offsetof(struct mvxpsec_session_header, miv_out))

#define MVXPSEC_SRAM_PAYLOAD_OFF \
    (offsetof(struct mvxpsec_crypt_sram, payload))

/* CESA device address (CESA internal SRAM address space) */
#define MVXPSEC_SRAM_DESC_DA		MVXPSEC_SRAM_DESC_OFF
#define MVXPSEC_SRAM_IV_WORK_DA		MVXPSEC_SRAM_IV_WORK_OFF
#define MVXPSEC_SRAM_IV_EXT_DA		MVXPSEC_SRAM_IV_EXT_OFF
#define MVXPSEC_SRAM_KEY_DA		MVXPSEC_SRAM_KEY_OFF
#define MVXPSEC_SRAM_KEY_D_DA		MVXPSEC_SRAM_KEY_D_OFF
#define MVXPSEC_SRAM_MIV_IN_DA		MVXPSEC_SRAM_MIV_IN_OFF
#define MVXPSEC_SRAM_MIV_OUT_DA		MVXPSEC_SRAM_MIV_OUT_OFF
#define MVXPSEC_SRAM_PAYLOAD_DA(offset) \
    (MVXPSEC_SRAM_PAYLOAD_OFF + (offset))

/*
 * Session management
 */
enum mvxpsec_data_type {
	MVXPSEC_DATA_NONE,
	MVXPSEC_DATA_RAW,
	MVXPSEC_DATA_MBUF,
	MVXPSEC_DATA_UIO,
	MVXPSEC_DATA_LAST,
};

/* session flags */
#define RDY_DATA	(1 << 0)
#define RDY_CRP_KEY	(1 << 1)
#define RDY_CRP_IV	(1 << 2)
#define RDY_MAC_KEY	(1 << 3)
#define RDY_MAC_IV	(1 << 4)
#define CRP_EXT_IV	(1 << 5)

#define SETUP_DONE	(1 << 10)
#define DELETED		(1 << 11)
#define DIR_ENCRYPT	(1 << 12)
#define DIR_DECRYPT	(1 << 13)

#define HW_RUNNING	(1 << 16)

/* 64 peer * 2 way(in/out) * 2 family(inet/inet6) * 2 state(mature/dying) */
#define MVXPSEC_MAX_SESSIONS	512

struct mvxpsec_session {
	struct mvxpsec_softc		*sc;
	uint32_t			sid;

	uint32_t			sflags;
	uint32_t			refs;

	/*
	 * Header of Security Accelerator
	 *   - include key entity for ciphers
	 *   - include iv for HMAC
	 */ 
	bus_dmamap_t			session_header_map; 
	struct mvxpsec_session_header	session_header;

	/* Key length for variable key length algorithm [bits] */
	int enc_klen;
	int mac_klen;

	/* IV Store */
	struct mvxpsec_crp_iv		session_iv;

	/* debug */
	int cipher_alg;		
	int hmac_alg;
};

struct mvxpsec_packet {
	struct mvxpsec_session		*mv_s;
	struct cryptop			*crp;
	int				flags;

	mvxpsec_dma_ring		dma_ring;

	bus_dmamap_t			pkt_header_map;
	struct mvxpsec_packet_header	pkt_header;

	bus_dmamap_t			data_map; 
	enum mvxpsec_data_type		data_type;
	uint32_t			data_len;
	union {
		/* payload buffer come from opencrypto API */
		void			*ptr;
		void			*raw;
		struct mbuf		*mbuf;
		struct uio		*uio;
	} data;

	/* IV place holder for EXPLICIT IV */
	void				*ext_iv;
	int				ext_ivlen;

	uint32_t			enc_off;
	uint32_t			enc_len;
	uint32_t			enc_ivoff;
	uint32_t			mac_off;
	uint32_t			mac_len;
	uint32_t			mac_dst;
#define data_ptr data.ptr
#define data_raw data.raw
#define data_mbuf data.mbuf
#define data_uio data.uio

	/* list */
	SIMPLEQ_ENTRY(mvxpsec_packet)	queue;
	SLIST_ENTRY(mvxpsec_packet)	free_list;
};
typedef SIMPLEQ_HEAD(mvxpsec_packet_queue, mvxpsec_packet) mvxpsec_queue_t;
typedef SLIST_HEAD(mvxpsec_packet_list, mvxpsec_packet) mvxpsec_list_t;

/*
 * DMA Configuration
 */
#define MVXPSEC_DMA_DESC_PAGES	16
#define MVXPSEC_DMA_MAX_SEGS	30
#define MVXPSEC_DMA_MAX_SIZE	2048 /* = SRAM size */

/*
 * Interrupt Configuration
 */
#define MVXPSEC_ALL_INT (0xffffffff)
#define MVXPSEC_ALL_ERR (0xffffffff)
#define MVXPSEC_DEFAULT_INT (MVXPSEC_INT_ACCTDMA)
#define MVXPSEC_DEFAULT_ERR (MVXPSEC_ALL_ERR)

/*
 * QUEUE Configuration
 */
#define MVXPSEC_MAX_QLEN		512
#define MVXPSEC_QLEN_HIWAT	256
#define MVXPSEC_QLEN_DEF_LOWAT	16
#define MVXPSEC_DEF_PENDING	0

/*
 * Event counters
 */
struct mvxpsec_evcnt {
	/* interuprts */
	struct evcnt intr_all;
	struct evcnt intr_auth;
	struct evcnt intr_des;
	struct evcnt intr_aes_enc;
	struct evcnt intr_aes_dec;
	struct evcnt intr_enc;
	struct evcnt intr_sa;
	struct evcnt intr_acctdma;
	struct evcnt intr_comp;
	struct evcnt intr_own;
	struct evcnt intr_acctdma_cont;

	/* session counter */
	struct evcnt session_new;
	struct evcnt session_free;

	/* packet counter */
	struct evcnt packet_ok;
	struct evcnt packet_err;

	/* queue */
	struct evcnt dispatch_packets;
	struct evcnt dispatch_queue;
	struct evcnt queue_full;
	struct evcnt max_dispatch;
	struct evcnt max_done;
};
#ifdef MVXPSEC_EVENT_COUNTERS
#define MVXPSEC_EVCNT_INCR(sc, name) do { \
	(sc)->sc_ev.name.ev_count++; \
} while (/*CONSTCOND*/0)
#define MVXPSEC_EVCNT_ADD(sc, name, val) do { \
	(sc)->sc_ev.name.ev_count += (val); \
} while (/*CONSTCOND*/0)
#define MVXPSEC_EVCNT_MAX(sc, name, val) do { \
	if ((val) > (sc)->sc_ev.name.ev_count) \
		(sc)->sc_ev.name.ev_count = (val); \
} while (/*CONSTCOND*/0)
#else
#define MVXPSEC_EVCNT_INCR(sc, name)		/* nothing */
#define MVXPSEC_EVCNT_ADD(sc, name, val)	/* nothing */
#define MVXPSEC_EVCNT_MAX(sc, name, val)	/* nothing */
#endif

struct mvxpsec_softc {
	device_t		sc_dev;
	uint32_t		sc_cid;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_dma_tag_t		sc_dmat;

	/* Memory Pools */
	struct mvxpsec_devmem	*sc_devmem_desc;
	struct mvxpsec_devmem	*sc_devmem_mmap;
	pool_cache_t		sc_session_pool;
	pool_cache_t		sc_packet_pool;

	/* Event Counters */
#ifdef MVXPSEC_EVENT_COUNTERS
	struct mvxpsec_evcnt	sc_ev;
#endif

	/* SRAM mappings */
	paddr_t			sc_sram_pa;
	void *			sc_sram_va;

	/* Interrupts and Timers */
	callout_t		sc_timeout;
	void *			sc_done_ih;
	void *			sc_error_ih;

	/* DMA Descriptors */
	kmutex_t		sc_dma_mtx;
	struct mvxpsec_descriptor_handle *sc_desc_ring;
	int			sc_desc_ring_size;
	int			sc_desc_ring_prod;
	int			sc_desc_ring_cons;

	/* Session */
	kmutex_t		sc_session_mtx;
	struct mvxpsec_session	*sc_sessions[MVXPSEC_MAX_SESSIONS];
	int			sc_nsessions;
	struct mvxpsec_session	*sc_last_session;

	/* Packet queue */
	kmutex_t		sc_queue_mtx;
	mvxpsec_queue_t		sc_wait_queue;
	int			sc_wait_qlen;
	int			sc_wait_qlimit;
	mvxpsec_queue_t		sc_run_queue;
	mvxpsec_list_t		sc_free_list;
	int			sc_free_qlen;
	uint32_t		sc_flags;

	/* Debug */
	int			sc_craft_conf;
	int			sc_craft_p0;
};
/* SRAM parameters accessor */
#define MVXPSEC_SRAM_BASE(sc) ((sc)->sc_sram_pa)
#define MVXPSEC_SRAM_SIZE(sc) (sizeof(struct mvxpsec_crypt_sram))
#define MVXPSEC_SRAM_PA(sc, offset)	\
    (MVXPSEC_SRAM_BASE(sc) + (offset))
#define MVXPSEC_SRAM_LIMIT(sc) \
    (MVXPSEC_SRAM_BASE(sc) + MVXPSEC_SRAM_SIZE(sc))
#define MVXPSEC_SRAM_PKT_HDR_PA(sc) \
    MVXPSEC_SRAM_PA((sc), MVXPSEC_SRAM_PKT_HDR_OFF)
#define MVXPSEC_SRAM_DESC_PA(sc) \
    MVXPSEC_SRAM_PA((sc), MVXPSEC_SRAM_DESC_OFF)
#define MVXPSEC_SRAM_IV_WORK_PA(sc) \
    MVXPSEC_SRAM_PA((sc), MVXPSEC_SRAM_IV_WORK_OFF)
#define MVXPSEC_SRAM_SESS_HDR_PA(sc) \
    MVXPSEC_SRAM_PA((sc), MVXPSEC_SRAM_SESS_HDR_OFF)
#define MVXPSEC_SRAM_KEY_PA(sc) \
    MVXPSEC_SRAM_PA((sc), MVXPSEC_SRAM_KEY_OFF)
#define MVXPSEC_SRAM_KEY_D_PA(sc) \
    MVXPSEC_SRAM_PA((sc), MVXPSEC_SRAM_KEY_D_OFF)
#define MVXPSEC_SRAM_MIV_IN_PA(sc) \
    MVXPSEC_SRAM_PA((sc), MVXPSEC_SRAM_MIV_IN_OFF)
#define MVXPSEC_SRAM_MIV_OUT_PA(sc) \
    MVXPSEC_SRAM_PA((sc), MVXPSEC_SRAM_MIV_OUT_OFF)
#define MVXPSEC_SRAM_PAYLOAD_PA(sc, offset) \
    MVXPSEC_SRAM_PA((sc), MVXPSEC_SRAM_PAYLOAD_OFF + (offset))

/*
 * OpenCrypto API
 */
extern int mvxpsec_register(struct mvxpsec_softc *);
extern int mvxpsec_newsession(void *, uint32_t *, struct cryptoini *);
extern int mvxpsec_freesession(void *, uint64_t);
extern int mvxpsec_dispatch(void *, struct cryptop *, int);
extern void mvxpsec_done(void *);

/* debug flags */
#define MVXPSEC_DEBUG_DMA		__BIT(0)
#define MVXPSEC_DEBUG_IOCTL		__BIT(1)
#define MVXPSEC_DEBUG_INTR		__BIT(2)
#define MVXPSEC_DEBUG_SRAM		__BIT(3)
#define MVXPSEC_DEBUG_OPENCRYPTO	__BIT(4)
#define MVXPSEC_DEBUG_PAYLOAD		__BIT(5)
#define MVXPSEC_DEBUG_HASH_IV		__BIT(6)
#define MVXPSEC_DEBUG_HASH_VAL		__BIT(7)
#define MVXPSEC_DEBUG_DESC		__BIT(8) /* descriptors and registers */
#define MVXPSEC_DEBUG_INPUT		__BIT(9)
#define MVXPSEC_DEBUG_ENC_IV		__BIT(10)
#define MVXPSEC_DEBUG_QUEUE		__BIT(11)

#define MVXPSEC_DEBUG_ALL		__BITS(11,0)

#ifdef MVXPSEC_DEBUG
#define MVXPSEC_PRINTF(level, fmt, ...) \
	do { \
		if (mvxpsec_debug & level) { \
			printf("%s: ", __func__); \
			printf((fmt), ##__VA_ARGS__); \
		} \
	} while (/*CONSTCOND*/0)
#else
#define MVXPSEC_PRINTF(level, fmt, ...) /* nothing */
#endif


#endif /* __MVXPSECVAR_H__ */

/*	$NetBSD: ubsecvar.h,v 1.10 2015/04/13 15:43:43 riastradh Exp $	*/
/*	$OpenBSD: ubsecvar.h,v 1.38 2009/03/27 13:31:30 reyk Exp $	*/

/*
 * Copyright (c) 2000 Theo de Raadt
 * Copyright (c) 2001 Patrik Lindergren (patrik@ipunplugged.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */

#ifndef	_DEV_PCI_UBSECVAR_H
#define	_DEV_PCI_UBSECVAR_H

#include <sys/types.h>
#include <sys/rndsource.h>

/* Maximum queue length */
#ifndef UBS_MAX_NQUEUE
#define UBS_MAX_NQUEUE		60
#endif

#define	UBS_MAX_SCATTER		64	/* Maximum scatter/gather depth */

#ifndef UBS_MAX_AGGR
#define	UBS_MAX_AGGR		17	/* Maximum aggregation count */
#endif

#ifndef UBS_MIN_AGGR
#define	UBS_MIN_AGGR		5	/* < 5827, Maximum aggregation count */
#endif

#define	UBSEC_CARD(sid)		(((sid) & 0xf0000000) >> 28)
#define	UBSEC_SESSION(sid)	( (sid) & 0x0fffffff)
#define	UBSEC_SID(crd, sesn)	(((crd) << 28) | ((sesn) & 0x0fffffff))

#define UBS_DEF_RTY		0xff	/* PCI Retry Timeout */
#define UBS_DEF_TOUT		0xff	/* PCI TRDY Timeout */
#define UBS_DEF_CACHELINE	0x01	/* Cache Line setting */

struct ubsec_dma_alloc {
	u_int32_t		dma_paddr;
	void *			dma_vaddr;
	bus_dmamap_t		dma_map;
	bus_dma_segment_t	dma_seg;
	bus_size_t		dma_size;
	int			dma_nseg;
};

struct ubsec_q2 {
	SIMPLEQ_ENTRY(ubsec_q2)		q_next;
	struct ubsec_dma_alloc		q_mcr;
	struct ubsec_dma_alloc		q_ctx;
	u_int				q_type;
};

struct ubsec_q2_rng {
	struct ubsec_q2			rng_q;
	struct ubsec_dma_alloc		rng_buf;
	int				rng_used;
};

/* C = (M ^ E) mod N */
#define	UBS_MODEXP_PAR_M	0
#define	UBS_MODEXP_PAR_E	1
#define	UBS_MODEXP_PAR_N	2
struct ubsec_q2_modexp {
	struct ubsec_q2			me_q;
	struct cryptkop *		me_krp;
	struct ubsec_dma_alloc		me_M;
	struct ubsec_dma_alloc		me_E;
	struct ubsec_dma_alloc		me_C;
	struct ubsec_dma_alloc		me_epb;
	int				me_modbits;
	int				me_shiftbits;
	int				me_normbits;
};

#define	UBS_RSAPRIV_PAR_P	0
#define	UBS_RSAPRIV_PAR_Q	1
#define	UBS_RSAPRIV_PAR_DP	2
#define	UBS_RSAPRIV_PAR_DQ	3
#define	UBS_RSAPRIV_PAR_PINV	4
#define	UBS_RSAPRIV_PAR_MSGIN	5
#define	UBS_RSAPRIV_PAR_MSGOUT	6
struct ubsec_q2_rsapriv {
	struct ubsec_q2			rpr_q;
	struct cryptkop *		rpr_krp;
	struct ubsec_dma_alloc		rpr_msgin;
	struct ubsec_dma_alloc		rpr_msgout;
};

#define	UBSEC_RNG_BUFSIZ	16		/* measured in 32bit words */

struct ubsec_dmachunk {
	struct ubsec_mcr	d_mcr;
	struct ubsec_mcr_add	d_mcradd[UBS_MAX_AGGR-1];
	struct ubsec_pktbuf	d_sbuf[UBS_MAX_SCATTER-1];
	struct ubsec_pktbuf	d_dbuf[UBS_MAX_SCATTER-1];
	u_int32_t		d_macbuf[5];
	union {
		struct ubsec_pktctx_aes256	ctx_aes256;
		struct ubsec_pktctx_aes192	ctx_aes192;
		struct ubsec_pktctx_aes128	ctx_aes128;
		struct ubsec_pktctx_3des	ctx_3des;
		struct ubsec_pktctx		ctx;
	} d_ctx;
};

struct ubsec_dma {
	SIMPLEQ_ENTRY(ubsec_dma)	d_next;
	struct ubsec_dmachunk		*d_dma;
	struct ubsec_dma_alloc		d_alloc;
};

#define	UBS_FLAGS_KEY		0x01		/* has key accelerator */
#define	UBS_FLAGS_LONGCTX	0x02		/* uses long ipsec ctx */
#define	UBS_FLAGS_BIGKEY	0x04		/* 2048bit keys */
#define	UBS_FLAGS_HWNORM	0x08		/* hardware normalization */
#define	UBS_FLAGS_RNG		0x10		/* hardware rng */
#define UBS_FLAGS_AES		0x20		/* supports AES */
#define UBS_FLAGS_MULTIMCR	0x40		/* 5827+ with 4 MCRs */
#define UBS_FLAGS_RNG4		0x80		/* 5827+ RNG on MCR4 */

struct ubsec_q {
	SIMPLEQ_ENTRY(ubsec_q)		q_next;
	int				q_nstacked_mcrs;
	struct ubsec_q			*q_stacked_mcr[UBS_MAX_AGGR-1];
	struct cryptop			*q_crp;
	struct ubsec_dma		*q_dma;

	struct mbuf			*q_src_m, *q_dst_m;
	struct uio			*q_src_io, *q_dst_io;

	int				q_sesn;
	int				q_flags;

	bus_dmamap_t			q_dst_map;
	bus_dmamap_t			q_src_map;	  /* cached src_map */
	bus_dmamap_t			q_cached_dst_map; /* cached dst_map */
};

struct ubsec_softc {
	device_t		sc_dev;		/* generic device */
	void			*sc_ih;		/* interrupt handler cookie */
	kmutex_t		sc_mtx;
	pci_chipset_tag_t	sc_pct;		/* pci chipset tag */
	bus_space_handle_t	sc_sh;		/* memory handle */
	bus_space_tag_t		sc_st;		/* memory tag */
	bus_dma_tag_t		sc_dmat;	/* dma tag */
	int			sc_flags;	/* device specific flags */
	int			sc_suspended;
	int			sc_needwakeup;	/* notify crypto layer */
	u_int32_t		sc_statmask;	/* interrupt status mask */
	int32_t			sc_cid;		/* crypto tag */
	int			sc_maxaggr;	/* max pkt aggregation */
	SIMPLEQ_HEAD(,ubsec_q)	sc_queue;	/* packet queue, mcr1 */
	int			sc_nqueue;	/* count enqueued, mcr1 */
	SIMPLEQ_HEAD(,ubsec_q)	sc_qchip;	/* on chip, mcr1 */
	int			sc_nqchip;	/* count on chip, mcr1 */
	SIMPLEQ_HEAD(,ubsec_q)	sc_freequeue;	/* list of free queue elements */
	SIMPLEQ_HEAD(,ubsec_q2)	sc_queue2;	/* packet queue, mcr2 */
	int			sc_nqueue2;	/* count enqueued, mcr2 */
	SIMPLEQ_HEAD(,ubsec_q2)	sc_qchip2;	/* on chip, mcr2 */
	SIMPLEQ_HEAD(,ubsec_q2)	sc_queue4;	/* packet queue, mcr4 */
	int			sc_nqueue4;	/* count enqueued, mcr4 */
	SIMPLEQ_HEAD(,ubsec_q2)	sc_qchip4;	/* on chip, mcr4 */
	int			sc_nsessions;	/* # of sessions */
	struct ubsec_session	*sc_sessions;	/* sessions */
	struct callout		sc_rngto;	/* rng timeout */
	int			sc_rnghz;	/* rng poll time */
	struct ubsec_q2_rng	sc_rng;
	krndsource_t		sc_rnd_source;
	int			sc_rng_need;	/* how many bytes wanted */
	struct ubsec_dma	sc_dmaa[UBS_MAX_NQUEUE];
	struct ubsec_q		*sc_queuea[UBS_MAX_NQUEUE];
	SIMPLEQ_HEAD(,ubsec_q2)	sc_q2free;	/* free list */
	bus_size_t		sc_memsize;	/* size mapped by sc_sh */
};

#define	UBSEC_QFLAGS_COPYOUTIV		0x1

struct ubsec_session {
	u_int32_t	ses_used;
	u_int32_t	ses_key[8];		/* 3DES/AES key */
	u_int32_t	ses_hminner[5];		/* hmac inner state */
	u_int32_t	ses_hmouter[5];		/* hmac outer state */
	u_int32_t	ses_iv[4];		/* [3]DES iv or AES iv/icv */
};

struct ubsec_stats {
	u_int64_t hst_ibytes;
	u_int64_t hst_obytes;
	u_int32_t hst_ipackets;
	u_int32_t hst_opackets;
	u_int32_t hst_invalid;		/* invalid argument */
	u_int32_t hst_badsession;	/* invalid session id */
	u_int32_t hst_badflags;		/* flags indicate !(mbuf | uio) */
	u_int32_t hst_nodesc;		/* op submitted w/o descriptors */
	u_int32_t hst_badalg;		/* unsupported algorithm */

	u_int32_t hst_nomem;
	u_int32_t hst_queuefull;
	u_int32_t hst_dmaerr;
	u_int32_t hst_mcrerr;
	u_int32_t hst_nodmafree;

	u_int32_t hst_lenmismatch;	/* enc/auth lengths different */
	u_int32_t hst_skipmismatch;	/* enc part begins before auth part */
	u_int32_t hst_iovmisaligned;	/* iov op not aligned */
	u_int32_t hst_noirq;		/* IRQ for no reason */
	u_int32_t hst_unaligned;	/* unaligned src caused copy */
	u_int32_t hst_nomap;		/* bus_dmamap_create failed */
	u_int32_t hst_noload;		/* bus_dmamap_load_* failed */
	u_int32_t hst_nombuf;		/* MGET* failed */
	u_int32_t hst_nomcl;		/* MCLGET* failed */
	u_int32_t hst_totbatch;		/* ops submitted w/o interrupt */
	u_int32_t hst_maxbatch;		/* max ops submitted together */
	u_int32_t hst_maxqueue;		/* max ops queued for submission */
	u_int32_t hst_maxqchip;		/* max mcr1 ops out for processing */
	u_int32_t hst_mcr1full;		/* MCR1 too busy to take ops */
	u_int32_t hst_rng;		/* RNG requests */
	u_int32_t hst_modexp;		/* MOD EXP requests */
	u_int32_t hst_modexpcrt;	/* MOD EXP CRT requests */
};

#endif	/* _DEV_PCI_UBSECVAR_H */

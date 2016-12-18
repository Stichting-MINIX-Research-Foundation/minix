/*	$NetBSD: hifn7751var.h,v 1.12 2015/04/14 20:32:36 riastradh Exp $	*/
/*	$OpenBSD: hifn7751var.h,v 1.18 2000/06/02 22:36:45 deraadt Exp $	*/

/*
 * Invertex AEON / Hifn 7751 driver
 * Copyright (c) 1999 Invertex Inc. All rights reserved.
 * Copyright (c) 1999 Theo de Raadt
 * Copyright (c) 2000-2001 Network Security Technologies, Inc.
 *			http://www.netsec.net
 *
 * Please send any comments, feedback, bug-fixes, or feature requests to
 * software@invertex.com.
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#ifndef __DEV_PCI_HIFN7751VAR_H__
#define __DEV_PCI_HIFN7751VAR_H__

#ifdef _KERNEL

#include <sys/rndsource.h>

/*
 *  Some configurable values for the driver
 */
#define	HIFN_D_CMD_RSIZE	24	/* command descriptors */
#define	HIFN_D_SRC_RSIZE	80	/* source descriptors */
#define	HIFN_D_DST_RSIZE	80	/* destination descriptors */
#define	HIFN_D_RES_RSIZE	24	/* result descriptors */

/*
 *  Length values for cryptography
 */
#define HIFN_DES_KEY_LENGTH		8
#define HIFN_3DES_KEY_LENGTH		24
#define HIFN_MAX_CRYPT_KEY_LENGTH	HIFN_3DES_KEY_LENGTH
#define HIFN_IV_LENGTH			8
#define	HIFN_AES_IV_LENGTH		16
#define HIFN_MAX_IV_LENGTH		HIFN_AES_IV_LENGTH

/*
 *  Length values for authentication
 */
#define HIFN_MAC_KEY_LENGTH		64
#define HIFN_MD5_LENGTH			16
#define HIFN_SHA1_LENGTH		20
#define HIFN_MAC_TRUNC_LENGTH		12

#define MAX_SCATTER 64

/*
 * Data structure to hold all 4 rings and any other ring related data.
 */
struct hifn_dma {
	/*
	 *  Descriptor rings.  We add +1 to the size to accommodate the
	 *  jump descriptor.
	 */
	struct hifn_desc	cmdr[HIFN_D_CMD_RSIZE+1];
	struct hifn_desc	srcr[HIFN_D_SRC_RSIZE+1];
	struct hifn_desc	dstr[HIFN_D_DST_RSIZE+1];
	struct hifn_desc	resr[HIFN_D_RES_RSIZE+1];

	struct hifn_command	*hifn_commands[HIFN_D_RES_RSIZE];

	u_char			command_bufs[HIFN_D_CMD_RSIZE][HIFN_MAX_COMMAND];
	u_char			result_bufs[HIFN_D_CMD_RSIZE][HIFN_MAX_RESULT];
	u_int32_t		slop[HIFN_D_CMD_RSIZE];

	u_int64_t		test_src, test_dst;

	/*
	 *  Our current positions for insertion and removal from the descriptor
	 *  rings.
	 */
	int			cmdi, srci, dsti, resi;
	volatile int		cmdu, srcu, dstu, resu;
	int			cmdk, srck, dstk, resk;
};

struct hifn_session {
	int hs_state;
	int hs_prev_op; /* XXX collapse into hs_flags? */
	u_int8_t hs_iv[HIFN_MAX_IV_LENGTH];
};

/* We use a state machine on sessions */
#define	HS_STATE_FREE	0		/* unused session entry */
#define	HS_STATE_USED	1		/* allocated, but key not on card */
#define	HS_STATE_KEY	2		/* allocated and key is on card */

#define	HIFN_RING_SYNC(sc, r, i, f)					\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_dmamap,		\
	    offsetof(struct hifn_dma, r[i]), sizeof(struct hifn_desc), (f))

#define	HIFN_CMDR_SYNC(sc, i, f)	HIFN_RING_SYNC((sc), cmdr, (i), (f))
#define	HIFN_RESR_SYNC(sc, i, f)	HIFN_RING_SYNC((sc), resr, (i), (f))
#define	HIFN_SRCR_SYNC(sc, i, f)	HIFN_RING_SYNC((sc), srcr, (i), (f))
#define	HIFN_DSTR_SYNC(sc, i, f)	HIFN_RING_SYNC((sc), dstr, (i), (f))

#define	HIFN_CMD_SYNC(sc, i, f)						\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_dmamap,		\
	    offsetof(struct hifn_dma, command_bufs[(i)][0]),		\
	    HIFN_MAX_COMMAND, (f))

#define	HIFN_RES_SYNC(sc, i, f)						\
	bus_dmamap_sync((sc)->sc_dmat, (sc)->sc_dmamap,		\
	    offsetof(struct hifn_dma, result_bufs[(i)][0]),		\
	    HIFN_MAX_RESULT, (f))

/*
 * Holds data specific to a single HIFN board.
 */
struct hifn_softc {
	device_t	sc_dv;		/* generic device */
	void *		sc_ih;		/* interrupt handler cookie */
	u_int32_t	sc_dmaier;
	u_int32_t	sc_drammodel;	/* 1=dram, 0=sram */

	bus_space_handle_t	sc_sh0, sc_sh1;
	bus_space_tag_t		sc_st0, sc_st1;
#ifdef __NetBSD__
	bus_size_t		sc_iosz0, sc_iosz1;
#endif
	bus_dma_tag_t		sc_dmat;

	struct hifn_dma *sc_dma;
	bus_dmamap_t sc_dmamap;
	bus_dma_segment_t sc_dmasegs[1];
	int sc_dmansegs;
	int32_t sc_cid;
	int sc_maxses;
	int sc_ramsize;
	int sc_flags;
#define	HIFN_HAS_RNG		0x01
#define	HIFN_HAS_PUBLIC		0x02
#define	HIFN_HAS_AES		0x04	/* includes AES support */
#define	HIFN_IS_7811		0x08	/* Hifn 7811 part */
#define	HIFN_IS_7956		0x10	/* Hifn 7956/7955 don't have SDRAM */
#define	HIFN_NO_BURSTWRITE	0x20
#define	HIFN_HAS_LEDS		0x40

#define HIFN_RNG_BITSPER	17	/* From Hifn 6500 paper: 0.06 bits
					   of entropy per RNG register bit
					   worst-case */

	struct callout		sc_rngto;	/* rng timeout */
	struct callout		sc_tickto;	/* led-clear timeout */
	krndsource_t	sc_rnd_source;
	int			sc_rnghz;
	int			sc_rng_need;	/* how many bytes wanted */
	int			sc_c_busy;	/* command ring busy */
	int			sc_s_busy;	/* source data ring busy */
	int			sc_d_busy;	/* destination data ring busy */
	int			sc_r_busy;	/* result ring busy */
	int			sc_active;	/* for initial countdown */
	int			sc_needwakeup;	/* ops q'd wating on resources */
	int			sc_curbatch;	/* # ops submitted w/o int */
	int			sc_suspended;
	struct hifn_session sc_sessions[2048];
	pci_chipset_tag_t sc_pci_pc;
	pcitag_t sc_pci_tag;
	bus_size_t sc_waw_lastreg;
	int sc_waw_lastgroup;
	kmutex_t		sc_mtx;
};

#define WRITE_REG_0(sc,reg,val)		hifn_write_4((sc), 0, (reg), (val))
#define WRITE_REG_1(sc,reg,val)		hifn_write_4((sc), 1, (reg), (val))
#define	READ_REG_0(sc,reg)		hifn_read_4((sc), 0, (reg))
#define	READ_REG_1(sc,reg)		hifn_read_4((sc), 1, (reg))

#define	SET_LED(sc,v)							\
	if (sc->sc_flags & HIFN_HAS_LEDS)				\
		WRITE_REG_1(sc, HIFN_1_7811_MIPSRST,			\
		    READ_REG_1(sc, HIFN_1_7811_MIPSRST) | (v))
#define	CLR_LED(sc,v)							\
	if (sc->sc_flags & HIFN_HAS_LEDS)				\
		WRITE_REG_1(sc, HIFN_1_7811_MIPSRST,			\
		    READ_REG_1(sc, HIFN_1_7811_MIPSRST) & ~(v))

/*
 *  struct hifn_command
 *
 *  This is the control structure used to pass commands to hifn_encrypt().
 *
 *  flags
 *  -----
 *  Flags is the bitwise "or" values for command configuration.  A single
 *  encrypt direction needs to be set:
 *
 *	HIFN_ENCODE or HIFN_DECODE
 *
 *  To use cryptography, a single crypto algorithm must be included:
 *
 *	HIFN_CRYPT_3DES or HIFN_CRYPT_DES
 *
 *  To use authentication, a single MAC algorithm must be included:
 *
 *	HIFN_MAC_MD5 or HIFN_MAC_SHA1
 *
 *  By default MD5 uses a 16 byte hash and SHA-1 uses a 20 byte hash.
 *  If the value below is set, hash values are truncated or assumed
 *  truncated to 12 bytes:
 *
 *	HIFN_MAC_TRUNC
 *
 *  Keys for encryption and authentication can be sent as part of a command,
 *  or the last key value used with a particular session can be retrieved
 *  and used again if either of these flags are not specified.
 *
 *	HIFN_CRYPT_NEW_KEY, HIFN_MAC_NEW_KEY
 *
 *  session_num
 *  -----------
 *  A number between 0 and 2048 (for DRAM models) or a number between
 *  0 and 768 (for SRAM models).  Those who don't want to use session
 *  numbers should leave value at zero and send a new crypt key and/or
 *  new MAC key on every command.  If you use session numbers and
 *  don't send a key with a command, the last key sent for that same
 *  session number will be used.
 *
 *  Warning:  Using session numbers and multiboard at the same time
 *            is currently broken.
 *
 *  mbuf
 *  ----
 *  Either fill in the mbuf pointer and npa=0 or
 *	 fill packp[] and packl[] and set npa to > 0
 *
 *  mac_header_skip
 *  ---------------
 *  The number of bytes of the source_buf that are skipped over before
 *  authentication begins.  This must be a number between 0 and 2^16-1
 *  and can be used by IPsec implementers to skip over IP headers.
 *  *** Value ignored if authentication not used ***
 *
 *  crypt_header_skip
 *  -----------------
 *  The number of bytes of the source_buf that are skipped over before
 *  the cryptographic operation begins.  This must be a number between 0
 *  and 2^16-1.  For IPsec, this number will always be 8 bytes larger
 *  than the auth_header_skip (to skip over the ESP header).
 *  *** Value ignored if cryptography not used ***
 *
 */
struct hifn_command {
	u_int16_t session_num;
	u_int16_t base_masks, cry_masks, mac_masks, comp_masks;
	u_int8_t iv[HIFN_MAX_IV_LENGTH], *ck, mac[HIFN_MAC_KEY_LENGTH];
	int cklen;
	int sloplen, slopidx;

	union {
		struct mbuf *src_m;
		struct uio *src_io;
	} srcu;
	bus_dmamap_t src_map;

	union {
		struct mbuf *dst_m;
		struct uio *dst_io;
	} dstu;
	bus_dmamap_t dst_map;

	u_short mac_header_skip, mac_process_len;
	u_short crypt_header_skip, crypt_process_len;

	struct hifn_softc *softc;
	struct cryptop *crp;
	struct cryptodesc *enccrd, *maccrd,  *compcrd;

};

/*
 *  Return values for hifn_crypto()
 */
#define HIFN_CRYPTO_SUCCESS	0
#define HIFN_CRYPTO_BAD_INPUT	(-1)
#define HIFN_CRYPTO_RINGS_FULL	(-2)


/**************************************************************************
 *
 *  Function:  hifn_crypto
 *
 *  Purpose:   Called by external drivers to begin an encryption on the
 *             HIFN board.
 *
 *  Blocking/Non-blocking Issues
 *  ============================
 *  The driver cannot block in hifn_crypto (no calls to tsleep) currently.
 *  hifn_crypto() returns HIFN_CRYPTO_RINGS_FULL if there is not enough
 *  room in any of the rings for the request to proceed.
 *
 *  Return Values
 *  =============
 *  0 for success, negative values on error
 *
 *  Defines for negative error codes are:
 *
 *    HIFN_CRYPTO_BAD_INPUT  :  The passed in command had invalid settings.
 *    HIFN_CRYPTO_RINGS_FULL :  All DMA rings were full and non-blocking
 *                              behaviour was requested.
 *
 *************************************************************************/

/*
 * Convert back and forth from 'sid' to 'card' and 'session'
 */
#define HIFN_CARD(sid)		(((sid) & 0xf0000000) >> 28)
#define HIFN_SESSION(sid)	((sid) & 0x000007ff)
#define HIFN_SID(crd,ses)	(((crd) << 28) | ((ses) & 0x7ff))

#endif /* _KERNEL */

struct hifn_stats {
	u_int64_t hst_ibytes;
	u_int64_t hst_obytes;
	u_int32_t hst_ipackets;
	u_int32_t hst_opackets;
	u_int32_t hst_invalid;
	u_int32_t hst_nomem;
	u_int32_t hst_abort;
};

#endif /* __DEV_PCI_HIFN7751VAR_H__ */

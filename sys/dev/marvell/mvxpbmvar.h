/*	$NetBSD: mvxpbmvar.h,v 1.1 2015/06/03 03:55:47 hsuenaga Exp $	*/
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
#ifndef _MVXPBMVAR_H_
#define _MVXPBMVAR_H_
#include <dev/marvell/marvellvar.h>

/*
 * Max number of unit.
 */
#define MVXPBM_UNIT_MAX		1

/*
 * Buffer alignement
 */
#define MVXPBM_NUM_SLOTS	2048	/* minimum number of slots */
#define MVXPBM_PACKET_SIZE	2000	/* minimum packet size */

#define MVXPBM_BUF_ALIGN	65536	/* Mbus window size granularity */
#define MVXPBM_BUF_MASK		(MVXPBM_BUF_ALIGN - 1)
#define MVXPBM_CHUNK_ALIGN	32	/* Cache line size */
#define MVXPBM_CHUNK_MASK	(MVXPBM_CHUNK_ALIGN - 1)
#define MVXPBM_DATA_ALIGN	32	/* Cache line size */
#define MVXPBM_DATA_MASK	(MVXPBM_DATA_ALIGN - 1)
#define MVXPBM_DATA_UNIT	8

/*
 * Packet Buffer Header
 *
 * this chunks may be managed by H/W Buffer Manger(BM) device,
 * but there is no device driver yet.
 *
 *                            +----------------+ bm_buf
 *                            |chunk header    | |
 * +----------------+         |                | |chunk->buf_off
 * |mbuf (M_EXT set)|<--------|struct mbuf *m  | V
 * +----------------+         +----------------+ chunk->buf_va/buf_pa
 * |   m_ext.ext_buf|-------->|packet buffer   | |
 * +----------------+         |                | |chunk->buf_size
 *                            |                | V
 *                            +----------------+
 *                            |chunk header    |
 *                            |....            |
 */

struct mvxpbm_chunk {
	struct mbuf	*m;		/* back pointer to  mbuf header */
	void		*sc;		/* back pointer to softc */
	off_t		off;		/* offset of chunk */
	paddr_t		pa;		/* physical address of chunk */

	off_t		buf_off;	/* offset of packet from sc_bm_buf */
	paddr_t		buf_pa;		/* physical address of packet */
	vaddr_t		buf_va;		/* virtual addres of packet */
	size_t		buf_size;	/* size of buffer (exclude hdr) */

	LIST_ENTRY(mvxpbm_chunk) link;
	/* followed by packet buffer */
};

struct mvxpbm_softc {
	device_t	sc_dev;
	bus_dma_tag_t	sc_dmat;
	bus_space_tag_t sc_iot;
	kmutex_t	sc_mtx;

	/* software emulated */
	int		sc_emul;

	/* DMA MAP for entire buffer */
	bus_dmamap_t	sc_buf_map;
	char		*sc_buf;
	paddr_t		sc_buf_pa;
	size_t		sc_buf_size;

	/* memory chunk properties */
	size_t		sc_slotsize;		/* size of bm_slots include header */
	uint32_t	sc_chunk_count;		/* number of chunks */
	size_t		sc_chunk_size;		/* size of packet buffer */
	size_t		sc_chunk_header_size;	/* size of hader + padding */ 
	off_t		sc_chunk_packet_offset;	/* allocate m_leading_space */

	/* for software based management */
	LIST_HEAD(__mvxpbm_freehead, mvxpbm_chunk) sc_free;
	LIST_HEAD(__mvxpbm_inusehead, mvxpbm_chunk) sc_inuse;
};

#define BM_SYNC_ALL	0

/* get mvxpbm device context */
struct mvxpbm_softc *mvxpbm_device(struct marvell_attach_args *);

/* allocate new memory chunk */
struct mvxpbm_chunk *mvxpbm_alloc(struct mvxpbm_softc *);

/* free memory chunk */
void mvxpbm_free_chunk(struct mvxpbm_chunk *);

/* prepare mbuf header after Rx */
int mvxpbm_init_mbuf_hdr(struct mvxpbm_chunk *);

/* sync DMA seguments */
void mvxpbm_dmamap_sync(struct mvxpbm_chunk *, size_t, int);

/* lock */
void mvxpbm_lock(struct mvxpbm_softc *);
void mvxpbm_unlock(struct mvxpbm_softc *);

/* get params */
const char *mvxpbm_xname(struct mvxpbm_softc *);
size_t mvxpbm_chunk_size(struct mvxpbm_softc *);
uint32_t mvxpbm_chunk_count(struct mvxpbm_softc *);
off_t mvxpbm_packet_offset(struct mvxpbm_softc *);
paddr_t mvxpbm_buf_pbase(struct mvxpbm_softc *);
size_t mvxpbm_buf_size(struct mvxpbm_softc *);
#endif /* _MVXPBMVAR_H_ */

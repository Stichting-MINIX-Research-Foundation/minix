/*	$NetBSD: mvxpbm.c,v 1.1 2015/06/03 03:55:47 hsuenaga Exp $	*/
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
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mvxpbm.c,v 1.1 2015/06/03 03:55:47 hsuenaga Exp $");

#include "opt_multiprocessor.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/mbuf.h>

#include <dev/marvell/marvellreg.h>
#include <dev/marvell/marvellvar.h>

#include "mvxpbmvar.h"

#ifdef DEBUG
#define STATIC /* nothing */
#define DPRINTF(fmt, ...) \
	do { \
		if (mvxpbm_debug >= 1) { \
			printf("%s: ", __func__); \
			printf((fmt), ##__VA_ARGS__); \
		} \
	} while (/*CONSTCOND*/0)
#define DPRINTFN(level , fmt, ...) \
	do { \
		if (mvxpbm_debug >= (level)) { \
			printf("%s: ", __func__); \
			printf((fmt), ##__VA_ARGS__); \
		} \
	} while (/*CONSTCOND*/0)
#define DPRINTDEV(dev, level, fmt, ...) \
	do { \
		if (mvxpbm_debug >= (level)) { \
			device_printf((dev), \
			    "%s: "fmt , __func__, ##__VA_ARGS__); \
		} \
	} while (/*CONSTCOND*/0)
#define DPRINTSC(sc, level, fmt, ...) \
	do { \
		device_t dev = (sc)->sc_dev; \
		if (mvxpbm_debug >= (level)) { \
			device_printf(dev, \
			    "%s: " fmt, __func__, ##__VA_ARGS__); \
		} \
	} while (/*CONSTCOND*/0)
#else
#define STATIC static
#define DPRINTF(fmt, ...)
#define DPRINTFN(level, fmt, ...)
#define DPRINTDEV(dev, level, fmt, ...)
#define DPRINTSC(sc, level, fmt, ...)
#endif

/* autoconf(9) */
STATIC int mvxpbm_match(device_t, cfdata_t, void *);
STATIC void mvxpbm_attach(device_t, device_t, void *);
STATIC int mvxpbm_evcnt_attach(struct mvxpbm_softc *);
CFATTACH_DECL_NEW(mvxpbm_mbus, sizeof(struct mvxpbm_softc),
    mvxpbm_match, mvxpbm_attach, NULL, NULL);

/* DMA buffers */
STATIC int mvxpbm_alloc_buffer(struct mvxpbm_softc *);

/* mbuf subroutines */
STATIC void mvxpbm_free_mbuf(struct mbuf *, void *, size_t, void *);

/* singleton device instance */
static struct mvxpbm_softc sc_emul;
static struct mvxpbm_softc *sc0;

/* debug level */
#ifdef DEBUG
static int mvxpbm_debug = 0;
#endif

/*
 * autoconf(9)
 */
STATIC int
mvxpbm_match(device_t parent, cfdata_t match, void *aux)
{
	struct marvell_attach_args *mva = aux;

	if (strcmp(mva->mva_name, match->cf_name) != 0)
		return 0;
	if (mva->mva_unit > MVXPBM_UNIT_MAX)
		return 0;
	if (sc0 != NULL)
		return 0;
	if (mva->mva_offset != MVA_OFFSET_DEFAULT) {
		/* Hardware BM is not supported yet. */
		return 0;
	}

	return 1;
}

STATIC void
mvxpbm_attach(device_t parnet, device_t self, void *aux)
{
	struct marvell_attach_args *mva = aux;
	struct mvxpbm_softc *sc = device_private(self);

	aprint_naive("\n");
	aprint_normal(": Marvell ARMADA Buffer Manager\n");
	memset(sc, 0, sizeof(*sc));
	sc->sc_dev = self;
	sc->sc_iot = mva->mva_iot;
	sc->sc_dmat = mva->mva_dmat;

	if (mva->mva_offset == MVA_OFFSET_DEFAULT) { 
		aprint_normal_dev(sc->sc_dev, "Software emulation.\n");
		sc->sc_emul = 1;
	}

	mutex_init(&sc->sc_mtx, MUTEX_DEFAULT, IPL_NET);
	LIST_INIT(&sc->sc_free);
	LIST_INIT(&sc->sc_inuse);

	/* DMA buffers */
	if (mvxpbm_alloc_buffer(sc) != 0)
		return;

	/* event counters */
	mvxpbm_evcnt_attach(sc);

	sc0 = sc;
	return;

}

STATIC int
mvxpbm_evcnt_attach(struct mvxpbm_softc *sc)
{
	return 0;
}

/*
 * DMA buffers
 */
STATIC int
mvxpbm_alloc_buffer(struct mvxpbm_softc *sc)
{
	bus_dma_segment_t segs;
	char *kva, *ptr, *ptr_next, *ptr_data;
	char *bm_buf_end;
	uint32_t align, pad;
	int nsegs;
	int error;

	/*
	 * set default buffer sizes. this will changed to satisfy 
	 * alignment restrictions.
	 */
	sc->sc_chunk_count = 0;
	sc->sc_chunk_size = MVXPBM_PACKET_SIZE;
	sc->sc_chunk_header_size = sizeof(struct mvxpbm_chunk);
	sc->sc_chunk_packet_offset = 64;

	/*
	 * adjust bm_chunk_size, bm_chunk_header_size, bm_slotsize
	 * to satisfy alignemnt restrictions. 
	 *
	 *    <----------------  bm_slotsize [oct.] ------------------>
	 *                               <--- bm_chunk_size[oct.] ---->
	 *    <--- header_size[oct] ---> <-- MBXPE_BM_SIZE[oct.] --->
	 *   +-----------------+--------+---------+-----------------+--+
	 *   | bm_chunk hdr    |pad     |pkt_off  |   packet data   |  |
	 *   +-----------------+--------+---------+-----------------+--+
	 *   ^                          ^         ^                    ^
	 *   |                          |         |                    |
	 *   ptr                 ptr_data  DMA here         ptr_next
	 *
	 * Restrictions:
	 *   - total buffer size must be multiple of MVXPBM_BUF_ALIGN
	 *   - ptr must be aligned to MVXPBM_CHUNK_ALIGN
	 *   - ptr_data must be aligned to MVXPEBM_DATA_ALIGN
	 *   - bm_chunk_size must be multiple of 8[bytes].
	 */
	/* start calclation from  0x0000.0000 */
	ptr = (char *)0;

	/* align start of packet data */
	ptr_data = ptr + sc->sc_chunk_header_size;
	align = (unsigned long)ptr_data & MVXPBM_DATA_MASK;
	if (align != 0) {
		pad = MVXPBM_DATA_ALIGN - align;
		sc->sc_chunk_header_size += pad;
		DPRINTSC(sc, 1, "added padding to BM header, %u bytes\n", pad);
	}

	/* align size of packet data */
	ptr_data = ptr + sc->sc_chunk_header_size;
	ptr_next = ptr_data + MVXPBM_PACKET_SIZE;
	align = (unsigned long)ptr_next & MVXPBM_CHUNK_MASK;
	if (align != 0) {
		pad = MVXPBM_CHUNK_ALIGN - align;
		ptr_next += pad;
		DPRINTSC(sc, 1, "added padding to BM pktbuf, %u bytes\n", pad);
	}
	sc->sc_slotsize = ptr_next - ptr;
	sc->sc_chunk_size = ptr_next - ptr_data;
	KASSERT((sc->sc_chunk_size % MVXPBM_DATA_UNIT) == 0);

	/* align total buffer size to Mbus window boundary */
	sc->sc_buf_size = sc->sc_slotsize * MVXPBM_NUM_SLOTS;
	align = (unsigned long)sc->sc_buf_size & MVXPBM_BUF_MASK;
	if (align != 0) {
		pad = MVXPBM_BUF_ALIGN - align;
		sc->sc_buf_size += pad;
		DPRINTSC(sc, 1,
		    "expand buffer to fit page boundary, %u bytes\n", pad);
	}

	/*
	 * get the aligned buffer from busdma(9) framework
	 */
	if (bus_dmamem_alloc(sc->sc_dmat, sc->sc_buf_size, MVXPBM_BUF_ALIGN, 0,
	    &segs, 1, &nsegs, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->sc_dev, "can't alloc BM buffers\n");
		return ENOBUFS;
	}
	if (bus_dmamem_map(sc->sc_dmat, &segs, nsegs, sc->sc_buf_size,
	    (void **)&kva, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->sc_dev,
		    "can't map dma buffers (%zu bytes)\n", sc->sc_buf_size);
		error = ENOBUFS;
		goto fail1;
	}
	if (bus_dmamap_create(sc->sc_dmat, sc->sc_buf_size, 1, sc->sc_buf_size,
	    0, BUS_DMA_NOWAIT, &sc->sc_buf_map)) {
		aprint_error_dev(sc->sc_dev, "can't create dma map\n");
		error = ENOBUFS;
		goto fail2;
	}
	if (bus_dmamap_load(sc->sc_dmat, sc->sc_buf_map,
	    kva, sc->sc_buf_size, NULL, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->sc_dev, "can't load dma map\n");
		error = ENOBUFS;
		goto fail3;
	}
	sc->sc_buf = (void *)kva;
	sc->sc_buf_pa = segs.ds_addr;
	bm_buf_end = (void *)(kva + sc->sc_buf_size);
	DPRINTSC(sc, 1, "memory pool at %p\n", sc->sc_buf);

	/* slice the buffer */
	mvxpbm_lock(sc);
	for (ptr = sc->sc_buf; ptr + sc->sc_slotsize <= bm_buf_end;
	    ptr += sc->sc_slotsize) {
		struct mvxpbm_chunk *chunk;

		/* initialzie chunk */
		ptr_data = ptr + sc->sc_chunk_header_size;
		chunk = (struct mvxpbm_chunk *)ptr;
		chunk->m = NULL;
		chunk->sc = sc;
		chunk->off = (ptr - sc->sc_buf);
		chunk->pa = (paddr_t)(sc->sc_buf_pa + chunk->off);
		chunk->buf_off = (ptr_data - sc->sc_buf);
		chunk->buf_pa = (paddr_t)(sc->sc_buf_pa + chunk->buf_off);
		chunk->buf_va = (vaddr_t)(sc->sc_buf + chunk->buf_off);
		chunk->buf_size = sc->sc_chunk_size;

		/* add to free list (for software management) */
		LIST_INSERT_HEAD(&sc->sc_free, chunk, link);
		mvxpbm_dmamap_sync(chunk, BM_SYNC_ALL, BUS_DMASYNC_PREREAD);
		sc->sc_chunk_count++;

		DPRINTSC(sc, 9, "new chunk %p\n", (void *)chunk->buf_va);
	}
	mvxpbm_unlock(sc);
	return 0;

fail3:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_buf_map);
fail2:
	bus_dmamem_unmap(sc->sc_dmat, kva, sc->sc_buf_size);
fail1:
	bus_dmamem_free(sc->sc_dmat, &segs, nsegs);

	return error;
}

/*
 * mbuf subroutines
 */
STATIC void
mvxpbm_free_mbuf(struct mbuf *m, void *buf, size_t size, void *arg)
{
	struct mvxpbm_chunk *chunk = (struct mvxpbm_chunk *)arg;
	int s;

	KASSERT(m != NULL);
	KASSERT(arg != NULL);

	DPRINTFN(3, "free packet %p\n", m);
	if (m->m_flags & M_PKTHDR)
		m_tag_delete_chain((m), NULL);
	chunk->m = NULL;
	s = splvm();
	pool_cache_put(mb_cache, m);
	splx(s);
	return mvxpbm_free_chunk(chunk);
}

/*
 * Exported APIs
 */
/* get mvxpbm device context */
struct mvxpbm_softc *
mvxpbm_device(struct marvell_attach_args *mva)
{
	struct mvxpbm_softc *sc;

	if (sc0 != NULL)
		return sc0;
	if (mva == NULL)
		return NULL;

	/* allocate software emulation context */
	sc = &sc_emul;
	memset(sc, 0, sizeof(*sc));
	sc->sc_emul = 1;
	sc->sc_iot = mva->mva_iot;
	sc->sc_dmat = mva->mva_dmat;

	mutex_init(&sc->sc_mtx, MUTEX_DEFAULT, IPL_NET);
	LIST_INIT(&sc->sc_free);
	LIST_INIT(&sc->sc_inuse);

	if (mvxpbm_alloc_buffer(sc) != 0)
		return NULL;
	mvxpbm_evcnt_attach(sc);
	sc0 = sc;
	return sc0;
}

/* allocate new memory chunk */
struct mvxpbm_chunk *
mvxpbm_alloc(struct mvxpbm_softc *sc)
{
	struct mvxpbm_chunk *chunk;

	mvxpbm_lock(sc);

	chunk = LIST_FIRST(&sc->sc_free);
	if (chunk == NULL) {
		mvxpbm_unlock(sc);
		return NULL;
	}

	LIST_REMOVE(chunk, link);
	LIST_INSERT_HEAD(&sc->sc_inuse, chunk, link);

	mvxpbm_unlock(sc);
	return chunk;
}

/* free memory chunk */
void
mvxpbm_free_chunk(struct mvxpbm_chunk *chunk)
{
	struct mvxpbm_softc *sc = chunk->sc;

	KASSERT(chunk->m == NULL);
	DPRINTFN(3, "bm chunk free\n");

	mvxpbm_lock(sc);

	LIST_REMOVE(chunk, link);
	LIST_INSERT_HEAD(&sc->sc_free, chunk, link);

	mvxpbm_unlock(sc);
}

/* prepare mbuf header after Rx */
int
mvxpbm_init_mbuf_hdr(struct mvxpbm_chunk *chunk)
{
	struct mvxpbm_softc *sc = chunk->sc;

	KASSERT(chunk->m == NULL);

	/* add new mbuf header */
	MGETHDR(chunk->m, M_DONTWAIT, MT_DATA);
	if (chunk->m == NULL) {
		aprint_error_dev(sc->sc_dev, "cannot get mbuf\n");
		return ENOBUFS;
	}
	MEXTADD(chunk->m, chunk->buf_va, chunk->buf_size, 0, 
		mvxpbm_free_mbuf, chunk);
	chunk->m->m_flags |= M_EXT_RW;
	chunk->m->m_len = chunk->m->m_pkthdr.len = chunk->buf_size;
	if (sc->sc_chunk_packet_offset)
		m_adj(chunk->m, sc->sc_chunk_packet_offset);

	return 0;
}

/* sync DMA seguments */
void
mvxpbm_dmamap_sync(struct mvxpbm_chunk *chunk, size_t size, int ops)
{
	struct mvxpbm_softc *sc = chunk->sc;

	KASSERT(size <= chunk->buf_size);
	if (size == 0)
		size = chunk->buf_size;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_buf_map, chunk->buf_off, size, ops);
}

/* lock */
void
mvxpbm_lock(struct mvxpbm_softc *sc)
{
	mutex_enter(&sc->sc_mtx);
}

void
mvxpbm_unlock(struct mvxpbm_softc *sc)
{
	mutex_exit(&sc->sc_mtx);
}

/* get params */
const char *
mvxpbm_xname(struct mvxpbm_softc *sc)
{
	if (sc->sc_emul) {
		return "software_bm";
	}
	return device_xname(sc->sc_dev);
}

size_t
mvxpbm_chunk_size(struct mvxpbm_softc *sc)
{
	return sc->sc_chunk_size;
}

uint32_t
mvxpbm_chunk_count(struct mvxpbm_softc *sc)
{
	return sc->sc_chunk_count;
}

off_t
mvxpbm_packet_offset(struct mvxpbm_softc *sc)
{
	return sc->sc_chunk_packet_offset;
}

paddr_t
mvxpbm_buf_pbase(struct mvxpbm_softc *sc)
{
	return sc->sc_buf_pa;
}

size_t
mvxpbm_buf_size(struct mvxpbm_softc *sc)
{
	return sc->sc_buf_size;
}

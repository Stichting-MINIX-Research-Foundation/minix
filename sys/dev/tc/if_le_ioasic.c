/*	$NetBSD: if_le_ioasic.c,v 1.33 2009/04/18 14:58:04 tsutsui Exp $	*/

/*
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * LANCE on DEC IOCTL ASIC.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_le_ioasic.c,v 1.33 2009/04/18 14:58:04 tsutsui Exp $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#endif

#include <dev/ic/lancereg.h>
#include <dev/ic/lancevar.h>
#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>

#include <dev/tc/if_levar.h>
#include <dev/tc/tcvar.h>
#include <dev/tc/ioasicreg.h>
#include <dev/tc/ioasicvar.h>

struct le_ioasic_softc {
	struct	am7990_softc sc_am7990;	/* glue to MI code */
	struct	lereg1 *sc_r1;		/* LANCE registers */
	/* XXX must match with le_softc of if_levar.h XXX */

	bus_dma_tag_t sc_dmat;		/* bus dma tag */
	bus_dmamap_t sc_dmamap;		/* bus dmamap */
};

static int  le_ioasic_match(device_t, cfdata_t, void *);
static void le_ioasic_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(le_ioasic, sizeof(struct le_softc),
    le_ioasic_match, le_ioasic_attach, NULL, NULL);

static void le_ioasic_copytobuf_gap2(struct lance_softc *, void *, int, int);
static void le_ioasic_copyfrombuf_gap2(struct lance_softc *, void *, int, int);
static void le_ioasic_copytobuf_gap16(struct lance_softc *, void *, int, int);
static void le_ioasic_copyfrombuf_gap16(struct lance_softc *, void *,
	    int, int);
static void le_ioasic_zerobuf_gap16(struct lance_softc *, int, int);

static int
le_ioasic_match(device_t parent, cfdata_t cf, void *aux)
{
	struct ioasicdev_attach_args *d = aux;

	if (strncmp("PMAD-BA ", d->iada_modname, TC_ROM_LLEN) != 0)
		return 0;

	return 1;
}

/* IOASIC LANCE DMA needs 128KB boundary aligned 128KB chunk */
#define	LE_IOASIC_MEMSIZE	(128 * 1024)
#define	LE_IOASIC_MEMALIGN	(128 * 1024)

static void
le_ioasic_attach(device_t parent, device_t self, void *aux)
{
	struct le_ioasic_softc *sc = device_private(self);
	struct ioasicdev_attach_args *d = aux;
	struct lance_softc *le = &sc->sc_am7990.lsc;
	struct ioasic_softc *iosc = device_private(parent);
	bus_space_tag_t ioasic_bst;
	bus_space_handle_t ioasic_bsh;
	bus_dma_tag_t dmat;
	bus_dma_segment_t seg;
	tc_addr_t tca;
	uint32_t ssr;
	int rseg;
	void *le_iomem;

	le->sc_dev = self;
	ioasic_bst = iosc->sc_bst;
	ioasic_bsh = iosc->sc_bsh;
	dmat = sc->sc_dmat = iosc->sc_dmat;
	/*
	 * Allocate a DMA area for the chip.
	 */
	if (bus_dmamem_alloc(dmat, LE_IOASIC_MEMSIZE, LE_IOASIC_MEMALIGN,
	    0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) {
		aprint_error(": can't allocate DMA area for LANCE\n");
		return;
	}
	if (bus_dmamem_map(dmat, &seg, rseg, LE_IOASIC_MEMSIZE,
	    &le_iomem, BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) {
		aprint_error(": can't map DMA area for LANCE\n");
		bus_dmamem_free(dmat, &seg, rseg);
		return;
	}
	/*
	 * Create and load the DMA map for the DMA area.
	 */
	if (bus_dmamap_create(dmat, LE_IOASIC_MEMSIZE, 1,
	    LE_IOASIC_MEMSIZE, 0, BUS_DMA_NOWAIT, &sc->sc_dmamap)) {
		aprint_error(": can't create DMA map\n");
		goto bad;
	}
	if (bus_dmamap_load(dmat, sc->sc_dmamap,
	    le_iomem, LE_IOASIC_MEMSIZE, NULL, BUS_DMA_NOWAIT)) {
		aprint_error(": can't load DMA map\n");
		goto bad;
	}
	/*
	 * Bind 128KB buffer with IOASIC DMA.
	 */
	tca = IOASIC_DMA_ADDR(sc->sc_dmamap->dm_segs[0].ds_addr);
	bus_space_write_4(ioasic_bst, ioasic_bsh, IOASIC_LANCE_DMAPTR, tca);
	ssr = bus_space_read_4(ioasic_bst, ioasic_bsh, IOASIC_CSR);
	ssr |= IOASIC_CSR_DMAEN_LANCE;
	bus_space_write_4(ioasic_bst, ioasic_bsh, IOASIC_CSR, ssr);

	sc->sc_r1 = (struct lereg1 *)
		TC_DENSE_TO_SPARSE(TC_PHYS_TO_UNCACHED(d->iada_addr));
	le->sc_mem = (void *)TC_PHYS_TO_UNCACHED(le_iomem);
	le->sc_copytodesc = le_ioasic_copytobuf_gap2;
	le->sc_copyfromdesc = le_ioasic_copyfrombuf_gap2;
	le->sc_copytobuf = le_ioasic_copytobuf_gap16;
	le->sc_copyfrombuf = le_ioasic_copyfrombuf_gap16;
	le->sc_zerobuf = le_ioasic_zerobuf_gap16;

	dec_le_common_attach(&sc->sc_am7990,
	    (uint8_t *)iosc->sc_base + IOASIC_SLOT_2_START);

	ioasic_intr_establish(parent, d->iada_cookie, TC_IPL_NET,
	    am7990_intr, sc);
	return;

 bad:
	bus_dmamem_unmap(dmat, le_iomem, LE_IOASIC_MEMSIZE);
	bus_dmamem_free(dmat, &seg, rseg);
}

/*
 * Special memory access functions needed by ioasic-attached LANCE
 * chips.
 */

/*
 * gap2: two bytes of data followed by two bytes of pad.
 *
 * Buffers must be 4-byte aligned.  The code doesn't worry about
 * doing an extra byte.
 */

void
le_ioasic_copytobuf_gap2(struct lance_softc *sc, void *fromv, int boff, int len)
{
	volatile void *buf = sc->sc_mem;
	uint8_t *from = fromv;
	volatile uint16_t *bptr;

	if (boff & 0x1) {
		/* handle unaligned first byte */
		bptr = ((volatile uint16_t *)buf) + (boff - 1);
		*bptr = (*from++ << 8) | (*bptr & 0xff);
		bptr += 2;
		len--;
	} else
		bptr = ((volatile uint16_t *)buf) + boff;
	while (len > 1) {
		*bptr = (from[1] << 8) | (from[0] & 0xff);
		bptr += 2;
		from += 2;
		len -= 2;
	}
	if (len == 1)
		*bptr = (uint16_t)*from;
}

void
le_ioasic_copyfrombuf_gap2(struct lance_softc *sc, void *tov, int boff, int len)
{
	volatile void *buf = sc->sc_mem;
	uint8_t *to = tov;
	volatile uint16_t *bptr;
	uint16_t tmp;

	if (boff & 0x1) {
		/* handle unaligned first byte */
		bptr = ((volatile uint16_t *)buf) + (boff - 1);
		*to++ = (*bptr >> 8) & 0xff;
		bptr += 2;
		len--;
	} else
		bptr = ((volatile uint16_t *)buf) + boff;
	while (len > 1) {
		tmp = *bptr;
		*to++ = tmp & 0xff;
		*to++ = (tmp >> 8) & 0xff;
		bptr += 2;
		len -= 2;
	}
	if (len == 1)
		*to = *bptr & 0xff;
}

/*
 * gap16: 16 bytes of data followed by 16 bytes of pad.
 *
 * Buffers must be 32-byte aligned.
 */

void
le_ioasic_copytobuf_gap16(struct lance_softc *sc, void *fromv, int boff,
    int len)
{
	uint8_t *buf = sc->sc_mem;
	uint8_t *from = fromv;
	uint8_t *bptr;

	bptr = buf + ((boff << 1) & ~0x1f);
	boff &= 0xf;

	/*
	 * Dispose of boff so destination of subsequent copies is
	 * 16-byte aligned.
	 */
	if (boff) {
		int xfer;
		xfer = min(len, 16 - boff);
		memcpy(bptr + boff, from, xfer);
		from += xfer;
		bptr += 32;
		len -= xfer;
	}

	/* Destination of  copies is now 16-byte aligned. */
	if (len >= 16)
		switch ((u_long)from & (sizeof(uint32_t) -1)) {
		case 2:
			/*  Ethernet headers make this the dominant case. */
		do {
			uint32_t *dst = (uint32_t *)bptr;
			uint16_t t0;
			uint32_t t1,  t2, t3, t4;

			/* read from odd-16-bit-aligned, cached src */
			t0 = *(uint16_t *)(from +  0);
			t1 = *(uint32_t *)(from +  2);
			t2 = *(uint32_t *)(from +  6);
			t3 = *(uint32_t *)(from + 10);
			t4 = *(uint16_t *)(from + 14);

			/* DMA buffer is uncached on mips */
			dst[0] =         t0 |  (t1 << 16);
			dst[1] = (t1 >> 16) |  (t2 << 16);
			dst[2] = (t2 >> 16) |  (t3 << 16);
			dst[3] = (t3 >> 16) |  (t4 << 16);

			from += 16;
			bptr += 32;
			len -= 16;
		} while (len >= 16);
		break;

		case 0:
		do {
			uint32_t *src = (uint32_t*)from;
			uint32_t *dst = (uint32_t*)bptr;
			uint32_t t0, t1, t2, t3;

			t0 = src[0]; t1 = src[1]; t2 = src[2]; t3 = src[3];
			dst[0] = t0; dst[1] = t1; dst[2] = t2; dst[3] = t3;

			from += 16;
			bptr += 32;
			len -= 16;
		} while (len >= 16);
		break;

		default:
		/* Does odd-aligned case ever happen? */
		do {
			memcpy(bptr, from, 16);
			from += 16;
			bptr += 32;
			len -= 16;
		} while (len >= 16);
		break;
	}
	if (len)
		memcpy(bptr, from, len);
}

void
le_ioasic_copyfrombuf_gap16(struct lance_softc *sc, void *tov, int boff,
    int len)
{
	uint8_t *buf = sc->sc_mem;
	uint8_t *to = tov;
	uint8_t *bptr;

	bptr = buf + ((boff << 1) & ~0x1f);
	boff &= 0xf;

	/* Dispose of boff. source of copy is subsequently 16-byte aligned. */
	if (boff) {
		int xfer;
		xfer = min(len, 16 - boff);
		memcpy(to, bptr + boff, xfer);
		to += xfer;
		bptr += 32;
		len -= xfer;
	}
	if (len >= 16)
	switch ((u_long)to & (sizeof(uint32_t) -1)) {
	case 2:
		/*
		 * to is aligned to an odd 16-bit boundary.  Ethernet headers
		 * make this the dominant case (98% or more).
		 */
		do {
			uint32_t *src = (uint32_t *)bptr;
			uint32_t t0, t1, t2, t3;

			/* read from uncached aligned DMA buf */
			t0 = src[0]; t1 = src[1]; t2 = src[2]; t3 = src[3];

			/* write to odd-16-bit-word aligned dst */
			*(uint16_t *)(to +  0) = (uint16_t)t0;
			*(uint32_t *)(to +  2) = (t0 >> 16) | (t1 << 16);
			*(uint32_t *)(to +  6) = (t1 >> 16) | (t2 << 16);
			*(uint32_t *)(to + 10) = (t2 >> 16) | (t3 << 16);
			*(uint16_t *)(to + 14) = (t3 >> 16);
			bptr += 32;
			to += 16;
			len -= 16;
		} while (len > 16);
		break;
	case 0:
		/* 32-bit aligned aligned copy. Rare. */
		do {
			uint32_t *src = (uint32_t *)bptr;
			uint32_t *dst = (uint32_t *)to;
			uint32_t t0, t1, t2, t3;

			t0 = src[0]; t1 = src[1]; t2 = src[2]; t3 = src[3];
			dst[0] = t0; dst[1] = t1; dst[2] = t2; dst[3] = t3;
			to += 16;
			bptr += 32;
			len -= 16;
		} while (len  > 16);
		break;

	/* XXX Does odd-byte-aligned case ever happen? */
	default:
		do {
			memcpy(to, bptr, 16);
			to += 16;
			bptr += 32;
			len -= 16;
		} while (len  > 16);
		break;
	}
	if (len)
		memcpy(to, bptr, len);
}

void
le_ioasic_zerobuf_gap16(struct lance_softc *sc, int boff, int len)
{
	uint8_t *buf = sc->sc_mem;
	uint8_t *bptr;
	int xfer;

	bptr = buf + ((boff << 1) & ~0x1f);
	boff &= 0xf;
	xfer = min(len, 16 - boff);
	while (len > 0) {
		memset(bptr + boff, 0, xfer);
		bptr += 32;
		boff = 0;
		len -= xfer;
		xfer = min(len, 16);
	}
}

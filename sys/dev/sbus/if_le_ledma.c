/*	$NetBSD: if_le_ledma.c,v 1.35 2010/01/19 22:07:43 pooka Exp $	*/

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum; Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center; Paul Kranenburg.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_le_ledma.c,v 1.35 2010/01/19 22:07:43 pooka Exp $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#endif

#include <sys/bus.h>
#include <sys/intr.h>
#include <machine/autoconf.h>

#include <dev/sbus/sbusvar.h>

#include <dev/ic/lsi64854reg.h>
#include <dev/ic/lsi64854var.h>

#include <dev/ic/lancereg.h>
#include <dev/ic/lancevar.h>
#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>

#include "ioconf.h"

/*
 * LANCE registers.
 */
#define LEREG1_RDP	0	/* Register Data port */
#define LEREG1_RAP	2	/* Register Address port */

struct	le_softc {
	struct	am7990_softc	sc_am7990;	/* glue to MI code */
	bus_space_tag_t		sc_bustag;
	bus_dmamap_t		sc_dmamap;
	bus_space_handle_t	sc_reg;		/* LANCE registers */
	struct	lsi64854_softc	*sc_dma;	/* pointer to my dma */
	u_int			sc_laddr;	/* LANCE DMA address */
	u_int			sc_lostcount;
#define LE_LOSTTHRESH	5	/* lost carrior count to switch media */
};

#define MEMSIZE		(16*1024)	/* LANCE memory size */
#define LEDMA_BOUNDARY	(16*1024*1024)	/* must not cross 16MB boundary */

int	lematch_ledma(device_t, cfdata_t, void *);
void	leattach_ledma(device_t, device_t, void *);

/*
 * Media types supported by the Sun4m.
 */
static int lemedia[] = {
	IFM_ETHER|IFM_10_T,
	IFM_ETHER|IFM_10_5,
	IFM_ETHER|IFM_AUTO,
};
#define NLEMEDIA	__arraycount(lemedia)

void	lesetutp(struct lance_softc *);
void	lesetaui(struct lance_softc *);

int	lemediachange(struct lance_softc *);
void	lemediastatus(struct lance_softc *, struct ifmediareq *);

CFATTACH_DECL_NEW(le_ledma, sizeof(struct le_softc),
    lematch_ledma, leattach_ledma, NULL, NULL);

#if defined(_KERNEL_OPT)
#include "opt_ddb.h"
#endif

static void lewrcsr(struct lance_softc *, uint16_t, uint16_t);
static uint16_t lerdcsr(struct lance_softc *, uint16_t);
static void lehwreset(struct lance_softc *);
static void lehwinit(struct lance_softc *);
static void lenocarrier(struct lance_softc *);

static void
lewrcsr(struct lance_softc *sc, uint16_t port, uint16_t val)
{
	struct le_softc *lesc = (struct le_softc *)sc;
	bus_space_tag_t t = lesc->sc_bustag;
	bus_space_handle_t h = lesc->sc_reg;

	bus_space_write_2(t, h, LEREG1_RAP, port);
	bus_space_write_2(t, h, LEREG1_RDP, val);

#if defined(SUN4M)
	/*
	 * We need to flush the Sbus->Mbus write buffers. This can most
	 * easily be accomplished by reading back the register that we
	 * just wrote (thanks to Chris Torek for this solution).
	 */
	(void)bus_space_read_2(t, h, LEREG1_RDP);
#endif
}

static uint16_t
lerdcsr(struct lance_softc *sc, uint16_t port)
{
	struct le_softc *lesc = (struct le_softc *)sc;
	bus_space_tag_t t = lesc->sc_bustag;
	bus_space_handle_t h = lesc->sc_reg;

	bus_space_write_2(t, h, LEREG1_RAP, port);
	return (bus_space_read_2(t, h, LEREG1_RDP));
}

void
lesetutp(struct lance_softc *sc)
{
	struct lsi64854_softc *dma = ((struct le_softc *)sc)->sc_dma;
	uint32_t csr;

	csr = L64854_GCSR(dma);
	csr |= E_TP_AUI;
	L64854_SCSR(dma, csr);
	delay(20000);	/* must not touch le for 20ms */
}

void
lesetaui(struct lance_softc *sc)
{
	struct lsi64854_softc *dma = ((struct le_softc *)sc)->sc_dma;
	uint32_t csr;

	csr = L64854_GCSR(dma);
	csr &= ~E_TP_AUI;
	L64854_SCSR(dma, csr);
	delay(20000);	/* must not touch le for 20ms */
}

int
lemediachange(struct lance_softc *sc)
{
	struct ifmedia *ifm = &sc->sc_media;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

	/*
	 * Switch to the selected media.  If autoselect is
	 * set, we don't really have to do anything.  We'll
	 * switch to the other media when we detect loss of
	 * carrier.
	 */
	switch (IFM_SUBTYPE(ifm->ifm_media)) {
	case IFM_10_T:
		lesetutp(sc);
		break;

	case IFM_10_5:
		lesetaui(sc);
		break;

	case IFM_AUTO:
		break;

	default:
		return (EINVAL);
	}

	return (0);
}

void
lemediastatus(struct lance_softc *sc, struct ifmediareq *ifmr)
{
	struct lsi64854_softc *dma = ((struct le_softc *)sc)->sc_dma;

	/*
	 * Notify the world which media we're currently using.
	 */
	if (L64854_GCSR(dma) & E_TP_AUI)
		ifmr->ifm_active = IFM_ETHER|IFM_10_T;
	else
		ifmr->ifm_active = IFM_ETHER|IFM_10_5;
}

static void
lehwreset(struct lance_softc *sc)
{
	struct le_softc *lesc = (struct le_softc *)sc;
	struct lsi64854_softc *dma = lesc->sc_dma;
	uint32_t csr;
	u_int aui_bit;

	/*
	 * Reset DMA channel.
	 */
	csr = L64854_GCSR(dma);
	aui_bit = csr & E_TP_AUI;
	DMA_RESET(dma);

	/* Write bits 24-31 of Lance address */
	bus_space_write_4(dma->sc_bustag, dma->sc_regs, L64854_REG_ENBAR,
			  lesc->sc_laddr & 0xff000000);

	DMA_ENINTR(dma);

	/*
	 * Disable E-cache invalidates on chip writes.
	 * Retain previous cable selection bit.
	 */
	csr = L64854_GCSR(dma);
	csr |= (E_DSBL_WR_INVAL | aui_bit);
	L64854_SCSR(dma, csr);
	delay(20000);	/* must not touch le for 20ms */
}

static void
lehwinit(struct lance_softc *sc)
{

	/*
	 * Make sure we're using the currently-enabled media type.
	 * XXX Actually, this is probably unnecessary, now.
	 */
	switch (IFM_SUBTYPE(sc->sc_media.ifm_cur->ifm_media)) {
	case IFM_10_T:
		lesetutp(sc);
		break;

	case IFM_10_5:
		lesetaui(sc);
		break;
	}
}

static void
lenocarrier(struct lance_softc *sc)
{
	struct le_softc *lesc = (struct le_softc *)sc;

	/* it may take a while for modern switches to set 10baseT media */
	if (lesc->sc_lostcount++ < LE_LOSTTHRESH)
		return;

	lesc->sc_lostcount = 0;

	/*
	 * Check if the user has requested a certain cable type, and
	 * if so, honor that request.
	 */
	printf("%s: lost carrier on ", device_xname(sc->sc_dev));

	if (L64854_GCSR(lesc->sc_dma) & E_TP_AUI) {
		printf("UTP port");
		switch (IFM_SUBTYPE(sc->sc_media.ifm_media)) {
		case IFM_10_5:
		case IFM_AUTO:
			printf(", switching to AUI port");
			lesetaui(sc);
		}
	} else {
		printf("AUI port");
		switch (IFM_SUBTYPE(sc->sc_media.ifm_media)) {
		case IFM_10_T:
		case IFM_AUTO:
			printf(", switching to UTP port");
			lesetutp(sc);
		}
	}
	printf("\n");
}

int
lematch_ledma(device_t parent, cfdata_t cf, void *aux)
{
	struct sbus_attach_args *sa = aux;

	return (strcmp(cf->cf_name, sa->sa_name) == 0);
}


void
leattach_ledma(device_t parent, device_t self, void *aux)
{
	struct le_softc *lesc = device_private(self);
	struct lance_softc *sc = &lesc->sc_am7990.lsc;
	struct lsi64854_softc *lsi = device_private(parent);
	struct sbus_attach_args *sa = aux;
	bus_dma_tag_t dmatag = sa->sa_dmatag;
	bus_dma_segment_t seg;
	int rseg, error;

	sc->sc_dev = self;
	lesc->sc_bustag = sa->sa_bustag;

	/* Establish link to `ledma' device */
	lesc->sc_dma = lsi;
	lesc->sc_dma->sc_client = lesc;

	/* Map device registers */
	if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_slot,
			 sa->sa_offset,
			 sa->sa_size,
			 0, &lesc->sc_reg) != 0) {
		aprint_error(": cannot map registers\n");
		return;
	}

	/* Allocate buffer memory */
	sc->sc_memsize = MEMSIZE;

	/* Get a DMA handle */
	if ((error = bus_dmamap_create(dmatag, MEMSIZE, 1, MEMSIZE,
					LEDMA_BOUNDARY, BUS_DMA_NOWAIT,
					&lesc->sc_dmamap)) != 0) {
		aprint_error(": DMA map create error %d\n", error);
		return;
	}

	/* Allocate DMA buffer */
	if ((error = bus_dmamem_alloc(dmatag, MEMSIZE, 0, LEDMA_BOUNDARY,
				 &seg, 1, &rseg, BUS_DMA_NOWAIT)) != 0) {
		aprint_error(": DMA buffer alloc error %d\n",error);
		return;
	}

	/* Map DMA buffer into kernel space */
	if ((error = bus_dmamem_map(dmatag, &seg, rseg, MEMSIZE,
			       (void **)&sc->sc_mem,
			       BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		aprint_error(": DMA buffer map error %d\n", error);
		bus_dmamem_free(dmatag, &seg, rseg);
		return;
	}

	/* Load DMA buffer */
	if ((error = bus_dmamap_load(dmatag, lesc->sc_dmamap, sc->sc_mem,
			MEMSIZE, NULL, BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		aprint_error(": DMA buffer map load error %d\n", error);
		bus_dmamem_free(dmatag, &seg, rseg);
		bus_dmamem_unmap(dmatag, sc->sc_mem, MEMSIZE);
		return;
	}

	lesc->sc_laddr = lesc->sc_dmamap->dm_segs[0].ds_addr;
	sc->sc_addr = lesc->sc_laddr & 0xffffff;
	sc->sc_conf3 = LE_C3_BSWP | LE_C3_ACON | LE_C3_BCON;
	lesc->sc_lostcount = 0;

	sc->sc_mediachange = lemediachange;
	sc->sc_mediastatus = lemediastatus;
	sc->sc_supmedia = lemedia;
	sc->sc_nsupmedia = NLEMEDIA;
	sc->sc_defaultmedia = IFM_ETHER|IFM_AUTO;

	prom_getether(sa->sa_node, sc->sc_enaddr);

	sc->sc_copytodesc = lance_copytobuf_contig;
	sc->sc_copyfromdesc = lance_copyfrombuf_contig;
	sc->sc_copytobuf = lance_copytobuf_contig;
	sc->sc_copyfrombuf = lance_copyfrombuf_contig;
	sc->sc_zerobuf = lance_zerobuf_contig;

	sc->sc_rdcsr = lerdcsr;
	sc->sc_wrcsr = lewrcsr;
	sc->sc_hwinit = lehwinit;
	sc->sc_nocarrier = lenocarrier;
	sc->sc_hwreset = lehwreset;

	/* Establish interrupt handler */
	if (sa->sa_nintr != 0)
		(void)bus_intr_establish(sa->sa_bustag, sa->sa_pri, IPL_NET,
					 am7990_intr, sc);

	am7990_config(&lesc->sc_am7990);

	/* now initialize DMA */
	lehwreset(sc);
}

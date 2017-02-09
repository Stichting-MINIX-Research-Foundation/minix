/* $NetBSD: atppc_puc.c,v 1.14 2014/03/29 19:28:24 christos Exp $ */

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jaromir Dolecek.
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

#include "opt_atppc.h"

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: atppc_puc.c,v 1.14 2014/03/29 19:28:24 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/termios.h>

#include <sys/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pucvar.h>

#include <dev/ic/atppcvar.h>

static int	atppc_puc_match(device_t, cfdata_t, void *);
static void	atppc_puc_attach(device_t, device_t, void *);

struct atppc_puc_softc {
	/* Machine independent device data */
	struct atppc_softc sc_atppc;

	bus_dmamap_t sc_dmamap;
};

CFATTACH_DECL_NEW(atppc_puc, sizeof(struct atppc_puc_softc), atppc_puc_match,
    atppc_puc_attach, NULL, NULL);

static int atppc_puc_dma_setup(struct atppc_puc_softc *);
static int atppc_puc_dma_start(struct atppc_softc *, void *, u_int,
	u_int8_t);
static int atppc_puc_dma_finish(struct atppc_softc *);
static int atppc_puc_dma_abort(struct atppc_softc *);
static int atppc_puc_dma_malloc(device_t, void **, bus_addr_t *,
	bus_size_t);
static void atppc_puc_dma_free(device_t, void **, bus_addr_t *,
	bus_size_t);

/*
 * atppc_acpi_match: autoconf(9) match routine
 */
static int
atppc_puc_match(device_t parent, cfdata_t match, void *aux)
{
	struct puc_attach_args *aa = aux;

	/*
	 * Locators already matched, just check the type.
	 */
	if (aa->type != PUC_PORT_TYPE_LPT)
		return (0);

	return (1);
}

static void
atppc_puc_attach(device_t parent, device_t self, void *aux)
{
	struct atppc_softc *sc = device_private(self);
	struct atppc_puc_softc *psc = device_private(self);
	struct puc_attach_args *aa = aux;
	const char *intrstr;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev_ok = ATPPC_NOATTACH;

	printf(": AT Parallel Port\n");

	/* Attach */
	sc->sc_dev = self;
	sc->sc_iot = aa->t;
	sc->sc_ioh = aa->h;
	sc->sc_dmat = aa->dmat;
	sc->sc_has = 0;

	intrstr = pci_intr_string(aa->pc, aa->intrhandle, intrbuf,
	    sizeof(intrbuf));
	sc->sc_ieh = pci_intr_establish(aa->pc, aa->intrhandle, IPL_TTY,
	    atppcintr, sc);
	if (sc->sc_ieh == NULL) {
		aprint_error_dev(sc->sc_dev, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(sc->sc_dev, "interrupting at %s\n", intrstr);
	sc->sc_has |= ATPPC_HAS_INTR;

	/* setup DMA hooks */
	if (atppc_puc_dma_setup(psc) == 0) {
		sc->sc_has |= ATPPC_HAS_DMA;
		sc->sc_dma_start = atppc_puc_dma_start;
		sc->sc_dma_finish = atppc_puc_dma_finish;
		sc->sc_dma_abort = atppc_puc_dma_abort;
		sc->sc_dma_malloc = atppc_puc_dma_malloc;
		sc->sc_dma_free = atppc_puc_dma_free;
	}

	/* Finished attach */
	sc->sc_dev_ok = ATPPC_ATTACHED;

	/* Run soft configuration attach */
	atppc_sc_attach(sc);
}

/* Setup DMA structures */
static int
atppc_puc_dma_setup(struct atppc_puc_softc *psc)
{
	return EOPNOTSUPP;	/* XXX DMA not tested yet */
#if 0
	struct atppc_softc *sc = (struct atppc_softc *)psc;
	int error;

#define BUFSIZE	PAGE_SIZE	/* XXX see lptvar.h */
	if ((error = bus_dmamap_create(sc->sc_dmat, BUFSIZE, 1, BUFSIZE, 0,
	    BUS_DMA_NOWAIT, &psc->sc_dmamap)))
		return error;

	return (0);
#endif
}

/* Start DMA operation over PCI bus */
static int
atppc_puc_dma_start(struct atppc_softc *sc, void *buf, u_int nbytes,
	u_int8_t mode)
{
	struct atppc_puc_softc *psc = (struct atppc_puc_softc *)sc;

	bus_dmamap_sync(sc->sc_dmat, psc->sc_dmamap, 0, nbytes,
	    (mode == ATPPC_DMA_MODE_WRITE) ? BUS_DMASYNC_PREWRITE
			: BUS_DMASYNC_PREREAD);

	return (0);
}

/* Stop DMA operation over PCI bus */
static int
atppc_puc_dma_finish(struct atppc_softc *sc)
{

	struct atppc_puc_softc *psc = (struct atppc_puc_softc *)sc;

	/*
	 * We don't know direction of DMA, so sync both. We can safely
	 *  assume the dma map is loaded.
	 */
	bus_dmamap_sync(sc->sc_dmat, psc->sc_dmamap, 0,
	    psc->sc_dmamap->dm_segs[0].ds_len,
	    BUS_DMASYNC_POSTWRITE|BUS_DMASYNC_POSTREAD);

	return (0);
}

/* Abort DMA operation over PCI bus */
int
atppc_puc_dma_abort(struct atppc_softc * lsc)
{

	/* Nothing to do - we do not need to sync, op is aborted */
	return (0);
}

/* Allocate memory for DMA over PCI bus */
int
atppc_puc_dma_malloc(device_t dev, void **buf, bus_addr_t *bus_addr,
	bus_size_t size)
{
	struct atppc_puc_softc *psc = device_private(dev);
	struct atppc_softc *sc = &psc->sc_atppc;
	int error;

	error = bus_dmamap_load(sc->sc_dmat, psc->sc_dmamap, *buf, size,
	    NULL /* kernel address */, BUS_DMA_WAITOK|BUS_DMA_STREAMING);
	if (error)
		return (error);

	*bus_addr = psc->sc_dmamap->dm_segs[0].ds_addr;
	return (0);
}

/* Free memory allocated by atppc_isa_dma_malloc() */
void
atppc_puc_dma_free(device_t dev, void **buf, bus_addr_t *bus_addr,
	bus_size_t size)
{
	struct atppc_puc_softc *psc = device_private(dev);
	struct atppc_softc *sc = &psc->sc_atppc;

	return (bus_dmamap_unload(sc->sc_dmat, psc->sc_dmamap));
}

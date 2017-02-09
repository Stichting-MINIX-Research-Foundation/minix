/* $NetBSD: atppc_isa.c,v 1.15 2011/05/26 02:29:23 jakllsch Exp $ */

/*-
 * Copyright (c) 2001 Alcove - Nicolas Souchu
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * FreeBSD: src/sys/isa/ppc.c,v 1.26.2.5 2001/10/02 05:21:45 nsouch Exp
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: atppc_isa.c,v 1.15 2011/05/26 02:29:23 jakllsch Exp $");

#include "opt_atppc.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <sys/intr.h>
#include <sys/bus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/atppcreg.h>
#include <dev/ic/atppcvar.h>
#include <dev/isa/atppc_isadma.h>

/*
 * ISA bus attach code for atppc driver.
 * Note on capabilities: capabilites may exist in the chipset but may not
 * necessarily be useable. I.e. you may specify an IRQ in the autoconfig, but
 * will the port actually have an IRQ assigned to it at the hardware level?
 * How can you test if the capabilites can be used? For interrupts, see if a
 * handler exists (sc_intr != NULL). For DMA, see if the sc_dma_start() and
 * sc_dma_finish() function pointers are not NULL.
 */

/* Configuration data for atppc on isa bus. */
struct atppc_isa_softc {
	/* Machine independent device data */
	struct atppc_softc sc_atppc;

	/* IRQ/DRQ/IO Port assignments on ISA bus */
	int sc_irq;
	int sc_drq;
	int sc_iobase;

	/* ISA chipset tag */
	isa_chipset_tag_t sc_ic;
};

/* Probe and attach functions for a atppc device on the ISA bus. */
static int atppc_isa_probe(device_t, cfdata_t, void *);
static void atppc_isa_attach(device_t, device_t, void *);

static int atppc_isa_dma_start(struct atppc_softc *, void *, u_int,
	u_int8_t);
static int atppc_isa_dma_finish(struct atppc_softc *);
static int atppc_isa_dma_abort(struct atppc_softc *);
static int atppc_isa_dma_malloc(device_t, void **, bus_addr_t *,
	bus_size_t);
static void atppc_isa_dma_free(device_t, void **, bus_addr_t *,
	bus_size_t);

CFATTACH_DECL_NEW(atppc_isa, sizeof(struct atppc_isa_softc), atppc_isa_probe,
	atppc_isa_attach, NULL, NULL);

/*
 * Probe function: find parallel port controller on isa bus. Combined from
 * lpt_isa_probe() in lpt.c and atppc_detect_port() from FreeBSD's ppc.c.
 */
static int
atppc_isa_probe(device_t parent, cfdata_t cf, void *aux)
{
	bus_space_handle_t ioh;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	int rval = 0;

	if (ia->ia_nio < 1)
		return (0);

	/* Disallow wildcarded i/o address */
	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return (0);

	if (bus_space_map(iot, ia->ia_io[0].ir_addr, IO_LPTSIZE, 0, &ioh))
		return (0);

	if (atppc_detect_port(iot, ioh) == 0)
		rval = 1;

	bus_space_unmap(iot, ioh, IO_LPTSIZE);

	if (rval) {
		ia->ia_nio = 1;
		ia->ia_io[0].ir_size = IO_LPTSIZE;
		ia->ia_nirq = 1;
		ia->ia_ndrq = 1;
		ia->ia_niomem = 0;
	}
	return (rval);
}

/* Attach function: attach and configure parallel port controller on isa bus. */
static void
atppc_isa_attach(device_t parent, device_t self, void *aux)
{
	struct atppc_isa_softc *sc = device_private(self);
	struct atppc_softc *lsc = &sc->sc_atppc;
	struct isa_attach_args *ia = aux;

	printf(": AT Parallel Port\n");

	lsc->sc_iot = ia->ia_iot;
	lsc->sc_dmat = ia->ia_dmat;
	lsc->sc_has = 0;
	sc->sc_ic = ia->ia_ic;
	sc->sc_iobase = ia->ia_io->ir_addr;

	if (bus_space_map(lsc->sc_iot, sc->sc_iobase, IO_LPTSIZE, 0,
		&lsc->sc_ioh) != 0) {
		aprint_error_dev(self, "attempt to map bus space failed, device not "
			"properly attached.\n");
		lsc->sc_dev_ok = ATPPC_NOATTACH;
		return;
	}

	lsc->sc_dev = self;
	lsc->sc_dev_ok = ATPPC_ATTACHED;

	/* Assign interrupt handler */
	if (!(device_cfdata(self)->cf_flags & ATPPC_FLAG_DISABLE_INTR)
	   && ia->ia_irq->ir_irq != ISA_UNKNOWN_IRQ
	   && ia->ia_nirq >= 1) {
		sc->sc_irq = ia->ia_irq[0].ir_irq;
	} else
		sc->sc_irq = -1;

	if (sc->sc_irq > 0) {
		/* Establish interrupt handler. */
		lsc->sc_ieh = isa_intr_establish(sc->sc_ic, sc->sc_irq,
			IST_EDGE, IPL_TTY, atppcintr, lsc->sc_dev);

		lsc->sc_has |= ATPPC_HAS_INTR;
	}

	/* Configure DMA */
	if (!(device_cfdata(self)->cf_flags & ATPPC_FLAG_DISABLE_DMA)
	    && ia->ia_drq->ir_drq != ISA_UNKNOWN_DRQ
	    && ia->ia_ndrq >= 1)
		sc->sc_drq = ia->ia_drq[0].ir_drq;
	else
		sc->sc_drq = -1;

	if (sc->sc_drq != -1
	    && atppc_isadma_setup(lsc, sc->sc_ic, sc->sc_drq) == 0) {
		lsc->sc_has |= ATPPC_HAS_DMA;

		/* setup DMA hooks */
		lsc->sc_dma_start = atppc_isa_dma_start;
		lsc->sc_dma_finish = atppc_isa_dma_finish;
		lsc->sc_dma_abort = atppc_isa_dma_abort;
		lsc->sc_dma_malloc = atppc_isa_dma_malloc;
		lsc->sc_dma_free = atppc_isa_dma_free;
	}

	/* Run soft configuration attach */
	atppc_sc_attach(lsc);

	return;
}

/* Start DMA operation over ISA bus */
static int
atppc_isa_dma_start(struct atppc_softc *lsc, void *buf, u_int nbytes,
	u_int8_t mode)
{
	struct atppc_isa_softc *sc = (struct atppc_isa_softc *) lsc;

	return atppc_isadma_start(sc->sc_ic, sc->sc_drq, buf, nbytes, mode);
}

/* Stop DMA operation over ISA bus */
static int
atppc_isa_dma_finish(struct atppc_softc *lsc)
{
	struct atppc_isa_softc *sc = (struct atppc_isa_softc *) lsc;

	return atppc_isadma_finish(sc->sc_ic, sc->sc_drq);
}

/* Abort DMA operation over ISA bus */
static int
atppc_isa_dma_abort(struct atppc_softc *lsc)
{
	struct atppc_isa_softc *sc = (struct atppc_isa_softc *) lsc;

	return atppc_isadma_abort(sc->sc_ic, sc->sc_drq);
}

/* Allocate memory for DMA over ISA bus */
static int
atppc_isa_dma_malloc(device_t dev, void **buf, bus_addr_t *bus_addr,
	bus_size_t size)
{
	struct atppc_isa_softc *sc = device_private(dev);

	return atppc_isadma_malloc(sc->sc_ic, sc->sc_drq, buf, bus_addr, size);
}

/* Free memory allocated by atppc_isa_dma_malloc() */
static void
atppc_isa_dma_free(device_t dev, void **buf, bus_addr_t *bus_addr,
	bus_size_t size)
{
	struct atppc_isa_softc *sc = device_private(dev);

	return atppc_isadma_free(sc->sc_ic, sc->sc_drq, buf, bus_addr, size);
}

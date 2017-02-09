/* $NetBSD: atppc_isapnp.c,v 1.11 2008/04/28 20:23:52 martin Exp $ */

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: atppc_isapnp.c,v 1.11 2008/04/28 20:23:52 martin Exp $");

#include "opt_atppc.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/termios.h>

#include <sys/bus.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/isapnp/isapnpreg.h>
#include <dev/isapnp/isapnpvar.h>
#include <dev/isapnp/isapnpdevs.h>

#include <dev/ic/atppcvar.h>
#include <dev/isa/atppc_isadma.h>

static int	atppc_isapnp_match(device_t, cfdata_t, void *);
static void	atppc_isapnp_attach(device_t, device_t, void *);

struct atppc_isapnp_softc {
	struct atppc_softc sc_atppc;

	isa_chipset_tag_t sc_ic;
	int sc_drq;
};

CFATTACH_DECL_NEW(atppc_isapnp, sizeof(struct atppc_isapnp_softc),
    atppc_isapnp_match, atppc_isapnp_attach, NULL, NULL);

static int atppc_isapnp_dma_start(struct atppc_softc *, void *, u_int,
	u_int8_t);
static int atppc_isapnp_dma_finish(struct atppc_softc *);
static int atppc_isapnp_dma_abort(struct atppc_softc *);
static int atppc_isapnp_dma_malloc(device_t, void **, bus_addr_t *,
	bus_size_t);
static void atppc_isapnp_dma_free(device_t, void **, bus_addr_t *,
	bus_size_t);
/*
 * atppc_isapnp_match: autoconf(9) match routine
 */
static int
atppc_isapnp_match(device_t parent, cfdata_t match, void *aux)
{
	int pri, variant;

	pri = isapnp_devmatch(aux, &isapnp_atppc_devinfo, &variant);
	if (pri && variant > 0)
		pri = 0;
	return (pri);
}

static void
atppc_isapnp_attach(device_t parent, device_t self, void *aux)
{
	struct atppc_softc *sc = device_private(self);
	struct atppc_isapnp_softc *asc = device_private(self);
	struct isapnp_attach_args *ipa = aux;

	sc->sc_dev_ok = ATPPC_NOATTACH;

	printf(": AT Parallel Port\n");

	if (isapnp_config(ipa->ipa_iot, ipa->ipa_memt, ipa)) {
		aprint_error_dev(sc->sc_dev, "error in region allocation\n");
		return;
	}

	/* Attach */
	sc->sc_dev = self;
	sc->sc_iot = ipa->ipa_iot;
	sc->sc_ioh = ipa->ipa_io[0].h;
	sc->sc_has = 0;
	asc->sc_ic = ipa->ipa_ic;
	asc->sc_drq = -1;	/* Initialized below */

	sc->sc_dev_ok = ATPPC_ATTACHED;

	sc->sc_ieh = isa_intr_establish(ipa->ipa_ic, ipa->ipa_irq[0].num,
	    ipa->ipa_irq[0].type, IPL_TTY, atppcintr, sc);
	sc->sc_has |= ATPPC_HAS_INTR;

	/* setup DMA hooks */
	if (ipa->ipa_ndrq > 0
	    && atppc_isadma_setup(sc, asc->sc_ic, ipa->ipa_drq[0].num) == 0) {
		asc->sc_drq = ipa->ipa_drq[0].num;
		sc->sc_has |= ATPPC_HAS_DMA;
		sc->sc_dma_start = atppc_isapnp_dma_start;
		sc->sc_dma_finish = atppc_isapnp_dma_finish;
		sc->sc_dma_abort = atppc_isapnp_dma_abort;
		sc->sc_dma_malloc = atppc_isapnp_dma_malloc;
		sc->sc_dma_free = atppc_isapnp_dma_free;
	}

	/* Run soft configuration attach */
	atppc_sc_attach(sc);
}

/* Start DMA operation over ISA bus */
static int
atppc_isapnp_dma_start(struct atppc_softc *lsc, void *buf, u_int nbytes,
	u_int8_t mode)
{
	struct atppc_isapnp_softc * sc = (struct atppc_isapnp_softc *) lsc;

	return atppc_isadma_start(sc->sc_ic, sc->sc_drq, buf, nbytes, mode);
}

/* Stop DMA operation over ISA bus */
static int
atppc_isapnp_dma_finish(struct atppc_softc * lsc)
{
	struct atppc_isapnp_softc * sc = (struct atppc_isapnp_softc *) lsc;

	return atppc_isadma_finish(sc->sc_ic, sc->sc_drq);
}

/* Abort DMA operation over ISA bus */
int
atppc_isapnp_dma_abort(struct atppc_softc * lsc)
{
	struct atppc_isapnp_softc * sc = (struct atppc_isapnp_softc *) lsc;

	return atppc_isadma_abort(sc->sc_ic, sc->sc_drq);
}

/* Allocate memory for DMA over ISA bus */
int
atppc_isapnp_dma_malloc(device_t dev, void ** buf, bus_addr_t * bus_addr,
	bus_size_t size)
{
	struct atppc_isapnp_softc * sc = device_private(dev);

	return atppc_isadma_malloc(sc->sc_ic, sc->sc_drq, buf, bus_addr, size);
}

/* Free memory allocated by atppc_isa_dma_malloc() */
void
atppc_isapnp_dma_free(device_t dev, void ** buf, bus_addr_t * bus_addr,
	bus_size_t size)
{
	struct atppc_isapnp_softc * sc = device_private(dev);

	return atppc_isadma_free(sc->sc_ic, sc->sc_drq, buf, bus_addr, size);
}

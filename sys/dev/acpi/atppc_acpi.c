/* $NetBSD: atppc_acpi.c,v 1.17 2010/03/05 14:00:17 jruoho Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: atppc_acpi.c,v 1.17 2010/03/05 14:00:17 jruoho Exp $");

#include "opt_atppc.h"

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/termios.h>

#include <dev/acpi/acpivar.h>

#include <dev/ic/atppcvar.h>

#include <dev/isa/isadmavar.h>
#include <dev/isa/atppc_isadma.h>

static int	atppc_acpi_match(device_t, cfdata_t, void *);
static void	atppc_acpi_attach(device_t, device_t, void *);

struct atppc_acpi_softc {
	struct atppc_softc sc_atppc;

	isa_chipset_tag_t sc_ic;
	int sc_drq;
};

CFATTACH_DECL_NEW(atppc_acpi, sizeof(struct atppc_acpi_softc), atppc_acpi_match,
    atppc_acpi_attach, NULL, NULL);

/*
 * Supported device IDs
 */

static const char * const atppc_acpi_ids[] = {
	"PNP04??",	/* Standard AT printer port */
	NULL
};

static int atppc_acpi_dma_start(struct atppc_softc *, void *, u_int,
	u_int8_t);
static int atppc_acpi_dma_finish(struct atppc_softc *);
static int atppc_acpi_dma_abort(struct atppc_softc *);
static int atppc_acpi_dma_malloc(device_t, void **, bus_addr_t *,
	bus_size_t);
static void atppc_acpi_dma_free(device_t, void **, bus_addr_t *,
	bus_size_t);
/*
 * atppc_acpi_match: autoconf(9) match routine
 */
static int
atppc_acpi_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	return acpi_match_hid(aa->aa_node->ad_devinfo, atppc_acpi_ids);
}

static void
atppc_acpi_attach(device_t parent, device_t self, void *aux)
{
	struct atppc_softc *sc = device_private(self);
	struct atppc_acpi_softc *asc = device_private(self);
	struct acpi_attach_args *aa = aux;
	struct acpi_resources res;
	struct acpi_io *io;
	struct acpi_irq *irq;
	struct acpi_drq *drq;
	ACPI_STATUS rv;
	int nirq;

	sc->sc_dev_ok = ATPPC_NOATTACH;

	sc->sc_dev = self;

	/* parse resources */
	rv = acpi_resource_parse(sc->sc_dev, aa->aa_node->ad_handle, "_CRS",
				 &res, &acpi_resource_parse_ops_default);
	if (ACPI_FAILURE(rv))
		return;

	/* find our i/o registers */
	io = acpi_res_io(&res, 0);
	if (io == NULL) {
		aprint_error_dev(sc->sc_dev, "unable to find i/o register resource\n");
		goto out;
	}

	/* find our IRQ */
	irq = acpi_res_irq(&res, 0);
	if (irq == NULL) {
		aprint_error_dev(sc->sc_dev, "unable to find irq resource\n");
		goto out;
	}
	nirq = irq->ar_irq;

	/* find our DRQ */
	drq = acpi_res_drq(&res, 0);
	if (drq == NULL) {
		aprint_error_dev(sc->sc_dev, "unable to find drq resource\n");
		goto out;
	}
	asc->sc_drq = drq->ar_drq;

	/* Attach */
	sc->sc_iot = aa->aa_iot;
	sc->sc_has = 0;
	asc->sc_ic = aa->aa_ic;

	sc->sc_dev_ok = ATPPC_ATTACHED;

	if (bus_space_map(sc->sc_iot, io->ar_base, io->ar_length, 0,
		&sc->sc_ioh) != 0) {
		aprint_error_dev(self, "attempt to map bus space failed, device not "
			"properly attached.\n");
		goto out;
	}

	sc->sc_ieh = isa_intr_establish(aa->aa_ic, nirq,
	    (irq->ar_type == ACPI_EDGE_SENSITIVE) ? IST_EDGE : IST_LEVEL,
	    IPL_TTY, atppcintr, sc->sc_dev);

	/* setup DMA hooks */
	if (atppc_isadma_setup(sc, asc->sc_ic, asc->sc_drq) == 0) {
		sc->sc_has |= ATPPC_HAS_DMA;
		sc->sc_dma_start = atppc_acpi_dma_start;
		sc->sc_dma_finish = atppc_acpi_dma_finish;
		sc->sc_dma_abort = atppc_acpi_dma_abort;
		sc->sc_dma_malloc = atppc_acpi_dma_malloc;
		sc->sc_dma_free = atppc_acpi_dma_free;
	}

	sc->sc_has |= ATPPC_HAS_INTR;

	/* Run soft configuration attach */
	atppc_sc_attach(sc);
 out:
	acpi_resource_cleanup(&res);
}

/* Start DMA operation over ISA bus */
static int
atppc_acpi_dma_start(struct atppc_softc *lsc, void *buf, u_int nbytes,
	u_int8_t mode)
{
	struct atppc_acpi_softc * sc = (struct atppc_acpi_softc *) lsc;

	return atppc_isadma_start(sc->sc_ic, sc->sc_drq, buf, nbytes, mode);
}

/* Stop DMA operation over ISA bus */
static int
atppc_acpi_dma_finish(struct atppc_softc * lsc)
{
	struct atppc_acpi_softc * sc = (struct atppc_acpi_softc *) lsc;

	return atppc_isadma_finish(sc->sc_ic, sc->sc_drq);
}

/* Abort DMA operation over ISA bus */
int
atppc_acpi_dma_abort(struct atppc_softc * lsc)
{
	struct atppc_acpi_softc * sc = (struct atppc_acpi_softc *) lsc;

	return atppc_isadma_abort(sc->sc_ic, sc->sc_drq);
}

/* Allocate memory for DMA over ISA bus */
static int
atppc_acpi_dma_malloc(device_t dev, void ** buf, bus_addr_t * bus_addr,
	bus_size_t size)
{
	struct atppc_acpi_softc * sc = device_private(dev);

	return atppc_isadma_malloc(sc->sc_ic, sc->sc_drq, buf, bus_addr, size);
}

/* Free memory allocated by atppc_isa_dma_malloc() */
static void
atppc_acpi_dma_free(device_t dev, void ** buf, bus_addr_t * bus_addr,
	bus_size_t size)
{
	struct atppc_acpi_softc * sc = device_private(dev);

	return atppc_isadma_free(sc->sc_ic, sc->sc_drq, buf, bus_addr, size);
}

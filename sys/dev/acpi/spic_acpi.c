/*	$NetBSD: spic_acpi.c,v 1.6 2010/03/05 14:00:17 jruoho Exp $	*/

/*
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net).
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
__KERNEL_RCSID(0, "$NetBSD: spic_acpi.c,v 1.6 2010/03/05 14:00:17 jruoho Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#include <dev/ic/spicvar.h>

#define _COMPONENT		ACPI_RESOURCE_COMPONENT
ACPI_MODULE_NAME		("spic_acpi")

struct spic_acpi_softc {
	struct spic_softc sc_spic;	/* spic device */

	struct acpi_devnode *sc_node;	/* our ACPI devnode */

	void *sc_ih;
};

static const char * const spic_acpi_ids[] = {
	"SNY6001",
	NULL
};

static int	spic_acpi_match(device_t, cfdata_t, void *);
static void	spic_acpi_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(spic_acpi, sizeof(struct spic_acpi_softc),
    spic_acpi_match, spic_acpi_attach, NULL, NULL);


static int
spic_acpi_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return (0);

	return (acpi_match_hid(aa->aa_node->ad_devinfo, spic_acpi_ids));
}

static void
spic_acpi_attach(device_t parent, device_t self, void *aux)
{
	struct spic_acpi_softc *sc = device_private(self);
	struct acpi_attach_args *aa = aux;
	struct acpi_io *io;
	struct acpi_irq *irq;
	struct acpi_resources res;

	ACPI_STATUS rv;

	sc->sc_spic.sc_dev = self;
	sc->sc_node = aa->aa_node;

	/* Parse our resources. */
	rv = acpi_resource_parse(self, sc->sc_node->ad_handle,
	    "_CRS", &res, &acpi_resource_parse_ops_default);
	if (ACPI_FAILURE(rv))
		return;

	sc->sc_spic.sc_iot = aa->aa_iot;
	io = acpi_res_io(&res, 0);
	if (io == NULL) {
		aprint_error_dev(self, "unable to find io resource\n");
		goto out;
	}
	if (bus_space_map(sc->sc_spic.sc_iot, io->ar_base, io->ar_length,
	    0, &sc->sc_spic.sc_ioh) != 0) {
		aprint_error_dev(self, "unable to map data register\n");
		goto out;
	}
	irq = acpi_res_irq(&res, 0);
	if (irq == NULL) {
		aprint_error_dev(self, "unable to find irq resource\n");
		/* XXX unmap */
		goto out;
	}
#if 0
	sc->sc_ih = isa_intr_establish(NULL, irq->ar_irq,
	    IST_EDGE, IPL_TTY, spic_intr, sc);
#endif

	if (!pmf_device_register(self, spic_suspend, spic_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");
	else
		pmf_class_input_register(self);

	spic_attach(&sc->sc_spic);
 out:
	acpi_resource_cleanup(&res);
}

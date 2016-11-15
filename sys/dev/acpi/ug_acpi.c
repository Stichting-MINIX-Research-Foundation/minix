/* $NetBSD: ug_acpi.c,v 1.6 2010/03/05 14:00:17 jruoho Exp $ */

/*
 * Copyright (c) 2007 Mihai Chelaru <kefren@netbsd.ro>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ug_acpi.c,v 1.6 2010/03/05 14:00:17 jruoho Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <dev/acpi/acpivar.h>

#include <dev/ic/ugreg.h>
#include <dev/ic/ugvar.h>

/* autoconf(9) functions */
static int	ug_acpi_match(device_t, cfdata_t, void *);
static void	ug_acpi_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(ug_acpi, sizeof(struct ug_softc), ug_acpi_match,
    ug_acpi_attach, NULL, NULL);

/*
 * Supported devices
 * XXX: only uGuru 2005 for now
 */

static const char* const ug_acpi_ids[] = {
	"ABT2005",	/* uGuru 2005 */
	NULL
};

static int
ug_acpi_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	return acpi_match_hid(aa->aa_node->ad_devinfo, ug_acpi_ids);
}

static void
ug_acpi_attach(device_t parent, device_t self, void *aux)
{
	struct ug_softc *sc = device_private(self);
	struct acpi_attach_args *aa = aux;
	struct acpi_resources res;
	struct acpi_io *io;
	bus_space_handle_t ioh;
	ACPI_STATUS rv;

	/* parse resources */
	rv = acpi_resource_parse(self, aa->aa_node->ad_handle, "_CRS",
	    &res, &acpi_resource_parse_ops_default);
	if (ACPI_FAILURE(rv))
		return;

	/* find our i/o registers */
	io = acpi_res_io(&res, 0);
	if (io == NULL) {
		aprint_error_dev(self,
		    "unable to find i/o register resource\n");
		acpi_resource_cleanup(&res);
		return;
	}

	if (bus_space_map(aa->aa_iot, io->ar_base, io->ar_length,
	    0, &ioh)) {
		aprint_error_dev(self, "can't map i/o space\n");
		acpi_resource_cleanup(&res);
		return;
	}

	aprint_normal("%s", device_xname(self));

	sc->version = 2;	/* uGuru 2005 */
	sc->sc_ioh = ioh;
	sc->sc_iot = aa->aa_iot;
	ug2_attach(self);

	acpi_resource_cleanup(&res);
}

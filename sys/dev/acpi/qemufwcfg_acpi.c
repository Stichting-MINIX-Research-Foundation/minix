/* $NetBSD: qemufwcfg_acpi.c,v 1.1 2017/11/25 16:31:03 jmcneill Exp $ */

/*-
 * Copyright (c) 2017 Jared McNeill <jmcneill@invisible.ca>
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
__KERNEL_RCSID(0, "$NetBSD: qemufwcfg_acpi.c,v 1.1 2017/11/25 16:31:03 jmcneill Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/mutex.h>
#include <sys/bus.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#include <dev/ic/qemufwcfgvar.h>

#define _COMPONENT	ACPI_RESOURCE_COMPONENT
ACPI_MODULE_NAME	("qemufwcfg_acpi")

static int	fwcfg_acpi_match(device_t, cfdata_t, void *);
static void	fwcfg_acpi_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(qemufwcfg_acpi, sizeof(struct fwcfg_softc),
    fwcfg_acpi_match,
    fwcfg_acpi_attach,
    NULL,
    NULL
);

static const char * const fwcfg_acpi_ids[] = {
	"QEMU0002",
	NULL
};

static int
fwcfg_acpi_match(device_t parent, cfdata_t match, void *opaque)
{
	struct acpi_attach_args *aa = opaque;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	return acpi_match_hid(aa->aa_node->ad_devinfo, fwcfg_acpi_ids);
}

static void
fwcfg_acpi_attach(device_t parent, device_t self, void *opaque)
{
	struct fwcfg_softc *sc = device_private(self);
	struct acpi_attach_args *aa = opaque;
	struct acpi_resources res;
	struct acpi_mem *mem;
	struct acpi_io *io;
	bus_addr_t base;
	bus_size_t size;
	ACPI_STATUS rv;

	rv = acpi_resource_parse(self, aa->aa_node->ad_handle, "_CRS",
	    &res, &acpi_resource_parse_ops_default);
	if (ACPI_FAILURE(rv))
		return;

	sc->sc_dev = self;

	if ((io = acpi_res_io(&res, 0)) != NULL) {
		sc->sc_bst = aa->aa_iot;
		base = io->ar_base;
		size = io->ar_length;
	} else if ((mem = acpi_res_mem(&res, 0)) != NULL) {
		sc->sc_bst = aa->aa_memt;
		base = mem->ar_base;
		size = mem->ar_length;
	} else {
		aprint_error_dev(self, "couldn't acquire resources\n");
		return;
	}
	acpi_resource_cleanup(&res);

	if (bus_space_map(sc->sc_bst, base, size, 0, &sc->sc_bsh) != 0) {
		aprint_error_dev(self, "couldn't map registers\n");
		return;
	}

	fwcfg_attach(sc);

	pmf_device_register(self, NULL, NULL);
}

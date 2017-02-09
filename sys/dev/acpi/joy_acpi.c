/* $NetBSD: joy_acpi.c,v 1.11 2011/11/23 23:07:31 jmcneill Exp $ */

/*
 * Copyright (c) 2002 Jared D. McNeill <jmcneill@invisible.ca>
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

/*
 * ACPI joy(4) attachment based in lpt_acpi.c by Jared D. McNeill.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: joy_acpi.c,v 1.11 2011/11/23 23:07:31 jmcneill Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <dev/acpi/acpivar.h>

#include <dev/ic/joyvar.h>

static int	joy_acpi_match(device_t, cfdata_t, void *);
static void	joy_acpi_attach(device_t, device_t, void *);

struct joy_acpi_softc {
	struct joy_softc sc_joy;
	kmutex_t sc_lock;
};

CFATTACH_DECL_NEW(joy_acpi, sizeof(struct joy_acpi_softc), joy_acpi_match,
    joy_acpi_attach, NULL, NULL);

/*
 * Supported device IDs
 */

static const char * const joy_acpi_ids[] = {
	"PNPB02F",	/* Joystick/Game port */
	NULL
};

/*
 * joy_acpi_match: autoconf(9) match routine
 */
static int
joy_acpi_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	return acpi_match_hid(aa->aa_node->ad_devinfo, joy_acpi_ids);
}

/*
 * joy_acpi_attach: autoconf(9) attach routine
 */
static void
joy_acpi_attach(device_t parent, device_t self, void *aux)
{
	struct joy_acpi_softc *asc = device_private(self);
	struct joy_softc *sc = &asc->sc_joy;
	struct acpi_attach_args *aa = aux;
	struct acpi_resources res;
	struct acpi_io *io;
	ACPI_STATUS rv;

	sc->sc_dev = self;

	/* parse resources */
	rv = acpi_resource_parse(sc->sc_dev, aa->aa_node->ad_handle, "_CRS",
	    &res, &acpi_resource_parse_ops_default);
	if (ACPI_FAILURE(rv))
		return;

	/* find our i/o registers */
	io = acpi_res_io(&res, 0);
	if (io == NULL) {
		aprint_error_dev(self,
		    "unable to find i/o register resource\n");
		goto out;
	}

	sc->sc_iot = aa->aa_iot;
	if (bus_space_map(sc->sc_iot, io->ar_base, io->ar_length,
		    0, &sc->sc_ioh)) {
		aprint_error_dev(self, "can't map i/o space\n");
		goto out;
	}

	mutex_init(&asc->sc_lock, MUTEX_DEFAULT, IPL_NONE);
	sc->sc_lock = &asc->sc_lock;

	joyattach(sc);

 out:
	acpi_resource_cleanup(&res);
}

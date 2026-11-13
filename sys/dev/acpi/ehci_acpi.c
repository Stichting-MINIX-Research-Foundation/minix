/* $NetBSD: ehci_acpi.c,v 1.3 2018/11/16 23:18:17 jmcneill Exp $ */

/*-
 * Copyright (c) 2018 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jared McNeill <jmcneill@invisible.ca>.
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
__KERNEL_RCSID(0, "$NetBSD: ehci_acpi.c,v 1.3 2018/11/16 23:18:17 jmcneill Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/device.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>
#include <dev/usb/ehcireg.h>
#include <dev/usb/ehcivar.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpi_intr.h>
#include <dev/acpi/acpi_usb.h>

static const char * const compatible[] = {
	"PNP0D20",	/* EHCI-compliant USB controller without standard debug */
	"PNP0D25",	/* EHCI-compliant USB controller with standard debug */
	NULL
};

struct ehci_acpi_softc {
	struct ehci_softc	sc_ehci;
	ACPI_HANDLE		sc_handle;
};

static int	ehci_acpi_match(device_t, cfdata_t, void *);
static void	ehci_acpi_attach(device_t, device_t, void *);

static void	ehci_acpi_init(struct ehci_softc *);

CFATTACH_DECL2_NEW(ehci_acpi, sizeof(struct ehci_acpi_softc),
	ehci_acpi_match, ehci_acpi_attach, NULL,
	ehci_activate, NULL, ehci_childdet);

static int
ehci_acpi_match(device_t parent, cfdata_t cf, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	return acpi_match_hid(aa->aa_node->ad_devinfo, compatible);
}

static void
ehci_acpi_attach(device_t parent, device_t self, void *aux)
{
	struct ehci_acpi_softc * const asc = device_private(self);
	struct ehci_softc * const sc = &asc->sc_ehci;
	struct acpi_attach_args *aa = aux;
	struct acpi_resources res;
	struct acpi_mem *mem;
	struct acpi_irq *irq;
	ACPI_STATUS rv;
	int error;
	void *ih;

	asc->sc_handle = aa->aa_node->ad_handle;

	sc->sc_dev = self;
	sc->sc_bus.ub_hcpriv = sc;
	sc->sc_bus.ub_dmatag = aa->aa_dmat;
	sc->sc_bus.ub_revision = USBREV_2_0;
	sc->sc_flags = EHCIF_ETTF;
	sc->sc_vendor_init = ehci_acpi_init;

	rv = acpi_resource_parse(sc->sc_dev, asc->sc_handle, "_CRS",
	    &res, &acpi_resource_parse_ops_default);
	if (ACPI_FAILURE(rv))
		return;

	mem = acpi_res_mem(&res, 0);
	if (mem == NULL) {
		aprint_error_dev(self, "couldn't find mem resource\n");
		goto done;
	}

	irq = acpi_res_irq(&res, 0);
	if (irq == NULL) {
		aprint_error_dev(self, "couldn't find irq resource\n");
		goto done;
	}

	sc->sc_size = mem->ar_length;
	sc->iot = aa->aa_memt;
	error = bus_space_map(sc->iot, mem->ar_base, mem->ar_length, 0, &sc->ioh);
	if (error) {
		aprint_error_dev(self, "couldn't map registers\n");
		return;
	}

	/* Disable interrupts */
	sc->sc_offs = EREAD1(sc, EHCI_CAPLENGTH);
	EOWRITE4(sc, EHCI_USBINTR, 0);

	ih = acpi_intr_establish(self, (uint64_t)aa->aa_node->ad_handle,
	    IPL_USB, true, ehci_intr, sc, device_xname(self));
	if (ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt\n");
		return;
	}

	error = ehci_init(sc);
	if (error) {
		aprint_error_dev(self, "init failed, error = %d\n", error);
		return;
	}

	sc->sc_child = config_found(self, &sc->sc_bus, usbctlprint);

done:
	acpi_resource_cleanup(&res);
}

static void
ehci_acpi_init(struct ehci_softc *sc)
{
	struct ehci_acpi_softc * const asc = (struct ehci_acpi_softc *)sc;

	acpi_usb_post_reset(asc->sc_handle);
}

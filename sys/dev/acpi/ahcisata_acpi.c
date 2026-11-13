/* $NetBSD: ahcisata_acpi.c,v 1.4 2018/11/16 23:18:17 jmcneill Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: ahcisata_acpi.c,v 1.4 2018/11/16 23:18:17 jmcneill Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/device.h>
#include <sys/termios.h>

#include <dev/ata/atavar.h>
#include <dev/ic/ahcisatavar.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpi_intr.h>

#include <dev/pci/pcireg.h>

static int	ahcisata_acpi_match(device_t, cfdata_t, void *);
static void	ahcisata_acpi_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(ahcisata_acpi, sizeof(struct ahci_softc), ahcisata_acpi_match, ahcisata_acpi_attach, NULL, NULL);

static int
ahcisata_acpi_match(device_t parent, cfdata_t cf, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	return acpi_match_class(aa->aa_node->ad_handle,
	    PCI_CLASS_MASS_STORAGE,
	    PCI_SUBCLASS_MASS_STORAGE_SATA,
	    PCI_INTERFACE_SATA_AHCI10);
}

static void
ahcisata_acpi_attach(device_t parent, device_t self, void *aux)
{
	struct ahci_softc * const sc = device_private(self);
	struct acpi_attach_args *aa = aux;
	struct acpi_resources res;
	struct acpi_mem *mem;
	struct acpi_irq *irq;
	ACPI_STATUS rv;
	void *ih;

	rv = acpi_resource_parse(self, aa->aa_node->ad_handle, "_CRS",
	    &res, &acpi_resource_parse_ops_default);
	if (ACPI_FAILURE(rv))
		return;

	mem = acpi_res_mem(&res, 0);
	if (mem == NULL) {
		aprint_error(": couldn't find mem resource\n");
		goto done;
	}

	irq = acpi_res_irq(&res, 0);
	if (irq == NULL) {
		aprint_error(": couldn't find irq resource\n");
		goto done;
	}

	sc->sc_atac.atac_dev = self;
	sc->sc_dmat = aa->aa_dmat;
	sc->sc_ahcit = aa->aa_memt;
	sc->sc_ahcis = mem->ar_length;
	if (bus_space_map(aa->aa_memt, mem->ar_base, mem->ar_length, 0, &sc->sc_ahcih) != 0) {
		aprint_error(": couldn't map registers\n");
		goto done;
	}

	ih = acpi_intr_establish(self, (uint64_t)aa->aa_node->ad_handle,
	    IPL_BIO, false, ahci_intr, sc, device_xname(self));
	if (ih == NULL) {
		aprint_error_dev(self, "couldn't install interrupt handler\n");
		return;
	}

	ahci_attach(sc);

done:
	acpi_resource_cleanup(&res);
}

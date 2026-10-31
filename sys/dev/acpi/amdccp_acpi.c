/* $NetBSD: amdccp_acpi.c,v 1.2 2018/10/21 11:09:20 jmcneill Exp $ */

/*
 * Copyright (c) 2018 Jonathan A. Kollasch
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: amdccp_acpi.c,v 1.2 2018/10/21 11:09:20 jmcneill Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/device.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#include <dev/ic/amdccpvar.h>

static int	amdccp_acpi_match(device_t, cfdata_t, void *);
static void	amdccp_acpi_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(amdccp_acpi, sizeof(struct amdccp_softc),
    amdccp_acpi_match, amdccp_acpi_attach, NULL, NULL);

static const char * const compatible[] = {
	"AMDI0C00",
	NULL
};

static int
amdccp_acpi_match(device_t parent, cfdata_t cf, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	return acpi_match_hid(aa->aa_node->ad_devinfo, compatible);
}

static void
amdccp_acpi_attach(device_t parent, device_t self, void *aux)
{
	struct amdccp_softc * const sc = device_private(self);
	struct acpi_attach_args *aa = aux;
	struct acpi_resources res;
	struct acpi_mem *mem;
#if notyet
	struct acpi_irq *irq;
#endif
	ACPI_STATUS rv;
#if notyet
	void *ih;
#endif

	sc->sc_dev = self;

	rv = acpi_resource_parse(sc->sc_dev, aa->aa_node->ad_handle, "_CRS",
	    &res, &acpi_resource_parse_ops_default);
	if (ACPI_FAILURE(rv))
		return;

	mem = acpi_res_mem(&res, 0);
	if (mem == NULL) {
		aprint_error_dev(self, "couldn't find mem resource\n");
		goto done;
	}

#if notyet
	irq = acpi_res_irq(&res, 0);
	if (irq == NULL) {
		aprint_error_dev(self, "couldn't find irq resource\n");
		goto done;
	}

#endif
	sc->sc_bst = aa->aa_memt;
	if (bus_space_map(aa->aa_memt, mem->ar_base, mem->ar_length, 0, &sc->sc_bsh) != 0) {
		aprint_error_dev(self, "couldn't map registers\n");
		goto done;
	}

#if notyet
	const int type = (irq->ar_type == ACPI_EDGE_SENSITIVE) ? IST_EDGE : IST_LEVEL;
	ih = intr_establish(irq->ar_irq, IPL_VM, type | IST_MPSAFE, amdccp_intr, sc);
	if (ih == NULL) {
		aprint_error_dev(self, "couldn't install interrupt handler\n");
		return;
	}

#endif

	amdccp_common_attach(sc);

done:
	acpi_resource_cleanup(&res);
}

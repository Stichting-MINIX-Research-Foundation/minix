/* $NetBSD: wb_acpi.c,v 1.5 2010/08/19 18:37:38 jmcneill Exp $ */

/*
 * Copyright (c) 2009 Jared D. McNeill <jmcneill@invisible.ca>
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
__KERNEL_RCSID(0, "$NetBSD: wb_acpi.c,v 1.5 2010/08/19 18:37:38 jmcneill Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#include <dev/ic/w83l518dvar.h>
#include <dev/ic/w83l518dreg.h>

#include <dev/isa/isadmavar.h>

#include <dev/sdmmc/sdmmcvar.h>

#define _COMPONENT	ACPI_RESOURCE_COMPONENT
ACPI_MODULE_NAME	("wb_acpi")

static int	wb_acpi_match(device_t, cfdata_t, void *);
static void	wb_acpi_attach(device_t, device_t, void *);
static int	wb_acpi_detach(device_t, int);
static bool	wb_acpi_suspend(device_t, const pmf_qual_t *);
static bool	wb_acpi_resume(device_t, const pmf_qual_t *);

struct wb_acpi_softc {
	struct wb_softc sc_wb;
	isa_chipset_tag_t sc_ic;
	void *sc_ih;
	int sc_ioh_length;

	ACPI_HANDLE sc_crs, sc_srs;
	ACPI_BUFFER sc_crs_buffer;
};

CFATTACH_DECL_NEW(wb_acpi, sizeof(struct wb_acpi_softc),
    wb_acpi_match,
    wb_acpi_attach,
    wb_acpi_detach,
    NULL
);

static const char * const wb_acpi_ids[] = {
#if notyet
	"WEC0515",	/* Memory Stick interface */
#endif
	"WEC0517",	/* SD Memory Card interface */
	NULL
};

static int
wb_acpi_match(device_t parent, cfdata_t match, void *opaque)
{
	struct acpi_attach_args *aa = opaque;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	return acpi_match_hid(aa->aa_node->ad_devinfo, wb_acpi_ids);
}

static void
wb_acpi_attach(device_t parent, device_t self, void *opaque)
{
	struct wb_acpi_softc *sc = device_private(self);
	struct acpi_attach_args *aa = opaque;
	struct acpi_resources res;
	struct acpi_io *io;
	struct acpi_irq *irq;
	bus_space_handle_t ioh;
	ACPI_STATUS rv;

	sc->sc_ic = aa->aa_ic;

	rv = acpi_resource_parse(self, aa->aa_node->ad_handle, "_CRS",
	    &res, &acpi_resource_parse_ops_default);
	if (ACPI_FAILURE(rv))
		return;

	AcpiGetHandle(aa->aa_node->ad_handle, "_CRS", &sc->sc_crs);
	AcpiGetHandle(aa->aa_node->ad_handle, "_SRS", &sc->sc_srs);
	if (sc->sc_crs && sc->sc_srs) {
		sc->sc_crs_buffer.Pointer = NULL;
		sc->sc_crs_buffer.Length = ACPI_ALLOCATE_LOCAL_BUFFER;
		rv = AcpiGetCurrentResources(sc->sc_crs, &sc->sc_crs_buffer);
		if (ACPI_FAILURE(rv))
			sc->sc_crs = sc->sc_srs = NULL;
	}

	io = acpi_res_io(&res, 0);
	irq = acpi_res_irq(&res, 0);
	if (io == NULL || irq == NULL) {
		aprint_error_dev(self, "incomplete resources\n");
		goto cleanup;
	}

	if (bus_space_map(aa->aa_iot, io->ar_base, io->ar_length, 0, &ioh)) {
		aprint_error_dev(self, "couldn't map registers\n");
		goto cleanup;
	}
	sc->sc_ioh_length = io->ar_length;

	sc->sc_ih = isa_intr_establish(sc->sc_ic, irq->ar_irq,
	    (irq->ar_type == ACPI_EDGE_SENSITIVE) ? IST_EDGE : IST_LEVEL,
	    IPL_SDMMC, wb_intr, &sc->sc_wb);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self,
		    "couldn't establish interrupt handler\n");
		goto cleanup;
	}

	sc->sc_wb.wb_dev = self;
	sc->sc_wb.wb_type = WB_DEVNO_SD;
	sc->sc_wb.wb_iot = aa->aa_iot;
	sc->sc_wb.wb_ioh = ioh;
	sc->sc_wb.wb_irq = irq->ar_irq;
	sc->sc_wb.wb_base = io->ar_base;
	wb_attach(&sc->sc_wb);

	pmf_device_register(self, wb_acpi_suspend, wb_acpi_resume);
	
cleanup:
	acpi_resource_cleanup(&res);
}

static int
wb_acpi_detach(device_t self, int flags)
{
	struct wb_acpi_softc *sc = device_private(self);
	int rv;

	pmf_device_deregister(self);

	if (sc->sc_crs_buffer.Pointer)
		ACPI_FREE(sc->sc_crs_buffer.Pointer);

	rv = wb_detach(&sc->sc_wb, flags);
	if (rv)
		return rv;

	if (sc->sc_ih)
		isa_intr_disestablish(sc->sc_ic, sc->sc_ih);

	if (sc->sc_ioh_length > 0)
		bus_space_unmap(sc->sc_wb.wb_iot, sc->sc_wb.wb_ioh,
		    sc->sc_ioh_length);

	return 0;
}

static bool
wb_acpi_suspend(device_t self, const pmf_qual_t *qual)
{
	struct wb_acpi_softc *sc = device_private(self);

	return wb_suspend(&sc->sc_wb);
}

static bool
wb_acpi_resume(device_t self, const pmf_qual_t *qual)
{
	struct wb_acpi_softc *sc = device_private(self);
	ACPI_STATUS rv;

	if (sc->sc_crs && sc->sc_srs) {
		rv = AcpiSetCurrentResources(sc->sc_srs, &sc->sc_crs_buffer);
		if (ACPI_FAILURE(rv))
			printf("%s: _SRS failed: %s\n",
			    device_xname(self), AcpiFormatException(rv));
	}

	return wb_resume(&sc->sc_wb);
}

/* $NetBSD: dwiic_acpi.c,v 1.2.6.1 2019/09/23 15:27:04 martin Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: dwiic_acpi.c,v 1.2.6.1 2019/09/23 15:27:04 martin Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/device.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpi_intr.h>
#include <dev/acpi/acpi_i2c.h>

#include <dev/ic/dwiic_var.h>

struct dwiic_acpi_param {
	uint16_t hcnt;
	uint16_t lcnt;
	uint32_t ht;
};

static int	dwiic_acpi_match(device_t, cfdata_t, void *);
static void	dwiic_acpi_attach(device_t, device_t, void *);

static void	dwiic_acpi_parse_param(struct dwiic_softc *, ACPI_HANDLE,
					  const char *, struct dwiic_acpi_param *);
static void	dwiic_acpi_configure(struct dwiic_softc *, ACPI_HANDLE);

CFATTACH_DECL_NEW(dwiic_acpi, sizeof(struct dwiic_softc), dwiic_acpi_match, dwiic_acpi_attach, NULL, NULL);

static const char * const compatible[] = {
	"AMDI0510",	/* AMD Seattle */
	"APMC0D0F",	/* Ampere eMAG */
	NULL
};

static int
dwiic_acpi_match(device_t parent, cfdata_t cf, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	return acpi_match_hid(aa->aa_node->ad_devinfo, compatible);
}

static void
dwiic_acpi_attach(device_t parent, device_t self, void *aux)
{
	struct dwiic_softc * const sc = device_private(self);
	struct acpi_attach_args *aa = aux;
	struct acpi_resources res;
	struct acpi_mem *mem;
	struct acpi_irq *irq;
	ACPI_STATUS rv;
	int error;
	void *ih;

	sc->sc_dev = self;
	sc->sc_type = dwiic_type_generic;
	sc->sc_iot = aa->aa_memt;

	rv = acpi_resource_parse(sc->sc_dev, aa->aa_node->ad_handle, "_CRS",
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

	error = bus_space_map(sc->sc_iot, mem->ar_base, mem->ar_length, 0, &sc->sc_ioh);
	if (error) {
		aprint_error_dev(self, "couldn't map registers\n");
		return;
	}

	ih = acpi_intr_establish(self, (uint64_t)aa->aa_node->ad_handle,
	    IPL_VM, true, dwiic_intr, sc, device_xname(self));
	if (ih == NULL) {
		aprint_error_dev(self, "couldn't install interrupt handler\n");
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, mem->ar_length);
		goto done;
	}

	dwiic_acpi_configure(sc, aa->aa_node->ad_handle);

	sc->sc_iba.iba_child_devices = acpi_enter_i2c_devs(aa->aa_node);

	dwiic_attach(sc);

	config_found_ia(self, "i2cbus", &sc->sc_iba, iicbus_print);

	pmf_device_register(self, dwiic_suspend, dwiic_resume);

done:
	acpi_resource_cleanup(&res);
}

static void
dwiic_acpi_parse_param(struct dwiic_softc *sc, ACPI_HANDLE handle, const char *path,
    struct dwiic_acpi_param *param)
{
	ACPI_BUFFER buf;
	ACPI_OBJECT *obj;

	memset(param, 0, sizeof(*param));

	if (ACPI_FAILURE(acpi_eval_struct(handle, path, &buf)))
		return;

	obj = buf.Pointer;
	if (obj->Type != ACPI_TYPE_PACKAGE || obj->Package.Count != 3)
		goto done;

	param->hcnt = (uint16_t)obj->Package.Elements[0].Integer.Value;
	param->lcnt = (uint16_t)obj->Package.Elements[1].Integer.Value;
	param->ht = (uint32_t)obj->Package.Elements[2].Integer.Value;

done:
	ACPI_FREE(buf.Pointer);
}

static void
dwiic_acpi_configure(struct dwiic_softc *sc, ACPI_HANDLE handle)
{
	struct dwiic_acpi_param sscn, fmcn;

	dwiic_acpi_parse_param(sc, handle, "SSCN", &sscn);
	sc->ss_hcnt = sscn.hcnt;
	sc->ss_lcnt = sscn.lcnt;

	dwiic_acpi_parse_param(sc, handle, "FMCN", &fmcn);
	sc->fs_hcnt = fmcn.hcnt;
	sc->fs_lcnt = fmcn.lcnt;

	/* XXX */
	sc->sda_hold_time = fmcn.ht;
}

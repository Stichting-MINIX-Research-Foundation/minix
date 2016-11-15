/*	$NetBSD: apple_smc_acpi.c,v 1.3 2014/04/01 17:49:40 riastradh Exp $	*/

/*
 * Apple System Management Controller: ACPI Attachment
 */

/*-
 * Copyright (c) 2013 Taylor R. Campbell
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: apple_smc_acpi.c,v 1.3 2014/04/01 17:49:40 riastradh Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/module.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#include <dev/ic/apple_smcreg.h>
#include <dev/ic/apple_smcvar.h>

#define _COMPONENT		ACPI_RESOURCE_COMPONENT
ACPI_MODULE_NAME		("apple_smc_acpi")

struct apple_smc_acpi_softc {
	struct apple_smc_tag	sc_smc;
};

static int	apple_smc_acpi_match(device_t, cfdata_t, void *);
static void	apple_smc_acpi_attach(device_t, device_t, void *);
static int	apple_smc_acpi_detach(device_t, int);
static int	apple_smc_acpi_rescan(device_t, const char *, const int *);
static void	apple_smc_acpi_child_detached(device_t, device_t);

CFATTACH_DECL2_NEW(apple_smc_acpi, sizeof(struct apple_smc_acpi_softc),
    apple_smc_acpi_match,
    apple_smc_acpi_attach,
    apple_smc_acpi_detach,
    NULL /* activate */,
    apple_smc_acpi_rescan,
    apple_smc_acpi_child_detached);

static const char *const apple_smc_ids[] = {
	"APP0001",
	NULL
};

static int
apple_smc_acpi_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	if (!acpi_match_hid(aa->aa_node->ad_devinfo, apple_smc_ids))
		return 0;

	return 1;
}

static void
apple_smc_acpi_attach(device_t parent, device_t self, void *aux)
{
	struct apple_smc_acpi_softc *sc = device_private(self);
	struct apple_smc_tag *smc = &sc->sc_smc;
	struct acpi_attach_args *aa = aux;
	struct acpi_resources res;
	struct acpi_io *io;
	int rv;

	smc->smc_dev = self;

	aprint_normal("\n");
	aprint_naive("\n");

	rv = acpi_resource_parse(self, aa->aa_node->ad_handle, "_CRS",
	    &res, &acpi_resource_parse_ops_default);
	if (ACPI_FAILURE(rv)) {
		aprint_error_dev(self, "couldn't parse SMC resources: %s\n",
		    AcpiFormatException(rv));
		goto out0;
	}

	io = acpi_res_io(&res, 0);
	if (io == NULL) {
		aprint_error_dev(self, "no I/O resource\n");
		goto out1;
	}

	if (io->ar_length < APPLE_SMC_REGSIZE) {
		aprint_error_dev(self, "I/O resources too small: %"PRId32"\n",
		    io->ar_length);
		goto out1;
	}

	if (bus_space_map(aa->aa_iot, io->ar_base, io->ar_length, 0,
		&smc->smc_bsh) != 0) {
		aprint_error_dev(self, "unable to map I/O registers\n");
		goto out1;
	}

	smc->smc_bst = aa->aa_iot;
	smc->smc_size = io->ar_length;

	apple_smc_attach(smc);

out1:	acpi_resource_cleanup(&res);
out0:	return;
}

static int
apple_smc_acpi_detach(device_t self, int flags)
{
	struct apple_smc_acpi_softc *sc = device_private(self);
	struct apple_smc_tag *smc = &sc->sc_smc;
	int error;

	if (smc->smc_size != 0) {
		error = apple_smc_detach(smc, flags);
		if (error)
			return error;

		bus_space_unmap(smc->smc_bst, smc->smc_bsh, smc->smc_size);
		smc->smc_size = 0;
	}

	return 0;
}

static int
apple_smc_acpi_rescan(device_t self, const char *ifattr, const int *locs)
{
	struct apple_smc_acpi_softc *const sc = device_private(self);

	return apple_smc_rescan(&sc->sc_smc, ifattr, locs);
}

static void
apple_smc_acpi_child_detached(device_t self, device_t child)
{
	struct apple_smc_acpi_softc *const sc = device_private(self);

	apple_smc_child_detached(&sc->sc_smc, child);
}

MODULE(MODULE_CLASS_DRIVER, apple_smc_acpi, "apple_smc");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
apple_smc_acpi_modcmd(modcmd_t cmd, void *arg __unused)
{
#ifdef _MODULE
	int error;
#endif

	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_apple_smc_acpi,
		    cfattach_ioconf_apple_smc_acpi,
		    cfdata_ioconf_apple_smc_acpi);
		if (error)
			return error;
#endif
		return 0;

	case MODULE_CMD_FINI:
#ifdef _MODULE
		error = config_fini_component(cfdriver_ioconf_apple_smc_acpi,
		    cfattach_ioconf_apple_smc_acpi,
		    cfdata_ioconf_apple_smc_acpi);
		if (error)
			return error;
#endif
		return 0;

	default:
		return ENOTTY;
	}
}

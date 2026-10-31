/* $NetBSD: ipmi_acpi.c,v 1.3 2019/01/30 20:20:36 christos Exp $ */

/*-
 * Copyright (c) 2018 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Michael van Elst
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
__KERNEL_RCSID(0, "$NetBSD: ipmi_acpi.c,v 1.3 2019/01/30 20:20:36 christos Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/module.h>
#include <sys/systm.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#include <dev/ipmivar.h>

#define _COMPONENT		ACPI_RESOURCE_COMPONENT
ACPI_MODULE_NAME		("ipmi_acpi")

typedef struct ipmi_acpi_softc {
	device_t		sc_dev;
	bool			sc_init;
} ipmi_acpi_softc_t;

static int	ipmi_acpi_match(device_t, cfdata_t, void *);
static void	ipmi_acpi_attach(device_t, device_t, void *);
static int	ipmi_acpi_detach(device_t, int);

CFATTACH_DECL3_NEW(ipmi_acpi, sizeof(ipmi_acpi_softc_t),
    ipmi_acpi_match, ipmi_acpi_attach, ipmi_acpi_detach, NULL, NULL, NULL,
    DVF_DETACH_SHUTDOWN);

static const char * const ipmi_ids[] = {
	"IPI0001",
	NULL
};

static int
ipmi_acpi_match(device_t parent, cfdata_t match, void *opaque)
{
	struct acpi_attach_args *aa = (struct acpi_attach_args *)opaque;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	if (!acpi_match_hid(aa->aa_node->ad_devinfo, ipmi_ids))
		return 0;

	return 1;
}

static void
ipmi_acpi_attach(device_t parent, device_t self, void *opaque)
{
	ipmi_acpi_softc_t *sc = device_private(self);
	struct acpi_attach_args *aa = (struct acpi_attach_args *)opaque;
	ACPI_STATUS rv;
	ACPI_INTEGER itype, ivers, adr;
	struct acpi_resources res;
	struct acpi_io *io;
	struct acpi_mem *mem;
#if notyet
	struct acpi_irq *irq;
#endif
	struct ipmi_attach_args IA, *ia = &IA;
	bus_addr_t reg2;
	uint16_t i2caddr;

	sc->sc_dev = self;

	aprint_naive("\n");

	rv = acpi_eval_integer(aa->aa_node->ad_handle, "_IFT", &itype);
	if (ACPI_FAILURE(rv)) {
		aprint_error("no _IFT\n");
		return;
	}

	rv = acpi_eval_integer(aa->aa_node->ad_handle, "_SRV", &ivers);
	if (ACPI_FAILURE(rv)) {
		aprint_error("no _SRV\n");
		return;
	}

	switch (itype) {
	case IPMI_IF_KCS:
	case IPMI_IF_SMIC:
	case IPMI_IF_BT:
		rv = acpi_resource_parse(self, aa->aa_node->ad_handle, "_CRS",
			&res, &acpi_resource_parse_ops_default);
		if (ACPI_FAILURE(rv)) {
			aprint_normal("\n");
			aprint_error_dev(self, "no resources\n");
			return;
		}

		io = acpi_res_io(&res, 0);
		mem = acpi_res_mem(&res, 0);
		if (io == NULL && mem == NULL) {
			aprint_error_dev(self, "no resources\n");
			return;
		}

#if notyet
		if ((irq = acpi_res_irq(&res, 0)) != NULL) {
			aprint_normal_dev(self, "IRQ %d type %d\n",
				irq->ar_irq, irq->ar_type);
		} else {
			aprint_error_dev(self, "no interrupt\n");
		}
#endif
		ia->iaa_iot = aa->aa_iot;
		ia->iaa_memt = aa->aa_memt;

		ia->iaa_if_type = itype;
		ia->iaa_if_rev = ivers;
		ia->iaa_if_iotype = io ? 'i' : 'm';
		ia->iaa_if_iobase = io ? io->ar_base : mem->ar_base;
		ia->iaa_if_iospacing = 1;
		ia->iaa_if_irq = -1;
		ia->iaa_if_irqlvl = 0;

		reg2 = 0;
		if (io) {
			io = acpi_res_io(&res, 1);
			if (io != NULL)
				reg2 = (bus_addr_t)io->ar_base;
		} else {
			mem = acpi_res_mem(&res, 1);
			if (mem != NULL)
				reg2 = mem->ar_base;
		}

		if (reg2 > ia->iaa_if_iobase)
			ia->iaa_if_iospacing = reg2 - ia->iaa_if_iobase;

		config_found_ia(self, "ipmibus", ia, 0);

		break;
	case IPMI_IF_SSIF:
		rv = acpi_eval_integer(aa->aa_node->ad_handle, "_ADR", &adr);
		if (ACPI_FAILURE(rv)) {
			aprint_normal("\n");
			aprint_error_dev(self, "no resources\n");
			return;
		}
		if (adr > 65535) {
			aprint_normal("\n");
			aprint_error_dev(self, "i2c address out of range\n");
			return;
		}
		i2caddr = adr;

		aprint_normal(": i2c 0x%x\n", i2caddr);
		break;
	default:
		aprint_normal("\n");
		aprint_error_dev(self, "unknown interface type\n");
		return;
	}

	sc->sc_init = true;

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

static int
ipmi_acpi_detach(device_t self, int flags)
{
	struct ipmi_acpi_softc *sc = device_private(self);
	int rc;

	if (!sc->sc_init)
		return 0;

	rc = config_detach_children(self, flags);
	if (rc)
		return rc;

	pmf_device_deregister(self);
	return 0;
}

/*	$NetBSD: vmbus_acpi.c,v 1.2 2019/05/24 14:28:48 nonaka Exp $	*/

/*
 * Copyright (c) 2018 Kimihiro Nonaka <nonaka@NetBSD.org>
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
__KERNEL_RCSID(0, "$NetBSD: vmbus_acpi.c,v 1.2 2019/05/24 14:28:48 nonaka Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/kmem.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#include <dev/hyperv/vmbusvar.h>

#define _COMPONENT	ACPI_RESOURCE_COMPONENT
ACPI_MODULE_NAME	("vmbus_acpi")

static int	vmbus_acpi_match(device_t, cfdata_t, void *);
static void	vmbus_acpi_attach(device_t, device_t, void *);
static int	vmbus_acpi_detach(device_t, int);

struct vmbus_acpi_softc {
	struct vmbus_softc sc;
};

CFATTACH_DECL_NEW(vmbus_acpi, sizeof(struct vmbus_acpi_softc),
    vmbus_acpi_match, vmbus_acpi_attach, vmbus_acpi_detach, NULL);

static const char * const vmbus_acpi_ids[] = {
	"VMBUS",
	"VMBus",
	NULL
};

static int
vmbus_acpi_match(device_t parent, cfdata_t match, void *opaque)
{
	struct acpi_attach_args *aa = opaque;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	if (!acpi_match_hid(aa->aa_node->ad_devinfo, vmbus_acpi_ids))
		return 0;

	if (!vmbus_match(parent, match, opaque))
		return 0;

	return 1;
}

static void
vmbus_acpi_attach(device_t parent, device_t self, void *opaque)
{
	struct vmbus_acpi_softc *sc = device_private(self);
	struct acpi_attach_args *aa = opaque;

	sc->sc.sc_dev = self;
	sc->sc.sc_iot = aa->aa_iot;
	sc->sc.sc_memt = aa->aa_memt;
	sc->sc.sc_dmat = aa->aa_dmat64 ? aa->aa_dmat64 : aa->aa_dmat;

	if (vmbus_attach(&sc->sc))
		return;

	(void)pmf_device_register(self, NULL, NULL);
}

static int
vmbus_acpi_detach(device_t self, int flags)
{
	struct vmbus_acpi_softc *sc = device_private(self);
	int rv;

	rv = vmbus_detach(&sc->sc, flags);
	if (rv)
		return rv;

	pmf_device_deregister(self);

	return 0;
}

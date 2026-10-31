/* $NetBSD: virtio_acpi.c,v 1.2 2018/11/16 23:18:17 jmcneill Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: virtio_acpi.c,v 1.2 2018/11/16 23:18:17 jmcneill Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/device.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpi_intr.h>

#define	VIRTIO_PRIVATE
#include <dev/virtio/virtio_mmiovar.h>

struct virtio_acpi_softc {
	struct virtio_mmio_softc	sc_msc;
	ACPI_HANDLE			sc_handle;
	int				sc_irq;
	int				sc_irqtype;
};

static int	virtio_acpi_match(device_t, cfdata_t, void *);
static void	virtio_acpi_attach(device_t, device_t, void *);
static int	virtio_acpi_rescan(device_t, const char *, const int *);
static int	virtio_acpi_detach(device_t, int);

static int	virtio_acpi_setup_interrupts(struct virtio_mmio_softc *);
static void	virtio_acpi_free_interrupts(struct virtio_mmio_softc *);

CFATTACH_DECL3_NEW(virtio_acpi, sizeof(struct virtio_acpi_softc),
    virtio_acpi_match, virtio_acpi_attach, virtio_acpi_detach, NULL,
    virtio_acpi_rescan, (void *)voidop, DVF_DETACH_SHUTDOWN);

static const char * const compatible[] = {
	"LNRO0005",
	NULL
};

static int
virtio_acpi_match(device_t parent, cfdata_t cf, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	return acpi_match_hid(aa->aa_node->ad_devinfo, compatible);
}

static void
virtio_acpi_attach(device_t parent, device_t self, void *aux)
{
	struct virtio_acpi_softc * const sc = device_private(self);
	struct virtio_mmio_softc * const msc = &sc->sc_msc;
	struct virtio_softc * const vsc = &msc->sc_sc;
	struct acpi_attach_args *aa = aux;
	struct acpi_resources res;
	struct acpi_mem *mem;
	struct acpi_irq *irq;
	ACPI_STATUS rv;
	int error;

	sc->sc_handle = aa->aa_node->ad_handle;
	msc->sc_iot = aa->aa_memt;
	vsc->sc_dev = self;
	vsc->sc_dmat = aa->aa_dmat;

	rv = acpi_resource_parse(self, aa->aa_node->ad_handle, "_CRS",
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
	sc->sc_irq = irq->ar_irq;
	sc->sc_irqtype = (irq->ar_type == ACPI_EDGE_SENSITIVE) ? IST_EDGE : IST_LEVEL;

	error = bus_space_map(msc->sc_iot, mem->ar_base, mem->ar_length, 0, &msc->sc_ioh);
	if (error) {
		aprint_error_dev(self, "couldn't map registers\n");
		return;
	}
	msc->sc_iosize = mem->ar_length;

	msc->sc_setup_interrupts = virtio_acpi_setup_interrupts;
	msc->sc_free_interrupts = virtio_acpi_free_interrupts;

	virtio_mmio_common_attach(msc);
	virtio_acpi_rescan(self, "virtio", NULL);

done:
	acpi_resource_cleanup(&res);
}

static int
virtio_acpi_detach(device_t self, int flags)
{
	struct virtio_acpi_softc * const sc = device_private(self);
	struct virtio_mmio_softc * const msc = &sc->sc_msc;

	return virtio_mmio_common_detach(msc, flags);
}

static int
virtio_acpi_rescan(device_t self, const char *ifattr, const int *locs)
{
	struct virtio_acpi_softc * const sc = device_private(self);
	struct virtio_mmio_softc * const msc = &sc->sc_msc;
	struct virtio_softc * const vsc = &msc->sc_sc;
	struct virtio_attach_args va;

	if (vsc->sc_child)
		return 0;

	memset(&va, 0, sizeof(va));
	va.sc_childdevid = vsc->sc_childdevid;

	config_found_ia(self, ifattr, &va, virtiobusprint);

	return 0;
}

static int
virtio_acpi_setup_interrupts(struct virtio_mmio_softc *msc)
{
	struct virtio_acpi_softc * const sc = (struct virtio_acpi_softc *)msc;
	struct virtio_softc * const vsc = &msc->sc_sc;

	msc->sc_ih = acpi_intr_establish(vsc->sc_dev, (uint64_t)sc->sc_handle,
            IPL_VM, false, virtio_mmio_intr, msc, device_xname(vsc->sc_dev));
	if (msc->sc_ih == NULL) {
		aprint_error_dev(vsc->sc_dev, "couldn't install interrupt handler\n");
		return -1;
	}

	aprint_normal_dev(vsc->sc_dev, "interrupting on irq %d\n", sc->sc_irq);

	return 0;
}

static void
virtio_acpi_free_interrupts(struct virtio_mmio_softc *msc)
{
	if (msc->sc_ih != NULL) {
		acpi_intr_disestablish(msc->sc_ih);
		msc->sc_ih = NULL;
	}
}

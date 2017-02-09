/*	$NetBSD: if_an_pci.c,v 1.35 2014/03/29 19:28:24 christos Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Atsushi Onoe.
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

/*
 * PCI bus front-end for the Aironet PC4500/PC4800 Wireless LAN Adapter.
 * Unlike WaveLAN, this adapter attached as PCI device using a PLX 9050
 * PCI to "dumb bus" bridge chip.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_an_pci.c,v 1.35 2014/03/29 19:28:24 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/callout.h>

#include <machine/endian.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <net80211/ieee80211_netbsd.h>
#include <net80211/ieee80211_var.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/ic/anreg.h>
#include <dev/ic/anvar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#define	AN_PCI_PLX_IOBA		0x14	/* i/o base for PLX chip */
#define AN_PCI_IOBA PCI_BAR(2)	/* i/o base */

struct an_pci_softc {
	struct an_softc sc_an;		/* real "an" softc */
	pci_chipset_tag_t sc_pct;
	pcitag_t sc_pcitag;

	/* PCI-specific goo. */
	void	*sc_ih;			/* interrupt handle */
};

static int	an_pci_match(device_t, cfdata_t, void *);
static void	an_pci_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(an_pci, sizeof(struct an_pci_softc),
    an_pci_match, an_pci_attach, NULL, NULL);

static const struct an_pci_product {
	u_int32_t	app_vendor;	/* PCI vendor ID */
	u_int32_t	app_product;	/* PCI product ID */
} an_pci_products[] = {
	{ PCI_VENDOR_AIRONET,		PCI_PRODUCT_AIRONET_PC4xxx },
	{ PCI_VENDOR_AIRONET,		PCI_PRODUCT_AIRONET_PC4500 },
	{ PCI_VENDOR_AIRONET,		PCI_PRODUCT_AIRONET_PC4800 },
	{ PCI_VENDOR_AIRONET,		PCI_PRODUCT_AIRONET_PCI350 },
	{ PCI_VENDOR_AIRONET,		PCI_PRODUCT_AIRONET_MPI350 },
	{ 0,				0			   }
};

static int
an_pci_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;
	const struct an_pci_product *app;

	for (app = an_pci_products; app->app_vendor != 0; app++) {
		if (PCI_VENDOR(pa->pa_id) == app->app_vendor &&
		    PCI_PRODUCT(pa->pa_id) == app->app_product)
			return 1;
	}
	return 0;
}

static void
an_pci_attach(device_t parent, device_t self, void *aux)
{
        struct pci_attach_args *pa = (struct pci_attach_args *)aux;
	struct an_pci_softc *psc = device_private(self);
	struct an_softc *sc = &psc->sc_an;
	char const *intrstr;
	pci_intr_handle_t ih;
	bus_size_t iosize;
	u_int32_t csr;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;
	psc->sc_pct = pa->pa_pc;
	psc->sc_pcitag = pa->pa_tag;

	pci_aprint_devinfo(pa, "802.11 controller");

        /* Map I/O registers */
        if (pci_mapreg_map(pa, AN_PCI_IOBA, PCI_MAPREG_TYPE_IO, 0,
	    &sc->sc_iot, &sc->sc_ioh, NULL, &iosize) != 0) {
                aprint_error_dev(self, "unable to map registers\n");
                return;
        }

        /* Enable the device. */
        csr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
        pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
                       csr | PCI_COMMAND_MASTER_ENABLE);

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih)) {
        	aprint_error_dev(self, "unable to map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih, intrbuf, sizeof(intrbuf));
	psc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_NET, an_intr, sc);
	if (psc->sc_ih == NULL) {
		aprint_error_dev(self, "unable to establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);
	sc->sc_enabled = 1;

	if (an_attach(sc) != 0) {
		aprint_error_dev(self, "failed to attach controller\n");
		pci_intr_disestablish(pa->pa_pc, psc->sc_ih);
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, iosize);
	}

	if (pmf_device_register(self, NULL, NULL))
		pmf_class_network_register(self, &sc->sc_if);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");
}

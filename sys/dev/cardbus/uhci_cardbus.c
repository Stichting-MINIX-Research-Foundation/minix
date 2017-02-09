/*	$NetBSD: uhci_cardbus.c,v 1.21 2014/09/21 15:07:19 christos Exp $	*/

/*
 * Copyright (c) 1998-2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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
__KERNEL_RCSID(0, "$NetBSD: uhci_cardbus.c,v 1.21 2014/09/21 15:07:19 christos Exp $");

#include "ehci_cardbus.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/queue.h>

#include <sys/bus.h>

#include <dev/cardbus/cardbusvar.h>
#include <dev/cardbus/usb_cardbus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <dev/usb/uhcireg.h>
#include <dev/usb/uhcivar.h>

struct uhci_cardbus_softc {
	uhci_softc_t		sc;
#if NEHCI_CARDBUS > 0
	struct usb_cardbus	sc_cardbus;
#endif
	cardbus_chipset_tag_t	sc_cc;
	cardbus_function_tag_t	sc_cf;
	cardbus_devfunc_t	sc_ct;
	pcitag_t		sc_tag;
	void 			*sc_ih;		/* interrupt vectoring */
};

static int	uhci_cardbus_match(device_t, cfdata_t, void *);
static void	uhci_cardbus_attach(device_t, device_t, void *);
static int	uhci_cardbus_detach(device_t, int);

CFATTACH_DECL_NEW(uhci_cardbus, sizeof(struct uhci_cardbus_softc),
    uhci_cardbus_match, uhci_cardbus_attach, uhci_cardbus_detach, uhci_activate);

static int
uhci_cardbus_match(device_t parent, cfdata_t match, void *aux)
{
	struct cardbus_attach_args *ca = (struct cardbus_attach_args *)aux;

	if (PCI_CLASS(ca->ca_class) == PCI_CLASS_SERIALBUS &&
	    PCI_SUBCLASS(ca->ca_class) == PCI_SUBCLASS_SERIALBUS_USB &&
	    PCI_INTERFACE(ca->ca_class) == PCI_INTERFACE_UHCI)
		return (1);

	return (0);
}

static void
uhci_cardbus_attach(device_t parent, device_t self,
    void *aux)
{
	struct uhci_cardbus_softc *sc = device_private(self);
	struct cardbus_attach_args *ca = (struct cardbus_attach_args *)aux;
	cardbus_devfunc_t ct = ca->ca_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;
	pcitag_t tag = ca->ca_tag;
	pcireg_t csr;
	const char *devname = device_xname(self);
	char devinfo[256];
	usbd_status r;

	sc->sc.sc_dev = self;
	sc->sc.sc_bus.hci_private = sc;

	pci_devinfo(ca->ca_id, ca->ca_class, 0, devinfo, sizeof(devinfo));
	printf(": %s (rev. 0x%02x)\n", devinfo, PCI_REVISION(ca->ca_class));

	/* Map I/O registers */
	if (Cardbus_mapreg_map(ct, PCI_CBIO, PCI_MAPREG_TYPE_IO, 0,
			   &sc->sc.iot, &sc->sc.ioh, NULL, &sc->sc.sc_size)) {
		printf("%s: can't map i/o space\n", devname);
		return;
	}

	sc->sc_cc = cc;
	sc->sc_cf = cf;
	sc->sc_ct = ct;
	sc->sc_tag = tag;
	sc->sc.sc_bus.dmatag = ca->ca_dmat;

	/* Enable the device. */
	csr = Cardbus_conf_read(ct, tag, PCI_COMMAND_STATUS_REG);
	Cardbus_conf_write(ct, tag, PCI_COMMAND_STATUS_REG,
		       csr | PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_IO_ENABLE);

	/* Disable interrupts, so we don't get any spurious ones. */
	bus_space_write_2(sc->sc.iot, sc->sc.ioh, UHCI_INTR, 0);

	/* Map and establish the interrupt. */
	sc->sc_ih = Cardbus_intr_establish(ct, IPL_USB, uhci_intr, sc);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt\n", devname);
		return;
	}

	/* Set LEGSUP register to its default value. */
	Cardbus_conf_write(ct, tag, PCI_LEGSUP, PCI_LEGSUP_USBPIRQDEN);

	switch(Cardbus_conf_read(ct, tag, PCI_USBREV) & PCI_USBREV_MASK) {
	case PCI_USBREV_PRE_1_0:
		sc->sc.sc_bus.usbrev = USBREV_PRE_1_0;
		break;
	case PCI_USBREV_1_0:
		sc->sc.sc_bus.usbrev = USBREV_1_0;
		break;
	case PCI_USBREV_1_1:
		sc->sc.sc_bus.usbrev = USBREV_1_1;
		break;
	default:
		sc->sc.sc_bus.usbrev = USBREV_UNKNOWN;
		break;
	}

	/* Figure out vendor for root hub descriptor. */
	sc->sc.sc_id_vendor = PCI_VENDOR(ca->ca_id);
	pci_findvendor(sc->sc.sc_vendor, sizeof(sc->sc.sc_vendor),
	    sc->sc.sc_id_vendor);

	r = uhci_init(&sc->sc);
	if (r != USBD_NORMAL_COMPLETION) {
		printf("%s: init failed, error=%d\n", devname, r);

		/* Avoid spurious interrupts. */
		Cardbus_intr_disestablish(ct, sc->sc_ih);
		sc->sc_ih = NULL;

		return;
	}

#if NEHCI_CARDBUS > 0
	usb_cardbus_add(&sc->sc_cardbus, ca, self);
#endif

	/* Attach usb device. */
	sc->sc.sc_child = config_found(self, &sc->sc.sc_bus, usbctlprint);
}

static int
uhci_cardbus_detach(device_t self, int flags)
{
	struct uhci_cardbus_softc *sc = device_private(self);
	struct cardbus_devfunc *ct = sc->sc_ct;
	int rv;

	rv = uhci_detach(&sc->sc, flags);
	if (rv)
		return (rv);
	if (sc->sc_ih != NULL) {
		Cardbus_intr_disestablish(ct, sc->sc_ih);
		sc->sc_ih = NULL;
	}
	if (sc->sc.sc_size) {
		Cardbus_mapreg_unmap(ct, PCI_CBIO, sc->sc.iot,
		    sc->sc.ioh, sc->sc.sc_size);
		sc->sc.sc_size = 0;
	}
#if NEHCI_CARDBUS > 0
	usb_cardbus_rem(&sc->sc_cardbus);
#endif
	return (0);
}

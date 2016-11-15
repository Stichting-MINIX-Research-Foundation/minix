/*	$NetBSD: ohci_cardbus.c,v 1.40 2014/09/21 15:07:19 christos Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
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

/*
 * USB Open Host Controller driver.
 *
 * OHCI spec: http://www.intel.com/design/usb/ohci11d.pdf
 * USB spec: http://www.teleport.com/cgi-bin/mailmerge.cgi/~usb/cgiform.tpl
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ohci_cardbus.c,v 1.40 2014/09/21 15:07:19 christos Exp $");

#include "ehci_cardbus.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <sys/bus.h>

#if defined pciinc
#include <dev/pci/pcidevs.h>
#endif

#include <dev/cardbus/cardbusvar.h>
#include <dev/pci/pcidevs.h>

#include <dev/cardbus/usb_cardbus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <dev/usb/ohcireg.h>
#include <dev/usb/ohcivar.h>

int	ohci_cardbus_match(device_t, cfdata_t, void *);
void	ohci_cardbus_attach(device_t, device_t, void *);
int	ohci_cardbus_detach(device_t, int);

struct ohci_cardbus_softc {
	ohci_softc_t		sc;
#if NEHCI_CARDBUS > 0
	struct usb_cardbus	sc_cardbus;
#endif
	cardbus_chipset_tag_t	sc_cc;
	cardbus_function_tag_t	sc_cf;
	cardbus_devfunc_t	sc_ct;
	void 			*sc_ih;		/* interrupt vectoring */
};

CFATTACH_DECL_NEW(ohci_cardbus, sizeof(struct ohci_cardbus_softc),
    ohci_cardbus_match, ohci_cardbus_attach, ohci_cardbus_detach, ohci_activate);

int
ohci_cardbus_match(device_t parent, cfdata_t match, void *aux)
{
	struct cardbus_attach_args *ca = (struct cardbus_attach_args *)aux;

	if (PCI_CLASS(ca->ca_class) == PCI_CLASS_SERIALBUS &&
	    PCI_SUBCLASS(ca->ca_class) == PCI_SUBCLASS_SERIALBUS_USB &&
	    PCI_INTERFACE(ca->ca_class) == PCI_INTERFACE_OHCI)
		return (1);

	return (0);
}

void
ohci_cardbus_attach(device_t parent, device_t self, void *aux)
{
	struct ohci_cardbus_softc *sc = device_private(self);
	struct cardbus_attach_args *ca = aux;
	cardbus_devfunc_t ct = ca->ca_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	cardbus_function_tag_t cf = ct->ct_cf;
	pcireg_t csr;
	char devinfo[256];
	usbd_status r;
	const char *devname = device_xname(self);

	sc->sc.sc_dev = self;
	sc->sc.sc_bus.hci_private = sc;

	pci_devinfo(ca->ca_id, ca->ca_class, 0, devinfo, sizeof(devinfo));
	printf(": %s (rev. 0x%02x)\n", devinfo,
	       PCI_REVISION(ca->ca_class));

	/* Map I/O registers */
	if (Cardbus_mapreg_map(ct, PCI_CBMEM, PCI_MAPREG_TYPE_MEM, 0,
			   &sc->sc.iot, &sc->sc.ioh, NULL, &sc->sc.sc_size)) {
		printf("%s: can't map mem space\n", devname);
		return;
	}

	sc->sc_cc = cc;
	sc->sc_cf = cf;
	sc->sc_ct = ct;
	sc->sc.sc_bus.dmatag = ca->ca_dmat;

	/* Enable the device. */
	csr = Cardbus_conf_read(ct, ca->ca_tag,
				PCI_COMMAND_STATUS_REG);
	Cardbus_conf_write(ct, ca->ca_tag, PCI_COMMAND_STATUS_REG,
		       csr | PCI_COMMAND_MASTER_ENABLE
			   | PCI_COMMAND_MEM_ENABLE);

	/* Disable interrupts, so we don't can any spurious ones. */
	bus_space_write_4(sc->sc.iot, sc->sc.ioh, OHCI_INTERRUPT_DISABLE,
			  OHCI_ALL_INTRS);

	sc->sc_ih = Cardbus_intr_establish(ct, IPL_USB, ohci_intr, sc);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt\n", devname);
		return;
	}

	/* Figure out vendor for root hub descriptor. */
	sc->sc.sc_id_vendor = PCI_VENDOR(ca->ca_id);
	pci_findvendor(sc->sc.sc_vendor, sizeof(sc->sc.sc_vendor),
	    sc->sc.sc_id_vendor);

	r = ohci_init(&sc->sc);
	if (r != USBD_NORMAL_COMPLETION) {
		printf("%s: init failed, error=%d\n", devname, r);

		/* Avoid spurious interrupts. */
		Cardbus_intr_disestablish(ct, sc->sc_ih);
		sc->sc_ih = 0;

		return;
	}

#if NEHCI_CARDBUS > 0
	usb_cardbus_add(&sc->sc_cardbus, ca, self);
#endif

	if (!pmf_device_register1(self, ohci_suspend, ohci_resume,
	                          ohci_shutdown))
		aprint_error_dev(self, "couldn't establish power handler\n");

	/* Attach usb device. */
	sc->sc.sc_child = config_found(self, &sc->sc.sc_bus, usbctlprint);
}

int
ohci_cardbus_detach(device_t self, int flags)
{
	struct ohci_cardbus_softc *sc = device_private(self);
	struct cardbus_devfunc *ct = sc->sc_ct;
	int rv;

	rv = ohci_detach(&sc->sc, flags);
	if (rv)
		return (rv);
	if (sc->sc_ih != NULL) {
		Cardbus_intr_disestablish(ct, sc->sc_ih);
		sc->sc_ih = NULL;
	}
	if (sc->sc.sc_size) {
		Cardbus_mapreg_unmap(ct, PCI_CBMEM, sc->sc.iot,
		    sc->sc.ioh, sc->sc.sc_size);
		sc->sc.sc_size = 0;
	}
#if NEHCI_CARDBUS > 0
	usb_cardbus_rem(&sc->sc_cardbus);
#endif
	return (0);
}

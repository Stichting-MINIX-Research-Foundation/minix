/*	$NetBSD: uhci_pci.c,v 1.59 2015/08/19 06:16:18 skrll Exp $	*/

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uhci_pci.c,v 1.59 2015/08/19 06:16:18 skrll Exp $");

#include "ehci.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/queue.h>

#include <sys/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/usb_pci.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <dev/usb/uhcireg.h>
#include <dev/usb/uhcivar.h>

static bool	uhci_pci_resume(device_t, const pmf_qual_t *);

struct uhci_pci_softc {
	uhci_softc_t		sc;
#if NEHCI > 0
	struct usb_pci		sc_pci;
#endif
	pci_chipset_tag_t	sc_pc;
	pcitag_t		sc_tag;
	void 			*sc_ih;		/* interrupt vectoring */
	unsigned		sc_initialized;
#define		SC_INIT_UHCI	1
#define		SC_INIT_PMF	2
};

static int
uhci_pci_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_SERIALBUS &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_SERIALBUS_USB &&
	    PCI_INTERFACE(pa->pa_class) == PCI_INTERFACE_UHCI)
		return (1);

	return (0);
}

static void
uhci_pci_attach(device_t parent, device_t self, void *aux)
{
	struct uhci_pci_softc *sc = device_private(self);
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t tag = pa->pa_tag;
	char const *intrstr;
	pci_intr_handle_t ih;
	pcireg_t csr;
	usbd_status r;
	int s;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc.sc_dev = self;
	sc->sc.sc_bus.hci_private = sc;

	pci_aprint_devinfo(pa, NULL);

	/* Map I/O registers */
	if (pci_mapreg_map(pa, PCI_CBIO, PCI_MAPREG_TYPE_IO, 0,
			   &sc->sc.iot, &sc->sc.ioh, NULL, &sc->sc.sc_size)) {
		aprint_error_dev(self, "can't map i/o space\n");
		return;
	}

	/*
	 * Disable interrupts, so we don't get any spurious ones.
	 * Acknowledge all pending interrupts.
	 */
	bus_space_write_2(sc->sc.iot, sc->sc.ioh, UHCI_INTR, 0);
	bus_space_write_2(sc->sc.iot, sc->sc.ioh, UHCI_STS,
	    bus_space_read_2(sc->sc.iot, sc->sc.ioh, UHCI_STS));

	sc->sc_pc = pc;
	sc->sc_tag = tag;
	sc->sc.sc_bus.dmatag = pa->pa_dmat;

	/* Enable the device. */
	csr = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG,
		       csr | PCI_COMMAND_MASTER_ENABLE);

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_USB, uhci_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	/*
	 * Set LEGSUP register to its default value.
	 * This can re-enable or trigger interrupts, so protect against
	 * them and explicitly disable and ACK them afterwards.
	 */
	s = splhardusb();
	pci_conf_write(pc, tag, PCI_LEGSUP, PCI_LEGSUP_USBPIRQDEN);
	bus_space_write_2(sc->sc.iot, sc->sc.ioh, UHCI_INTR, 0);
	bus_space_write_2(sc->sc.iot, sc->sc.ioh, UHCI_STS,
	    bus_space_read_2(sc->sc.iot, sc->sc.ioh, UHCI_STS));
	splx(s);

	switch(pci_conf_read(pc, tag, PCI_USBREV) & PCI_USBREV_MASK) {
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
	sc->sc.sc_id_vendor = PCI_VENDOR(pa->pa_id);
	pci_findvendor(sc->sc.sc_vendor, sizeof(sc->sc.sc_vendor),
	    sc->sc.sc_id_vendor);
	r = uhci_init(&sc->sc);
	if (r != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(self, "init failed, error=%d\n", r);
		return;
	}
	sc->sc_initialized = SC_INIT_UHCI;

#if NEHCI > 0
	usb_pci_add(&sc->sc_pci, pa, self);
#endif

	if (!pmf_device_register(self, uhci_suspend, uhci_pci_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");
	else
		sc->sc_initialized |= SC_INIT_PMF;

	/* Attach usb device. */
	sc->sc.sc_child = config_found(self, &sc->sc.sc_bus, usbctlprint);
}

static int
uhci_pci_detach(device_t self, int flags)
{
	struct uhci_pci_softc *sc = device_private(self);
	int rv;

	if (sc->sc_initialized & SC_INIT_UHCI) {
		rv = uhci_detach(&sc->sc, flags);
		if (rv)
			return (rv);
	}

	if (sc->sc_initialized & SC_INIT_PMF)
		pmf_device_deregister(self);

	/* disable interrupts and acknowledge any pending */
	bus_space_write_2(sc->sc.iot, sc->sc.ioh, UHCI_INTR, 0);
	bus_space_write_2(sc->sc.iot, sc->sc.ioh, UHCI_STS,
	    bus_space_read_2(sc->sc.iot, sc->sc.ioh, UHCI_STS));

	if (sc->sc_ih != NULL) {
		pci_intr_disestablish(sc->sc_pc, sc->sc_ih);
		sc->sc_ih = NULL;
	}
	if (sc->sc.sc_size) {
		bus_space_unmap(sc->sc.iot, sc->sc.ioh, sc->sc.sc_size);
		sc->sc.sc_size = 0;
	}
#if NEHCI > 0
	usb_pci_rem(&sc->sc_pci);
#endif
	return (0);
}

static bool
uhci_pci_resume(device_t dv, const pmf_qual_t *qual)
{
	struct uhci_pci_softc *sc = device_private(dv);

	/* Set LEGSUP register to its default value. */
	pci_conf_write(sc->sc_pc, sc->sc_tag, PCI_LEGSUP,
	    PCI_LEGSUP_USBPIRQDEN);

	return uhci_resume(dv, qual);
}

CFATTACH_DECL3_NEW(uhci_pci, sizeof(struct uhci_pci_softc),
    uhci_pci_match, uhci_pci_attach, uhci_pci_detach, uhci_activate,
    NULL, uhci_childdet, DVF_DETACH_SHUTDOWN);

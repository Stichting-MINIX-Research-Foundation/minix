/*	$NetBSD: fwohci_pci.c,v 1.42 2014/03/29 19:28:24 christos Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry.
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
__KERNEL_RCSID(0, "$NetBSD: fwohci_pci.c,v 1.42 2014/03/29 19:28:24 christos Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/intr.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/systm.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/ieee1394/firewire.h>
#include <dev/ieee1394/firewirereg.h>
#include <dev/ieee1394/fwdma.h>
#include <dev/ieee1394/fwohcireg.h>
#include <dev/ieee1394/fwohcivar.h>

struct fwohci_pci_softc {
	struct fwohci_softc psc_sc;

	pci_chipset_tag_t psc_pc;
	pcitag_t psc_tag;

	void *psc_ih;
};

static int fwohci_pci_match(device_t, cfdata_t, void *);
static void fwohci_pci_attach(device_t, device_t, void *);
static int fwohci_pci_detach(device_t, int);

static bool fwohci_pci_suspend(device_t, const pmf_qual_t *);
static bool fwohci_pci_resume(device_t, const pmf_qual_t *);

CFATTACH_DECL_NEW(fwohci_pci, sizeof(struct fwohci_pci_softc),
    fwohci_pci_match, fwohci_pci_attach, fwohci_pci_detach, NULL);

static int
fwohci_pci_match(device_t parent, cfdata_t match,
    void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	/*
	 * XXX
	 * Firewire controllers used in some G3 PowerBooks hang the system
	 * when trying to discover devices - don't attach to those for now
	 * until someone with the right hardware can investigate
	 */
	if ((PCI_VENDOR(pa->pa_id) == PCI_VENDOR_APPLE) &&
	    (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_APPLE_PBG3_FW))
	    return 0;
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_SERIALBUS &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_SERIALBUS_FIREWIRE &&
	    PCI_INTERFACE(pa->pa_class) == PCI_INTERFACE_OHCI)
		return 1;

	return 0;
}

static void
fwohci_pci_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;
	struct fwohci_pci_softc *psc = device_private(self);
	char const *intrstr;
	pci_intr_handle_t ih;
	uint32_t csr;
	char intrbuf[PCI_INTRSTR_LEN];

	pci_aprint_devinfo(pa, "IEEE 1394 Controller");

	fwohci_init(&psc->psc_sc);

	psc->psc_sc.fc.dev = self;
	psc->psc_sc.fc.dmat = pa->pa_dmat;
	psc->psc_pc = pa->pa_pc;
	psc->psc_tag = pa->pa_tag;

	/* Map I/O registers */
	if (pci_mapreg_map(pa, PCI_OHCI_MAP_REGISTER, PCI_MAPREG_TYPE_MEM, 0,
	    &psc->psc_sc.bst, &psc->psc_sc.bsh,
	    NULL, &psc->psc_sc.bssize)) {
		aprint_error_dev(self, "can't map OHCI register space\n");
		goto fail;
	}

	/* Disable interrupts, so we don't get any spurious ones. */
	OWRITE(&psc->psc_sc, FWOHCI_INTMASKCLR, OHCI_INT_EN);

	/* Enable the device. */
	csr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	csr |= PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_MEM_ENABLE;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, csr);

	/*
	 * Some Sun FireWire controllers have their intpin register
	 * bogusly set to 0, although it should be 3. Correct that.
	 */
	if ((PCI_VENDOR(pa->pa_id) == PCI_VENDOR_SUN) &&
	    (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_SUN_FIREWIRE))
		if (pa->pa_intrpin == 0)
			pa->pa_intrpin = 3;

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		goto fail;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih, intrbuf, sizeof(intrbuf));
	psc->psc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO, fwohci_intr,
	    &psc->psc_sc);
	if (psc->psc_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		goto fail;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	if (fwohci_attach(&psc->psc_sc) != 0)
		goto fail;

	if (!pmf_device_register(self, fwohci_pci_suspend, fwohci_pci_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");

	return;

fail:
	/* In the event that we fail to attach, register a null pnp handler */
	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");

	return;
}

static int
fwohci_pci_detach(device_t self, int flags)
{
	struct fwohci_pci_softc *psc = device_private(self);
	int rv;

	pmf_device_deregister(self);
	rv = fwohci_detach(&psc->psc_sc, flags);
	if (rv)
		return rv;

	if (psc->psc_ih != NULL) {
		pci_intr_disestablish(psc->psc_pc, psc->psc_ih);
		psc->psc_ih = NULL;
	}
	if (psc->psc_sc.bssize) {
		bus_space_unmap(psc->psc_sc.bst, psc->psc_sc.bsh,
		    psc->psc_sc.bssize);
		psc->psc_sc.bssize = 0;
	}
	return 0;
}

static bool
fwohci_pci_suspend(device_t dv, const pmf_qual_t *qual)
{
	struct fwohci_pci_softc *psc = device_private(dv);
	int s;

	s = splbio();
	fwohci_stop(&psc->psc_sc);
	splx(s);

	return true;
}

static bool
fwohci_pci_resume(device_t dv, const pmf_qual_t *qual)
{
	struct fwohci_pci_softc *psc = device_private(dv);
	int s;

	s = splbio();
	fwohci_resume(&psc->psc_sc);
	splx(s);

	return true;
}

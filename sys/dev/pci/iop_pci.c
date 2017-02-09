/*	$NetBSD: iop_pci.c,v 1.28 2014/03/29 19:28:25 christos Exp $	*/

/*-
 * Copyright (c) 2000, 2001, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
 * PCI front-end for `iop' driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: iop_pci.c,v 1.28 2014/03/29 19:28:25 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/proc.h>

#include <machine/endian.h>
#include <sys/bus.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>

#include <dev/i2o/i2o.h>
#include <dev/i2o/iopreg.h>
#include <dev/i2o/iopio.h>
#include <dev/i2o/iopvar.h>

#define	PCI_INTERFACE_I2O_POLLED	0x00
#define	PCI_INTERFACE_I2O_INTRDRIVEN	0x01

static void	iop_pci_attach(device_t, device_t, void *);
static int	iop_pci_match(device_t, cfdata_t, void *);

CFATTACH_DECL_NEW(iop_pci, sizeof(struct iop_softc),
    iop_pci_match, iop_pci_attach, NULL, NULL);

static int
iop_pci_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa;
	u_int product, vendor;
	pcireg_t reg;

	pa = aux;

	/*
	 * Look for an "intelligent I/O processor" that adheres to the I2O
	 * specification.  Ignore the device if it doesn't support interrupt
	 * driven operation.
	 */
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_I2O &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_I2O_STANDARD &&
	    PCI_INTERFACE(pa->pa_class) == PCI_INTERFACE_I2O_INTRDRIVEN)
		return (1);

	/*
	 * Match boards that don't conform exactly to the spec.
	 */
	vendor = PCI_VENDOR(pa->pa_id);
	product = PCI_PRODUCT(pa->pa_id);

	if (vendor == PCI_VENDOR_DPT &&
	    (product == PCI_PRODUCT_DPT_RAID_I2O ||
	    product == PCI_PRODUCT_DPT_RAID_2005S))
		return (1);

	if (vendor == PCI_VENDOR_INTEL &&
	    (product == PCI_PRODUCT_INTEL_80960RM_2 ||
	    product == PCI_PRODUCT_INTEL_80960_RP)) {
		reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);
		if (PCI_VENDOR(reg) == PCI_VENDOR_PROMISE)
			return (1);
	}

	return (0);
}

static void
iop_pci_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa;
	struct iop_softc *sc;
	pci_chipset_tag_t pc;
	pci_intr_handle_t ih;
	const char *intrstr;
	pcireg_t reg;
	int i;
	char intrbuf[PCI_INTRSTR_LEN];

	sc = device_private(self);
	sc->sc_dev = self;
	pa = aux;
	pc = pa->pa_pc;
	printf(": ");

	/*
	 * The kernel always uses the first memory mapping to communicate
	 * with the IOP.
	 */
	for (i = PCI_MAPREG_START; i < PCI_MAPREG_END; i += 4) {
		reg = pci_conf_read(pc, pa->pa_tag, i);
		if (PCI_MAPREG_TYPE(reg) == PCI_MAPREG_TYPE_MEM) {
			sc->sc_memaddr = PCI_MAPREG_MEM_ADDR(reg);
			break;
		}
	}
	if (i == PCI_MAPREG_END) {
		printf("can't find mapping\n");
		return;
	}

	/* Map the register window. */
	if (pci_mapreg_map(pa, i, PCI_MAPREG_TYPE_MEM, 0, &sc->sc_iot,
	    &sc->sc_ioh, NULL, NULL)) {
		aprint_error_dev(self, "can't map register window\n");
		return;
	}

	/* Map the 2nd register window. */
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_DPT &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_DPT_RAID_2005S) {
		i += 4;	/* next BAR */
		if (i == PCI_MAPREG_END) {
			printf("can't find mapping\n");
			return;
		}

#if 0
		/* Should we check it? (see FreeBSD's asr driver) */
		reg = pci_conf_read(pc, pa->pa_tag, PCI_SUBSYS_ID_REG);
		printf("subid %x, %x\n", PCI_VENDOR(reg), PCI_PRODUCT(reg));
#endif
		if (pci_mapreg_map(pa, i, PCI_MAPREG_TYPE_MEM, 0,
		    &sc->sc_msg_iot, &sc->sc_msg_ioh, NULL, NULL)) {
			aprint_error_dev(self, "can't map 2nd register window\n");
			return;
		}
	} else {
		/* iop devices other than 2005S */
		sc->sc_msg_iot = sc->sc_iot;
		sc->sc_msg_ioh = sc->sc_ioh;
	}

	sc->sc_pcibus = pa->pa_bus;
	sc->sc_pcidev = pa->pa_device;
	sc->sc_dmat = pa->pa_dmat;
	sc->sc_bus_memt = pa->pa_memt;
	sc->sc_bus_iot = pa->pa_iot;

	/* Enable the device. */
	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
		       reg | PCI_COMMAND_MASTER_ENABLE);

	/* Map and establish the interrupt.. */
	if (pci_intr_map(pa, &ih)) {
		printf("can't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_BIO, iop_intr, sc);
	if (sc->sc_ih == NULL) {
		printf("can't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}

	/* Attach to the bus-independent code. */
	iop_init(sc, intrstr);
}

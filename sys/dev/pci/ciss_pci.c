/*	$NetBSD: ciss_pci.c,v 1.11 2014/03/29 19:28:24 christos Exp $	*/
/*	$OpenBSD: ciss_pci.c,v 1.9 2005/12/13 15:56:01 brad Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ciss_pci.c,v 1.11 2014/03/29 19:28:24 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>

#include <sys/bus.h>

#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsipi_disk.h>
#include <dev/scsipi/scsipiconf.h>

#include <dev/ic/cissreg.h>
#include <dev/ic/cissvar.h>

#define	CISS_BAR	0x10

int	ciss_pci_match(device_t, cfdata_t, void *);
void	ciss_pci_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(ciss_pci, sizeof(struct ciss_softc),
	ciss_pci_match, ciss_pci_attach, NULL, NULL);

const struct {
	int vendor;
	int product;
	const char *name;
} ciss_pci_devices[] = {
	{
		PCI_VENDOR_COMPAQ,
		PCI_PRODUCT_COMPAQ_CSA532,
		"Compaq Smart Array 532"
	},
	{
		PCI_VENDOR_COMPAQ,
		PCI_PRODUCT_COMPAQ_CSA5300,
		"Compaq Smart Array 5300 V1"
	},
	{
		PCI_VENDOR_COMPAQ,	
		PCI_PRODUCT_COMPAQ_CSA5300_2,
		"Compaq Smart Array 5300 V2"
	},
	{
		PCI_VENDOR_COMPAQ,
		PCI_PRODUCT_COMPAQ_CSA5312,
		"Compaq Smart Array 5312"
	},
	{
		PCI_VENDOR_COMPAQ,
		PCI_PRODUCT_COMPAQ_CSA5i,
		"Compaq Smart Array 5i"
	},
	{
		PCI_VENDOR_COMPAQ,
		PCI_PRODUCT_COMPAQ_CSA5i_2,
		"Compaq Smart Array 5i V2"
	},
	{
		PCI_VENDOR_COMPAQ,
		PCI_PRODUCT_COMPAQ_CSA6i,
		"Compaq Smart Array 6i"
	},
	{
		PCI_VENDOR_COMPAQ,
		PCI_PRODUCT_COMPAQ_CSA641,
		"Compaq Smart Array 641"
	},
	{
		PCI_VENDOR_COMPAQ,
		PCI_PRODUCT_COMPAQ_CSA642,
		"Compaq Smart Array 642"
	},
	{
		PCI_VENDOR_COMPAQ,
		PCI_PRODUCT_COMPAQ_CSA6400,
		"Compaq Smart Array 6400"
	},
	{
		PCI_VENDOR_COMPAQ,
		PCI_PRODUCT_COMPAQ_CSA6400EM,
		"Compaq Smart Array 6400EM"
	},
	{
		PCI_VENDOR_COMPAQ,
		PCI_PRODUCT_COMPAQ_CSA6422,
		"Compaq Smart Array 6422"
	},
	{
		PCI_VENDOR_COMPAQ,
		PCI_PRODUCT_COMPAQ_CSA64XX,
		"Compaq Smart Array 64XX"
	},
	{
		PCI_VENDOR_HP,
		PCI_PRODUCT_HP_HPSAE200,
		"Smart Array E200"
	},
	{
		PCI_VENDOR_HP,
		PCI_PRODUCT_HP_HPSAE200I_1,
		"HP Smart Array E200I-1"
	},
	{
		PCI_VENDOR_HP,
		PCI_PRODUCT_HP_HPSAE200I_2,
		"HP Smart Array E200I-2"
	},
	{
		PCI_VENDOR_HP,
		PCI_PRODUCT_HP_HPSAE200I_3,
		"HP Smart Array E200I-3"
	},
	{
		PCI_VENDOR_HP,
		PCI_PRODUCT_HP_HPSAP600,
		"HP Smart Array P600"
	},
	{
		PCI_VENDOR_HP,
		PCI_PRODUCT_HP_HPSAP800,
		"HP Smart Array P800"
	},
	{
		PCI_VENDOR_HP,
		PCI_PRODUCT_HP_HPSAV100,
		"HP Smart Array V100"
	},
	{
		PCI_VENDOR_HP,
		PCI_PRODUCT_HP_HPSA_1,
		"HP Smart Array 1"
	},
	{
		PCI_VENDOR_HP,
		PCI_PRODUCT_HP_HPSA_2,
		"HP Smart Array 2"
	},
	{
		PCI_VENDOR_HP,
		PCI_PRODUCT_HP_HPSA_3,
		"HP Smart Array 3"
	},
	{
		PCI_VENDOR_HP,
		PCI_PRODUCT_HP_HPSA_4,
		"HP Smart Array 4"
	},
	{
		PCI_VENDOR_HP,
		PCI_PRODUCT_HP_HPSA_5,
		"HP Smart Array 5"
	},
	{
		PCI_VENDOR_HP,
		PCI_PRODUCT_HP_HPSA_6,
		"HP Smart Array 6"
	},
	{
		PCI_VENDOR_HP,
		PCI_PRODUCT_HP_HPSA_7,
		"HP Smart Array 7"
	},
	{
		PCI_VENDOR_HP,
		PCI_PRODUCT_HP_HPSA_8,
		"HP Smart Array 8"
	},
	{
		PCI_VENDOR_HP,
		PCI_PRODUCT_HP_HPSA_9,
		"HP Smart Array 9"
	},
	{
		PCI_VENDOR_HP,
		PCI_PRODUCT_HP_HPSA_10,
		"HP Smart Array 10"
	},
	{
		PCI_VENDOR_HP,
		PCI_PRODUCT_HP_HPSA_11,
		"HP Smart Array 11"
	},
	{
		PCI_VENDOR_HP,
		PCI_PRODUCT_HP_HPSA_12,
		"HP Smart Array 12"
	},
	{
		PCI_VENDOR_HP,
		PCI_PRODUCT_HP_HPSA_13,
		"HP Smart Array 13"
	},
	{
		0,
		0,
		NULL
	}
};

int
ciss_pci_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;
	pcireg_t reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);
	int i;

	for (i = 0; ciss_pci_devices[i].vendor; i++)
	{
		if ((PCI_VENDOR(pa->pa_id) == ciss_pci_devices[i].vendor &&
		     PCI_PRODUCT(pa->pa_id) == ciss_pci_devices[i].product) ||
		    (PCI_VENDOR(reg) == ciss_pci_devices[i].vendor &&
		     PCI_PRODUCT(reg) == ciss_pci_devices[i].product))
			return 1;
	}

	return 0;
}

void
ciss_pci_attach(device_t parent, device_t self, void *aux)
{
	struct ciss_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	bus_size_t size, cfgsz;
	pci_intr_handle_t ih;
	const char *intrstr;
	int cfg_bar, memtype;
	pcireg_t reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);
	int i;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;

	for (i = 0; ciss_pci_devices[i].vendor; i++)
	{
		if ((PCI_VENDOR(pa->pa_id) == ciss_pci_devices[i].vendor &&
		     PCI_PRODUCT(pa->pa_id) == ciss_pci_devices[i].product) ||
		    (PCI_VENDOR(reg) == ciss_pci_devices[i].vendor &&
		     PCI_PRODUCT(reg) == ciss_pci_devices[i].product))
		{
			printf(": %s\n", ciss_pci_devices[i].name);
			break;
		}
	}

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, CISS_BAR);
	if (memtype != (PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT) &&
	    memtype != (PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT)) {
		printf(": wrong BAR type\n");
		return;
	}
	if (pci_mapreg_map(pa, CISS_BAR, memtype, 0,
	    &sc->sc_iot, &sc->sc_ioh, NULL, &size)) {
		printf(": can't map controller i/o space\n");
		return;
	}
	sc->sc_dmat = pa->pa_dmat;

	sc->iem = CISS_READYENA;
	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);
	if (PCI_VENDOR(reg) == PCI_VENDOR_COMPAQ &&
	    (PCI_PRODUCT(reg) == PCI_PRODUCT_COMPAQ_CSA5i ||
	     PCI_PRODUCT(reg) == PCI_PRODUCT_COMPAQ_CSA532 ||
	     PCI_PRODUCT(reg) == PCI_PRODUCT_COMPAQ_CSA5312))
		sc->iem = CISS_READYENAB;

	cfg_bar = bus_space_read_2(sc->sc_iot, sc->sc_ioh, CISS_CFG_BAR);
	sc->cfgoff = bus_space_read_4(sc->sc_iot, sc->sc_ioh, CISS_CFG_OFF);
	if (cfg_bar != CISS_BAR) {
		if (pci_mapreg_map(pa, cfg_bar, PCI_MAPREG_TYPE_MEM, 0,
		    NULL, &sc->cfg_ioh, NULL, &cfgsz)) {
			printf(": can't map controller config space\n");  
			bus_space_unmap(sc->sc_iot, sc->sc_ioh, size);
			return;
		}
	} else {
		sc->cfg_ioh = sc->sc_ioh;
		cfgsz = size;
	}

	if (sc->cfgoff + sizeof(struct ciss_config) > cfgsz) {
		printf(": unfit config space\n");
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, size);
		if (cfg_bar != CISS_BAR)
			bus_space_unmap(sc->sc_iot, sc->cfg_ioh, cfgsz);
		return;
	}

	/* disable interrupts until ready */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, CISS_IMR,
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, CISS_IMR) | sc->iem);

	if (pci_intr_map(pa, &ih)) {
		printf(": can't map interrupt\n");
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, size);
		if (cfg_bar != CISS_BAR)
			bus_space_unmap(sc->sc_iot, sc->cfg_ioh, cfgsz);
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO, ciss_intr, sc);
	if (!sc->sc_ih) {
		aprint_error_dev(sc->sc_dev, "can't establish interrupt");
		if (intrstr)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, size);
		if (cfg_bar != CISS_BAR)
			bus_space_unmap(sc->sc_iot, sc->cfg_ioh, cfgsz);
	}

	printf("%s: interrupting at %s\n%s", device_xname(sc->sc_dev), intrstr,
	       device_xname(sc->sc_dev));

	if (ciss_attach(sc)) {
		pci_intr_disestablish(pa->pa_pc, sc->sc_ih);
		sc->sc_ih = NULL;
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, size);
		if (cfg_bar != CISS_BAR)
			bus_space_unmap(sc->sc_iot, sc->cfg_ioh, cfgsz);
		return;
	}

	/* enable interrupts now */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, CISS_IMR,
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, CISS_IMR) & ~sc->iem);
}

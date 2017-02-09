/* $NetBSD: siisata_pci.c,v 1.13 2014/03/29 19:28:25 christos Exp $ */

/*
 * Copyright (c) 2006 Manuel Bouyer.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * Copyright (c) 2007, 2008, 2009 Jonathan A. Kollasch.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: siisata_pci.c,v 1.13 2014/03/29 19:28:25 christos Exp $");

#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/ic/siisatavar.h>

struct siisata_pci_softc {
	struct siisata_softc si_sc;
	pci_chipset_tag_t sc_pc;
	pcitag_t sc_pcitag;
	void * sc_ih;
};

static int siisata_pci_match(device_t, cfdata_t, void *);
static void siisata_pci_attach(device_t, device_t, void *);
static int siisata_pci_detach(device_t, int);
static bool siisata_pci_resume(device_t, const pmf_qual_t *);

struct siisata_pci_board {
	pci_vendor_id_t		spb_vend;
	pci_product_id_t	spb_prod;
	uint16_t		spb_port;
	uint16_t		spb_chip;
};

static const struct siisata_pci_board siisata_pci_boards[] = {
	{
		.spb_vend = PCI_VENDOR_CMDTECH,
		.spb_prod = PCI_PRODUCT_CMDTECH_3124,
		.spb_port = 4,
		.spb_chip = 3124,
	},
	{
		.spb_vend = PCI_VENDOR_CMDTECH,
		.spb_prod = PCI_PRODUCT_CMDTECH_3132,
		.spb_port = 2, 
		.spb_chip = 3132,
	},
	{
		.spb_vend = PCI_VENDOR_CMDTECH,
		.spb_prod = PCI_PRODUCT_CMDTECH_3531,
		.spb_port = 1,
		.spb_chip = 3531,
	},
};

CFATTACH_DECL_NEW(siisata_pci, sizeof(struct siisata_pci_softc),
    siisata_pci_match, siisata_pci_attach, siisata_pci_detach, NULL);

static const struct siisata_pci_board *
siisata_pci_lookup(const struct pci_attach_args * pa)
{
	int i;

	for (i = 0; i < __arraycount(siisata_pci_boards); i++) {
		if (siisata_pci_boards[i].spb_vend != PCI_VENDOR(pa->pa_id))
			continue;
		if (siisata_pci_boards[i].spb_prod == PCI_PRODUCT(pa->pa_id))
			return &siisata_pci_boards[i];
	}

	return NULL;
}

static int
siisata_pci_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (siisata_pci_lookup(pa) != NULL)
		return 3;

	return 0;
}

static void
siisata_pci_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct siisata_pci_softc *psc = device_private(self);
	struct siisata_softc *sc = &psc->si_sc;
	const char *intrstr;
	pcireg_t csr, memtype;
	const struct siisata_pci_board *spbp;
	pci_intr_handle_t intrhandle;
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	uint32_t gcreg;
	int memh_valid;
	bus_size_t grsize, prsize;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_atac.atac_dev = self;
	
	psc->sc_pc = pa->pa_pc;
	psc->sc_pcitag = pa->pa_tag;

	pci_aprint_devinfo(pa, "SATA-II HBA");

	/* map BAR 0, global registers */
	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, SIISATA_PCI_BAR0);
	switch (memtype) {
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
		memh_valid = (pci_mapreg_map(pa, SIISATA_PCI_BAR0,
			memtype, 0, &memt, &memh, NULL, &grsize) == 0);
		break;
	default:
		memh_valid = 0;
	}
	if (memh_valid) {
		sc->sc_grt = memt;
		sc->sc_grh = memh;
		sc->sc_grs = grsize;
	} else {
		aprint_error_dev(self, "couldn't map global registers\n");
		return;
	}

	/* map BAR 1, port registers */
	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, SIISATA_PCI_BAR1);
	switch (memtype) {
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
		memh_valid = (pci_mapreg_map(pa, SIISATA_PCI_BAR1,
			memtype, 0, &memt, &memh, NULL, &prsize) == 0);
		break;
	default:
		memh_valid = 0;
	}
	if (memh_valid) {
		sc->sc_prt = memt;
		sc->sc_prh = memh;
		sc->sc_prs = prsize;
	} else {
		bus_space_unmap(sc->sc_grt, sc->sc_grh, grsize);
		aprint_error_dev(self, "couldn't map port registers\n");
		return;
	}

	if (pci_dma64_available(pa))
		sc->sc_dmat = pa->pa_dmat64;
	else
		sc->sc_dmat = pa->pa_dmat;

	/* map interrupt */
	if (pci_intr_map(pa, &intrhandle) != 0) {
		bus_space_unmap(sc->sc_grt, sc->sc_grh, grsize);
		bus_space_unmap(sc->sc_prt, sc->sc_prh, prsize);
		aprint_error_dev(self, "couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, intrhandle, intrbuf, sizeof(intrbuf));
	psc->sc_ih = pci_intr_establish(pa->pa_pc, intrhandle,
	    IPL_BIO, siisata_intr, sc);
	if (psc->sc_ih == NULL) {
		bus_space_unmap(sc->sc_grt, sc->sc_grh, grsize);
		bus_space_unmap(sc->sc_prt, sc->sc_prh, prsize);
		aprint_error_dev(self, "couldn't establish interrupt at %s\n",
			intrstr);
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n",
		intrstr ? intrstr : "unknown interrupt");

	/* fill in number of ports on this device */
	spbp = siisata_pci_lookup(pa);
	KASSERT(spbp != NULL);
	sc->sc_atac.atac_nchannels = spbp->spb_port;

	/* set the necessary bits in case the firmware didn't */
	csr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	csr |= PCI_COMMAND_MASTER_ENABLE;
	csr |= PCI_COMMAND_MEM_ENABLE;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, csr);

	gcreg = GRREAD(sc, GR_GC);

	aprint_verbose_dev(self, "SiI%d, %sGb/s\n",
		spbp->spb_chip, (gcreg & GR_GC_3GBPS) ? "3.0" : "1.5" );
	if (spbp->spb_chip == 3124) {
		short width;
		short speed;
		char pcix = 1;

		width = (gcreg & GR_GC_REQ64) ? 64 : 32;

		switch (gcreg & (GR_GC_DEVSEL | GR_GC_STOP | GR_GC_TRDY)) {
		case 0:
			speed = (gcreg & GR_GC_M66EN) ? 66 : 33;
			pcix = 0;
			break;
		case GR_GC_TRDY:
			speed = 66;
			break;
		case GR_GC_STOP:
			speed = 100;
			break;
		case GR_GC_STOP | GR_GC_TRDY:
			speed = 133;
			break;
		default:
			speed = -1;
			break;
		}
		aprint_verbose_dev(self, "%hd-bit %hdMHz PCI%s\n",
			width, speed, pcix ? "-X" : "");
	}

	siisata_attach(sc);

	if (!pmf_device_register(self, NULL, siisata_pci_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

static int
siisata_pci_detach(device_t dv, int flags)
{
	struct siisata_pci_softc *psc = device_private(dv);
	struct siisata_softc *sc = &psc->si_sc;
	int rv;

	rv = siisata_detach(sc, flags);
	if (rv)
		return rv;

	if (psc->sc_ih != NULL) {
		pci_intr_disestablish(psc->sc_pc, psc->sc_ih);
	}

	bus_space_unmap(sc->sc_prt, sc->sc_prh, sc->sc_prs);
	bus_space_unmap(sc->sc_grt, sc->sc_grh, sc->sc_grs);
	
	return 0;
}

static bool
siisata_pci_resume(device_t dv, const pmf_qual_t *qual)
{
	struct siisata_pci_softc *psc = device_private(dv);
	struct siisata_softc *sc = &psc->si_sc;
	int s;

	s = splbio();
	siisata_resume(sc);
	splx(s);
	
	return true;
}

/* $NetBSD: mfi_pci.c,v 1.18 2014/03/29 19:28:25 christos Exp $ */
/* $OpenBSD: mfi_pci.c,v 1.11 2006/08/06 04:40:08 brad Exp $ */
/*
 * Copyright (c) 2006 Marco Peereboom <marco@peereboom.us>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mfi_pci.c,v 1.18 2014/03/29 19:28:25 christos Exp $");

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
#include <dev/scsipi/scsiconf.h>

#include <dev/ic/mfireg.h>
#include <dev/ic/mfivar.h>

#define	MFI_BAR		0x10
#define	MFI_BAR_GEN2	0x14
#define	MFI_PCI_MEMSIZE	0x2000 /* 8k */

struct mfi_pci_softc {
	struct mfi_softc	psc_sc;
	pci_chipset_tag_t	psc_pc;
};

const struct mfi_pci_device *mfi_pci_find_device(struct pci_attach_args *);
int	mfi_pci_match(device_t, cfdata_t, void *);
void	mfi_pci_attach(device_t, device_t, void *);
int	mfi_pci_detach(device_t, int);

CFATTACH_DECL3_NEW(mfi_pci, sizeof(struct mfi_pci_softc),
    mfi_pci_match, mfi_pci_attach, mfi_pci_detach, NULL, mfi_rescan,
    mfi_childdetached, DVF_DETACH_SHUTDOWN);

struct mfi_pci_subtype {
	pcireg_t 	st_vendor;
	pcireg_t 	st_product;
	const char 	*st_string;
};

static const struct mfi_pci_subtype mfi_1078_subtypes[] = {
	{ PCI_VENDOR_SYMBIOS, 	0x1006, 	"SAS 8888ELP" },
	{ PCI_VENDOR_SYMBIOS, 	0x100a, 	"SAS 8708ELP" },
	{ PCI_VENDOR_SYMBIOS, 	0x100f, 	"SAS 8708E" },
	{ PCI_VENDOR_SYMBIOS, 	0x1012, 	"SAS 8704ELP" },
	{ PCI_VENDOR_SYMBIOS, 	0x1013, 	"SAS 8708EM2" },
	{ PCI_VENDOR_SYMBIOS, 	0x1016, 	"SAS 8880EM2" },
	{ PCI_VENDOR_DELL, 	0x1f0a, 	"Dell PERC 6/e" },
	{ PCI_VENDOR_DELL, 	0x1f0b, 	"Dell PERC 6/i" },
	{ PCI_VENDOR_DELL, 	0x1f0c, 	"Dell PERC 6/i integrated" },
	{ PCI_VENDOR_DELL, 	0x1f0d, 	"Dell CERC 6/i" },
	{ PCI_VENDOR_DELL, 	0x1f11, 	"Dell CERC 6/i integrated" },
	{ 0, 			0, 		NULL }
};

static const struct mfi_pci_subtype mfi_perc5_subtypes[] = {
	{ PCI_VENDOR_DELL,	0x1f01, 	"Dell PERC 5/e" },
	{ PCI_VENDOR_DELL, 	0x1f02, 	"Dell PERC 5/i" },
	{ PCI_VENDOR_DELL, 	0x1f03, 	"Dell PERC 5/i integrated" },
	{ 0, 			0, 		NULL }
};

static const struct mfi_pci_subtype mfi_gen2_subtypes[] = {
	{ PCI_VENDOR_SYMBIOS,	0x9261,		"SAS 9260-8i" },
	{ PCI_VENDOR_SYMBIOS,	0x9263,		"SAS 9261-8i" },
	{ PCI_VENDOR_IBM,	0x03c7,		"IBM ServeRAID M5014 SAS/SATA" },
	{ PCI_VENDOR_DELL,	0x1f15,		"Dell PERC H800 Adapter" },
	{ PCI_VENDOR_DELL,	0x1f16,		"Dell PERC H700 Adapter" },
	{ PCI_VENDOR_DELL,	0x1f17,		"Dell PERC H700 Integrated" },
	{ PCI_VENDOR_DELL,	0x1f18,		"Dell PERC H700 Modular" },
	{ PCI_VENDOR_DELL,	0x1f19,		"Dell PERC H700" },
	{ PCI_VENDOR_DELL,	0x1f1a,		"Dell PERC H800 Proto Adapter" },
	{ PCI_VENDOR_DELL,	0x1f1b,		"Dell PERC H800" },
	{ 0x0,			0,		"" }
};

static const struct mfi_pci_subtype mfi_skinny_subtypes[] = {
	{ PCI_VENDOR_IBM,	0x03b1,		"IBM ServeRAID M1015 SAS/SATA" },
	{ 0x0,			0,		"" }
};

static const struct mfi_pci_subtype mfi_tbolt_subtypes[] = {
	{ PCI_VENDOR_SYMBIOS,	0x9265,		"SAS 9265-8i" },
	{ PCI_VENDOR_DELL,	0x1f2d,		"Dell PERC H810 Adapter" },
	{ PCI_VENDOR_DELL,	0x1f30,		"Dell PERC H710 Embedded" },
	{ PCI_VENDOR_DELL,	0x1f31,		"Dell PERC H710P Adapter" },
	{ PCI_VENDOR_DELL,	0x1f33,		"Dell PERC H710P Mini (blades)" },
	{ PCI_VENDOR_DELL,	0x1f34,		"Dell PERC H710P Mini (blades)" },
	{ PCI_VENDOR_DELL,	0x1f35,		"Dell PERC H710 Adapter" },
	{ PCI_VENDOR_DELL,	0x1f37,		"Dell PERC H710 Mini (blades)" },
	{ PCI_VENDOR_DELL,	0x1f38,		"Dell PERC H710 Mini (monolithics)" },
	{ PCI_VENDOR_INTEL,	0x9265,		"Intel (R) RAID Controller RS25DB080" },
	{ PCI_VENDOR_INTEL,	0x9285,		"Intel (R) RAID Controller RS25NB008" },
	{ 0x0,			0,		"" }
};

static const
struct mfi_pci_device {
	pcireg_t			mpd_vendor;
	pcireg_t			mpd_product;
	enum mfi_iop			mpd_iop;
	const struct mfi_pci_subtype 	*mpd_subtype;
} mfi_pci_devices[] = {
	{ PCI_VENDOR_SYMBIOS, 	PCI_PRODUCT_SYMBIOS_MEGARAID_SAS,
	  MFI_IOP_XSCALE, 	NULL },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_MEGARAID_VERDE_ZCR,
	  MFI_IOP_XSCALE,	NULL },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS1078,
	  MFI_IOP_PPC,		mfi_1078_subtypes },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS1078DE,
	  MFI_IOP_PPC,		mfi_1078_subtypes },
	{ PCI_VENDOR_DELL,	PCI_PRODUCT_DELL_PERC_5,
	  MFI_IOP_XSCALE,	mfi_perc5_subtypes },
	{ PCI_VENDOR_DELL,	PCI_PRODUCT_DELL_PERC_6,
	  MFI_IOP_PPC, 		mfi_1078_subtypes },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS2108_1,
	  MFI_IOP_GEN2,		mfi_gen2_subtypes },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS2108_2,
	  MFI_IOP_GEN2,		mfi_gen2_subtypes },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS2008_1,
	  MFI_IOP_SKINNY,	mfi_skinny_subtypes },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_MEGARAID_2208,
	  MFI_IOP_TBOLT,	mfi_tbolt_subtypes },
};

const struct mfi_pci_device *
mfi_pci_find_device(struct pci_attach_args *pa)
{
	const struct mfi_pci_device *mpd;
	int i;

	for (i = 0; i < __arraycount(mfi_pci_devices); i++) {
		mpd = &mfi_pci_devices[i];

		if (mpd->mpd_vendor == PCI_VENDOR(pa->pa_id) &&
		    mpd->mpd_product == PCI_PRODUCT(pa->pa_id))
			return mpd;
	}

	return NULL;
}

int
mfi_pci_match(device_t parent, cfdata_t match, void *aux)
{
	return (mfi_pci_find_device(aux) != NULL) ? 1 : 0;
}

int
mfi_pci_detach(device_t self, int flags)
{
	struct mfi_pci_softc	*psc = device_private(self);
	struct mfi_softc	*sc = &psc->psc_sc;
	int error;

	if ((error = mfi_detach(sc, flags)) != 0)
		return error;

	/* xxx */
	pci_intr_disestablish(psc->psc_pc, sc->sc_ih);
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_size);
	return 0;
}

void
mfi_pci_attach(device_t parent, device_t self, void *aux)
{
	struct mfi_pci_softc	*psc = device_private(self);
	struct mfi_softc	*sc = &psc->psc_sc;
	struct pci_attach_args	*pa = aux;
	const struct mfi_pci_device *mpd;
	const struct mfi_pci_subtype *st;
	const char		*intrstr;
	pci_intr_handle_t	ih;
	pcireg_t		csr;
	int			regbar;
	const char 		*subtype = NULL;
	uint32_t		subsysid;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;
	psc->psc_pc = pa->pa_pc;

	mpd = mfi_pci_find_device(pa);
	if (mpd == NULL) {
		printf(": can't find matching pci device\n");
		return;
	}

	if (mpd->mpd_iop == MFI_IOP_GEN2 || mpd->mpd_iop == MFI_IOP_SKINNY ||
	    mpd->mpd_iop == MFI_IOP_TBOLT)
		regbar = MFI_BAR_GEN2;
	else
		regbar = MFI_BAR;

	csr = pci_mapreg_type(pa->pa_pc, pa->pa_tag, regbar);
	csr |= PCI_MAPREG_MEM_TYPE_32BIT;
	if (pci_mapreg_map(pa, regbar, csr, 0,
	    &sc->sc_iot, &sc->sc_ioh, NULL, &sc->sc_size)) {
		aprint_error(": can't map controller pci space\n");
		return;
	}

	sc->sc_dmat = pa->pa_dmat;
	if (mpd->mpd_iop == MFI_IOP_TBOLT && pci_dma64_available(pa)) {
		sc->sc_datadmat = pa->pa_dmat64;
		sc->sc_64bit_dma = 1;
	} else {
		sc->sc_datadmat = pa->pa_dmat;
		sc->sc_64bit_dma = 0;
	}

	if (pci_intr_map(pa, &ih)) {
		aprint_error(": can't map interrupt\n");
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_size);
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih, intrbuf, sizeof(intrbuf));
	if (mpd->mpd_iop == MFI_IOP_TBOLT) {
		sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO,
		    mfi_tbolt_intrh, sc);
	} else {
		sc->sc_ih =
		    pci_intr_establish(pa->pa_pc, ih, IPL_BIO, mfi_intr, sc);
	}
	if (!sc->sc_ih) {
		aprint_error(": can't establish interrupt");
		if (intrstr)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_size);
		return;
	}

	subsysid = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);
	if (mpd->mpd_subtype != NULL) {
		st = mpd->mpd_subtype;
		while (st->st_vendor != 0) {
			if (PCI_VENDOR(subsysid) == st->st_vendor &&
			    PCI_PRODUCT(subsysid) == st->st_product) {
				subtype = st->st_string;
				break;
			}
			st++;
		}
		if (subtype) {
			aprint_normal(": %s\n", subtype);
		} else {
			aprint_normal(": vendor 0x%x product 0x%x\n",
			    PCI_VENDOR(subsysid), PCI_PRODUCT(subsysid));
		}
	}

	aprint_normal("%s: interrupting at %s\n", DEVNAME(sc), intrstr);

	if (mfi_attach(sc, mpd->mpd_iop)) {
		aprint_error("%s: can't attach", DEVNAME(sc));
		pci_intr_disestablish(pa->pa_pc, sc->sc_ih);
		sc->sc_ih = NULL;
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_size);
	}
}

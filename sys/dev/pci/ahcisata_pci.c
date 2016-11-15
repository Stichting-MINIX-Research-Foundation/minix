/*	$NetBSD: ahcisata_pci.c,v 1.36 2014/03/29 19:28:24 christos Exp $	*/

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ahcisata_pci.c,v 1.36 2014/03/29 19:28:24 christos Exp $");

#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/disklabel.h>
#include <sys/pmf.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>
#include <dev/ic/ahcisatavar.h>

struct ahci_pci_quirk { 
	pci_vendor_id_t  vendor;	/* Vendor ID */
	pci_product_id_t product;	/* Product ID */
	int              quirks;	/* quirks; same as sc_ahci_quirks */
};

static const struct ahci_pci_quirk ahci_pci_quirks[] = {
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP65_SATA,
	    AHCI_PCI_QUIRK_FORCE | AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP65_SATA2,
	    AHCI_PCI_QUIRK_FORCE | AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP65_SATA3,
	    AHCI_PCI_QUIRK_FORCE | AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP65_SATA4,
	    AHCI_PCI_QUIRK_FORCE | AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP65_AHCI_1,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP65_AHCI_2,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP65_AHCI_3,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP65_AHCI_4,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP67_SATA,
	    AHCI_PCI_QUIRK_FORCE | AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP67_SATA2,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP67_SATA3,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP67_SATA4,
	     AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP67_AHCI_1,
	     AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP67_AHCI_2,
	     AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP67_AHCI_3,
	     AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP67_AHCI_4,
	     AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP67_AHCI_5,
	     AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP67_AHCI_6,
	     AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP67_AHCI_7,
	     AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP67_AHCI_8,
	     AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP73_AHCI_1,
	    AHCI_PCI_QUIRK_FORCE | AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP73_AHCI_2,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP73_AHCI_3,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP73_AHCI_4,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP73_AHCI_5,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP73_AHCI_6,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP73_AHCI_7,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP73_AHCI_8,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP73_AHCI_9,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP73_AHCI_10,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP73_AHCI_11,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP73_AHCI_12,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP77_AHCI_1,
	    AHCI_PCI_QUIRK_FORCE | AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP77_AHCI_2,
	    AHCI_PCI_QUIRK_FORCE | AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP77_AHCI_3,
	    AHCI_PCI_QUIRK_FORCE | AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP77_AHCI_4,
	    AHCI_PCI_QUIRK_FORCE | AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP77_AHCI_5,
	    AHCI_PCI_QUIRK_FORCE | AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP77_AHCI_6,
	    AHCI_PCI_QUIRK_FORCE | AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP77_AHCI_7,
	    AHCI_PCI_QUIRK_FORCE | AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP77_AHCI_8,
	    AHCI_PCI_QUIRK_FORCE | AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP77_AHCI_9,
	    AHCI_PCI_QUIRK_FORCE | AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP77_AHCI_10,
	    AHCI_PCI_QUIRK_FORCE | AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP77_AHCI_11,
	    AHCI_PCI_QUIRK_FORCE | AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP77_AHCI_12,
	    AHCI_PCI_QUIRK_FORCE | AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP79_AHCI_1,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP79_AHCI_2,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP79_AHCI_3,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP79_AHCI_4,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP79_AHCI_5,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP79_AHCI_6,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP79_AHCI_7,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP79_AHCI_8,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP79_AHCI_9,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP79_AHCI_10,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP79_AHCI_11,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP79_AHCI_12,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_ALI, PCI_PRODUCT_ALI_M5288,
	    AHCI_PCI_QUIRK_FORCE },
	{ PCI_VENDOR_MARVELL, PCI_PRODUCT_MARVELL_88SE6121,
	    AHCI_PCI_QUIRK_FORCE | AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_MARVELL, PCI_PRODUCT_MARVELL_88SE6145,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_MARVELL2, PCI_PRODUCT_MARVELL2_88SE91XX,
	    AHCI_PCI_QUIRK_FORCE },
	/* ATI SB600 AHCI 64-bit DMA only works on some boards/BIOSes */
	{ PCI_VENDOR_ATI, PCI_PRODUCT_ATI_SB600_SATA_1,
	    AHCI_PCI_QUIRK_BAD64 | AHCI_QUIRK_BADPMPRESET },
	{ PCI_VENDOR_ATI, PCI_PRODUCT_ATI_SB700_SATA_AHCI,
	    AHCI_QUIRK_BADPMPRESET },
	{ PCI_VENDOR_ATI, PCI_PRODUCT_ATI_SB700_SATA_RAID,
	    AHCI_QUIRK_BADPMPRESET },
	{ PCI_VENDOR_ATI, PCI_PRODUCT_ATI_SB700_SATA_RAID5,
	    AHCI_QUIRK_BADPMPRESET },
	{ PCI_VENDOR_ATI, PCI_PRODUCT_ATI_SB700_SATA_FC,
	    AHCI_QUIRK_BADPMPRESET },
	{ PCI_VENDOR_ATI, PCI_PRODUCT_ATI_SB700_SATA_AHCI2,
	    AHCI_QUIRK_BADPMPRESET },
	{ PCI_VENDOR_VIATECH, PCI_PRODUCT_VIATECH_VT8237R_SATA,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_VIATECH, PCI_PRODUCT_VIATECH_VT8251_SATA,
	    AHCI_QUIRK_BADPMP },
	{ PCI_VENDOR_ASMEDIA, PCI_PRODUCT_ASMEDIA_ASM1061_01,
	    AHCI_PCI_QUIRK_FORCE },
	{ PCI_VENDOR_ASMEDIA, PCI_PRODUCT_ASMEDIA_ASM1061_02,
	    AHCI_PCI_QUIRK_FORCE },
	{ PCI_VENDOR_ASMEDIA, PCI_PRODUCT_ASMEDIA_ASM1061_11,
	    AHCI_PCI_QUIRK_FORCE },
	{ PCI_VENDOR_ASMEDIA, PCI_PRODUCT_ASMEDIA_ASM1061_12,
	    AHCI_PCI_QUIRK_FORCE },
};

struct ahci_pci_softc {
	struct ahci_softc ah_sc;
	pci_chipset_tag_t sc_pc;
	pcitag_t sc_pcitag;
	void * sc_ih;
};

static int  ahci_pci_has_quirk(pci_vendor_id_t, pci_product_id_t);
static int  ahci_pci_match(device_t, cfdata_t, void *);
static void ahci_pci_attach(device_t, device_t, void *);
static int  ahci_pci_detach(device_t, int);
static bool ahci_pci_resume(device_t, const pmf_qual_t *);


CFATTACH_DECL_NEW(ahcisata_pci, sizeof(struct ahci_pci_softc),
    ahci_pci_match, ahci_pci_attach, ahci_pci_detach, NULL);

static int
ahci_pci_has_quirk(pci_vendor_id_t vendor, pci_product_id_t product)
{
	int i;

	for (i = 0; i < __arraycount(ahci_pci_quirks); i++)
		if (vendor == ahci_pci_quirks[i].vendor &&
		    product == ahci_pci_quirks[i].product)
			return ahci_pci_quirks[i].quirks;
	return 0;
}

static int
ahci_pci_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;
	bus_space_tag_t regt;
	bus_space_handle_t regh;
	bus_size_t size;
	int ret = 0;
	bool force;

	force = ((ahci_pci_has_quirk( PCI_VENDOR(pa->pa_id),
	    PCI_PRODUCT(pa->pa_id)) & AHCI_PCI_QUIRK_FORCE) != 0);

	/* if wrong class and not forced by quirks, don't match */
	if ((PCI_CLASS(pa->pa_class) != PCI_CLASS_MASS_STORAGE ||
	    ((PCI_SUBCLASS(pa->pa_class) != PCI_SUBCLASS_MASS_STORAGE_SATA ||
	     PCI_INTERFACE(pa->pa_class) != PCI_INTERFACE_SATA_AHCI) &&
	     PCI_SUBCLASS(pa->pa_class) != PCI_SUBCLASS_MASS_STORAGE_RAID)) &&
	    (force == false))
		return 0;

	if (pci_mapreg_map(pa, AHCI_PCI_ABAR,
	    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &regt, &regh, NULL, &size) != 0)
		return 0;

	if ((PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_MASS_STORAGE_SATA &&
	     PCI_INTERFACE(pa->pa_class) == PCI_INTERFACE_SATA_AHCI) ||
	    (bus_space_read_4(regt, regh, AHCI_GHC) & AHCI_GHC_AE) ||
	    (force == true))
		ret = 3;

	bus_space_unmap(regt, regh, size);
	return ret;
}

static void
ahci_pci_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct ahci_pci_softc *psc = device_private(self);
	struct ahci_softc *sc = &psc->ah_sc;
	const char *intrstr;
	bool ahci_cap_64bit;
	bool ahci_bad_64bit;
	pci_intr_handle_t intrhandle;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_atac.atac_dev = self;

	if (pci_mapreg_map(pa, AHCI_PCI_ABAR,
	    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &sc->sc_ahcit, &sc->sc_ahcih, NULL, &sc->sc_ahcis) != 0) {
		aprint_error_dev(self, "can't map ahci registers\n");
		return;
	}
	psc->sc_pc = pa->pa_pc;
	psc->sc_pcitag = pa->pa_tag;

	pci_aprint_devinfo(pa, "AHCI disk controller");
	
	if (pci_intr_map(pa, &intrhandle) != 0) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, intrhandle,
	    intrbuf, sizeof(intrbuf));
	psc->sc_ih = pci_intr_establish(pa->pa_pc, intrhandle, IPL_BIO, ahci_intr, sc);
	if (psc->sc_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt\n");
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	sc->sc_dmat = pa->pa_dmat;

	sc->sc_ahci_quirks = ahci_pci_has_quirk(PCI_VENDOR(pa->pa_id),
					    PCI_PRODUCT(pa->pa_id));

	ahci_cap_64bit = (AHCI_READ(sc, AHCI_CAP) & AHCI_CAP_64BIT) != 0;
	ahci_bad_64bit = ((sc->sc_ahci_quirks & AHCI_PCI_QUIRK_BAD64) != 0);

	if (pci_dma64_available(pa) && ahci_cap_64bit) {
		if (!ahci_bad_64bit)
			sc->sc_dmat = pa->pa_dmat64;
		aprint_verbose_dev(self, "64-bit DMA%s\n",
		    (sc->sc_dmat == pa->pa_dmat) ? " unavailable" : "");
	}

	if (PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_MASS_STORAGE_RAID) {
		AHCIDEBUG_PRINT(("%s: RAID mode\n", AHCINAME(sc)), DEBUG_PROBE);
		sc->sc_atac_capflags = ATAC_CAP_RAID;
	} else {
		AHCIDEBUG_PRINT(("%s: SATA mode\n", AHCINAME(sc)), DEBUG_PROBE);
	}

	ahci_attach(sc);

	if (!pmf_device_register(self, NULL, ahci_pci_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

static int
ahci_pci_detach(device_t dv, int flags)
{
	struct ahci_pci_softc *psc;
	struct ahci_softc *sc;
	int rv;

	psc = device_private(dv);
	sc = &psc->ah_sc;

	if ((rv = ahci_detach(sc, flags)))
		return rv;

	pmf_device_deregister(dv);

	if (psc->sc_ih != NULL)
		pci_intr_disestablish(psc->sc_pc, psc->sc_ih);

	bus_space_unmap(sc->sc_ahcit, sc->sc_ahcih, sc->sc_ahcis);

	return 0;
}

static bool
ahci_pci_resume(device_t dv, const pmf_qual_t *qual)
{
	struct ahci_pci_softc *psc = device_private(dv);
	struct ahci_softc *sc = &psc->ah_sc;
	int s;

	s = splbio();
	ahci_resume(sc);
	splx(s);

	return true;
}

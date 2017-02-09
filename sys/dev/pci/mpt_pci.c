/*	$NetBSD: mpt_pci.c,v 1.23 2014/03/29 19:28:25 christos Exp $	*/

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Additional contributions by Garrett D'Amore on behalf of TELES AG.
 */

/*
 * mpt_pci.c:
 *
 * NetBSD PCI-specific routines for LSI Fusion adapters.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mpt_pci.c,v 1.23 2014/03/29 19:28:25 christos Exp $");

#include <dev/ic/mpt.h>			/* pulls in all headers */

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#define	MPT_PCI_MMBA		(PCI_MAPREG_START+0x04)

struct mpt_pci_softc {
	mpt_softc_t sc_mpt;

	pci_chipset_tag_t sc_pc;
	pcitag_t sc_tag;

	void *sc_ih;

	/* Saved volatile PCI configuration registers. */
	pcireg_t sc_pci_csr;
	pcireg_t sc_pci_bhlc;
	pcireg_t sc_pci_io_bar;
	pcireg_t sc_pci_mem0_bar[2];
	pcireg_t sc_pci_mem1_bar[2];
	pcireg_t sc_pci_rom_bar;
	pcireg_t sc_pci_int;
	pcireg_t sc_pci_pmcsr;
};

#define	MPP_F_FC	0x01	/* Fibre Channel adapter */
#define	MPP_F_DUAL	0x02	/* Dual port adapter */

static const struct mpt_pci_product {
	pci_vendor_id_t		mpp_vendor;
	pci_product_id_t	mpp_product;
} mpt_pci_products[] = {
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_1030 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_FC909 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_FC909A },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_FC929 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_FC929_1 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_FC919 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_FC919_1},
	{ PCI_VENDOR_SYMBIOS,   PCI_PRODUCT_SYMBIOS_FC929X },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_FC919X },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_FC929X },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_FC939X },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_FC949X },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS1064 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS1064A },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS1064E },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS1064E_2 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS1066 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS1066E },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS1068 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS1068_2 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS1068E },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS1068E_2 },
	{ 0,			0 }
};

static int
mpt_pci_match(device_t parent, cfdata_t cf, void *aux)
{
	struct pci_attach_args *pa = aux;
	const struct mpt_pci_product *mpp;

	for (mpp = mpt_pci_products; mpp->mpp_vendor != 0; mpp++) {
		if (PCI_VENDOR(pa->pa_id) == mpp->mpp_vendor &&
		    PCI_PRODUCT(pa->pa_id) == mpp->mpp_product)
			return (1);
	}

	return (0);
}

static void
mpt_pci_attach(device_t parent, device_t self, void *aux)
{
	struct mpt_pci_softc *psc = device_private(self);
	mpt_softc_t *mpt = &psc->sc_mpt;
	struct pci_attach_args *pa = aux;
	pci_intr_handle_t ih;
	const char *intrstr;
	pcireg_t reg, memtype;
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	int memh_valid;
	char intrbuf[PCI_INTRSTR_LEN];

	pci_aprint_devinfo(pa, NULL);

	psc->sc_pc = pa->pa_pc;
	psc->sc_tag = pa->pa_tag;

	mpt->sc_dev = self;
	mpt->sc_dmat = pa->pa_dmat;

	/*
	 * Map the device.
	 */
	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, MPT_PCI_MMBA);
	switch (memtype) {
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
		memh_valid = (pci_mapreg_map(pa, MPT_PCI_MMBA,
		    memtype, 0, &memt, &memh, NULL, NULL) == 0);
		break;

	default:
		memh_valid = 0;
	}

	if (memh_valid) {
		mpt->sc_st = memt;
		mpt->sc_sh = memh;
	} else {
		aprint_error_dev(mpt->sc_dev, "unable to map device registers\n");
		return;
	}

	/*
	 * Make sure the PCI command register is properly configured.
	 */
	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	reg |= PCI_COMMAND_MASTER_ENABLE;
	/* XXX PCI_COMMAND_INVALIDATE_ENABLE */
	/* XXX PCI_COMMAND_PARITY_ENABLE */
	/* XXX PCI_COMMAND_SERR_ENABLE */
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, reg);

	/*
	 * Disable split transactions on the 53c1020 and 53c1030 for
	 * revisions older than 8.
	 */
	if ((PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_SYMBIOS_1030) &&
	    (PCI_REVISION(pa->pa_class) < 0x08)) {
		aprint_normal_dev(mpt->sc_dev, "applying 1030 quirk\n");
		reg = pci_conf_read(pa->pa_pc, pa->pa_tag, 0x68);
		reg &= 0x8fffff;
		pci_conf_write(pa->pa_pc, pa->pa_tag, 0x68, reg);
	}

	/*
	 * Ensure that the ROM is disabled.
	 */
	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_MAPREG_ROM);
	reg &= ~1;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_MAPREG_ROM, reg);

	/*
	 * Map and establish our interrupt.
	 */
	if (pci_intr_map(pa, &ih) != 0) {
		aprint_error_dev(mpt->sc_dev, "unable to map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih, intrbuf, sizeof(intrbuf));
	psc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO, mpt_intr, mpt);
	if (psc->sc_ih == NULL) {
		aprint_error_dev(mpt->sc_dev, "unable to establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(mpt->sc_dev, "interrupting at %s\n",
	    intrstr);

	/* Disable interrupts on the part. */
	mpt_disable_ints(mpt);

	/* Allocate DMA memory. */
	if (mpt_dma_mem_alloc(mpt) != 0) {
		aprint_error_dev(mpt->sc_dev, "unable to allocate DMA memory\n");
		return;
	}

	/* Initialize the hardware. */
	if (mpt_init(mpt, MPT_DB_INIT_HOST) != 0) {
		/* Error message already printed. */
		return;
	}

	/* Attach to scsipi. */
	mpt_scsipi_attach(mpt);
}

CFATTACH_DECL_NEW(mpt_pci, sizeof(struct mpt_pci_softc),
    mpt_pci_match, mpt_pci_attach, NULL, NULL);


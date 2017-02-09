/*	$NetBSD: adv_pci.c,v 1.29 2014/10/18 08:33:28 snj Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc. All rights reserved.
 *
 * Author: Baldassare Dante Profeta <dante@mclink.it>
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
 * Device probe and attach routines for the following
 * Advanced Systems Inc. SCSI controllers:
 *
 *    Connectivity Products:
 *      ABP920 - Bus-Master PCI (16 CDB)
 *      ABP930 - Bus-Master PCI (16 CDB)		(Footnote 1)
 *      ABP930U - Bus-Master PCI Ultra (16 CDB)
 *      ABP930UA - Bus-Master PCI Ultra (16 CDB)
 *      ABP960 - Bus-Master PCI MAC/PC (16 CDB)		(Footnote 2)
 *      ABP960U - Bus-Master PCI MAC/PC Ultra (16 CDB)	(Footnote 2)
 *
 *   Single Channel Products:
 *      ABP940 - Bus-Master PCI (240 CDB)
 *      ABP940U - Bus-Master PCI Ultra (240 CDB)
 *      ABP970 - Bus-Master PCI MAC/PC (240 CDB)
 *      ABP970U - Bus-Master PCI MAC/PC Ultra (240 CDB)
 *      ABP940UW - Bus-Master PCI Ultra-Wide (240 CDB)
 *
 *   Multi Channel Products:
 *      ABP950 - Dual Channel Bus-Master PCI (240 CDB Per Channel)
 *      ABP980 - Four Channel Bus-Master PCI (240 CDB Per Channel)
 *      ABP980U - Four Channel Bus-Master PCI Ultra (240 CDB Per Channel)
 *
 *   Footnotes:
 *     1. This board has been sold by SIIG as the Fast SCSI Pro PCI.
 *     2. This board has been sold by Iomega as a Jaz Jet PCI adapter.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: adv_pci.c,v 1.29 2014/10/18 08:33:28 snj Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/device.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/advlib.h>
#include <dev/ic/adv.h>

/******************************************************************************/

#define PCI_BASEADR_IO        0x10

/******************************************************************************/
/*
 * Check the slots looking for a board we recognise
 * If we find one, note its address (slot) and call
 * the actual probe routine to check it out.
 */
static int
adv_pci_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ADVSYS)
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_ADVSYS_1200A:
		case PCI_PRODUCT_ADVSYS_1200B:
		case PCI_PRODUCT_ADVSYS_ULTRA:
			return (1);
		}

	return 0;
}

static void
adv_pci_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	ASC_SOFTC      *sc = device_private(self);
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	pci_intr_handle_t ih;
	pci_chipset_tag_t pc = pa->pa_pc;
	u_int32_t       command;
	const char     *intrstr;
	char intrbuf[PCI_INTRSTR_LEN];

	aprint_naive(": SCSI controller\n");

	sc->sc_dev = self;
	sc->sc_flags = 0x0;
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ADVSYS)
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_ADVSYS_1200A:
			aprint_normal(": AdvanSys ASC1200A SCSI adapter\n");
			break;

		case PCI_PRODUCT_ADVSYS_1200B:
			aprint_normal(": AdvanSys ASC1200B SCSI adapter\n");
			break;

		case PCI_PRODUCT_ADVSYS_ULTRA:
			switch (PCI_REVISION(pa->pa_class)) {
			case ASC_PCI_REVISION_3050:
				aprint_normal(
				    ": AdvanSys ABP-9xxUA SCSI adapter\n");
				break;

			case ASC_PCI_REVISION_3150:
				aprint_normal(
				    ": AdvanSys ABP-9xxU SCSI adapter\n");
				break;
			}
			break;

		default:
			aprint_error(": unknown model!\n");
			return;
		}


	/*
	 * Make sure IO/MEM/MASTER are enabled
	 */
	command = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	if ((command & (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
			PCI_COMMAND_MASTER_ENABLE)) !=
	    (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
	     PCI_COMMAND_MASTER_ENABLE)) {
		pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
		 command | (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
			    PCI_COMMAND_MASTER_ENABLE));
	}
	/*
	 * Latency timer settings.
	 */
	{
		u_int32_t       bhlcr;

		bhlcr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_BHLC_REG);

		if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ADVSYS_1200A ||
		    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ADVSYS_1200B) {
			bhlcr &= 0xFFFF00FFul;
			pci_conf_write(pa->pa_pc, pa->pa_tag,
					PCI_BHLC_REG, bhlcr);
		} else if ((PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ADVSYS_ULTRA)
			    && (PCI_LATTIMER(bhlcr) < 0x20)) {
			bhlcr &= 0xFFFF00FFul;
			bhlcr |= 0x00002000ul;
			pci_conf_write(pa->pa_pc, pa->pa_tag,
					PCI_BHLC_REG, bhlcr);
		}
	}


	/*
	 * Map Device Registers for I/O
	 */
	if (pci_mapreg_map(pa, PCI_BASEADR_IO, PCI_MAPREG_TYPE_IO, 0,
			&iot, &ioh, NULL, NULL)) {
		aprint_error_dev(sc->sc_dev, "unable to map device registers\n");
		return;
	}

	ASC_SET_CHIP_CONTROL(iot, ioh, ASC_CC_HALT);
	ASC_SET_CHIP_STATUS(iot, ioh, 0);

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_dmat = pa->pa_dmat;
	sc->pci_device_id = pa->pa_id;
	sc->bus_type = ASC_IS_PCI;
	sc->chip_version = ASC_GET_CHIP_VER_NO(iot, ioh);

	/*
	 * Initialize the board
	 */
	if (adv_init(sc))
		panic("adv_pci_attach: adv_init failed");

	/*
	 * Map Interrupt line
	 */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(sc->sc_dev, "couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));

	/*
	 * Establish Interrupt handler
	 */
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_BIO, adv_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(sc->sc_dev, "interrupting at %s\n", intrstr);

	/*
	 * Attach all the sub-devices we can find
	 */
	adv_attach(sc);
}

CFATTACH_DECL_NEW(adv_pci, sizeof(ASC_SOFTC),
    adv_pci_match, adv_pci_attach, NULL, NULL);

/* $NetBSD: adw_pci.c,v 1.28 2014/10/18 08:33:28 snj Exp $	 */

/*
 * Copyright (c) 1998, 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
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
 *      ABP-940UW	- Bus-Master PCI Ultra-Wide (253 CDB)
 *	ABP-940UW (68)	- Bus-Master PCI Ultra-Wide (253 CDB)
 *	ABP-940UWD	- Bus-Master PCI Ultra-Wide (253 CDB)
 *	ABP-970UW	- Bus-Master PCI Ultra-Wide (253 CDB)
 *	ASB-3940UW	- Bus-Master PCI Ultra-Wide (253 CDB)
 *	ASB-3940U2W-00	- Bus-Master PCI Ultra2-Wide (253 CDB)
 *	ASB-3940U3W-00	- Bus-Master PCI Ultra3-Wide (253 CDB)
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: adw_pci.c,v 1.28 2014/10/18 08:33:28 snj Exp $");

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

#include <dev/ic/adwlib.h>
#include <dev/ic/adwmcode.h>
#include <dev/ic/adw.h>

/******************************************************************************/

#define PCI_BASEADR_IO        0x10

/******************************************************************************/
/*
 * Check the slots looking for a board we recognise
 * If we find one, note its address (slot) and call
 * the actual probe routine to check it out.
 */
static int
adw_pci_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ADVSYS)
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_ADVSYS_WIDE:
		case PCI_PRODUCT_ADVSYS_U2W:
		case PCI_PRODUCT_ADVSYS_U3W:
			return (1);
		}

	return 0;
}


static void
adw_pci_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa = aux;
	ADW_SOFTC      *sc = device_private(self);
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	pci_intr_handle_t ih;
	pci_chipset_tag_t pc = pa->pa_pc;
	u_int32_t       command;
	const char     *intrstr;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;

	aprint_naive(": SCSI controller\n");

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ADVSYS)
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_ADVSYS_WIDE:
			sc->chip_type = ADW_CHIP_ASC3550;
			aprint_normal(
			    ": AdvanSys ASB-3940UW-00 SCSI adapter\n");
			break;

		case PCI_PRODUCT_ADVSYS_U2W:
			sc->chip_type = ADW_CHIP_ASC38C0800;
			aprint_normal(
			    ": AdvanSys ASB-3940U2W-00 SCSI adapter\n");
			break;

		case PCI_PRODUCT_ADVSYS_U3W:
			sc->chip_type = ADW_CHIP_ASC38C1600;
			aprint_normal(
			    ": AdvanSys ASB-3940U3W-00 SCSI adapter\n");
			break;

		default:
			aprint_error(": unknown model!\n");
			return;
		}


	/*
	 * Make sure IO/MEM/MASTER are enabled
	 */
	command = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	command |= PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
			PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, command);

	if ( (command & PCI_COMMAND_PARITY_ENABLE) == 0) {
		sc->cfg.control_flag |= CONTROL_FLAG_IGNORE_PERR;
	}
	/*
	 * Map Device Registers for I/O
	 */
	if (pci_mapreg_map(pa, PCI_BASEADR_IO, PCI_MAPREG_TYPE_IO, 0,
			   &iot, &ioh, NULL, NULL)) {
		aprint_error_dev(self, "unable to map device registers\n");
		return;
	}
	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_dmat = pa->pa_dmat;

	/*
	 * Initialize the board
	 */
	if (adw_init(sc)) {
		aprint_error_dev(self, "adw_init failed");
		return;
	}

	/*
	 * Map Interrupt line
	 */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));

	/*
	 * Establish Interrupt handler
	 */
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_BIO, adw_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	/*
	 * Attach all the sub-devices we can find
	 */
	adw_attach(sc);
}

CFATTACH_DECL_NEW(adw_pci, sizeof(ADW_SOFTC),
    adw_pci_match, adw_pci_attach, NULL, NULL);

/*	$NetBSD: adv_isa.c,v 1.20 2012/10/27 17:18:23 chs Exp $	*/

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
 *	ABP510/5150 - Bus-Master ISA (240 CDB) (Footnote 1)
 *      ABP5140 - Bus-Master ISA (16 CDB) (Footnote 1) (Footnote 2)
 *      ABP5142 - Bus-Master ISA with floppy (16 CDB) (Footnote 3)
 *
 *   Single Channel Products:
 *	ABP542 - Bus-Master ISA with floppy (240 CDB)
 *	ABP842 - Bus-Master VL (240 CDB)
 *
 *   Dual Channel Products:
 *	ABP852 - Dual Channel Bus-Master VL (240 CDB Per Channel)
 *
 *   Footnotes:
 *     1. This board has been shipped by HP with the 4020i CD-R drive.
 *        The board has no BIOS so it cannot control a boot device, but
 *        it can control any secondary SCSI device.
 *     2. This board has been sold by SIIG as the i540 SpeedMaster.
 *     3. This board has been sold by SIIG as the i542 SpeedMaster.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: adv_isa.c,v 1.20 2012/10/27 17:18:23 chs Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/queue.h>

#include <sys/bus.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/isa/isavar.h>

#include <dev/ic/advlib.h>
#include <dev/ic/adv.h>


/* Possible port addresses an ISA or VL adapter can live at */
static int asc_ioport[ASC_IOADR_TABLE_MAX_IX] =
{
	0x100,
	ASC_IOADR_1,	/* First selection in BIOS setup */
	0x120,
	ASC_IOADR_2,	/* Second selection in BIOS setup */
	0x140,
	ASC_IOADR_3,	/* Third selection in BIOS setup */
	ASC_IOADR_4,	/* Fourth selection in BIOS setup */
	ASC_IOADR_5,	/* Fifth selection in BIOS setup */
	ASC_IOADR_6,	/* Sixth selection in BIOS setup */
	ASC_IOADR_7,	/* Seventh selection in BIOS setup */
	ASC_IOADR_8	/* Eighth and default selection in BIOS setup */
};

/******************************************************************************/

int	adv_isa_probe(device_t, cfdata_t, void *);
void	adv_isa_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(adv_isa, sizeof(ASC_SOFTC),
    adv_isa_probe, adv_isa_attach, NULL, NULL);

/******************************************************************************/

int
adv_isa_probe(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	int port_index;
	int iobase, irq, drq;
	int rv = 0;

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);
	if (ia->ia_ndrq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	/*
	 * If the I/O address is wildcarded, look for boards
	 * in ascending order.
	 */
	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT) {
		for (port_index = 0; port_index < ASC_IOADR_TABLE_MAX_IX;
		     port_index++) {
			iobase = asc_ioport[port_index];

			if (iobase) {
				if (bus_space_map(iot, iobase, ASC_IOADR_GAP,
				    0, &ioh))
					continue;

				rv = AscFindSignature(iot, ioh);

				if (rv) {
					ia->ia_io[0].ir_addr = iobase;
					break;
				}

				bus_space_unmap(iot, ioh, ASC_IOADR_GAP);
			}
		}
		if (rv == 0)
			return (0);
	} else {
		iobase = ia->ia_io[0].ir_addr;
		if (bus_space_map(iot, iobase, ASC_IOADR_GAP, 0, &ioh))
			return (0);

		rv = AscFindSignature(iot, ioh);

		if (rv == 0) {
			bus_space_unmap(iot, ioh, ASC_IOADR_GAP);
			return (0);
		}
	}

	/* XXXJRT Probe routines should not have side-effects!! */
	ASC_SET_CHIP_CONTROL(iot, ioh, ASC_CC_HALT);
	ASC_SET_CHIP_STATUS(iot, ioh, 0);

	irq = AscGetChipIRQ(iot, ioh, ASC_IS_ISA);
	drq = AscGetIsaDmaChannel(iot, ioh);

	/* Verify that the IRQ/DRQ match (or are wildcarded). */
	if (ia->ia_irq[0].ir_irq != ISA_UNKNOWN_IRQ &&
	    ia->ia_irq[0].ir_irq != irq) {
		rv = 0;
		goto out;
	}
	if (ia->ia_drq[0].ir_drq != ISA_UNKNOWN_DRQ &&
	    ia->ia_drq[0].ir_drq != drq) {
		rv = 0;
		goto out;
	}

	ia->ia_nio = 1;
	ia->ia_io[0].ir_addr = iobase;
	ia->ia_io[0].ir_size = ASC_IOADR_GAP;

	ia->ia_nirq = 1;
	ia->ia_irq[0].ir_irq = irq;

	ia->ia_ndrq = 1;
	ia->ia_drq[0].ir_drq = drq;

	ia->ia_niomem = 0;

 out:
	bus_space_unmap(iot, ioh, ASC_IOADR_GAP);
	return rv;
}


void
adv_isa_attach(device_t parent, device_t self, void *aux)
{
	struct isa_attach_args *ia = aux;
	ASC_SOFTC *sc = device_private(self);
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	isa_chipset_tag_t ic = ia->ia_ic;
	int error;

	printf("\n");

	sc->sc_dev = self;
	sc->sc_flags = 0x0;

	if (bus_space_map(iot, ia->ia_io[0].ir_addr, ASC_IOADR_GAP, 0, &ioh)) {
		aprint_error_dev(sc->sc_dev, "can't map i/o space\n");
		return;
	}

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_dmat = ia->ia_dmat;
	sc->bus_type = ASC_IS_ISA;
	sc->chip_version = ASC_GET_CHIP_VER_NO(iot, ioh);
	/*
	 * Memo:
	 * for EISA cards:
	 * sc->chip_version = (ASC_CHIP_MIN_VER_EISA - 1) + ea->ea_pid[1];
	 */

	/*
	 * Initialize the board
	 */
	if (adv_init(sc)) {
		aprint_error_dev(sc->sc_dev, "adv_init failed\n");
		return;
	}

	if ((error = isa_dmacascade(ic, ia->ia_drq[0].ir_drq)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to cascade DRQ, error = %d\n", error);
		return;
	}

	sc->sc_ih = isa_intr_establish(ic, ia->ia_irq[0].ir_irq, IST_EDGE,
	    IPL_BIO, adv_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "couldn't establish interrupt\n");
		return;
	}

	adv_attach(sc);
}

/*	$NetBSD: aha_mca.c,v 1.22 2009/09/22 13:22:53 tsutsui Exp $	*/

/*
 * Copyright (c) 2000-2002 The NetBSD Foundation, Inc.
 * Copyright (c) 1996-1999 Scott D. Telford.
 * Copyright (c) 1994, 1996 Charles M. Hannum.  All rights reserved.
 * Portions:
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
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

/*
 * Originally written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 */

/*
 * AHA-1640 MCA bus code by Scott Telford
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: aha_mca.c,v 1.22 2009/09/22 13:22:53 tsutsui Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/ahareg.h>
#include <dev/ic/ahavar.h>

#include <dev/mca/mcareg.h>
#include <dev/mca/mcavar.h>
#include <dev/mca/mcadevs.h>

#define	AHA_ISA_IOSIZE	4

static int	aha_mca_probe(device_t, cfdata_t, void *);
static void	aha_mca_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(aha_mca, sizeof(struct aha_softc),
    aha_mca_probe, aha_mca_attach, NULL, NULL);

int
aha_mca_probe(device_t parent, cfdata_t match, void *aux)
{
	register struct mca_attach_args *ma = aux;

	if (ma->ma_id == MCA_PRODUCT_AHA1640)
		return (1);

	return (0);
}


void
aha_mca_attach(device_t parent, device_t self, void *aux)
{
	struct mca_attach_args *ma = aux;
	struct aha_softc *sc = device_private(self);
	bus_space_tag_t iot = ma->ma_iot;
	bus_space_handle_t ioh;
	struct aha_probe_data apd;
	mca_chipset_tag_t mc = ma->ma_mc;
	bus_addr_t iobase;

	sc->sc_dev =self;

	/*
	 * POS registers differ much between 8003 and 8013, so they are
	 * divided to two sections.
	 *
	 * POS register 2: (adf pos0)
	 * 7 6 5 4 3 2 1 0
	 * | 0 0 0 0 0 0 \__ enable: 0=adapter disabled, 1=adapter enabled
	 *  \_______________ Adapter BIOS location
	 *
	 * POS register 3: (adf pos1)
	 * 7 6 5 4 3 2 1 0
	 * \_/ \___/ \___/
	 *  |    |       \__ I/O Port Address (upper part)
	 *  |    |__________ Adapter BIOS location
	 *  |_______________ I/O Port Address (lower part)
	 *
	 * POS register 4: (adf pos2)
	 * 7 6 5 4 3 2 1 0
	 * \___/ | | \___/
	 *   |   | |     \__ IRQ: XXX+8
	 *   |   | |________ Sync xfer started by AHA-1640: 1=YES 2=NO
	 *   |   |__________ SCSI Parity Checking On/Off
	 *   |______________ SCSI Address
	 *
	 * POS register 3: (adf pos3)
	 * 7 6 5 4 3 2 1 0
	 *       | \_____/
	 *       |       \__ Arbitration lvl (DMA channel)
	 *       |__________ Fairness On/Off
	 *
	 */

	apd.sc_irq = (ma->ma_pos[4] & 0x7) + 8;
	apd.sc_drq = ma->ma_pos[5] & 0xf;
	apd.sc_scsi_dev = (ma->ma_pos[4] & 0xe0) >> 5;

	printf(" slot %d irq %d drq %d: Adaptec AHA-1640 SCSI Adapter\n",
		ma->ma_slot + 1,
		apd.sc_irq, apd.sc_drq);

	iobase = ((ma->ma_pos[3] & 0x03) << 8) + 0x30 +
		 ((ma->ma_pos[3] & 0x40) >> 4);

	if (bus_space_map(iot, iobase, AHA_ISA_IOSIZE, 0, &ioh)) {
		aprint_error_dev(self, "can't map i/o space\n");
		return;
	}

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_dmat = ma->ma_dmat;

	/*
	 * AHA1640 is bus-mastering card, does not need any assistance
	 * with DMA transfers by MCA DMA controller.
	 */

	sc->sc_ih = mca_intr_establish(mc, apd.sc_irq, IPL_BIO, aha_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt\n");
		return;
	}

	aha_attach(sc, &apd);
}

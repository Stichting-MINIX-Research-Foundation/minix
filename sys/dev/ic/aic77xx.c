/*	$NetBSD: aic77xx.c,v 1.8 2009/03/14 15:36:17 dsl Exp $	*/

/*
 * Common routines for AHA-27/284X and aic7770 motherboard SCSI controllers.
 *
 * Copyright (c) 1994, 1995, 1996, 1997, 1998 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/aic7xxx/ahc_eisa.c,v 1.15 2000/01/29 14:22:19 peter Exp $
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: aic77xx.c,v 1.8 2009/03/14 15:36:17 dsl Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/ic/aic7xxx_osm.h>
#include <dev/ic/aic7xxx_inline.h>
#include <dev/ic/aic77xxreg.h>
#include <dev/ic/aic77xxvar.h>

/*
 * Return irq setting of the board, otherwise -1.
 */
int
ahc_aic77xx_irq(bus_space_tag_t iot, bus_space_handle_t ioh)
{
	int irq;
	u_int8_t intdef;
	u_int8_t hcntrl;

	/* Pause the card preseving the IRQ type */
	hcntrl = bus_space_read_1(iot, ioh, HCNTRL) & IRQMS;
	bus_space_write_1(iot, ioh, HCNTRL, hcntrl | PAUSE);

	intdef = bus_space_read_1(iot, ioh, INTDEF);
	irq = (intdef & INTDEF_IRQ_MASK);
	switch (irq) {
	case 9:
	case 10:
	case 11:
	case 12:
	case 14:
	case 15:
		break;
	default:
		printf("ahc_aic77xx_irq: illegal irq setting %d\n", intdef);
		return (-1);
	}

	return (irq);
}

int
ahc_aic77xx_attach(struct ahc_softc *ahc)
{
	u_int8_t sblkctl;
	u_int8_t sblkctl_orig;
	u_int8_t hostconf;

	/*
	 * See if we have a Rev E or higher aic7770. Anything below a
	 * Rev E will have a R/O autoflush disable configuration bit.
	 */
	sblkctl_orig = ahc_inb(ahc, SBLKCTL);
	sblkctl = sblkctl_orig ^ AUTOFLUSHDIS;
	ahc_outb(ahc, SBLKCTL, sblkctl);
	sblkctl = ahc_inb(ahc, SBLKCTL);
	if (sblkctl != sblkctl_orig) {
		printf("%s: aic7770 >= Rev E: R/O autoflush enabled\n",
		    ahc_name(ahc));
		/*
		 * Ensure autoflush is enabled
		 */
		sblkctl &= ~AUTOFLUSHDIS;
		ahc_outb(ahc, SBLKCTL, sblkctl);
	}

	/* Setup the FIFO threshold and the bus off time */
	hostconf = ahc_inb(ahc, HOSTCONF);
	ahc_outb(ahc, BUSSPD, hostconf & DFTHRSH);
	ahc_outb(ahc, BUSTIME, (hostconf << 2) & BOFF);

	/*
	 * Generic aic7xxx initialization.
	 */
	if (ahc_init(ahc)) {
		/*
		 * The board's IRQ line is not yet enabled so it's safe
		 * to release the irq.
		 */
		return (ENXIO);
	}

	/*
	 * Enable the board's BUS drivers
	 */
	ahc_outb(ahc, BCTL, ENABLE);

	/* Attach sub-devices - always succeeds */
	ahc_attach(ahc);

	return 0;
}

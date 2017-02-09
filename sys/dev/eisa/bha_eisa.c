/*	$NetBSD: bha_eisa.c,v 1.37 2014/10/18 08:33:27 snj Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: bha_eisa.c,v 1.37 2014/10/18 08:33:27 snj Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/eisa/eisavar.h>
#include <dev/eisa/eisadevs.h>

#include <dev/ic/bhareg.h>
#include <dev/ic/bhavar.h>

#define BHA_EISA_SLOT_OFFSET	0x0c80
#define	BHA_EISA_IOSIZE		0x0010
#define	BHA_ISA_IOSIZE		0x0004

#define	BHA_EISA_IOCONF		0x0c

static int	bha_eisa_address(bus_space_tag_t, bus_space_handle_t, int *);
static int	bha_eisa_match(device_t, cfdata_t, void *);
static void	bha_eisa_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(bha_eisa, sizeof(struct bha_softc),
    bha_eisa_match, bha_eisa_attach, NULL, NULL);

static int
bha_eisa_address(bus_space_tag_t iot, bus_space_handle_t ioh, int *portp)
{
	int port;

	switch (bus_space_read_1(iot, ioh, BHA_EISA_IOCONF) & 0x07) {
	case 0x00:
		port = 0x330;
		break;
	case 0x01:
		port = 0x334;
		break;
	case 0x02:
		port = 0x230;
		break;
	case 0x03:
		port = 0x234;
		break;
	case 0x04:
		port = 0x130;
		break;
	case 0x05:
		port = 0x134;
		break;
	default:
		return (1);
	}

	*portp = port;
	return (0);
}

/*
 * Check the slots looking for a board we recognise
 * If we find one, note its address (slot) and call
 * the actual probe routine to check it out.
 */
static int
bha_eisa_match(device_t parent, cfdata_t match, void *aux)
{
	struct eisa_attach_args *ea = aux;
	bus_space_tag_t iot = ea->ea_iot;
	bus_space_handle_t ioh, ioh2;
	int port;
	int rv;

	/* must match one of our known ID strings */
	if (strcmp(ea->ea_idstring, "BUS4201") &&
	    strcmp(ea->ea_idstring, "BUS4202"))
		return (0);

	if (bus_space_map(iot,
	    EISA_SLOT_ADDR(ea->ea_slot) + BHA_EISA_SLOT_OFFSET, BHA_EISA_IOSIZE,
	    0, &ioh))
		return (0);

	if (bha_eisa_address(iot, ioh, &port) ||
	    bus_space_map(iot, port, BHA_ISA_IOSIZE, 0, &ioh2)) {
		bus_space_unmap(iot, ioh, BHA_EISA_IOSIZE);
		return (0);
	}

	rv = bha_find(iot, ioh2);

	bus_space_unmap(iot, ioh2, BHA_ISA_IOSIZE);
	bus_space_unmap(iot, ioh, BHA_EISA_IOSIZE);

	return (rv);
}

/*
 * Attach all the sub-devices we can find
 */
static void
bha_eisa_attach(device_t parent, device_t self, void *aux)
{
	struct eisa_attach_args *ea = aux;
	struct bha_softc *sc = device_private(self);
	bus_space_tag_t iot = ea->ea_iot;
	bus_space_handle_t ioh, ioh2;
	int port;
	struct bha_probe_data bpd;
	eisa_chipset_tag_t ec = ea->ea_ec;
	eisa_intr_handle_t ih;
	const char *model, *intrstr;
	char intrbuf[EISA_INTRSTR_LEN];

	sc->sc_dev = self;

	if (!strcmp(ea->ea_idstring, "BUS4201"))
		model = EISA_PRODUCT_BUS4201;
	else if (!strcmp(ea->ea_idstring, "BUS4202"))
		model = EISA_PRODUCT_BUS4202;
	else
		model = "unknown model!";
	printf(": %s\n", model);

	if (bus_space_map(iot,
	    EISA_SLOT_ADDR(ea->ea_slot) + BHA_EISA_SLOT_OFFSET, BHA_EISA_IOSIZE,
	    0, &ioh))
		panic("bha_eisa_attach: could not map EISA slot");

	if (bha_eisa_address(iot, ioh, &port) ||
	    bus_space_map(iot, port, BHA_ISA_IOSIZE, 0, &ioh2))
		panic("bha_eisa_attach: could not map ISA address");

	sc->sc_iot = iot;
	sc->sc_ioh = ioh2;
	sc->sc_dmat = ea->ea_dmat;
	if (!bha_probe_inquiry(iot, ioh2, &bpd))
		panic("bha_eisa_attach failed");

	sc->sc_dmaflags = 0;

	if (eisa_intr_map(ec, bpd.sc_irq, &ih)) {
		aprint_error_dev(sc->sc_dev, "couldn't map interrupt (%d)\n",
		    bpd.sc_irq);
		return;
	}
	intrstr = eisa_intr_string(ec, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = eisa_intr_establish(ec, ih, IST_LEVEL, IPL_BIO,
	    bha_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(sc->sc_dev, "interrupting at %s\n", intrstr);

	bha_attach(sc);
}

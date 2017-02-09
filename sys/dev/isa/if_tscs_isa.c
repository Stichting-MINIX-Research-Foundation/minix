/*	$NetBSD: if_tscs_isa.c,v 1.16 2015/04/13 16:33:24 riastradh Exp $	*/

/*-
 * Copyright (c) 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jesse Off
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
__KERNEL_RCSID(0, "$NetBSD: if_tscs_isa.c,v 1.16 2015/04/13 16:33:24 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/cs89x0reg.h>
#include <dev/ic/cs89x0var.h>
#include <dev/isa/cs89x0isavar.h>

static int	tscs_isa_probe(device_t, cfdata_t, void *);
static void	tscs_isa_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(tscs_isa, sizeof(struct cs_softc_isa),
    tscs_isa_probe, tscs_isa_attach, NULL, NULL);

int
tscs_isa_probe(device_t parent, cfdata_t cf, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh, pldh;
	struct cs_softc sc;
	int rv = 0, have_io = 0, have_pld = 0, irq;
	u_int8_t jpcfg;

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	/*
	 * Disallow wildcarded I/O base.
	 */
	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return (0);

	/*
	 * Map the I/O space.
	 */
	if (bus_space_map(ia->ia_iot, ia->ia_io[0].ir_addr - 0x300, 9,
	    0, &pldh))
		goto out;
	have_pld = 1;

	if ((bus_space_read_1(ia->ia_iot, pldh, 0)  & 0xf) == 0xa &&
	    (bus_space_read_1(ia->ia_iot, pldh, 0x8)  & 0xf) == 0x5 &&
	    (bus_space_read_1(ia->ia_iot, pldh, 0x8)  & 0xf) == 0xa) {
		bus_space_read_1(ia->ia_iot, pldh, 0x8); /* PLD rev. */
		jpcfg = bus_space_read_1(ia->ia_iot, pldh, 0x8) & 0xf;
	} else {
		goto out;
	}

	if (bus_space_map(ia->ia_iot, ia->ia_io[0].ir_addr, CS8900_IOSIZE,
	    0, &ioh))
		goto out;
	have_io = 1;

	memset(&sc, 0, sizeof sc);
	sc.sc_iot = iot;
	sc.sc_ioh = ioh;
	/* Verify that it's a Crystal product. */
	if (CS_READ_PACKET_PAGE_IO(&sc, PKTPG_EISA_NUM) !=
	    EISA_NUM_CRYSTAL)
		goto out;

	/*
	 * Verify that it's a supported chip.
	 */
	switch (CS_READ_PACKET_PAGE_IO(&sc, PKTPG_PRODUCT_ID) &
	    PROD_ID_MASK) {
	case PROD_ID_CS8900:
		break;
	default:
		/* invalid product ID */
		goto out;
	}

	/*
	 * If the IRQ wasn't specified, get it from the EEPROM.
	 */
	if (ia->ia_irq[0].ir_irq == ISA_UNKNOWN_IRQ) {
		switch (jpcfg >> 1) {
		case 0x1:
			irq = 5;
			break;
		case 0x2:
			irq = 6;
			break;
		case 0x4:
			irq = 7;
			break;
		default:
			goto out;
		}
	} else
		irq = ia->ia_irq[0].ir_irq;

	ia->ia_nio = 1;
	ia->ia_io[0].ir_size = CS8900_IOSIZE;
	ia->ia_niomem = 0;
	ia->ia_nirq = 1;
	ia->ia_ndrq = 0;
	ia->ia_irq[0].ir_irq = irq;

	rv = 1;

 out:
	if (have_io)
		bus_space_unmap(iot, ioh, CS8900_IOSIZE);
	if (have_pld)
		bus_space_unmap(iot, pldh, 0x9);

	return (rv);
}

void
tscs_isa_attach(device_t parent, device_t self, void *aux)
{
	struct cs_softc_isa *isc = device_private(self);
	struct cs_softc *sc = &isc->sc_cs;
	struct isa_attach_args *ia = aux;

	sc->sc_dev = self;
	isc->sc_ic = ia->ia_ic;
	sc->sc_iot = ia->ia_iot;
	sc->sc_memt = ia->ia_memt;

	isc->sc_drq = -1;
	sc->sc_irq = ia->ia_irq[0].ir_irq;

	printf(": Technologic Systems TS-ETH10\n");

	/*
	 * Map the device.
	 */
	if (bus_space_map(sc->sc_iot, ia->ia_io[0].ir_addr, CS8900_IOSIZE,
	    0, &sc->sc_ioh)) {
		aprint_error_dev(self, "unable to map i/o space\n");
		return;
	}

	sc->sc_ih = isa_intr_establish(ia->ia_ic, sc->sc_irq, IST_EDGE,
	    IPL_NET, cs_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "unable to establish interrupt\n");
		return;
	}

	sc->sc_dma_chipinit = NULL;
	sc->sc_dma_attach = NULL;
	sc->sc_dma_process_rx = NULL;

	/* all jumper routed IRQs are connected to pseudo CS8900 IRQ 5 */
	sc->sc_irq = 5;

	cs_attach(sc, NULL, NULL, 0, 0);
}

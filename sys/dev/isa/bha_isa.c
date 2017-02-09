/*	$NetBSD: bha_isa.c,v 1.36 2014/10/18 08:33:28 snj Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: bha_isa.c,v 1.36 2014/10/18 08:33:28 snj Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/bhareg.h>
#include <dev/ic/bhavar.h>

#define	BHA_ISA_IOSIZE	4

int	bha_isa_probe(device_t, cfdata_t, void *);
void	bha_isa_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(bha_isa, sizeof(struct bha_softc),
    bha_isa_probe, bha_isa_attach, NULL, NULL);

/*
 * Check the slots looking for a board we recognise
 * If we find one, note its address (slot) and call
 * the actual probe routine to check it out.
 */
int
bha_isa_probe(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	struct bha_probe_data bpd;
	int rv;

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);
	if (ia->ia_ndrq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	/* Disallow wildcarded i/o address. */
	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return (0);

	if (bus_space_map(iot, ia->ia_io[0].ir_addr, BHA_ISA_IOSIZE, 0, &ioh))
		return (0);

	rv = bha_probe_inquiry(iot, ioh, &bpd);

	bus_space_unmap(iot, ioh, BHA_ISA_IOSIZE);

	if (rv) {
		if (ia->ia_irq[0].ir_irq != ISA_UNKNOWN_IRQ &&
		    ia->ia_irq[0].ir_irq != bpd.sc_irq)
			return (0);
		if (ia->ia_drq[0].ir_drq != ISA_UNKNOWN_DRQ &&
		    ia->ia_drq[0].ir_drq != bpd.sc_drq)
			return (0);

		ia->ia_nio = 1;
		ia->ia_io[0].ir_size = BHA_ISA_IOSIZE;

		ia->ia_nirq = 1;
		ia->ia_irq[0].ir_irq = bpd.sc_irq;

		ia->ia_ndrq = 1;
		ia->ia_drq[0].ir_drq = bpd.sc_drq;

		ia->ia_niomem = 0;
	}
	return (rv);
}

/*
 * Attach all the sub-devices we can find
 */
void
bha_isa_attach(device_t parent, device_t self, void *aux)
{
	struct isa_attach_args *ia = aux;
	struct bha_softc *sc = device_private(self);
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	struct bha_probe_data bpd;
	isa_chipset_tag_t ic = ia->ia_ic;
	int error;

	printf("\n");

	sc->sc_dev = self;
	if (bus_space_map(iot, ia->ia_io[0].ir_addr, BHA_ISA_IOSIZE, 0, &ioh)) {
		aprint_error_dev(sc->sc_dev, "can't map i/o space\n");
		return;
	}

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_dmat = ia->ia_dmat;
	if (!bha_probe_inquiry(iot, ioh, &bpd)) {
		aprint_error_dev(sc->sc_dev, "bha_isa_attach failed\n");
		return;
	}

	sc->sc_dmaflags = 0;
	if (bpd.sc_drq != -1) {
		if ((error = isa_dmacascade(ic, bpd.sc_drq)) != 0) {
			aprint_error_dev(sc->sc_dev, " unable to cascade DRQ, error = %d\n", error);
			return;
		}
	} else {
		/*
		 * We have a VLB controller.  If we're at least both
		 * hardware revision E and firmware revision 3.37,
		 * we can do 32-bit DMA (earlier revisions are buggy
		 * in this regard).
		 */
		(void) bha_info(sc);
		if (strcmp(sc->sc_firmware, "3.37") < 0)
		    printf("%s: buggy VLB controller, disabling 32-bit DMA\n",
		        device_xname(sc->sc_dev));
		else
			sc->sc_dmaflags = ISABUS_DMA_32BIT;
	}

	sc->sc_ih = isa_intr_establish(ic, bpd.sc_irq, IST_EDGE, IPL_BIO,
	    bha_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "couldn't establish interrupt\n");
		return;
	}

	bha_attach(sc);
}

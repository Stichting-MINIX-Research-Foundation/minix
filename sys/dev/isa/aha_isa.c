/*	$NetBSD: aha_isa.c,v 1.30 2014/10/18 08:33:28 snj Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: aha_isa.c,v 1.30 2014/10/18 08:33:28 snj Exp $");

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

#define	AHA_ISA_IOSIZE	4

static int	aha_isa_probe(device_t, cfdata_t, void *);
static void	aha_isa_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(aha_isa, sizeof(struct aha_softc),
    aha_isa_probe, aha_isa_attach, NULL, NULL);

/*
 * Check the slots looking for a board we recognise
 * If we find one, note its address (slot) and call
 * the actual probe routine to check it out.
 */
int
aha_isa_probe(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	struct aha_probe_data apd;
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

	if (bus_space_map(iot, ia->ia_io[0].ir_addr, AHA_ISA_IOSIZE, 0, &ioh))
		return (0);

	rv = aha_find(iot, ioh, &apd);

	bus_space_unmap(iot, ioh, AHA_ISA_IOSIZE);

	if (rv) {
		if (ia->ia_irq[0].ir_irq != ISA_UNKNOWN_IRQ &&
		    ia->ia_irq[0].ir_irq != apd.sc_irq)
			return (0);
		if (ia->ia_drq[0].ir_drq != ISA_UNKNOWN_DRQ &&
		    ia->ia_drq[0].ir_drq != apd.sc_drq)
			return (0);

		ia->ia_nio = 1;
		ia->ia_io[0].ir_size = AHA_ISA_IOSIZE;

		ia->ia_nirq = 1;
		ia->ia_irq[0].ir_irq = apd.sc_irq;

		ia->ia_ndrq = 1;
		ia->ia_drq[0].ir_drq = apd.sc_drq;

		ia->ia_niomem = 0;
	}
	return (rv);
}

/*
 * Attach all the sub-devices we can find
 */
void
aha_isa_attach(device_t parent, device_t self, void *aux)
{
	struct isa_attach_args *ia = aux;
	struct aha_softc *sc = device_private(self);
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	struct aha_probe_data apd;
	isa_chipset_tag_t ic = ia->ia_ic;
	int error;

	sc->sc_dev = self;

	printf("\n");

	if (bus_space_map(iot, ia->ia_io[0].ir_addr, AHA_ISA_IOSIZE, 0, &ioh)) {
		aprint_error_dev(self, "can't map i/o space\n");
		return;
	}

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_dmat = ia->ia_dmat;
	if (!aha_find(iot, ioh, &apd)) {
		aprint_error_dev(self, "aha_find failed\n");
		return;
	}

	if (apd.sc_drq != -1) {
		if ((error = isa_dmacascade(ic, apd.sc_drq)) != 0) {
			aprint_error_dev(self,
			    "unable to cascade DRQ, error = %d\n", error);
			return;
		}
	}

	sc->sc_ih = isa_intr_establish(ic, apd.sc_irq, IST_EDGE, IPL_BIO,
	    aha_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt\n");
		return;
	}

	aha_attach(sc, &apd);
}

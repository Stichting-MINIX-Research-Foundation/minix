/*	$NetBSD: fdc_isa.c,v 1.20 2015/04/13 16:33:24 riastradh Exp $	*/

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

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Don Ahn.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)fd.c	7.4 (Berkeley) 5/25/91
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: fdc_isa.c,v 1.20 2015/04/13 16:33:24 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/queue.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/isa/fdreg.h>
#include <dev/isa/fdcvar.h>

static int	fdc_isa_probe(device_t, cfdata_t, void *);
static void	fdc_isa_attach(device_t, device_t, void *);
static int	fdc_isa_detach(device_t, int);

struct fdc_isa_softc {
	struct fdc_softc sc_fdc;	/* base fdc device */

	bus_space_handle_t sc_baseioh;	/* base I/O handle */
};

CFATTACH_DECL2_NEW(fdc_isa, sizeof(struct fdc_isa_softc),
    fdc_isa_probe, fdc_isa_attach, fdc_isa_detach, NULL, NULL, fdc_childdet);

#ifdef NEWCONFIG
void	fdc_isa_forceintr(void *);
#endif

static int
fdc_isa_probe(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot;
	bus_space_handle_t ioh, ctl_ioh, base_ioh;
	int rv, iobase;

	iot = ia->ia_iot;
	rv = 0;

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);
	if (ia->ia_ndrq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	/* Disallow wildcarded I/O addresses. */
	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return (0);

	/* Don't allow wildcarded IRQ/DRQ. */
	if (ia->ia_irq[0].ir_irq == ISA_UNKNOWN_IRQ)
		return (0);

	if (ia->ia_drq[0].ir_drq == ISA_UNKNOWN_DRQ)
		return (0);

	/* Map the I/O space. */
	iobase = ia->ia_io[0].ir_addr;
	if (bus_space_map(iot, iobase, 6 /* FDC_NPORT */, 0, &base_ioh))
		return (0);

	if (bus_space_subregion(iot, base_ioh, 2, 4, &ioh)) {
		bus_space_unmap(iot, base_ioh, 6);
		return (0);
	}

	if (bus_space_map(iot, iobase + fdctl + 2, 1, 0, &ctl_ioh)) {
		bus_space_unmap(iot, base_ioh, 6);
		return (0);
	}

	/* Not needed for the rest of the probe. */
	bus_space_unmap(iot, ctl_ioh, 1);

	/* reset */
	bus_space_write_1(iot, ioh, fdout, 0);
	delay(100);
	bus_space_write_1(iot, ioh, fdout, FDO_FRST);

	/* see if it can handle a command */
	if (out_fdc(iot, ioh, NE7CMD_SPECIFY) < 0)
		goto out;
	out_fdc(iot, ioh, 0xdf);
	out_fdc(iot, ioh, 2);

	rv = 1;
	ia->ia_nio = 1;
	ia->ia_io[0].ir_size = FDC_NPORT;

	ia->ia_nirq = 1;
	ia->ia_ndrq = 1;

	ia->ia_niomem = 0;

 out:
	bus_space_unmap(iot, base_ioh, 6 /* FDC_NPORT */);
	return (rv);
}

static int
fdc_isa_detach(device_t self, int flags)
{
	int rc;
	struct fdc_isa_softc *isc = device_private(self);
	struct fdc_softc *fdc = &isc->sc_fdc;

	if ((rc = fdcdetach(self, flags)) != 0)
		return rc;

	isa_intr_disestablish(fdc->sc_ic, fdc->sc_ih);

	bus_space_unmap(fdc->sc_iot, fdc->sc_fdctlioh, 1);

	bus_space_unmap(fdc->sc_iot, isc->sc_baseioh, 6 /* FDC_NPORT */);

	return 0;
}

static void
fdc_isa_attach(device_t parent, device_t self, void *aux)
{
	struct fdc_isa_softc *isc = device_private(self);
	struct fdc_softc *fdc = &isc->sc_fdc;
	struct isa_attach_args *ia = aux;

	aprint_naive("\n");
	aprint_normal("\n");

	fdc->sc_dev = self;
	fdc->sc_iot = ia->ia_iot;
	fdc->sc_ic = ia->ia_ic;
	fdc->sc_drq = ia->ia_drq[0].ir_drq;

	if (bus_space_map(fdc->sc_iot, ia->ia_io[0].ir_addr,
	    6 /* FDC_NPORT */, 0, &isc->sc_baseioh)) {
		aprint_normal_dev(fdc->sc_dev, "unable to map I/O space\n");
		return;
	}

	if (bus_space_subregion(fdc->sc_iot, isc->sc_baseioh, 2, 4,
	    &fdc->sc_ioh)) {
		aprint_normal_dev(fdc->sc_dev,
		    "unable to subregion I/O space\n");
		return;
	}

	if (bus_space_map(fdc->sc_iot, ia->ia_io[0].ir_addr + fdctl + 2, 1, 0,
	    &fdc->sc_fdctlioh)) {
		aprint_normal_dev(fdc->sc_dev,
		    "unable to map CTL I/O space\n");
		return;
	}

	fdc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq[0].ir_irq,
	    IST_EDGE, IPL_BIO, fdcintr, fdc);

	fdcattach(fdc);
}

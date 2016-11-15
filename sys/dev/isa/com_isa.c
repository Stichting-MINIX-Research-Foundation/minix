/*	$NetBSD: com_isa.c,v 1.39 2010/02/24 22:37:58 dyoung Exp $	*/

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
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
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
 *	@(#)com.c	7.5 (Berkeley) 5/16/91
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: com_isa.c,v 1.39 2010/02/24 22:37:58 dyoung Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/device.h>

#include <sys/intr.h>
#include <sys/bus.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>
#ifdef COM_HAYESP
#include <dev/ic/hayespreg.h>
#endif

#include <dev/isa/isavar.h>

struct com_isa_softc {
	struct	com_softc sc_com;	/* real "com" softc */

	/* ISA-specific goo. */
	isa_chipset_tag_t sc_ic;
	void	*sc_ih;			/* interrupt handler */
	int	sc_irq;
};

static bool com_isa_suspend(device_t, const pmf_qual_t *);
static bool com_isa_resume(device_t, const pmf_qual_t *);

int com_isa_probe(device_t, cfdata_t , void *);
void com_isa_attach(device_t, device_t, void *);
static int com_isa_detach(device_t, int);
#ifdef COM_HAYESP
int com_isa_isHAYESP(bus_space_handle_t, struct com_softc *);
#endif


CFATTACH_DECL3_NEW(com_isa, sizeof(struct com_isa_softc),
    com_isa_probe, com_isa_attach, com_isa_detach, NULL,
    NULL, NULL, DVF_DETACH_SHUTDOWN);

int
com_isa_probe(device_t parent, cfdata_t match, void *aux)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int iobase;
	int rv = 1;
	struct isa_attach_args *ia = aux;

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	/* Disallow wildcarded i/o address. */
	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return (0);

	/* Don't allow wildcarded IRQ. */
	if (ia->ia_irq[0].ir_irq == ISA_UNKNOWN_IRQ)
		return (0);

	iot = ia->ia_iot;
	iobase = ia->ia_io[0].ir_addr;

	/* if it's in use as console, it's there. */
	if (!com_is_console(iot, iobase, 0)) {
		if (bus_space_map(iot, iobase, COM_NPORTS, 0, &ioh)) {
			return 0;
		}
		rv = comprobe1(iot, ioh);
		bus_space_unmap(iot, ioh, COM_NPORTS);
	}

	if (rv) {
		ia->ia_nio = 1;
		ia->ia_io[0].ir_size = COM_NPORTS;

		ia->ia_nirq = 1;

		ia->ia_niomem = 0;
		ia->ia_ndrq = 0;
	}
	return (rv);
}

void
com_isa_attach(device_t parent, device_t self, void *aux)
{
	struct com_isa_softc *isc = device_private(self);
	struct com_softc *sc = &isc->sc_com;
	int iobase, irq;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct isa_attach_args *ia = aux;
#ifdef COM_HAYESP
	int	hayesp_ports[] = { 0x140, 0x180, 0x280, 0x300, 0 };
	int	*hayespp;
#endif

	/*
	 * We're living on an isa.
	 */
	iobase = ia->ia_io[0].ir_addr;
	iot = ia->ia_iot;

	if (!com_is_console(iot, iobase, &ioh) &&
	    bus_space_map(iot, iobase, COM_NPORTS, 0, &ioh)) {
		printf(": can't map i/o space\n");
		return;
	}

	sc->sc_dev = self;

	COM_INIT_REGS(sc->sc_regs, iot, ioh, iobase);

	sc->sc_frequency = COM_FREQ;
	irq = ia->ia_irq[0].ir_irq;

#ifdef COM_HAYESP
	for (hayespp = hayesp_ports; *hayespp != 0; hayespp++) {
		bus_space_handle_t	hayespioh;
#define	HAYESP_NPORTS	8
		if (bus_space_map(iot, *hayespp, HAYESP_NPORTS, 0, &hayespioh))
			continue;
		if (com_isa_isHAYESP(hayespioh, sc)) {
			break;
		}
		bus_space_unmap(iot, hayespioh, HAYESP_NPORTS);
	}
#endif

	com_attach_subr(sc);

	if (!pmf_device_register1(self, com_isa_suspend, com_isa_resume,
	    com_cleanup))
		aprint_error_dev(self, "couldn't establish power handler\n");

	isc->sc_ic = ia->ia_ic;
	isc->sc_irq = irq;
	isc->sc_ih = isa_intr_establish(ia->ia_ic, irq, IST_EDGE, IPL_SERIAL,
	    comintr, sc);
}

static bool
com_isa_suspend(device_t self, const pmf_qual_t *qual)
{
	struct com_isa_softc *isc = device_private(self);

	if (!com_suspend(self, qual))
		return false;

	isa_intr_disestablish(isc->sc_ic, isc->sc_ih);
	isc->sc_ih = NULL;

	return true;
}

static bool
com_isa_resume(device_t self, const pmf_qual_t *qual)
{
	struct com_isa_softc *isc = device_private(self);
	struct com_softc *sc = &isc->sc_com;

	isc->sc_ih = isa_intr_establish(isc->sc_ic, isc->sc_irq, IST_EDGE,
	    IPL_SERIAL, comintr, sc);

	return com_resume(self, qual);
}

static int
com_isa_detach(device_t self, int flags)
{
	struct com_isa_softc *isc = device_private(self);
	struct com_softc *sc = &isc->sc_com;
	const struct com_regs *cr = &sc->sc_regs;
	int rc;

	if ((rc = com_detach(self, flags)) != 0)
		return rc;

	if (isc->sc_ih != NULL)
		isa_intr_disestablish(isc->sc_ic, isc->sc_ih);

	pmf_device_deregister(self);

	com_cleanup(self, 0);

#ifdef COM_HAYESP
	if (sc->sc_type == COM_TYPE_HAYESP)
		bus_space_unmap(cr->cr_iot, sc->sc_hayespioh, HAYESP_NPORTS);
#endif
	bus_space_unmap(cr->cr_iot, cr->cr_ioh, COM_NPORTS);

	return 0;
}

#ifdef COM_HAYESP
int
com_isa_isHAYESP(bus_space_handle_t hayespioh, struct com_softc *sc)
{
	char	val, dips;
	int	combaselist[] = { 0x3f8, 0x2f8, 0x3e8, 0x2e8 };
	bus_space_tag_t iot = sc->sc_regs.cr_iot;

	/*
	 * Hayes ESP cards have two iobases.  One is for compatibility with
	 * 16550 serial chips, and at the same ISA PC base addresses.  The
	 * other is for ESP-specific enhanced features, and lies at a
	 * different addressing range entirely (0x140, 0x180, 0x280, or 0x300).
	 */

	/* Test for ESP signature */
	if ((bus_space_read_1(iot, hayespioh, 0) & 0xf3) == 0)
		return (0);

	/*
	 * ESP is present at ESP enhanced base address; unknown com port
	 */

	/* Get the dip-switch configurations */
	bus_space_write_1(iot, hayespioh, HAYESP_CMD1, HAYESP_GETDIPS);
	dips = bus_space_read_1(iot, hayespioh, HAYESP_STATUS1);

	/* Determine which com port this ESP card services: bits 0,1 of  */
	/*  dips is the port # (0-3); combaselist[val] is the com_iobase */
	if (sc->sc_regs.cr_iobase != combaselist[dips & 0x03])
		return (0);

	printf(": ESP");

 	/* Check ESP Self Test bits. */
	/* Check for ESP version 2.0: bits 4,5,6 == 010 */
	bus_space_write_1(iot, hayespioh, HAYESP_CMD1, HAYESP_GETTEST);
	val = bus_space_read_1(iot, hayespioh, HAYESP_STATUS1); /* Clear reg1 */
	val = bus_space_read_1(iot, hayespioh, HAYESP_STATUS2);
	if ((val & 0x70) < 0x20) {
		printf("-old (%o)", val & 0x70);
		/* we do not support the necessary features */
		return (0);
	}

	/* Check for ability to emulate 16550: bit 8 == 1 */
	if ((dips & 0x80) == 0) {
		printf(" slave");
		/* XXX Does slave really mean no 16550 support?? */
		return (0);
	}

	/*
	 * If we made it this far, we are a full-featured ESP v2.0 (or
	 * better), at the correct com port address.
	 */
	sc->sc_type = COM_TYPE_HAYESP;
	sc->sc_hayespioh = hayespioh;
	sc->sc_fifolen = 1024;
	sc->sc_prescaler = 0;			/* set prescaler to x1. */
	printf(", 1024 byte fifo\n");
	return (1);
}
#endif

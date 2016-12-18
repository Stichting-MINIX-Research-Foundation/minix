/*	$NetBSD: dz_uba.c,v 1.28 2008/03/15 00:57:15 matt Exp $ */
/*
 * Copyright (c) 1998 Ludd, University of Lule}, Sweden. All rights reserved.
 * Copyright (c) 1996  Ken C. Wellsch.  All rights reserved.
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
 *      This product includes software developed at Ludd, University of
 *      Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dz_uba.c,v 1.28 2008/03/15 00:57:15 matt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/device.h>

#include <sys/bus.h>
#include <machine/pte.h>
#include <machine/trap.h>
#include <machine/scb.h>

#include <dev/qbus/ubavar.h>

#include <dev/dec/dzreg.h>
#include <dev/dec/dzvar.h>

#include "ioconf.h"

static	int	dz_uba_match(device_t, cfdata_t, void *);
static	void	dz_uba_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(dz_uba, sizeof(struct dz_softc),
    dz_uba_match, dz_uba_attach, NULL, NULL);

/* Autoconfig handles: setup the controller to interrupt, */
/* then complete the housecleaning for full operation */

static int
dz_uba_match(device_t parent, cfdata_t cf, void *aux)
{
	struct uba_attach_args *ua = aux;
	bus_space_tag_t	iot = ua->ua_iot;
	bus_space_handle_t ioh = ua->ua_ioh;
	int n;

	iot = iot; /* Silly GCC */
	/* Reset controller to initialize, enable TX interrupts */
	/* to catch floating vector info elsewhere when completed */

	bus_space_write_2(iot, ioh, DZ_UBA_CSR, DZ_CSR_MSE | DZ_CSR_TXIE);
	bus_space_write_1(iot, ioh, DZ_UBA_TCR, 1);

	DELAY(100000);	/* delay 1/10 second */

	bus_space_write_2(iot, ioh, DZ_UBA_CSR, DZ_CSR_RESET);

	/* Now wait up to 3 seconds for reset/clear to complete. */

	for (n = 0; n < 300; n++) {
		DELAY(10000);
		if ((bus_space_read_2(iot, ioh, DZ_UBA_CSR)&DZ_CSR_RESET) == 0)
			break;
	}

	/* If the RESET did not clear after 3 seconds, */
	/* the controller must be broken. */

	if (n >= 300)
		return (0);

	/* Register the TX interrupt handler */


	return (1);
}

static void
dz_uba_attach(device_t parent, device_t self, void *aux)
{
	struct dz_softc *sc = device_private(self);
	struct uba_attach_args *ua = aux;

	sc->sc_dev = self;
	sc->sc_iot = ua->ua_iot;
	sc->sc_ioh = ua->ua_ioh;

	sc->sc_dr.dr_csr = DZ_UBA_CSR;
	sc->sc_dr.dr_rbuf = DZ_UBA_RBUF;
	sc->sc_dr.dr_dtr = DZ_UBA_DTR;
	sc->sc_dr.dr_break = DZ_UBA_BREAK;
	sc->sc_dr.dr_tbuf = DZ_UBA_TBUF;
	sc->sc_dr.dr_tcr = DZ_UBA_TCR;
	sc->sc_dr.dr_dcd = DZ_UBA_DCD;
	sc->sc_dr.dr_ring = DZ_UBA_RING;

	sc->sc_dr.dr_firstreg = DZ_UBA_FIRSTREG;
	sc->sc_dr.dr_winsize = DZ_UBA_WINSIZE;

	sc->sc_type = DZ_DZ;

	/* Now register the TX & RX interrupt handlers */
	uba_intr_establish(ua->ua_icookie, ua->ua_cvec,
		dzxint, sc, &sc->sc_tintrcnt);
	uba_intr_establish(ua->ua_icookie, ua->ua_cvec - 4,
		dzrint, sc, &sc->sc_rintrcnt);
	uba_reset_establish(dzreset, self);

	dzattach(sc, ua->ua_evcnt, -1);
}

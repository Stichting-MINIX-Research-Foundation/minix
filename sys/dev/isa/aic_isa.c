/*	$NetBSD: aic_isa.c,v 1.26 2009/11/23 02:13:46 rmind Exp $	*/

/*
 * Copyright (c) 1994, 1995, 1996 Charles M. Hannum.  All rights reserved.
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
 * Copyright (c) 1994 Jarle Greipsland
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Acknowledgements: Many of the algorithms used in this driver are
 * inspired by the work of Julian Elischer (julian@tfs.com) and
 * Charles Hannum (mycroft@duality.gnu.ai.mit.edu).  Thanks a million!
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: aic_isa.c,v 1.26 2009/11/23 02:13:46 rmind Exp $");

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
#include <sys/intr.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/isa/isavar.h>

#include <dev/ic/aic6360reg.h>
#include <dev/ic/aic6360var.h>

static int	aic_isa_probe(device_t, cfdata_t, void *);
static void	aic_isa_attach(device_t, device_t, void *);

struct aic_isa_softc {
	struct	aic_softc sc_aic;	/* real "aic" softc */

	/* ISA-specific goo. */
	void	*sc_ih;			/* interrupt handler */
};

CFATTACH_DECL_NEW(aic_isa, sizeof(struct aic_isa_softc),
    aic_isa_probe, aic_isa_attach, NULL, NULL);


/*
 * INITIALIZATION ROUTINES (probe, attach ++)
 */

/*
 * aic_isa_probe: probe for AIC6360 SCSI-controller
 * returns non-zero value if a controller is found.
 */
int
aic_isa_probe(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	int rv;

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	/* Disallow wildcarded i/o address. */
	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return (0);

	/* Disallow wildcarded IRQ. */
	if (ia->ia_irq[0].ir_irq == ISA_UNKNOWN_IRQ)
		return (0);

	if (bus_space_map(iot, ia->ia_io[0].ir_addr, AIC_ISA_IOSIZE, 0, &ioh))
		return (0);

	rv = aic_find(iot, ioh);

	bus_space_unmap(iot, ioh, AIC_ISA_IOSIZE);

	if (rv) {
		ia->ia_nio = 1;
		ia->ia_io[0].ir_size = AIC_ISA_IOSIZE;

		ia->ia_nirq = 1;

		ia->ia_niomem = 0;
		ia->ia_ndrq = 0;
	}
	return rv;
}

void
aic_isa_attach(device_t parent, device_t self, void *aux)
{
	struct isa_attach_args *ia = aux;
	struct aic_isa_softc *isc = device_private(self);
	struct aic_softc *sc = &isc->sc_aic;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	isa_chipset_tag_t ic = ia->ia_ic;

	sc->sc_dev = self;

	printf("\n");

	if (bus_space_map(iot, ia->ia_io[0].ir_addr, AIC_ISA_IOSIZE, 0, &ioh)) {
		aprint_error_dev(self, "can't map i/o space\n");
		return;
	}

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	if (!aic_find(iot, ioh)) {
		aprint_error_dev(self, "aic_find failed\n");
		return;
	}

	isc->sc_ih = isa_intr_establish(ic, ia->ia_irq[0].ir_irq, IST_EDGE,
	    IPL_BIO, aicintr, sc);
	if (isc->sc_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt\n");
		return;
	}

	aicattach(sc);
}

/*	$NetBSD: slhci_isa.c,v 1.12 2011/03/08 04:58:21 kiyohara Exp $	*/

/*
 * Copyright (c) 2001 Kiyoshi Ikehara. All rights reserved.
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
 *      This product includes software developed by Kiyoshi Ikehara.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * ISA-USB host board
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: slhci_isa.c,v 1.12 2011/03/08 04:58:21 kiyohara Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <sys/bus.h>
#include <sys/cpu.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>

#include <dev/ic/sl811hsreg.h>
#include <dev/ic/sl811hsvar.h>

#include <dev/isa/isavar.h>

struct slhci_isa_softc {
	struct slhci_softc sc;
	isa_chipset_tag_t sc_ic;
	void	*sc_ih;
};

static int  slhci_isa_match(device_t, cfdata_t, void *);
static void slhci_isa_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(slhci_isa, sizeof(struct slhci_isa_softc),
    slhci_isa_match, slhci_isa_attach, NULL, slhci_activate);

static int
slhci_isa_match(device_t parent, cfdata_t cf, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	int result = 0;
	uint8_t sltype;

	if (bus_space_map(iot, ia->ia_io[0].ir_addr, SL11_PORTSIZE, 0, &ioh))
		goto out;

	bus_space_write_1(iot, ioh, SL11_IDX_ADDR, SL11_REV);
	sltype = SL11_GET_REV(bus_space_read_1(iot, ioh, SL11_IDX_DATA));

	result = slhci_supported_rev(sltype);

	bus_space_unmap(iot, ioh, SL11_PORTSIZE);

 out:
	return (result);
}

static void
slhci_isa_attach(device_t parent, device_t self, void *aux)
{
	struct slhci_isa_softc *isc = device_private(self);
	struct slhci_softc *sc = &isc->sc;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;

	sc->sc_dev = self;
	sc->sc_bus.hci_private = sc;

	printf("\n"); /* XXX still needed? */

	/* Map I/O space */
	if (bus_space_map(iot, ia->ia_io[0].ir_addr, SL11_PORTSIZE, 0, &ioh)) {
		printf("%s: can't map I/O space\n", SC_NAME(sc));
		return;
	}

	/* Initialize sc XXX power value unconfirmed */
	slhci_preinit(sc, NULL, iot, ioh, 30, SL11_IDX_DATA);

	/* Establish the interrupt handler */
	isc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq[0].ir_irq,
					IST_EDGE, IPL_USB, slhci_intr, sc);
	if (isc->sc_ih == NULL) {
		printf("%s: can't establish interrupt\n", SC_NAME(sc));
		return;
	}

	/* Attach SL811HS/T */
	if (slhci_attach(sc))
		printf("%s: slhci_attach failed\n", SC_NAME(sc));
}

/*	$NetBSD: com_ofisa.c,v 1.15 2012/10/27 17:18:27 chs Exp $	*/

/*
 * Copyright 1997, 1998
 * Digital Equipment Corporation. All rights reserved.
 *
 * This software is furnished under license and may be used and
 * copied only in accordance with the following terms and conditions.
 * Subject to these conditions, you may download, copy, install,
 * use, modify and distribute this software in source and/or binary
 * form. No title or ownership is transferred hereby.
 *
 * 1) Any source code used, modified or distributed must reproduce
 *    and retain this copyright notice and list of conditions as
 *    they appear in the source file.
 *
 * 2) No right is granted to use any trade name, trademark, or logo of
 *    Digital Equipment Corporation. Neither the "Digital Equipment
 *    Corporation" name nor any trademark or logo of Digital Equipment
 *    Corporation may be used to endorse or promote products derived
 *    from this software without the prior written permission of
 *    Digital Equipment Corporation.
 *
 * 3) This software is provided "AS-IS" and any express or implied
 *    warranties, including but not limited to, any implied warranties
 *    of merchantability, fitness for a particular purpose, or
 *    non-infringement are disclaimed. In no event shall DIGITAL be
 *    liable for any damages whatsoever, and in particular, DIGITAL
 *    shall not be liable for special, indirect, consequential, or
 *    incidental damages or damages for lost profits, loss of
 *    revenue or loss of use, whether such damages arise in contract,
 *    negligence, tort, under statute, in equity, at law or otherwise,
 *    even if advised of the possibility of such damage.
 */

/*
 * OFW Attachment for 'com' serial driver
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: com_ofisa.c,v 1.15 2012/10/27 17:18:27 chs Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/tty.h>

#include <sys/intr.h>
#include <sys/bus.h>

#include <dev/ofw/openfirm.h>
#include <dev/isa/isavar.h>
#include <dev/ofisa/ofisavar.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

struct com_ofisa_softc {
	struct	com_softc sc_com;	/* real "com" softc */

	/* OFW ISA-specific goo. */
	void	*sc_ih;			/* interrupt handler */
};

int com_ofisa_probe(device_t, cfdata_t , void *);
void com_ofisa_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(com_ofisa, sizeof(struct com_ofisa_softc),
    com_ofisa_probe, com_ofisa_attach, NULL, NULL);

int
com_ofisa_probe(device_t parent, cfdata_t cf, void *aux)
{
	struct ofisa_attach_args *aa = aux;
	static const char *const compatible_strings[] = { "pnpPNP,501", NULL };
	int rv = 0;

	if (of_compatible(aa->oba.oba_phandle, compatible_strings) != -1)
		rv = 5;
#ifdef _COM_OFISA_MD_MATCH
	if (!rv)
		rv = com_ofisa_md_match(parent, cf, aux);
#endif
	return (rv);
}

void
com_ofisa_attach(device_t parent, device_t self, void *aux)
{
	struct com_ofisa_softc *osc = device_private(self);
        struct com_softc *sc = &osc->sc_com;
	struct ofisa_attach_args *aa = aux;
	struct ofisa_reg_desc reg;
	struct ofisa_intr_desc intr;
	bus_space_handle_t ioh;
	bus_space_tag_t iot;
	bus_addr_t iobase;
	int freq;
	char freqbuf[4];
	int n;

	sc->sc_dev = self;

	/*
	 * We're living on an ofw.  We have to ask the OFW what our
	 * registers and interrupts properties look like.
	 *
	 * We expect exactly one register region and one interrupt.
	 */

	n = ofisa_reg_get(aa->oba.oba_phandle, &reg, 1);
#ifdef _COM_OFISA_MD_REG_FIXUP
	n = com_ofisa_md_reg_fixup(parent, self, aux, &reg, 1, n);
#endif
	if (n != 1) {
		aprint_error(": error getting register data\n");
		return;
	}
	if (reg.len != 8) {
		aprint_error(": weird register size (%lu, expected 8)\n",
		    (unsigned long)reg.len);
		return;
	}

	n = ofisa_intr_get(aa->oba.oba_phandle, &intr, 1);
#ifdef _COM_OFISA_MD_INTR_FIXUP
	n = com_ofisa_md_intr_fixup(parent, self, aux, &intr, 1, n);
#endif
	if (n != 1) {
		aprint_error(": error getting interrupt data\n");
		return;
	}

	if (OF_getproplen(aa->oba.oba_phandle, "clock-frequency") != 4 ||
	    OF_getprop(aa->oba.oba_phandle, "clock-frequency", freqbuf,
	      sizeof freqbuf) != 4)
		freq = COM_FREQ;
	else
		freq = of_decode_int(&freqbuf[0]);

	iot = (reg.type == OFISA_REG_TYPE_IO) ? aa->iot : aa->memt;
	iobase = reg.addr;
	sc->sc_frequency = freq;

	if (!com_is_console(iot, iobase, &ioh) &&
	    bus_space_map(iot, iobase, reg.len, 0, &ioh)) {
		aprint_error(": can't map register space\n");
                return;
        }
	COM_INIT_REGS(sc->sc_regs, iot, ioh, iobase);

	osc->sc_ih = isa_intr_establish(aa->ic, intr.irq, intr.share,
	    IPL_SERIAL, comintr, sc);

	com_attach_subr(sc);

#if 0
	printf("%s: registers: ", device_xname(sc->sc_dev));
	ofisa_reg_print(&reg, 1);
	printf("\n");
	printf("%s: interrupts: ", device_xname(sc->sc_dev));
	ofisa_intr_print(&intr, 1);
	printf("\n");
#endif
}

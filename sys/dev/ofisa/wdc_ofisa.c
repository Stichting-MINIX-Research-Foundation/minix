/*	$NetBSD: wdc_ofisa.c,v 1.33 2012/07/31 15:50:35 bouyer Exp $	*/

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
 * OFW Attachment for 'wdc' disk controller driver
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: wdc_ofisa.c,v 1.33 2012/07/31 15:50:35 bouyer Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/malloc.h>

#include <sys/intr.h>
#include <sys/bus.h>

#include <dev/ofw/openfirm.h>
#include <dev/isa/isavar.h>
#include <dev/ofisa/ofisavar.h>

#include <dev/ic/wdcreg.h>		/* ??? */
#include <dev/ata/atavar.h>
#include <dev/ic/wdcvar.h>

struct wdc_ofisa_softc {
	struct wdc_softc sc_wdcdev;
	struct ata_channel *sc_chanlist[1];
	struct ata_channel sc_channel;
	struct ata_queue sc_chqueue;
	struct wdc_regs wdc_regs;
	void	*sc_ih;
};

static int wdc_ofisa_probe(device_t, cfdata_t, void *);
static void wdc_ofisa_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(wdc_ofisa, sizeof(struct wdc_ofisa_softc),
    wdc_ofisa_probe, wdc_ofisa_attach, NULL, NULL);

static int
wdc_ofisa_probe(device_t parent, cfdata_t cf, void *aux)
{
	struct ofisa_attach_args *aa = aux;
	static const char *const compatible_strings[] = { "pnpPNP,600", NULL };
	int rv = 0;

	if (of_compatible(aa->oba.oba_phandle, compatible_strings) != -1)
		rv = 5;
#ifdef _WDC_OFISA_MD_MATCH
	if (!rv)
		rv = wdc_ofisa_md_match(parent, cf, aux);
#endif
	return (rv);
}

static void
wdc_ofisa_attach(device_t parent, device_t self, void *aux)
{
	struct wdc_ofisa_softc *sc = device_private(self);
	struct wdc_regs *wdr;
	struct ofisa_attach_args *aa = aux;
	struct ofisa_reg_desc reg[2];
	struct ofisa_intr_desc intr;
	int n;
	bus_space_handle_t ioh;

	/*
	 * We're living on an ofw.  We have to ask the OFW what our
	 * registers and interrupts properties look like.
	 *
	 * We expect exactly two register regions and one interrupt.
	 */

	sc->sc_wdcdev.sc_atac.atac_dev = self;
	sc->sc_wdcdev.regs = wdr = &sc->wdc_regs;

	n = ofisa_reg_get(aa->oba.oba_phandle, reg, 2);
#ifdef _WDC_OFISA_MD_REG_FIXUP
	n = wdc_ofisa_md_reg_fixup(parent, self, aux, reg, 2, n);
#endif
	if (n != 2) {
		aprint_error(": error getting register data\n");
		return;
	}
	if (reg[0].len != 8 || reg[1].len != 2) {
		aprint_error(": weird register size (%lu/%lu, expected 8/2)\n",
		    (unsigned long)reg[0].len, (unsigned long)reg[1].len);
		return;
	}

	n = ofisa_intr_get(aa->oba.oba_phandle, &intr, 1);
#ifdef _WDC_OFISA_MD_INTR_FIXUP
	n = wdc_ofisa_md_intr_fixup(parent, self, aux, &intr, 1, n);
#endif
	if (n != 1) {
		aprint_error(": error getting interrupt data\n");
		return;
	}

	wdr->cmd_iot = (reg[0].type == OFISA_REG_TYPE_IO) ? aa->iot : aa->memt;
	wdr->ctl_iot = (reg[1].type == OFISA_REG_TYPE_IO) ? aa->iot : aa->memt;
        if (bus_space_map(wdr->cmd_iot, reg[0].addr, 8, 0, &ioh) ||
            bus_space_map(wdr->ctl_iot, reg[1].addr, 1, 0,
	      &wdr->ctl_ioh)) {
                aprint_error(": can't map register spaces\n");
		return;
        }
	wdr->cmd_baseioh = ioh;

	for (n = 0; n < WDC_NREG; n++) {
		if (bus_space_subregion(wdr->cmd_iot, ioh, n,
		    n == 0 ? 4 : 1, &wdr->cmd_iohs[n]) != 0) {
                	aprint_error(": can't subregion register space\n");
			return;
		}
	}

	sc->sc_ih = isa_intr_establish(aa->ic, intr.irq, intr.share,
	    IPL_BIO, wdcintr, &sc->sc_channel);

	aprint_normal("\n");
	sc->sc_wdcdev.sc_atac.atac_cap |= ATAC_CAP_DATA16;
	sc->sc_chanlist[0] = &sc->sc_channel;
	sc->sc_wdcdev.sc_atac.atac_channels = sc->sc_chanlist;
	sc->sc_wdcdev.sc_atac.atac_nchannels = 1;
	sc->sc_wdcdev.wdc_maxdrives = 2;
	sc->sc_channel.ch_channel = 0;
	sc->sc_channel.ch_atac = &sc->sc_wdcdev.sc_atac;
	sc->sc_channel.ch_queue = &sc->sc_chqueue;

	wdc_init_shadow_regs(&sc->sc_channel);

	wdcattach(&sc->sc_channel);

#if 0
	aprint_verbose_dev(self, "registers: ");
	ofisa_reg_print(reg, 2);
	aprint_verbose("\n");
	aprint_verbose_dev(self, "interrupts: ");
	ofisa_intr_print(&intr, 1);
	aprint_verbose("\n");
#endif
}

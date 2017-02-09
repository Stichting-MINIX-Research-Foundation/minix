/*	$NetBSD: if_tr_isapnp.c,v 1.21 2012/10/27 17:18:26 chs Exp $	*/

/*
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Onno van der Linden.
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
__KERNEL_RCSID(0, "$NetBSD: if_tr_isapnp.c,v 1.21 2012/10/27 17:18:26 chs Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <sys/cpu.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/ic/tropicreg.h>
#include <dev/ic/tropicvar.h>

#include <dev/isa/isavar.h>

#include <dev/isapnp/isapnpreg.h>
#include <dev/isapnp/isapnpvar.h>
#include <dev/isapnp/isapnpdevs.h>

int	tr_isapnp_match(device_t, cfdata_t, void *);
void	tr_isapnp_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(tr_isapnp, sizeof(struct tr_softc),
    tr_isapnp_match, tr_isapnp_attach, NULL, NULL);

int
tr_isapnp_match(device_t parent, cfdata_t match, void *aux)
{
	int pri, variant;

	pri =  isapnp_devmatch(aux, &isapnp_tr_devinfo, &variant);
	if (pri && variant > 0)
		pri = 0;
	return pri;
}


void
tr_isapnp_attach(device_t parent, device_t self, void *aux)
{
	struct tr_softc *sc = device_private(self);
	struct isapnp_attach_args *ipa = aux;
	int mmioidx, sramidx;

	printf("\n");

	if (isapnp_config(ipa->ipa_iot, ipa->ipa_memt, ipa)) {
		aprint_error_dev(self, "error in region allocation\n");
		return;
	}

	printf("%s: %s %s\n", device_xname(self), ipa->ipa_devident,
	    ipa->ipa_devclass);

	sc->sc_dev = self;
	sc->sc_piot = ipa->ipa_iot;
	sc->sc_pioh = ipa->ipa_io[0].h;

	if (strcmp(ipa->ipa_devlogic, "TCM3190") == 0) {
		mmioidx = 0;
		sramidx = 1;
	}
	else {	/* Default */
		mmioidx = 1;
		sramidx = 0;
	}
	sc->sc_memt = ipa->ipa_memt;
	sc->sc_mmioh = ipa->ipa_mem[mmioidx].h;
	sc->sc_sramh = ipa->ipa_mem[sramidx].h;
	sc->sc_memwinsz = ipa->ipa_mem[sramidx].length;
	/*
	 * Determine total RAM on adapter and decide how much to use.
	 * XXX Since we don't use RAM paging, use sc_memwinsz for now.
	 */
	sc->sc_memsize = sc->sc_memwinsz;
	sc->sc_memreserved = 0;

	sc->sc_aca = TR_ACA_OFFSET;
	sc->sc_maddr = ipa->ipa_mem[sramidx].base;
	/*
	 * Reset the card.
	 */
	if (tr_reset(sc))
		return;

	sc->sc_mediastatus = NULL;
	sc->sc_mediachange = NULL;

	if (tr_attach(sc))
		return;

	sc->sc_ih = isa_intr_establish(ipa->ipa_ic, ipa->ipa_irq[0].num,
		ipa->ipa_irq[0].type, IPL_NET, tr_intr, sc);
}

/*	$NetBSD: if_ne_mca.c,v 1.15 2008/04/28 20:23:53 martin Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jaromir Dolecek.
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

/*
 * Driver for Novell NE/2 Ethernet Adapter (and clones).
 *
 * According to Linux ne2 driver, Arco and Compex card should be also
 * supported by this driver. However, NetBSD driver was only tested
 * with the Novell adapter so far.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_ne_mca.c,v 1.15 2008/04/28 20:23:53 martin Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/protosw.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <sys/bus.h>

#include <dev/ic/dp8390reg.h>
#include <dev/ic/dp8390var.h>

#include <dev/ic/ne2000reg.h>
#include <dev/ic/ne2000var.h>

#include <dev/ic/rtl80x9reg.h>
#include <dev/ic/rtl80x9var.h>

#include <dev/mca/mcadevs.h>
#include <dev/mca/mcavar.h>

#define	NE2_NPORTS	0x30

struct ne_mca_softc {
	struct ne2000_softc sc_ne2000;		/* real "ne2000" softc */

	/* MCA-specific goo */
	void		*sc_ih;		/* interrupt handle */
};

int	ne_mca_match(device_t, cfdata_t , void *);
void	ne_mca_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(ne_mca, sizeof(struct ne_mca_softc),
    ne_mca_match, ne_mca_attach, NULL, NULL);

static const struct ne_mca_products {
	u_int32_t ne_id;
	const char *ne_name;
} ne_mca_products[] = {
	{ MCA_PRODUCT_ARCOAE,	"Arco Electronics AE/2 Ethernet Adapter" },
	{ MCA_PRODUCT_NE2,	"Novell NE/2 Ethernet Adapter" },
	{ MCA_PRODUCT_CENET16, "Compex Inc. PS/2 ENET16-MC/P Microchannel Ad."},
	{ 0, NULL }
};

static const struct ne_mca_products *ne_mca_lookup(int id);

static const struct ne_mca_products *
ne_mca_lookup(int id)
{
	const struct ne_mca_products *np;

	for(np = ne_mca_products; np->ne_name; np++)
		if (id == np->ne_id)
			return (np);

	return (NULL);
}

int
ne_mca_match(device_t parent, cfdata_t cf, void *aux)
{
	struct mca_attach_args *ma = aux;

	if (ne_mca_lookup(ma->ma_id))
		return (1);

	return (0);
}

/* These values were taken from NE/2 ADF file */
static const int ne_mca_irq[] = {
	3, 4, 5, 9
};
static const int ne_mca_iobase[] = {
	0, 0x1000, 0x2020, 0x8020, 0xa0a0, 0xb0b0, 0xc0c0, 0xc3d0
};

void
ne_mca_attach(device_t parent, device_t self, void *aux)
{
	struct ne_mca_softc *psc = device_private(self);
	struct ne2000_softc *nsc = &psc->sc_ne2000;
	struct dp8390_softc *dsc = &nsc->sc_dp8390;
	struct mca_attach_args *ma = aux;
	bus_space_tag_t nict;
	bus_space_handle_t nich;
	bus_space_tag_t asict;
	bus_space_handle_t asich;
	int pos2, iobase, irq;
	const struct ne_mca_products *np;

	dsc->sc_dev = self;

	pos2 = mca_conf_read(ma->ma_mc, ma->ma_slot, 2);

	/*
	 * POS register 2: (adf pos0)
	 *
	 * 7 6 5 4 3 2 1 0
	 *   \_/ | \___/ \__ enable: 0=adapter disabled, 1=adapter enabled
	 *    |  |     \____ I/O, Mem: 001=0x1000-0x102f 010=0x2020-0x204f
	 *    |  |             011=0x8020-0x804f 100=0xa0a0-0xa0cf
	 *    |  |             101=0xb0b0-0xb0df 110=0xc0c0-0xc0ef
	 *     \  \            111=0xc3d0-0xc3ff
	 *      \  \________ Boot Rom: 1=disabled 0=enabled
	 *       \__________ Interrupt level: 00=3 01=4 10=5 11=9
	 */

	np = ne_mca_lookup(ma->ma_id);

	iobase = ne_mca_iobase[(pos2 & 0x0e) >> 1];
	irq = ne_mca_irq[(pos2 & 0x60) >> 5];

	aprint_normal(" slot %d irq %d: %s\n", ma->ma_slot + 1, irq,
	    np->ne_name);

	nict = ma->ma_iot;

	/* Map the device. */
	if (bus_space_map(nict, iobase, NE2_NPORTS, 0, &nich)) {
		aprint_error_dev(self, "can't map i/o space\n");
		return;
	}

	asict = nict;
	if (bus_space_subregion(nict, nich, NE2000_ASIC_OFFSET,
	    NE2000_ASIC_NPORTS, &asich)) {
		aprint_error_dev(self, "can't subregion i/o space\n");
		return;
	}

	dsc->sc_regt = nict;
	dsc->sc_regh = nich;

	nsc->sc_asict = asict;
	nsc->sc_asich = asich;

	/* This interface is always enabled. */
	dsc->sc_enabled = 1;

	dsc->sc_mediachange	= NULL;
	dsc->sc_mediastatus	= NULL;
	dsc->sc_media_init	= NULL;
	dsc->init_card		= NULL;

	/*
	 * This is necessary for NE/2. Hopefully the other clones also work
	 * this way.
	 */
	nsc->sc_type = NE2000_TYPE_AX88190;

	/*
	 * Do generic NE2000 attach.  This will read the station address
	 * from the EEPROM.
	 */
	if (ne2000_attach(nsc, NULL))
		return;

	/* establish interrupt handler */
	psc->sc_ih = mca_intr_establish(ma->ma_mc, irq, IPL_NET, dp8390_intr,
			dsc);
	if (psc->sc_ih == NULL) {
		aprint_error_dev(self,
		    "couldn't establish interrupt handler\n");
		return;
	}
}

/*	$NetBSD: if_ep_mca.c,v 1.22 2009/03/14 15:36:18 dsl Exp $	*/

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
 * Copyright (c) 1997 Jonathan Stone <jonathan@NetBSD.org>
 * Copyright (c) 1994 Herb Peyerl <hpeyerl@beer.org>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Herb Peyerl.
 * 4. The name of Herb Peyerl may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

/*
 * Driver for 3Com 3c529 cards.
 *
 * If you encouter sucky performance, try kernel without DEBUG/DIAGNOSTIC.
 * This helped on my test machine to change the performance of the card
 * from like 5KB/s to like 800 KB/s.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_ep_mca.c,v 1.22 2009/03/14 15:36:18 dsl Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <sys/bus.h>

#include <dev/mii/miivar.h>

#include <dev/ic/elink3var.h>
#include <dev/ic/elink3reg.h>

#include <dev/mca/mcavar.h>
#include <dev/mca/mcadevs.h>

/*
 * MCA constants.
 */
#define MCA_CBIO		0x200	/* Configuration Base IO Address */
#define MCA_IOSZ		0x10	/* I/O space size */

int ep_mca_match(device_t , cfdata_t , void *);
void ep_mca_attach(device_t , device_t , void *);

CFATTACH_DECL_NEW(ep_mca, sizeof(struct ep_softc),
    ep_mca_match, ep_mca_attach, NULL, NULL);

const struct ep_mca_product {
	u_int32_t	epp_prodid;	/* MCA product ID */
	const char	*epp_name;	/* device name */
} ep_mca_products[] = {
	{ MCA_PRODUCT_3C529,	"3C529 Ethernet Adapter" },
	{ MCA_PRODUCT_3C529_TP,	"3c529-TP Ethernet Adapter" },
	{ MCA_PRODUCT_3C529_TM, "3c529 Ethernet Adapter (test mode)" },
	{ MCA_PRODUCT_3C529_2T, "3c529 Ethernet Adapter (10base2/T)" },
	{ MCA_PRODUCT_3C529_T,	"3c529 Ethernet Adapter (10baseT)"   },

	{ 0,			NULL },
};

static const struct ep_mca_product *ep_mca_lookup
   (const struct mca_attach_args *);

static const struct ep_mca_product *
ep_mca_lookup(const struct mca_attach_args *ma)
{
	const struct ep_mca_product *epp;

	for (epp = ep_mca_products; epp->epp_name != NULL; epp++)
		if (ma->ma_id == epp->epp_prodid)
			return (epp);

	return (NULL);
}

int
ep_mca_match(device_t parent, cfdata_t match, void *aux)
{
	struct mca_attach_args *ma = (struct mca_attach_args *) aux;

	if (ep_mca_lookup(ma) != NULL)
		return (1);

	return (0);
}

void
ep_mca_attach(device_t parent, device_t self, void *aux)
{
	struct ep_softc *sc = device_private(self);
	struct mca_attach_args *ma = aux;
	bus_space_handle_t ioh;
	int pos4, pos5, iobase, irq, media;
	const struct ep_mca_product *epp;

	pos4 = mca_conf_read(ma->ma_mc, ma->ma_slot, 4);
	pos5 = mca_conf_read(ma->ma_mc, ma->ma_slot, 5);

	/*
	 * POS register 2: (adf pos0)
	 * 7 6 5 4 3 2 1 0
	 *               \__ enable: 0=adapter disabled, 1=adapter enabled
	 *
	 * POS register 3: (adf pos1)
	 *
	 * 7 6 5 4 3 2 1 0
	 * \________/
	 *          \_______ Boot ROM Address Range: 0=disabled
	 *                     X=0xc2000-0xc3fff + (x * 0x2000)
	 *
	 * POS register 4: (adf pos2)
	 *
	 * 7 6 5 4 3 2 1 0
	 * \________/  \_/
	 *          \    \__ Transceiver Type: 00=on-board (RJ45), 01=ext(AUI)
	 *           \______ I/O Address Range: 0x200-0x20f + ((x>>2) * 0x400)
	 *
	 * POS register 5: (adf pos3)
	 *
	 * 7 6 5 4 3 2 1 0
	 *          \____/
	 *               \__ Interrupt level
	 */

	iobase = MCA_CBIO + (((pos4 & 0xfc) >> 2) * 0x400);
	irq = (pos5 & 0x0f);

	sc->sc_dev = self;
	/* map the pio registers */
	if (bus_space_map(ma->ma_iot, iobase, MCA_IOSZ, 0, &ioh)) {
		aprint_error_dev(sc->sc_dev, "unable to map i/o space\n");
		return;
	}

	sc->sc_iot = ma->ma_iot;
	sc->sc_ioh = ioh;

	epp = ep_mca_lookup(ma);
	if (epp == NULL) {
		printf("\n");
		panic("ep_mca_attach: impossible");
	}

	printf(" slot %d irq %d: 3Com %s\n", ma->ma_slot + 1,
		irq, epp->epp_name);

	sc->enable = NULL;
	sc->disable = NULL;
	sc->enabled = 1;

	sc->bustype = ELINK_BUS_MCA;
	sc->ep_flags = 0;

	if (epconfig(sc, ELINK_CHIPSET_3C509, NULL))
		return;

	/* Map and establish the interrupt. */
	sc->sc_ih = mca_intr_establish(ma->ma_mc, irq, IPL_NET, epintr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "couldn't establish interrupt handler\n");
		return;
	}

	/*
	 * Set default media to be same as the one selected in POS.
	 */
	switch (pos4 & 0x03) {
	case 0x00:
		/* on-board RJ-45 */
		media = IFM_10_T;
		break;
	case 0x01:
		/* external AUI */
		media = IFM_10_5;
		break;
	case 0x02:
		/* undefined, should never be set */
		media = IFM_NONE;
		break;
	case 0x03:
		/* BNC */
		media = IFM_10_2;
		break;
	default:
		aprint_error_dev(sc->sc_dev, " cannot determine media\n");
		return;
	}
	ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|media);
}

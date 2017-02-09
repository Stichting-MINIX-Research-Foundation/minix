/*	$NetBSD: if_ep_eisa.c,v 1.43 2014/03/29 19:28:24 christos Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *      This product includes software developed by Jonathan Stone.
 * 4. The name of Herb Peyerl or Jonathan Stone may not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
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
__KERNEL_RCSID(0, "$NetBSD: if_ep_eisa.c,v 1.43 2014/03/29 19:28:24 christos Exp $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
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

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_inarp.h>
#endif

#include <sys/cpu.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/mii/miivar.h>

#include <dev/ic/elink3var.h>
#include <dev/ic/elink3reg.h>

#include <dev/eisa/eisareg.h>
#include <dev/eisa/eisavar.h>
#include <dev/eisa/eisadevs.h>

static int	ep_eisa_match(device_t , cfdata_t , void *);
static void	ep_eisa_attach(device_t , device_t , void *);

CFATTACH_DECL_NEW(ep_eisa, sizeof(struct ep_softc),
    ep_eisa_match, ep_eisa_attach, NULL, NULL);

/* XXX move these somewhere else */
/* While attaching we need a few special EISA registers of the card,
   these are mapped at slotbase+EP_EISA_CFG_BASE with len EP_EISA_CFG_SIZE */
#define EP_EISA_CFG_BASE	0x0c80
#define	EP_EISA_CFG_CONTROL	0x0004	/* 1 byte */
#define	EP_EISA_CFG_RESOURCE	0x0008	/* 2 byte */
#define	EP_EISA_CFG_SIZE	0x000a

/* The generic driver backend only needs access to the standard ports,
   mapped at the beginning of the eisa slot space */
#define	EP_IOPORT_OFFSET	0x0000
#define	EP_IOPORT_SIZE		0x0020

/* Bits for the eisa control register */
#define EISA_RESET	0x04
#define EISA_ERROR	0x02
#define EISA_ENABLE	0x01

static const struct ep_eisa_product {
	const char	*eep_eisaid;	/* EISA ID */
	u_short		eep_chipset;	/* 3Com chipset used */
	int		eep_flags;	/* initial softc flags */
	const char	*eep_name;	/* device name */
} ep_eisa_products[] = {
	{ "TCM5090",			ELINK_CHIPSET_3C509,
	  0,				EISA_PRODUCT_TCM5090 },
	{ "TCM5091",			ELINK_CHIPSET_3C509,
	  0,				EISA_PRODUCT_TCM5091 },
	{ "TCM5092",			ELINK_CHIPSET_3C509,
	  0,				EISA_PRODUCT_TCM5092 },
	{ "TCM5093",			ELINK_CHIPSET_3C509,
	  0,				EISA_PRODUCT_TCM5093 },
	{ "TCM5094",			ELINK_CHIPSET_3C509,
	  0,				EISA_PRODUCT_TCM5094 },
	{ "TCM5095",			ELINK_CHIPSET_3C509,
	  0,				EISA_PRODUCT_TCM5095 },
	{ "TCM5098",			ELINK_CHIPSET_3C509,
	  0,				EISA_PRODUCT_TCM5098 },

	/*
	 * Note: The 3c597 Fast Etherlink MII (TCM5972) is an
	 * MII connector for an external PHY.  We treat it as
	 * `manual' in the core driver.
	 */
	{ "TCM5920",			ELINK_CHIPSET_VORTEX,
	  0,				EISA_PRODUCT_TCM5920 },
	{ "TCM5970",			ELINK_CHIPSET_VORTEX,
	  0,				EISA_PRODUCT_TCM5970 },
	{ "TCM5971",			ELINK_CHIPSET_VORTEX,
	  0,				EISA_PRODUCT_TCM5971 },
	{ "TCM5972",			ELINK_CHIPSET_VORTEX,
	  0,				EISA_PRODUCT_TCM5972 },

	{ NULL,				0,
	  0,				NULL },
};

static const struct ep_eisa_product *
ep_eisa_lookup(const struct eisa_attach_args *ea)
{
	const struct ep_eisa_product *eep;

	for (eep = ep_eisa_products; eep->eep_name != NULL; eep++)
		if (strcmp(ea->ea_idstring, eep->eep_eisaid) == 0)
			return (eep);

	return (NULL);
}

static int
ep_eisa_match(device_t parent, cfdata_t match, void *aux)
{
	struct eisa_attach_args *ea = aux;

	/* must match one of our known ID strings */
	if (ep_eisa_lookup(ea) != NULL)
		return (1);

	return (0);
}

static void
ep_eisa_attach(device_t parent, device_t self, void *aux)
{
	struct ep_softc *sc = device_private(self);
	struct eisa_attach_args *ea = aux;
	bus_space_tag_t iot = ea->ea_iot;
	bus_space_handle_t ioh, ioh_cfg;
	eisa_chipset_tag_t ec = ea->ea_ec;
	eisa_intr_handle_t ih;
	const char *intrstr;
	const struct ep_eisa_product *eep;
	u_int irq;
	char intrbuf[EISA_INTRSTR_LEN];

	/* Map i/o space. */
	if (bus_space_map(iot, EISA_SLOT_ADDR(ea->ea_slot) + EP_EISA_CFG_BASE,
	    EP_EISA_CFG_SIZE, 0, &ioh_cfg)) {
		printf("\n");
		panic("ep_eisa_attach: can't map eisa cfg i/o space");
	}
	if (bus_space_map(iot, EISA_SLOT_ADDR(ea->ea_slot) + EP_IOPORT_OFFSET,
	    EP_IOPORT_SIZE, 0, &ioh)) {
		printf("\n");
		panic("ep_eisa_attach: can't map i/o space");
	}

	sc->sc_dev = self;
	sc->sc_ioh = ioh;
	sc->sc_iot = iot;

	bus_space_write_1(iot, ioh_cfg, EP_EISA_CFG_CONTROL, EISA_ENABLE);
	delay(4000);

	/* Read the IRQ from the card. */
	irq = bus_space_read_2(iot, ioh_cfg, EP_EISA_CFG_RESOURCE) >> 12;

	eep = ep_eisa_lookup(ea);
	if (eep == NULL) {
		printf("\n");
		panic("ep_eisa_attach: impossible");
	}

	/* we don't need access to the config registers any more, but
	   noone else should be able to map this space, so keep it
	   reserved? */
#if 0
	bus_space_unmap(iot, ioh_cfg, EP_EISA_CFG_SIZE);
#endif

	printf(": %s\n", eep->eep_name);

	sc->enable = NULL;
	sc->disable = NULL;
	sc->enabled = 1;

	sc->bustype = ELINK_BUS_EISA;
	sc->ep_flags = eep->eep_flags;

	if (eisa_intr_map(ec, irq, &ih)) {
		aprint_error_dev(sc->sc_dev, "couldn't map interrupt (%u)\n",
		    irq);
		return;
	}
	intrstr = eisa_intr_string(ec, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = eisa_intr_establish(ec, ih, IST_EDGE, IPL_NET,
	    epintr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	if (intrstr != NULL)
		aprint_normal_dev(sc->sc_dev, "interrupting at %s\n", intrstr);

	epconfig(sc, eep->eep_chipset, NULL);
}

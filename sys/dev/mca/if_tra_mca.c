/*	$NetBSD: if_tra_mca.c,v 1.17 2015/04/13 16:33:24 riastradh Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
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
 * Driver for Tiara LANCard/E II and friends adapted from if_ate_mca.c
 * by Dave J. Barnes 2004.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_tra_mca.c,v 1.17 2015/04/13 16:33:24 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/ic/mb86950reg.h>
#include <dev/ic/mb86950var.h>

#include <dev/mca/mcavar.h>
#include <dev/mca/mcadevs.h>

int	tiara_mca_match(device_t, cfdata_t, void *);
void	tiara_mca_attach(device_t, device_t, void *);

#define TIARA_NPORTS 0x20 /* 32 */
#define TIARA_PROM_ID 24 /* offset to mac addr stored in prom */

struct tiara_softc {
	struct	mb86950_softc sc_mb86950;	/* real "mb86950" softc */

	/* MCA-specific goo. */
	void	*sc_ih;				/* interrupt cookie */
};

CFATTACH_DECL_NEW(tra_mca, sizeof(struct tiara_softc),
    tiara_mca_match, tiara_mca_attach, NULL, NULL);

static const struct tiara_mca_product {
	u_int32_t	tra_prodid;	/* MCA product ID */
	const char	*tra_name;	/* device name */
} tiara_mca_products[] = {
	{ MCA_PRODUCT_TIARA,	"Tiara LANCard/E2"},
	{ MCA_PRODUCT_TIARA_TP,	"Tiara LANCard/E2 TP"},
	{ MCA_PRODUCT_SMC3016,  "SMC 3016/MC"},
	{ 0,			NULL },
};

static const struct tiara_mca_product *tiara_mca_lookup(u_int32_t);

static const struct tiara_mca_product *
tiara_mca_lookup(u_int32_t id)
{
	const struct tiara_mca_product *tra_p;

	for (tra_p = tiara_mca_products; tra_p->tra_name != NULL; tra_p++)
		if (id == tra_p->tra_prodid)
			return (tra_p);

	return (NULL);
}

int
tiara_mca_match(device_t parent, cfdata_t match, void *aux)
{
	struct mca_attach_args *ma = (struct mca_attach_args *) aux;

	if (tiara_mca_lookup(ma->ma_id) != NULL)
		return (1);

	return (0);
}

/* see POS diagrams below for explanation */
static const int tiara_irq[] = {
	3, 4, 7, 9
};
static const int smc_iobase[] = {
	0x300, 0x340, 0x360, 0x1980, 0x2000, 0x5680, 0x5900, 0x8080
};
static const int smc_irq[] = {
	9, 10, 11, 15, 3, 5, 7, 4
};

void
tiara_mca_attach(device_t parent, device_t self, void *aux)
{
	struct tiara_softc *isc = device_private(self);
	struct mb86950_softc *sc = &isc->sc_mb86950;
	struct mca_attach_args *ma = aux;
	bus_space_tag_t iot = ma->ma_iot;
	bus_space_handle_t ioh;
	u_int8_t myea[ETHER_ADDR_LEN];
	int pos2;
	int iobase = 0, irq = 0;
	const struct tiara_mca_product *tra_p;

	pos2 = mca_conf_read(ma->ma_mc, ma->ma_slot, 2);

	tra_p = tiara_mca_lookup(ma->ma_id);

	switch (tra_p->tra_prodid) {

	case MCA_PRODUCT_TIARA:
	case MCA_PRODUCT_TIARA_TP:
		/*
		 * POS register 2: (adf pos0)
		 * 7 6 5 4 3 2 1 0
		 * \_____/ \_/  \ \__ enable: 0=disabled, 1=enabled
		 *       \   \   \___ boot rom: 0=disabled, 1=enabled
		 *        \   \______ IRQ 00=3 01=4 10=7 11=9
		 *         \_________ Base I/O Port
		 *			0000=0x1200 0001=0x1220 ... 1110=0x13c0 1111=0x13e0
		 *
		 * POS register 3: (adf pos1) not used
		 * POS register 4: (adf pos2) not used
		 *
		 * POS register 5: (adf pos3) ignored
		 *
		 * 7 6 5 4 3 2 1 0
		 * 1 1 0 X \____/
		 *              \____EPROM Address
		 */
		iobase = 0x1200 + ((pos2 & 0xf0) << 1);
		irq = tiara_irq[((pos2 & 0x0c) >> 2)];

		/* XXX SWAG for number pkts. */
		/* My Tiara LANCard has 128K memory ?!? */
		sc->txb_num_pkt = 4;
		sc->rxb_num_pkt = (65535 - 8192 - 4) / 64;
		/* XXX                       */

		break;

	case MCA_PRODUCT_SMC3016:
		/*
		 * POS register 2: (adf pos0)
		 * 7 6 5 4 3 2 1 0
		 * \_____/ \___/  \__ enable: 0=disabled, 1=enabled
		 *       \     \_____ I/O Address (see ioaddr table)
		 *        \__________ IRQ (see irq table)
		 *
		 * POS register 3: (adf pos1) ignored
		 * 7 6 5 4 3 2 1 0
		 * X X X X \____/
		 *              \____EPROM Address (0000 = not used)
		 */
		iobase = smc_iobase[((pos2 & 0x0e) >> 1)];
		if ((pos2 & 0x80) != 0)
			irq = smc_irq[((pos2 & 0x70) >> 4)];
		else {
			aprint_error_dev(self, "unsupported irq selected\n");
			return;
		}

		/* XXX SWAG for number pkts. */
		/* The SMC3016 has a 12K rx buffer and a 4k tx buffer */
		sc->txb_num_pkt = 2;
		sc->rxb_num_pkt = (12288 - 4) / 64;
		/* XXX                       */

		break;
	}


#ifdef DIAGNOSTIC
	tra_p = tiara_mca_lookup(ma->ma_id);
	if (tra_p == NULL) {
		aprint_normal("\n");
		aprint_error_dev(self, "where did the card go?\n");
		return;
	}
#endif

	printf(" slot %d ports %#x-%#x irq %d: %s\n", ma->ma_slot + 1,iobase, iobase + TIARA_NPORTS, irq, tra_p->tra_name);

	/* Map i/o space. */
	if (bus_space_map(iot, iobase, TIARA_NPORTS, 0, &ioh)) {
		aprint_error_dev(self, "can't map i/o space\n");
		return;
	}

	sc->sc_dev = self;
	sc->sc_bst = iot;
	sc->sc_bsh = ioh;

	/* Get ethernet address from PROM */
	bus_space_read_region_1(iot, ioh, TIARA_PROM_ID, myea, ETHER_ADDR_LEN);

	/* This interface is always enabled. */
	/* XXX
	sc->sc_stat |= NIC_STAT_ENABLED;
	*/

	/*
	 * Do generic MB86950 attach.
	 */
	mb86950_attach(sc, myea);


	mb86950_config(sc, NULL, 0, 0);

	/* Establish the interrupt handler. */
	isc->sc_ih = mca_intr_establish(ma->ma_mc, irq, IPL_NET,
			mb86950_intr, sc);
	if (isc->sc_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt handler\n");
		return;
	}
}

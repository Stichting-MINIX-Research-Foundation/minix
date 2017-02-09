/* $NetBSD: if_tr_mca.c,v 1.23 2013/11/08 03:12:17 christos Exp $ */

/*_
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gregory McGarry <g.mcgarry@ieee.org>.
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
__KERNEL_RCSID(0, "$NetBSD: if_tr_mca.c,v 1.23 2013/11/08 03:12:17 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <dev/mca/mcareg.h>
#include <dev/mca/mcavar.h>
#include <dev/mca/mcadevs.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <dev/ic/tropicreg.h>
#include <dev/ic/tropicvar.h>

#ifdef DEBUG
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif

#define TR_PRI_IOADDR 0x0a20
#define TR_ALT_IOADDR 0x0a24
#define TR_PIOSIZE 4
#define TR_MMIOSIZE 8192
#define TR_MBPS_4 0
#define TR_MBPS_16 1

int	tr_mca_probe(device_t, cfdata_t, void *);
void	tr_mca_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(tr_mca, sizeof(struct tr_softc),
    tr_mca_probe, tr_mca_attach, NULL, NULL);

/* supported products */
static const struct tr_mca_product {
	u_int32_t tr_id;
	const char *tr_name;
} tr_mca_products[] = {
	{ MCA_PRODUCT_ITR, "IBM Token Ring 16/4 Adapter/A" },
	{ 0, NULL },
};

static const struct tr_mca_product *tr_mca_lookup(int);

static const struct tr_mca_product *
tr_mca_lookup(int id)
{
	const struct tr_mca_product *trp;

	for(trp = tr_mca_products; trp->tr_name; trp++)
		if (trp->tr_id == id)
			return (trp);

	return (NULL);
}

int
tr_mca_probe(device_t parent, cfdata_t match,
    void *aux)
{
	struct mca_attach_args *ma = aux;

	if (tr_mca_lookup(ma->ma_id) != NULL)
		return (1);

	return (0);
}


void
tr_mca_attach(device_t parent, device_t self, void *aux)
{
	struct tr_softc *sc = device_private(self);
	struct mca_attach_args *ma = aux;
	bus_space_handle_t pioh, mmioh, sramh;
	int iobase, irq, sram_size, sram_addr, rom_addr;
	int pos2, pos3, pos4, pos5;
	const struct tr_mca_product *tp;

	pos2 = mca_conf_read(ma->ma_mc, ma->ma_slot, 2);
	pos3 = mca_conf_read(ma->ma_mc, ma->ma_slot, 3);
	pos4 = mca_conf_read(ma->ma_mc, ma->ma_slot, 4);
	pos5 = mca_conf_read(ma->ma_mc, ma->ma_slot, 5);
	__USE(pos5);

	/*
	 * POS register 2: (adf pos0)
	 *
	 * 7 6 5 4 3 2 1 0
	 * \___________/ \__ enable: 0=adapter disabled, 1=adapter enabled
	 *       \__________ RAM addr: pos2&0xfe
	 *
	 * POS register 3: (adf pos1)
	 *
	 * 7 6 5 4 3 2 1 0
	 * | \___/ | | | \__ port address: 1=0x0a24-0x0a27, 0=0x0a20-0x0a23
	 * |   |   \ / \____ speed: 0=4Mbps, 1=16Mbps
	 * |   |    \_______ RAM size: 00=8kb, 01=16kb, 10=32kb, 11=64kb
	 * |   \____________ reserved: 010
	 * \________________ irq component: 0=2, 1=3 (see also pos4)
	 *
	 * POS register 4: (adf pos2)
	 *
	 * 7 6 5 4 3 2 1 0
	 * \___________/ \__ interrupt controller: 0=1st, 1=2nd
	 *       \__________ ROM address: pos4&0xfe
	 *
	 * POS register 5: (adf pos3)
	 *
	 * 7 6 5 4 3 2 1 0
	 * | | \_____/
	 * | |    \_____ reserved: 0x0
	 * | \__________ autosense: 0=off, 1=on
	 * \____________ boot rom: 0=enabled, 1=disabled
	 */

	if (pos3 & 0x01)
		iobase = TR_ALT_IOADDR;
	else
		iobase = TR_PRI_IOADDR;

	irq = 2 + (pos3 >> 7) + ((pos4 & 0x01) << 3);
	if (irq == 2)
		irq = 9;

	tp = tr_mca_lookup(ma->ma_id);

	printf(" slot %d irq %d: %s\n", ma->ma_slot + 1, irq, tp->tr_name);

	sram_size = 8 << (((pos3 & 0x0c) >> 2) + 10);
	sram_addr = (pos2 & 0xfe) << 12;

	rom_addr = (pos4 & 0xfe) << 12;

	/* map the pio registers */
	if (bus_space_map(ma->ma_iot, iobase, TR_PIOSIZE, 0, &pioh)) {
		aprint_error_dev(self, "unable to map PIO space\n");
		return;
	}

	/* map the mmio registers */
	if (bus_space_map(ma->ma_memt, rom_addr, TR_MMIOSIZE, 0, &mmioh)) {
		aprint_error_dev(self, "unable to map MMIO space\n");
		return;
	}

	/* map the sram space */
	if (bus_space_map(ma->ma_memt, sram_addr, sram_size, 0, &sramh)) {
		aprint_error_dev(self, "unable to map SRAM space\n");
		return;
	}

	sc->sc_dev = self;
	sc->sc_piot = ma->ma_iot;
	sc->sc_pioh = pioh;
	sc->sc_memt = ma->ma_memt;
	sc->sc_mmioh = mmioh;
	sc->sc_sramh = sramh;

	/* set ACA offset */
	sc->sc_aca = TR_ACA_OFFSET;
	sc->sc_memwinsz = sram_size;
	sc->sc_maddr = sram_addr;

	/*
	 * Determine total RAM on adapter and decide how much to use.
	 * XXX Since we don't use RAM paging, use sc_memwinsz for now.
	 */
	sc->sc_memsize = sc->sc_memwinsz;
	sc->sc_memreserved = 0;

	if (tr_reset(sc) != 0)
		return;

	/* XXX not implemented (yet) */
	sc->sc_mediastatus = NULL;
	sc->sc_mediachange = NULL;

	/* do generic attach */
	if (tr_attach(sc) != 0)
		return;

	/* establish interrupt handler */
	sc->sc_ih = mca_intr_establish(ma->ma_mc, irq, IPL_NET, tr_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt handler\n");
		return;
	}

}

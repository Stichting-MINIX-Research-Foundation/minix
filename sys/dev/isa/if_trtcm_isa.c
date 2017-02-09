/*	$NetBSD: if_trtcm_isa.c,v 1.19 2009/05/12 09:10:15 cegger Exp $	*/

/* XXXJRT verify doens't change isa_attach_args too early */

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
__KERNEL_RCSID(0, "$NetBSD: if_trtcm_isa.c,v 1.19 2009/05/12 09:10:15 cegger Exp $");

#undef TRTCMISADEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <sys/bus.h>

#include <dev/isa/isavar.h>
#include <dev/isa/elink.h>

#include <dev/ic/tropicreg.h>
#include <dev/ic/tropicvar.h>

#include <dev/ic/elink3reg.h>

u_int16_t	tcmreadeeprom(bus_space_tag_t, bus_space_handle_t, int);
#ifdef TRTCMISADEBUG
void	tcmdumpeeprom(bus_space_tag_t, bus_space_handle_t);
#endif

int	trtcm_isa_probe(device_t, cfdata_t, void *);

int	trtcm_isa_mediachange(struct tr_softc *);
void	trtcm_isa_mediastatus(struct tr_softc *, struct ifmediareq *);

/*
 * TODO:
 *
 * if_media handling in the 3com case
 * mediachange() and mediastatus() function
 * certain newer cards can set their speed on the fly via
 * DIR_SET_DEFAULT_RING_SPEED or set the speed in the eeprom ??
 */

static	void tcmaddcard(int, int, int, int, u_int, int, int);

/*
 * This keeps track of which ISAs have been through a 3com probe sequence.
 * A simple static variable isn't enough, since it's conceivable that
 * a system might have more than one ISA bus.
 *
 * The "tcm_bus" member is the unit number of the parent ISA bus, e.g. "0"
 * for "isa0".
 */
struct tcm_isa_done_probe {
	LIST_ENTRY(tcm_isa_done_probe)	tcm_link;
	int				tcm_bus;
};
static LIST_HEAD(, tcm_isa_done_probe) tcm_isa_all_probes;
static int tcm_isa_probes_initialized;

#define MAXTCMCARDS	20	/* if you have more than 20, you lose */

static struct tcmcard {
	int	bus;
	int	iobase;
	int	irq;
	int	maddr;
	u_int	msize;
	long	model;
	char	available;
	char	pnpmode;
} tcmcards[MAXTCMCARDS];

static int ntcmcards = 0;

static void
tcmaddcard(int bus, int iobase, int irq, int maddr, u_int msiz, int model, int pnpmode)
{

	if (ntcmcards >= MAXTCMCARDS)
		return;
	tcmcards[ntcmcards].bus = bus;
	tcmcards[ntcmcards].iobase = iobase;
	tcmcards[ntcmcards].irq = irq;
	tcmcards[ntcmcards].maddr = maddr;
	tcmcards[ntcmcards].msize = msiz;
	tcmcards[ntcmcards].model = model;
	tcmcards[ntcmcards].available = 1;
	tcmcards[ntcmcards].pnpmode = pnpmode;
	ntcmcards++;
}

/*
 * We get eeprom data from the id_port given an offset into the
 * eeprom.  Basically; after the ID_sequence is sent to all of
 * the cards; they enter the ID_CMD state where they will accept
 * command requests. 0x80-0xbf loads the eeprom data.  We then
 * read the port 16 times and with every read; the cards check
 * for contention (ie: if one card writes a 0 bit and another
 * writes a 1 bit then the host sees a 0. At the end of the cycle;
 * each card compares the data on the bus; if there is a difference
 * then that card goes into ID_WAIT state again). In the meantime;
 * one bit of data is returned in the AX register which is conveniently
 * returned to us by bus_space_read_1().  Hence; we read 16 times getting one
 * bit of data with each read.
 *
 * NOTE: the caller must provide an i/o handle for ELINK_ID_PORT!
 */
u_int16_t
tcmreadeeprom(bus_space_tag_t iot, bus_space_handle_t ioh, int offset)
{
	u_int16_t data = 0;
	int i;

	bus_space_write_1(iot, ioh, 0, 0x80 + offset);
	delay(1000);
	for (i = 0; i < 16; i++)
		data = (data << 1) | (bus_space_read_2(iot, ioh, 0) & 1);
	return (data);
}

#ifdef TRTCMISADEBUG
/*
 * Dump the contents of the EEPROM to the console.
 */
void
tcmdumpeeprom(bus_space_tag_t iot, bus_space_handle_t ioh)
{
	unsigned int off, val;

	printf("EEPROM contents:");
	for (off=0; off < 32; off++) {
		val = tcmreadeeprom(iot, ioh, off);
		if ((off % 8) == 0)
			printf("\n");
		printf("%04x ", val);
	}
	printf("\n");
}
#endif

int
trtcm_isa_mediachange(struct tr_softc *sc)
{
	return EINVAL;
}

void
trtcm_isa_mediastatus(struct tr_softc *sc, struct ifmediareq *ifmr)
{
	struct ifmedia	*ifm = &sc->sc_media;

	ifmr->ifm_active = ifm->ifm_cur->ifm_media;
}

/* XXX hard coded constants in readeeprom elink_idseq */

int
trtcm_isa_probe(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args	*ia = aux;
	int	bus = device_unit(parent);
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	u_int	msiz;
	int slot, iobase, irq, i, maddr, rsrccfg, pnpmode;
	u_int16_t vendor, model;
	struct tcm_isa_done_probe *tcm;
	static int irqs[] = { 7, 15, 6, 11, 3, 10, 9, 5 };

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	if (tcm_isa_probes_initialized == 0) {
		LIST_INIT(&tcm_isa_all_probes);
		tcm_isa_probes_initialized = 1;
	}

	/*
	 * Probe this bus if we haven't done so already.
	 */
	for (tcm = tcm_isa_all_probes.lh_first; tcm != NULL;
	    tcm = tcm->tcm_link.le_next)
		if (tcm->tcm_bus == bus)
			goto bus_probed;

	/*
	 * Mark this bus so we don't probe it again.
	 */
	tcm = (struct tcm_isa_done_probe *)
	    malloc(sizeof(struct tcm_isa_done_probe), M_DEVBUF, M_NOWAIT);
	if (tcm == NULL) {
		printf("trtcm_isa_probe: can't allocate state storage");
		return 0;
	}

	tcm->tcm_bus = bus;
	LIST_INSERT_HEAD(&tcm_isa_all_probes, tcm, tcm_link);

	/*
	 * Map the TokenLink ID port for the probe sequence.
	 */
	if (bus_space_map(iot, ELINK_ID_PORT, 1, 0, &ioh)) {
		printf("trtcm_isa_probe: can't map TokenLink ID port\n");
		return 0;
	}

	for (slot = 0; slot < MAXTCMCARDS; slot++) {
		pnpmode = 0;
		elink_reset(iot, ioh, bus);
		elink_idseq(iot, ioh, TLINK_619_POLY);

		/* Untag all the adapters so they will talk to us. */
		if (slot == 0)
			bus_space_write_1(iot, ioh, 0, TAG_ADAPTER + 0);

		vendor = htons(tcmreadeeprom(iot, ioh, EEPROM_MFG_ID));
		if (vendor != MFG_ID)
			continue;

		model = htons(tcmreadeeprom(iot, ioh, EEPROM_PROD_ID));
		if (((model & 0xfff0) != 0x6190) &&
		    ((model & 0xfff0) != 0x3190)) { /* XXX hardcoded */
#if 0
			printf("trtcm: unknown model 0x%04x\n", model);
#endif
			continue;
		}
#ifdef TRTCMISADEBUG
		tcmdumpeeprom(iot, ioh);

		printf("speed: %d\n", (tcmreadeeprom(iot,ioh,8) & 2) ? 4 : 16);
#endif

		rsrccfg = iobase = tcmreadeeprom(iot, ioh, EEPROM_RESOURCE_CFG);
		if (iobase & 0x20)
			iobase = tcmreadeeprom(iot, ioh, EEPROM_ADDR_CFG) & 1 ?
			    0xa20 : 0xa24;
		else
			iobase = (iobase & 0x1f) * 0x10 + 0x200;

		maddr = ((tcmreadeeprom(iot, ioh, EEPROM_OEM_ADDR0) & 0xfc00)
		    << 3) + 0x80000;
		msiz = 65536 >> ((tcmreadeeprom(iot, ioh, 8) & 0x0c) >> 2);

		irq = tcmreadeeprom(iot, ioh, EEPROM_ADDR_CFG) & 0x180;
		irq |= (tcmreadeeprom(iot, ioh, EEPROM_RESOURCE_CFG) & 0x40);
		irq = irqs[irq >> 6];

		/* Tag card so it will not respond to contention again. */
		bus_space_write_1(iot, ioh, 0, TAG_ADAPTER + 1);

		/*
		 * Don't attach a 3c319 in PnP mode.
		 *
		 * XXX Testing for the 13th bit in iobase being 0 might not
		 * be the right thing to do, but the EEPROM of the 3C319 is
		 * undocumented according to 3COM and this is one of the
		 * three bits that changed when I put the card in PnP mode. -chb
		 */
		if (((model & 0xffff0) == 0x3190) &&
		    ((rsrccfg & 0x1000) == 0)) {
			printf("3COM 3C319 TokenLink Velocity in PnP mode\n");
			pnpmode = 1;
		}
		else {
			/*
			 * XXX: this should probably not be done here
			 * because it enables the drq/irq lines from
			 * the board. Perhaps it should be done after
			 * we have checked for irq/drq collisions?
			 */
			bus_space_write_1(iot, ioh, 0,
			    ACTIVATE_ADAPTER_TO_CONFIG);
		}
		tcmaddcard(bus, iobase, irq, maddr, msiz, model, pnpmode);
	}
	bus_space_unmap(iot, ioh, 1);

bus_probed:

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_niomem < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);

	for (i = 0; i < ntcmcards; i++) {
		if (tcmcards[i].bus != bus)
			continue;
		if (tcmcards[i].available == 0)
			continue;
		if (ia->ia_io[0].ir_addr != ISA_UNKNOWN_PORT &&
		    ia->ia_io[0].ir_addr != tcmcards[i].iobase)
			continue;
		if (ia->ia_irq[0].ir_irq != ISA_UNKNOWN_IRQ &&
		    ia->ia_irq[0].ir_irq != tcmcards[i].irq)
			continue;
		goto good;
	}
	return 0;

good:
	tcmcards[i].available = 0;
	if (tcmcards[i].pnpmode)
		return -1;	/* XXX Don't actually probe this card. */

	ia->ia_nio = 1;
	ia->ia_io[0].ir_addr = tcmcards[i].iobase;
	/* XXX probably right, but ...... */
	if (ia->ia_io[0].ir_addr == 0xa20 || ia->ia_io[0].ir_addr == 0xa24)
		ia->ia_io[0].ir_size = 4;
	else
		ia->ia_io[0].ir_size = 16;

	ia->ia_niomem = 1;
	ia->ia_iomem[0].ir_addr = tcmcards[i].maddr;
	ia->ia_iomem[0].ir_size = tcmcards[i].msize;

	ia->ia_nirq = 1;
	ia->ia_irq[0].ir_irq = tcmcards[i].irq;

	ia->ia_ndrq = 0;

	ia->ia_aux = (void *) tcmcards[i].model;
	return 1;
}

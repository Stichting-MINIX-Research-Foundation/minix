/* $NetBSD: isic_pci_elsa_qs1p.c,v 1.20 2012/10/27 17:18:34 chs Exp $ */

/*
 * Copyright (c) 1997, 1999 Hellmuth Michaelis. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	isic - I4B Siemens ISDN Chipset Driver for ELSA Quickstep 1000pro PCI
 *	=====================================================================
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: isic_pci_elsa_qs1p.c,v 1.20 2012/10/27 17:18:34 chs Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/callout.h>

#include <sys/bus.h>
#include <sys/device.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <netisdn/i4b_debug.h>
#include <netisdn/i4b_ioctl.h>
#include <netisdn/i4b_global.h>
#include <netisdn/i4b_debug.h>
#include <netisdn/i4b_l2.h>
#include <netisdn/i4b_l1l2.h>
#include <netisdn/i4b_mbuf.h>

#include <dev/ic/isic_l1.h>
#include <dev/ic/isac.h>
#include <dev/ic/hscx.h>
#include <dev/ic/ipac.h>
#include <dev/pci/isic_pci.h>

/* masks for register encoded in base addr */

#define ELSA_BASE_MASK		0x0ffff
#define ELSA_OFF_MASK		0xf0000

/* register id's to be encoded in base addr */

#define ELSA_IDISAC		0x00000
#define ELSA_IDHSCXA		0x10000
#define ELSA_IDHSCXB		0x20000
#define ELSA_IDIPAC		0x40000

/* offsets from base address */

#define ELSA_OFF_ALE		0x00
#define ELSA_OFF_RW		0x01

/* LED values */
#define	ELSA_NO_LED		0xff
#define	ELSA_GREEN_LED		0x40
#define	ELSA_YELLOW_LED		0x80

#define ELSA_PORT0_MEM_MAPOFF	PCI_MAPREG_START
#define ELSA_PORT0_IO_MAPOFF	PCI_MAPREG_START+4
#define ELSA_PORT1_MAPOFF	PCI_MAPREG_START+12


static void elsa_cmd_req(struct isic_softc *sc, int cmd, void *data);
static void elsa_led_handler(void *token);

/*---------------------------------------------------------------------------*
 *      ELSA QuickStep 1000pro/PCI ISAC get fifo routine
 *---------------------------------------------------------------------------*/

static void
eqs1pp_read_fifo(struct isic_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[1].t;
	bus_space_handle_t h = sc->sc_maps[1].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_ISAC_OFF);
			bus_space_read_multi_1(t, h, ELSA_OFF_RW, buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_HSCXA_OFF);
			bus_space_read_multi_1(t, h, ELSA_OFF_RW, buf, size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_HSCXB_OFF);
			bus_space_read_multi_1(t, h, ELSA_OFF_RW, buf, size);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *      ELSA QuickStep 1000pro/PCI ISAC put fifo routine
 *---------------------------------------------------------------------------*/

static void
eqs1pp_write_fifo(struct isic_softc *sc, int what, const void *buf, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[1].t;
	bus_space_handle_t h = sc->sc_maps[1].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_ISAC_OFF);
			bus_space_write_multi_1(t, h, ELSA_OFF_RW, buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_HSCXA_OFF);
			bus_space_write_multi_1(t, h, ELSA_OFF_RW, buf, size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_HSCXB_OFF);
			bus_space_write_multi_1(t, h, ELSA_OFF_RW, buf, size);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *      ELSA QuickStep 1000pro/PCI ISAC put register routine
 *---------------------------------------------------------------------------*/

static void
eqs1pp_write_reg(struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	bus_space_tag_t t = sc->sc_maps[1].t;
	bus_space_handle_t h = sc->sc_maps[1].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_ISAC_OFF+offs);
			bus_space_write_1(t, h, ELSA_OFF_RW, data);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_HSCXA_OFF+offs);
			bus_space_write_1(t, h, ELSA_OFF_RW, data);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_HSCXB_OFF+offs);
			bus_space_write_1(t, h, ELSA_OFF_RW, data);
			break;
		case ISIC_WHAT_IPAC:
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_IPAC_OFF+offs);
			bus_space_write_1(t, h, ELSA_OFF_RW, data);
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	ELSA QuickStep 1000pro/PCI ISAC get register routine
 *---------------------------------------------------------------------------*/

static u_int8_t
eqs1pp_read_reg(struct isic_softc *sc, int what, bus_size_t offs)
{
	bus_space_tag_t t = sc->sc_maps[1].t;
	bus_space_handle_t h = sc->sc_maps[1].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_ISAC_OFF+offs);
			return bus_space_read_1(t, h, ELSA_OFF_RW);
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_HSCXA_OFF+offs);
			return bus_space_read_1(t, h, ELSA_OFF_RW);
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_HSCXB_OFF+offs);
			return bus_space_read_1(t, h, ELSA_OFF_RW);
		case ISIC_WHAT_IPAC:
		{
			bus_space_write_1(t, h, ELSA_OFF_ALE, IPAC_IPAC_OFF+offs);
			return bus_space_read_1(t, h, ELSA_OFF_RW);
		}
	}

	return 0;
}

/*---------------------------------------------------------------------------*
 *	isic_attach_Eqs1pp - attach for ELSA QuickStep 1000pro/PCI
 *---------------------------------------------------------------------------*/

int
isic_attach_Eqs1pp(struct pci_isic_softc *psc, struct pci_attach_args *pa)
{
	struct isic_softc *sc = &psc->sc_isic;

	/* setup io mappings */
	sc->sc_num_mappings = 2;
	MALLOC_MAPS(sc);
	sc->sc_maps[0].size = 0;
	if (pci_mapreg_map(pa, ELSA_PORT0_MEM_MAPOFF, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->sc_maps[0].t, &sc->sc_maps[0].h, &psc->sc_base, &psc->sc_size) != 0
	   && pci_mapreg_map(pa, ELSA_PORT0_IO_MAPOFF, PCI_MAPREG_TYPE_IO, 0,
	    &sc->sc_maps[0].t, &sc->sc_maps[0].h, &psc->sc_base, &psc->sc_size) != 0) {
		aprint_error_dev(sc->sc_dev, "can't map card registers\n");
		return (0);
	}

	/* PLX9050 Errata #1 */
	if (PCI_REVISION(pa->pa_class) == 1 && psc->sc_base & 0x00000080) {
#ifdef DEBUG
		printf("%s: no LCR access\n", device_xname(sc->sc_dev));
#endif
	} else
		psc->flags |= PCIISIC_LCROK;

	sc->sc_maps[1].size = 0;
	if (pci_mapreg_map(pa, ELSA_PORT1_MAPOFF, PCI_MAPREG_TYPE_IO, 0,
	    &sc->sc_maps[1].t, &sc->sc_maps[1].h, NULL, NULL)) {
		aprint_error_dev(sc->sc_dev, "can't map i/o space\n");
		return (0);
	}

	/* setup access routines */

	sc->clearirq = NULL;
	sc->readreg = eqs1pp_read_reg;
	sc->writereg = eqs1pp_write_reg;

	sc->readfifo = eqs1pp_read_fifo;
	sc->writefifo = eqs1pp_write_fifo;

	sc->drv_command = elsa_cmd_req;

	/* setup card type */

	sc->sc_cardtyp = CARD_TYPEP_ELSAQS1PCI;

	/* setup IOM bus type */

	sc->sc_bustyp = BUS_TYPE_IOM2;

	/* setup chip type = IPAC ! */

	sc->sc_ipac = 1;
	sc->sc_bfifolen = IPAC_BFIFO_LEN;

	IPAC_WRITE(IPAC_ACFG, 0);	/* outputs are open drain */
	IPAC_WRITE(IPAC_AOE,		/* aux 5..2 are inputs, 7, 6 outputs */
		(IPAC_AOE_OE5 | IPAC_AOE_OE4 | IPAC_AOE_OE3 | IPAC_AOE_OE2));
	IPAC_WRITE(IPAC_ATX, ELSA_NO_LED);	/* set all output lines high */
	callout_init(&((struct pci_isic_softc *)sc)->ledcallout, 0);

	/* disable any interrupts */
	IPAC_WRITE(IPAC_MASK, 0xff);
        bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h, 0x4c, 0x01);

	return (1);
}

int
isic_intr_qs1p(void *vsc)
{
	struct pci_isic_softc *psc = vsc;
	struct isic_softc *sc = &psc->sc_isic;
	u_int32_t intcsr;

	/*
	 * if we are not hit by the PLX bug we can try a shortcut
	 * (should improve speed for shared IRQs)
	 */
	if (psc->flags & PCIISIC_LCROK) {
		intcsr = bus_space_read_4(sc->sc_maps[0].t, sc->sc_maps[0].h,
					  0x4c /* INTCSR */);
		if (!(intcsr & 0x4 /* LINTi1STAT */))
			return (0);
	}

	return (isicintr(sc));
}

static void
elsa_cmd_req(struct isic_softc *sc, int cmd, void *data)
{
	intptr_t v;
	int s;
	struct pci_isic_softc *psc = (struct pci_isic_softc *)sc;

	switch (cmd) {
	case CMR_DOPEN:
		s = splnet();
		/* enable hscx/isac irq's */
		IPAC_WRITE(IPAC_MASK, (IPAC_MASK_INT1 | IPAC_MASK_INT0));
		/* enable card interrupt */
	        bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h, 0x4c, 0x41);
		splx(s);
		break;
	case CMR_DCLOSE:
		s = splnet();
		callout_stop(&psc->ledcallout);
		IPAC_WRITE(IPAC_ATX, ELSA_NO_LED);
		IPAC_WRITE(IPAC_MASK, 0xff);
	        bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h, 0x4c, 0x01);
		splx(s);
		break;
	case CMR_SETLEDS:
		v = (intptr_t)data;
		callout_stop(&psc->ledcallout);

		/* the magic value and keep reset off */
		psc->ledstat = ELSA_NO_LED;
		psc->ledblinkmask = 0;
		psc->ledblinkfreq = 0;

		/* now see what LEDs we want to add */
		if (v & CMRLEDS_TEI)
			psc->ledstat &= ~ELSA_GREEN_LED;

		if (v & (CMRLEDS_B0|CMRLEDS_B1)) {
			psc->ledstat &= ~ELSA_YELLOW_LED;
			psc->ledblinkmask |= ELSA_YELLOW_LED;
			if ((v & (CMRLEDS_B0|CMRLEDS_B1))
			    == (CMRLEDS_B0|CMRLEDS_B1))
				psc->ledblinkfreq = hz/4;
			else
				psc->ledblinkfreq = hz;
		}

		elsa_led_handler(psc);
		break;
	}
}

static void
elsa_led_handler(void *token)
{
	struct pci_isic_softc *psc = token;
	struct isic_softc *sc = token; /* XXX */
	int s;

	s = splnet();
	IPAC_WRITE(IPAC_ATX, psc->ledstat);
	splx(s);
	if (psc->ledblinkfreq) {
		psc->ledstat ^= psc->ledblinkmask;
		callout_reset(&psc->ledcallout, psc->ledblinkfreq,
		    elsa_led_handler, psc);
	}
}

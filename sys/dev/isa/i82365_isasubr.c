/*	$NetBSD: i82365_isasubr.c,v 1.48 2012/10/27 17:18:24 chs Exp $	*/

/*
 * Copyright (c) 2000 Christian E. Hopps.  All rights reserved.
 * Copyright (c) 1998 Bill Sommerfeld.  All rights reserved.
 * Copyright (c) 1997 Marc Horowitz.  All rights reserved.
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
 *	This product includes software developed by Marc Horowitz.
 * 4. The name of the author may not be used to endorse or promote products
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: i82365_isasubr.c,v 1.48 2012/10/27 17:18:24 chs Exp $");

#define	PCICISADEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/extent.h>
#include <sys/malloc.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciachip.h>

#include <dev/ic/i82365reg.h>
#include <dev/ic/i82365var.h>
#include <dev/isa/i82365_isavar.h>

/*****************************************************************************
 * Configurable parameters.
 *****************************************************************************/

#include "opt_pcic_isa_alloc_iobase.h"
#include "opt_pcic_isa_alloc_iosize.h"
#include "opt_pcic_isa_intr_alloc_mask.h"

/*
 * Default I/O allocation range.  If both are set to non-zero, these
 * values will be used instead.  Otherwise, the code attempts to probe
 * the bus width.  Systems with 10 address bits should use 0x300 and 0xff.
 * Systems with 12 address bits (most) should use 0x400 and 0xbff.
 */

#ifndef PCIC_ISA_ALLOC_IOBASE
#define	PCIC_ISA_ALLOC_IOBASE		0
#endif

#ifndef PCIC_ISA_ALLOC_IOSIZE
#define	PCIC_ISA_ALLOC_IOSIZE		0
#endif

int	pcic_isa_alloc_iobase = PCIC_ISA_ALLOC_IOBASE;
int	pcic_isa_alloc_iosize = PCIC_ISA_ALLOC_IOSIZE;


/*
 * Default IRQ allocation bitmask.  This defines the range of allowable
 * IRQs for PCMCIA slots.  Useful if order of probing would screw up other
 * devices, or if PCIC hardware/cards have trouble with certain interrupt
 * lines.
 */

#ifndef PCIC_ISA_INTR_ALLOC_MASK
#define	PCIC_ISA_INTR_ALLOC_MASK	0xffff
#endif

int	pcic_isa_intr_alloc_mask = PCIC_ISA_INTR_ALLOC_MASK;

#ifndef	PCIC_IRQ_PROBE
#ifdef hpcmips
/*
 * The irq probing doesn't work with current vrisab implementation.
 * The irq is just an key to find matching GPIO port to use and is fixed.
 */
#define	PCIC_IRQ_PROBE	0
#else
#define	PCIC_IRQ_PROBE	1
#endif
#endif

int	pcic_irq_probe = PCIC_IRQ_PROBE;

/*****************************************************************************
 * End of configurable parameters.
 *****************************************************************************/

#ifdef PCICISADEBUG
int	pcicsubr_debug = 0;
#define	DPRINTF(arg) do { if (pcicsubr_debug) printf arg ; } while (0)
#else
#define	DPRINTF(arg)
#endif

/*
 * count the interrupt if we have a status set
 * just use socket 0
 */

void pcic_isa_probe_interrupts(struct pcic_isa_softc *, struct pcic_handle *);
static int pcic_isa_count_intr(void *);

static int
pcic_isa_count_intr(void *arg)
{
	struct pcic_softc *sc;
	struct pcic_isa_softc *isc;
	struct pcic_handle *h;
	int cscreg;

	h = arg;
	isc = device_private(h->ph_parent);
	sc = &isc->sc_pcic;

	cscreg = pcic_read(h, PCIC_CSC);
	if (cscreg & PCIC_CSC_CD) {
		if ((++sc->intr_detect % 20) == 0)
			printf(".");
		else
			DPRINTF(("."));
		return 1;
	}

	/*
	 * make sure we don't get stuck in a loop due to
	 * unhandled level interrupts
	 */
	if (++sc->intr_false > 40) {
		pcic_write(h, PCIC_CSC_INTR, 0);
		delay(10);
	}

#ifdef PCICISADEBUG
	if (cscreg)
		DPRINTF(("o"));
	else
		DPRINTF(("X"));
#endif
	return cscreg ? 1 : 0;
}

/*
 * use soft interrupt card detect to find out which irqs are available
 * for this controller
 */
void
pcic_isa_probe_interrupts(struct pcic_isa_softc *isc, struct pcic_handle *h)
{
	struct pcic_softc *sc = &isc->sc_pcic;
	isa_chipset_tag_t ic;
	int i, j, mask, irq;
	int cd, cscintr, intr, csc;

	ic = isc->sc_ic;

	printf("%s: controller %d detecting irqs with mask 0x%04x:",
	    device_xname(sc->dev), h->chip, sc->intr_mask[h->chip]);
	DPRINTF(("\n"));

	/* clear any current interrupt */
	pcic_read(h, PCIC_CSC);

	/* first disable the status irq, card detect is enabled later */
	pcic_write(h, PCIC_CSC_INTR, 0);

	/* steer the interrupt to isa and disable ring and interrupt */
	intr = pcic_read(h, PCIC_INTR);
	DPRINTF(("pcic: old intr 0x%x\n", intr));
	intr &= ~(PCIC_INTR_RI_ENABLE | PCIC_INTR_ENABLE | PCIC_INTR_IRQ_MASK);
	pcic_write(h, PCIC_INTR, intr);


	/* clear any current interrupt */
	pcic_read(h, PCIC_CSC);

	cd = pcic_read(h, PCIC_CARD_DETECT);
	cd |= PCIC_CARD_DETECT_SW_INTR;

	mask = 0;
	for (i = 0; i < 16; i++) {
		/* honor configured limitations */
		if ((sc->intr_mask[h->chip] & (1 << i)) == 0)
			continue;

		DPRINTF(("probing irq %d: ", i));

		/* ask for a pulse interrupt so we don't share */
		if (isa_intr_alloc(ic, (1 << i), IST_PULSE, &irq)) {
			DPRINTF(("currently allocated\n"));
			continue;
		}

		cscintr = PCIC_CSC_INTR_CD_ENABLE;
		cscintr |= (irq << PCIC_CSC_INTR_IRQ_SHIFT);
		pcic_write(h, PCIC_CSC_INTR, cscintr);
		delay(10);

		/* Clear any pending interrupt. */
		(void) pcic_read(h, PCIC_CSC);

		if ((sc->ih = isa_intr_establish(ic, irq, IST_EDGE, IPL_TTY,
		    pcic_isa_count_intr, h)) == NULL)
			panic("cant get interrupt");

		/* interrupt 40 times */
		sc->intr_detect = 0;
		for (j = 0; j < 40 && sc->ih; j++) {
			sc->intr_false = 0;
			pcic_write(h, PCIC_CARD_DETECT, cd);
			delay(100);
			csc = pcic_read(h, PCIC_CSC);
			DPRINTF(("%s", csc ? "-" : ""));
		}
		DPRINTF((" total %d\n", sc->intr_detect));
		/* allow for misses */
		if (sc->intr_detect > 37 && sc->intr_detect <= 40) {
			printf("%d", i);
			DPRINTF((" succeded\n"));
			mask |= (1 << i);
		}

		if (sc->ih != NULL) {
			isa_intr_disestablish(ic, sc->ih);
			sc->ih = NULL;

			pcic_write(h, PCIC_CSC_INTR, 0);
			delay(10);
		}
	}
	sc->intr_mask[h->chip] = mask;

	printf("%s\n", sc->intr_mask[h->chip] ? "" : " none");
}

/*
 * called with interrupts enabled, light up the irqs to find out
 * which irq lines are actually hooked up to our pcic
 */
void
pcic_isa_config_interrupts(device_t self)
{
	struct pcic_softc *sc;
	struct pcic_isa_softc *isc;
	struct pcic_handle *h;
	isa_chipset_tag_t ic;
	int s, i, chipmask, chipuniq;

	isc = device_private(self);
	sc = &isc->sc_pcic;
	ic = isc->sc_ic;

	/* probe each controller */
	chipmask = 0xffff;
	for (i = 0; i < PCIC_NSLOTS; i += 2) {
		if (sc->handle[i].flags & PCIC_FLAG_SOCKETP)
			h = &sc->handle[i];
		else if (sc->handle[i + 1].flags & PCIC_FLAG_SOCKETP)
			h = &sc->handle[i + 1];
		else
			continue;

		sc->intr_mask[h->chip] =
		    PCIC_INTR_IRQ_VALIDMASK & pcic_isa_intr_alloc_mask;

		/* the cirrus chips lack support for the soft interrupt */
		if (pcic_irq_probe != 0 &&
		    h->vendor != PCIC_VENDOR_CIRRUS_PD67XX)
			pcic_isa_probe_interrupts(isc, h);

		chipmask &= sc->intr_mask[h->chip];
	}
	/* now see if there is at least one irq per chip not shared by all */
	chipuniq = 1;
	for (i = 0; i < PCIC_NSLOTS; i += 2) {
		if ((sc->handle[i].flags & PCIC_FLAG_SOCKETP) == 0 &&
		    (sc->handle[i + 1].flags & PCIC_FLAG_SOCKETP) == 0)
			continue;
		if ((sc->intr_mask[i / 2] & ~chipmask) == 0) {
			chipuniq = 0;
			break;
		}
	}
	/*
	 * the rest of the following code used to run at config time with
	 * no interrupts and gets unhappy if this is violated so...
	 */
	s = splhigh();

	/*
	 * allocate our irq.  it will be used by both controllers.  I could
	 * use two different interrupts, but interrupts are relatively
	 * scarce, shareable, and for PCIC controllers, very infrequent.
	 */
	if ((device_cfdata(self)->cf_flags & 1) == 0) {
		if (sc->irq != ISA_UNKNOWN_IRQ) {
			if ((chipmask & (1 << sc->irq)) == 0)
				printf("%s: warning: configured irq %d not "
				    "detected as available\n",
				    device_xname(self), sc->irq);
		} else if (chipmask == 0 ||
		    isa_intr_alloc(ic, chipmask, IST_EDGE, &sc->irq)) {
			aprint_error_dev(self, "no available irq; ");
			sc->irq = ISA_UNKNOWN_IRQ;
		} else if ((chipmask & ~(1 << sc->irq)) == 0 && chipuniq == 0) {
			aprint_error_dev(self, "can't share irq with cards; ");
			sc->irq = ISA_UNKNOWN_IRQ;
		}
	} else {
		printf("%s: ", device_xname(self));
		sc->irq = ISA_UNKNOWN_IRQ;
	}

	if (sc->irq != ISA_UNKNOWN_IRQ) {
		sc->ih = isa_intr_establish(ic, sc->irq, IST_EDGE, IPL_TTY,
		    pcic_intr, sc);
		if (sc->ih == NULL) {
			aprint_error_dev(self, "can't establish interrupt");
			sc->irq = ISA_UNKNOWN_IRQ;
		}
	}
	if (sc->irq == ISA_UNKNOWN_IRQ)
		printf("polling for socket events\n");
	else
		printf("%s: using irq %d for socket events\n",
		    device_xname(self), sc->irq);

	pcic_attach_sockets_finish(sc);

	splx(s);
}

/*
 * XXX This routine does not deal with the aliasing issue that its
 * trying to.
 *
 * Any isa device may be decoding only 10 bits of address including
 * the pcic.  This routine only detects if the pcic is doing 10 bits.
 *
 * What should be done is detect the pcic's idea of the bus width,
 * and then within those limits allocate a sparse map, where the
 * each sub region is offset by 0x400.
 */
void pcic_isa_bus_width_probe(struct pcic_softc *sc, bus_space_tag_t iot,
    bus_space_handle_t ioh, bus_addr_t base, uint32_t length)
{
	bus_space_handle_t ioh_high;
	int i, iobuswidth, tmp1, tmp2;

	/*
	 * figure out how wide the isa bus is.  Do this by checking if the
	 * pcic controller is mirrored 0x400 above where we expect it to be.
	 */

	iobuswidth = 12;

	/* Map i/o space. */
	if (bus_space_map(iot, base + 0x400, length, 0, &ioh_high)) {
		aprint_error_dev(sc->dev, "can't map high i/o space\n");
		return;
	}

	for (i = 0; i < PCIC_NSLOTS; i++) {
		if (sc->handle[i].flags & PCIC_FLAG_SOCKETP) {
			/*
			 * read the ident flags from the normal space and
			 * from the mirror, and compare them
			 */

			bus_space_write_1(iot, ioh, PCIC_REG_INDEX,
			    sc->handle[i].sock + PCIC_IDENT);
			tmp1 = bus_space_read_1(iot, ioh, PCIC_REG_DATA);

			bus_space_write_1(iot, ioh_high, PCIC_REG_INDEX,
			    sc->handle[i].sock + PCIC_IDENT);
			tmp2 = bus_space_read_1(iot, ioh_high, PCIC_REG_DATA);

			if (tmp1 == tmp2)
				iobuswidth = 10;
		}
	}

	bus_space_free(iot, ioh_high, length);

	/*
	 * XXX some hardware doesn't seem to grok addresses in 0x400 range--
	 * apparently missing a bit or more of address lines. (e.g.
	 * CIRRUS_PD672X with Linksys EthernetCard ne2000 clone in TI
	 * TravelMate 5000--not clear which is at fault)
	 *
	 * Add a kludge to detect 10 bit wide buses and deal with them,
	 * and also a config file option to override the probe.
	 */

	if (iobuswidth == 10) {
		sc->iobase = 0x300;
		sc->iosize = 0x0ff;
	} else {
		sc->iobase = 0x400;
		sc->iosize = 0xbff;
	}

	DPRINTF(("%s: bus_space_alloc range 0x%04lx-0x%04lx (probed)\n",
	    device_xname(sc->dev), (long) sc->iobase,
	    (long)(sc->iobase + sc->iosize)));

	if (pcic_isa_alloc_iobase && pcic_isa_alloc_iosize) {
		sc->iobase = pcic_isa_alloc_iobase;
		sc->iosize = pcic_isa_alloc_iosize;

		DPRINTF(("%s: bus_space_alloc range 0x%04lx-0x%04lx "
		    "(config override)\n", device_xname(sc->dev),
		    (long) sc->iobase, (long)(sc->iobase + sc->iosize)));
	}
}

void *
pcic_isa_chip_intr_establish(pcmcia_chipset_handle_t pch,
    struct pcmcia_function *pf, int ipl, int (*fct)(void *), void *arg)
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
	struct pcic_isa_softc *isc = device_private(h->ph_parent);
	struct pcic_softc *sc = &isc->sc_pcic;
	isa_chipset_tag_t ic = isc->sc_ic;
	int irq, ist;
	void *ih;
	int reg;

	/*
	 * PLEASE NOTE:
	 * The IRQLEVEL bit has no bearing on what happens on the host side of
	 * the PCMCIA controller.  ISA interrupts are defined to be edge-
	 * triggered, and as this attachment is for ISA devices, the interrupt
	 * *must* be configured for edge-trigger.  If you think you should
	 * change this to use IST_LEVEL, you are *wrong*.  You should figure
	 * out what your real problem is and leave this code alone rather than
	 * breaking everyone else's systems.  - mycroft
	 */
	if (pf->cfe->flags & PCMCIA_CFE_IRQLEVEL)
		ist = IST_EDGE;		/* SEE COMMENT ABOVE */
	else if (pf->cfe->flags & PCMCIA_CFE_IRQPULSE)
		ist = IST_PULSE;	/* SEE COMMENT ABOVE */
	else
		ist = IST_EDGE;		/* SEE COMMENT ABOVE */

	if (isa_intr_alloc(ic, sc->intr_mask[h->chip], ist, &irq))
		return NULL;

	h->ih_irq = irq;
	if (h->flags & PCIC_FLAG_ENABLED) {
		reg = pcic_read(h, PCIC_INTR);
		reg &= ~PCIC_INTR_IRQ_MASK;
		pcic_write(h, PCIC_INTR, reg | irq);
	}

	if ((ih = isa_intr_establish(ic, irq, ist, ipl, fct, arg)) == NULL)
		return NULL;

	printf("%s: card irq %d\n", device_xname(h->pcmcia), irq);

	return ih;
}

void
pcic_isa_chip_intr_disestablish(pcmcia_chipset_handle_t pch, void *ih)
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
	struct pcic_isa_softc *isc = device_private(h->ph_parent);
	isa_chipset_tag_t ic = isc->sc_ic;
	int reg;

	isa_intr_disestablish(ic, ih);

	h->ih_irq = 0;
	if (h->flags & PCIC_FLAG_ENABLED) {
		reg = pcic_read(h, PCIC_INTR);
		reg &= ~PCIC_INTR_IRQ_MASK;
		pcic_write(h, PCIC_INTR, reg);
	}
}

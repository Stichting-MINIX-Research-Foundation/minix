/*	$NetBSD: i82365.c,v 1.116 2013/10/13 06:55:34 riz Exp $	*/

/*
 * Copyright (c) 2004 Charles M. Hannum.  All rights reserved.
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
 *      This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 */

/*
 * Copyright (c) 2000 Christian E. Hopps.  All rights reserved.
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
__KERNEL_RCSID(0, "$NetBSD: i82365.c,v 1.116 2013/10/13 06:55:34 riz Exp $");

#define	PCICDEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/extent.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/kthread.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>

#include <dev/ic/i82365reg.h>
#include <dev/ic/i82365var.h>

#include "locators.h"

#ifdef PCICDEBUG
int	pcic_debug = 0;
#define	DPRINTF(arg) if (pcic_debug) printf arg;
#else
#define	DPRINTF(arg)
#endif

/*
 * Individual drivers will allocate their own memory and io regions. Memory
 * regions must be a multiple of 4k, aligned on a 4k boundary.
 */

#define	PCIC_MEM_ALIGN	PCIC_MEM_PAGESIZE

void	pcic_attach_socket(struct pcic_handle *);
void	pcic_attach_socket_finish(struct pcic_handle *);

int	pcic_print (void *arg, const char *pnp);
int	pcic_intr_socket(struct pcic_handle *);
void	pcic_poll_intr(void *);

void	pcic_attach_card(struct pcic_handle *);
void	pcic_detach_card(struct pcic_handle *, int);
void	pcic_deactivate_card(struct pcic_handle *);

void	pcic_chip_do_mem_map(struct pcic_handle *, int);
void	pcic_chip_do_io_map(struct pcic_handle *, int);

void	pcic_event_thread(void *);

void	pcic_queue_event(struct pcic_handle *, int);
void	pcic_power(int, void *);

static int	pcic_wait_ready(struct pcic_handle *);
static void	pcic_delay(struct pcic_handle *, int, const char *);

static uint8_t st_pcic_read(struct pcic_handle *, int);
static void st_pcic_write(struct pcic_handle *, int, uint8_t);

int
pcic_ident_ok(int ident)
{

	/* this is very empirical and heuristic */

	if ((ident == 0) || (ident == 0xff) || (ident & PCIC_IDENT_ZERO))
		return 0;

	if ((ident & PCIC_IDENT_REV_MASK) == 0)
		return 0;

	if ((ident & PCIC_IDENT_IFTYPE_MASK) != PCIC_IDENT_IFTYPE_MEM_AND_IO) {
#ifdef DIAGNOSTIC
		printf("pcic: does not support memory and I/O cards, "
		    "ignored (ident=%0x)\n", ident);
#endif
		return 0;
	}

	return 1;
}

int
pcic_vendor(struct pcic_handle *h)
{
	int reg;
	int vendor;

	reg = pcic_read(h, PCIC_IDENT);

	if ((reg & PCIC_IDENT_REV_MASK) == 0)
		return PCIC_VENDOR_NONE;

	switch (reg) {
	case 0x00:
	case 0xff:
		return PCIC_VENDOR_NONE;
	case PCIC_IDENT_ID_INTEL0:
		vendor = PCIC_VENDOR_I82365SLR0;
		break;
	case PCIC_IDENT_ID_INTEL1:
		vendor = PCIC_VENDOR_I82365SLR1;
		break;
	case PCIC_IDENT_ID_INTEL2:
		vendor = PCIC_VENDOR_I82365SL_DF;
		break;
	case PCIC_IDENT_ID_IBM1:
	case PCIC_IDENT_ID_IBM2:
		vendor = PCIC_VENDOR_IBM;
		break;
	case PCIC_IDENT_ID_IBM3:
		vendor = PCIC_VENDOR_IBM_KING;
		break;
	default:
		vendor = PCIC_VENDOR_UNKNOWN;
		break;
	}

	if (vendor == PCIC_VENDOR_I82365SLR0 ||
	    vendor == PCIC_VENDOR_I82365SLR1) {
		/*
		 * Check for Cirrus PD67xx.
		 * the chip_id of the cirrus toggles between 11 and 00 after a
		 * write.  weird.
		 */
		pcic_write(h, PCIC_CIRRUS_CHIP_INFO, 0);
		reg = pcic_read(h, -1);
		if ((reg & PCIC_CIRRUS_CHIP_INFO_CHIP_ID) ==
		    PCIC_CIRRUS_CHIP_INFO_CHIP_ID) {
			reg = pcic_read(h, -1);
			if ((reg & PCIC_CIRRUS_CHIP_INFO_CHIP_ID) == 0)
				return PCIC_VENDOR_CIRRUS_PD67XX;
		}

		/*
		 * check for Ricoh RF5C[23]96
		 */
		reg = pcic_read(h, PCIC_RICOH_REG_CHIP_ID);
		switch (reg) {
		case PCIC_RICOH_CHIP_ID_5C296:
			return PCIC_VENDOR_RICOH_5C296;
		case PCIC_RICOH_CHIP_ID_5C396:
			return PCIC_VENDOR_RICOH_5C396;
		}
	}

	return vendor;
}

const char *
pcic_vendor_to_string(int vendor)
{

	switch (vendor) {
	case PCIC_VENDOR_I82365SLR0:
		return "Intel 82365SL Revision 0";
	case PCIC_VENDOR_I82365SLR1:
		return "Intel 82365SL Revision 1";
	case PCIC_VENDOR_CIRRUS_PD67XX:
		return "Cirrus PD6710/2X";
	case PCIC_VENDOR_I82365SL_DF:
		return "Intel 82365SL-DF";
	case PCIC_VENDOR_RICOH_5C296:
		return "Ricoh RF5C296";
	case PCIC_VENDOR_RICOH_5C396:
		return "Ricoh RF5C396";
	case PCIC_VENDOR_IBM:
		return "IBM PCIC";
	case PCIC_VENDOR_IBM_KING:
		return "IBM KING";
	}

	return "Unknown controller";
}

void
pcic_attach(struct pcic_softc *sc)
{
	int i, reg, chip, socket;
	struct pcic_handle *h;
	device_t self;

	DPRINTF(("pcic ident regs:"));

	self = sc->dev;
	mutex_init(&sc->sc_pcic_lock, MUTEX_DEFAULT, IPL_NONE);

	/* find and configure for the available sockets */
	for (i = 0; i < __arraycount(sc->handle); i++) {
		h = &sc->handle[i];
		chip = i / 2;
		socket = i % 2;

		h->ph_parent = self;
		h->chip = chip;
		h->socket = socket;
		h->sock = chip * PCIC_CHIP_OFFSET + socket * PCIC_SOCKET_OFFSET;
		h->laststate = PCIC_LASTSTATE_EMPTY;
		/* initialize pcic_read and pcic_write functions */
		h->ph_read = st_pcic_read;
		h->ph_write = st_pcic_write;
		h->ph_bus_t = sc->iot;
		h->ph_bus_h = sc->ioh;
		h->flags = 0;

		/* need to read vendor -- for cirrus to report no xtra chip */
		if (socket == 0) {
			h->vendor = pcic_vendor(h);
			if (i < __arraycount(sc->handle) - 1)
				(h + 1)->vendor = h->vendor;
		}

		switch (h->vendor) {
		case PCIC_VENDOR_NONE:
			/* no chip */
			continue;
		case PCIC_VENDOR_CIRRUS_PD67XX:
			reg = pcic_read(h, PCIC_CIRRUS_CHIP_INFO);
			if (socket == 0 ||
			    (reg & PCIC_CIRRUS_CHIP_INFO_SLOTS))
				h->flags = PCIC_FLAG_SOCKETP;
			break;
		default:
			/*
			 * During the socket probe, read the ident register
			 * twice.  I don't understand why, but sometimes the
			 * clone chips in hpcmips boxes read all-0s the first
			 * time. -- mycroft
			 */
			reg = pcic_read(h, PCIC_IDENT);
			DPRINTF(("socket %d ident reg 0x%02x\n", i, reg));
			reg = pcic_read(h, PCIC_IDENT);
			DPRINTF(("socket %d ident reg 0x%02x\n", i, reg));
			if (pcic_ident_ok(reg))
				h->flags = PCIC_FLAG_SOCKETP;
			break;
		}
	}

	for (i = 0; i < __arraycount(sc->handle); i++) {
		h = &sc->handle[i];

		if (h->flags & PCIC_FLAG_SOCKETP) {
			SIMPLEQ_INIT(&h->events);

			/* disable interrupts and leave socket in reset */
			pcic_write(h, PCIC_INTR, 0);

			/* zero out the address windows */
			pcic_write(h, PCIC_ADDRWIN_ENABLE, 0);

			/* power down the socket */
			pcic_write(h, PCIC_PWRCTL, 0);

			pcic_write(h, PCIC_CSC_INTR, 0);
			(void) pcic_read(h, PCIC_CSC);
		}
	}

	/* print detected info */
	for (i = 0; i < __arraycount(sc->handle) - 1; i += 2) {
		h = &sc->handle[i];
		chip = i / 2;

		if (h->vendor == PCIC_VENDOR_NONE)
			continue;

		aprint_normal_dev(self, "controller %d (%s) has ",
		    chip, pcic_vendor_to_string(sc->handle[i].vendor));

		if ((h->flags & PCIC_FLAG_SOCKETP) &&
		    ((h + 1)->flags & PCIC_FLAG_SOCKETP))
			aprint_normal("sockets A and B\n");
		else if (h->flags & PCIC_FLAG_SOCKETP)
			aprint_normal("socket A only\n");
		else if ((h + 1)->flags & PCIC_FLAG_SOCKETP)
			aprint_normal("socket B only\n");
		else
			aprint_normal("no sockets\n");
	}
}

/*
 * attach the sockets before we know what interrupts we have
 */
void
pcic_attach_sockets(struct pcic_softc *sc)
{
	int i;

	for (i = 0; i < __arraycount(sc->handle); i++)
		if (sc->handle[i].flags & PCIC_FLAG_SOCKETP)
			pcic_attach_socket(&sc->handle[i]);
}

void
pcic_power(int why, void *arg)
{
	struct pcic_handle *h = arg;
	struct pcic_softc *sc = device_private(h->ph_parent);
	int reg;

	DPRINTF(("%s: power: why %d\n", device_xname(h->ph_parent), why));

	if (h->flags & PCIC_FLAG_SOCKETP) {
		if ((why == PWR_RESUME) &&
		    (pcic_read(h, PCIC_CSC_INTR) == 0)) {
#ifdef PCICDEBUG
			char bitbuf[64];
#endif
			reg = PCIC_CSC_INTR_CD_ENABLE;
			if (sc->irq != -1)
			    reg |= sc->irq << PCIC_CSC_INTR_IRQ_SHIFT;
			pcic_write(h, PCIC_CSC_INTR, reg);
#ifdef PCICDEBUG
			snprintb(bitbuf, sizeof(bitbuf), PCIC_CSC_INTR_FORMAT,
			    pcic_read(h, PCIC_CSC_INTR));
#endif
			DPRINTF(("%s: CSC_INTR was zero; reset to %s\n",
			    device_xname(sc->dev), bitbuf));
		}

		/*
		 * check for card insertion or removal during suspend period.
		 * XXX: the code can't cope with card swap (remove then insert).
		 * how can we detect such situation?
		 */
		if (why == PWR_RESUME)
			(void)pcic_intr_socket(h);
	}
}


/*
 * attach a socket -- we don't know about irqs yet
 */
void
pcic_attach_socket(struct pcic_handle *h)
{
	struct pcmciabus_attach_args paa;
	struct pcic_softc *sc = device_private(h->ph_parent);
	int locs[PCMCIABUSCF_NLOCS];

	/* initialize the rest of the handle */

	h->shutdown = 0;
	h->memalloc = 0;
	h->ioalloc = 0;
	h->ih_irq = 0;

	/* now, config one pcmcia device per socket */

	paa.paa_busname = "pcmcia";
	paa.pct = (pcmcia_chipset_tag_t) sc->pct;
	paa.pch = (pcmcia_chipset_handle_t) h;

	locs[PCMCIABUSCF_CONTROLLER] = h->chip;
	locs[PCMCIABUSCF_SOCKET] = h->socket;

	h->pcmcia = config_found_sm_loc(sc->dev, "pcmciabus", locs, &paa,
					pcic_print, config_stdsubmatch);
	if (h->pcmcia == NULL) {
		h->flags &= ~PCIC_FLAG_SOCKETP;
		return;
	}

}

/*
 * now finish attaching the sockets, we are ready to allocate
 * interrupts
 */
void
pcic_attach_sockets_finish(struct pcic_softc *sc)
{
	int i;

	for (i = 0; i < __arraycount(sc->handle); i++)
		if (sc->handle[i].flags & PCIC_FLAG_SOCKETP)
			pcic_attach_socket_finish(&sc->handle[i]);
}

/*
 * finishing attaching the socket.  Interrupts may now be on
 * if so expects the pcic interrupt to be blocked
 */
void
pcic_attach_socket_finish(struct pcic_handle *h)
{
	struct pcic_softc *sc = device_private(h->ph_parent);
	int reg;
	char cs[4];

	DPRINTF(("%s: attach finish socket %ld\n", device_xname(h->ph_parent),
	    (long) (h - &sc->handle[0])));

	/*
	 * Set up a powerhook to ensure it continues to interrupt on
	 * card detect even after suspend.
	 * (this works around a bug seen in suspend-to-disk on the
	 * Sony VAIO Z505; on resume, the CSC_INTR state is not preserved).
	 */
	powerhook_establish(device_xname(h->ph_parent), pcic_power, h);

	/* enable interrupts on card detect, poll for them if no irq avail */
	reg = PCIC_CSC_INTR_CD_ENABLE;
	if (sc->irq == -1) {
		if (sc->poll_established == 0) {
			callout_init(&sc->poll_ch, 0);
			callout_reset(&sc->poll_ch, hz / 2, pcic_poll_intr, sc);
			sc->poll_established = 1;
		}
	} else
		reg |= sc->irq << PCIC_CSC_INTR_IRQ_SHIFT;
	pcic_write(h, PCIC_CSC_INTR, reg);

	/* steer above mgmt interrupt to configured place */
	if (sc->irq == 0)
		pcic_write(h, PCIC_INTR, PCIC_INTR_ENABLE);

	/* clear possible card detect interrupt */
	(void) pcic_read(h, PCIC_CSC);

	DPRINTF(("%s: attach finish vendor 0x%02x\n",
	    device_xname(h->ph_parent), h->vendor));

	/* unsleep the cirrus controller */
	if (h->vendor == PCIC_VENDOR_CIRRUS_PD67XX) {
		reg = pcic_read(h, PCIC_CIRRUS_MISC_CTL_2);
		if (reg & PCIC_CIRRUS_MISC_CTL_2_SUSPEND) {
			DPRINTF(("%s: socket %02x was suspended\n",
			    device_xname(h->ph_parent), h->sock));
			reg &= ~PCIC_CIRRUS_MISC_CTL_2_SUSPEND;
			pcic_write(h, PCIC_CIRRUS_MISC_CTL_2, reg);
		}
	}

	/* if there's a card there, then attach it. */
	reg = pcic_read(h, PCIC_IF_STATUS);
	if ((reg & PCIC_IF_STATUS_CARDDETECT_MASK) ==
	    PCIC_IF_STATUS_CARDDETECT_PRESENT) {
		pcic_queue_event(h, PCIC_EVENT_INSERTION);
		h->laststate = PCIC_LASTSTATE_PRESENT;
	} else {
		h->laststate = PCIC_LASTSTATE_EMPTY;
	}

	/*
	 * queue creation of a kernel thread to handle insert/removal events.
	 */
#ifdef DIAGNOSTIC
	if (h->event_thread != NULL)
		panic("pcic_attach_socket: event thread");
#endif
	config_pending_incr(sc->dev);
	snprintf(cs, sizeof(cs), "%d,%d", h->chip, h->socket);

	if (kthread_create(PRI_NONE, 0, NULL, pcic_event_thread, h,
	    &h->event_thread, "%s,%s", device_xname(h->ph_parent), cs)) {
		aprint_error_dev(h->ph_parent,
		    "unable to create event thread for sock 0x%02x\n", h->sock);
		panic("pcic_attach_socket");
	}
}

void
pcic_event_thread(void *arg)
{
	struct pcic_handle *h = arg;
	struct pcic_event *pe;
	int s, first = 1;
	struct pcic_softc *sc = device_private(h->ph_parent);

	while (h->shutdown == 0) {
		/*
		 * Serialize event processing on the PCIC.  We may
		 * sleep while we hold this lock.
		 */
		mutex_enter(&sc->sc_pcic_lock);

		s = splhigh();
		if ((pe = SIMPLEQ_FIRST(&h->events)) == NULL) {
			splx(s);
			if (first) {
				first = 0;
				config_pending_decr(sc->dev);
			}
			/*
			 * No events to process; release the PCIC lock.
			 */
			(void) mutex_exit(&sc->sc_pcic_lock);
			(void) tsleep(&h->events, PWAIT, "pcicev", 0);
			continue;
		} else {
			splx(s);
			/* sleep .25s to be enqueued chatterling interrupts */
			(void) tsleep((void *)pcic_event_thread, PWAIT,
			    "pcicss", hz / 4);
		}
		s = splhigh();
		SIMPLEQ_REMOVE_HEAD(&h->events, pe_q);
		splx(s);

		switch (pe->pe_type) {
		case PCIC_EVENT_INSERTION:
			s = splhigh();
			for (;;) {
				struct pcic_event *pe1, *pe2;

				if ((pe1 = SIMPLEQ_FIRST(&h->events)) == NULL)
					break;
				if (pe1->pe_type != PCIC_EVENT_REMOVAL)
					break;
				if ((pe2 = SIMPLEQ_NEXT(pe1, pe_q)) == NULL)
					break;
				if (pe2->pe_type == PCIC_EVENT_INSERTION) {
					SIMPLEQ_REMOVE_HEAD(&h->events, pe_q);
					free(pe1, M_TEMP);
					SIMPLEQ_REMOVE_HEAD(&h->events, pe_q);
					free(pe2, M_TEMP);
				}
			}
			splx(s);

			DPRINTF(("%s: insertion event\n",
			    device_xname(h->ph_parent)));
			pcic_attach_card(h);
			break;

		case PCIC_EVENT_REMOVAL:
			s = splhigh();
			for (;;) {
				struct pcic_event *pe1, *pe2;

				if ((pe1 = SIMPLEQ_FIRST(&h->events)) == NULL)
					break;
				if (pe1->pe_type != PCIC_EVENT_INSERTION)
					break;
				if ((pe2 = SIMPLEQ_NEXT(pe1, pe_q)) == NULL)
					break;
				if (pe2->pe_type == PCIC_EVENT_REMOVAL) {
					SIMPLEQ_REMOVE_HEAD(&h->events, pe_q);
					free(pe1, M_TEMP);
					SIMPLEQ_REMOVE_HEAD(&h->events, pe_q);
					free(pe2, M_TEMP);
				}
			}
			splx(s);

			DPRINTF(("%s: removal event\n",
			    device_xname(h->ph_parent)));
			pcic_detach_card(h, DETACH_FORCE);
			break;

		default:
			panic("pcic_event_thread: unknown event %d",
			    pe->pe_type);
		}
		free(pe, M_TEMP);

		mutex_exit(&sc->sc_pcic_lock);
	}

	h->event_thread = NULL;

	/* In case parent is waiting for us to exit. */
	wakeup(sc);

	kthread_exit(0);
}

int
pcic_print(void *arg, const char *pnp)
{
	struct pcmciabus_attach_args *paa = arg;
	struct pcic_handle *h = (struct pcic_handle *) paa->pch;

	/* Only "pcmcia"s can attach to "pcic"s... easy. */
	if (pnp)
		aprint_normal("pcmcia at %s", pnp);

	aprint_normal(" controller %d socket %d", h->chip, h->socket);

	return UNCONF;
}

void
pcic_poll_intr(void *arg)
{
	struct pcic_softc *sc;
	int i, s;

	s = spltty();
	sc = arg;
	for (i = 0; i < __arraycount(sc->handle); i++)
		if (sc->handle[i].flags & PCIC_FLAG_SOCKETP)
			(void)pcic_intr_socket(&sc->handle[i]);
	callout_reset(&sc->poll_ch, hz / 2, pcic_poll_intr, sc);
	splx(s);
}

int
pcic_intr(void *arg)
{
	struct pcic_softc *sc = arg;
	int i, ret = 0;

	DPRINTF(("%s: intr\n", device_xname(sc->dev)));

	for (i = 0; i < __arraycount(sc->handle); i++)
		if (sc->handle[i].flags & PCIC_FLAG_SOCKETP)
			ret += pcic_intr_socket(&sc->handle[i]);

	return ret ? 1 : 0;
}

int
pcic_intr_socket(struct pcic_handle *h)
{
	int cscreg;

	cscreg = pcic_read(h, PCIC_CSC);

	cscreg &= (PCIC_CSC_GPI |
		   PCIC_CSC_CD |
		   PCIC_CSC_READY |
		   PCIC_CSC_BATTWARN |
		   PCIC_CSC_BATTDEAD);

	if (cscreg & PCIC_CSC_GPI) {
		DPRINTF(("%s: %02x GPI\n",
		    device_xname(h->ph_parent), h->sock));
	}
	if (cscreg & PCIC_CSC_CD) {
		int statreg;

		statreg = pcic_read(h, PCIC_IF_STATUS);

		DPRINTF(("%s: %02x CD %x\n", device_xname(h->ph_parent),
		    h->sock, statreg));

		if ((statreg & PCIC_IF_STATUS_CARDDETECT_MASK) ==
		    PCIC_IF_STATUS_CARDDETECT_PRESENT) {
			if (h->laststate != PCIC_LASTSTATE_PRESENT) {
				DPRINTF(("%s: enqueing INSERTION event\n",
				    device_xname(h->ph_parent)));
				pcic_queue_event(h, PCIC_EVENT_INSERTION);
			}
			h->laststate = PCIC_LASTSTATE_PRESENT;
		} else {
			if (h->laststate == PCIC_LASTSTATE_PRESENT) {
				/* Deactivate the card now. */
				DPRINTF(("%s: deactivating card\n",
				    device_xname(h->ph_parent)));
				pcic_deactivate_card(h);

				DPRINTF(("%s: enqueing REMOVAL event\n",
				    device_xname(h->ph_parent)));
				pcic_queue_event(h, PCIC_EVENT_REMOVAL);
			}
			h->laststate = PCIC_LASTSTATE_EMPTY;
		}
	}
	if (cscreg & PCIC_CSC_READY) {
		DPRINTF(("%s: %02x READY\n", device_xname(h->ph_parent),
		     h->sock));
		/* shouldn't happen */
	}
	if (cscreg & PCIC_CSC_BATTWARN) {
		DPRINTF(("%s: %02x BATTWARN\n", device_xname(h->ph_parent),
		    h->sock));
	}
	if (cscreg & PCIC_CSC_BATTDEAD) {
		DPRINTF(("%s: %02x BATTDEAD\n", device_xname(h->ph_parent),
		    h->sock));
	}
	return cscreg ? 1 : 0;
}

void
pcic_queue_event(struct pcic_handle *h, int event)
{
	struct pcic_event *pe;
	int s;

	pe = malloc(sizeof(*pe), M_TEMP, M_NOWAIT);
	if (pe == NULL)
		panic("pcic_queue_event: can't allocate event");

	pe->pe_type = event;
	s = splhigh();
	SIMPLEQ_INSERT_TAIL(&h->events, pe, pe_q);
	splx(s);
	wakeup(&h->events);
}

void
pcic_attach_card(struct pcic_handle *h)
{

	if ((h->flags & PCIC_FLAG_CARDP) == 0) {
		/* call the MI attach function */
		pcmcia_card_attach(h->pcmcia);

		h->flags |= PCIC_FLAG_CARDP;
	} else {
		DPRINTF(("pcic_attach_card: already attached"));
	}
}

void
pcic_detach_card(struct pcic_handle *h, int flags)
	/* flags:		 DETACH_* */
{

	if (h->flags & PCIC_FLAG_CARDP) {
		h->flags &= ~PCIC_FLAG_CARDP;

		/* call the MI detach function */
		pcmcia_card_detach(h->pcmcia, flags);
	} else {
		DPRINTF(("pcic_detach_card: already detached"));
	}
}

void
pcic_deactivate_card(struct pcic_handle *h)
{
	int intr;

	/* call the MI deactivate function */
	pcmcia_card_deactivate(h->pcmcia);

	/* reset the socket */
	intr = pcic_read(h, PCIC_INTR);
	intr &= PCIC_INTR_ENABLE;
	pcic_write(h, PCIC_INTR, intr);

	/* power down the socket */
	pcic_write(h, PCIC_PWRCTL, 0);
}

int
pcic_chip_mem_alloc(pcmcia_chipset_handle_t pch, bus_size_t size,
    struct pcmcia_mem_handle *pcmhp)
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
	bus_space_handle_t memh;
	bus_addr_t addr;
	bus_size_t sizepg;
	int i, mask, mhandle;
	struct pcic_softc *sc = device_private(h->ph_parent);

	/* out of sc->memh, allocate as many pages as necessary */

	/* convert size to PCIC pages */
	sizepg = (size + (PCIC_MEM_ALIGN - 1)) / PCIC_MEM_ALIGN;
	if (sizepg > PCIC_MAX_MEM_PAGES)
		return 1;

	mask = (1 << sizepg) - 1;

	addr = 0;		/* XXX gcc -Wuninitialized */
	mhandle = 0;		/* XXX gcc -Wuninitialized */

	for (i = 0; i <= PCIC_MAX_MEM_PAGES - sizepg; i++) {
		if ((sc->subregionmask & (mask << i)) == (mask << i)) {
			if (bus_space_subregion(sc->memt, sc->memh,
			    i * PCIC_MEM_PAGESIZE,
			    sizepg * PCIC_MEM_PAGESIZE, &memh))
				return 1;
			mhandle = mask << i;
			addr = sc->membase + (i * PCIC_MEM_PAGESIZE);
			sc->subregionmask &= ~(mhandle);
			pcmhp->memt = sc->memt;
			pcmhp->memh = memh;
			pcmhp->addr = addr;
			pcmhp->size = size;
			pcmhp->mhandle = mhandle;
			pcmhp->realsize = sizepg * PCIC_MEM_PAGESIZE;
			return 0;
		}
	}

	return 1;
}

void
pcic_chip_mem_free(pcmcia_chipset_handle_t pch, struct pcmcia_mem_handle *pcmhp)
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
	struct pcic_softc *sc = device_private(h->ph_parent);

	sc->subregionmask |= pcmhp->mhandle;
}

static const struct mem_map_index_st {
	int	sysmem_start_lsb;
	int	sysmem_start_msb;
	int	sysmem_stop_lsb;
	int	sysmem_stop_msb;
	int	cardmem_lsb;
	int	cardmem_msb;
	int	memenable;
} mem_map_index[] = {
	{
		PCIC_SYSMEM_ADDR0_START_LSB,
		PCIC_SYSMEM_ADDR0_START_MSB,
		PCIC_SYSMEM_ADDR0_STOP_LSB,
		PCIC_SYSMEM_ADDR0_STOP_MSB,
		PCIC_CARDMEM_ADDR0_LSB,
		PCIC_CARDMEM_ADDR0_MSB,
		PCIC_ADDRWIN_ENABLE_MEM0,
	},
	{
		PCIC_SYSMEM_ADDR1_START_LSB,
		PCIC_SYSMEM_ADDR1_START_MSB,
		PCIC_SYSMEM_ADDR1_STOP_LSB,
		PCIC_SYSMEM_ADDR1_STOP_MSB,
		PCIC_CARDMEM_ADDR1_LSB,
		PCIC_CARDMEM_ADDR1_MSB,
		PCIC_ADDRWIN_ENABLE_MEM1,
	},
	{
		PCIC_SYSMEM_ADDR2_START_LSB,
		PCIC_SYSMEM_ADDR2_START_MSB,
		PCIC_SYSMEM_ADDR2_STOP_LSB,
		PCIC_SYSMEM_ADDR2_STOP_MSB,
		PCIC_CARDMEM_ADDR2_LSB,
		PCIC_CARDMEM_ADDR2_MSB,
		PCIC_ADDRWIN_ENABLE_MEM2,
	},
	{
		PCIC_SYSMEM_ADDR3_START_LSB,
		PCIC_SYSMEM_ADDR3_START_MSB,
		PCIC_SYSMEM_ADDR3_STOP_LSB,
		PCIC_SYSMEM_ADDR3_STOP_MSB,
		PCIC_CARDMEM_ADDR3_LSB,
		PCIC_CARDMEM_ADDR3_MSB,
		PCIC_ADDRWIN_ENABLE_MEM3,
	},
	{
		PCIC_SYSMEM_ADDR4_START_LSB,
		PCIC_SYSMEM_ADDR4_START_MSB,
		PCIC_SYSMEM_ADDR4_STOP_LSB,
		PCIC_SYSMEM_ADDR4_STOP_MSB,
		PCIC_CARDMEM_ADDR4_LSB,
		PCIC_CARDMEM_ADDR4_MSB,
		PCIC_ADDRWIN_ENABLE_MEM4,
	},
};

void
pcic_chip_do_mem_map(struct pcic_handle *h, int win)
{
	int reg;
	int kind = h->mem[win].kind & ~PCMCIA_WIDTH_MEM_MASK;
	int mem8 =
	    (h->mem[win].kind & PCMCIA_WIDTH_MEM_MASK) == PCMCIA_WIDTH_MEM8
	    || (kind == PCMCIA_MEM_ATTR);

	DPRINTF(("mem8 %d\n", mem8));
	/* mem8 = 1; */

	pcic_write(h, mem_map_index[win].sysmem_start_lsb,
	    (h->mem[win].addr >> PCIC_SYSMEM_ADDRX_SHIFT) & 0xff);
	pcic_write(h, mem_map_index[win].sysmem_start_msb,
	    ((h->mem[win].addr >> (PCIC_SYSMEM_ADDRX_SHIFT + 8)) &
	    PCIC_SYSMEM_ADDRX_START_MSB_ADDR_MASK) |
	    (mem8 ? 0 : PCIC_SYSMEM_ADDRX_START_MSB_DATASIZE_16BIT));

	pcic_write(h, mem_map_index[win].sysmem_stop_lsb,
	    ((h->mem[win].addr + h->mem[win].size) >>
	    PCIC_SYSMEM_ADDRX_SHIFT) & 0xff);
	pcic_write(h, mem_map_index[win].sysmem_stop_msb,
	    (((h->mem[win].addr + h->mem[win].size) >>
	    (PCIC_SYSMEM_ADDRX_SHIFT + 8)) &
	    PCIC_SYSMEM_ADDRX_STOP_MSB_ADDR_MASK) |
	    PCIC_SYSMEM_ADDRX_STOP_MSB_WAIT2);

	pcic_write(h, mem_map_index[win].cardmem_lsb,
	    (h->mem[win].offset >> PCIC_CARDMEM_ADDRX_SHIFT) & 0xff);
	pcic_write(h, mem_map_index[win].cardmem_msb,
	    ((h->mem[win].offset >> (PCIC_CARDMEM_ADDRX_SHIFT + 8)) &
	    PCIC_CARDMEM_ADDRX_MSB_ADDR_MASK) |
	    ((kind == PCMCIA_MEM_ATTR) ?
	    PCIC_CARDMEM_ADDRX_MSB_REGACTIVE_ATTR : 0));

	reg = pcic_read(h, PCIC_ADDRWIN_ENABLE);
	reg |= (mem_map_index[win].memenable | PCIC_ADDRWIN_ENABLE_MEMCS16);
	pcic_write(h, PCIC_ADDRWIN_ENABLE, reg);

	delay(100);

#ifdef PCICDEBUG
	{
		int r1, r2, r3, r4, r5, r6;

		r1 = pcic_read(h, mem_map_index[win].sysmem_start_msb);
		r2 = pcic_read(h, mem_map_index[win].sysmem_start_lsb);
		r3 = pcic_read(h, mem_map_index[win].sysmem_stop_msb);
		r4 = pcic_read(h, mem_map_index[win].sysmem_stop_lsb);
		r5 = pcic_read(h, mem_map_index[win].cardmem_msb);
		r6 = pcic_read(h, mem_map_index[win].cardmem_lsb);

		DPRINTF(("pcic_chip_do_mem_map window %d: %02x%02x %02x%02x "
		    "%02x%02x\n", win, r1, r2, r3, r4, r5, r6));
	}
#endif
}

int
pcic_chip_mem_map(pcmcia_chipset_handle_t pch, int kind, bus_addr_t card_addr,
    bus_size_t size, struct pcmcia_mem_handle *pcmhp, bus_size_t *offsetp,
    int *windowp)
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
	bus_addr_t busaddr;
	long card_offset;
	int i, win;

	win = -1;
	for (i = 0; i < (sizeof(mem_map_index) / sizeof(mem_map_index[0]));
	    i++) {
		if ((h->memalloc & (1 << i)) == 0) {
			win = i;
			h->memalloc |= (1 << i);
			break;
		}
	}

	if (win == -1)
		return 1;

	*windowp = win;

	/* XXX this is pretty gross */

{
	struct pcic_softc *sc = device_private(h->ph_parent);
	if (!bus_space_is_equal(sc->memt, pcmhp->memt))
		panic("pcic_chip_mem_map memt is bogus");
}

	busaddr = pcmhp->addr;

	/*
	 * compute the address offset to the pcmcia address space for the
	 * pcic.  this is intentionally signed.  The masks and shifts below
	 * will cause TRT to happen in the pcic registers.  Deal with making
	 * sure the address is aligned, and return the alignment offset.
	 */

	*offsetp = card_addr % PCIC_MEM_ALIGN;
	card_addr -= *offsetp;

	DPRINTF(("pcic_chip_mem_map window %d bus %lx+%lx+%lx at card addr "
	    "%lx\n", win, (u_long) busaddr, (u_long) * offsetp, (u_long) size,
	    (u_long) card_addr));

	/*
	 * include the offset in the size, and decrement size by one, since
	 * the hw wants start/stop
	 */
	size += *offsetp - 1;

	card_offset = (((long) card_addr) - ((long) busaddr));

	h->mem[win].addr = busaddr;
	h->mem[win].size = size;
	h->mem[win].offset = card_offset;
	h->mem[win].kind = kind;

	pcic_chip_do_mem_map(h, win);

	return 0;
}

void
pcic_chip_mem_unmap(pcmcia_chipset_handle_t pch, int window)
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
	int reg;

	if (window >= (sizeof(mem_map_index) / sizeof(mem_map_index[0])))
		panic("pcic_chip_mem_unmap: window out of range");

	reg = pcic_read(h, PCIC_ADDRWIN_ENABLE);
	reg &= ~mem_map_index[window].memenable;
	pcic_write(h, PCIC_ADDRWIN_ENABLE, reg);

	h->memalloc &= ~(1 << window);
}

int
pcic_chip_io_alloc(pcmcia_chipset_handle_t pch, bus_addr_t start,
    bus_size_t size, bus_size_t align, struct pcmcia_io_handle *pcihp)
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_addr_t ioaddr;
	int flags = 0;
	struct pcic_softc *sc = device_private(h->ph_parent);

	/*
	 * Allocate some arbitrary I/O space.
	 */

	iot = sc->iot;

	if (start) {
		ioaddr = start;
		if (bus_space_map(iot, start, size, 0, &ioh))
			return 1;
		DPRINTF(("pcic_chip_io_alloc map port %lx+%lx\n",
		    (u_long) ioaddr, (u_long) size));
	} else {
		flags |= PCMCIA_IO_ALLOCATED;
		if (bus_space_alloc(iot, sc->iobase,
		    sc->iobase + sc->iosize, size, align, 0, 0,
		    &ioaddr, &ioh))
			return 1;
		DPRINTF(("pcic_chip_io_alloc alloc port %lx+%lx\n",
		    (u_long) ioaddr, (u_long) size));
	}

	pcihp->iot = iot;
	pcihp->ioh = ioh;
	pcihp->addr = ioaddr;
	pcihp->size = size;
	pcihp->flags = flags;

	return 0;
}

void
pcic_chip_io_free(pcmcia_chipset_handle_t pch, struct pcmcia_io_handle *pcihp)
{
	bus_space_tag_t iot = pcihp->iot;
	bus_space_handle_t ioh = pcihp->ioh;
	bus_size_t size = pcihp->size;

	if (pcihp->flags & PCMCIA_IO_ALLOCATED)
		bus_space_free(iot, ioh, size);
	else
		bus_space_unmap(iot, ioh, size);
}


static const struct io_map_index_st {
	int	start_lsb;
	int	start_msb;
	int	stop_lsb;
	int	stop_msb;
	int	ioenable;
	int	ioctlmask;
	int	ioctlbits[3];		/* indexed by PCMCIA_WIDTH_* */
}               io_map_index[] = {
	{
		PCIC_IOADDR0_START_LSB,
		PCIC_IOADDR0_START_MSB,
		PCIC_IOADDR0_STOP_LSB,
		PCIC_IOADDR0_STOP_MSB,
		PCIC_ADDRWIN_ENABLE_IO0,
		PCIC_IOCTL_IO0_WAITSTATE | PCIC_IOCTL_IO0_ZEROWAIT |
		PCIC_IOCTL_IO0_IOCS16SRC_MASK | PCIC_IOCTL_IO0_DATASIZE_MASK,
		{
			PCIC_IOCTL_IO0_IOCS16SRC_CARD,
			PCIC_IOCTL_IO0_IOCS16SRC_DATASIZE |
			    PCIC_IOCTL_IO0_DATASIZE_8BIT,
			PCIC_IOCTL_IO0_IOCS16SRC_DATASIZE |
			    PCIC_IOCTL_IO0_DATASIZE_16BIT,
		},
	},
	{
		PCIC_IOADDR1_START_LSB,
		PCIC_IOADDR1_START_MSB,
		PCIC_IOADDR1_STOP_LSB,
		PCIC_IOADDR1_STOP_MSB,
		PCIC_ADDRWIN_ENABLE_IO1,
		PCIC_IOCTL_IO1_WAITSTATE | PCIC_IOCTL_IO1_ZEROWAIT |
		PCIC_IOCTL_IO1_IOCS16SRC_MASK | PCIC_IOCTL_IO1_DATASIZE_MASK,
		{
			PCIC_IOCTL_IO1_IOCS16SRC_CARD,
			PCIC_IOCTL_IO1_IOCS16SRC_DATASIZE |
			    PCIC_IOCTL_IO1_DATASIZE_8BIT,
			PCIC_IOCTL_IO1_IOCS16SRC_DATASIZE |
			    PCIC_IOCTL_IO1_DATASIZE_16BIT,
		},
	},
};

void
pcic_chip_do_io_map(struct pcic_handle *h, int win)
{
	int reg;

	DPRINTF(("pcic_chip_do_io_map win %d addr %lx size %lx width %d\n",
	    win, (long) h->io[win].addr, (long) h->io[win].size,
	    h->io[win].width * 8));

	pcic_write(h, io_map_index[win].start_lsb, h->io[win].addr & 0xff);
	pcic_write(h, io_map_index[win].start_msb,
	    (h->io[win].addr >> 8) & 0xff);

	pcic_write(h, io_map_index[win].stop_lsb,
	    (h->io[win].addr + h->io[win].size - 1) & 0xff);
	pcic_write(h, io_map_index[win].stop_msb,
	    ((h->io[win].addr + h->io[win].size - 1) >> 8) & 0xff);

	reg = pcic_read(h, PCIC_IOCTL);
	reg &= ~io_map_index[win].ioctlmask;
	reg |= io_map_index[win].ioctlbits[h->io[win].width];
	pcic_write(h, PCIC_IOCTL, reg);

	reg = pcic_read(h, PCIC_ADDRWIN_ENABLE);
	reg |= io_map_index[win].ioenable;
	pcic_write(h, PCIC_ADDRWIN_ENABLE, reg);
}

int
pcic_chip_io_map(pcmcia_chipset_handle_t pch, int width, bus_addr_t offset,
    bus_size_t size, struct pcmcia_io_handle *pcihp, int *windowp)
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
	bus_addr_t ioaddr = pcihp->addr + offset;
	int i, win;
#ifdef PCICDEBUG
	static const char *width_names[] = { "auto", "io8", "io16" };
#endif
	struct pcic_softc *sc = device_private(h->ph_parent);

	/* XXX Sanity check offset/size. */

	win = -1;
	for (i = 0; i < (sizeof(io_map_index) / sizeof(io_map_index[0])); i++) {
		if ((h->ioalloc & (1 << i)) == 0) {
			win = i;
			h->ioalloc |= (1 << i);
			break;
		}
	}

	if (win == -1)
		return 1;

	*windowp = win;

	/* XXX this is pretty gross */

	if (!bus_space_is_equal(sc->iot, pcihp->iot))
		panic("pcic_chip_io_map iot is bogus");

	DPRINTF(("pcic_chip_io_map window %d %s port %lx+%lx\n",
	    win, width_names[width], (u_long) ioaddr, (u_long) size));

	/* XXX wtf is this doing here? */

	printf("%s: port 0x%lx", device_xname(sc->dev), (u_long) ioaddr);
	if (size > 1)
		printf("-0x%lx", (u_long) ioaddr + (u_long) size - 1);
	printf("\n");

	h->io[win].addr = ioaddr;
	h->io[win].size = size;
	h->io[win].width = width;

	pcic_chip_do_io_map(h, win);

	return 0;
}

void
pcic_chip_io_unmap(pcmcia_chipset_handle_t pch, int window)
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
	int reg;

	if (window >= (sizeof(io_map_index) / sizeof(io_map_index[0])))
		panic("pcic_chip_io_unmap: window out of range");

	reg = pcic_read(h, PCIC_ADDRWIN_ENABLE);
	reg &= ~io_map_index[window].ioenable;
	pcic_write(h, PCIC_ADDRWIN_ENABLE, reg);

	h->ioalloc &= ~(1 << window);
}

static int
pcic_wait_ready(struct pcic_handle *h)
{
	uint8_t stat;
	int i;

	/* wait an initial 10ms for quick cards */
	stat = pcic_read(h, PCIC_IF_STATUS);
	if (stat & PCIC_IF_STATUS_READY)
		return 0;
	pcic_delay(h, 10, "pccwr0");
	for (i = 0; i < 50; i++) {
		stat = pcic_read(h, PCIC_IF_STATUS);
		if (stat & PCIC_IF_STATUS_READY)
			return 0;
		if ((stat & PCIC_IF_STATUS_CARDDETECT_MASK) !=
		    PCIC_IF_STATUS_CARDDETECT_PRESENT)
			return ENXIO;
		/* wait .1s (100ms) each iteration now */
		pcic_delay(h, 100, "pccwr1");
	}

	printf("pcic_wait_ready: ready never happened, status=%02x\n", stat);
	return EWOULDBLOCK;
}

/*
 * Perform long (msec order) delay.
 */
static void
pcic_delay(struct pcic_handle *h, int timo, const char *wmesg)
	/* timo:			 in ms.  must not be zero */
{

#ifdef DIAGNOSTIC
	if (timo <= 0)
		panic("pcic_delay: called with timeout %d", timo);
	if (!curlwp)
		panic("pcic_delay: called in interrupt context");
	if (!h->event_thread)
		panic("pcic_delay: no event thread");
#endif
	DPRINTF(("pcic_delay: \"%s\" %p, sleep %d ms\n",
	    wmesg, h->event_thread, timo));
	if (doing_shutdown)
		delay(timo * 1000);
	else
		tsleep(pcic_delay, PWAIT, wmesg,
		    roundup(timo * hz, 1000) / 1000);
}

void
pcic_chip_socket_enable(pcmcia_chipset_handle_t pch)
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
	int win;
	uint8_t power, intr;
#ifdef DIAGNOSTIC
	int reg;
#endif

#ifdef DIAGNOSTIC
	if (h->flags & PCIC_FLAG_ENABLED)
		printf("pcic_chip_socket_enable: enabling twice\n");
#endif

	/* disable interrupts; assert RESET */
	intr = pcic_read(h, PCIC_INTR);
	intr &= PCIC_INTR_ENABLE;
	pcic_write(h, PCIC_INTR, intr);

	/* zero out the address windows */
	pcic_write(h, PCIC_ADDRWIN_ENABLE, 0);

	/* power off; assert output enable bit */
	power = PCIC_PWRCTL_OE;
	pcic_write(h, PCIC_PWRCTL, power);

	/*
	 * power hack for RICOH RF5C[23]96
	 */
	switch (h->vendor) {
	case PCIC_VENDOR_RICOH_5C296:
	case PCIC_VENDOR_RICOH_5C396:
	    {
		int regtmp;
		regtmp = pcic_read(h, PCIC_RICOH_REG_MCR2);
#ifdef RICOH_POWER_HACK
		regtmp |= PCIC_RICOH_MCR2_VCC_DIRECT;
#else
		regtmp &=
		    ~(PCIC_RICOH_MCR2_VCC_DIRECT|PCIC_RICOH_MCR2_VCC_SEL_3V);
#endif
		pcic_write(h, PCIC_RICOH_REG_MCR2, regtmp);
	    }
		break;
	default:
		break;
	}

#ifdef VADEM_POWER_HACK
	bus_space_write_1(sc->iot, sc->ioh, PCIC_REG_INDEX, 0x0e);
	bus_space_write_1(sc->iot, sc->ioh, PCIC_REG_INDEX, 0x37);
	printf("prcr = %02x\n", pcic_read(h, 0x02));
	printf("cvsr = %02x\n", pcic_read(h, 0x2f));
	printf("DANGER WILL ROBINSON!  Changing voltage select!\n");
	pcic_write(h, 0x2f, pcic_read(h, 0x2f) & ~0x03);
	printf("cvsr = %02x\n", pcic_read(h, 0x2f));
#endif

	/* power up the socket */
	power |= PCIC_PWRCTL_PWR_ENABLE | PCIC_PWRCTL_VPP1_VCC;
	pcic_write(h, PCIC_PWRCTL, power);

	/*
	 * Table 4-18 and figure 4-6 of the PC Card specifiction say:
	 * Vcc Rising Time (Tpr) = 100ms
	 * RESET Width (Th (Hi-z RESET)) = 1ms
	 * RESET Width (Tw (RESET)) = 10us
	 *
	 * some machines require some more time to be settled
	 * (100ms is added here).
	 */
	pcic_delay(h, 200 + 1, "pccen1");

	/* negate RESET */
	intr |= PCIC_INTR_RESET;
	pcic_write(h, PCIC_INTR, intr);

	/*
	 * RESET Setup Time (Tsu (RESET)) = 20ms
	 */
	pcic_delay(h, 20, "pccen2");

#ifdef DIAGNOSTIC
	reg = pcic_read(h, PCIC_IF_STATUS);
	if ((reg & PCIC_IF_STATUS_POWERACTIVE) == 0)
		printf("pcic_chip_socket_enable: no power, status=%x\n", reg);
#endif

	/* wait for the chip to finish initializing */
	if (pcic_wait_ready(h)) {
		/* XXX return a failure status?? */
		pcic_write(h, PCIC_PWRCTL, 0);
		return;
	}

	/* reinstall all the memory and io mappings */
	for (win = 0; win < PCIC_MEM_WINS; win++)
		if (h->memalloc & (1 << win))
			pcic_chip_do_mem_map(h, win);
	for (win = 0; win < PCIC_IO_WINS; win++)
		if (h->ioalloc & (1 << win))
			pcic_chip_do_io_map(h, win);

	h->flags |= PCIC_FLAG_ENABLED;
}

void
pcic_chip_socket_disable(pcmcia_chipset_handle_t pch)
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
	uint8_t intr;

	DPRINTF(("pcic_chip_socket_disable\n"));

	/* disable interrupts; assert RESET */
	intr = pcic_read(h, PCIC_INTR);
	intr &= PCIC_INTR_ENABLE;
	pcic_write(h, PCIC_INTR, intr);

	/* zero out the address windows */
	pcic_write(h, PCIC_ADDRWIN_ENABLE, 0);

	/* disable socket: negate output enable bit and power off */
	pcic_write(h, PCIC_PWRCTL, 0);

	/*
	 * Vcc Falling Time (Tpf) = 300ms
	 */
	pcic_delay(h, 300, "pccwr1");

	h->flags &= ~PCIC_FLAG_ENABLED;
}

void
pcic_chip_socket_settype(pcmcia_chipset_handle_t pch, int type)
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
	int intr;

	intr = pcic_read(h, PCIC_INTR);
	intr &= ~(PCIC_INTR_IRQ_MASK | PCIC_INTR_CARDTYPE_MASK);
	if (type == PCMCIA_IFTYPE_IO) {
		intr |= PCIC_INTR_CARDTYPE_IO;
		intr |= h->ih_irq << PCIC_INTR_IRQ_SHIFT;
	} else
		intr |= PCIC_INTR_CARDTYPE_MEM;
	pcic_write(h, PCIC_INTR, intr);

	DPRINTF(("%s: pcic_chip_socket_settype %02x type %s %02x\n",
	    device_xname(h->ph_parent), h->sock,
	    ((type == PCMCIA_IFTYPE_IO) ? "io" : "mem"), intr));
}

static uint8_t
st_pcic_read(struct pcic_handle *h, int idx)
{

	if (idx != -1)
		bus_space_write_1(h->ph_bus_t, h->ph_bus_h, PCIC_REG_INDEX,
		    h->sock + idx);
	return bus_space_read_1(h->ph_bus_t, h->ph_bus_h, PCIC_REG_DATA);
}

static void
st_pcic_write(struct pcic_handle *h, int idx, uint8_t data)
{

	if (idx != -1)
		bus_space_write_1(h->ph_bus_t, h->ph_bus_h, PCIC_REG_INDEX,
		    h->sock + idx);
	bus_space_write_1(h->ph_bus_t, h->ph_bus_h, PCIC_REG_DATA, data);
}

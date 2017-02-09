/*	$NetBSD: cardslot.c,v 1.55 2013/10/12 16:49:00 christos Exp $	*/

/*
 * Copyright (c) 1999 and 2000
 *       HAYAKAWA Koichi.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cardslot.c,v 1.55 2013/10/12 16:49:00 christos Exp $");

#include "opt_cardslot.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/kthread.h>

#include <sys/bus.h>

#include <dev/cardbus/cardslotvar.h>
#include <dev/cardbus/cardbusvar.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciachip.h>

#include "locators.h"

#if defined CARDSLOT_DEBUG
#define STATIC
#define DPRINTF(a) printf a
#else
#define STATIC static
#define DPRINTF(a)
#endif



STATIC void cardslotchilddet(device_t, device_t);
STATIC void cardslotattach(device_t, device_t, void *);
STATIC int cardslotdetach(device_t, int);

STATIC int cardslotmatch(device_t, cfdata_t, void *);
static void cardslot_event_thread(void *arg);

STATIC int cardslot_cb_print(void *aux, const char *pcic);
static int cardslot_16_print(void *, const char *);
static int cardslot_16_submatch(device_t, cfdata_t, const int *, void *);

CFATTACH_DECL3_NEW(cardslot, sizeof(struct cardslot_softc),
    cardslotmatch, cardslotattach, cardslotdetach, NULL, NULL, cardslotchilddet,
    DVF_DETACH_SHUTDOWN);

STATIC int
cardslotmatch(device_t parent, cfdata_t cf,
    void *aux)
{
	struct cardslot_attach_args *caa = aux;

	if (caa->caa_cb_attach == NULL && caa->caa_16_attach == NULL) {
		/* Neither CardBus nor 16-bit PCMCIA are defined. */
		return 0;
	}

	return 1;
}

STATIC void
cardslotchilddet(device_t self, device_t child)
{
	struct cardslot_softc *sc = device_private(self);

	KASSERT(sc->sc_cb_softc == device_private(child) ||
	    sc->sc_16_softc == child);

	if (sc->sc_cb_softc == device_private(child))
		sc->sc_cb_softc = NULL;
	else if (sc->sc_16_softc == child)
		sc->sc_16_softc = NULL;
}

STATIC void
cardslotattach(device_t parent, device_t self, void *aux)
{
	struct cardslot_softc *sc = device_private(self);
	struct cardslot_attach_args *caa = aux;

	struct cbslot_attach_args *cba = caa->caa_cb_attach;
	struct pcmciabus_attach_args *pa = caa->caa_16_attach;

	struct cardbus_softc *csc = NULL;
	struct pcmcia_softc *psc = NULL;

	sc->sc_dev = self;

	sc->sc_cb_softc = NULL;
	sc->sc_16_softc = NULL;
	SIMPLEQ_INIT(&sc->sc_events);
	sc->sc_th_enable = 0;

	aprint_naive("\n");
	aprint_normal("\n");

	DPRINTF(("%s attaching CardBus bus...\n", device_xname(self)));
	if (cba != NULL) {
		csc = device_private(config_found_ia(self, "cbbus", cba,
				     cardslot_cb_print));
		if (csc) {
			/* cardbus found */
			DPRINTF(("%s: found cardbus on %s\n", __func__,
				 device_xname(self)));
			sc->sc_cb_softc = csc;
		}
	}

	if (pa != NULL) {
		sc->sc_16_softc = config_found_sm_loc(self, "pcmciabus", NULL,
						      pa, cardslot_16_print,
						      cardslot_16_submatch);
		if (sc->sc_16_softc) {
			/* pcmcia 16-bit bus found */
			DPRINTF(("%s: found 16-bit pcmcia bus\n", __func__));
			psc = device_private(sc->sc_16_softc);
		}
	}

	if (csc != NULL || psc != NULL) {
		config_pending_incr(self);
		if (kthread_create(PRI_NONE, 0, NULL, cardslot_event_thread,
		    sc, &sc->sc_event_thread, "%s", device_xname(self))) {
			aprint_error_dev(sc->sc_dev,
					 "unable to create thread\n");
			panic("cardslotattach");
		}
		sc->sc_th_enable = 1;
	}

	if (csc && (csc->sc_cf->cardbus_ctrl)(csc->sc_cc, CARDBUS_CD)) {
		DPRINTF(("%s: CardBus card found\n", __func__));
		/* attach deferred */
		cardslot_event_throw(sc, CARDSLOT_EVENT_INSERTION_CB);
	}

	if (psc && (psc->pct->card_detect)(psc->pch)) {
		DPRINTF(("%s: 16-bit card found\n", __func__));
		/* attach deferred */
		cardslot_event_throw(sc, CARDSLOT_EVENT_INSERTION_16);
	}

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

STATIC int
cardslotdetach(device_t self, int flags)
{
	int rc;
	struct cardslot_softc *sc = device_private(self);

	if ((rc = config_detach_children(self, flags)) != 0)
		return rc;

	sc->sc_th_enable = 0;
	wakeup(&sc->sc_events);
	while (sc->sc_event_thread != NULL)
		(void)tsleep(sc, PWAIT, "cardslotthd", 0);

	if (!SIMPLEQ_EMPTY(&sc->sc_events))
		aprint_error_dev(self, "events outstanding");

	pmf_device_deregister(self);
	return 0;
}

STATIC int
cardslot_cb_print(void *aux, const char *pnp)
{
	struct cbslot_attach_args *cba = aux;

	if (pnp != NULL) {
		aprint_normal("cardbus at %s subordinate bus %d",
		    pnp, cba->cba_bus);
	}

	return UNCONF;
}


static int
cardslot_16_submatch(device_t parent, cfdata_t cf,
    const int *ldesc, void *aux)
{

	if (cf->cf_loc[PCMCIABUSCF_CONTROLLER] != PCMCIABUSCF_CONTROLLER_DEFAULT
	    && cf->cf_loc[PCMCIABUSCF_CONTROLLER] != 0) {
		return 0;
	}

	if (cf->cf_loc[PCMCIABUSCF_CONTROLLER] == PCMCIABUSCF_CONTROLLER_DEFAULT) {
		return (config_match(parent, cf, aux));
	}

	return 0;
}



static int
cardslot_16_print(void *arg, const char *pnp)
{

	if (pnp != NULL) {
		aprint_normal("pcmciabus at %s", pnp);
	}

	return UNCONF;
}


/*
 * void cardslot_event_throw(struct cardslot_softc *sc, int ev)
 *
 *   This function throws an event to the event handler.  If the state
 *   of a slot is changed, it should be noticed using this function.
 */
void
cardslot_event_throw(struct cardslot_softc *sc, int ev)
{
	struct cardslot_event *ce;

	DPRINTF(("cardslot_event_throw: an event %s comes\n",
	    ev == CARDSLOT_EVENT_INSERTION_CB ? "CardBus Card inserted" :
	    ev == CARDSLOT_EVENT_INSERTION_16 ? "16-bit Card inserted" :
	    ev == CARDSLOT_EVENT_REMOVAL_CB ? "CardBus Card removed" :
	    ev == CARDSLOT_EVENT_REMOVAL_16 ? "16-bit Card removed" : "???"));

	if (NULL == (ce = (struct cardslot_event *)malloc(sizeof (struct cardslot_event), M_TEMP, M_NOWAIT))) {
		panic("cardslot_enevt");
	}

	ce->ce_type = ev;

	{
		int s = spltty();
		SIMPLEQ_INSERT_TAIL(&sc->sc_events, ce, ce_q);
		splx(s);
	}

	wakeup(&sc->sc_events);

	return;
}


/*
 * static void cardslot_event_thread(void *arg)
 *
 *   This function is the main routine handing cardslot events such as
 *   insertions and removals.
 *
 */
static void
cardslot_event_thread(void *arg)
{
	struct cardslot_softc *sc = arg;
	struct cardslot_event *ce;
	int s, first = 1;
	static int antonym_ev[4] = {
		CARDSLOT_EVENT_REMOVAL_16, CARDSLOT_EVENT_INSERTION_16,
		CARDSLOT_EVENT_REMOVAL_CB, CARDSLOT_EVENT_INSERTION_CB
	};

	while (sc->sc_th_enable) {
		s = spltty();
		if ((ce = SIMPLEQ_FIRST(&sc->sc_events)) == NULL) {
			splx(s);
			if (first) {
				first = 0;
				config_pending_decr(sc->sc_dev);
			}
			(void) tsleep(&sc->sc_events, PWAIT, "cardslotev", 0);
			continue;
		}
		SIMPLEQ_REMOVE_HEAD(&sc->sc_events, ce_q);
		splx(s);

		if (IS_CARDSLOT_INSERT_REMOVE_EV(ce->ce_type)) {
			/* Chattering suppression */
			s = spltty();
			while (1) {
				struct cardslot_event *ce1, *ce2;

				if ((ce1 = SIMPLEQ_FIRST(&sc->sc_events)) == NULL) {
					break;
				}
				if (ce1->ce_type != antonym_ev[ce->ce_type]) {
					break;
				}
				if ((ce2 = SIMPLEQ_NEXT(ce1, ce_q)) == NULL) {
					break;
				}
				if (ce2->ce_type == ce->ce_type) {
					SIMPLEQ_REMOVE_HEAD(&sc->sc_events,
					    ce_q);
					free(ce1, M_TEMP);
					SIMPLEQ_REMOVE_HEAD(&sc->sc_events,
					    ce_q);
					free(ce2, M_TEMP);
				}
			}
			splx(s);
		}

		switch (ce->ce_type) {
		case CARDSLOT_EVENT_INSERTION_CB:
			if ((CARDSLOT_CARDTYPE(sc->sc_status) == CARDSLOT_STATUS_CARD_CB)
			    || (CARDSLOT_CARDTYPE(sc->sc_status) == CARDSLOT_STATUS_CARD_16)) {
				if (CARDSLOT_WORK(sc->sc_status) == CARDSLOT_STATUS_WORKING) {
					/*
					 * A card has already been
					 * inserted and works.
					 */
					break;
				}
			}

			if (sc->sc_cb_softc) {
				CARDSLOT_SET_CARDTYPE(sc->sc_status,
				    CARDSLOT_STATUS_CARD_CB);
				if (cardbus_attach_card(sc->sc_cb_softc) > 0) {
					/* at least one function works */
					CARDSLOT_SET_WORK(sc->sc_status, CARDSLOT_STATUS_WORKING);
				} else {
					/*
					 * no functions work or this
					 * card is not known
					 */
					CARDSLOT_SET_WORK(sc->sc_status,
					    CARDSLOT_STATUS_NOTWORK);
				}
			} else {
				panic("no cardbus on %s",
				      device_xname(sc->sc_dev));
			}

			break;

		case CARDSLOT_EVENT_INSERTION_16:
			if ((CARDSLOT_CARDTYPE(sc->sc_status) == CARDSLOT_STATUS_CARD_CB)
			    || (CARDSLOT_CARDTYPE(sc->sc_status) == CARDSLOT_STATUS_CARD_16)) {
				if (CARDSLOT_WORK(sc->sc_status) == CARDSLOT_STATUS_WORKING) {
					/*
					 * A card has already been
					 * inserted and work.
					 */
					break;
				}
			}
			if (sc->sc_16_softc) {
				CARDSLOT_SET_CARDTYPE(sc->sc_status, CARDSLOT_STATUS_CARD_16);
				if (pcmcia_card_attach(sc->sc_16_softc)) {
					/* Do not attach */
					CARDSLOT_SET_WORK(sc->sc_status,
					    CARDSLOT_STATUS_NOTWORK);
				} else {
					/* working */
					CARDSLOT_SET_WORK(sc->sc_status,
					    CARDSLOT_STATUS_WORKING);
				}
			} else {
				panic("no 16-bit pcmcia on %s",
				      device_xname(sc->sc_dev));
			}

			break;

		case CARDSLOT_EVENT_REMOVAL_CB:
			if (CARDSLOT_CARDTYPE(sc->sc_status) == CARDSLOT_STATUS_CARD_CB) {
				/* CardBus card has not been inserted. */
				if (CARDSLOT_WORK(sc->sc_status) == CARDSLOT_STATUS_WORKING) {
					cardbus_detach_card(sc->sc_cb_softc);
					CARDSLOT_SET_WORK(sc->sc_status,
					    CARDSLOT_STATUS_NOTWORK);
					CARDSLOT_SET_WORK(sc->sc_status,
					    CARDSLOT_STATUS_CARD_NONE);
				}
				CARDSLOT_SET_CARDTYPE(sc->sc_status,
				    CARDSLOT_STATUS_CARD_NONE);
			} else if (CARDSLOT_CARDTYPE(sc->sc_status) != CARDSLOT_STATUS_CARD_16) {
				/* Unknown card... */
				CARDSLOT_SET_CARDTYPE(sc->sc_status,
				    CARDSLOT_STATUS_CARD_NONE);
			}
			CARDSLOT_SET_WORK(sc->sc_status,
			    CARDSLOT_STATUS_NOTWORK);
			break;

		case CARDSLOT_EVENT_REMOVAL_16:
			DPRINTF(("%s: removal event\n", device_xname(sc->sc_dev)));
			if (CARDSLOT_CARDTYPE(sc->sc_status) != CARDSLOT_STATUS_CARD_16) {
				/* 16-bit card has not been inserted. */
				break;
			}
			if ((sc->sc_16_softc != NULL)
			    && (CARDSLOT_WORK(sc->sc_status) == CARDSLOT_STATUS_WORKING)) {
				struct pcmcia_softc *psc =
					device_private(sc->sc_16_softc);

				pcmcia_card_deactivate(sc->sc_16_softc);
				pcmcia_chip_socket_disable(psc->pct, psc->pch);
				pcmcia_card_detach(sc->sc_16_softc,
						   DETACH_FORCE);
			}
			CARDSLOT_SET_CARDTYPE(sc->sc_status, CARDSLOT_STATUS_CARD_NONE);
			CARDSLOT_SET_WORK(sc->sc_status, CARDSLOT_STATUS_NOTWORK);
			break;

		default:
			panic("cardslot_event_thread: unknown event %d", ce->ce_type);
		}
		free(ce, M_TEMP);
	}

	sc->sc_event_thread = NULL;

	/* In case the parent device is waiting for us to exit. */
	wakeup(sc);

	kthread_exit(0);
}

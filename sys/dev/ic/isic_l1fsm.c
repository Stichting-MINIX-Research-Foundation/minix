/*
 * Copyright (c) 1997, 2000 Hellmuth Michaelis. All rights reserved.
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
 *	i4b_l1fsm.c - isdn4bsd layer 1 I.430 state machine
 *	--------------------------------------------------
 *
 *	$Id: isic_l1fsm.c,v 1.14 2011/07/17 20:54:51 joerg Exp $
 *
 *      last edit-date: [Fri Jan  5 11:36:11 2001]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: isic_l1fsm.c,v 1.14 2011/07/17 20:54:51 joerg Exp $");

#include <sys/param.h>
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <sys/ioccom.h>
#else
#include <sys/ioctl.h>
#endif
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>

#ifdef __FreeBSD__
#include <machine/clock.h>
#include <i386/isa/isa_device.h>
#else
#ifndef __bsdi__
#include <sys/bus.h>
#endif
#include <sys/device.h>
#endif

#include <sys/socket.h>
#include <net/if.h>

#if defined(__NetBSD__) && __NetBSD_Version__ >= 104230000
#include <sys/callout.h>
#endif

#ifdef __FreeBSD__
#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#else
#include <netisdn/i4b_debug.h>
#include <netisdn/i4b_ioctl.h>
#endif

#include <netisdn/i4b_global.h>
#include <netisdn/i4b_trace.h>
#include <netisdn/i4b_l2.h>
#include <netisdn/i4b_l1l2.h>
#include <netisdn/i4b_mbuf.h>

#include <dev/ic/isic_l1.h>
#include <dev/ic/isac.h>
#include <dev/ic/hscx.h>

#include "nisac.h"
#include "nisacsx.h"

#if DO_I4B_DEBUG
static const char *state_text[N_STATES] = {
	"F3 Deactivated",
	"F4 Awaiting Signal",
	"F5 Identifying Input",
	"F6 Synchronized",
	"F7 Activated",
	"F8 Lost Framing",
	"Illegal State"
};

static const char *event_text[N_EVENTS] = {
	"EV_PHAR PH_ACT_REQ",
	"EV_T3 Timer 3 expired",
	"EV_INFO0 INFO0 received",
	"EV_RSY Level Detected",
	"EV_INFO2 INFO2 received",
	"EV_INFO48 INFO4 received",
	"EV_INFO410 INFO4 received",
	"EV_DR Deactivate Req",
	"EV_PU Power UP",
	"EV_DIS Disconnected",
	"EV_EI Error Ind",
	"Illegal Event"
};
#endif

/* Function prototypes */

static void timer3_expired (struct isic_softc *sc);
static void T3_start (struct isic_softc *sc);
static void T3_stop (struct isic_softc *sc);
static void F_T3ex (struct isic_softc *sc);
static void timer4_expired (struct isic_softc *sc);
static void T4_start (struct isic_softc *sc);
static void T4_stop (struct isic_softc *sc);
static void F_AI8 (struct isic_softc *sc);
static void F_AI10 (struct isic_softc *sc);
static void F_I01 (struct isic_softc *sc);
static void F_I02 (struct isic_softc *sc);
static void F_I03 (struct isic_softc *sc);
static void F_I2 (struct isic_softc *sc);
static void F_ill (struct isic_softc *sc);
static void F_NULL (struct isic_softc *sc);

/*---------------------------------------------------------------------------*
 *	I.430 Timer T3 expire function
 *---------------------------------------------------------------------------*/
static void
timer3_expired(struct isic_softc *sc)
{
	if(sc->sc_I430T3)
	{
		NDBGL1(L1_T_ERR, "state = %s", isic_printstate(sc));
		sc->sc_I430T3 = 0;

		/* XXX try some recovery here XXX */
		switch(sc->sc_cardtyp) {
#if NNISACSX > 0
			case CARD_TYPEP_AVMA1PCIV2:
				isic_isacsx_recover(sc);
				break;
#endif /* NNISACSX > 0 */
			default:
#if NNISAC > 0
				isic_recover(sc);
#endif /* NNISAC > 0 */
				break;
		}

		sc->sc_init_tries++;	/* increment retry count */

/*XXX*/		if(sc->sc_init_tries > 4)
		{
			int s = splnet();

			sc->sc_init_tries = 0;

			if(sc->sc_obuf2 != NULL)
			{
				i4b_Dfreembuf(sc->sc_obuf2);
				sc->sc_obuf2 = NULL;
			}
			if(sc->sc_obuf != NULL)
			{
				i4b_Dfreembuf(sc->sc_obuf);
				sc->sc_obuf = NULL;
				sc->sc_freeflag = 0;
				sc->sc_op = NULL;
				sc->sc_ol = 0;
			}

			splx(s);

			isdn_layer2_status_ind(&sc->sc_l2, sc->sc_l3token, STI_NOL1ACC, 0);
		}

		isic_next_state(sc, EV_T3);
	}
	else
	{
		NDBGL1(L1_T_ERR, "expired without starting it ....");
	}
}

/*---------------------------------------------------------------------------*
 *	I.430 Timer T3 start
 *---------------------------------------------------------------------------*/
static void
T3_start(struct isic_softc *sc)
{
	NDBGL1(L1_T_MSG, "state = %s", isic_printstate(sc));
	sc->sc_I430T3 = 1;

	START_TIMER(sc->sc_T3_callout, timer3_expired, sc, 2*hz);
}

/*---------------------------------------------------------------------------*
 *	I.430 Timer T3 stop
 *---------------------------------------------------------------------------*/
static void
T3_stop(struct isic_softc *sc)
{
	NDBGL1(L1_T_MSG, "state = %s", isic_printstate(sc));

	sc->sc_init_tries = 0;	/* init connect retry count */

	if(sc->sc_I430T3)
	{
		sc->sc_I430T3 = 0;
		STOP_TIMER(sc->sc_T3_callout, timer3_expired, sc);
	}
}

/*---------------------------------------------------------------------------*
 *	I.430 Timer T3 expiry
 *---------------------------------------------------------------------------*/
static void
F_T3ex(struct isic_softc *sc)
{
	NDBGL1(L1_F_MSG, "FSM function F_T3ex executing");
	if(((struct isdn_l3_driver*)sc->sc_l3token)->protocol != PROTOCOL_D64S)
		isdn_layer2_activate_ind(&sc->sc_l2, sc->sc_l3token, 0);
}

/*---------------------------------------------------------------------------*
 *	Timer T4 expire function
 *---------------------------------------------------------------------------*/
static void
timer4_expired(struct isic_softc *sc)
{
	if(sc->sc_I430T4)
	{
		NDBGL1(L1_T_MSG, "state = %s", isic_printstate(sc));
		sc->sc_I430T4 = 0;
		isdn_layer2_status_ind(&sc->sc_l2, sc->sc_l3token, STI_PDEACT, 0);
	}
	else
	{
		NDBGL1(L1_T_ERR, "expired without starting it ....");
	}
}

/*---------------------------------------------------------------------------*
 *	Timer T4 start
 *---------------------------------------------------------------------------*/
static void
T4_start(struct isic_softc *sc)
{
	NDBGL1(L1_T_MSG, "state = %s", isic_printstate(sc));
	sc->sc_I430T4 = 1;

	START_TIMER(sc->sc_T4_callout, timer4_expired, sc, hz);
}

/*---------------------------------------------------------------------------*
 *	Timer T4 stop
 *---------------------------------------------------------------------------*/
static void
T4_stop(struct isic_softc *sc)
{
	NDBGL1(L1_T_MSG, "state = %s", isic_printstate(sc));

	if(sc->sc_I430T4)
	{
		sc->sc_I430T4 = 0;
		STOP_TIMER(sc->sc_T4_callout, timer4_expired, sc);
	}
}

/*---------------------------------------------------------------------------*
 *	FSM function: received AI8
 *---------------------------------------------------------------------------*/
static void
F_AI8(struct isic_softc *sc)
{
	T4_stop(sc);

	NDBGL1(L1_F_MSG, "FSM function F_AI8 executing");

	if(((struct isdn_l3_driver*)sc->sc_l3token)->protocol != PROTOCOL_D64S)
		isdn_layer2_activate_ind(&sc->sc_l2, sc->sc_l3token, 1);

	T3_stop(sc);

	if(sc->sc_trace & TRACE_I)
	{
		i4b_trace_hdr hdr;
		char info = INFO4_8;

		hdr.type = TRC_CH_I;
		hdr.dir = FROM_NT;
		hdr.count = 0;
		isdn_layer2_trace_ind(&sc->sc_l2, sc->sc_l3token, &hdr, 1, &info);
	}
}

/*---------------------------------------------------------------------------*
 *	FSM function: received AI10
 *---------------------------------------------------------------------------*/
static void
F_AI10(struct isic_softc *sc)
{
	T4_stop(sc);

	NDBGL1(L1_F_MSG, "FSM function F_AI10 executing");

	if(((struct isdn_l3_driver*)sc->sc_l3token)->protocol != PROTOCOL_D64S)
		isdn_layer2_activate_ind(&sc->sc_l2, sc->sc_l3token, 1);

	T3_stop(sc);

	if(sc->sc_trace & TRACE_I)
	{
		i4b_trace_hdr hdr;
		char info = INFO4_10;

		hdr.type = TRC_CH_I;
		hdr.dir = FROM_NT;
		hdr.count = 0;
		isdn_layer2_trace_ind(&sc->sc_l2, sc->sc_l3token, &hdr, 1, &info);
	}
}

/*---------------------------------------------------------------------------*
 *	FSM function: received INFO 0 in states F3 .. F5
 *---------------------------------------------------------------------------*/
static void
F_I01(struct isic_softc *sc)
{
	NDBGL1(L1_F_MSG, "FSM function F_I01 executing");

	if(sc->sc_trace & TRACE_I)
	{
		i4b_trace_hdr hdr;
		char info = INFO0;

		hdr.type = TRC_CH_I;
		hdr.dir = FROM_NT;
		hdr.count = 0;
		isdn_layer2_trace_ind(&sc->sc_l2, sc->sc_l3token, &hdr, 1, &info);
	}
}

/*---------------------------------------------------------------------------*
 *	FSM function: received INFO 0 in state F6
 *---------------------------------------------------------------------------*/
static void
F_I02(struct isic_softc *sc)
{
	NDBGL1(L1_F_MSG, "FSM function F_I02 executing");

	if(((struct isdn_l3_driver*)sc->sc_l3token)->protocol != PROTOCOL_D64S)
		isdn_layer2_activate_ind(&sc->sc_l2, sc->sc_l3token, 0);

	if(sc->sc_trace & TRACE_I)
	{
		i4b_trace_hdr hdr;
		char info = INFO0;

		hdr.type = TRC_CH_I;
		hdr.dir = FROM_NT;
		hdr.count = 0;
		isdn_layer2_trace_ind(&sc->sc_l2, sc->sc_l3token, &hdr, 1, &info);
	}
}

/*---------------------------------------------------------------------------*
 *	FSM function: received INFO 0 in state F7 or F8
 *---------------------------------------------------------------------------*/
static void
F_I03(struct isic_softc *sc)
{
	NDBGL1(L1_F_MSG, "FSM function F_I03 executing");

	if(((struct isdn_l3_driver*)sc->sc_l3token)->protocol != PROTOCOL_D64S)
		isdn_layer2_activate_ind(&sc->sc_l2, sc->sc_l3token, 0);

	T4_start(sc);

	if(sc->sc_trace & TRACE_I)
	{
		i4b_trace_hdr hdr;
		char info = INFO0;

		hdr.type = TRC_CH_I;
		hdr.dir = FROM_NT;
		hdr.count = 0;
		isdn_layer2_trace_ind(&sc->sc_l2, sc->sc_l3token, &hdr, 1, &info);
	}
}

/*---------------------------------------------------------------------------*
 *	FSM function: activate request
 *---------------------------------------------------------------------------*/
static void
F_AR(struct isic_softc *sc)
{
	NDBGL1(L1_F_MSG, "FSM function F_AR executing");

	if(sc->sc_trace & TRACE_I)
	{
		i4b_trace_hdr hdr;
		char info = INFO1_8;

		hdr.type = TRC_CH_I;
		hdr.dir = FROM_TE;
		hdr.count = 0;
		isdn_layer2_trace_ind(&sc->sc_l2, sc->sc_l3token, &hdr, 1, &info);
	}

	switch(sc->sc_cardtyp) {
#if NNISACSX > 0
		case CARD_TYPEP_AVMA1PCIV2:
			isic_isacsx_l1_cmd(sc, CMD_AR8);
			break;
#endif /* NNISACSX > 0 */
		default:
#if NNISAC > 0
			isic_isac_l1_cmd(sc, CMD_AR8);
#endif /* NNISAC > 0 */
			break;
	}

	T3_start(sc);
}

/*---------------------------------------------------------------------------*
 *	FSM function: received INFO2
 *---------------------------------------------------------------------------*/
static void
F_I2(struct isic_softc *sc)
{
	NDBGL1(L1_F_MSG, "FSM function F_I2 executing");

	if(sc->sc_trace & TRACE_I)
	{
		i4b_trace_hdr hdr;
		char info = INFO2;

		hdr.type = TRC_CH_I;
		hdr.dir = FROM_NT;
		hdr.count = 0;
		isdn_layer2_trace_ind(&sc->sc_l2, sc->sc_l3token, &hdr, 1, &info);
	}
}

/*---------------------------------------------------------------------------*
 *	illegal state default action
 *---------------------------------------------------------------------------*/
static void
F_ill(struct isic_softc *sc)
{
	NDBGL1(L1_F_ERR, "FSM function F_ill executing");
}

/*---------------------------------------------------------------------------*
 *	No action
 *---------------------------------------------------------------------------*/
static void
F_NULL(struct isic_softc *sc)
{
	NDBGL1(L1_F_MSG, "FSM function F_NULL executing");
}


/*---------------------------------------------------------------------------*
 *	layer 1 state transition table
 *---------------------------------------------------------------------------*/
struct isic_state_tab {
	void (*func) (struct isic_softc *sc);	/* function to execute */
	int newstate;				/* next state */
} isic_state_tab[N_EVENTS][N_STATES] = {

/* STATE:	F3			F4			F5			F6			F7			F8			ILLEGAL STATE     */
/* -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* EV_PHAR x*/	{{F_AR,   ST_F4},	{F_NULL, ST_F4},	{F_NULL, ST_F5},	{F_NULL, ST_F6},	{F_ill,  ST_ILL},	{F_NULL, ST_F8},	{F_ill, ST_ILL}},
/* EV_T3   x*/	{{F_NULL, ST_F3},	{F_T3ex, ST_F3},	{F_T3ex, ST_F3},	{F_T3ex, ST_F3},	{F_NULL, ST_F7},	{F_NULL, ST_F8},	{F_ill, ST_ILL}},
/* EV_INFO0 */	{{F_I01,  ST_F3},	{F_I01,  ST_F4},	{F_I01,  ST_F5},	{F_I02,  ST_F3},	{F_I03,  ST_F3},	{F_I03,  ST_F3},	{F_ill, ST_ILL}},
/* EV_RSY  x*/	{{F_NULL, ST_F3},	{F_NULL, ST_F5},	{F_NULL, ST_F5}, 	{F_NULL, ST_F8},	{F_NULL, ST_F8},	{F_NULL, ST_F8},	{F_ill, ST_ILL}},
/* EV_INFO2 */	{{F_I2,   ST_F6},	{F_I2,   ST_F6},	{F_I2,   ST_F6},	{F_I2,   ST_F6},	{F_I2,   ST_F6},	{F_I2,   ST_F6},	{F_ill, ST_ILL}},
/* EV_INFO48*/	{{F_AI8,  ST_F7},	{F_AI8,  ST_F7},	{F_AI8,  ST_F7},	{F_AI8,  ST_F7},	{F_NULL, ST_F7},	{F_AI8,  ST_F7},	{F_ill, ST_ILL}},
/* EV_INFO41*/	{{F_AI10, ST_F7},	{F_AI10, ST_F7},	{F_AI10, ST_F7},	{F_AI10, ST_F7},	{F_NULL, ST_F7},	{F_AI10, ST_F7},	{F_ill, ST_ILL}},
/* EV_DR    */	{{F_NULL, ST_F3},	{F_NULL, ST_F4},	{F_NULL, ST_F5},	{F_NULL, ST_F6},	{F_NULL, ST_F7},	{F_NULL, ST_F8},	{F_ill, ST_ILL}},
/* EV_PU    */	{{F_NULL, ST_F3},	{F_NULL, ST_F4},	{F_NULL, ST_F5},	{F_NULL, ST_F6},	{F_NULL, ST_F7},	{F_NULL, ST_F8},	{F_ill, ST_ILL}},
/* EV_DIS   */	{{F_ill,  ST_ILL},	{F_ill,  ST_ILL},	{F_ill,  ST_ILL},	{F_ill,  ST_ILL},	{F_ill,  ST_ILL},	{F_ill,  ST_ILL},	{F_ill, ST_ILL}},
/* EV_EI    */	{{F_NULL, ST_F3},	{F_NULL, ST_F3},	{F_NULL, ST_F3},	{F_NULL, ST_F3},	{F_NULL, ST_F3},	{F_NULL, ST_F3},	{F_ill, ST_ILL}},
/* EV_ILL   */	{{F_ill,  ST_ILL},	{F_ill,  ST_ILL},	{F_ill,  ST_ILL},	{F_ill,  ST_ILL},	{F_ill,  ST_ILL},	{F_ill,  ST_ILL},	{F_ill, ST_ILL}}
};

/*---------------------------------------------------------------------------*
 *	event handler
 *---------------------------------------------------------------------------*/
void
isic_next_state(struct isic_softc *sc, int event)
{
	int currstate, newstate;

	if(event >= N_EVENTS)
		panic("i4b_l1fsm.c: event >= N_EVENTS");

	currstate = sc->sc_I430state;

	if(currstate >= N_STATES)
		panic("i4b_l1fsm.c: currstate >= N_STATES");

	newstate = isic_state_tab[event][currstate].newstate;

	if(newstate >= N_STATES)
		panic("i4b_l1fsm.c: newstate >= N_STATES");

	NDBGL1(L1_F_MSG, "FSM event [%s]: [%s => %s]", event_text[event],
                                           state_text[currstate],
                                           state_text[newstate]);

        (*isic_state_tab[event][currstate].func)(sc);

	if(newstate == ST_ILL)
	{
		newstate = ST_F3;
		NDBGL1(L1_F_ERR, "FSM Illegal State ERROR, oldstate = %s, newstate = %s, event = %s!",
					state_text[currstate],
					state_text[newstate],
					event_text[event]);
	}

	sc->sc_I430state = newstate;
}

#if DO_I4B_DEBUG
/*---------------------------------------------------------------------------*
 *	return pointer to current state description
 *---------------------------------------------------------------------------*/
const char *
isic_printstate(struct isic_softc *sc)
{
	return(state_text[sc->sc_I430state]);
}
#endif

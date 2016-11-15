/* $NetBSD: isic_l1.c,v 1.19 2012/10/27 17:18:21 chs Exp $ */

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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: isic_l1.c,v 1.19 2012/10/27 17:18:21 chs Exp $");

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>

#include <sys/bus.h>
#include <sys/device.h>

#include <sys/socket.h>
#include <net/if.h>

#include <sys/callout.h>

#include <netisdn/i4b_debug.h>
#include <netisdn/i4b_ioctl.h>
#include <netisdn/i4b_trace.h>

#include <netisdn/i4b_l2.h>
#include <netisdn/i4b_l1l2.h>
#include <netisdn/i4b_mbuf.h>
#include <netisdn/i4b_global.h>

#include <dev/ic/isic_l1.h>
#include <dev/ic/isac.h>
#include <dev/ic/ipac.h>
#include <dev/ic/hscx.h>

#include "nisac.h"
#include "nisacsx.h"

unsigned int i4b_l1_debug = L1_DEBUG_DEFAULT;

static int isic_std_ph_data_req(isdn_layer1token, struct mbuf *, int);
static int isic_std_ph_activate_req(isdn_layer1token);
static int isic_std_mph_command_req(isdn_layer1token, int, void*);
static void isic_enable_intr(struct isic_softc *sc, int enable);

const struct isdn_layer1_isdnif_driver isic_std_driver = {
	isic_std_ph_data_req,
	isic_std_ph_activate_req,
	isic_std_mph_command_req
};

/* from i4btrc driver i4b_trace.c */
extern int get_trace_data_from_l1(int unit, int what, int len, char *buf);

/*---------------------------------------------------------------------------*
 *
 *	L2 -> L1: PH-DATA-REQUEST
 *	=========================
 *
 *	parms:
 *		token		softc of physical driver
 *		m		mbuf containing L2 frame to be sent out
 *		freeflag	MBUF_FREE: free mbuf here after having sent
 *						it out
 *				MBUF_DONTFREE: mbuf is freed by Layer 2
 *	returns:
 *		==0	fail, nothing sent out
 *		!=0	ok, frame sent out
 *
 *---------------------------------------------------------------------------*/
static int
isic_std_ph_data_req(isdn_layer1token token, struct mbuf *m, int freeflag)
{
	struct isic_softc *sc = (struct isic_softc*)token;
	u_char cmd;
	int s;

	if (m == NULL)			/* failsafe */
		return (0);

	s = splnet();

	if(sc->sc_I430state == ST_F3)	/* layer 1 not running ? */
	{
		NDBGL1(L1_I_ERR, "still in state F3!");
		isic_std_ph_activate_req(token);
	}

	if(sc->sc_state & ISAC_TX_ACTIVE)
	{
		if(sc->sc_obuf2 == NULL)
		{
			sc->sc_obuf2 = m;		/* save mbuf ptr */

			if(freeflag)
				sc->sc_freeflag2 = 1;	/* IRQ must mfree */
			else
				sc->sc_freeflag2 = 0;	/* IRQ must not mfree */

			NDBGL1(L1_I_MSG, "using 2nd ISAC TX buffer, state = %s", isic_printstate(sc));

			if(sc->sc_trace & TRACE_D_TX)
			{
				i4b_trace_hdr hdr;
				hdr.type = TRC_CH_D;
				hdr.dir = FROM_TE;
				hdr.count = ++sc->sc_trace_dcount;
				isdn_layer2_trace_ind(&sc->sc_l2, sc->sc_l3token, &hdr, m->m_len, m->m_data);
			}
			splx(s);
			return(1);
		}

		NDBGL1(L1_I_ERR, "No Space in TX FIFO, state = %s", isic_printstate(sc));

		if(freeflag == MBUF_FREE)
			i4b_Dfreembuf(m);

		splx(s);
		return (0);
	}

	if(sc->sc_trace & TRACE_D_TX)
	{
		i4b_trace_hdr hdr;
		hdr.type = TRC_CH_D;
		hdr.dir = FROM_TE;
		hdr.count = ++sc->sc_trace_dcount;
		isdn_layer2_trace_ind(&sc->sc_l2, sc->sc_l3token, &hdr, m->m_len, m->m_data);
	}

	sc->sc_state |= ISAC_TX_ACTIVE;	/* set transmitter busy flag */

	NDBGL1(L1_I_MSG, "ISAC_TX_ACTIVE set");

	sc->sc_freeflag = 0;		/* IRQ must NOT mfree */

	ISAC_WRFIFO(m->m_data, min(m->m_len, ISAC_FIFO_LEN)); /* output to TX fifo */

	if(m->m_len > ISAC_FIFO_LEN)	/* message > 32 bytes ? */
	{
		sc->sc_obuf = m;	/* save mbuf ptr */
		sc->sc_op = m->m_data + ISAC_FIFO_LEN; 	/* ptr for irq hdl */
		sc->sc_ol = m->m_len - ISAC_FIFO_LEN;	/* length for irq hdl */

		if(freeflag)
			sc->sc_freeflag = 1;	/* IRQ must mfree */

		cmd = ISAC_CMDR_XTF;
	}
	else
	{
		sc->sc_obuf = NULL;
		sc->sc_op = NULL;
		sc->sc_ol = 0;

		if(freeflag)
			i4b_Dfreembuf(m);

		cmd = ISAC_CMDR_XTF | ISAC_CMDR_XME;
  	}

	ISAC_WRITE(I_CMDR, cmd);
	ISACCMDRWRDELAY();

	splx(s);

	return(1);
}

/*---------------------------------------------------------------------------*
 *
 *	L2 -> L1: PH-ACTIVATE-REQUEST
 *	=============================
 *
 *	parms:
 *		token		softc of physical interface
 *
 *	returns:
 *		==0
 *		!=0
 *
 *---------------------------------------------------------------------------*/
static int
isic_std_ph_activate_req(isdn_layer1token token)
{
	struct isic_softc *sc = (struct isic_softc*)token;

	NDBGL1(L1_PRIM, " %s ", device_xname(sc->sc_dev));
	isic_next_state(sc, EV_PHAR);
	return(0);
}

/*---------------------------------------------------------------------------*
 *	command from the upper layers
 *---------------------------------------------------------------------------*/
static int
isic_std_mph_command_req(isdn_layer1token token, int command, void *parm)
{
	struct isic_softc *sc = (struct isic_softc*)token;
	int s, pass_down = 0;

	s = splnet();
	switch(command)
	{
		case CMR_DOPEN:		/* daemon running */
			NDBGL1(L1_PRIM, "%s, command = CMR_DOPEN", device_xname(sc->sc_dev));
			sc->sc_intr_valid = ISIC_INTR_VALID;
			pass_down = 1;
			break;

		case CMR_DCLOSE:	/* daemon not running */
			NDBGL1(L1_PRIM, "%s, command = CMR_DCLOSE", device_xname(sc->sc_dev));
			sc->sc_intr_valid = ISIC_INTR_DISABLED;
			isic_enable_intr(sc, 0);
			pass_down = 1;
			break;

		case CMR_SETLEDS:
			pass_down = 1;
			break;

		case CMR_SETTRACE:
			NDBGL1(L1_PRIM, "%s, command = CMR_SETTRACE, parm = %p", device_xname(sc->sc_dev), parm);
			sc->sc_trace = (int)(unsigned long)parm;
			break;

		default:
			NDBGL1(L1_ERROR, "ERROR, unknown command = %d, %s, parm = %p", command, device_xname(sc->sc_dev), parm);
			break;
	}

	if (pass_down && sc->drv_command != NULL)
		sc->drv_command(sc, command, parm);

	if (command == CMR_DOPEN)
		isic_enable_intr(sc, 1);

	splx(s);

	return(0);
}

static void
isic_enable_intr(struct isic_softc *sc, int enable)
{
#if NNISACSX > 0
	if (sc->sc_cardtyp == CARD_TYPEP_AVMA1PCIV2) {
		if (enable)
			isic_isacsx_init(sc);
		else isic_isacsx_disable_intr(sc);
		return;
	}
#endif /* NNISACSX > 0 */

#if NNISAC > 0
	if (enable) {
			isic_isac_init(sc);
	} else {
		/* disable receiver */
		ISAC_WRITE(I_MODE, ISAC_MODE_MDS2|ISAC_MODE_MDS1|ISAC_MODE_DIM0);
		/* mask interrupts */
		if (sc->sc_ipac) {
			IPAC_WRITE(IPAC_MASK, 0xff);
		} else {
			ISAC_WRITE(I_MASK, 0xff);
		}
	}
#endif /* NNISAC > 0 */
}

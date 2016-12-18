/*	$NetBSD: iwic_dchan.c,v 1.9 2014/03/23 02:54:12 christos Exp $	*/

/*
 * Copyright (c) 1999, 2000 Dave Boyce. All rights reserved.
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
 *      i4b_iwic - isdn4bsd Winbond W6692 driver
 *      ----------------------------------------
 *
 * $FreeBSD$
 *
 *      last edit-date: [Tue Jan 16 13:20:14 2001]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: iwic_dchan.c,v 1.9 2014/03/23 02:54:12 christos Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/callout.h>
#include <net/if.h>

#include <sys/bus.h>

#include <dev/pci/pcivar.h>

#include <dev/pci/iwicreg.h>
#include <dev/pci/iwicvar.h>

#include <netisdn/i4b_global.h>
#include <netisdn/i4b_mbuf.h>

#define MAX_DFRAME_LEN	264

static void dchan_receive(struct iwic_softc *sc, int ista);

/*---------------------------------------------------------------------------*
 *	initialize D-channel variables and registers
 *---------------------------------------------------------------------------*/
void
iwic_dchan_init(struct iwic_softc *sc)
{
	sc->sc_dchan.ibuf = NULL;
	sc->sc_dchan.rx_count = 0;

	sc->sc_dchan.obuf = NULL;
	sc->sc_dchan.obuf2 = NULL;
	sc->sc_dchan.tx_count = 0;
	sc->sc_dchan.tx_ready = 0;

	IWIC_WRITE(sc, D_CTL, D_CTL_SRST);

	DELAY(5000);

	IWIC_WRITE(sc, D_CTL, 0);

	IWIC_WRITE(sc, SQX, SQX_SCIE);

	IWIC_WRITE(sc, PCTL, 0x00);
	IWIC_WRITE(sc, MOCR, 0x00);
	IWIC_WRITE(sc, GCR, 0x00);

	IWIC_WRITE(sc, D_CMDR, D_CMDR_RRST | D_CMDR_XRST);
	IWIC_WRITE(sc, D_MODE, D_MODE_RACT);

	IWIC_WRITE(sc, D_SAM, 0xff);
	IWIC_WRITE(sc, D_TAM, 0xff);

	IWIC_WRITE(sc, D_EXIM, 0x00);
}

/*---------------------------------------------------------------------------*
 *	Extended IRQ handler for the D-channel
 *---------------------------------------------------------------------------*/
void
iwic_dchan_xirq(struct iwic_softc *sc)
{
	int irq_stat;
	int stat;

	irq_stat = IWIC_READ(sc, D_EXIR);

	if (irq_stat & D_EXIR_RDOV)
	{
		NDBGL1(L1_I_ERR, "RDOV in state %s", iwic_printstate(sc));
		IWIC_WRITE(sc, D_CMDR, D_CMDR_RRST);
	}
	if (irq_stat & D_EXIR_XDUN)
	{
		NDBGL1(L1_I_ERR, "XDUN in state %s", iwic_printstate(sc));
		sc->sc_dchan.tx_ready = 0;
	}
	if (irq_stat & D_EXIR_XCOL)
	{
		NDBGL1(L1_I_ERR, "XCOL in state %s", iwic_printstate(sc));
		IWIC_WRITE(sc, D_CMDR, D_CMDR_XRST);
		sc->sc_dchan.tx_ready = 0;
	}
	if (irq_stat & D_EXIR_TIN2)
	{
		NDBGL1(L1_I_ERR, "TIN2 in state %s", iwic_printstate(sc));
	}
	if (irq_stat & D_EXIR_MOC)
	{
		stat = IWIC_READ(sc, MOR);
		NDBGL1(L1_I_ERR, "MOC in state %s, byte = 0x%x", iwic_printstate(sc), stat);
	}

	if (irq_stat & D_EXIR_ISC)
	{
		stat = (IWIC_READ(sc, CIR)) & 0x0f;

		switch (stat)
		{
			case CIR_CE:
				NDBGL1(L1_I_CICO, "rx CE in state %s", iwic_printstate(sc));
				iwic_next_state(sc, EV_CE);
				break;
			case CIR_DRD:
				NDBGL1(L1_I_CICO, "rx DRD in state %s", iwic_printstate(sc));
				iwic_next_state(sc, EV_INFO0);
				isdn_layer2_status_ind(&sc->sc_l2, sc->sc_l3token, STI_L1STAT, LAYER_IDLE);
				break;
			case CIR_LD:
				NDBGL1(L1_I_CICO, "rx LD in state %s", iwic_printstate(sc));
				iwic_next_state(sc, EV_RSY);
				break;
			case CIR_ARD:
				NDBGL1(L1_I_CICO, "rx ARD in state %s", iwic_printstate(sc));
				iwic_next_state(sc, EV_INFO2);
				break;
			case CIR_TI:
				NDBGL1(L1_I_CICO, "rx TI in state %s", iwic_printstate(sc));
				iwic_next_state(sc, EV_INFO0);
				break;
			case CIR_ATI:
				NDBGL1(L1_I_CICO, "rx ATI in state %s", iwic_printstate(sc));
				iwic_next_state(sc, EV_INFO0);
				break;
			case CIR_AI8:
				NDBGL1(L1_I_CICO, "rx AI8 in state %s", iwic_printstate(sc));
				isdn_layer2_status_ind(&sc->sc_l2, sc->sc_l3token, STI_L1STAT, LAYER_ACTIVE);
				iwic_next_state(sc, EV_INFO48);
				break;
			case CIR_AI10:
				NDBGL1(L1_I_CICO, "rx AI10 in state %s", iwic_printstate(sc));
				isdn_layer2_status_ind(&sc->sc_l2, sc->sc_l3token, STI_L1STAT, LAYER_ACTIVE);
				iwic_next_state(sc, EV_INFO410);
				break;
			case CIR_CD:
				NDBGL1(L1_I_CICO, "rx DIS in state %s", iwic_printstate(sc));
				iwic_next_state(sc, EV_DIS);
				break;
			default:
				NDBGL1(L1_I_ERR, "ERROR, unknown indication 0x%x in state %s", stat, iwic_printstate(sc));
				iwic_next_state(sc, EV_INFO0);
				break;
		}
	}

	if (irq_stat & D_EXIR_TEXP)
	{
		NDBGL1(L1_I_ERR, "TEXP in state %s", iwic_printstate(sc));
	}

	if (irq_stat & D_EXIR_WEXP)
	{
		NDBGL1(L1_I_ERR, "WEXP in state %s", iwic_printstate(sc));
	}
}

/*---------------------------------------------------------------------------*
 *	All receiving and transmitting takes place here.
 *---------------------------------------------------------------------------*/
void
iwic_dchan_xfer_irq(struct iwic_softc *sc, int ista)
{
	NDBGL1(L1_I_MSG, "ISTA = 0x%x", ista);

	if (ista & (ISTA_D_RMR | ISTA_D_RME))
	{
		/* Receive message ready */
		dchan_receive(sc, ista);
	}
	if (ista & ISTA_D_XFR)
	{
		/* Transmitter ready */
		sc->sc_dchan.tx_ready = 1;

		iwic_dchan_transmit(sc);
	}
}

/*---------------------------------------------------------------------------*
 *	disable D-channel
 *---------------------------------------------------------------------------*/
void
iwic_dchan_disable(struct iwic_softc *sc)
{
	int s;

	s = splnet();

	if (sc->sc_dchan.obuf)
	{
		if (sc->sc_dchan.free_obuf)
			i4b_Dfreembuf(sc->sc_dchan.obuf);
		sc->sc_dchan.obuf = NULL;
	}

	if (sc->sc_dchan.obuf2)
	{
		if (sc->sc_dchan.free_obuf2)
			i4b_Dfreembuf(sc->sc_dchan.obuf2);
		sc->sc_dchan.obuf2 = NULL;
	}

	splx(s);

	IWIC_WRITE(sc, CIX, CIX_DRC);
}

/*---------------------------------------------------------------------------*
 *	queue D-channel message for transmission
 *---------------------------------------------------------------------------*/
int
iwic_dchan_data_req(struct iwic_softc *sc, struct mbuf *m, int freeflag)
{
	int s;

	if (!m)
		return 0;

	s = splnet();

	/* Queue message */

	if (sc->sc_dchan.obuf)
	{
		if (sc->sc_dchan.obuf2)
		{
			NDBGL1(L1_I_ERR, "no buffer space!");
		}
		else
		{
			sc->sc_dchan.obuf2 = m;
			sc->sc_dchan.free_obuf2 = freeflag;
		}
	}
	else
	{
		sc->sc_dchan.obuf = m;
		sc->sc_dchan.obuf_ptr = m->m_data;
		sc->sc_dchan.obuf_len = m->m_len;
		sc->sc_dchan.free_obuf = freeflag;
	}

	iwic_dchan_transmit(sc);

	splx(s);

	return (0);
}

/*---------------------------------------------------------------------------*
 *	allocate an mbuf
 *---------------------------------------------------------------------------*/
static void
dchan_get_mbuf(struct iwic_softc *sc, int len)
{
	sc->sc_dchan.ibuf = i4b_Dgetmbuf(len);

	if (!sc->sc_dchan.ibuf)
		panic("dchan_get_mbuf: unable to allocate %d bytes for mbuf!", len);

	sc->sc_dchan.ibuf_ptr = sc->sc_dchan.ibuf->m_data;
	sc->sc_dchan.ibuf_max_len = sc->sc_dchan.ibuf->m_len;
	sc->sc_dchan.ibuf_len = 0;
}

/*---------------------------------------------------------------------------*
 *	D-channel receive data interrupt
 *---------------------------------------------------------------------------*/
static void
dchan_receive(struct iwic_softc *sc, int ista)
{
	int command = D_CMDR_RACK;

	if (ista & ISTA_D_RMR)
	{
		/* Got 64 bytes in FIFO */

		if (!sc->sc_dchan.ibuf)
		{
			dchan_get_mbuf(sc, MAX_DFRAME_LEN);

		}
		else if ((sc->sc_dchan.ibuf_len + MAX_DFRAME_LEN) >
			 sc->sc_dchan.ibuf_max_len)
		{
			panic("dchan_receive: not enough space in buffer!");
		}

		IWIC_RDDFIFO(sc, sc->sc_dchan.ibuf_ptr, 64);

		sc->sc_dchan.ibuf_ptr += 64;
		sc->sc_dchan.ibuf_len += 64;
		sc->sc_dchan.rx_count += 64;
	}
	if (ista & ISTA_D_RME)
	{
		/* Got end of frame */
		int status;

		status = IWIC_READ(sc, D_RSTA);

		if (status & (D_RSTA_RDOV | D_RSTA_CRCE | D_RSTA_RMB))
		{
			if (status & D_RSTA_RDOV)
				NDBGL1(L1_I_ERR, "%s: D-channel Receive Data Overflow", device_xname(sc->sc_dev));
			if (status & D_RSTA_CRCE)
				NDBGL1(L1_I_ERR, "%s: D-channel CRC Error", device_xname(sc->sc_dev));
			if (status & D_RSTA_RMB)
				NDBGL1(L1_I_ERR, "%s: D-channel Receive Message Aborted", device_xname(sc->sc_dev));
			command |= D_CMDR_RRST;
		}
		else
		{
			int lo;

			lo = IWIC_READ(sc, D_RBCL);
			(void)IWIC_READ(sc, D_RBCH);
			lo = lo & 0x3f;

			if (lo == 0)
				lo = IWIC_DCHAN_FIFO_LEN;

			if (!sc->sc_dchan.ibuf)
			{
				dchan_get_mbuf(sc, lo);
			}
			else if ((sc->sc_dchan.ibuf_len + lo) >
				 sc->sc_dchan.ibuf_max_len)
			{
				panic("dchan_receive: buffer not long enough");
			}

			IWIC_RDDFIFO(sc, sc->sc_dchan.ibuf_ptr, lo);
			sc->sc_dchan.ibuf_len += lo;
			sc->sc_dchan.rx_count += lo;

			sc->sc_dchan.ibuf->m_len = sc->sc_dchan.ibuf_len;

			if(sc->sc_trace & TRACE_D_RX)
			{
				i4b_trace_hdr hdr;
				hdr.type = TRC_CH_D;
				hdr.dir = FROM_NT;
				hdr.count = ++sc->sc_dchan.trace_count;
				isdn_layer2_trace_ind(&sc->sc_l2, sc->sc_l3token, &hdr, sc->sc_dchan.ibuf->m_len, sc->sc_dchan.ibuf->m_data);
			}
			isdn_layer2_data_ind(&sc->sc_l2,sc->sc_l3token,sc->sc_dchan.ibuf);

			sc->sc_dchan.ibuf = NULL;
		}
	}
	IWIC_WRITE(sc, D_CMDR, command);
}

/*---------------------------------------------------------------------------*
 *	transmit D-channel frame
 *---------------------------------------------------------------------------*/
void
iwic_dchan_transmit(struct iwic_softc *sc)
{
	int cmd;
	u_char *ptr;
	int len;

	if (!sc->sc_dchan.tx_ready)
		return;

	if (!sc->sc_dchan.obuf)
		return;

	if (sc->sc_I430state != ST_F7)
		return;

	ptr = sc->sc_dchan.obuf_ptr;
	len = min(sc->sc_dchan.obuf_len, IWIC_DCHAN_FIFO_LEN);

	if(sc->sc_trace & TRACE_D_TX)
	{
		i4b_trace_hdr hdr;
		hdr.type = TRC_CH_D;
		hdr.dir = FROM_TE;
		hdr.count = ++sc->sc_dchan.trace_count;
		isdn_layer2_trace_ind(&sc->sc_l2, sc->sc_l3token, &hdr, len, ptr);
	}

	IWIC_WRDFIFO(sc, ptr, len);

	sc->sc_dchan.tx_count += len;

	if (len < sc->sc_dchan.obuf_len)
	{
		sc->sc_dchan.obuf_ptr += len;
		sc->sc_dchan.obuf_len -= len;

		cmd = D_CMDR_XMS;

	}
	else
	{
		if (sc->sc_dchan.free_obuf)
			i4b_Dfreembuf(sc->sc_dchan.obuf);

		sc->sc_dchan.obuf = NULL;
		sc->sc_dchan.obuf_ptr = NULL;
		sc->sc_dchan.obuf_len = 0;

		if (sc->sc_dchan.obuf2)
		{
			sc->sc_dchan.obuf = sc->sc_dchan.obuf2;
			sc->sc_dchan.obuf_ptr = sc->sc_dchan.obuf->m_data;
			sc->sc_dchan.obuf_len = sc->sc_dchan.obuf->m_len;
			sc->sc_dchan.free_obuf = sc->sc_dchan.free_obuf2;

			sc->sc_dchan.obuf2 = NULL;
		}
		cmd = D_CMDR_XMS | D_CMDR_XME;
	}
	sc->sc_dchan.tx_ready = 0;
	IWIC_WRITE(sc, D_CMDR, cmd);
}

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
 *	i4b_isac.c - i4b siemens isdn chipset driver ISAC handler
 *	---------------------------------------------------------
 *
 *	$Id: isac.c,v 1.24 2012/10/27 17:18:21 chs Exp $
 *
 *      last edit-date: [Fri Jan  5 11:36:10 2001]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: isac.c,v 1.24 2012/10/27 17:18:21 chs Exp $");

#ifdef __FreeBSD__
#include "opt_i4b.h"
#endif
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
#include <machine/i4b_trace.h>
#else
#include <netisdn/i4b_debug.h>
#include <netisdn/i4b_ioctl.h>
#include <netisdn/i4b_trace.h>
#endif

#include <netisdn/i4b_global.h>
#include <netisdn/i4b_l2.h>
#include <netisdn/i4b_l1l2.h>
#include <netisdn/i4b_mbuf.h>

#include <dev/ic/isic_l1.h>
#include <dev/ic/isac.h>
#include <dev/ic/ipac.h>
#include <dev/ic/hscx.h>

static u_char isic_isac_exir_hdlr(register struct isic_softc *sc, u_char exir);
static void isic_isac_ind_hdlr(register struct isic_softc *sc, int ind);

/*---------------------------------------------------------------------------*
 *	ISAC interrupt service routine
 *---------------------------------------------------------------------------*/
void
isic_isac_irq(struct isic_softc *sc, int ista)
{
	register u_char c = 0;
	NDBGL1(L1_F_MSG, "%s: ista = 0x%02x", device_xname(sc->sc_dev), ista);

	if(ista & ISAC_ISTA_EXI)	/* extended interrupt */
	{
		u_int8_t exirstat = ISAC_READ(I_EXIR);
		if (sc->sc_intr_valid == ISIC_INTR_VALID)
			c |= isic_isac_exir_hdlr(sc, exirstat);
	}

	if(ista & ISAC_ISTA_RME)	/* receive message end */
	{
		register int rest;
		u_char rsta;

		/* get rx status register */

		rsta = ISAC_READ(I_RSTA);

		if((rsta & ISAC_RSTA_MASK) != 0x20)
		{
			int error = 0;

			if(!(rsta & ISAC_RSTA_CRC))	/* CRC error */
			{
				error++;
				NDBGL1(L1_I_ERR, "%s: CRC error", device_xname(sc->sc_dev));
			}

			if(rsta & ISAC_RSTA_RDO)	/* ReceiveDataOverflow */
			{
				error++;
				NDBGL1(L1_I_ERR, "%s: Data Overrun error", device_xname(sc->sc_dev));
			}

			if(rsta & ISAC_RSTA_RAB)	/* ReceiveABorted */
			{
				error++;
				NDBGL1(L1_I_ERR, "%s: Receive Aborted error", device_xname(sc->sc_dev));
			}

			if(error == 0)
			{
				NDBGL1(L1_I_ERR, "%s: RME unknown error, RSTA = 0x%02x!", device_xname(sc->sc_dev), rsta);
			}

			i4b_Dfreembuf(sc->sc_ibuf);

			c |= ISAC_CMDR_RMC|ISAC_CMDR_RRES;

			sc->sc_ibuf = NULL;
			sc->sc_ib = NULL;
			sc->sc_ilen = 0;

			ISAC_WRITE(I_CMDR, ISAC_CMDR_RMC|ISAC_CMDR_RRES);
			ISACCMDRWRDELAY();

			return;
		}

		rest = (ISAC_READ(I_RBCL) & (ISAC_FIFO_LEN-1));

		if(rest == 0)
			rest = ISAC_FIFO_LEN;

		if(sc->sc_ibuf == NULL)
		{
			if((sc->sc_ibuf = i4b_Dgetmbuf(rest)) != NULL)
				sc->sc_ib = sc->sc_ibuf->m_data;
			else
				panic("isic_isac_irq: RME, i4b_Dgetmbuf returns NULL!");
			sc->sc_ilen = 0;
		}

		if(sc->sc_ilen <= (MAX_DFRAME_LEN - rest))
		{
			ISAC_RDFIFO(sc->sc_ib, rest);
			sc->sc_ilen += rest;

			sc->sc_ibuf->m_pkthdr.len =
				sc->sc_ibuf->m_len = sc->sc_ilen;

			if(sc->sc_trace & TRACE_D_RX)
			{
				i4b_trace_hdr hdr;
				memset(&hdr, 0, sizeof hdr);
				hdr.type = TRC_CH_D;
				hdr.dir = FROM_NT;
				hdr.count = ++sc->sc_trace_dcount;
				isdn_layer2_trace_ind(&sc->sc_l2, sc->sc_l3token, &hdr, sc->sc_ibuf->m_len, sc->sc_ibuf->m_data);
			}

			c |= ISAC_CMDR_RMC;

			if(sc->sc_intr_valid == ISIC_INTR_VALID &&
			   (((struct isdn_l3_driver*)sc->sc_l3token)->protocol != PROTOCOL_D64S))
			{
				isdn_layer2_data_ind(&sc->sc_l2, sc->sc_l3token, sc->sc_ibuf);
			}
			else
			{
				i4b_Dfreembuf(sc->sc_ibuf);
			}
		}
		else
		{
			NDBGL1(L1_I_ERR, "RME, input buffer overflow!");
			i4b_Dfreembuf(sc->sc_ibuf);
			c |= ISAC_CMDR_RMC|ISAC_CMDR_RRES;
		}

		sc->sc_ibuf = NULL;
		sc->sc_ib = NULL;
		sc->sc_ilen = 0;
	}

	if(ista & ISAC_ISTA_RPF)	/* receive fifo full */
	{
		if(sc->sc_ibuf == NULL)
		{
			if((sc->sc_ibuf = i4b_Dgetmbuf(MAX_DFRAME_LEN)) != NULL)
				sc->sc_ib= sc->sc_ibuf->m_data;
			else
				panic("isic_isac_irq: RPF, i4b_Dgetmbuf returns NULL!");
			sc->sc_ilen = 0;
		}

		if(sc->sc_ilen <= (MAX_DFRAME_LEN - ISAC_FIFO_LEN))
		{
			ISAC_RDFIFO(sc->sc_ib, ISAC_FIFO_LEN);
			sc->sc_ilen += ISAC_FIFO_LEN;
			sc->sc_ib += ISAC_FIFO_LEN;
			c |= ISAC_CMDR_RMC;
		}
		else
		{
			NDBGL1(L1_I_ERR, "RPF, input buffer overflow!");
			i4b_Dfreembuf(sc->sc_ibuf);
			sc->sc_ibuf = NULL;
			sc->sc_ib = NULL;
			sc->sc_ilen = 0;
			c |= ISAC_CMDR_RMC|ISAC_CMDR_RRES;
		}
	}

	if(ista & ISAC_ISTA_XPR)	/* transmit fifo empty (XPR bit set) */
	{
		if((sc->sc_obuf2 != NULL) && (sc->sc_obuf == NULL))
		{
			sc->sc_freeflag = sc->sc_freeflag2;
			sc->sc_obuf = sc->sc_obuf2;
			sc->sc_op = sc->sc_obuf->m_data;
			sc->sc_ol = sc->sc_obuf->m_len;
			sc->sc_obuf2 = NULL;
#ifdef NOTDEF
			printf("ob2=%x, op=%x, ol=%d, f=%d #",
				sc->sc_obuf,
				sc->sc_op,
				sc->sc_ol,
				sc->sc_state);
#endif
		}
		else
		{
#ifdef NOTDEF
			printf("ob=%x, op=%x, ol=%d, f=%d #",
				sc->sc_obuf,
				sc->sc_op,
				sc->sc_ol,
				sc->sc_state);
#endif
		}

		if(sc->sc_obuf)
		{
			ISAC_WRFIFO(sc->sc_op, min(sc->sc_ol, ISAC_FIFO_LEN));

			if(sc->sc_ol > ISAC_FIFO_LEN)	/* length > 32 ? */
			{
				sc->sc_op += ISAC_FIFO_LEN; /* bufferptr+32 */
				sc->sc_ol -= ISAC_FIFO_LEN; /* length - 32 */
				c |= ISAC_CMDR_XTF;	    /* set XTF bit */
			}
			else
			{
				if(sc->sc_freeflag)
				{
					i4b_Dfreembuf(sc->sc_obuf);
					sc->sc_freeflag = 0;
				}
				sc->sc_obuf = NULL;
				sc->sc_op = NULL;
				sc->sc_ol = 0;

				c |= ISAC_CMDR_XTF | ISAC_CMDR_XME;
			}
		}
		else
		{
			sc->sc_state &= ~ISAC_TX_ACTIVE;
		}
	}

	if(ista & ISAC_ISTA_CISQ)	/* channel status change CISQ */
	{
		register u_char ci;

		/* get command/indication rx register*/

		ci = ISAC_READ(I_CIRR);

		/* if S/Q IRQ, read SQC reg to clr SQC IRQ */

		if(ci & ISAC_CIRR_SQC)
			(void) ISAC_READ(I_SQRR);

		/* C/I code change IRQ (flag already cleared by CIRR read) */

		if(ci & ISAC_CIRR_CIC0)
			isic_isac_ind_hdlr(sc, (ci >> 2) & 0xf);
	}

	if(c)
	{
		ISAC_WRITE(I_CMDR, c);
		ISACCMDRWRDELAY();
	}
}

/*---------------------------------------------------------------------------*
 *	ISAC L1 Extended IRQ handler
 *---------------------------------------------------------------------------*/
static u_char
isic_isac_exir_hdlr(register struct isic_softc *sc, u_char exir)
{
	u_char c = 0;

	if(exir & ISAC_EXIR_XMR)
	{
		NDBGL1(L1_I_ERR, "EXIRQ Tx Message Repeat");

		c |= ISAC_CMDR_XRES;
	}

	if(exir & ISAC_EXIR_XDU)
	{
		NDBGL1(L1_I_ERR, "EXIRQ Tx Data Underrun");

		c |= ISAC_CMDR_XRES;
	}

	if(exir & ISAC_EXIR_PCE)
	{
		NDBGL1(L1_I_ERR, "EXIRQ Protocol Error");
	}

	if(exir & ISAC_EXIR_RFO)
	{
		NDBGL1(L1_I_ERR, "EXIRQ Rx Frame Overflow");

		c |= ISAC_CMDR_RMC|ISAC_CMDR_RRES;
	}

	if(exir & ISAC_EXIR_SOV)
	{
		NDBGL1(L1_I_ERR, "EXIRQ Sync Xfer Overflow");
	}

	if(exir & ISAC_EXIR_MOS)
	{
		NDBGL1(L1_I_ERR, "EXIRQ Monitor Status");
	}

	if(exir & ISAC_EXIR_SAW)
	{
		/* cannot happen, STCR:TSF is set to 0 */

		NDBGL1(L1_I_ERR, "EXIRQ Subscriber Awake");
	}

	if(exir & ISAC_EXIR_WOV)
	{
		/* cannot happen, STCR:TSF is set to 0 */

		NDBGL1(L1_I_ERR, "EXIRQ Watchdog Timer Overflow");
	}

	return(c);
}

/*---------------------------------------------------------------------------*
 *	ISAC L1 Indication handler
 *---------------------------------------------------------------------------*/
static void
isic_isac_ind_hdlr(register struct isic_softc *sc, int ind)
{
	register int event;

	switch(ind)
	{
		case ISAC_CIRR_IAI8:
			NDBGL1(L1_I_CICO, "rx AI8 in state %s", isic_printstate(sc));
			if(sc->sc_bustyp == BUS_TYPE_IOM2)
				isic_isac_l1_cmd(sc, CMD_AR8);
			event = EV_INFO48;
			isdn_layer2_status_ind(&sc->sc_l2, sc->sc_l3token, STI_L1STAT, LAYER_ACTIVE);
			break;

		case ISAC_CIRR_IAI10:
			NDBGL1(L1_I_CICO, "rx AI10 in state %s", isic_printstate(sc));
			if(sc->sc_bustyp == BUS_TYPE_IOM2)
				isic_isac_l1_cmd(sc, CMD_AR10);
			event = EV_INFO410;
			isdn_layer2_status_ind(&sc->sc_l2, sc->sc_l3token, STI_L1STAT, LAYER_ACTIVE);
			break;

		case ISAC_CIRR_IRSY:
			NDBGL1(L1_I_CICO, "rx RSY in state %s", isic_printstate(sc));
			event = EV_RSY;
			break;

		case ISAC_CIRR_IPU:
			NDBGL1(L1_I_CICO, "rx PU in state %s", isic_printstate(sc));
			event = EV_PU;
			break;

		case ISAC_CIRR_IDR:
			NDBGL1(L1_I_CICO, "rx DR in state %s", isic_printstate(sc));
			isic_isac_l1_cmd(sc, CMD_DIU);
			event = EV_DR;
			break;

		case ISAC_CIRR_IDID:
			NDBGL1(L1_I_CICO, "rx DID in state %s", isic_printstate(sc));
			event = EV_INFO0;
			isdn_layer2_status_ind(&sc->sc_l2, sc->sc_l3token, STI_L1STAT, LAYER_IDLE);
			break;

		case ISAC_CIRR_IDIS:
			NDBGL1(L1_I_CICO, "rx DIS in state %s", isic_printstate(sc));
			event = EV_DIS;
			break;

		case ISAC_CIRR_IEI:
			NDBGL1(L1_I_CICO, "rx EI in state %s", isic_printstate(sc));
			isic_isac_l1_cmd(sc, CMD_DIU);
			event = EV_EI;
			break;

		case ISAC_CIRR_IARD:
			NDBGL1(L1_I_CICO, "rx ARD in state %s", isic_printstate(sc));
			event = EV_INFO2;
			break;

		case ISAC_CIRR_ITI:
			NDBGL1(L1_I_CICO, "rx TI in state %s", isic_printstate(sc));
			event = EV_INFO0;
			break;

		case ISAC_CIRR_IATI:
			NDBGL1(L1_I_CICO, "rx ATI in state %s", isic_printstate(sc));
			event = EV_INFO0;
			break;

		case ISAC_CIRR_ISD:
			NDBGL1(L1_I_CICO, "rx SD in state %s", isic_printstate(sc));
			event = EV_INFO0;
			break;

		default:
			NDBGL1(L1_I_ERR, "UNKNOWN Indication 0x%x in state %s", ind, isic_printstate(sc));
			event = EV_INFO0;
			break;
	}
	isic_next_state(sc, event);
}

/*---------------------------------------------------------------------------*
 *	execute a layer 1 command
 *---------------------------------------------------------------------------*/
void
isic_isac_l1_cmd(struct isic_softc *sc, int command)
{
	u_char cmd;

#ifdef I4B_SMP_WORKAROUND

	/* XXXXXXXXXXXXXXXXXXX */

	/*
	 * patch from Wolfgang Helbig:
	 *
	 * Here is a patch that makes i4b work on an SMP:
	 * The card (TELES 16.3) didn't interrupt on an SMP machine.
	 * This is a gross workaround, but anyway it works *and* provides
	 * some information as how to finally fix this problem.
	 */

	HSCX_WRITE(0, H_MASK, 0xff);
	HSCX_WRITE(1, H_MASK, 0xff);
	ISAC_WRITE(I_MASK, 0xff);
	DELAY(100);
	HSCX_WRITE(0, H_MASK, HSCX_A_IMASK);
	HSCX_WRITE(1, H_MASK, HSCX_B_IMASK);
	ISAC_WRITE(I_MASK, ISAC_IMASK);

	/* XXXXXXXXXXXXXXXXXXX */

#endif /* I4B_SMP_WORKAROUND */

	if(command < 0 || command > CMD_ILL)
	{
		NDBGL1(L1_I_ERR, "illegal cmd 0x%x in state %s", command, isic_printstate(sc));
		return;
	}

	if(sc->sc_bustyp == BUS_TYPE_IOM2)
		cmd = ISAC_CIX0_LOW;
	else
		cmd = 0;

	switch(command)
	{
		case CMD_TIM:
			NDBGL1(L1_I_CICO, "tx TIM in state %s", isic_printstate(sc));
			cmd |= (ISAC_CIXR_CTIM << 2);
			break;

		case CMD_RS:
			NDBGL1(L1_I_CICO, "tx RS in state %s", isic_printstate(sc));
			cmd |= (ISAC_CIXR_CRS << 2);
			break;

		case CMD_AR8:
			NDBGL1(L1_I_CICO, "tx AR8 in state %s", isic_printstate(sc));
			cmd |= (ISAC_CIXR_CAR8 << 2);
			break;

		case CMD_AR10:
			NDBGL1(L1_I_CICO, "tx AR10 in state %s", isic_printstate(sc));
			cmd |= (ISAC_CIXR_CAR10 << 2);
			break;

		case CMD_DIU:
			NDBGL1(L1_I_CICO, "tx DIU in state %s", isic_printstate(sc));
			cmd |= (ISAC_CIXR_CDIU << 2);
			break;
	}
	ISAC_WRITE(I_CIXR, cmd);
}

/*---------------------------------------------------------------------------*
 *	L1 ISAC initialization
 *---------------------------------------------------------------------------*/
int
isic_isac_init(struct isic_softc *sc)
{
	ISAC_IMASK = 0xff;		/* disable all irqs */

	ISAC_WRITE(I_MASK, ISAC_IMASK);

	if(sc->sc_bustyp != BUS_TYPE_IOM2)
	{
		NDBGL1(L1_I_SETUP, "configuring for IOM-1 mode");

		/* ADF2: Select mode IOM-1 */
		ISAC_WRITE(I_ADF2, 0x00);

		/* SPCR: serial port control register:
		 *	SPU - software power up = 0
		 *	SAC - SIP port high Z
		 *	SPM - timing mode 0
		 *	TLP - test loop = 0
		 *	C1C, C2C - B1 and B2 switched to/from SPa
		 */
		ISAC_WRITE(I_SPCR, ISAC_SPCR_C1C1|ISAC_SPCR_C2C1);

		/* SQXR: S/Q channel xmit register:
                 *	SQIE - S/Q IRQ enable = 0
		 *	SQX1-4 - Fa bits = 1
		 */
		ISAC_WRITE(I_SQXR, ISAC_SQXR_SQX1|ISAC_SQXR_SQX2|ISAC_SQXR_SQX3|ISAC_SQXR_SQX4);

		/* ADF1: additional feature reg 1:
		 *	WTC - watchdog = 0
		 *	TEM - test mode = 0
		 *	PFS - pre-filter = 0
		 *	CFS - IOM clock/frame always active
		 *	FSC1/2 - polarity of 8kHz strobe
		 *	ITF - interframe fill = idle
		 */
		ISAC_WRITE(I_ADF1, ISAC_ADF1_FC2);	/* ADF1 */

		/* STCR: sync transfer control reg:
		 *	TSF - terminal secific functions = 0
		 *	TBA - TIC bus address = 7
		 *	STx/SCx = 0
		 */
		ISAC_WRITE(I_STCR, ISAC_STCR_TBA2|ISAC_STCR_TBA1|ISAC_STCR_TBA0);
	}
	else
	{
		NDBGL1(L1_I_SETUP, "configuring for IOM-2 mode");

		/* ADF2: Select mode IOM-2 */
		ISAC_WRITE(I_ADF2, ISAC_ADF2_IMS);

		/* SPCR: serial port control register:
		 *	SPU - software power up = 0
		 *	SPM - timing mode 0
		 *	TLP - test loop = 0
		 *	C1C, C2C - B1 + C1 and B2 + IC2 monitoring
		 */
		ISAC_WRITE(I_SPCR, 0x00);

		/* SQXR: S/Q channel xmit register:
		 *	IDC  - IOM direction = 0 (master)
		 *	CFS  - Config Select = 0 (clock always active)
		 *	CI1E - C/I channel 1 IRQ enable = 0
                 *	SQIE - S/Q IRQ enable = 0
		 *	SQX1-4 - Fa bits = 1
		 */
		ISAC_WRITE(I_SQXR, ISAC_SQXR_SQX1|ISAC_SQXR_SQX2|ISAC_SQXR_SQX3|ISAC_SQXR_SQX4);

		/* ADF1: additional feature reg 1:
		 *	WTC - watchdog = 0
		 *	TEM - test mode = 0
		 *	PFS - pre-filter = 0
		 *	IOF - IOM i/f off = 0
		 *	ITF - interframe fill = idle
		 */
		ISAC_WRITE(I_ADF1, 0x00);

		/* STCR: sync transfer control reg:
		 *	TSF - terminal secific functions = 0
		 *	TBA - TIC bus address = 7
		 *	STx/SCx = 0
		 */
		ISAC_WRITE(I_STCR, ISAC_STCR_TBA2|ISAC_STCR_TBA1|ISAC_STCR_TBA0);
	}


	/* MODE: Mode Register:
	 *	MDSx - transparent mode 2
	 *	TMD  - timer mode = external
	 *	RAC  - Receiver enabled
	 *	DIMx - digital i/f mode
	 */
	ISAC_WRITE(I_MODE, ISAC_MODE_MDS2|ISAC_MODE_MDS1|ISAC_MODE_RAC|ISAC_MODE_DIM0);

	/* enabled interrupts:
	 * ===================
	 * RME  - receive message end
	 * RPF  - receive pool full
	 * XPR  - transmit pool ready
	 * CISQ - CI or S/Q channel change
	 * EXI  - extended interrupt
	 */

	ISAC_IMASK = ISAC_MASK_RSC |	/* auto mode only	*/
		     ISAC_MASK_TIN | 	/* timer irq		*/
		     ISAC_MASK_SIN;	/* sync xfer irq	*/

	ISAC_WRITE(I_MASK, ISAC_IMASK);

	ISAC_WRITE(I_CMDR, ISAC_CMDR_RRES|ISAC_CMDR_XRES);
	ISACCMDRWRDELAY();

	return(0);
}

/*---------------------------------------------------------------------------*
 *	isic_recovery - try to recover from irq lockup
 *---------------------------------------------------------------------------*/
void
isic_recover(struct isic_softc *sc)
{
	u_char byte;

	/* get hscx irq status from hscx b ista */

	byte = HSCX_READ(HSCX_CH_B, H_ISTA);

	NDBGL1(L1_ERROR, "HSCX B: ISTA = 0x%x", byte);

	if(byte & HSCX_ISTA_ICA)
		NDBGL1(L1_ERROR, "HSCX A: ISTA = 0x%x", (u_char)HSCX_READ(HSCX_CH_A, H_ISTA));

	if(byte & HSCX_ISTA_EXB)
		NDBGL1(L1_ERROR, "HSCX B: EXIR = 0x%x", (u_char)HSCX_READ(HSCX_CH_B, H_EXIR));

	if(byte & HSCX_ISTA_EXA)
		NDBGL1(L1_ERROR, "HSCX A: EXIR = 0x%x", (u_char)HSCX_READ(HSCX_CH_A, H_EXIR));

	/* get isac irq status */

	byte = ISAC_READ(I_ISTA);

	NDBGL1(L1_ERROR, "  ISAC: ISTA = 0x%x", byte);

	if(byte & ISAC_ISTA_EXI)
		NDBGL1(L1_ERROR, "  ISAC: EXIR = 0x%x", (u_char)ISAC_READ(I_EXIR));

	if(byte & ISAC_ISTA_CISQ)
	{
		byte = ISAC_READ(I_CIRR);

		NDBGL1(L1_ERROR, "  ISAC: CISQ = 0x%x", byte);

		if(byte & ISAC_CIRR_SQC)
			NDBGL1(L1_ERROR, "  ISAC: SQRR = 0x%x", (u_char)ISAC_READ(I_SQRR));
	}

	NDBGL1(L1_ERROR, "HSCX B: IMASK = 0x%x", HSCX_B_IMASK);
	NDBGL1(L1_ERROR, "HSCX A: IMASK = 0x%x", HSCX_A_IMASK);

	HSCX_WRITE(0, H_MASK, 0xff);
	HSCX_WRITE(1, H_MASK, 0xff);
	DELAY(100);
	HSCX_WRITE(0, H_MASK, HSCX_A_IMASK);
	HSCX_WRITE(1, H_MASK, HSCX_B_IMASK);
	DELAY(100);

	NDBGL1(L1_ERROR, "  ISAC: IMASK = 0x%x", ISAC_IMASK);

	ISAC_WRITE(I_MASK, 0xff);
	DELAY(100);
	ISAC_WRITE(I_MASK, ISAC_IMASK);
}

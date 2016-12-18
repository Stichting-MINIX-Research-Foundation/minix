/*	$NetBSD: iwic_bchan.c,v 1.9 2014/03/23 02:53:12 christos Exp $	*/

/*
 * Copyright (c) 1999, 2000 Dave Boyce. All rights reserved.
 *
 * Copyright (c) 2000, 2001 Hellmuth Michaelis. All rights reserved.
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
 *      last edit-date: [Tue Jan 16 13:21:24 2001]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: iwic_bchan.c,v 1.9 2014/03/23 02:53:12 christos Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/callout.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <net/if.h>

#include <sys/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/iwicreg.h>
#include <dev/pci/iwicvar.h>

#include <netisdn/i4b_debug.h>
#include <netisdn/i4b_ioctl.h>
#include <netisdn/i4b_trace.h>

#include <netisdn/i4b_l2.h>
#include <netisdn/i4b_l1l2.h>
#include <netisdn/i4b_mbuf.h>
#include <netisdn/i4b_global.h>

static void iwic_bchan_init(struct iwic_softc *sc, int chan_no, int activate);

/*---------------------------------------------------------------------------*
 *	B-channel interrupt handler
 *---------------------------------------------------------------------------*/
void
iwic_bchan_xirq(struct iwic_softc *sc, int chan_no)
{
	int irq_stat;
	struct iwic_bchan *chan;
	int cmd = 0;

	chan = &sc->sc_bchan[chan_no];

	irq_stat = IWIC_READ(sc, chan->offset + B_EXIR);

	NDBGL1(L1_H_IRQ, "irq_stat = 0x%x", irq_stat);

	if((irq_stat & (B_EXIR_RMR | B_EXIR_RME | B_EXIR_RDOV | B_EXIR_XFR | B_EXIR_XDUN)) == 0)
	{
		NDBGL1(L1_H_XFRERR, "spurious IRQ!");
		return;
	}

	if (irq_stat & B_EXIR_RDOV)
	{
		NDBGL1(L1_H_XFRERR, "%s: EXIR B-channel Receive Data Overflow", device_xname(sc->sc_dev));
	}

	if (irq_stat & B_EXIR_XDUN)
	{
		NDBGL1(L1_H_XFRERR, "%s: EXIR B-channel Transmit Data Underrun", device_xname(sc->sc_dev));
		cmd |= (B_CMDR_XRST);	/*XXX must retransmit frame ! */
	}

/* RX message end interrupt */

	if(irq_stat & B_EXIR_RME)
	{
		int error;

		NDBGL1(L1_H_IRQ, "B_EXIR_RME");

		error = (IWIC_READ(sc,chan->offset+B_STAR) &
			 (B_STAR_RDOV | B_STAR_CRCE | B_STAR_RMB));

		if(error)
		{
			if(error & B_STAR_RDOV)
				NDBGL1(L1_H_XFRERR, "%s: B-channel Receive Data Overflow", device_xname(sc->sc_dev));
			if(error & B_STAR_CRCE)
				NDBGL1(L1_H_XFRERR, "%s: B-channel CRC Error", device_xname(sc->sc_dev));
			if(error & B_STAR_RMB)
				NDBGL1(L1_H_XFRERR, "%s: B-channel Receive Message Aborted", device_xname(sc->sc_dev));
		}

		/* all error conditions checked, now decide and take action */

		if(error == 0)
		{
			register int fifo_data_len;
			fifo_data_len = ((IWIC_READ(sc,chan->offset+B_RBCL)) &
					((IWIC_BCHAN_FIFO_LEN)-1));

			if(fifo_data_len == 0)
				fifo_data_len = IWIC_BCHAN_FIFO_LEN;


			if(chan->in_mbuf == NULL)
			{
				if((chan->in_mbuf = i4b_Bgetmbuf(BCH_MAX_DATALEN)) == NULL)
					panic("L1 iwic_bchan_irq: RME, cannot allocate mbuf!");
				chan->in_cbptr = chan->in_mbuf->m_data;
				chan->in_len = 0;
			}

			if((chan->in_len + fifo_data_len) <= BCH_MAX_DATALEN)
			{
				/* read data from fifo */

				NDBGL1(L1_H_IRQ, "B_EXIR_RME, rd fifo, len = %d", fifo_data_len);

				IWIC_RDBFIFO(sc, chan, chan->in_cbptr, fifo_data_len);

				cmd |= (B_CMDR_RACK | B_CMDR_RACT);
				IWIC_WRITE(sc, chan->offset + B_CMDR, cmd);
				cmd = 0;

		                chan->in_len += fifo_data_len;
				chan->rxcount += fifo_data_len;

				/* setup mbuf data length */

				chan->in_mbuf->m_len = chan->in_len;
				chan->in_mbuf->m_pkthdr.len = chan->in_len;

				if(sc->sc_trace & TRACE_B_RX)
				{
					i4b_trace_hdr hdr;
					hdr.type = (chan_no == IWIC_BCH_A ? TRC_CH_B1 : TRC_CH_B2);
					hdr.dir = FROM_NT;
					hdr.count = ++sc->sc_bchan[chan_no].sc_trace_bcount;
					isdn_layer2_trace_ind(&sc->sc_l2, sc->sc_l3token, &hdr,chan->in_mbuf->m_len, chan->in_mbuf->m_data);
				}

				(*chan->l4_driver->bch_rx_data_ready)(chan->l4_driver_softc);


				/* mark buffer ptr as unused */

				chan->in_mbuf = NULL;
				chan->in_cbptr = NULL;
				chan->in_len = 0;
			}
			else
			{
				NDBGL1(L1_H_XFRERR, "RAWHDLC rx buffer overflow in RME, in_len=%d, fifolen=%d", chan->in_len, fifo_data_len);
				chan->in_cbptr = chan->in_mbuf->m_data;
				chan->in_len = 0;
				cmd |= (B_CMDR_RRST | B_CMDR_RACK);
			}
		}
		else
		{
			if (chan->in_mbuf != NULL)
			{
				i4b_Bfreembuf(chan->in_mbuf);
				chan->in_mbuf = NULL;
				chan->in_cbptr = NULL;
				chan->in_len = 0;
			}
			cmd |= (B_CMDR_RRST | B_CMDR_RACK);
		}
	}

/* RX fifo full interrupt */

	if(irq_stat & B_EXIR_RMR)
	{
		NDBGL1(L1_H_IRQ, "B_EXIR_RMR");

		if(chan->in_mbuf == NULL)
		{
			if((chan->in_mbuf = i4b_Bgetmbuf(BCH_MAX_DATALEN)) == NULL)
				panic("L1 iwic_bchan_irq: RMR, cannot allocate mbuf!");
			chan->in_cbptr = chan->in_mbuf->m_data;
			chan->in_len = 0;
		}

		chan->rxcount += IWIC_BCHAN_FIFO_LEN;

		if((chan->in_len + IWIC_BCHAN_FIFO_LEN) <= BCH_MAX_DATALEN)
		{
			/* read data from fifo */

			NDBGL1(L1_H_IRQ, "B_EXIR_RMR, rd fifo, len = max (64)");

			IWIC_RDBFIFO(sc, chan, chan->in_cbptr, IWIC_BCHAN_FIFO_LEN);

			chan->in_cbptr += IWIC_BCHAN_FIFO_LEN;
	                chan->in_len += IWIC_BCHAN_FIFO_LEN;
		}
		else
		{
			if(chan->bprot == BPROT_NONE)
			{
				/* setup mbuf data length */

				chan->in_mbuf->m_len = chan->in_len;
				chan->in_mbuf->m_pkthdr.len = chan->in_len;

				if(sc->sc_trace & TRACE_B_RX)
				{
					i4b_trace_hdr hdr;
					hdr.type = (chan_no == IWIC_BCH_A ? TRC_CH_B1 : TRC_CH_B2);
					hdr.dir = FROM_NT;
					hdr.count = ++sc->sc_bchan[chan_no].sc_trace_bcount;
					isdn_layer2_trace_ind(&sc->sc_l2, sc->sc_l3token, &hdr,chan->in_mbuf->m_len, chan->in_mbuf->m_data);
				}

				/* silence detection */

				isdn_bchan_silence(chan->in_mbuf->m_data, chan->in_mbuf->m_len);

#if defined (__FreeBSD__) && __FreeBSD__ > 4
				(void) IF_HANDOFF(&chan->rx_queue, chan->in_mbuf, NULL);
#else
				if(!(IF_QFULL(&chan->rx_queue)))
				{
					IF_ENQUEUE(&chan->rx_queue, chan->in_mbuf);
				}
				else
				{
					i4b_Bfreembuf(chan->in_mbuf);
				}
#endif
				/* signal upper driver that data is available */

				(*chan->l4_driver->bch_rx_data_ready)(chan->l4_driver_softc);

				/* alloc new buffer */

				if((chan->in_mbuf = i4b_Bgetmbuf(BCH_MAX_DATALEN)) == NULL)
					panic("L1 iwic_bchan_irq: RMR, cannot allocate new mbuf!");

				/* setup new data ptr */

				chan->in_cbptr = chan->in_mbuf->m_data;

				/* read data from fifo */

				NDBGL1(L1_H_IRQ, "B_EXIR_RMR, rd fifo1, len = max (64)");

				IWIC_RDBFIFO(sc, chan, chan->in_cbptr, IWIC_BCHAN_FIFO_LEN);

				chan->in_cbptr += IWIC_BCHAN_FIFO_LEN;
				chan->in_len = IWIC_BCHAN_FIFO_LEN;

				chan->rxcount += IWIC_BCHAN_FIFO_LEN;
			}
			else
			{
				NDBGL1(L1_H_XFRERR, "RAWHDLC rx buffer overflow in RPF, in_len=%d", chan->in_len);
				chan->in_cbptr = chan->in_mbuf->m_data;
				chan->in_len = 0;
				cmd |= (B_CMDR_RRST | B_CMDR_RACK);
			}
		}

		/* command to release fifo space */

		cmd |= B_CMDR_RACK;
	}

/* TX interrupt */

	if (irq_stat & B_EXIR_XFR)
	{
		/* transmit fifo empty, new data can be written to fifo */

		int len;
		int nextlen;

		NDBGL1(L1_H_IRQ, "B_EXIR_XFR");

		if(chan->out_mbuf_cur == NULL) 	/* last frame is transmitted */
		{
			IF_DEQUEUE(&chan->tx_queue, chan->out_mbuf_head);

			if(chan->out_mbuf_head == NULL)
			{
				chan->state &= ~ST_TX_ACTIVE;
				(*chan->l4_driver->bch_tx_queue_empty)(chan->l4_driver_softc);
			}
			else
			{
				chan->state |= ST_TX_ACTIVE;
				chan->out_mbuf_cur = chan->out_mbuf_head;
				chan->out_mbuf_cur_ptr = chan->out_mbuf_cur->m_data;
				chan->out_mbuf_cur_len = chan->out_mbuf_cur->m_len;

				if(sc->sc_trace & TRACE_B_TX)
				{
					i4b_trace_hdr hdr;
					hdr.type = (chan_no == IWIC_BCH_A ? TRC_CH_B1 : TRC_CH_B2);
					hdr.dir = FROM_TE;
					hdr.count = ++sc->sc_bchan[chan_no].sc_trace_bcount;
					isdn_layer2_trace_ind(&sc->sc_l2, sc->sc_l3token, &hdr, chan->out_mbuf_cur->m_len, chan->out_mbuf_cur->m_data);
				}

				if(chan->bprot == BPROT_NONE)
				{
					isdn_bchan_silence(chan->out_mbuf_cur->m_data, chan->out_mbuf_cur->m_len);
				}
			}
		}

		len = 0;

		while(chan->out_mbuf_cur && len != IWIC_BCHAN_FIFO_LEN)
		{
			nextlen = min(chan->out_mbuf_cur_len, IWIC_BCHAN_FIFO_LEN - len);

			NDBGL1(L1_H_IRQ, "B_EXIR_XFR, wr fifo, len = %d", nextlen);

			IWIC_WRBFIFO(sc, chan, chan->out_mbuf_cur_ptr, nextlen);

			cmd |= B_CMDR_XMS;

			len += nextlen;
			chan->txcount += nextlen;

			chan->out_mbuf_cur_ptr += nextlen;
			chan->out_mbuf_cur_len -= nextlen;

			if(chan->out_mbuf_cur_len == 0)
			{
				if((chan->out_mbuf_cur = chan->out_mbuf_cur->m_next) != NULL)
				{
					chan->out_mbuf_cur_ptr = chan->out_mbuf_cur->m_data;
					chan->out_mbuf_cur_len = chan->out_mbuf_cur->m_len;

					if(sc->sc_trace & TRACE_B_TX)
					{
						i4b_trace_hdr hdr;
						hdr.type = (chan_no == IWIC_BCH_A ? TRC_CH_B1 : TRC_CH_B2);
						hdr.dir = FROM_TE;
						hdr.count = ++sc->sc_bchan[chan_no].sc_trace_bcount;
						isdn_layer2_trace_ind(&sc->sc_l2, sc->sc_l3token, &hdr, chan->out_mbuf_cur->m_len, chan->out_mbuf_cur->m_data);
					}
				}
				else
				{
					if (chan->bprot != BPROT_NONE)
						cmd |= B_CMDR_XME;
					i4b_Bfreembuf(chan->out_mbuf_head);
					chan->out_mbuf_head = NULL;
				}
			}
		}
	}
	if(cmd)
	{
		cmd |= B_CMDR_RACT;
		IWIC_WRITE(sc, chan->offset + B_CMDR, cmd);
	}
}

/*---------------------------------------------------------------------------*
 *	initialize one B channels rx/tx data structures
 *---------------------------------------------------------------------------*/
void
iwic_bchannel_setup(isdn_layer1token t, int chan_no, int bprot, int activate)
{
	struct iwic_softc *sc = t;
	struct iwic_bchan *chan = &sc->sc_bchan[chan_no];

	int s = splnet();

	NDBGL1(L1_BCHAN, "%s: chan %d, bprot %d, activate %d",
	    device_xname(sc->sc_dev), chan_no, bprot, activate);

	/* general part */

	chan->bprot = bprot;		/* B channel protocol */
	chan->state = ST_IDLE;		/* B channel state */

	if(activate == 0)
	{
		/* deactivation */
		iwic_bchan_init(sc, chan_no, activate);
	}

	/* receiver part */

	chan->rx_queue.ifq_maxlen = IFQ_MAXLEN;

#if defined (__FreeBSD__) && __FreeBSD__ > 4
	if(!mtx_initialized(&chan->rx_queue.ifq_mtx))
		mtx_init(&chan->rx_queue.ifq_mtx, "i4b_iwic_rx", NULL, MTX_DEF);
#endif

	i4b_Bcleanifq(&chan->rx_queue);	/* clean rx queue */

	chan->rxcount = 0;		/* reset rx counter */

	i4b_Bfreembuf(chan->in_mbuf);	/* clean rx mbuf */

	chan->in_mbuf = NULL;		/* reset mbuf ptr */
	chan->in_cbptr = NULL;		/* reset mbuf curr ptr */
	chan->in_len = 0;		/* reset mbuf data len */

	/* transmitter part */

	chan->tx_queue.ifq_maxlen = IFQ_MAXLEN;

#if defined (__FreeBSD__) && __FreeBSD__ > 4
	if(!mtx_initialized(&chan->tx_queue.ifq_mtx))
		mtx_init(&chan->tx_queue.ifq_mtx, "i4b_iwic_tx", NULL, MTX_DEF);
#endif

	i4b_Bcleanifq(&chan->tx_queue);	/* clean tx queue */

	chan->txcount = 0;		/* reset tx counter */

	i4b_Bfreembuf(chan->out_mbuf_head);	/* clean tx mbuf */

	chan->out_mbuf_head = NULL;	/* reset head mbuf ptr */
	chan->out_mbuf_cur = NULL;	/* reset current mbuf ptr */
	chan->out_mbuf_cur_ptr = NULL;	/* reset current mbuf data ptr */
	chan->out_mbuf_cur_len = 0;	/* reset current mbuf data cnt */

	if(activate != 0)
	{
		/* activation */
		iwic_bchan_init(sc, chan_no, activate);
	}

	splx(s);
}

/*---------------------------------------------------------------------------*
 *	initalize / deinitialize B-channel hardware
 *---------------------------------------------------------------------------*/
static void
iwic_bchan_init(struct iwic_softc *sc, int chan_no, int activate)
{
	struct iwic_bchan *bchan = &sc->sc_bchan[chan_no];

	NDBGL1(L1_BCHAN, "chan %d, activate %d", chan_no, activate);

	if(activate)
	{
		if(bchan->bprot == BPROT_NONE)
		{
			/* Extended transparent mode */
			IWIC_WRITE(sc, bchan->offset + B_MODE, B_MODE_MMS);
		}
		else
		{
			/* Transparent mode */
			IWIC_WRITE(sc, bchan->offset + B_MODE, 0);
			/* disable address comparation */
			IWIC_WRITE (sc, bchan->offset+B_ADM1, 0xff);
			IWIC_WRITE (sc, bchan->offset+B_ADM2, 0xff);
		}

		/* reset & start receiver */
		IWIC_WRITE(sc, bchan->offset + B_CMDR, B_CMDR_RRST|B_CMDR_RACT);

		/* clear irq mask */
		IWIC_WRITE(sc, bchan->offset + B_EXIM, 0);
	}
	else
	{
		/* mask all irqs */
		IWIC_WRITE(sc, bchan->offset + B_EXIM, 0xff);

		/* reset mode */
		IWIC_WRITE(sc, bchan->offset + B_MODE, 0);

		/* Bring interface down */
		IWIC_WRITE(sc, bchan->offset + B_CMDR, B_CMDR_RRST | B_CMDR_XRST);

		/* Flush pending interrupts */
		IWIC_READ(sc, bchan->offset + B_EXIR);
	}
}

/*---------------------------------------------------------------------------*
 *	start transmission on a b channel
 *---------------------------------------------------------------------------*/
static void
iwic_bchannel_start(isdn_layer1token t,int h_chan)
{
	struct iwic_softc *sc = (void *)t;
	struct iwic_bchan *chan = &sc->sc_bchan[h_chan];
	register int len;
	register int next_len;

	int s;
	int activity = -1;
	int cmd = 0;

	s = splnet();				/* enter critical section */

	NDBGL1(L1_BCHAN, "%s: channel %d", device_xname(sc->sc_dev), h_chan);

	if(chan->state & ST_TX_ACTIVE)		/* already running ? */
	{
		splx(s);
		return;				/* yes, leave */
	}

	/* get next mbuf from queue */

	IF_DEQUEUE(&chan->tx_queue, chan->out_mbuf_head);

	if(chan->out_mbuf_head == NULL)		/* queue empty ? */
	{
		splx(s);			/* leave critical section */
		return;				/* yes, exit */
	}

	/* init current mbuf values */

	chan->out_mbuf_cur = chan->out_mbuf_head;
	chan->out_mbuf_cur_len = chan->out_mbuf_cur->m_len;
	chan->out_mbuf_cur_ptr = chan->out_mbuf_cur->m_data;

	/* activity indicator for timeout handling */

	if(chan->bprot == BPROT_NONE)
	{
		if(!(isdn_bchan_silence(chan->out_mbuf_cur->m_data, chan->out_mbuf_cur->m_len)))
			activity = ACT_TX;
	}
	else
	{
		activity = ACT_TX;
	}

	chan->state |= ST_TX_ACTIVE;		/* we start transmitting */

	if(sc->sc_trace & TRACE_B_TX)	/* if trace, send mbuf to trace dev */
	{
		i4b_trace_hdr hdr;
		hdr.type = (h_chan == IWIC_BCH_A ? TRC_CH_B1 : TRC_CH_B2);
		hdr.dir = FROM_TE;
		hdr.count = ++sc->sc_bchan[h_chan].sc_trace_bcount;
		isdn_layer2_trace_ind(&sc->sc_l2, sc->sc_l3token, &hdr, chan->out_mbuf_cur->m_len, chan->out_mbuf_cur->m_data);
	}

	len = 0;	/* # of chars put into tx fifo this time */

	/*
	 * fill the tx fifo with data from the current mbuf. if
	 * current mbuf holds less data than fifo length, try to
	 * get the next mbuf from (a possible) mbuf chain. if there is
	 * not enough data in a single mbuf or in a chain, then this
	 * is the last mbuf and we tell the chip that it has to send
	 * CRC and closing flag
	 */

	while((len < IWIC_BCHAN_FIFO_LEN) && chan->out_mbuf_cur)
	{
		/*
		 * put as much data into the fifo as is
		 * available from the current mbuf
		 */

		if((len + chan->out_mbuf_cur_len) >= IWIC_BCHAN_FIFO_LEN)
			next_len = IWIC_BCHAN_FIFO_LEN - len;
		else
			next_len = chan->out_mbuf_cur_len;

		/* write what we have from current mbuf to fifo */

		IWIC_WRBFIFO(sc, chan, chan->out_mbuf_cur_ptr, next_len);

		len += next_len;		/* update # of bytes written */
		chan->txcount += next_len;	/* statistics */
		chan->out_mbuf_cur_ptr += next_len;	/* data ptr */
		chan->out_mbuf_cur_len -= next_len;	/* data len */

		/*
		 * in case the current mbuf (of a possible chain) data
		 * has been put into the fifo, check if there is a next
		 * mbuf in the chain. If there is one, get ptr to it
		 * and update the data ptr and the length
		 */

		if((chan->out_mbuf_cur_len <= 0)	&&
		  ((chan->out_mbuf_cur = chan->out_mbuf_cur->m_next) != NULL))
		{
			chan->out_mbuf_cur_ptr = chan->out_mbuf_cur->m_data;
			chan->out_mbuf_cur_len = chan->out_mbuf_cur->m_len;

			if(sc->sc_trace & TRACE_B_TX)
			{
				i4b_trace_hdr hdr;
				hdr.type = (h_chan == IWIC_BCH_A ? TRC_CH_B1 : TRC_CH_B2);
				hdr.dir = FROM_TE;
				hdr.count = ++sc->sc_bchan[h_chan].sc_trace_bcount;
				isdn_layer2_trace_ind(&sc->sc_l2, sc->sc_l3token, &hdr, chan->out_mbuf_cur->m_len, chan->out_mbuf_cur->m_data);
			}
		}
	}

	/*
	 * if there is either still data in the current mbuf and/or
	 * there is a successor on the chain available issue just
	 * a XTF (transmit) command to the chip. if there is no more
	 * data available from the current mbuf (-chain), issue
	 * an XTF and an XME (message end) command which will then
	 * send the CRC and the closing HDLC flag sequence
	 */

	if(chan->out_mbuf_cur && (chan->out_mbuf_cur_len > 0))
	{
		/*
		 * more data available, send current fifo out.
		 * next xfer to tx fifo is done in the
		 * interrupt routine.
		 */

		cmd |= B_CMDR_XMS;
	}
	else
	{
		/* end of mbuf chain */

		if(chan->bprot == BPROT_NONE)
			cmd |= B_CMDR_XMS;
		else
			cmd |= (B_CMDR_XMS | B_CMDR_XME);

		i4b_Bfreembuf(chan->out_mbuf_head);	/* free mbuf chain */

		chan->out_mbuf_head = NULL;
		chan->out_mbuf_cur = NULL;
		chan->out_mbuf_cur_ptr = NULL;
		chan->out_mbuf_cur_len = 0;
	}

	/* call timeout handling routine */

	if(activity == ACT_RX || activity == ACT_TX)
		(*chan->l4_driver->bch_activity)(chan->l4_driver_softc, activity);

	if(cmd)
	{
		cmd |= B_CMDR_RACT;
		IWIC_WRITE(sc, chan->offset + B_CMDR, cmd);
	}

	splx(s);
}

/*---------------------------------------------------------------------------*
 *	return B-channel statistics
 *---------------------------------------------------------------------------*/
static void
iwic_bchannel_stat(isdn_layer1token t, int h_chan, bchan_statistics_t *bsp)
{
	struct iwic_softc *sc = t;
	struct iwic_bchan *bchan = &sc->sc_bchan[h_chan];

	int s = splnet();

	bsp->outbytes = bchan->txcount;
	bsp->inbytes = bchan->rxcount;

	bchan->txcount = 0;
	bchan->rxcount = 0;

	splx(s);
}

/*---------------------------------------------------------------------------*
 *	initialize our local linktab
 *---------------------------------------------------------------------------*/
static const struct isdn_l4_bchannel_functions iwic_bchan_driver = {
	iwic_bchannel_setup,
	iwic_bchannel_start,
	iwic_bchannel_stat
};

void
iwic_init_linktab(struct iwic_softc *sc)
{
	struct iwic_bchan *chan;
	isdn_link_t *lt;

	/* channel A */

	chan = &sc->sc_bchan[IWIC_BCH_A];
	lt = &chan->iwic_isdn_linktab;

	lt->l1token = sc;
	lt->channel = IWIC_BCH_A;
	lt->bchannel_driver = &iwic_bchan_driver;
	lt->tx_queue = &chan->tx_queue;

	/* used by non-HDLC data transfers, i.e. telephony drivers */
	lt->rx_queue = &chan->rx_queue;

	/* used by HDLC data transfers, i.e. ipr and isp drivers */
	lt->rx_mbuf = &chan->in_mbuf;

	/* channel B */

	chan = &sc->sc_bchan[IWIC_BCH_B];
	lt = &chan->iwic_isdn_linktab;

	lt->l1token = sc;
	lt->channel = IWIC_BCH_B;
	lt->bchannel_driver = &iwic_bchan_driver;
	lt->tx_queue = &chan->tx_queue;

	/* used by non-HDLC data transfers, i.e. telephony drivers */
	lt->rx_queue = &chan->rx_queue;

	/* used by HDLC data transfers, i.e. ipr and isp drivers */
	lt->rx_mbuf = &chan->in_mbuf;
}

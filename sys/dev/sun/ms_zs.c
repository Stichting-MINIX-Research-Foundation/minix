/*	$NetBSD: ms_zs.c,v 1.20 2013/11/07 02:38:07 christos Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ms.c	8.1 (Berkeley) 6/11/93
 */

/*
 * Mouse driver (/dev/mouse)
 */

/*
 * Zilog Z8530 Dual UART driver (mouse interface)
 *
 * This is the "slave" driver that will be attached to
 * the "zsc" driver for a Sun mouse.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ms_zs.c,v 1.20 2013/11/07 02:38:07 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/syslog.h>

#include <machine/vuid_event.h>

#include <dev/ic/z8530reg.h>
#include <machine/z8530var.h>
#include <dev/sun/event_var.h>
#include <dev/sun/msvar.h>

static void ms_zs_rxint(struct zs_chanstate *);
static void ms_zs_stint(struct zs_chanstate *, int);
static void ms_zs_txint(struct zs_chanstate *);
static void ms_zs_softint(struct zs_chanstate *);

struct zsops zsops_ms = {
	ms_zs_rxint,	/* receive char available */
	ms_zs_stint,	/* external/status */
	ms_zs_txint,	/* xmit buffer empty */
	ms_zs_softint,	/* process software interrupt */
};

/* Fall-back baud rate */
#ifdef SUN_MS_BPS
int	ms_zs_bps = SUN_MS_BPS;
#else
int	ms_zs_bps = MS_DEFAULT_BPS;
#endif

static int	ms_zs_match(device_t, cfdata_t, void *);
static void	ms_zs_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(ms_zs, sizeof(struct ms_softc),
    ms_zs_match, ms_zs_attach, NULL, NULL);

/*
 * ms_match: how is this zs channel configured?
 */
int
ms_zs_match(device_t parent, cfdata_t cf, void *aux)
{
	struct zsc_attach_args *args = aux;

	if (ms_zs_bps == 0)
		return 0;

	/* Exact match required for keyboard. */
	if (cf->cf_loc[ZSCCF_CHANNEL] == args->channel)
		return 2;

	return 0;
}

void
ms_zs_attach(device_t parent, device_t self, void *aux)

{
	struct zsc_softc *zsc = device_private(parent);
	struct ms_softc *ms = device_private(self);
	struct zsc_attach_args *args = aux;
	struct zs_chanstate *cs;
	int channel;
	int reset, s;
	int bps;

	ms->ms_dev = self;
	channel = args->channel;
	cs = zsc->zsc_cs[channel];
	cs->cs_private = ms;
	cs->cs_ops = &zsops_ms;
	ms->ms_cs = cs;
	/* Allow kernel option SUN_MS_BPS to hard-code baud rate */
#ifndef SUN_MS_BPS
	if ((bps = cs->cs_defspeed) == 0)
#endif
		bps = ms_zs_bps;

	aprint_normal(": baud rate %d\n", bps);

	/* Initialize the speed, etc. */
	s = splzs();
	/* May need reset... */
	reset = (channel == 0) ?
	    ZSWR9_A_RESET : ZSWR9_B_RESET;
	zs_write_reg(cs, 9, reset);
	/* These are OK as set by zscc: WR3, WR4, WR5 */
	/* We don't care about status or tx interrupts. */
	cs->cs_preg[1] = ZSWR1_RIE;
	(void)zs_set_speed(cs, bps);
	zs_loadchannelregs(cs);
	splx(s);

	/* Initialize translator. */
	ms->ms_byteno = -1;
}

/****************************************************************
 * Interface to the lower layer (zscc)
 ****************************************************************/

static void
ms_zs_rxint(struct zs_chanstate *cs)
{
	struct ms_softc *ms;
	int put, put_next;
	uint8_t c, rr1;

	ms = cs->cs_private;
	put = ms->ms_rbput;

	/*
	 * First read the status, because reading the received char
	 * destroys the status of this char.
	 */
	rr1 = zs_read_reg(cs, 1);
	c = zs_read_data(cs);

	if (rr1 & (ZSRR1_FE | ZSRR1_DO | ZSRR1_PE)) {
		/* Clear the receive error. */
		zs_write_csr(cs, ZSWR0_RESET_ERRORS);
	}

	ms->ms_rbuf[put] = (c << 8) | rr1;
	put_next = (put + 1) & MS_RX_RING_MASK;

	/* Would overrun if increment makes (put==get). */
	if (put_next == ms->ms_rbget) {
		ms->ms_intr_flags |= INTR_RX_OVERRUN;
	} else {
		/* OK, really increment. */
		put = put_next;
	}

	/* Done reading. */
	ms->ms_rbput = put;

	/* Ask for softint() call. */
	cs->cs_softreq = 1;
}

static void
ms_zs_txint(struct zs_chanstate *cs)
{
	struct ms_softc *ms;

	ms = cs->cs_private;
	zs_write_csr(cs, ZSWR0_RESET_TXINT);
	ms->ms_intr_flags |= INTR_TX_EMPTY;
	/* Ask for softint() call. */
	cs->cs_softreq = 1;
}

static void
ms_zs_stint(struct zs_chanstate *cs, int force)
{
	struct ms_softc *ms;
	uint8_t rr0;

	ms = cs->cs_private;

	rr0 = zs_read_csr(cs);
	zs_write_csr(cs, ZSWR0_RESET_STATUS);

	/*
	 * We have to accumulate status line changes here.
	 * Otherwise, if we get multiple status interrupts
	 * before the softint runs, we could fail to notice
	 * some status line changes in the softint routine.
	 * Fix from Bill Studenmund, October 1996.
	 */
	cs->cs_rr0_delta |= (cs->cs_rr0 ^ rr0);
	cs->cs_rr0 = rr0;
	ms->ms_intr_flags |= INTR_ST_CHECK;

	/* Ask for softint() call. */
	cs->cs_softreq = 1;
}

static void
ms_zs_softint(struct zs_chanstate *cs)
{
	struct ms_softc *ms;
	int get, c, s;
	int intr_flags;
	uint16_t ring_data;

	ms = cs->cs_private;

	/* Atomically get and clear flags. */
	s = splzs();
	intr_flags = ms->ms_intr_flags;
	ms->ms_intr_flags = 0;

	/* Now lower to spltty for the rest. */
	(void)spltty();

	/*
	 * Copy data from the receive ring to the event layer.
	 */
	get = ms->ms_rbget;
	while (get != ms->ms_rbput) {
		ring_data = ms->ms_rbuf[get];
		get = (get + 1) & MS_RX_RING_MASK;

		/* low byte of ring_data is rr1 */
		c = (ring_data >> 8) & 0xff;

		if (ring_data & ZSRR1_DO)
			intr_flags |= INTR_RX_OVERRUN;
		if (ring_data & (ZSRR1_FE | ZSRR1_PE)) {
			log(LOG_ERR, "%s: input error (0x%x)\n",
			    device_xname(ms->ms_dev), ring_data);
			c = -1;	/* signal input error */
		}

		/* Pass this up to the "middle" layer. */
		ms_input(ms, c);
	}
	if (intr_flags & INTR_RX_OVERRUN) {
		log(LOG_ERR, "%s: input overrun\n",
		    device_xname(ms->ms_dev));
	}
	ms->ms_rbget = get;

	if (intr_flags & INTR_TX_EMPTY) {
		/*
		 * Transmit done.  (Not expected.)
		 */
		log(LOG_ERR, "%s: transmit interrupt?\n",
		    device_xname(ms->ms_dev));
	}

	if (intr_flags & INTR_ST_CHECK) {
		/*
		 * Status line change.  (Not expected.)
		 */
		log(LOG_ERR, "%s: status interrupt?\n",
		    device_xname(ms->ms_dev));
		cs->cs_rr0_delta = 0;
	}

	splx(s);
}

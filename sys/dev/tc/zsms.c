/*	$NetBSD: zsms.c,v 1.18 2009/05/12 14:47:05 cegger Exp $	*/

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
 * VSXXX mice attached with channel A of the 1st SCC
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: zsms.c,v 1.18 2009/05/12 14:47:05 cegger Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/tty.h>

#include <dev/ic/z8530reg.h>
#include <machine/z8530var.h>

#include <dev/dec/lk201.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

#include "locators.h"

/*
 * How many input characters we can buffer.
 * The port-specific var.h may override this.
 * Note: must be a power of two!
 */
#define	ZSMS_RX_RING_SIZE	256
#define ZSMS_RX_RING_MASK (ZSMS_RX_RING_SIZE-1)
/*
 * Output buffer.  Only need a few chars.
 */
#define	ZSMS_TX_RING_SIZE	16
#define ZSMS_TX_RING_MASK (ZSMS_TX_RING_SIZE-1)

#define ZSMS_BPS 4800

struct zsms_softc {		/* driver status information */
	device_t zsms_dev;	/* required first: base device */
	struct	zs_chanstate *zsms_cs;

	/* Flags to communicate with zsms_softintr() */
	volatile int zsms_intr_flags;
#define	INTR_RX_OVERRUN 1
#define INTR_TX_EMPTY   2
#define INTR_ST_CHECK   4

	/*
	 * The receive ring buffer.
	 */
	u_int	zsms_rbget;	/* ring buffer `get' index */
	volatile u_int	zsms_rbput;	/* ring buffer `put' index */
	u_short	zsms_rbuf[ZSMS_RX_RING_SIZE]; /* rr1, data pairs */

	int sc_enabled;		/* input enabled? */
	int sc_selftest;	/* self test in progress */

	int inputstate;
	u_int buttons;
	int dx, dy;

	device_t sc_wsmousedev;
};

static struct zsops zsops_zsms;

static int  zsms_match(device_t, cfdata_t, void *);
static void zsms_attach(device_t, device_t, void *);
static void zsms_input(void *, int);

CFATTACH_DECL_NEW(zsms, sizeof(struct zsms_softc),
    zsms_match, zsms_attach, NULL, NULL);

static int  zsms_enable(void *);
static int  zsms_ioctl(void *, u_long, void *, int, struct lwp *);
static void zsms_disable(void *);

static const struct wsmouse_accessops zsms_accessops = {
	zsms_enable,
	zsms_ioctl,
	zsms_disable,
};

static int
zsms_match(device_t parent, cfdata_t cf, void *aux)
{
	struct zsc_attach_args *args = aux;

	/* Exact match is better than wildcard. */
	if (cf->cf_loc[ZSCCF_CHANNEL] == args->channel)
		return 2;

	/* This driver accepts wildcard. */
	if (cf->cf_loc[ZSCCF_CHANNEL] == ZSCCF_CHANNEL_DEFAULT)
		return 1;

	return 0;
}

static void
zsms_attach(device_t parent, device_t self, void *aux)
{
	struct zsc_softc *zsc = device_private(parent);
	struct zsms_softc *zsms = device_private(self);
	struct zsc_attach_args *args = aux;
	struct zs_chanstate *cs;
	struct wsmousedev_attach_args a;
	int s;

	zsms->zsms_dev = self;
	cs = zsc->zsc_cs[args->channel];
	cs->cs_private = zsms;
	cs->cs_ops = &zsops_zsms;
	zsms->zsms_cs = cs;

	aprint_normal("\n");

	/* Initialize the speed, etc. */
	s = splzs();
	/* May need reset... */
	zs_write_reg(cs, 9, ZSWR9_A_RESET);
	/* These are OK as set by zscc: WR3, WR5 */
	/* We don't care about status or tx interrupts. */
	cs->cs_preg[1] = ZSWR1_RIE;
	(void) zs_set_speed(cs, ZSMS_BPS);

	/* mouse wants odd parity */
	cs->cs_preg[4] |= ZSWR4_PARENB;
	/* cs->cs_preg[4] &= ~ZSWR4_EVENP; (no-op) */

	zs_loadchannelregs(cs);
	splx(s);

	a.accessops = &zsms_accessops;
	a.accesscookie = zsms;

	zsms->sc_enabled = 0;
	zsms->sc_selftest = 0;
	zsms->sc_wsmousedev = config_found(self, &a, wsmousedevprint);
}

static int
zsms_enable(void *v)
{
	struct zsms_softc *sc = v;

	if (sc->sc_enabled)
		return EBUSY;

	sc->sc_selftest = 4; /* wait for 4 byte reply upto 1/2 sec */
	zs_write_data(sc->zsms_cs,  MOUSE_SELF_TEST);
	(void)tsleep(zsms_enable, TTIPRI, "zsmsopen", hz / 2);
	if (sc->sc_selftest != 0) {
		sc->sc_selftest = 0;
		return ENXIO;
	}
	/* XXX DELAY before mode set? */
	zs_write_data(sc->zsms_cs, MOUSE_INCREMENTAL);
	sc->sc_enabled = 1;
	sc->inputstate = 0;
	return 0;
}

static void
zsms_disable(void *v)
{
	struct zsms_softc *sc = v;

	sc->sc_enabled = 0;
}

static int
zsms_ioctl(void *v, u_long cmd, void *data, int flag, struct lwp *l)
{

	if (cmd == WSMOUSEIO_GTYPE) {
		*(u_int *)data = WSMOUSE_TYPE_VSXXX;
		return 0;
	}
	return EPASSTHROUGH;
}

static void
zsms_input(void *vsc, int data)
{
	struct zsms_softc *sc = vsc;

	if (sc->sc_enabled == 0) {
		if (sc->sc_selftest > 0) {
			sc->sc_selftest -= 1;
			if (sc->sc_selftest == 0)
				wakeup(zsms_enable);
		}
		return;
	}

	if ((data & MOUSE_START_FRAME) != 0)
		sc->inputstate = 1;
	else
		sc->inputstate++;

	if (sc->inputstate == 1) {
		/* LMR -> RML: wsevents counts 0 for the left-most */
		sc->buttons = data & 02;
		if (data & 01)
			sc->buttons |= 04;
		if (data & 04)
			sc->buttons |= 01;
		sc->dx = data & MOUSE_X_SIGN;
		sc->dy = data & MOUSE_Y_SIGN;
	} else if (sc->inputstate == 2) {
		if (sc->dx == 0)
			sc->dx = -data;
		else
			sc->dx = data;
	} else if (sc->inputstate == 3) {
		sc->inputstate = 0;
		if (sc->dy == 0)
			sc->dy = -data;
		else
			sc->dy = data;
		wsmouse_input(sc->sc_wsmousedev,
				sc->buttons,
		    		sc->dx, sc->dy, 0, 0,
				WSMOUSE_INPUT_DELTA);
	}

	return;
}

/****************************************************************
 * Interface to the lower layer (zscc)
 ****************************************************************/

static void zsms_rxint(struct zs_chanstate *);
static void zsms_stint(struct zs_chanstate *, int);
static void zsms_txint(struct zs_chanstate *);
static void zsms_softint(struct zs_chanstate *);

static void
zsms_rxint(struct zs_chanstate *cs)
{
	struct zsms_softc *zsms;
	int put, put_next;
	uint8_t c, rr1;

	zsms = cs->cs_private;
	put = zsms->zsms_rbput;

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

	zsms->zsms_rbuf[put] = (c << 8) | rr1;
	put_next = (put + 1) & ZSMS_RX_RING_MASK;

	/* Would overrun if increment makes (put==get). */
	if (put_next == zsms->zsms_rbget) {
		zsms->zsms_intr_flags |= INTR_RX_OVERRUN;
	} else {
		/* OK, really increment. */
		put = put_next;
	}

	/* Done reading. */
	zsms->zsms_rbput = put;

	/* Ask for softint() call. */
	cs->cs_softreq = 1;
}


static void
zsms_txint(struct zs_chanstate *cs)
{
	struct zsms_softc *zsms;

	zsms = cs->cs_private;
	zs_write_csr(cs, ZSWR0_RESET_TXINT);
	zsms->zsms_intr_flags |= INTR_TX_EMPTY;
	/* Ask for softint() call. */
	cs->cs_softreq = 1;
}


static void
zsms_stint(struct zs_chanstate *cs, int force)
{
	struct zsms_softc *zsms;
	int rr0;

	zsms = cs->cs_private;

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
	zsms->zsms_intr_flags |= INTR_ST_CHECK;

	/* Ask for softint() call. */
	cs->cs_softreq = 1;
}


static void
zsms_softint(struct zs_chanstate *cs)
{
	struct zsms_softc *zsms;
	int get, c, s;
	int intr_flags;
	u_short ring_data;

	zsms = cs->cs_private;

	/* Atomically get and clear flags. */
	s = splzs();
	intr_flags = zsms->zsms_intr_flags;
	zsms->zsms_intr_flags = 0;

	/* Now lower to spltty for the rest. */
	(void) spltty();

	/*
	 * Copy data from the receive ring to the event layer.
	 */
	get = zsms->zsms_rbget;
	while (get != zsms->zsms_rbput) {
		ring_data = zsms->zsms_rbuf[get];
		get = (get + 1) & ZSMS_RX_RING_MASK;

		/* low byte of ring_data is rr1 */
		c = (ring_data >> 8) & 0xff;

		if (ring_data & ZSRR1_DO)
			intr_flags |= INTR_RX_OVERRUN;
		if (ring_data & (ZSRR1_FE | ZSRR1_PE)) {
			log(LOG_ERR, "%s: input error (0x%x)\n",
			    device_xname(zsms->zsms_dev), ring_data);
			c = -1;	/* signal input error */
		}

		/* Pass this up to the "middle" layer. */
		zsms_input(zsms, c);
	}
	if (intr_flags & INTR_RX_OVERRUN) {
		log(LOG_ERR, "%s: input overrun\n",
		    device_xname(zsms->zsms_dev));
	}
	zsms->zsms_rbget = get;

	if (intr_flags & INTR_TX_EMPTY) {
		/*
		 * Transmit done.  (Not expected.)
		 */
		log(LOG_ERR, "%s: transmit interrupt?\n",
		    device_xname(zsms->zsms_dev));
	}

	if (intr_flags & INTR_ST_CHECK) {
		/*
		 * Status line change.  (Not expected.)
		 */
		log(LOG_ERR, "%s: status interrupt?\n",
		    device_xname(zsms->zsms_dev));
		cs->cs_rr0_delta = 0;
	}

	splx(s);
}

static struct zsops zsops_zsms = {
	zsms_rxint,	/* receive char available */
	zsms_stint,	/* external/status */
	zsms_txint,	/* xmit buffer empty */
	zsms_softint,	/* process software interrupt */
};

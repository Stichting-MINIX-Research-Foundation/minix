/*	$NetBSD: zskbd.c,v 1.18 2015/01/02 21:32:26 jklos Exp $	*/

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
 *	@(#)kbd.c	8.2 (Berkeley) 10/30/93
 */

/*
 * LK200/LK400 keyboard attached with channel A of the 2nd SCC
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: zskbd.c,v 1.18 2015/01/02 21:32:26 jklos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/malloc.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>
#include <dev/dec/wskbdmap_lk201.h>

#include <dev/ic/z8530reg.h>
#include <machine/z8530var.h>

#include <dev/tc/tcvar.h>
#include <dev/tc/zs_ioasicvar.h>
#include <dev/dec/lk201reg.h>
#include <dev/dec/lk201var.h>

#include "locators.h"

/*
 * How many input characters we can buffer.
 * The port-specific var.h may override this.
 * Note: must be a power of two!
 */
#define	ZSKBD_RX_RING_SIZE	256
#define ZSKBD_RX_RING_MASK (ZSKBD_RX_RING_SIZE-1)
/*
 * Output buffer.  Only need a few chars.
 */
#define	ZSKBD_TX_RING_SIZE	16
#define ZSKBD_TX_RING_MASK (ZSKBD_TX_RING_SIZE-1)

#define ZSKBD_BPS 4800

struct zskbd_internal {
	struct zs_chanstate *zsi_cs;
	struct lk201_state zsi_ks;
};

static struct zskbd_internal zskbd_console_internal;

struct zskbd_softc {
	device_t zskbd_dev;	/* required first: base device */

	struct zskbd_internal *sc_itl;

	/* Flags to communicate with zskbd_softintr() */
	volatile int zskbd_intr_flags;
#define	INTR_RX_OVERRUN 1
#define INTR_TX_EMPTY   2
#define INTR_ST_CHECK   4

	/*
	 * The receive ring buffer.
	 */
	u_int	zskbd_rbget;	/* ring buffer `get' index */
	volatile u_int	zskbd_rbput;	/* ring buffer `put' index */
	u_short	zskbd_rbuf[ZSKBD_RX_RING_SIZE]; /* rr1, data pairs */

	int sc_enabled;
	int kbd_type;

	device_t sc_wskbddev;
};

static struct zsops zsops_zskbd;

static void	zskbd_input(struct zskbd_softc *, int);

static int	zskbd_match(device_t, cfdata_t, void *);
static void	zskbd_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(zskbd, sizeof(struct zskbd_softc),
    zskbd_match, zskbd_attach, NULL, NULL);

static int	zskbd_enable(void *, int);
static void	zskbd_set_leds(void *, int);
static int	zskbd_ioctl(void *, u_long, void *, int, struct lwp *);

static const struct wskbd_accessops zskbd_accessops = {
	zskbd_enable,
	zskbd_set_leds,
	zskbd_ioctl,
};

static void	zskbd_cngetc(void *, u_int *, int *);
static void	zskbd_cnpollc(void *, int);

static const struct wskbd_consops zskbd_consops = {
	zskbd_cngetc,
	zskbd_cnpollc,
};

static int	zskbd_sendchar(void *, u_char);

static const struct wskbd_mapdata zskbd_keymapdata = {
	lkkbd_keydesctab,
#ifdef ZSKBD_LAYOUT
	ZSKBD_LAYOUT,
#else
	KB_US | KB_LK401,
#endif
};

int zskbd_cnattach(struct zs_chanstate *);	/* EXPORTED */

/*
 * kbd_match: how is this zs channel configured?
 */
static int
zskbd_match(device_t parent, cfdata_t cf, void *aux)
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
zskbd_attach(device_t parent, device_t self, void *aux)
{
	struct zsc_softc *zsc = device_private(parent);
	struct zskbd_softc *zskbd = device_private(self);
	struct zsc_attach_args *args = aux;
	struct zs_chanstate *cs;
	struct zskbd_internal *zsi;
	struct wskbddev_attach_args a;
	int s, isconsole;

	zskbd->zskbd_dev = self;

	cs = zsc->zsc_cs[args->channel];
	cs->cs_private = zskbd;
	cs->cs_ops = &zsops_zskbd;

	isconsole = (args->hwflags & ZS_HWFLAG_CONSOLE);

	if (isconsole) {
		zsi = &zskbd_console_internal;
	} else {
		zsi = malloc(sizeof(struct zskbd_internal),
				       M_DEVBUF, M_NOWAIT);
		zsi->zsi_ks.attmt.sendchar = zskbd_sendchar;
		zsi->zsi_ks.attmt.cookie = cs;
		zsi->zsi_cs = cs;
	}
	zskbd->sc_itl = zsi;

	aprint_normal("\n");

	/* Initialize the speed, etc. */
	s = splzs();
	/* May need reset... */
	zs_write_reg(cs, 9, ZSWR9_A_RESET);
	/* These are OK as set by zscc: WR3, WR4, WR5 */
	/* We don't care about status or tx interrupts. */
	cs->cs_preg[1] = ZSWR1_RIE;
	(void) zs_set_speed(cs, ZSKBD_BPS);
	zs_loadchannelregs(cs);
	splx(s);

	if (!isconsole)
		lk201_init(&zsi->zsi_ks);

	/* XXX should identify keyboard ID here XXX */
	/* XXX layout and the number of LED is varying XXX */

	zskbd->kbd_type = WSKBD_TYPE_LK201;

	zskbd->sc_enabled = 1;

	a.console = isconsole;
	a.keymap = &zskbd_keymapdata;
	a.accessops = &zskbd_accessops;
	a.accesscookie = zskbd;

	zskbd->sc_wskbddev = config_found(self, &a, wskbddevprint);
}

int
zskbd_cnattach(struct zs_chanstate *cs)
{
	(void) zs_set_speed(cs, ZSKBD_BPS);
	zs_loadchannelregs(cs);

	zskbd_console_internal.zsi_ks.attmt.sendchar = zskbd_sendchar;
	zskbd_console_internal.zsi_ks.attmt.cookie = cs;
	lk201_init(&zskbd_console_internal.zsi_ks);
	zskbd_console_internal.zsi_cs = cs;

	wskbd_cnattach(&zskbd_consops, &zskbd_console_internal,
		       &zskbd_keymapdata);

	return 0;
}

static int
zskbd_enable(void *v, int on)
{
	struct zskbd_softc *sc = v;

	sc->sc_enabled = on;
	return 0;
}

static int
zskbd_sendchar(void *v, u_char c)
{
	struct zs_chanstate *cs = v;
	zs_write_data(cs, c);
	DELAY(4000);

	return (0);
}

static void
zskbd_cngetc(void *v, u_int *type, int *data)
{
	struct zskbd_internal *zsi = v;
	int c;

	do {
		c = zs_getc(zsi->zsi_cs);
	} while (!lk201_decode(&zsi->zsi_ks, 0, c, type, data) == LKD_NODATA);
}

static void
zskbd_cnpollc(void *v, int on)
{
#if 0
	struct zskbd_internal *zsi = v;
#endif
}

static void
zskbd_set_leds(void *v, int leds)
{
	struct zskbd_softc *sc = v;

	lk201_set_leds(&sc->sc_itl->zsi_ks, leds);
}

static int
zskbd_ioctl(void *v, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct zskbd_softc *sc = v;

	switch (cmd) {
	case WSKBDIO_GTYPE:
		*(int *)data = sc->kbd_type;
		return 0;
	case WSKBDIO_SETLEDS:
		lk201_set_leds(&sc->sc_itl->zsi_ks, *(int *)data);
		return 0;
	case WSKBDIO_GETLEDS:
		/* XXX don't dig in kbd internals */
		*(int *)data = sc->sc_itl->zsi_ks.leds_state;
		return 0;
	case WSKBDIO_COMPLEXBELL:
		lk201_bell(&sc->sc_itl->zsi_ks,
			   (struct wskbd_bell_data *)data);
		return 0;
	case WSKBDIO_SETKEYCLICK:
		lk201_set_keyclick(&sc->sc_itl->zsi_ks, *(int *)data);
		return 0;
	case WSKBDIO_GETKEYCLICK:
		/* XXX don't dig in kbd internals */
		*(int *)data = sc->sc_itl->zsi_ks.kcvol;
		return 0;
	}
	return EPASSTHROUGH;
}

static void
zskbd_input(struct zskbd_softc *sc, int data)
{
	u_int type;
	int val;
	int decode;

	do {
		decode = lk201_decode(&sc->sc_itl->zsi_ks, 1,
                    data, &type, &val);
                if (decode != LKD_NODATA)
                        wskbd_input(sc->sc_wskbddev, type, val);
        } while (decode == LKD_MORE);

}

/****************************************************************
 * Interface to the lower layer (zscc)
 ****************************************************************/

static void zskbd_rxint(struct zs_chanstate *);
static void zskbd_stint(struct zs_chanstate *, int);
static void zskbd_txint(struct zs_chanstate *);
static void zskbd_softint(struct zs_chanstate *);

static void
zskbd_rxint(struct zs_chanstate *cs)
{
	struct zskbd_softc *zskbd;
	int put, put_next;
	u_char c, rr1;

	zskbd = cs->cs_private;
	put = zskbd->zskbd_rbput;

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

	zskbd->zskbd_rbuf[put] = (c << 8) | rr1;
	put_next = (put + 1) & ZSKBD_RX_RING_MASK;

	/* Would overrun if increment makes (put==get). */
	if (put_next == zskbd->zskbd_rbget) {
		zskbd->zskbd_intr_flags |= INTR_RX_OVERRUN;
	} else {
		/* OK, really increment. */
		put = put_next;
	}

	/* Done reading. */
	zskbd->zskbd_rbput = put;

	/* Ask for softint() call. */
	cs->cs_softreq = 1;
}


static void
zskbd_txint(struct zs_chanstate *cs)
{
	struct zskbd_softc *zskbd;

	zskbd = cs->cs_private;
	zs_write_csr(cs, ZSWR0_RESET_TXINT);
	zskbd->zskbd_intr_flags |= INTR_TX_EMPTY;
	/* Ask for softint() call. */
	cs->cs_softreq = 1;
}


static void
zskbd_stint(struct zs_chanstate *cs, int force)
{
	struct zskbd_softc *zskbd;
	int rr0;

	zskbd = cs->cs_private;

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
	zskbd->zskbd_intr_flags |= INTR_ST_CHECK;

	/* Ask for softint() call. */
	cs->cs_softreq = 1;
}


static void
zskbd_softint(struct zs_chanstate *cs)
{
	struct zskbd_softc *zskbd;
	int get, c, s;
	int intr_flags;
	u_short ring_data;

	zskbd = cs->cs_private;

	/* Atomically get and clear flags. */
	s = splzs();
	intr_flags = zskbd->zskbd_intr_flags;
	zskbd->zskbd_intr_flags = 0;

	/* Now lower to spltty for the rest. */
	(void) spltty();

	/*
	 * Copy data from the receive ring to the event layer.
	 */
	get = zskbd->zskbd_rbget;
	while (get != zskbd->zskbd_rbput) {
		ring_data = zskbd->zskbd_rbuf[get];
		get = (get + 1) & ZSKBD_RX_RING_MASK;

		/* low byte of ring_data is rr1 */
		c = (ring_data >> 8) & 0xff;

		if (ring_data & ZSRR1_DO)
			intr_flags |= INTR_RX_OVERRUN;
		if (ring_data & (ZSRR1_FE | ZSRR1_PE)) {
#if 0 /* XXX */
			log(LOG_ERR, "%s: input error (0x%x)\n",
			    device_xname(zskbd->zskbd_dev), ring_data);
			c = -1;	/* signal input error */
#endif
		}

		/* Pass this up to the "middle" layer. */
		zskbd_input(zskbd, c);
	}
	if (intr_flags & INTR_RX_OVERRUN) {
#if 0 /* XXX */
		log(LOG_ERR, "%s: input overrun\n",
		    device_xname(zskbd->zskbd_dev));
#endif
	}
	zskbd->zskbd_rbget = get;

	if (intr_flags & INTR_TX_EMPTY) {
		/*
		 * Transmit done.  (Not expected.)
		 */
#if 0
		log(LOG_ERR, "%s: transmit interrupt?\n",
		    device_xname(zskbd->zskbd_dev));
#endif
	}

	if (intr_flags & INTR_ST_CHECK) {
		/*
		 * Status line change.  (Not expected.)
		 */
		log(LOG_ERR, "%s: status interrupt?\n",
		    device_xname(zskbd->zskbd_dev));
		cs->cs_rr0_delta = 0;
	}

	splx(s);
}

static struct zsops zsops_zskbd = {
	zskbd_rxint,	/* receive char available */
	zskbd_stint,	/* external/status */
	zskbd_txint,	/* xmit buffer empty */
	zskbd_softint,	/* process software interrupt */
};

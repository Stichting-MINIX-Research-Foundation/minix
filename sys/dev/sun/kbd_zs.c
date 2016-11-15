/*	$NetBSD: kbd_zs.c,v 1.23 2008/04/20 15:44:01 tsutsui Exp $	*/

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
 * /dev/kbd lower layer for sun keyboard off a zs channel.
 * This driver uses kbdsun middle layer to hook up to /dev/kbd.
 */

/*
 * Zilog Z8530 Dual UART driver (keyboard interface)
 *
 * This is the 8530 portion of the driver that will be attached to
 * the "zsc" driver for a Sun keyboard.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kbd_zs.c,v 1.23 2008/04/20 15:44:01 tsutsui Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/syslog.h>

#include <dev/ic/z8530reg.h>
#include <machine/z8530var.h>
#include <dev/sun/vuid_event.h>
#include <dev/sun/event_var.h>
#include <dev/sun/kbd_reg.h>
#include <dev/sun/kbd_xlate.h>
#include <dev/sun/kbdvar.h>
#include <dev/sun/kbdsunvar.h>

#if NWSKBD > 0
void kbd_wskbd_attach(struct kbd_softc *k, int isconsole);
#endif

/****************************************************************
 * Interface to the lower layer (zscc)
 ****************************************************************/

static void kbd_zs_rxint(struct zs_chanstate *);
static void kbd_zs_stint(struct zs_chanstate *, int);
static void kbd_zs_txint(struct zs_chanstate *);
static void kbd_zs_softint(struct zs_chanstate *);

struct zsops zsops_kbd = {
	kbd_zs_rxint,	/* receive char available */
	kbd_zs_stint,	/* external/status */
	kbd_zs_txint,	/* xmit buffer empty */
	kbd_zs_softint,	/* process software interrupt */
};

static int	kbd_zs_match(device_t, cfdata_t, void *);
static void	kbd_zs_attach(device_t, device_t, void *);
static void	kbd_zs_write_data(struct kbd_sun_softc *, int);

CFATTACH_DECL_NEW(kbd_zs, sizeof(struct kbd_sun_softc),
    kbd_zs_match, kbd_zs_attach, NULL, NULL);

/* Fall-back baud rate */
int	kbd_zs_bps = KBD_DEFAULT_BPS;

/*
 * kbd_zs_match: how is this zs channel configured?
 */
int
kbd_zs_match(device_t parent, cfdata_t cf, void *aux)
{
	struct zsc_attach_args *args = aux;

	/* Exact match required for keyboard. */
	if (cf->cf_loc[ZSCCF_CHANNEL] == args->channel)
		return 2;

	return 0;
}

void
kbd_zs_attach(device_t parent, device_t self, void *aux)
{
	struct kbd_sun_softc *k = device_private(self);
	struct zsc_softc *zsc = device_private(parent);
	struct zsc_attach_args *args = aux;
	struct zs_chanstate *cs;
	int channel;
	int reset, s;
	int bps;

	k->k_kbd.k_dev = self;

	/* provide upper layer with a link to the middle layer */
	k->k_kbd.k_ops = &kbd_ops_sun;

	/* provide middle layer with a link to the lower layer (i.e. us) */
	channel = args->channel;
	cs = zsc->zsc_cs[channel];
	cs->cs_private = k;
	cs->cs_ops = &zsops_kbd;
	k->k_cs = cs;
	k->k_write_data = kbd_zs_write_data;

	if ((bps = cs->cs_defspeed) == 0)
		bps = kbd_zs_bps;

	aprint_normal(": baud rate %d", bps);

	if ((args->hwflags & ZS_HWFLAG_CONSOLE_INPUT) != 0) {
		/*
		 * Hookup ourselves as the console input channel
		 */
		struct cons_channel *cc = kbd_cc_alloc(&k->k_kbd);

		if (cc == NULL)
			return;

		cons_attach_input(cc, args->consdev);
		k->k_kbd.k_isconsole = 1;
		aprint_normal(" (console input)");
	}
	aprint_normal("\n");

	/* Initialize the speed, etc. */
	s = splzs();
	if (k->k_kbd.k_isconsole == 0) {
		/* Not the console; may need reset. */
		reset = (channel == 0) ?
		    ZSWR9_A_RESET : ZSWR9_B_RESET;
		zs_write_reg(cs, 9, reset);
	}
	/* These are OK as set by zscc: WR3, WR4, WR5 */
	/* We don't care about status interrupts. */
	cs->cs_preg[1] = ZSWR1_RIE | ZSWR1_TIE;
	(void) zs_set_speed(cs, bps);
	zs_loadchannelregs(cs);
	splx(s);

	/* Do this before any calls to kbd_rint(). */
	kbd_xlate_init(&k->k_kbd.k_state);

	/* Magic sequence. */
	k->k_magic1 = KBD_L1;
	k->k_magic2 = KBD_A;
#if NWSKBD > 0
	kbd_wskbd_attach(&k->k_kbd, k->k_kbd.k_isconsole);
#endif
}

/*
 * used by kbd_sun_start_tx();
 */
void
kbd_zs_write_data(struct kbd_sun_softc *k, int c)
{
	int s;

	/* Need splzs to avoid interruption of the delay. */
	s = splzs();
	zs_write_data(k->k_cs, c);
	splx(s);
}

static void
kbd_zs_rxint(struct zs_chanstate *cs)
{
	struct kbd_sun_softc *k;
	int put, put_next;
	uint8_t c, rr1;

	k = cs->cs_private;
	put = k->k_rbput;

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

	/*
	 * Check NOW for a console abort sequence, so that we can
	 * abort even when interrupts are locking up the machine.
	 */
	if (k->k_magic1_down) {
		/* The last keycode was "MAGIC1" down. */
		k->k_magic1_down = 0;
		if (c == k->k_magic2) {
			/* Magic "L1-A" sequence; enter debugger. */
			if (k->k_kbd.k_isconsole) {
				zs_abort(cs);
				/* Debugger done.  Fake L1-up to finish it. */
				c = k->k_magic1 | KBD_UP;
			} else {
				printf("%s: magic sequence, but not console\n",
				    device_xname(k->k_kbd.k_dev));
			}
		}
	}
	if (c == k->k_magic1) {
		k->k_magic1_down = 1;
	}

	k->k_rbuf[put] = (c << 8) | rr1;
	put_next = (put + 1) & KBD_RX_RING_MASK;

	/* Would overrun if increment makes (put==get). */
	if (put_next == k->k_rbget) {
		k->k_intr_flags |= INTR_RX_OVERRUN;
	} else {
		/* OK, really increment. */
		put = put_next;
	}

	/* Done reading. */
	k->k_rbput = put;

	/* Ask for softint() call. */
	cs->cs_softreq = 1;
}


static void
kbd_zs_txint(struct zs_chanstate *cs)
{
	struct kbd_sun_softc *k;

	k = cs->cs_private;
	zs_write_csr(cs, ZSWR0_RESET_TXINT);
	k->k_intr_flags |= INTR_TX_EMPTY;
	/* Ask for softint() call. */
	cs->cs_softreq = 1;
}


static void
kbd_zs_stint(struct zs_chanstate *cs, int force)
{
	struct kbd_sun_softc *k;
	uint8_t rr0;

	k = cs->cs_private;

	rr0 = zs_read_csr(cs);
	zs_write_csr(cs, ZSWR0_RESET_STATUS);

#if 0
	if (rr0 & ZSRR0_BREAK) {
		/* Keyboard unplugged? */
		zs_abort(cs);
		return;
	}
#endif

	/*
	 * We have to accumulate status line changes here.
	 * Otherwise, if we get multiple status interrupts
	 * before the softint runs, we could fail to notice
	 * some status line changes in the softint routine.
	 * Fix from Bill Studenmund, October 1996.
	 */
	cs->cs_rr0_delta |= (cs->cs_rr0 ^ rr0);
	cs->cs_rr0 = rr0;
	k->k_intr_flags |= INTR_ST_CHECK;

	/* Ask for softint() call. */
	cs->cs_softreq = 1;
}

/*
 * Get input from the receive ring and pass it on.
 * Note: this is called at splsoftclock()
 */
static void
kbd_zs_softint(struct zs_chanstate *cs)
{
	struct kbd_sun_softc *k;
	int get, c, s;
	int intr_flags;
	uint16_t ring_data;

	k = cs->cs_private;

	/* Atomically get and clear flags. */
	s = splzs();
	intr_flags = k->k_intr_flags;
	k->k_intr_flags = 0;

	/* Now lower to spltty for the rest. */
	(void)spltty();

	/*
	 * Copy data from the receive ring to the event layer.
	 */
	get = k->k_rbget;
	while (get != k->k_rbput) {
		ring_data = k->k_rbuf[get];
		get = (get + 1) & KBD_RX_RING_MASK;

		/* low byte of ring_data is rr1 */
		c = (ring_data >> 8) & 0xff;

		if (ring_data & ZSRR1_DO)
			intr_flags |= INTR_RX_OVERRUN;
		if (ring_data & (ZSRR1_FE | ZSRR1_PE)) {
			/*
			 * After garbage, flush pending input, and
			 * send a reset to resync key translation.
			 */
			log(LOG_ERR, "%s: input error (0x%x)\n",
			    device_xname(k->k_kbd.k_dev), ring_data);
			get = k->k_rbput; /* flush */
			goto send_reset;
		}

		/* Pass this up to the "middle" layer. */
		kbd_sun_input(k, c);
	}
	if (intr_flags & INTR_RX_OVERRUN) {
		log(LOG_ERR, "%s: input overrun\n",
		    device_xname(k->k_kbd.k_dev));
	send_reset:
		/* Send a reset to resync translation. */
		kbd_sun_output(k, KBD_CMD_RESET);
		kbd_sun_start_tx(k);
	}
	k->k_rbget = get;

	if (intr_flags & INTR_TX_EMPTY) {
		/*
		 * Transmit done.  Try to send more, or
		 * clear busy and wakeup drain waiters.
		 */
		k->k_txflags &= ~K_TXBUSY;
		kbd_sun_start_tx(k);
	}

	if (intr_flags & INTR_ST_CHECK) {
		/*
		 * Status line change.  (Not expected.)
		 */
		log(LOG_ERR, "%s: status interrupt?\n",
		    device_xname(k->k_kbd.k_dev));
		cs->cs_rr0_delta = 0;
	}

	splx(s);
}

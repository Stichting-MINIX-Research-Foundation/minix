/*	$NetBSD: kbdsun.c,v 1.11 2008/03/29 19:15:36 tsutsui Exp $	*/
/*	NetBSD: kbd.c,v 1.29 2001/11/13 06:54:32 lukem Exp	*/

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
 * /dev/kbd middle layer for sun keyboard off a serial line
 * This code is used by kbd_zs and sunkbd drivers (lower layer).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kbdsun.c,v 1.11 2008/03/29 19:15:36 tsutsui Exp $");

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
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <sys/file.h>

#include <dev/sun/kbd_reg.h>
#include <dev/sun/kbio.h>
#include <dev/sun/vuid_event.h>
#include <dev/sun/event_var.h>
#include <dev/sun/kbd_xlate.h>

#include <dev/sun/kbdvar.h>
#include <dev/sun/kbdsunvar.h>


/* callbacks for the upper /dev/kbd layer */
static int	kbd_sun_open(struct kbd_softc *);
static int	kbd_sun_close(struct kbd_softc *);
static int	kbd_sun_do_cmd(struct kbd_softc *, int, int);
static int	kbd_sun_set_leds(struct kbd_softc *, int, int);

static void	kbd_sun_set_leds1(struct kbd_softc *, int); /* aux */

const struct kbd_ops kbd_ops_sun = {
	kbd_sun_open,
	kbd_sun_close,
	kbd_sun_do_cmd,
	kbd_sun_set_leds
};

/* in user context, wait for keyboard output to finish */
static int 	kbd_sun_drain_tx(struct kbd_sun_softc *);

/* helper functions for kbd_sun_input */
static void	kbd_sun_was_reset(struct kbd_sun_softc *);
static void	kbd_sun_new_layout(struct kbd_sun_softc *);


/***********************************************************************
 *		      Callbacks for upper layer.
 */

/*
 * Initialization to be done at first open.
 * This is called from kbdopen() or kd_cc_open()
 * Called with user context.
 */
static int
kbd_sun_open(struct kbd_softc *kbd)
{
	struct kbd_sun_softc *k = (struct kbd_sun_softc *)kbd;
	struct kbd_state *ks;
	int error, ntries, s;

	if (kbd == NULL)
		return (ENXIO);

	ks = &kbd->k_state;

	/* tolerate extra calls. */
	if (k->k_isopen)
		return (0);

	/* open internal device */
	if (k->k_deviopen)
		(*k->k_deviopen)(k->k_kbd.k_dev, FREAD|FWRITE);

	s = spltty();

	/* reset the keyboard and find out its type */
	kbd_sun_output(k, KBD_CMD_RESET);
	kbd_sun_start_tx(k);
	kbd_sun_drain_tx(k);

	/* the wakeup for this is in kbd_sun_was_reset(). */
	for (ntries = 30; ntries; ntries--) {
		error = tsleep((void *)&ks->kbd_id, PZERO | PCATCH, devopn,
				hz/10);
		if (ks->kbd_id)
			break;
	}

	if (error == EWOULDBLOCK || ks->kbd_id == 0) { /* no response */
		log(LOG_ERR, "%s: reset failed\n", device_xname(kbd->k_dev));

		/*
		 * Allow the open anyway (to keep getty happy)
		 * but assume the "least common denominator".
		 */
		error = 0;
		ks->kbd_id = KB_SUN2;
	}

	/* earlier than type 4 does not know "layout" */
	if (ks->kbd_id >= KB_SUN4) {
		ks->kbd_layout = 0xff;

		/* ask for the layout */
		kbd_sun_output(k, KBD_CMD_GETLAYOUT);
		kbd_sun_start_tx(k);
		kbd_sun_drain_tx(k);

		/* the wakeup for this is in kbd_sun_new_layout() */
		for (ntries = 200; ntries; ntries--) {
			error = tsleep((void *)&ks->kbd_layout, PZERO | PCATCH,
					devopn, hz);
			if (ks->kbd_layout != 0xff || error)
				break;
			DELAY(10000);
		}
		if (error == EWOULDBLOCK || ks->kbd_layout == 0xff) {
			log(LOG_ERR, "%s: no response to get_layout\n",
			    device_xname(kbd->k_dev));
			error = 0;
			ks->kbd_layout = 0; /* US layout */
		}
	}

	/* initialize the table pointers for this type/layout */
	kbd_xlate_init(ks);

	splx(s);

	if (error == 0)
		k->k_isopen = 1;
	return (error);
}


static int
kbd_sun_close(struct kbd_softc *kbd)
{

	return (0);		/* nothing to do so far */
}


/*
 * keyboard command ioctl
 * ``unimplemented commands are ignored'' (blech)
 * XXX: This is also exported to the fb driver (for bell).
 */
static int
kbd_sun_do_cmd(struct kbd_softc *kbd, int cmd, int isioctl)
{
	struct kbd_sun_softc *k = (struct kbd_sun_softc *)kbd;
	struct kbd_state *ks;
	int error, s;

	error = 0;
	ks = &kbd->k_state;

	switch (cmd) {

	case KBD_CMD_BELL:
	case KBD_CMD_NOBELL:
		/* Supported by type 2, 3, and 4 keyboards */
		break;

	case KBD_CMD_CLICK:
	case KBD_CMD_NOCLICK:
		/* Unsupported by type 2 keyboards */
		if (ks->kbd_id <= KB_SUN2)
			return (0);
		ks->kbd_click = (cmd == KBD_CMD_CLICK);
		break;

	default:
		return (0);
	}

	s = spltty();

	if (isioctl)
		error = kbd_sun_drain_tx(k);

	if (error == 0) {
		kbd_sun_output(k, cmd);
		kbd_sun_start_tx(k);
	}

	splx(s);

	return (error);
}


/*
 * KIOCSLED.  Has user context.
 * Take care about spl and call kbd_sun_set_leds.
 */
static int
kbd_sun_set_leds(struct kbd_softc *kbd, int leds, int isioctl)
{
	struct kbd_sun_softc *k = (struct kbd_sun_softc *)kbd;

	if (isioctl) {
		int error, s;
		s = spltty();
		error = kbd_sun_drain_tx(k);
		if (error == 0) {
			kbd_sun_set_leds1(kbd, leds);
		}
		splx(s);
		return (error);
	}
	else {
		kbd_sun_set_leds1(kbd, leds);
		return (0);
	}
}


/*
 * Safe to call from intterupt handler.  Called at spltty()
 * by kbd_sun_iocsled and kbd_sun_input (via kbd_update_leds).
 */
static void
kbd_sun_set_leds1(struct kbd_softc *kbd, int new_leds)
{
	struct kbd_sun_softc *k = (struct kbd_sun_softc *)kbd;
	struct kbd_state *ks = &kbd->k_state;

	/* Don't send unless state changes. */
	if (ks->kbd_leds == new_leds)
		return;

	ks->kbd_leds = new_leds;

	/* Only type 4 and later has LEDs anyway. */
	if (ks->kbd_id < KB_SUN4)
		return;

	kbd_sun_output(k, KBD_CMD_SETLED);
	kbd_sun_output(k, new_leds);
	kbd_sun_start_tx(k);
}



/***********************************************************************
 *	Methods for lower layer to call and related functions.
 */

/*
 * Enqueue some output for the keyboard
 * Called at spltty().
 */
void
kbd_sun_output(struct kbd_sun_softc *k, int c)
{
	int put;

	put = k->k_tbput;
	k->k_tbuf[put] = (uint8_t)c;
	put = (put + 1) & KBD_TX_RING_MASK;

	/* Would overrun if increment makes (put == get) */
	if (put == k->k_tbget) {
		log(LOG_WARNING, "%s: output overrun\n",
		    device_xname(k->k_kbd.k_dev));
	} else {
		/* OK, really increment. */
		k->k_tbput = put;
	}
}


/*
 * In user context.  Called at spltty().
 * Wait for output to keyboard to finish.
 */
static int
kbd_sun_drain_tx(struct kbd_sun_softc *k)
{
	int error = 0, bail = 0;

	while ((k->k_txflags & K_TXBUSY) && (!error) && (bail<1000)) {
		k->k_txflags |= K_TXWANT;
		error = tsleep(&k->k_txflags, PZERO | PCATCH, "kbdout", 1);
		bail++;
	}
	if (bail == 1000)
		error = EIO;
	return (error);
}

/*
 * Start the sending data from the output queue
 * Called at spltty().
 */
void
kbd_sun_start_tx(struct kbd_sun_softc *k)
{
	int get;
	uint8_t c;

	if (k->k_txflags & K_TXBUSY)
		return;

	/* Is there anything to send? */
	get = k->k_tbget;
	if (get == k->k_tbput) {
		/* Nothing to send.  Wake drain waiters. */
		if (k->k_txflags & K_TXWANT) {
			k->k_txflags &= ~K_TXWANT;
			wakeup(&k->k_txflags);
		}
		return;
	}

	/* Have something to send. */
	c = k->k_tbuf[get];
	get = (get + 1) & KBD_TX_RING_MASK;
	k->k_tbget = get;
	k->k_txflags |= K_TXBUSY;

	/* Pass data down to the underlying device. */
	(*k->k_write_data)(k, c);
}


/*
 * Called by underlying driver's softint() routine on input,
 * which passes us the raw hardware make/break codes.
 * Called at spltty()
 */
int
kbd_sun_input(struct kbd_sun_softc *k, int code)
{
	struct kbd_softc *kbd = (struct kbd_softc *)k;

	/* XXX - Input errors already handled. */

	/* Are we expecting special input? */
	if (k->k_expect) {
		if (k->k_expect & KBD_EXPECT_IDCODE) {
			/* We read a KBD_RESET last time. */
			kbd->k_state.kbd_id = code;
			kbd_sun_was_reset(k);
		}
		if (k->k_expect & KBD_EXPECT_LAYOUT) {
			/* We read a KBD_LAYOUT last time. */
			kbd->k_state.kbd_layout = code;
			kbd_sun_new_layout(k);
		}
		k->k_expect = 0;
		return(0);
	}

	/* Is this one of the "special" input codes? */
	if (KBD_SPECIAL(code)) {
		switch (code) {
		case KBD_RESET:
			k->k_expect |= KBD_EXPECT_IDCODE;
			/* Fake an "all-up" to resync. translation. */
			code = KBD_IDLE;
			break;

		case KBD_LAYOUT:
			k->k_expect |= KBD_EXPECT_LAYOUT;
			return(0);

		case KBD_ERROR:
			log(LOG_WARNING, "%s: received error indicator\n",
			    device_xname(kbd->k_dev));
			return(-1);

		case KBD_IDLE:
			/* Let this go to the translator. */
			break;
		}
	}

	kbd_input(kbd, code);
	return(0);
}


/*
 * Called by kbd_sun_input to handle keyboard's response to reset.
 * Called at spltty().
 */
static void
kbd_sun_was_reset(struct kbd_sun_softc *k)
{
	struct kbd_state *ks = &k->k_kbd.k_state;

	/*
	 * On first identification, wake up anyone waiting for type
	 * and set up the table pointers.
	 */
	wakeup((void *)&ks->kbd_id);

	/* Restore keyclick, if necessary */
	switch (ks->kbd_id) {

	case KB_SUN2:
		/* Type 2 keyboards don't support keyclick */
		break;

	case KB_SUN3:
		/* Type 3 keyboards come up with keyclick on */
		if (!ks->kbd_click) {
			/* turn off the click */
			kbd_sun_output(k, KBD_CMD_NOCLICK);
			kbd_sun_start_tx(k);
		}
		break;

	case KB_SUN4:
		/* Type 4 keyboards come up with keyclick off */
		if (ks->kbd_click) {
			/* turn on the click */
			kbd_sun_output(k, KBD_CMD_CLICK);
			kbd_sun_start_tx(k);
		}
		break;
	default:
		printf("%s: unknown keyboard type ID %u\n",
		    device_xname(k->k_kbd.k_dev), (unsigned int)ks->kbd_id);
	}

	/* LEDs are off after reset. */
	ks->kbd_leds = 0;
}

/*
 * Called by kbd_sun_input to handle response to layout request.
 * Called at spltty().
 */
static void
kbd_sun_new_layout(struct kbd_sun_softc *k)
{
	struct kbd_state *ks = &k->k_kbd.k_state;

	/*
	 * On first identification, wake up anyone waiting for type
	 * and set up the table pointers.
	 */
	wakeup((void *)&ks->kbd_layout);

	/* XXX: switch decoding tables? */
}

/*	$NetBSD: kbdvar.h,v 1.21 2012/04/26 00:50:10 macallan Exp $	*/

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

#include "wskbd.h"	/* for NWSKBD */
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/sysmon/sysmonvar.h>

#if NWSKBD > 0
#include "opt_wsdisplay_compat.h"
#endif

struct kbd_softc {
	device_t k_dev;		/* required first: base device */

	struct sysmon_pswitch k_sm_pbutton;
	int k_ev;

	/* middle layer methods */
	const struct kbd_ops *k_ops;

	/* state of the upper layer */
	int k_evmode;		/* set if we should produce events */
	struct evvar k_events;	/* event queue state */

#if NWSKBD > 0
	device_t  k_wskbd;/* handle for wskbd, if it is attached */
	int k_wsenabled;	/* set if we are using wskbd */
#ifdef WSDISPLAY_COMPAT_RAWKBD
	int k_wsraw;		/* send raw events to wscons */
#endif
	struct callout k_wsbell;/* to shut the bell off */
#endif

	/* ASCII translation state */
	struct kbd_state k_state;

	/* console hooks */
	int k_isconsole;
	struct cons_channel *k_cc;

	/* autorepeat for console input */
	int k_repeat_start; 	/* initial delay */
	int k_repeat_step;  	/* inter-char delay */
	int k_repeatsym;	/* repeating symbol */
	int k_repeating;	/* callout is active (use callout_active?) */
	struct callout k_repeat_ch;
	int k_leds;
};


/*
 * Downcalls to the middle layer.
 */
struct kbd_ops {
	int (*open)(struct kbd_softc *);
	int (*close)(struct kbd_softc *);
	int (*docmd)(struct kbd_softc *, int, int);
	int (*setleds)(struct kbd_softc *, int, int);
};


/*
 * kbd console input channel interface.
 * XXX - does not belong in this header; but for now, kbd is the only user...
 */
struct cons_channel {
	/*
	 * Callbacks provided by underlying device (e.g. keyboard driver).
	 * Console driver will call these before console is opened/closed.
	 */
	void *cc_private;	/* underlying device private data */
	int (*cc_iopen)(struct cons_channel *);  /* open underlying device */
	int (*cc_iclose)(struct cons_channel *); /* close underlying device */

	/*
	 * Callback provided by the console driver.  Keyboard driver
	 * calls it to pass input character up as console input.
	 */
	void (*cc_upstream)(int);
};


/*
 * Allocate and link up console channel.
 * Should be called by the lower layer during attachment.
 */
extern struct cons_channel *kbd_cc_alloc(struct kbd_softc *);

/*
 * Feed sun make/break code as keyboard input to the upper layer.
 * Should be called by the middle layer.
 */
extern void kbd_input(struct kbd_softc *, int);

/*
 * Special hook to attach the keyboard driver to the console.
 * XXX: this should be hidden in kbd_cc_alloc().
 */
struct consdev;
extern void cons_attach_input(struct cons_channel *, struct consdev *);

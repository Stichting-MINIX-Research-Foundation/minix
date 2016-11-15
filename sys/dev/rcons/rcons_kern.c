/*	$NetBSD: rcons_kern.c,v 1.22 2009/03/14 15:36:20 dsl Exp $ */

/*
 * Copyright (c) 1991, 1993
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
 *	@(#)rcons_kern.c	8.1 (Berkeley) 6/11/93
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rcons_kern.c,v 1.22 2009/03/14 15:36:20 dsl Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/proc.h>

#include <dev/rcons/raster.h>
#include <dev/rcons/rcons.h>

static void rcons_belltmr(void *);

static struct rconsole *mydevicep; /* XXX */
static void rcons_output(struct tty *);

void
rcons_cnputc(int c)
{
	char buf[1];
	long attr;

	/* Swap in kernel attribute */
	attr = mydevicep->rc_attr;
	mydevicep->rc_attr = mydevicep->rc_kern_attr;

	if (c == '\n')
		rcons_puts(mydevicep, "\r\n", 2);
	else {
		buf[0] = c;
		rcons_puts(mydevicep, buf, 1);
	}

	/* Swap out kernel attribute */
	mydevicep->rc_attr = attr;
}

static void
rcons_output(struct tty *tp)
{
	int s, n;
	char buf[OBUFSIZ];

	s = spltty();
	if (tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP)) {
		splx(s);
		return;
	}
	tp->t_state |= TS_BUSY;
	splx(s);
	n = q_to_b(&tp->t_outq, buf, sizeof(buf));
	rcons_puts(mydevicep, buf, n);

	s = spltty();
	tp->t_state &= ~TS_BUSY;
	/* Come back if there's more to do */
	if (ttypull(tp)) {
		tp->t_state |= TS_TIMEOUT;
		callout_schedule(&tp->t_rstrt_ch, 1);
	}
	splx(s);
}

/* Ring the console bell */
void
rcons_bell(struct rconsole *rc)
{
	int i, s;

	if (rc->rc_bits & FB_VISBELL) {
		/* invert the screen twice */
		i = ((rc->rc_bits & FB_INVERT) == 0);
		rcons_invert(rc, i);
		rcons_invert(rc, i ^ 1);
	}

	s = splhigh();
	if (rc->rc_belldepth++) {
		if (rc->rc_belldepth > 3)
			rc->rc_belldepth = 3;
		splx(s);
	} else {
		rc->rc_ringing = 1;
		splx(s);
		(*rc->rc_bell)(1);
		/* XXX Chris doesn't like the following divide */
		callout_reset(&rc->rc_belltmr_ch, hz / 10,
		    rcons_belltmr, rc);
	}
}

/* Bell timer service routine */
static void
rcons_belltmr(void *p)
{
	struct rconsole *rc = p;
	int s = splhigh(), i;

	if (rc->rc_ringing) {
		rc->rc_ringing = 0;
		i = --rc->rc_belldepth;
		splx(s);
		(*rc->rc_bell)(0);
		if (i != 0)
			/* XXX Chris doesn't like the following divide */
			callout_reset(&rc->rc_belltmr_ch, hz / 30,
			    rcons_belltmr, rc);
	} else {
		rc->rc_ringing = 1;
		splx(s);
		(*rc->rc_bell)(1);
		callout_reset(&rc->rc_belltmr_ch, hz / 10,
		    rcons_belltmr, rc);
	}
}

void
rcons_init(struct rconsole *rc, int clear)
{
	mydevicep = rc;

	callout_init(&rc->rc_belltmr_ch, 0);

	/* Initialize operations set, clear screen and turn cursor on */
	rcons_init_ops(rc);
	if (clear) {
		rc->rc_col = 0;
		rc->rc_row = 0;
		rcons_clear2eop(rc);
	}
	rcons_cursor(rc);
}

void
rcons_ttyinit(struct tty *tp)
{
	/* XXX this should go away */
	struct rconsole *rc = mydevicep;
	struct winsize *ws;

	if (rc == NULL)
		return;

	/* Let the system know how big the console is */
	ws = &tp->t_winsize;
	ws->ws_row = rc->rc_maxrow;
	ws->ws_col = rc->rc_maxcol;
	ws->ws_xpixel = rc->rc_width;
	ws->ws_ypixel = rc->rc_height;

	/* Initialization done; hook us up */
	tp->t_oproc = rcons_output;
	/*tp->t_stop = (void (*)()) nullop;*/
}

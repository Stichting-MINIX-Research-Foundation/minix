/*	$NetBSD: biconsdev.c,v 1.23 2015/08/20 14:40:18 christos Exp $	*/

/*-
 * Copyright (c) 1999-2001
 *         Shin Takemura and PocketBSD Project. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the PocketBSD project
 *	and its contributors.
 * 4. Neither the name of the project nor the names of its contributors
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
 */
/*
 * Copyright (c) 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ted Lemon.
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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: biconsdev.c,v 1.23 2015/08/20 14:40:18 christos Exp $");

#include "biconsdev.h"
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/kauth.h>

#include <dev/cons.h>
#include <dev/hpc/bicons.h>
#include <dev/hpc/biconsvar.h>

#include "ioconf.h"

struct tty biconsdev_tty[NBICONSDEV];
static	void biconsdev_output(struct tty *);

dev_type_open(biconsdevopen);
dev_type_close(biconsdevclose);
dev_type_read(biconsdevread);
dev_type_write(biconsdevwrite);
dev_type_ioctl(biconsdevioctl);
dev_type_tty(biconsdevtty);
dev_type_poll(biconsdevpoll);

const struct cdevsw biconsdev_cdevsw = {
	.d_open = biconsdevopen,
	.d_close = biconsdevclose,
	.d_read = biconsdevread,
	.d_write = biconsdevwrite,
	.d_ioctl = biconsdevioctl,
	.d_stop = nostop,
	.d_tty = biconsdevtty,
	.d_poll = biconsdevpoll,
	.d_mmap = nommap,
	.d_kqfilter = ttykqfilter,
	.d_discard = nodiscard,
	.d_flag = D_TTY
};

void
biconsdevattach(int n)
{
	struct tty *tp = &biconsdev_tty[0];
	int maj;

	/* locate the major number */
	maj = cdevsw_lookup_major(&biconsdev_cdevsw);

	/* Set up the tty queues now... */
	clalloc(&tp->t_rawq, 1024, 1);
	clalloc(&tp->t_canq, 1024, 1);
	/* output queue doesn't need quoting */
	clalloc(&tp->t_outq, 1024, 0);
	/* Set default line discipline. */
	tp->t_linesw = ttyldisc_default();


	tp->t_dev = makedev(maj, 0);
	tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
	tp->t_param = (int (*)(struct tty *, struct termios *))nullop;
	tp->t_winsize.ws_row = bicons_height;
	tp->t_winsize.ws_col = bicons_width;
	tp->t_winsize.ws_xpixel = bicons_xpixel;
	tp->t_winsize.ws_ypixel = bicons_ypixel;
	tp->t_oproc = biconsdev_output;
}


static void
biconsdev_output(struct tty *tp)
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
	bicons_putn(buf, n);

	s = spltty();
	tp->t_state &= ~TS_BUSY;
	/* Come back if there's more to do */
	if (ttypull(tp)) {
		tp->t_state |= TS_TIMEOUT;
		callout_schedule(&tp->t_rstrt_ch, 1);
	}
	splx(s);
}


int
biconsdevopen(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct tty *tp = &biconsdev_tty[0];
	int status;

	if (kauth_authorize_device_tty(l->l_cred, KAUTH_DEVICE_TTY_OPEN, tp))
		return (EBUSY);

	if ((tp->t_state & TS_ISOPEN) == 0) {
		/*
		 * Leave baud rate alone!
		 */
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_state = TS_ISOPEN | TS_CARR_ON;
		(void)(*tp->t_param)(tp, &tp->t_termios);
		ttsetwater(tp);
	}

	status = (*tp->t_linesw->l_open)(dev, tp);
	return status;
}


int
biconsdevclose(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct tty *tp = &biconsdev_tty[0];

	(*tp->t_linesw->l_close)(tp, flag);
	ttyclose(tp);

	return (0);
}


int
biconsdevread(dev_t dev, struct uio *uio, int flag)
{
	struct tty *tp = &biconsdev_tty[0];

	return ((*tp->t_linesw->l_read)(tp, uio, flag));
}


int
biconsdevwrite(dev_t dev, struct uio *uio, int flag)
{
	struct tty *tp = &biconsdev_tty[0];

	return ((*tp->t_linesw->l_write)(tp, uio, flag));
}


int
biconsdevpoll(dev_t dev, int events, struct lwp *l)
{
	struct tty *tp = &biconsdev_tty[0];

	return ((*tp->t_linesw->l_poll)(tp, events, l));
}


struct tty *
biconsdevtty(dev_t dev)
{
	struct tty *tp = &biconsdev_tty[0];

        return (tp);
}

int
biconsdevioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct tty *tp = &biconsdev_tty[0];
	int error;

	if ((error = tp->t_linesw->l_ioctl(tp, cmd, data, flag, l)) !=
	    EPASSTHROUGH)
		return (error);
	return (ttioctl(tp, cmd, data, flag, l));
}

/*	$NetBSD: ofcons.c,v 1.45 2014/07/25 08:10:37 dholland Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
 * All rights reserved.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ofcons.c,v 1.45 2014/07/25 08:10:37 dholland Exp $");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/tty.h>
#include <sys/kauth.h>

#include <dev/cons.h>

#include <dev/ofw/openfirm.h>

struct ofcons_softc {
	struct tty *of_tty;
	struct callout sc_poll_ch;
	int of_flags;
};
/* flags: */
#define	OFPOLL		1

#define	OFBURSTLEN	128	/* max number of bytes to write in one chunk */

cons_decl(ofcons_);

static int stdin, stdout;

static int ofcons_match(device_t, cfdata_t, void *);
static void ofcons_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(ofcons, sizeof(struct ofcons_softc),
    ofcons_match, ofcons_attach, NULL, NULL);

extern struct cfdriver ofcons_cd;

dev_type_open(ofcons_open);
dev_type_close(ofcons_close);
dev_type_read(ofcons_read);
dev_type_write(ofcons_write);
dev_type_ioctl(ofcons_ioctl);
dev_type_tty(ofcons_tty);
dev_type_poll(ofcons_poll);

const struct cdevsw ofcons_cdevsw = {
	.d_open = ofcons_open,
	.d_close = ofcons_close,
	.d_read = ofcons_read,
	.d_write = ofcons_write,
	.d_ioctl = ofcons_ioctl,
	.d_stop = nostop,
	.d_tty = ofcons_tty,
	.d_poll = ofcons_poll,
	.d_mmap = nommap,
	.d_kqfilter = ttykqfilter,
	.d_discard = nodiscard,
	.d_flag = D_TTY
};

static int ofcons_probe(void);

static int
ofcons_match(device_t parent, cfdata_t match, void *aux)
{
	struct ofbus_attach_args *oba = aux;

	if (strcmp(oba->oba_busname, "ofw"))
		return (0);
	if (!ofcons_probe())
		return 0;
	return OF_instance_to_package(stdin) == oba->oba_phandle
		|| OF_instance_to_package(stdout) == oba->oba_phandle;
}

static void
ofcons_attach(device_t parent, device_t self, void *aux)
{
	struct ofcons_softc *sc = device_private(self);

	printf("\n");

	callout_init(&sc->sc_poll_ch, 0);
}

static void ofcons_start(struct tty *);
static int ofcons_param(struct tty *, struct termios *);
static void ofcons_pollin(void *);

int
ofcons_open(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct ofcons_softc *sc;
	struct tty *tp;

	sc = device_lookup_private(&ofcons_cd, minor(dev));
	if (!sc)
		return ENXIO;
	if (!(tp = sc->of_tty))
		sc->of_tty = tp = tty_alloc();
	tp->t_oproc = ofcons_start;
	tp->t_param = ofcons_param;
	tp->t_dev = dev;
	if (kauth_authorize_device_tty(l->l_cred, KAUTH_DEVICE_TTY_OPEN, tp))
		return (EBUSY);
	if (!(tp->t_state & TS_ISOPEN)) {
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		ofcons_param(tp, &tp->t_termios);
		ttsetwater(tp);
	}
	tp->t_state |= TS_CARR_ON;

	if (!(sc->of_flags & OFPOLL)) {
		sc->of_flags |= OFPOLL;
		callout_reset(&sc->sc_poll_ch, 1, ofcons_pollin, sc);
	}

	return (*tp->t_linesw->l_open)(dev, tp);
}

int
ofcons_close(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct ofcons_softc *sc = device_lookup_private(&ofcons_cd, minor(dev));
	struct tty *tp = sc->of_tty;

	callout_stop(&sc->sc_poll_ch);
	sc->of_flags &= ~OFPOLL;
	(*tp->t_linesw->l_close)(tp, flag);
	ttyclose(tp);
	return 0;
}

int
ofcons_read(dev_t dev, struct uio *uio, int flag)
{
	struct ofcons_softc *sc = device_lookup_private(&ofcons_cd, minor(dev));
	struct tty *tp = sc->of_tty;

	return (*tp->t_linesw->l_read)(tp, uio, flag);
}

int
ofcons_write(dev_t dev, struct uio *uio, int flag)
{
	struct ofcons_softc *sc = device_lookup_private(&ofcons_cd, minor(dev));
	struct tty *tp = sc->of_tty;

	return (*tp->t_linesw->l_write)(tp, uio, flag);
}

int
ofcons_poll(dev_t dev, int events, struct lwp *l)
{
	struct ofcons_softc *sc = device_lookup_private(&ofcons_cd, minor(dev));
	struct tty *tp = sc->of_tty;

	return ((*tp->t_linesw->l_poll)(tp, events, l));
}
int
ofcons_ioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct ofcons_softc *sc = device_lookup_private(&ofcons_cd, minor(dev));
	struct tty *tp = sc->of_tty;
	int error;

	if ((error = (*tp->t_linesw->l_ioctl)(tp, cmd, data, flag, l)) != EPASSTHROUGH)
		return error;
	return ttioctl(tp, cmd, data, flag, l);
}

struct tty *
ofcons_tty(dev_t dev)
{
	struct ofcons_softc *sc = device_lookup_private(&ofcons_cd, minor(dev));

	return sc->of_tty;
}

static void
ofcons_start(struct tty *tp)
{
	int s, len;
	u_char buf[OFBURSTLEN];

	s = spltty();
	if (tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP)) {
		splx(s);
		return;
	}
	tp->t_state |= TS_BUSY;
	splx(s);
	len = q_to_b(&tp->t_outq, buf, OFBURSTLEN);
	OF_write(stdout, buf, len);
	s = spltty();
	tp->t_state &= ~TS_BUSY;
	if (ttypull(tp)) {
		tp->t_state |= TS_TIMEOUT;
		callout_schedule(&tp->t_rstrt_ch, 1);
	}
	splx(s);
}

static int
ofcons_param(struct tty *tp, struct termios *t)
{
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;
	return 0;
}

static void
ofcons_pollin(void *aux)
{
	struct ofcons_softc *sc = aux;
	struct tty *tp = sc->of_tty;
	char ch;

	while (OF_read(stdin, &ch, 1) > 0) {
		if (tp && (tp->t_state & TS_ISOPEN))
			(*tp->t_linesw->l_rint)(ch, tp);
	}
	callout_reset(&sc->sc_poll_ch, 1, ofcons_pollin, sc);
}

static int
ofcons_probe(void)
{
	int chosen;
	char stdinbuf[4], stdoutbuf[4];

	if (stdin)
		return 1;
	if ((chosen = OF_finddevice("/chosen")) == -1)
		return 0;
	if (OF_getprop(chosen, "stdin", stdinbuf, sizeof stdinbuf) !=
	      sizeof stdinbuf ||
	    OF_getprop(chosen, "stdout", stdoutbuf, sizeof stdoutbuf) !=
	      sizeof stdoutbuf)
		return 0;

	/* Decode properties. */
	stdin = of_decode_int(stdinbuf);
	stdout = of_decode_int(stdoutbuf);

	return 1;
}

void
ofcons_cnprobe(struct consdev *cd)
{
	int maj;

	if (!ofcons_probe())
		return;

	maj = cdevsw_lookup_major(&ofcons_cdevsw);
	cd->cn_dev = makedev(maj, 0);
	cd->cn_pri = CN_INTERNAL;
}

void
ofcons_cninit(struct consdev *cd)
{
}

int
ofcons_cngetc(dev_t dev)
{
	unsigned char ch = '\0';
	int l;

	while ((l = OF_read(stdin, &ch, 1)) != 1)
		if (l != -2 && l != 0)
			return -1;
	return ch;
}

void
ofcons_cnputc(dev_t dev, int c)
{
	char ch = c;

	OF_write(stdout, &ch, 1);
}

void
ofcons_cnpollc(dev_t dev, int on)
{
	struct ofcons_softc *sc = device_lookup_private(&ofcons_cd, minor(dev));

	if (!sc)
		return;
	if (on) {
		if (sc->of_flags & OFPOLL)
			callout_stop(&sc->sc_poll_ch);
		sc->of_flags &= ~OFPOLL;
	} else {
		if (!(sc->of_flags & OFPOLL)) {
			sc->of_flags |= OFPOLL;
			callout_reset(&sc->sc_poll_ch, 1, ofcons_pollin, sc);
		}
	}
}

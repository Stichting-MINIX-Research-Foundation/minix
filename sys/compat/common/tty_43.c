/*	$NetBSD: tty_43.c,v 1.30 2014/05/22 16:31:19 dholland Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)tty_compat.c	8.2 (Berkeley) 1/9/95
 */

/*
 * mapping routines for old line discipline (yuck)
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tty_43.c,v 1.30 2014/05/22 16:31:19 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/termios.h>
#include <sys/file.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/ioctl_compat.h>

int ttydebug = 0;

static const struct speedtab compatspeeds[] = {
#define MAX_SPEED	17
	{ 115200, 17 },
	{ 57600, 16 },
	{ 38400, 15 },
	{ 19200, 14 },
	{ 9600,	13 },
	{ 4800,	12 },
	{ 2400,	11 },
	{ 1800,	10 },
	{ 1200,	9 },
	{ 600,	8 },
	{ 300,	7 },
	{ 200,	6 },
	{ 150,	5 },
	{ 134,	4 },
	{ 110,	3 },
	{ 75,	2 },
	{ 50,	1 },
	{ 0,	0 },
	{ -1,	-1 },
};
static const int compatspcodes[] = {
	0, 50, 75, 110, 134, 150, 200, 300, 600, 1200,
	1800, 2400, 4800, 9600, 19200, 38400, 57600, 115200
};

static int ttcompatgetflags(struct tty *);
static void ttcompatsetflags(struct tty *, struct termios *);
static void ttcompatsetlflags(struct tty *, struct termios *);
int	ttcompat(struct tty *, u_long, void *, int, struct lwp *);

/*ARGSUSED*/
int
ttcompat(struct tty *tp, u_long com, void *data, int flag, struct lwp *l)
{

	switch (com) {
	case TIOCGETP: {
		struct sgttyb *sg = (struct sgttyb *)data;
		int speed;

		mutex_spin_enter(&tty_lock);
		speed = ttspeedtab(tp->t_ospeed, compatspeeds);
		sg->sg_ospeed = (speed == -1) ? MAX_SPEED : speed;
		if (tp->t_ispeed == 0)
			sg->sg_ispeed = sg->sg_ospeed;
		else {
			speed = ttspeedtab(tp->t_ispeed, compatspeeds);
			sg->sg_ispeed = (speed == -1) ? MAX_SPEED : speed;
		}
		sg->sg_erase = tty_getctrlchar(tp, VERASE);
		sg->sg_kill = tty_getctrlchar(tp, VKILL);
		sg->sg_flags = ttcompatgetflags(tp);
		mutex_spin_exit(&tty_lock);
		break;
	}

	case TIOCSETP:
	case TIOCSETN: {
		struct sgttyb *sg = (struct sgttyb *)data;
		struct termios term;
		int speed;

		mutex_spin_enter(&tty_lock);
		term = tp->t_termios;
		if ((speed = sg->sg_ispeed) > MAX_SPEED || speed < 0)
			term.c_ispeed = speed;
		else
			term.c_ispeed = compatspcodes[speed];
		if ((speed = sg->sg_ospeed) > MAX_SPEED || speed < 0)
			term.c_ospeed = speed;
		else
			term.c_ospeed = compatspcodes[speed];
		term.c_cc[VERASE] = sg->sg_erase;
		term.c_cc[VKILL] = sg->sg_kill;
		tp->t_flags = (ttcompatgetflags(tp)&0xffff0000) | (sg->sg_flags&0xffff);
		ttcompatsetflags(tp, &term);
		mutex_spin_exit(&tty_lock);
		return (ttioctl(tp, com == TIOCSETP ? TIOCSETAF : TIOCSETA,
			(void *)&term, flag, l));
	}

	case TIOCGETC: {
		struct tchars *tc = (struct tchars *)data;

		tc->t_intrc = tty_getctrlchar(tp, VINTR);
		tc->t_quitc = tty_getctrlchar(tp, VQUIT);
		tc->t_startc = tty_getctrlchar(tp, VSTART);
		tc->t_stopc = tty_getctrlchar(tp, VSTOP);
		tc->t_eofc = tty_getctrlchar(tp, VEOF);
		tc->t_brkc = tty_getctrlchar(tp, VEOL);
		break;
	}
	case TIOCSETC: {
		struct tchars *tc = (struct tchars *)data;

		tty_setctrlchar(tp, VINTR, tc->t_intrc);
		tty_setctrlchar(tp, VQUIT, tc->t_quitc);
		tty_setctrlchar(tp, VSTART, tc->t_startc);
		tty_setctrlchar(tp, VSTOP, tc->t_stopc);
		tty_setctrlchar(tp, VEOF, tc->t_eofc);
		tty_setctrlchar(tp, VEOL, tc->t_brkc);
		if (tc->t_brkc == (char)-1)
			tty_setctrlchar(tp, VEOL2, _POSIX_VDISABLE);
		break;
	}
	case TIOCSLTC: {
		struct ltchars *ltc = (struct ltchars *)data;

		tty_setctrlchar(tp, VSUSP, ltc->t_suspc);
		tty_setctrlchar(tp, VDSUSP, ltc->t_dsuspc);
		tty_setctrlchar(tp, VREPRINT, ltc->t_rprntc);
		tty_setctrlchar(tp, VDISCARD, ltc->t_flushc);
		tty_setctrlchar(tp, VWERASE, ltc->t_werasc);
		tty_setctrlchar(tp, VLNEXT, ltc->t_lnextc);
		break;
	}
	case TIOCGLTC: {
		struct ltchars *ltc = (struct ltchars *)data;

		ltc->t_suspc = tty_getctrlchar(tp, VSUSP);
		ltc->t_dsuspc = tty_getctrlchar(tp, VDSUSP);
		ltc->t_rprntc = tty_getctrlchar(tp, VREPRINT);
		ltc->t_flushc = tty_getctrlchar(tp, VDISCARD);
		ltc->t_werasc = tty_getctrlchar(tp, VWERASE);
		ltc->t_lnextc = tty_getctrlchar(tp, VLNEXT);
		break;
	}
	case TIOCLBIS:
	case TIOCLBIC:
	case TIOCLSET: {
		struct termios term;
		int flags;

		mutex_spin_enter(&tty_lock);
		term = tp->t_termios;
		flags = ttcompatgetflags(tp);
		switch (com) {
		case TIOCLSET:
			tp->t_flags = (flags&0xffff) | (*(int *)data<<16);
			break;
		case TIOCLBIS:
			tp->t_flags = flags | (*(int *)data<<16);
			break;
		case TIOCLBIC:
			tp->t_flags = flags & ~(*(int *)data<<16);
			break;
		}
		ttcompatsetlflags(tp, &term);
		mutex_spin_exit(&tty_lock);
		return (ttioctl(tp, TIOCSETA, (void *)&term, flag, l));
	}
	case TIOCLGET:
		mutex_spin_enter(&tty_lock);
		*(int *)data = ttcompatgetflags(tp)>>16;
		mutex_spin_exit(&tty_lock);
		if (ttydebug)
			printf("CLGET: returning %x\n", *(int *)data);
		break;

	case OTIOCGETD:
		mutex_spin_enter(&tty_lock);
		*(int *)data = (tp->t_linesw == NULL) ?
		    2 /* XXX old NTTYDISC */ : tp->t_linesw->l_no;
		mutex_spin_exit(&tty_lock);
		break;

	case OTIOCSETD: {
		int ldisczero = 0;

		return (ttioctl(tp, TIOCSETD,
			*(int *)data == 2 ? (void *)&ldisczero : data, flag,
			l));
	    }

	case OTIOCCONS:
		*(int *)data = 1;
		return (ttioctl(tp, TIOCCONS, data, flag, l));

	case TIOCHPCL:
		mutex_spin_enter(&tty_lock);
		SET(tp->t_cflag, HUPCL);
		mutex_spin_exit(&tty_lock);
		break;

	case TIOCGSID:
		mutex_enter(proc_lock);
		if (tp->t_session == NULL) {
			mutex_exit(proc_lock);
			return ENOTTY;
		}
		if (tp->t_session->s_leader == NULL) {
			mutex_exit(proc_lock);
			return ENOTTY;
		}
		*(int *) data =  tp->t_session->s_leader->p_pid;
		mutex_exit(proc_lock);
		break;

	default:
		return (EPASSTHROUGH);
	}
	return (0);
}

static int
ttcompatgetflags(struct tty *tp)
{
	tcflag_t iflag = tp->t_iflag;
	tcflag_t lflag = tp->t_lflag;
	tcflag_t oflag = tp->t_oflag;
	tcflag_t cflag = tp->t_cflag;
	int flags = 0;

	KASSERT(mutex_owned(&tty_lock));

	if (ISSET(iflag, IXOFF))
		SET(flags, TANDEM);
	if (ISSET(iflag, ICRNL) || ISSET(oflag, ONLCR))
		SET(flags, CRMOD);
	if (ISSET(cflag, PARENB)) {
		if (ISSET(iflag, INPCK)) {
			if (ISSET(cflag, PARODD))
				SET(flags, ODDP);
			else
				SET(flags, EVENP);
		} else
			SET(flags, ANYP);
	}

	if (!ISSET(lflag, ICANON)) {
		/* fudge */
		if (ISSET(iflag, IXON) || ISSET(lflag, ISIG|IEXTEN) ||
		    ISSET(cflag, PARENB))
			SET(flags, CBREAK);
		else
			SET(flags, RAW);
	}

	if (ISSET(flags, RAW))
		SET(flags, ISSET(tp->t_flags, LITOUT|PASS8));
	else if (ISSET(cflag, CSIZE) == CS8) {
		if (!ISSET(oflag, OPOST))
			SET(flags, LITOUT);
		if (!ISSET(iflag, ISTRIP))
			SET(flags, PASS8);
	}

	if (ISSET(cflag, MDMBUF))
		SET(flags, MDMBUF);
	if (!ISSET(cflag, HUPCL))
		SET(flags, NOHANG);
	if (ISSET(oflag, OXTABS))
		SET(flags, XTABS);
	if (ISSET(lflag, ECHOE))
		SET(flags, CRTERA|CRTBS);
	if (ISSET(lflag, ECHOKE))
		SET(flags, CRTKIL|CRTBS);
	if (ISSET(lflag, ECHOPRT))
		SET(flags, PRTERA);
	if (ISSET(lflag, ECHOCTL))
		SET(flags, CTLECH);
	if (!ISSET(iflag, IXANY))
		SET(flags, DECCTQ);
	SET(flags, ISSET(lflag, ECHO|TOSTOP|FLUSHO|PENDIN|NOFLSH));
	if (ttydebug)
		printf("getflags: %x\n", flags);
	return (flags);
}

static void
ttcompatsetflags(struct tty *tp, struct termios *t)
{
	int flags = tp->t_flags;

	KASSERT(mutex_owned(&tty_lock));

	tcflag_t iflag = t->c_iflag;
	tcflag_t oflag = t->c_oflag;
	tcflag_t lflag = t->c_lflag;
	tcflag_t cflag = t->c_cflag;

	if (ISSET(flags, TANDEM))
		SET(iflag, IXOFF);
	else
		CLR(iflag, IXOFF);
	if (ISSET(flags, ECHO))
		SET(lflag, ECHO);
	else
		CLR(lflag, ECHO);
	if (ISSET(flags, CRMOD)) {
		SET(iflag, ICRNL);
		SET(oflag, ONLCR);
	} else {
		CLR(iflag, ICRNL);
		CLR(oflag, ONLCR);
	}
	if (ISSET(flags, XTABS))
		SET(oflag, OXTABS);
	else
		CLR(oflag, OXTABS);


	if (ISSET(flags, RAW)) {
		iflag &= IXOFF;
		CLR(lflag, ISIG|ICANON|IEXTEN);
		CLR(cflag, PARENB);
	} else {
		SET(iflag, BRKINT|IXON|IMAXBEL);
		SET(lflag, ISIG|IEXTEN);
		if (ISSET(flags, CBREAK))
			CLR(lflag, ICANON);
		else
			SET(lflag, ICANON);
		switch (ISSET(flags, ANYP)) {
		case 0:
			CLR(cflag, PARENB);
			break;
		case ANYP:
			SET(cflag, PARENB);
			CLR(iflag, INPCK);
			break;
		case EVENP:
			SET(cflag, PARENB);
			SET(iflag, INPCK);
			CLR(cflag, PARODD);
			break;
		case ODDP:
			SET(cflag, PARENB);
			SET(iflag, INPCK);
			SET(cflag, PARODD);
			break;
		}
	}

	if (ISSET(flags, RAW|LITOUT|PASS8)) {
		CLR(cflag, CSIZE);
		SET(cflag, CS8);
		if (!ISSET(flags, RAW|PASS8))
			SET(iflag, ISTRIP);
		else
			CLR(iflag, ISTRIP);
		if (!ISSET(flags, RAW|LITOUT))
			SET(oflag, OPOST);
		else
			CLR(oflag, OPOST);
	} else {
		CLR(cflag, CSIZE);
		SET(cflag, CS7);
		SET(iflag, ISTRIP);
		SET(oflag, OPOST);
	}

	t->c_iflag = iflag;
	t->c_oflag = oflag;
	t->c_lflag = lflag;
	t->c_cflag = cflag;
}

static void
ttcompatsetlflags(struct tty *tp, struct termios *t)
{
	int flags = tp->t_flags;
	tcflag_t iflag = t->c_iflag;
	tcflag_t oflag = t->c_oflag;
	tcflag_t lflag = t->c_lflag;
	tcflag_t cflag = t->c_cflag;

	KASSERT(mutex_owned(&tty_lock));

	/* Nothing we can do with CRTBS. */
	if (ISSET(flags, PRTERA))
		SET(lflag, ECHOPRT);
	else
		CLR(lflag, ECHOPRT);
	if (ISSET(flags, CRTERA))
		SET(lflag, ECHOE);
	else
		CLR(lflag, ECHOE);
	/* Nothing we can do with TILDE. */
	if (ISSET(flags, MDMBUF))
		SET(cflag, MDMBUF);
	else
		CLR(cflag, MDMBUF);
	if (ISSET(flags, NOHANG))
		CLR(cflag, HUPCL);
	else
		SET(cflag, HUPCL);
	if (ISSET(flags, CRTKIL))
		SET(lflag, ECHOKE);
	else
		CLR(lflag, ECHOKE);
	if (ISSET(flags, CTLECH))
		SET(lflag, ECHOCTL);
	else
		CLR(lflag, ECHOCTL);
	if (!ISSET(flags, DECCTQ))
		SET(iflag, IXANY);
	else
		CLR(iflag, IXANY);
	CLR(lflag, TOSTOP|FLUSHO|PENDIN|NOFLSH);
	SET(lflag, ISSET(flags, TOSTOP|FLUSHO|PENDIN|NOFLSH));

	if (ISSET(flags, RAW|LITOUT|PASS8)) {
		CLR(cflag, CSIZE);
		SET(cflag, CS8);
		if (!ISSET(flags, RAW|PASS8))
			SET(iflag, ISTRIP);
		else
			CLR(iflag, ISTRIP);
		if (!ISSET(flags, RAW|LITOUT))
			SET(oflag, OPOST);
		else
			CLR(oflag, OPOST);
	} else {
		CLR(cflag, CSIZE);
		SET(cflag, CS7);
		SET(iflag, ISTRIP);
		SET(oflag, OPOST);
	}

	t->c_iflag = iflag;
	t->c_oflag = oflag;
	t->c_lflag = lflag;
	t->c_cflag = cflag;
}

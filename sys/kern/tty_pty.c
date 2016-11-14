/*	$NetBSD: tty_pty.c,v 1.142 2015/08/20 09:45:45 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)tty_pty.c	8.4 (Berkeley) 2/20/95
 */

/*
 * Pseudo-teletype Driver
 * (Actually two drivers, requiring two entries in 'cdevsw')
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tty_pty.c,v 1.142 2015/08/20 09:45:45 christos Exp $");

#include "opt_ptm.h"

#define TTY_ALLOW_PRIVATE

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/ioctl_compat.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/signalvar.h>
#include <sys/uio.h>
#include <sys/filedesc.h>
#include <sys/conf.h>
#include <sys/poll.h>
#include <sys/pty.h>
#include <sys/kauth.h>

#include "ioconf.h"

#define	DEFAULT_NPTYS		16	/* default number of initial ptys */
#define DEFAULT_MAXPTYS		992	/* default maximum number of ptys */

#define BUFSIZ 100		/* Chunk size iomoved to/from user */

struct	pt_softc {
	struct	tty *pt_tty;
	int	pt_flags;
	struct	selinfo pt_selr, pt_selw;
	u_char	pt_send;
	u_char	pt_ucntl;
};

static struct pt_softc **pt_softc = NULL;	/* pty array */
static int maxptys = DEFAULT_MAXPTYS;	/* maximum number of ptys (sysctable) */
kmutex_t pt_softc_mutex;
int npty = 0;			/* for pstat -t */

#define	PF_PKT		0x08		/* packet mode */
#define	PF_STOPPED	0x10		/* user told stopped */
#define	PF_REMOTE	0x20		/* remote and flow controlled input */
#define	PF_NOSTOP	0x40
#define PF_UCNTL	0x80		/* user control mode */

void	ptcwakeup(struct tty *, int);
void	ptsstart(struct tty *);
int	pty_maxptys(int, int);

static struct pt_softc **ptyarralloc(int);

dev_type_open(ptcopen);
dev_type_close(ptcclose);
dev_type_read(ptcread);
dev_type_write(ptcwrite);
dev_type_poll(ptcpoll);
dev_type_kqfilter(ptckqfilter);

dev_type_open(ptsopen);
dev_type_close(ptsclose);
dev_type_read(ptsread);
dev_type_write(ptswrite);
dev_type_stop(ptsstop);
dev_type_poll(ptspoll);

dev_type_ioctl(ptyioctl);
dev_type_tty(ptytty);

const struct cdevsw ptc_cdevsw = {
	.d_open = ptcopen,
	.d_close = ptcclose,
	.d_read = ptcread,
	.d_write = ptcwrite,
	.d_ioctl = ptyioctl,
	.d_stop = nullstop,
	.d_tty = ptytty,
	.d_poll = ptcpoll,
	.d_mmap = nommap,
	.d_kqfilter = ptckqfilter,
	.d_discard = nodiscard,
	.d_flag = D_TTY
};

const struct cdevsw pts_cdevsw = {
	.d_open = ptsopen,
	.d_close = ptsclose,
	.d_read = ptsread,
	.d_write = ptswrite,
	.d_ioctl = ptyioctl,
	.d_stop = ptsstop,
	.d_tty = ptytty,
	.d_poll = ptspoll,
	.d_mmap = nommap,
	.d_kqfilter = ttykqfilter,
	.d_discard = nodiscard,
	.d_flag = D_TTY
};

#if defined(pmax)
/*
 * Used by arch/pmax/conf/majors.pmax, which needs a second copy as it
 * needs to map this stuff to two pairs of majors.
 */

const struct cdevsw ptc_ultrix_cdevsw = {
	.d_open = ptcopen,
	.d_close = ptcclose,
	.d_read = ptcread,
	.d_write = ptcwrite,
	.d_ioctl = ptyioctl,
	.d_stop = nullstop,
	.d_tty = ptytty,
	.d_poll = ptcpoll,
	.d_mmap = nommap,
	.d_kqfilter = ptckqfilter,
	.d_discard = nodiscard,
	.d_flag = D_TTY
};

const struct cdevsw pts_ultrix_cdevsw = {
	.d_open = ptsopen,
	.d_close = ptsclose,
	.d_read = ptsread,
	.d_write = ptswrite,
	.d_ioctl = ptyioctl,
	.d_stop = ptsstop,
	.d_tty = ptytty,
	.d_poll = ptspoll,
	.d_mmap = nommap,
	.d_kqfilter = ttykqfilter,
	.d_discard = nodiscard,
	.d_flag = D_TTY
};
#endif /* defined(pmax) */

/*
 * Check if a pty is free to use.
 */
int
pty_isfree(int minor, int lock)
{
	struct pt_softc *pt = pt_softc[minor];
	if (lock)
		mutex_enter(&pt_softc_mutex);
	minor = pt == NULL || pt->pt_tty == NULL ||
	    pt->pt_tty->t_oproc == NULL;
	if (lock)
		mutex_exit(&pt_softc_mutex);
	return minor;
}

/*
 * Allocate and zero array of nelem elements.
 */
static struct pt_softc **
ptyarralloc(int nelem)
{
	struct pt_softc **pt;
	nelem += 10;
	pt = kmem_zalloc(nelem * sizeof(*pt), KM_SLEEP);
	return pt;
}

static void
ptyarrfree(struct pt_softc **pt, int nelem)
{

	nelem += 10;
	kmem_free(pt, nelem * sizeof(*pt));
}

/*
 * Check if the minor is correct and ensure necessary structures
 * are properly allocated.
 */
int
pty_check(int ptn)
{
	struct pt_softc *pti;

	if (ptn >= npty) {
		struct pt_softc **newpt, **oldpt;
		int newnpty;
		int oldnpty;

		/* check if the requested pty can be granted */
		if (ptn >= maxptys) {
	    limit_reached:
			tablefull("pty", "increase kern.maxptys");
			return ENXIO;
		}

		/* Allocate a larger pty array */
		for (newnpty = npty; newnpty <= ptn;)
			newnpty *= 2;
		if (newnpty > maxptys)
			newnpty = maxptys;
		newpt = ptyarralloc(newnpty);

		/*
		 * Now grab the pty array mutex - we need to ensure
		 * that the pty array is consistent while copying its
		 * content to newly allocated, larger space; we also
		 * need to be safe against pty_maxptys().
		 */
		mutex_enter(&pt_softc_mutex);

		if (newnpty >= maxptys) {
			/* limit cut away beneath us... */
			if (ptn >= maxptys) {
				mutex_exit(&pt_softc_mutex);
				ptyarrfree(newpt, newnpty);
				goto limit_reached;
			}
			newnpty = maxptys;
		}

		/*
		 * If the pty array was not enlarged while we were waiting
		 * for mutex, copy current contents of pt_softc[] to newly
		 * allocated array and start using the new bigger array.
		 */
		if (newnpty > npty) {
			memcpy(newpt, pt_softc, npty*sizeof(struct pt_softc *));
			oldpt = pt_softc;
			oldnpty = npty;
			pt_softc = newpt;
			npty = newnpty;
		} else {
			/* was enlarged when waited for lock, free new space */
			oldpt = newpt;
			oldnpty = newnpty;
		}

		mutex_exit(&pt_softc_mutex);
		ptyarrfree(oldpt, oldnpty);
	}

	/*
	 * If the entry is not yet allocated, allocate one. The mutex is
	 * needed so that the state of pt_softc[] array is consistant
	 * in case it has been lengthened above.
	 */
	if (!pt_softc[ptn]) {
		pti = kmem_zalloc(sizeof(*pti), KM_SLEEP);

		selinit(&pti->pt_selr);
		selinit(&pti->pt_selw);
		pti->pt_tty = tty_alloc();

		mutex_enter(&pt_softc_mutex);

		/*
		 * Check the entry again - it might have been
		 * added while we were waiting for mutex.
		 */
		if (pt_softc[ptn]) {
			mutex_exit(&pt_softc_mutex);
			tty_free(pti->pt_tty);
			seldestroy(&pti->pt_selr);
			seldestroy(&pti->pt_selw);
			kmem_free(pti, sizeof(*pti));
			return 0;
		}
		tty_attach(pti->pt_tty);
		pt_softc[ptn] = pti;

		mutex_exit(&pt_softc_mutex);
	}

	return 0;
}

/*
 * Set maxpty in thread-safe way. Returns 0 in case of error, otherwise
 * new value of maxptys.
 */
int
pty_maxptys(int newmax, int set)
{
	if (!set)
		return maxptys;

	/*
	 * We have to grab the pt_softc lock, so that we would pick correct
	 * value of npty (might be modified in pty_check()).
	 */
	mutex_enter(&pt_softc_mutex);

	/*
	 * The value cannot be set to value lower than the highest pty
	 * number ever allocated.
	 */
	if (newmax >= npty)
		maxptys = newmax;
	else
		newmax = 0;

	mutex_exit(&pt_softc_mutex);

	return newmax;
}

/*
 * Establish n (or default if n is 1) ptys in the system.
 */
void
ptyattach(int n)
{

	mutex_init(&pt_softc_mutex, MUTEX_DEFAULT, IPL_NONE);

	/* maybe should allow 0 => none? */
	if (n <= 1)
		n = DEFAULT_NPTYS;
	pt_softc = ptyarralloc(n);
	npty = n;
#ifndef NO_DEV_PTM
	ptmattach(1);
#endif
}

/*ARGSUSED*/
int
ptsopen(dev_t dev, int flag, int devtype, struct lwp *l)
{
	struct pt_softc *pti;
	struct tty *tp;
	int error;
	int ptn = minor(dev);

	if ((error = pty_check(ptn)) != 0)
		return error;

	mutex_spin_enter(&tty_lock);
	pti = pt_softc[ptn];
	tp = pti->pt_tty;
	if (!ISSET(tp->t_state, TS_ISOPEN)) {
		tp->t_dev = dev;
		ttychars(tp);		/* Set up default chars */
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		ttsetwater(tp);		/* would be done in xxparam() */
	} else if (kauth_authorize_device_tty(l->l_cred, KAUTH_DEVICE_TTY_OPEN,
	    tp) != 0) {
		mutex_spin_exit(&tty_lock);
		return EBUSY;
	}
	if (tp->t_oproc)			/* Ctrlr still around. */
		SET(tp->t_state, TS_CARR_ON);
	if (!ISSET(flag, O_NONBLOCK)) {
		while (!ISSET(tp->t_state, TS_CARR_ON)) {
			tp->t_wopen++;
			error = ttysleep(tp, &tp->t_rawcv, true, 0);
			tp->t_wopen--;
			if (error != 0) {
				mutex_spin_exit(&tty_lock);
				return error;
			}
		}
	}
	mutex_spin_exit(&tty_lock);
	error = (*tp->t_linesw->l_open)(dev, tp);
	ptcwakeup(tp, FREAD|FWRITE);
	return error;
}

int
ptsclose(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct pt_softc *pti = pt_softc[minor(dev)];
	struct tty *tp = pti->pt_tty;
	int error;

	error = (*tp->t_linesw->l_close)(tp, flag);
	error |= ttyclose(tp);
	ptcwakeup(tp, FREAD|FWRITE);
	return error;
}

int
ptsread(dev_t dev, struct uio *uio, int flag)
{
	struct proc *p = curproc;
	struct pt_softc *pti = pt_softc[minor(dev)];
	struct tty *tp = pti->pt_tty;
	int error = 0;
	int cc, c;

again:
	if (pti->pt_flags & PF_REMOTE) {
		mutex_spin_enter(&tty_lock);
		while (isbackground(p, tp)) {	/* XXXSMP */
			if (sigismasked(curlwp, SIGTTIN) ||
			    p->p_pgrp->pg_jobc == 0 ||
			    p->p_lflag & PL_PPWAIT) {
				mutex_spin_exit(&tty_lock);
				return EIO;
			}
			ttysig(tp, TTYSIG_PG1, SIGTTIN);
			error = ttypause(tp, hz);
			if (error != 0) {
				mutex_spin_exit(&tty_lock);
				return error;
			}
		}
		if (tp->t_canq.c_cc == 0) {
			if (flag & IO_NDELAY) {
				mutex_spin_exit(&tty_lock);
				return EWOULDBLOCK;
			}
			error = ttysleep(tp, &tp->t_cancv, true, 0);
			mutex_spin_exit(&tty_lock);
			if (error != 0)
				return error;
			goto again;
		}
		while(error == 0 && tp->t_canq.c_cc > 1 && uio->uio_resid > 0) {
			c = getc(&tp->t_canq);
			mutex_spin_exit(&tty_lock);
			error = ureadc(c, uio);
			mutex_spin_enter(&tty_lock);
			/* Re-check terminal state here? */
		}
		if (tp->t_canq.c_cc == 1)
			(void) getc(&tp->t_canq);
		cc = tp->t_canq.c_cc;
		mutex_spin_exit(&tty_lock);
		if (cc)
			return error;
	} else if (tp->t_oproc)
		error = (*tp->t_linesw->l_read)(tp, uio, flag);
	ptcwakeup(tp, FWRITE);
	return error;
}

/*
 * Write to pseudo-tty.
 * Wakeups of controlling tty will happen
 * indirectly, when tty driver calls ptsstart.
 */
int
ptswrite(dev_t dev, struct uio *uio, int flag)
{
	struct pt_softc *pti = pt_softc[minor(dev)];
	struct tty *tp = pti->pt_tty;

	if (tp->t_oproc == NULL)
		return EIO;
	return (*tp->t_linesw->l_write)(tp, uio, flag);
}

/*
 * Poll pseudo-tty.
 */
int
ptspoll(dev_t dev, int events, struct lwp *l)
{
	struct pt_softc *pti = pt_softc[minor(dev)];
	struct tty *tp = pti->pt_tty;

	if (tp->t_oproc == NULL)
		return POLLHUP;

	return (*tp->t_linesw->l_poll)(tp, events, l);
}

/*
 * Start output on pseudo-tty.
 * Wake up process polling or sleeping for input from controlling tty.
 */
void
ptsstart(struct tty *tp)
{
	struct pt_softc *pti;

	KASSERT(tp->t_dev != NODEV);
	pti = pt_softc[minor(tp->t_dev)];

	KASSERT(mutex_owned(&tty_lock));

	if (ISSET(tp->t_state, TS_TTSTOP))
		return;
	if (pti->pt_flags & PF_STOPPED) {
		pti->pt_flags &= ~PF_STOPPED;
		pti->pt_send = TIOCPKT_START;
	}

	selnotify(&pti->pt_selr, 0, NOTE_SUBMIT);
	cv_broadcast(&tp->t_outcvf);
}

/*
 * Stop output.
 */
void
ptsstop(struct tty *tp, int flush)
{
	struct pt_softc *pti;

	KASSERT(tp->t_dev != NODEV);
	pti = pt_softc[minor(tp->t_dev)];

	KASSERT(mutex_owned(&tty_lock));

	/* note: FLUSHREAD and FLUSHWRITE already ok */
	CTASSERT(TIOCPKT_FLUSHREAD == FREAD);
	CTASSERT(TIOCPKT_FLUSHWRITE == FWRITE);
	if (flush == 0) {
		flush = TIOCPKT_STOP;
		pti->pt_flags |= PF_STOPPED;
	} else
		pti->pt_flags &= ~PF_STOPPED;
	pti->pt_send |= flush;

	/* change of perspective */
	if (flush & FREAD) {
		selnotify(&pti->pt_selw, 0, NOTE_SUBMIT);
		cv_broadcast(&tp->t_rawcvf);
	}
	if (flush & FWRITE) {
		selnotify(&pti->pt_selr, 0, NOTE_SUBMIT);
		cv_broadcast(&tp->t_outcvf);
	}
}

void
ptcwakeup(struct tty *tp, int flag)
{
	struct pt_softc *pti;

	if (tp->t_dev == NODEV)
		return;	/* client side not open yet */

	pti = pt_softc[minor(tp->t_dev)];
	KASSERT(pti != NULL);

	mutex_spin_enter(&tty_lock);
	if (flag & FREAD) {
		selnotify(&pti->pt_selr, 0, NOTE_SUBMIT);
		cv_broadcast(&tp->t_outcvf);
	}
	if (flag & FWRITE) {
		selnotify(&pti->pt_selw, 0, NOTE_SUBMIT);
		cv_broadcast(&tp->t_rawcvf);
	}
	mutex_spin_exit(&tty_lock);
}

/*ARGSUSED*/
int
ptcopen(dev_t dev, int flag, int devtype, struct lwp *l)
{
	struct pt_softc *pti;
	struct tty *tp;
	int error;
	int ptn = minor(dev);

	if ((error = pty_check(ptn)) != 0)
		return error;

	pti = pt_softc[ptn];
	tp = pti->pt_tty;

	mutex_spin_enter(&tty_lock);
	if (tp->t_oproc) {
		mutex_spin_exit(&tty_lock);
		return EIO;
	}
	tp->t_dev = dev;
	tp->t_oproc = ptsstart;
	mutex_spin_exit(&tty_lock);
	(void)(*tp->t_linesw->l_modem)(tp, 1);
	CLR(tp->t_lflag, EXTPROC);
	pti->pt_flags = 0;
	pti->pt_send = 0;
	pti->pt_ucntl = 0;
	return 0;
}

/*ARGSUSED*/
int
ptcclose(dev_t dev, int flag, int devtype, struct lwp *l)
{
	struct pt_softc *pti = pt_softc[minor(dev)];
	struct tty *tp = pti->pt_tty;

	(void)(*tp->t_linesw->l_modem)(tp, 0);
	mutex_spin_enter(&tty_lock);
	CLR(tp->t_state, TS_CARR_ON);
	tp->t_oproc = NULL;		/* mark closed */
	mutex_spin_exit(&tty_lock);
	return 0;
}

int
ptcread(dev_t dev, struct uio *uio, int flag)
{
	struct pt_softc *pti = pt_softc[minor(dev)];
	struct tty *tp = pti->pt_tty;
	u_char bf[BUFSIZ];
	int error = 0, cc;
	int c;

	if (uio->uio_resid <= 0)
		return EINVAL;

	/*
	 * We want to block until the slave
	 * is open, and there's something to read;
	 * but if we lost the slave or we're NBIO,
	 * then return the appropriate error instead.
	 */
	mutex_spin_enter(&tty_lock);
	for (;;) {
		if (ISSET(tp->t_state, TS_ISOPEN)) {
			if (pti->pt_flags & PF_PKT && (c = pti->pt_send)) {
				pti->pt_send = 0;
				mutex_spin_exit(&tty_lock);
				error = ureadc(c, uio);
				if (error != 0)
					return error;
				/*
				 * Since we don't have the tty locked, there's
				 * a risk of messing up `t_termios'. This is
				 * relevant only if the tty got closed and then
				 * opened again while we were out uiomoving.
				 */
				if (c & TIOCPKT_IOCTL) {
					cc = min(uio->uio_resid,
						sizeof(tp->t_termios));
					uiomove((void *) &tp->t_termios,
						cc, uio);
				}
				return 0;
			}
			if (pti->pt_flags & PF_UCNTL && (c = pti->pt_ucntl)) {
				pti->pt_ucntl = 0;
				mutex_spin_exit(&tty_lock);
				error = ureadc(c, uio);
				if (error != 0)
					return error;
				return 0;
			}
			if (tp->t_outq.c_cc && !ISSET(tp->t_state, TS_TTSTOP))
				break;
		}
		if (!ISSET(tp->t_state, TS_CARR_ON)) {
			error = 0;	/* EOF */
			goto out;
		}
		if (flag & IO_NDELAY) {
			error = EWOULDBLOCK;
			goto out;
		}
		error = cv_wait_sig(&tp->t_outcvf, &tty_lock);
		if (error != 0)
			goto out;
	}

	if (pti->pt_flags & (PF_PKT|PF_UCNTL)) {
		mutex_spin_exit(&tty_lock);
		error = ureadc(0, uio);
		mutex_spin_enter(&tty_lock);
		if (error == 0 && !ISSET(tp->t_state, TS_ISOPEN))
			error = EIO;
	}
	while (uio->uio_resid > 0 && error == 0) {
		cc = q_to_b(&tp->t_outq, bf, min(uio->uio_resid, BUFSIZ));
		if (cc <= 0)
			break;
		mutex_spin_exit(&tty_lock);
		error = uiomove(bf, cc, uio);
		mutex_spin_enter(&tty_lock);
		if (error == 0 && !ISSET(tp->t_state, TS_ISOPEN))
			error = EIO;
	}
	ttypull(tp);
out:
	mutex_spin_exit(&tty_lock);
	return error;
}


int
ptcwrite(dev_t dev, struct uio *uio, int flag)
{
	struct pt_softc *pti = pt_softc[minor(dev)];
	struct tty *tp = pti->pt_tty;
	u_char *cp = NULL;
	int cc = 0;
	u_char locbuf[BUFSIZ];
	int cnt = 0;
	int error = 0;

again:
	mutex_spin_enter(&tty_lock);
	if (!ISSET(tp->t_state, TS_ISOPEN))
		goto block;
	if (pti->pt_flags & PF_REMOTE) {
		if (tp->t_canq.c_cc)
			goto block;
		while (uio->uio_resid > 0 && tp->t_canq.c_cc < TTYHOG) {
			if (cc == 0) {
				cc = min(uio->uio_resid, BUFSIZ);
				cc = min(cc, TTYHOG - tp->t_canq.c_cc);
				cp = locbuf;
				mutex_spin_exit(&tty_lock);
				error = uiomove(cp, cc, uio);
				if (error != 0)
					return error;
				mutex_spin_enter(&tty_lock);
				/* check again for safety */
				if (!ISSET(tp->t_state, TS_ISOPEN)) {
					/*
					 * adjust for data copied in but not
					 * written
					 */
					uio->uio_resid += cc;
					error = EIO;
					goto out;
				}
			}
			if (cc) {
				cc = b_to_q(cp, cc, &tp->t_outq);
				if (cc > 0)
					goto block;
			}
		}
		(void) putc(0, &tp->t_canq);
		ttwakeup(tp);
		cv_broadcast(&tp->t_cancv);
		error = 0;
		goto out;
	}
	while (uio->uio_resid > 0) {
		if (cc == 0) {
			cc = min(uio->uio_resid, BUFSIZ);
			cp = locbuf;
			mutex_spin_exit(&tty_lock);
			error = uiomove(cp, cc, uio);
			if (error != 0)
				return error;
			mutex_spin_enter(&tty_lock);
			/* check again for safety */
			if (!ISSET(tp->t_state, TS_ISOPEN)) {
				/* adjust for data copied in but not written */
				uio->uio_resid += cc;
				error = EIO;
				goto out;
			}
		}
		while (cc > 0) {
			int used = tp->t_rawq.c_cc + tp->t_canq.c_cc;
			int canon = ISSET(tp->t_lflag, ICANON) ? 1 : 0;
			/*
			 * We need space for 2 characters if canonical
			 * because we might need to print ^C
			 */
			if (used >= (TTYHOG - canon) &&
			   (tp->t_canq.c_cc > 0 || !canon)) {
				cv_broadcast(&tp->t_rawcv);
				goto block;
			}
			/*
			 * XXX - should change l_rint to be called with lock
			 *	 see also tty.c:ttyinput_wlock()
			 */
			mutex_spin_exit(&tty_lock);
			(*tp->t_linesw->l_rint)(*cp++, tp);
			mutex_spin_enter(&tty_lock);
			cnt++;
			cc--;
		}
	}
	error = 0;
	goto out;

block:
	/*
	 * Come here to wait for slave to open, for space
	 * in outq, or space in rawq.
	 */
	if (!ISSET(tp->t_state, TS_CARR_ON)) {
		/* adjust for data copied in but not written */
		uio->uio_resid += cc;
		error = EIO;
		goto out;
	}
	if (flag & IO_NDELAY) {
		/* adjust for data copied in but not written */
		uio->uio_resid += cc;
		error = cnt == 0 ? EWOULDBLOCK : 0;
		goto out;
	}
	error = cv_wait_sig(&tp->t_rawcvf, &tty_lock);
	mutex_spin_exit(&tty_lock);
	if (error != 0) {
		/* adjust for data copied in but not written */
		uio->uio_resid += cc;
		return error;
	}
	goto again;

out:
	mutex_spin_exit(&tty_lock);
	return error;
}

int
ptcpoll(dev_t dev, int events, struct lwp *l)
{
	struct pt_softc *pti = pt_softc[minor(dev)];
	struct tty *tp = pti->pt_tty;
	int revents = 0;

	mutex_spin_enter(&tty_lock);

	if (events & (POLLIN | POLLRDNORM))
		if (ISSET(tp->t_state, TS_ISOPEN) &&
		    ((tp->t_outq.c_cc > 0 && !ISSET(tp->t_state, TS_TTSTOP)) ||
		     ((pti->pt_flags & PF_PKT) && pti->pt_send) ||
		     ((pti->pt_flags & PF_UCNTL) && pti->pt_ucntl)))
			revents |= events & (POLLIN | POLLRDNORM);

	if (events & (POLLOUT | POLLWRNORM))
		if (ISSET(tp->t_state, TS_ISOPEN) &&
		    ((pti->pt_flags & PF_REMOTE) ?
		     (tp->t_canq.c_cc == 0) :
		     ((tp->t_rawq.c_cc + tp->t_canq.c_cc < TTYHOG-2) ||
		      (tp->t_canq.c_cc == 0 && ISSET(tp->t_lflag, ICANON)))))
			revents |= events & (POLLOUT | POLLWRNORM);

	if (events & POLLHUP)
		if (!ISSET(tp->t_state, TS_CARR_ON))
			revents |= POLLHUP;

	if (revents == 0) {
		if (events & (POLLIN | POLLHUP | POLLRDNORM))
			selrecord(l, &pti->pt_selr);

		if (events & (POLLOUT | POLLWRNORM))
			selrecord(l, &pti->pt_selw);
	}

	mutex_spin_exit(&tty_lock);

	return revents;
}

static void
filt_ptcrdetach(struct knote *kn)
{
	struct pt_softc *pti;

	pti = kn->kn_hook;

	mutex_spin_enter(&tty_lock);
	SLIST_REMOVE(&pti->pt_selr.sel_klist, kn, knote, kn_selnext);
	mutex_spin_exit(&tty_lock);
}

static int
filt_ptcread(struct knote *kn, long hint)
{
	struct pt_softc *pti;
	struct tty	*tp;
	int canread;

	pti = kn->kn_hook;
	tp = pti->pt_tty;

	if ((hint & NOTE_SUBMIT) == 0) {
		mutex_spin_enter(&tty_lock);
	}

	canread = (ISSET(tp->t_state, TS_ISOPEN) &&
		    ((tp->t_outq.c_cc > 0 && !ISSET(tp->t_state, TS_TTSTOP)) ||
		     ((pti->pt_flags & PF_PKT) && pti->pt_send) ||
		     ((pti->pt_flags & PF_UCNTL) && pti->pt_ucntl)));

	if (canread) {
		/*
		 * c_cc is number of characters after output post-processing;
		 * the amount of data actually read(2) depends on
		 * setting of input flags for the terminal.
		 */
		kn->kn_data = tp->t_outq.c_cc;
		if (((pti->pt_flags & PF_PKT) && pti->pt_send) ||
		    ((pti->pt_flags & PF_UCNTL) && pti->pt_ucntl))
			kn->kn_data++;
	}

	if ((hint & NOTE_SUBMIT) == 0) {
		mutex_spin_exit(&tty_lock);
	}

	return canread;
}

static void
filt_ptcwdetach(struct knote *kn)
{
	struct pt_softc *pti;

	pti = kn->kn_hook;

	mutex_spin_enter(&tty_lock);
	SLIST_REMOVE(&pti->pt_selw.sel_klist, kn, knote, kn_selnext);
	mutex_spin_exit(&tty_lock);
}

static int
filt_ptcwrite(struct knote *kn, long hint)
{
	struct pt_softc *pti;
	struct tty	*tp;
	int canwrite;
	int nwrite;

	pti = kn->kn_hook;
	tp = pti->pt_tty;

	if ((hint & NOTE_SUBMIT) == 0) {
		mutex_spin_enter(&tty_lock);
	}

	canwrite = (ISSET(tp->t_state, TS_ISOPEN) &&
		    ((pti->pt_flags & PF_REMOTE) ?
		     (tp->t_canq.c_cc == 0) :
		     ((tp->t_rawq.c_cc + tp->t_canq.c_cc < TTYHOG-2) ||
		      (tp->t_canq.c_cc == 0 && ISSET(tp->t_lflag, ICANON)))));

	if (canwrite) {
		if (pti->pt_flags & PF_REMOTE)
			nwrite = tp->t_canq.c_cn;
		else {
			/* this is guaranteed to be > 0 due to above check */
			nwrite = tp->t_canq.c_cn
				- (tp->t_rawq.c_cc + tp->t_canq.c_cc);
		}
		kn->kn_data = nwrite;
	}

	if ((hint & NOTE_SUBMIT) == 0) {
		mutex_spin_exit(&tty_lock);
	}

	return canwrite;
}

static const struct filterops ptcread_filtops =
	{ 1, NULL, filt_ptcrdetach, filt_ptcread };
static const struct filterops ptcwrite_filtops =
	{ 1, NULL, filt_ptcwdetach, filt_ptcwrite };

int
ptckqfilter(dev_t dev, struct knote *kn)
{
	struct pt_softc *pti = pt_softc[minor(dev)];
	struct klist	*klist;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		klist = &pti->pt_selr.sel_klist;
		kn->kn_fop = &ptcread_filtops;
		break;
	case EVFILT_WRITE:
		klist = &pti->pt_selw.sel_klist;
		kn->kn_fop = &ptcwrite_filtops;
		break;
	default:
		return EINVAL;
	}

	kn->kn_hook = pti;

	mutex_spin_enter(&tty_lock);
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	mutex_spin_exit(&tty_lock);

	return 0;
}

struct tty *
ptytty(dev_t dev)
{
	struct pt_softc *pti = pt_softc[minor(dev)];
	struct tty *tp = pti->pt_tty;

	return tp;
}

/*ARGSUSED*/
int
ptyioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct pt_softc *pti = pt_softc[minor(dev)];
	struct tty *tp = pti->pt_tty;
	const struct cdevsw *cdev;
	u_char *cc = tp->t_cc;
	int stop, error, sig;
#ifndef NO_DEV_PTM
	struct mount *mp;
#endif

	/*
	 * IF CONTROLLER STTY THEN MUST FLUSH TO PREVENT A HANG.
	 * ttywflush(tp) will hang if there are characters in the outq.
	 */
	if (cmd == TIOCEXT) {
		/*
		 * When the EXTPROC bit is being toggled, we need
		 * to send an TIOCPKT_IOCTL if the packet driver
		 * is turned on.
		 */
		if (*(int *)data) {
			if (pti->pt_flags & PF_PKT) {
				pti->pt_send |= TIOCPKT_IOCTL;
				ptcwakeup(tp, FREAD);
			}
			SET(tp->t_lflag, EXTPROC);
		} else {
			if (ISSET(tp->t_lflag, EXTPROC) &&
			    (pti->pt_flags & PF_PKT)) {
				pti->pt_send |= TIOCPKT_IOCTL;
				ptcwakeup(tp, FREAD);
			}
			CLR(tp->t_lflag, EXTPROC);
		}
		return(0);
	}

#ifndef NO_DEV_PTM
	/* Allow getting the name from either the master or the slave */
	if (cmd == TIOCPTSNAME) {
		if ((error = pty_getmp(l, &mp)) != 0)
			return error;
		return pty_fill_ptmget(l, dev, -1, -1, data, mp);
	}
#endif

	cdev = cdevsw_lookup(dev);
	if (cdev != NULL && cdev->d_open == ptcopen)
		switch (cmd) {
#ifndef NO_DEV_PTM
		case TIOCGRANTPT:
			if ((error = pty_getmp(l, &mp)) != 0)
				return error;
			return pty_grant_slave(l, dev, mp);
#endif

		case TIOCGPGRP:
			/*
			 * We avoid calling ttioctl on the controller since,
			 * in that case, tp must be the controlling terminal.
			 */
			*(int *)data = tp->t_pgrp ? tp->t_pgrp->pg_id : 0;
			return 0;

		case TIOCPKT:
			if (*(int *)data) {
				if (pti->pt_flags & PF_UCNTL)
					return EINVAL;
				pti->pt_flags |= PF_PKT;
			} else
				pti->pt_flags &= ~PF_PKT;
			return 0;

		case TIOCUCNTL:
			if (*(int *)data) {
				if (pti->pt_flags & PF_PKT)
					return EINVAL;
				pti->pt_flags |= PF_UCNTL;
			} else
				pti->pt_flags &= ~PF_UCNTL;
			return 0;

		case TIOCREMOTE:
			if (*(int *)data)
				pti->pt_flags |= PF_REMOTE;
			else
				pti->pt_flags &= ~PF_REMOTE;
			mutex_spin_enter(&tty_lock);
			ttyflush(tp, FREAD|FWRITE);
			mutex_spin_exit(&tty_lock);
			return 0;

		case TIOCSETP:
		case TIOCSETN:
		case TIOCSETD:
		case TIOCSETA:
		case TIOCSETAW:
		case TIOCSETAF:
			mutex_spin_enter(&tty_lock);
			ndflush(&tp->t_outq, tp->t_outq.c_cc);
			mutex_spin_exit(&tty_lock);
			break;

		case TIOCSIG:
			sig = (int)(long)*(void **)data;
			if (sig <= 0 || sig >= NSIG)
				return EINVAL;
			mutex_spin_enter(&tty_lock);
			if (!ISSET(tp->t_lflag, NOFLSH))
				ttyflush(tp, FREAD|FWRITE);
			tp->t_state |= TS_SIGINFO;
			ttysig(tp, TTYSIG_PG1, sig);
			mutex_spin_exit(&tty_lock);
			return 0;

		case FIONREAD:
			mutex_spin_enter(&tty_lock);
			*(int *)data = tp->t_outq.c_cc;
			mutex_spin_exit(&tty_lock);
			return 0;
		}

	error = (*tp->t_linesw->l_ioctl)(tp, cmd, data, flag, l);
	if (error == EPASSTHROUGH)
		 error = ttioctl(tp, cmd, data, flag, l);
	if (error == EPASSTHROUGH) {
		if (pti->pt_flags & PF_UCNTL &&
		    (cmd & ~0xff) == UIOCCMD(0)) {
			if (cmd & 0xff) {
				pti->pt_ucntl = (u_char)cmd;
				ptcwakeup(tp, FREAD);
			}
			return 0;
		}
	}
	/*
	 * If external processing and packet mode send ioctl packet.
	 */
	if (ISSET(tp->t_lflag, EXTPROC) && (pti->pt_flags & PF_PKT)) {
		switch(cmd) {
		case TIOCSETA:
		case TIOCSETAW:
		case TIOCSETAF:
		case TIOCSETP:
		case TIOCSETN:
		case TIOCSETC:
		case TIOCSLTC:
		case TIOCLBIS:
		case TIOCLBIC:
		case TIOCLSET:
			pti->pt_send |= TIOCPKT_IOCTL;
			ptcwakeup(tp, FREAD);
		default:
			break;
		}
	}
	stop = ISSET(tp->t_iflag, IXON) && CCEQ(cc[VSTOP], CTRL('s'))
		&& CCEQ(cc[VSTART], CTRL('q'));
	if (pti->pt_flags & PF_NOSTOP) {
		if (stop) {
			pti->pt_send &= ~TIOCPKT_NOSTOP;
			pti->pt_send |= TIOCPKT_DOSTOP;
			pti->pt_flags &= ~PF_NOSTOP;
			ptcwakeup(tp, FREAD);
		}
	} else {
		if (!stop) {
			pti->pt_send &= ~TIOCPKT_DOSTOP;
			pti->pt_send |= TIOCPKT_NOSTOP;
			pti->pt_flags |= PF_NOSTOP;
			ptcwakeup(tp, FREAD);
		}
	}
	return error;
}

/*	$NetBSD: tty_tty.c,v 1.40 2014/07/25 08:10:40 dholland Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1991, 1993, 1995
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
 *	@(#)tty_tty.c	8.2 (Berkeley) 9/23/93
 */

/*
 * Indirect driver for controlling tty.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tty_tty.c,v 1.40 2014/07/25 08:10:40 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/conf.h>
#include <sys/kauth.h>

/* XXXSMP */
#define cttyvp(p) ((p)->p_lflag & PL_CONTROLT ? (p)->p_session->s_ttyvp : NULL)

/*ARGSUSED*/
static int
cttyopen(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct vnode *ttyvp = cttyvp(l->l_proc);
	int error;

	if (ttyvp == NULL)
		return (ENXIO);
	vn_lock(ttyvp, LK_EXCLUSIVE | LK_RETRY);
#ifdef PARANOID
	/*
	 * Since group is tty and mode is 620 on most terminal lines
	 * and since sessions protect terminals from processes outside
	 * your session, this check is probably no longer necessary.
	 * Since it inhibits setuid root programs that later switch
	 * to another user from accessing /dev/tty, we have decided
	 * to delete this test. (mckusick 5/93)
	 */
	error = VOP_ACCESS(ttyvp,
	  (flag&FREAD ? VREAD : 0) | (flag&FWRITE ? VWRITE : 0), l->l_cred, l);
	if (!error)
#endif /* PARANOID */
		error = VOP_OPEN(ttyvp, flag, NOCRED);
	VOP_UNLOCK(ttyvp);
	return (error);
}

/*ARGSUSED*/
static int
cttyread(dev_t dev, struct uio *uio, int flag)
{
	struct vnode *ttyvp = cttyvp(curproc);
	int error;

	if (ttyvp == NULL)
		return (EIO);
	vn_lock(ttyvp, LK_EXCLUSIVE | LK_RETRY);
	error = VOP_READ(ttyvp, uio, flag, NOCRED);
	VOP_UNLOCK(ttyvp);
	return (error);
}

/*ARGSUSED*/
static int
cttywrite(dev_t dev, struct uio *uio, int flag)
{
	struct vnode *ttyvp = cttyvp(curproc);
	int error;

	if (ttyvp == NULL)
		return (EIO);
	vn_lock(ttyvp, LK_EXCLUSIVE | LK_RETRY);
	error = VOP_WRITE(ttyvp, uio, flag, NOCRED);
	VOP_UNLOCK(ttyvp);
	return (error);
}

/*ARGSUSED*/
static int
cttyioctl(dev_t dev, u_long cmd, void *addr, int flag, struct lwp *l)
{
	struct vnode *ttyvp = cttyvp(l->l_proc);
	int rv;

	if (ttyvp == NULL)
		return (EIO);
	if (cmd == TIOCSCTTY)		/* XXX */
		return (EINVAL);
	if (cmd == TIOCNOTTY) {
		mutex_enter(proc_lock);
		if (!SESS_LEADER(l->l_proc)) {
			l->l_proc->p_lflag &= ~PL_CONTROLT;
			rv = 0;
		} else
			rv = EINVAL;
		mutex_exit(proc_lock);
		return (rv);
	}
	return (VOP_IOCTL(ttyvp, cmd, addr, flag, NOCRED));
}

/*ARGSUSED*/
static int
cttypoll(dev_t dev, int events, struct lwp *l)
{
	struct vnode *ttyvp = cttyvp(l->l_proc);

	if (ttyvp == NULL)
		return (seltrue(dev, events, l));
	return (VOP_POLL(ttyvp, events));
}

static int
cttykqfilter(dev_t dev, struct knote *kn)
{
	/* This is called from filt_fileattach() by the attaching process. */
	struct proc *p = curproc;
	struct vnode *ttyvp = cttyvp(p);

	if (ttyvp == NULL)
		return (1);
	return (VOP_KQFILTER(ttyvp, kn));
}

const struct cdevsw ctty_cdevsw = {
	.d_open = cttyopen,
	.d_close = nullclose,
	.d_read = cttyread,
	.d_write = cttywrite,
	.d_ioctl = cttyioctl,
	.d_stop = nullstop,
	.d_tty = notty,
	.d_poll = cttypoll,
	.d_mmap = nommap,
	.d_kqfilter = cttykqfilter,
	.d_discard = nodiscard,
	.d_flag = D_TTY
};

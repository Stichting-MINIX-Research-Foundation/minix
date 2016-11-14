/*	$NetBSD: subr_log.c,v 1.55 2015/05/20 11:18:36 pooka Exp $	*/

/*-
 * Copyright (c) 2007, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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

/*
 * Copyright (c) 1982, 1986, 1993
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
 *	@(#)subr_log.c	8.3 (Berkeley) 2/14/95
 */

/*
 * Error log buffer for kernel printf's.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_log.c,v 1.55 2015/05/20 11:18:36 pooka Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/ioctl.h>
#include <sys/msgbuf.h>
#include <sys/file.h>
#include <sys/syslog.h>
#include <sys/conf.h>
#include <sys/select.h>
#include <sys/poll.h> 
#include <sys/intr.h>
#include <sys/sysctl.h>
#include <sys/ktrace.h>

static int sysctl_msgbuf(SYSCTLFN_PROTO);

static void	logsoftintr(void *);

static bool	log_async;
static struct selinfo log_selp;		/* process waiting on select call */
static pid_t	log_pgid;		/* process/group for async I/O */
static kcondvar_t log_cv;
static void	*log_sih;

kmutex_t log_lock;
int	log_open;			/* also used in log() */
int	msgbufmapped;			/* is the message buffer mapped */
int	msgbufenabled;			/* is logging to the buffer enabled */
struct	kern_msgbuf *msgbufp;		/* the mapped buffer, itself. */

void
initmsgbuf(void *bf, size_t bufsize)
{
	struct kern_msgbuf *mbp;
	long new_bufs;

	/* Sanity-check the given size. */
	if (bufsize < sizeof(struct kern_msgbuf))
		return;

	mbp = msgbufp = (struct kern_msgbuf *)bf;

	new_bufs = bufsize - offsetof(struct kern_msgbuf, msg_bufc);
	if ((mbp->msg_magic != MSG_MAGIC) || (mbp->msg_bufs != new_bufs) ||
	    (mbp->msg_bufr < 0) || (mbp->msg_bufr >= mbp->msg_bufs) ||
	    (mbp->msg_bufx < 0) || (mbp->msg_bufx >= mbp->msg_bufs)) {
		/*
		 * If the buffer magic number is wrong, has changed
		 * size (which shouldn't happen often), or is
		 * internally inconsistent, initialize it.
		 */

		memset(bf, 0, bufsize);
		mbp->msg_magic = MSG_MAGIC;
		mbp->msg_bufs = new_bufs;
	}

	/* mark it as ready for use. */
	msgbufmapped = msgbufenabled = 1;
}

void
loginit(void)
{

	mutex_init(&log_lock, MUTEX_DEFAULT, IPL_VM);
	selinit(&log_selp);
	cv_init(&log_cv, "klog");
	log_sih = softint_establish(SOFTINT_CLOCK | SOFTINT_MPSAFE,
	    logsoftintr, NULL);

	sysctl_createv(NULL, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_INT, "msgbufsize",
		       SYSCTL_DESCR("Size of the kernel message buffer"),
		       sysctl_msgbuf, 0, NULL, 0,
		       CTL_KERN, KERN_MSGBUFSIZE, CTL_EOL);
	sysctl_createv(NULL, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_INT, "msgbuf",
		       SYSCTL_DESCR("Kernel message buffer"),
		       sysctl_msgbuf, 0, NULL, 0,
		       CTL_KERN, KERN_MSGBUF, CTL_EOL);
}

/*ARGSUSED*/
static int
logopen(dev_t dev, int flags, int mode, struct lwp *l)
{
	struct kern_msgbuf *mbp = msgbufp;
	int error = 0;

	mutex_spin_enter(&log_lock);
	if (log_open) {
		error = EBUSY;
	} else {
		log_open = 1;
		log_pgid = l->l_proc->p_pid;	/* signal process only */
		/*
		 * The message buffer is initialized during system
		 * configuration.  If it's been clobbered, note that
		 * and return an error.  (This allows a user to read
		 * the buffer via /dev/kmem, and try to figure out
		 * what clobbered it.
		 */
		if (mbp->msg_magic != MSG_MAGIC) {
			msgbufenabled = 0;
			error = ENXIO;
		}
	}
	mutex_spin_exit(&log_lock);

	return error;
}

/*ARGSUSED*/
static int
logclose(dev_t dev, int flag, int mode, struct lwp *l)
{

	mutex_spin_enter(&log_lock);
	log_pgid = 0;
	log_open = 0;
	log_async = 0;
	mutex_spin_exit(&log_lock);

	return 0;
}

/*ARGSUSED*/
static int
logread(dev_t dev, struct uio *uio, int flag)
{
	struct kern_msgbuf *mbp = msgbufp;
	long l;
	int error = 0;

	mutex_spin_enter(&log_lock);
	while (mbp->msg_bufr == mbp->msg_bufx) {
		if (flag & IO_NDELAY) {
			mutex_spin_exit(&log_lock);
			return EWOULDBLOCK;
		}
		error = cv_wait_sig(&log_cv, &log_lock);
		if (error) {
			mutex_spin_exit(&log_lock);
			return error;
		}
	}
	while (uio->uio_resid > 0) {
		l = mbp->msg_bufx - mbp->msg_bufr;
		if (l < 0)
			l = mbp->msg_bufs - mbp->msg_bufr;
		l = min(l, uio->uio_resid);
		if (l == 0)
			break;
		mutex_spin_exit(&log_lock);
		error = uiomove(&mbp->msg_bufc[mbp->msg_bufr], (int)l, uio);
		mutex_spin_enter(&log_lock);
		if (error)
			break;
		mbp->msg_bufr += l;
		if (mbp->msg_bufr < 0 || mbp->msg_bufr >= mbp->msg_bufs)
			mbp->msg_bufr = 0;
	}
	mutex_spin_exit(&log_lock);

	return error;
}

/*ARGSUSED*/
static int
logpoll(dev_t dev, int events, struct lwp *l)
{
	int revents = 0;

	if (events & (POLLIN | POLLRDNORM)) {
		mutex_spin_enter(&log_lock);
		if (msgbufp->msg_bufr != msgbufp->msg_bufx)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(l, &log_selp);
		mutex_spin_exit(&log_lock);
	}

	return revents;
}

static void
filt_logrdetach(struct knote *kn)
{

	mutex_spin_enter(&log_lock);
	SLIST_REMOVE(&log_selp.sel_klist, kn, knote, kn_selnext);
	mutex_spin_exit(&log_lock);
}

static int
filt_logread(struct knote *kn, long hint)
{
	int rv;

	if ((hint & NOTE_SUBMIT) == 0)
		mutex_spin_enter(&log_lock);
	if (msgbufp->msg_bufr == msgbufp->msg_bufx) {
		rv = 0;
	} else if (msgbufp->msg_bufr < msgbufp->msg_bufx) {
		kn->kn_data = msgbufp->msg_bufx - msgbufp->msg_bufr;
		rv = 1;
	} else {
		kn->kn_data = (msgbufp->msg_bufs - msgbufp->msg_bufr) +
		    msgbufp->msg_bufx;
		rv = 1;
	}
	if ((hint & NOTE_SUBMIT) == 0)
		mutex_spin_exit(&log_lock);

	return rv;
}

static const struct filterops logread_filtops =
	{ 1, NULL, filt_logrdetach, filt_logread };

static int
logkqfilter(dev_t dev, struct knote *kn)
{
	struct klist *klist;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		klist = &log_selp.sel_klist;
		kn->kn_fop = &logread_filtops;
		break;

	default:
		return (EINVAL);
	}

	mutex_spin_enter(&log_lock);
	kn->kn_hook = NULL;
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	mutex_spin_exit(&log_lock);

	return (0);
}

void
logwakeup(void)
{

	if (!cold && log_open) {
		mutex_spin_enter(&log_lock);
		selnotify(&log_selp, 0, NOTE_SUBMIT);
		if (log_async)
			softint_schedule(log_sih);
		cv_broadcast(&log_cv);
		mutex_spin_exit(&log_lock);
	}
}

static void
logsoftintr(void *cookie)
{
	pid_t pid;

	if ((pid = log_pgid) != 0)
		fownsignal(pid, SIGIO, 0, 0, NULL);
}

/*ARGSUSED*/
static int
logioctl(dev_t dev, u_long com, void *data, int flag, struct lwp *lwp)
{
	long l;

	switch (com) {

	/* return number of characters immediately available */
	case FIONREAD:
		mutex_spin_enter(&log_lock);
		l = msgbufp->msg_bufx - msgbufp->msg_bufr;
		if (l < 0)
			l += msgbufp->msg_bufs;
		mutex_spin_exit(&log_lock);
		*(int *)data = l;
		break;

	case FIONBIO:
		break;

	case FIOASYNC:
		/* No locking needed, 'thread private'. */
		log_async = (*((int *)data) != 0);
		break;

	case TIOCSPGRP:
	case FIOSETOWN:
		return fsetown(&log_pgid, com, data);

	case TIOCGPGRP:
	case FIOGETOWN:
		return fgetown(log_pgid, com, data);

	default:
		return (EPASSTHROUGH);
	}
	return (0);
}

void
logputchar(int c)
{
	struct kern_msgbuf *mbp;

	if (!cold)
		mutex_spin_enter(&log_lock);
	if (msgbufenabled) {
		mbp = msgbufp;
		if (mbp->msg_magic != MSG_MAGIC) {
			/*
			 * Arguably should panic or somehow notify the
			 * user...  but how?  Panic may be too drastic,
			 * and would obliterate the message being kicked
			 * out (maybe a panic itself), and printf
			 * would invoke us recursively.  Silently punt
			 * for now.  If syslog is running, it should
			 * notice.
			 */
			msgbufenabled = 0;
		} else {
			mbp->msg_bufc[mbp->msg_bufx++] = c;
			if (mbp->msg_bufx < 0 || mbp->msg_bufx >= mbp->msg_bufs)
				mbp->msg_bufx = 0;
			/* If the buffer is full, keep the most recent data. */
			if (mbp->msg_bufr == mbp->msg_bufx) {
				char c0;
				int i;

				/*
				 * Move forward read pointer to the next line
				 * in the buffer.  Note that the buffer is
				 * a ring buffer so we should reset msg_bufr
				 * to 0 when msg_bufr exceeds msg_bufs.
				 *
				 * To prevent to loop forever, give up if we
				 * cannot find a newline in mbp->msg_bufs
				 * characters (the max size of the buffer).
				 */
				for (i = 0; i < mbp->msg_bufs; i++) {
					c0 = mbp->msg_bufc[mbp->msg_bufr];
					if (++mbp->msg_bufr >= mbp->msg_bufs)
						mbp->msg_bufr = 0;
					if (c0 == '\n')
						break;
				}
			}
		}
	}
	if (!cold)
		mutex_spin_exit(&log_lock);
}

/*
 * sysctl helper routine for kern.msgbufsize and kern.msgbuf. For the
 * former it merely checks the message buffer is set up. For the latter,
 * it also copies out the data if necessary.
 */
static int
sysctl_msgbuf(SYSCTLFN_ARGS)
{
	char *where = oldp;
	size_t len, maxlen;
	long beg, end;
	extern kmutex_t log_lock;
	int error;

	if (!msgbufenabled || msgbufp->msg_magic != MSG_MAGIC) {
		msgbufenabled = 0;
		return (ENXIO);
	}

	switch (rnode->sysctl_num) {
	case KERN_MSGBUFSIZE: {
		struct sysctlnode node = *rnode;
		int msg_bufs = (int)msgbufp->msg_bufs;
		node.sysctl_data = &msg_bufs;
		return (sysctl_lookup(SYSCTLFN_CALL(&node)));
	}
	case KERN_MSGBUF:
		break;
	default:
		return (EOPNOTSUPP);
	}

	if (newp != NULL)
		return (EPERM);

	if (oldp == NULL) {
		/* always return full buffer size */
		*oldlenp = msgbufp->msg_bufs;
		return (0);
	}

	sysctl_unlock();

	/*
	 * First, copy from the write pointer to the end of
	 * message buffer.
	 */
	error = 0;
	mutex_spin_enter(&log_lock);
	maxlen = MIN(msgbufp->msg_bufs, *oldlenp);
	beg = msgbufp->msg_bufx;
	end = msgbufp->msg_bufs;
	mutex_spin_exit(&log_lock);

	while (maxlen > 0) {
		len = MIN(end - beg, maxlen);
		if (len == 0)
			break;
		/* XXX unlocked, but hardly matters. */
		error = copyout(&msgbufp->msg_bufc[beg], where, len);
		ktrmibio(-1, UIO_READ, where, len, error);
		if (error)
			break;
		where += len;
		maxlen -= len;

		/*
		 * ... then, copy from the beginning of message buffer to
		 * the write pointer.
		 */
		beg = 0;
		end = msgbufp->msg_bufx;
	}

	sysctl_relock();
	return (error);
}

const struct cdevsw log_cdevsw = {
	.d_open = logopen,
	.d_close = logclose,
	.d_read = logread,
	.d_write = nowrite,
	.d_ioctl = logioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = logpoll,
	.d_mmap = nommap,
	.d_kqfilter = logkqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER | D_MPSAFE
};

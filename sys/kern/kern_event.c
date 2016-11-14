/*	$NetBSD: kern_event.c,v 1.83 2015/03/02 19:24:53 christos Exp $	*/

/*-
 * Copyright (c) 2008, 2009 The NetBSD Foundation, Inc.
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

/*-
 * Copyright (c) 1999,2000,2001 Jonathan Lemon <jlemon@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * FreeBSD: src/sys/kern/kern_event.c,v 1.27 2001/07/05 17:10:44 rwatson Exp
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_event.c,v 1.83 2015/03/02 19:24:53 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/queue.h>
#include <sys/event.h>
#include <sys/eventvar.h>
#include <sys/poll.h>
#include <sys/kmem.h>
#include <sys/stat.h>
#include <sys/filedesc.h>
#include <sys/syscallargs.h>
#include <sys/kauth.h>
#include <sys/conf.h>
#include <sys/atomic.h>

static int	kqueue_scan(file_t *, size_t, struct kevent *,
			    const struct timespec *, register_t *,
			    const struct kevent_ops *, struct kevent *,
			    size_t);
static int	kqueue_ioctl(file_t *, u_long, void *);
static int	kqueue_fcntl(file_t *, u_int, void *);
static int	kqueue_poll(file_t *, int);
static int	kqueue_kqfilter(file_t *, struct knote *);
static int	kqueue_stat(file_t *, struct stat *);
static int	kqueue_close(file_t *);
static int	kqueue_register(struct kqueue *, struct kevent *);
static void	kqueue_doclose(struct kqueue *, struct klist *, int);

static void	knote_detach(struct knote *, filedesc_t *fdp, bool);
static void	knote_enqueue(struct knote *);
static void	knote_activate(struct knote *);

static void	filt_kqdetach(struct knote *);
static int	filt_kqueue(struct knote *, long hint);
static int	filt_procattach(struct knote *);
static void	filt_procdetach(struct knote *);
static int	filt_proc(struct knote *, long hint);
static int	filt_fileattach(struct knote *);
static void	filt_timerexpire(void *x);
static int	filt_timerattach(struct knote *);
static void	filt_timerdetach(struct knote *);
static int	filt_timer(struct knote *, long hint);

static const struct fileops kqueueops = {
	.fo_read = (void *)enxio,
	.fo_write = (void *)enxio,
	.fo_ioctl = kqueue_ioctl,
	.fo_fcntl = kqueue_fcntl,
	.fo_poll = kqueue_poll,
	.fo_stat = kqueue_stat,
	.fo_close = kqueue_close,
	.fo_kqfilter = kqueue_kqfilter,
	.fo_restart = fnullop_restart,
};

static const struct filterops kqread_filtops =
	{ 1, NULL, filt_kqdetach, filt_kqueue };
static const struct filterops proc_filtops =
	{ 0, filt_procattach, filt_procdetach, filt_proc };
static const struct filterops file_filtops =
	{ 1, filt_fileattach, NULL, NULL };
static const struct filterops timer_filtops =
	{ 0, filt_timerattach, filt_timerdetach, filt_timer };

static u_int	kq_ncallouts = 0;
static int	kq_calloutmax = (4 * 1024);

#define	KN_HASHSIZE		64		/* XXX should be tunable */
#define	KN_HASH(val, mask)	(((val) ^ (val >> 8)) & (mask))

extern const struct filterops sig_filtops;

/*
 * Table for for all system-defined filters.
 * These should be listed in the numeric order of the EVFILT_* defines.
 * If filtops is NULL, the filter isn't implemented in NetBSD.
 * End of list is when name is NULL.
 * 
 * Note that 'refcnt' is meaningless for built-in filters.
 */
struct kfilter {
	const char	*name;		/* name of filter */
	uint32_t	filter;		/* id of filter */
	unsigned	refcnt;		/* reference count */
	const struct filterops *filtops;/* operations for filter */
	size_t		namelen;	/* length of name string */
};

/* System defined filters */
static struct kfilter sys_kfilters[] = {
	{ "EVFILT_READ",	EVFILT_READ,	0, &file_filtops, 0 },
	{ "EVFILT_WRITE",	EVFILT_WRITE,	0, &file_filtops, 0, },
	{ "EVFILT_AIO",		EVFILT_AIO,	0, NULL, 0 },
	{ "EVFILT_VNODE",	EVFILT_VNODE,	0, &file_filtops, 0 },
	{ "EVFILT_PROC",	EVFILT_PROC,	0, &proc_filtops, 0 },
	{ "EVFILT_SIGNAL",	EVFILT_SIGNAL,	0, &sig_filtops, 0 },
	{ "EVFILT_TIMER",	EVFILT_TIMER,	0, &timer_filtops, 0 },
	{ NULL,			0,		0, NULL, 0 },
};

/* User defined kfilters */
static struct kfilter	*user_kfilters;		/* array */
static int		user_kfilterc;		/* current offset */
static int		user_kfiltermaxc;	/* max size so far */
static size_t		user_kfiltersz;		/* size of allocated memory */

/* Locks */
static krwlock_t	kqueue_filter_lock;	/* lock on filter lists */
static kmutex_t		kqueue_misc_lock;	/* miscellaneous */

static kauth_listener_t	kqueue_listener;

static int
kqueue_listener_cb(kauth_cred_t cred, kauth_action_t action, void *cookie,
    void *arg0, void *arg1, void *arg2, void *arg3)
{
	struct proc *p;
	int result;

	result = KAUTH_RESULT_DEFER;
	p = arg0;

	if (action != KAUTH_PROCESS_KEVENT_FILTER)
		return result;

	if ((kauth_cred_getuid(p->p_cred) != kauth_cred_getuid(cred) ||
	    ISSET(p->p_flag, PK_SUGID)))
		return result;

	result = KAUTH_RESULT_ALLOW;

	return result;
}

/*
 * Initialize the kqueue subsystem.
 */
void
kqueue_init(void)
{

	rw_init(&kqueue_filter_lock);
	mutex_init(&kqueue_misc_lock, MUTEX_DEFAULT, IPL_NONE);

	kqueue_listener = kauth_listen_scope(KAUTH_SCOPE_PROCESS,
	    kqueue_listener_cb, NULL);
}

/*
 * Find kfilter entry by name, or NULL if not found.
 */
static struct kfilter *
kfilter_byname_sys(const char *name)
{
	int i;

	KASSERT(rw_lock_held(&kqueue_filter_lock));

	for (i = 0; sys_kfilters[i].name != NULL; i++) {
		if (strcmp(name, sys_kfilters[i].name) == 0)
			return &sys_kfilters[i];
	}
	return NULL;
}

static struct kfilter *
kfilter_byname_user(const char *name)
{
	int i;

	KASSERT(rw_lock_held(&kqueue_filter_lock));

	/* user filter slots have a NULL name if previously deregistered */
	for (i = 0; i < user_kfilterc ; i++) {
		if (user_kfilters[i].name != NULL &&
		    strcmp(name, user_kfilters[i].name) == 0)
			return &user_kfilters[i];
	}
	return NULL;
}

static struct kfilter *
kfilter_byname(const char *name)
{
	struct kfilter *kfilter;

	KASSERT(rw_lock_held(&kqueue_filter_lock));

	if ((kfilter = kfilter_byname_sys(name)) != NULL)
		return kfilter;

	return kfilter_byname_user(name);
}

/*
 * Find kfilter entry by filter id, or NULL if not found.
 * Assumes entries are indexed in filter id order, for speed.
 */
static struct kfilter *
kfilter_byfilter(uint32_t filter)
{
	struct kfilter *kfilter;

	KASSERT(rw_lock_held(&kqueue_filter_lock));

	if (filter < EVFILT_SYSCOUNT)	/* it's a system filter */
		kfilter = &sys_kfilters[filter];
	else if (user_kfilters != NULL &&
	    filter < EVFILT_SYSCOUNT + user_kfilterc)
					/* it's a user filter */
		kfilter = &user_kfilters[filter - EVFILT_SYSCOUNT];
	else
		return (NULL);		/* out of range */
	KASSERT(kfilter->filter == filter);	/* sanity check! */
	return (kfilter);
}

/*
 * Register a new kfilter. Stores the entry in user_kfilters.
 * Returns 0 if operation succeeded, or an appropriate errno(2) otherwise.
 * If retfilter != NULL, the new filterid is returned in it.
 */
int
kfilter_register(const char *name, const struct filterops *filtops,
		 int *retfilter)
{
	struct kfilter *kfilter;
	size_t len;
	int i;

	if (name == NULL || name[0] == '\0' || filtops == NULL)
		return (EINVAL);	/* invalid args */

	rw_enter(&kqueue_filter_lock, RW_WRITER);
	if (kfilter_byname(name) != NULL) {
		rw_exit(&kqueue_filter_lock);
		return (EEXIST);	/* already exists */
	}
	if (user_kfilterc > 0xffffffff - EVFILT_SYSCOUNT) {
		rw_exit(&kqueue_filter_lock);
		return (EINVAL);	/* too many */
	}

	for (i = 0; i < user_kfilterc; i++) {
		kfilter = &user_kfilters[i];
		if (kfilter->name == NULL) {
			/* Previously deregistered slot.  Reuse. */
			goto reuse;
		}
	}

	/* check if need to grow user_kfilters */
	if (user_kfilterc + 1 > user_kfiltermaxc) {
		/* Grow in KFILTER_EXTENT chunks. */
		user_kfiltermaxc += KFILTER_EXTENT;
		len = user_kfiltermaxc * sizeof(*kfilter);
		kfilter = kmem_alloc(len, KM_SLEEP);
		memset((char *)kfilter + user_kfiltersz, 0, len - user_kfiltersz);
		if (user_kfilters != NULL) {
			memcpy(kfilter, user_kfilters, user_kfiltersz);
			kmem_free(user_kfilters, user_kfiltersz);
		}
		user_kfiltersz = len;
		user_kfilters = kfilter;
	}
	/* Adding new slot */
	kfilter = &user_kfilters[user_kfilterc++];
reuse:
	kfilter->namelen = strlen(name) + 1;
	kfilter->name = kmem_alloc(kfilter->namelen, KM_SLEEP);
	memcpy(__UNCONST(kfilter->name), name, kfilter->namelen);

	kfilter->filter = (kfilter - user_kfilters) + EVFILT_SYSCOUNT;

	kfilter->filtops = kmem_alloc(sizeof(*filtops), KM_SLEEP);
	memcpy(__UNCONST(kfilter->filtops), filtops, sizeof(*filtops));

	if (retfilter != NULL)
		*retfilter = kfilter->filter;
	rw_exit(&kqueue_filter_lock);

	return (0);
}

/*
 * Unregister a kfilter previously registered with kfilter_register.
 * This retains the filter id, but clears the name and frees filtops (filter
 * operations), so that the number isn't reused during a boot.
 * Returns 0 if operation succeeded, or an appropriate errno(2) otherwise.
 */
int
kfilter_unregister(const char *name)
{
	struct kfilter *kfilter;

	if (name == NULL || name[0] == '\0')
		return (EINVAL);	/* invalid name */

	rw_enter(&kqueue_filter_lock, RW_WRITER);
	if (kfilter_byname_sys(name) != NULL) {
		rw_exit(&kqueue_filter_lock);
		return (EINVAL);	/* can't detach system filters */
	}

	kfilter = kfilter_byname_user(name);
	if (kfilter == NULL) {
		rw_exit(&kqueue_filter_lock);
		return (ENOENT);
	}
	if (kfilter->refcnt != 0) {
		rw_exit(&kqueue_filter_lock);
		return (EBUSY);
	}

	/* Cast away const (but we know it's safe. */
	kmem_free(__UNCONST(kfilter->name), kfilter->namelen);
	kfilter->name = NULL;	/* mark as `not implemented' */

	if (kfilter->filtops != NULL) {
		/* Cast away const (but we know it's safe. */
		kmem_free(__UNCONST(kfilter->filtops),
		    sizeof(*kfilter->filtops));
		kfilter->filtops = NULL; /* mark as `not implemented' */
	}
	rw_exit(&kqueue_filter_lock);

	return (0);
}


/*
 * Filter attach method for EVFILT_READ and EVFILT_WRITE on normal file
 * descriptors. Calls fileops kqfilter method for given file descriptor.
 */
static int
filt_fileattach(struct knote *kn)
{
	file_t *fp;

	fp = kn->kn_obj;

	return (*fp->f_ops->fo_kqfilter)(fp, kn);
}

/*
 * Filter detach method for EVFILT_READ on kqueue descriptor.
 */
static void
filt_kqdetach(struct knote *kn)
{
	struct kqueue *kq;

	kq = ((file_t *)kn->kn_obj)->f_kqueue;

	mutex_spin_enter(&kq->kq_lock);
	SLIST_REMOVE(&kq->kq_sel.sel_klist, kn, knote, kn_selnext);
	mutex_spin_exit(&kq->kq_lock);
}

/*
 * Filter event method for EVFILT_READ on kqueue descriptor.
 */
/*ARGSUSED*/
static int
filt_kqueue(struct knote *kn, long hint)
{
	struct kqueue *kq;
	int rv;

	kq = ((file_t *)kn->kn_obj)->f_kqueue;

	if (hint != NOTE_SUBMIT)
		mutex_spin_enter(&kq->kq_lock);
	kn->kn_data = kq->kq_count;
	rv = (kn->kn_data > 0);
	if (hint != NOTE_SUBMIT)
		mutex_spin_exit(&kq->kq_lock);

	return rv;
}

/*
 * Filter attach method for EVFILT_PROC.
 */
static int
filt_procattach(struct knote *kn)
{
	struct proc *p;
	struct lwp *curl;

	curl = curlwp;

	mutex_enter(proc_lock);
	if (kn->kn_flags & EV_FLAG1) {
		/*
		 * NOTE_TRACK attaches to the child process too early
		 * for proc_find, so do a raw look up and check the state
		 * explicitly.
		 */
		p = proc_find_raw(kn->kn_id);
		if (p != NULL && p->p_stat != SIDL)
			p = NULL;
	} else {
		p = proc_find(kn->kn_id);
	}

	if (p == NULL) {
		mutex_exit(proc_lock);
		return ESRCH;
	}

	/*
	 * Fail if it's not owned by you, or the last exec gave us
	 * setuid/setgid privs (unless you're root).
	 */
	mutex_enter(p->p_lock);
	mutex_exit(proc_lock);
	if (kauth_authorize_process(curl->l_cred, KAUTH_PROCESS_KEVENT_FILTER,
	    p, NULL, NULL, NULL) != 0) {
	    	mutex_exit(p->p_lock);
		return EACCES;
	}

	kn->kn_obj = p;
	kn->kn_flags |= EV_CLEAR;	/* automatically set */

	/*
	 * internal flag indicating registration done by kernel
	 */
	if (kn->kn_flags & EV_FLAG1) {
		kn->kn_data = kn->kn_sdata;	/* ppid */
		kn->kn_fflags = NOTE_CHILD;
		kn->kn_flags &= ~EV_FLAG1;
	}
	SLIST_INSERT_HEAD(&p->p_klist, kn, kn_selnext);
    	mutex_exit(p->p_lock);

	return 0;
}

/*
 * Filter detach method for EVFILT_PROC.
 *
 * The knote may be attached to a different process, which may exit,
 * leaving nothing for the knote to be attached to.  So when the process
 * exits, the knote is marked as DETACHED and also flagged as ONESHOT so
 * it will be deleted when read out.  However, as part of the knote deletion,
 * this routine is called, so a check is needed to avoid actually performing
 * a detach, because the original process might not exist any more.
 */
static void
filt_procdetach(struct knote *kn)
{
	struct proc *p;

	if (kn->kn_status & KN_DETACHED)
		return;

	p = kn->kn_obj;

	mutex_enter(p->p_lock);
	SLIST_REMOVE(&p->p_klist, kn, knote, kn_selnext);
	mutex_exit(p->p_lock);
}

/*
 * Filter event method for EVFILT_PROC.
 */
static int
filt_proc(struct knote *kn, long hint)
{
	u_int event, fflag;
	struct kevent kev;
	struct kqueue *kq;
	int error;

	event = (u_int)hint & NOTE_PCTRLMASK;
	kq = kn->kn_kq;
	fflag = 0;

	/* If the user is interested in this event, record it. */
	if (kn->kn_sfflags & event)
		fflag |= event;

	if (event == NOTE_EXIT) {
		struct proc *p = kn->kn_obj;

		if (p != NULL)
			kn->kn_data = p->p_xstat;
		/*
		 * Process is gone, so flag the event as finished.
		 *
		 * Detach the knote from watched process and mark
		 * it as such. We can't leave this to kqueue_scan(),
		 * since the process might not exist by then. And we
		 * have to do this now, since psignal KNOTE() is called
		 * also for zombies and we might end up reading freed
		 * memory if the kevent would already be picked up
		 * and knote g/c'ed.
		 */
		filt_procdetach(kn);

		mutex_spin_enter(&kq->kq_lock);
		kn->kn_status |= KN_DETACHED;
		/* Mark as ONESHOT, so that the knote it g/c'ed when read */
		kn->kn_flags |= (EV_EOF | EV_ONESHOT);
		kn->kn_fflags |= fflag;
		mutex_spin_exit(&kq->kq_lock);

		return 1;
	}

	mutex_spin_enter(&kq->kq_lock);
	if ((event == NOTE_FORK) && (kn->kn_sfflags & NOTE_TRACK)) {
		/*
		 * Process forked, and user wants to track the new process,
		 * so attach a new knote to it, and immediately report an
		 * event with the parent's pid.  Register knote with new
		 * process.
		 */
		kev.ident = hint & NOTE_PDATAMASK;	/* pid */
		kev.filter = kn->kn_filter;
		kev.flags = kn->kn_flags | EV_ADD | EV_ENABLE | EV_FLAG1;
		kev.fflags = kn->kn_sfflags;
		kev.data = kn->kn_id;			/* parent */
		kev.udata = kn->kn_kevent.udata;	/* preserve udata */
		mutex_spin_exit(&kq->kq_lock);
		error = kqueue_register(kq, &kev);
		mutex_spin_enter(&kq->kq_lock);
		if (error != 0)
			kn->kn_fflags |= NOTE_TRACKERR;
	}
	kn->kn_fflags |= fflag;
	fflag = kn->kn_fflags;
	mutex_spin_exit(&kq->kq_lock);

	return fflag != 0;
}

static void
filt_timerexpire(void *knx)
{
	struct knote *kn = knx;
	int tticks;

	mutex_enter(&kqueue_misc_lock);
	kn->kn_data++;
	knote_activate(kn);
	if ((kn->kn_flags & EV_ONESHOT) == 0) {
		tticks = mstohz(kn->kn_sdata);
		if (tticks <= 0)
			tticks = 1;
		callout_schedule((callout_t *)kn->kn_hook, tticks);
	}
	mutex_exit(&kqueue_misc_lock);
}

/*
 * data contains amount of time to sleep, in milliseconds
 */
static int
filt_timerattach(struct knote *kn)
{
	callout_t *calloutp;
	struct kqueue *kq;
	int tticks;

	tticks = mstohz(kn->kn_sdata);

	/* if the supplied value is under our resolution, use 1 tick */
	if (tticks == 0) {
		if (kn->kn_sdata == 0)
			return EINVAL;
		tticks = 1;
	}

	if (atomic_inc_uint_nv(&kq_ncallouts) >= kq_calloutmax ||
	    (calloutp = kmem_alloc(sizeof(*calloutp), KM_NOSLEEP)) == NULL) {
		atomic_dec_uint(&kq_ncallouts);
		return ENOMEM;
	}
	callout_init(calloutp, CALLOUT_MPSAFE);

	kq = kn->kn_kq;
	mutex_spin_enter(&kq->kq_lock);
	kn->kn_flags |= EV_CLEAR;		/* automatically set */
	kn->kn_hook = calloutp;
	mutex_spin_exit(&kq->kq_lock);

	callout_reset(calloutp, tticks, filt_timerexpire, kn);

	return (0);
}

static void
filt_timerdetach(struct knote *kn)
{
	callout_t *calloutp;

	calloutp = (callout_t *)kn->kn_hook;
	callout_halt(calloutp, NULL);
	callout_destroy(calloutp);
	kmem_free(calloutp, sizeof(*calloutp));
	atomic_dec_uint(&kq_ncallouts);
}

static int
filt_timer(struct knote *kn, long hint)
{
	int rv;

	mutex_enter(&kqueue_misc_lock);
	rv = (kn->kn_data != 0);
	mutex_exit(&kqueue_misc_lock);

	return rv;
}

/*
 * filt_seltrue:
 *
 *	This filter "event" routine simulates seltrue().
 */
int
filt_seltrue(struct knote *kn, long hint)
{

	/*
	 * We don't know how much data can be read/written,
	 * but we know that it *can* be.  This is about as
	 * good as select/poll does as well.
	 */
	kn->kn_data = 0;
	return (1);
}

/*
 * This provides full kqfilter entry for device switch tables, which
 * has same effect as filter using filt_seltrue() as filter method.
 */
static void
filt_seltruedetach(struct knote *kn)
{
	/* Nothing to do */
}

const struct filterops seltrue_filtops =
	{ 1, NULL, filt_seltruedetach, filt_seltrue };

int
seltrue_kqfilter(dev_t dev, struct knote *kn)
{
	switch (kn->kn_filter) {
	case EVFILT_READ:
	case EVFILT_WRITE:
		kn->kn_fop = &seltrue_filtops;
		break;
	default:
		return (EINVAL);
	}

	/* Nothing more to do */
	return (0);
}

/*
 * kqueue(2) system call.
 */
static int
kqueue1(struct lwp *l, int flags, register_t *retval)
{
	struct kqueue *kq;
	file_t *fp;
	int fd, error;

	if ((error = fd_allocfile(&fp, &fd)) != 0)
		return error;
	fp->f_flag = FREAD | FWRITE | (flags & (FNONBLOCK|FNOSIGPIPE));
	fp->f_type = DTYPE_KQUEUE;
	fp->f_ops = &kqueueops;
	kq = kmem_zalloc(sizeof(*kq), KM_SLEEP);
	mutex_init(&kq->kq_lock, MUTEX_DEFAULT, IPL_SCHED);
	cv_init(&kq->kq_cv, "kqueue");
	selinit(&kq->kq_sel);
	TAILQ_INIT(&kq->kq_head);
	fp->f_kqueue = kq;
	*retval = fd;
	kq->kq_fdp = curlwp->l_fd;
	fd_set_exclose(l, fd, (flags & O_CLOEXEC) != 0);
	fd_affix(curproc, fp, fd);
	return error;
}

/*
 * kqueue(2) system call.
 */
int
sys_kqueue(struct lwp *l, const void *v, register_t *retval)
{
	return kqueue1(l, 0, retval);
}

int
sys_kqueue1(struct lwp *l, const struct sys_kqueue1_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int) flags;
	} */
	return kqueue1(l, SCARG(uap, flags), retval);
}

/*
 * kevent(2) system call.
 */
int
kevent_fetch_changes(void *ctx, const struct kevent *changelist,
    struct kevent *changes, size_t index, int n)
{

	return copyin(changelist + index, changes, n * sizeof(*changes));
}

int
kevent_put_events(void *ctx, struct kevent *events,
    struct kevent *eventlist, size_t index, int n)
{

	return copyout(events, eventlist + index, n * sizeof(*events));
}

static const struct kevent_ops kevent_native_ops = {
	.keo_private = NULL,
	.keo_fetch_timeout = copyin,
	.keo_fetch_changes = kevent_fetch_changes,
	.keo_put_events = kevent_put_events,
};

int
sys___kevent50(struct lwp *l, const struct sys___kevent50_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(const struct kevent *) changelist;
		syscallarg(size_t) nchanges;
		syscallarg(struct kevent *) eventlist;
		syscallarg(size_t) nevents;
		syscallarg(const struct timespec *) timeout;
	} */

	return kevent1(retval, SCARG(uap, fd), SCARG(uap, changelist),
	    SCARG(uap, nchanges), SCARG(uap, eventlist), SCARG(uap, nevents),
	    SCARG(uap, timeout), &kevent_native_ops);
}

int
kevent1(register_t *retval, int fd,
	const struct kevent *changelist, size_t nchanges,
	struct kevent *eventlist, size_t nevents,
	const struct timespec *timeout,
	const struct kevent_ops *keops)
{
	struct kevent *kevp;
	struct kqueue *kq;
	struct timespec	ts;
	size_t i, n, ichange;
	int nerrors, error;
	struct kevent kevbuf[KQ_NEVENTS];	/* approx 300 bytes on 64-bit */
	file_t *fp;

	/* check that we're dealing with a kq */
	fp = fd_getfile(fd);
	if (fp == NULL)
		return (EBADF);

	if (fp->f_type != DTYPE_KQUEUE) {
		fd_putfile(fd);
		return (EBADF);
	}

	if (timeout != NULL) {
		error = (*keops->keo_fetch_timeout)(timeout, &ts, sizeof(ts));
		if (error)
			goto done;
		timeout = &ts;
	}

	kq = fp->f_kqueue;
	nerrors = 0;
	ichange = 0;

	/* traverse list of events to register */
	while (nchanges > 0) {
		n = MIN(nchanges, __arraycount(kevbuf));
		error = (*keops->keo_fetch_changes)(keops->keo_private,
		    changelist, kevbuf, ichange, n);
		if (error)
			goto done;
		for (i = 0; i < n; i++) {
			kevp = &kevbuf[i];
			kevp->flags &= ~EV_SYSFLAGS;
			/* register each knote */
			error = kqueue_register(kq, kevp);
			if (error) {
				if (nevents != 0) {
					kevp->flags = EV_ERROR;
					kevp->data = error;
					error = (*keops->keo_put_events)
					    (keops->keo_private, kevp,
					    eventlist, nerrors, 1);
					if (error)
						goto done;
					nevents--;
					nerrors++;
				} else {
					goto done;
				}
			}
		}
		nchanges -= n;	/* update the results */
		ichange += n;
	}
	if (nerrors) {
		*retval = nerrors;
		error = 0;
		goto done;
	}

	/* actually scan through the events */
	error = kqueue_scan(fp, nevents, eventlist, timeout, retval, keops,
	    kevbuf, __arraycount(kevbuf));
 done:
	fd_putfile(fd);
	return (error);
}

/*
 * Register a given kevent kev onto the kqueue
 */
static int
kqueue_register(struct kqueue *kq, struct kevent *kev)
{
	struct kfilter *kfilter;
	filedesc_t *fdp;
	file_t *fp;
	fdfile_t *ff;
	struct knote *kn, *newkn;
	struct klist *list;
	int error, fd, rv;

	fdp = kq->kq_fdp;
	fp = NULL;
	kn = NULL;
	error = 0;
	fd = 0;

	newkn = kmem_zalloc(sizeof(*newkn), KM_SLEEP);

	rw_enter(&kqueue_filter_lock, RW_READER);
	kfilter = kfilter_byfilter(kev->filter);
	if (kfilter == NULL || kfilter->filtops == NULL) {
		/* filter not found nor implemented */
		rw_exit(&kqueue_filter_lock);
		kmem_free(newkn, sizeof(*newkn));
		return (EINVAL);
	}

	/* search if knote already exists */
	if (kfilter->filtops->f_isfd) {
		/* monitoring a file descriptor */
		fd = kev->ident;
		if ((fp = fd_getfile(fd)) == NULL) {
			rw_exit(&kqueue_filter_lock);
			kmem_free(newkn, sizeof(*newkn));
			return EBADF;
		}
		mutex_enter(&fdp->fd_lock);
		ff = fdp->fd_dt->dt_ff[fd];
		if (fd <= fdp->fd_lastkqfile) {
			SLIST_FOREACH(kn, &ff->ff_knlist, kn_link) {
				if (kq == kn->kn_kq &&
				    kev->filter == kn->kn_filter)
					break;
			}
		}
	} else {
		/*
		 * not monitoring a file descriptor, so
		 * lookup knotes in internal hash table
		 */
		mutex_enter(&fdp->fd_lock);
		if (fdp->fd_knhashmask != 0) {
			list = &fdp->fd_knhash[
			    KN_HASH((u_long)kev->ident, fdp->fd_knhashmask)];
			SLIST_FOREACH(kn, list, kn_link) {
				if (kev->ident == kn->kn_id &&
				    kq == kn->kn_kq &&
				    kev->filter == kn->kn_filter)
					break;
			}
		}
	}

	/*
	 * kn now contains the matching knote, or NULL if no match
	 */
	if (kev->flags & EV_ADD) {
		if (kn == NULL) {
			/* create new knote */
			kn = newkn;
			newkn = NULL;
			kn->kn_obj = fp;
			kn->kn_id = kev->ident;
			kn->kn_kq = kq;
			kn->kn_fop = kfilter->filtops;
			kn->kn_kfilter = kfilter;
			kn->kn_sfflags = kev->fflags;
			kn->kn_sdata = kev->data;
			kev->fflags = 0;
			kev->data = 0;
			kn->kn_kevent = *kev;

			/*
			 * apply reference count to knote structure, and
			 * do not release it at the end of this routine.
			 */
			fp = NULL;

			if (!kn->kn_fop->f_isfd) {
				/*
				 * If knote is not on an fd, store on
				 * internal hash table.
				 */
				if (fdp->fd_knhashmask == 0) {
					/* XXXAD can block with fd_lock held */
					fdp->fd_knhash = hashinit(KN_HASHSIZE,
					    HASH_LIST, true,
					    &fdp->fd_knhashmask);
				}
				list = &fdp->fd_knhash[KN_HASH(kn->kn_id,
				    fdp->fd_knhashmask)];
			} else {
				/* Otherwise, knote is on an fd. */
				list = (struct klist *)
				    &fdp->fd_dt->dt_ff[kn->kn_id]->ff_knlist;
				if ((int)kn->kn_id > fdp->fd_lastkqfile)
					fdp->fd_lastkqfile = kn->kn_id;
			}
			SLIST_INSERT_HEAD(list, kn, kn_link);

			KERNEL_LOCK(1, NULL);		/* XXXSMP */
			error = (*kfilter->filtops->f_attach)(kn);
			KERNEL_UNLOCK_ONE(NULL);	/* XXXSMP */
			if (error != 0) {
#ifdef DIAGNOSTIC
				printf("%s: event not supported for file type"
				    " %d\n", __func__, fp ? fp->f_type : -1);
#endif
				/* knote_detach() drops fdp->fd_lock */
				knote_detach(kn, fdp, false);
				goto done;
			}
			atomic_inc_uint(&kfilter->refcnt);
		} else {
			/*
			 * The user may change some filter values after the
			 * initial EV_ADD, but doing so will not reset any
			 * filter which have already been triggered.
			 */
			kn->kn_sfflags = kev->fflags;
			kn->kn_sdata = kev->data;
			kn->kn_kevent.udata = kev->udata;
		}
		/*
		 * We can get here if we are trying to attach
		 * an event to a file descriptor that does not
		 * support events, and the attach routine is
		 * broken and does not return an error.
		 */
		KASSERT(kn->kn_fop->f_event != NULL);
		KERNEL_LOCK(1, NULL);			/* XXXSMP */
		rv = (*kn->kn_fop->f_event)(kn, 0);
		KERNEL_UNLOCK_ONE(NULL);		/* XXXSMP */
		if (rv)
			knote_activate(kn);
	} else {
		if (kn == NULL) {
			error = ENOENT;
		 	mutex_exit(&fdp->fd_lock);
			goto done;
		}
		if (kev->flags & EV_DELETE) {
			/* knote_detach() drops fdp->fd_lock */
			knote_detach(kn, fdp, true);
			goto done;
		}
	}

	/* disable knote */
	if ((kev->flags & EV_DISABLE)) {
		mutex_spin_enter(&kq->kq_lock);
		if ((kn->kn_status & KN_DISABLED) == 0)
			kn->kn_status |= KN_DISABLED;
		mutex_spin_exit(&kq->kq_lock);
	}

	/* enable knote */
	if ((kev->flags & EV_ENABLE)) {
		knote_enqueue(kn);
	}
	mutex_exit(&fdp->fd_lock);
 done:
	rw_exit(&kqueue_filter_lock);
	if (newkn != NULL)
		kmem_free(newkn, sizeof(*newkn));
	if (fp != NULL)
		fd_putfile(fd);
	return (error);
}

#if defined(DEBUG)
static void
kq_check(struct kqueue *kq)
{
	const struct knote *kn;
	int count;
	int nmarker;

	KASSERT(mutex_owned(&kq->kq_lock));
	KASSERT(kq->kq_count >= 0);

	count = 0;
	nmarker = 0;
	TAILQ_FOREACH(kn, &kq->kq_head, kn_tqe) {
		if ((kn->kn_status & (KN_MARKER | KN_QUEUED)) == 0) {
			panic("%s: kq=%p kn=%p inconsist 1", __func__, kq, kn);
		}
		if ((kn->kn_status & KN_MARKER) == 0) {
			if (kn->kn_kq != kq) {
				panic("%s: kq=%p kn=%p inconsist 2",
				    __func__, kq, kn);
			}
			if ((kn->kn_status & KN_ACTIVE) == 0) {
				panic("%s: kq=%p kn=%p: not active",
				    __func__, kq, kn);
			}
			count++;
			if (count > kq->kq_count) {
				goto bad;
			}
		} else {
			nmarker++;
#if 0
			if (nmarker > 10000) {
				panic("%s: kq=%p too many markers: %d != %d, "
				    "nmarker=%d",
				    __func__, kq, kq->kq_count, count, nmarker);
			}
#endif
		}
	}
	if (kq->kq_count != count) {
bad:
		panic("%s: kq=%p inconsist 3: %d != %d, nmarker=%d",
		    __func__, kq, kq->kq_count, count, nmarker);
	}
}
#else /* defined(DEBUG) */
#define	kq_check(a)	/* nothing */
#endif /* defined(DEBUG) */

/*
 * Scan through the list of events on fp (for a maximum of maxevents),
 * returning the results in to ulistp. Timeout is determined by tsp; if
 * NULL, wait indefinitely, if 0 valued, perform a poll, otherwise wait
 * as appropriate.
 */
static int
kqueue_scan(file_t *fp, size_t maxevents, struct kevent *ulistp,
	    const struct timespec *tsp, register_t *retval,
	    const struct kevent_ops *keops, struct kevent *kevbuf,
	    size_t kevcnt)
{
	struct kqueue	*kq;
	struct kevent	*kevp;
	struct timespec	ats, sleepts;
	struct knote	*kn, *marker;
	size_t		count, nkev, nevents;
	int		timeout, error, rv;
	filedesc_t	*fdp;

	fdp = curlwp->l_fd;
	kq = fp->f_kqueue;
	count = maxevents;
	nkev = nevents = error = 0;
	if (count == 0) {
		*retval = 0;
		return 0;
	}

	if (tsp) {				/* timeout supplied */
		ats = *tsp;
		if (inittimeleft(&ats, &sleepts) == -1) {
			*retval = maxevents;
			return EINVAL;
		}
		timeout = tstohz(&ats);
		if (timeout <= 0)
			timeout = -1;           /* do poll */
	} else {
		/* no timeout, wait forever */
		timeout = 0;
	}	

	marker = kmem_zalloc(sizeof(*marker), KM_SLEEP);
	marker->kn_status = KN_MARKER;
	mutex_spin_enter(&kq->kq_lock);
 retry:
	kevp = kevbuf;
	if (kq->kq_count == 0) {
		if (timeout >= 0) {
			error = cv_timedwait_sig(&kq->kq_cv,
			    &kq->kq_lock, timeout);
			if (error == 0) {
				 if (tsp == NULL || (timeout =
				     gettimeleft(&ats, &sleepts)) > 0)
					goto retry;
			} else {
				/* don't restart after signals... */
				if (error == ERESTART)
					error = EINTR;
				if (error == EWOULDBLOCK)
					error = 0;
			}
		}
	} else {
		/* mark end of knote list */
		TAILQ_INSERT_TAIL(&kq->kq_head, marker, kn_tqe);

		while (count != 0) {
			kn = TAILQ_FIRST(&kq->kq_head);	/* get next knote */
			while ((kn->kn_status & KN_MARKER) != 0) {
				if (kn == marker) {
					/* it's our marker, stop */
					TAILQ_REMOVE(&kq->kq_head, kn, kn_tqe);
					if (count < maxevents || (tsp != NULL &&
					    (timeout = gettimeleft(&ats,
					    &sleepts)) <= 0))
						goto done;
					goto retry;
				}
				/* someone else's marker. */
				kn = TAILQ_NEXT(kn, kn_tqe);
			}
			kq_check(kq);
			TAILQ_REMOVE(&kq->kq_head, kn, kn_tqe);
			kq->kq_count--;
			kn->kn_status &= ~KN_QUEUED;
			kq_check(kq);
			if (kn->kn_status & KN_DISABLED) {
				/* don't want disabled events */
				continue;
			}
			if ((kn->kn_flags & EV_ONESHOT) == 0) {
				mutex_spin_exit(&kq->kq_lock);
				KERNEL_LOCK(1, NULL);		/* XXXSMP */
				rv = (*kn->kn_fop->f_event)(kn, 0);
				KERNEL_UNLOCK_ONE(NULL);	/* XXXSMP */
				mutex_spin_enter(&kq->kq_lock);
				/* Re-poll if note was re-enqueued. */
				if ((kn->kn_status & KN_QUEUED) != 0)
					continue;
				if (rv == 0) {
					/*
					 * non-ONESHOT event that hasn't
					 * triggered again, so de-queue.
					 */
					kn->kn_status &= ~KN_ACTIVE;
					continue;
				}
			}
			/* XXXAD should be got from f_event if !oneshot. */
			*kevp++ = kn->kn_kevent;
			nkev++;
			if (kn->kn_flags & EV_ONESHOT) {
				/* delete ONESHOT events after retrieval */
				mutex_spin_exit(&kq->kq_lock);
				mutex_enter(&fdp->fd_lock);
				knote_detach(kn, fdp, true);
				mutex_spin_enter(&kq->kq_lock);
			} else if (kn->kn_flags & EV_CLEAR) {
				/* clear state after retrieval */
				kn->kn_data = 0;
				kn->kn_fflags = 0;
				kn->kn_status &= ~KN_ACTIVE;
			} else {
				/* add event back on list */
				kq_check(kq);
				TAILQ_INSERT_TAIL(&kq->kq_head, kn, kn_tqe);
				kq->kq_count++;
				kn->kn_status |= KN_QUEUED;
				kq_check(kq);
			}
			if (nkev == kevcnt) {
				/* do copyouts in kevcnt chunks */
				mutex_spin_exit(&kq->kq_lock);
				error = (*keops->keo_put_events)
				    (keops->keo_private,
				    kevbuf, ulistp, nevents, nkev);
				mutex_spin_enter(&kq->kq_lock);
				nevents += nkev;
				nkev = 0;
				kevp = kevbuf;
			}
			count--;
			if (error != 0 || count == 0) {
				/* remove marker */
				TAILQ_REMOVE(&kq->kq_head, marker, kn_tqe);
				break;
			}
		}
	}
 done:
 	mutex_spin_exit(&kq->kq_lock);
	if (marker != NULL)
		kmem_free(marker, sizeof(*marker));
	if (nkev != 0) {
		/* copyout remaining events */
		error = (*keops->keo_put_events)(keops->keo_private,
		    kevbuf, ulistp, nevents, nkev);
	}
	*retval = maxevents - count;

	return error;
}

/*
 * fileops ioctl method for a kqueue descriptor.
 *
 * Two ioctls are currently supported. They both use struct kfilter_mapping:
 *	KFILTER_BYNAME		find name for filter, and return result in
 *				name, which is of size len.
 *	KFILTER_BYFILTER	find filter for name. len is ignored.
 */
/*ARGSUSED*/
static int
kqueue_ioctl(file_t *fp, u_long com, void *data)
{
	struct kfilter_mapping	*km;
	const struct kfilter	*kfilter;
	char			*name;
	int			error;

	km = data;
	error = 0;
	name = kmem_alloc(KFILTER_MAXNAME, KM_SLEEP);

	switch (com) {
	case KFILTER_BYFILTER:	/* convert filter -> name */
		rw_enter(&kqueue_filter_lock, RW_READER);
		kfilter = kfilter_byfilter(km->filter);
		if (kfilter != NULL) {
			strlcpy(name, kfilter->name, KFILTER_MAXNAME);
			rw_exit(&kqueue_filter_lock);
			error = copyoutstr(name, km->name, km->len, NULL);
		} else {
			rw_exit(&kqueue_filter_lock);
			error = ENOENT;
		}
		break;

	case KFILTER_BYNAME:	/* convert name -> filter */
		error = copyinstr(km->name, name, KFILTER_MAXNAME, NULL);
		if (error) {
			break;
		}
		rw_enter(&kqueue_filter_lock, RW_READER);
		kfilter = kfilter_byname(name);
		if (kfilter != NULL)
			km->filter = kfilter->filter;
		else
			error = ENOENT;
		rw_exit(&kqueue_filter_lock);
		break;

	default:
		error = ENOTTY;
		break;

	}
	kmem_free(name, KFILTER_MAXNAME);
	return (error);
}

/*
 * fileops fcntl method for a kqueue descriptor.
 */
static int
kqueue_fcntl(file_t *fp, u_int com, void *data)
{

	return (ENOTTY);
}

/*
 * fileops poll method for a kqueue descriptor.
 * Determine if kqueue has events pending.
 */
static int
kqueue_poll(file_t *fp, int events)
{
	struct kqueue	*kq;
	int		revents;

	kq = fp->f_kqueue;

	revents = 0;
	if (events & (POLLIN | POLLRDNORM)) {
		mutex_spin_enter(&kq->kq_lock);
		if (kq->kq_count != 0) {
			revents |= events & (POLLIN | POLLRDNORM);
		} else {
			selrecord(curlwp, &kq->kq_sel);
		}
		kq_check(kq);
		mutex_spin_exit(&kq->kq_lock);
	}

	return revents;
}

/*
 * fileops stat method for a kqueue descriptor.
 * Returns dummy info, with st_size being number of events pending.
 */
static int
kqueue_stat(file_t *fp, struct stat *st)
{
	struct kqueue *kq;

	kq = fp->f_kqueue;

	memset(st, 0, sizeof(*st));
	st->st_size = kq->kq_count;
	st->st_blksize = sizeof(struct kevent);
	st->st_mode = S_IFIFO;

	return 0;
}

static void
kqueue_doclose(struct kqueue *kq, struct klist *list, int fd)
{
	struct knote *kn;
	filedesc_t *fdp;

	fdp = kq->kq_fdp;

	KASSERT(mutex_owned(&fdp->fd_lock));

	for (kn = SLIST_FIRST(list); kn != NULL;) {
		if (kq != kn->kn_kq) {
			kn = SLIST_NEXT(kn, kn_link);
			continue;
		}
		knote_detach(kn, fdp, true);
		mutex_enter(&fdp->fd_lock);
		kn = SLIST_FIRST(list);
	}
}


/*
 * fileops close method for a kqueue descriptor.
 */
static int
kqueue_close(file_t *fp)
{
	struct kqueue *kq;
	filedesc_t *fdp;
	fdfile_t *ff;
	int i;

	kq = fp->f_kqueue;
	fp->f_kqueue = NULL;
	fp->f_type = 0;
	fdp = curlwp->l_fd;

	mutex_enter(&fdp->fd_lock);
	for (i = 0; i <= fdp->fd_lastkqfile; i++) {
		if ((ff = fdp->fd_dt->dt_ff[i]) == NULL)
			continue;
		kqueue_doclose(kq, (struct klist *)&ff->ff_knlist, i);
	}
	if (fdp->fd_knhashmask != 0) {
		for (i = 0; i < fdp->fd_knhashmask + 1; i++) {
			kqueue_doclose(kq, &fdp->fd_knhash[i], -1);
		}
	}
	mutex_exit(&fdp->fd_lock);

	KASSERT(kq->kq_count == 0);
	mutex_destroy(&kq->kq_lock);
	cv_destroy(&kq->kq_cv);
	seldestroy(&kq->kq_sel);
	kmem_free(kq, sizeof(*kq));

	return (0);
}

/*
 * struct fileops kqfilter method for a kqueue descriptor.
 * Event triggered when monitored kqueue changes.
 */
static int
kqueue_kqfilter(file_t *fp, struct knote *kn)
{
	struct kqueue *kq;

	kq = ((file_t *)kn->kn_obj)->f_kqueue;

	KASSERT(fp == kn->kn_obj);

	if (kn->kn_filter != EVFILT_READ)
		return 1;

	kn->kn_fop = &kqread_filtops;
	mutex_enter(&kq->kq_lock);
	SLIST_INSERT_HEAD(&kq->kq_sel.sel_klist, kn, kn_selnext);
	mutex_exit(&kq->kq_lock);

	return 0;
}


/*
 * Walk down a list of knotes, activating them if their event has
 * triggered.  The caller's object lock (e.g. device driver lock)
 * must be held.
 */
void
knote(struct klist *list, long hint)
{
	struct knote *kn, *tmpkn;

	SLIST_FOREACH_SAFE(kn, list, kn_selnext, tmpkn) {
		if ((*kn->kn_fop->f_event)(kn, hint))
			knote_activate(kn);
	}
}

/*
 * Remove all knotes referencing a specified fd
 */
void
knote_fdclose(int fd)
{
	struct klist *list;
	struct knote *kn;
	filedesc_t *fdp;

	fdp = curlwp->l_fd;
	list = (struct klist *)&fdp->fd_dt->dt_ff[fd]->ff_knlist;
	mutex_enter(&fdp->fd_lock);
	while ((kn = SLIST_FIRST(list)) != NULL) {
		knote_detach(kn, fdp, true);
		mutex_enter(&fdp->fd_lock);
	}
	mutex_exit(&fdp->fd_lock);
}

/*
 * Drop knote.  Called with fdp->fd_lock held, and will drop before
 * returning.
 */
static void
knote_detach(struct knote *kn, filedesc_t *fdp, bool dofop)
{
	struct klist *list;
	struct kqueue *kq;

	kq = kn->kn_kq;

	KASSERT((kn->kn_status & KN_MARKER) == 0);
	KASSERT(mutex_owned(&fdp->fd_lock));

	/* Remove from monitored object. */
	if (dofop) {
		KERNEL_LOCK(1, NULL);		/* XXXSMP */
		(*kn->kn_fop->f_detach)(kn);
		KERNEL_UNLOCK_ONE(NULL);	/* XXXSMP */
	}

	/* Remove from descriptor table. */
	if (kn->kn_fop->f_isfd)
		list = (struct klist *)&fdp->fd_dt->dt_ff[kn->kn_id]->ff_knlist;
	else
		list = &fdp->fd_knhash[KN_HASH(kn->kn_id, fdp->fd_knhashmask)];

	SLIST_REMOVE(list, kn, knote, kn_link);

	/* Remove from kqueue. */
	/* XXXAD should verify not in use by kqueue_scan. */
	mutex_spin_enter(&kq->kq_lock);
	if ((kn->kn_status & KN_QUEUED) != 0) {
		kq_check(kq);
		TAILQ_REMOVE(&kq->kq_head, kn, kn_tqe);
		kn->kn_status &= ~KN_QUEUED;
		kq->kq_count--;
		kq_check(kq);
	}
	mutex_spin_exit(&kq->kq_lock);

	mutex_exit(&fdp->fd_lock);
	if (kn->kn_fop->f_isfd)		
		fd_putfile(kn->kn_id);
	atomic_dec_uint(&kn->kn_kfilter->refcnt);
	kmem_free(kn, sizeof(*kn));
}

/*
 * Queue new event for knote.
 */
static void
knote_enqueue(struct knote *kn)
{
	struct kqueue *kq;

	KASSERT((kn->kn_status & KN_MARKER) == 0);

	kq = kn->kn_kq;

	mutex_spin_enter(&kq->kq_lock);
	if ((kn->kn_status & KN_DISABLED) != 0) {
		kn->kn_status &= ~KN_DISABLED;
	}
	if ((kn->kn_status & (KN_ACTIVE | KN_QUEUED)) == KN_ACTIVE) {
		kq_check(kq);
		TAILQ_INSERT_TAIL(&kq->kq_head, kn, kn_tqe);
		kn->kn_status |= KN_QUEUED;
		kq->kq_count++;
		kq_check(kq);
		cv_broadcast(&kq->kq_cv);
		selnotify(&kq->kq_sel, 0, NOTE_SUBMIT);
	}
	mutex_spin_exit(&kq->kq_lock);
}
/*
 * Queue new event for knote.
 */
static void
knote_activate(struct knote *kn)
{
	struct kqueue *kq;

	KASSERT((kn->kn_status & KN_MARKER) == 0);

	kq = kn->kn_kq;

	mutex_spin_enter(&kq->kq_lock);
	kn->kn_status |= KN_ACTIVE;
	if ((kn->kn_status & (KN_QUEUED | KN_DISABLED)) == 0) {
		kq_check(kq);
		TAILQ_INSERT_TAIL(&kq->kq_head, kn, kn_tqe);
		kn->kn_status |= KN_QUEUED;
		kq->kq_count++;
		kq_check(kq);
		cv_broadcast(&kq->kq_cv);
		selnotify(&kq->kq_sel, 0, NOTE_SUBMIT);
	}
	mutex_spin_exit(&kq->kq_lock);
}

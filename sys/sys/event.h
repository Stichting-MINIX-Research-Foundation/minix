/*	$NetBSD: event.h,v 1.24 2015/01/14 22:21:00 christos Exp $	*/

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
 *	$FreeBSD: src/sys/sys/event.h,v 1.12 2001/02/24 01:44:03 jlemon Exp $
 */

#ifndef _SYS_EVENT_H_
#define	_SYS_EVENT_H_

#include <sys/featuretest.h>
#include <sys/types.h>			/* for size_t */
#include <sys/inttypes.h>		/* for uintptr_t */
#include <sys/null.h>			/* for NULL */

#define	EVFILT_READ		0U
#define	EVFILT_WRITE		1U
#define	EVFILT_AIO		2U	/* attached to aio requests */
#define	EVFILT_VNODE		3U	/* attached to vnodes */
#define	EVFILT_PROC		4U	/* attached to struct proc */
#define	EVFILT_SIGNAL		5U	/* attached to struct proc */
#define	EVFILT_TIMER		6U	/* arbitrary timer (in ms) */
#define	EVFILT_SYSCOUNT		7U	/* number of filters */

#define	EV_SET(kevp, a, b, c, d, e, f)					\
do {									\
	(kevp)->ident = (a);						\
	(kevp)->filter = (b);						\
	(kevp)->flags = (c);						\
	(kevp)->fflags = (d);						\
	(kevp)->data = (e);						\
	(kevp)->udata = (f);						\
} while (/* CONSTCOND */ 0)


struct kevent {
	uintptr_t	ident;		/* identifier for this event */
	uint32_t	filter;		/* filter for event */
	uint32_t	flags;		/* action flags for kqueue */
	uint32_t	fflags;		/* filter flag value */
	int64_t		data;		/* filter data value */
	intptr_t	udata;		/* opaque user data identifier */
};

/* actions */
#define	EV_ADD		0x0001U		/* add event to kq (implies ENABLE) */
#define	EV_DELETE	0x0002U		/* delete event from kq */
#define	EV_ENABLE	0x0004U		/* enable event */
#define	EV_DISABLE	0x0008U		/* disable event (not reported) */

/* flags */
#define	EV_ONESHOT	0x0010U		/* only report one occurrence */
#define	EV_CLEAR	0x0020U		/* clear event state after reporting */

#define	EV_SYSFLAGS	0xF000U		/* reserved by system */
#define	EV_FLAG1	0x2000U		/* filter-specific flag */

/* returned values */
#define	EV_EOF		0x8000U		/* EOF detected */
#define	EV_ERROR	0x4000U		/* error, data contains errno */

/*
 * hint flag for in-kernel use - must not equal any existing note
 */
#ifdef _KERNEL
#define NOTE_SUBMIT	0x01000000U		/* initial knote submission */
#endif
/*
 * data/hint flags for EVFILT_{READ|WRITE}, shared with userspace
 */
#define	NOTE_LOWAT	0x0001U			/* low water mark */

/*
 * data/hint flags for EVFILT_VNODE, shared with userspace
 */
#define	NOTE_DELETE	0x0001U			/* vnode was removed */
#define	NOTE_WRITE	0x0002U			/* data contents changed */
#define	NOTE_EXTEND	0x0004U			/* size increased */
#define	NOTE_ATTRIB	0x0008U			/* attributes changed */
#define	NOTE_LINK	0x0010U			/* link count changed */
#define	NOTE_RENAME	0x0020U			/* vnode was renamed */
#define	NOTE_REVOKE	0x0040U			/* vnode access was revoked */

/*
 * data/hint flags for EVFILT_PROC, shared with userspace
 */
#define	NOTE_EXIT	0x80000000U		/* process exited */
#define	NOTE_FORK	0x40000000U		/* process forked */
#define	NOTE_EXEC	0x20000000U		/* process exec'd */
#define	NOTE_PCTRLMASK	0xf0000000U		/* mask for hint bits */
#define	NOTE_PDATAMASK	0x000fffffU		/* mask for pid */

/* additional flags for EVFILT_PROC */
#define	NOTE_TRACK	0x00000001U		/* follow across forks */
#define	NOTE_TRACKERR	0x00000002U		/* could not track child */
#define	NOTE_CHILD	0x00000004U		/* am a child process */

/*
 * This is currently visible to userland to work around broken
 * programs which pull in <sys/proc.h> or <sys/select.h>.
 */
#include <sys/queue.h>
struct knote;
SLIST_HEAD(klist, knote);


/*
 * ioctl(2)s supported on kqueue descriptors.
 */
#include <sys/ioctl.h>

struct kfilter_mapping {
	char		*name;		/* name to lookup or return */
	size_t		len;		/* length of name */
	uint32_t	filter;		/* filter to lookup or return */
};

/* map filter to name (max size len) */
#define KFILTER_BYFILTER	_IOWR('k', 0, struct kfilter_mapping)
/* map name to filter (len ignored) */
#define KFILTER_BYNAME		_IOWR('k', 1, struct kfilter_mapping)

#ifdef _KERNEL

#define	KNOTE(list, hint)	if (!SLIST_EMPTY(list)) knote(list, hint)

/*
 * Flag indicating hint is a signal.  Used by EVFILT_SIGNAL, and also
 * shared by EVFILT_PROC  (all knotes attached to p->p_klist)
 */
#define	NOTE_SIGNAL	0x08000000U

/*
 * Callback methods for each filter type.
 */
struct filterops {
	int	f_isfd;			/* true if ident == filedescriptor */
	int	(*f_attach)	(struct knote *);
					/* called when knote is ADDed */
	void	(*f_detach)	(struct knote *);
					/* called when knote is DELETEd */
	int	(*f_event)	(struct knote *, long);
					/* called when event is triggered */
};

/*
 * Field locking:
 *
 * f	kn_kq->kq_fdp->fd_lock
 * q	kn_kq->kq_lock
 * o	object mutex (e.g. device driver or vnode interlock)
 */
struct kfilter;

struct knote {
	SLIST_ENTRY(knote)	kn_link;	/* f: for fd */
	SLIST_ENTRY(knote)	kn_selnext;	/* o: for struct selinfo */
	TAILQ_ENTRY(knote)	kn_tqe;		/* q: for struct kqueue */
	struct kqueue		*kn_kq;		/* q: which queue we are on */
	struct kevent		kn_kevent;
	uint32_t		kn_status;
	uint32_t		kn_sfflags;	/*   saved filter flags */
	uintptr_t		kn_sdata;	/*   saved data field */
	void			*kn_obj;	/*   pointer to monitored obj */
	const struct filterops	*kn_fop;
	struct kfilter		*kn_kfilter;
	void 			*kn_hook;

#define	KN_ACTIVE	0x01U			/* event has been triggered */
#define	KN_QUEUED	0x02U			/* event is on queue */
#define	KN_DISABLED	0x04U			/* event is disabled */
#define	KN_DETACHED	0x08U			/* knote is detached */
#define	KN_MARKER	0x10U			/* is a marker */

#define	kn_id		kn_kevent.ident
#define	kn_filter	kn_kevent.filter
#define	kn_flags	kn_kevent.flags
#define	kn_fflags	kn_kevent.fflags
#define	kn_data		kn_kevent.data
};

#include <sys/systm.h> /* for copyin_t */

struct lwp;
struct timespec;

void	kqueue_init(void);
void	knote(struct klist *, long);
void	knote_fdclose(int);

typedef	int (*kevent_fetch_changes_t)(void *, const struct kevent *,
    struct kevent *, size_t, int);
typedef	int (*kevent_put_events_t)(void *, struct kevent *, struct kevent *,
    size_t, int);

struct kevent_ops {
	void *keo_private;
	copyin_t keo_fetch_timeout;
	kevent_fetch_changes_t keo_fetch_changes;
	kevent_put_events_t keo_put_events;
};


int	kevent_fetch_changes(void *, const struct kevent *, struct kevent *,
    size_t, int);
int 	kevent_put_events(void *, struct kevent *, struct kevent *, size_t,
    int);
int	kevent1(register_t *, int, const struct kevent *,
    size_t, struct kevent *, size_t, const struct timespec *,
    const struct kevent_ops *);

int	kfilter_register(const char *, const struct filterops *, int *);
int	kfilter_unregister(const char *);

int	filt_seltrue(struct knote *, long);
extern const struct filterops seltrue_filtops;

#else 	/* !_KERNEL */

#include <sys/cdefs.h>
struct timespec;

__BEGIN_DECLS
#if defined(_NETBSD_SOURCE)
int	kqueue(void);
int	kqueue1(int);
#ifndef __LIBC12_SOURCE__
int	kevent(int, const struct kevent *, size_t, struct kevent *, size_t,
		    const struct timespec *) __RENAME(__kevent50);
#endif
#endif /* !_POSIX_C_SOURCE */
__END_DECLS

#endif /* !_KERNEL */

#endif /* !_SYS_EVENT_H_ */

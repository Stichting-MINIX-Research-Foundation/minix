/*	$NetBSD: kern_select_50.c,v 1.1 2011/01/17 15:57:04 pooka Exp $	*/

/*-
 * Copyright (c) 2008, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_select_50.c,v 1.1 2011/01/17 15:57:04 pooka Exp $");

#include <sys/param.h>
#include <sys/event.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/syscallargs.h>

#include <compat/sys/time.h>

static int
compat_50_kevent_fetch_timeout(const void *src, void *dest, size_t length)
{
	struct timespec50 ts50;
	int error;

	KASSERT(length == sizeof(struct timespec));

	error = copyin(src, &ts50, sizeof(ts50));
	if (error)
		return error;
	timespec50_to_timespec(&ts50, (struct timespec *)dest);
	return 0;
}

int
compat_50_sys_kevent(struct lwp *l, const struct compat_50_sys_kevent_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(keventp_t) changelist;
		syscallarg(size_t) nchanges;
		syscallarg(keventp_t) eventlist;
		syscallarg(size_t) nevents;
		syscallarg(struct timespec50) timeout;
	} */
	static const struct kevent_ops compat_50_kevent_ops = {
		.keo_private = NULL,
		.keo_fetch_timeout = compat_50_kevent_fetch_timeout,
		.keo_fetch_changes = kevent_fetch_changes,
		.keo_put_events = kevent_put_events,
	};

	return kevent1(retval, SCARG(uap, fd), SCARG(uap, changelist),
	    SCARG(uap, nchanges), SCARG(uap, eventlist), SCARG(uap, nevents),
	    (const struct timespec *)(const void *)SCARG(uap, timeout),
	    &compat_50_kevent_ops);
}

int
compat_50_sys_select(struct lwp *l,
    const struct compat_50_sys_select_args *uap, register_t *retval)
{
	/* {
		syscallarg(int)			nd;
		syscallarg(fd_set *)		in;
		syscallarg(fd_set *)		ou;
		syscallarg(fd_set *)		ex;
		syscallarg(struct timeval50 *)	tv;
	} */
	struct timespec ats, *ts = NULL;
	struct timeval50 atv50;
	int error;

	if (SCARG(uap, tv)) {
		error = copyin(SCARG(uap, tv), (void *)&atv50, sizeof(atv50));
		if (error)
			return error;
		ats.tv_sec = atv50.tv_sec;
		ats.tv_nsec = atv50.tv_usec * 1000;
		ts = &ats;
	}

	return selcommon(retval, SCARG(uap, nd), SCARG(uap, in),
	    SCARG(uap, ou), SCARG(uap, ex), ts, NULL);
}

int
compat_50_sys_pselect(struct lwp *l,
    const struct compat_50_sys_pselect_args *uap, register_t *retval)
{
	/* {
		syscallarg(int)				nd;
		syscallarg(fd_set *)			in;
		syscallarg(fd_set *)			ou;
		syscallarg(fd_set *)			ex;
		syscallarg(const struct timespec50 *)	ts;
		syscallarg(sigset_t *)			mask;
	} */
	struct timespec50	ats50;
	struct timespec	ats, *ts = NULL;
	sigset_t	amask, *mask = NULL;
	int		error;

	if (SCARG(uap, ts)) {
		error = copyin(SCARG(uap, ts), &ats50, sizeof(ats50));
		if (error)
			return error;
		timespec50_to_timespec(&ats50, &ats);
		ts = &ats;
	}
	if (SCARG(uap, mask) != NULL) {
		error = copyin(SCARG(uap, mask), &amask, sizeof(amask));
		if (error)
			return error;
		mask = &amask;
	}

	return selcommon(retval, SCARG(uap, nd), SCARG(uap, in),
	    SCARG(uap, ou), SCARG(uap, ex), ts, mask);
}

int
compat_50_sys_pollts(struct lwp *l, const struct compat_50_sys_pollts_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(struct pollfd *)		fds;
		syscallarg(u_int)			nfds;
		syscallarg(const struct timespec50 *)	ts;
		syscallarg(const sigset_t *)		mask;
	} */
	struct timespec	ats, *ts = NULL;
	struct timespec50 ats50;
	sigset_t	amask, *mask = NULL;
	int		error;

	if (SCARG(uap, ts)) {
		error = copyin(SCARG(uap, ts), &ats50, sizeof(ats50));
		if (error)
			return error;
		timespec50_to_timespec(&ats50, &ats);
		ts = &ats;
	}
	if (SCARG(uap, mask)) {
		error = copyin(SCARG(uap, mask), &amask, sizeof(amask));
		if (error)
			return error;
		mask = &amask;
	}

	return pollcommon(retval, SCARG(uap, fds), SCARG(uap, nfds), ts, mask);
}

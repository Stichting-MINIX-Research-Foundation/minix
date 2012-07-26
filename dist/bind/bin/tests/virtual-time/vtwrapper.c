/*
 * Copyright (C) 2010  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: vtwrapper.c,v 1.4 2010-08-12 09:31:50 fdupont Exp $ */

#define _GNU_SOURCE
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef SYS_select
#include <sys/select.h>
#endif
#ifdef SYS_poll
#include <poll.h>
#endif
#ifdef SYS_kevent
#include <sys/event.h>
#endif
#ifdef SYS_epoll_wait
#include <sys/epoll.h>
#endif


#ifdef SYS_gettimeofday
#define VIRTUAL_TIME
#ifdef VIRTUAL_TIME
static struct timeval epoch = { 0, 0 };
static int _init_called = 0;

void
_init(void) {
	(void)syscall(SYS_gettimeofday, &epoch, NULL);
	_init_called = 1;
}

static void
absolute_inflate(struct timeval *vt, struct timeval *rt)
{
	double d;

	rt->tv_sec = vt->tv_sec;
	rt->tv_usec = vt->tv_usec;

	if ((epoch.tv_sec > vt->tv_sec) ||
	    ((epoch.tv_sec == vt->tv_sec) && (epoch.tv_usec > vt->tv_usec)))
		return;

	rt->tv_sec -= epoch.tv_sec;
	rt->tv_usec -= epoch.tv_usec;
	while (rt->tv_usec < 0) {
		rt->tv_sec -= 1;
		rt->tv_usec += 1000000;
	}

	if (rt->tv_sec == 0)
		goto done;

	d = (double) (rt->tv_sec - 1);
	d += (double) rt->tv_usec / 1000000.;
	d = exp(d);
	rt->tv_sec = (time_t) d;
	d -= (double) rt->tv_sec;
	rt->tv_usec = (suseconds_t) (d * 1000000.);

 done:
	rt->tv_sec += epoch.tv_sec;
	rt->tv_usec += epoch.tv_usec;
	while (rt->tv_usec >= 1000000) {
		rt->tv_sec += 1;
		rt->tv_usec -= 1000000;
	}
	return;
}

static void
absolute_deflate(struct timeval *rt, struct timeval *vt) {
	double d;

	vt->tv_sec = rt->tv_sec;
	vt->tv_usec = rt->tv_usec;

	if ((epoch.tv_sec > rt->tv_sec) ||
	    ((epoch.tv_sec == rt->tv_sec) && (epoch.tv_usec > rt->tv_usec)))
		return;

	vt->tv_sec -= epoch.tv_sec;
	vt->tv_usec -= epoch.tv_usec;
	while (vt->tv_usec < 0) {
		vt->tv_sec -= 1;
		vt->tv_usec += 1000000;
	}

	if (vt->tv_sec == 0)
		goto done;

	d = (double) vt->tv_sec;
	d += (double) vt->tv_usec / 1000000.;
	d = log(d);
	vt->tv_sec = (time_t) d;
	d -= (double) vt->tv_sec;
	vt->tv_sec += 1;
	vt->tv_usec = (suseconds_t) (d * 1000000.);

 done:
	vt->tv_sec += epoch.tv_sec;
	vt->tv_usec += epoch.tv_usec;
	while (vt->tv_usec >= 1000000) {
		vt->tv_sec += 1;
		vt->tv_usec -= 1000000;
	}
	return;
}

static void
interval_inflate(struct timeval *vt, struct timeval *rt) {
	struct timeval now, tv;

	(void) gettimeofday(&now, NULL);

	absolute_deflate(&now, &tv);

	tv.tv_sec += vt->tv_sec;
	tv.tv_usec += vt->tv_usec;
	while (tv.tv_usec >= 1000000) {
		tv.tv_sec += 1;
		tv.tv_usec -= 1000000;
	}

	absolute_inflate(&tv, rt);

	rt->tv_sec -= now.tv_sec;
	rt->tv_usec -= now.tv_usec;
	if (rt->tv_usec < 0) {
		rt->tv_sec -= 1;
		rt->tv_usec += 1000000;
	}
	return;
}

static void
interval_deflate(struct timeval *rt, struct timeval *vt) {
	struct timeval now, tv;

	vt->tv_sec = rt->tv_sec;
	vt->tv_usec = rt->tv_usec;

	if ((vt->tv_sec == 0) && (vt->tv_usec <= 10000))
		return;

	(void) gettimeofday(&now, NULL);

	tv.tv_sec = now.tv_sec + rt->tv_sec;
	tv.tv_usec = now.tv_usec + rt->tv_usec;
	while (tv.tv_usec >= 1000000) {
		tv.tv_sec += 1;
		tv.tv_usec -= 1000000;
	}

	absolute_deflate(&now, &now);
	absolute_deflate(&tv, vt);

	vt->tv_sec -= now.tv_sec;
	vt->tv_usec -= now.tv_usec;
	while (vt->tv_usec < 0) {
		vt->tv_sec -= 1;
		vt->tv_usec += 1000000;
	}

	if ((vt->tv_sec == 0) && (vt->tv_usec < 10000))
		vt->tv_usec = 10000;
	return;
}
#endif

int
gettimeofday(struct timeval *tv, struct timezone *tz) {
#ifdef VIRTUAL_TIME
	struct timeval now;
	int ret;

	if (!_init_called) _init();

	if (epoch.tv_sec == 0)
		return syscall(SYS_gettimeofday, tv, tz);

	ret = syscall(SYS_gettimeofday, &now, tz);
	if (ret == 0)
		absolute_inflate(&now, tv);
	return ret;
#else
	return syscall(SYS_gettimeofday, tv, tz);
#endif
}

#ifdef SYS_select
int
select(int nfds, fd_set *rfds, fd_set *wfds, fd_set *xfds,
       struct timeval *timeout)
{
#ifdef VIRTUAL_TIME
	struct timeval tv;

	if (!_init_called) _init();

	if (epoch.tv_sec == 0 || timeout == NULL ||
	    (timeout->tv_sec == 0 && timeout->tv_usec == 0))
		return syscall(SYS_select, nfds, rfds, wfds, xfds, timeout);

	interval_deflate(timeout, &tv);
	return syscall(SYS_select, nfds, rfds, wfds, xfds, &tv);
#else
	return syscall(SYS_select, nfds, rfds, wfds, xfds, timeout);
#endif
}
#endif

#ifdef SYS_poll
int
poll(struct pollfd fds[], nfds_t nfds, int timeout) {
#ifdef VIRTUAL_TIME
	struct timeval in, out;

	if (!_init_called) _init();

	if (timeout <= 0 || epoch.tv_sec == 0)
		return syscall(SYS_poll, fds, nfds, timeout);

	in.tv_sec = timeout / 1000;
	in.tv_usec = (timeout % 1000) * 1000;
	interval_deflate(&in, &out);
	timeout = out.tv_sec * 1000 + out.tv_usec / 1000;
	return syscall(SYS_poll, fds, nfds, timeout);
#else
	return syscall(SYS_poll, fds, nfds, timeout);
#endif
}
#endif

#ifdef SYS_kevent
int
kevent(int kq, struct kevent *changelist, int nchanges,
       struct kevent *eventlist, int nevents, const struct timespec *timeout)
{
#ifdef VIRTUAL_TIME
	struct timeval in, out;
	struct timespec ts;

	if (!_init_called) _init();

	if (epoch.tv_sec == 0 || timeout == NULL ||
	    (timeout->tv_sec == 0 && timeout->tv_nsec == 0))
		return syscall(SYS_kevent, kq, changelist, nchanges,
			       eventlist, nevents, timeout);

	in.tv_sec = timeout->tv_sec;
	in.tv_usec = timeout->tv_nsec / 1000;
	interval_deflate(&in, &out);
	ts.tv_sec = out.tv_sec;
	ts.tv_nsec = out.tv_usec * 1000;
	return syscall(SYS_kevent, kq, changelist, nchanges, eventlist,
		       nevents, &ts);
#else
	return syscall(SYS_kevent, kq, changelist, nchanges, eventlist,
		       nevents, timeout);
#endif
}
#endif

#ifdef SYS_epoll_wait
int
epoll_wait(int fd, struct epoll_event *events, int maxevents, int timeout) {
#ifdef VIRTUAL_TIME
	struct timeval in, out;

	if (!_init_called) _init();

	if (timeout == 0 || timeout == -1 || epoch.tv_sec == 0)
		return syscall(SYS_epoll_wait, fd, events, maxevents, timeout);

	in.tv_sec = timeout / 1000;
	in.tv_usec = (timeout % 1000) * 1000;
	interval_deflate(&in, &out);
	timeout = out.tv_sec * 1000 + out.tv_usec / 1000;
	return syscall(SYS_poll, fd, events, maxevents, timeout);
#else
	return syscall(SYS_poll, fd, events, maxevents, timeout);
#endif
}
#endif
#endif

#include <sys/cdefs.h>
 __RCSID("$NetBSD: eloop.c,v 1.11 2015/05/16 23:31:32 roy Exp $");

/*
 * dhcpcd - DHCP client daemon
 * Copyright (c) 2006-2015 Roy Marples <roy@marples.name>
 * All rights reserved

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
 */

#include <sys/time.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* config.h should define HAVE_KQUEUE, HAVE_EPOLL, etc */
#include "config.h"
#include "eloop.h"

#ifndef UNUSED
#define UNUSED(a) (void)((a))
#endif
#ifndef __unused
#ifdef __GNUC__
#define __unused   __attribute__((__unused__))
#else
#define __unused
#endif
#endif

#ifndef MSEC_PER_SEC
#define MSEC_PER_SEC	1000L
#define NSEC_PER_MSEC	1000000L
#endif

#if defined(HAVE_KQUEUE)
#include <sys/event.h>
#include <fcntl.h>
#ifdef __NetBSD__
/* udata is void * except on NetBSD
 * lengths are int except on NetBSD */
#define UPTR(x)	((intptr_t)(x))
#define LENC(x)	(x)
#else
#define UPTR(x)	(x)
#define LENC(x)	((int)(x))
#endif
#define eloop_event_setup_fds(eloop)
#elif defined(HAVE_EPOLL)
#include <sys/epoll.h>
#define eloop_event_setup_fds(eloop)
#else
#include <poll.h>
static void
eloop_event_setup_fds(struct eloop *eloop)
{
	struct eloop_event *e;
	size_t i;

	i = 0;
	TAILQ_FOREACH(e, &eloop->events, next) {
		eloop->fds[i].fd = e->fd;
		eloop->fds[i].events = 0;
		if (e->read_cb)
			eloop->fds[i].events |= POLLIN;
		if (e->write_cb)
			eloop->fds[i].events |= POLLOUT;
		eloop->fds[i].revents = 0;
		e->pollfd = &eloop->fds[i];
		i++;
	}
}

#ifndef pollts
/* Wrapper around pselect, to imitate the NetBSD pollts call. */
#if !defined(__minix)
static int
#else /* defined(__minix) */
int
#endif /* defined(__minix) */
pollts(struct pollfd * fds, nfds_t nfds,
    const struct timespec *ts, const sigset_t *sigmask)
{
	fd_set read_fds;
	nfds_t n;
	int maxfd, r;
#if defined(__minix)
	sigset_t omask;
	struct timeval tv, *tvp;
#endif /* defined(__minix) */

	FD_ZERO(&read_fds);
	maxfd = 0;
	for (n = 0; n < nfds; n++) {
		if (fds[n].events & POLLIN) {
			FD_SET(fds[n].fd, &read_fds);
			if (fds[n].fd > maxfd)
				maxfd = fds[n].fd;
		}
	}

#if !defined(__minix)
	r = pselect(maxfd + 1, &read_fds, NULL, NULL, ts, sigmask);
#else /* defined(__minix) */
	/* XXX FIXME - horrible hack with race condition */
	sigprocmask(SIG_SETMASK, sigmask, &omask);
	if (ts != NULL) {
		tv.tv_sec = ts->tv_sec;
		tv.tv_usec = ts->tv_nsec / 1000;
		tvp = &tv;
	} else
		tvp = NULL;
	r = select(maxfd + 1, &read_fds, NULL, NULL, tvp);
	sigprocmask(SIG_SETMASK, &omask, NULL);
#endif /* defined(__minix) */
	if (r > 0) {
		for (n = 0; n < nfds; n++) {
			fds[n].revents =
			    FD_ISSET(fds[n].fd, &read_fds) ? POLLIN : 0;
		}
	}

	return r;
}
#endif
#endif

int
eloop_event_add(struct eloop *eloop, int fd,
    void (*read_cb)(void *), void *read_cb_arg,
    void (*write_cb)(void *), void *write_cb_arg)
{
	struct eloop_event *e;
#if defined(HAVE_KQUEUE)
	struct kevent ke[2];
#elif defined(HAVE_EPOLL)
	struct epoll_event epe;
#else
	struct pollfd *nfds;
#endif

	assert(eloop != NULL);
	assert(read_cb != NULL || write_cb != NULL);
	if (fd == -1) {
		errno = EINVAL;
		return -1;
	}

#ifdef HAVE_EPOLL
	memset(&epe, 0, sizeof(epe));
	epe.data.fd = fd;
	epe.events = EPOLLIN;
	if (write_cb)
		epe.events |= EPOLLOUT;
#endif

	/* We should only have one callback monitoring the fd */
	TAILQ_FOREACH(e, &eloop->events, next) {
		if (e->fd == fd) {
			int error;

#if defined(HAVE_KQUEUE)
			EV_SET(&ke[0], (uintptr_t)fd, EVFILT_READ, EV_ADD,
			    0, 0, UPTR(e));
			if (write_cb)
				EV_SET(&ke[1], (uintptr_t)fd, EVFILT_WRITE,
				    EV_ADD, 0, 0, UPTR(e));
			else if (e->write_cb)
				EV_SET(&ke[1], (uintptr_t)fd, EVFILT_WRITE,
				    EV_DELETE, 0, 0, UPTR(e));
			error = kevent(eloop->poll_fd, ke,
			    e->write_cb || write_cb ? 2 : 1, NULL, 0, NULL);
#elif defined(HAVE_EPOLL)
			epe.data.ptr = e;
			error = epoll_ctl(eloop->poll_fd, EPOLL_CTL_MOD,
			    fd, &epe);
#else
			error = 0;
#endif
			if (read_cb) {
				e->read_cb = read_cb;
				e->read_cb_arg = read_cb_arg;
			}
			if (write_cb) {
				e->write_cb = write_cb;
				e->write_cb_arg = write_cb_arg;
			}
			eloop_event_setup_fds(eloop);
			return error;
		}
	}

	/* Allocate a new event if no free ones already allocated */
	if ((e = TAILQ_FIRST(&eloop->free_events))) {
		TAILQ_REMOVE(&eloop->free_events, e, next);
	} else {
		e = malloc(sizeof(*e));
		if (e == NULL)
			goto err;
	}

	/* Ensure we can actually listen to it */
	eloop->events_len++;
#if !defined(HAVE_KQUEUE) && !defined(HAVE_EPOLL)
	if (eloop->events_len > eloop->fds_len) {
		nfds = realloc(eloop->fds,
		    sizeof(*eloop->fds) * (eloop->fds_len + 5));
		if (nfds == NULL)
			goto err;
		eloop->fds_len += 5;
		eloop->fds = nfds;
	}
#endif

	/* Now populate the structure and add it to the list */
	e->fd = fd;
	e->read_cb = read_cb;
	e->read_cb_arg = read_cb_arg;
	e->write_cb = write_cb;
	e->write_cb_arg = write_cb_arg;

#if defined(HAVE_KQUEUE)
	if (read_cb != NULL)
		EV_SET(&ke[0], (uintptr_t)fd, EVFILT_READ,
		    EV_ADD, 0, 0, UPTR(e));
	if (write_cb != NULL)
		EV_SET(&ke[1], (uintptr_t)fd, EVFILT_WRITE,
		    EV_ADD, 0, 0, UPTR(e));
	if (kevent(eloop->poll_fd, ke, write_cb ? 2 : 1, NULL, 0, NULL) == -1)
		goto err;
#elif defined(HAVE_EPOLL)
	epe.data.ptr = e;
	if (epoll_ctl(eloop->poll_fd, EPOLL_CTL_ADD, fd, &epe) == -1)
		goto err;
#endif

	/* The order of events should not matter.
	 * However, some PPP servers love to close the link right after
	 * sending their final message. So to ensure dhcpcd processes this
	 * message (which is likely to be that the DHCP addresses are wrong)
	 * we insert new events at the queue head as the link fd will be
	 * the first event added. */
	TAILQ_INSERT_HEAD(&eloop->events, e, next);
	eloop_event_setup_fds(eloop);
	return 0;

err:
	if (e) {
		eloop->events_len--;
		TAILQ_INSERT_TAIL(&eloop->free_events, e, next);
	}
	return -1;
}

void
eloop_event_delete_write(struct eloop *eloop, int fd, int write_only)
{
	struct eloop_event *e;
#if defined(HAVE_KQUEUE)
	struct kevent ke[2];
#elif defined(HAVE_EPOLL)
	struct epoll_event epe;
#endif

	assert(eloop != NULL);

	TAILQ_FOREACH(e, &eloop->events, next) {
		if (e->fd == fd) {
			if (write_only && e->read_cb != NULL) {
				if (e->write_cb != NULL) {
					e->write_cb = NULL;
					e->write_cb_arg = NULL;
#if defined(HAVE_KQUEUE)
					EV_SET(&ke[0], (uintptr_t)fd,
					    EVFILT_WRITE, EV_DELETE,
					    0, 0, UPTR(NULL));
					kevent(eloop->poll_fd, ke, 1, NULL, 0,
					    NULL);
#elif defined(HAVE_EPOLL)
					memset(&epe, 0, sizeof(epe));
					epe.data.fd = e->fd;
					epe.data.ptr = e;
					epe.events = EPOLLIN;
					epoll_ctl(eloop->poll_fd,
					    EPOLL_CTL_MOD, fd, &epe);
#endif
				}
			} else {
				TAILQ_REMOVE(&eloop->events, e, next);
#if defined(HAVE_KQUEUE)
				EV_SET(&ke[0], (uintptr_t)fd, EVFILT_READ,
				    EV_DELETE, 0, 0, UPTR(NULL));
				if (e->write_cb)
					EV_SET(&ke[1], (uintptr_t)fd,
					    EVFILT_WRITE, EV_DELETE,
					    0, 0, UPTR(NULL));
				kevent(eloop->poll_fd, ke, e->write_cb ? 2 : 1,
				    NULL, 0, NULL);
#elif defined(HAVE_EPOLL)
				/* NULL event is safe because we
				 * rely on epoll_pwait which as added
				 * after the delete without event was fixed. */
				epoll_ctl(eloop->poll_fd, EPOLL_CTL_DEL,
				    fd, NULL);
#endif
				TAILQ_INSERT_TAIL(&eloop->free_events, e, next);
				eloop->events_len--;
			}
			eloop_event_setup_fds(eloop);
			break;
		}
	}
}

int
eloop_q_timeout_add_tv(struct eloop *eloop, int queue,
    const struct timespec *when, void (*callback)(void *), void *arg)
{
	struct timespec now, w;
	struct eloop_timeout *t, *tt = NULL;

	assert(eloop != NULL);
	assert(when != NULL);
	assert(callback != NULL);

	clock_gettime(CLOCK_MONOTONIC, &now);
	timespecadd(&now, when, &w);
	/* Check for time_t overflow. */
	if (timespeccmp(&w, &now, <)) {
		errno = ERANGE;
		return -1;
	}

	/* Remove existing timeout if present */
	TAILQ_FOREACH(t, &eloop->timeouts, next) {
		if (t->callback == callback && t->arg == arg) {
			TAILQ_REMOVE(&eloop->timeouts, t, next);
			break;
		}
	}

	if (t == NULL) {
		/* No existing, so allocate or grab one from the free pool */
		if ((t = TAILQ_FIRST(&eloop->free_timeouts))) {
			TAILQ_REMOVE(&eloop->free_timeouts, t, next);
		} else {
			if ((t = malloc(sizeof(*t))) == NULL)
				return -1;
		}
	}

	t->when = w;
	t->callback = callback;
	t->arg = arg;
	t->queue = queue;

	/* The timeout list should be in chronological order,
	 * soonest first. */
	TAILQ_FOREACH(tt, &eloop->timeouts, next) {
		if (timespeccmp(&t->when, &tt->when, <)) {
			TAILQ_INSERT_BEFORE(tt, t, next);
			return 0;
		}
	}
	TAILQ_INSERT_TAIL(&eloop->timeouts, t, next);
	return 0;
}

int
eloop_q_timeout_add_sec(struct eloop *eloop, int queue, time_t when,
    void (*callback)(void *), void *arg)
{
	struct timespec tv;

	tv.tv_sec = when;
	tv.tv_nsec = 0;
	return eloop_q_timeout_add_tv(eloop, queue, &tv, callback, arg);
}

int
eloop_q_timeout_add_msec(struct eloop *eloop, int queue, long when,
    void (*callback)(void *), void *arg)
{
	struct timespec tv;

	tv.tv_sec = when / MSEC_PER_SEC;
	tv.tv_nsec = (when % MSEC_PER_SEC) * NSEC_PER_MSEC;
	return eloop_q_timeout_add_tv(eloop, queue, &tv, callback, arg);
}

#if !defined(HAVE_KQUEUE)
static int
eloop_timeout_add_now(struct eloop *eloop,
    void (*callback)(void *), void *arg)
{

	assert(eloop->timeout0 == NULL);
	eloop->timeout0 = callback;
	eloop->timeout0_arg = arg;
	return 0;
}
#endif

void
eloop_q_timeout_delete(struct eloop *eloop, int queue,
    void (*callback)(void *), void *arg)
{
	struct eloop_timeout *t, *tt;

	assert(eloop != NULL);

	TAILQ_FOREACH_SAFE(t, &eloop->timeouts, next, tt) {
		if ((queue == 0 || t->queue == queue) &&
		    t->arg == arg &&
		    (!callback || t->callback == callback))
		{
			TAILQ_REMOVE(&eloop->timeouts, t, next);
			TAILQ_INSERT_TAIL(&eloop->free_timeouts, t, next);
		}
	}
}

void
eloop_exit(struct eloop *eloop, int code)
{

	assert(eloop != NULL);

	eloop->exitcode = code;
	eloop->exitnow = 1;
}

#if defined(HAVE_KQUEUE) || defined(HAVE_EPOLL)
static int
eloop_open(struct eloop *eloop)
{

#if defined(HAVE_KQUEUE1)
	return (eloop->poll_fd = kqueue1(O_CLOEXEC));
#elif defined(HAVE_KQUEUE)
	int i;

	if ((eloop->poll_fd = kqueue()) == -1)
		return -1;
	if ((i = fcntl(eloop->poll_fd, F_GETFD, 0)) == -1 ||
	    fcntl(eloop->poll_fd, F_SETFD, i | FD_CLOEXEC) == -1)
	{
		close(eloop->poll_fd);
		eloop->poll_fd = -1;
		return -1;
	}

	return eloop->poll_fd;
#elif defined (HAVE_EPOLL)
	return (eloop->poll_fd = epoll_create1(EPOLL_CLOEXEC));
#endif
}

int
eloop_requeue(struct eloop *eloop)
{
	struct eloop_event *e;
	int error;
#if defined(HAVE_KQUEUE)
	size_t i;
	struct kevent *ke;
#elif defined(HAVE_EPOLL)
	struct epoll_event epe;
#endif

	assert(eloop != NULL);

	if (eloop->poll_fd != -1)
		close(eloop->poll_fd);
	if (eloop_open(eloop) == -1)
		return -1;
#if defined (HAVE_KQUEUE)
	i = eloop->signals_len;
	TAILQ_FOREACH(e, &eloop->events, next) {
		i++;
		if (e->write_cb)
			i++;
	}

	if ((ke = malloc(sizeof(*ke) * i)) == NULL)
		return -1;

	for (i = 0; i < eloop->signals_len; i++)
		EV_SET(&ke[i], (uintptr_t)eloop->signals[i],
		    EVFILT_SIGNAL, EV_ADD, 0, 0, UPTR(NULL));

	TAILQ_FOREACH(e, &eloop->events, next) {
		EV_SET(&ke[i], (uintptr_t)e->fd, EVFILT_READ,
		    EV_ADD, 0, 0, UPTR(e));
		i++;
		if (e->write_cb) {
			EV_SET(&ke[i], (uintptr_t)e->fd, EVFILT_WRITE,
			    EV_ADD, 0, 0, UPTR(e));
			i++;
		}
	}

	error =  kevent(eloop->poll_fd, ke, LENC(i), NULL, 0, NULL);
	free(ke);

#elif defined(HAVE_EPOLL)

	error = 0;
	TAILQ_FOREACH(e, &eloop->events, next) {
		memset(&epe, 0, sizeof(epe));
		epe.data.fd = e->fd;
		epe.events = EPOLLIN;
		if (e->write_cb)
			epe.events |= EPOLLOUT;
		epe.data.ptr = e;
		if (epoll_ctl(eloop->poll_fd, EPOLL_CTL_ADD, e->fd, &epe) == -1)
			error = -1;
	}
#endif

	return error;
}
#endif

int
eloop_signal_set_cb(struct eloop *eloop,
    const int *signals, size_t signals_len,
    void (*signal_cb)(int, void *), void *signal_cb_ctx)
{

	assert(eloop != NULL);

	eloop->signals = signals;
	eloop->signals_len = signals_len;
	eloop->signal_cb = signal_cb;
	eloop->signal_cb_ctx = signal_cb_ctx;
	return eloop_requeue(eloop);
}

#ifndef HAVE_KQUEUE
struct eloop_siginfo {
	int sig;
	struct eloop *eloop;
};
static struct eloop_siginfo _eloop_siginfo;
static struct eloop *_eloop;

static void
eloop_signal1(void *arg)
{
	struct eloop_siginfo *si = arg;

	si->eloop->signal_cb(si->sig, si->eloop->signal_cb_ctx);
}

static void
#if !defined(__minix)
eloop_signal3(int sig, __unused siginfo_t *siginfo, __unused void *arg)
#else /* defined(__minix) */
eloop_signal3(int sig)
#endif /* defined(__minix) */
{

	/* So that we can operate safely under a signal we instruct
	 * eloop to pass a copy of the siginfo structure to handle_signal1
	 * as the very first thing to do. */
	_eloop_siginfo.eloop = _eloop;
	_eloop_siginfo.sig = sig;
	eloop_timeout_add_now(_eloop_siginfo.eloop,
	    eloop_signal1, &_eloop_siginfo);
}
#endif

int
eloop_signal_mask(struct eloop *eloop, sigset_t *oldset)
{
	sigset_t newset;
#ifndef HAVE_KQUEUE
	size_t i;
	struct sigaction sa;
#endif

	assert(eloop != NULL);

	sigfillset(&newset);
	if (sigprocmask(SIG_SETMASK, &newset, oldset) == -1)
		return -1;

#ifdef HAVE_KQUEUE
	UNUSED(eloop);
#else
	memset(&sa, 0, sizeof(sa));
#if !defined(__minix)
	sa.sa_sigaction = eloop_signal3;
	sa.sa_flags = SA_SIGINFO;
#else /* defined(__minix) */
	sa.sa_handler = eloop_signal3;
#endif /* defined(__minix) */
	sigemptyset(&sa.sa_mask);

	for (i = 0; i < eloop->signals_len; i++) {
		if (sigaction(eloop->signals[i], &sa, NULL) == -1)
			return -1;
	}
#endif
	return 0;
}

struct eloop *
eloop_new(void)
{
	struct eloop *eloop;
	struct timespec now;

	/* Check we have a working monotonic clock. */
	if (clock_gettime(CLOCK_MONOTONIC, &now) == -1)
		return NULL;

	eloop = calloc(1, sizeof(*eloop));
	if (eloop) {
		TAILQ_INIT(&eloop->events);
		TAILQ_INIT(&eloop->free_events);
		TAILQ_INIT(&eloop->timeouts);
		TAILQ_INIT(&eloop->free_timeouts);
		eloop->exitcode = EXIT_FAILURE;
#if defined(HAVE_KQUEUE) || defined(HAVE_EPOLL)
		eloop->poll_fd = -1;
		eloop_open(eloop);
#endif
	}

	return eloop;
}

void eloop_free(struct eloop *eloop)
{
	struct eloop_event *e;
	struct eloop_timeout *t;

	if (eloop == NULL)
		return;

	while ((e = TAILQ_FIRST(&eloop->events))) {
		TAILQ_REMOVE(&eloop->events, e, next);
		free(e);
	}
	while ((e = TAILQ_FIRST(&eloop->free_events))) {
		TAILQ_REMOVE(&eloop->free_events, e, next);
		free(e);
	}
	while ((t = TAILQ_FIRST(&eloop->timeouts))) {
		TAILQ_REMOVE(&eloop->timeouts, t, next);
		free(t);
	}
	while ((t = TAILQ_FIRST(&eloop->free_timeouts))) {
		TAILQ_REMOVE(&eloop->free_timeouts, t, next);
		free(t);
	}
#if defined(HAVE_KQUEUE) || defined(HAVE_EPOLL)
	close(eloop->poll_fd);
#else
	free(eloop->fds);
#endif
	free(eloop);
}

int
eloop_start(struct eloop *eloop, sigset_t *signals)
{
	int n;
	struct eloop_event *e;
	struct eloop_timeout *t;
	struct timespec now, ts, *tsp;
	void (*t0)(void *);
#if defined(HAVE_KQUEUE)
	struct kevent ke;
	UNUSED(signals);
#elif defined(HAVE_EPOLL)
	struct epoll_event epe;
#endif
#ifndef HAVE_KQUEUE
	int timeout;

	_eloop = eloop;
#endif

	assert(eloop != NULL);

	for (;;) {
		if (eloop->exitnow)
			break;

		/* Run all timeouts first */
		if (eloop->timeout0) {
			t0 = eloop->timeout0;
			eloop->timeout0 = NULL;
			t0(eloop->timeout0_arg);
			continue;
		}
		if ((t = TAILQ_FIRST(&eloop->timeouts))) {
			clock_gettime(CLOCK_MONOTONIC, &now);
			if (timespeccmp(&now, &t->when, >)) {
				TAILQ_REMOVE(&eloop->timeouts, t, next);
				t->callback(t->arg);
				TAILQ_INSERT_TAIL(&eloop->free_timeouts, t, next);
				continue;
			}
			timespecsub(&t->when, &now, &ts);
			tsp = &ts;
		} else
			/* No timeouts, so wait forever */
			tsp = NULL;

		if (tsp == NULL && eloop->events_len == 0)
			break;

#ifndef HAVE_KQUEUE
		if (tsp == NULL)
			timeout = -1;
		else if (tsp->tv_sec > INT_MAX / 1000 ||
		    (tsp->tv_sec == INT_MAX / 1000 &&
		    (tsp->tv_nsec + 999999) / 1000000 > INT_MAX % 1000000))
			timeout = INT_MAX;
		else
			timeout = (int)(tsp->tv_sec * 1000 +
			    (tsp->tv_nsec + 999999) / 1000000);
#endif

#if defined(HAVE_KQUEUE)
		n = kevent(eloop->poll_fd, NULL, 0, &ke, 1, tsp);
#elif defined(HAVE_EPOLL)
		if (signals)
			n = epoll_pwait(eloop->poll_fd, &epe, 1,
			    timeout, signals);
		else
			n = epoll_wait(eloop->poll_fd, &epe, 1, timeout);
#else
		if (signals)
			n = pollts(eloop->fds, (nfds_t)eloop->events_len,
			    tsp, signals);
		else
			n = poll(eloop->fds, (nfds_t)eloop->events_len,
			    timeout);
#endif
		if (n == -1) {
			if (errno == EINTR)
				continue;
			return -errno;
		}

		/* Process any triggered events.
		 * We go back to the start after calling each callback incase
		 * the current event or next event is removed. */
#if defined(HAVE_KQUEUE)
		if (n) {
			if (ke.filter == EVFILT_SIGNAL) {
				eloop->signal_cb((int)ke.ident,
				    eloop->signal_cb_ctx);
				continue;
			}
			e = (struct eloop_event *)ke.udata;
			if (ke.filter == EVFILT_WRITE) {
				e->write_cb(e->write_cb_arg);
				continue;
			} else if (ke.filter == EVFILT_READ) {
				e->read_cb(e->read_cb_arg);
				continue;
			}
		}
#elif defined(HAVE_EPOLL)
		if (n) {
			e = (struct eloop_event *)epe.data.ptr;
			if (epe.events & EPOLLOUT && e->write_cb != NULL) {
				e->write_cb(e->write_cb_arg);
				continue;
			}
			if (epe.events &
			    (EPOLLIN | EPOLLERR | EPOLLHUP) &&
			    e->read_cb != NULL)
			{
				e->read_cb(e->read_cb_arg);
				continue;
			}
		}
#else
		if (n > 0) {
			TAILQ_FOREACH(e, &eloop->events, next) {
				if (e->pollfd->revents & POLLOUT &&
				    e->write_cb != NULL)
				{
					e->write_cb(e->write_cb_arg);
					break;
				}
				if (e->pollfd->revents && e->read_cb != NULL) {
					e->read_cb(e->read_cb_arg);
					break;
				}
			}
		}
#endif
	}

	return eloop->exitcode;
}

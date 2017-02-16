/* $NetBSD: eloop.h,v 1.9 2015/05/16 23:31:32 roy Exp $ */

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

#ifndef ELOOP_H
#define ELOOP_H

#include <time.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
/* Attempt to autodetect kqueue or epoll.
 * If we can't, the system has to support pselect, which is a POSIX call. */
#if (defined(__unix__) || defined(unix)) && !defined(USG)
#include <sys/param.h>
#endif
#if defined(BSD)
/* Assume BSD has a working sys/queue.h and kqueue(2) interface */
#define HAVE_SYS_QUEUE_H
#define HAVE_KQUEUE
#elif defined(__linux__)
/* Assume Linux has a working epoll(3) interface */
#define HAVE_EPOLL
#endif
#endif

/* Our structures require TAILQ macros, which really every libc should
 * ship as they are useful beyond belief.
 * Sadly some libc's don't have sys/queue.h and some that do don't have
 * the TAILQ_FOREACH macro. For those that don't, the application using
 * this implementation will need to ship a working queue.h somewhere.
 * If we don't have sys/queue.h found in config.h, then
 * allow QUEUE_H to override loading queue.h in the current directory. */
#ifndef TAILQ_FOREACH
#ifdef HAVE_SYS_QUEUE_H
#include <sys/queue.h>
#elif defined(QUEUE_H)
#define __QUEUE_HEADER(x) #x
#define _QUEUE_HEADER(x) __QUEUE_HEADER(x)
#include _QUEUE_HEADER(QUEUE_H)
#else
#include "queue.h"
#endif
#endif

/* Some systems don't define timespec macros */
#ifndef timespecclear
#define timespecclear(tsp)      (tsp)->tv_sec = (time_t)((tsp)->tv_nsec = 0L)
#define timespecisset(tsp)      ((tsp)->tv_sec || (tsp)->tv_nsec)
#define timespeccmp(tsp, usp, cmp)                                      \
        (((tsp)->tv_sec == (usp)->tv_sec) ?                             \
            ((tsp)->tv_nsec cmp (usp)->tv_nsec) :                       \
            ((tsp)->tv_sec cmp (usp)->tv_sec))
#define timespecadd(tsp, usp, vsp)                                      \
        do {                                                            \
                (vsp)->tv_sec = (tsp)->tv_sec + (usp)->tv_sec;          \
                (vsp)->tv_nsec = (tsp)->tv_nsec + (usp)->tv_nsec;       \
                if ((vsp)->tv_nsec >= 1000000000L) {                    \
                        (vsp)->tv_sec++;                                \
                        (vsp)->tv_nsec -= 1000000000L;                  \
                }                                                       \
        } while (/* CONSTCOND */ 0)
#define timespecsub(tsp, usp, vsp)                                      \
        do {                                                            \
                (vsp)->tv_sec = (tsp)->tv_sec - (usp)->tv_sec;          \
                (vsp)->tv_nsec = (tsp)->tv_nsec - (usp)->tv_nsec;       \
                if ((vsp)->tv_nsec < 0) {                               \
                        (vsp)->tv_sec--;                                \
                        (vsp)->tv_nsec += 1000000000L;                  \
                }                                                       \
        } while (/* CONSTCOND */ 0)
#endif

/* eloop queues are really only for deleting timeouts registered
 * for a function or object.
 * The idea being that one interface as different timeouts for
 * say DHCP and DHCPv6. */
#ifndef ELOOP_QUEUE
  #define ELOOP_QUEUE 1
#endif

struct eloop_event {
	TAILQ_ENTRY(eloop_event) next;
	int fd;
	void (*read_cb)(void *);
	void *read_cb_arg;
	void (*write_cb)(void *);
	void *write_cb_arg;
#if !defined(HAVE_KQUEUE) && !defined(HAVE_EPOLL)
	struct pollfd *pollfd;
#endif
};

struct eloop_timeout {
	TAILQ_ENTRY(eloop_timeout) next;
	struct timespec when;
	void (*callback)(void *);
	void *arg;
	int queue;
};

struct eloop {
	size_t events_len;
	TAILQ_HEAD (event_head, eloop_event) events;
	struct event_head free_events;

	TAILQ_HEAD (timeout_head, eloop_timeout) timeouts;
	struct timeout_head free_timeouts;

	void (*timeout0)(void *);
	void *timeout0_arg;
	const int *signals;
	size_t signals_len;
	void (*signal_cb)(int, void *);
	void *signal_cb_ctx;

#if defined(HAVE_KQUEUE) || defined(HAVE_EPOLL)
	int poll_fd;
#else
	struct pollfd *fds;
	size_t fds_len;
#endif

	int exitnow;
	int exitcode;
};

int eloop_event_add(struct eloop *, int,
    void (*)(void *), void *,
    void (*)(void *), void *);
#define eloop_event_delete(eloop, fd) \
    eloop_event_delete_write((eloop), (fd), 0)
#define eloop_event_remove_writecb(eloop, fd) \
    eloop_event_delete_write((eloop), (fd), 1)
void eloop_event_delete_write(struct eloop *, int, int);

#define eloop_timeout_add_tv(eloop, tv, cb, ctx) \
    eloop_q_timeout_add_tv((eloop), ELOOP_QUEUE, (tv), (cb), (ctx))
#define eloop_timeout_add_sec(eloop, tv, cb, ctx) \
    eloop_q_timeout_add_sec((eloop), ELOOP_QUEUE, (tv), (cb), (ctx))
#define eloop_timeout_add_msec(eloop, ms, cb, ctx) \
    eloop_q_timeout_add_msec((eloop), ELOOP_QUEUE, (ms), (cb), (ctx))
#define eloop_timeout_delete(eloop, cb, ctx) \
    eloop_q_timeout_delete((eloop), ELOOP_QUEUE, (cb), (ctx))
int eloop_q_timeout_add_tv(struct eloop *, int,
    const struct timespec *, void (*)(void *), void *);
int eloop_q_timeout_add_sec(struct eloop *, int,
    time_t, void (*)(void *), void *);
int eloop_q_timeout_add_msec(struct eloop *, int,
    long, void (*)(void *), void *);
void eloop_q_timeout_delete(struct eloop *, int, void (*)(void *), void *);

int eloop_signal_set_cb(struct eloop *, const int *, size_t,
    void (*)(int, void *), void *);
int eloop_signal_mask(struct eloop *, sigset_t *oldset);

struct eloop * eloop_new(void);
#if defined(HAVE_KQUEUE) || defined(HAVE_EPOLL)
int eloop_requeue(struct eloop *);
#else
#define eloop_requeue(eloop) (0)
#endif
void eloop_free(struct eloop *);
void eloop_exit(struct eloop *, int);
int eloop_start(struct eloop *, sigset_t *);

#endif

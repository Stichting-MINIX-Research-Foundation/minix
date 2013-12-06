/*	$NetBSD: time_types.h,v 1.1 2009/11/05 16:59:01 pooka Exp $	*/

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
 *	@(#)time.h	8.5 (Berkeley) 5/4/95
 */

#ifndef _COMPAT_SYS_TIME_TYPES_H_
#define	_COMPAT_SYS_TIME_TYPES_H_

/*
 * Structure returned by gettimeofday(2) system call,
 * and used in other calls.
 */
struct timeval50 {
	long	tv_sec;		/* seconds */
	long	tv_usec;	/* and microseconds */
};

struct	itimerval50 {
	struct	timeval50 it_interval;	/* timer interval */
	struct	timeval50 it_value;	/* current value */
};

/*
 * Structure defined by POSIX.1b to be like a timeval.
 */
struct timespec50 {
	int32_t	tv_sec;		/* seconds */
	long	tv_nsec;	/* and nanoseconds */
};

/*
 * Structure defined by POSIX.1b to be like a itimerval, but with
 * timespecs. Used in the timer_*() system calls.
 */
struct	itimerspec50 {
	struct	timespec50 it_interval;
	struct	timespec50 it_value;
};

static __inline void timeval50_to_timeval(const struct timeval50 *ts50,
    struct timeval *ts)
{
	ts->tv_sec = ts50->tv_sec;
	ts->tv_usec = (suseconds_t)ts50->tv_usec;
}

static __inline void timeval_to_timeval50(const struct timeval *ts,
    struct timeval50 *ts50)
{
	ts50->tv_sec = (long)ts->tv_sec;
	ts50->tv_usec = ts->tv_usec;
}

static __inline void timespec50_to_timespec(const struct timespec50 *ts50,
    struct timespec *ts)
{
	ts->tv_sec = ts50->tv_sec;
	ts->tv_nsec = ts50->tv_nsec;
}

static __inline void timespec_to_timespec50(const struct timespec *ts,
    struct timespec50 *ts50)
{
	ts50->tv_sec = (int32_t)ts->tv_sec;
	ts50->tv_nsec = ts->tv_nsec;
}

static __inline void itimerval50_to_itimerval(const struct itimerval50 *ts50,
    struct itimerval *ts)
{
	timeval50_to_timeval(&ts50->it_interval, &ts->it_interval);
	timeval50_to_timeval(&ts50->it_value, &ts->it_value);
}

static __inline void itimerval_to_itimerval50(const struct itimerval *ts,
    struct itimerval50 *ts50)
{
	timeval_to_timeval50(&ts->it_interval, &ts50->it_interval);
	timeval_to_timeval50(&ts->it_value, &ts50->it_value);
}

static __inline void itimerspec50_to_itimerspec(const struct itimerspec50 *ts50,
    struct itimerspec *ts)
{
	timespec50_to_timespec(&ts50->it_interval, &ts->it_interval);
	timespec50_to_timespec(&ts50->it_value, &ts->it_value);
}

static __inline void itimerspec_to_itimerspec50(const struct itimerspec *ts,
    struct itimerspec50 *ts50)
{
	timespec_to_timespec50(&ts->it_interval, &ts50->it_interval);
	timespec_to_timespec50(&ts->it_value, &ts50->it_value);
}

#endif /* _COMPAT_SYS_TIME_TYPES_H_ */

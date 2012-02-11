/*	$NetBSD: time.h,v 1.3 2010/12/16 18:38:06 christos Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)time.h	8.3 (Berkeley) 1/21/94
 */

#ifndef _COMPAT_TIME_H_
#define	_COMPAT_TIME_H_

#include <compat/sys/time.h>
#if defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE) || \
    defined(_NETBSD_SOURCE)
#define CLK_TCK 100
#endif

__BEGIN_DECLS
#if (_XOPEN_SOURCE - 0) >= 4 || defined(_NETBSD_SOURCE)
char *strptime(const char * __restrict, const char * __restrict,
    struct tm * __restrict);
#if 0
#if defined(_NETBSD_SOURCE)
char *timezone(int, int);
#endif /* _NETBSD_SOURCE */
#endif

#endif /* !_ANSI_SOURCE */
char *ctime(const int32_t *);
double difftime(int32_t, int32_t);
struct tm *gmtime(const int32_t *);
struct tm *localtime(const int32_t *);
int32_t time(int32_t *);
int32_t mktime(struct tm *);
void tzset(void);
void tzsetwall(void);
void __tzset50(void);
void __tzsetwall50(void);

int clock_getres(clockid_t, struct timespec50 *);
int clock_gettime(clockid_t, struct timespec50 *);
int clock_settime(clockid_t, const struct timespec50 *);
int __clock_getres50(clockid_t, struct timespec *);
int __clock_gettime50(clockid_t, struct timespec *);
int __clock_settime50(clockid_t, const struct timespec *);
int nanosleep(const struct timespec50 *, struct timespec50 *);
int __nanosleep50(const struct timespec *, struct timespec *);
int timer_gettime(timer_t, struct itimerspec50 *);
int timer_settime(timer_t, int, const struct itimerspec50 * __restrict, 
    struct itimerspec50 * __restrict);
int __timer_gettime50(timer_t, struct itimerspec *);
int __timer_settime50(timer_t, int, const struct itimerspec * __restrict, 
    struct itimerspec * __restrict);
int __timer_getres50(timer_t, struct itimerspec *);
char *ctime_r(const int32_t *, char *);
struct tm *gmtime_r(const int32_t * __restrict, struct tm * __restrict);
struct tm *localtime_r(const int32_t * __restrict, struct tm * __restrict);
struct tm *offtime(const int32_t *, long);
struct tm *offtime_r(const int32_t *, long, struct tm *);
int32_t timelocal(struct tm *);
int32_t timegm(struct tm *);
int32_t timeoff(struct tm *, long);
int32_t time2posix(int32_t);
int32_t posix2time(int32_t);
struct tm *localtime_rz(const timezone_t, const int32_t * __restrict,
    struct tm * __restrict);
char *ctime_rz(const timezone_t, const int32_t *, char *);
int32_t mktime_z(const timezone_t, struct tm *);
int32_t timelocal_z(const timezone_t, struct tm *);
int32_t time2posix_z(const timezone_t, int32_t);
int32_t posix2time_z(const timezone_t, int32_t);
timezone_t tzalloc(const char *);
void tzfree(const timezone_t);
const char *tzgetname(const timezone_t, int);

#endif /* !_COMPAT_TIME_H_ */

/* $NetBSD: common.h,v 1.10 2015/07/09 10:15:34 roy Exp $ */

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

#ifndef COMMON_H
#define COMMON_H

#include <sys/param.h>
#include <sys/time.h>
#include <stdio.h>
#include <syslog.h>

#include "config.h"
#include "defs.h"
#include "dhcpcd.h"

#ifndef HOSTNAME_MAX_LEN
#define HOSTNAME_MAX_LEN	250	/* 255 - 3 (FQDN) - 2 (DNS enc) */
#endif

#ifndef MIN
#define MIN(a,b)		((/*CONSTCOND*/(a)<(b))?(a):(b))
#define MAX(a,b)		((/*CONSTCOND*/(a)>(b))?(a):(b))
#endif

#define UNCONST(a)		((void *)(unsigned long)(const void *)(a))
#define STRINGIFY(a)		#a
#define TOSTRING(a)		STRINGIFY(a)
#define UNUSED(a)		(void)(a)

#define USEC_PER_SEC		1000000L
#define USEC_PER_NSEC		1000L
#define NSEC_PER_SEC		1000000000L
#define NSEC_PER_MSEC		1000000L
#define MSEC_PER_SEC		1000L
#define CSEC_PER_SEC		100L
#define NSEC_PER_CSEC		10000000L

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

#define timespec_to_double(tv)						     \
	((double)(tv)->tv_sec + (double)((tv)->tv_nsec) / 1000000000.0)
#define timespecnorm(tv) do {						     \
	while ((tv)->tv_nsec >=  NSEC_PER_SEC) {			     \
		(tv)->tv_sec++;						     \
		(tv)->tv_nsec -= NSEC_PER_SEC;				     \
	}								     \
} while (0 /* CONSTCOND */);
#define ts_to_ms(ms, tv) do {						     \
	ms = (tv)->tv_sec * MSEC_PER_SEC;				     \
	ms += (tv)->tv_nsec / NSEC_PER_MSEC;				     \
} while (0 /* CONSTCOND */);
#define ms_to_ts(tv, ms) do {						     \
	(tv)->tv_sec = ms / MSEC_PER_SEC;				     \
	(tv)->tv_nsec = (suseconds_t)(ms - ((tv)->tv_sec * MSEC_PER_SEC))    \
	    * NSEC_PER_MSEC;						     \
} while (0 /* CONSTCOND */);

#ifndef TIMEVAL_TO_TIMESPEC
#define	TIMEVAL_TO_TIMESPEC(tv, ts) do {				\
	(ts)->tv_sec = (tv)->tv_sec;					\
	(ts)->tv_nsec = (tv)->tv_usec * USEC_PER_NSEC;			\
} while (0 /* CONSTCOND */)
#endif

#if __GNUC__ > 2 || defined(__INTEL_COMPILER)
# ifndef __dead
#  define __dead __attribute__((__noreturn__))
# endif
# ifndef __packed
#  define __packed   __attribute__((__packed__))
# endif
# ifndef __printflike
#  define __printflike(a, b) __attribute__((format(printf, a, b)))
# endif
# ifndef __unused
#  define __unused   __attribute__((__unused__))
# endif
#else
# ifndef __dead
#  define __dead
# endif
# ifndef __packed
#  define __packed
# endif
# ifndef __printflike
#  define __printflike
# endif
# ifndef __unused
#  define __unused
# endif
#endif

#ifndef __arraycount
#define __arraycount(__x)       (sizeof(__x) / sizeof(__x[0]))
#endif

/* We don't really need this as our supported systems define __restrict
 * automatically for us, but it is here for completeness. */
#ifndef __restrict
# if defined(__lint__)
#  define __restrict
# elif __STDC_VERSION__ >= 199901L
#  define __restrict restrict
# elif !(2 < __GNUC__ || (2 == __GNU_C && 95 <= __GNUC_VERSION__))
#  define __restrict
# endif
#endif

void get_line_free(void);
const char *get_hostname(char *, size_t, int);
extern int clock_monotonic;
int get_monotonic(struct timespec *);

/* We could shave a few k off the binary size by just using the
 * syslog(3) interface.
 * However, this results in a ugly output on the command line
 * and relies on syslogd(8) starting before dhcpcd which is not
 * always the case. */
#ifndef USE_LOGFILE
# define USE_LOGFILE 1
#endif
#if USE_LOGFILE
void logger_open(struct dhcpcd_ctx *);
#define logger_mask(ctx, lvl) setlogmask((lvl))
__printflike(3, 4) void logger(struct dhcpcd_ctx *, int, const char *, ...);
void logger_close(struct dhcpcd_ctx *);
#else
#define logger_open(ctx) openlog(PACKAGE, LOG_PERROR | LOG_PID, LOG_DAEMON)
#define logger_mask(ctx, lvl) setlogmask((lvl))
#define logger(ctx, pri, fmt, ...)			\
	do {						\
		UNUSED((ctx));				\
		syslog((pri), (fmt), ##__VA_ARGS__);	\
	} while (0 /*CONSTCOND */)
#define logger_close(ctx) closelog()
#endif

ssize_t setvar(struct dhcpcd_ctx *,
    char **, const char *, const char *, const char *);
ssize_t setvard(struct dhcpcd_ctx *,
    char **, const char *, const char *, size_t);
ssize_t addvar(struct dhcpcd_ctx *,
    char ***, const char *, const char *, const char *);
ssize_t addvard(struct dhcpcd_ctx *,
    char ***, const char *, const char *, size_t);

char *hwaddr_ntoa(const unsigned char *, size_t, char *, size_t);
size_t hwaddr_aton(unsigned char *, const char *);
#endif

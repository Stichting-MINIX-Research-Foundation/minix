/*	$NetBSD: rumpuser.c,v 1.67 2015/08/16 11:05:06 pooka Exp $	*/

/*
 * Copyright (c) 2007-2010 Antti Kantee.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "rumpuser_port.h"

#if !defined(lint)
__RCSID("$NetBSD: rumpuser.c,v 1.67 2015/08/16 11:05:06 pooka Exp $");
#endif /* !lint */

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <rump/rumpuser.h>

#include "rumpuser_int.h"

struct rumpuser_hyperup rumpuser__hyp;

int
rumpuser_init(int version, const struct rumpuser_hyperup *hyp)
{
	int rv;

	if (version != RUMPUSER_VERSION) {
		fprintf(stderr, "rumpuser mismatch, kern: %d, hypervisor %d\n",
		    version, RUMPUSER_VERSION);
		abort();
	}

	rv = rumpuser__random_init();
	if (rv != 0) {
		ET(rv);
	}

	rumpuser__thrinit();
	rumpuser__hyp = *hyp;

	return 0;
}

int
rumpuser_clock_gettime(int enum_rumpclock, int64_t *sec, long *nsec)
{
	enum rumpclock rclk = enum_rumpclock;
	struct timespec ts;
	clockid_t clk;
	int rv;

	switch (rclk) {
	case RUMPUSER_CLOCK_RELWALL:
		clk = CLOCK_REALTIME;
		break;
	case RUMPUSER_CLOCK_ABSMONO:
#ifdef HAVE_CLOCK_NANOSLEEP
		clk = CLOCK_MONOTONIC;
#else
		clk = CLOCK_REALTIME;
#endif
		break;
	default:
		abort();
	}

	if (clock_gettime(clk, &ts) == -1) {
		rv = errno;
	} else {
		*sec = ts.tv_sec;
		*nsec = ts.tv_nsec;
		rv = 0;
	}

	ET(rv);
}

int
rumpuser_clock_sleep(int enum_rumpclock, int64_t sec, long nsec)
{
	enum rumpclock rclk = enum_rumpclock;
	struct timespec rqt, rmt;
	int nlocks;
	int rv;

	rumpkern_unsched(&nlocks, NULL);

	/*LINTED*/
	rqt.tv_sec = sec;
	/*LINTED*/
	rqt.tv_nsec = nsec;

	switch (rclk) {
	case RUMPUSER_CLOCK_RELWALL:
		do {
			rv = nanosleep(&rqt, &rmt);
			rqt = rmt;
		} while (rv == -1 && errno == EINTR);
		if (rv == -1) {
			rv = errno;
		}
		break;
	case RUMPUSER_CLOCK_ABSMONO:
		do {
#ifdef HAVE_CLOCK_NANOSLEEP
			rv = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
			    &rqt, NULL);
#else
			/* le/la/der/die/das sigh. timevalspec tailspin */
			struct timespec ts, tsr;
			if ((rv = clock_gettime(CLOCK_REALTIME, &ts)) == -1)
				continue;
			if (ts.tv_sec == rqt.tv_sec ?
			    ts.tv_nsec > rqt.tv_nsec : ts.tv_sec > rqt.tv_sec) {
				rv = 0;
			} else {
				tsr.tv_sec = rqt.tv_sec - ts.tv_sec;
				tsr.tv_nsec = rqt.tv_nsec - ts.tv_nsec;
				if (tsr.tv_nsec < 0) {
					tsr.tv_sec--;
					tsr.tv_nsec += 1000*1000*1000;
				}
				rv = nanosleep(&tsr, NULL);
				if (rv == -1)
					rv = errno;
			}
#endif
		} while (rv == EINTR);
		break;
	default:
		abort();
	}

	rumpkern_sched(nlocks, NULL);

	ET(rv);
}

static int
gethostncpu(void)
{
	int ncpu = 1; /* unknown, really */

#ifdef _SC_NPROCESSORS_ONLN
	ncpu = sysconf(_SC_NPROCESSORS_ONLN);
#endif
	
	return ncpu;
}

int
rumpuser_getparam(const char *name, void *buf, size_t blen)
{
	int rv;

	if (strcmp(name, RUMPUSER_PARAM_NCPU) == 0) {
		int ncpu;

		if (getenv_r("RUMP_NCPU", buf, blen) == -1) {
			sprintf(buf, "2"); /* default */
		} else if (strcmp(buf, "host") == 0) {
			ncpu = gethostncpu();
			snprintf(buf, blen, "%d", ncpu);
		}
		rv = 0;
	} else if (strcmp(name, RUMPUSER_PARAM_HOSTNAME) == 0) {
		char tmp[MAXHOSTNAMELEN];

		if (gethostname(tmp, sizeof(tmp)) == -1) {
			snprintf(buf, blen, "rump-%05d", (int)getpid());
		} else {
			snprintf(buf, blen, "rump-%05d.%s",
			    (int)getpid(), tmp);
		}
		rv = 0;
	} else if (*name == '_') {
		rv = EINVAL;
	} else {
		if (getenv_r(name, buf, blen) == -1)
			rv = errno;
		else
			rv = 0;
	}

	ET(rv);
}

void
rumpuser_putchar(int c)
{

	putchar(c);
}

__dead void
rumpuser_exit(int rv)
{

	printf("halted\n");
	if (rv == RUMPUSER_PANIC)
		abort();
	else
		exit(rv);
}

void
rumpuser_seterrno(int error)
{

	errno = error;
}

/*
 * This is meant for safe debugging prints from the kernel.
 */
void
rumpuser_dprintf(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
}

int
rumpuser_kill(int64_t pid, int rumpsig)
{
	int sig;

	sig = rumpuser__sig_rump2host(rumpsig);
	if (sig > 0)
		raise(sig);
	return 0;
}

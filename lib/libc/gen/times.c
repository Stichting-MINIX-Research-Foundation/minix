/*	$NetBSD: times.c,v 1.15 2009/01/11 02:46:27 christos Exp $	*/

/*-
 * Copyright (c) 1990, 1993
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
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)times.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: times.c,v 1.15 2009/01/11 02:46:27 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/param.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/resource.h>

#include <assert.h>
#include <errno.h>
#include <time.h>

/*
 * Convert usec to clock ticks; could do (usec * CLK_TCK) / 1000000,
 * but this would overflow if we switch to nanosec.
 */
#define	CONVTCK(r)	\
    (clock_t)(r.tv_sec * clk_tck + r.tv_usec / (1000000 / (uint)clk_tck))

#ifndef __times_rusage
#define __times_rusage struct rusage
#endif
#ifndef __times_timeval
#define __times_timeval struct timeval
#endif

clock_t
times(struct tms *tp)
{
	__times_rusage ru;
	__times_timeval t;
	static clock_t clk_tck;
	
	_DIAGASSERT(tp != NULL);

	/*
	 * we use a local copy of CLK_TCK because it expands to a
	 * moderately expensive function call.
	 */
	if (clk_tck == 0)
		clk_tck = (clock_t)CLK_TCK;

	if (getrusage(RUSAGE_SELF, &ru) < 0)
		return ((clock_t)-1);
	tp->tms_utime = CONVTCK(ru.ru_utime);
	tp->tms_stime = CONVTCK(ru.ru_stime);
	if (getrusage(RUSAGE_CHILDREN, &ru) < 0)
		return ((clock_t)-1);
	tp->tms_cutime = CONVTCK(ru.ru_utime);
	tp->tms_cstime = CONVTCK(ru.ru_stime);
	if (gettimeofday(&t, (struct timezone *)0))
		return ((clock_t)-1);
	return ((clock_t)(CONVTCK(t)));
}

/*	$NetBSD: clock.c,v 1.10 2009/01/11 02:46:27 christos Exp $	*/

/*
 * Copyright (c) 1989, 1993
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
static char sccsid[] = "@(#)clock.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: clock.c,v 1.10 2009/01/11 02:46:27 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <time.h>

/*
 * This code is all rather silly because the kernel counts actual
 * execution time (to usec accuracy) then splits it into user, system and
 * interrupt based on when clock ticks happen.  getrusage apportions the
 * time based on the number of ticks, and here we are trying to generate
 * a number which was, traditionally, the number of ticks!
 *
 * Due to the way the time is apportioned, this code (and indeed getrusage
 * itself) are not guaranteed monotonic.
 */

clock_t
clock(void)
{
	struct rusage ru;
	clock_t hz = CLOCKS_PER_SEC;

	if (getrusage(RUSAGE_SELF, &ru))
		return ((clock_t) -1);
	return (clock_t)((ru.ru_utime.tv_sec + ru.ru_stime.tv_sec) * hz +
	    (ru.ru_utime.tv_usec + ru.ru_stime.tv_usec + 50)
	    / 100 * hz / 10000);
}

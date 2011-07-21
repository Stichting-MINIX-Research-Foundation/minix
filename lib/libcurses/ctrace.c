/*	$NetBSD: ctrace.c,v 1.20 2009/01/17 15:25:36 christos Exp $	*/

/*-
 * Copyright (c) 1992, 1993
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
#ifndef lint
#if 0
static char sccsid[] = "@(#)ctrace.c	8.2 (Berkeley) 10/5/93";
#else
__RCSID("$NetBSD: ctrace.c,v 1.20 2009/01/17 15:25:36 christos Exp $");
#endif
#endif				/* not lint */

#ifdef DEBUG
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/time.h>
#include <string.h>

#include "curses.h"
#include "curses_private.h"

static FILE *tracefp = NULL;		/* Curses debugging file descriptor. */

static int tracemask;	/* Areas of trace output we want. */

static int init_done = 0;

void
__CTRACE_init()
{
	char *tf, *tm;

	tm = getenv("CURSES_TRACE_MASK");
	if (tm == NULL)
		tracemask = __CTRACE_ALL;
	else {
		tracemask = (int) strtol(tm, NULL, 0);
	}
	if (tracemask < 0)
		tracemask = (0 - tracemask) ^ __CTRACE_ALL;
	if (tracemask == 0)
		return;

	tf = getenv("CURSES_TRACE_FILE");

	if ((tf != NULL) && !strcmp( tf, "<none>"))
		tf = NULL;

	if (tf != NULL)
		tracefp = fopen(tf, "w");

	init_done = 1;
	__CTRACE(__CTRACE_ALL, "Trace mask: 0x%08x\n", tracemask);
}

void
__CTRACE(int area, const char *fmt,...)
{
	struct timeval tv;
        static int seencr = 1;
	va_list ap;

	if (!init_done)
		__CTRACE_init();
	if (tracefp == NULL || !(tracemask & area)) {
		return;
	}
	gettimeofday(&tv, NULL);
        if (seencr && (tracemask & __CTRACE_TSTAMP)) {
                gettimeofday(&tv, NULL);
                (void) fprintf(tracefp, "%llu.%06lu: ", 
		    (long long)tv.tv_sec, (long)tv.tv_usec);
        }
	va_start(ap, fmt);
        (void) vfprintf(tracefp, fmt, ap);
        seencr = (strchr(fmt, '\n') != NULL);
	va_end(ap);
	(void) fflush(tracefp);
}
#else
/* this kills the empty translation unit message from lint... */
void
__cursesi_make_lint_shut_up_if_debug_not_defined(void);

void
__cursesi_make_lint_shut_up_if_debug_not_defined(void)
{
	return;
}
#endif

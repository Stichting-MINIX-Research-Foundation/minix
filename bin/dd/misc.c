/*	$NetBSD: misc.c,v 1.23 2011/11/07 22:24:23 jym Exp $	*/

/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego and Lance
 * Visser of Convex Computer Corporation.
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
static char sccsid[] = "@(#)misc.c	8.3 (Berkeley) 4/2/94";
#else
__RCSID("$NetBSD: misc.c,v 1.23 2011/11/07 22:24:23 jym Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>
#include <inttypes.h>

#include "dd.h"
#include "extern.h"

#define	tv2mS(tv) ((tv).tv_sec * 1000LL + ((tv).tv_usec + 500) / 1000)

static void posix_summary(void);
#ifndef NO_MSGFMT
static void custom_summary(void);
static void human_summary(void);
static void quiet_summary(void);

static void buffer_write(const char *, size_t, int);
#endif /* NO_MSGFMT */

void
summary(void)
{

	if (progress)
		(void)write(STDERR_FILENO, "\n", 1);

#ifdef NO_MSGFMT
	return posix_summary();
#else /* NO_MSGFMT */
	if (strncmp(msgfmt, "human", sizeof("human")) == 0)
		return human_summary();

	if (strncmp(msgfmt, "posix", sizeof("posix")) == 0)
		return posix_summary();

	if (strncmp(msgfmt, "quiet", sizeof("quiet")) == 0)
		return quiet_summary();

	return custom_summary();
#endif /* NO_MSGFMT */
}

static void
posix_summary(void)
{
	char buf[100];
	int64_t mS;
	struct timeval tv;

	if (progress)
		(void)write(STDERR_FILENO, "\n", 1);

	(void)gettimeofday(&tv, NULL);
	mS = tv2mS(tv) - tv2mS(st.start);
	if (mS == 0)
		mS = 1;

	/* Use snprintf(3) so that we don't reenter stdio(3). */
	(void)snprintf(buf, sizeof(buf),
	    "%llu+%llu records in\n%llu+%llu records out\n",
	    (unsigned long long)st.in_full,  (unsigned long long)st.in_part,
	    (unsigned long long)st.out_full, (unsigned long long)st.out_part);
	(void)write(STDERR_FILENO, buf, strlen(buf));
	if (st.swab) {
		(void)snprintf(buf, sizeof(buf), "%llu odd length swab %s\n",
		    (unsigned long long)st.swab,
		    (st.swab == 1) ? "block" : "blocks");
		(void)write(STDERR_FILENO, buf, strlen(buf));
	}
	if (st.trunc) {
		(void)snprintf(buf, sizeof(buf), "%llu truncated %s\n",
		    (unsigned long long)st.trunc,
		    (st.trunc == 1) ? "block" : "blocks");
		(void)write(STDERR_FILENO, buf, strlen(buf));
	}
	if (st.sparse) {
		(void)snprintf(buf, sizeof(buf), "%llu sparse output %s\n",
		    (unsigned long long)st.sparse,
		    (st.sparse == 1) ? "block" : "blocks");
		(void)write(STDERR_FILENO, buf, strlen(buf));
	}
	(void)snprintf(buf, sizeof(buf),
	    "%llu bytes transferred in %lu.%03d secs (%llu bytes/sec)\n",
	    (unsigned long long) st.bytes,
	    (long) (mS / 1000),
	    (int) (mS % 1000),
	    (unsigned long long) (st.bytes * 1000LL / mS));
	(void)write(STDERR_FILENO, buf, strlen(buf));
}

/* ARGSUSED */
void
summaryx(int notused)
{

	summary();
}

/* ARGSUSED */
void
terminate(int signo)
{

	summary();
	(void)raise_default_signal(signo);
	_exit(127);
}

#ifndef NO_MSGFMT
/*
 * Buffer write(2) calls
 */
static void
buffer_write(const char *str, size_t size, int flush)
{
	static char wbuf[128];
	static size_t cnt = 0; /* Internal counter to allow wbuf to wrap */
	
	unsigned int i;

	for (i = 0; i < size; i++) {
		if (str != NULL) {
			wbuf[cnt++] = str[i];
		}
		if (cnt >= sizeof(wbuf)) {
			(void)write(STDERR_FILENO, wbuf, cnt);
			cnt = 0;
		}
	}

	if (flush != 0) {
		(void)write(STDERR_FILENO, wbuf, cnt);
		cnt = 0;
	}
}

/*
 * Write summary to stderr according to format 'fmt'. If 'enable' is 0, it
 * will not attempt to write anything. Can be used to validate the
 * correctness of the 'fmt' string.
 */
int
dd_write_msg(const char *fmt, int enable)
{
	char hbuf[7], nbuf[32];
	const char *ptr;
	int64_t mS;
	struct timeval tv;

	(void)gettimeofday(&tv, NULL);
	mS = tv2mS(tv) - tv2mS(st.start);
	if (mS == 0)
		mS = 1;

#define ADDC(c) do { if (enable != 0) buffer_write(&c, 1, 0); } \
	while (/*CONSTCOND*/0)
#define ADDS(p) do { if (enable != 0) buffer_write(p, strlen(p), 0); } \
	while (/*CONSTCOND*/0)

	for (ptr = fmt; *ptr; ptr++) {
		if (*ptr != '%') {
			ADDC(*ptr);
			continue;
		}

 		switch (*++ptr) {
		case 'b':
			(void)snprintf(nbuf, sizeof(nbuf), "%llu",
			    (unsigned long long)st.bytes);
			ADDS(nbuf);
			break;
		case 'B':
			if (humanize_number(hbuf, sizeof(hbuf),
			    st.bytes, "B",
			    HN_AUTOSCALE, HN_DECIMAL) == -1)
				warnx("humanize_number (bytes transferred)");
			ADDS(hbuf);
			break;
		case 'e':
			(void)snprintf(nbuf, sizeof(nbuf), "%llu",
			    (unsigned long long) (st.bytes * 1000LL / mS));
			ADDS(nbuf);
			break;
		case 'E':
			if (humanize_number(hbuf, sizeof(hbuf),
			    st.bytes * 1000LL / mS, "B",
			    HN_AUTOSCALE, HN_DECIMAL) == -1)
				warnx("humanize_number (bytes per second)");
			ADDS(hbuf); ADDS("/sec");
			break;
		case 'i':
			(void)snprintf(nbuf, sizeof(nbuf), "%llu",
			    (unsigned long long)st.in_part);
			ADDS(nbuf);
			break;
		case 'I':
			(void)snprintf(nbuf, sizeof(nbuf), "%llu",
			    (unsigned long long)st.in_full);
			ADDS(nbuf);
			break;
		case 'o':
			(void)snprintf(nbuf, sizeof(nbuf), "%llu",
			    (unsigned long long)st.out_part);
			ADDS(nbuf);
			break;
		case 'O':
			(void)snprintf(nbuf, sizeof(nbuf), "%llu",
			    (unsigned long long)st.out_full);
			ADDS(nbuf);
			break;
		case 's':
			(void)snprintf(nbuf, sizeof(nbuf), "%li.%03d",
			    (long) (mS / 1000), (int) (mS % 1000));
			ADDS(nbuf);
			break;
		case 'p':
			(void)snprintf(nbuf, sizeof(nbuf), "%llu",
			    (unsigned long long)st.sparse);
			ADDS(nbuf);
			break;
		case 't':
			(void)snprintf(nbuf, sizeof(nbuf), "%llu",
			    (unsigned long long)st.trunc);
			ADDS(nbuf);
			break;
		case 'w':
			(void)snprintf(nbuf, sizeof(nbuf), "%llu",
			    (unsigned long long)st.swab);
			ADDS(nbuf);
			break;
		case 'P':
			ADDS("block");
			if (st.sparse != 1) ADDS("s");
			break;
		case 'T':
			ADDS("block");
			if (st.trunc != 1) ADDS("s");
			break;
		case 'W':
			ADDS("block");
			if (st.swab != 1) ADDS("s");
			break;
		case '%':
			ADDC(*ptr);
			break;
		default:
			if (*ptr == '\0')
				goto done;
			errx(EXIT_FAILURE, "unknown specifier '%c' in "
			    "msgfmt string", *ptr);
			/* NOTREACHED */
		}
	}

done:
	/* flush buffer */
	buffer_write(NULL, 0, 1);
	return 0;
}

static void
custom_summary(void)
{

	dd_write_msg(msgfmt, 1);
}

static void
human_summary(void)
{
	(void)dd_write_msg("%I+%i records in\n%O+%o records out\n", 1);
	if (st.swab) {
		(void)dd_write_msg("%w odd length swab %W\n", 1);
	}
	if (st.trunc) {
		(void)dd_write_msg("%t truncated %T\n", 1);
	}
	if (st.sparse) {
		(void)dd_write_msg("%p sparse output %P\n", 1);
	}
	(void)dd_write_msg("%b bytes (%B) transferred in %s secs "
	    "(%e bytes/sec - %E)\n", 1);
}

static void
quiet_summary(void)
{

	/* stay quiet */
}
#endif /* NO_MSGFMT */

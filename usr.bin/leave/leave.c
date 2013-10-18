/*	$NetBSD: leave.c,v 1.15 2011/09/16 15:39:27 joerg Exp $	*/

/*
 * Copyright (c) 1980, 1988, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1980, 1988, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)leave.c	8.1 (Berkeley) 6/6/93";
#else
__RCSID("$NetBSD: leave.c,v 1.15 2011/09/16 15:39:27 joerg Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <err.h>
#include <unistd.h>

#define	SECOND	1
#define MINUTE	(SECOND * 60)
#define	HOUR	(MINUTE * 60) 

/*
 * leave [[+]hhmm]
 *
 * Reminds you when you have to leave.
 * Leave prompts for input and goes away if you hit return.
 * It nags you like a mother hen.
 */

__dead static void doalarm(u_int);
__dead static void usage(void);

int
main(int argc, char **argv)
{
	u_int secs;
	int hours, minutes;
	char c, *cp;
	struct tm *t = NULL;
	time_t now;
	int plusnow;
	char buf[50];

	if (setvbuf(stdout, NULL, _IONBF, 0) != 0)
		errx(1, "Cannot set stdout to unbuffered.");

	if (argc < 2) {
		(void)puts("When do you have to leave? ");
		cp = fgets(buf, sizeof(buf), stdin);
		if (cp == NULL || *cp == '\n')
			exit(0);
	} else
		cp = argv[1];

	if (*cp == '+') {
		plusnow = 1;
		++cp;
	} else {
		plusnow = 0;
		(void)time(&now);
		t = localtime(&now);
	}

	for (hours = 0; (c = *cp) && c != '\n'; ++cp) {
		if (!isdigit((unsigned char)c))
			usage();
		hours = hours * 10 + (c - '0');
	}
	minutes = hours % 100;
	hours /= 100;

	if (minutes < 0 || minutes > 59)
		usage();
	if (plusnow)
		secs = (hours * HOUR) + (minutes * MINUTE);
	else {
		if (hours > 23)
			usage();
		if (t->tm_hour >= 12)
			t->tm_hour -= 12;
		if (hours >= 12)
			hours -= 12;
		if (t->tm_hour > hours ||
		    (t->tm_hour == hours && minutes <= t->tm_min))
			hours += 12;
		secs = (hours - t->tm_hour) * HOUR;
		secs += (minutes - t->tm_min) * MINUTE;
	}
	doalarm(secs);
	exit(0);
}

static void
doalarm(u_int secs)
{
	int bother;
	time_t daytime;

	switch (fork()) {
	case 0:
		break;
	case -1:
		err(1, "Fork failed");
		/*NOTREACHED*/
	default:
		exit(0);
	}

	(void)time(&daytime);
	daytime += secs;
	printf("Alarm set for %.16s. (pid %u)\n",
	    ctime(&daytime), (unsigned)getpid());

	/*
	 * if write fails, we've lost the terminal through someone else
	 * causing a vhangup by logging in.
	 */
#define	FIVEMIN	(5 * MINUTE)
	if (secs >= FIVEMIN) {
		sleep(secs - FIVEMIN);
		if (puts("\07\07You have to leave in 5 minutes.\n") == EOF)
			exit(0);
		secs = FIVEMIN;
	}

#define	ONEMIN	(MINUTE)
	if (secs >= ONEMIN) {
		sleep(secs - ONEMIN);
		if (puts("\07\07Just one more minute!\n") == EOF)
			exit(0);
	}

	for (bother = 10; bother--;) {
		sleep((u_int)ONEMIN);
		if (puts("\07\07Time to leave!\n") == EOF)
			exit(0);
	}

	(void)puts("\07\07That was the last time I'll tell you.  Bye.\n");
	exit(0);
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage: %s [[+]hhmm]\n", getprogname());
	exit(1);
}

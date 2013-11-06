/*	$NetBSD: jot.c,v 1.25 2009/04/12 11:19:18 lukem Exp $	*/

/*-
 * Copyright (c) 1993
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
__COPYRIGHT("@(#) Copyright (c) 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)jot.c	8.1 (Berkeley) 6/6/93";
#endif
__RCSID("$NetBSD: jot.c,v 1.25 2009/04/12 11:19:18 lukem Exp $");
#endif /* not lint */

/*
 * jot - print sequential or random data
 *
 * Author:  John Kunze, Office of Comp. Affairs, UCB
 */

#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define	REPS_DEF	100
#define	BEGIN_DEF	1
#define	ENDER_DEF	100
#define	STEP_DEF	1

#define	is_default(s)	(strcmp((s), "-") == 0)

static double	begin = BEGIN_DEF;
static double	ender = ENDER_DEF;
static double	step = STEP_DEF;
static long	reps = REPS_DEF;
static int	randomize;
static int	boring;
static int	prec = -1;
static int	dox;
static int	chardata;
static int	nofinalnl;
static const char *sepstring = "\n";
static char	format[BUFSIZ];

static void	getargs(int, char *[]);
static void	getformat(void);
static int	getprec(char *);
static void	putdata(double, long);
static void	usage(void) __dead;

int
main(int argc, char *argv[])
{
	double	x;
	long	i;

	getargs(argc, argv);
	if (randomize) {
		x = ender - begin;
		if (x < 0) {
			x = -x;
			begin = ender;
		}
		if (dox == 0)
			/*
			 * We are printing floating point, generate random
			 * number that include both supplied limits.
			 * Due to FP routing for display the low and high
			 * values are likely to occur half as often as all
			 * the others.
			 */
			x /= (1u << 31) - 1.0;
		else {
			/*
			 * We are printing integers increase the range by
			 * one but ensure we never generate it.
			 * This makes all the integer values equally likely.
			 */
			x += 1.0;
			x /= (1u << 31);
		}
		srandom((unsigned long) step);
		for (i = 1; i <= reps || reps == 0; i++)
			putdata(random() * x + begin, reps - i);
	} else {
		/*
		 * If we are going to display as integer, add 0.5 here
		 * and use floor(x) later to get sane rounding.
		 */
		x = begin;
		if (dox)
			x += 0.5;
		for (i = 1; i <= reps || reps == 0; i++, x += step)
			putdata(x, reps - i);
	}
	if (!nofinalnl)
		putchar('\n');
	exit(0);
}

static void
getargs(int argc, char *argv[])
{
	unsigned int have = 0;
#define BEGIN	1
#define	STEP	2	/* seed if -r */
#define REPS	4
#define	ENDER	8
	int n = 0;
	long t;
	char *ep;

	for (;;) {
		switch (getopt(argc, argv, "b:cnp:rs:w:")) {
		default:
			usage();
		case -1:
			break;
		case 'c':
			chardata = 1;
			continue;
		case 'n':
			nofinalnl = 1;
			continue;
		case 'p':
			prec = strtol(optarg, &ep, 0);
			if (*ep != 0 || prec < 0)
				errx(EXIT_FAILURE, "Bad precision value");
			continue;
		case 'r':
			randomize = 1;
			continue;
		case 's':
			sepstring = optarg;
			continue;
		case 'b':
			boring = 1;
			/* FALLTHROUGH */
		case 'w':
			strlcpy(format, optarg, sizeof(format));
			continue;
		}
		break;
	}
	argc -= optind;
	argv += optind;

	switch (argc) {	/* examine args right to left, falling thru cases */
	case 4:
		if (!is_default(argv[3])) {
			step = strtod(argv[3], &ep);
			if (*ep != 0)
				errx(EXIT_FAILURE, "Bad step value:  %s",
				    argv[3]);
			have |= STEP;
		}
	case 3:
		if (!is_default(argv[2])) {
			if (!sscanf(argv[2], "%lf", &ender))
				ender = argv[2][strlen(argv[2])-1];
			have |= ENDER;
			if (prec < 0)
				n = getprec(argv[2]);
		}
	case 2:
		if (!is_default(argv[1])) {
			if (!sscanf(argv[1], "%lf", &begin))
				begin = argv[1][strlen(argv[1])-1];
			have |= BEGIN;
			if (prec < 0)
				prec = getprec(argv[1]);
			if (n > prec)		/* maximum precision */
				prec = n;
		}
	case 1:
		if (!is_default(argv[0])) {
			reps = strtoul(argv[0], &ep, 0);
			if (*ep != 0 || reps < 0)
				errx(EXIT_FAILURE, "Bad reps value:  %s",
				    argv[0]);
			have |= REPS;
		}
		break;
	case 0:
		usage();
		break;
	default:
		errx(EXIT_FAILURE,
		    "Too many arguments.  What do you mean by %s?", argv[4]);
	}
	getformat();

	if (prec == -1)
		prec = 0;

	if (randomize) {
		/* 'step' is the seed here, use pseudo-random default */
		if (!(have & STEP))
			step = time(NULL) * getpid();
		/* Take the default values for everything else */
		return;
	}

	/*
	 * The loop we run uses begin/step/reps, so if we have been
	 * given an end value (ender) we must use it to replace the
	 * default values of the others.
	 * We will assume a begin of 0 and step of 1 if necessary.
	 */

	switch (have) {

	case ENDER | STEP:
	case ENDER | STEP | BEGIN:
		/* Calculate reps */
		if (step == 0.0)
			reps = 0;	/* ie infinite */
		else {
			reps = (ender - begin + step) / step;
			if (reps <= 0)
				errx(EXIT_FAILURE, "Impossible stepsize");
		}
		break;

	case REPS | ENDER:
	case REPS | ENDER | STEP:
		/* Calculate begin */
		if (reps == 0)
			errx(EXIT_FAILURE,
			    "Must specify begin if reps == 0");
		begin = ender - reps * step + step;
		break;

	case REPS | BEGIN | ENDER:
		/* Calculate step */
		if (reps == 0)
			errx(EXIT_FAILURE,
			    "Infinite sequences cannot be bounded");
		if (reps == 1)
			step = 0.0;
		else
			step = (ender - begin) / (reps - 1);
		break;

	case REPS | BEGIN | ENDER | STEP:
		/* reps given and implied - take smaller */
		if (step == 0.0)
			break;
		t = (ender - begin + step) / step;
		if (t <= 0)
			errx(EXIT_FAILURE,
			    "Impossible stepsize");
		if (t < reps)
			reps = t;
		break;

	default:
		/* No values can be calculated, use defaults */
		break;
	}
}

static void
putdata(double x, long notlast)
{

	if (boring)				/* repeated word */
		printf("%s", format);
	else if (dox)				/* scalar */
		printf(format, (long)floor(x));
	else					/* real */
		printf(format, x);
	if (notlast != 0)
		fputs(sepstring, stdout);
}

__dead static void
usage(void)
{
	(void)fprintf(stderr, "usage: %s [-cnr] [-b word] [-p precision] "
	    "[-s string] [-w word] [reps [begin [end [step | seed]]]]\n",
	    getprogname());
	exit(1);
}

static int
getprec(char *num_str)
{

	num_str = strchr(num_str, '.');
	if (num_str == NULL)
		return 0;
	return strspn(num_str + 1, "0123456789");
}

static void
getformat(void)
{
	char	*p;
	size_t	sz;

	if (boring)				/* no need to bother */
		return;
	for (p = format; *p; p++) {		/* look for '%' */
		if (*p == '%') {
			if (*(p+1) != '%')
				break;
			p++;		/* leave %% alone */
		}
	}
	sz = sizeof(format) - strlen(format) - 1;
	if (!*p) {
		if (chardata || prec == 0) {
			if ((size_t)snprintf(p, sz, "%%%s", chardata ? "c" : "ld") >= sz)
				errx(EXIT_FAILURE, "-w word too long");
			dox = 1;
		} else {
			if (snprintf(p, sz, "%%.%df", prec) >= (int)sz)
				errx(EXIT_FAILURE, "-w word too long");
		}
	} else if (!*(p+1)) {
		if (sz <= 0)
			errx(EXIT_FAILURE, "-w word too long");
		strcat(format, "%");		/* cannot end in single '%' */
	} else {
		p++;				/* skip leading % */
		for(; *p && !isalpha((unsigned char)*p); p++) {
			/* allow all valid printf(3) flags, but deny '*' */
			if (!strchr("0123456789#-+. ", *p))
				break;
		}
		/* Allow 'l' prefix, but no other. */
		if (*p == 'l')
			p++;
		switch (*p) {
		case 'f': case 'e': case 'g': case '%':
		case 'E': case 'G':
			break;
		case 's':
			errx(EXIT_FAILURE,
			    "cannot convert numeric data to strings");
			break;
		case 'd': case 'o': case 'x': case 'u':
		case 'D': case 'O': case 'X': case 'U':
		case 'c': case 'i':
			dox = 1;
			break;
		default:
			errx(EXIT_FAILURE, "unknown or invalid format `%s'",
			    format);
		}
		/* Need to check for trailing stuff to print */
		for (; *p; p++)		/* look for '%' */
			if (*p == '%') {
				if (*(p+1) != '%')
					break;
				p++;		/* leave %% alone */
			}
		if (*p)
			errx(EXIT_FAILURE, "unknown or invalid format `%s'",
			    format);
	}
}

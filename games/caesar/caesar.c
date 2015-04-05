/*	$NetBSD: caesar.c,v 1.22 2008/07/20 01:03:21 lukem Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Adams.
 *
 * Authors:
 *	Stan King, John Eldridge, based on algorithm suggested by
 *	Bob Morris
 * 29-Sep-82
 *      Roland Illig, 2005
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
__COPYRIGHT("@(#) Copyright (c) 1989, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)caesar.c	8.1 (Berkeley) 5/31/93";
#else
__RCSID("$NetBSD: caesar.c,v 1.22 2008/07/20 01:03:21 lukem Exp $");
#endif
#endif /* not lint */

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NCHARS			(1 << CHAR_BIT)
#define LETTERS			(26)

/*
 * letter frequencies (taken from some unix(tm) documentation)
 * (unix is a trademark of Bell Laboratories)
 */
static const unsigned char upper[LETTERS] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const unsigned char lower[LETTERS] = "abcdefghijklmnopqrstuvwxyz";
static double stdf[LETTERS] = {
	7.97, 1.35, 3.61, 4.78, 12.37, 2.01, 1.46, 4.49, 6.39, 0.04,
	0.42, 3.81, 2.69, 5.92,  6.96, 2.91, 0.08, 6.63, 8.77, 9.68,
	2.62, 0.81, 1.88, 0.23,  2.07, 0.06
};

static unsigned char rottbl[NCHARS];


static void
init_rottbl(int rot)
{
	size_t i;

	rot %= LETTERS;		/* prevent integer overflow */

	for (i = 0; i < NCHARS; i++)
		rottbl[i] = (unsigned char)i;

	for (i = 0; i < LETTERS; i++)
		rottbl[upper[i]] = upper[(i + rot) % LETTERS];

	for (i = 0; i < LETTERS; i++)
		rottbl[lower[i]] = lower[(i + rot) % LETTERS];
}

static void
print_file(void)
{
	int ch;

	while ((ch = getchar()) != EOF) {
		if (putchar(rottbl[ch]) == EOF) {
			err(EXIT_FAILURE, "<stdout>");
			/* NOTREACHED */
		}
	}
}

static void
print_array(const unsigned char *a, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++) {
		if (putchar(rottbl[a[i]]) == EOF) {
			err(EXIT_FAILURE, "<stdout>");
			/* NOTREACHED */
		}
	}
}

static int
get_rotation(const char *arg)
{
	long rot;
	char *endp;

	errno = 0;
	rot = strtol(arg, &endp, 10);
	if (errno == 0 && (arg[0] == '\0' || *endp != '\0'))
		errno = EINVAL;
	if (errno == 0 && (rot < 0 || rot > INT_MAX))
		errno = ERANGE;
	if (errno)
		err(EXIT_FAILURE, "Bad rotation value `%s'", arg);
	return (int)rot;
}

static void
guess_and_rotate(void)
{
	unsigned char inbuf[2048];
	unsigned int obs[NCHARS];
	size_t i, nread;
	double dot, winnerdot;
	int try, winner;
	int ch;

	/* adjust frequency table to weight low probs REAL low */
	for (i = 0; i < LETTERS; i++)
		stdf[i] = log(stdf[i]) + log(LETTERS / 100.0);

	/* zero out observation table */
	(void)memset(obs, 0, sizeof(obs));

	for (nread = 0; nread < sizeof(inbuf); nread++) {
		if ((ch = getchar()) == EOF)
			break;
		inbuf[nread] = (unsigned char) ch;
	}

	for (i = 0; i < nread; i++)
		obs[inbuf[i]]++;

	/*
	 * now "dot" the freqs with the observed letter freqs
	 * and keep track of best fit
	 */
	winner = 0;
	winnerdot = 0.0;
	for (try = 0; try < LETTERS; try++) {
		dot = 0.0;
		for (i = 0; i < LETTERS; i++)
			dot += (obs[upper[i]] + obs[lower[i]])
			     * stdf[(i + try) % LETTERS];

		if (try == 0 || dot > winnerdot) {
			/* got a new winner! */
			winner = try;
			winnerdot = dot;
		}
	}

	init_rottbl(winner);
	print_array(inbuf, nread);
	print_file();
}

int
main(int argc, char **argv)
{

	if (argc == 1) {
		guess_and_rotate();
	} else if (argc == 2) {
		init_rottbl(get_rotation(argv[1]));
		print_file();
	} else {
		(void)fprintf(stderr, "usage: caesar [rotation]\n");
		exit(EXIT_FAILURE);
		/* NOTREACHED */
	}

	if (ferror(stdin)) {
		errx(EXIT_FAILURE, "<stdin>");
		/* NOTREACHED */
	}

	(void)fflush(stdout);
	if (ferror(stdout)) {
		errx(EXIT_FAILURE, "<stdout>");
		/* NOTREACHED */
	}

	return 0;
}

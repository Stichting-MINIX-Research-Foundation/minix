/*	$NetBSD: split.c,v 1.26 2011/09/16 15:39:29 joerg Exp $	*/

/*
 * Copyright (c) 1987, 1993, 1994
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
__COPYRIGHT("@(#) Copyright (c) 1987, 1993, 1994\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)split.c	8.3 (Berkeley) 4/25/94";
#endif
__RCSID("$NetBSD: split.c,v 1.26 2011/09/16 15:39:29 joerg Exp $");
#endif /* not lint */

#include <sys/param.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFLINE	1000		/* Default num lines per file. */

static int file_open;		/* If a file open. */
static int ifd = STDIN_FILENO, ofd = -1; /* Input/output file descriptors. */
static char *fname;		/* File name prefix. */
static size_t sfxlen = 2;		/* suffix length. */

static void newfile(void);
static void split1(off_t, int) __dead;
static void split2(off_t) __dead;
static void split3(off_t) __dead;
static void usage(void) __dead;
static size_t bigwrite(int, void const *, size_t);

int
main(int argc, char *argv[])
{
	int ch;
	char *ep, *p;
	char const *base;
	off_t bytecnt = 0;	/* Byte count to split on. */
	off_t numlines = 0;	/* Line count to split on. */
	off_t chunks = 0;	/* Number of chunks to split into. */

	while ((ch = getopt(argc, argv, "0123456789b:l:a:n:")) != -1)
		switch (ch) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			/*
			 * Undocumented kludge: split was originally designed
			 * to take a number after a dash.
			 */
			if (numlines == 0) {
				p = argv[optind - 1];
				if (p[0] == '-' && p[1] == ch && !p[2])
					p++;
				else
					p = argv[optind] + 1;
				numlines = strtoull(p, &ep, 10);
				if (numlines == 0 || *ep != '\0')
					errx(1, "%s: illegal line count.", p);
			}
			break;
		case 'b':		/* Byte count. */
			if (!isdigit((unsigned char)optarg[0]) ||
			    (bytecnt = strtoull(optarg, &ep, 10)) == 0 ||
			    (*ep != '\0' && *ep != 'k' && *ep != 'm'))
				errx(1, "%s: illegal byte count.", optarg);
			if (*ep == 'k')
				bytecnt *= 1024;
			else if (*ep == 'm')
				bytecnt *= 1024 * 1024;
			break;
		case 'l':		/* Line count. */
			if (numlines != 0)
				usage();
			if (!isdigit((unsigned char)optarg[0]) ||
			    (numlines = strtoull(optarg, &ep, 10)) == 0 ||
			    *ep != '\0')
				errx(1, "%s: illegal line count.", optarg);
			break;
		case 'a':		/* Suffix length. */
			if (!isdigit((unsigned char)optarg[0]) ||
			    (sfxlen = (size_t)strtoul(optarg, &ep, 10)) == 0 ||
			    *ep != '\0')
				errx(1, "%s: illegal suffix length.", optarg);
			break;
		case 'n':		/* Chunks. */
			if (!isdigit((unsigned char)optarg[0]) ||
			    (chunks = (size_t)strtoul(optarg, &ep, 10)) == 0 ||
			    *ep != '\0')
				errx(1, "%s: illegal number of chunks.", optarg);
			break;
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

	if (*argv != NULL) {
		if (strcmp(*argv, "-") != 0 &&
		    (ifd = open(*argv, O_RDONLY, 0)) < 0)
			err(1, "%s", *argv);
		++argv;
	}


	base = (*argv != NULL) ? *argv++ : "x";
	if ((fname = malloc(strlen(base) + sfxlen + 1)) == NULL)
		err(EXIT_FAILURE, NULL);
	(void)strcpy(fname, base);		/* File name prefix. */

	if (*argv != NULL)
		usage();

	if (numlines == 0)
		numlines = DEFLINE;
	else if (bytecnt || chunks)
		usage();

	if (bytecnt && chunks)
		usage();

	if (bytecnt)
		split1(bytecnt, 0);
	else if (chunks)
		split3(chunks);
	else 
		split2(numlines);

	return 0;
}

/*
 * split1 --
 *	Split the input by bytes.
 */
static void
split1(off_t bytecnt, int maxcnt)
{
	off_t bcnt;
	ssize_t dist, len;
	char *C;
	char bfr[MAXBSIZE];
	int nfiles;

	nfiles = 0;

	for (bcnt = 0;;)
		switch (len = read(ifd, bfr, MAXBSIZE)) {
		case 0:
			exit(0);
			/* NOTREACHED */
		case -1:
			err(1, "read");
			/* NOTREACHED */
		default:
			if (!file_open) {
				if (!maxcnt || (nfiles < maxcnt)) {
					newfile();
					nfiles++;
					file_open = 1;
				}
			}
			if (bcnt + len >= bytecnt) {
				/* LINTED: bytecnt - bcnt <= len */
				dist = bytecnt - bcnt;
				if (bigwrite(ofd, bfr, dist) != (size_t)dist)
					err(1, "write");
				len -= dist;
				for (C = bfr + dist; len >= bytecnt;
				    /* LINTED: bytecnt <= len */
				    len -= bytecnt, C += bytecnt) {
					if (!maxcnt || (nfiles < maxcnt)) {
						newfile();
						nfiles++;
					}
					/* LINTED: as above */
					if (bigwrite(ofd,
					    C, bytecnt) != (size_t)bytecnt)
						err(1, "write");
				}
				if (len) {
					if (!maxcnt || (nfiles < maxcnt)) {
						newfile();
						nfiles++;
					}
					/* LINTED: len >= 0 */
					if (bigwrite(ofd, C, len) != (size_t)len)
						err(1, "write");
				} else
					file_open = 0;
				bcnt = len;
			} else {
				bcnt += len;
				/* LINTED: len >= 0 */
				if (bigwrite(ofd, bfr, len) != (size_t)len)
					err(1, "write");
			}
		}
}

/*
 * split2 --
 *	Split the input by lines.
 */
static void
split2(off_t numlines)
{
	off_t lcnt;
	size_t bcnt;
	ssize_t len;
	char *Ce, *Cs;
	char bfr[MAXBSIZE];

	for (lcnt = 0;;)
		switch (len = read(ifd, bfr, MAXBSIZE)) {
		case 0:
			exit(0);
			/* NOTREACHED */
		case -1:
			err(1, "read");
			/* NOTREACHED */
		default:
			if (!file_open) {
				newfile();
				file_open = 1;
			}
			for (Cs = Ce = bfr; len--; Ce++)
				if (*Ce == '\n' && ++lcnt == numlines) {
					bcnt = Ce - Cs + 1;
					if (bigwrite(ofd, Cs, bcnt) != (size_t)bcnt)
						err(1, "write");
					lcnt = 0;
					Cs = Ce + 1;
					if (len)
						newfile();
					else
						file_open = 0;
				}
			if (Cs < Ce) {
				bcnt = Ce - Cs;
				if (bigwrite(ofd, Cs, bcnt) != (size_t)bcnt)
					err(1, "write");
			}
		}
}

/*
 * split3 --
 *	Split the input into specified number of chunks
 */
static void
split3(off_t chunks)
{
	struct stat sb;

	if (fstat(ifd, &sb) == -1) {
		err(1, "stat");
		/* NOTREACHED */
	}

	if (chunks > sb.st_size) {
		errx(1, "can't split into more than %d files",
				(int)sb.st_size);
		/* NOTREACHED */
	}

	split1(sb.st_size/chunks, chunks);
}

/*
 * newfile --
 *	Open a new output file.
 */
static void
newfile(void)
{
	static int fnum;
	static char *fpnt;
	int quot, i;

	if (ofd == -1) {
		fpnt = fname + strlen(fname);
		fpnt[sfxlen] = '\0';
	} else if (close(ofd) != 0)
		err(1, "%s", fname);

	quot = fnum;
	for (i = sfxlen - 1; i >= 0; i--) {
		fpnt[i] = quot % 26 + 'a';
		quot = quot / 26;
	}
	if (quot > 0)
		errx(1, "too many files.");
	++fnum;
	if ((ofd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, DEFFILEMODE)) < 0)
		err(1, "%s", fname);
}

static size_t
bigwrite(int fd, const void *buf, size_t len)
{
	const char *ptr = buf;
	size_t sofar = 0;
	ssize_t w;

	while (len != 0) {
		if  ((w = write(fd, ptr, len)) == -1)
			return sofar;
		len -= w;
		ptr += w;
		sofar += w;
	}
	return sofar;
}


static void
usage(void)
{
	(void)fprintf(stderr,
"usage: %s [-b byte_count] [-l line_count] [-n chunk_count] [-a suffix_length] "
"[file [prefix]]\n", getprogname());
	exit(1);
}

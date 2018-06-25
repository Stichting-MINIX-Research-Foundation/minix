/* $NetBSD: cat.c,v 1.57 2016/06/16 00:52:37 sevan Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kevin Fall.
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

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if !defined(lint)
__COPYRIGHT(
"@(#) Copyright (c) 1989, 1993\
 The Regents of the University of California.  All rights reserved.");
#if 0
static char sccsid[] = "@(#)cat.c	8.2 (Berkeley) 4/27/95";
#else
__RCSID("$NetBSD: cat.c,v 1.57 2016/06/16 00:52:37 sevan Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int bflag, eflag, fflag, lflag, nflag, sflag, tflag, vflag;
static size_t bsize;
static int rval;
static const char *filename;

void cook_args(char *argv[]);
void cook_buf(FILE *);
void raw_args(char *argv[]);
void raw_cat(int);

int
main(int argc, char *argv[])
{
	int ch;
	struct flock stdout_lock;

	setprogname(argv[0]);
	(void)setlocale(LC_ALL, "");

	while ((ch = getopt(argc, argv, "B:beflnstuv")) != -1)
		switch (ch) {
		case 'B':
			bsize = (size_t)strtol(optarg, NULL, 0);
			break;
		case 'b':
			bflag = nflag = 1;	/* -b implies -n */
			break;
		case 'e':
			eflag = vflag = 1;	/* -e implies -v */
			break;
		case 'f':
			fflag = 1;
			break;
		case 'l':
			lflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case 't':
			tflag = vflag = 1;	/* -t implies -v */
			break;
		case 'u':
			setbuf(stdout, NULL);
			break;
		case 'v':
			vflag = 1;
			break;
		default:
		case '?':
			(void)fprintf(stderr,
			    "Usage: %s [-beflnstuv] [-B bsize] [-] "
			    "[file ...]\n", getprogname());
			return EXIT_FAILURE;
		}
	argv += optind;

	if (lflag) {
		stdout_lock.l_len = 0;
		stdout_lock.l_start = 0;
		stdout_lock.l_type = F_WRLCK;
		stdout_lock.l_whence = SEEK_SET;
		if (fcntl(STDOUT_FILENO, F_SETLKW, &stdout_lock) == -1)
			err(EXIT_FAILURE, "stdout");
	}

	if (bflag || eflag || nflag || sflag || tflag || vflag)
		cook_args(argv);
	else
		raw_args(argv);
	if (fclose(stdout))
		err(EXIT_FAILURE, "stdout");
	return rval;
}

void
cook_args(char **argv)
{
	FILE *fp;

	fp = stdin;
	filename = "stdin";
	do {
		if (*argv) {
			if (!strcmp(*argv, "-"))
				fp = stdin;
			else if ((fp = fopen(*argv,
			    fflag ? "rf" : "r")) == NULL) {
				warn("%s", *argv);
				rval = EXIT_FAILURE;
				++argv;
				continue;
			}
			filename = *argv++;
		}
		cook_buf(fp);
		if (fp != stdin)
			(void)fclose(fp);
		else
			clearerr(fp);
	} while (*argv);
}

void
cook_buf(FILE *fp)
{
	int ch, gobble, line, prev;

	line = gobble = 0;
	for (prev = '\n'; (ch = getc(fp)) != EOF; prev = ch) {
		if (prev == '\n') {
			if (sflag) {
				if (ch == '\n') {
					if (gobble)
						continue;
					gobble = 1;
				} else
					gobble = 0;
				}
				if (nflag) {
					if (!bflag || ch != '\n') {
						(void)fprintf(stdout,
						    "%6d\t", ++line);
						if (ferror(stdout))
							break;
					} else if (eflag) {
						(void)fprintf(stdout,
						    "%6s\t", "");
						if (ferror(stdout))
							break;
					}
				}
			}
		if (ch == '\n') {
			if (eflag)
				if (putchar('$') == EOF)
					break;
		} else if (ch == '\t') {
			if (tflag) {
				if (putchar('^') == EOF || putchar('I') == EOF)
					break;
				continue;
			}
		} else if (vflag) {
			if (!isascii(ch)) {
				if (putchar('M') == EOF || putchar('-') == EOF)
					break;
				ch = toascii(ch);
			}
			if (iscntrl(ch)) {
				if (putchar('^') == EOF ||
				    putchar(ch == '\177' ? '?' :
				    ch | 0100) == EOF)
					break;
				continue;
			}
		}
		if (putchar(ch) == EOF)
			break;
	}
	if (ferror(fp)) {
		warn("%s", filename);
		rval = EXIT_FAILURE;
		clearerr(fp);
	}
	if (ferror(stdout))
		err(EXIT_FAILURE, "stdout");
}

void
raw_args(char **argv)
{
	int fd;

	fd = fileno(stdin);
	filename = "stdin";
	do {
		if (*argv) {
			if (!strcmp(*argv, "-")) {
				fd = fileno(stdin);
				if (fd < 0)
					goto skip;
			} else if (fflag) {
				struct stat st;
				fd = open(*argv, O_RDONLY|O_NONBLOCK, 0);
				if (fd < 0)
					goto skip;

				if (fstat(fd, &st) == -1) {
					close(fd);
					goto skip;
				}
				if (!S_ISREG(st.st_mode)) {
					close(fd);
					warnx("%s: not a regular file", *argv);
					goto skipnomsg;
				}
			}
			else if ((fd = open(*argv, O_RDONLY, 0)) < 0) {
skip:
				warn("%s", *argv);
skipnomsg:
				rval = EXIT_FAILURE;
				++argv;
				continue;
			}
			filename = *argv++;
		} else if (fd < 0) {
			err(EXIT_FAILURE, "stdin");
		}
		raw_cat(fd);
		if (fd != fileno(stdin))
			(void)close(fd);
	} while (*argv);
}

void
raw_cat(int rfd)
{
	static char *buf;
	static char fb_buf[BUFSIZ];

	ssize_t nr, nw, off;
	int wfd;

	wfd = fileno(stdout);
	if (wfd < 0)
		err(EXIT_FAILURE, "stdout");
	if (buf == NULL) {
		struct stat sbuf;

		if (bsize == 0) {
			if (fstat(wfd, &sbuf) == 0 && sbuf.st_blksize > 0 &&
			    (size_t)sbuf.st_blksize > sizeof(fb_buf))
				bsize = sbuf.st_blksize;
		}
		if (bsize > sizeof(fb_buf)) {
			buf = malloc(bsize);
			if (buf == NULL)
				warnx("malloc, using %zu buffer", bsize);
		}
		if (buf == NULL) {
			bsize = sizeof(fb_buf);
			buf = fb_buf;
		}
	}
	while ((nr = read(rfd, buf, bsize)) > 0)
		for (off = 0; nr; nr -= nw, off += nw)
			if ((nw = write(wfd, buf + off, (size_t)nr)) < 0)
				err(EXIT_FAILURE, "stdout");
	if (nr < 0) {
		warn("%s", filename);
		rval = EXIT_FAILURE;
	}
}

/*	$NetBSD: wc.c,v 1.35 2011/09/16 15:39:30 joerg Exp $	*/

/*
 * Copyright (c) 1980, 1987, 1991, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1980, 1987, 1991, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)wc.c	8.2 (Berkeley) 5/2/95";
#else
__RCSID("$NetBSD: wc.c,v 1.35 2011/09/16 15:39:30 joerg Exp $");
#endif
#endif /* not lint */

/* wc line, word, char count and optionally longest line. */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>

#include <ctype.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#ifdef NO_QUAD
typedef u_long wc_count_t;
# define WCFMT	" %7lu"
# define WCCAST unsigned long
#else
typedef u_quad_t wc_count_t;
# define WCFMT	" %7llu"
# define WCCAST	unsigned long long
#endif

static wc_count_t	tlinect, twordct, tcharct, tlongest;
static bool		doline, doword, dobyte, dochar, dolongest;
static int 		rval = 0;

static void	cnt(const char *);
static void	print_counts(wc_count_t, wc_count_t, wc_count_t, wc_count_t,
		    const char *);
__dead static void	usage(void);
static size_t	do_mb(wchar_t *, const char *, size_t, mbstate_t *,
		    size_t *, const char *);

int
main(int argc, char *argv[])
{
	int ch;

	setlocale(LC_ALL, "");

	while ((ch = getopt(argc, argv, "lwcmL")) != -1)
		switch (ch) {
		case 'l':
			doline = true;
			break;
		case 'w':
			doword = true;
			break;
		case 'm':
			dochar = true;
			dobyte = 0;
			break;
		case 'c':
			dochar = 0;
			dobyte = true;
			break;
		case 'L':
			dolongest = true;
			break;
		case '?':
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

	/* Wc's flags are on by default. */
	if (!(doline || doword || dobyte || dochar || dolongest))
		doline = doword = dobyte = true;

	if (*argv == NULL) {
		cnt(NULL);
	} else {
		bool dototal = (argc > 1);

		do {
			cnt(*argv);
		} while(*++argv);

		if (dototal) {
			print_counts(tlinect, twordct, tcharct, tlongest,
			    "total");
		}
	}

	exit(rval);
}

static size_t
do_mb(wchar_t *wc, const char *p, size_t len, mbstate_t *st,
    size_t *retcnt, const char *file)
{
	size_t r;
	size_t c = 0;

	do {
		r = mbrtowc(wc, p, len, st);
		if (r == (size_t)-1) {
			warnx("%s: invalid byte sequence", file);
			rval = 1;

			/* XXX skip 1 byte */
			len--;
			p++;
			memset(st, 0, sizeof(*st));
			continue;
		} else if (r == (size_t)-2)
			break;
		else if (r == 0)
			r = 1;
		c++;
		if (wc)
			wc++;
		len -= r;
		p += r;
	} while (len > 0);

	*retcnt = c;

	return (r);
}

static void
cnt(const char *file)
{
	u_char buf[MAXBSIZE];
	wchar_t wbuf[MAXBSIZE];
	struct stat sb;
	wc_count_t charct, linect, wordct, longest;
	mbstate_t st;
	u_char *C;
	wchar_t *WC;
	const char *name;			/* filename or <stdin> */
	size_t r = 0;
	int fd, len = 0;

	linect = wordct = charct = longest = 0;
	if (file != NULL) {
		if ((fd = open(file, O_RDONLY, 0)) < 0) {
			warn("%s", file);
			rval = 1;
			return;
		}
		name = file;
	} else {
		fd = STDIN_FILENO;
		name = "<stdin>";
	}

	if (dochar || doword || dolongest)
		(void)memset(&st, 0, sizeof(st));

	if (!(doword || dolongest)) {
		/*
		 * line counting is split out because it's a lot
		 * faster to get lines than to get words, since
		 * the word count requires some logic.
		 */
		if (doline || dochar) {
			while ((len = read(fd, buf, MAXBSIZE)) > 0) {
				if (dochar) {
					size_t wlen;

					r = do_mb(0, (char *)buf, (size_t)len,
					    &st, &wlen, name);
					charct += wlen;
				} else if (dobyte)
					charct += len;
				if (doline) {
					for (C = buf; len--; ++C) {
						if (*C == '\n')
							++linect;
					}
				}
			}
		}

		/*
		 * if all we need is the number of characters and
		 * it's a directory or a regular or linked file, just
		 * stat the puppy.  We avoid testing for it not being
		 * a special device in case someone adds a new type
		 * of inode.
		 */
		else if (dobyte) {
			if (fstat(fd, &sb)) {
				warn("%s", name);
				rval = 1;
			} else {
				if (S_ISREG(sb.st_mode) ||
				    S_ISLNK(sb.st_mode) ||
				    S_ISDIR(sb.st_mode)) {
					charct = sb.st_size;
				} else {
					while ((len =
					    read(fd, buf, MAXBSIZE)) > 0)
						charct += len;
				}
			}
		}
	} else {
		/* do it the hard way... */
		wc_count_t linelen;
                bool       gotsp;

		linelen = 0;
		gotsp = true;
		while ((len = read(fd, buf, MAXBSIZE)) > 0) {
			size_t wlen;

			r = do_mb(wbuf, (char *)buf, (size_t)len, &st, &wlen,
			    name);
			if (dochar) {
				charct += wlen;
			} else if (dobyte) {
				charct += len;
			}
			for (WC = wbuf; wlen--; ++WC) {
				if (iswspace(*WC)) {
					gotsp = true;
					if (*WC == L'\n') {
						++linect;
						if (linelen > longest)
							longest = linelen;
						linelen = 0;
					} else {
						linelen++;
					}
				} else {
					/*
					 * This line implements the POSIX
					 * spec, i.e. a word is a "maximal
					 * string of characters delimited by
					 * whitespace."  Notice nothing was
					 * said about a character being
					 * printing or non-printing.
					 */
					if (gotsp) {
						gotsp = false;
						++wordct;
					}

					linelen++;
				}
			}
		}
	}

	if (len == -1) {
		warn("%s", name);
		rval = 1;
	}
	if (dochar && r == (size_t)-2) {
		warnx("%s: incomplete multibyte character", name);
		rval = 1;
	}

	print_counts(linect, wordct, charct, longest, file);

	/*
	 * don't bother checkint doline, doword, or dobyte --- speeds
	 * up the common case
	 */
	tlinect += linect;
	twordct += wordct;
	tcharct += charct;
	if (dolongest && longest > tlongest)
		tlongest = longest;

	if (close(fd)) {
		warn("%s", name);
		rval = 1;
	}
}

static void
print_counts(wc_count_t lines, wc_count_t words, wc_count_t chars,
    wc_count_t longest, const char *name)
{

	if (doline)
		(void)printf(WCFMT, (WCCAST)lines);
	if (doword)
		(void)printf(WCFMT, (WCCAST)words);
	if (dobyte || dochar)
		(void)printf(WCFMT, (WCCAST)chars);
	if (dolongest)
		(void)printf(WCFMT, (WCCAST)longest);

	if (name != NULL)
		(void)printf(" %s\n", name);
	else
		(void)putchar('\n');
}

static void
usage(void)
{

	(void)fprintf(stderr, "usage: wc [-c | -m] [-Llw] [file ...]\n");
	exit(1);
}

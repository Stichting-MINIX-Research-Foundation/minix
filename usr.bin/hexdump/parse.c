/*	$NetBSD: parse.c,v 1.27 2011/09/04 20:27:27 joerg Exp $	*/

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

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if !defined(lint)
#if 0
static char sccsid[] = "@(#)parse.c	8.1 (Berkeley) 6/6/93";
#else
__RCSID("$NetBSD: parse.c,v 1.27 2011/09/04 20:27:27 joerg Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/file.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util.h>

#include "hexdump.h"

__dead static void	 badcnt(char *);
__dead static void	 badconv(char *);
__dead static void	 badfmt(const char *);
__dead static void	 badsfmt(void);

FU *endfu;					/* format at end-of-data */

void
addfile(char *name)
{
	char *p;
	FILE *fp;
	int ch;
	char buf[2048 + 1];

	if ((fp = fopen(name, "r")) == NULL)
		err(1, "fopen %s", name);
	while (fgets(buf, sizeof(buf), fp)) {
		if (!(p = strchr(buf, '\n'))) {
			warnx("line too long.");
			while ((ch = getchar()) != '\n' && ch != EOF);
			continue;
		}
		*p = '\0';
		for (p = buf; *p && isspace((unsigned char)*p); ++p);
		if (!*p || *p == '#')
			continue;
		add(p);
	}
	(void)fclose(fp);
}

void
add(const char *fmt)
{
	const char *p;
	static FS **nextfs;
	FS *tfs;
	FU *tfu, **nextfu;
	const char *savep;

	/* start new linked list of format units */
	tfs = ecalloc(1, sizeof(FS));
	if (!fshead)
		fshead = tfs;
	else
		*nextfs = tfs;
	nextfs = &tfs->nextfs;
	nextfu = &tfs->nextfu;

	/* take the format string and break it up into format units */
	for (p = fmt;;) {
		/* skip leading white space */
		for (; isspace((unsigned char)*p); ++p);
		if (!*p)
			break;

		/* allocate a new format unit and link it in */
		tfu = ecalloc(1, sizeof(FU));
		*nextfu = tfu;
		nextfu = &tfu->nextfu;
		tfu->reps = 1;

		/* if leading digit, repetition count */
		if (isdigit((unsigned char)*p)) {
			for (savep = p; isdigit((unsigned char)*p); ++p);
			if (!isspace((unsigned char)*p) && *p != '/')
				badfmt(fmt);
			/* may overwrite either white space or slash */
			tfu->reps = atoi(savep);
			tfu->flags = F_SETREP;
			/* skip trailing white space */
			for (++p; isspace((unsigned char)*p); ++p);
		}

		/* skip slash and trailing white space */
		if (*p == '/')
			while (isspace((unsigned char)*++p));

		/* byte count */
		if (isdigit((unsigned char)*p)) {
			for (savep = p; isdigit((unsigned char)*p); ++p);
			if (!isspace((unsigned char)*p))
				badfmt(fmt);
			tfu->bcnt = atoi(savep);
			/* skip trailing white space */
			for (++p; isspace((unsigned char)*p); ++p);
		}

		/* format */
		if (*p != '"')
			badfmt(fmt);
		for (savep = ++p; *p != '"';)
			if (*p++ == 0)
				badfmt(fmt);
		tfu->fmt = emalloc(p - savep + 1);
		(void) strncpy(tfu->fmt, savep, p - savep);
		tfu->fmt[p - savep] = '\0';
		escape(tfu->fmt);
		p++;
	}
}

static const char *spec = ".#-+ 0123456789";

int
size(FS *fs)
{
	FU *fu;
	int bcnt, cursize;
	char *fmt;
	int prec;

	/* figure out the data block size needed for each format unit */
	for (cursize = 0, fu = fs->nextfu; fu; fu = fu->nextfu) {
		if (fu->bcnt) {
			cursize += fu->bcnt * fu->reps;
			continue;
		}
		for (bcnt = prec = 0, fmt = fu->fmt; *fmt; ++fmt) {
			if (*fmt != '%')
				continue;
			/*
			 * skip any special chars -- save precision in
			 * case it's a %s format.
			 */
			while (strchr(spec + 1, *++fmt));
			if (*fmt == '.' && isdigit((unsigned char)*++fmt)) {
				prec = atoi(fmt);
				while (isdigit((unsigned char)*++fmt));
			}
			switch(*fmt) {
			case 'c':
				bcnt += 1;
				break;
			case 'd': case 'i': case 'o': case 'u':
			case 'x': case 'X':
				bcnt += 4;
				break;
			case 'e': case 'E': case 'f': case 'g': case 'G':
				bcnt += 8;
				break;
			case 's':
				bcnt += prec;
				break;
			case '_':
				switch(*++fmt) {
				case 'c': case 'p': case 'u':
					bcnt += 1;
					break;
				}
			}
		}
		cursize += bcnt * fu->reps;
	}
	return (cursize);
}

void
rewrite(FS *fs)
{
	enum { NOTOKAY, USEBCNT, USEPREC } sokay;
	PR *pr, **nextpr;
	FU *fu;
	char *p1, *p2;
	char savech, *fmtp, cs[sizeof(PRId64)];
	int nconv, prec;

	prec = 0;
	for (fu = fs->nextfu; fu; fu = fu->nextfu) {
		/*
		 * Break each format unit into print units; each conversion
		 * character gets its own.
		 */
		nextpr = &fu->nextpr;
		for (nconv = 0, fmtp = fu->fmt; *fmtp; nextpr = &pr->nextpr) {
			pr = ecalloc(1, sizeof(*pr));
			*nextpr = pr;

			/* Skip preceding text and up to the next % sign. */
			for (p1 = fmtp; *p1 && *p1 != '%'; ++p1);

			/* Only text in the string. */
			if (!*p1) {
				pr->fmt = fmtp;
				pr->flags = F_TEXT;
				break;
			}

			/*
			 * Get precision for %s -- if have a byte count, don't
			 * need it.
			 */
			if (fu->bcnt) {
				sokay = USEBCNT;
				/* Skip to conversion character. */
				for (++p1; *p1 && strchr(spec, *p1); ++p1);
			} else {
				/* Skip any special chars, field width. */
				while (*++p1 && strchr(spec + 1, *p1));
				if (*p1 == '.' &&
				    isdigit((unsigned char)*++p1)) {
					sokay = USEPREC;
					prec = atoi(p1);
					while (isdigit((unsigned char)*++p1))
						continue;
				} else
					sokay = NOTOKAY;
			}

			p2 = *p1 ? p1 + 1 : p1;	/* Set end pointer. */
			cs[0] = *p1;		/* Set conversion string. */
			cs[1] = '\0';

			/*
			 * Figure out the byte count for each conversion;
			 * rewrite the format as necessary, set up blank-
			 * padding for end of data.
			 */
			switch(cs[0]) {
			case 'c':
				pr->flags = F_CHAR;
				switch(fu->bcnt) {
				case 0: case 1:
					pr->bcnt = 1;
					break;
				default:
					p1[1] = '\0';
					badcnt(p1);
				}
				break;
			case 'd': case 'i':
				pr->flags = F_INT;
				goto isint;
			case 'o': case 'u': case 'x': case 'X':
				pr->flags = F_UINT;
isint:
				/*
				 * Regardless of pr->bcnt, all integer
				 * values are cast to [u]int64_t before
				 * being printed by display().  We
				 * therefore need to use PRI?64 as the
				 * format, where '?' could actually
				 * be any of [diouxX].  We make the
				 * assumption (not guaranteed by the
				 * C99 standard) that we can derive
				 * all the other PRI?64 values from
				 * PRId64 simply by changing the last
				 * character.  For example, if PRId64 is
				 * "lld" or "qd", and cs[0] is 'o', then
				 * we end up with "llo" or "qo".
				 */
				savech = cs[0];
				strncpy(cs, PRId64, sizeof(PRId64) - 2);
				cs[sizeof(PRId64) - 2] = savech;
				cs[sizeof(PRId64) - 1] = '\0';
				switch(fu->bcnt) {
				case 0: case 4:
					pr->bcnt = 4;
					break;
				case 1:
					pr->bcnt = 1;
					break;
				case 2:
					pr->bcnt = 2;
					break;
				case 8:
					pr->bcnt = 8;
					break;
				default:
					p1[1] = '\0';
					badcnt(p1);
				}
				break;
			case 'e': case 'E': case 'f': case 'g': case 'G':
				pr->flags = F_DBL;
				switch(fu->bcnt) {
				case 0: case 8:
					pr->bcnt = 8;
					break;
				case 4:
					pr->bcnt = 4;
					break;
				default:
					p1[1] = '\0';
					badcnt(p1);
				}
				break;
			case 's':
				pr->flags = F_STR;
				switch(sokay) {
				case NOTOKAY:
					badsfmt();
				case USEBCNT:
					pr->bcnt = fu->bcnt;
					break;
				case USEPREC:
					pr->bcnt = prec;
					break;
				}
				break;
			case '_':
				++p2;
				switch(p1[1]) {
				case 'A':
					endfu = fu;
					fu->flags |= F_IGNORE;
					/* FALLTHROUGH */
				case 'a':
					pr->flags = F_ADDRESS;
					++p2;
					switch(p1[2]) {
					case 'd': case 'o': case'x':
						/*
						 * See comments above for
						 * the way we use PRId64.
						 */
						strncpy(cs, PRId64,
							sizeof(PRId64) - 2);
						cs[sizeof(PRId64) - 2] = p1[2];
						cs[sizeof(PRId64) - 1] = '\0';
						break;
					default:
						p1[3] = '\0';
						badconv(p1);
					}
					break;
				case 'c':
					pr->flags = F_C;
					/* cs[0] = 'c';	set in conv_c */
					goto isint2;
				case 'p':
					pr->flags = F_P;
					cs[0] = 'c';
					goto isint2;
				case 'u':
					pr->flags = F_U;
					/* cs[0] = 'c';	set in conv_u */
isint2:					switch(fu->bcnt) {
					case 0: case 1:
						pr->bcnt = 1;
						break;
					default:
						p1[2] = '\0';
						badcnt(p1);
					}
					break;
				default:
					p1[2] = '\0';
					badconv(p1);
				}
				break;
			default:
				p1[1] = '\0';
				badconv(p1);
			}

			/*
			 * Copy to PR format string, set conversion character
			 * pointer, update original.
			 */
			savech = *p2;
			p1[0] = '\0';
			pr->fmt = emalloc(strlen(fmtp) + strlen(cs) + 1);
			(void)strcpy(pr->fmt, fmtp);
			(void)strcat(pr->fmt, cs);
			*p2 = savech;
			pr->cchar = pr->fmt + (p1 - fmtp);
			fmtp = p2;

			/* Only one conversion character if byte count. */
			if (!(pr->flags&F_ADDRESS) && fu->bcnt && nconv++)
				errx(1,
			    "byte count with multiple conversion characters");
		}
		/*
		 * If format unit byte count not specified, figure it out
		 * so can adjust rep count later.
		 */
		if (!fu->bcnt)
			for (pr = fu->nextpr; pr; pr = pr->nextpr)
				fu->bcnt += pr->bcnt;
	}
	/*
	 * If the format string interprets any data at all, and it's
	 * not the same as the blocksize, and its last format unit
	 * interprets any data at all, and has no iteration count,
	 * repeat it as necessary.
	 *
	 * If, rep count is greater than 1, no trailing whitespace
	 * gets output from the last iteration of the format unit.
	 */
	for (fu = fs->nextfu; fu; fu = fu->nextfu) {
		if (!fu->nextfu && fs->bcnt < blocksize &&
		    !(fu->flags&F_SETREP) && fu->bcnt)
			fu->reps += (blocksize - fs->bcnt) / fu->bcnt;
		if (fu->reps > 1) {
			if (!fu->nextpr)
				break;
			for (pr = fu->nextpr;; pr = pr->nextpr)
				if (!pr->nextpr)
					break;
			for (p1 = pr->fmt, p2 = NULL; *p1; ++p1)
				p2 = isspace((unsigned char)*p1) ? p1 : NULL;
			if (p2)
				pr->nospace = p2;
		}
	}
#ifdef DEBUG
	for (fu = fs->nextfu; fu; fu = fu->nextfu) {
		(void)printf("fmt:");
		for (pr = fu->nextpr; pr; pr = pr->nextpr)
			(void)printf(" {%s}", pr->fmt);
		(void)printf("\n");
	}
#endif
}

void
escape(char *p1)
{
	char *p2;

	/* alphabetic escape sequences have to be done in place */
	for (p2 = p1;; ++p1, ++p2) {
		if (!*p1) {
			*p2 = *p1;
			break;
		}
		if (*p1 == '\\')
			switch(*++p1) {
			case '\0':
				*p2 = '\\';
				*++p2 = '\0';
				return;	/* incomplete escape sequence */
			case 'a':
			     /* *p2 = '\a'; */
				*p2 = '\007';
				break;
			case 'b':
				*p2 = '\b';
				break;
			case 'f':
				*p2 = '\f';
				break;
			case 'n':
				*p2 = '\n';
				break;
			case 'r':
				*p2 = '\r';
				break;
			case 't':
				*p2 = '\t';
				break;
			case 'v':
				*p2 = '\v';
				break;
			default:
				*p2 = *p1;
				break;
			}
		else
			*p2 = *p1;
	}
}

static void
badcnt(char *s)
{
	errx(1, "%s: bad byte count", s);
}

static void
badsfmt(void)
{
	errx(1, "%%s: requires a precision or a byte count");
}

static void
badfmt(const char *fmt)
{
	errx(1, "\"%s\": bad format", fmt);
}

static void
badconv(char *ch)
{
	errx(1, "%%%s: bad conversion character", ch);
}

/*	$NetBSD: odsyntax.c,v 1.26 2010/02/09 14:06:37 drochner Exp $	*/

/*-
 * Copyright (c) 1990, 1993
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
#if 0
#if !defined(lint)
#if 0
static char sccsid[] = "@(#)odsyntax.c	8.2 (Berkeley) 5/4/95";
#else
__RCSID("$NetBSD: odsyntax.c,v 1.26 2010/02/09 14:06:37 drochner Exp $");
#endif
#endif /* not lint */
#endif

#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <util.h>

#include "hexdump.h"

#define PADDING "         "

struct odformat {
	char type;
	int nbytes;
	char const *format;
	int minwidth;
};

struct odaddrformat {
	char type;
	char const *format1;
	char const *format2;
};

int odmode;

static void odoffset(int, char ***);
static void posixtypes(char const *);

void
odsyntax(int argc, char ***argvp)
{
	static char empty[] = "", padding[] = PADDING;
	int ch;
	char *p, **argv;

#define TYPE_OFFSET 7
	add("\"%07.7_Ao\n\"");
	add("\"%07.7_ao  \"");

	odmode = 1;
	argv = *argvp;
	while ((ch = getopt(argc, argv,
	    "A:aBbcDdeFfHhIij:LlN:Oot:vXx")) != -1)
		switch (ch) {
		case 'A':
			switch (*optarg) {
			case 'd': case 'o': case 'x':
				fshead->nextfu->fmt[TYPE_OFFSET] = *optarg;
				fshead->nextfs->nextfu->fmt[TYPE_OFFSET] =
					*optarg;
				break;
			case 'n':
				fshead->nextfu->fmt = empty;
				fshead->nextfs->nextfu->fmt = padding;
				break;
			default:
				errx(1, "%s: invalid address base", optarg);
			}
			break;
		case 'a':
			posixtypes("a");
			break;
		case 'B':
		case 'o':
			posixtypes("o2");
			break;
		case 'b':
			posixtypes("o1");
			break;
		case 'c':
			posixtypes("c");
			break;
		case 'd':
			posixtypes("u2");
			break;
		case 'D':
			posixtypes("u4");
			break;
		case 'e':		/* undocumented in od */
		case 'F':
			posixtypes("f8");
			break;
		case 'f':
			posixtypes("f4");
			break;
		case 'H':
		case 'X':
			posixtypes("x4");
			break;
		case 'h':
		case 'x':
			posixtypes("x2");
			break;
		case 'I':
		case 'L':
		case 'l':
			posixtypes("d4");
			break;
		case 'i':
			posixtypes("d2");
			break;
		case 'j':
			if ((skip = strtol(optarg, &p, 0)) < 0)
				errx(1, "%s: bad skip value", optarg);
			switch(*p) {
			case 'b':
				skip *= 512;
				break;
			case 'k':
				skip *= 1024;
				break;
			case 'm':
				skip *= 1048576;
				break;
			}
			break;
		case 'N':
			if ((length = atoi(optarg)) < 0)
				errx(1, "%s: bad length value", optarg);
			break;
		case 'O':
			posixtypes("o4");
			break;
		case 't':
			posixtypes(optarg);
			break;
		case 'v':
			vflag = ALL;
			break;
		case '?':
		default:
			usage();
		}

	if (fshead->nextfs->nextfs == NULL)
		posixtypes("oS");

	argc -= optind;
	*argvp += optind;

	if (argc)
		odoffset(argc, argvp);
}

/* formats used for -t */

static const struct odformat odftab[] = {
	{ 'a', 1, "%3_u",  4 },
	{ 'c', 1, "%3_c",  4 },
	{ 'd', 1, "%4d",   5 },
	{ 'd', 2, "%6d",   6 },
	{ 'd', 4, "%11d", 11 },
	{ 'd', 8, "%20d", 20 },
	{ 'o', 1, "%03o",  4 },
	{ 'o', 2, "%06o",  7 },
	{ 'o', 4, "%011o", 12 },
	{ 'o', 8, "%022o", 23 },
	{ 'u', 1, "%03u" , 4 },
	{ 'u', 2, "%05u" , 6 },
	{ 'u', 4, "%010u", 11 },
	{ 'u', 8, "%020u", 21 },
	{ 'x', 1, "%02x",  3 },
	{ 'x', 2, "%04x",  5 },
	{ 'x', 4, "%08x",  9 },
	{ 'x', 8, "%016x", 17 },
	{ 'f', 4, "%14.7e",  15 },
	{ 'f', 8, "%21.14e", 22 },
	{ 0, 0, NULL, 0 }
};

/*
 * Interpret a POSIX-style -t argument.
 */
static void
posixtypes(char const *type_string)
{
	int nbytes = 0;
	char *fmt, type, *tmp;
	struct odformat const *odf;

	while (*type_string) {
		switch ((type = *type_string++)) {
		case 'a':
		case 'c':
			nbytes = 1;
			break;
		case 'f':
			if (isupper((unsigned char)*type_string)) {
				switch(*type_string) {
				case 'F':
					nbytes = sizeof(float);
					break;
				case 'D':
					nbytes = sizeof(double);
					break;
				case 'L':
					nbytes = sizeof(long double);
					break;
				default:
					warnx("Bad type-size qualifier '%c'",
					    *type_string);
					usage();
				}
				type_string++;
			} else if (isdigit((unsigned char)*type_string)) {
				nbytes = strtol(type_string, &tmp, 10);
				type_string = tmp;
			} else
				nbytes = 8;
			break;
		case 'd':
		case 'o':
		case 'u':
		case 'x':
			if (isupper((unsigned char)*type_string)) {
				switch(*type_string) {
				case 'C':
					nbytes = sizeof(char);
					break;
				case 'S':
					nbytes = sizeof(short);
					break;
				case 'I':
					nbytes = sizeof(int);
					break;
				case 'L':
					nbytes = sizeof(long);
					break;
				default:
					warnx("Bad type-size qualifier '%c'",
					    *type_string);
					usage();
				}
				type_string++;
			} else if (isdigit((unsigned char)*type_string)) {
				nbytes = strtol(type_string, &tmp, 10);
				type_string = tmp;
			} else
				nbytes = 4;
			break;
		default:
			usage();
		}
		for (odf = odftab; odf->type != 0; odf++)
			if (odf->type == type && odf->nbytes == nbytes)
				break;
		if (odf->type == 0)
			errx(1, "%c%d: format not supported", type, nbytes);
		(void)easprintf(&fmt, "%d/%d  \"%*s%s \" \"\\n\"",
		    16 / nbytes, nbytes,
		    4 * nbytes - odf->minwidth, "", odf->format);
		add(fmt);
	}
}

static void
odoffset(int argc, char ***argvp)
{
	char *num, *p;
	int base;
	char *end;

	/*
	 * The offset syntax of od(1) was genuinely bizarre.  First, if
	 * it started with a plus it had to be an offset.  Otherwise, if
	 * there were at least two arguments, a number or lower-case 'x'
	 * followed by a number makes it an offset.  By default it was
	 * octal; if it started with 'x' or '0x' it was hex.  If it ended
	 * in a '.', it was decimal.  If a 'b' or 'B' was appended, it
	 * multiplied the number by 512 or 1024 byte units.  There was
	 * no way to assign a block count to a hex offset.
	 *
	 * We assume it's a file if the offset is bad.
	 */
	p = argc == 1 ? (*argvp)[0] : (*argvp)[1];
	if (!p)
		return;

	if (*p != '+' && (argc < 2 ||
	    (!isdigit((unsigned char)p[0]) &&
	    (p[0] != 'x' || !isxdigit((unsigned char)p[1])))))
		return;

	base = 0;
	/*
	 * skip over leading '+', 'x[0-9a-fA-f]' or '0x', and
	 * set base.
	 */
	if (p[0] == '+')
		++p;
	if (p[0] == 'x' && isxdigit((unsigned char)p[1])) {
		++p;
		base = 16;
	} else if (p[0] == '0' && p[1] == 'x') {
		p += 2;
		base = 16;
	}

	/* skip over the number */
	if (base == 16)
		for (num = p; isxdigit((unsigned char)*p); ++p);
	else
		for (num = p; isdigit((unsigned char)*p); ++p);

	/* check for no number */
	if (num == p)
		return;

	/* if terminates with a '.', base is decimal */
	if (*p == '.') {
		if (base)
			return;
		base = 10;
	}

	skip = strtol(num, &end, base ? base : 8);

	/* if end isn't the same as p, we got a non-octal digit */
	if (end != p) {
		skip = 0;
		return;
	}

	if (*p) {
		if (*p == 'B') {
			skip *= 1024;
			++p;
		} else if (*p == 'b') {
			skip *= 512;
			++p;
		}
	}
	if (*p) {
		skip = 0;
		return;
	}
	/*
	 * If the offset uses a non-octal base, the base of the offset
	 * is changed as well.  This isn't pretty, but it's easy.
	 */
	if (base == 16) {
		fshead->nextfu->fmt[TYPE_OFFSET] = 'x';
		fshead->nextfs->nextfu->fmt[TYPE_OFFSET] = 'x';
	} else if (base == 10) {
		fshead->nextfu->fmt[TYPE_OFFSET] = 'd';
		fshead->nextfs->nextfu->fmt[TYPE_OFFSET] = 'd';
	}

	/* Terminate file list. */
	(*argvp)[1] = NULL;
}

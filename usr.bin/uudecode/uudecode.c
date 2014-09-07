/*	$NetBSD: uudecode.c,v 1.28 2013/01/28 19:50:30 apb Exp $	*/

/*-
 * Copyright (c) 1983, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1983, 1993\
 The Regents of the University of California.  All rights reserved.");
#if 0
static char sccsid[] = "@(#)uudecode.c	8.2 (Berkeley) 4/2/94";
#endif
__RCSID("$NetBSD: uudecode.c,v 1.28 2013/01/28 19:50:30 apb Exp $");
#endif /* not lint */

/*
 * uudecode [file ...]
 *
 * create the specified file, decoding as you go.
 * used with uuencode.
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef NO_BASE64
#include <netinet/in.h>
#include <resolv.h>
#endif

static int decode(char *);
__dead static void usage(void);
static int checkend(const char *, const char *, const char *);
static int base64_decode(void);

static int base64;
static const char *inputname;

int
main(int argc, char *argv[])
{
	int ch, rval;
	char *outputname = NULL;

	setlocale(LC_ALL, "");
	setprogname(argv[0]);

	while ((ch = getopt(argc, argv, "mo:p")) != -1)
		switch (ch) {
		case 'm':
			base64 = 1;
			break;
		case 'o':
			outputname = optarg;
			break;
		case 'p':
			outputname = __UNCONST("/dev/stdout");
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (*argv) {
		rval = 0;
		do {
			if (!freopen(inputname = *argv, "r", stdin)) {
				warn("%s", *argv);
				rval = 1;
				continue;
			}
			rval |= decode(outputname);
		} while (*++argv);
	} else {
		inputname = "stdin";
		rval = decode(outputname);
	}
	exit(rval);
}

/*
 * Decode one file from stdin.  If outputname is not NULL
 * then it overrides the file name embedded in the input data.
 */
static int
decode(char *outputname)
{
	struct passwd *pw;
	int n;
	char ch, *p;
	int n1;
	long mode;
	char *fn;
	char buf[MAXPATHLEN];

	/* search for header line */
	for (;;) {
		if (!fgets(buf, sizeof(buf), stdin)) {
			warnx("%s: no \"%s\" line", inputname, base64 ? 
					"begin-base64" : "begin");
			return(1);
		}
		p = buf;
		if (strncmp(p, "begin-base64", 12) == 0) {
			base64 = 1;
			p += 13;
			break;
		} else if (strncmp(p, "begin", 5) == 0)  {
			p += 6;
			break;
		} else
			continue;
	} 

        /* must be followed by an octal mode and a space */
	mode = strtol(p, &fn, 8);
	if (fn == (p) || !isspace((unsigned char)*fn) || mode==LONG_MIN || mode==LONG_MAX)
	{
	        warnx("%s: invalid mode on \"%s\" line", inputname,
			base64 ? "begin-base64" : "begin");
		return(1);
	}
	/* skip whitespace for file name */
	while (*fn && isspace((unsigned char)*fn)) fn++;
	if (*fn == 0) {
                warnx("%s: no filename on \"%s\" line", inputname,
			base64 ? "begin-base64" : "begin");
		return(1);
	}
	/* zap newline */
	for (p = fn; *p && *p != '\n'; p++) 
	        ;
	if (*p) *p = 0;

	/* outputname overrides fn */
	if (outputname)
		fn = outputname;
	
	/* handle ~user/file format */
	if (*fn == '~') {
		if (!(p = strchr(fn, '/'))) {
			warnx("%s: illegal ~user.", inputname);
			return(1);
		}
		*p++ = '\0';
		if (!(pw = getpwnam(fn + 1))) {
			warnx("%s: no user %s.", inputname, buf);
			return(1);
		}
		n = strlen(pw->pw_dir);
		n1 = strlen(p);
		if (n + n1 + 2 > MAXPATHLEN) {
			warnx("%s: path too long.", inputname);
			return(1);
		}
		/* make space at beginning of buf by moving end of pathname */
		memmove(buf + n + 1, p, n1 + 1);
		memmove(buf, pw->pw_dir, n);
		buf[n] = '/';
		fn = buf;
	}

	if (strcmp(fn, "/dev/stdout") == 0 || strcmp(fn, "-") == 0) {
		/*
		 * POSIX.1-2008 says that both "-" and "/dev/stdout"
		 * refer to standard output when they appear in the file
		 * header, but only "/dev/stdout" refers to standard
		 * output when it appears as the argument to the "-o"
		 * command line option.
		 *
		 * We handle both special names, regardless of whether
		 * they came from the "-o" option or from the header of
		 * the input stream.
		 */
	} else {
		/*
		 * Create output file, and set its mode.  POSIX.1-2008
		 * requires the mode to be used exactly, ignoring the
		 * umask and anything else, but we mask it with 0666.
		 */
		if (freopen(fn, "w", stdout) == NULL ||
		    fchmod(fileno(stdout), mode & 0666) != 0) { 
			warn("%s: %s", fn, inputname);
			return(1);
		}
	}

	if (base64)
		return base64_decode();
	else {
		/* for each input line */
		for (;;) {
			if (!fgets(p = buf, sizeof(buf), stdin)) {
				warnx("%s: short file.", inputname);
				return(1);
			}
#define	DEC(c)	(((c) - ' ') & 077)		/* single character decode */
			/*
		 	* `n' is used to avoid writing out all the characters
		 	* at the end of the file.
		 	*/
			if ((n = DEC(*p)) <= 0)
				break;
			for (++p; n > 0; p += 4, n -= 3)
				if (n >= 3) {
					ch = DEC(p[0]) << 2 | DEC(p[1]) >> 4;
					putchar(ch);
					ch = DEC(p[1]) << 4 | DEC(p[2]) >> 2;
					putchar(ch);
					ch = DEC(p[2]) << 6 | DEC(p[3]);
					putchar(ch);
				}
				else {
					if (n >= 1) {
						ch = DEC(p[0]) << 2 | DEC(p[1]) >> 4;
						putchar(ch);
					}
					if (n >= 2) {
						ch = DEC(p[1]) << 4 | DEC(p[2]) >> 2;
						putchar(ch);
					}
					if (n >= 3) {
						ch = DEC(p[2]) << 6 | DEC(p[3]);
						putchar(ch);
					}
				}
		}
		if (!fgets(buf, sizeof(buf), stdin) || strcmp(buf, "end\n")) {
			warnx("%s: no \"end\" line.", inputname);
			return(1);
		}
		return(0);
	}
}

static int
checkend(const char *ptr, const char *end, const char *msg)
{
	size_t n;

	n = strlen(end);
	if (strncmp(ptr, end, n) != 0 ||
	    strspn(ptr + n, "\t\r\n") != strlen(ptr + n)) {
		warnx("%s", msg); 
		return (1);
	}
	return (0);
}

static int
base64_decode(void)
{
	int n;
	char inbuf[MAXPATHLEN];
	unsigned char outbuf[MAXPATHLEN * 4];

	for (;;) {
		if (!fgets(inbuf, sizeof(inbuf), stdin)) {
			warnx("%s: short file.", inputname);
			return (1);
		}
#ifdef NO_BASE64
		warnx("%s: base64 decoding is not supported", inputname);
		return (1);
#else
		n = b64_pton(inbuf, outbuf, sizeof(outbuf));
#endif
		if (n < 0)
			break;
		fwrite(outbuf, 1, n, stdout);
	}
	return (checkend(inbuf, "====",
			"error decoding base64 input stream"));
}

static void
usage(void)
{
	(void)fprintf(stderr,
		      "usage: %s [-m] [-p | -o outfile] [encoded-file ...]\n",
		      getprogname());
	exit(1);
}

/*	$NetBSD: uuencode.c,v 1.16 2014/09/06 18:58:35 dholland Exp $	*/

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

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1983, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)uuencode.c	8.2 (Berkeley) 4/2/94";
#else
__RCSID("$NetBSD: uuencode.c,v 1.16 2014/09/06 18:58:35 dholland Exp $");
#endif
#endif /* not lint */

/*
 * uuencode [input] output
 *
 * Encode a file so it can be mailed to a remote system.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <err.h>
#include <errno.h>
#include <locale.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void encode(void);
static void base64_encode(void);
__dead static void usage(void);

int
main(int argc, char *argv[])
{
	struct stat sb;
	int base64, ch, mode;

	mode = 0;
	base64 = 0;
	setlocale(LC_ALL, "");
	setprogname(argv[0]);

	while ((ch = getopt(argc, argv, "m")) != -1) {
		switch(ch) {
		case 'm':
			base64 = 1;
			break;
		default:
			usage();
		}
	}
	argv += optind;
	argc -= optind;

	switch(argc) {
	case 2:			/* optional first argument is input file */
		if (!freopen(*argv, "r", stdin) || fstat(fileno(stdin), &sb))
			err(1, "%s", *argv);
#define	RWX	(S_IRWXU|S_IRWXG|S_IRWXO)
		mode = sb.st_mode & RWX;
		++argv;
		break;
	case 1:
#define	RW	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
		mode = RW & ~umask(RW);
		break;
	case 0:
	default:
		usage();
	}

	if (base64) {
		(void)printf("begin-base64 %o %s\n", mode, *argv);
		base64_encode();
		(void)printf("====\n");
	} else {
		(void)printf("begin %o %s\n", mode, *argv);
		encode();
		(void)printf("end\n");
	}

	if (ferror(stdout))
		err(1, "write error");
	exit(0);
}

/* ENC is the basic 1 character encoding function to make a char printing */
#define	ENC(c) ((c) ? ((c) & 077) + ' ': '`')

/*
 * copy from in to out, encoding in base64 as you go along.
 */
static void
base64_encode(void)
{
	/*
	 * Output must fit into 80 columns, chunks come in 4, leave 1.
	 */
#define GROUPS 	((70 / 4) - 1)
	unsigned char buf[3];
	char buf2[sizeof(buf) * 2 + 1];
	size_t n;
	int rv, sequence;

	sequence = 0;

	while ((n = fread(buf, 1, sizeof(buf), stdin))) {
		++sequence;
		rv = b64_ntop(buf, n, buf2, (sizeof(buf2) / sizeof(buf2[0])));
		if (rv == -1)
			errx(1, "b64_ntop: error encoding base64");
		printf("%s%s", buf2, (sequence % GROUPS) ? "" : "\n");
	}
	if (sequence % GROUPS)
		printf("\n");
}

/*
 * copy from in to out, encoding as you go along.
 */
static void
encode(void)
{
	int ch, n;
	char *p;
	char buf[80];

	while ((n = fread(buf, 1, 45, stdin)) > 0) {
		ch = ENC(n);
		if (putchar(ch) == EOF)
			break;
		for (p = buf; n > 0; n -= 3, p += 3) {
			ch = *p >> 2;
			ch = ENC(ch);
			if (putchar(ch) == EOF)
				break;
			ch = ((*p << 4) & 060) | ((p[1] >> 4) & 017);
			ch = ENC(ch);
			if (putchar(ch) == EOF)
				break;
			ch = ((p[1] << 2) & 074) | ((p[2] >> 6) & 03);
			ch = ENC(ch);
			if (putchar(ch) == EOF)
				break;
			ch = p[2] & 077;
			ch = ENC(ch);
			if (putchar(ch) == EOF)
				break;
		}
		if (putchar('\n') == EOF)
			break;
	}
	if (ferror(stdin))
		err(1, "read error.");
	ch = ENC('\0');
	(void)putchar(ch);
	(void)putchar('\n');
}

static void
usage(void)
{
	(void)fprintf(stderr,
		      "usage: %s [-m] [inputfile] headername > encodedfile\n",
		      getprogname());
	exit(1);
}

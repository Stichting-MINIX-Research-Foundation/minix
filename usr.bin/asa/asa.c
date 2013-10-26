/*	$NetBSD: asa.c,v 1.16 2007/06/24 23:23:10 christos Exp $	*/

/*
 * Copyright (c) 1993,94 Winning Strategies, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Winning Strategies, Inc.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: asa.c,v 1.16 2007/06/24 23:23:10 christos Exp $");
#endif

#include <err.h>
#include <stdio.h>
#include <stdlib.h>

static void asa(FILE *);
int main(int, char *[]);

int
/*ARGSUSED*/
main (int argc, char *argv[])
{
	FILE *fp;

	/* skip progname */
	argv++;

	if (*argv == NULL)
		asa(stdin);
	else
        	do {
			if ((fp = fopen(*argv, "r")) == NULL) {
				warn("%s", *argv);
				continue;
			}
			asa(fp);
			(void)fclose(fp);
        	} while (*++argv != NULL);

	return 0;
}

static void
asa(FILE *f)
{
	char *buf;
	size_t len;

	if ((buf = fgetln(f, &len)) != NULL) {
		if (len > 0 && buf[len - 1] == '\n')
			buf[--len] = '\0';
		/* special case the first line */
		switch (buf[0]) {
		case '0':
			(void)putchar('\n');
			break;
		case '1':
			(void)putchar('\f');
			break;
		}

		if (len > 1 && buf[0] && buf[1])
			(void)fwrite(buf + 1, 1, len - 1, stdout);

		while ((buf = fgetln(f, &len)) != NULL) {
			if (len > 0 && buf[len - 1] == '\n')
				buf[--len] = '\0';
			switch (buf[0]) {
			default:
			case ' ':
				(void)putchar('\n');
				break;
			case '0':
				(void)putchar('\n');
				(void)putchar('\n');
				break;
			case '1':
				(void)putchar('\f');
				break;
			case '+':
				(void)putchar('\r');
				break;
			}

			if (len > 1 && buf[0] && buf[1])
				(void)fwrite(buf + 1, 1, len - 1, stdout);
		}

		(void)putchar('\n');
	}
}

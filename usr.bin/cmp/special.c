/*	$NetBSD: special.c,v 1.14 2011/11/28 10:10:10 wiz Exp $	*/

/*-
 * Copyright (c) 1991, 1993, 1994
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
#if 0
static char sccsid[] = "@(#)special.c	8.3 (Berkeley) 4/2/94";
#else
__RCSID("$NetBSD: special.c,v 1.14 2011/11/28 10:10:10 wiz Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>

#include <err.h>
#include <stdlib.h>
#include <stdio.h>

#include "extern.h"

void
c_special(int fd1, const char *file1, off_t skip1, int fd2, const char *file2, off_t skip2)
{
	int ch1, ch2;
	off_t byte, line;
	FILE *fp1, *fp2;
	int dfound;

	dfound = 0;
	if ((fp1 = fdopen(fd1, "r")) == NULL)
		err(ERR_EXIT, "%s", file1);
	if ((fp2 = fdopen(fd2, "r")) == NULL)
		err(ERR_EXIT, "%s", file2);

	for (byte = line = 1; skip1--; byte++) {
		ch1 = getc(fp1);
		if (ch1 == EOF)
			goto eof;
		if (ch1 == '\n')
			line++;
	}
	for (byte = line = 1; skip2--; byte++) {
		ch2 = getc(fp2);
		if (ch2 == EOF)
			goto eof;
		if (ch2 == '\n')
			line++;
	}
	dfound = 0;
	for (byte = line = 1;; ++byte) {
		ch1 = getc(fp1);
		ch2 = getc(fp2);
		if (ch1 == EOF || ch2 == EOF)
			break;
		if (ch1 != ch2) {
			if (lflag) {
				dfound = 1;
				(void)printf("%6lld %3o %3o\n", (long long)byte,
				    ch1, ch2);
			} else
				diffmsg(file1, file2, byte, line);
				/* NOTREACHED */
		}
		if (ch1 == '\n')
			++line;
	}

 eof:
	if (ferror(fp1))
		errmsg(file1, byte, line);
	if (ferror(fp2))
		errmsg(file2, byte, line);
	if (feof(fp1)) {
		if (!feof(fp2))
			eofmsg(file1, byte, line);
	} else
		if (feof(fp2))
			eofmsg(file2, byte, line);
	(void)fclose(fp1);
	(void)fclose(fp2);
	if (dfound)
		exit(DIFF_EXIT);
}

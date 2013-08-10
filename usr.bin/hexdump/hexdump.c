/*	$NetBSD: hexdump.c,v 1.18 2012/07/06 09:06:43 wiz Exp $	*/

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
__COPYRIGHT("@(#) Copyright (c) 1989, 1993\
 The Regents of the University of California.  All rights reserved.");
#if 0
static char sccsid[] = "@(#)hexdump.c	8.1 (Berkeley) 6/6/93";
#else
__RCSID("$NetBSD: hexdump.c,v 1.18 2012/07/06 09:06:43 wiz Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hexdump.h"

FS *fshead;				/* head of format strings */
int blocksize;				/* data block size */
int exitval;				/* final exit value */
int length = -1;			/* max bytes to read */
static int isod = 0;

int
main(int argc, char *argv[])
{
	FS *tfs;
	char *p;

	setlocale(LC_ALL, "");

	isod = 0;
	p = strrchr(argv[0], 'o');
	if (p != NULL && strcmp(p, "od") == 0)
		isod = 1;

	if (isod)
		odsyntax(argc, &argv);
	else
		hexsyntax(argc, &argv);

	/* figure out the data block size */
	for (blocksize = 0, tfs = fshead; tfs; tfs = tfs->nextfs) {
		tfs->bcnt = size(tfs);
		if (blocksize < tfs->bcnt)
			blocksize = tfs->bcnt;
	}
	/* rewrite the rules, do syntax checking */
	for (tfs = fshead; tfs; tfs = tfs->nextfs)
		rewrite(tfs);

	(void)next(argv);
	display();
	exit(exitval);
}

void
usage(void)
{
	const char *pname = getprogname();

	(void)fprintf(stderr, "usage: %s ", pname);
	if (isod)
		(void)fprintf(stderr, "[-aBbcDdeFfHhIiLlOovXx] [-A base] "
		    "[-j skip] [-N length] [-t type_string] [[+]offset[.][Bb]] "
		    "[file ...]\n");
	else
		(void)fprintf(stderr, "[-bCcdovx] [-e format_string] [-f format_file] "
		    "[-n length] [-s skip] [file ...]\n");
	exit(1);
}

/*	$NetBSD: banner.c,v 1.15 2012/02/29 08:55:25 wiz Exp $	*/

/*
 *	Changes for banner(1)
 *
 *      @(#)Copyright (c) 1995, Simon J. Gerraty.
 *      
 *      This is free software.  It comes with NO WARRANTY.
 *      Permission to use, modify and distribute this source code 
 *      is granted subject to the following conditions.
 *      1/ that the above copyright notice and this notice 
 *      are preserved in all copies and that due credit be given 
 *      to the author.  
 *      2/ that any changes to this code are clearly commented 
 *      as such so that the author does not get blamed for bugs 
 *      other than his own.
 *      
 *      Please send copies of changes and bug-fixes to:
 *      sjg@zen.void.oz.au
 */

/*
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
static char sccsid[] = "@(#)printjob.c	8.2 (Berkeley) 4/16/94";
#else
__RCSID("$NetBSD: banner.c,v 1.15 2012/02/29 08:55:25 wiz Exp $");
#endif
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "banner.h"

static long PW = LINELEN;
/*
 * <sjg> lpd makes chars out of the letter in question.
 * the results are somewhat mixed.  Sticking to '#' as
 * banner(1) does is more consistent.
 */
static int ForeGnd = '#';
static int BackGnd = ' ';
static int Drop = 0;				/* 3 for the LPD font */

static	int	dropit (int);
static	void	scan_out (int, char *, int);
static	char   *scnline (int, char *, int);
__dead static	void	usage(void);

/* the char gen code below is lifted from lpd */

static char *
scnline(int key, char *p, int c)
{
	int scnwidth;

	if (ForeGnd)
	    c = ForeGnd;
	
	for (scnwidth = WIDTH; --scnwidth;) {
		key <<= 1;
		*p++ = key & 0200 ? c : BackGnd;
	}
	return (p);
}

#define TRC(q)	(((q)-' ')&0177)


static int
dropit(int c)
{
	switch(c) {

	case TRC('_'):
	case TRC(';'):
	case TRC(','):
	case TRC('g'):
	case TRC('j'):
	case TRC('p'):
	case TRC('q'):
	case TRC('y'):
		return (Drop);

	default:
		return (0);
	}
}

static void
scan_out(int scfd, char *scsp, int dlm)
{
	char *strp;
	int nchrs, j;
	char outbuf[LINELEN+1], *sp, c, cc;
	int d, scnhgt;

	for (scnhgt = 0; scnhgt++ < HEIGHT+Drop; ) {
		strp = &outbuf[0];
		if (BackGnd != ' ')
		    *strp++ = BackGnd;
		sp = scsp;
		for (nchrs = 0; *sp != dlm && *sp != '\0'; ) {
			cc = *sp++;
			if(cc < ' ' || ((int)cc) >= 0x7f)
				cc = ' ';
			d = dropit(c = TRC(cc));
			if ((!d && scnhgt > HEIGHT) || (scnhgt <= Drop && d))
				for (j = WIDTH; --j;)
					*strp++ = BackGnd;
			else if (Drop == 0)
				strp = scnline(
				    scnkey_def[(int)c][scnhgt-1-d], strp, cc);
			else
				strp = scnline(
				    scnkey_lpd[(int)c][scnhgt-1-d], strp, cc);
			if (nchrs++ >= PW/(WIDTH+1)-1)
				break;
			*strp++ = BackGnd;
		}
		if (BackGnd != ' ')
		    *strp++ = BackGnd;
		else {
		    while (*--strp == ' ' && strp >= outbuf)
			;
		    strp++;
		}
		*strp++ = '\n';	
		(void) write(scfd, outbuf, strp-outbuf);
	}
}

/*
 * for each word, print up to 10 chars in big letters.
 */
int
main(int argc, char **argv)
{
	char word[10+1];		/* strings limited to 10 chars */
	int c;

	while ((c = getopt(argc, argv, "b:f:l")) != -1) {
	    switch (c) {
	    case 'f':
		if (*optarg == '-')
		    ForeGnd = 0;
		else
		    ForeGnd = *optarg;
		break;
	    case 'b':
		BackGnd = *optarg;
		break;
	    case 'l':
		Drop = 3;			/* for LPD font */
		break;
	    default:
		usage();
	    }
	}

	for (; optind < argc; ++optind) {
		(void)strlcpy(word, argv[optind], sizeof (word));
		scan_out(STDOUT_FILENO, word, '\0');
	}
	exit(0);
}

static void
usage(void)
{
    fprintf(stderr, "usage: %s [-l] [-b bg] [-f fg] string ...\n",
	getprogname());
    exit(1);
}

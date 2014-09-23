/*	$NetBSD: snscore.c,v 1.19 2012/06/19 05:46:09 dholland Exp $	*/

/*
 * Copyright (c) 1980, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1980, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)snscore.c	8.1 (Berkeley) 7/19/93";
#else
__RCSID("$NetBSD: snscore.c,v 1.19 2012/06/19 05:46:09 dholland Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <err.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "pathnames.h"

static const char *recfile = _PATH_RAWSCORES;
#define MAXPLAYERS 256

struct	player	{
	short	uids;
	short	scores;
	char	*name;
};

static struct player players[MAXPLAYERS], temp;

int	main(void);

int
main(void)
{
	short	uid, score;
	FILE	*fd;
	int	noplayers;
	int	i, j, notsorted;
	short	whoallbest, allbest;
	const	char *q;
	struct	passwd	*p;

	/* Revoke setgid privileges */
	setgid(getgid());

	fd = fopen(recfile, "r");
	if (fd == NULL)
		err(1, "opening `%s'", recfile);
	printf("Snake players scores to date\n");
	if (fread(&whoallbest, sizeof(short), 1, fd) == 0) {
		printf("No scores recorded yet!\n");
		exit(0);
	}
	fread(&allbest, sizeof(short), 1, fd);
	noplayers = 0;
	for (uid = 2; ;uid++) {
		if(fread(&score, sizeof(short), 1, fd) == 0)
			break;
		if (score > 0) {
			if (noplayers >= MAXPLAYERS) {
				printf("too many players\n");
				exit(2);
			}
			players[noplayers].uids = uid;
			players[noplayers].scores = score;
			p = getpwuid(uid);
			if (p == NULL)
				continue;
			q = p -> pw_name;
			players[noplayers].name = strdup(q);
			if (players[noplayers].name == NULL)
				err(1, NULL);
			noplayers++;
		}
	}

	/* bubble sort scores */
	for (notsorted = 1; notsorted; ) {
		notsorted = 0;
		for (i = 0; i < noplayers - 1; i++)
			if (players[i].scores < players[i + 1].scores) {
				temp = players[i];
				players[i] = players[i + 1];
				players[i + 1] = temp;
				notsorted++;
			}
	}

	j = 1;
	for (i = 0; i < noplayers; i++) {
		printf("%d:\t$%d\t%s\n", j, players[i].scores, players[i].name);
		if (players[i].scores > players[i + 1].scores)
			j = i + 2;
	}
	exit(0);
}

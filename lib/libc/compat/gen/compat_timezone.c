/*	$NetBSD: compat_timezone.c,v 1.4 2012/03/20 17:05:59 matt Exp $	*/

/*
 * Copyright (c) 1987, 1993
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
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)timezone.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: compat_timezone.c,v 1.4 2012/03/20 17:05:59 matt Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#define __LIBC12_SOURCE__

#include "namespace.h"
#include <sys/types.h>
#include <time.h>
#include <compat/include/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tzfile.h>

__warn_references(timezone,
     "warning: reference to compatibility timezone; include <time.h> to generate correct reference")

/*
 * timezone --
 *	The arguments are the number of minutes of time you are westward
 *	from Greenwich and whether DST is in effect.  It returns a string
 *	giving the name of the local timezone.  Should be replaced, in the
 *	application code, by a call to localtime.
 */

char *_tztab(int, int);

static char	czone[TZ_MAX_CHARS];		/* space for zone name */

char *timezone(int, int);
char *
timezone(int zone, int dst)
{
	char	*beg,
			*end;

	if ((beg = getenv("TZNAME")) != NULL) {	/* set in environment */
		if ((end = strchr(beg, ',')) != NULL) {	/* "PST,PDT" */
			if (dst)
				return(++end);
			*end = '\0';
			(void)strlcpy(czone, beg, sizeof(czone));
			*end = ',';
			return(czone);
		}
		return(beg);
	}
	return(_tztab(zone,dst));	/* default: table or created zone */
}

static const struct zone {
	int		offset;
	const char	*stdzone;
	const char	*dlzone;
} zonetab[] = {
	{ -1*60,	"MET",	"MET DST" },	/* Middle European */
	{ -2*60,	"EET",	"EET DST" },	/* Eastern European */
	{ 4*60,		"AST",	"ADT" },	/* Atlantic */
	{ 5*60,		"EST",	"EDT" },	/* Eastern */
	{ 6*60,		"CST",	"CDT" },	/* Central */
	{ 7*60,		"MST",	"MDT" },	/* Mountain */
	{ 8*60,		"PST",	"PDT" },	/* Pacific */
#ifdef notdef
	/* there's no way to distinguish this from WET */
	{ 0,		"GMT",	0 },		/* Greenwich */
#endif
	{ 0*60,		"WET",	"WET DST" },	/* Western European */
	{ -10*60,	"EST",	"EST" },	/* Aust: Eastern */
     	{ -10*60+30,	"CST",	"CST" },	/* Aust: Central */
 	{ -8*60,	"WST",	0 },		/* Aust: Western */
	{ -1,		NULL,	NULL }
};

/*
 * _tztab --
 *	check static tables or create a new zone name; broken out so that
 *	we can make a guess as to what the zone is if the standard tables
 *	aren't in place in /etc.  DO NOT USE THIS ROUTINE OUTSIDE OF THE
 *	STANDARD LIBRARY.
 */
char *
_tztab(int zone, int dst)
{
	const struct zone	*zp;
	char	sign;

	for (zp = zonetab; zp->offset != -1;++zp)	/* static tables */
		if (zp->offset == zone) {
			if (dst && zp->dlzone)
				return __UNCONST(zp->dlzone);
			if (!dst && zp->stdzone)
				return __UNCONST(zp->stdzone);
		}

	if (zone < 0) {					/* create one */
		zone = -zone;
		sign = '+';
	}
	else
		sign = '-';
	(void)snprintf(czone, TZ_MAX_CHARS, "GMT%c%d:%02d", sign, zone / 60,
	     zone % 60);
	return(czone);
}

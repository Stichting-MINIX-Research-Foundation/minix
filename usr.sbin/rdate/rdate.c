/*	$NetBSD: rdate.c,v 1.19 2009/10/21 01:07:47 snj Exp $	*/

/*
 * Copyright (c) 1994 Christos Zoulas
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

/*
 * rdate.c: Set the date from the specified host
 * 
 * 	Uses the rfc868 time protocol at socket 37.
 *	Time is returned as the number of seconds since
 *	midnight January 1st 1900.
 */
#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: rdate.c,v 1.19 2009/10/21 01:07:47 snj Exp $");
#endif /* lint */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>

#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

/* seconds from midnight Jan 1900 - 1970 */
#define DIFFERENCE 2208988800ULL

	int	main(int, char **);
static	void	usage(void);

static void
usage(void)
{
	(void) fprintf(stderr, "usage: %s [-psa] host\n", getprogname());
	(void) fprintf(stderr, "  -p: just print, don't set\n");
	(void) fprintf(stderr, "  -s: just set, don't print\n");
	(void) fprintf(stderr, "  -a: use adjtime instead of instant change\n");
}

int
main(int argc, char *argv[])
{
	int             pr = 0, silent = 0, s;
	int		slidetime = 0;
	int		adjustment;
	uint32_t	data;
	time_t          tim;
	char           *hname;
	const char     *emsg = NULL;
	struct addrinfo	hints, *res, *res0;
	int             c;
	int		error;

	adjustment = 0;
	while ((c = getopt(argc, argv, "psa")) != -1)
		switch (c) {
		case 'p':
			pr++;
			break;

		case 's':
			silent++;
			break;

		case 'a':
			slidetime++;
			break;

		default:
			usage();
			return 1;
		}

	if (argc - 1 != optind) {
		usage();
		return 1;
	}
	hname = argv[optind];

	memset(&hints, 0, sizeof (hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_CANONNAME;
	error = getaddrinfo(hname, "time", &hints, &res0);
	if (error)
		errx(1, "%s: %s", gai_strerror(error), hname);

	for (res = res0, s = -1; res != NULL; res = res->ai_next) {
		s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (s < 0) {
			emsg = "socket";
			continue;
		}

		if (connect(s, res->ai_addr, res->ai_addrlen)) {
			close(s);
			s = -1;
			emsg = "connect";
			continue;
		}
		
		break;
	}
	if (s < 0)
		err(1, "%s", emsg);

	if (read(s, &data, sizeof(uint32_t)) != sizeof(uint32_t))
		err(1, "Could not read data");

	(void) close(s);
	tim = ntohl(data) - DIFFERENCE;

	if (!pr) {
	    struct timeval  tv;
	    if (!slidetime) {
		    logwtmp("|", "date", "");
		    tv.tv_sec = tim;
		    tv.tv_usec = 0;
		    if (settimeofday(&tv, NULL) == -1)
			    err(1, "Could not set time of day");
		    logwtmp("{", "date", "");
	    } else {
		    struct timeval tv_current;
		    if (gettimeofday(&tv_current, NULL) == -1)
			    err(1, "Could not get local time of day");
		    adjustment = tv.tv_sec = tim - tv_current.tv_sec;
		    tv.tv_usec = 0;
		    if (adjtime(&tv, NULL) == -1)
			    err(1, "Could not adjust time of day");
	    }
	}

	if (!silent) {
		(void) fputs(ctime(&tim), stdout);
		if (slidetime)
		    (void) fprintf(stdout, 
				   "%s: adjust local clock by %d seconds\n",
				   getprogname(), adjustment);
	}
	return 0;
}

/* $NetBSD: hostname.c,v 1.20 2013/07/19 15:53:00 christos Exp $ */

/*
 * Copyright (c) 1988, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1988, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)hostname.c	8.2 (Berkeley) 4/28/95";
#else
__RCSID("$NetBSD: hostname.c,v 1.20 2013/07/19 15:53:00 christos Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in.h>

#include <err.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

__dead static void usage(void);

int
main(int argc, char *argv[])
{
	int ch, Aflag, aflag, dflag, Iflag, iflag, fflag, sflag, i;
	char *p, hostname[MAXHOSTNAMELEN + 1];
	struct addrinfo hints, *ainfos, *ai;
	struct hostent *hent;
	struct ifaddrs *ifa, *ifp;
	struct sockaddr_in6 *sin6;
	char buf[MAX(MAXHOSTNAMELEN + 1, INET6_ADDRSTRLEN)];

	setprogname(argv[0]);
	Aflag = aflag = dflag = Iflag = iflag = fflag = sflag = 0;
	while ((ch = getopt(argc, argv, "AadIifs")) != -1)
		switch (ch) {
		case 'A':
			Aflag = 1;
			break;
		case 'a':
			aflag = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'I':
			Iflag = 1;
			break;
		case 'i':
			iflag = 1;
			break;
		case 'f':
			fflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc > 1)
		usage();

	if (*argv) {
		if (sethostname(*argv, strlen(*argv)))
			err(1, "sethostname");
	} else if (Aflag || Iflag) {
		if (getifaddrs(&ifa) == -1)
			err(1, "getifaddrs");
		for (ifp = ifa; ifp; ifp = ifp->ifa_next) {
			if (ifp->ifa_addr == NULL ||
#if !defined(__minix)
			    ifp->ifa_flags & IFF_LOOPBACK ||
#endif
			    !(ifp->ifa_flags & IFF_UP))
				continue;

			switch(ifp->ifa_addr->sa_family) {
			case AF_INET:
				break;
			case AF_INET6:
				/* Skip link local addresses */
				sin6 = (struct sockaddr_in6 *)ifp->ifa_addr;
				if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr) ||
				    IN6_IS_ADDR_MC_LINKLOCAL(&sin6->sin6_addr))
					continue;
				break;
			default:
				/* We only translate IPv4 or IPv6 addresses */
				continue;
			}
			i = getnameinfo(ifp->ifa_addr, ifp->ifa_addr->sa_len,
			    buf, sizeof(buf), NULL, 0,
			    Iflag ? NI_NUMERICHOST: NI_NAMEREQD);
			if (i) {
				if (Iflag && i != EAI_NONAME)
					errx(1, "getnameinfo: %s",
					    gai_strerror(i));
			} else
				printf("%s\n", buf);
		}
		freeifaddrs(ifa);
	} else {
		if (gethostname(hostname, sizeof(hostname)))
			err(1, "gethostname");
		hostname[sizeof(hostname) - 1] = '\0';
		if (aflag) {
			if ((hent = gethostbyname(hostname)) == NULL)
				errx(1, "gethostbyname: %s",
				    hstrerror(h_errno));
			for (i = 0; hent->h_aliases[i]; i++)
				printf("%s\n", hent->h_aliases[i]);
		} else if (dflag || iflag || fflag) {
			memset(&hints, 0, sizeof(hints));
			hints.ai_family = AF_UNSPEC;
			hints.ai_socktype = SOCK_DGRAM;
			hints.ai_flags = AI_CANONNAME;
			i = getaddrinfo(hostname, NULL, &hints, &ainfos);
			if (i)
				errx(1, "getaddrinfo: %s", gai_strerror(i));
			if (ainfos) {
				if (dflag) {
					if ((p = strchr(ainfos->ai_canonname,
					    '.')))
						printf("%s\n", p + 1);
				} else if (iflag) {
					for (ai = ainfos; ai; ai = ai->ai_next)
					{
						i = getnameinfo(ai->ai_addr,
						    ai->ai_addrlen,
						    buf, sizeof(buf), NULL, 0,
						    NI_NUMERICHOST);
						if (i)
							errx(1,
							    "getnameinfo: %s",
							    gai_strerror(i));
						printf("%s\n", buf);
					}
				} else {
					if (sflag &&
					    (p = strchr(ainfos->ai_canonname,
					    '.')))
						*p = '\0';
					printf("%s\n", ainfos->ai_canonname);
				}
				freeaddrinfo(ainfos);
			}
		} else {
			if (sflag && (p = strchr(hostname, '.')))
				*p = '\0';
			printf("%s\n", hostname);
		}
	}
	exit(0);
	/* NOTREACHED */
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage: %s [-AadfIis] [name-of-host]\n",
	    getprogname());
	exit(1);
	/* NOTREACHED */
}

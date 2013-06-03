/*	$NetBSD: ifaddrlist.c,v 1.10 2011/09/11 01:06:26 christos Exp $	*/

/*
 * Copyright (c) 1997, 1998, 1999, 2000
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
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
static const char rcsid[] =
    "@(#) Header: ifaddrlist.c,v 1.2 97/04/22 13:31:05 leres Exp  (LBL)";
    "@(#) Id: ifaddrlist.c,v 1.9 2000/11/23 20:01:55 leres Exp  (LBL)";
#else
__RCSID("$NetBSD: ifaddrlist.c,v 1.10 2011/09/11 01:06:26 christos Exp $");
#endif
#endif

#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif
#include <sys/time.h>				/* concession to AIX */

struct mbuf;
struct rtentry;

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ifaddrs.h>

#include "gnuc.h"
#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

#include "ifaddrlist.h"

/* Not all systems have IFF_LOOPBACK */
#ifdef IFF_LOOPBACK
#define ISLOOPBACK(p) ((p)->ifa_flags & IFF_LOOPBACK)
#else
#define ISLOOPBACK(p) (strcmp((p)->ifa_name, "lo0") == 0)
#endif

/*
 * Return the interface list
 */
ssize_t
ifaddrlist(struct ifaddrlist **ipaddrp, char *errbuf, size_t buflen)
{
	struct sockaddr_in *sin;
	struct ifaddrs *ifap = NULL, *ifa;
	struct ifaddrlist *al = NULL, *nal;
	size_t i = 0, maxal = 10;

	if (getifaddrs(&ifap) != 0)
		goto out;

	if ((al = malloc(maxal * sizeof(*al))) == NULL)
		goto out;
		
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;

		/* Must be up */
		if ((ifa->ifa_flags & IFF_UP) == 0)
			continue;

		/*
		 * Must not be a loopback address (127/8)
		 */
		sin = (struct sockaddr_in *)ifa->ifa_addr;
		if (ISLOOPBACK(ifa))
			if (ntohl(sin->sin_addr.s_addr) == INADDR_LOOPBACK)
				continue;

		if (i == maxal) {
			maxal <<= 1;
			if ((nal = realloc(al, maxal * sizeof(*al))) == NULL)
				goto out;
			al = nal;
		}

		al[i].addr = sin->sin_addr.s_addr;
		if ((al[i].device = strdup(ifa->ifa_name)) == NULL)
			goto out;
		i++;
	}
	if ((nal = realloc(al, i * sizeof(*al))) == NULL)
		goto out;
	freeifaddrs(ifap);
	*ipaddrp = nal;
	return (ssize_t)i;
out:
	if (ifap)
		freeifaddrs(ifap);
	if (al) {
		while (i > 0)
			free(al[--i].device);
		free(al);
	}
	(void)snprintf(errbuf, buflen, "%s: %s", __func__, strerror(errno));
	return -1;
}

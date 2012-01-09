/*	$NetBSD: udp.c,v 1.11 2011/05/11 16:23:40 zoltan Exp $	*/

/*
 * Copyright (c) 1992 Regents of the University of California.
 * All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
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
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 * @(#) Header: net.c,v 1.9 93/08/06 19:32:15 leres Exp  (LBL)
 */

#include <sys/param.h>
#include <sys/socket.h>

#ifdef _STANDALONE
#include <lib/libkern/libkern.h>
#else
#include <string.h>
#endif

#include <net/if.h>
#include <net/if_ether.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include "stand.h"
#include "net.h"


/* Caller must leave room for ethernet, ip and udp headers in front!! */
ssize_t
sendudp(struct iodesc *d, void *pkt, size_t len)
{
	ssize_t cc;
	struct udphdr *uh;

#ifdef NET_DEBUG
 	if (debug) {
		printf("sendudp: d=%lx called.\n", (long)d);
		if (d) {
			printf("saddr: %s:%d",
			    inet_ntoa(d->myip), ntohs(d->myport));
			printf(" daddr: %s:%d\n",
			    inet_ntoa(d->destip), ntohs(d->destport));
		}
	}
#endif

	uh = (struct udphdr *)pkt - 1;
	len += sizeof(*uh);

	(void)memset(uh, 0, sizeof(*uh));

	uh->uh_sport = d->myport;
	uh->uh_dport = d->destport;
	uh->uh_ulen = htons(len);

	cc = sendip(d, uh, len, IPPROTO_UDP);
	if (cc == -1)
		return -1;
	if ((size_t)cc != len)
		panic("sendudp: bad write (%zd != %zu)", cc, len);
	return (cc - sizeof(*uh));
}

/*
 * Receive a UDP packet and validate it is for us.
 * Caller leaves room for the headers (Ether, IP, UDP)
 */
ssize_t
readudp(struct iodesc *d, void *pkt, size_t len, saseconds_t tleft)
{
	ssize_t n;
	struct udphdr *uh;

	uh = (struct udphdr *)pkt - 1;
	n = readip(d, uh, len + sizeof(*uh), tleft, IPPROTO_UDP);
	if (n == -1 || (size_t)n < sizeof(*uh))
		return -1;

	if (uh->uh_dport != d->myport) {
#ifdef NET_DEBUG
		if (debug)
			printf("readudp: bad dport %d != %d\n",
				d->myport, ntohs(uh->uh_dport));
#endif
		return -1;
	}

	if (ntohs(uh->uh_ulen) < sizeof(*uh)) {
#ifdef NET_DEBUG
		if (debug)
			printf("readudp: bad udp len %d < %d\n",
				ntohs(uh->uh_ulen), (int)sizeof(*uh));
#endif
		return -1;
	}

	n -= sizeof(*uh);
	return n;
}

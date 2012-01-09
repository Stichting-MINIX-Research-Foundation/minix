/* $NetBSD: ip.c,v 1.2 2011/05/13 23:35:09 nakayama Exp $ */

/*
 * Copyright (c) 1992 Regents of the University of California.
 * Copyright (c) 2010 Zoltan Arnold NAGY
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
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_ether.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#ifdef _STANDALONE
#include <lib/libkern/libkern.h>
#else
#include <string.h>
#endif

#include "stand.h"
#include "net.h"

/*
 * sends an IP packet, if it's alredy constructed
*/
static ssize_t
_sendip(struct iodesc * d, struct ip * ip, size_t len)
{
	u_char *ea;

	if (ip->ip_dst.s_addr == INADDR_BROADCAST || ip->ip_src.s_addr == 0 ||
	    netmask == 0 || SAMENET(ip->ip_src, ip->ip_dst, netmask)) {
		ea = arpwhohas(d, ip->ip_dst);
	} else {
		ea = arpwhohas(d, gateip);
	}

	return sendether(d, ip, len, ea, ETHERTYPE_IP);
}

/*
 * fills out the IP header
 * Caller must leave room for ethernet, ip and udp headers in front!
*/
ssize_t
sendip(struct iodesc * d, void *pkt, size_t len, u_int8_t proto)
{
	ssize_t cc;
	struct ip *ip;

	ip = (struct ip *) pkt - 1;
	len += sizeof(*ip);

	(void) memset(ip, 0, sizeof(*ip));

	ip->ip_v = IPVERSION;
	ip->ip_hl = sizeof(*ip) >> 2;
	ip->ip_len = htons(len);
	ip->ip_p = proto;
	ip->ip_ttl = IPDEFTTL;
	ip->ip_src = d->myip;
	ip->ip_dst = d->destip;
	ip->ip_sum = ip_cksum(ip, sizeof(*ip));

	cc = _sendip(d, ip, len);

	if (cc == -1)
		return -1;
	if ((size_t) cc != len)
		panic("sendip: bad write (%zd != %zu)", cc, len);
	return (cc - (sizeof(*ip)));
}

/*
 * reads an IP packet
 * WARNING: the old version stripped the IP options, if there were
 * any. Because we have absolutely no idea if the upper layer needs
 * these or not, it's best to leave them there.
 *
 * The size returned is the size indicated in the header.
 */
ssize_t
readip(struct iodesc * d, void *pkt, size_t len, time_t tleft, u_int8_t proto)
{
	ssize_t n;
	size_t hlen;
	struct ip *ip;
	u_int16_t etype;

	ip = (struct ip *) pkt - 1;

	n = readether(d, ip, len + sizeof(*ip), tleft, &etype);
	if (n == -1 || (size_t) n < sizeof(*ip))
		return -1;

	if (etype == ETHERTYPE_ARP) {
		struct arphdr *ah = (void *) ip;
		if (ah->ar_op == htons(ARPOP_REQUEST)) {
			/* Send ARP reply */
			arp_reply(d, ah);
		}
		return -1;
	}

	if (etype != ETHERTYPE_IP) {
#ifdef NET_DEBUG
		if (debug)
			printf("readip: not IP. ether_type=%x\n", etype);
#endif
		return -1;
	}

	/* Check ip header */
	if (ip->ip_v != IPVERSION ||
	    ip->ip_p != proto) { /* half char */
#ifdef NET_DEBUG
		if (debug) {
			printf("readip: wrong IP version or wrong proto "
			        "ip_v=%d ip_p=%d\n", ip->ip_v, ip->ip_p);
		}
#endif
		return -1;
	}

	hlen = ip->ip_hl << 2;
	if (hlen < sizeof(*ip) || ip_cksum(ip, hlen) != 0) {
#ifdef NET_DEBUG
		if (debug)
			printf("readip: short hdr or bad cksum.\n");
#endif
		return -1;
	}
	if (n < ntohs(ip->ip_len)) {
#ifdef NET_DEBUG
		if (debug)
			printf("readip: bad length %d < %d.\n",
			       (int) n, ntohs(ip->ip_len));
#endif
		return -1;
	}
	if (d->myip.s_addr && ip->ip_dst.s_addr != d->myip.s_addr) {
#ifdef NET_DEBUG
		if (debug) {
			printf("readip: bad saddr %s != ", inet_ntoa(d->myip));
			printf("%s\n", inet_ntoa(ip->ip_dst));
		}
#endif
		return -1;
	}
	return (ntohs(ip->ip_len) - 20);
}

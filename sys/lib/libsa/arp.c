/*	$NetBSD: arp.c,v 1.33 2009/01/17 14:00:36 tsutsui Exp $	*/

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
 * @(#) Header: arp.c,v 1.5 93/07/15 05:52:26 leres Exp  (LBL)
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_ether.h>
#include <netinet/in.h>

#include <netinet/in_systm.h>

#ifdef _STANDALONE
#include <lib/libkern/libkern.h>
#else
#include <string.h>
#endif

#include "stand.h"
#include "net.h"

/*
 * Ethernet Address Resolution Protocol.
 *
 * See RFC 826 for protocol description.  Structure below is adapted
 * to resolving internet addresses.  Field names used correspond to
 * RFC 826.
 */
struct ether_arp {
	struct	 arphdr ea_hdr;			/* fixed-size header */
	u_int8_t arp_sha[ETHER_ADDR_LEN];	/* sender hardware address */
	u_int8_t arp_spa[4];			/* sender protocol address */
	u_int8_t arp_tha[ETHER_ADDR_LEN];	/* target hardware address */
	u_int8_t arp_tpa[4];			/* target protocol address */
};
#define	arp_hrd	ea_hdr.ar_hrd
#define	arp_pro	ea_hdr.ar_pro
#define	arp_hln	ea_hdr.ar_hln
#define	arp_pln	ea_hdr.ar_pln
#define	arp_op	ea_hdr.ar_op

/* Cache stuff */
#define ARP_NUM 8			/* need at most 3 arp entries */

struct arp_list {
	struct in_addr	addr;
	u_char		ea[6];
} arp_list[ARP_NUM] = {
	/* XXX - net order `INADDR_BROADCAST' must be a constant */
	{ {0xffffffff}, BA }
};
int arp_num = 1;

/* Local forwards */
static	ssize_t arpsend(struct iodesc *, void *, size_t);
static	ssize_t arprecv(struct iodesc *, void *, size_t, saseconds_t);

/* Broadcast an ARP packet, asking who has addr on interface d */
u_char *
arpwhohas(struct iodesc *d, struct in_addr addr)
{
	int i;
	struct ether_arp *ah;
	struct arp_list *al;
	struct {
		struct ether_header eh;
		struct {
			struct ether_arp arp;
			u_char pad[18]; 	/* 60 - sizeof(...) */
		} data;
	} wbuf;
	struct {
		struct ether_header eh;
		struct {
			struct ether_arp arp;
			u_char pad[24]; 	/* extra space */
		} data;
	} rbuf;

	/* Try for cached answer first */
	for (i = 0, al = arp_list; i < arp_num; ++i, ++al)
		if (addr.s_addr == al->addr.s_addr)
			return al->ea;

	/* Don't overflow cache */
	if (arp_num > ARP_NUM - 1) {
		arp_num = 1;	/* recycle */
		printf("arpwhohas: overflowed arp_list!\n");
	}

#ifdef ARP_DEBUG
 	if (debug)
 		printf("arpwhohas: send request for %s\n", inet_ntoa(addr));
#endif

	(void)memset(&wbuf.data, 0, sizeof(wbuf.data));
	ah = &wbuf.data.arp;
	ah->arp_hrd = htons(ARPHRD_ETHER);
	ah->arp_pro = htons(ETHERTYPE_IP);
	ah->arp_hln = sizeof(ah->arp_sha); /* hardware address length */
	ah->arp_pln = sizeof(ah->arp_spa); /* protocol address length */
	ah->arp_op = htons(ARPOP_REQUEST);
	MACPY(d->myea, ah->arp_sha);
	(void)memcpy(ah->arp_spa, &d->myip, sizeof(ah->arp_spa));
	/* Leave zeros in arp_tha */
	(void)memcpy(ah->arp_tpa, &addr, sizeof(ah->arp_tpa));

	/* Store ip address in cache (incomplete entry). */
	al->addr = addr;

	i = sendrecv(d,
	    arpsend, &wbuf.data, sizeof(wbuf.data),
	    arprecv, &rbuf.data, sizeof(rbuf.data));
	if (i == -1) {
		panic("arp: no response for %s",
			  inet_ntoa(addr));
	}

	/* Store ethernet address in cache */
	ah = &rbuf.data.arp;
#ifdef ARP_DEBUG
 	if (debug) {
		printf("arp: response from %s\n",
		    ether_sprintf(rbuf.eh.ether_shost));
		printf("arp: cacheing %s --> %s\n",
		    inet_ntoa(addr), ether_sprintf(ah->arp_sha));
	}
#endif
	MACPY(ah->arp_sha, al->ea);
	++arp_num;

	return al->ea;
}

static ssize_t
arpsend(struct iodesc *d, void *pkt, size_t len)
{

#ifdef ARP_DEBUG
 	if (debug)
		printf("arpsend: called\n");
#endif

	return sendether(d, pkt, len, bcea, ETHERTYPE_ARP);
}

/*
 * Returns 0 if this is the packet we're waiting for
 * else -1 (and errno == 0)
 */
static ssize_t
arprecv(struct iodesc *d, void *pkt, size_t len, saseconds_t tleft)
{
	ssize_t n;
	struct ether_arp *ah;
	u_int16_t etype;	/* host order */

#ifdef ARP_DEBUG
 	if (debug)
		printf("arprecv: ");
#endif

	n = readether(d, pkt, len, tleft, &etype);
	errno = 0;	/* XXX */
	if (n == -1 || (size_t)n < sizeof(struct ether_arp)) {
#ifdef ARP_DEBUG
		if (debug)
			printf("bad len=%ld\n", (signed long) n);
#endif
		return -1;
	}

	if (etype != ETHERTYPE_ARP) {
#ifdef ARP_DEBUG
		if (debug)
			printf("not arp type=%d\n", etype);
#endif
		return -1;
	}

	/* Ethernet address now checked in readether() */

	ah = (struct ether_arp *)pkt;
	if (ah->arp_hrd != htons(ARPHRD_ETHER) ||
	    ah->arp_pro != htons(ETHERTYPE_IP) ||
	    ah->arp_hln != sizeof(ah->arp_sha) ||
	    ah->arp_pln != sizeof(ah->arp_spa) )
	{
#ifdef ARP_DEBUG
		if (debug)
			printf("bad hrd/pro/hln/pln\n");
#endif
		return -1;
	}

	if (ah->arp_op == htons(ARPOP_REQUEST)) {
#ifdef ARP_DEBUG
		if (debug)
			printf("is request\n");
#endif
		arp_reply(d, ah);
		return -1;
	}

	if (ah->arp_op != htons(ARPOP_REPLY)) {
#ifdef ARP_DEBUG
		if (debug)
			printf("not ARP reply\n");
#endif
		return -1;
	}

	/* Is the reply from the source we want? */
	if (memcmp(&arp_list[arp_num].addr,
			 ah->arp_spa, sizeof(ah->arp_spa)))
	{
#ifdef ARP_DEBUG
		if (debug)
			printf("unwanted address\n");
#endif
		return -1;
	}
	/* We don't care who the reply was sent to. */

	/* We have our answer. */
#ifdef ARP_DEBUG
 	if (debug)
		printf("got it\n");
#endif
	return n;
}

/*
 * Convert an ARP request into a reply and send it.
 * Notes:  Re-uses buffer.  Pad to length = 46.
 */
void
arp_reply(struct iodesc *d, void *pkt)
{
	struct ether_arp *arp = pkt;

	if (arp->arp_hrd != htons(ARPHRD_ETHER) ||
	    arp->arp_pro != htons(ETHERTYPE_IP) ||
	    arp->arp_hln != sizeof(arp->arp_sha) ||
	    arp->arp_pln != sizeof(arp->arp_spa) )
	{
#ifdef ARP_DEBUG
		if (debug)
			printf("arp_reply: bad hrd/pro/hln/pln\n");
#endif
		return;
	}

	if (arp->arp_op != htons(ARPOP_REQUEST)) {
#ifdef ARP_DEBUG
		if (debug)
			printf("arp_reply: not request!\n");
#endif
		return;
	}

	/* If we are not the target, ignore the request. */
	if (memcmp(arp->arp_tpa, &d->myip, sizeof(arp->arp_tpa)))
		return;

#ifdef ARP_DEBUG
	if (debug) {
		printf("arp_reply: to %s\n", ether_sprintf(arp->arp_sha));
	}
#endif

	arp->arp_op = htons(ARPOP_REPLY);
	/* source becomes target */
	(void)memcpy(arp->arp_tha, arp->arp_sha, sizeof(arp->arp_tha));
	(void)memcpy(arp->arp_tpa, arp->arp_spa, sizeof(arp->arp_tpa));
	/* here becomes source */
	(void)memcpy(arp->arp_sha, d->myea, sizeof(arp->arp_sha));
	(void)memcpy(arp->arp_spa, &d->myip, sizeof(arp->arp_spa));

	/*
	 * No need to get fancy here.  If the send fails, the
	 * requestor will just ask again.
	 */
	(void) sendether(d, pkt, sizeof(*arp) + 18,
	                 arp->arp_tha, ETHERTYPE_ARP);
}

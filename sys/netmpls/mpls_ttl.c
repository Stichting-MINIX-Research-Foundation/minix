/*	$NetBSD: mpls_ttl.c,v 1.5 2015/08/24 22:21:27 pooka Exp $ */

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*-
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Public Access Networks Corporation ("Panix").  It was developed under
 * contract to Panix by Eric Haszlakiewicz and Thor Lancelot Simon.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of Zembu Labs, Inc.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Mihai Chelaru <kefren@NetBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982, 1986, 1988, 1993
 *      The Regents of the University of California.  All rights reserved.
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
 *
 *      @(#)ip_icmp.c   8.2 (Berkeley) 1/4/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mpls_ttl.c,v 1.5 2015/08/24 22:21:27 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#include "opt_mpls.h"
#endif

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp_private.h>
#include <netinet/icmp_var.h>

#include <netmpls/mpls.h>
#include <netmpls/mpls_var.h>

#ifdef INET

/* in netinet/ip_icmp.c */
extern int icmpreturndatabytes;

/* ICMP Extensions */

#define ICMP_EXT_VERSION 2
#define	MPLS_RETURN_DATA 128
#define	ICMP_EXT_OFFSET	128

struct icmp_ext_cmn_hdr {
#if BYTE_ORDER == BIG_ENDIAN
        unsigned char   version:4;
        unsigned char   reserved1:4;
#else
        unsigned char   reserved1:4;
        unsigned char   version:4;
#endif
	unsigned char   reserved2;
	unsigned short  checksum;
};

struct icmp_ext_obj_hdr {
	u_short length;
	u_char  class_num;
#define MPLS_STACK_ENTRY_CLASS 1
	u_char  c_type;
#define MPLS_STACK_ENTRY_C_TYPE 1
};

struct mpls_extension {
	struct icmp_ext_cmn_hdr cmn_hdr;
	struct icmp_ext_obj_hdr obj_hdr;
	union mpls_shim ms;
} __packed;

static void mpls_icmp_error(struct mbuf *, int, int, n_long, int,
	union mpls_shim *);
static bool ip4_check(struct mbuf *);

/*
 * References: RFC 4884 and RFC 4950
 * This should be in sync with icmp_error() in sys/netinet/ip_icmp.c
 * XXX: is called only for ICMP_TIMXCEED_INTRANS but code is too general
 */

static void
mpls_icmp_error(struct mbuf *n, int type, int code, n_long dest,
    int destmtu, union mpls_shim *shim)
{
	struct ip *oip = mtod(n, struct ip *), *nip;
	unsigned oiplen = oip->ip_hl << 2;
	struct icmp *icp;
	struct mbuf *m;
	unsigned icmplen, mblen, packetlen;
	struct mpls_extension mpls_icmp_ext;
	
	memset(&mpls_icmp_ext, 0, sizeof(mpls_icmp_ext));
	mpls_icmp_ext.cmn_hdr.version = ICMP_EXT_VERSION;
	mpls_icmp_ext.cmn_hdr.checksum = 0;

	mpls_icmp_ext.obj_hdr.length = htons(sizeof(union mpls_shim) + 
					sizeof(struct icmp_ext_obj_hdr));
	mpls_icmp_ext.obj_hdr.class_num = MPLS_STACK_ENTRY_CLASS;
	mpls_icmp_ext.obj_hdr.c_type = MPLS_STACK_ENTRY_C_TYPE;

	mpls_icmp_ext.ms.s_addr = shim->s_addr;

#ifdef ICMPPRINTFS
	if (icmpprintfs)
		printf("mpls_icmp_error(%p, type:%d, code:%d)\n", oip, type,
			code);
#endif
	if (type != ICMP_REDIRECT)
		ICMP_STATINC(ICMP_STAT_ERROR);
	/*
	 * Don't send error if the original packet was encrypted.
	 * Don't send error if not the first fragment of message.
	 * Don't error if the old packet protocol was ICMP
	 * error message, only known informational types.
	 */
	if (n->m_flags & M_DECRYPTED)
		goto freeit;
	if (oip->ip_off &~ htons(IP_MF|IP_DF))
		goto freeit;
	if (oip->ip_p == IPPROTO_ICMP && type != ICMP_REDIRECT &&
	    n->m_len >= oiplen + ICMP_MINLEN &&
	    !ICMP_INFOTYPE(((struct icmp *)((char *)oip + oiplen))->icmp_type))
	{
		ICMP_STATINC(ICMP_STAT_OLDICMP);
		goto freeit;
	}
	/* Don't send error in response to a multicast or broadcast packet */
	if (n->m_flags & (M_BCAST|M_MCAST))
		goto freeit;

	/*
	 * First, do a rate limitation check.
	 */
	if (icmp_ratelimit(&oip->ip_src, type, code))
		/* XXX stats */
		goto freeit;

	/*
	 * Now, formulate icmp message
	 */
	icmplen = min(ICMP_EXT_OFFSET, ntohs(oip->ip_len));
	/*
	 * Defend against mbuf chains shorter than oip->ip_len - oiplen:
	 */
	mblen = 0;
	for (m = n; m && (mblen < icmplen); m = m->m_next)
		mblen += m->m_len;
	icmplen = min(mblen, icmplen);

	packetlen = sizeof(struct ip) + offsetof(struct icmp, icmp_ip) +
	    ICMP_EXT_OFFSET + sizeof(mpls_icmp_ext);

	/*
	 * As we are not required to return everything we have,
	 * we return whatever we can return at ease.
	 *
	 * Note that ICMP datagrams longer than 576 octets are out of spec
	 * according to RFC1812; the limit on icmpreturndatabytes below in
	 * icmp_sysctl will keep things below that limit.
	 */

	KASSERT (packetlen <= MCLBYTES);

	m = m_gethdr(M_DONTWAIT, MT_HEADER);
	if (m && (packetlen > MHLEN)) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			m = NULL;
		}
	}
	if (m == NULL)
		goto freeit;
	MCLAIM(m, n->m_owner);
	m->m_len = packetlen;
	if ((m->m_flags & M_EXT) == 0)
		MH_ALIGN(m, m->m_len);
	m->m_data += sizeof(struct ip);
	m->m_len -= sizeof(struct ip);

	icp = mtod(m, struct icmp *);
	if ((u_int)type > ICMP_MAXTYPE)
		panic("icmp error (mpls_ttl)");
	ICMP_STATINC(ICMP_STAT_OUTHIST + type);
	icp->icmp_type = type;
	if (type == ICMP_REDIRECT)
		icp->icmp_gwaddr.s_addr = dest;
	else {
		icp->icmp_void = 0;
		/*
		 * The following assignments assume an overlay with the
		 * zeroed icmp_void field.
		 */
		if (type == ICMP_PARAMPROB) {
			icp->icmp_pptr = code;
			code = 0;
		} else if (type == ICMP_UNREACH &&
		    code == ICMP_UNREACH_NEEDFRAG && destmtu)
			icp->icmp_nextmtu = htons(destmtu);
	}

	icp->icmp_code = code;

	memset(&icp->icmp_ip, 0, ICMP_EXT_OFFSET);
	m_copydata(n, 0, icmplen, (char *)&icp->icmp_ip);

	/* Append the extension structure */
	memcpy(((char*)&icp->icmp_ip) + ICMP_EXT_OFFSET,
	    &mpls_icmp_ext, sizeof(mpls_icmp_ext));

	/*
	 * Now, copy old ip header (without options)
	 * in front of icmp message.
	*/
	if ((m->m_flags & M_EXT) == 0 &&
	    m->m_data - sizeof(struct ip) < m->m_pktdat)
		panic("icmp len");
	m->m_data -= sizeof(struct ip);
	m->m_len += sizeof(struct ip);
	m->m_pkthdr.len = m->m_len;
	m->m_pkthdr.rcvif = n->m_pkthdr.rcvif;
	nip = mtod(m, struct ip *);
	/* ip_v set in ip_output */
	nip->ip_hl = sizeof(struct ip) >> 2;
	nip->ip_tos = 0;
	nip->ip_len = htons(m->m_len);
	/* ip_id set in ip_output */
	nip->ip_off = htons(0);
	/* ip_ttl set in icmp_reflect */
	nip->ip_p = IPPROTO_ICMP;
	nip->ip_src = oip->ip_src;
	nip->ip_dst = oip->ip_dst;
	icmp_reflect(m);

freeit:
	m_freem(n);

}

static bool
ip4_check(struct mbuf *m)
{
	struct ip *iph;
	int hlen, len;

	if (m->m_len < sizeof(struct ip) &&
	    (m = m_pullup(m, sizeof(struct ip))) == NULL)
		return false;

	iph = mtod(m, struct ip *);

	if (iph->ip_v != IPVERSION)
		goto freeit;
	hlen = iph->ip_hl << 2;
	if (hlen < sizeof(struct ip))
		goto freeit;
	if (hlen > m->m_len) {
		if ((m = m_pullup(m, hlen)) == NULL)
			return false;
		iph = mtod(m, struct ip *);
	}
	if (IN_MULTICAST(iph->ip_src.s_addr) ||
	    (ntohl(iph->ip_dst.s_addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET ||
	    (ntohl(iph->ip_src.s_addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET ||
	    in_cksum(m, hlen) != 0)
		goto freeit;

	len = ntohs(iph->ip_len);
	if (len < hlen || m->m_pkthdr.len < len)
		goto freeit;

	return true;
freeit:
	m_freem(m);
	return false;

}

#endif	/* INET */

struct mbuf *
mpls_ttl_dec(struct mbuf *m)
{
	union mpls_shim *mshim;
#ifdef INET
	union mpls_shim top_shim, bossh;
#endif

	if (__predict_false(m->m_len < sizeof(union mpls_shim) &&
	    (m = m_pullup(m, sizeof(union mpls_shim))) == NULL))
		return NULL;
	mshim = mtod(m, union mpls_shim *);
	mshim->s_addr = ntohl(mshim->s_addr);
	mshim->shim.ttl--;

	if (mshim->shim.ttl == 0) {
		if (!mpls_icmp_respond) {
			m_freem(m);
			return NULL;
		}

#ifdef INET
		/*
		 * shim ttl exceeded
		 * send back ICMP type 11 code 0
		 */
		bossh.s_addr = mshim->s_addr;
		top_shim.s_addr = htonl(mshim->s_addr);
		m_adj(m, sizeof(union mpls_shim));

		/* Goto BOS */
		while(bossh.shim.bos == 0) {
			if (m->m_len < sizeof(union mpls_shim) &&
			    (m = m_pullup(m, sizeof(union mpls_shim))) == NULL) {
				m_freem(m);
				return NULL;
			}
			bossh.s_addr = ntohl(mtod(m, union mpls_shim *)->s_addr);
			m_adj(m, sizeof(union mpls_shim));
		}
		
		if (ip4_check(m) == true)
			mpls_icmp_error(m, ICMP_TIMXCEED, ICMP_TIMXCEED_INTRANS,
			    0, 0, &top_shim);
#else
		m_freem(m);
#endif
		return NULL;
	}

	mshim->s_addr = htonl(mshim->s_addr);

	return m;
}

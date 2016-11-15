/*	$NetBSD: if_agrether_hash.c,v 1.3 2007/05/05 18:23:23 yamt Exp $	*/

/*-
 * Copyright (c)2005 YAMAMOTO Takashi,
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_agrether_hash.c,v 1.3 2007/05/05 18:23:23 yamt Exp $");

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/hash.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_vlanvar.h>

#include <net/agr/if_agrethervar.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>

#define	HASH(p, l, h)	hash32_buf((p), (l), (h))

static const void *agr_m_extract(struct mbuf *, int, int, void *);

static const void *
agr_m_extract(struct mbuf *m, int off, int len, void *buf)
{
	const void *result;

	KASSERT((m->m_flags & M_PKTHDR) != 0);

	if (m->m_pkthdr.len < off + len) {
		result = NULL;
	} else if (m->m_len >= off + len) {
		result = mtod(m, char *) + off;
	} else {
		m_copydata(m, off, len, buf);
		result = buf;
	}

	return result;
}

uint32_t
agrether_hashmbuf(struct agr_softc *sc, struct mbuf *m)
{
	struct ether_header eh_store;
	const struct ether_header *eh;
	uint32_t hash = HASH32_BUF_INIT;
	int off = 0;
	uint16_t tci;
	uint16_t etype;
	struct m_tag *mtag;

	eh = agr_m_extract(m, off, sizeof(*eh), &eh_store);
	if (eh == NULL) {
		return hash;
	}

	hash = HASH(&eh->ether_dhost, sizeof(eh->ether_dhost), hash);
	hash = HASH(&eh->ether_shost, sizeof(eh->ether_shost), hash);
	etype = eh->ether_type;

	if (etype == htobe16(ETHERTYPE_VLAN)) {
		struct ether_vlan_header vlanhdr_store;
		const struct ether_vlan_header *vlanhdr;

		vlanhdr = agr_m_extract(m, off, sizeof(*vlanhdr),
		    &vlanhdr_store);
		if (vlanhdr == NULL) {
			return hash;
		}

		tci = vlanhdr->evl_tag;
		etype = vlanhdr->evl_proto;
		off += sizeof(*vlanhdr) - sizeof(*eh);
	} else if ((mtag = m_tag_find(m, PACKET_TAG_VLAN, NULL)) != NULL) {
		tci = htole16((*(u_int *)(mtag + 1)) & 0xffff);
	} else {
		tci = 0;
	}
	hash = HASH(&tci, sizeof(tci), hash);

	off += sizeof(*eh);
	if (etype == htobe16(ETHERTYPE_IP)) {
		struct ip ip_store;
		const struct ip *ip;

		ip = agr_m_extract(m, off, sizeof(*ip), &ip_store);
		if (ip == NULL) {
			return hash;
		}

		hash = HASH(&ip->ip_src, sizeof(ip->ip_src), hash);
		hash = HASH(&ip->ip_dst, sizeof(ip->ip_dst), hash);
		hash = HASH(&ip->ip_p, sizeof(ip->ip_p), hash);

		/* use port numbers for tcp and udp? */

	} else if (etype == htobe16(ETHERTYPE_IPV6)) {
		struct ip6_hdr ip6_store;
		const struct ip6_hdr *ip6;
		uint32_t flowlabel;

		ip6 = agr_m_extract(m, off, sizeof(*ip6), &ip6_store);
		if (ip6 == NULL) {
			return hash;
		}

		hash = HASH(&ip6->ip6_src, sizeof(ip6->ip6_src), hash);
		hash = HASH(&ip6->ip6_dst, sizeof(ip6->ip6_dst), hash);
		/* hash = HASH(&ip6->ip6_nxt, sizeof(ip6->ip6_nxt), hash); */

		flowlabel = ip6->ip6_flow & IPV6_FLOWLABEL_MASK;
		hash = HASH(&flowlabel, sizeof(flowlabel), hash);

		/* use port numbers for tcp and udp? */
	}

	return hash;
}

/*	$NetBSD: rthdr.c,v 1.18 2012/03/13 21:13:42 christos Exp $	*/

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

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: rthdr.c,v 1.18 2012/03/13 21:13:42 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/ip6.h>

#include <assert.h>
#include <string.h>
#include <stdio.h>

#ifdef __weak_alias
__weak_alias(inet6_rthdr_add,_inet6_rthdr_add)
__weak_alias(inet6_rthdr_getaddr,_inet6_rthdr_getaddr)
__weak_alias(inet6_rthdr_getflags,_inet6_rthdr_getflags)
__weak_alias(inet6_rthdr_init,_inet6_rthdr_init)
__weak_alias(inet6_rthdr_lasthop,_inet6_rthdr_lasthop)
__weak_alias(inet6_rthdr_segments,_inet6_rthdr_segments)
__weak_alias(inet6_rthdr_space,_inet6_rthdr_space)
__weak_alias(inet6_rth_space, _inet6_rth_space)
__weak_alias(inet6_rth_init, _inet6_rth_init)
__weak_alias(inet6_rth_add, _inet6_rth_add)
__weak_alias(inet6_rth_reverse, _inet6_rth_reverse)
__weak_alias(inet6_rth_segments, _inet6_rth_segments)
__weak_alias(inet6_rth_getaddr, _inet6_rth_getaddr)
#endif

/*
 * RFC2292 API
 */

size_t
inet6_rthdr_space(int type, int seg)
{
	switch (type) {
	case IPV6_RTHDR_TYPE_0:
		if (seg < 1 || seg > 23)
			return (0);
		return (CMSG_SPACE(sizeof(struct in6_addr) * seg +
		    sizeof(struct ip6_rthdr0)));
	default:
		return (0);
	}
}

struct cmsghdr *
inet6_rthdr_init(void *bp, int type)
{
	struct cmsghdr *ch;
	struct ip6_rthdr *rthdr;

	_DIAGASSERT(bp != NULL);

	ch = (struct cmsghdr *)bp;
	rthdr = (struct ip6_rthdr *)(void *)CMSG_DATA(ch);

	ch->cmsg_level = IPPROTO_IPV6;
	ch->cmsg_type = IPV6_RTHDR;

	switch (type) {
	case IPV6_RTHDR_TYPE_0:
#ifdef COMPAT_RFC2292
		ch->cmsg_len = CMSG_LEN(sizeof(struct ip6_rthdr0) -
		    sizeof(struct in6_addr));
#else
		ch->cmsg_len = CMSG_LEN(sizeof(struct ip6_rthdr0));
#endif
		(void)memset(rthdr, 0, sizeof(struct ip6_rthdr0));
		rthdr->ip6r_type = IPV6_RTHDR_TYPE_0;
		return (ch);
	default:
		return (NULL);
	}
}

int
inet6_rthdr_add(struct cmsghdr *cmsg, const struct in6_addr *addr, u_int flags)
{
	struct ip6_rthdr *rthdr;

	_DIAGASSERT(cmsg != NULL);
	_DIAGASSERT(addr != NULL);

	rthdr = (struct ip6_rthdr *)(void *)CMSG_DATA(cmsg);

	switch (rthdr->ip6r_type) {
	case IPV6_RTHDR_TYPE_0:
	{
		size_t len;
		struct ip6_rthdr0 *rt0 = (struct ip6_rthdr0 *)(void *)rthdr;
		if (flags != IPV6_RTHDR_LOOSE && flags != IPV6_RTHDR_STRICT)
			return (-1);
		if (rt0->ip6r0_segleft == 23)
			return (-1);
		if (flags != IPV6_RTHDR_LOOSE)
			return (-1);
		rt0->ip6r0_segleft++;
		(void)memcpy(((caddr_t)(void *)rt0) +
		    ((rt0->ip6r0_len + 1) << 3), addr, sizeof(struct in6_addr));
		rt0->ip6r0_len += sizeof(struct in6_addr) >> 3;
		len = CMSG_LEN((rt0->ip6r0_len + 1) << 3);
		_DIAGASSERT(__type_fit(socklen_t, len));
		cmsg->cmsg_len = (socklen_t)len;
		break;
	}
	default:
		return (-1);
	}

	return (0);
}

int
inet6_rthdr_lasthop(struct cmsghdr *cmsg, unsigned int flags)
{
	struct ip6_rthdr *rthdr;

	_DIAGASSERT(cmsg != NULL);

	rthdr = (struct ip6_rthdr *)(void *)CMSG_DATA(cmsg);

	switch (rthdr->ip6r_type) {
	case IPV6_RTHDR_TYPE_0:
	{
		struct ip6_rthdr0 *rt0 = (struct ip6_rthdr0 *)(void *)rthdr;
		if (rt0->ip6r0_segleft > 23)
			return (-1);
		if (flags != IPV6_RTHDR_LOOSE)
			return (-1);
		break;
	}
	default:
		return (-1);
	}

	return (0);
}

#if 0
int
inet6_rthdr_reverse(const struct cmsghdr *in, struct cmsghdr *out)
{

	return (-1);
}
#endif

int
inet6_rthdr_segments(const struct cmsghdr *cmsg)
{
	const struct ip6_rthdr *rthdr;

	_DIAGASSERT(cmsg != NULL);

	rthdr = __UNCONST(CCMSG_DATA(cmsg));

	switch (rthdr->ip6r_type) {
	case IPV6_RTHDR_TYPE_0:
	{
		const struct ip6_rthdr0 *rt0 =
		    (const struct ip6_rthdr0 *)(const void *)rthdr;
		size_t len;

		if (rt0->ip6r0_len % 2 || 46 < rt0->ip6r0_len)
			return (-1);

		len = (rt0->ip6r0_len * 8) / sizeof(struct in6_addr);
		_DIAGASSERT(__type_fit(int, len));
		return (int)len;
	}

	default:
		return (-1);
	}
}

struct in6_addr *
inet6_rthdr_getaddr(struct cmsghdr *cmsg, int idx)
{
	struct ip6_rthdr *rthdr;

	_DIAGASSERT(cmsg != NULL);

	rthdr = (struct ip6_rthdr *)(void *)CMSG_DATA(cmsg);

	switch (rthdr->ip6r_type) {
	case IPV6_RTHDR_TYPE_0:
	{
		struct ip6_rthdr0 *rt0 = (struct ip6_rthdr0 *)(void *)rthdr;
		int naddr;
		size_t len;

		if (rt0->ip6r0_len % 2 || 46 < rt0->ip6r0_len)
			return NULL;
		len = (rt0->ip6r0_len * 8) / sizeof(struct in6_addr);
		_DIAGASSERT(__type_fit(int, len));
		naddr = (int)len;
		if (idx <= 0 || naddr < idx)
			return NULL;
#ifdef COMPAT_RFC2292
		return ((struct in6_addr *)(void *)(rt0 + 1)) + idx - 1;
#else
		return ((struct in6_addr *)(void *)(rt0 + 1)) + idx;
#endif
	}

	default:
		return NULL;
	}
}

int
inet6_rthdr_getflags(const struct cmsghdr *cmsg, int idx)
{
	const struct ip6_rthdr *rthdr;

	_DIAGASSERT(cmsg != NULL);

	rthdr = __UNCONST(CCMSG_DATA(cmsg));

	switch (rthdr->ip6r_type) {
	case IPV6_RTHDR_TYPE_0:
	{
		const struct ip6_rthdr0 *rt0 = (const struct ip6_rthdr0 *)
		(const void *)rthdr;
		int naddr;
		size_t len;

		if (rt0->ip6r0_len % 2 || 46 < rt0->ip6r0_len)
			return (-1);
		len = (rt0->ip6r0_len * 8) / sizeof(struct in6_addr);
		_DIAGASSERT(__type_fit(int, len));
		naddr = (int)len;
		if (idx < 0 || naddr < idx)
			return (-1);
		return IPV6_RTHDR_LOOSE;
	}

	default:
		return (-1);
	}
}

/*
 * RFC3542 (2292bis) API
 */

socklen_t
inet6_rth_space(int type, int segments)
{
	switch (type) {
	case IPV6_RTHDR_TYPE_0:
		return (((segments * 2) + 1) << 3);
	default:
		return (0);	/* type not suppported */
	}
}

void *
inet6_rth_init(void *bp, socklen_t bp_len, int type, int segments)
{
	struct ip6_rthdr *rth;
	struct ip6_rthdr0 *rth0;

	_DIAGASSERT(bp != NULL);

	rth = (struct ip6_rthdr *)bp;

	switch (type) {
	case IPV6_RTHDR_TYPE_0:
		/* length validation */
		if (bp_len < inet6_rth_space(IPV6_RTHDR_TYPE_0, segments))
			return (NULL);

		memset(bp, 0, bp_len);
		rth0 = (struct ip6_rthdr0 *)(void *)rth;
		rth0->ip6r0_len = segments * 2;
		rth0->ip6r0_type = IPV6_RTHDR_TYPE_0;
		rth0->ip6r0_segleft = 0;
		rth0->ip6r0_reserved = 0;
		break;
	default:
		return (NULL);	/* type not supported */
	}

	return (bp);
}

int
inet6_rth_add(void *bp, const struct in6_addr *addr)
{
	struct ip6_rthdr *rth;
	struct ip6_rthdr0 *rth0;
	struct in6_addr *nextaddr;

	_DIAGASSERT(bp != NULL);

	rth = (struct ip6_rthdr *)bp;

	switch (rth->ip6r_type) {
	case IPV6_RTHDR_TYPE_0:
		rth0 = (struct ip6_rthdr0 *)(void *)rth;
		nextaddr = (struct in6_addr *)(void *)(rth0 + 1)
		    + rth0->ip6r0_segleft;
		*nextaddr = *addr;
		rth0->ip6r0_segleft++;
		break;
	default:
		return (-1);	/* type not supported */
	}

	return (0);
}

int
inet6_rth_reverse(const void *in, void *out)
{
	const struct ip6_rthdr *rth_in;
	const struct ip6_rthdr0 *rth0_in;
	struct ip6_rthdr0 *rth0_out;
	int i, segments;

	_DIAGASSERT(in != NULL);
	_DIAGASSERT(out != NULL);

	rth_in = (const struct ip6_rthdr *)in;

	switch (rth_in->ip6r_type) {
	case IPV6_RTHDR_TYPE_0:
		rth0_in = (const struct ip6_rthdr0 *)in;
		rth0_out = (struct ip6_rthdr0 *)out;

		/* parameter validation XXX too paranoid? */
		if (rth0_in->ip6r0_len % 2)
			return (-1);
		segments = rth0_in->ip6r0_len / 2;

		/* we can't use memcpy here, since in and out may overlap */
		memmove((void *)rth0_out, (const void *)rth0_in,
			(unsigned int)(((rth0_in->ip6r0_len) + 1) << 3));
		rth0_out->ip6r0_segleft = segments;

		/* reverse the addresses */
		for (i = 0; i < segments / 2; i++) {
			struct in6_addr addr_tmp, *addr1, *addr2;

			addr1 = (struct in6_addr *)(void *)(rth0_out + 1) + i;
			addr2 = (struct in6_addr *)(void *)(rth0_out + 1) +
				(segments - i - 1);
			addr_tmp = *addr1;
			*addr1 = *addr2;
			*addr2 = addr_tmp;
		}
		
		break;
	default:
		return (-1);	/* type not supported */
	}

	return (0);
}

int
inet6_rth_segments(const void *bp)
{
	const struct ip6_rthdr *rh;
	const struct ip6_rthdr0 *rh0;
	unsigned int addrs;

	_DIAGASSERT(bp != NULL);

	rh = (const struct ip6_rthdr *)bp;

	switch (rh->ip6r_type) {
	case IPV6_RTHDR_TYPE_0:
		rh0 = (const struct ip6_rthdr0 *)bp;

		/*
		 * Validation for a type-0 routing header.
		 * Is this too strict?
		 */
		if ((rh0->ip6r0_len % 2) != 0 ||
		    (addrs = (rh0->ip6r0_len / 2)) < rh0->ip6r0_segleft)
			return (-1);

		return (addrs);
	default:
		return (-1);	/* unknown type */
	}
}

struct in6_addr *
inet6_rth_getaddr(const void *bp, int idx)
{
	const struct ip6_rthdr *rh;
	const struct ip6_rthdr0 *rh0;
	unsigned int addrs;

	_DIAGASSERT(bp != NULL);

	rh = (const struct ip6_rthdr *)bp;

	switch (rh->ip6r_type) {
	case IPV6_RTHDR_TYPE_0:
		 rh0 = (const struct ip6_rthdr0 *)bp;
		 
		/*
		 * Validation for a type-0 routing header.
		 * Is this too strict?
		 */
		if ((rh0->ip6r0_len % 2) != 0 ||
		    (addrs = (rh0->ip6r0_len / 2)) < rh0->ip6r0_segleft)
			return (NULL);

		if (idx < 0 || addrs <= (unsigned int)idx)
			return (NULL);

		return (((struct in6_addr *)(void *)__UNCONST(rh0 + 1)) + idx);
	default:
		return (NULL);	/* unknown type */
	}
}

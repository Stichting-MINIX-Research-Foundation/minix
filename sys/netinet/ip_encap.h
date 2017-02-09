/*	$NetBSD: ip_encap.h,v 1.13 2008/11/25 18:28:05 pooka Exp $	*/
/*	$KAME: ip_encap.h,v 1.7 2000/03/25 07:23:37 sumikawa Exp $	*/

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

#ifndef _NETINET_IP_ENCAP_H_
#define _NETINET_IP_ENCAP_H_

#ifdef _KERNEL

#ifndef RNF_NORMAL
#include <net/radix.h>
#endif

struct encaptab {
	struct radix_node nodes[2];
	LIST_ENTRY(encaptab) chain;
	int af;
	int proto;			/* -1: don't care, I'll check myself */
	struct sockaddr *addrpack;	/* malloc'ed, for radix lookup */
	struct sockaddr *maskpack;	/* ditto */
	struct sockaddr *src;		/* my addr */
	struct sockaddr *srcmask;
	struct sockaddr *dst;		/* remote addr */
	struct sockaddr *dstmask;
	int (*func) (struct mbuf *, int, int, void *);
	const struct protosw *psw;	/* only pr_input will be used */
	void *arg;			/* passed via PACKET_TAG_ENCAP */
};

/* to lookup a pair of address using radix tree */
struct sockaddr_pack {
	u_int8_t sp_len;
	u_int8_t sp_family;	/* not really used */
	/* followed by variable-length data */
};

struct ip_pack4 {
	struct sockaddr_pack p;
	struct sockaddr_in mine;
	struct sockaddr_in yours;
};
struct ip_pack6 {
	struct sockaddr_pack p;
	struct sockaddr_in6 mine;
	struct sockaddr_in6 yours;
};

void	encap_init(void);
void	encap4_input(struct mbuf *, ...);
int	encap6_input(struct mbuf **, int *, int);
const struct encaptab *encap_attach(int, int, const struct sockaddr *,
	const struct sockaddr *, const struct sockaddr *,
	const struct sockaddr *, const struct protosw *, void *);
const struct encaptab *encap_attach_func(int, int,
	int (*)(struct mbuf *, int, int, void *),
	const struct protosw *, void *);
void	*encap6_ctlinput(int, const struct sockaddr *, void *);
int	encap_detach(const struct encaptab *);
void	*encap_getarg(struct mbuf *);
#endif

#endif /* !_NETINET_IP_ENCAP_H_ */

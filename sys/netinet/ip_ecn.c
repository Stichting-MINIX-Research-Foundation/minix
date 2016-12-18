/*	$NetBSD: ip_ecn.c,v 1.16 2015/08/24 22:21:26 pooka Exp $	*/
/*	$KAME: ip_ecn.c,v 1.11 2001/05/03 16:09:29 itojun Exp $	*/

/*
 * Copyright (C) 1999 WIDE Project.
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
 *
 */
/*
 * ECN consideration on tunnel ingress/egress operation.
 * http://www.aciri.org/floyd/papers/draft-ipsec-ecn-00.txt
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ip_ecn.c,v 1.16 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/errno.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#ifdef INET6
#include <netinet/ip6.h>
#endif

#include <netinet/ip_ecn.h>

/*
 * modify outer ECN (TOS) field on ingress operation (tunnel encapsulation).
 */
void
ip_ecn_ingress(int mode, u_int8_t *outer, const u_int8_t *inner)
{
	if (!outer || !inner)
		panic("NULL pointer passed to ip_ecn_ingress");

	*outer = *inner;
	switch (mode) {
	case ECN_ALLOWED:		/* ECN allowed */
		*outer &= ~IPTOS_ECN_CE;
		break;
	case ECN_FORBIDDEN:		/* ECN forbidden */
		*outer &= ~(IPTOS_ECN_ECT0 | IPTOS_ECN_CE);
		break;
	case ECN_NOCARE:	/* no consideration to ECN */
		break;
	}
}

/*
 * modify inner ECN (TOS) field on egress operation (tunnel decapsulation).
 */
void
ip_ecn_egress(int mode, const u_int8_t *outer, u_int8_t *inner)
{
	if (!outer || !inner)
		panic("NULL pointer passed to ip_ecn_egress");

	switch (mode) {
	case ECN_ALLOWED:
		if (*outer & IPTOS_ECN_CE)
			*inner |= IPTOS_ECN_CE;
		break;
	case ECN_FORBIDDEN:		/* ECN forbidden */
	case ECN_NOCARE:	/* no consideration to ECN */
		break;
	}
}

#ifdef INET6
void
ip6_ecn_ingress(int mode, u_int32_t *outer, const u_int32_t *inner)
{
	u_int8_t outer8, inner8;

	if (!outer || !inner)
		panic("NULL pointer passed to ip6_ecn_ingress");

	outer8 = (ntohl(*outer) >> 20) & 0xff;
	inner8 = (ntohl(*inner) >> 20) & 0xff;
	ip_ecn_ingress(mode, &outer8, &inner8);
	*outer &= ~htonl(0xff << 20);
	*outer |= htonl((u_int32_t)outer8 << 20);
}

void
ip6_ecn_egress(int mode, const u_int32_t *outer, u_int32_t *inner)
{
	u_int8_t outer8, inner8;

	if (!outer || !inner)
		panic("NULL pointer passed to ip6_ecn_egress");

	outer8 = (ntohl(*outer) >> 20) & 0xff;
	inner8 = (ntohl(*inner) >> 20) & 0xff;
	ip_ecn_egress(mode, &outer8, &inner8);
	*inner &= ~htonl(0xff << 20);
	*inner |= htonl((u_int32_t)inner8 << 20);
}
#endif

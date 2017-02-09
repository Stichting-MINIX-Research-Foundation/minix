/*	$NetBSD: rtbl.c,v 1.2 2015/08/24 22:21:26 pooka Exp $	*/

/*-
 * Copyright (c) 1998, 2008, 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Kevin M. Lahey of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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

/*
 * Copyright (c) 1980, 1986, 1991, 1993
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
 *
 *	@(#)route.c	8.3 (Berkeley) 1/9/95
 */

#if defined(_KERNEL) && defined(_KERNEL_OPT)
#include "opt_route.h"
#endif /* _KERNEL && _KERNEL_OPT */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rtbl.c,v 1.2 2015/08/24 22:21:26 pooka Exp $");

#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/pool.h>
#include <sys/kauth.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/raw_cb.h>

static rtbl_t *rt_tables[AF_MAX+1];

int
rt_inithead(rtbl_t **tp, int off)
{
	rtbl_t *t;
	if (*tp != NULL)
		return 1;
	if ((t = kmem_alloc(sizeof(*t), KM_SLEEP)) == NULL)
		return 0;
	*tp = t;
	return rn_inithead0(&t->t_rnh, off);
}

struct rtentry *
rt_matchaddr(rtbl_t *t, const struct sockaddr *dst)
{
	struct radix_node_head *rnh = &t->t_rnh;
	struct radix_node *rn;

	rn = rnh->rnh_matchaddr(dst, rnh);
	if (rn == NULL || (rn->rn_flags & RNF_ROOT) != 0)
		return NULL;
	return (struct rtentry *)rn;
}

int
rt_addaddr(rtbl_t *t, struct rtentry *rt, const struct sockaddr *netmask)
{
	struct radix_node_head *rnh = &t->t_rnh;
	struct radix_node *rn;

	rn = rnh->rnh_addaddr(rt_getkey(rt), netmask, rnh, rt->rt_nodes);

	return (rn == NULL) ? EEXIST : 0;
}

struct rtentry *
rt_lookup(rtbl_t *t, const struct sockaddr *dst, const struct sockaddr *netmask)
{
	struct radix_node_head *rnh = &t->t_rnh;
	struct radix_node *rn;

	rn = rnh->rnh_lookup(dst, netmask, rnh);
	if (rn == NULL || (rn->rn_flags & RNF_ROOT) != 0)
		return NULL;
	return (struct rtentry *)rn;
}

struct rtentry *
rt_deladdr(rtbl_t *t, const struct sockaddr *dst,
    const struct sockaddr *netmask)
{
	struct radix_node_head *rnh = &t->t_rnh;
	struct radix_node *rn;

	if ((rn = rnh->rnh_deladdr(dst, netmask, rnh)) == NULL)
		return NULL;
	if (rn->rn_flags & (RNF_ACTIVE | RNF_ROOT))
		panic("%s", __func__);
	return (struct rtentry *)rn;
}

static int
rt_walktree_visitor(struct radix_node *rn, void *v)
{
	struct rtwalk *rw = (struct rtwalk *)v;

	return (*rw->rw_f)((struct rtentry *)rn, rw->rw_v);
}

int
rt_walktree(sa_family_t family, int (*f)(struct rtentry *, void *), void *v)
{
	rtbl_t *t = rt_tables[family];
	struct rtwalk rw;

	if (t == NULL)
		return 0;

	rw.rw_f = f;
	rw.rw_v = v;

	return rn_walktree(&t->t_rnh, rt_walktree_visitor, &rw);
}

rtbl_t *
rt_gettable(sa_family_t af)
{
	if (af >= __arraycount(rt_tables))
		return NULL;
	return rt_tables[af];
}

void
rtbl_init(void)
{
	struct domain *dom;
	DOMAIN_FOREACH(dom)
		if (dom->dom_rtattach)
			dom->dom_rtattach(&rt_tables[dom->dom_family],
			    dom->dom_rtoffset);
}

void
rt_assert_inactive(const struct rtentry *rt)
{
	if (rt->rt_nodes->rn_flags & (RNF_ACTIVE | RNF_ROOT))
		panic ("rtfree 2");
}

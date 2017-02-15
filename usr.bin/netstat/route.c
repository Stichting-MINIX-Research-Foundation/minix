/*	$NetBSD: route.c,v 1.84 2015/05/25 03:56:20 manu Exp $	*/

/*
 * Copyright (c) 1983, 1988, 1993
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
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "from: @(#)route.c	8.3 (Berkeley) 3/9/94";
#else
__RCSID("$NetBSD: route.c,v 1.84 2015/05/25 03:56:20 manu Exp $");
#endif
#endif /* not lint */

#include <stdbool.h>
#include <sys/param.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/un.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netatalk/at.h>
#include <netmpls/mpls.h>

#include <sys/sysctl.h>

#include <arpa/inet.h>

#include <err.h>
#include <kvm.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "netstat.h"
#include "rtutil.h"

#define kget(p, d) (kread((u_long)(p), (char *)&(d), sizeof (d)))

/*
 * XXX we put all of the sockaddr types in here to force the alignment
 * to be correct.
 */
static union sockaddr_union {
	struct	sockaddr u_sa;
	struct	sockaddr_in u_in;
	struct	sockaddr_un u_un;
	struct	sockaddr_at u_at;
	struct	sockaddr_dl u_dl;
	u_short	u_data[128];
	int u_dummy;		/* force word-alignment */
} pt_u;

int	do_rtent = 0;
struct	rtentry rtentry;
struct	radix_node rnode;
struct	radix_mask rmask;

static struct sockaddr *kgetsa(const struct sockaddr *);
static void p_tree(struct radix_node *);
static void p_rtnode(void);
static void p_krtentry(struct rtentry *);

/*
 * Print routing tables.
 */
void
routepr(u_long rtree)
{
	struct radix_node_head *rnh, head;
	struct radix_node_head *rt_nodes[AF_MAX+1];
	int i;

	printf("Routing tables\n");

	if (rtree == 0) {
		printf("rt_tables: symbol not in namelist\n");
		return;
	}

	kget(rtree, rt_nodes);
	for (i = 0; i <= AF_MAX; i++) {
		if ((rnh = rt_nodes[i]) == 0)
			continue;
		kget(rnh, head);
		if (i == AF_UNSPEC) {
			if (Aflag && (af == 0 || af == 0xff)) {
				printf("Netmasks:\n");
				p_tree(head.rnh_treetop);
			}
		} else if (af == AF_UNSPEC || af == i) {
			p_family(i);
			do_rtent = 1;
			p_rthdr(i, Aflag);
			p_tree(head.rnh_treetop);
		}
	}
}

static struct sockaddr *
kgetsa(const struct sockaddr *dst)
{

	kget(dst, pt_u.u_sa);
	if (pt_u.u_sa.sa_len > sizeof (pt_u.u_sa))
		kread((u_long)dst, (char *)pt_u.u_data, pt_u.u_sa.sa_len);
	return (&pt_u.u_sa);
}

static void
p_tree(struct radix_node *rn)
{

again:
	kget(rn, rnode);
	if (rnode.rn_b < 0) {
		if (Aflag)
			printf("%-8.8lx ", (u_long) rn);
		if (rnode.rn_flags & RNF_ROOT) {
			if (Aflag)
				printf("(root node)%s",
				    rnode.rn_dupedkey ? " =>\n" : "\n");
		} else if (do_rtent) {
			kget(rn, rtentry);
			p_krtentry(&rtentry);
			if (Aflag)
				p_rtnode();
		} else {
			p_sockaddr(kgetsa((const struct sockaddr *)rnode.rn_key),
			    NULL, 0, 44, nflag);
			putchar('\n');
		}
		if ((rn = rnode.rn_dupedkey) != NULL)
			goto again;
	} else {
		if (Aflag && do_rtent) {
			printf("%-8.8lx ", (u_long) rn);
			p_rtnode();
		}
		rn = rnode.rn_r;
		p_tree(rnode.rn_l);
		p_tree(rn);
	}
}

static void
p_rtnode(void)
{
	struct radix_mask *rm = rnode.rn_mklist;
	char	nbuf[20];

	if (rnode.rn_b < 0) {
		if (rnode.rn_mask) {
			printf("\t  mask ");
			p_sockaddr(kgetsa((const struct sockaddr *)rnode.rn_mask),
				    NULL, 0, -1, nflag);
		} else if (rm == 0)
			return;
	} else {
		(void)snprintf(nbuf, sizeof nbuf, "(%d)", rnode.rn_b);
		printf("%6.6s %8.8lx : %8.8lx", nbuf, (u_long) rnode.rn_l,
		    (u_long) rnode.rn_r);
	}
	while (rm) {
		kget(rm, rmask);
		(void)snprintf(nbuf, sizeof nbuf, " %d refs, ", rmask.rm_refs);
		printf(" mk = %8.8lx {(%d),%s", (u_long) rm,
		    -1 - rmask.rm_b, rmask.rm_refs ? nbuf : " ");
		if (rmask.rm_flags & RNF_NORMAL) {
			struct radix_node rnode_aux;
			printf(" <normal>, ");
			kget(rmask.rm_leaf, rnode_aux);
			p_sockaddr(kgetsa((const struct sockaddr *)rnode_aux.rn_mask),
				    NULL, 0, -1, nflag);
		} else
			p_sockaddr(kgetsa((const struct sockaddr *)rmask.rm_mask),
			    NULL, 0, -1, nflag);
		putchar('}');
		if ((rm = rmask.rm_mklist) != NULL)
			printf(" ->");
	}
	putchar('\n');
}

static struct sockaddr *sockcopy(struct sockaddr *, union sockaddr_union *);

/*
 * copy a sockaddr into an allocated region, allocate at least sockaddr
 * bytes and zero unused
 */
static struct sockaddr *
sockcopy(struct sockaddr *sp, union sockaddr_union *dp)
{
	int len;

	if (sp == 0 || sp->sa_len == 0)
		(void)memset(dp, 0, sizeof (*sp));
	else {
		len = (sp->sa_len >= sizeof (*sp)) ? sp->sa_len : sizeof (*sp);
		(void)memcpy(dp, sp, len);
	}
	return ((struct sockaddr *)dp);
}

static void
p_krtentry(struct rtentry *rt)
{
	static struct ifnet ifnet, *lastif;
	union sockaddr_union addr_un, mask_un;
	struct sockaddr *addr, *mask;

	if (Lflag && (rt->rt_flags & RTF_LLINFO))
		return;

	memset(&addr_un, 0, sizeof(addr_un));
	memset(&mask_un, 0, sizeof(mask_un));
	addr = sockcopy(kgetsa(rt_getkey(rt)), &addr_un);
	if (rt_mask(rt))
		mask = sockcopy(kgetsa(rt_mask(rt)), &mask_un);
	else
		mask = sockcopy(NULL, &mask_un);
	p_addr(addr, mask, rt->rt_flags, nflag);
	p_gwaddr(kgetsa(rt->rt_gateway), kgetsa(rt->rt_gateway)->sa_family, nflag);
	p_flags(rt->rt_flags);
	printf("%6d %8"PRIu64" ", rt->rt_refcnt, rt->rt_use);
	if (rt->rt_rmx.rmx_mtu)
		printf("%6"PRIu64, rt->rt_rmx.rmx_mtu); 
	else
		printf("%6s", "-");
	putchar((rt->rt_rmx.rmx_locks & RTV_MTU) ? 'L' : ' ');
	if (tagflag == 1) {
#ifndef SMALL
		if (rt->rt_tag != NULL) {
			const struct sockaddr *tagsa = kgetsa(rt->rt_tag);
			char *tagstr;

			if (tagsa->sa_family == AF_MPLS) {
				tagstr = mpls_ntoa(tagsa);
				if (strlen(tagstr) < 7)
					printf("%7s", tagstr);
				else
					printf("%s", tagstr);
			}
			else
				printf("%7s", "-");
		} else
#endif
			printf("%7s", "-");
	}
	if (rt->rt_ifp) {
		if (rt->rt_ifp != lastif) {
			kget(rt->rt_ifp, ifnet);
			lastif = rt->rt_ifp;
		}
		printf(" %.16s%s", ifnet.if_xname,
			rt->rt_nodes[0].rn_dupedkey ? " =>" : "");
	}
	putchar('\n');
#ifndef SMALL
	if (vflag)
		p_rtrmx(&rt->rt_rmx);
#endif
}

/*
 * Print routing statistics
 */
void
rt_stats(u_long off)
{
	struct rtstat rtstats;

	if (use_sysctl) {
		size_t rtsize = sizeof(rtstats);

		if (sysctlbyname("net.route.stats", &rtstats, &rtsize,
		    NULL, 0) == -1)
			err(1, "rt_stats: sysctl");
	} else 	if (off == 0) {
		printf("rtstat: symbol not in namelist\n");
		return;
	} else
		kread(off, (char *)&rtstats, sizeof(rtstats));

	printf("routing:\n");
	printf("\t%llu bad routing redirect%s\n",
		(unsigned long long)rtstats.rts_badredirect,
		plural(rtstats.rts_badredirect));
	printf("\t%llu dynamically created route%s\n",
		(unsigned long long)rtstats.rts_dynamic,
		plural(rtstats.rts_dynamic));
	printf("\t%llu new gateway%s due to redirects\n",
		(unsigned long long)rtstats.rts_newgateway,
		plural(rtstats.rts_newgateway));
	printf("\t%llu destination%s found unreachable\n",
		(unsigned long long)rtstats.rts_unreach,
		plural(rtstats.rts_unreach));
	printf("\t%llu use%s of a wildcard route\n",
		(unsigned long long)rtstats.rts_wildcard,
		plural(rtstats.rts_wildcard));
}

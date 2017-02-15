/*	$NetBSD: mroute6.c,v 1.15 2014/11/06 21:30:09 christos Exp $	*/

/*
 * Copyright (C) 1998 WIDE Project.
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
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Stephen Deering of Stanford University.
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
 *	@(#)mroute.c	8.2 (Berkeley) 4/28/95
 */

/*
 * Copyright (c) 1989 Stephen Deering
 *
 * This code is derived from software contributed to Berkeley by
 * Stephen Deering of Stanford University.
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
 *	California, Berkeley and its contributors.
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
 *	@(#)mroute.c	8.2 (Berkeley) 4/28/95
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>

#include <net/if.h>

#include <netinet/in.h>

#define _KERNEL 1
#include <netinet6/ip6_mroute.h>
#undef _KERNEL

#include <stdio.h>
#include <kvm.h>
#include "netstat.h"
#include "rtutil.h"

#ifdef INET6

#define	WID_ORG	(lflag ? 39 : (numeric_addr ? 29 : 18)) /* width of origin column */
#define	WID_GRP	(lflag ? 18 : (numeric_addr ? 16 : 18)) /* width of group column */

void
mroute6pr(u_long mrpaddr, u_long mfcaddr, u_long mifaddr)
{
	u_int mrtproto;
	struct mf6c *mf6ctable[MF6CTBLSIZ], *mfcp;
	struct mif6 mif6table[MAXMIFS];
	struct mf6c mfc;
	struct rtdetq rte, *rtep;
	register struct mif6 *mifp;
	register mifi_t mifi;
	register int i;
	register int banner_printed;
	register int saved_numeric_addr;
	int waitings;

	if (mrpaddr == 0) {
		printf("mroute6pr: symbol not in namelist\n");
		return;
	}

	kread(mrpaddr, (char *)&mrtproto, sizeof(mrtproto));
	switch (mrtproto) {
	case 0:
		printf("no IPv6 multicast routing compiled into this system\n");
		return;

	case IPPROTO_PIM:
		break;

	default:
		printf("IPv6 multicast routing protocol %u, unknown\n",
		    mrtproto);
		return;
	}

	if (mfcaddr == 0) {
		printf("mf6ctable: symbol not in namelist\n");
		return;
	}
	if (mifaddr == 0) {
		printf("miftable: symbol not in namelist\n");
		return;
	}

	saved_numeric_addr = numeric_addr;
	numeric_addr = 1;

	kread(mifaddr, (char *)&mif6table, sizeof(mif6table));
	banner_printed = 0;
	for (mifi = 0, mifp = mif6table; mifi < MAXMIFS; ++mifi, ++mifp) {
		struct ifnet ifnet;
		char ifname[IFNAMSIZ];

		if (mifp->m6_ifp == NULL)
			continue;

		kread((u_long)mifp->m6_ifp, (char *)&ifnet, sizeof(ifnet));
		if (!banner_printed) {
			printf("\nIPv6 Multicast Interface Table\n"
			    " Mif   Rate   PhyIF   Pkts-In   Pkts-Out\n");
			banner_printed = 1;
		}

		printf("  %2u   %4d", mifi, mifp->m6_rate_limit);
		printf("   %5s", (mifp->m6_flags & MIFF_REGISTER) ?
		    "reg0" : if_indextoname(ifnet.if_index, ifname));

		printf(" %9llu  %9llu\n", (unsigned long long)mifp->m6_pkt_in,
		    (unsigned long long)mifp->m6_pkt_out);
	}
	if (!banner_printed)
		printf("\nIPv6 Multicast Interface Table is empty\n");

	kread(mfcaddr, (char *)&mf6ctable, sizeof(mf6ctable));
	banner_printed = 0;
	for (i = 0; i < MF6CTBLSIZ; ++i) {
		mfcp = mf6ctable[i];
		while(mfcp) {
			kread((u_long)mfcp, (char *)&mfc, sizeof(mfc));
			if (!banner_printed) {
				printf ("\nIPv6 Multicast Forwarding Cache\n");
				printf(" %-*.*s %-*.*s %s",
				    WID_ORG, WID_ORG, "Origin",
				    WID_GRP, WID_GRP, "Group",
				    "  Packets Waits In-Mif  Out-Mifs\n");
				banner_printed = 1;
			}
			
			printf(" %-*.*s", WID_ORG, WID_ORG,
			    routename6(&mfc.mf6c_origin, nflag));
			printf(" %-*.*s", WID_GRP, WID_GRP,
			    routename6(&mfc.mf6c_mcastgrp, nflag));
			printf(" %9llu", (unsigned long long)mfc.mf6c_pkt_cnt);

			for (waitings = 0, rtep = mfc.mf6c_stall; rtep; ) {
				waitings++;
				kread((u_long)rtep, (char *)&rte, sizeof(rte));
				rtep = rte.next;
			}
			printf("   %3d", waitings);

			if (mfc.mf6c_parent == MF6C_INCOMPLETE_PARENT)
				printf("  ---   ");
			else
				printf("  %3d   ", mfc.mf6c_parent);
			for (mifi = 0; mifi <= MAXMIFS; mifi++) {
				if (IF_ISSET(mifi, &mfc.mf6c_ifset))
					printf(" %u", mifi);
			}
			printf("\n");

			mfcp = mfc.mf6c_next;
		}
	}
	if (!banner_printed)
		printf("\nIPv6 Multicast Routing Table is empty\n");

	printf("\n");
	numeric_addr = saved_numeric_addr;
}

void
mrt6_stats(u_long mrpaddr, u_long mstaddr)
{
#define	p(f, m) printf(m, (unsigned long long)mrtstat.f, plural(mrtstat.f))
#define	pes(f, m) printf(m, (unsigned long long)mrtstat.f, plurales(mrtstat.f))
	u_int mrtproto;
	struct mrt6stat mrtstat;

	if (mrpaddr == 0) {
		printf("mrt6_stats: symbol not in namelist\n");
		return;
	}

	kread(mrpaddr, (char *)&mrtproto, sizeof(mrtproto));
	switch (mrtproto) {
	case 0:
		printf("no IPv6 multicast routing compiled into this system\n");
		return;

	case IPPROTO_PIM:
		break;

	default:
		printf("IPv6 multicast routing protocol %u, unknown\n",
			mrtproto);
		return;
	}

	if (mstaddr == 0) {
		printf("mrt6_stats: symbol not in namelist\n");
		return;
	}

	kread(mstaddr, (char *)&mrtstat, sizeof(mrtstat));
	printf("multicast forwarding:\n");
	p(mrt6s_mfc_lookups, " %10llu multicast forwarding cache lookup%s\n");
	pes(mrt6s_mfc_misses, " %10llu multicast forwarding cache miss%s\n");
	p(mrt6s_upcalls, " %10llu upcall%s to mrouted\n");
	p(mrt6s_upq_ovflw, " %10llu upcall queue overflow%s\n");
	p(mrt6s_upq_sockfull,
	    " %10llu upcall%s dropped due to full socket buffer\n");
	p(mrt6s_cache_cleanups, " %10llu cache cleanup%s\n");
	p(mrt6s_no_route, " %10llu datagram%s with no route for origin\n");
	p(mrt6s_bad_tunnel, " %10llu datagram%s arrived with bad tunneling\n");
	p(mrt6s_cant_tunnel, " %10llu datagram%s could not be tunneled\n");
	p(mrt6s_wrong_if, " %10llu datagram%s arrived on wrong interface\n");
	p(mrt6s_drop_sel, " %10llu datagram%s selectively dropped\n");
	p(mrt6s_q_overflow,
	    " %10llu datagram%s dropped due to queue overflow\n");
	p(mrt6s_pkt2large, " %10llu datagram%s dropped for being too large\n");
#undef p
#undef pes
}
#endif /*INET6*/

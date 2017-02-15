/*	$NetBSD: mroute.c,v 1.25 2014/11/06 21:30:09 christos Exp $	*/

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
 *	from: @(#)mroute.c	8.1 (Berkeley) 6/6/93
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
 *	from: @(#)mroute.c	8.1 (Berkeley) 6/6/93
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "from: @(#)mroute.c	8.1 (Berkeley) 6/6/93";
#else
__RCSID("$NetBSD: mroute.c,v 1.25 2014/11/06 21:30:09 christos Exp $");
#endif
#endif /* not lint */

/*
 * Print multicast routing structures and statistics.
 *
 * MROUTING 1.0
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>

#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/igmp.h>
#define _KERNEL
#include <netinet/ip_mroute.h>
#undef _KERNEL

#include <stdio.h>
#include <stdlib.h>
#include <kvm.h>
#include "netstat.h"
#include "rtutil.h"

static char *pktscale(u_long);
static void print_bw_meter(struct bw_meter *, int *);

static char *
pktscale(u_long n)
{
	static char buf[20];
	char t;

	if (n < 1024)
		t = ' ';
	else if (n < 1024 * 1024) {
		t = 'k';
		n /= 1024;
	} else {
		t = 'm';
		n /= 1048576;
	}

	(void)snprintf(buf, sizeof buf, "%lu%c", n, t);
	return (buf);
}

void
mroutepr(u_long mrpaddr, u_long mfchashtbladdr, u_long mfchashaddr,
	u_long vifaddr)
{
	u_int mrtproto;
	LIST_HEAD(, mfc) *mfchashtbl;
	u_long mfchash, i;
	struct vif viftable[MAXVIFS];
	struct mfc *mfcp, mfc;
	struct vif *v;
	vifi_t vifi;
	int banner_printed;
	int saved_numeric_addr;
	int numvifs;
	int nmfc;		/* No. of cache entries */

	if (mrpaddr == 0) {
		printf("ip_mrtproto: symbol not in namelist\n");
		return;
	}

	kread(mrpaddr, (char *)&mrtproto, sizeof(mrtproto));
	switch (mrtproto) {
	case 0:
		printf("no multicast routing compiled into this system\n");
		return;

	case IGMP_DVMRP:
		break;

	default:
		printf("multicast routing protocol %u, unknown\n", mrtproto);
		return;
	}

	if (mfchashtbladdr == 0) {
		printf("mfchashtbl: symbol not in namelist\n");
		return;
	}
	if (mfchashaddr == 0) {
		printf("mfchash: symbol not in namelist\n");
		return;
	}
	if (vifaddr == 0) {
		printf("viftable: symbol not in namelist\n");
		return;
	}

	saved_numeric_addr = numeric_addr;
	numeric_addr = 1;

	kread(vifaddr, (char *)&viftable, sizeof(viftable));
	banner_printed = 0;
	numvifs = 0;

	for (vifi = 0, v = viftable; vifi < MAXVIFS; ++vifi, ++v) {
		if (v->v_lcl_addr.s_addr == 0)
			continue;
		numvifs = vifi;

		if (!banner_printed) {
			printf("\nVirtual Interface Table\n %s%s",
			    "Vif  Thresh  Limit  Local-Address    ",
			    "Remote-Address   Pkt_in  Pkt_out\n");
			banner_printed = 1;
		}

		printf(" %3u     %3u  %5u  %-15.15s",
		    vifi, v->v_threshold, v->v_rate_limit,
		    routename4(v->v_lcl_addr.s_addr, nflag));
		printf("  %-15.15s  %6lu  %7lu\n", (v->v_flags & VIFF_TUNNEL) ?
		    routename4(v->v_rmt_addr.s_addr, nflag) : "",
		    v->v_pkt_in, v->v_pkt_out);
	}
	if (!banner_printed)
		printf("\nVirtual Interface Table is empty\n");

	kread(mfchashtbladdr, (char *)&mfchashtbl, sizeof(mfchashtbl));
	kread(mfchashaddr, (char *)&mfchash, sizeof(mfchash));
	banner_printed = 0;
	nmfc = 0;

	if (mfchashtbl != 0)
	for (i = 0; i <= mfchash; ++i) {
		kread((u_long)&mfchashtbl[i], (char *)&mfcp, sizeof(mfcp));

		for (; mfcp != 0; mfcp = mfc.mfc_hash.le_next) {
			if (!banner_printed) {
				printf("\nMulticast Forwarding Cache\n %s%s",
				    "Hash  Origin           Mcastgroup       ",
				    "Traffic  In-Vif  Out-Vifs/Forw-ttl\n");
				banner_printed = 1;
			}

			kread((u_long)mfcp, (char *)&mfc, sizeof(mfc));
			printf("  %3lu  %-15.15s",
			    i, routename4(mfc.mfc_origin.s_addr, nflag));
			printf("  %-15.15s  %7s     %3u ",
			    routename4(mfc.mfc_mcastgrp.s_addr, nflag),
			    pktscale(mfc.mfc_pkt_cnt), mfc.mfc_parent);
			for (vifi = 0; vifi <= numvifs; ++vifi)
				if (mfc.mfc_ttls[vifi])
					printf(" %u/%u", vifi, mfc.mfc_ttls[vifi]);

			printf("\n");

			/* Print the bw meter information */
			{
				struct bw_meter bw_meter, *bwm;
				int banner_printed2 = 0;

				bwm = mfc.mfc_bw_meter;
				while (bwm) {
				kread((u_long)bwm,
				      (char *)&bw_meter,
				      sizeof bw_meter);
				print_bw_meter(&bw_meter,
					       &banner_printed2);
				bwm = bw_meter.bm_mfc_next;
				}
#if 0	/* Don't ever print it? */
				if (! banner_printed2)
				    printf("\n  No Bandwidth Meters\n");
#endif
			}

			nmfc++;
		}
	}
	if (!banner_printed)
		printf("\nMulticast Forwarding Cache is empty\n");
	else
		printf("\nTotal no. of entries in cache: %d\n", nmfc);

	printf("\n");
	numeric_addr = saved_numeric_addr;
}

static void
print_bw_meter(struct bw_meter *bw_meter, int *banner_printed)
{
	char s0[256], s1[256], s2[256], s3[256];
	struct timeval now, end, delta;

	gettimeofday(&now, NULL);

	if (! *banner_printed) {
		printf(" Bandwidth Meters\n");
		printf("  %-30s", "Measured(Start|Packets|Bytes)");
		printf(" %s", "Type");
		printf("  %-30s", "Thresh(Interval|Packets|Bytes)");
		printf(" Remain");
		printf("\n");
		*banner_printed = 1;
	}

	/* The measured values */
	if (bw_meter->bm_flags & BW_METER_UNIT_PACKETS)
		sprintf(s1, "%llu", (unsigned long long)bw_meter->bm_measured.b_packets);
	else
		sprintf(s1, "?");
	if (bw_meter->bm_flags & BW_METER_UNIT_BYTES)
		sprintf(s2, "%llu", (unsigned long long)bw_meter->bm_measured.b_bytes);
	else
		sprintf(s2, "?");
	sprintf(s0, "%lld.%ld|%s|%s",
		(long long)bw_meter->bm_start_time.tv_sec,
		(long)bw_meter->bm_start_time.tv_usec,
		s1, s2);
	printf("  %-30s", s0);

	/* The type of entry */
	sprintf(s0, "%s", "?");
	if (bw_meter->bm_flags & BW_METER_GEQ)
		sprintf(s0, "%s", ">=");
	else if (bw_meter->bm_flags & BW_METER_LEQ)
		sprintf(s0, "%s", "<=");
	printf("  %-3s", s0);

	/* The threshold values */
	if (bw_meter->bm_flags & BW_METER_UNIT_PACKETS)
		sprintf(s1, "%llu", (unsigned long long)bw_meter->bm_threshold.b_packets);
	else
		sprintf(s1, "?");
	if (bw_meter->bm_flags & BW_METER_UNIT_BYTES)
		sprintf(s2, "%llu", (unsigned long long)bw_meter->bm_threshold.b_bytes);
	else
		sprintf(s2, "?");
	sprintf(s0, "%lld.%ld|%s|%s",
		(long long)bw_meter->bm_threshold.b_time.tv_sec,
		(long)bw_meter->bm_threshold.b_time.tv_usec,
		s1, s2);
	printf("  %-30s", s0);

	/* Remaining time */
	timeradd(&bw_meter->bm_start_time,
		 &bw_meter->bm_threshold.b_time, &end);
	if (timercmp(&now, &end, <=)) {
		timersub(&end, &now, &delta);
		sprintf(s3, "%lld.%ld",
		    (long long)delta.tv_sec, (long)delta.tv_usec);
	} else {
		/* Negative time */
		timersub(&now, &end, &delta);
		sprintf(s3, "-%lld.%ld",
		    (long long)delta.tv_sec, (long)delta.tv_usec);
	}
	printf(" %s", s3);

	printf("\n");
}

void
mrt_stats(u_long mrpaddr, u_long mstaddr)
{
	u_int mrtproto;
	struct mrtstat mrtstat;

	if (mrpaddr == 0) {
		printf("ip_mrtproto: symbol not in namelist\n");
		return;
	}

	kread(mrpaddr, (char *)&mrtproto, sizeof(mrtproto));
	switch (mrtproto) {
	case 0:
		printf("no multicast routing compiled into this system\n");
		return;

	case IGMP_DVMRP:
		break;

	default:
		printf("multicast routing protocol %u, unknown\n", mrtproto);
		return;
	}

	if (mstaddr == 0) {
		printf("mrtstat: symbol not in namelist\n");
		return;
	}

	kread(mstaddr, (char *)&mrtstat, sizeof(mrtstat));
	printf("multicast routing:\n");
	printf("\t%lu datagram%s with no route for origin\n",
	    mrtstat.mrts_no_route, plural(mrtstat.mrts_no_route));
	printf("\t%lu upcall%s made to mrouted\n",
	    mrtstat.mrts_upcalls, plural(mrtstat.mrts_upcalls));
	printf("\t%lu datagram%s with malformed tunnel options\n",
	    mrtstat.mrts_bad_tunnel, plural(mrtstat.mrts_bad_tunnel));
	printf("\t%lu datagram%s with no room for tunnel options\n",
	    mrtstat.mrts_cant_tunnel, plural(mrtstat.mrts_cant_tunnel));
	printf("\t%lu datagram%s arrived on wrong interface\n",
	    mrtstat.mrts_wrong_if, plural(mrtstat.mrts_wrong_if));
	printf("\t%lu datagram%s dropped due to upcall Q overflow\n",
	    mrtstat.mrts_upq_ovflw, plural(mrtstat.mrts_upq_ovflw));
	printf("\t%lu datagram%s dropped due to upcall socket overflow\n",
	    mrtstat.mrts_upq_sockfull, plural(mrtstat.mrts_upq_sockfull));
	printf("\t%lu datagram%s cleaned up by the cache\n",
	    mrtstat.mrts_cache_cleanups, plural(mrtstat.mrts_cache_cleanups));
	printf("\t%lu datagram%s dropped selectively by ratelimiter\n",
	    mrtstat.mrts_drop_sel, plural(mrtstat.mrts_drop_sel));
	printf("\t%lu datagram%s dropped - bucket Q overflow\n",
	    mrtstat.mrts_q_overflow, plural(mrtstat.mrts_q_overflow));
	printf("\t%lu datagram%s dropped - larger than bkt size\n",
	    mrtstat.mrts_pkt2large, plural(mrtstat.mrts_pkt2large));
}

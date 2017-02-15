/*	$NetBSD: mbuf.c,v 1.33 2015/07/28 19:46:42 christos Exp $	*/

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
static char sccsid[] = "from: @(#)mbuf.c	8.1 (Berkeley) 6/6/93";
#else
__RCSID("$NetBSD: mbuf.c,v 1.33 2015/07/28 19:46:42 christos Exp $");
#endif
#endif /* not lint */

#define	__POOL_EXPOSE

#include <sys/param.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/pool.h>
#include <sys/sysctl.h>

#include <stdio.h>
#include <kvm.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <err.h>
#include "netstat.h"
#include "prog_ops.h"

#define	YES	1

struct	mbstat mbstat;
struct pool mbpool, mclpool;
struct pool_allocator mbpa, mclpa;

static struct mbtypes {
	int		mt_type;
	const char	*mt_name;
} mbtypes[] = {
	{ MT_DATA,	"data" },
	{ MT_OOBDATA,	"oob data" },
	{ MT_CONTROL,	"ancillary data" },
	{ MT_HEADER,	"packet headers" },
	{ MT_FTABLE,	"fragment reassembly queue headers" },	/* XXX */
	{ MT_SONAME,	"socket names and addresses" },
	{ MT_SOOPTS,	"socket options" },
	{ 0, 0 }
};

const int nmbtypes = sizeof(mbstat.m_mtypes) / sizeof(short);
bool seen[256];			/* "have we seen this type yet?" */

int mbstats_ctl[] = { CTL_KERN, KERN_MBUF, MBUF_STATS };
int mowners_ctl[] = { CTL_KERN, KERN_MBUF, MBUF_MOWNERS };

/*
 * Print mbuf statistics.
 */
void
mbpr(u_long mbaddr, u_long msizeaddr, u_long mclbaddr, u_long mbpooladdr,
	u_long mclpooladdr)
{
	u_long totmem, totused, totpct;
	u_int totmbufs;
	int i, lines;
	struct mbtypes *mp;
	size_t len;
	void *data;
	struct mowner_user *mo;
	int mclbytes, msize;

	if (nmbtypes != 256) {
		fprintf(stderr,
		    "%s: unexpected change to mbstat; check source\n",
		        getprogname());
		return;
	}

	if (use_sysctl) {
		size_t mbstatlen = sizeof(mbstat);
		if (prog_sysctl(mbstats_ctl,
			    sizeof(mbstats_ctl) / sizeof(mbstats_ctl[0]),
			    &mbstat, &mbstatlen, NULL, 0) < 0) {
			warn("mbstat: sysctl failed");
			return;
		}
		goto printit;
	}

	if (mbaddr == 0) {
		fprintf(stderr, "%s: mbstat: symbol not in namelist\n",
		    getprogname());
		return;
	}
/*XXX*/
	if (msizeaddr != 0)
		kread(msizeaddr, (char *)&msize, sizeof (msize));
	else
		msize = MSIZE;
	if (mclbaddr != 0)
		kread(mclbaddr, (char *)&mclbytes, sizeof (mclbytes));
	else
		mclbytes = MCLBYTES;
/*XXX*/

	if (kread(mbaddr, (char *)&mbstat, sizeof (mbstat)))
		return;

	if (kread(mbpooladdr, (char *)&mbpool, sizeof (mbpool)))
		return;

	if (kread(mclpooladdr, (char *)&mclpool, sizeof (mclpool)))
		return;

	mbpooladdr = (u_long) mbpool.pr_alloc;
	mclpooladdr = (u_long) mclpool.pr_alloc;

	if (kread(mbpooladdr, (char *)&mbpa, sizeof (mbpa)))
		return;

	if (kread(mclpooladdr, (char *)&mclpa, sizeof (mclpa)))
		return;

    printit:
	totmbufs = 0;
	for (mp = mbtypes; mp->mt_name; mp++)
		totmbufs += mbstat.m_mtypes[mp->mt_type];
	printf("%u mbufs in use:\n", totmbufs);
	for (mp = mbtypes; mp->mt_name; mp++)
		if (mbstat.m_mtypes[mp->mt_type]) {
			seen[mp->mt_type] = YES;
			printf("\t%u mbufs allocated to %s\n",
			    mbstat.m_mtypes[mp->mt_type], mp->mt_name);
		}
	seen[MT_FREE] = YES;
	for (i = 0; i < nmbtypes; i++)
		if (!seen[i] && mbstat.m_mtypes[i]) {
			printf("\t%u mbufs allocated to <mbuf type %d>\n",
			    mbstat.m_mtypes[i], i);
		}

	if (use_sysctl)		/* XXX */
		goto dump_drain;

	printf("%lu/%lu mapped pages in use\n",
	       (u_long)(mclpool.pr_nget - mclpool.pr_nput),
	       ((u_long)mclpool.pr_npages * mclpool.pr_itemsperpage));
	totmem = (mbpool.pr_npages << mbpa.pa_pageshift) +
	    (mclpool.pr_npages << mclpa.pa_pageshift);
	totused = (mbpool.pr_nget - mbpool.pr_nput) * mbpool.pr_size +
	    (mclpool.pr_nget - mclpool.pr_nput) * mclpool.pr_size;
	if (totmem == 0)
		totpct = 0;
	else if (totused < (ULONG_MAX/100))
		totpct = (totused * 100)/totmem;
	else {
		u_long totmem1 = totmem/100;
		u_long totused1 = totused/100;
		totpct = (totused1 * 100)/totmem1;
	}
	
	printf("%lu Kbytes allocated to network (%lu%% in use)\n",
	    totmem / 1024, totpct);

dump_drain:
	printf("%lu calls to protocol drain routines\n", mbstat.m_drain);

 	if (sflag < 2)
		return;

	if (!use_sysctl)
		return;

	if (prog_sysctl(mowners_ctl,
	    sizeof(mowners_ctl)/sizeof(mowners_ctl[0]),
	    NULL, &len, NULL, 0) < 0) {
		if (errno == ENOENT)
			return;
		warn("mowners: sysctl test");
		return;
	}
	len += 10 * sizeof(*mo);		/* add some slop */
	data = malloc(len);
	if (data == NULL) {
		warn("malloc(%lu)", (u_long)len);
		return;
	}

	if (prog_sysctl(mowners_ctl,
	    sizeof(mowners_ctl)/sizeof(mowners_ctl[0]),
	    data, &len, NULL, 0) < 0) {
		warn("mowners: sysctl get");
		free(data);
		return;
	}

	for (mo = (void *) data, lines = 0; len >= sizeof(*mo);
	    len -= sizeof(*mo), mo++) {
		char buf[32];
		if (vflag == 1 &&
		    mo->mo_counter[MOWNER_COUNTER_CLAIMS] == 0 &&
		    mo->mo_counter[MOWNER_COUNTER_EXT_CLAIMS] == 0 &&
		    mo->mo_counter[MOWNER_COUNTER_CLUSTER_CLAIMS] == 0)
			continue;
		if (vflag == 0 &&
		    mo->mo_counter[MOWNER_COUNTER_CLAIMS] ==
		    mo->mo_counter[MOWNER_COUNTER_RELEASES] &&
		    mo->mo_counter[MOWNER_COUNTER_EXT_CLAIMS] ==
		    mo->mo_counter[MOWNER_COUNTER_EXT_RELEASES] &&
		    mo->mo_counter[MOWNER_COUNTER_CLUSTER_CLAIMS] ==
		    mo->mo_counter[MOWNER_COUNTER_CLUSTER_RELEASES])
			continue;
		snprintf(buf, sizeof(buf), "%16s %-13s",
		    mo->mo_name, mo->mo_descr);
		if ((lines % 24) == 0 || lines > 24) {
			printf("%30s %-8s %10s %10s %10s\n",
			    "", "", "small", "ext", "cluster");
			lines = 1;
		}
		printf("%30s %-8s %10lu %10lu %10lu\n",
		    buf, "inuse",
		    mo->mo_counter[MOWNER_COUNTER_CLAIMS] -
		    mo->mo_counter[MOWNER_COUNTER_RELEASES],
		    mo->mo_counter[MOWNER_COUNTER_EXT_CLAIMS] -
		    mo->mo_counter[MOWNER_COUNTER_EXT_RELEASES],
		    mo->mo_counter[MOWNER_COUNTER_CLUSTER_CLAIMS] -
		    mo->mo_counter[MOWNER_COUNTER_CLUSTER_RELEASES]);
		lines++;
		if (vflag) {
			printf("%30s %-8s %10lu %10lu %10lu\n",
			    "", "claims",
			    mo->mo_counter[MOWNER_COUNTER_CLAIMS],
			    mo->mo_counter[MOWNER_COUNTER_EXT_CLAIMS],
			    mo->mo_counter[MOWNER_COUNTER_CLUSTER_CLAIMS]);
			printf("%30s %-8s %10lu %10lu %10lu\n",
			    "", "releases",
			    mo->mo_counter[MOWNER_COUNTER_RELEASES],
			    mo->mo_counter[MOWNER_COUNTER_EXT_RELEASES],
			    mo->mo_counter[MOWNER_COUNTER_CLUSTER_RELEASES]);
			lines += 2;
		}
	}
	free(data);
}

/*	$NetBSD: pfsync.c,v 1.1 2011/03/01 19:01:59 dyoung Exp $	*/

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
__RCSID("$NetBSD: pfsync.c,v 1.1 2011/03/01 19:01:59 dyoung Exp $");
#endif /* not lint */

#define	_CALLOUT_PRIVATE	/* for defs in sys/callout.h */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/sysctl.h>

#include <net/if_arp.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_icmp.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif

#include <net/pfvar.h>
#include <net/if_pfsync.h>

#include <arpa/inet.h>
#include <kvm.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include "netstat.h"
#include "prog_ops.h"

/*
 * Dump PFSYNC statistics structure.
 */
void
pfsync_stats(u_long off, const char *name)
{
	uint64_t pfsyncstat[PFSYNC_NSTATS];

	if (use_sysctl) {
		size_t size = sizeof(pfsyncstat);

		if (sysctlbyname("net.inet.pfsync.stats", pfsyncstat, &size,
				 NULL, 0) == -1)
			return;
	} else {
		warnx("%s stats not available via KVM.", name);
		return;
	}

	printf("%s:\n", name);

#define p(f, m) if (pfsyncstat[f] || sflag <= 1) \
	printf(m, pfsyncstat[f], plural(pfsyncstat[f]))
#define p2(f, m) if (pfsyncstat[f] || sflag <= 1) \
	printf(m, pfsyncstat[f])

	p(PFSYNC_STAT_IPACKETS, "\t%" PRIu64 " packet%s received (IPv4)\n");
	p(PFSYNC_STAT_IPACKETS6,"\t%" PRIu64 " packet%s received (IPv6)\n");
	p(PFSYNC_STAT_BADIF, "\t\t%" PRIu64 " packet%s discarded for bad interface\n");
	p(PFSYNC_STAT_BADTTL, "\t\t%" PRIu64 " packet%s discarded for bad ttl\n");
	p(PFSYNC_STAT_HDROPS, "\t\t%" PRIu64 " packet%s shorter than header\n");
	p(PFSYNC_STAT_BADVER, "\t\t%" PRIu64 " packet%s discarded for bad version\n");
	p(PFSYNC_STAT_BADAUTH, "\t\t%" PRIu64 " packet%s discarded for bad HMAC\n");
	p(PFSYNC_STAT_BADACT,"\t\t%" PRIu64 " packet%s discarded for bad action\n");
	p(PFSYNC_STAT_BADLEN, "\t\t%" PRIu64 " packet%s discarded for short packet\n");
	p(PFSYNC_STAT_BADVAL, "\t\t%" PRIu64 " state%s discarded for bad values\n");
	p(PFSYNC_STAT_STALE, "\t\t%" PRIu64 " stale state%s\n");
	p(PFSYNC_STAT_BADSTATE, "\t\t%" PRIu64 " failed state lookup/insert%s\n");
	p(PFSYNC_STAT_OPACKETS, "\t%" PRIu64 " packet%s sent (IPv4)\n");
	p(PFSYNC_STAT_OPACKETS6, "\t%" PRIu64 " packet%s sent (IPv6)\n");
	p2(PFSYNC_STAT_ONOMEM, "\t\t%" PRIu64 " send failed due to mbuf memory error\n");
	p2(PFSYNC_STAT_OERRORS, "\t\t%" PRIu64 " send error\n");
#undef p
#undef p2
}



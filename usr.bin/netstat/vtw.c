/*	$NetBSD: vtw.c,v 1.8 2015/06/16 22:54:10 christos Exp $	*/

/*
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Coyote Point Systems, Inc.
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
static char sccsid[] = "from: @(#)inet.c	8.4 (Berkeley) 4/20/94";
#else
__RCSID("$NetBSD: vtw.c,v 1.8 2015/06/16 22:54:10 christos Exp $");
#endif
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

#include <netinet/icmp_var.h>
#include <netinet/igmp_var.h>
#include <netinet/ip_var.h>
#include <netinet/pim_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_debug.h>
#include <netinet/udp.h>
#include <netinet/ip_carp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp_vtw.h>

#include <arpa/inet.h>
#include <kvm.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include "netstat.h"
#include "vtw.h"
#include "prog_ops.h"

static void	snarf(const void *, void *, size_t);
static void	*lookup(const char *);
static void	process_vtw(const vtw_ctl_t *, void (*)(const vtw_t *));

static void 
snarf(const void *addr, void *buf, size_t len)
{
	size_t cc;

	memset(buf, 0, len);

	cc = kvm_read(get_kvmd(), (unsigned long) addr, buf, len);

	if (cc != len) {
		warnx("%s: short read at %p, len %zx cc %zx", __func__, addr,
		    len, cc);
	}
}

static void *
lookup(const char *name)
{
	kvm_t *k;
	struct nlist nl[2];

	nl[0].n_name = name;
	nl[0].n_value = 0;
	nl[1].n_name = NULL;

	if ((k = get_kvmd()) == NULL) {
		if (Vflag)
			errx(EXIT_FAILURE, "kvm not available");
		return NULL;
	}
	switch (kvm_nlist(k, &nl[0])) {
	case -1:
		err(EXIT_FAILURE, "kvm_nlist");
		break;

	case 0:
		return (void *)nl[0].n_value;

	default:
		if (Vflag)
			errx(EXIT_FAILURE, "%s missing in symbol table", name);
		break;
	}

	return NULL;
}

void
timebase(struct timeval *tv)
{
	void *p;
	struct bintime timebasebin;

	p = lookup("timebasebin");
	if (!p)
		return;
	snarf(p, &timebasebin, sizeof(timebasebin));
	bintime2timeval(&timebasebin, tv);
}

static void 
process_vtw(const vtw_ctl_t * ctl, void (*print)(const vtw_t *))
{
	vtw_t *vp;

	for (vp = ctl->base.v; vp && vp <= ctl->lim.v;) {

		(*print)(vp);

		if (ctl->is_v4) {
			vtw_v4_t *v4 = (vtw_v4_t *)vp;

			vp = &(++v4)->common;
		} else if (ctl->is_v6) {
			vtw_v6_t *v6 = (vtw_v6_t *)vp;

			vp = &(++v6)->common;
		}
	}
}

void
show_vtw_stats(void)
{
	vtw_stats_t stats;
	void *p;

	if (!Vflag)
		return;

	if ((p = lookup("vtw_stats")) == NULL)
		return;
	snarf(p, &stats, sizeof(stats));

	printf("\t\t%" PRIu64 " inserts\n", stats.ins);
	printf("\t\t%" PRIu64 " deletes\n", stats.del);
	printf("\t\t%" PRIu64 " assassinations\n", stats.kill);
	printf("\tvestigial time-wait lookup_connect\n");
	printf("\t\t%" PRIu64 " look\n", stats.look[0]);
	printf("\t\t%" PRIu64 " hit\n", stats.hit[0]);
	printf("\t\t%" PRIu64 " miss\n", stats.miss[0]);
	printf("\t\t%" PRIu64 " probe\n", stats.probe[0]);
	printf("\t\t%" PRIu64 " losing\n", stats.losing[0]);
	printf("\t\t%" PRIu64 " max_chain\n", stats.max_chain[0]);
	printf("\t\t%" PRIu64 " max_probe\n", stats.max_probe[0]);
	printf("\t\t%" PRIu64 " max_loss\n", stats.max_loss[0]);
	printf("\tvestigial time-wait lookup_port\n");
	printf("\t\t%" PRIu64 " look\n", stats.look[1]);
	printf("\t\t%" PRIu64 " hit\n", stats.hit[1]);
	printf("\t\t%" PRIu64 " miss\n", stats.miss[1]);
	printf("\t\t%" PRIu64 " probe\n", stats.probe[1]);
	printf("\t\t%" PRIu64 " losing\n", stats.losing[1]);
	printf("\t\t%" PRIu64 " max_chain\n", stats.max_chain[1]);
	printf("\t\t%" PRIu64 " max_probe\n", stats.max_probe[1]);
	printf("\t\t%" PRIu64 " max_loss\n", stats.max_loss[1]);
}

void 
show_vtw_v4(void (*print)(const vtw_t *))
{
	fatp_t *base, *lim;
	fatp_t **hash, **port;
	size_t n;
	fatp_ctl_t fat_tcpv4;
	vtw_ctl_t  vtw_tcpv4[VTW_NCLASS];
	int i;
	int mem = 0;
	void *p;

	if ((p = lookup("fat_tcpv4")) == NULL)
		return;
	snarf(p, &fat_tcpv4, sizeof(fat_tcpv4));

	if ((p = lookup("vtw_tcpv4")) == NULL)
		return;
	snarf(p, &vtw_tcpv4[0], sizeof(vtw_tcpv4));

	mem += sizeof(fat_tcpv4);
	mem += sizeof(vtw_tcpv4);

	/* snarf/adjust vtw_ctl */
	for (i = 0; i < VTW_NCLASS; ++i) {
		vtw_v4_t *kbase, *klim;
		vtw_v4_t *ubase;
		ptrdiff_t delta;

		kbase = vtw_tcpv4[i].base.v4;
		klim = vtw_tcpv4[i].lim.v4;

		if (!kbase | !klim)
			continue;

		n = (klim - kbase + 1);

		if (!i) {
			if ((ubase = malloc(n * sizeof(*kbase))) == NULL)
				err(EXIT_FAILURE, NULL);
			snarf(kbase, ubase, n * sizeof(*ubase));

			mem += n * sizeof(*ubase);
		} else {
			ubase = vtw_tcpv4[0].base.v4;
		}

		delta = ubase - kbase;

		vtw_tcpv4[i].base.v4 += delta;
		vtw_tcpv4[i].lim.v4 += delta;
		vtw_tcpv4[i].alloc.v4 += delta;
		vtw_tcpv4[i].fat = &fat_tcpv4;

		if (vtw_tcpv4[i].oldest.v4)
			vtw_tcpv4[i].oldest.v4 += delta;
	}

	/* snarf/adjust fat_ctl */

	base = fat_tcpv4.base;
	lim = fat_tcpv4.lim;

	if (!base | !lim)
		goto end;

	mem += (lim - base + 1) * sizeof(*base);

	fat_tcpv4.base = malloc((lim - base + 1) * sizeof(*base));
	if (fat_tcpv4.base == NULL)
		err(EXIT_FAILURE, NULL);
	fat_tcpv4.lim = fat_tcpv4.base + (lim - base);

	snarf(base, fat_tcpv4.base, sizeof(*base) * (lim - base + 1));

	fat_tcpv4.vtw = &vtw_tcpv4[0];
	fat_tcpv4.free = fat_tcpv4.base + (fat_tcpv4.free - base);

	n = fat_tcpv4.mask + 1;
	hash = fat_tcpv4.hash;
	port = fat_tcpv4.port;

	fat_tcpv4.hash = malloc(n * sizeof(*hash));
	fat_tcpv4.port = malloc(n * sizeof(*port));
	if (fat_tcpv4.hash == NULL || fat_tcpv4.port == NULL)
		err(EXIT_FAILURE, NULL);

	snarf(hash, fat_tcpv4.hash, n * sizeof(*hash));
	snarf(port, fat_tcpv4.port, n * sizeof(*port));

end:
	process_vtw(&vtw_tcpv4[0], print);

#if 0
	if (Vflag && vflag) {
		printf("total memory for VTW in current config: %d bytes %f MB\n"
		    ,mem
		    ,mem / (1024.0 * 1024));
	}
#endif
}

void 
show_vtw_v6(void (*print)(const vtw_t *))
{
	fatp_t *base, *lim;
	fatp_t **hash, **port;
	size_t n;
	fatp_ctl_t fat_tcpv6;
	vtw_ctl_t  vtw_tcpv6[VTW_NCLASS];
	int i;
	int mem = 0;
	void *p;

	if ((p = lookup("fat_tcpv6")) == NULL)
		return;
	snarf(p, &fat_tcpv6, sizeof(fat_tcpv6));
	if ((p = lookup("vtw_tcpv6")) == NULL)
		return;
	snarf(p, &vtw_tcpv6[0], sizeof(vtw_tcpv6));

	mem += sizeof(fat_tcpv6);
	mem += sizeof(vtw_tcpv6);

	for (i = 0; i < VTW_NCLASS; ++i) {
		vtw_v6_t *kbase, *klim;
		vtw_v6_t *ubase;
		ptrdiff_t delta;

		kbase = vtw_tcpv6[i].base.v6;
		klim = vtw_tcpv6[i].lim.v6;

		if (!kbase | !klim)
			continue;

		n = (klim - kbase + 1);

		if (!i) {
			if ((ubase = malloc(n * sizeof(*kbase))) == NULL)
				err(EXIT_FAILURE, NULL);

			snarf(kbase, ubase, n * sizeof(*ubase));

			mem += n * sizeof(*ubase);
		} else {
			ubase = vtw_tcpv6[0].base.v6;
		}

		delta = ubase - kbase;

		vtw_tcpv6[i].base.v6 += delta;
		vtw_tcpv6[i].lim.v6 += delta;
		vtw_tcpv6[i].alloc.v6 += delta;
		vtw_tcpv6[i].fat = &fat_tcpv6;

		if (vtw_tcpv6[i].oldest.v6)
			vtw_tcpv6[i].oldest.v6 += delta;
	}

	base = fat_tcpv6.base;
	lim = fat_tcpv6.lim;

	if (!base | !lim)
		goto end;

	mem += (lim - base + 1) * sizeof(*base);

	fat_tcpv6.base = malloc((lim - base + 1) * sizeof(*base));
	if (fat_tcpv6.base == NULL)
		err(EXIT_FAILURE, NULL);
	fat_tcpv6.lim = fat_tcpv6.base + (lim - base);

	snarf(base, fat_tcpv6.base, sizeof(*base) * (lim - base + 1));

	fat_tcpv6.vtw = &vtw_tcpv6[0];
	fat_tcpv6.free = fat_tcpv6.base + (fat_tcpv6.free - base);

	n = fat_tcpv6.mask + 1;
	hash = fat_tcpv6.hash;
	port = fat_tcpv6.port;

	fat_tcpv6.hash = malloc(n * sizeof(*hash));
	fat_tcpv6.port = malloc(n * sizeof(*port));
	if (fat_tcpv6.hash == NULL || fat_tcpv6.port == NULL)
		err(EXIT_FAILURE, NULL);

	snarf(hash, fat_tcpv6.hash, n * sizeof(*hash));
	snarf(port, fat_tcpv6.port, n * sizeof(*port));

end:

	process_vtw(&vtw_tcpv6[0], print);
#if 0
	if (Vflag && vflag) {
		printf("total memory for VTW in current config: %d bytes %f MB\n"
		    ,mem
		    ,mem / (1024.0 * 1024));
	}
#endif
}

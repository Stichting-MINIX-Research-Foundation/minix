/*	$NetBSD: pfkey.c,v 1.1 2012/01/06 14:21:16 drochner Exp $	*/
/*	$KAME: ipsec.c,v 1.33 2003/07/25 09:54:32 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, 1998, and 1999 WIDE Project.
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
#ifdef __NetBSD__
__RCSID("$NetBSD: pfkey.c,v 1.1 2012/01/06 14:21:16 drochner Exp $");
#endif
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#ifdef IPSEC
#include <netipsec/keysock.h>
#endif

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "netstat.h"

#ifdef IPSEC 

static const char *pfkey_msgtypenames[] = {
	"reserved", "getspi", "update", "add", "delete",
	"get", "acquire", "register", "expire", "flush",
	"dump", "x_promisc", "x_pchange", "x_spdupdate", "x_spdadd",
	"x_spddelete", "x_spdget", "x_spdacquire", "x_spddump", "x_spdflush",
	"x_spdsetidx", "x_spdexpire", "x_spddelete2"
};

static const char *pfkey_msgtype_names(int);

static const char *
pfkey_msgtype_names(int x)
{
	const int max =
	    sizeof(pfkey_msgtypenames)/sizeof(pfkey_msgtypenames[0]);
	static char buf[20];

	if (x < max && pfkey_msgtypenames[x])
		return pfkey_msgtypenames[x];
	snprintf(buf, sizeof(buf), "#%d", x);
	return buf;
}

void
pfkey_stats(u_long off, const char *name)
{
	uint64_t pfkeystat[PFKEY_NSTATS];
	int first, type;

	if (use_sysctl) {
		size_t size = sizeof(pfkeystat);

		if (sysctlbyname("net.key.stats", pfkeystat, &size,
				 NULL, 0) == -1)
			return;
	} else {
		warnx("%s stats not available via KVM.", name);
		return;
	}

	printf ("%s:\n", name);

#define	p(f, m) if (pfkeystat[f] || sflag <= 1) \
    printf(m, (unsigned long long)pfkeystat[f], plural(pfkeystat[f]))

	/* userland -> kernel */
	p(PFKEY_STAT_OUT_TOTAL, "\t%llu request%s sent from userland\n");
	p(PFKEY_STAT_OUT_BYTES, "\t%llu byte%s sent from userland\n");
	for (first = 1, type = 0; type < 256; type++) {
		if (pfkeystat[PFKEY_STAT_OUT_MSGTYPE + type] == 0)
			continue;
		if (first) {
			printf("\thistogram by message type:\n");
			first = 0;
		}
		printf("\t\t%s: %llu\n", pfkey_msgtype_names(type),
		    (unsigned long long)pfkeystat[PFKEY_STAT_OUT_MSGTYPE + type]);
	}
	p(PFKEY_STAT_OUT_INVLEN, "\t%llu message%s with invalid length field\n");
	p(PFKEY_STAT_OUT_INVVER, "\t%llu message%s with invalid version field\n");
	p(PFKEY_STAT_OUT_INVMSGTYPE, "\t%llu message%s with invalid message type field\n");
	p(PFKEY_STAT_OUT_TOOSHORT, "\t%llu message%s too short\n");
	p(PFKEY_STAT_OUT_NOMEM, "\t%llu message%s with memory allocation failure\n");
	p(PFKEY_STAT_OUT_DUPEXT, "\t%llu message%s with duplicate extension\n");
	p(PFKEY_STAT_OUT_INVEXTTYPE, "\t%llu message%s with invalid extension type\n");
	p(PFKEY_STAT_OUT_INVSATYPE, "\t%llu message%s with invalid sa type\n");
	p(PFKEY_STAT_OUT_INVADDR, "\t%llu message%s with invalid address extension\n");

	/* kernel -> userland */
	p(PFKEY_STAT_IN_TOTAL, "\t%llu request%s sent to userland\n");
	p(PFKEY_STAT_IN_BYTES, "\t%llu byte%s sent to userland\n");
	for (first = 1, type = 0; type < 256; type++) {
		if (pfkeystat[PFKEY_STAT_IN_MSGTYPE + type] == 0)
			continue;
		if (first) {
			printf("\thistogram by message type:\n");
			first = 0;
		}
		printf("\t\t%s: %llu\n", pfkey_msgtype_names(type),
		    (unsigned long long)pfkeystat[PFKEY_STAT_IN_MSGTYPE + type]);
	}
	p(PFKEY_STAT_IN_MSGTARGET + KEY_SENDUP_ONE,
	    "\t%llu message%s toward single socket\n");
	p(PFKEY_STAT_IN_MSGTARGET + KEY_SENDUP_ALL,
	    "\t%llu message%s toward all sockets\n");
	p(PFKEY_STAT_IN_MSGTARGET + KEY_SENDUP_REGISTERED,
	    "\t%llu message%s toward registered sockets\n");
	p(PFKEY_STAT_IN_NOMEM, "\t%llu message%s with memory allocation failure\n");
#undef p
}
#endif /*IPSEC*/

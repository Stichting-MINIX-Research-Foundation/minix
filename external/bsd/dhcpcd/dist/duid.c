#include <sys/cdefs.h>
 __RCSID("$NetBSD: duid.c,v 1.9 2015/07/09 10:15:34 roy Exp $");

/*
 * dhcpcd - DHCP client daemon
 * Copyright (c) 2006-2015 Roy Marples <roy@marples.name>
 * All rights reserved

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define DUID_TIME_EPOCH 946684800
#define DUID_LLT	1
#define DUID_LL		3

#include <sys/socket.h>
#include <sys/types.h>

#include <net/if.h>
#include <net/if_arp.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef ARPHRD_NETROM
#  define ARPHRD_NETROM	0
#endif

#include "common.h"
#include "dhcpcd.h"
#include "duid.h"

static size_t
duid_make(unsigned char *d, const struct interface *ifp, uint16_t type)
{
	unsigned char *p;
	uint16_t u16;
	time_t t;
	uint32_t u32;

	p = d;
	u16 = htons(type);
	memcpy(p, &u16, 2);
	p += 2;
	u16 = htons(ifp->family);
	memcpy(p, &u16, 2);
	p += 2;
	if (type == DUID_LLT) {
		/* time returns seconds from jan 1 1970, but DUID-LLT is
		 * seconds from jan 1 2000 modulo 2^32 */
		t = time(NULL) - DUID_TIME_EPOCH;
		u32 = htonl((uint32_t)t & 0xffffffff);
		memcpy(p, &u32, 4);
		p += 4;
	}
	/* Finally, add the MAC address of the interface */
	memcpy(p, ifp->hwaddr, ifp->hwlen);
	p += ifp->hwlen;
	return (size_t)(p - d);
}

#define DUID_STRLEN DUID_LEN * 3
static size_t
duid_get(unsigned char *d, const struct interface *ifp)
{
	FILE *fp;
	int x = 0;
	size_t len = 0;
	char line[DUID_STRLEN];
	const struct interface *ifp2;

	/* If we already have a DUID then use it as it's never supposed
	 * to change once we have one even if the interfaces do */
	if ((fp = fopen(DUID, "r"))) {
		while (fgets(line, DUID_STRLEN, fp)) {
			len = strlen(line);
			if (len) {
				if (line[len - 1] == '\n')
					line[len - 1] = '\0';
			}
			len = hwaddr_aton(NULL, line);
			if (len && len <= DUID_LEN) {
				hwaddr_aton(d, line);
				break;
			}
			len = 0;
		}
		fclose(fp);
		if (len)
			return len;
	} else {
		if (errno != ENOENT)
			logger(ifp->ctx, LOG_ERR,
			    "error reading DUID: %s: %m", DUID);
	}

	/* No file? OK, lets make one based on our interface */
	if (ifp->family == ARPHRD_NETROM) {
		logger(ifp->ctx, LOG_WARNING,
		    "%s: is a NET/ROM psuedo interface", ifp->name);
		TAILQ_FOREACH(ifp2, ifp->ctx->ifaces, next) {
			if (ifp2->family != ARPHRD_NETROM)
				break;
		}
		if (ifp2) {
			ifp = ifp2;
			logger(ifp->ctx, LOG_WARNING,
			    "picked interface %s to generate a DUID",
			    ifp->name);
		} else {
			logger(ifp->ctx, LOG_WARNING,
			    "no interfaces have a fixed hardware address");
			return duid_make(d, ifp, DUID_LL);
		}
	}

	if (!(fp = fopen(DUID, "w"))) {
		logger(ifp->ctx, LOG_ERR, "error writing DUID: %s: %m", DUID);
		return duid_make(d, ifp, DUID_LL);
	}
	len = duid_make(d, ifp, DUID_LLT);
	x = fprintf(fp, "%s\n", hwaddr_ntoa(d, len, line, sizeof(line)));
	if (fclose(fp) == EOF)
		x = -1;
	/* Failed to write the duid? scrub it, we cannot use it */
	if (x < 1) {
		logger(ifp->ctx, LOG_ERR, "error writing DUID: %s: %m", DUID);
		unlink(DUID);
		return duid_make(d, ifp, DUID_LL);
	}
	return len;
}

size_t duid_init(const struct interface *ifp)
{

	if (ifp->ctx->duid == NULL) {
		ifp->ctx->duid = malloc(DUID_LEN);
		if (ifp->ctx->duid == NULL) {
			logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
			return 0;
		}
		ifp->ctx->duid_len = duid_get(ifp->ctx->duid, ifp);
	}
	return ifp->ctx->duid_len;
}

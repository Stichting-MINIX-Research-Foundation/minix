/* $NetBSD: ipv4ll.h,v 1.10 2015/08/21 10:39:00 roy Exp $ */

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

#ifndef IPV4LL_H
#define IPV4LL_H

#include "arp.h"

extern const struct in_addr inaddr_llmask;
extern const struct in_addr inaddr_llbcast;

#define LINKLOCAL_ADDR	0xa9fe0000
#define LINKLOCAL_MASK	IN_CLASSB_NET
#define LINKLOCAL_BRDC	(LINKLOCAL_ADDR | ~LINKLOCAL_MASK)

#ifndef IN_LINKLOCAL
# define IN_LINKLOCAL(addr) ((addr & IN_CLASSB_NET) == LINKLOCAL_ADDR)
#endif

struct ipv4ll_state {
	struct in_addr addr;
	struct arp_state *arp;
	unsigned int conflicts;
	struct timespec defend;
	char randomstate[128];
	uint8_t down;
};

#define IPV4LL_STATE(ifp)						       \
	((struct ipv4ll_state *)(ifp)->if_data[IF_DATA_IPV4LL])
#define IPV4LL_CSTATE(ifp)						       \
	((const struct ipv4ll_state *)(ifp)->if_data[IF_DATA_IPV4LL])
#define IPV4LL_STATE_RUNNING(ifp)					       \
	(IPV4LL_CSTATE((ifp)) && !IPV4LL_CSTATE((ifp))->down &&	       \
	IN_LINKLOCAL(ntohl(IPV4LL_CSTATE((ifp))->addr.s_addr)))

struct rt* ipv4ll_subnet_route(const struct interface *);
struct rt* ipv4ll_default_route(const struct interface *);
ssize_t ipv4ll_env(char **, const char *, const struct interface *);
void ipv4ll_start(void *);
void ipv4ll_claimed(void *);
void ipv4ll_handle_failure(void *);

#define ipv4ll_free(ifp) ipv4ll_freedrop((ifp), 0);
#define ipv4ll_drop(ifp) ipv4ll_freedrop((ifp), 1);
void ipv4ll_freedrop(struct interface *, int);

#endif

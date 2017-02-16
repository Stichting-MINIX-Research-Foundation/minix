/* $NetBSD: arp.h,v 1.11 2015/07/09 10:15:34 roy Exp $ */

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

#ifndef ARP_H
#define ARP_H

/* ARP timings from RFC5227 */
#define PROBE_WAIT		 1
#define PROBE_NUM		 3
#define PROBE_MIN		 1
#define PROBE_MAX		 2
#define ANNOUNCE_WAIT		 2
#define ANNOUNCE_NUM		 2
#define ANNOUNCE_INTERVAL	 2
#define MAX_CONFLICTS		10
#define RATE_LIMIT_INTERVAL	60
#define DEFEND_INTERVAL		10

#include "dhcpcd.h"

struct arp_msg {
	uint16_t op;
	unsigned char sha[HWADDR_LEN];
	struct in_addr sip;
	unsigned char tha[HWADDR_LEN];
	struct in_addr tip;
};

struct arp_state {
	TAILQ_ENTRY(arp_state) next;
	struct interface *iface;

	void (*probed_cb)(struct arp_state *);
	void (*announced_cb)(struct arp_state *);
	void (*conflicted_cb)(struct arp_state *, const struct arp_msg *);
	void (*free_cb)(struct arp_state *);

	struct in_addr addr;
	int probes;
	int claims;
	struct in_addr failed;
};
TAILQ_HEAD(arp_statehead, arp_state);

struct iarp_state {
	int fd;
	struct arp_statehead arp_states;
};

#define ARP_STATE(ifp)							       \
	((struct iarp_state *)(ifp)->if_data[IF_DATA_ARP])
#define ARP_CSTATE(ifp)							       \
	((const struct iarp_state *)(ifp)->if_data[IF_DATA_ARP])

#ifdef INET
void arp_report_conflicted(const struct arp_state *, const struct arp_msg *);
void arp_announce(struct arp_state *);
void arp_probe(struct arp_state *);
struct arp_state *arp_new(struct interface *, const struct in_addr *);
void arp_cancel(struct arp_state *);
void arp_free(struct arp_state *);
void arp_free_but(struct arp_state *);
struct arp_state *arp_find(struct interface *, const struct in_addr *);
void arp_close(struct interface *);

void arp_handleifa(int, struct interface *, const struct in_addr *, int);
#else
#define arp_close(a) {}
#endif
#endif

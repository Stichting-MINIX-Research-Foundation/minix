#include <sys/cdefs.h>
 __RCSID("$NetBSD: arp.c,v 1.14 2015/07/09 10:15:34 roy Exp $");

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

#include <sys/socket.h>
#include <sys/types.h>

#include <arpa/inet.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ELOOP_QUEUE 5
#include "config.h"
#include "arp.h"
#include "if.h"
#include "ipv4.h"
#include "common.h"
#include "dhcpcd.h"
#include "eloop.h"
#include "if.h"
#include "if-options.h"
#include "ipv4ll.h"

#define ARP_LEN								      \
	(sizeof(struct arphdr) + (2 * sizeof(uint32_t)) + (2 * HWADDR_LEN))

static ssize_t
arp_request(const struct interface *ifp, in_addr_t sip, in_addr_t tip)
{
	uint8_t arp_buffer[ARP_LEN];
	struct arphdr ar;
	size_t len;
	uint8_t *p;

	ar.ar_hrd = htons(ifp->family);
	ar.ar_pro = htons(ETHERTYPE_IP);
	ar.ar_hln = ifp->hwlen;
	ar.ar_pln = sizeof(sip);
	ar.ar_op = htons(ARPOP_REQUEST);

	p = arp_buffer;
	len = 0;

#define CHECK(fun, b, l)						\
	do {								\
		if (len + (l) > sizeof(arp_buffer))			\
			goto eexit;					\
		fun(p, (b), (l));					\
		p += (l);						\
		len += (l);						\
	} while (/* CONSTCOND */ 0)
#define APPEND(b, l)	CHECK(memcpy, b, l)
#define ZERO(l)		CHECK(memset, 0, l)

	APPEND(&ar, sizeof(ar));
	APPEND(ifp->hwaddr, ifp->hwlen);
	APPEND(&sip, sizeof(sip));
	ZERO(ifp->hwlen);
	APPEND(&tip, sizeof(tip));
	return if_sendrawpacket(ifp, ETHERTYPE_ARP, arp_buffer, len);

eexit:
	errno = ENOBUFS;
	return -1;
}

void
arp_report_conflicted(const struct arp_state *astate, const struct arp_msg *amsg)
{

	if (amsg != NULL) {
		char buf[HWADDR_LEN * 3];

		logger(astate->iface->ctx, LOG_ERR,
		    "%s: hardware address %s claims %s",
		    astate->iface->name,
		    hwaddr_ntoa(amsg->sha, astate->iface->hwlen,
		    buf, sizeof(buf)),
		    inet_ntoa(astate->failed));
	} else
		logger(astate->iface->ctx, LOG_ERR,
		    "%s: DAD detected %s",
		    astate->iface->name, inet_ntoa(astate->failed));
}

static void
arp_packet(void *arg)
{
	struct interface *ifp = arg;
	const struct interface *ifn;
	uint8_t arp_buffer[ARP_LEN];
	struct arphdr ar;
	struct arp_msg arm;
	ssize_t bytes;
	struct iarp_state *state;
	struct arp_state *astate, *astaten;
	unsigned char *hw_s, *hw_t;
	int flags;

	state = ARP_STATE(ifp);
	flags = 0;
	while (!(flags & RAW_EOF)) {
		bytes = if_readrawpacket(ifp, ETHERTYPE_ARP,
		    arp_buffer, sizeof(arp_buffer), &flags);
		if (bytes == -1) {
			logger(ifp->ctx, LOG_ERR,
			    "%s: arp if_readrawpacket: %m", ifp->name);
			arp_close(ifp);
			return;
		}
		/* We must have a full ARP header */
		if ((size_t)bytes < sizeof(ar))
			continue;
		memcpy(&ar, arp_buffer, sizeof(ar));
		/* Families must match */
		if (ar.ar_hrd != htons(ifp->family))
			continue;
		/* Protocol must be IP. */
		if (ar.ar_pro != htons(ETHERTYPE_IP))
			continue;
		if (ar.ar_pln != sizeof(arm.sip.s_addr))
			continue;
		/* Only these types are recognised */
		if (ar.ar_op != htons(ARPOP_REPLY) &&
		    ar.ar_op != htons(ARPOP_REQUEST))
			continue;

		/* Get pointers to the hardware addreses */
		hw_s = arp_buffer + sizeof(ar);
		hw_t = hw_s + ar.ar_hln + ar.ar_pln;
		/* Ensure we got all the data */
		if ((hw_t + ar.ar_hln + ar.ar_pln) - arp_buffer > bytes)
			continue;
		/* Ignore messages from ourself */
		TAILQ_FOREACH(ifn, ifp->ctx->ifaces, next) {
			if (ar.ar_hln == ifn->hwlen &&
			    memcmp(hw_s, ifn->hwaddr, ifn->hwlen) == 0)
				break;
		}
		if (ifn)
			continue;
		/* Copy out the HW and IP addresses */
		memcpy(&arm.sha, hw_s, ar.ar_hln);
		memcpy(&arm.sip.s_addr, hw_s + ar.ar_hln, ar.ar_pln);
		memcpy(&arm.tha, hw_t, ar.ar_hln);
		memcpy(&arm.tip.s_addr, hw_t + ar.ar_hln, ar.ar_pln);

		/* Run the conflicts */
		TAILQ_FOREACH_SAFE(astate, &state->arp_states, next, astaten) {
			if (astate->conflicted_cb)
				astate->conflicted_cb(astate, &arm);
		}
	}
}

static void
arp_open(struct interface *ifp)
{
	struct iarp_state *state;

	state = ARP_STATE(ifp);
	if (state->fd == -1) {
		state->fd = if_openrawsocket(ifp, ETHERTYPE_ARP);
		if (state->fd == -1) {
			logger(ifp->ctx, LOG_ERR, "%s: %s: %m",
			    __func__, ifp->name);
			return;
		}
		eloop_event_add(ifp->ctx->eloop, state->fd,
		    arp_packet, ifp, NULL, NULL);
	}
}

static void
arp_announced(void *arg)
{
	struct arp_state *astate = arg;

	if (astate->announced_cb) {
		astate->announced_cb(astate);
		return;
	}

	/* Nothing more to do, so free us */
	arp_free(astate);
}

static void
arp_announce1(void *arg)
{
	struct arp_state *astate = arg;
	struct interface *ifp = astate->iface;

	if (++astate->claims < ANNOUNCE_NUM)
		logger(ifp->ctx, LOG_DEBUG,
		    "%s: ARP announcing %s (%d of %d), "
		    "next in %d.0 seconds",
		    ifp->name, inet_ntoa(astate->addr),
		    astate->claims, ANNOUNCE_NUM, ANNOUNCE_WAIT);
	else
		logger(ifp->ctx, LOG_DEBUG,
		    "%s: ARP announcing %s (%d of %d)",
		    ifp->name, inet_ntoa(astate->addr),
		    astate->claims, ANNOUNCE_NUM);
	if (arp_request(ifp, astate->addr.s_addr, astate->addr.s_addr) == -1)
		logger(ifp->ctx, LOG_ERR, "send_arp: %m");
	eloop_timeout_add_sec(ifp->ctx->eloop, ANNOUNCE_WAIT,
	    astate->claims < ANNOUNCE_NUM ? arp_announce1 : arp_announced,
	    astate);
}

void
arp_announce(struct arp_state *astate)
{

	arp_open(astate->iface);
	astate->claims = 0;
	arp_announce1(astate);
}

static void
arp_probed(void *arg)
{
	struct arp_state *astate = arg;

	astate->probed_cb(astate);
}

static void
arp_probe1(void *arg)
{
	struct arp_state *astate = arg;
	struct interface *ifp = astate->iface;
	struct timespec tv;

	if (++astate->probes < PROBE_NUM) {
		tv.tv_sec = PROBE_MIN;
		tv.tv_nsec = (suseconds_t)arc4random_uniform(
		    (PROBE_MAX - PROBE_MIN) * NSEC_PER_SEC);
		timespecnorm(&tv);
		eloop_timeout_add_tv(ifp->ctx->eloop, &tv, arp_probe1, astate);
	} else {
		tv.tv_sec = ANNOUNCE_WAIT;
		tv.tv_nsec = 0;
		eloop_timeout_add_tv(ifp->ctx->eloop, &tv, arp_probed, astate);
	}
	logger(ifp->ctx, LOG_DEBUG,
	    "%s: ARP probing %s (%d of %d), next in %0.1f seconds",
	    ifp->name, inet_ntoa(astate->addr),
	    astate->probes ? astate->probes : PROBE_NUM, PROBE_NUM,
	    timespec_to_double(&tv));
	if (arp_request(ifp, 0, astate->addr.s_addr) == -1)
		logger(ifp->ctx, LOG_ERR, "send_arp: %m");
}

void
arp_probe(struct arp_state *astate)
{

	arp_open(astate->iface);
	astate->probes = 0;
	logger(astate->iface->ctx, LOG_DEBUG, "%s: probing for %s",
	    astate->iface->name, inet_ntoa(astate->addr));
	arp_probe1(astate);
}

struct arp_state *
arp_find(struct interface *ifp, const struct in_addr *addr)
{
	struct iarp_state *state;
	struct arp_state *astate;

	if ((state = ARP_STATE(ifp)) == NULL)
		goto out;
	TAILQ_FOREACH(astate, &state->arp_states, next) {
		if (astate->addr.s_addr == addr->s_addr && astate->iface == ifp)
			return astate;
	}
out:
	errno = ESRCH;
	return NULL;
}

struct arp_state *
arp_new(struct interface *ifp, const struct in_addr *addr)
{
	struct iarp_state *state;
	struct arp_state *astate;

	if ((state = ARP_STATE(ifp)) == NULL) {
	        ifp->if_data[IF_DATA_ARP] = malloc(sizeof(*state));
		state = ARP_STATE(ifp);
		if (state == NULL) {
			logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
			return NULL;
		}
		state->fd = -1;
		TAILQ_INIT(&state->arp_states);
	} else {
		if (addr && (astate = arp_find(ifp, addr)))
			return astate;
	}

	if ((astate = calloc(1, sizeof(*astate))) == NULL) {
		logger(ifp->ctx, LOG_ERR, "%s: %s: %m", ifp->name, __func__);
		return NULL;
	}
	astate->iface = ifp;
	if (addr)
		astate->addr = *addr;
	state = ARP_STATE(ifp);
	TAILQ_INSERT_TAIL(&state->arp_states, astate, next);
	return astate;
}

void
arp_cancel(struct arp_state *astate)
{

	eloop_timeout_delete(astate->iface->ctx->eloop, NULL, astate);
}

void
arp_free(struct arp_state *astate)
{

	if (astate != NULL) {
		struct interface *ifp;
		struct iarp_state *state;

		ifp = astate->iface;
		eloop_timeout_delete(ifp->ctx->eloop, NULL, astate);
		state =	ARP_STATE(ifp);
		TAILQ_REMOVE(&state->arp_states, astate, next);
		if (astate->free_cb)
			astate->free_cb(astate);
		free(astate);

		/* If there are no more ARP states, close the socket. */
		if (state->fd != -1 &&
		    TAILQ_FIRST(&state->arp_states) == NULL)
		{
			eloop_event_delete(ifp->ctx->eloop, state->fd);
			close(state->fd);
			free(state);
			ifp->if_data[IF_DATA_ARP] = NULL;
		}
	}
}

void
arp_free_but(struct arp_state *astate)
{
	struct iarp_state *state;
	struct arp_state *p, *n;

	state = ARP_STATE(astate->iface);
	TAILQ_FOREACH_SAFE(p, &state->arp_states, next, n) {
		if (p != astate)
			arp_free(p);
	}
}

void
arp_close(struct interface *ifp)
{
	struct iarp_state *state;
	struct arp_state *astate;

	/* Freeing the last state will also free the main state,
	 * so test for both. */
	for (;;) {
		if ((state = ARP_STATE(ifp)) == NULL ||
		    (astate = TAILQ_FIRST(&state->arp_states)) == NULL)
			break;
		arp_free(astate);
	}
}

void
arp_handleifa(int cmd, struct interface *ifp, const struct in_addr *addr,
    int flags)
{
#ifdef IN_IFF_DUPLICATED
	struct iarp_state *state;
	struct arp_state *astate, *asn;

	if (cmd != RTM_NEWADDR || (state = ARP_STATE(ifp)) == NULL)
		return;

	TAILQ_FOREACH_SAFE(astate, &state->arp_states, next, asn) {
		if (astate->addr.s_addr == addr->s_addr) {
			if (flags & IN_IFF_DUPLICATED) {
				if (astate->conflicted_cb)
					astate->conflicted_cb(astate, NULL);
			} else if (!(flags & IN_IFF_NOTUSEABLE)) {
				if (astate->probed_cb)
					astate->probed_cb(astate);
			}
		}
	}
#else
	UNUSED(cmd);
	UNUSED(ifp);
	UNUSED(addr);
	UNUSED(flags);
#endif
}

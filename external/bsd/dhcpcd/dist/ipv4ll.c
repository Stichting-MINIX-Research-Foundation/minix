#include <sys/cdefs.h>
 __RCSID("$NetBSD: ipv4ll.c,v 1.12 2015/08/21 10:39:00 roy Exp $");

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

#include <arpa/inet.h>

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ELOOP_QUEUE 6
#include "config.h"
#include "arp.h"
#include "common.h"
#include "eloop.h"
#include "if.h"
#include "if-options.h"
#include "ipv4.h"
#include "ipv4ll.h"
#include "script.h"

const struct in_addr inaddr_llmask = { HTONL(LINKLOCAL_MASK) };
const struct in_addr inaddr_llbcast = { HTONL(LINKLOCAL_BRDC) };

static in_addr_t
ipv4ll_pick_addr(const struct arp_state *astate)
{
	struct in_addr addr;
	struct ipv4ll_state *istate;

	istate = IPV4LL_STATE(astate->iface);
	setstate(istate->randomstate);

	do {
		/* RFC 3927 Section 2.1 states that the first 256 and
		 * last 256 addresses are reserved for future use.
		 * See ipv4ll_start for why we don't use arc4_random. */
		addr.s_addr = ntohl(LINKLOCAL_ADDR |
		    ((uint32_t)(random() % 0xFD00) + 0x0100));

		/* No point using a failed address */
		if (addr.s_addr == astate->failed.s_addr)
			continue;
		/* Ensure we don't have the address on another interface */
	} while (ipv4_findaddr(astate->iface->ctx, &addr) != NULL);

	/* Restore the original random state */
	setstate(astate->iface->ctx->randomstate);

	return addr.s_addr;
}

struct rt *
ipv4ll_subnet_route(const struct interface *ifp)
{
	const struct ipv4ll_state *state;
	struct rt *rt;

	assert(ifp != NULL);
	if ((state = IPV4LL_CSTATE(ifp)) == NULL ||
	    state->addr.s_addr == INADDR_ANY)
		return NULL;

	if ((rt = calloc(1, sizeof(*rt))) == NULL) {
		logger(ifp->ctx, LOG_ERR, "%s: malloc: %m", __func__);
		return NULL;
	}
	rt->iface = ifp;
	rt->dest.s_addr = state->addr.s_addr & inaddr_llmask.s_addr;
	rt->net = inaddr_llmask;
	rt->gate.s_addr = INADDR_ANY;
	rt->src = state->addr;
	return rt;
}

struct rt *
ipv4ll_default_route(const struct interface *ifp)
{
	const struct ipv4ll_state *state;
	struct rt *rt;

	assert(ifp != NULL);
	if ((state = IPV4LL_CSTATE(ifp)) == NULL ||
	    state->addr.s_addr == INADDR_ANY)
		return NULL;

	if ((rt = calloc(1, sizeof(*rt))) == NULL) {
		logger(ifp->ctx, LOG_ERR, "%s: malloc: %m", __func__);
		return NULL;
	}
	rt->iface = ifp;
	rt->dest.s_addr = INADDR_ANY;
	rt->net.s_addr = INADDR_ANY;
	rt->gate.s_addr = INADDR_ANY;
	rt->src = state->addr;
	return rt;
}

ssize_t
ipv4ll_env(char **env, const char *prefix, const struct interface *ifp)
{
	const struct ipv4ll_state *state;
	const char *pf = prefix == NULL ? "" : "_";
	struct in_addr netnum;

	assert(ifp != NULL);
	if ((state = IPV4LL_CSTATE(ifp)) == NULL)
		return 0;

	if (env == NULL)
		return 5;

	/* Emulate a DHCP environment */
	if (asprintf(&env[0], "%s%sip_address=%s",
	    prefix, pf, inet_ntoa(state->addr)) == -1)
		return -1;
	if (asprintf(&env[1], "%s%ssubnet_mask=%s",
	    prefix, pf, inet_ntoa(inaddr_llmask)) == -1)
		return -1;
	if (asprintf(&env[2], "%s%ssubnet_cidr=%d",
	    prefix, pf, inet_ntocidr(inaddr_llmask)) == -1)
		return -1;
	if (asprintf(&env[3], "%s%sbroadcast_address=%s",
	    prefix, pf, inet_ntoa(inaddr_llbcast)) == -1)
		return -1;
	netnum.s_addr = state->addr.s_addr & inaddr_llmask.s_addr;
	if (asprintf(&env[4], "%s%snetwork_number=%s",
	    prefix, pf, inet_ntoa(netnum)) == -1)
		return -1;
	return 5;
}

static void
ipv4ll_probed(struct arp_state *astate)
{
	struct interface *ifp;
	struct ipv4ll_state *state;
	struct ipv4_addr *ia;

	assert(astate != NULL);
	assert(astate->iface != NULL);

	ifp = astate->iface;
	state = IPV4LL_STATE(ifp);
	assert(state != NULL);

	ia = ipv4_iffindaddr(ifp, &astate->addr, &inaddr_llmask);
#ifdef IN_IFF_NOTREADY
	if (ia == NULL || ia->addr_flags & IN_IFF_NOTREADY)
#endif
		logger(ifp->ctx, LOG_INFO, "%s: using IPv4LL address %s",
		  ifp->name, inet_ntoa(astate->addr));
	if (ia == NULL)
		ia = ipv4_addaddr(ifp, &astate->addr,
		    &inaddr_llmask, &inaddr_llbcast);
	if (ia == NULL)
		return;
#ifdef IN_IFF_NOTREADY
	if (ia->addr_flags & IN_IFF_NOTREADY)
		return;
	logger(ifp->ctx, LOG_DEBUG, "%s: DAD completed for %s",
	    ifp->name, inet_ntoa(astate->addr));
#endif
	state->addr = astate->addr;
	timespecclear(&state->defend);
	if_initrt(ifp);
	ipv4_buildroutes(ifp->ctx);
	arp_announce(astate);
	script_runreason(ifp, "IPV4LL");
	dhcpcd_daemonise(ifp->ctx);
}

static void
ipv4ll_announced(struct arp_state *astate)
{
	struct ipv4ll_state *state = IPV4LL_STATE(astate->iface);

	state->conflicts = 0;
	/* Need to keep the arp state so we can defend our IP. */
}

static void
ipv4ll_probe(void *arg)
{

#ifdef IN_IFF_TENTATIVE
	ipv4ll_probed(arg);
#else
	arp_probe(arg);
#endif
}

static void
ipv4ll_conflicted(struct arp_state *astate, const struct arp_msg *amsg)
{
	struct interface *ifp;
	struct ipv4ll_state *state;
	in_addr_t fail;

	assert(astate != NULL);
	assert(astate->iface != NULL);
	ifp = astate->iface;
	state = IPV4LL_STATE(ifp);
	assert(state != NULL);

	fail = 0;
	/* RFC 3927 2.2.1, Probe Conflict Detection */
	if (amsg == NULL ||
	    (amsg->sip.s_addr == astate->addr.s_addr ||
	    (amsg->sip.s_addr == 0 && amsg->tip.s_addr == astate->addr.s_addr)))
		fail = astate->addr.s_addr;

	/* RFC 3927 2.5, Conflict Defense */
	if (IN_LINKLOCAL(ntohl(state->addr.s_addr)) &&
	    amsg && amsg->sip.s_addr == state->addr.s_addr)
		fail = state->addr.s_addr;

	if (fail == 0)
		return;

	astate->failed.s_addr = fail;
	arp_report_conflicted(astate, amsg);

	if (astate->failed.s_addr == state->addr.s_addr) {
		struct timespec now, defend;

		/* RFC 3927 Section 2.5 */
		defend.tv_sec = state->defend.tv_sec + DEFEND_INTERVAL;
		defend.tv_nsec = state->defend.tv_nsec;
		clock_gettime(CLOCK_MONOTONIC, &now);
		if (timespeccmp(&defend, &now, >)) {
			logger(ifp->ctx, LOG_WARNING,
			    "%s: IPv4LL %d second defence failed for %s",
			    ifp->name, DEFEND_INTERVAL,
			    inet_ntoa(state->addr));
			ipv4_deladdr(ifp, &state->addr, &inaddr_llmask, 1);
			state->down = 1;
			script_runreason(ifp, "IPV4LL");
			state->addr.s_addr = INADDR_ANY;
		} else {
			logger(ifp->ctx, LOG_DEBUG,
			    "%s: defended IPv4LL address %s",
			    ifp->name, inet_ntoa(state->addr));
			state->defend = now;
			return;
		}
	}

	arp_cancel(astate);
	if (++state->conflicts == MAX_CONFLICTS)
		logger(ifp->ctx, LOG_ERR,
		    "%s: failed to acquire an IPv4LL address",
		    ifp->name);
	astate->addr.s_addr = ipv4ll_pick_addr(astate);
	eloop_timeout_add_sec(ifp->ctx->eloop,
		state->conflicts >= MAX_CONFLICTS ?
		RATE_LIMIT_INTERVAL : PROBE_WAIT,
		ipv4ll_probe, astate);
}

static void
ipv4ll_arpfree(struct arp_state *astate)
{
	struct ipv4ll_state *state;

	state = IPV4LL_STATE(astate->iface);
	if (state->arp == astate)
		state->arp = NULL;
}

void
ipv4ll_start(void *arg)
{
	struct interface *ifp;
	struct ipv4ll_state *state;
	struct arp_state *astate;
	struct ipv4_addr *ia;

	assert(arg != NULL);
	ifp = arg;
	if ((state = IPV4LL_STATE(ifp)) == NULL) {
		ifp->if_data[IF_DATA_IPV4LL] = calloc(1, sizeof(*state));
		if ((state = IPV4LL_STATE(ifp)) == NULL) {
			syslog(LOG_ERR, "%s: calloc %m", __func__);
			return;
		}

		state->addr.s_addr = INADDR_ANY;
	}

	if (state->arp != NULL)
		return;

	/* RFC 3927 Section 2.1 states that the random number generator
	 * SHOULD be seeded with a value derived from persistent information
	 * such as the IEEE 802 MAC address so that it usually picks
	 * the same address without persistent storage. */
	if (state->conflicts == 0) {
		unsigned int seed;
		char *orig;

		if (sizeof(seed) > ifp->hwlen) {
			seed = 0;
			memcpy(&seed, ifp->hwaddr, ifp->hwlen);
		} else
			memcpy(&seed, ifp->hwaddr + ifp->hwlen - sizeof(seed),
			    sizeof(seed));
		orig = initstate(seed,
		    state->randomstate, sizeof(state->randomstate));

		/* Save the original state. */
		if (ifp->ctx->randomstate == NULL)
			ifp->ctx->randomstate = orig;

		/* Set back the original state until we need the seeded one. */
		setstate(ifp->ctx->randomstate);
	}

	if ((astate = arp_new(ifp, NULL)) == NULL)
		return;

	state->arp = astate;
	astate->probed_cb = ipv4ll_probed;
	astate->announced_cb = ipv4ll_announced;
	astate->conflicted_cb = ipv4ll_conflicted;
	astate->free_cb = ipv4ll_arpfree;

	/* Find an existing IPv4LL address and ensure we can work with it. */
	ia = ipv4_iffindlladdr(ifp);
#ifdef IN_IFF_TENTATIVE
	if (ia != NULL && ia->addr_flags & IN_IFF_DUPLICATED) {
		ipv4_deladdr(ifp, &ia->addr, &ia->net, 0);
		ia = NULL;
	}
#endif
	if (ia != NULL) {
		astate->addr = ia->addr;
#ifdef IN_IFF_TENTATIVE
		if (ia->addr_flags & (IN_IFF_TENTATIVE | IN_IFF_DETACHED)) {
			logger(ifp->ctx, LOG_INFO,
			    "%s: waiting for DAD to complete on %s",
			    ifp->name, inet_ntoa(ia->addr));
			return;
		}
		logger(ifp->ctx, LOG_INFO, "%s: using IPv4LL address %s",
		  ifp->name, inet_ntoa(astate->addr));
#endif
		ipv4ll_probed(astate);
		return;
	}

	logger(ifp->ctx, LOG_INFO, "%s: probing for an IPv4LL address",
	    ifp->name);
	astate->addr.s_addr = ipv4ll_pick_addr(astate);
#ifdef IN_IFF_TENTATIVE
	ipv4ll_probed(astate);
#else
	arp_probe(astate);
#endif
}

void
ipv4ll_freedrop(struct interface *ifp, int drop)
{
	struct ipv4ll_state *state;
	int dropped;

	assert(ifp != NULL);
	state = IPV4LL_STATE(ifp);
	dropped = 0;

	/* Free ARP state first because ipv4_deladdr might also ... */
	if (state && state->arp) {
		eloop_timeout_delete(ifp->ctx->eloop, NULL, state->arp);
		arp_free(state->arp);
		state->arp = NULL;
	}

	if (drop && (ifp->options->options & DHCPCD_NODROP) != DHCPCD_NODROP) {
		struct ipv4_state *istate;

		if (state && state->addr.s_addr != INADDR_ANY) {
			ipv4_deladdr(ifp, &state->addr, &inaddr_llmask, 1);
			state->addr.s_addr = INADDR_ANY;
			dropped = 1;
		}

		/* Free any other link local addresses that might exist. */
		if ((istate = IPV4_STATE(ifp)) != NULL) {
			struct ipv4_addr *ia, *ian;

			TAILQ_FOREACH_SAFE(ia, &istate->addrs, next, ian) {
				if (IN_LINKLOCAL(ntohl(ia->addr.s_addr))) {
					ipv4_deladdr(ifp, &ia->addr,
					    &ia->net, 0);
					dropped = 1;
				}
			}
		}
	}

	if (state) {
		free(state);
		ifp->if_data[IF_DATA_IPV4LL] = NULL;

		if (dropped) {
			ipv4_buildroutes(ifp->ctx);
			script_runreason(ifp, "IPV4LL");
		}
	}
}

/*	$NetBSD: npf_handler.c,v 1.33 2014/07/23 01:25:34 rmind Exp $	*/

/*-
 * Copyright (c) 2009-2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This material is based upon work partially supported by The
 * NetBSD Foundation under a contract with Mindaugas Rasiukevicius.
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
 * NPF packet handler.
 *
 * Note: pfil(9) hooks are currently locked by softnet_lock and kernel-lock.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: npf_handler.c,v 1.33 2014/07/23 01:25:34 rmind Exp $");

#include <sys/types.h>
#include <sys/param.h>

#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <net/if.h>
#include <net/pfil.h>
#include <sys/socketvar.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>

#include "npf_impl.h"
#include "npf_conn.h"

static bool		pfil_registered = false;
static pfil_head_t *	npf_ph_if = NULL;
static pfil_head_t *	npf_ph_inet = NULL;
static pfil_head_t *	npf_ph_inet6 = NULL;

#ifndef INET6
#define ip6_reass_packet(x, y)	ENOTSUP
#endif

/*
 * npf_ifhook: hook handling interface changes.
 */
static int
npf_ifhook(void *arg, struct mbuf **mp, ifnet_t *ifp, int di)
{
	u_long cmd = (u_long)mp;

	if (di == PFIL_IFNET) {
		switch (cmd) {
		case PFIL_IFNET_ATTACH:
			npf_ifmap_attach(ifp);
			break;
		case PFIL_IFNET_DETACH:
			npf_ifmap_detach(ifp);
			break;
		}
	}
	return 0;
}

static int
npf_reassembly(npf_cache_t *npc, struct mbuf **mp)
{
	nbuf_t *nbuf = npc->npc_nbuf;
	int error = EINVAL;

	/* Reset the mbuf as it may have changed. */
	*mp = nbuf_head_mbuf(nbuf);
	nbuf_reset(nbuf);

	if (npf_iscached(npc, NPC_IP4)) {
		struct ip *ip = nbuf_dataptr(nbuf);
		error = ip_reass_packet(mp, ip);
	} else if (npf_iscached(npc, NPC_IP6)) {
		/*
		 * Note: ip6_reass_packet() offset is the start of
		 * the fragment header.
		 */
		error = ip6_reass_packet(mp, npc->npc_hlen);
		if (error && *mp == NULL) {
			memset(nbuf, 0, sizeof(nbuf_t));
		}
	}
	if (error) {
		npf_stats_inc(NPF_STAT_REASSFAIL);
		return error;
	}
	if (*mp == NULL) {
		/* More fragments should come. */
		npf_stats_inc(NPF_STAT_FRAGMENTS);
		return 0;
	}

	/*
	 * Reassembly is complete, we have the final packet.
	 * Cache again, since layer 4 data is accessible now.
	 */
	nbuf_init(nbuf, *mp, nbuf->nb_ifp);
	npc->npc_info = 0;

	if (npf_cache_all(npc) & NPC_IPFRAG) {
		return EINVAL;
	}
	npf_stats_inc(NPF_STAT_REASSEMBLY);
	return 0;
}

/*
 * npf_packet_handler: main packet handling routine for layer 3.
 *
 * Note: packet flow and inspection logic is in strict order.
 */
int
npf_packet_handler(void *arg, struct mbuf **mp, ifnet_t *ifp, int di)
{
	nbuf_t nbuf;
	npf_cache_t npc;
	npf_conn_t *con;
	npf_rule_t *rl;
	npf_rproc_t *rp;
	int error, retfl;
	int decision;

	/*
	 * Initialise packet information cache.
	 * Note: it is enough to clear the info bits.
	 */
	KASSERT(ifp != NULL);
	nbuf_init(&nbuf, *mp, ifp);
	npc.npc_nbuf = &nbuf;
	npc.npc_info = 0;

	decision = NPF_DECISION_BLOCK;
	error = 0;
	retfl = 0;
	rp = NULL;

	/* Cache everything.  Determine whether it is an IP fragment. */
	if (__predict_false(npf_cache_all(&npc) & NPC_IPFRAG)) {
		/*
		 * Pass to IPv4 or IPv6 reassembly mechanism.
		 */
		error = npf_reassembly(&npc, mp);
		if (error) {
			con = NULL;
			goto out;
		}
		if (*mp == NULL) {
			/* More fragments should come; return. */
			return 0;
		}
	}

	/* Inspect the list of connections (if found, acquires a reference). */
	con = npf_conn_inspect(&npc, di, &error);

	/* If "passing" connection found - skip the ruleset inspection. */
	if (con && npf_conn_pass(con, &rp)) {
		npf_stats_inc(NPF_STAT_PASS_CONN);
		KASSERT(error == 0);
		goto pass;
	}
	if (__predict_false(error)) {
		if (error == ENETUNREACH)
			goto block;
		goto out;
	}

	/* Acquire the lock, inspect the ruleset using this packet. */
	int slock = npf_config_read_enter();
	npf_ruleset_t *rlset = npf_config_ruleset();

	rl = npf_ruleset_inspect(&npc, rlset, di, NPF_LAYER_3);
	if (__predict_false(rl == NULL)) {
		const bool pass = npf_default_pass();
		npf_config_read_exit(slock);

		if (pass) {
			npf_stats_inc(NPF_STAT_PASS_DEFAULT);
			goto pass;
		}
		npf_stats_inc(NPF_STAT_BLOCK_DEFAULT);
		goto block;
	}

	/*
	 * Get the rule procedure (acquires a reference) for association
	 * with a connection (if any) and execution.
	 */
	KASSERT(rp == NULL);
	rp = npf_rule_getrproc(rl);

	/* Conclude with the rule and release the lock. */
	error = npf_rule_conclude(rl, &retfl);
	npf_config_read_exit(slock);

	if (error) {
		npf_stats_inc(NPF_STAT_BLOCK_RULESET);
		goto block;
	}
	npf_stats_inc(NPF_STAT_PASS_RULESET);

	/*
	 * Establish a "pass" connection, if required.  Just proceed if
	 * connection creation fails (e.g. due to unsupported protocol).
	 */
	if ((retfl & NPF_RULE_STATEFUL) != 0 && !con) {
		con = npf_conn_establish(&npc, di,
		    (retfl & NPF_RULE_MULTIENDS) == 0);
		if (con) {
			/*
			 * Note: the reference on the rule procedure is
			 * transfered to the connection.  It will be
			 * released on connection destruction.
			 */
			npf_conn_setpass(con, rp);
		}
	}
pass:
	decision = NPF_DECISION_PASS;
	KASSERT(error == 0);
	/*
	 * Perform NAT.
	 */
	error = npf_do_nat(&npc, con, di);
block:
	/*
	 * Execute the rule procedure, if any is associated.
	 * It may reverse the decision from pass to block.
	 */
	if (rp && !npf_rproc_run(&npc, rp, &decision)) {
		if (con) {
			npf_conn_release(con);
		}
		npf_rproc_release(rp);
		*mp = NULL;
		return 0;
	}
out:
	/*
	 * Release the reference on a connection.  Release the reference
	 * on a rule procedure only if there was no association.
	 */
	if (con) {
		npf_conn_release(con);
	} else if (rp) {
		npf_rproc_release(rp);
	}

	/* Reset mbuf pointer before returning to the caller. */
	if ((*mp = nbuf_head_mbuf(&nbuf)) == NULL) {
		return error ? error : ENOMEM;
	}

	/* Pass the packet if decided and there is no error. */
	if (decision == NPF_DECISION_PASS && !error) {
		/*
		 * XXX: Disable for now, it will be set accordingly later,
		 * for optimisations (to reduce inspection).
		 */
		(*mp)->m_flags &= ~M_CANFASTFWD;
		return 0;
	}

	/*
	 * Block the packet.  ENETUNREACH is used to indicate blocking.
	 * Depending on the flags and protocol, return TCP reset (RST) or
	 * ICMP destination unreachable.
	 */
	if (retfl && npf_return_block(&npc, retfl)) {
		*mp = NULL;
	}

	if (!error) {
		error = ENETUNREACH;
	}

	if (*mp) {
		m_freem(*mp);
		*mp = NULL;
	}
	return error;
}

/*
 * npf_pfil_register: register pfil(9) hooks.
 */
int
npf_pfil_register(bool init)
{
	int error = 0;

	mutex_enter(softnet_lock);
	KERNEL_LOCK(1, NULL);

	/* Init: interface re-config and attach/detach hook. */
	if (!npf_ph_if) {
		npf_ph_if = pfil_head_get(PFIL_TYPE_IFNET, 0);
		if (!npf_ph_if) {
			error = ENOENT;
			goto out;
		}
		error = pfil_add_hook(npf_ifhook, NULL,
		    PFIL_IFADDR | PFIL_IFNET, npf_ph_if);
		KASSERT(error == 0);
	}
	if (init) {
		goto out;
	}

	/* Check if pfil hooks are not already registered. */
	if (pfil_registered) {
		error = EEXIST;
		goto out;
	}

	/* Capture points of the activity in the IP layer. */
	npf_ph_inet = pfil_head_get(PFIL_TYPE_AF, (void *)AF_INET);
	npf_ph_inet6 = pfil_head_get(PFIL_TYPE_AF, (void *)AF_INET6);
	if (!npf_ph_inet && !npf_ph_inet6) {
		error = ENOENT;
		goto out;
	}

	/* Packet IN/OUT handlers for IP layer. */
	if (npf_ph_inet) {
		error = pfil_add_hook(npf_packet_handler, NULL,
		    PFIL_ALL, npf_ph_inet);
		KASSERT(error == 0);
	}
	if (npf_ph_inet6) {
		error = pfil_add_hook(npf_packet_handler, NULL,
		    PFIL_ALL, npf_ph_inet6);
		KASSERT(error == 0);
	}
	pfil_registered = true;
out:
	KERNEL_UNLOCK_ONE(NULL);
	mutex_exit(softnet_lock);

	return error;
}

/*
 * npf_pfil_unregister: unregister pfil(9) hooks.
 */
void
npf_pfil_unregister(bool fini)
{
	mutex_enter(softnet_lock);
	KERNEL_LOCK(1, NULL);

	if (fini && npf_ph_if) {
		(void)pfil_remove_hook(npf_ifhook, NULL,
		    PFIL_IFADDR | PFIL_IFNET, npf_ph_if);
	}
	if (npf_ph_inet) {
		(void)pfil_remove_hook(npf_packet_handler, NULL,
		    PFIL_ALL, npf_ph_inet);
	}
	if (npf_ph_inet6) {
		(void)pfil_remove_hook(npf_packet_handler, NULL,
		    PFIL_ALL, npf_ph_inet6);
	}
	pfil_registered = false;

	KERNEL_UNLOCK_ONE(NULL);
	mutex_exit(softnet_lock);
}

bool
npf_pfil_registered_p(void)
{
	return pfil_registered;
}

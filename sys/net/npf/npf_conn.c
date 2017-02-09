/*	$NetBSD: npf_conn.c,v 1.16 2015/02/05 22:04:03 rmind Exp $	*/

/*-
 * Copyright (c) 2014-2015 Mindaugas Rasiukevicius <rmind at netbsd org>
 * Copyright (c) 2010-2014 The NetBSD Foundation, Inc.
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
 * NPF connection tracking for stateful filtering and translation.
 *
 * Overview
 *
 *	Connection direction is identified by the direction of its first
 *	packet.  Packets can be incoming or outgoing with respect to an
 *	interface.  To describe the packet in the context of connection
 *	direction we will use the terms "forwards stream" and "backwards
 *	stream".  All connections have two keys and thus two entries:
 *
 *		npf_conn_t::c_forw_entry for the forwards stream and
 *		npf_conn_t::c_back_entry for the backwards stream.
 *
 *	The keys are formed from the 5-tuple (source/destination address,
 *	source/destination port and the protocol).  Additional matching
 *	is performed for the interface (a common behaviour is equivalent
 *	to the 6-tuple lookup including the interface ID).  Note that the
 *	key may be formed using translated values in a case of NAT.
 *
 *	Connections can serve two purposes: for the implicit passing or
 *	to accommodate the dynamic NAT.  Connections for the former purpose
 *	are created by the rules with "stateful" attribute and are used for
 *	stateful filtering.  Such connections indicate that the packet of
 *	the backwards stream should be passed without inspection of the
 *	ruleset.  The other purpose is to associate a dynamic NAT mechanism
 *	with a connection.  Such connections are created by the NAT policies
 *	and they have a relationship with NAT translation structure via
 *	npf_conn_t::c_nat.  A single connection can serve both purposes,
 *	which is a common case.
 *
 * Connection life-cycle
 *
 *	Connections are established when a packet matches said rule or
 *	NAT policy.  Both keys of the established connection are inserted
 *	into the connection database.  A garbage collection thread
 *	periodically scans all connections and depending on connection
 *	properties (e.g. last activity time, protocol) removes connection
 *	entries and expires the actual connections.
 *
 *	Each connection has a reference count.  The reference is acquired
 *	on lookup and should be released by the caller.  It guarantees that
 *	the connection will not be destroyed, although it may be expired.
 *
 * Synchronisation
 *
 *	Connection database is accessed in a lock-less manner by the main
 *	routines: npf_conn_inspect() and npf_conn_establish().  Since they
 *	are always called from a software interrupt, the database is
 *	protected using passive serialisation.  The main place which can
 *	destroy a connection is npf_conn_worker().  The database itself
 *	can be replaced and destroyed in npf_conn_reload().
 *
 * ALG support
 *
 *	Application-level gateways (ALGs) can override generic connection
 *	inspection (npf_alg_conn() call in npf_conn_inspect() function) by
 *	performing their own lookup using different key.  Recursive call
 *	to npf_conn_inspect() is not allowed.  The ALGs ought to use the
 *	npf_conn_lookup() function for this purpose.
 *
 * Lock order
 *
 *	npf_config_lock ->
 *		conn_lock ->
 *			npf_conn_t::c_lock
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: npf_conn.c,v 1.16 2015/02/05 22:04:03 rmind Exp $");

#include <sys/param.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <sys/atomic.h>
#include <sys/condvar.h>
#include <sys/kmem.h>
#include <sys/kthread.h>
#include <sys/mutex.h>
#include <net/pfil.h>
#include <sys/pool.h>
#include <sys/queue.h>
#include <sys/systm.h>

#define __NPF_CONN_PRIVATE
#include "npf_conn.h"
#include "npf_impl.h"

/*
 * Connection flags: PFIL_IN and PFIL_OUT values are reserved for direction.
 */
CTASSERT(PFIL_ALL == (0x001 | 0x002));
#define	CONN_ACTIVE	0x004	/* visible on inspection */
#define	CONN_PASS	0x008	/* perform implicit passing */
#define	CONN_EXPIRE	0x010	/* explicitly expire */
#define	CONN_REMOVED	0x020	/* "forw/back" entries removed */

/*
 * Connection tracking state: disabled (off) or enabled (on).
 */
enum { CONN_TRACKING_OFF, CONN_TRACKING_ON };
static volatile int	conn_tracking	__cacheline_aligned;

/* Connection tracking database, connection cache and the lock. */
static npf_conndb_t *	conn_db		__read_mostly;
static pool_cache_t	conn_cache	__read_mostly;
static kmutex_t		conn_lock	__cacheline_aligned;

static void	npf_conn_worker(void);
static void	npf_conn_destroy(npf_conn_t *);

/*
 * npf_conn_sys{init,fini}: initialise/destroy connection tracking.
 */

void
npf_conn_sysinit(void)
{
	conn_cache = pool_cache_init(sizeof(npf_conn_t), coherency_unit,
	    0, 0, "npfconpl", NULL, IPL_NET, NULL, NULL, NULL);
	mutex_init(&conn_lock, MUTEX_DEFAULT, IPL_NONE);
	conn_tracking = CONN_TRACKING_OFF;
	conn_db = npf_conndb_create();

	npf_worker_register(npf_conn_worker);
}

void
npf_conn_sysfini(void)
{
	/* Note: the caller should have flushed the connections. */
	KASSERT(conn_tracking == CONN_TRACKING_OFF);
	npf_worker_unregister(npf_conn_worker);

	npf_conndb_destroy(conn_db);
	pool_cache_destroy(conn_cache);
	mutex_destroy(&conn_lock);
}

/*
 * npf_conn_load: perform the load by flushing the current connection
 * database and replacing it with the new one or just destroying.
 *
 * => The caller must disable the connection tracking and ensure that
 *    there are no connection database lookups or references in-flight.
 */
void
npf_conn_load(npf_conndb_t *ndb, bool track)
{
	npf_conndb_t *odb = NULL;

	KASSERT(npf_config_locked_p());

	/*
	 * The connection database is in the quiescent state.
	 * Prevent G/C thread from running and install a new database.
	 */
	mutex_enter(&conn_lock);
	if (ndb) {
		KASSERT(conn_tracking == CONN_TRACKING_OFF);
		odb = conn_db;
		conn_db = ndb;
		membar_sync();
	}
	if (track) {
		/* After this point lookups start flying in. */
		conn_tracking = CONN_TRACKING_ON;
	}
	mutex_exit(&conn_lock);

	if (odb) {
		/*
		 * Flush all, no sync since the caller did it for us.
		 * Also, release the pool cache memory.
		 */
		npf_conn_gc(odb, true, false);
		npf_conndb_destroy(odb);
		pool_cache_invalidate(conn_cache);
	}
}

/*
 * npf_conn_tracking: enable/disable connection tracking.
 */
void
npf_conn_tracking(bool track)
{
	KASSERT(npf_config_locked_p());
	conn_tracking = track ? CONN_TRACKING_ON : CONN_TRACKING_OFF;
}

static inline bool
npf_conn_trackable_p(const npf_cache_t *npc)
{
	/*
	 * Check if connection tracking is on.  Also, if layer 3 and 4 are
	 * not cached - protocol is not supported or packet is invalid.
	 */
	if (conn_tracking != CONN_TRACKING_ON) {
		return false;
	}
	if (!npf_iscached(npc, NPC_IP46) || !npf_iscached(npc, NPC_LAYER4)) {
		return false;
	}
	return true;
}

/*
 * npf_conn_conkey: construct a key for the connection lookup.
 *
 * => Returns the key length in bytes or zero on failure.
 */
unsigned
npf_conn_conkey(const npf_cache_t *npc, npf_connkey_t *key, const bool forw)
{
	const u_int alen = npc->npc_alen;
	const struct tcphdr *th;
	const struct udphdr *uh;
	u_int keylen, isrc, idst;
	uint16_t id[2];

	switch (npc->npc_proto) {
	case IPPROTO_TCP:
		KASSERT(npf_iscached(npc, NPC_TCP));
		th = npc->npc_l4.tcp;
		id[NPF_SRC] = th->th_sport;
		id[NPF_DST] = th->th_dport;
		break;
	case IPPROTO_UDP:
		KASSERT(npf_iscached(npc, NPC_UDP));
		uh = npc->npc_l4.udp;
		id[NPF_SRC] = uh->uh_sport;
		id[NPF_DST] = uh->uh_dport;
		break;
	case IPPROTO_ICMP:
		if (npf_iscached(npc, NPC_ICMP_ID)) {
			const struct icmp *ic = npc->npc_l4.icmp;
			id[NPF_SRC] = ic->icmp_id;
			id[NPF_DST] = ic->icmp_id;
			break;
		}
		return 0;
	case IPPROTO_ICMPV6:
		if (npf_iscached(npc, NPC_ICMP_ID)) {
			const struct icmp6_hdr *ic6 = npc->npc_l4.icmp6;
			id[NPF_SRC] = ic6->icmp6_id;
			id[NPF_DST] = ic6->icmp6_id;
			break;
		}
		return 0;
	default:
		/* Unsupported protocol. */
		return 0;
	}

	if (__predict_true(forw)) {
		isrc = NPF_SRC, idst = NPF_DST;
	} else {
		isrc = NPF_DST, idst = NPF_SRC;
	}

	/*
	 * Construct a key formed out of 32-bit integers.  The key layout:
	 *
	 * Field: | proto  |  alen  | src-id | dst-id | src-addr | dst-addr |
	 *        +--------+--------+--------+--------+----------+----------+
	 * Bits:  |   16   |   16   |   16   |   16   |  32-128  |  32-128  |
	 *
	 * The source and destination are inverted if they key is for the
	 * backwards stream (forw == false).  The address length depends
	 * on the 'alen' field; it is a length in bytes, either 4 or 16.
	 */

	key->ck_key[0] = ((uint32_t)npc->npc_proto << 16) | (alen & 0xffff);
	key->ck_key[1] = ((uint32_t)id[isrc] << 16) | id[idst];

	if (__predict_true(alen == sizeof(in_addr_t))) {
		key->ck_key[2] = npc->npc_ips[isrc]->s6_addr32[0];
		key->ck_key[3] = npc->npc_ips[idst]->s6_addr32[0];
		keylen = 4 * sizeof(uint32_t);
	} else {
		const u_int nwords = alen >> 2;
		memcpy(&key->ck_key[2], npc->npc_ips[isrc], alen);
		memcpy(&key->ck_key[2 + nwords], npc->npc_ips[idst], alen);
		keylen = (2 + (nwords * 2)) * sizeof(uint32_t);
	}
	return keylen;
}

static __inline void
connkey_set_addr(npf_connkey_t *key, const npf_addr_t *naddr, const int di)
{
	const u_int alen = key->ck_key[0] & 0xffff;
	uint32_t *addr = &key->ck_key[2 + ((alen >> 2) * di)];

	KASSERT(alen > 0);
	memcpy(addr, naddr, alen);
}

static __inline void
connkey_set_id(npf_connkey_t *key, const uint16_t id, const int di)
{
	const uint32_t oid = key->ck_key[1];
	const u_int shift = 16 * !di;
	const uint32_t mask = 0xffff0000 >> shift;

	key->ck_key[1] = ((uint32_t)id << shift) | (oid & mask);
}

/*
 * npf_conn_lookup: lookup if there is an established connection.
 *
 * => If found, we will hold a reference for the caller.
 */
npf_conn_t *
npf_conn_lookup(const npf_cache_t *npc, const int di, bool *forw)
{
	const nbuf_t *nbuf = npc->npc_nbuf;
	npf_conn_t *con;
	npf_connkey_t key;
	u_int flags, cifid;
	bool ok, pforw;

	/* Construct a key and lookup for a connection in the store. */
	if (!npf_conn_conkey(npc, &key, true)) {
		return NULL;
	}
	con = npf_conndb_lookup(conn_db, &key, forw);
	if (con == NULL) {
		return NULL;
	}
	KASSERT(npc->npc_proto == con->c_proto);

	/* Check if connection is active and not expired. */
	flags = con->c_flags;
	ok = (flags & (CONN_ACTIVE | CONN_EXPIRE)) == CONN_ACTIVE;
	if (__predict_false(!ok)) {
		atomic_dec_uint(&con->c_refcnt);
		return NULL;
	}

	/*
	 * Match the interface and the direction of the connection entry
	 * and the packet.
	 */
	cifid = con->c_ifid;
	if (__predict_false(cifid && cifid != nbuf->nb_ifid)) {
		atomic_dec_uint(&con->c_refcnt);
		return NULL;
	}
	pforw = (flags & PFIL_ALL) == di;
	if (__predict_false(*forw != pforw)) {
		atomic_dec_uint(&con->c_refcnt);
		return NULL;
	}

	/* Update the last activity time. */
	getnanouptime(&con->c_atime);
	return con;
}

/*
 * npf_conn_inspect: lookup a connection and inspecting the protocol data.
 *
 * => If found, we will hold a reference for the caller.
 */
npf_conn_t *
npf_conn_inspect(npf_cache_t *npc, const int di, int *error)
{
	nbuf_t *nbuf = npc->npc_nbuf;
	npf_conn_t *con;
	bool forw, ok;

	KASSERT(!nbuf_flag_p(nbuf, NBUF_DATAREF_RESET));
	if (!npf_conn_trackable_p(npc)) {
		return NULL;
	}

	/* Query ALG which may lookup connection for us. */
	if ((con = npf_alg_conn(npc, di)) != NULL) {
		/* Note: reference is held. */
		return con;
	}
	if (nbuf_head_mbuf(nbuf) == NULL) {
		*error = ENOMEM;
		return NULL;
	}
	KASSERT(!nbuf_flag_p(nbuf, NBUF_DATAREF_RESET));

	/* Main lookup of the connection. */
	if ((con = npf_conn_lookup(npc, di, &forw)) == NULL) {
		return NULL;
	}

	/* Inspect the protocol data and handle state changes. */
	mutex_enter(&con->c_lock);
	ok = npf_state_inspect(npc, &con->c_state, forw);
	mutex_exit(&con->c_lock);

	if (__predict_false(!ok)) {
		/* Invalid: let the rules deal with it. */
		npf_conn_release(con);
		npf_stats_inc(NPF_STAT_INVALID_STATE);
		con = NULL;
	}
	return con;
}

/*
 * npf_conn_establish: create a new connection, insert into the global list.
 *
 * => Connection is created with the reference held for the caller.
 * => Connection will be activated on the first reference release.
 */
npf_conn_t *
npf_conn_establish(npf_cache_t *npc, int di, bool per_if)
{
	const nbuf_t *nbuf = npc->npc_nbuf;
	npf_conn_t *con;
	int error = 0;

	KASSERT(!nbuf_flag_p(nbuf, NBUF_DATAREF_RESET));

	if (!npf_conn_trackable_p(npc)) {
		return NULL;
	}

	/* Allocate and initialise the new connection. */
	con = pool_cache_get(conn_cache, PR_NOWAIT);
	if (__predict_false(!con)) {
		return NULL;
	}
	NPF_PRINTF(("NPF: create conn %p\n", con));
	npf_stats_inc(NPF_STAT_CONN_CREATE);

	mutex_init(&con->c_lock, MUTEX_DEFAULT, IPL_SOFTNET);
	con->c_flags = (di & PFIL_ALL);
	con->c_refcnt = 0;
	con->c_rproc = NULL;
	con->c_nat = NULL;

	/* Initialize the protocol state. */
	if (!npf_state_init(npc, &con->c_state)) {
		npf_conn_destroy(con);
		return NULL;
	}

	KASSERT(npf_iscached(npc, NPC_IP46));
	npf_connkey_t *fw = &con->c_forw_entry;
	npf_connkey_t *bk = &con->c_back_entry;

	/*
	 * Construct "forwards" and "backwards" keys.  Also, set the
	 * interface ID for this connection (unless it is global).
	 */
	if (!npf_conn_conkey(npc, fw, true) ||
	    !npf_conn_conkey(npc, bk, false)) {
		npf_conn_destroy(con);
		return NULL;
	}
	fw->ck_backptr = bk->ck_backptr = con;
	con->c_ifid = per_if ? nbuf->nb_ifid : 0;
	con->c_proto = npc->npc_proto;

	/*
	 * Set last activity time for a new connection and acquire
	 * a reference for the caller before we make it visible.
	 */
	getnanouptime(&con->c_atime);
	con->c_refcnt = 1;

	/*
	 * Insert both keys (entries representing directions) of the
	 * connection.  At this point it becomes visible, but we activate
	 * the connection later.
	 */
	mutex_enter(&con->c_lock);
	if (!npf_conndb_insert(conn_db, fw, con)) {
		error = EISCONN;
		goto err;
	}
	if (!npf_conndb_insert(conn_db, bk, con)) {
		npf_conn_t *ret __diagused;
		ret = npf_conndb_remove(conn_db, fw);
		KASSERT(ret == con);
		error = EISCONN;
		goto err;
	}
err:
	/*
	 * If we have hit the duplicate: mark the connection as expired
	 * and let the G/C thread to take care of it.  We cannot do it
	 * here since there might be references acquired already.
	 */
	if (error) {
		atomic_or_uint(&con->c_flags, CONN_REMOVED | CONN_EXPIRE);
		atomic_dec_uint(&con->c_refcnt);
		npf_stats_inc(NPF_STAT_RACE_CONN);
	} else {
		NPF_PRINTF(("NPF: establish conn %p\n", con));
	}

	/* Finally, insert into the connection list. */
	npf_conndb_enqueue(conn_db, con);
	mutex_exit(&con->c_lock);

	return error ? NULL : con;
}

static void
npf_conn_destroy(npf_conn_t *con)
{
	KASSERT(con->c_refcnt == 0);

	if (con->c_nat) {
		/* Release any NAT structures. */
		npf_nat_destroy(con->c_nat);
	}
	if (con->c_rproc) {
		/* Release the rule procedure. */
		npf_rproc_release(con->c_rproc);
	}

	/* Destroy the state. */
	npf_state_destroy(&con->c_state);
	mutex_destroy(&con->c_lock);

	/* Free the structure, increase the counter. */
	pool_cache_put(conn_cache, con);
	npf_stats_inc(NPF_STAT_CONN_DESTROY);
	NPF_PRINTF(("NPF: conn %p destroyed\n", con));
}

/*
 * npf_conn_setnat: associate NAT entry with the connection, update and
 * re-insert connection entry using the translation values.
 *
 * => The caller must be holding a reference.
 */
int
npf_conn_setnat(const npf_cache_t *npc, npf_conn_t *con,
    npf_nat_t *nt, u_int ntype)
{
	static const u_int nat_type_dimap[] = {
		[NPF_NATOUT] = NPF_DST,
		[NPF_NATIN] = NPF_SRC,
	};
	npf_connkey_t key, *bk;
	npf_conn_t *ret __diagused;
	npf_addr_t *taddr;
	in_port_t tport;
	u_int tidx;

	KASSERT(con->c_refcnt > 0);

	npf_nat_gettrans(nt, &taddr, &tport);
	KASSERT(ntype == NPF_NATOUT || ntype == NPF_NATIN);
	tidx = nat_type_dimap[ntype];

	/* Construct a "backwards" key. */
	if (!npf_conn_conkey(npc, &key, false)) {
		return EINVAL;
	}

	/* Acquire the lock and check for the races. */
	mutex_enter(&con->c_lock);
	if (__predict_false(con->c_flags & CONN_EXPIRE)) {
		/* The connection got expired. */
		mutex_exit(&con->c_lock);
		return EINVAL;
	}
	KASSERT((con->c_flags & CONN_REMOVED) == 0);

	if (__predict_false(con->c_nat != NULL)) {
		/* Race with a duplicate packet. */
		mutex_exit(&con->c_lock);
		npf_stats_inc(NPF_STAT_RACE_NAT);
		return EISCONN;
	}

	/* Remove the "backwards" entry. */
	ret = npf_conndb_remove(conn_db, &con->c_back_entry);
	KASSERT(ret == con);

	/* Set the source/destination IDs to the translation values. */
	bk = &con->c_back_entry;
	connkey_set_addr(bk, taddr, tidx);
	if (tport) {
		connkey_set_id(bk, tport, tidx);
	}

	/* Finally, re-insert the "backwards" entry. */
	if (!npf_conndb_insert(conn_db, bk, con)) {
		/*
		 * Race: we have hit the duplicate, remove the "forwards"
		 * entry and expire our connection; it is no longer valid.
		 */
		ret = npf_conndb_remove(conn_db, &con->c_forw_entry);
		KASSERT(ret == con);

		atomic_or_uint(&con->c_flags, CONN_REMOVED | CONN_EXPIRE);
		mutex_exit(&con->c_lock);

		npf_stats_inc(NPF_STAT_RACE_NAT);
		return EISCONN;
	}

	/* Associate the NAT entry and release the lock. */
	con->c_nat = nt;
	mutex_exit(&con->c_lock);
	return 0;
}

/*
 * npf_conn_expire: explicitly mark connection as expired.
 */
void
npf_conn_expire(npf_conn_t *con)
{
	/* KASSERT(con->c_refcnt > 0); XXX: npf_nat_freepolicy() */
	atomic_or_uint(&con->c_flags, CONN_EXPIRE);
}

/*
 * npf_conn_pass: return true if connection is "pass" one, otherwise false.
 */
bool
npf_conn_pass(const npf_conn_t *con, npf_rproc_t **rp)
{
	KASSERT(con->c_refcnt > 0);
	if (__predict_true(con->c_flags & CONN_PASS)) {
		*rp = con->c_rproc;
		return true;
	}
	return false;
}

/*
 * npf_conn_setpass: mark connection as a "pass" one and associate the
 * rule procedure with it.
 */
void
npf_conn_setpass(npf_conn_t *con, npf_rproc_t *rp)
{
	KASSERT((con->c_flags & CONN_ACTIVE) == 0);
	KASSERT(con->c_refcnt > 0);
	KASSERT(con->c_rproc == NULL);

	/*
	 * No need for atomic since the connection is not yet active.
	 * If rproc is set, the caller transfers its reference to us,
	 * which will be released on npf_conn_destroy().
	 */
	atomic_or_uint(&con->c_flags, CONN_PASS);
	con->c_rproc = rp;
}

/*
 * npf_conn_release: release a reference, which might allow G/C thread
 * to destroy this connection.
 */
void
npf_conn_release(npf_conn_t *con)
{
	if ((con->c_flags & (CONN_ACTIVE | CONN_EXPIRE)) == 0) {
		/* Activate: after this, connection is globally visible. */
		atomic_or_uint(&con->c_flags, CONN_ACTIVE);
	}
	KASSERT(con->c_refcnt > 0);
	atomic_dec_uint(&con->c_refcnt);
}

/*
 * npf_conn_getnat: return associated NAT data entry and indicate
 * whether it is a "forwards" or "backwards" stream.
 */
npf_nat_t *
npf_conn_getnat(npf_conn_t *con, const int di, bool *forw)
{
	KASSERT(con->c_refcnt > 0);
	*forw = (con->c_flags & PFIL_ALL) == di;
	return con->c_nat;
}

/*
 * npf_conn_expired: criterion to check if connection is expired.
 */
static inline bool
npf_conn_expired(const npf_conn_t *con, const struct timespec *tsnow)
{
	const int etime = npf_state_etime(&con->c_state, con->c_proto);
	struct timespec tsdiff;

	if (__predict_false(con->c_flags & CONN_EXPIRE)) {
		/* Explicitly marked to be expired. */
		return true;
	}
	timespecsub(tsnow, &con->c_atime, &tsdiff);
	return tsdiff.tv_sec > etime;
}

/*
 * npf_conn_gc: garbage collect the expired connections.
 *
 * => Must run in a single-threaded manner.
 * => If it is a flush request, then destroy all connections.
 * => If 'sync' is true, then perform passive serialisation.
 */
void
npf_conn_gc(npf_conndb_t *cd, bool flush, bool sync)
{
	npf_conn_t *con, *prev, *gclist = NULL;
	struct timespec tsnow;

	getnanouptime(&tsnow);

	/*
	 * Scan all connections and check them for expiration.
	 */
	prev = NULL;
	con = npf_conndb_getlist(cd);
	while (con) {
		npf_conn_t *next = con->c_next;

		/* Expired?  Flushing all? */
		if (!npf_conn_expired(con, &tsnow) && !flush) {
			prev = con;
			con = next;
			continue;
		}

		/* Remove both entries of the connection. */
		mutex_enter(&con->c_lock);
		if ((con->c_flags & CONN_REMOVED) == 0) {
			npf_conn_t *ret __diagused;

			ret = npf_conndb_remove(cd, &con->c_forw_entry);
			KASSERT(ret == con);
			ret = npf_conndb_remove(cd, &con->c_back_entry);
			KASSERT(ret == con);
		}

		/* Flag the removal and expiration. */
		atomic_or_uint(&con->c_flags, CONN_REMOVED | CONN_EXPIRE);
		mutex_exit(&con->c_lock);

		/* Move to the G/C list. */
		npf_conndb_dequeue(cd, con, prev);
		con->c_next = gclist;
		gclist = con;

		/* Next.. */
		con = next;
	}
	npf_conndb_settail(cd, prev);

	/*
	 * Ensure it is safe to destroy the connections.
	 * Note: drop the conn_lock (see the lock order).
	 */
	if (sync) {
		mutex_exit(&conn_lock);
		if (gclist) {
			npf_config_enter();
			npf_config_sync();
			npf_config_exit();
		}
	}

	/*
	 * Garbage collect all expired connections.
	 * May need to wait for the references to drain.
	 */
	con = gclist;
	while (con) {
		npf_conn_t *next = con->c_next;

		/*
		 * Destroy only if removed and no references.
		 * Otherwise, wait for a tiny moment.
		 */
		if (__predict_false(con->c_refcnt)) {
			kpause("npfcongc", false, 1, NULL);
			continue;
		}
		npf_conn_destroy(con);
		con = next;
	}
}

/*
 * npf_conn_worker: G/C to run from a worker thread.
 */
static void
npf_conn_worker(void)
{
	mutex_enter(&conn_lock);
	/* Note: the conn_lock will be released (sync == true). */
	npf_conn_gc(conn_db, false, true);
}

/*
 * npf_conndb_export: construct a list of connections prepared for saving.
 * Note: this is expected to be an expensive operation.
 */
int
npf_conndb_export(prop_array_t conlist)
{
	npf_conn_t *con, *prev;

	/*
	 * Note: acquire conn_lock to prevent from the database
	 * destruction and G/C thread.
	 */
	mutex_enter(&conn_lock);
	if (conn_tracking != CONN_TRACKING_ON) {
		mutex_exit(&conn_lock);
		return 0;
	}
	prev = NULL;
	con = npf_conndb_getlist(conn_db);
	while (con) {
		npf_conn_t *next = con->c_next;
		prop_dictionary_t cdict;

		if ((cdict = npf_conn_export(con)) != NULL) {
			prop_array_add(conlist, cdict);
			prop_object_release(cdict);
		}
		prev = con;
		con = next;
	}
	npf_conndb_settail(conn_db, prev);
	mutex_exit(&conn_lock);
	return 0;
}

/*
 * npf_conn_export: serialise a single connection.
 */
prop_dictionary_t
npf_conn_export(const npf_conn_t *con)
{
	prop_dictionary_t cdict;
	prop_data_t d;

	if ((con->c_flags & (CONN_ACTIVE|CONN_EXPIRE)) != CONN_ACTIVE) {
		return NULL;
	}
	cdict = prop_dictionary_create();
	prop_dictionary_set_uint32(cdict, "flags", con->c_flags);
	prop_dictionary_set_uint32(cdict, "proto", con->c_proto);
	if (con->c_ifid) {
		const char *ifname = npf_ifmap_getname(con->c_ifid);
		prop_dictionary_set_cstring(cdict, "ifname", ifname);
	}

	d = prop_data_create_data(&con->c_state, sizeof(npf_state_t));
	prop_dictionary_set_and_rel(cdict, "state", d);

	const uint32_t *fkey = con->c_forw_entry.ck_key;
	d = prop_data_create_data(fkey, NPF_CONN_MAXKEYLEN);
	prop_dictionary_set_and_rel(cdict, "forw-key", d);

	const uint32_t *bkey = con->c_back_entry.ck_key;
	d = prop_data_create_data(bkey, NPF_CONN_MAXKEYLEN);
	prop_dictionary_set_and_rel(cdict, "back-key", d);

	if (con->c_nat) {
		npf_nat_export(cdict, con->c_nat);
	}
	return cdict;
}

/*
 * npf_conn_import: fully reconstruct a single connection from a
 * directory and insert into the given database.
 */
int
npf_conn_import(npf_conndb_t *cd, prop_dictionary_t cdict,
    npf_ruleset_t *natlist)
{
	npf_conn_t *con;
	npf_connkey_t *fw, *bk;
	prop_object_t obj;
	const char *ifname;
	const void *d;

	/* Allocate a connection and initialise it (clear first). */
	con = pool_cache_get(conn_cache, PR_WAITOK);
	memset(con, 0, sizeof(npf_conn_t));
	mutex_init(&con->c_lock, MUTEX_DEFAULT, IPL_SOFTNET);
	npf_stats_inc(NPF_STAT_CONN_CREATE);

	prop_dictionary_get_uint32(cdict, "proto", &con->c_proto);
	prop_dictionary_get_uint32(cdict, "flags", &con->c_flags);
	con->c_flags &= PFIL_ALL | CONN_ACTIVE | CONN_PASS;
	getnanouptime(&con->c_atime);

	if (prop_dictionary_get_cstring_nocopy(cdict, "ifname", &ifname) &&
	    (con->c_ifid = npf_ifmap_register(ifname)) == 0) {
		goto err;
	}

	obj = prop_dictionary_get(cdict, "state");
	if ((d = prop_data_data_nocopy(obj)) == NULL ||
	    prop_data_size(obj) != sizeof(npf_state_t)) {
		goto err;
	}
	memcpy(&con->c_state, d, sizeof(npf_state_t));

	/* Reconstruct NAT association, if any. */
	if ((obj = prop_dictionary_get(cdict, "nat")) != NULL &&
	    (con->c_nat = npf_nat_import(obj, natlist, con)) == NULL) {
		goto err;
	}

	/*
	 * Fetch and copy the keys for each direction.
	 */
	obj = prop_dictionary_get(cdict, "forw-key");
	if ((d = prop_data_data_nocopy(obj)) == NULL ||
	    prop_data_size(obj) != NPF_CONN_MAXKEYLEN) {
		goto err;
	}
	fw = &con->c_forw_entry;
	memcpy(&fw->ck_key, d, NPF_CONN_MAXKEYLEN);

	obj = prop_dictionary_get(cdict, "back-key");
	if ((d = prop_data_data_nocopy(obj)) == NULL ||
	    prop_data_size(obj) != NPF_CONN_MAXKEYLEN) {
		goto err;
	}
	bk = &con->c_back_entry;
	memcpy(&bk->ck_key, d, NPF_CONN_MAXKEYLEN);

	fw->ck_backptr = bk->ck_backptr = con;

	/* Insert the entries and the connection itself. */
	if (!npf_conndb_insert(cd, fw, con)) {
		goto err;
	}
	if (!npf_conndb_insert(cd, bk, con)) {
		npf_conndb_remove(cd, fw);
		goto err;
	}

	NPF_PRINTF(("NPF: imported conn %p\n", con));
	npf_conndb_enqueue(cd, con);
	return 0;
err:
	npf_conn_destroy(con);
	return EINVAL;
}

#if defined(DDB) || defined(_NPF_TESTING)

void
npf_conn_print(const npf_conn_t *con)
{
	const u_int alen = NPF_CONN_GETALEN(&con->c_forw_entry);
	const uint32_t *fkey = con->c_forw_entry.ck_key;
	const uint32_t *bkey = con->c_back_entry.ck_key;
	const u_int proto = con->c_proto;
	struct timespec tsnow, tsdiff;
	const void *src, *dst;
	int etime;

	getnanouptime(&tsnow);
	timespecsub(&tsnow, &con->c_atime, &tsdiff);
	etime = npf_state_etime(&con->c_state, proto);

	printf("%p:\n\tproto %d flags 0x%x tsdiff %d etime %d\n",
	    con, proto, con->c_flags, (int)tsdiff.tv_sec, etime);

	src = &fkey[2], dst = &fkey[2 + (alen >> 2)];
	printf("\tforw %s:%d", npf_addr_dump(src, alen), ntohs(fkey[1] >> 16));
	printf("-> %s:%d\n", npf_addr_dump(dst, alen), ntohs(fkey[1] & 0xffff));

	src = &bkey[2], dst = &bkey[2 + (alen >> 2)];
	printf("\tback %s:%d", npf_addr_dump(src, alen), ntohs(bkey[1] >> 16));
	printf("-> %s:%d\n", npf_addr_dump(dst, alen), ntohs(bkey[1] & 0xffff));

	npf_state_dump(&con->c_state);
	if (con->c_nat) {
		npf_nat_dump(con->c_nat);
	}
}

#endif

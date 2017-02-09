/*	$NetBSD: npf_conndb.c,v 1.2 2014/07/23 01:25:34 rmind Exp $	*/

/*-
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
 * NPF connection storage.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: npf_conndb.c,v 1.2 2014/07/23 01:25:34 rmind Exp $");

#include <sys/param.h>
#include <sys/types.h>

#include <sys/atomic.h>
#include <sys/cprng.h>
#include <sys/hash.h>
#include <sys/kmem.h>

#define __NPF_CONN_PRIVATE
#include "npf_conn.h"
#include "npf_impl.h"

#define	CONNDB_HASH_BUCKETS	1024	/* XXX tune + make tunable */
#define	CONNDB_HASH_MASK	(CONNDB_HASH_BUCKETS - 1)

typedef struct {
	rb_tree_t		hb_tree;
	krwlock_t		hb_lock;
	u_int			hb_count;
} npf_hashbucket_t;

struct npf_conndb {
	npf_conn_t *		cd_recent;
	npf_conn_t *		cd_list;
	npf_conn_t *		cd_tail;
	uint32_t		cd_seed;
	npf_hashbucket_t	cd_hashtbl[];
};

/*
 * Connection hash table and RB-tree helper routines.
 * Note: (node1 < node2) shall return negative.
 */

static signed int
conndb_rbtree_cmp_nodes(void *ctx, const void *n1, const void *n2)
{
	const npf_connkey_t * const ck1 = n1;
	const npf_connkey_t * const ck2 = n2;
	const u_int keylen = MIN(NPF_CONN_KEYLEN(ck1), NPF_CONN_KEYLEN(ck2));

	KASSERT((keylen >> 2) <= NPF_CONN_NKEYWORDS);
	return memcmp(ck1->ck_key, ck2->ck_key, keylen);
}

static signed int
conndb_rbtree_cmp_key(void *ctx, const void *n1, const void *key)
{
	const npf_connkey_t * const ck1 = n1;
	const npf_connkey_t * const ck2 = key;
	return conndb_rbtree_cmp_nodes(ctx, ck1, ck2);
}

static const rb_tree_ops_t conndb_rbtree_ops = {
	.rbto_compare_nodes	= conndb_rbtree_cmp_nodes,
	.rbto_compare_key	= conndb_rbtree_cmp_key,
	.rbto_node_offset	= offsetof(npf_connkey_t, ck_rbnode),
	.rbto_context		= NULL
};

static npf_hashbucket_t *
conndb_hash_bucket(npf_conndb_t *cd, const npf_connkey_t *key)
{
	const u_int keylen = NPF_CONN_KEYLEN(key);
	uint32_t hash = murmurhash2(key->ck_key, keylen, cd->cd_seed);
	return &cd->cd_hashtbl[hash & CONNDB_HASH_MASK];
}

npf_conndb_t *
npf_conndb_create(void)
{
	size_t len = offsetof(npf_conndb_t, cd_hashtbl[CONNDB_HASH_BUCKETS]);
	npf_conndb_t *cd;

	cd = kmem_zalloc(len, KM_SLEEP);
	for (u_int i = 0; i < CONNDB_HASH_BUCKETS; i++) {
		npf_hashbucket_t *hb = &cd->cd_hashtbl[i];

		rb_tree_init(&hb->hb_tree, &conndb_rbtree_ops);
		rw_init(&hb->hb_lock);
		hb->hb_count = 0;
	}
	cd->cd_seed = cprng_fast32();
	return cd;
}

void
npf_conndb_destroy(npf_conndb_t *cd)
{
	size_t len = offsetof(npf_conndb_t, cd_hashtbl[CONNDB_HASH_BUCKETS]);

	KASSERT(cd->cd_recent == NULL);
	KASSERT(cd->cd_list == NULL);
	KASSERT(cd->cd_tail == NULL);

	for (u_int i = 0; i < CONNDB_HASH_BUCKETS; i++) {
		npf_hashbucket_t *hb = &cd->cd_hashtbl[i];

		KASSERT(hb->hb_count == 0);
		KASSERT(!rb_tree_iterate(&hb->hb_tree, NULL, RB_DIR_LEFT));
		rw_destroy(&hb->hb_lock);
	}
	kmem_free(cd, len);
}

/*
 * npf_conndb_lookup: find a connection given the key.
 */
npf_conn_t *
npf_conndb_lookup(npf_conndb_t *cd, const npf_connkey_t *key, bool *forw)
{
	npf_connkey_t *foundkey;
	npf_hashbucket_t *hb;
	npf_conn_t *con;

	/* Get a hash bucket from the cached key data. */
	hb = conndb_hash_bucket(cd, key);
	if (hb->hb_count == 0) {
		return NULL;
	}

	/* Lookup the tree given the key and get the actual connection. */
	rw_enter(&hb->hb_lock, RW_READER);
	foundkey = rb_tree_find_node(&hb->hb_tree, key);
	if (foundkey == NULL) {
		rw_exit(&hb->hb_lock);
		return NULL;
	}
	con = foundkey->ck_backptr;
	*forw = (foundkey == &con->c_forw_entry);

	/* Acquire the reference and return the connection. */
	atomic_inc_uint(&con->c_refcnt);
	rw_exit(&hb->hb_lock);
	return con;
}

/*
 * npf_conndb_insert: insert the key representing the connection.
 */
bool
npf_conndb_insert(npf_conndb_t *cd, npf_connkey_t *key, npf_conn_t *con)
{
	npf_hashbucket_t *hb = conndb_hash_bucket(cd, key);
	bool ok;

	rw_enter(&hb->hb_lock, RW_WRITER);
	ok = rb_tree_insert_node(&hb->hb_tree, key) == key;
	hb->hb_count += (u_int)ok;
	rw_exit(&hb->hb_lock);
	return ok;
}

/*
 * npf_conndb_remove: find and delete the key and return the connection
 * it represents.
 */
npf_conn_t *
npf_conndb_remove(npf_conndb_t *cd, const npf_connkey_t *key)
{
	npf_hashbucket_t *hb = conndb_hash_bucket(cd, key);
	npf_connkey_t *foundkey;
	npf_conn_t *con;

	rw_enter(&hb->hb_lock, RW_WRITER);
	if ((foundkey = rb_tree_find_node(&hb->hb_tree, key)) != NULL) {
		rb_tree_remove_node(&hb->hb_tree, foundkey);
		con = foundkey->ck_backptr;
		hb->hb_count--;
	} else {
		con = NULL;
	}
	rw_exit(&hb->hb_lock);
	return con;
}

/*
 * npf_conndb_enqueue: atomically insert the connection into the
 * singly-linked list of "recent" connections.
 */
void
npf_conndb_enqueue(npf_conndb_t *cd, npf_conn_t *con)
{
	npf_conn_t *head;

	do {
		head = cd->cd_recent;
		con->c_next = head;
	} while (atomic_cas_ptr(&cd->cd_recent, head, con) != head);
}

/*
 * npf_conndb_dequeue: remove the connection from a singly-linked list
 * given the previous element; no concurrent writers are allowed here.
 */
void
npf_conndb_dequeue(npf_conndb_t *cd, npf_conn_t *con, npf_conn_t *prev)
{
	if (prev == NULL) {
		KASSERT(cd->cd_list == con);
		cd->cd_list = con->c_next;
	} else {
		prev->c_next = con->c_next;
	}
}

/*
 * npf_conndb_getlist: atomically take the "recent" connections and add
 * them to the singly-linked list of the connections.
 */
npf_conn_t *
npf_conndb_getlist(npf_conndb_t *cd)
{
	npf_conn_t *con, *prev;

	con = atomic_swap_ptr(&cd->cd_recent, NULL);
	if ((prev = cd->cd_tail) == NULL) {
		KASSERT(cd->cd_list == NULL);
		cd->cd_list = con;
	} else {
		KASSERT(prev->c_next == NULL);
		prev->c_next = con;
	}
	return cd->cd_list;
}

/*
 * npf_conndb_settail: assign a new tail of the singly-linked list.
 */
void
npf_conndb_settail(npf_conndb_t *cd, npf_conn_t *con)
{
	KASSERT(con || cd->cd_list == NULL);
	KASSERT(!con || con->c_next == NULL);
	cd->cd_tail = con;
}

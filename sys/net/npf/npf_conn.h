/*	$NetBSD: npf_conn.h,v 1.8 2014/12/20 16:19:43 rmind Exp $	*/

/*-
 * Copyright (c) 2009-2014 The NetBSD Foundation, Inc.
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

#ifndef _NPF_CONN_H_
#define _NPF_CONN_H_

#if !defined(_KERNEL)
#error "kernel-level header only"
#endif

#include <sys/types.h>

#include "npf_impl.h"

typedef struct npf_connkey npf_connkey_t;

#if defined(__NPF_CONN_PRIVATE)

#include <sys/rbtree.h>

/*
 * See npf_conn_conkey() function for the key layout description.
 */
#define	NPF_CONN_NKEYWORDS	(2 + ((sizeof(npf_addr_t) * 2) >> 2))
#define	NPF_CONN_MAXKEYLEN	(NPF_CONN_NKEYWORDS * sizeof(uint32_t))
#define	NPF_CONN_GETALEN(key)	((key)->ck_key[0] & 0xffff)
#define	NPF_CONN_KEYLEN(key)	(8 + (2 * NPF_CONN_GETALEN(key)))

struct npf_connkey {
	/* Entry node and back-pointer to the actual connection. */
	rb_node_t		ck_rbnode;
	uint32_t		ck_key[NPF_CONN_NKEYWORDS];
	npf_conn_t *		ck_backptr;
};

/*
 * The main connection tracking structure.
 */

struct npf_conn {
	/*
	 * Connection "forwards" and "backwards" entries, plus the
	 * interface ID (if zero, then the state is global).
	 */
	npf_connkey_t		c_forw_entry;
	npf_connkey_t		c_back_entry;
	u_int			c_proto;
	u_int			c_ifid;

	/* Flags and entry in the connection database or G/C list. */
	u_int			c_flags;
	npf_conn_t *		c_next;

	/* Associated rule procedure or NAT (if any). */
	npf_rproc_t *		c_rproc;
	npf_nat_t *		c_nat;

	/*
	 * The protocol state, reference count and the last activity
	 * time (used to calculate expiration time).
	 */
	kmutex_t		c_lock;
	npf_state_t		c_state;
	u_int			c_refcnt;
	struct timespec		c_atime;
};

#endif

/*
 * Connection tracking interface.
 */
void		npf_conn_sysinit(void);
void		npf_conn_sysfini(void);
void		npf_conn_tracking(bool);
void		npf_conn_load(npf_conndb_t *, bool);

unsigned	npf_conn_conkey(const npf_cache_t *, npf_connkey_t *, bool);
npf_conn_t *	npf_conn_lookup(const npf_cache_t *, const int, bool *);
npf_conn_t *	npf_conn_inspect(npf_cache_t *, const int, int *);
npf_conn_t *	npf_conn_establish(npf_cache_t *, int, bool);
void		npf_conn_release(npf_conn_t *);
void		npf_conn_expire(npf_conn_t *);
bool		npf_conn_pass(const npf_conn_t *, npf_rproc_t **);
void		npf_conn_setpass(npf_conn_t *, npf_rproc_t *);
int		npf_conn_setnat(const npf_cache_t *, npf_conn_t *,
		    npf_nat_t *, u_int);
npf_nat_t *	npf_conn_getnat(npf_conn_t *, const int, bool *);
void		npf_conn_gc(npf_conndb_t *, bool, bool);
int		npf_conn_import(npf_conndb_t *, prop_dictionary_t,
		    npf_ruleset_t *);
prop_dictionary_t npf_conn_export(const npf_conn_t *);
void		npf_conn_print(const npf_conn_t *);

/*
 * Connection database (aka state table) interface.
 */
npf_conndb_t *	npf_conndb_create(void);
void		npf_conndb_destroy(npf_conndb_t *);

npf_conn_t *	npf_conndb_lookup(npf_conndb_t *, const npf_connkey_t *,
		    bool *);
bool		npf_conndb_insert(npf_conndb_t *, npf_connkey_t *,
		    npf_conn_t *);
npf_conn_t *	npf_conndb_remove(npf_conndb_t *, const npf_connkey_t *);

void		npf_conndb_enqueue(npf_conndb_t *, npf_conn_t *);
void		npf_conndb_dequeue(npf_conndb_t *, npf_conn_t *,
		    npf_conn_t *);
npf_conn_t *	npf_conndb_getlist(npf_conndb_t *);
void		npf_conndb_settail(npf_conndb_t *, npf_conn_t *);
int		npf_conndb_export(prop_array_t);

#endif	/* _NPF_CONN_H_ */

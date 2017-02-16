/*	$NetBSD: mdb6.c,v 1.5 2014/07/12 12:09:38 spz Exp $	*/
/*
 * Copyright (C) 2007-2013 by Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: mdb6.c,v 1.5 2014/07/12 12:09:38 spz Exp $");

/*!
 * \todo assert()
 * \todo simplify functions, as pool is now in iaaddr
 */

/*! \file server/mdb6.c
 *
 * \page ipv6structures IPv6 Structures Overview
 *
 * A brief description of the IPv6 structures as reverse engineered.
 *
 * There are four major data structures in the lease configuraion.
 *
 * - shared_network - The shared network is the outer enclosing scope for a
 *                    network region that shares a broadcast domain.  It is
 *                    composed of one or more subnets all of which are valid
 *                    in the given region.  The share network may be
 *                    explicitly defined or implicitly created if there is
 *                    only a subnet statement.  This structrure is shared
 *                    with v4.  Each shared network statment or naked subnet
 *                    will map to one of these structures
 *
 * - subnet     - The subnet structure mostly specifies the address range
 *                that could be valid in a given region.  This structute
 *                doesn't include the addresses that the server can delegate
 *                those are in the ipv6_pool.  This structure is also shared
 *                with v4.  Each subnet statement will map to one of these
 *                structures.
 *
 * - ipv6_pond  - The pond structure is a grouping of the address and prefix
 *                information via the pointers to the ipv6_pool and the
 *                allowability of this pool for given clinets via the permit
 *                lists and the valid TIMEs.  This is equivilent to the v4
 *                pool structure and would have been named ip6_pool except
 *                that the name was already in use.  Generally each pool6
 *                statement will map to one of these structures. In addition
 *                there may be one or for each group of naked range6 and
 *                prefix6 statements within a shared network that share
 *                the same group of statements.
 *
 * - ipv6_pool - this contains information about a pool of addresses or prefixes
 *               that the server is using.  This includes a hash table that
 *               tracks the active items and a pair of heap tables one for
 *               active items and one for non-active items.  The heap tables
 *               are used to determine the next items to be modified due to
 *               timing events (expire mostly).  
 * 
 * The linkages then look like this:
 * \verbatim
 *+--------------+   +-------------+
 *|Shared Network|   | ipv6_pond   |
 *|   group      |   |   group     |
 *|              |   | permit info |
 *|              |   |    next    ---->
 *|    ponds    ---->|             |
 *|              |<----  shared    |
 *|   Subnets    |   |    pools    |
 *+-----|--------+   +------|------+
 *      |  ^                |    ^
 *      |  |                v    |
 *      |  |         +-----------|-+
 *      |  |         | ipv6_pool | |
 *      |  |         |    type   | |
 *      |  |         |   ipv6_pond |
 *      |  |         |             |
 *      |  |         |    next    ---->    
 *      |  |         |             |
 *      |  |         |   subnet    |
 *      |  |         +-----|-------+
 *      |  |               |
 *      |  |               v
 *      |  |         +-------------+
 *      |  |         |   subnet    |
 *      |  +----------   shared    |
 *      +----------->|             |
 *                   |   group     |
 *                   +-------------+
 *
 * The shared network contains a list of all the subnets that are on a broadcast
 * doamin.  These can be used to determine if an address makes sense in a given
 * domain, but the subnets do not contain the addresses the server can delegate.
 * Those are stored in the ponds and pools.
 *
 * In the simple case to find an acceptable address the server would first find
 * the shared network the client is on based on either the interface used to 
 * receive the request or the relay agent's information.  From the shared 
 * network the server will walk through it's list of ponds.  For each pond it 
 * will evaluate the permit information against the (already done) classification.
 * If it finds an acceptable pond it will then walk through the pools for that
 * pond.  The server first checks the type of the pool (NA, TA and PD) agaisnt the
 * request and if they match it attemps to find an address within that pool.  On
 * success the address is used, on failure the server steps to the next pool and
 * if necessary to the next pond.
 *
 * When the server is successful in finding an address it will execute any
 * statements assocaited with the pond, then the subnet, then the shared
 * network the group field is for in the above picture).
 *
 * In configurations that don't include either a shared network or a pool6
 * statement (or both) the missing pieces are created.
 * 
 *
 * There are three major data structuress involved in the lease database:
 *
 * - ipv6_pool - see above
 * - ia_xx   - this contains information about a single IA from a request
 *             normally it will contain one pointer to a lease for the client
 *             but it may contain more in some circumstances.  There are 3
 *             hash tables to aid in accessing these one each for NA, TA and PD.
 * - iasubopt - the v6 lease structure.  These are created dynamically when
 *              a client asks for something and will eventually be destroyed
 *              if the client doesn't re-ask for that item.  A lease has space
 *              for backpointers to the IA and to the pool to which it belongs.
 *              The pool backpointer is always filled, the IA pointer may not be.
 *
 * In normal use we then have something like this:
 *
 * \verbatim
 * ia hash tables
 *  ia_na_active                           +----------------+
 *  ia_ta_active          +------------+   | pool           |
 *  ia_pd_active          | iasubopt   |<--|  active hash   |
 * +-----------------+    | aka lease  |<--|  active heap   |
 * | ia_xx           |    |  pool ptr  |-->|                |
 * |  iasubopt array |<---|  iaptr     |<--|  inactive heap |
 * |   lease ptr     |--->|            |   |                |
 * +-----------------+    +------------+   +----------------+
 * \endverbatim
 *
 * For the pool either the inactive heap will have a pointer
 * or both the active heap and the active hash will have pointers.
 *
 * I think there are several major items to notice.   The first is
 * that as a lease moves around it will be added to and removed
 * from the address hash table in the pool and between the active
 * and inactive hash tables.  The hash table and the active heap
 * are used when the lease is either active or abandoned.  The
 * inactive heap is used for all other states.  In particular a
 * lease that has expired or been released will be cleaned
 * (DDNS removal etc) and then moved to the inactive heap.  After
 * some time period (currently 1 hour) it will be freed.
 *
 * The second is that when a client requests specific addresses,
 * either because it previously owned them or if the server supplied
 * them as part of a solicit, the server will try to lookup the ia_xx
 * associated with the client and find the addresses there.  If it
 * does find appropriate leases it moves them from the old IA to
 * a new IA and eventually replaces the old IA with the new IA
 * in the IA hash tables.
 *
 */
#include "config.h"

#include <sys/types.h>
#include <time.h>
#include <netinet/in.h>

#include <stdarg.h>
#include "dhcpd.h"
#include "omapip/omapip.h"
#include "omapip/hash.h"
#include <isc/md5.h>

HASH_FUNCTIONS(ia, unsigned char *, struct ia_xx, ia_hash_t,
	       ia_reference, ia_dereference, do_string_hash)

ia_hash_t *ia_na_active;
ia_hash_t *ia_ta_active;
ia_hash_t *ia_pd_active;

HASH_FUNCTIONS(iasubopt, struct in6_addr *, struct iasubopt, iasubopt_hash_t,
	       iasubopt_reference, iasubopt_dereference, do_string_hash)

struct ipv6_pool **pools;
int num_pools;

/*
 * Create a new IAADDR/PREFIX structure.
 *
 * - iasubopt must be a pointer to a (struct iasubopt *) pointer previously
 *   initialized to NULL
 */
isc_result_t
iasubopt_allocate(struct iasubopt **iasubopt, const char *file, int line) {
	struct iasubopt *tmp;

	if (iasubopt == NULL) {
		log_error("%s(%d): NULL pointer reference", file, line);
		return DHCP_R_INVALIDARG;
	}
	if (*iasubopt != NULL) {
		log_error("%s(%d): non-NULL pointer", file, line);
		return DHCP_R_INVALIDARG;
	}

	tmp = dmalloc(sizeof(*tmp), file, line);
	if (tmp == NULL) {
		return ISC_R_NOMEMORY;
	}

	tmp->refcnt = 1;
	tmp->state = FTS_FREE;
	tmp->heap_index = -1;
	tmp->plen = 255;

	*iasubopt = tmp;
	return ISC_R_SUCCESS;
}

/*
 * Reference an IAADDR/PREFIX structure.
 *
 * - iasubopt must be a pointer to a (struct iasubopt *) pointer previously
 *   initialized to NULL
 */
isc_result_t
iasubopt_reference(struct iasubopt **iasubopt, struct iasubopt *src,
		 const char *file, int line) {
	if (iasubopt == NULL) {
		log_error("%s(%d): NULL pointer reference", file, line);
		return DHCP_R_INVALIDARG;
	}
	if (*iasubopt != NULL) {
		log_error("%s(%d): non-NULL pointer", file, line);
		return DHCP_R_INVALIDARG;
	}
	if (src == NULL) {
		log_error("%s(%d): NULL pointer reference", file, line);
		return DHCP_R_INVALIDARG;
	}
	*iasubopt = src;
	src->refcnt++;
	return ISC_R_SUCCESS;
}


/*
 * Dereference an IAADDR/PREFIX structure.
 *
 * If it is the last reference, then the memory for the 
 * structure is freed.
 */
isc_result_t
iasubopt_dereference(struct iasubopt **iasubopt, const char *file, int line) {
	struct iasubopt *tmp;

	if ((iasubopt == NULL) || (*iasubopt == NULL)) {
		log_error("%s(%d): NULL pointer", file, line);
		return DHCP_R_INVALIDARG;
	}

	tmp = *iasubopt;
	*iasubopt = NULL;

	tmp->refcnt--;
	if (tmp->refcnt < 0) {
		log_error("%s(%d): negative refcnt", file, line);
		tmp->refcnt = 0;
	}
	if (tmp->refcnt == 0) {
		if (tmp->ia != NULL) {
			ia_dereference(&(tmp->ia), file, line);
		}
		if (tmp->ipv6_pool != NULL) {
			ipv6_pool_dereference(&(tmp->ipv6_pool), file, line);
		}
		if (tmp->scope != NULL) {
			binding_scope_dereference(&tmp->scope, file, line);
		}

		if (tmp->on_star.on_expiry != NULL) {
			executable_statement_dereference
				(&tmp->on_star.on_expiry, MDL);
		}
		if (tmp->on_star.on_commit != NULL) {
			executable_statement_dereference
				(&tmp->on_star.on_commit, MDL);
		}
		if (tmp->on_star.on_release != NULL) {
			executable_statement_dereference
				(&tmp->on_star.on_release, MDL);
		}

		dfree(tmp, file, line);
	}

	return ISC_R_SUCCESS;
}

/* 
 * Make the key that we use for IA.
 */
isc_result_t
ia_make_key(struct data_string *key, u_int32_t iaid,
	    const char *duid, unsigned int duid_len,
	    const char *file, int line) {

	memset(key, 0, sizeof(*key));
	key->len = duid_len + sizeof(iaid);
	if (!buffer_allocate(&(key->buffer), key->len, file, line)) {
		return ISC_R_NOMEMORY;
	}
	key->data = key->buffer->data;
	memcpy((char *)key->data, &iaid, sizeof(iaid));
	memcpy((char *)key->data + sizeof(iaid), duid, duid_len);

	return ISC_R_SUCCESS;
}

/*
 * Create a new IA structure.
 *
 * - ia must be a pointer to a (struct ia_xx *) pointer previously
 *   initialized to NULL
 * - iaid and duid are values from the client
 *
 * XXXsk: we don't concern ourself with the byte order of the IAID, 
 *        which might be a problem if we transfer this structure 
 *        between machines of different byte order
 */
isc_result_t
ia_allocate(struct ia_xx **ia, u_int32_t iaid, 
	    const char *duid, unsigned int duid_len,
	    const char *file, int line) {
	struct ia_xx *tmp;

	if (ia == NULL) {
		log_error("%s(%d): NULL pointer reference", file, line);
		return DHCP_R_INVALIDARG;
	}
	if (*ia != NULL) {
		log_error("%s(%d): non-NULL pointer", file, line);
		return DHCP_R_INVALIDARG;
	}

	tmp = dmalloc(sizeof(*tmp), file, line);
	if (tmp == NULL) {
		return ISC_R_NOMEMORY;
	}

	if (ia_make_key(&tmp->iaid_duid, iaid, 
			duid, duid_len, file, line) != ISC_R_SUCCESS) {
		dfree(tmp, file, line);
		return ISC_R_NOMEMORY;
	}

	tmp->refcnt = 1;

	*ia = tmp;
	return ISC_R_SUCCESS;
}

/*
 * Reference an IA structure.
 *
 * - ia must be a pointer to a (struct ia_xx *) pointer previously
 *   initialized to NULL
 */
isc_result_t
ia_reference(struct ia_xx **ia, struct ia_xx *src,
	     const char *file, int line) {
	if (ia == NULL) {
		log_error("%s(%d): NULL pointer reference", file, line);
		return DHCP_R_INVALIDARG;
	}
	if (*ia != NULL) {
		log_error("%s(%d): non-NULL pointer", file, line);
		return DHCP_R_INVALIDARG;
	}
	if (src == NULL) {
		log_error("%s(%d): NULL pointer reference", file, line);
		return DHCP_R_INVALIDARG;
	}
	*ia = src;
	src->refcnt++;
	return ISC_R_SUCCESS;
}

/*
 * Dereference an IA structure.
 *
 * If it is the last reference, then the memory for the 
 * structure is freed.
 */
isc_result_t
ia_dereference(struct ia_xx **ia, const char *file, int line) {
	struct ia_xx *tmp;
	int i;

	if ((ia == NULL) || (*ia == NULL)) {
		log_error("%s(%d): NULL pointer", file, line);
		return DHCP_R_INVALIDARG;
	}

	tmp = *ia;
	*ia = NULL;

	tmp->refcnt--;
	if (tmp->refcnt < 0) {
		log_error("%s(%d): negative refcnt", file, line);
		tmp->refcnt = 0;
	}
	if (tmp->refcnt == 0) {
		if (tmp->iasubopt != NULL) {
			for (i=0; i<tmp->num_iasubopt; i++) {
				iasubopt_dereference(&(tmp->iasubopt[i]), 
						     file, line);
			}
			dfree(tmp->iasubopt, file, line);
		}
		data_string_forget(&(tmp->iaid_duid), file, line);
		dfree(tmp, file, line);
	}
	return ISC_R_SUCCESS;
}


/*
 * Add an IAADDR/PREFIX entry to an IA structure.
 */
isc_result_t
ia_add_iasubopt(struct ia_xx *ia, struct iasubopt *iasubopt, 
		const char *file, int line) {
	int max;
	struct iasubopt **new;

	/* 
	 * Grow our array if we need to.
	 * 
	 * Note: we pick 4 as the increment, as that seems a reasonable
	 *       guess as to how many addresses/prefixes we might expect
	 *       on an interface.
	 */
	if (ia->max_iasubopt <= ia->num_iasubopt) {
		max = ia->max_iasubopt + 4;
		new = dmalloc(max * sizeof(struct iasubopt *), file, line);
		if (new == NULL) {
			return ISC_R_NOMEMORY;
		}
		memcpy(new, ia->iasubopt, 
		       ia->num_iasubopt * sizeof(struct iasubopt *));
		ia->iasubopt = new;
		ia->max_iasubopt = max;
	}

	iasubopt_reference(&(ia->iasubopt[ia->num_iasubopt]), iasubopt, 
			   file, line);
	ia->num_iasubopt++;

	return ISC_R_SUCCESS;
}

/*
 * Remove an IAADDR/PREFIX entry to an IA structure.
 *
 * Note: if a suboption appears more than once, then only ONE will be removed.
 */
void
ia_remove_iasubopt(struct ia_xx *ia, struct iasubopt *iasubopt,
		   const char *file, int line) {
	int i, j;
        if (ia == NULL || iasubopt == NULL)
            return;

	for (i=0; i<ia->num_iasubopt; i++) {
		if (ia->iasubopt[i] == iasubopt) {
			/* remove this sub option */
			iasubopt_dereference(&(ia->iasubopt[i]), file, line);
			/* move remaining suboption pointers down one */
			for (j=i+1; j < ia->num_iasubopt; j++) {
				ia->iasubopt[j-1] = ia->iasubopt[j];
			}
			/* decrease our total count */
			/* remove the back-reference in the suboption itself */
			ia_dereference(&iasubopt->ia, file, line);
			ia->num_iasubopt--;
			return;
		}
	}
	log_error("%s(%d): IAADDR/PREFIX not in IA", file, line);
}

/*
 * Remove all addresses/prefixes from an IA.
 */
void
ia_remove_all_lease(struct ia_xx *ia, const char *file, int line) {
	int i;

	for (i=0; i<ia->num_iasubopt; i++) {
		ia_dereference(&(ia->iasubopt[i]->ia), file, line);
		iasubopt_dereference(&(ia->iasubopt[i]), file, line);
	}
	ia->num_iasubopt = 0;
}

/*
 * Compare two IA.
 */
isc_boolean_t
ia_equal(const struct ia_xx *a, const struct ia_xx *b) 
{
	isc_boolean_t found;
	int i, j;

	/*
	 * Handle cases where one or both of the inputs is NULL.
	 */
	if (a == NULL) {
		if (b == NULL) {
			return ISC_TRUE;
		} else {
			return ISC_FALSE;
		}
	}	

	/*
	 * Check the type is the same.
	 */
	if (a->ia_type != b->ia_type) {
		return ISC_FALSE;
	}

	/*
	 * Check the DUID is the same.
	 */
	if (a->iaid_duid.len != b->iaid_duid.len) {
		return ISC_FALSE;
	}
	if (memcmp(a->iaid_duid.data, 
		   b->iaid_duid.data, a->iaid_duid.len) != 0) {
		return ISC_FALSE;
	}

	/*
	 * Make sure we have the same number of addresses/prefixes in each.
	 */
	if (a->num_iasubopt != b->num_iasubopt) {
		return ISC_FALSE;
	}

	/*
	 * Check that each address/prefix is present in both.
	 */
	for (i=0; i<a->num_iasubopt; i++) {
		found = ISC_FALSE;
		for (j=0; j<a->num_iasubopt; j++) {
			if (a->iasubopt[i]->plen != b->iasubopt[i]->plen)
				continue;
			if (memcmp(&(a->iasubopt[i]->addr),
			           &(b->iasubopt[j]->addr), 
				   sizeof(struct in6_addr)) == 0) {
				found = ISC_TRUE;
				break;
			}
		}
		if (!found) {
			return ISC_FALSE;
		}
	}

	/*
	 * These are the same in every way we care about.
	 */
	return ISC_TRUE;
}

/*
 * Helper function for lease heaps.
 * Makes the top of the heap the oldest lease.
 */
static isc_boolean_t 
lease_older(void *a, void *b) {
	struct iasubopt *la = (struct iasubopt *)a;
	struct iasubopt *lb = (struct iasubopt *)b;

	if (la->hard_lifetime_end_time == lb->hard_lifetime_end_time) {
		return difftime(la->soft_lifetime_end_time,
				lb->soft_lifetime_end_time) < 0;
	} else {
		return difftime(la->hard_lifetime_end_time, 
				lb->hard_lifetime_end_time) < 0;
	}
}

/*
 * Helper function for lease address/prefix heaps.
 * Callback when an address's position in the heap changes.
 */
static void
lease_index_changed(void *iasubopt, unsigned int new_heap_index) {
	((struct iasubopt *)iasubopt)-> heap_index = new_heap_index;
}


/*!
 *
 * \brief Create a new IPv6 lease pool structure
 *
 * Allocate space for a new ipv6_pool structure and return a reference
 * to it, includes setting the reference count to 1.
 *
 * \param     pool       = space for returning a referenced pointer to the pool.
 *			   This must point to a space that has been initialzied
 *			   to NULL by the caller.
 * \param[in] type       = The type of the pool NA, TA or PD
 * \param[in] start_addr = The first address in the range for the pool
 * \param[in] bits       = The contiguous bits of the pool

 * 
 * \return
 * ISC_R_SUCCESS     = The pool was successfully created, pool points to it.
 * DHCP_R_INVALIDARG = One of the arugments was invalid, pool has not been
 *		       modified
 * ISC_R_NOMEMORY    = The system wasn't able to allocate memory, pool has
 *		       not been modified.
 */
isc_result_t
ipv6_pool_allocate(struct ipv6_pool **pool, u_int16_t type,
		   const struct in6_addr *start_addr, int bits, 
		   int units, const char *file, int line) {
	struct ipv6_pool *tmp;

	if (pool == NULL) {
		log_error("%s(%d): NULL pointer reference", file, line);
		return DHCP_R_INVALIDARG;
	}
	if (*pool != NULL) {
		log_error("%s(%d): non-NULL pointer", file, line);
		return DHCP_R_INVALIDARG;
	}

	tmp = dmalloc(sizeof(*tmp), file, line);
	if (tmp == NULL) {
		return ISC_R_NOMEMORY;
	}

	tmp->refcnt = 1;
	tmp->pool_type = type;
	tmp->start_addr = *start_addr;
	tmp->bits = bits;
	tmp->units = units;
	if (!iasubopt_new_hash(&tmp->leases, DEFAULT_HASH_SIZE, file, line)) {
		dfree(tmp, file, line);
		return ISC_R_NOMEMORY;
	}
	if (isc_heap_create(dhcp_gbl_ctx.mctx, lease_older, lease_index_changed,
			    0, &(tmp->active_timeouts)) != ISC_R_SUCCESS) {
		iasubopt_free_hash_table(&(tmp->leases), file, line);
		dfree(tmp, file, line);
		return ISC_R_NOMEMORY;
	}
	if (isc_heap_create(dhcp_gbl_ctx.mctx, lease_older, lease_index_changed,
			    0, &(tmp->inactive_timeouts)) != ISC_R_SUCCESS) {
		isc_heap_destroy(&(tmp->active_timeouts));
		iasubopt_free_hash_table(&(tmp->leases), file, line);
		dfree(tmp, file, line);
		return ISC_R_NOMEMORY;
	}

	*pool = tmp;
	return ISC_R_SUCCESS;
}

/*!
 *
 * \brief reference an IPv6 pool structure.
 *
 * This function genreates a reference to an ipv6_pool structure
 * and increments the reference count on the structure.
 *
 * \param[out] pool = space for returning a referenced pointer to the pool.
 *		      This must point to a space that has been initialzied
 *		      to NULL by the caller.
 * \param[in]  src  = A pointer to the pool to reference.  This must not be
 *		      NULL.
 *
 * \return
 * ISC_R_SUCCESS     = The pool was successfully referenced, pool now points
 *		       to src.
 * DHCP_R_INVALIDARG = One of the arugments was invalid, pool has not been
 *		       modified.
 */
isc_result_t
ipv6_pool_reference(struct ipv6_pool **pool, struct ipv6_pool *src,
		    const char *file, int line) {
	if (pool == NULL) {
		log_error("%s(%d): NULL pointer reference", file, line);
		return DHCP_R_INVALIDARG;
	}
	if (*pool != NULL) {
		log_error("%s(%d): non-NULL pointer", file, line);
		return DHCP_R_INVALIDARG;
	}
	if (src == NULL) {
		log_error("%s(%d): NULL pointer reference", file, line);
		return DHCP_R_INVALIDARG;
	}
	*pool = src;
	src->refcnt++;
	return ISC_R_SUCCESS;
}

/* 
 * Note: Each IAADDR/PREFIX in a pool is referenced by the pool. This is needed
 * to prevent the lease from being garbage collected out from under the
 * pool.
 *
 * The references are made from the hash and from the heap. The following
 * helper functions dereference these when a pool is destroyed.
 */

/*
 * Helper function for pool cleanup.
 * Dereference each of the hash entries in a pool.
 */
static isc_result_t 
dereference_hash_entry(const void *name, unsigned len, void *value) {
	struct iasubopt *iasubopt = (struct iasubopt *)value;

	iasubopt_dereference(&iasubopt, MDL);
	return ISC_R_SUCCESS;
}

/*
 * Helper function for pool cleanup.
 * Dereference each of the heap entries in a pool.
 */
static void
dereference_heap_entry(void *value, void *dummy) {
	struct iasubopt *iasubopt = (struct iasubopt *)value;

	iasubopt_dereference(&iasubopt, MDL);
}

/*!
 *
 * \brief de-reference an IPv6 pool structure.
 *
 * This function decrements the reference count in an ipv6_pool structure.
 * If this was the last reference then the memory for the structure is
 * freed.
 *
 * \param[in] pool = A pointer to the pointer to the pool that should be
 *		     de-referenced.  On success the pointer to the pool
 *		     is cleared.  It must not be NULL and must not point
 *		     to NULL.
 *
 * \return
 * ISC_R_SUCCESS     = The pool was successfully de-referenced, pool now points
 *		       to NULL
 * DHCP_R_INVALIDARG = One of the arugments was invalid, pool has not been
 *		       modified.
 */
isc_result_t
ipv6_pool_dereference(struct ipv6_pool **pool, const char *file, int line) {
	struct ipv6_pool *tmp;

	if ((pool == NULL) || (*pool == NULL)) {
		log_error("%s(%d): NULL pointer", file, line);
		return DHCP_R_INVALIDARG;
	}

	tmp = *pool;
	*pool = NULL;

	tmp->refcnt--;
	if (tmp->refcnt < 0) {
		log_error("%s(%d): negative refcnt", file, line);
		tmp->refcnt = 0;
	}
	if (tmp->refcnt == 0) {
		iasubopt_hash_foreach(tmp->leases, dereference_hash_entry);
		iasubopt_free_hash_table(&(tmp->leases), file, line);
		isc_heap_foreach(tmp->active_timeouts, 
				 dereference_heap_entry, NULL);
		isc_heap_destroy(&(tmp->active_timeouts));
		isc_heap_foreach(tmp->inactive_timeouts, 
				 dereference_heap_entry, NULL);
		isc_heap_destroy(&(tmp->inactive_timeouts));
		dfree(tmp, file, line);
	}

	return ISC_R_SUCCESS;
}

/* 
 * Create an address by hashing the input, and using that for
 * the non-network part.
 */
static void
build_address6(struct in6_addr *addr, 
	       const struct in6_addr *net_start_addr, int net_bits, 
	       const struct data_string *input) {
	isc_md5_t ctx;
	int net_bytes;
	int i;
	char *str;
	const char *net_str;

	/* 
	 * Use MD5 to get a nice 128 bit hash of the input.
	 * Yes, we know MD5 isn't cryptographically sound. 
	 * No, we don't care.
	 */
	isc_md5_init(&ctx);
	isc_md5_update(&ctx, input->data, input->len);
	isc_md5_final(&ctx, (unsigned char *)addr);

	/*
	 * Copy the [0..128] network bits over.
	 */
	str = (char *)addr;
	net_str = (const char *)net_start_addr;
	net_bytes = net_bits / 8;
	for (i = 0; i < net_bytes; i++) {
		str[i] = net_str[i];
	}
	switch (net_bits % 8) {
		case 1: str[i] = (str[i] & 0x7F) | (net_str[i] & 0x80); break;
		case 2: str[i] = (str[i] & 0x3F) | (net_str[i] & 0xC0); break;
		case 3: str[i] = (str[i] & 0x1F) | (net_str[i] & 0xE0); break;
		case 4: str[i] = (str[i] & 0x0F) | (net_str[i] & 0xF0); break;
		case 5: str[i] = (str[i] & 0x07) | (net_str[i] & 0xF8); break;
		case 6: str[i] = (str[i] & 0x03) | (net_str[i] & 0xFC); break;
		case 7: str[i] = (str[i] & 0x01) | (net_str[i] & 0xFE); break;
	}

	/*
	 * Set the universal/local bit ("u bit") to zero for /64s.  The
	 * individual/group bit ("g bit") is unchanged, because the g-bit
	 * has no meaning when the u-bit is cleared.
	 */
	if (net_bits == 64)
		str[8] &= ~0x02;
}

/* 
 * Create a temporary address by a variant of RFC 4941 algo.
 * Note: this should not be used for prefixes shorter than 64 bits.
 */
static void
build_temporary6(struct in6_addr *addr, 
		 const struct in6_addr *net_start_addr, int net_bits,
		 const struct data_string *input) {
	static u_int32_t history[2];
	static u_int32_t counter = 0;
	isc_md5_t ctx;
	unsigned char md[16];

	/*
	 * First time/time to reseed.
	 * Please use a good pseudo-random generator here!
	 */
	if (counter == 0) {
		isc_random_get(&history[0]);
		isc_random_get(&history[1]);
	}

	/* 
	 * Use MD5 as recommended by RFC 4941.
	 */
	isc_md5_init(&ctx);
	isc_md5_update(&ctx, (unsigned char *)&history[0], 8UL);
	isc_md5_update(&ctx, input->data, input->len);
	isc_md5_final(&ctx, md);

	/*
	 * Build the address.
	 */
	if (net_bits == 64) {
		memcpy(&addr->s6_addr[0], &net_start_addr->s6_addr[0], 8);
		memcpy(&addr->s6_addr[8], md, 8);
		addr->s6_addr[8] &= ~0x02;
	} else {
		int net_bytes;
		int i;
		char *str;
		const char *net_str;

		/*
		 * Copy the [0..128] network bits over.
		 */
		str = (char *)addr;
		net_str = (const char *)net_start_addr;
		net_bytes = net_bits / 8;
		for (i = 0; i < net_bytes; i++) {
			str[i] = net_str[i];
		}
		memcpy(str + net_bytes, md, 16 - net_bytes);
		switch (net_bits % 8) {
		case 1: str[i] = (str[i] & 0x7F) | (net_str[i] & 0x80); break;
		case 2: str[i] = (str[i] & 0x3F) | (net_str[i] & 0xC0); break;
		case 3: str[i] = (str[i] & 0x1F) | (net_str[i] & 0xE0); break;
		case 4: str[i] = (str[i] & 0x0F) | (net_str[i] & 0xF0); break;
		case 5: str[i] = (str[i] & 0x07) | (net_str[i] & 0xF8); break;
		case 6: str[i] = (str[i] & 0x03) | (net_str[i] & 0xFC); break;
		case 7: str[i] = (str[i] & 0x01) | (net_str[i] & 0xFE); break;
		}
	}


	/*
	 * Save history for the next call.
	 */
	memcpy((unsigned char *)&history[0], md + 8, 8);
	counter++;
}

/* Reserved Subnet Router Anycast ::0:0:0:0. */
static struct in6_addr rtany;
/* Reserved Subnet Anycasts ::fdff:ffff:ffff:ff80-::fdff:ffff:ffff:ffff. */
static struct in6_addr resany;

/*
 * Create a lease for the given address and client duid.
 *
 * - pool must be a pointer to a (struct ipv6_pool *) pointer previously
 *   initialized to NULL
 *
 * Right now we simply hash the DUID, and if we get a collision, we hash 
 * again until we find a free address. We try this a fixed number of times,
 * to avoid getting stuck in a loop (this is important on small pools
 * where we can run out of space).
 *
 * We return the number of attempts that it took to find an available
 * lease. This tells callers when a pool is are filling up, as
 * well as an indication of how full the pool is; statistically the 
 * more full a pool is the more attempts must be made before finding
 * a free lease. Realistically this will only happen in very full
 * pools.
 *
 * We probably want different algorithms depending on the network size, in
 * the long term.
 */
isc_result_t
create_lease6(struct ipv6_pool *pool, struct iasubopt **addr, 
	      unsigned int *attempts,
	      const struct data_string *uid, time_t soft_lifetime_end_time) {
	struct data_string ds;
	struct in6_addr tmp;
	struct iasubopt *test_iaaddr;
	struct data_string new_ds;
	struct iasubopt *iaaddr;
	isc_result_t result;
	isc_boolean_t reserved_iid;
	static isc_boolean_t init_resiid = ISC_FALSE;

	/*
	 * Fill the reserved IIDs.
	 */
	if (!init_resiid) {
		memset(&rtany, 0, 16);
		memset(&resany, 0, 8);
		resany.s6_addr[8] = 0xfd;
		memset(&resany.s6_addr[9], 0xff, 6);
		init_resiid = ISC_TRUE;
	}

	/* 
	 * Use the UID as our initial seed for the hash
	 */
	memset(&ds, 0, sizeof(ds));
	data_string_copy(&ds, (struct data_string *)uid, MDL);

	*attempts = 0;
	for (;;) {
		/*
		 * Give up at some point.
		 */
		if (++(*attempts) > 100) {
			data_string_forget(&ds, MDL);
			return ISC_R_NORESOURCES;
		}

		/* 
		 * Build a resource.
		 */
		switch (pool->pool_type) {
		case D6O_IA_NA:
			/* address */
			build_address6(&tmp, &pool->start_addr,
				       pool->bits, &ds);
			break;
		case D6O_IA_TA:
			/* temporary address */
			build_temporary6(&tmp, &pool->start_addr,
					 pool->bits, &ds);
			break;
		case D6O_IA_PD:
			/* prefix */
			log_error("create_lease6: prefix pool.");
			return DHCP_R_INVALIDARG;
		default:
			log_error("create_lease6: untyped pool.");
			return DHCP_R_INVALIDARG;
		}

		/*
		 * Avoid reserved interface IDs. (cf. RFC 5453)
		 */
		reserved_iid = ISC_FALSE;
		if (memcmp(&tmp.s6_addr[8], &rtany.s6_addr[8], 8) == 0) {
			reserved_iid = ISC_TRUE;
		}
		if (!reserved_iid &&
		    (memcmp(&tmp.s6_addr[8], &resany.s6_addr[8], 7) == 0) &&
		    ((tmp.s6_addr[15] & 0x80) == 0x80)) {
			reserved_iid = ISC_TRUE;
		}

		/*
		 * If this address is not in use, we're happy with it
		 */
		test_iaaddr = NULL;
		if (!reserved_iid &&
		    (iasubopt_hash_lookup(&test_iaaddr, pool->leases,
					  &tmp, sizeof(tmp), MDL) == 0)) {
			break;
		}
		if (test_iaaddr != NULL)
			iasubopt_dereference(&test_iaaddr, MDL);

		/* 
		 * Otherwise, we create a new input, adding the address
		 */
		memset(&new_ds, 0, sizeof(new_ds));
		new_ds.len = ds.len + sizeof(tmp);
		if (!buffer_allocate(&new_ds.buffer, new_ds.len, MDL)) {
			data_string_forget(&ds, MDL);
			return ISC_R_NOMEMORY;
		}
		new_ds.data = new_ds.buffer->data;
		memcpy(new_ds.buffer->data, ds.data, ds.len);
		memcpy(new_ds.buffer->data + ds.len, &tmp, sizeof(tmp));
		data_string_forget(&ds, MDL);
		data_string_copy(&ds, &new_ds, MDL);
		data_string_forget(&new_ds, MDL);
	}

	data_string_forget(&ds, MDL);

	/* 
	 * We're happy with the address, create an IAADDR
	 * to hold it.
	 */
	iaaddr = NULL;
	result = iasubopt_allocate(&iaaddr, MDL);
	if (result != ISC_R_SUCCESS) {
		return result;
	}
	iaaddr->plen = 0;
	memcpy(&iaaddr->addr, &tmp, sizeof(iaaddr->addr));

	/*
	 * Add the lease to the pool (note state is free, not active?!).
	 */
	result = add_lease6(pool, iaaddr, soft_lifetime_end_time);
	if (result == ISC_R_SUCCESS) {
		iasubopt_reference(addr, iaaddr, MDL);
	}
	iasubopt_dereference(&iaaddr, MDL);
	return result;
}


/*!
 *
 * \brief Cleans up leases when reading from a lease file
 *
 * This function is only expected to be run when reading leases in from a file.
 * It checks to see if a lease already exists for the new leases's address.
 * We don't add expired leases to the structures when reading a lease file
 * which limits what can happen.  We have two variables the owners of the leases
 * being the same or different and the new lease being active or non-active:
 * Owners active
 * same   no     remove old lease and its connections
 * same   yes    nothing to do, other code will update the structures.
 * diff   no     nothing to do
 * diff   yes    this combination shouldn't happen, we should only have a
 *               single active lease per address at a time and that lease
 *               should move to non-active before any other lease can
 *               become active for that address.
 *               Currently we delete the previous lease and pass an error
 *               to the caller who should log an error.
 *
 * When we remove a lease we remove it from the hash table and active heap
 * (remember only active leases are in the structures at this time) for the
 * pool, and from the IA's array.  If, after we've removed the pointer from
 * IA's array to the lease, the IA has no more pointers we remove it from
 * the appropriate hash table as well.
 *
 * \param[in] ia_table = the hash table for the IA
 * \param[in] pool     = the pool to update
 * \param[in] lease    = the new lease we want to add
 * \param[in] ia       = the new ia we are building
 *
 * \return
 * ISC_R_SUCCESS = the incoming lease and any previous lease were in
 *                 an expected state - one of the first 3 options above.
 *                 If necessary the old lease was removed.
 * ISC_R_FAILURE = there is already an active lease for the address in
 *                 the incoming lease.  This shouldn't happen if it does
 *                 flag an error for the caller to log.
 */

isc_result_t
cleanup_lease6(ia_hash_t *ia_table,
	       struct ipv6_pool *pool,
	       struct iasubopt *lease,
	       struct ia_xx *ia) {

	struct iasubopt *test_iasubopt, *tmp_iasubopt;
	struct ia_xx *old_ia;
	isc_result_t status = ISC_R_SUCCESS;

	test_iasubopt = NULL;
	old_ia = NULL;

	/*
	 * Look up the address - if we don't find a lease
	 * we don't need to do anything.
	 */
	if (iasubopt_hash_lookup(&test_iasubopt, pool->leases,
				 &lease->addr, sizeof(lease->addr),
				 MDL) == 0) {
		return (ISC_R_SUCCESS);
	}

	if (test_iasubopt->ia == NULL) {
		/* no old ia, no work to do */
		iasubopt_dereference(&test_iasubopt, MDL);
		return (status);
	}

	ia_reference(&old_ia, test_iasubopt->ia, MDL);

	if ((old_ia->iaid_duid.len == ia->iaid_duid.len) &&
	    (memcmp((unsigned char *)ia->iaid_duid.data,
		    (unsigned char *)old_ia->iaid_duid.data,
		    ia->iaid_duid.len) == 0)) {
		/* same IA */
		if ((lease->state == FTS_ACTIVE) ||
		    (lease->state == FTS_ABANDONED)) {
			/* still active, no need to delete */
			goto cleanup;
		}
	} else {
		/* different IA */
		if ((lease->state != FTS_ACTIVE) &&
		    (lease->state != FTS_ABANDONED)) {
			/* new lease isn't active, no work */
			goto cleanup;
		}

		/*
		 * We appear to have two active leases, this shouldn't happen.
		 * Before a second lease can be set to active the first lease
		 * should be set to inactive (released, expired etc). For now
		 * delete the previous lease and indicate a failure to the
		 * caller so it can generate a warning.
		 * In the future we may try and determine which is the better
		 * lease to keep.
		 */

		status = ISC_R_FAILURE;
	}

	/*
	 * Remove the old lease from the active heap and from the hash table
	 * then remove the lease from the IA and clean up the IA if necessary.
	 */
	isc_heap_delete(pool->active_timeouts, test_iasubopt->heap_index);
	pool->num_active--;

	iasubopt_hash_delete(pool->leases, &test_iasubopt->addr,
			     sizeof(test_iasubopt->addr), MDL);
	ia_remove_iasubopt(old_ia, test_iasubopt, MDL);
	if (old_ia->num_iasubopt <= 0) {
		ia_hash_delete(ia_table,
			       (unsigned char *)old_ia->iaid_duid.data,
			       old_ia->iaid_duid.len, MDL);
	}

	/*
	 * We derefenrece the subopt here as we've just removed it from
	 * the hash table in the pool.  We need to make a copy as we
	 * need to derefernece it again later.
	 */
	tmp_iasubopt = test_iasubopt;
	iasubopt_dereference(&tmp_iasubopt, MDL);

      cleanup:
	ia_dereference(&old_ia, MDL);

	/*
	 * Clean up the reference, this is in addition to the deference
	 * above after removing the entry from the hash table
	 */
	iasubopt_dereference(&test_iasubopt, MDL);

	return (status);
}

/*
 * Put a lease in the pool directly. This is intended to be used when
 * loading leases from the file.
 */
isc_result_t
add_lease6(struct ipv6_pool *pool, struct iasubopt *lease,
	   time_t valid_lifetime_end_time) {
	isc_result_t insert_result;
	struct iasubopt *test_iasubopt;
	struct iasubopt *tmp_iasubopt;

	/* If a state was not assigned by the caller, assume active. */
	if (lease->state == 0)
		lease->state = FTS_ACTIVE;

	ipv6_pool_reference(&lease->ipv6_pool, pool, MDL);

	/*
	 * If this IAADDR/PREFIX is already in our structures, remove the 
	 * old one.
	 */
	test_iasubopt = NULL;
	if (iasubopt_hash_lookup(&test_iasubopt, pool->leases,
				 &lease->addr, sizeof(lease->addr), MDL)) {
		/* XXX: we should probably ask the lease what heap it is on
		 * (as a consistency check).
		 * XXX: we should probably have one function to "put this lease
		 * on its heap" rather than doing these if's everywhere.  If
		 * you add more states to this list, don't.
		 */
		if ((test_iasubopt->state == FTS_ACTIVE) ||
		    (test_iasubopt->state == FTS_ABANDONED)) {
			isc_heap_delete(pool->active_timeouts,
					test_iasubopt->heap_index);
			pool->num_active--;
		} else {
			isc_heap_delete(pool->inactive_timeouts,
					test_iasubopt->heap_index);
			pool->num_inactive--;
		}

		iasubopt_hash_delete(pool->leases, &test_iasubopt->addr, 
				     sizeof(test_iasubopt->addr), MDL);

		/*
		 * We're going to do a bit of evil trickery here.
		 *
		 * We need to dereference the entry once to remove our
		 * current reference (in test_iasubopt), and then one
		 * more time to remove the reference left when the
		 * address was added to the pool before.
		 */
		tmp_iasubopt = test_iasubopt;
		iasubopt_dereference(&test_iasubopt, MDL);
		iasubopt_dereference(&tmp_iasubopt, MDL);
	}

	/* 
	 * Add IAADDR/PREFIX to our structures.
	 */
	tmp_iasubopt = NULL;
	iasubopt_reference(&tmp_iasubopt, lease, MDL);
	if ((tmp_iasubopt->state == FTS_ACTIVE) ||
	    (tmp_iasubopt->state == FTS_ABANDONED)) {
		tmp_iasubopt->hard_lifetime_end_time = valid_lifetime_end_time;
		iasubopt_hash_add(pool->leases, &tmp_iasubopt->addr, 
				  sizeof(tmp_iasubopt->addr), lease, MDL);
		insert_result = isc_heap_insert(pool->active_timeouts,
						tmp_iasubopt);
		if (insert_result == ISC_R_SUCCESS)
			pool->num_active++;
	} else {
		tmp_iasubopt->soft_lifetime_end_time = valid_lifetime_end_time;
		insert_result = isc_heap_insert(pool->inactive_timeouts,
						tmp_iasubopt);
		if (insert_result == ISC_R_SUCCESS)
			pool->num_inactive++;
	}
	if (insert_result != ISC_R_SUCCESS) {
		iasubopt_hash_delete(pool->leases, &lease->addr, 
				     sizeof(lease->addr), MDL);
		iasubopt_dereference(&tmp_iasubopt, MDL);
		return insert_result;
	}

	/* 
	 * Note: we intentionally leave tmp_iasubopt referenced; there
	 * is a reference in the heap/hash, after all.
	 */

	return ISC_R_SUCCESS;
}

/*
 * Determine if an address is present in a pool or not.
 */
isc_boolean_t
lease6_exists(const struct ipv6_pool *pool, const struct in6_addr *addr) {
	struct iasubopt *test_iaaddr;

	test_iaaddr = NULL;
	if (iasubopt_hash_lookup(&test_iaaddr, pool->leases, 
				 (void *)addr, sizeof(*addr), MDL)) {
		iasubopt_dereference(&test_iaaddr, MDL);
		return ISC_TRUE;
	} else {
		return ISC_FALSE;
	}
}

/*!
 *
 * \brief Check if address is available to a lease
 *
 * Determine if the address in the lease is available to that
 * lease.  Either the address isn't in use or it is in use
 * but by that lease.
 *
 * \param[in] lease = lease to check
 *
 * \return
 * ISC_TRUE  = The lease is allowed to use that address
 * ISC_FALSE = The lease isn't allowed to use that address
 */
isc_boolean_t
lease6_usable(struct iasubopt *lease) {
	struct iasubopt *test_iaaddr;
	isc_boolean_t status = ISC_TRUE;

	test_iaaddr = NULL;
	if (iasubopt_hash_lookup(&test_iaaddr, lease->ipv6_pool->leases,
				 (void *)&lease->addr,
				 sizeof(lease->addr), MDL)) {
		if (test_iaaddr != lease) {
			status = ISC_FALSE;
		}
		iasubopt_dereference(&test_iaaddr, MDL);
	}

	return (status);
}

/*
 * Put the lease on our active pool.
 */
static isc_result_t
move_lease_to_active(struct ipv6_pool *pool, struct iasubopt *lease) {
	isc_result_t insert_result;
	int old_heap_index;

	old_heap_index = lease->heap_index;
	insert_result = isc_heap_insert(pool->active_timeouts, lease);
	if (insert_result == ISC_R_SUCCESS) {
       		iasubopt_hash_add(pool->leases, &lease->addr, 
				  sizeof(lease->addr), lease, MDL);
		isc_heap_delete(pool->inactive_timeouts, old_heap_index);
		pool->num_active++;
		pool->num_inactive--;
		lease->state = FTS_ACTIVE;
	}
	return insert_result;
}

/*!
 *
 * \brief Renew a lease in the pool.
 *
 * The hard_lifetime_end_time of the lease should be set to
 * the current expiration time.
 * The soft_lifetime_end_time of the lease should be set to
 * the desired expiration time.
 *
 * This routine will compare the two and call the correct
 * heap routine to move the lease.  If the lease is active
 * and the new expiration time is greater (the normal case)
 * then we call isc_heap_decreased() as a larger time is a
 * lower priority.  If the new expiration time is less then
 * we call isc_heap_increased().
 *
 * If the lease is abandoned then it will be on the active list
 * and we will always call isc_heap_increased() as the previous
 * expiration would have been all 1s (as close as we can get
 * to infinite).
 *
 * If the lease is moving to active we call that routine
 * which will move it from the inactive list to the active list.
 *
 * \param pool  = a pool the lease belongs to
 * \param lease = the lease to be renewed
 *
 * \return result of the renew operation (ISC_R_SUCCESS if successful,
           ISC_R_NOMEMORY when run out of memory)
 */
isc_result_t
renew_lease6(struct ipv6_pool *pool, struct iasubopt *lease) {
	time_t old_end_time = lease->hard_lifetime_end_time;
	lease->hard_lifetime_end_time = lease->soft_lifetime_end_time;
	lease->soft_lifetime_end_time = 0;

	if (lease->state == FTS_ACTIVE) {
		if (old_end_time <= lease->hard_lifetime_end_time) {
			isc_heap_decreased(pool->active_timeouts,
					   lease->heap_index);
		} else {
			isc_heap_increased(pool->active_timeouts,
					   lease->heap_index);
		}
		return ISC_R_SUCCESS;
	} else if (lease->state == FTS_ABANDONED) {
		char tmp_addr[INET6_ADDRSTRLEN];
                lease->state = FTS_ACTIVE;
                isc_heap_increased(pool->active_timeouts, lease->heap_index);
		log_info("Reclaiming previously abandoned address %s",
			 inet_ntop(AF_INET6, &(lease->addr), tmp_addr,
				   sizeof(tmp_addr)));
                return ISC_R_SUCCESS;
	} else {
		return move_lease_to_active(pool, lease);
	}
}

/*
 * Put the lease on our inactive pool, with the specified state.
 */
static isc_result_t
move_lease_to_inactive(struct ipv6_pool *pool, struct iasubopt *lease, 
		       binding_state_t state) {
	isc_result_t insert_result;
	int old_heap_index;

	old_heap_index = lease->heap_index;
	insert_result = isc_heap_insert(pool->inactive_timeouts, lease);
	if (insert_result == ISC_R_SUCCESS) {
		/*
		 * Handle expire and release statements
		 * To get here we must be active and have done a commit so
		 * we should run the proper statements if they exist, though
		 * that will change when we remove the inactive heap.
		 * In addition we get rid of the references for both as we
		 * can only do one (expire or release) on a lease
		 */
		if (lease->on_star.on_expiry != NULL) {
			if (state == FTS_EXPIRED) {
				execute_statements(NULL, NULL, NULL,
						   NULL, NULL, NULL,
						   &lease->scope,
						   lease->on_star.on_expiry,
						   &lease->on_star);
			}
			executable_statement_dereference
				(&lease->on_star.on_expiry, MDL);
		}

		if (lease->on_star.on_release != NULL) {
			if (state == FTS_RELEASED) {
				execute_statements(NULL, NULL, NULL,
						   NULL, NULL, NULL,
						   &lease->scope,
						   lease->on_star.on_release,
						   &lease->on_star);
			}
			executable_statement_dereference
				(&lease->on_star.on_release, MDL);
		}

#if defined (NSUPDATE)
		/* Process events upon expiration. */
		if (pool->pool_type != D6O_IA_PD) {
			(void) ddns_removals(NULL, lease, NULL, ISC_FALSE);
		}
#endif

		/* Binding scopes are no longer valid after expiry or
		 * release.
		 */
		if (lease->scope != NULL) {
			binding_scope_dereference(&lease->scope, MDL);
		}

		iasubopt_hash_delete(pool->leases, 
				     &lease->addr, sizeof(lease->addr), MDL);
		isc_heap_delete(pool->active_timeouts, old_heap_index);
		lease->state = state;
		pool->num_active--;
		pool->num_inactive++;
	}
	return insert_result;
}

/*
 * Expire the oldest lease if it's lifetime_end_time is 
 * older than the given time.
 *
 * - leasep must be a pointer to a (struct iasubopt *) pointer previously
 *   initialized to NULL
 *
 * On return leasep has a reference to the removed entry. It is left
 * pointing to NULL if the oldest lease has not expired.
 */
isc_result_t
expire_lease6(struct iasubopt **leasep, struct ipv6_pool *pool, time_t now) {
	struct iasubopt *tmp;
	isc_result_t result;

	if (leasep == NULL) {
		log_error("%s(%d): NULL pointer reference", MDL);
		return DHCP_R_INVALIDARG;
	}
	if (*leasep != NULL) {
		log_error("%s(%d): non-NULL pointer", MDL);
		return DHCP_R_INVALIDARG;
	}

	if (pool->num_active > 0) {
		tmp = (struct iasubopt *)
				isc_heap_element(pool->active_timeouts, 1);
		if (now > tmp->hard_lifetime_end_time) {
			result = move_lease_to_inactive(pool, tmp,
							FTS_EXPIRED);
			if (result == ISC_R_SUCCESS) {
				iasubopt_reference(leasep, tmp, MDL);
			}
			return result;
		}
	}
	return ISC_R_SUCCESS;
}


/*
 * For a declined lease, leave it on the "active" pool, but mark
 * it as declined. Give it an infinite (well, really long) life.
 */
isc_result_t
decline_lease6(struct ipv6_pool *pool, struct iasubopt *lease) {
	isc_result_t result;

	if ((lease->state != FTS_ACTIVE) &&
	    (lease->state != FTS_ABANDONED)) {
		result = move_lease_to_active(pool, lease);
		if (result != ISC_R_SUCCESS) {
			return result;
		}
	}
	lease->state = FTS_ABANDONED;
	lease->hard_lifetime_end_time = MAX_TIME;
	isc_heap_decreased(pool->active_timeouts, lease->heap_index);
	return ISC_R_SUCCESS;
}

/*
 * Put the returned lease on our inactive pool.
 */
isc_result_t
release_lease6(struct ipv6_pool *pool, struct iasubopt *lease) {
	if (lease->state == FTS_ACTIVE) {
		return move_lease_to_inactive(pool, lease, FTS_RELEASED);
	} else {
		return ISC_R_SUCCESS;
	}
}

/* 
 * Create a prefix by hashing the input, and using that for
 * the part subject to allocation.
 */
static void
build_prefix6(struct in6_addr *pref, 
	      const struct in6_addr *net_start_pref,
	      int pool_bits, int pref_bits,
	      const struct data_string *input) {
	isc_md5_t ctx;
	int net_bytes;
	int i;
	char *str;
	const char *net_str;

	/* 
	 * Use MD5 to get a nice 128 bit hash of the input.
	 * Yes, we know MD5 isn't cryptographically sound. 
	 * No, we don't care.
	 */
	isc_md5_init(&ctx);
	isc_md5_update(&ctx, input->data, input->len);
	isc_md5_final(&ctx, (unsigned char *)pref);

	/*
	 * Copy the network bits over.
	 */
	str = (char *)pref;
	net_str = (const char *)net_start_pref;
	net_bytes = pool_bits / 8;
	for (i = 0; i < net_bytes; i++) {
		str[i] = net_str[i];
	}
	i = net_bytes;
	switch (pool_bits % 8) {
		case 1: str[i] = (str[i] & 0x7F) | (net_str[i] & 0x80); break;
		case 2: str[i] = (str[i] & 0x3F) | (net_str[i] & 0xC0); break;
		case 3: str[i] = (str[i] & 0x1F) | (net_str[i] & 0xE0); break;
		case 4: str[i] = (str[i] & 0x0F) | (net_str[i] & 0xF0); break;
		case 5: str[i] = (str[i] & 0x07) | (net_str[i] & 0xF8); break;
		case 6: str[i] = (str[i] & 0x03) | (net_str[i] & 0xFC); break;
		case 7: str[i] = (str[i] & 0x01) | (net_str[i] & 0xFE); break;
	}
	/*
	 * Zero the remaining bits.
	 */
	net_bytes = pref_bits / 8;
	for (i=net_bytes+1; i<16; i++) {
		str[i] = 0;
	}
	i = net_bytes;
	switch (pref_bits % 8) {
		case 0: str[i] &= 0; break;
		case 1: str[i] &= 0x80; break;
		case 2: str[i] &= 0xC0; break;
		case 3: str[i] &= 0xE0; break;
		case 4: str[i] &= 0xF0; break;
		case 5: str[i] &= 0xF8; break;
		case 6: str[i] &= 0xFC; break;
		case 7: str[i] &= 0xFE; break;
	}
}

/*
 * Create a lease for the given prefix and client duid.
 *
 * - pool must be a pointer to a (struct ipv6_pool *) pointer previously
 *   initialized to NULL
 *
 * Right now we simply hash the DUID, and if we get a collision, we hash 
 * again until we find a free prefix. We try this a fixed number of times,
 * to avoid getting stuck in a loop (this is important on small pools
 * where we can run out of space).
 *
 * We return the number of attempts that it took to find an available
 * prefix. This tells callers when a pool is are filling up, as
 * well as an indication of how full the pool is; statistically the 
 * more full a pool is the more attempts must be made before finding
 * a free prefix. Realistically this will only happen in very full
 * pools.
 *
 * We probably want different algorithms depending on the network size, in
 * the long term.
 */
isc_result_t
create_prefix6(struct ipv6_pool *pool, struct iasubopt **pref, 
	       unsigned int *attempts,
	       const struct data_string *uid,
	       time_t soft_lifetime_end_time) {
	struct data_string ds;
	struct in6_addr tmp;
	struct iasubopt *test_iapref;
	struct data_string new_ds;
	struct iasubopt *iapref;
	isc_result_t result;

	/* 
	 * Use the UID as our initial seed for the hash
	 */
	memset(&ds, 0, sizeof(ds));
	data_string_copy(&ds, (struct data_string *)uid, MDL);

	*attempts = 0;
	for (;;) {
		/*
		 * Give up at some point.
		 */
		if (++(*attempts) > 10) {
			data_string_forget(&ds, MDL);
			return ISC_R_NORESOURCES;
		}

		/* 
		 * Build a prefix
		 */
		build_prefix6(&tmp, &pool->start_addr,
			      pool->bits, pool->units, &ds);

		/*
		 * If this prefix is not in use, we're happy with it
		 */
		test_iapref = NULL;
		if (iasubopt_hash_lookup(&test_iapref, pool->leases,
					 &tmp, sizeof(tmp), MDL) == 0) {
			break;
		}
		iasubopt_dereference(&test_iapref, MDL);

		/* 
		 * Otherwise, we create a new input, adding the prefix
		 */
		memset(&new_ds, 0, sizeof(new_ds));
		new_ds.len = ds.len + sizeof(tmp);
		if (!buffer_allocate(&new_ds.buffer, new_ds.len, MDL)) {
			data_string_forget(&ds, MDL);
			return ISC_R_NOMEMORY;
		}
		new_ds.data = new_ds.buffer->data;
		memcpy(new_ds.buffer->data, ds.data, ds.len);
		memcpy(new_ds.buffer->data + ds.len, &tmp, sizeof(tmp));
		data_string_forget(&ds, MDL);
		data_string_copy(&ds, &new_ds, MDL);
		data_string_forget(&new_ds, MDL);
	}

	data_string_forget(&ds, MDL);

	/* 
	 * We're happy with the prefix, create an IAPREFIX
	 * to hold it.
	 */
	iapref = NULL;
	result = iasubopt_allocate(&iapref, MDL);
	if (result != ISC_R_SUCCESS) {
		return result;
	}
	iapref->plen = (u_int8_t)pool->units;
	memcpy(&iapref->addr, &tmp, sizeof(iapref->addr));

	/*
	 * Add the prefix to the pool (note state is free, not active?!).
	 */
	result = add_lease6(pool, iapref, soft_lifetime_end_time);
	if (result == ISC_R_SUCCESS) {
		iasubopt_reference(pref, iapref, MDL);
	}
	iasubopt_dereference(&iapref, MDL);
	return result;
}

/*
 * Determine if a prefix is present in a pool or not.
 */
isc_boolean_t
prefix6_exists(const struct ipv6_pool *pool,
	       const struct in6_addr *pref, u_int8_t plen) {
	struct iasubopt *test_iapref;

	if ((int)plen != pool->units)
		return ISC_FALSE;

	test_iapref = NULL;
	if (iasubopt_hash_lookup(&test_iapref, pool->leases, 
				 (void *)pref, sizeof(*pref), MDL)) {
		iasubopt_dereference(&test_iapref, MDL);
		return ISC_TRUE;
	} else {
		return ISC_FALSE;
	}
}

/*
 * Mark an IPv6 address/prefix as unavailable from a pool.
 *
 * This is used for host entries and the addresses of the server itself.
 */
static isc_result_t
mark_lease_unavailable(struct ipv6_pool *pool, const struct in6_addr *addr) {
	struct iasubopt *dummy_iasubopt;
	isc_result_t result;

	dummy_iasubopt = NULL;
	result = iasubopt_allocate(&dummy_iasubopt, MDL);
	if (result == ISC_R_SUCCESS) {
		dummy_iasubopt->addr = *addr;
		iasubopt_hash_add(pool->leases, &dummy_iasubopt->addr,
				  sizeof(*addr), dummy_iasubopt, MDL);
	}
	return result;
}

/* 
 * Add a pool.
 */
isc_result_t
add_ipv6_pool(struct ipv6_pool *pool) {
	struct ipv6_pool **new_pools;

	new_pools = dmalloc(sizeof(struct ipv6_pool *) * (num_pools+1), MDL);
	if (new_pools == NULL) {
		return ISC_R_NOMEMORY;
	}

	if (num_pools > 0) {
		memcpy(new_pools, pools, 
		       sizeof(struct ipv6_pool *) * num_pools);
		dfree(pools, MDL);
	}
	pools = new_pools;

	pools[num_pools] = NULL;
	ipv6_pool_reference(&pools[num_pools], pool, MDL);
	num_pools++;
	return ISC_R_SUCCESS;
}

static void
cleanup_old_expired(struct ipv6_pool *pool) {
	struct iasubopt *tmp;
	struct ia_xx *ia;
	struct ia_xx *ia_active;
	unsigned char *tmpd;
	time_t timeout;
	
	while (pool->num_inactive > 0) {
		tmp = (struct iasubopt *)
				isc_heap_element(pool->inactive_timeouts, 1);
		if (tmp->hard_lifetime_end_time != 0) {
			timeout = tmp->hard_lifetime_end_time;
			timeout += EXPIRED_IPV6_CLEANUP_TIME;
		} else {
			timeout = tmp->soft_lifetime_end_time;
		}
		if (cur_time < timeout) {
			break;
		}

		isc_heap_delete(pool->inactive_timeouts, tmp->heap_index);
		pool->num_inactive--;

		if (tmp->ia != NULL) {
			/*
			 * Check to see if this IA is in an active list,
			 * but has no remaining resources. If so, remove it
			 * from the active list.
			 */
			ia = NULL;
			ia_reference(&ia, tmp->ia, MDL);
			ia_remove_iasubopt(ia, tmp, MDL);
			ia_active = NULL;
			tmpd = (unsigned char *)ia->iaid_duid.data;
			if ((ia->ia_type == D6O_IA_NA) &&
			    (ia->num_iasubopt <= 0) &&
			    (ia_hash_lookup(&ia_active, ia_na_active, tmpd,
					    ia->iaid_duid.len, MDL) == 0) &&
			    (ia_active == ia)) {
				ia_hash_delete(ia_na_active, tmpd, 
					       ia->iaid_duid.len, MDL);
			}
			if ((ia->ia_type == D6O_IA_TA) &&
			    (ia->num_iasubopt <= 0) &&
			    (ia_hash_lookup(&ia_active, ia_ta_active, tmpd,
					    ia->iaid_duid.len, MDL) == 0) &&
			    (ia_active == ia)) {
				ia_hash_delete(ia_ta_active, tmpd, 
					       ia->iaid_duid.len, MDL);
			}
			if ((ia->ia_type == D6O_IA_PD) &&
			    (ia->num_iasubopt <= 0) &&
			    (ia_hash_lookup(&ia_active, ia_pd_active, tmpd,
					    ia->iaid_duid.len, MDL) == 0) &&
			    (ia_active == ia)) {
				ia_hash_delete(ia_pd_active, tmpd, 
					       ia->iaid_duid.len, MDL);
			}
			ia_dereference(&ia, MDL);
		}
		iasubopt_dereference(&tmp, MDL);
	}
}

static void
lease_timeout_support(void *vpool) {
	struct ipv6_pool *pool;
	struct iasubopt *lease;
	
	pool = (struct ipv6_pool *)vpool;
	for (;;) {
		/*
		 * Get the next lease scheduled to expire.
		 *
		 * Note that if there are no leases in the pool, 
		 * expire_lease6() will return ISC_R_SUCCESS with 
		 * a NULL lease.
		 *
		 * expire_lease6() will call move_lease_to_inactive() which
		 * calls ddns_removals() do we want that on the standard
		 * expiration timer or a special 'depref' timer?  Original
		 * query from DH, moved here by SAR.
		 */
		lease = NULL;
		if (expire_lease6(&lease, pool, cur_time) != ISC_R_SUCCESS) {
			break;
		}
		if (lease == NULL) {
			break;
		}

		write_ia(lease->ia);

		iasubopt_dereference(&lease, MDL);
	}

	/*
	 * If appropriate commit and rotate the lease file
	 * As commit_leases_timed() checks to see if we've done any writes
	 * we don't bother tracking if this function called write _ia
	 */
	(void) commit_leases_timed();

	/*
	 * Do some cleanup of our expired leases.
	 */
	cleanup_old_expired(pool);

	/*
	 * Schedule next round of expirations.
	 */
	schedule_lease_timeout(pool);
}

/*
 * For a given pool, add a timer that will remove the next
 * lease to expire.
 */
void 
schedule_lease_timeout(struct ipv6_pool *pool) {
	struct iasubopt *tmp;
	time_t timeout;
	time_t next_timeout;
	struct timeval tv;

	next_timeout = MAX_TIME;

	if (pool->num_active > 0) {
		tmp = (struct iasubopt *)
				isc_heap_element(pool->active_timeouts, 1);
		if (tmp->hard_lifetime_end_time < next_timeout) {
			next_timeout = tmp->hard_lifetime_end_time + 1;
		}
	}

	if (pool->num_inactive > 0) {
		tmp = (struct iasubopt *)
				isc_heap_element(pool->inactive_timeouts, 1);
		if (tmp->hard_lifetime_end_time != 0) {
			timeout = tmp->hard_lifetime_end_time;
			timeout += EXPIRED_IPV6_CLEANUP_TIME;
		} else {
			timeout = tmp->soft_lifetime_end_time + 1;
		}
		if (timeout < next_timeout) {
			next_timeout = timeout;
		}
	}

	if (next_timeout < MAX_TIME) {
		tv.tv_sec = next_timeout;
		tv.tv_usec = 0;
		add_timeout(&tv, lease_timeout_support, pool,
			    (tvref_t)ipv6_pool_reference, 
			    (tvunref_t)ipv6_pool_dereference);
	}
}

/*
 * Schedule timeouts across all pools.
 */
void
schedule_all_ipv6_lease_timeouts(void) {
	int i;

	for (i=0; i<num_pools; i++) {
		schedule_lease_timeout(pools[i]);
	}
}

/* 
 * Given an address and the length of the network mask, return
 * only the network portion.
 *
 * Examples:
 *
 *   "fe80::216:6fff:fe49:7d9b", length 64 = "fe80::"
 *   "2001:888:1936:2:216:6fff:fe49:7d9b", length 48 = "2001:888:1936::"
 */
static void
ipv6_network_portion(struct in6_addr *result, 
		     const struct in6_addr *addr, int bits) {
	unsigned char *addrp;
	int mask_bits;
	int bytes;
	int extra_bits;
	int i;

	static const unsigned char bitmasks[] = {
		0x00, 0xFE, 0xFC, 0xF8, 
		0xF0, 0xE0, 0xC0, 0x80, 
	};

	/* 
	 *  Sanity check our bits. ;)
	 */
	if ((bits < 0) || (bits > 128)) {
		log_fatal("ipv6_network_portion: bits %d not between 0 and 128",
			  bits);
	}

	/* 
	 * Copy our address portion.
	 */
	*result = *addr;
	addrp = ((unsigned char *)result) + 15;

	/* 
	 * Zero out masked portion.
	 */
	mask_bits = 128 - bits;
	bytes = mask_bits / 8;
	extra_bits = mask_bits % 8;

	for (i=0; i<bytes; i++) {
		*addrp = 0;
		addrp--;
	}
	if (extra_bits) {
		*addrp &= bitmasks[extra_bits];
	}
}

/*
 * Determine if the given address/prefix is in the pool.
 */
isc_boolean_t
ipv6_in_pool(const struct in6_addr *addr, const struct ipv6_pool *pool) {
	struct in6_addr tmp;

	ipv6_network_portion(&tmp, addr, pool->bits);
	if (memcmp(&tmp, &pool->start_addr, sizeof(tmp)) == 0) {
		return ISC_TRUE;
	} else {
		return ISC_FALSE;
	}
}

/*
 * Find the pool that contains the given address.
 *
 * - pool must be a pointer to a (struct ipv6_pool *) pointer previously
 *   initialized to NULL
 */
isc_result_t
find_ipv6_pool(struct ipv6_pool **pool, u_int16_t type,
	       const struct in6_addr *addr) {
	int i;

	if (pool == NULL) {
		log_error("%s(%d): NULL pointer reference", MDL);
		return DHCP_R_INVALIDARG;
	}
	if (*pool != NULL) {
		log_error("%s(%d): non-NULL pointer", MDL);
		return DHCP_R_INVALIDARG;
	}

	for (i=0; i<num_pools; i++) {
		if (pools[i]->pool_type != type)
			continue;
		if (ipv6_in_pool(addr, pools[i])) { 
			ipv6_pool_reference(pool, pools[i], MDL);
			return ISC_R_SUCCESS;
		}
	}
	return ISC_R_NOTFOUND;
}

/*
 * Helper function for the various functions that act across all
 * pools.
 */
static isc_result_t 
change_leases(struct ia_xx *ia, 
	      isc_result_t (*change_func)(struct ipv6_pool *,
					  struct iasubopt *)) {
	isc_result_t retval;
	isc_result_t renew_retval;
	struct ipv6_pool *pool;
	struct in6_addr *addr;
	int i;

	retval = ISC_R_SUCCESS;
	for (i=0; i<ia->num_iasubopt; i++) {
		pool = NULL;
		addr = &ia->iasubopt[i]->addr;
		if (find_ipv6_pool(&pool, ia->ia_type,
				   addr) == ISC_R_SUCCESS) {
			renew_retval = change_func(pool, ia->iasubopt[i]);
			if (renew_retval != ISC_R_SUCCESS) {
				retval = renew_retval;
			}
		}
		/* XXXsk: should we warn if we don't find a pool? */
	}
	return retval;
}

/*
 * Renew all leases in an IA from all pools.
 *
 * The new lifetime should be in the soft_lifetime_end_time
 * and will be moved to hard_lifetime_end_time by renew_lease6.
 */
isc_result_t 
renew_leases(struct ia_xx *ia) {
	return change_leases(ia, renew_lease6);
}

/*
 * Release all leases in an IA from all pools.
 */
isc_result_t 
release_leases(struct ia_xx *ia) {
	return change_leases(ia, release_lease6);
}

/*
 * Decline all leases in an IA from all pools.
 */
isc_result_t 
decline_leases(struct ia_xx *ia) {
	return change_leases(ia, decline_lease6);
}

#ifdef DHCPv6
/*
 * Helper function to output leases.
 */
static int write_error;

static isc_result_t 
write_ia_leases(const void *name, unsigned len, void *value) {
	struct ia_xx *ia = (struct ia_xx *)value;
	
	if (!write_error) { 
		if (!write_ia(ia)) {
			write_error = 1;
		}
	}
	return ISC_R_SUCCESS;
}

/*
 * Write all DHCPv6 information.
 */
int
write_leases6(void) {
	int nas, tas, pds;

	write_error = 0;
	write_server_duid();
	nas = ia_hash_foreach(ia_na_active, write_ia_leases);
	if (write_error) {
		return 0;
	}
	tas = ia_hash_foreach(ia_ta_active, write_ia_leases);
	if (write_error) {
		return 0;
	}
	pds = ia_hash_foreach(ia_pd_active, write_ia_leases);
	if (write_error) {
		return 0;
	}

	log_info("Wrote %d NA, %d TA, %d PD leases to lease file.",
		 nas, tas, pds);
	return 1;
}
#endif /* DHCPv6 */

static isc_result_t
mark_hosts_unavailable_support(const void *name, unsigned len, void *value) {
	struct host_decl *h;
	struct data_string fixed_addr;
	struct in6_addr addr;
	struct ipv6_pool *p;

	h = (struct host_decl *)value;

	/*
	 * If the host has no address, we don't need to mark anything.
	 */
	if (h->fixed_addr == NULL) {
		return ISC_R_SUCCESS;
	}

	/* 
	 * Evaluate the fixed address.
	 */
	memset(&fixed_addr, 0, sizeof(fixed_addr));
	if (!evaluate_option_cache(&fixed_addr, NULL, NULL, NULL, NULL, NULL,
				   &global_scope, h->fixed_addr, MDL)) {
		log_error("mark_hosts_unavailable: "
			  "error evaluating host address.");
		return ISC_R_SUCCESS;
	}
	if (fixed_addr.len != 16) {
		log_error("mark_hosts_unavailable: "
			  "host address is not 128 bits.");
		return ISC_R_SUCCESS;
	}
	memcpy(&addr, fixed_addr.data, 16);
	data_string_forget(&fixed_addr, MDL);

	/*
	 * Find the pool holding this host, and mark the address.
	 * (I suppose it is arguably valid to have a host that does not
	 * sit in any pool.)
	 */
	p = NULL;
	if (find_ipv6_pool(&p, D6O_IA_NA, &addr) == ISC_R_SUCCESS) {
		mark_lease_unavailable(p, &addr);
		ipv6_pool_dereference(&p, MDL);
	} 
	if (find_ipv6_pool(&p, D6O_IA_TA, &addr) == ISC_R_SUCCESS) {
		mark_lease_unavailable(p, &addr);
		ipv6_pool_dereference(&p, MDL);
	} 

	return ISC_R_SUCCESS;
}

void
mark_hosts_unavailable(void) {
	hash_foreach(host_name_hash, mark_hosts_unavailable_support);
}

static isc_result_t
mark_phosts_unavailable_support(const void *name, unsigned len, void *value) {
	struct host_decl *h;
	struct iaddrcidrnetlist *l;
	struct in6_addr pref;
	struct ipv6_pool *p;

	h = (struct host_decl *)value;

	/*
	 * If the host has no prefix, we don't need to mark anything.
	 */
	if (h->fixed_prefix == NULL) {
		return ISC_R_SUCCESS;
	}

	/* 
	 * Get the fixed prefixes.
	 */
	for (l = h->fixed_prefix; l != NULL; l = l->next) {
		if (l->cidrnet.lo_addr.len != 16) {
			continue;
		}
		memcpy(&pref, l->cidrnet.lo_addr.iabuf, 16);

		/*
		 * Find the pool holding this host, and mark the prefix.
		 * (I suppose it is arguably valid to have a host that does not
		 * sit in any pool.)
		 */
		p = NULL;
		if (find_ipv6_pool(&p, D6O_IA_PD, &pref) != ISC_R_SUCCESS) {
			continue;
		}
		if (l->cidrnet.bits != p->units) {
			ipv6_pool_dereference(&p, MDL);
			continue;
		}
		mark_lease_unavailable(p, &pref);
		ipv6_pool_dereference(&p, MDL);
	} 

	return ISC_R_SUCCESS;
}

void
mark_phosts_unavailable(void) {
	hash_foreach(host_name_hash, mark_phosts_unavailable_support);
}

void 
mark_interfaces_unavailable(void) {
	struct interface_info *ip;
	int i;
	struct ipv6_pool *p;

	ip = interfaces;
	while (ip != NULL) {
		for (i=0; i<ip->v6address_count; i++) {
			p = NULL;
			if (find_ipv6_pool(&p, D6O_IA_NA, &ip->v6addresses[i]) 
							== ISC_R_SUCCESS) {
				mark_lease_unavailable(p, 
						       &ip->v6addresses[i]);
				ipv6_pool_dereference(&p, MDL);
			} 
			if (find_ipv6_pool(&p, D6O_IA_TA, &ip->v6addresses[i]) 
							== ISC_R_SUCCESS) {
				mark_lease_unavailable(p, 
						       &ip->v6addresses[i]);
				ipv6_pool_dereference(&p, MDL);
			} 
		}
		ip = ip->next;
	}
}

/*!
 * \brief Create a new IPv6 pond structure.
 *
 * Allocate space for a new ipv6_pond structure and return a reference
 * to it, includes setting the reference count to 1.
 *
 * \param pond = space for returning a referenced pointer to the pond.
 *		 This must point to a space that has been initialzied
 *		 to NULL by the caller.
 *
 * \return
 * ISC_R_SUCCESS     = The pond was successfully created, pond points to it.
 * DHCP_R_INVALIDARG = One of the arugments was invalid, pond has not been
 *		       modified
 * ISC_R_NOMEMORY    = The system wasn't able to allocate memory, pond has
 *		       not been modified.
 */
isc_result_t
ipv6_pond_allocate(struct ipv6_pond **pond, const char *file, int line) {
	struct ipv6_pond *tmp;

	if (pond == NULL) {
		log_error("%s(%d): NULL pointer reference", file, line);
		return DHCP_R_INVALIDARG;
	}
	if (*pond != NULL) {
		log_error("%s(%d): non-NULL pointer", file, line);
		return DHCP_R_INVALIDARG;
	}

	tmp = dmalloc(sizeof(*tmp), file, line);
	if (tmp == NULL) {
		return ISC_R_NOMEMORY;
	}

	tmp->refcnt = 1;

	*pond = tmp;
	return ISC_R_SUCCESS;
}

/*!
 *
 * \brief reference an IPv6 pond structure.
 *
 * This function genreates a reference to an ipv6_pond structure
 * and increments the reference count on the structure.
 *
 * \param[out] pond = space for returning a referenced pointer to the pond.
 *		      This must point to a space that has been initialzied
 *		      to NULL by the caller.
 * \param[in]  src  = A pointer to the pond to reference.  This must not be
 *		      NULL.
 *
 * \return
 * ISC_R_SUCCESS     = The pond was successfully referenced, pond now points
 *		       to src.
 * DHCP_R_INVALIDARG = One of the arugments was invalid, pond has not been
 *		       modified.
 */
isc_result_t
ipv6_pond_reference(struct ipv6_pond **pond, struct ipv6_pond *src,
		    const char *file, int line) {
	if (pond == NULL) {
		log_error("%s(%d): NULL pointer reference", file, line);
		return DHCP_R_INVALIDARG;
	}
	if (*pond != NULL) {
		log_error("%s(%d): non-NULL pointer", file, line);
		return DHCP_R_INVALIDARG;
	}
	if (src == NULL) {
		log_error("%s(%d): NULL pointer reference", file, line);
		return DHCP_R_INVALIDARG;
	}
	*pond = src;
	src->refcnt++;
	return ISC_R_SUCCESS;
}

/*!
 *
 * \brief de-reference an IPv6 pond structure.
 *
 * This function decrements the reference count in an ipv6_pond structure.
 * If this was the last reference then the memory for the structure is
 * freed.
 *
 * \param[in] pond = A pointer to the pointer to the pond that should be
 *		     de-referenced.  On success the pointer to the pond
 *		     is cleared.  It must not be NULL and must not point
 *		     to NULL.
 *
 * \return
 * ISC_R_SUCCESS     = The pond was successfully de-referenced, pond now points
 *		       to NULL
 * DHCP_R_INVALIDARG = One of the arugments was invalid, pond has not been
 *		       modified.
 */

isc_result_t
ipv6_pond_dereference(struct ipv6_pond **pond, const char *file, int line) {
	struct ipv6_pond *tmp;

	if ((pond == NULL) || (*pond == NULL)) {
		log_error("%s(%d): NULL pointer", file, line);
		return DHCP_R_INVALIDARG;
	}

	tmp = *pond;
	*pond = NULL;

	tmp->refcnt--;
	if (tmp->refcnt < 0) {
		log_error("%s(%d): negative refcnt", file, line);
		tmp->refcnt = 0;
	}
	if (tmp->refcnt == 0) {
		dfree(tmp, file, line);
	}

	return ISC_R_SUCCESS;
}

/* unittest moved to server/tests/mdb6_unittest.c */

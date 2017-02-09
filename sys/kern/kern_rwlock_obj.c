/*	$NetBSD: kern_rwlock_obj.c,v 1.3 2011/05/13 22:16:43 rmind Exp $	*/

/*-
 * Copyright (c) 2008, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_rwlock_obj.c,v 1.3 2011/05/13 22:16:43 rmind Exp $");

#include <sys/param.h>
#include <sys/atomic.h>
#include <sys/pool.h>
#include <sys/rwlock.h>

/* Mutex cache */
#define	RW_OBJ_MAGIC	0x85d3c85d
struct krwobj {
	krwlock_t	ro_lock;
	u_int		ro_magic;
	u_int		ro_refcnt;
};

static int	rw_obj_ctor(void *, void *, int);

static pool_cache_t	rw_obj_cache	__read_mostly;

/*
 * rw_obj_init:
 *
 *	Initialize the rw object store.
 */
void
rw_obj_init(void)
{

	rw_obj_cache = pool_cache_init(sizeof(struct krwobj),
	    coherency_unit, 0, 0, "rwlock", NULL, IPL_NONE, rw_obj_ctor,
	    NULL, NULL);
}

/*
 * rw_obj_ctor:
 *
 *	Initialize a new lock for the cache.
 */
static int
rw_obj_ctor(void *arg, void *obj, int flags)
{
	struct krwobj * ro = obj;

	ro->ro_magic = RW_OBJ_MAGIC;

	return 0;
}

/*
 * rw_obj_alloc:
 *
 *	Allocate a single lock object.
 */
krwlock_t *
rw_obj_alloc(void)
{
	struct krwobj *ro;

	ro = pool_cache_get(rw_obj_cache, PR_WAITOK);
	rw_init(&ro->ro_lock);
	ro->ro_refcnt = 1;

	return (krwlock_t *)ro;
}

/*
 * rw_obj_hold:
 *
 *	Add a single reference to a lock object.  A reference to the object
 *	must already be held, and must be held across this call.
 */
void
rw_obj_hold(krwlock_t *lock)
{
	struct krwobj *ro = (struct krwobj *)lock;

	KASSERT(ro->ro_magic == RW_OBJ_MAGIC);
	KASSERT(ro->ro_refcnt > 0);

	atomic_inc_uint(&ro->ro_refcnt);
}

/*
 * rw_obj_free:
 *
 *	Drop a reference from a lock object.  If the last reference is being
 *	dropped, free the object and return true.  Otherwise, return false.
 */
bool
rw_obj_free(krwlock_t *lock)
{
	struct krwobj *ro = (struct krwobj *)lock;

	KASSERT(ro->ro_magic == RW_OBJ_MAGIC);
	KASSERT(ro->ro_refcnt > 0);

	if (atomic_dec_uint_nv(&ro->ro_refcnt) > 0) {
		return false;
	}
	rw_destroy(&ro->ro_lock);
	pool_cache_put(rw_obj_cache, ro);
	return true;
}

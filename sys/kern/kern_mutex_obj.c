/*	$NetBSD: kern_mutex_obj.c,v 1.5 2011/09/27 01:02:38 jym Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: kern_mutex_obj.c,v 1.5 2011/09/27 01:02:38 jym Exp $");

#include <sys/param.h>
#include <sys/atomic.h>
#include <sys/mutex.h>
#include <sys/pool.h>

/* Mutex cache */
#define	MUTEX_OBJ_MAGIC	0x5aa3c85d
struct kmutexobj {
	kmutex_t	mo_lock;
	u_int		mo_magic;
	u_int		mo_refcnt;
};

static int	mutex_obj_ctor(void *, void *, int);

static pool_cache_t	mutex_obj_cache		__read_mostly;

/*
 * mutex_obj_init:
 *
 *	Initialize the mutex object store.
 */
void
mutex_obj_init(void)
{

	mutex_obj_cache = pool_cache_init(sizeof(struct kmutexobj),
	    coherency_unit, 0, 0, "mutex", NULL, IPL_NONE, mutex_obj_ctor,
	    NULL, NULL);
}

/*
 * mutex_obj_ctor:
 *
 *	Initialize a new lock for the cache.
 */
static int
mutex_obj_ctor(void *arg, void *obj, int flags)
{
	struct kmutexobj * mo = obj;

	mo->mo_magic = MUTEX_OBJ_MAGIC;

	return 0;
}

/*
 * mutex_obj_alloc:
 *
 *	Allocate a single lock object.
 */
kmutex_t *
mutex_obj_alloc(kmutex_type_t type, int ipl)
{
	struct kmutexobj *mo;

	mo = pool_cache_get(mutex_obj_cache, PR_WAITOK);
	mutex_init(&mo->mo_lock, type, ipl);
	mo->mo_refcnt = 1;

	return (kmutex_t *)mo;
}

/*
 * mutex_obj_hold:
 *
 *	Add a single reference to a lock object.  A reference to the object
 *	must already be held, and must be held across this call.
 */
void
mutex_obj_hold(kmutex_t *lock)
{
	struct kmutexobj *mo = (struct kmutexobj *)lock;

	KASSERTMSG(mo->mo_magic == MUTEX_OBJ_MAGIC,
	    "%s: lock %p: mo->mo_magic (%#x) != MUTEX_OBJ_MAGIC (%#x)",
	     __func__, mo, mo->mo_magic, MUTEX_OBJ_MAGIC);
	KASSERTMSG(mo->mo_refcnt > 0,
	    "%s: lock %p: mo->mo_refcnt (%#x) == 0",
	     __func__, mo, mo->mo_refcnt);

	atomic_inc_uint(&mo->mo_refcnt);
}

/*
 * mutex_obj_free:
 *
 *	Drop a reference from a lock object.  If the last reference is being
 *	dropped, free the object and return true.  Otherwise, return false.
 */
bool
mutex_obj_free(kmutex_t *lock)
{
	struct kmutexobj *mo = (struct kmutexobj *)lock;

	KASSERTMSG(mo->mo_magic == MUTEX_OBJ_MAGIC,
	    "%s: lock %p: mo->mo_magic (%#x) != MUTEX_OBJ_MAGIC (%#x)",
	     __func__, mo, mo->mo_magic, MUTEX_OBJ_MAGIC);
	KASSERTMSG(mo->mo_refcnt > 0,
	    "%s: lock %p: mo->mo_refcnt (%#x) == 0",
	     __func__, mo, mo->mo_refcnt);

	if (atomic_dec_uint_nv(&mo->mo_refcnt) > 0) {
		return false;
	}
	mutex_destroy(&mo->mo_lock);
	pool_cache_put(mutex_obj_cache, mo);
	return true;
}

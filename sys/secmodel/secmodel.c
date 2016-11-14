/* $NetBSD: secmodel.c,v 1.2 2014/11/04 16:01:58 maxv Exp $ */
/*-
 * Copyright (c) 2011 Elad Efrat <elad@NetBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>

#include <sys/atomic.h>
#include <sys/kauth.h>
#include <sys/kmem.h>
#include <sys/queue.h>
#include <sys/rwlock.h>
#include <secmodel/secmodel.h>
#include <prop/proplib.h>

/* List of secmodels, parameters, and lock. */
static LIST_HEAD(, secmodel_descr) secmodels =
    LIST_HEAD_INITIALIZER(secmodels);
static unsigned int secmodel_copy_cred_on_fork = false;
static krwlock_t secmodels_lock;
static int nsecmodels = 0; /* number of registered secmodels */

static int secmodel_plug(secmodel_t);
static int secmodel_unplug(secmodel_t);

int
secmodel_nsecmodels(void)
{

	return nsecmodels;
}

void
secmodel_init(void)
{

	rw_init(&secmodels_lock);

	secmodel_copy_cred_on_fork = false;
}

/*
 * Register a new secmodel.
 */
int
secmodel_register(secmodel_t *secmodel, const char *id, const char *name,
		  prop_dictionary_t behavior,
		  secmodel_eval_t eval, secmodel_setinfo_t setinfo)
{
	int err;
	secmodel_t sm;

	sm = kmem_alloc(sizeof(*sm), KM_SLEEP);

	sm->sm_id = id;
	sm->sm_name = name;
	sm->sm_behavior = behavior;
	sm->sm_eval = eval;
	sm->sm_setinfo = setinfo;

	err = secmodel_plug(sm);
	if (err == 0) {
		atomic_inc_uint(&nsecmodels);
	} else {
		kmem_free(sm, sizeof(*sm));
		sm = NULL;
	}

	*secmodel = sm;
	return err;
}

/*
 * Deregister a secmodel.
 */
int
secmodel_deregister(secmodel_t sm)
{
	int error;

	error = secmodel_unplug(sm);
	if (error == 0) {
		atomic_dec_uint(&nsecmodels);
		kmem_free(sm, sizeof(*sm));
	}

	return error;
}

/*
 * Lookup a secmodel by its id.
 *
 * Requires "secmodels_lock" handling by the caller.
 */
static secmodel_t
secmodel_lookup(const char *id)
{
	secmodel_t tsm;

	KASSERT(rw_lock_held(&secmodels_lock));

	LIST_FOREACH(tsm, &secmodels, sm_list) {
		if (strcasecmp(tsm->sm_id, id) == 0) {
			return tsm;
		}
	}

	return NULL;
}

/*
 * Adjust system-global secmodel behavior following the addition
 * or removal of a secmodel.
 *
 * Requires "secmodels_lock" to be held by the caller.
 */
static void
secmodel_adjust_behavior(secmodel_t sm, bool added)
{
	bool r, b;

	KASSERT(rw_write_held(&secmodels_lock));

#define	ADJUST_COUNTER(which, added)		\
	do {					\
		if (added) {			\
			(which)++;		\
		} else {			\
			if ((which) > 0)	\
				(which)--;	\
		}				\
	} while (/*CONSTCOND*/0)

	/* Copy credentials on fork? */
	r = prop_dictionary_get_bool(sm->sm_behavior, "copy-cred-on-fork", &b);
	if (r) {
		ADJUST_COUNTER(secmodel_copy_cred_on_fork, added);
	}

#undef ADJUST_COUNTER
}

static int
secmodel_plug(secmodel_t sm)
{
	secmodel_t tsm;
	int error = 0;

	if (sm == NULL)
		return EFAULT;

	/* Check if the secmodel is already present. */
	rw_enter(&secmodels_lock, RW_WRITER);
	tsm = secmodel_lookup(sm->sm_id);
	if (tsm != NULL) {
		error = EEXIST;
		goto out;
	}

	/* Add the secmodel. */
	LIST_INSERT_HEAD(&secmodels, sm, sm_list);

	/* Adjust behavior. */
	secmodel_adjust_behavior(sm, true);

 out:
	/* Unlock the secmodels list. */
	rw_exit(&secmodels_lock);

	return error;
}

static int
secmodel_unplug(secmodel_t sm)
{
	secmodel_t tsm;
	int error = 0;

	if (sm == NULL)
		return EFAULT;

	/* Make sure the secmodel is present. */
	rw_enter(&secmodels_lock, RW_WRITER);
	tsm = secmodel_lookup(sm->sm_id);
	if (tsm == NULL) {
		error = ENOENT;
		goto out;
	}

	/* Remove the secmodel. */
	LIST_REMOVE(tsm, sm_list);

	/* Adjust behavior. */
	secmodel_adjust_behavior(tsm, false);

 out:
	/* Unlock the secmodels list. */
	rw_exit(&secmodels_lock);

	return error;
}

/* XXX TODO */
int
secmodel_setinfo(const char *id, void *v, int *err)
{

	return EOPNOTSUPP;
}

int
secmodel_eval(const char *id, const char *what, void *arg, void *ret)
{
	secmodel_t sm;
	int error = 0;

	rw_enter(&secmodels_lock, RW_READER);
	sm = secmodel_lookup(id);
	if (sm == NULL) {
		error = EINVAL;
		goto out;
	}

	if (sm->sm_eval == NULL) {
		error = ENOENT;
		goto out;
	}

	if (ret == NULL) {
		error = EFAULT;
		goto out;
	}

	error = sm->sm_eval(what, arg, ret);
	/* pass error from a secmodel(9) callback as a negative value */
	error = -error;

 out:
	rw_exit(&secmodels_lock);

	return error;
}

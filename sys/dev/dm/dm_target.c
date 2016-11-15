/*        $NetBSD: dm_target.c,v 1.19 2014/10/18 08:33:27 snj Exp $      */

/*
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Hamsik.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code Must retain the above copyright
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

#include <sys/types.h>
#include <sys/param.h>

#include <sys/kmem.h>
#include <sys/module.h>


#include "netbsd-dm.h"
#include "dm.h"

static dm_target_t *dm_target_lookup_name(const char *);

TAILQ_HEAD(dm_target_head, dm_target);

static struct dm_target_head dm_target_list =
TAILQ_HEAD_INITIALIZER(dm_target_list);

kmutex_t dm_target_mutex;

/*
 * Called indirectly from dm_table_load_ioctl to mark target as used.
 */
void
dm_target_busy(dm_target_t * target)
{
	atomic_inc_32(&target->ref_cnt);
}
/*
 * Release reference counter on target.
 */
void
dm_target_unbusy(dm_target_t * target)
{
	KASSERT(target->ref_cnt > 0);
	atomic_dec_32(&target->ref_cnt);
}
/*
 * Try to autoload target module if it was not found in current
 * target list.
 */
dm_target_t *
dm_target_autoload(const char *dm_target_name)
{
	char name[30];
	u_int gen;
	dm_target_t *dmt;

	snprintf(name, sizeof(name), "dm_target_%s", dm_target_name);
	name[29] = '\0';

	do {
		gen = module_gen;

		/* Try to autoload target module */
		(void) module_autoload(name, MODULE_CLASS_MISC);
	} while (gen != module_gen);

	mutex_enter(&dm_target_mutex);
	dmt = dm_target_lookup_name(dm_target_name);
	if (dmt != NULL)
		dm_target_busy(dmt);
	mutex_exit(&dm_target_mutex);

	return dmt;
}
/*
 * Lookup for target in global target list.
 */
dm_target_t *
dm_target_lookup(const char *dm_target_name)
{
	dm_target_t *dmt;

	dmt = NULL;

	if (dm_target_name == NULL)
		return NULL;

	mutex_enter(&dm_target_mutex);

	dmt = dm_target_lookup_name(dm_target_name);
	if (dmt != NULL)
		dm_target_busy(dmt);

	mutex_exit(&dm_target_mutex);

	return dmt;
}
/*
 * Search for name in TAIL and return apropriate pointer.
 */
static dm_target_t *
dm_target_lookup_name(const char *dm_target_name)
{
	dm_target_t *dm_target;
	int dlen;
	int slen;

	slen = strlen(dm_target_name) + 1;

	TAILQ_FOREACH(dm_target, &dm_target_list, dm_target_next) {
		dlen = strlen(dm_target->name) + 1;
		if (dlen != slen)
			continue;

		if (strncmp(dm_target_name, dm_target->name, slen) == 0)
			return dm_target;
	}

	return NULL;
}
/*
 * Insert new target struct into the TAIL.
 * dm_target
 *   contains name, version, function pointer to specifif target functions.
 */
int
dm_target_insert(dm_target_t * dm_target)
{
	dm_target_t *dmt;

	/* Sanity check for any missing function */
	KASSERT(dm_target->init != NULL);
	KASSERT(dm_target->status != NULL);
	KASSERT(dm_target->strategy != NULL);
	KASSERT(dm_target->deps != NULL);
	KASSERT(dm_target->destroy != NULL);
	KASSERT(dm_target->upcall != NULL);
	KASSERT(dm_target->sync != NULL);
	KASSERT(dm_target->secsize != NULL);

	mutex_enter(&dm_target_mutex);

	dmt = dm_target_lookup_name(dm_target->name);
	if (dmt != NULL) {
		mutex_exit(&dm_target_mutex);
		return EEXIST;
	}
	TAILQ_INSERT_TAIL(&dm_target_list, dm_target, dm_target_next);

	mutex_exit(&dm_target_mutex);

	return 0;
}


/*
 * Remove target from TAIL, target is selected with its name.
 */
int
dm_target_rem(char *dm_target_name)
{
	dm_target_t *dmt;

	KASSERT(dm_target_name != NULL);

	mutex_enter(&dm_target_mutex);

	dmt = dm_target_lookup_name(dm_target_name);
	if (dmt == NULL) {
		mutex_exit(&dm_target_mutex);
		return ENOENT;
	}
	if (dmt->ref_cnt > 0) {
		mutex_exit(&dm_target_mutex);
		return EBUSY;
	}
	TAILQ_REMOVE(&dm_target_list,
	    dmt, dm_target_next);

	mutex_exit(&dm_target_mutex);

	(void) kmem_free(dmt, sizeof(dm_target_t));

	return 0;
}
/*
 * Destroy all targets and remove them from queue.
 * This routine is called from dm_detach, before module
 * is unloaded.
 */
int
dm_target_destroy(void)
{
	dm_target_t *dm_target;

	mutex_enter(&dm_target_mutex);
	while (TAILQ_FIRST(&dm_target_list) != NULL) {

		dm_target = TAILQ_FIRST(&dm_target_list);

		TAILQ_REMOVE(&dm_target_list, TAILQ_FIRST(&dm_target_list),
		    dm_target_next);

		(void) kmem_free(dm_target, sizeof(dm_target_t));
	}
	mutex_exit(&dm_target_mutex);

	mutex_destroy(&dm_target_mutex);

	return 0;
}
/*
 * Allocate new target entry.
 */
dm_target_t *
dm_target_alloc(const char *name)
{
	return kmem_zalloc(sizeof(dm_target_t), KM_SLEEP);
}
/*
 * Return prop_array of dm_target dictionaries.
 */
prop_array_t
dm_target_prop_list(void)
{
	prop_array_t target_array, ver;
	prop_dictionary_t target_dict;
	dm_target_t *dm_target;

	size_t i;

	target_array = prop_array_create();

	mutex_enter(&dm_target_mutex);

	TAILQ_FOREACH(dm_target, &dm_target_list, dm_target_next) {

		target_dict = prop_dictionary_create();
		ver = prop_array_create();
		prop_dictionary_set_cstring(target_dict, DM_TARGETS_NAME,
		    dm_target->name);

		for (i = 0; i < 3; i++)
			prop_array_add_uint32(ver, dm_target->version[i]);

		prop_dictionary_set(target_dict, DM_TARGETS_VERSION, ver);
		prop_array_add(target_array, target_dict);

		prop_object_release(ver);
		prop_object_release(target_dict);
	}

	mutex_exit(&dm_target_mutex);

	return target_array;
}
/* Initialize dm_target subsystem. */
int
dm_target_init(void)
{
	dm_target_t *dmt, *dmt3;
	int r;

	r = 0;

	mutex_init(&dm_target_mutex, MUTEX_DEFAULT, IPL_NONE);

	dmt = dm_target_alloc("linear");
	dmt3 = dm_target_alloc("striped");

	dmt->version[0] = 1;
	dmt->version[1] = 0;
	dmt->version[2] = 2;
	strlcpy(dmt->name, "linear", DM_MAX_TYPE_NAME);
	dmt->init = &dm_target_linear_init;
	dmt->status = &dm_target_linear_status;
	dmt->strategy = &dm_target_linear_strategy;
	dmt->sync = &dm_target_linear_sync;
	dmt->deps = &dm_target_linear_deps;
	dmt->destroy = &dm_target_linear_destroy;
	dmt->upcall = &dm_target_linear_upcall;
	dmt->secsize = &dm_target_linear_secsize;

	r = dm_target_insert(dmt);

	dmt3->version[0] = 1;
	dmt3->version[1] = 0;
	dmt3->version[2] = 3;
	strlcpy(dmt3->name, "striped", DM_MAX_TYPE_NAME);
	dmt3->init = &dm_target_stripe_init;
	dmt3->status = &dm_target_stripe_status;
	dmt3->strategy = &dm_target_stripe_strategy;
	dmt3->sync = &dm_target_stripe_sync;
	dmt3->deps = &dm_target_stripe_deps;
	dmt3->destroy = &dm_target_stripe_destroy;
	dmt3->upcall = &dm_target_stripe_upcall;
	dmt3->secsize = &dm_target_stripe_secsize;

	r = dm_target_insert(dmt3);

	return r;
}

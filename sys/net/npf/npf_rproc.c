/*	$NetBSD: npf_rproc.c,v 1.12 2014/08/11 01:54:12 rmind Exp $	*/

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
 * NPF extension and rule procedure interface.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD");

#include <sys/param.h>
#include <sys/types.h>

#include <sys/atomic.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/module.h>

#include "npf_impl.h"

#define	EXT_NAME_LEN		32

typedef struct npf_ext {
	char			ext_callname[EXT_NAME_LEN];
	LIST_ENTRY(npf_ext)	ext_entry;
	const npf_ext_ops_t *	ext_ops;
	unsigned		ext_refcnt;
} npf_ext_t;

struct npf_rprocset {
	LIST_HEAD(, npf_rproc)	rps_list;
};

#define	RPROC_NAME_LEN		32
#define	RPROC_EXT_COUNT		16

struct npf_rproc {
	/* Flags and reference count. */
	uint32_t		rp_flags;
	u_int			rp_refcnt;

	/* Associated extensions and their metadata . */
	unsigned		rp_ext_count;
	npf_ext_t *		rp_ext[RPROC_EXT_COUNT];
	void *			rp_ext_meta[RPROC_EXT_COUNT];

	/* Name of the procedure and list entry. */
	char			rp_name[RPROC_NAME_LEN];
	LIST_ENTRY(npf_rproc)	rp_entry;
};

static LIST_HEAD(, npf_ext)	ext_list	__cacheline_aligned;
static kmutex_t			ext_lock	__cacheline_aligned;

void
npf_ext_sysinit(void)
{
	mutex_init(&ext_lock, MUTEX_DEFAULT, IPL_NONE);
	LIST_INIT(&ext_list);
}

void
npf_ext_sysfini(void)
{
	KASSERT(LIST_EMPTY(&ext_list));
	mutex_destroy(&ext_lock);
}

/*
 * NPF extension management for the rule procedures.
 */

static const char npf_ext_prefix[] = "npf_ext_";
#define NPF_EXT_PREFLEN (sizeof(npf_ext_prefix) - 1)

static npf_ext_t *
npf_ext_lookup(const char *name, bool autoload)
{
	npf_ext_t *ext;
	char modname[RPROC_NAME_LEN + NPF_EXT_PREFLEN];
	int error;

	KASSERT(mutex_owned(&ext_lock));

again:
	LIST_FOREACH(ext, &ext_list, ext_entry)
		if (strcmp(ext->ext_callname, name) == 0)
			break;

	if (ext != NULL || !autoload)
		return ext;

	mutex_exit(&ext_lock);
	autoload = false;
	snprintf(modname, sizeof(modname), "%s%s", npf_ext_prefix, name);
	error = module_autoload(modname, MODULE_CLASS_MISC);
	mutex_enter(&ext_lock);

	if (error)
		return NULL;
	goto again;
}

void *
npf_ext_register(const char *name, const npf_ext_ops_t *ops)
{
	npf_ext_t *ext;

	ext = kmem_zalloc(sizeof(npf_ext_t), KM_SLEEP);
	strlcpy(ext->ext_callname, name, EXT_NAME_LEN);
	ext->ext_ops = ops;

	mutex_enter(&ext_lock);
	if (npf_ext_lookup(name, false)) {
		mutex_exit(&ext_lock);
		kmem_free(ext, sizeof(npf_ext_t));
		return NULL;
	}
	LIST_INSERT_HEAD(&ext_list, ext, ext_entry);
	mutex_exit(&ext_lock);

	return (void *)ext;
}

int
npf_ext_unregister(void *extid)
{
	npf_ext_t *ext = extid;

	/*
	 * Check if in-use first (re-check with the lock held).
	 */
	if (ext->ext_refcnt) {
		return EBUSY;
	}

	mutex_enter(&ext_lock);
	if (ext->ext_refcnt) {
		mutex_exit(&ext_lock);
		return EBUSY;
	}
	KASSERT(npf_ext_lookup(ext->ext_callname, false));
	LIST_REMOVE(ext, ext_entry);
	mutex_exit(&ext_lock);

	kmem_free(ext, sizeof(npf_ext_t));
	return 0;
}

int
npf_ext_construct(const char *name, npf_rproc_t *rp, prop_dictionary_t params)
{
	const npf_ext_ops_t *extops;
	npf_ext_t *ext;
	unsigned i;
	int error;

	if (rp->rp_ext_count >= RPROC_EXT_COUNT) {
		return ENOSPC;
	}

	mutex_enter(&ext_lock);
	ext = npf_ext_lookup(name, true);
	if (ext) {
		atomic_inc_uint(&ext->ext_refcnt);
	}
	mutex_exit(&ext_lock);

	if (!ext) {
		return ENOENT;
	}

	extops = ext->ext_ops;
	KASSERT(extops != NULL);

	error = extops->ctor(rp, params);
	if (error) {
		atomic_dec_uint(&ext->ext_refcnt);
		return error;
	}
	i = rp->rp_ext_count++;
	rp->rp_ext[i] = ext;
	return 0;
}

/*
 * Rule procedure management.
 */

npf_rprocset_t *
npf_rprocset_create(void)
{
	npf_rprocset_t *rpset;

	rpset = kmem_zalloc(sizeof(npf_rprocset_t), KM_SLEEP);
	LIST_INIT(&rpset->rps_list);
	return rpset;
}

void
npf_rprocset_destroy(npf_rprocset_t *rpset)
{
	npf_rproc_t *rp;

	while ((rp = LIST_FIRST(&rpset->rps_list)) != NULL) {
		LIST_REMOVE(rp, rp_entry);
		npf_rproc_release(rp);
	}
	kmem_free(rpset, sizeof(npf_rprocset_t));
}

/*
 * npf_rproc_lookup: find a rule procedure by the name.
 */
npf_rproc_t *
npf_rprocset_lookup(npf_rprocset_t *rpset, const char *name)
{
	npf_rproc_t *rp;

	LIST_FOREACH(rp, &rpset->rps_list, rp_entry) {
		if (strncmp(rp->rp_name, name, RPROC_NAME_LEN) == 0)
			break;
	}
	return rp;
}

/*
 * npf_rproc_insert: insert a new rule procedure into the set.
 */
void
npf_rprocset_insert(npf_rprocset_t *rpset, npf_rproc_t *rp)
{
	LIST_INSERT_HEAD(&rpset->rps_list, rp, rp_entry);
}

int
npf_rprocset_export(const npf_rprocset_t *rpset, prop_array_t rprocs)
{
	prop_dictionary_t rpdict;
	const npf_rproc_t *rp;

	LIST_FOREACH(rp, &rpset->rps_list, rp_entry) {
		rpdict = prop_dictionary_create();
		prop_dictionary_set_cstring(rpdict, "name", rp->rp_name);
		prop_dictionary_set_uint32(rpdict, "flags", rp->rp_flags);
		prop_array_add(rprocs, rpdict);
		prop_object_release(rpdict);
	}
	return 0;
}

/*
 * npf_rproc_create: construct a new rule procedure, lookup and associate
 * the extension calls with it.
 */
npf_rproc_t *
npf_rproc_create(prop_dictionary_t rpdict)
{
	const char *name;
	npf_rproc_t *rp;

	if (!prop_dictionary_get_cstring_nocopy(rpdict, "name", &name)) {
		return NULL;
	}

	rp = kmem_intr_zalloc(sizeof(npf_rproc_t), KM_SLEEP);
	rp->rp_refcnt = 1;

	strlcpy(rp->rp_name, name, RPROC_NAME_LEN);
	prop_dictionary_get_uint32(rpdict, "flags", &rp->rp_flags);
	return rp;
}

/*
 * npf_rproc_acquire: acquire the reference on the rule procedure.
 */
void
npf_rproc_acquire(npf_rproc_t *rp)
{
	atomic_inc_uint(&rp->rp_refcnt);
}

/*
 * npf_rproc_release: drop the reference count and destroy the rule
 * procedure on the last reference.
 */
void
npf_rproc_release(npf_rproc_t *rp)
{

	KASSERT(rp->rp_refcnt > 0);
	if (atomic_dec_uint_nv(&rp->rp_refcnt) != 0) {
		return;
	}
	/* XXXintr */
	for (unsigned i = 0; i < rp->rp_ext_count; i++) {
		npf_ext_t *ext = rp->rp_ext[i];
		const npf_ext_ops_t *extops = ext->ext_ops;

		extops->dtor(rp, rp->rp_ext_meta[i]);
		atomic_dec_uint(&ext->ext_refcnt);
	}
	kmem_intr_free(rp, sizeof(npf_rproc_t));
}

void
npf_rproc_assign(npf_rproc_t *rp, void *params)
{
	unsigned i = rp->rp_ext_count;

	/* Note: params may be NULL. */
	KASSERT(i < RPROC_EXT_COUNT);
	rp->rp_ext_meta[i] = params;
}

/*
 * npf_rproc_run: run the rule procedure by executing each extension call.
 *
 * => Reference on the rule procedure must be held.
 */
bool
npf_rproc_run(npf_cache_t *npc, npf_rproc_t *rp, int *decision)
{
	const unsigned extcount = rp->rp_ext_count;

	KASSERT(!nbuf_flag_p(npc->npc_nbuf, NBUF_DATAREF_RESET));
	KASSERT(rp->rp_refcnt > 0);

	for (unsigned i = 0; i < extcount; i++) {
		const npf_ext_t *ext = rp->rp_ext[i];
		const npf_ext_ops_t *extops = ext->ext_ops;

		KASSERT(ext->ext_refcnt > 0);
		if (!extops->proc(npc, rp->rp_ext_meta[i], decision)) {
			return false;
		}

		if (nbuf_flag_p(npc->npc_nbuf, NBUF_DATAREF_RESET)) {
			npf_recache(npc);
		}
	}

	return true;
}

/*	$NetBSD: npf_alg.c,v 1.15 2014/08/11 23:48:01 rmind Exp $	*/

/*-
 * Copyright (c) 2010-2013 The NetBSD Foundation, Inc.
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
 * NPF interface for the Application Level Gateways (ALGs).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: npf_alg.c,v 1.15 2014/08/11 23:48:01 rmind Exp $");

#include <sys/param.h>
#include <sys/types.h>

#include <sys/kmem.h>
#include <sys/pserialize.h>
#include <sys/mutex.h>
#include <net/pfil.h>
#include <sys/module.h>

#include "npf_impl.h"

/*
 * NAT ALG description structure.  For more compact use of cache,
 * the functions are separated in their own arrays.  The number of
 * ALGs is expected to be very small.
 */

struct npf_alg {
	const char *	na_name;
	u_int		na_slot;
};

/* List of ALGs and the count. */
static pserialize_t	alg_psz			__cacheline_aligned;
static npf_alg_t	alg_list[NPF_MAX_ALGS]	__read_mostly;
static u_int		alg_count		__read_mostly;

/* Matching, inspection and translation functions. */
static npfa_funcs_t	alg_funcs[NPF_MAX_ALGS]	__read_mostly;

static const char	alg_prefix[] = "npf_alg_";
#define	NPF_EXT_PREFLEN	(sizeof(alg_prefix) - 1)

void
npf_alg_sysinit(void)
{
	alg_psz = pserialize_create();
	memset(alg_list, 0, sizeof(alg_list));
	memset(alg_funcs, 0, sizeof(alg_funcs));
	alg_count = 0;
}

void
npf_alg_sysfini(void)
{
	pserialize_destroy(alg_psz);
}

static npf_alg_t *
npf_alg_lookup(const char *name)
{
	KASSERT(npf_config_locked_p());

	for (u_int i = 0; i < alg_count; i++) {
		npf_alg_t *alg = &alg_list[i];
		const char *aname = alg->na_name;

		if (aname && strcmp(aname, name) == 0)
			return alg;
	}
	return NULL;
}

npf_alg_t *
npf_alg_construct(const char *name)
{
	npf_alg_t *alg;

	npf_config_enter();
	if ((alg = npf_alg_lookup(name)) == NULL) {
		char modname[NPF_EXT_PREFLEN + 64];
		snprintf(modname, sizeof(modname), "%s%s", alg_prefix, name);
		npf_config_exit();

		if (module_autoload(modname, MODULE_CLASS_MISC) != 0) {
			return NULL;
		}
		npf_config_enter();
		alg = npf_alg_lookup(name);
	}
	npf_config_exit();
	return alg;
}

/*
 * npf_alg_register: register application-level gateway.
 */
npf_alg_t *
npf_alg_register(const char *name, const npfa_funcs_t *funcs)
{
	npf_alg_t *alg;
	u_int i;

	npf_config_enter();
	if (npf_alg_lookup(name) != NULL) {
		npf_config_exit();
		return NULL;
	}

	/* Find a spare slot. */
	for (i = 0; i < NPF_MAX_ALGS; i++) {
		alg = &alg_list[i];
		if (alg->na_name == NULL) {
			break;
		}
	}
	if (i == NPF_MAX_ALGS) {
		npf_config_exit();
		return NULL;
	}

	/* Register the ALG. */
	alg->na_name = name;
	alg->na_slot = i;

	/* Assign the functions. */
	alg_funcs[i].match = funcs->match;
	alg_funcs[i].translate = funcs->translate;
	alg_funcs[i].inspect = funcs->inspect;

	alg_count = MAX(alg_count, i + 1);
	npf_config_exit();

	return alg;
}

/*
 * npf_alg_unregister: unregister application-level gateway.
 */
int
npf_alg_unregister(npf_alg_t *alg)
{
	u_int i = alg->na_slot;

	/* Deactivate the functions first. */
	npf_config_enter();
	alg_funcs[i].match = NULL;
	alg_funcs[i].translate = NULL;
	alg_funcs[i].inspect = NULL;
	pserialize_perform(alg_psz);

	/* Finally, unregister the ALG. */
	npf_ruleset_freealg(npf_config_natset(), alg);
	alg->na_name = NULL;
	npf_config_exit();

	return 0;
}

/*
 * npf_alg_match: call ALG matching inspectors, determine if any ALG matches.
 */
bool
npf_alg_match(npf_cache_t *npc, npf_nat_t *nt, int di)
{
	bool match = false;
	int s;

	s = pserialize_read_enter();
	for (u_int i = 0; i < alg_count; i++) {
		const npfa_funcs_t *f = &alg_funcs[i];

		if (f->match && f->match(npc, nt, di)) {
			match = true;
			break;
		}
	}
	pserialize_read_exit(s);
	return match;
}

/*
 * npf_alg_exec: execute ALG hooks for translation.
 */
void
npf_alg_exec(npf_cache_t *npc, npf_nat_t *nt, bool forw)
{
	int s;

	s = pserialize_read_enter();
	for (u_int i = 0; i < alg_count; i++) {
		const npfa_funcs_t *f = &alg_funcs[i];

		if (f->translate) {
			f->translate(npc, nt, forw);
		}
	}
	pserialize_read_exit(s);
}

npf_conn_t *
npf_alg_conn(npf_cache_t *npc, int di)
{
	npf_conn_t *con = NULL;
	int s;

	s = pserialize_read_enter();
	for (u_int i = 0; i < alg_count; i++) {
		const npfa_funcs_t *f = &alg_funcs[i];

		if (!f->inspect)
			continue;
		if ((con = f->inspect(npc, di)) != NULL)
			break;
	}
	pserialize_read_exit(s);
	return con;
}

prop_array_t
npf_alg_export(void)
{
	prop_array_t alglist = prop_array_create();

	KASSERT(npf_config_locked_p());

	for (u_int i = 0; i < alg_count; i++) {
		const npf_alg_t *alg = &alg_list[i];

		if (alg->na_name == NULL) {
			continue;
		}
		prop_dictionary_t algdict = prop_dictionary_create();
		prop_dictionary_set_cstring(algdict, "name", alg->na_name);
		prop_array_add(alglist, algdict);
		prop_object_release(algdict);
	}
	return alglist;
}

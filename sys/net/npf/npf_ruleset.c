/*	$NetBSD: npf_ruleset.c,v 1.42 2015/03/20 23:36:28 rmind Exp $	*/

/*-
 * Copyright (c) 2009-2015 The NetBSD Foundation, Inc.
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
 * NPF ruleset module.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: npf_ruleset.c,v 1.42 2015/03/20 23:36:28 rmind Exp $");

#include <sys/param.h>
#include <sys/types.h>

#include <sys/atomic.h>
#include <sys/kmem.h>
#include <sys/queue.h>
#include <sys/mbuf.h>
#include <sys/types.h>

#include <net/bpf.h>
#include <net/bpfjit.h>
#include <net/pfil.h>
#include <net/if.h>

#include "npf_impl.h"

struct npf_ruleset {
	/*
	 * - List of all rules.
	 * - Dynamic (i.e. named) rules.
	 * - G/C list for convenience.
	 */
	LIST_HEAD(, npf_rule)	rs_all;
	LIST_HEAD(, npf_rule)	rs_dynamic;
	LIST_HEAD(, npf_rule)	rs_gc;

	/* Unique ID counter. */
	uint64_t		rs_idcnt;

	/* Number of array slots and active rules. */
	u_int			rs_slots;
	u_int			rs_nitems;

	/* Array of ordered rules. */
	npf_rule_t *		rs_rules[];
};

struct npf_rule {
	/* Attributes, interface and skip slot. */
	uint32_t		r_attr;
	u_int			r_ifid;
	u_int			r_skip_to;

	/* Code to process, if any. */
	int			r_type;
	bpfjit_func_t		r_jcode;
	void *			r_code;
	u_int			r_clen;

	/* NAT policy (optional), rule procedure and subset. */
	npf_natpolicy_t *	r_natp;
	npf_rproc_t *		r_rproc;

	union {
		/*
		 * Dynamic group: rule subset and a group list entry.
		 */
		struct {
			npf_rule_t *		r_subset;
			LIST_ENTRY(npf_rule)	r_dentry;
		};

		/*
		 * Dynamic rule: priority, parent group and next rule.
		 */
		struct {
			int			r_priority;
			npf_rule_t *		r_parent;
			npf_rule_t *		r_next;
		};
	};

	/* Rule ID, name and the optional key. */
	uint64_t		r_id;
	char			r_name[NPF_RULE_MAXNAMELEN];
	uint8_t			r_key[NPF_RULE_MAXKEYLEN];

	/* All-list entry and the auxiliary info. */
	LIST_ENTRY(npf_rule)	r_aentry;
	prop_data_t		r_info;
};

#define	SKIPTO_ADJ_FLAG		(1U << 31)
#define	SKIPTO_MASK		(SKIPTO_ADJ_FLAG - 1)

static int	npf_rule_export(const npf_ruleset_t *,
    const npf_rule_t *, prop_dictionary_t);

/*
 * Private attributes - must be in the NPF_RULE_PRIVMASK range.
 */
#define	NPF_RULE_KEEPNAT	(0x01000000 & NPF_RULE_PRIVMASK)

#define	NPF_DYNAMIC_GROUP_P(attr) \
    (((attr) & NPF_DYNAMIC_GROUP) == NPF_DYNAMIC_GROUP)

#define	NPF_DYNAMIC_RULE_P(attr) \
    (((attr) & NPF_DYNAMIC_GROUP) == NPF_RULE_DYNAMIC)

npf_ruleset_t *
npf_ruleset_create(size_t slots)
{
	size_t len = offsetof(npf_ruleset_t, rs_rules[slots]);
	npf_ruleset_t *rlset;

	rlset = kmem_zalloc(len, KM_SLEEP);
	LIST_INIT(&rlset->rs_dynamic);
	LIST_INIT(&rlset->rs_all);
	LIST_INIT(&rlset->rs_gc);
	rlset->rs_slots = slots;

	return rlset;
}

void
npf_ruleset_destroy(npf_ruleset_t *rlset)
{
	size_t len = offsetof(npf_ruleset_t, rs_rules[rlset->rs_slots]);
	npf_rule_t *rl;

	while ((rl = LIST_FIRST(&rlset->rs_all)) != NULL) {
		if (NPF_DYNAMIC_GROUP_P(rl->r_attr)) {
			/*
			 * Note: r_subset may point to the rules which
			 * were inherited by a new ruleset.
			 */
			rl->r_subset = NULL;
			LIST_REMOVE(rl, r_dentry);
		}
		if (NPF_DYNAMIC_RULE_P(rl->r_attr)) {
			/* Not removing from r_subset, see above. */
			KASSERT(rl->r_parent != NULL);
		}
		LIST_REMOVE(rl, r_aentry);
		npf_rule_free(rl);
	}
	KASSERT(LIST_EMPTY(&rlset->rs_dynamic));
	KASSERT(LIST_EMPTY(&rlset->rs_gc));
	kmem_free(rlset, len);
}

/*
 * npf_ruleset_insert: insert the rule into the specified ruleset.
 */
void
npf_ruleset_insert(npf_ruleset_t *rlset, npf_rule_t *rl)
{
	u_int n = rlset->rs_nitems;

	KASSERT(n < rlset->rs_slots);

	LIST_INSERT_HEAD(&rlset->rs_all, rl, r_aentry);
	if (NPF_DYNAMIC_GROUP_P(rl->r_attr)) {
		LIST_INSERT_HEAD(&rlset->rs_dynamic, rl, r_dentry);
	} else {
		KASSERTMSG(rl->r_parent == NULL, "cannot be dynamic rule");
		rl->r_attr &= ~NPF_RULE_DYNAMIC;
	}

	rlset->rs_rules[n] = rl;
	rlset->rs_nitems++;

	if (rl->r_skip_to < ++n) {
		rl->r_skip_to = SKIPTO_ADJ_FLAG | n;
	}
}

static npf_rule_t *
npf_ruleset_lookup(npf_ruleset_t *rlset, const char *name)
{
	npf_rule_t *rl;

	KASSERT(npf_config_locked_p());

	LIST_FOREACH(rl, &rlset->rs_dynamic, r_dentry) {
		KASSERT(NPF_DYNAMIC_GROUP_P(rl->r_attr));
		if (strncmp(rl->r_name, name, NPF_RULE_MAXNAMELEN) == 0)
			break;
	}
	return rl;
}

/*
 * npf_ruleset_add: insert dynamic rule into the (active) ruleset.
 */
int
npf_ruleset_add(npf_ruleset_t *rlset, const char *rname, npf_rule_t *rl)
{
	npf_rule_t *rg, *it, *target;
	int priocmd;

	if (!NPF_DYNAMIC_RULE_P(rl->r_attr)) {
		return EINVAL;
	}
	rg = npf_ruleset_lookup(rlset, rname);
	if (rg == NULL) {
		return ESRCH;
	}

	/* Dynamic rule - assign a unique ID and save the parent. */
	rl->r_id = ++rlset->rs_idcnt;
	rl->r_parent = rg;

	/*
	 * Rule priority: (highest) 1, 2 ... n (lowest).
	 * Negative priority indicates an operation and is reset to zero.
	 */
	if ((priocmd = rl->r_priority) < 0) {
		rl->r_priority = 0;
	}

	/*
	 * WARNING: once rg->subset or target->r_next of an *active*
	 * rule is set, then our rule becomes globally visible and active.
	 * Must issue a load fence to ensure rl->r_next visibility first.
	 */
	switch (priocmd) {
	case NPF_PRI_LAST:
	default:
		target = NULL;
		it = rg->r_subset;
		while (it && it->r_priority <= rl->r_priority) {
			target = it;
			it = it->r_next;
		}
		if (target) {
			rl->r_next = target->r_next;
			membar_producer();
			target->r_next = rl;
			break;
		}
		/* FALLTHROUGH */

	case NPF_PRI_FIRST:
		rl->r_next = rg->r_subset;
		membar_producer();
		rg->r_subset = rl;
		break;
	}

	/* Finally, add into the all-list. */
	LIST_INSERT_HEAD(&rlset->rs_all, rl, r_aentry);
	return 0;
}

static void
npf_ruleset_unlink(npf_rule_t *rl, npf_rule_t *prev)
{
	KASSERT(NPF_DYNAMIC_RULE_P(rl->r_attr));
	if (prev) {
		prev->r_next = rl->r_next;
	} else {
		npf_rule_t *rg = rl->r_parent;
		rg->r_subset = rl->r_next;
	}
	LIST_REMOVE(rl, r_aentry);
}

/*
 * npf_ruleset_remove: remove the dynamic rule given the rule ID.
 */
int
npf_ruleset_remove(npf_ruleset_t *rlset, const char *rname, uint64_t id)
{
	npf_rule_t *rg, *prev = NULL;

	if ((rg = npf_ruleset_lookup(rlset, rname)) == NULL) {
		return ESRCH;
	}
	for (npf_rule_t *rl = rg->r_subset; rl; rl = rl->r_next) {
		KASSERT(rl->r_parent == rg);
		KASSERT(NPF_DYNAMIC_RULE_P(rl->r_attr));

		/* Compare ID.  On match, remove and return. */
		if (rl->r_id == id) {
			npf_ruleset_unlink(rl, prev);
			LIST_INSERT_HEAD(&rlset->rs_gc, rl, r_aentry);
			return 0;
		}
		prev = rl;
	}
	return ENOENT;
}

/*
 * npf_ruleset_remkey: remove the dynamic rule given the rule key.
 */
int
npf_ruleset_remkey(npf_ruleset_t *rlset, const char *rname,
    const void *key, size_t len)
{
	npf_rule_t *rg, *rlast = NULL, *prev = NULL, *lastprev = NULL;

	KASSERT(len && len <= NPF_RULE_MAXKEYLEN);

	if ((rg = npf_ruleset_lookup(rlset, rname)) == NULL) {
		return ESRCH;
	}

	/* Compare the key and find the last in the list. */
	for (npf_rule_t *rl = rg->r_subset; rl; rl = rl->r_next) {
		KASSERT(rl->r_parent == rg);
		KASSERT(NPF_DYNAMIC_RULE_P(rl->r_attr));
		if (memcmp(rl->r_key, key, len) == 0) {
			lastprev = prev;
			rlast = rl;
		}
		prev = rl;
	}
	if (!rlast) {
		return ENOENT;
	}
	npf_ruleset_unlink(rlast, lastprev);
	LIST_INSERT_HEAD(&rlset->rs_gc, rlast, r_aentry);
	return 0;
}

/*
 * npf_ruleset_list: serialise and return the dynamic rules.
 */
prop_dictionary_t
npf_ruleset_list(npf_ruleset_t *rlset, const char *rname)
{
	prop_dictionary_t rgdict;
	prop_array_t rules;
	npf_rule_t *rg;

	KASSERT(npf_config_locked_p());

	if ((rg = npf_ruleset_lookup(rlset, rname)) == NULL) {
		return NULL;
	}
	if ((rgdict = prop_dictionary_create()) == NULL) {
		return NULL;
	}
	if ((rules = prop_array_create()) == NULL) {
		prop_object_release(rgdict);
		return NULL;
	}

	for (npf_rule_t *rl = rg->r_subset; rl; rl = rl->r_next) {
		prop_dictionary_t rldict;

		KASSERT(rl->r_parent == rg);
		KASSERT(NPF_DYNAMIC_RULE_P(rl->r_attr));

		rldict = prop_dictionary_create();
		if (npf_rule_export(rlset, rl, rldict)) {
			prop_object_release(rldict);
			prop_object_release(rules);
			return NULL;
		}
		prop_array_add(rules, rldict);
		prop_object_release(rldict);
	}

	if (!prop_dictionary_set(rgdict, "rules", rules)) {
		prop_object_release(rgdict);
		rgdict = NULL;
	}
	prop_object_release(rules);
	return rgdict;
}

/*
 * npf_ruleset_flush: flush the dynamic rules in the ruleset by inserting
 * them into the G/C list.
 */
int
npf_ruleset_flush(npf_ruleset_t *rlset, const char *rname)
{
	npf_rule_t *rg, *rl;

	if ((rg = npf_ruleset_lookup(rlset, rname)) == NULL) {
		return ESRCH;
	}

	rl = atomic_swap_ptr(&rg->r_subset, NULL);
	membar_producer();

	while (rl) {
		KASSERT(NPF_DYNAMIC_RULE_P(rl->r_attr));
		KASSERT(rl->r_parent == rg);

		LIST_REMOVE(rl, r_aentry);
		LIST_INSERT_HEAD(&rlset->rs_gc, rl, r_aentry);
		rl = rl->r_next;
	}
	return 0;
}

/*
 * npf_ruleset_gc: destroy the rules in G/C list.
 */
void
npf_ruleset_gc(npf_ruleset_t *rlset)
{
	npf_rule_t *rl;

	while ((rl = LIST_FIRST(&rlset->rs_gc)) != NULL) {
		LIST_REMOVE(rl, r_aentry);
		npf_rule_free(rl);
	}
}

/*
 * npf_ruleset_export: serialise and return the static rules.
 */
int
npf_ruleset_export(const npf_ruleset_t *rlset, prop_array_t rules)
{
	const u_int nitems = rlset->rs_nitems;
	int error = 0;
	u_int n = 0;

	KASSERT(npf_config_locked_p());

	while (n < nitems) {
		const npf_rule_t *rl = rlset->rs_rules[n];
		const npf_natpolicy_t *natp = rl->r_natp;
		prop_dictionary_t rldict;

		rldict = prop_dictionary_create();
		if ((error = npf_rule_export(rlset, rl, rldict)) != 0) {
			prop_object_release(rldict);
			break;
		}
		if (natp && (error = npf_nat_policyexport(natp, rldict)) != 0) {
			prop_object_release(rldict);
			break;
		}
		prop_array_add(rules, rldict);
		prop_object_release(rldict);
		n++;
	}
	return error;
}

/*
 * npf_ruleset_reload: prepare the new ruleset by scanning the active
 * ruleset and: 1) sharing the dynamic rules 2) sharing NAT policies.
 *
 * => The active (old) ruleset should be exclusively locked.
 */
void
npf_ruleset_reload(npf_ruleset_t *newset, npf_ruleset_t *oldset, bool load)
{
	npf_rule_t *rg, *rl;
	uint64_t nid = 0;

	KASSERT(npf_config_locked_p());

	/*
	 * Scan the dynamic rules and share (migrate) if needed.
	 */
	LIST_FOREACH(rg, &newset->rs_dynamic, r_dentry) {
		npf_rule_t *active_rgroup;

		/* Look for a dynamic ruleset group with such name. */
		active_rgroup = npf_ruleset_lookup(oldset, rg->r_name);
		if (active_rgroup == NULL) {
			continue;
		}

		/*
		 * ATOMICITY: Copy the head pointer of the linked-list,
		 * but do not remove the rules from the active r_subset.
		 * This is necessary because the rules are still active
		 * and therefore are accessible for inspection via the
		 * old ruleset.
		 */
		rg->r_subset = active_rgroup->r_subset;

		/*
		 * We can safely migrate to the new all-rule list and
		 * reset the parent rule, though.
		 */
		for (rl = rg->r_subset; rl; rl = rl->r_next) {
			KASSERT(NPF_DYNAMIC_RULE_P(rl->r_attr));
			LIST_REMOVE(rl, r_aentry);
			LIST_INSERT_HEAD(&newset->rs_all, rl, r_aentry);

			KASSERT(rl->r_parent == active_rgroup);
			rl->r_parent = rg;
		}
	}

	/*
	 * If performing the load of connections then NAT policies may
	 * already have translated connections associated with them and
	 * we should not share or inherit anything.
	 */
	if (load)
		return;

	/*
	 * Scan all rules in the new ruleset and share NAT policies.
	 * Also, assign a unique ID for each policy here.
	 */
	LIST_FOREACH(rl, &newset->rs_all, r_aentry) {
		npf_natpolicy_t *np;
		npf_rule_t *actrl;

		/* Does the rule have a NAT policy associated? */
		if ((np = rl->r_natp) == NULL) {
			continue;
		}

		/*
		 * First, try to share the active port map.  If this
		 * policy will be unused, npf_nat_freepolicy() will
		 * drop the reference.
		 */
		npf_ruleset_sharepm(oldset, np);

		/* Does it match with any policy in the active ruleset? */
		LIST_FOREACH(actrl, &oldset->rs_all, r_aentry) {
			if (!actrl->r_natp)
				continue;
			if ((actrl->r_attr & NPF_RULE_KEEPNAT) != 0)
				continue;
			if (npf_nat_cmppolicy(actrl->r_natp, np))
				break;
		}
		if (!actrl) {
			/* No: just set the ID and continue. */
			npf_nat_setid(np, ++nid);
			continue;
		}

		/* Yes: inherit the matching NAT policy. */
		rl->r_natp = actrl->r_natp;
		npf_nat_setid(rl->r_natp, ++nid);

		/*
		 * Finally, mark the active rule to not destroy its NAT
		 * policy later as we inherited it (but the rule must be
		 * kept active for now).  Destroy the new/unused policy.
		 */
		actrl->r_attr |= NPF_RULE_KEEPNAT;
		npf_nat_freepolicy(np);
	}

	/* Inherit the ID counter. */
	newset->rs_idcnt = oldset->rs_idcnt;
}

/*
 * npf_ruleset_sharepm: attempt to share the active NAT portmap.
 */
npf_rule_t *
npf_ruleset_sharepm(npf_ruleset_t *rlset, npf_natpolicy_t *mnp)
{
	npf_natpolicy_t *np;
	npf_rule_t *rl;

	/*
	 * Scan the NAT policies in the ruleset and match with the
	 * given policy based on the translation IP address.  If they
	 * match - adjust the given NAT policy to use the active NAT
	 * portmap.  In such case the reference on the old portmap is
	 * dropped and acquired on the active one.
	 */
	LIST_FOREACH(rl, &rlset->rs_all, r_aentry) {
		np = rl->r_natp;
		if (np == NULL || np == mnp)
			continue;
		if (npf_nat_sharepm(np, mnp))
			break;
	}
	return rl;
}

npf_natpolicy_t *
npf_ruleset_findnat(npf_ruleset_t *rlset, uint64_t id)
{
	npf_rule_t *rl;

	LIST_FOREACH(rl, &rlset->rs_all, r_aentry) {
		npf_natpolicy_t *np = rl->r_natp;
		if (np && npf_nat_getid(np) == id) {
			return np;
		}
	}
	return NULL;
}

/*
 * npf_ruleset_freealg: inspect the ruleset and disassociate specified
 * ALG from all NAT entries using it.
 */
void
npf_ruleset_freealg(npf_ruleset_t *rlset, npf_alg_t *alg)
{
	npf_rule_t *rl;
	npf_natpolicy_t *np;

	LIST_FOREACH(rl, &rlset->rs_all, r_aentry) {
		if ((np = rl->r_natp) != NULL) {
			npf_nat_freealg(np, alg);
		}
	}
}

/*
 * npf_rule_alloc: allocate a rule and initialise it.
 */
npf_rule_t *
npf_rule_alloc(prop_dictionary_t rldict)
{
	npf_rule_t *rl;
	const char *rname;
	prop_data_t d;

	/* Allocate a rule structure. */
	rl = kmem_zalloc(sizeof(npf_rule_t), KM_SLEEP);
	rl->r_natp = NULL;

	/* Name (optional) */
	if (prop_dictionary_get_cstring_nocopy(rldict, "name", &rname)) {
		strlcpy(rl->r_name, rname, NPF_RULE_MAXNAMELEN);
	} else {
		rl->r_name[0] = '\0';
	}

	/* Attributes, priority and interface ID (optional). */
	prop_dictionary_get_uint32(rldict, "attr", &rl->r_attr);
	rl->r_attr &= ~NPF_RULE_PRIVMASK;

	if (NPF_DYNAMIC_RULE_P(rl->r_attr)) {
		/* Priority of the dynamic rule. */
		prop_dictionary_get_int32(rldict, "prio", &rl->r_priority);
	} else {
		/* The skip-to index.  No need to validate it. */
		prop_dictionary_get_uint32(rldict, "skip-to", &rl->r_skip_to);
	}

	/* Interface name; register and get the npf-if-id. */
	if (prop_dictionary_get_cstring_nocopy(rldict, "ifname", &rname)) {
		if ((rl->r_ifid = npf_ifmap_register(rname)) == 0) {
			kmem_free(rl, sizeof(npf_rule_t));
			return NULL;
		}
	} else {
		rl->r_ifid = 0;
	}

	/* Key (optional). */
	prop_object_t obj = prop_dictionary_get(rldict, "key");
	const void *key = prop_data_data_nocopy(obj);

	if (key) {
		size_t len = prop_data_size(obj);
		if (len > NPF_RULE_MAXKEYLEN) {
			kmem_free(rl, sizeof(npf_rule_t));
			return NULL;
		}
		memcpy(rl->r_key, key, len);
	}

	if ((d = prop_dictionary_get(rldict, "info")) != NULL) {
		rl->r_info = prop_data_copy(d);
	}
	return rl;
}

static int
npf_rule_export(const npf_ruleset_t *rlset, const npf_rule_t *rl,
    prop_dictionary_t rldict)
{
	u_int skip_to = 0;
	prop_data_t d;

	prop_dictionary_set_uint32(rldict, "attr", rl->r_attr);
	prop_dictionary_set_int32(rldict, "prio", rl->r_priority);
	if ((rl->r_skip_to & SKIPTO_ADJ_FLAG) == 0) {
		skip_to = rl->r_skip_to & SKIPTO_MASK;
	}
	prop_dictionary_set_uint32(rldict, "skip-to", skip_to);
	prop_dictionary_set_int32(rldict, "code-type", rl->r_type);
	if (rl->r_code) {
		d = prop_data_create_data(rl->r_code, rl->r_clen);
		prop_dictionary_set_and_rel(rldict, "code", d);
	}

	if (rl->r_ifid) {
		const char *ifname = npf_ifmap_getname(rl->r_ifid);
		prop_dictionary_set_cstring(rldict, "ifname", ifname);
	}
	prop_dictionary_set_uint64(rldict, "id", rl->r_id);

	if (rl->r_name[0]) {
		prop_dictionary_set_cstring(rldict, "name", rl->r_name);
	}
	if (NPF_DYNAMIC_RULE_P(rl->r_attr)) {
		d = prop_data_create_data(rl->r_key, NPF_RULE_MAXKEYLEN);
		prop_dictionary_set_and_rel(rldict, "key", d);
	}
	if (rl->r_info) {
		prop_dictionary_set(rldict, "info", rl->r_info);
	}
	return 0;
}

/*
 * npf_rule_setcode: assign filter code to the rule.
 *
 * => The code must be validated by the caller.
 * => JIT compilation may be performed here.
 */
void
npf_rule_setcode(npf_rule_t *rl, const int type, void *code, size_t size)
{
	KASSERT(type == NPF_CODE_BPF);

	rl->r_type = type;
	rl->r_code = code;
	rl->r_clen = size;
	rl->r_jcode = npf_bpf_compile(code, size);
}

/*
 * npf_rule_setrproc: assign a rule procedure and hold a reference on it.
 */
void
npf_rule_setrproc(npf_rule_t *rl, npf_rproc_t *rp)
{
	npf_rproc_acquire(rp);
	rl->r_rproc = rp;
}

/*
 * npf_rule_free: free the specified rule.
 */
void
npf_rule_free(npf_rule_t *rl)
{
	npf_natpolicy_t *np = rl->r_natp;
	npf_rproc_t *rp = rl->r_rproc;

	if (np && (rl->r_attr & NPF_RULE_KEEPNAT) == 0) {
		/* Free NAT policy. */
		npf_nat_freepolicy(np);
	}
	if (rp) {
		/* Release rule procedure. */
		npf_rproc_release(rp);
	}
	if (rl->r_code) {
		/* Free byte-code. */
		kmem_free(rl->r_code, rl->r_clen);
	}
	if (rl->r_jcode) {
		/* Free JIT code. */
		bpf_jit_freecode(rl->r_jcode);
	}
	if (rl->r_info) {
		prop_object_release(rl->r_info);
	}
	kmem_free(rl, sizeof(npf_rule_t));
}

/*
 * npf_rule_getid: return the unique ID of a rule.
 * npf_rule_getrproc: acquire a reference and return rule procedure, if any.
 * npf_rule_getnat: get NAT policy assigned to the rule.
 */

uint64_t
npf_rule_getid(const npf_rule_t *rl)
{
	KASSERT(NPF_DYNAMIC_RULE_P(rl->r_attr));
	return rl->r_id;
}

npf_rproc_t *
npf_rule_getrproc(const npf_rule_t *rl)
{
	npf_rproc_t *rp = rl->r_rproc;

	if (rp) {
		npf_rproc_acquire(rp);
	}
	return rp;
}

npf_natpolicy_t *
npf_rule_getnat(const npf_rule_t *rl)
{
	return rl->r_natp;
}

/*
 * npf_rule_setnat: assign NAT policy to the rule and insert into the
 * NAT policy list in the ruleset.
 */
void
npf_rule_setnat(npf_rule_t *rl, npf_natpolicy_t *np)
{
	KASSERT(rl->r_natp == NULL);
	rl->r_natp = np;
}

/*
 * npf_rule_inspect: match the interface, direction and run the filter code.
 * Returns true if rule matches and false otherwise.
 */
static inline bool
npf_rule_inspect(const npf_rule_t *rl, bpf_args_t *bc_args,
    const int di_mask, const u_int ifid)
{
	/* Match the interface. */
	if (rl->r_ifid && rl->r_ifid != ifid) {
		return false;
	}

	/* Match the direction. */
	if ((rl->r_attr & NPF_RULE_DIMASK) != NPF_RULE_DIMASK) {
		if ((rl->r_attr & di_mask) == 0)
			return false;
	}

	/* Any code? */
	if (!rl->r_code) {
		KASSERT(rl->r_jcode == NULL);
		return true;
	}
	KASSERT(rl->r_type == NPF_CODE_BPF);
	return npf_bpf_filter(bc_args, rl->r_code, rl->r_jcode) != 0;
}

/*
 * npf_rule_reinspect: re-inspect the dynamic rule by iterating its list.
 * This is only for the dynamic rules.  Subrules cannot have nested rules.
 */
static inline npf_rule_t *
npf_rule_reinspect(const npf_rule_t *rg, bpf_args_t *bc_args,
    const int di_mask, const u_int ifid)
{
	npf_rule_t *final_rl = NULL, *rl;

	KASSERT(NPF_DYNAMIC_GROUP_P(rg->r_attr));

	for (rl = rg->r_subset; rl; rl = rl->r_next) {
		KASSERT(!final_rl || rl->r_priority >= final_rl->r_priority);
		if (!npf_rule_inspect(rl, bc_args, di_mask, ifid)) {
			continue;
		}
		if (rl->r_attr & NPF_RULE_FINAL) {
			return rl;
		}
		final_rl = rl;
	}
	return final_rl;
}

/*
 * npf_ruleset_inspect: inspect the packet against the given ruleset.
 *
 * Loop through the rules in the set and run the byte-code of each rule
 * against the packet (nbuf chain).  If sub-ruleset is found, inspect it.
 */
npf_rule_t *
npf_ruleset_inspect(npf_cache_t *npc, const npf_ruleset_t *rlset,
    const int di, const int layer)
{
	nbuf_t *nbuf = npc->npc_nbuf;
	const int di_mask = (di & PFIL_IN) ? NPF_RULE_IN : NPF_RULE_OUT;
	const u_int nitems = rlset->rs_nitems;
	const u_int ifid = nbuf->nb_ifid;
	npf_rule_t *final_rl = NULL;
	bpf_args_t bc_args;
	u_int n = 0;

	KASSERT(((di & PFIL_IN) != 0) ^ ((di & PFIL_OUT) != 0));

	/*
	 * Prepare the external memory store and the arguments for
	 * the BPF programs to be executed.
	 */
	uint32_t bc_words[NPF_BPF_NWORDS];
	npf_bpf_prepare(npc, &bc_args, bc_words);

	while (n < nitems) {
		npf_rule_t *rl = rlset->rs_rules[n];
		const u_int skip_to = rl->r_skip_to & SKIPTO_MASK;
		const uint32_t attr = rl->r_attr;

		KASSERT(!nbuf_flag_p(nbuf, NBUF_DATAREF_RESET));
		KASSERT(n < skip_to);

		/* Group is a barrier: return a matching if found any. */
		if ((attr & NPF_RULE_GROUP) != 0 && final_rl) {
			break;
		}

		/* Main inspection of the rule. */
		if (!npf_rule_inspect(rl, &bc_args, di_mask, ifid)) {
			n = skip_to;
			continue;
		}

		if (NPF_DYNAMIC_GROUP_P(attr)) {
			/*
			 * If this is a dynamic rule, re-inspect the subrules.
			 * If it has any matching rule, then it is final.
			 */
			rl = npf_rule_reinspect(rl, &bc_args, di_mask, ifid);
			if (rl != NULL) {
				final_rl = rl;
				break;
			}
		} else if ((attr & NPF_RULE_GROUP) == 0) {
			/*
			 * Groups themselves are not matching.
			 */
			final_rl = rl;
		}

		/* Set the matching rule and check for "final". */
		if (attr & NPF_RULE_FINAL) {
			break;
		}
		n++;
	}

	KASSERT(!nbuf_flag_p(nbuf, NBUF_DATAREF_RESET));
	return final_rl;
}

/*
 * npf_rule_conclude: return decision and the flags for conclusion.
 *
 * => Returns ENETUNREACH if "block" and 0 if "pass".
 */
int
npf_rule_conclude(const npf_rule_t *rl, int *retfl)
{
	/* If not passing - drop the packet. */
	*retfl = rl->r_attr;
	return (rl->r_attr & NPF_RULE_PASS) ? 0 : ENETUNREACH;
}


#if defined(DDB) || defined(_NPF_TESTING)

void
npf_ruleset_dump(const char *name)
{
	npf_ruleset_t *rlset = npf_config_ruleset();
	npf_rule_t *rg, *rl;

	LIST_FOREACH(rg, &rlset->rs_dynamic, r_dentry) {
		printf("ruleset '%s':\n", rg->r_name);
		for (rl = rg->r_subset; rl; rl = rl->r_next) {
			printf("\tid %"PRIu64", key: ", rl->r_id);
			for (u_int i = 0; i < NPF_RULE_MAXKEYLEN; i++)
				printf("%x", rl->r_key[i]);
			printf("\n");
		}
	}
}

#endif

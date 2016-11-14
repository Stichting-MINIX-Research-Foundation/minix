/* $NetBSD: ppath.c,v 1.4 2012/12/29 20:08:23 christos Exp $ */

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by David Young <dyoung@NetBSD.org>.
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
__RCSID("$NetBSD: ppath.c,v 1.4 2012/12/29 20:08:23 christos Exp $");

#ifdef _KERNEL
#include <sys/systm.h>
#endif
#include <ppath/ppath.h>
#include <ppath/ppath_impl.h>

enum _ppath_type {
	  PPATH_T_IDX = 0
	, PPATH_T_KEY = 1
};

typedef enum _ppath_type ppath_type_t;

struct _ppath_component {
	unsigned int	pc_refcnt;
	ppath_type_t	pc_type;
	union {
		char *u_key;
		unsigned int u_idx;
	} pc_u;
#define pc_key pc_u.u_key
#define pc_idx pc_u.u_idx
};

struct _ppath {
	unsigned int	p_refcnt;
	unsigned int	p_len;
	ppath_component_t *p_cmpt[PPATH_MAX_COMPONENTS];
};

static int ppath_copydel_object_of_type(prop_object_t, prop_object_t *,
    const ppath_t *, prop_type_t);
static int ppath_copyset_object_helper(prop_object_t, prop_object_t *,
    const ppath_t *, const prop_object_t);

static void
ppath_strfree(char *s)
{
	size_t size = strlen(s) + 1;

	ppath_free(s, size);
}

static char *
ppath_strdup(const char *s)
{
	size_t size = strlen(s) + 1;
	char *p;

	if ((p = ppath_alloc(size)) == NULL)
		return NULL;

	return strcpy(p, s);
}

int
ppath_component_idx(const ppath_component_t *pc)
{
	if (pc->pc_type != PPATH_T_IDX)
		return -1;
	return pc->pc_idx;
}

const char *
ppath_component_key(const ppath_component_t *pc)
{
	if (pc->pc_type != PPATH_T_KEY)
		return NULL;
	return pc->pc_key;
}

ppath_component_t *
ppath_idx(unsigned int idx)
{
	ppath_component_t *pc;

	if ((pc = ppath_alloc(sizeof(*pc))) == NULL)
		return NULL;
	pc->pc_idx = idx;
	pc->pc_type = PPATH_T_IDX;
	pc->pc_refcnt = 1;
	ppath_component_extant_inc();
	return pc;
}

ppath_component_t *
ppath_key(const char *key)
{
	ppath_component_t *pc;

	if ((pc = ppath_alloc(sizeof(*pc))) == NULL)
		return NULL;

	if ((pc->pc_key = ppath_strdup(key)) == NULL) {
		ppath_free(pc, sizeof(*pc));
		return NULL;
	}
	pc->pc_type = PPATH_T_KEY;
	pc->pc_refcnt = 1;
	ppath_component_extant_inc();
	return pc;
}

ppath_component_t *
ppath_component_retain(ppath_component_t *pc)
{
	ppath_assert(pc->pc_refcnt != 0);
	pc->pc_refcnt++;

	return pc;
}

void
ppath_component_release(ppath_component_t *pc)
{
	ppath_assert(pc->pc_refcnt != 0);

	if (--pc->pc_refcnt != 0)
		return;
	if (pc->pc_type == PPATH_T_KEY)
		ppath_strfree(pc->pc_key);
	ppath_component_extant_dec();
	ppath_free(pc, sizeof(*pc));
}

ppath_t *
ppath_create(void)
{
	ppath_t *p;

	if ((p = ppath_alloc(sizeof(*p))) == NULL)
		return NULL;

	p->p_refcnt = 1;
	ppath_extant_inc();

	return p;
}

ppath_t *
ppath_pop(ppath_t *p, ppath_component_t **pcp)
{
	ppath_component_t *pc;

	if (p == NULL || p->p_len == 0)
		return NULL;

	pc = p->p_cmpt[--p->p_len];

	if (pcp != NULL)
		*pcp = pc;
	else
		ppath_component_release(pc);

	return p;
}

ppath_t *
ppath_push(ppath_t *p, ppath_component_t *pc)
{
	if (p == NULL || p->p_len == __arraycount(p->p_cmpt))
		return NULL;

	p->p_cmpt[p->p_len++] = ppath_component_retain(pc);

	return p;
}

ppath_component_t *
ppath_component_at(const ppath_t *p, unsigned int i)
{
	ppath_component_t *pc;

	if (p == NULL || i >= p->p_len)
		return NULL;
	
	pc = p->p_cmpt[i];

	return ppath_component_retain(pc);
}

unsigned int
ppath_length(const ppath_t *p)
{
	return p->p_len;
}

ppath_t *
ppath_subpath(const ppath_t *p, unsigned int first, unsigned int exclast)
{
	unsigned int i;
	ppath_t *np;
	ppath_component_t *pc;

	if (p == NULL || (np = ppath_create()) == NULL)
		return NULL;

	for (i = first; i < exclast; i++) {
		if ((pc = ppath_component_at(p, i)) == NULL)
			break;
		ppath_push(np, pc);
		ppath_component_release(pc);
	}
	return np;
}

ppath_t *
ppath_push_idx(ppath_t *p0, unsigned int idx)
{
	ppath_component_t *pc;
	ppath_t *p;

	if ((pc = ppath_idx(idx)) == NULL)
		return NULL;

	p = ppath_push(p0, pc);
	ppath_component_release(pc);
	return p;
}

ppath_t *
ppath_push_key(ppath_t *p0, const char *key)
{
	ppath_component_t *pc;
	ppath_t *p;

	if ((pc = ppath_key(key)) == NULL)
		return NULL;

	p = ppath_push(p0, pc);
	ppath_component_release(pc);
	return p;
}

ppath_t *
ppath_replace_idx(ppath_t *p, unsigned int idx)
{
	ppath_component_t *pc0, *pc;

	if (p == NULL || p->p_len == 0)
		return NULL;
	
	pc0 = p->p_cmpt[p->p_len - 1];

	if (pc0->pc_type != PPATH_T_IDX)
		return NULL;

	if ((pc = ppath_idx(idx)) == NULL)
		return NULL;
	
	p->p_cmpt[p->p_len - 1] = pc;
	ppath_component_release(pc0);

	return p;
}

ppath_t *
ppath_replace_key(ppath_t *p, const char *key)
{
	ppath_component_t *pc0, *pc;

	if (p == NULL || p->p_len == 0)
		return NULL;
	
	pc0 = p->p_cmpt[p->p_len - 1];

	if (pc0->pc_type != PPATH_T_KEY)
		return NULL;

	if ((pc = ppath_key(key)) == NULL)
		return NULL;
	
	p->p_cmpt[p->p_len - 1] = pc;
	ppath_component_release(pc0);

	return p;
}

ppath_t *
ppath_copy(const ppath_t *p0)
{
	ppath_t *p;
	unsigned int i;

	if ((p = ppath_create()) == NULL)
		return NULL;

	for (i = 0; i < p0->p_len; i++)
		p->p_cmpt[i] = ppath_component_retain(p0->p_cmpt[i]);
	p->p_len = p0->p_len;
	return p;
}

ppath_t *
ppath_retain(ppath_t *p)
{
	assert(p->p_refcnt != 0);

	p->p_refcnt++;

	return p;
}

void
ppath_release(ppath_t *p)
{
	unsigned int i;

	assert(p->p_refcnt != 0);

	if (--p->p_refcnt != 0)
		return;

	for (i = 0; i < p->p_len; i++)
		ppath_component_release(p->p_cmpt[i]);

	ppath_extant_dec();
	ppath_free(p, sizeof(*p));
}

static prop_object_t
ppath_lookup_helper(prop_object_t o0, const ppath_t *p, prop_object_t *pop,
    ppath_component_t **pcp, unsigned int *ip)
{
	unsigned int i;
	prop_object_t o, po;
	ppath_type_t t;
	ppath_component_t *pc = NULL;

	for (po = NULL, o = o0, i = 0; i < p->p_len && o != NULL; i++) {
		pc = p->p_cmpt[i];
		t = pc->pc_type;
		switch (prop_object_type(o)) {
		case PROP_TYPE_ARRAY:
			po = o;
			o = (t == PPATH_T_IDX)
			    ? prop_array_get(o, pc->pc_idx)
			    : NULL;
			break;
		case PROP_TYPE_DICTIONARY:
			po = o;
			o = (t == PPATH_T_KEY)
			    ? prop_dictionary_get(o, pc->pc_key)
			    : NULL;
			break;
		default:
			o = NULL;
			break;
		}
	}
	if (pop != NULL)
		*pop = po;
	if (pcp != NULL)
		*pcp = pc;
	if (ip != NULL)
		*ip = i;
	return o;
}

prop_object_t
ppath_lookup(prop_object_t o, const ppath_t *p)
{
	return ppath_lookup_helper(o, p, NULL, NULL, NULL);
}

static int
ppath_create_object_and_release(prop_object_t o, const ppath_t *p,
    prop_object_t v)
{
	int rc;

	rc = ppath_create_object(o, p, v);
	prop_object_release(v);
	return rc;
}

int
ppath_create_string(prop_object_t o, const ppath_t *p, const char *s)
{
	return ppath_create_object_and_release(o, p,
	    prop_string_create_cstring(s));
}

int
ppath_create_data(prop_object_t o, const ppath_t *p,
    const void *data, size_t size)
{
	return ppath_create_object_and_release(o, p,
	    prop_data_create_data(data, size));
}

int
ppath_create_uint64(prop_object_t o, const ppath_t *p, uint64_t u)
{
	return ppath_create_object_and_release(o, p,
	    prop_number_create_unsigned_integer(u));
}

int
ppath_create_int64(prop_object_t o, const ppath_t *p, int64_t i)
{
	return ppath_create_object_and_release(o, p,
	    prop_number_create_integer(i));
}

int
ppath_create_bool(prop_object_t o, const ppath_t *p, bool b)
{
	return ppath_create_object_and_release(o, p, prop_bool_create(b));
}

int
ppath_create_object(prop_object_t o, const ppath_t *p, prop_object_t v)
{
	unsigned int i;
	ppath_component_t *pc;
	prop_object_t po;

	if (ppath_lookup_helper(o, p, &po, &pc, &i) != NULL)
		return EEXIST;
	
	if (i != ppath_length(p))
		return ENOENT;

	switch (pc->pc_type) {
	case PPATH_T_IDX:
		return prop_array_set(po, pc->pc_idx, v) ? 0 : ENOMEM;
	case PPATH_T_KEY:
		return prop_dictionary_set(po, pc->pc_key, v) ? 0 : ENOMEM;
	default:
		return ENOENT;
	}
}

int
ppath_set_object(prop_object_t o, const ppath_t *p, prop_object_t v)
{
	ppath_component_t *pc;
	prop_object_t po;

	if (ppath_lookup_helper(o, p, &po, &pc, NULL) == NULL)
		return ENOENT;

	switch (pc->pc_type) {
	case PPATH_T_IDX:
		return prop_array_set(po, pc->pc_idx, v) ? 0 : ENOMEM;
	case PPATH_T_KEY:
		return prop_dictionary_set(po, pc->pc_key, v) ? 0 : ENOMEM;
	default:
		return ENOENT;
	}
}

static int
ppath_set_object_and_release(prop_object_t o, const ppath_t *p, prop_object_t v)
{
	prop_object_t ov;
	int rc;

	if (v == NULL)
		return ENOMEM;

	if ((ov = ppath_lookup_helper(o, p, NULL, NULL, NULL)) == NULL)
		return ENOENT;

	if (prop_object_type(ov) != prop_object_type(v))
		return EFTYPE;

	rc = ppath_set_object(o, p, v);
	prop_object_release(v);
	return rc;
}

int
ppath_get_object(prop_object_t o, const ppath_t *p, prop_object_t *vp)
{
	prop_object_t v;

	if ((v = ppath_lookup_helper(o, p, NULL, NULL, NULL)) == NULL)
		return ENOENT;

	if (vp != NULL)
		*vp = v;
	return 0;
}

static int
ppath_get_object_of_type(prop_object_t o, const ppath_t *p, prop_object_t *vp,
    prop_type_t t)
{
	int rc;

	if ((rc = ppath_get_object(o, p, vp)) != 0)
		return rc;

	return (prop_object_type(*vp) == t) ? 0 : EFTYPE;
}

int
ppath_delete_object(prop_object_t o, const ppath_t *p)
{
	ppath_component_t *pc;
	prop_object_t po;

	if (ppath_lookup_helper(o, p, &po, &pc, NULL) == NULL)
		return ENOENT;

	switch (pc->pc_type) {
	case PPATH_T_IDX:
		prop_array_remove(po, pc->pc_idx);
		return 0;
	case PPATH_T_KEY:
		prop_dictionary_remove(po, pc->pc_key);
		return 0;
	default:
		return ENOENT;
	}
}

static int
ppath_delete_object_of_type(prop_object_t o, const ppath_t *p, prop_type_t t)
{
	prop_object_t v;

	if ((v = ppath_lookup_helper(o, p, NULL, NULL, NULL)) == NULL)
		return ENOENT;

	if (prop_object_type(v) != t)
		return EFTYPE;

	return ppath_delete_object(o, p);
}

int
ppath_copydel_string(prop_object_t o, prop_object_t *op, const ppath_t *p)
{
	return ppath_copydel_object_of_type(o, op, p, PROP_TYPE_STRING);
}

int
ppath_copydel_data(prop_object_t o, prop_object_t *op, const ppath_t *p)
{
	return ppath_copydel_object_of_type(o, op, p, PROP_TYPE_DATA);
}

int
ppath_copydel_uint64(prop_object_t o, prop_object_t *op, const ppath_t *p)
{
	return ppath_copydel_object_of_type(o, op, p, PROP_TYPE_NUMBER);
}

int
ppath_copydel_int64(prop_object_t o, prop_object_t *op, const ppath_t *p)
{
	return ppath_copydel_object_of_type(o, op, p, PROP_TYPE_NUMBER);
}

int
ppath_copydel_bool(prop_object_t o, prop_object_t *op, const ppath_t *p)
{
	return ppath_copydel_object_of_type(o, op, p, PROP_TYPE_BOOL);
}

static int
ppath_copydel_object_of_type(prop_object_t o, prop_object_t *op,
    const ppath_t *p, prop_type_t t)
{
	prop_object_t v;

	if ((v = ppath_lookup_helper(o, p, NULL, NULL, NULL)) == NULL)
		return ENOENT;

	if (prop_object_type(v) != t)
		return EFTYPE;

	return ppath_copydel_object(o, op, p);
}

int
ppath_copydel_object(prop_object_t o, prop_object_t *op, const ppath_t *p)
{
	return ppath_copyset_object_helper(o, op, p, NULL);
}

int
ppath_copyset_object(prop_object_t o, prop_object_t *op, const ppath_t *p,
    const prop_object_t v)
{
	ppath_assert(v != NULL);
	return ppath_copyset_object_helper(o, op, p, v);
}

static int
ppath_copyset_object_helper(prop_object_t o, prop_object_t *op,
    const ppath_t *p0, const prop_object_t v0)
{
	bool copy, success;
	ppath_component_t *npc, *pc;
	ppath_t *cp, *p;
	prop_object_t npo = NULL, po, v;

	for (cp = p = ppath_copy(p0), v = v0;
	     p != NULL;
	     p = ppath_pop(p, NULL), v = npo) {

		if (ppath_lookup_helper(o, p, &po, &pc, NULL) == NULL)
			return ENOENT;

		if (pc == NULL)
			break;

		if (ppath_lookup_helper(*op, p, &npo, &npc, NULL) == NULL)
			npo = po;

		copy = (npo == po);

		switch (pc->pc_type) {
		case PPATH_T_IDX:
			if (copy && (npo = prop_array_copy_mutable(po)) == NULL)
				return ENOMEM;
			success = (v == NULL)
			    ? (prop_array_remove(npo, pc->pc_idx), true)
			    : prop_array_set(npo, pc->pc_idx, v);
			break;
		case PPATH_T_KEY:
			if (copy &&
			    (npo = prop_dictionary_copy_mutable(po)) == NULL)
				return ENOMEM;
			success = (v == NULL)
			    ? (prop_dictionary_remove(npo, pc->pc_key), true)
			    : prop_dictionary_set(npo, pc->pc_key, v);
			break;
		default:
			return ENOENT;
		}
		if (!success) {
			if (copy)
				prop_object_release(npo);
			return ENOMEM;
		}
	}

	if (cp == NULL)
		return ENOMEM;

	ppath_release(cp);

	if (op != NULL && npo != NULL)
		*op = npo;
	else if (npo != NULL)
		prop_object_release(npo);

	return 0;
}

static int
ppath_copyset_object_and_release(prop_object_t o, prop_object_t *op,
    const ppath_t *p, prop_object_t v)
{
	prop_object_t ov;
	int rc;

	if (v == NULL)
		return ENOMEM;

	if ((ov = ppath_lookup_helper(o, p, NULL, NULL, NULL)) == NULL)
		return ENOENT;

	if (prop_object_type(ov) != prop_object_type(v))
		return EFTYPE;

	rc = ppath_copyset_object(o, op, p, v);
	prop_object_release(v);
	return rc;
}

int
ppath_copyset_bool(prop_object_t o, prop_object_t *op, const ppath_t *p, bool b)
{
	return ppath_copyset_object_and_release(o, op, p, prop_bool_create(b));
}

int
ppath_set_bool(prop_object_t o, const ppath_t *p, bool b)
{
	return ppath_set_object_and_release(o, p, prop_bool_create(b));
}

int
ppath_get_bool(prop_object_t o, const ppath_t *p, bool *bp)
{
	prop_object_t v;
	int rc;

	if ((rc = ppath_get_object_of_type(o, p, &v, PROP_TYPE_BOOL)) != 0)
		return rc;

	if (bp != NULL)
		*bp = prop_bool_true(v);

	return 0;
}

int
ppath_delete_bool(prop_object_t o, const ppath_t *p)
{
	return ppath_delete_object_of_type(o, p, PROP_TYPE_BOOL);
}

int
ppath_copyset_data(prop_object_t o, prop_object_t *op, const ppath_t *p,
    const void *data, size_t size)
{
	return ppath_copyset_object_and_release(o, op, p,
	    prop_data_create_data(data, size));
}

int
ppath_set_data(prop_object_t o, const ppath_t *p, const void *data, size_t size)
{
	return ppath_set_object_and_release(o, p,
	    prop_data_create_data(data, size));
}

int
ppath_get_data(prop_object_t o, const ppath_t *p, const void **datap,
    size_t *sizep)
{
	prop_object_t v;
	int rc;

	if ((rc = ppath_get_object_of_type(o, p, &v, PROP_TYPE_DATA)) != 0)
		return rc;

	if (datap != NULL)
		*datap = prop_data_data_nocopy(v);
	if (sizep != NULL)
		*sizep = prop_data_size(v);

	return 0;
}

int
ppath_dup_data(prop_object_t o, const ppath_t *p, void **datap, size_t *sizep)
{
	prop_object_t v;
	int rc;

	if ((rc = ppath_get_object_of_type(o, p, &v, PROP_TYPE_DATA)) != 0)
		return rc;

	if (datap != NULL)
		*datap = prop_data_data(v);
	if (sizep != NULL)
		*sizep = prop_data_size(v);

	return 0;
}

int
ppath_delete_data(prop_object_t o, const ppath_t *p)
{
	return ppath_delete_object_of_type(o, p, PROP_TYPE_DATA);
}

int
ppath_copyset_int64(prop_object_t o, prop_object_t *op, const ppath_t *p,
    int64_t i)
{
	return ppath_copyset_object_and_release(o, op, p,
	    prop_number_create_integer(i));
}

int
ppath_set_int64(prop_object_t o, const ppath_t *p, int64_t i)
{
	return ppath_set_object_and_release(o, p,
	    prop_number_create_integer(i));
}

int
ppath_get_int64(prop_object_t o, const ppath_t *p, int64_t *ip)
{
	prop_object_t v;
	int rc;

	if ((rc = ppath_get_object_of_type(o, p, &v, PROP_TYPE_DATA)) != 0)
		return rc;

	if (prop_number_unsigned(v))
		return EFTYPE;

	if (ip != NULL)
		*ip = prop_number_integer_value(v);

	return 0;
}

int
ppath_delete_int64(prop_object_t o, const ppath_t *p)
{
	return ppath_delete_object_of_type(o, p, PROP_TYPE_NUMBER);
}

int
ppath_copyset_string(prop_object_t o, prop_object_t *op, const ppath_t *p,
    const char *s)
{
	return ppath_copyset_object_and_release(o, op, p,
	    prop_string_create_cstring(s));
}

int
ppath_set_string(prop_object_t o, const ppath_t *p, const char *s)
{
	return ppath_set_object_and_release(o, p,
	    prop_string_create_cstring(s));
}

int
ppath_get_string(prop_object_t o, const ppath_t *p, const char **sp)
{
	int rc;
	prop_object_t v;

	if ((rc = ppath_get_object_of_type(o, p, &v, PROP_TYPE_STRING)) != 0)
		return rc;

	if (sp != NULL)
		*sp = prop_string_cstring_nocopy(v);

	return 0;
}

int
ppath_dup_string(prop_object_t o, const ppath_t *p, char **sp)
{
	int rc;
	prop_object_t v;

	if ((rc = ppath_get_object_of_type(o, p, &v, PROP_TYPE_STRING)) != 0)
		return rc;

	if (sp != NULL)
		*sp = prop_string_cstring(v);

	return 0;
}

int
ppath_delete_string(prop_object_t o, const ppath_t *p)
{
	return ppath_delete_object_of_type(o, p, PROP_TYPE_STRING);
}

int
ppath_copyset_uint64(prop_object_t o, prop_object_t *op, const ppath_t *p,
    uint64_t u)
{
	return ppath_copyset_object_and_release(o, op, p,
	    prop_number_create_unsigned_integer(u));
}

int
ppath_set_uint64(prop_object_t o, const ppath_t *p, uint64_t u)
{
	return ppath_set_object_and_release(o, p,
	    prop_number_create_unsigned_integer(u));
}

int
ppath_get_uint64(prop_object_t o, const ppath_t *p, uint64_t *up)
{
	prop_object_t v;
	int rc;

	if ((rc = ppath_get_object_of_type(o, p, &v, PROP_TYPE_DATA)) != 0)
		return rc;

	if (!prop_number_unsigned(v))
		return EFTYPE;

	if (up != NULL)
		*up = prop_number_unsigned_integer_value(v);

	return 0;
}

int
ppath_delete_uint64(prop_object_t o, const ppath_t *p)
{
	return ppath_delete_object_of_type(o, p, PROP_TYPE_NUMBER);
}

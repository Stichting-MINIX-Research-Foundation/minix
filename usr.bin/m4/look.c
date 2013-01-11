/*	$NetBSD: look.c,v 1.12 2012/03/20 20:34:58 matt Exp $	*/
/*	$OpenBSD: look.c,v 1.21 2009/10/14 17:23:17 sthen Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ozan Yigit at York University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * look.c
 * Facility: m4 macro processor
 * by: oz
 */
#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif
#include <sys/cdefs.h>
__RCSID("$NetBSD: look.c,v 1.12 2012/03/20 20:34:58 matt Exp $");
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <ohash.h>
#include "mdef.h"
#include "stdd.h"
#include "extern.h"

static void *hash_alloc(size_t, void *);
static void hash_free(void *, size_t, void *);
static void *element_alloc(size_t, void *);
static void setup_definition(struct macro_definition *, const char *, 
    const char *);

static struct ohash_info macro_info = {
	offsetof(struct ndblock, name),
	NULL, hash_alloc, hash_free, element_alloc };

struct ohash macros;

/* Support routines for hash tables.  */
void *
hash_alloc(size_t s, void *u UNUSED)
{
	void *storage = xalloc(s, "hash alloc");
	if (storage)
		memset(storage, 0, s);
	return storage;
}

void
hash_free(void *p, size_t s UNUSED, void *u UNUSED)
{
	free(p);
}

void *
element_alloc(size_t s, void *u UNUSED)
{
	return xalloc(s, "element alloc");
}

void
init_macros(void)
{
	ohash_init(&macros, 10, &macro_info);
}

/*
 * find name in the hash table
 */
ndptr 
lookup(const char *name)
{
	return ohash_find(&macros, ohash_qlookup(&macros, name));
}

struct macro_definition *
lookup_macro_definition(const char *name)
{
	ndptr p;

	p = ohash_find(&macros, ohash_qlookup(&macros, name));
	if (p)
		return p->d;
	else
		return NULL;
}

static void 
setup_definition(struct macro_definition *d, const char *defn, const char *name)
{
	ndptr p;

	if (strncmp(defn, BUILTIN_MARKER, sizeof(BUILTIN_MARKER)-1) == 0 &&
	    (p = macro_getbuiltin(defn+sizeof(BUILTIN_MARKER)-1)) != NULL) {
		d->type = macro_builtin_type(p);
		d->defn = xstrdup(defn+sizeof(BUILTIN_MARKER)-1);
	} else {
		if (!*defn)
			d->defn = xstrdup(null);
		else
			d->defn = xstrdup(defn);
		d->type = MACRTYPE;
	}
	if (STREQ(name, defn))
		d->type |= RECDEF;
}

static ndptr
create_entry(const char *name)
{
	const char *end = NULL;
	unsigned int i;
	ndptr n;

	i = ohash_qlookupi(&macros, name, &end);
	n = ohash_find(&macros, i);
	if (n == NULL) {
		n = ohash_create_entry(&macro_info, name, &end);
		ohash_insert(&macros, i, n);
		n->trace_flags = FLAG_NO_TRACE;
		n->builtin_type = MACRTYPE;
		n->d = NULL;
	}
	return n;
}

void
macro_define(const char *name, const char *defn)
{
	ndptr n = create_entry(name);
	if (n->d != NULL) {
		if (n->d->defn != null)
			free(n->d->defn);
	} else {
		n->d = xalloc(sizeof(struct macro_definition), NULL);
		n->d->next = NULL;
	}
	setup_definition(n->d, defn, name);
}

void
macro_pushdef(const char *name, const char *defn)
{
	ndptr n;
	struct macro_definition *d;
	
	n = create_entry(name);
	d = xalloc(sizeof(struct macro_definition), NULL);
	d->next = n->d;
	n->d = d;
	setup_definition(n->d, defn, name);
}

void
macro_undefine(const char *name)
{
	ndptr n = lookup(name);
	if (n != NULL) {
		struct macro_definition *r, *r2;

		for (r = n->d; r != NULL; r = r2) {
			r2 = r->next;
			if (r->defn != null)
				free(r->defn);
			free(r);
		}
		n->d = NULL;
	}
}

void
macro_popdef(const char *name)
{
	ndptr n = lookup(name);

	if (n != NULL) {
		struct macro_definition *r = n->d;
		if (r != NULL) {
			n->d = r->next;
			if (r->defn != null)
				free(r->defn);
			free(r);
		}
	}
}

void
macro_for_all(void (*f)(const char *, struct macro_definition *))
{
	ndptr n;
	unsigned int i;

	for (n = ohash_first(&macros, &i); n != NULL; 
	    n = ohash_next(&macros, &i))
		if (n->d != NULL)
			f(n->name, n->d);
}

void 
setup_builtin(const char *name, unsigned int type)
{
	ndptr n;
	char *name2;

	if (prefix_builtins) {
		name2 = xalloc(strlen(name)+3+1, NULL);
		memcpy(name2, "m4_", 3);
		memcpy(name2 + 3, name, strlen(name)+1);
	} else
		name2 = xstrdup(name);

	n = create_entry(name2);
	n->builtin_type = type;
	n->d = xalloc(sizeof(struct macro_definition), NULL);
	n->d->defn = name2;
	n->d->type = type;
	n->d->next = NULL;
}

void
mark_traced(const char *name, int on)
{
	ndptr p;
	unsigned int i;

	if (name == NULL) {
		if (on)
			trace_flags |= TRACE_ALL;
		else
			trace_flags &= ~TRACE_ALL;
		for (p = ohash_first(&macros, &i); p != NULL; 
		    p = ohash_next(&macros, &i))
		    	p->trace_flags = FLAG_NO_TRACE;
	} else {
		p = create_entry(name);
		p->trace_flags = on;
	}
}

ndptr 
macro_getbuiltin(const char *name)
{
	ndptr p;

	p = lookup(name);
	if (p == NULL || p->builtin_type == MACRTYPE)
		return NULL;
	else
		return p;
}


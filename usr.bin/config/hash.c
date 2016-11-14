/*	$NetBSD: hash.c,v 1.11 2014/10/29 17:14:50 christos Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratories.
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
 *
 *	from: @(#)hash.c	8.1 (Berkeley) 6/6/93
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
__RCSID("$NetBSD: hash.c,v 1.11 2014/10/29 17:14:50 christos Exp $");

#include <sys/param.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <util.h>
#include "defs.h"

/*
 * Interned strings are kept in a hash table.  By making each string
 * unique, the program can compare strings by comparing pointers.
 */
struct hashent {
	// XXXLUKEM: a SIMPLEQ might be more appropriate
	TAILQ_ENTRY(hashent) h_next;
	const char *h_names[2];		/* the string */
#define	h_name1	h_names[0]
#define	h_name2	h_names[1]
#define	h_name	h_name1
	u_int	h_hash;			/* its hash value */
	void	*h_value;		/* other values (for name=value) */
};
struct hashtab {
	size_t	ht_size;		/* size (power of 2) */
	size_t	ht_mask;		/* == ht_size - 1 */
	size_t	ht_used;		/* number of entries used */
	size_t	ht_lim;			/* when to expand */
	TAILQ_HEAD(hashenthead, hashent) *ht_tab;
};

static struct hashtab strings;

/*
 * HASHFRACTION controls ht_lim, which in turn controls the average chain
 * length.  We allow a few entries, on average, as comparing them is usually
 * cheap (the h_hash values prevent a strcmp).
 */
#define	HASHFRACTION(sz) ((sz) * 3 / 2)

static void			ht_expand(struct hashtab *);
static void			ht_init(struct hashtab *, size_t);
static inline u_int		hash(u_int, const char *);
static inline u_int		hash2(u_int, const char *, const char *);
static inline struct hashent   *newhashent(const char *, u_int);

/*
 * Initialize a new hash table.  The size must be a power of 2.
 */
static void
ht_init(struct hashtab *ht, size_t sz)
{
	u_int n;

	ht->ht_tab = emalloc(sz * sizeof (ht->ht_tab[0]));
	ht->ht_size = sz;
	ht->ht_mask = sz - 1;
	for (n = 0; n < sz; n++)
		TAILQ_INIT(&ht->ht_tab[n]);
	ht->ht_used = 0;
	ht->ht_lim = HASHFRACTION(sz);
}

/*
 * Expand an existing hash table.
 */
static void
ht_expand(struct hashtab *ht)
{
	struct hashenthead *h, *oldh;
	struct hashent *p;
	size_t n, i;

	n = ht->ht_size * 2;
	h = emalloc(n * sizeof *h);
	for (i = 0; i < n; i++)
		TAILQ_INIT(&h[i]);
	oldh = ht->ht_tab;
	n--;
	for (i = 0; i < ht->ht_size; i++) {
		while ((p = TAILQ_FIRST(&oldh[i])) != NULL) {
			TAILQ_REMOVE(&oldh[i], p, h_next);
				// XXXLUKEM: really should be TAILQ_INSERT_TAIL
			TAILQ_INSERT_HEAD(&h[p->h_hash & n], p, h_next);
		}
	}
	free(ht->ht_tab);
	ht->ht_tab = h;
	ht->ht_mask = n;
	ht->ht_size = ++n;
	ht->ht_lim = HASHFRACTION(n);
}

/*
 * Make a new hash entry, setting its h_next to NULL.
 * If the free list is not empty, use the first entry from there,
 * otherwise allocate a new entry.
 */
static inline struct hashent *
newhashent2(const char *name1, const char *name2, u_int h)
{
	struct hashent *hp;

	hp = ecalloc(1, sizeof(*hp));

	hp->h_name1 = name1;
	hp->h_name2 = name2;
	hp->h_hash = h;
	return (hp);
}

static inline struct hashent *
newhashent(const char *name, u_int h)
{
	return newhashent2(name, NULL, h);
}

static inline u_int
hv(u_int h, char c)
{
	return (h << 5) + h + (unsigned char)c;
}

/*
 * Hash a string.
 */
static inline u_int
hash(u_int h, const char *str)
{

	while (str && *str)
		h = hv(h, *str++);
	return (h);
}

#define	HASH2DELIM	' '

static inline u_int
hash2(u_int h, const char *str1, const char *str2)
{

	h = hash(h, str1);
	h = hv(h, HASH2DELIM);
	h = hash(h, str2);
	return (h);
}

void
initintern(void)
{

	ht_init(&strings, 128);
}

/*
 * Generate a single unique copy of the given string.  We expect this
 * function to be used frequently, so it should be fast.
 */
const char *
intern(const char *s)
{
	struct hashtab *ht;
	struct hashent *hp;
	struct hashenthead *hpp;
	u_int h;
	char *p;

	ht = &strings;
	h = hash2(0, s, NULL);
	hpp = &ht->ht_tab[h & ht->ht_mask];
	TAILQ_FOREACH(hp, hpp, h_next) {
		if (hp->h_hash == h && strcmp(hp->h_name, s) == 0)
			return (hp->h_name);
	}
	p = estrdup(s);
	hp = newhashent(p, h);
	TAILQ_INSERT_TAIL(hpp, hp, h_next);
	if (++ht->ht_used > ht->ht_lim)
		ht_expand(ht);
	return (p);
}

struct hashtab *
ht_new(void)
{
	struct hashtab *ht;

	ht = ecalloc(1, sizeof *ht);
	ht_init(ht, 8);
	return (ht);
}

void
ht_free(struct hashtab *ht)
{
	size_t i;
	struct hashent *hp;
	struct hashenthead *hpp;

	for (i = 0; i < ht->ht_size; i++) {
		hpp = &ht->ht_tab[i];
		while ((hp = TAILQ_FIRST(hpp)) != NULL) {
			TAILQ_REMOVE(hpp, hp, h_next);
			free(hp);
			ht->ht_used--;
		}
	}

	assert(ht->ht_used == 0);
	free(ht->ht_tab);
	free(ht);
}

/*
 * Insert and/or replace.
 */
int
ht_insrep2(struct hashtab *ht, const char *nam1, const char *nam2, void *val, int replace)
{
	struct hashent *hp;
	struct hashenthead *hpp;
	u_int h;

	h = hash2(0, nam1, nam2);
	hpp = &ht->ht_tab[h & ht->ht_mask];
	TAILQ_FOREACH(hp, hpp, h_next) {
		if (hp->h_name1 == nam1 &&
		    hp->h_name2 == nam2) {
			if (replace)
				hp->h_value = val;
			return (1);
		}
	}
	hp = newhashent2(nam1, nam2, h);
	TAILQ_INSERT_TAIL(hpp, hp, h_next);
	hp->h_value = val;
	if (++ht->ht_used > ht->ht_lim)
		ht_expand(ht);
	return (0);
}

int
ht_insrep(struct hashtab *ht, const char *nam, void *val, int replace)
{
	return ht_insrep2(ht, nam, NULL, val, replace);
}

/*
 * Remove.
 */
int
ht_remove2(struct hashtab *ht, const char *name1, const char *name2)
{
	struct hashent *hp;
	struct hashenthead *hpp;
	u_int h;

	h = hash2(0, name1, name2);
	hpp = &ht->ht_tab[h & ht->ht_mask];

	TAILQ_FOREACH(hp, hpp, h_next) {
		if (hp->h_name1 != name1 || hp->h_name2 != name2)
			continue;
		TAILQ_REMOVE(hpp, hp, h_next);

		free(hp);
		ht->ht_used--;
		return (0);
	}
	return (1);
}

int
ht_remove(struct hashtab *ht, const char *name)
{
	return ht_remove2(ht, name, NULL);
}

void *
ht_lookup2(struct hashtab *ht, const char *nam1, const char *nam2)
{
	struct hashent *hp;
	struct hashenthead *hpp;
	u_int h;

	h = hash2(0, nam1, nam2);
	hpp = &ht->ht_tab[h & ht->ht_mask];
	TAILQ_FOREACH(hp, hpp, h_next)
		if (hp->h_name == nam1)
			return (hp->h_value);
	return (NULL);
}

void *
ht_lookup(struct hashtab *ht, const char *nam)
{
	return ht_lookup2(ht, nam, NULL);
}

/*
 * first parameter to callback is the entry name from the hash table
 * second parameter is the value from the hash table
 * third argument is passed through from the "arg" parameter to ht_enumerate()
 */

int
ht_enumerate2(struct hashtab *ht, ht_callback2 cbfunc2, void *arg)
{
	struct hashent *hp;
	struct hashenthead *hpp;
	size_t i;
	int rval = 0;
	
	for (i = 0; i < ht->ht_size; i++) {
		hpp = &ht->ht_tab[i];
		TAILQ_FOREACH(hp, hpp, h_next)
			rval += (*cbfunc2)(hp->h_name1, hp->h_name2, hp->h_value, arg);
	}
	return rval;
}

int
ht_enumerate(struct hashtab *ht, ht_callback cbfunc, void *arg)
{
	struct hashent *hp;
	struct hashenthead *hpp;
	size_t i;
	int rval = 0;
	
	for (i = 0; i < ht->ht_size; i++) {
		hpp = &ht->ht_tab[i];
		TAILQ_FOREACH(hp, hpp, h_next)
			rval += (*cbfunc)(hp->h_name, hp->h_value, arg);
	}
	return rval;
}

/************************************************************/

/*
 * Type-safe wrappers.
 */

#define DEFHASH(HT, VT) \
	struct HT {						\
		struct hashtab imp;				\
	};							\
								\
	struct HT *						\
	HT##_create(void)					\
	{							\
		struct HT *tbl;					\
								\
		tbl = ecalloc(1, sizeof(*tbl));			\
		ht_init(&tbl->imp, 8);				\
		return tbl;					\
	}							\
								\
	int							\
	HT##_insert(struct HT *tbl, const char *name, struct VT *val) \
	{							\
		return ht_insert(&tbl->imp, name, val);		\
	}							\
								\
	int							\
	HT##_replace(struct HT *tbl, const char *name, struct VT *val) \
	{							\
		return ht_replace(&tbl->imp, name, val);	\
	}							\
								\
	int							\
	HT##_remove(struct HT *tbl, const char *name)		\
	{							\
		return ht_remove(&tbl->imp, name);		\
	}							\
								\
	struct VT *						\
	HT##_lookup(struct HT *tbl, const char *name)		\
	{							\
		return ht_lookup(&tbl->imp, name);		\
	}							\
								\
	struct HT##_enumcontext {				\
		int (*func)(const char *, struct VT *, void *);	\
		void *userctx;					\
	};							\
								\
	static int						\
	HT##_enumerate_thunk(const char *name, void *value, void *voidctx) \
	{							\
		struct HT##_enumcontext *ctx = voidctx;		\
								\
		return ctx->func(name, value, ctx->userctx);	\
	}							\
								\
	int							\
	HT##_enumerate(struct HT *tbl,				\
		      int (*func)(const char *, struct VT *, void *), \
		      void *userctx)				\
	{							\
		struct HT##_enumcontext ctx;			\
								\
		ctx.func = func;				\
		ctx.userctx = userctx;				\
		return ht_enumerate(&tbl->imp, HT##_enumerate_thunk, &ctx); \
	}

DEFHASH(nvhash, nvlist);
DEFHASH(dlhash, defoptlist);

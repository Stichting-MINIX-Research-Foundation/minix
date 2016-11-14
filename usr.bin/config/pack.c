/*	$NetBSD: pack.c,v 1.10 2015/09/12 19:11:13 joerg Exp $	*/

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
 *	from: @(#)pack.c	8.1 (Berkeley) 6/6/93
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
__RCSID("$NetBSD: pack.c,v 1.10 2015/09/12 19:11:13 joerg Exp $");

#include <sys/param.h>
#include <stdlib.h>
#include <string.h>
#include <util.h>
#include "defs.h"

/*
 * Packing.  We have three separate kinds of packing here.
 *
 * First, we pack device instances which have identical parent specs.
 *
 * Second, we pack locators.  Given something like
 *
 *	hp0 at mba0 drive 0
 *	hp* at mba* drive ?
 *	ht0 at mba0 drive 0
 *	tu0 at ht0 slave 0
 *	ht* at mba* drive ?
 *	tu* at ht* slave ?
 *
 * (where the default drive and slave numbers are -1), we have three
 * locators whose value is 0 and three whose value is -1.  Rather than
 * emitting six integers, we emit just two.
 *
 * When packing locators, we would like to find sequences such as
 *	{1 2 3} {2 3 4} {3} {4 5}
 * and turn this into the flat sequence {1 2 3 4 5}, with each subsequence
 * given by the appropriate offset (here 0, 1, 2, and 3 respectively).
 * Non-overlapping packing is much easier, and so we use that here
 * and miss out on the chance to squeeze the locator sequence optimally.
 * (So it goes.)
 */

typedef int (*vec_cmp_func)(const void *, int, int);

#define	TAILHSIZE	128
#define	PVHASH(i)	((i) & (TAILHSIZE - 1))
#define	LOCHASH(l)	(((long)(l) >> 2) & (TAILHSIZE - 1))
struct tails {
	struct	tails *t_next;
	int	t_ends_at;
};

static struct tails *tails[TAILHSIZE];
static int locspace;

static void packdevi(void);
static void packlocs(void);

static int sameas(struct devi *, struct devi *);
static int findvec(const void *, int, int, vec_cmp_func, int);
static int samelocs(const void *, int, int);
static int addlocs(const char **, int);
static int loclencmp(const void *, const void *);
static void resettails(void);

void
pack(void)
{
	struct pspec *p;
	struct devi *i;

	/* Pack instances and make parent vectors. */
	packdevi();

	/*
	 * Now that we know what we have, find upper limits on space
	 * needed for the loc[] table.  The loc table size is bounded by
	 * what we would get if no packing occurred.
	 */
	locspace = 0;
	TAILQ_FOREACH(i, &alldevi, i_next) {
		if (i->i_active != DEVI_ACTIVE || i->i_collapsed)
			continue;
		if ((p = i->i_pspec) == NULL)
			continue;
		locspace += p->p_iattr->a_loclen;
	}

	/* Allocate and pack loc[]. */
	locators.vec = ecalloc((size_t)locspace, sizeof(*locators.vec));
	locators.used = 0;
	packlocs();
}

/*
 * Pack device instances together wherever possible.
 */
static void
packdevi(void)
{
	struct devi *firststar, *i, **ip, *l, *p;
	struct devbase *d;
	u_short j, m, n;

	/*
	 * Sort all the cloning units to after the non-cloning units,
	 * preserving order of cloning and non-cloning units with
	 * respect to other units of the same type.
	 *
	 * Algorithm: Walk down the list until the first cloning unit is
	 * seen for the second time (or until the end of the list, if there
	 * are no cloning units on the list), moving starred units to the
	 * end of the list.
	 */
	TAILQ_FOREACH(d, &allbases, d_next) {
		ip = &d->d_ihead;
		firststar = NULL;

		for (i = *ip; i != firststar; i = *ip) {
			if (i->i_unit != STAR) {
				/* try i->i_bsame next */
				ip = &i->i_bsame;
			} else {
				if (firststar == NULL)
					firststar = i;

				*d->d_ipp = i;
				d->d_ipp = &i->i_bsame;

				*ip = i->i_bsame;
				i->i_bsame = NULL;

				/* leave ip alone; try (old) i->i_bsame next */
			}
		}
	}

	packed = ecalloc((size_t)ndevi + 1, sizeof *packed);
	n = 0;
	TAILQ_FOREACH(d, &allbases, d_next) {
		/*
		 * For each instance of each device, add or collapse
		 * all its aliases.
		 *
		 * Pseudo-devices have a non-empty d_ihead for convenience.
		 * Ignore them.
		 */
		if (d->d_ispseudo)
			continue;
		for (i = d->d_ihead; i != NULL; i = i->i_bsame) {
			m = n;
			for (l = i; l != NULL; l = l->i_alias) {
				if (l->i_active != DEVI_ACTIVE
				    || i->i_pseudoroot)
					continue;
				l->i_locoff = -1;
				/* try to find an equivalent for l */
				for (j = m; j < n; j++) {
					p = packed[j];
					if (sameas(l, p)) {
						l->i_collapsed = 1;
						l->i_cfindex = p->i_cfindex;
						goto nextalias;
					}
				}
				/* could not find a suitable alias */
				l->i_collapsed = 0;
				l->i_cfindex = n;
				packed[n++] = l;
 nextalias:;
			}
		}
	}
	npacked = n;
	packed[n] = NULL;
}

/*
 * Return true if two aliases are "the same".  In this case, they need
 * to have the same parent spec, have the same config flags, and have
 * the same locators.
 */
static int
sameas(struct devi *i1, struct devi *i2)
{
	const char **p1, **p2;

	if (i1->i_pspec != i2->i_pspec)
		return (0);
	if (i1->i_cfflags != i2->i_cfflags)
		return (0);
	for (p1 = i1->i_locs, p2 = i2->i_locs; *p1 == *p2; p2++)
		if (*p1++ == 0)
			return (1);
	return (0);
}

static void
packlocs(void)
{
	struct pspec *ps;
	struct devi **p, *i;
	int l,o;
	extern int Pflag;

	qsort(packed, npacked, sizeof *packed, loclencmp);
	for (p = packed; (i = *p) != NULL; p++) {
		if ((ps = i->i_pspec) != NULL &&
		    (l = ps->p_iattr->a_loclen) > 0) {
			if (Pflag) {
				o = findvec(i->i_locs, 
				    LOCHASH(i->i_locs[l - 1]), l,
				    samelocs, locators.used);
				i->i_locoff = o < 0 ?
				    addlocs(i->i_locs, l) : o;
			} else
				i->i_locoff = addlocs(i->i_locs, l);
		} else
			i->i_locoff = -1;
	}
	resettails();
}

/*
 * Return the index at which the given vector already exists, or -1
 * if it is not anywhere in the current set.  If we return -1, we assume
 * our caller will add it at the end of the current set, and we make
 * sure that next time, we will find it there.
 */
static int
findvec(const void *ptr, int hash, int len, vec_cmp_func cmp, int nextplace)
{
	struct tails *t, **hp;
	int off;

	hp = &tails[hash];
	for (t = *hp; t != NULL; t = t->t_next) {
		off = t->t_ends_at - len;
		if (off >= 0 && (*cmp)(ptr, off, len))
			return (off);
	}
	t = ecalloc(1, sizeof(*t));
	t->t_next = *hp;
	*hp = t;
	t->t_ends_at = nextplace + len;
	return (-1);
}

/*
 * Comparison function for locators.
 */
static int
samelocs(const void *ptr, int off, int len)
{
	const char * const *p, * const *q;

	for (p = &locators.vec[off], q = (const char * const *)ptr; --len >= 0;)
		if (*p++ != *q++)
			return (0);	/* different */
	return (1);			/* same */
}

/*
 * Add the given locators at the end of the global loc[] table.
 */
static int
addlocs(const char **locs, int len)
{
	const char **p;
	int ret;

	ret = locators.used;
	if ((locators.used = ret + len) > locspace)
		panic("addlocs: overrun");
	for (p = &locators.vec[ret]; --len >= 0;)
		*p++ = *locs++;
	return (ret);
}

/*
 * Comparison function for qsort-by-locator-length, longest first.
 * We rashly assume that subtraction of these lengths does not overflow.
 */
static int
loclencmp(const void *a, const void *b)
{
	const struct pspec *p1, *p2;
	int l1, l2;

	p1 = (*(const struct devi * const *)a)->i_pspec;
	l1 = p1 != NULL ? p1->p_iattr->a_loclen : 0;

	p2 = (*(const struct devi * const *)b)->i_pspec;
	l2 = p2 != NULL ? p2->p_iattr->a_loclen : 0;

	return (l2 - l1);
}

static void
resettails(void)
{
	struct tails **p, *t, *next;
	int i;

	for (p = tails, i = TAILHSIZE; --i >= 0; p++) {
		for (t = *p; t != NULL; t = next) {
			next = t->t_next;
			free(t);
		}
		*p = NULL;
	}
}

/*	$NetBSD: radix.c,v 1.45 2015/08/24 22:21:26 pooka Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)radix.c	8.6 (Berkeley) 10/17/95
 */

/*
 * Routines to build and maintain radix trees for routing lookups.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: radix.c,v 1.45 2015/08/24 22:21:26 pooka Exp $");

#ifndef _NET_RADIX_H_
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/kmem.h>
#ifdef	_KERNEL
#ifdef _KERNEL_OPT
#include "opt_inet.h"
#endif

#include <sys/systm.h>
#include <sys/malloc.h>
#define	M_DONTWAIT M_NOWAIT
#include <sys/domain.h>
#else
#include <stdlib.h>
#endif
#include <sys/syslog.h>
#include <net/radix.h>
#endif

typedef void (*rn_printer_t)(void *, const char *fmt, ...);

int	max_keylen;
struct radix_mask *rn_mkfreelist;
struct radix_node_head *mask_rnhead;
static char *addmask_key;
static const char normal_chars[] =
    {0, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, -1};
static char *rn_zeros, *rn_ones;

#define rn_masktop (mask_rnhead->rnh_treetop)

static int rn_satisfies_leaf(const char *, struct radix_node *, int);
static int rn_lexobetter(const void *, const void *);
static struct radix_mask *rn_new_radix_mask(struct radix_node *,
    struct radix_mask *);
static struct radix_node *rn_walknext(struct radix_node *, rn_printer_t,
    void *);
static struct radix_node *rn_walkfirst(struct radix_node *, rn_printer_t,
    void *);
static void rn_nodeprint(struct radix_node *, rn_printer_t, void *,
    const char *);

#define	SUBTREE_OPEN	"[ "
#define	SUBTREE_CLOSE	" ]"

#ifdef RN_DEBUG
static void rn_treeprint(struct radix_node_head *, rn_printer_t, void *);
#endif /* RN_DEBUG */

/*
 * The data structure for the keys is a radix tree with one way
 * branching removed.  The index rn_b at an internal node n represents a bit
 * position to be tested.  The tree is arranged so that all descendants
 * of a node n have keys whose bits all agree up to position rn_b - 1.
 * (We say the index of n is rn_b.)
 *
 * There is at least one descendant which has a one bit at position rn_b,
 * and at least one with a zero there.
 *
 * A route is determined by a pair of key and mask.  We require that the
 * bit-wise logical and of the key and mask to be the key.
 * We define the index of a route to associated with the mask to be
 * the first bit number in the mask where 0 occurs (with bit number 0
 * representing the highest order bit).
 *
 * We say a mask is normal if every bit is 0, past the index of the mask.
 * If a node n has a descendant (k, m) with index(m) == index(n) == rn_b,
 * and m is a normal mask, then the route applies to every descendant of n.
 * If the index(m) < rn_b, this implies the trailing last few bits of k
 * before bit b are all 0, (and hence consequently true of every descendant
 * of n), so the route applies to all descendants of the node as well.
 *
 * Similar logic shows that a non-normal mask m such that
 * index(m) <= index(n) could potentially apply to many children of n.
 * Thus, for each non-host route, we attach its mask to a list at an internal
 * node as high in the tree as we can go.
 *
 * The present version of the code makes use of normal routes in short-
 * circuiting an explicit mask and compare operation when testing whether
 * a key satisfies a normal route, and also in remembering the unique leaf
 * that governs a subtree.
 */

struct radix_node *
rn_search(
	const void *v_arg,
	struct radix_node *head)
{
	const u_char * const v = v_arg;
	struct radix_node *x;

	for (x = head; x->rn_b >= 0;) {
		if (x->rn_bmask & v[x->rn_off])
			x = x->rn_r;
		else
			x = x->rn_l;
	}
	return x;
}

struct radix_node *
rn_search_m(
	const void *v_arg,
	struct radix_node *head,
	const void *m_arg)
{
	struct radix_node *x;
	const u_char * const v = v_arg;
	const u_char * const m = m_arg;

	for (x = head; x->rn_b >= 0;) {
		if ((x->rn_bmask & m[x->rn_off]) &&
		    (x->rn_bmask & v[x->rn_off]))
			x = x->rn_r;
		else
			x = x->rn_l;
	}
	return x;
}

int
rn_refines(
	const void *m_arg,
	const void *n_arg)
{
	const char *m = m_arg;
	const char *n = n_arg;
	const char *lim = n + *(const u_char *)n;
	const char *lim2 = lim;
	int longer = (*(const u_char *)n++) - (int)(*(const u_char *)m++);
	int masks_are_equal = 1;

	if (longer > 0)
		lim -= longer;
	while (n < lim) {
		if (*n & ~(*m))
			return 0;
		if (*n++ != *m++)
			masks_are_equal = 0;
	}
	while (n < lim2)
		if (*n++)
			return 0;
	if (masks_are_equal && (longer < 0))
		for (lim2 = m - longer; m < lim2; )
			if (*m++)
				return 1;
	return !masks_are_equal;
}

struct radix_node *
rn_lookup(
	const void *v_arg,
	const void *m_arg,
	struct radix_node_head *head)
{
	struct radix_node *x;
	const char *netmask = NULL;

	if (m_arg) {
		if ((x = rn_addmask(m_arg, 1, head->rnh_treetop->rn_off)) == 0)
			return NULL;
		netmask = x->rn_key;
	}
	x = rn_match(v_arg, head);
	if (x != NULL && netmask != NULL) {
		while (x != NULL && x->rn_mask != netmask)
			x = x->rn_dupedkey;
	}
	return x;
}

static int
rn_satisfies_leaf(
	const char *trial,
	struct radix_node *leaf,
	int skip)
{
	const char *cp = trial;
	const char *cp2 = leaf->rn_key;
	const char *cp3 = leaf->rn_mask;
	const char *cplim;
	int length = min(*(const u_char *)cp, *(const u_char *)cp2);

	if (cp3 == 0)
		cp3 = rn_ones;
	else
		length = min(length, *(const u_char *)cp3);
	cplim = cp + length; cp3 += skip; cp2 += skip;
	for (cp += skip; cp < cplim; cp++, cp2++, cp3++)
		if ((*cp ^ *cp2) & *cp3)
			return 0;
	return 1;
}

struct radix_node *
rn_match(
	const void *v_arg,
	struct radix_node_head *head)
{
	const char * const v = v_arg;
	struct radix_node *t = head->rnh_treetop;
	struct radix_node *top = t;
	struct radix_node *x;
	struct radix_node *saved_t;
	const char *cp = v;
	const char *cp2;
	const char *cplim;
	int off = t->rn_off;
	int vlen = *(const u_char *)cp;
	int matched_off;
	int test, b, rn_b;

	/*
	 * Open code rn_search(v, top) to avoid overhead of extra
	 * subroutine call.
	 */
	for (; t->rn_b >= 0; ) {
		if (t->rn_bmask & cp[t->rn_off])
			t = t->rn_r;
		else
			t = t->rn_l;
	}
	/*
	 * See if we match exactly as a host destination
	 * or at least learn how many bits match, for normal mask finesse.
	 *
	 * It doesn't hurt us to limit how many bytes to check
	 * to the length of the mask, since if it matches we had a genuine
	 * match and the leaf we have is the most specific one anyway;
	 * if it didn't match with a shorter length it would fail
	 * with a long one.  This wins big for class B&C netmasks which
	 * are probably the most common case...
	 */
	if (t->rn_mask)
		vlen = *(const u_char *)t->rn_mask;
	cp += off; cp2 = t->rn_key + off; cplim = v + vlen;
	for (; cp < cplim; cp++, cp2++)
		if (*cp != *cp2)
			goto on1;
	/*
	 * This extra grot is in case we are explicitly asked
	 * to look up the default.  Ugh!
	 */
	if ((t->rn_flags & RNF_ROOT) && t->rn_dupedkey)
		t = t->rn_dupedkey;
	return t;
on1:
	test = (*cp ^ *cp2) & 0xff; /* find first bit that differs */
	for (b = 7; (test >>= 1) > 0;)
		b--;
	matched_off = cp - v;
	b += matched_off << 3;
	rn_b = -1 - b;
	/*
	 * If there is a host route in a duped-key chain, it will be first.
	 */
	if ((saved_t = t)->rn_mask == 0)
		t = t->rn_dupedkey;
	for (; t; t = t->rn_dupedkey)
		/*
		 * Even if we don't match exactly as a host,
		 * we may match if the leaf we wound up at is
		 * a route to a net.
		 */
		if (t->rn_flags & RNF_NORMAL) {
			if (rn_b <= t->rn_b)
				return t;
		} else if (rn_satisfies_leaf(v, t, matched_off))
				return t;
	t = saved_t;
	/* start searching up the tree */
	do {
		struct radix_mask *m;
		t = t->rn_p;
		m = t->rn_mklist;
		if (m) {
			/*
			 * If non-contiguous masks ever become important
			 * we can restore the masking and open coding of
			 * the search and satisfaction test and put the
			 * calculation of "off" back before the "do".
			 */
			do {
				if (m->rm_flags & RNF_NORMAL) {
					if (rn_b <= m->rm_b)
						return m->rm_leaf;
				} else {
					off = min(t->rn_off, matched_off);
					x = rn_search_m(v, t, m->rm_mask);
					while (x && x->rn_mask != m->rm_mask)
						x = x->rn_dupedkey;
					if (x && rn_satisfies_leaf(v, x, off))
						return x;
				}
				m = m->rm_mklist;
			} while (m);
		}
	} while (t != top);
	return NULL;
}

static void
rn_nodeprint(struct radix_node *rn, rn_printer_t printer, void *arg,
    const char *delim)
{
	(*printer)(arg, "%s(%s%p: p<%p> l<%p> r<%p>)",
	    delim, ((void *)rn == arg) ? "*" : "", rn, rn->rn_p,
	    rn->rn_l, rn->rn_r);
}

#ifdef RN_DEBUG
int	rn_debug =  1;

static void
rn_dbg_print(void *arg, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vlog(LOG_DEBUG, fmt, ap);
	va_end(ap);
}

static void
rn_treeprint(struct radix_node_head *h, rn_printer_t printer, void *arg)
{
	struct radix_node *dup, *rn;
	const char *delim;

	if (printer == NULL)
		return;

	rn = rn_walkfirst(h->rnh_treetop, printer, arg);
	for (;;) {
		/* Process leaves */
		delim = "";
		for (dup = rn; dup != NULL; dup = dup->rn_dupedkey) {
			if ((dup->rn_flags & RNF_ROOT) != 0)
				continue;
			rn_nodeprint(dup, printer, arg, delim);
			delim = ", ";
		}
		rn = rn_walknext(rn, printer, arg);
		if (rn->rn_flags & RNF_ROOT)
			return;
	}
	/* NOTREACHED */
}

#define	traverse(__head, __rn)	rn_treeprint((__head), rn_dbg_print, (__rn))
#endif /* RN_DEBUG */

struct radix_node *
rn_newpair(
	const void *v,
	int b,
	struct radix_node nodes[2])
{
	struct radix_node *tt = nodes;
	struct radix_node *t = tt + 1;
	t->rn_b = b; t->rn_bmask = 0x80 >> (b & 7);
	t->rn_l = tt; t->rn_off = b >> 3;
	tt->rn_b = -1; tt->rn_key = v; tt->rn_p = t;
	tt->rn_flags = t->rn_flags = RNF_ACTIVE;
	return t;
}

struct radix_node *
rn_insert(
	const void *v_arg,
	struct radix_node_head *head,
	int *dupentry,
	struct radix_node nodes[2])
{
	struct radix_node *top = head->rnh_treetop;
	struct radix_node *t = rn_search(v_arg, top);
	struct radix_node *tt;
	const char *v = v_arg;
	int head_off = top->rn_off;
	int vlen = *((const u_char *)v);
	const char *cp = v + head_off;
	int b;
    	/*
	 * Find first bit at which v and t->rn_key differ
	 */
    {
	const char *cp2 = t->rn_key + head_off;
	const char *cplim = v + vlen;
	int cmp_res;

	while (cp < cplim)
		if (*cp2++ != *cp++)
			goto on1;
	*dupentry = 1;
	return t;
on1:
	*dupentry = 0;
	cmp_res = (cp[-1] ^ cp2[-1]) & 0xff;
	for (b = (cp - v) << 3; cmp_res; b--)
		cmp_res >>= 1;
    }
    {
	struct radix_node *p, *x = top;
	cp = v;
	do {
		p = x;
		if (cp[x->rn_off] & x->rn_bmask)
			x = x->rn_r;
		else x = x->rn_l;
	} while (b > (unsigned) x->rn_b); /* x->rn_b < b && x->rn_b >= 0 */
#ifdef RN_DEBUG
	if (rn_debug)
		log(LOG_DEBUG, "%s: Going In:\n", __func__), traverse(head, p);
#endif
	t = rn_newpair(v_arg, b, nodes); tt = t->rn_l;
	if ((cp[p->rn_off] & p->rn_bmask) == 0)
		p->rn_l = t;
	else
		p->rn_r = t;
	x->rn_p = t; t->rn_p = p; /* frees x, p as temp vars below */
	if ((cp[t->rn_off] & t->rn_bmask) == 0) {
		t->rn_r = x;
	} else {
		t->rn_r = tt; t->rn_l = x;
	}
#ifdef RN_DEBUG
	if (rn_debug) {
		log(LOG_DEBUG, "%s: Coming Out:\n", __func__),
		    traverse(head, p);
	}
#endif /* RN_DEBUG */
    }
	return tt;
}

struct radix_node *
rn_addmask(
	const void *n_arg,
	int search,
	int skip)
{
	const char *netmask = n_arg;
	const char *cp;
	const char *cplim;
	struct radix_node *x;
	struct radix_node *saved_x;
	int b = 0, mlen, j;
	int maskduplicated, m0, isnormal;
	static int last_zeroed = 0;

	if ((mlen = *(const u_char *)netmask) > max_keylen)
		mlen = max_keylen;
	if (skip == 0)
		skip = 1;
	if (mlen <= skip)
		return mask_rnhead->rnh_nodes;
	if (skip > 1)
		memmove(addmask_key + 1, rn_ones + 1, skip - 1);
	if ((m0 = mlen) > skip)
		memmove(addmask_key + skip, netmask + skip, mlen - skip);
	/*
	 * Trim trailing zeroes.
	 */
	for (cp = addmask_key + mlen; (cp > addmask_key) && cp[-1] == 0;)
		cp--;
	mlen = cp - addmask_key;
	if (mlen <= skip) {
		if (m0 >= last_zeroed)
			last_zeroed = mlen;
		return mask_rnhead->rnh_nodes;
	}
	if (m0 < last_zeroed)
		memset(addmask_key + m0, 0, last_zeroed - m0);
	*addmask_key = last_zeroed = mlen;
	x = rn_search(addmask_key, rn_masktop);
	if (memcmp(addmask_key, x->rn_key, mlen) != 0)
		x = 0;
	if (x || search)
		return x;
	R_Malloc(x, struct radix_node *, max_keylen + 2 * sizeof (*x));
	if ((saved_x = x) == NULL)
		return NULL;
	memset(x, 0, max_keylen + 2 * sizeof (*x));
	cp = netmask = (void *)(x + 2);
	memmove(x + 2, addmask_key, mlen);
	x = rn_insert(cp, mask_rnhead, &maskduplicated, x);
	if (maskduplicated) {
		log(LOG_ERR, "rn_addmask: mask impossibly already in tree\n");
		Free(saved_x);
		return x;
	}
	/*
	 * Calculate index of mask, and check for normalcy.
	 */
	cplim = netmask + mlen; isnormal = 1;
	for (cp = netmask + skip; (cp < cplim) && *(const u_char *)cp == 0xff;)
		cp++;
	if (cp != cplim) {
		for (j = 0x80; (j & *cp) != 0; j >>= 1)
			b++;
		if (*cp != normal_chars[b] || cp != (cplim - 1))
			isnormal = 0;
	}
	b += (cp - netmask) << 3;
	x->rn_b = -1 - b;
	if (isnormal)
		x->rn_flags |= RNF_NORMAL;
	return x;
}

static int	/* XXX: arbitrary ordering for non-contiguous masks */
rn_lexobetter(
	const void *m_arg,
	const void *n_arg)
{
	const u_char *mp = m_arg;
	const u_char *np = n_arg;
	const u_char *lim;

	if (*mp > *np)
		return 1;  /* not really, but need to check longer one first */
	if (*mp == *np)
		for (lim = mp + *mp; mp < lim;)
			if (*mp++ > *np++)
				return 1;
	return 0;
}

static struct radix_mask *
rn_new_radix_mask(
	struct radix_node *tt,
	struct radix_mask *next)
{
	struct radix_mask *m;

	MKGet(m);
	if (m == NULL) {
		log(LOG_ERR, "Mask for route not entered\n");
		return NULL;
	}
	memset(m, 0, sizeof(*m));
	m->rm_b = tt->rn_b;
	m->rm_flags = tt->rn_flags;
	if (tt->rn_flags & RNF_NORMAL)
		m->rm_leaf = tt;
	else
		m->rm_mask = tt->rn_mask;
	m->rm_mklist = next;
	tt->rn_mklist = m;
	return m;
}

struct radix_node *
rn_addroute(
	const void *v_arg,
	const void *n_arg,
	struct radix_node_head *head,
	struct radix_node treenodes[2])
{
	const char *v = v_arg, *netmask = n_arg;
	struct radix_node *t, *x = NULL, *tt;
	struct radix_node *saved_tt, *top = head->rnh_treetop;
	short b = 0, b_leaf = 0;
	int keyduplicated;
	const char *mmask;
	struct radix_mask *m, **mp;

	/*
	 * In dealing with non-contiguous masks, there may be
	 * many different routes which have the same mask.
	 * We will find it useful to have a unique pointer to
	 * the mask to speed avoiding duplicate references at
	 * nodes and possibly save time in calculating indices.
	 */
	if (netmask != NULL) {
		if ((x = rn_addmask(netmask, 0, top->rn_off)) == NULL)
			return NULL;
		b_leaf = x->rn_b;
		b = -1 - x->rn_b;
		netmask = x->rn_key;
	}
	/*
	 * Deal with duplicated keys: attach node to previous instance
	 */
	saved_tt = tt = rn_insert(v, head, &keyduplicated, treenodes);
	if (keyduplicated) {
		for (t = tt; tt != NULL; t = tt, tt = tt->rn_dupedkey) {
			if (tt->rn_mask == netmask)
				return NULL;
			if (netmask == NULL ||
			    (tt->rn_mask != NULL &&
			     (b_leaf < tt->rn_b || /* index(netmask) > node */
			       rn_refines(netmask, tt->rn_mask) ||
			       rn_lexobetter(netmask, tt->rn_mask))))
				break;
		}
		/*
		 * If the mask is not duplicated, we wouldn't
		 * find it among possible duplicate key entries
		 * anyway, so the above test doesn't hurt.
		 *
		 * We sort the masks for a duplicated key the same way as
		 * in a masklist -- most specific to least specific.
		 * This may require the unfortunate nuisance of relocating
		 * the head of the list.
		 *
		 * We also reverse, or doubly link the list through the
		 * parent pointer.
		 */
		if (tt == saved_tt) {
			struct	radix_node *xx = x;
			/* link in at head of list */
			(tt = treenodes)->rn_dupedkey = t;
			tt->rn_flags = t->rn_flags;
			tt->rn_p = x = t->rn_p;
			t->rn_p = tt;
			if (x->rn_l == t)
				x->rn_l = tt;
			else
				x->rn_r = tt;
			saved_tt = tt;
			x = xx;
		} else {
			(tt = treenodes)->rn_dupedkey = t->rn_dupedkey;
			t->rn_dupedkey = tt;
			tt->rn_p = t;
			if (tt->rn_dupedkey)
				tt->rn_dupedkey->rn_p = tt;
		}
		tt->rn_key = v;
		tt->rn_b = -1;
		tt->rn_flags = RNF_ACTIVE;
	}
	/*
	 * Put mask in tree.
	 */
	if (netmask != NULL) {
		tt->rn_mask = netmask;
		tt->rn_b = x->rn_b;
		tt->rn_flags |= x->rn_flags & RNF_NORMAL;
	}
	t = saved_tt->rn_p;
	if (keyduplicated)
		goto on2;
	b_leaf = -1 - t->rn_b;
	if (t->rn_r == saved_tt)
		x = t->rn_l;
	else
		x = t->rn_r;
	/* Promote general routes from below */
	if (x->rn_b < 0) {
		for (mp = &t->rn_mklist; x != NULL; x = x->rn_dupedkey) {
			if (x->rn_mask != NULL && x->rn_b >= b_leaf &&
			    x->rn_mklist == NULL) {
				*mp = m = rn_new_radix_mask(x, NULL);
				if (m != NULL)
					mp = &m->rm_mklist;
			}
		}
	} else if (x->rn_mklist != NULL) {
		/*
		 * Skip over masks whose index is > that of new node
		 */
		for (mp = &x->rn_mklist; (m = *mp) != NULL; mp = &m->rm_mklist)
			if (m->rm_b >= b_leaf)
				break;
		t->rn_mklist = m;
		*mp = NULL;
	}
on2:
	/* Add new route to highest possible ancestor's list */
	if (netmask == NULL || b > t->rn_b)
		return tt; /* can't lift at all */
	b_leaf = tt->rn_b;
	do {
		x = t;
		t = t->rn_p;
	} while (b <= t->rn_b && x != top);
	/*
	 * Search through routes associated with node to
	 * insert new route according to index.
	 * Need same criteria as when sorting dupedkeys to avoid
	 * double loop on deletion.
	 */
	for (mp = &x->rn_mklist; (m = *mp) != NULL; mp = &m->rm_mklist) {
		if (m->rm_b < b_leaf)
			continue;
		if (m->rm_b > b_leaf)
			break;
		if (m->rm_flags & RNF_NORMAL) {
			mmask = m->rm_leaf->rn_mask;
			if (tt->rn_flags & RNF_NORMAL) {
				log(LOG_ERR, "Non-unique normal route,"
				    " mask not entered\n");
				return tt;
			}
		} else
			mmask = m->rm_mask;
		if (mmask == netmask) {
			m->rm_refs++;
			tt->rn_mklist = m;
			return tt;
		}
		if (rn_refines(netmask, mmask) || rn_lexobetter(netmask, mmask))
			break;
	}
	*mp = rn_new_radix_mask(tt, *mp);
	return tt;
}

struct radix_node *
rn_delete1(
	const void *v_arg,
	const void *netmask_arg,
	struct radix_node_head *head,
	struct radix_node *rn)
{
	struct radix_node *t, *p, *x, *tt;
	struct radix_mask *m, *saved_m, **mp;
	struct radix_node *dupedkey, *saved_tt, *top;
	const char *v, *netmask;
	int b, head_off, vlen;

	v = v_arg;
	netmask = netmask_arg;
	x = head->rnh_treetop;
	tt = rn_search(v, x);
	head_off = x->rn_off;
	vlen =  *(const u_char *)v;
	saved_tt = tt;
	top = x;
	if (tt == NULL ||
	    memcmp(v + head_off, tt->rn_key + head_off, vlen - head_off) != 0)
		return NULL;
	/*
	 * Delete our route from mask lists.
	 */
	if (netmask != NULL) {
		if ((x = rn_addmask(netmask, 1, head_off)) == NULL)
			return NULL;
		netmask = x->rn_key;
		while (tt->rn_mask != netmask)
			if ((tt = tt->rn_dupedkey) == NULL)
				return NULL;
	}
	if (tt->rn_mask == NULL || (saved_m = m = tt->rn_mklist) == NULL)
		goto on1;
	if (tt->rn_flags & RNF_NORMAL) {
		if (m->rm_leaf != tt || m->rm_refs > 0) {
			log(LOG_ERR, "rn_delete: inconsistent annotation\n");
			return NULL;  /* dangling ref could cause disaster */
		}
	} else {
		if (m->rm_mask != tt->rn_mask) {
			log(LOG_ERR, "rn_delete: inconsistent annotation\n");
			goto on1;
		}
		if (--m->rm_refs >= 0)
			goto on1;
	}
	b = -1 - tt->rn_b;
	t = saved_tt->rn_p;
	if (b > t->rn_b)
		goto on1; /* Wasn't lifted at all */
	do {
		x = t;
		t = t->rn_p;
	} while (b <= t->rn_b && x != top);
	for (mp = &x->rn_mklist; (m = *mp) != NULL; mp = &m->rm_mklist) {
		if (m == saved_m) {
			*mp = m->rm_mklist;
			MKFree(m);
			break;
		}
	}
	if (m == NULL) {
		log(LOG_ERR, "rn_delete: couldn't find our annotation\n");
		if (tt->rn_flags & RNF_NORMAL)
			return NULL; /* Dangling ref to us */
	}
on1:
	/*
	 * Eliminate us from tree
	 */
	if (tt->rn_flags & RNF_ROOT)
		return NULL;
#ifdef RN_DEBUG
	if (rn_debug)
		log(LOG_DEBUG, "%s: Going In:\n", __func__), traverse(head, tt);
#endif
	t = tt->rn_p;
	dupedkey = saved_tt->rn_dupedkey;
	if (dupedkey != NULL) {
		/*
		 * Here, tt is the deletion target, and
		 * saved_tt is the head of the dupedkey chain.
		 */
		if (tt == saved_tt) {
			x = dupedkey;
			x->rn_p = t;
			if (t->rn_l == tt)
				t->rn_l = x;
			else
				t->rn_r = x;
		} else {
			/* find node in front of tt on the chain */
			for (x = p = saved_tt;
			     p != NULL && p->rn_dupedkey != tt;)
				p = p->rn_dupedkey;
			if (p != NULL) {
				p->rn_dupedkey = tt->rn_dupedkey;
				if (tt->rn_dupedkey != NULL)
					tt->rn_dupedkey->rn_p = p;
			} else
				log(LOG_ERR, "rn_delete: couldn't find us\n");
		}
		t = tt + 1;
		if  (t->rn_flags & RNF_ACTIVE) {
			*++x = *t;
			p = t->rn_p;
			if (p->rn_l == t)
				p->rn_l = x;
			else
				p->rn_r = x;
			x->rn_l->rn_p = x;
			x->rn_r->rn_p = x;
		}
		goto out;
	}
	if (t->rn_l == tt)
		x = t->rn_r;
	else
		x = t->rn_l;
	p = t->rn_p;
	if (p->rn_r == t)
		p->rn_r = x;
	else
		p->rn_l = x;
	x->rn_p = p;
	/*
	 * Demote routes attached to us.
	 */
	if (t->rn_mklist == NULL)
		;
	else if (x->rn_b >= 0) {
		for (mp = &x->rn_mklist; (m = *mp) != NULL; mp = &m->rm_mklist)
			;
		*mp = t->rn_mklist;
	} else {
		/* If there are any key,mask pairs in a sibling
		   duped-key chain, some subset will appear sorted
		   in the same order attached to our mklist */
		for (m = t->rn_mklist;
		     m != NULL && x != NULL;
		     x = x->rn_dupedkey) {
			if (m == x->rn_mklist) {
				struct radix_mask *mm = m->rm_mklist;
				x->rn_mklist = NULL;
				if (--(m->rm_refs) < 0)
					MKFree(m);
				m = mm;
			}
		}
		if (m != NULL) {
			log(LOG_ERR, "rn_delete: Orphaned Mask %p at %p\n",
			    m, x);
		}
	}
	/*
	 * We may be holding an active internal node in the tree.
	 */
	x = tt + 1;
	if (t != x) {
		*t = *x;
		t->rn_l->rn_p = t;
		t->rn_r->rn_p = t;
		p = x->rn_p;
		if (p->rn_l == x)
			p->rn_l = t;
		else
			p->rn_r = t;
	}
out:
#ifdef RN_DEBUG
	if (rn_debug) {
		log(LOG_DEBUG, "%s: Coming Out:\n", __func__),
		    traverse(head, tt);
	}
#endif /* RN_DEBUG */
	tt->rn_flags &= ~RNF_ACTIVE;
	tt[1].rn_flags &= ~RNF_ACTIVE;
	return tt;
}

struct radix_node *
rn_delete(
	const void *v_arg,
	const void *netmask_arg,
	struct radix_node_head *head)
{
	return rn_delete1(v_arg, netmask_arg, head, NULL);
}

static struct radix_node *
rn_walknext(struct radix_node *rn, rn_printer_t printer, void *arg)
{
	/* If at right child go back up, otherwise, go right */
	while (rn->rn_p->rn_r == rn && (rn->rn_flags & RNF_ROOT) == 0) {
		if (printer != NULL)
			(*printer)(arg, SUBTREE_CLOSE);
		rn = rn->rn_p;
	}
	if (printer)
		rn_nodeprint(rn->rn_p, printer, arg, "");
	/* Find the next *leaf* since next node might vanish, too */
	for (rn = rn->rn_p->rn_r; rn->rn_b >= 0;) {
		if (printer != NULL)
			(*printer)(arg, SUBTREE_OPEN);
		rn = rn->rn_l;
	}
	return rn;
}

static struct radix_node *
rn_walkfirst(struct radix_node *rn, rn_printer_t printer, void *arg)
{
	/* First time through node, go left */
	while (rn->rn_b >= 0) {
		if (printer != NULL)
			(*printer)(arg, SUBTREE_OPEN);
		rn = rn->rn_l;
	}
	return rn;
}

int
rn_walktree(
	struct radix_node_head *h,
	int (*f)(struct radix_node *, void *),
	void *w)
{
	int error;
	struct radix_node *base, *next, *rn;
	/*
	 * This gets complicated because we may delete the node
	 * while applying the function f to it, so we need to calculate
	 * the successor node in advance.
	 */
	rn = rn_walkfirst(h->rnh_treetop, NULL, NULL);
	for (;;) {
		base = rn;
		next = rn_walknext(rn, NULL, NULL);
		/* Process leaves */
		while ((rn = base) != NULL) {
			base = rn->rn_dupedkey;
			if (!(rn->rn_flags & RNF_ROOT) && (error = (*f)(rn, w)))
				return error;
		}
		rn = next;
		if (rn->rn_flags & RNF_ROOT)
			return 0;
	}
	/* NOTREACHED */
}

struct delayinit {
	void **head;
	int off;
	SLIST_ENTRY(delayinit) entries;
};
static SLIST_HEAD(, delayinit) delayinits = SLIST_HEAD_INITIALIZER(delayheads);
static int radix_initialized;

/*
 * Initialize a radix tree once radix is initialized.  Only for bootstrap.
 * Assume that no concurrency protection is necessary at this stage.
 */
void
rn_delayedinit(void **head, int off)
{
	struct delayinit *di;

	KASSERT(radix_initialized == 0);

	di = kmem_alloc(sizeof(*di), KM_SLEEP);
	di->head = head;
	di->off = off;
	SLIST_INSERT_HEAD(&delayinits, di, entries);
}

int
rn_inithead(void **head, int off)
{
	struct radix_node_head *rnh;

	if (*head != NULL)
		return 1;
	R_Malloc(rnh, struct radix_node_head *, sizeof (*rnh));
	if (rnh == NULL)
		return 0;
	*head = rnh;
	return rn_inithead0(rnh, off);
}

int
rn_inithead0(struct radix_node_head *rnh, int off)
{
	struct radix_node *t;
	struct radix_node *tt;
	struct radix_node *ttt;

	memset(rnh, 0, sizeof(*rnh));
	t = rn_newpair(rn_zeros, off, rnh->rnh_nodes);
	ttt = rnh->rnh_nodes + 2;
	t->rn_r = ttt;
	t->rn_p = t;
	tt = t->rn_l;
	tt->rn_flags = t->rn_flags = RNF_ROOT | RNF_ACTIVE;
	tt->rn_b = -1 - off;
	*ttt = *tt;
	ttt->rn_key = rn_ones;
	rnh->rnh_addaddr = rn_addroute;
	rnh->rnh_deladdr = rn_delete;
	rnh->rnh_matchaddr = rn_match;
	rnh->rnh_lookup = rn_lookup;
	rnh->rnh_treetop = t;
	return 1;
}

void
rn_init(void)
{
	char *cp, *cplim;
	struct delayinit *di;
#ifdef _KERNEL
	struct domain *dp;

	if (radix_initialized)
		panic("radix already initialized");
	radix_initialized = 1;

	DOMAIN_FOREACH(dp) {
		if (dp->dom_maxrtkey > max_keylen)
			max_keylen = dp->dom_maxrtkey;
	}
#endif
	if (max_keylen == 0) {
		log(LOG_ERR,
		    "rn_init: radix functions require max_keylen be set\n");
		return;
	}

	R_Malloc(rn_zeros, char *, 3 * max_keylen);
	if (rn_zeros == NULL)
		panic("rn_init");
	memset(rn_zeros, 0, 3 * max_keylen);
	rn_ones = cp = rn_zeros + max_keylen;
	addmask_key = cplim = rn_ones + max_keylen;
	while (cp < cplim)
		*cp++ = -1;
	if (rn_inithead((void *)&mask_rnhead, 0) == 0)
		panic("rn_init 2");

	while ((di = SLIST_FIRST(&delayinits)) != NULL) {
		if (!rn_inithead(di->head, di->off))
			panic("delayed rn_inithead failed");
		SLIST_REMOVE_HEAD(&delayinits, entries);
		kmem_free(di, sizeof(*di));
	}
}

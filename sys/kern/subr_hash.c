/*	$NetBSD: subr_hash.c,v 1.6 2014/05/29 21:15:55 rmind Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)kern_subr.c	8.4 (Berkeley) 2/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_hash.c,v 1.6 2014/05/29 21:15:55 rmind Exp $");

#include <sys/param.h>
#include <sys/bitops.h>
#include <sys/kmem.h>
#include <sys/systm.h>

static size_t
hash_list_size(enum hashtype htype)
{
	LIST_HEAD(, generic) *hashtbl_list;
	SLIST_HEAD(, generic) *hashtbl_slist;
	TAILQ_HEAD(, generic) *hashtbl_tailq;
	size_t esize;

	switch (htype) {
	case HASH_LIST:
		esize = sizeof(*hashtbl_list);
		break;
	case HASH_SLIST:
		esize = sizeof(*hashtbl_slist);
		break;
	case HASH_TAILQ:
		esize = sizeof(*hashtbl_tailq);
		break;
	default:
		panic("hashdone: invalid table type");
	}
	return esize;
}

/*
 * General routine to allocate a hash table.
 * Allocate enough memory to hold at least `elements' list-head pointers.
 * Return a pointer to the allocated space and set *hashmask to a pattern
 * suitable for masking a value to use as an index into the returned array.
 */
void *
hashinit(u_int elements, enum hashtype htype, bool waitok, u_long *hashmask)
{
	LIST_HEAD(, generic) *hashtbl_list;
	SLIST_HEAD(, generic) *hashtbl_slist;
	TAILQ_HEAD(, generic) *hashtbl_tailq;
	u_long hashsize, i;
	size_t esize;
	void *p;

	KASSERT(elements > 0);

#define MAXELEMENTS (1U << ((sizeof(elements) * NBBY) - 1))
	if (elements > MAXELEMENTS)
		elements = MAXELEMENTS;

	hashsize = 1UL << (ilog2(elements - 1) + 1);
	esize = hash_list_size(htype);

	p = kmem_alloc(hashsize * esize, waitok ? KM_SLEEP : KM_NOSLEEP);
	if (p == NULL)
		return NULL;

	switch (htype) {
	case HASH_LIST:
		hashtbl_list = p;
		for (i = 0; i < hashsize; i++)
			LIST_INIT(&hashtbl_list[i]);
		break;
	case HASH_SLIST:
		hashtbl_slist = p;
		for (i = 0; i < hashsize; i++)
			SLIST_INIT(&hashtbl_slist[i]);
		break;
	case HASH_TAILQ:
		hashtbl_tailq = p;
		for (i = 0; i < hashsize; i++)
			TAILQ_INIT(&hashtbl_tailq[i]);
		break;
	}
	*hashmask = hashsize - 1;
	return p;
}

/*
 * Free memory from hash table previosly allocated via hashinit().
 */
void
hashdone(void *hashtbl, enum hashtype htype, u_long hashmask)
{
	const size_t esize = hash_list_size(htype);
	kmem_free(hashtbl, esize * (hashmask + 1));
}

/*	$NetBSD: chfs_vnode_cache.c,v 1.3 2012/10/19 12:44:39 ttoth Exp $	*/

/*-
 * Copyright (c) 2010 Department of Software Engineering,
 *		      University of Szeged, Hungary
 * Copyright (C) 2010 Tamas Toth <ttoth@inf.u-szeged.hu>
 * Copyright (C) 2010 Adam Hoka <ahoka@NetBSD.org>
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by the Department of Software Engineering, University of Szeged, Hungary
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "chfs.h"
#include <sys/pool.h>

/* vnode cache is a hashtable for vnodes */

/* chfs_vnocache_hash_init - initializing the hashtable */
struct chfs_vnode_cache **
chfs_vnocache_hash_init(void)
{
	return kmem_zalloc(VNODECACHE_SIZE *
	    sizeof(struct chfs_vnode_cache *), KM_SLEEP);
}

/*
 * chfs_vnode_cache_get - get a vnode_cache from the hashtable
 * Returns the vnode_cache.
 */
struct chfs_vnode_cache *
chfs_vnode_cache_get(struct chfs_mount *chmp, ino_t vno)
{
	struct chfs_vnode_cache* ret;

	KASSERT(mutex_owned(&chmp->chm_lock_vnocache));

	ret = chmp->chm_vnocache_hash[vno % VNODECACHE_SIZE];

	if (ret == NULL) {
		return NULL;
	}

	while (ret && ret->vno < vno) {
		ret = ret->next;
	}

	if (ret && ret->vno != vno) {
		ret = NULL;
	}

	return ret;
}

/* chfs_vnode_cache_add - add a vnode_cache to the hashtable */
void
chfs_vnode_cache_add(struct chfs_mount *chmp,
    struct chfs_vnode_cache* new)
{
	struct chfs_vnode_cache** prev;

	KASSERT(mutex_owned(&chmp->chm_lock_vnocache));

	if (!new->vno) {
		new->vno = ++chmp->chm_max_vno;
	}

	prev = &chmp->chm_vnocache_hash[new->vno % VNODECACHE_SIZE];

	while ((*prev) && (*prev)->vno < new->vno) {
		prev = &((*prev)->next);
	}
	new->next = *prev;
	*prev = new;
}

/* chfs_vnode_cache_remove - removes a vnode_cache from the hashtable */
void
chfs_vnode_cache_remove(struct chfs_mount *chmp,
    struct chfs_vnode_cache* old)
{
	struct chfs_vnode_cache** prev;

	KASSERT(mutex_owned(&chmp->chm_lock_vnocache));

	prev = &chmp->chm_vnocache_hash[old->vno % VNODECACHE_SIZE];
	while ((*prev) && (*prev)->vno < old->vno) {
		prev = &(*prev)->next;
	}

	if ((*prev) == old) {
		*prev = old->next;
	}

	if (old->state != VNO_STATE_READING &&
	    old->state != VNO_STATE_CLEARING) {
		chfs_vnode_cache_free(old);
	}
}

/* chfs_vnocache_hash_destroy - destroying the vnode cache */
void
chfs_vnocache_hash_destroy(struct chfs_vnode_cache **hash)
{
	struct chfs_vnode_cache *this, *next;
	int i;

	/* free every row */
	for (i = 0; i < VNODECACHE_SIZE; i++) {
		this = hash[i];
		while (this) {
			next = this->next;
			chfs_vnode_cache_free(this);
			this = next;
		}
		hash[i] = NULL;
	}
}


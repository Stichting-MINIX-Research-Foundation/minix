/*	$NetBSD: chfs_malloc.c,v 1.4 2012/10/19 12:44:39 ttoth Exp $	*/

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

pool_cache_t chfs_vnode_cache;
pool_cache_t chfs_nrefs_cache;
pool_cache_t chfs_flash_vnode_cache;
pool_cache_t chfs_flash_dirent_cache;
pool_cache_t chfs_flash_dnode_cache;
pool_cache_t chfs_node_frag_cache;
pool_cache_t chfs_tmp_dnode_cache;
pool_cache_t chfs_tmp_dnode_info_cache;

/* chfs_alloc_pool_caches - allocating pool caches */
int
chfs_alloc_pool_caches(void)
{
	chfs_vnode_cache = pool_cache_init(
		sizeof(struct chfs_vnode_cache),
		0, 0, 0, "chfs_vnode_cache", NULL, IPL_NONE, NULL, NULL,
		NULL);
	if (!chfs_vnode_cache)
		goto err_vnode;

	chfs_nrefs_cache = pool_cache_init(
		(REFS_BLOCK_LEN + 1) * sizeof(struct chfs_node_ref), 0, 0,
		0, "chfs_nrefs_pool", NULL, IPL_NONE, NULL, NULL, NULL);
	if (!chfs_nrefs_cache)
		goto err_nrefs;

	chfs_flash_vnode_cache = pool_cache_init(
		sizeof(struct chfs_flash_vnode), 0, 0, 0,
		"chfs_flash_vnode_pool", NULL, IPL_NONE, NULL, NULL, NULL);
	if (!chfs_flash_vnode_cache)
		goto err_flash_vnode;

	chfs_flash_dirent_cache = pool_cache_init(
		sizeof(struct chfs_flash_dirent_node), 0, 0, 0,
		"chfs_flash_dirent_pool", NULL, IPL_NONE, NULL, NULL, NULL);
	if (!chfs_flash_dirent_cache)
		goto err_flash_dirent;

	chfs_flash_dnode_cache = pool_cache_init(
		sizeof(struct chfs_flash_data_node), 0, 0, 0,
		"chfs_flash_dnode_pool", NULL, IPL_NONE, NULL, NULL, NULL);
	if (!chfs_flash_dnode_cache)
		goto err_flash_dnode;

	chfs_node_frag_cache = pool_cache_init(
		sizeof(struct chfs_node_frag), 0, 0, 0,
		"chfs_node_frag_pool", NULL, IPL_NONE, NULL, NULL, NULL);
	if (!chfs_node_frag_cache)
		goto err_node_frag;

	chfs_tmp_dnode_cache = pool_cache_init(
		sizeof(struct chfs_tmp_dnode), 0, 0, 0,
		"chfs_tmp_dnode_pool", NULL, IPL_NONE, NULL, NULL, NULL);
	if (!chfs_tmp_dnode_cache)
		goto err_tmp_dnode;

	chfs_tmp_dnode_info_cache = pool_cache_init(
		sizeof(struct chfs_tmp_dnode_info), 0, 0, 0,
		"chfs_tmp_dnode_info_pool", NULL, IPL_NONE, NULL, NULL, NULL);
	if (!chfs_tmp_dnode_info_cache)
		goto err_tmp_dnode_info;

	return 0;

err_tmp_dnode_info:
	pool_cache_destroy(chfs_tmp_dnode_cache);
err_tmp_dnode:
	pool_cache_destroy(chfs_node_frag_cache);
err_node_frag:
	pool_cache_destroy(chfs_flash_dnode_cache);
err_flash_dnode:
	pool_cache_destroy(chfs_flash_dirent_cache);
err_flash_dirent:
	pool_cache_destroy(chfs_flash_vnode_cache);
err_flash_vnode:
	pool_cache_destroy(chfs_nrefs_cache);
err_nrefs:
	pool_cache_destroy(chfs_vnode_cache);
err_vnode:

	return ENOMEM;
}

/* chfs_destroy_pool_caches - destroying pool caches */
void
chfs_destroy_pool_caches(void)
{
	if (chfs_vnode_cache)
		pool_cache_destroy(chfs_vnode_cache);

	if (chfs_nrefs_cache)
		pool_cache_destroy(chfs_nrefs_cache);

	if (chfs_flash_vnode_cache)
		pool_cache_destroy(chfs_flash_vnode_cache);

	if (chfs_flash_dirent_cache)
		pool_cache_destroy(chfs_flash_dirent_cache);

	if (chfs_flash_dnode_cache)
		pool_cache_destroy(chfs_flash_dnode_cache);

	if (chfs_node_frag_cache)
		pool_cache_destroy(chfs_node_frag_cache);

	if (chfs_tmp_dnode_cache)
		pool_cache_destroy(chfs_tmp_dnode_cache);

	if (chfs_tmp_dnode_info_cache)
		pool_cache_destroy(chfs_tmp_dnode_info_cache);
}

/* chfs_vnode_cache_alloc - allocating and initializing a vnode cache */
struct chfs_vnode_cache *
chfs_vnode_cache_alloc(ino_t vno)
{
	struct chfs_vnode_cache* vc;
	vc = pool_cache_get(chfs_vnode_cache, PR_WAITOK);

	memset(vc, 0, sizeof(*vc));
	vc->vno = vno;
	/* vnode cache is the last element of all chain */
	vc->v = (void *)vc;
	vc->dirents = (void *)vc;
	vc->dnode = (void *)vc;
	TAILQ_INIT(&vc->scan_dirents);
	vc->highest_version = 0;

	return vc;
}

/* chfs_vnode_cache_free - freeing a vnode cache */
void
chfs_vnode_cache_free(struct chfs_vnode_cache *vc)
{
	pool_cache_put(chfs_vnode_cache, vc);
}

/*
 * chfs_alloc_refblock - allocating a refblock
 *
 * Returns a pointer of the first element in the block.
 *
 * We are not allocating just one node ref, instead we allocating REFS_BLOCK_LEN
 * number of node refs, the last element will be a pointer to the next block.
 * We do this, because we need a chain of nodes which have been ordered by the
 * physical address of them.
 *
 */
struct chfs_node_ref*
chfs_alloc_refblock(void)
{
	int i;
	struct chfs_node_ref *nref;
	nref = pool_cache_get(chfs_nrefs_cache, PR_WAITOK);

	for (i = 0; i < REFS_BLOCK_LEN; i++) {
		nref[i].nref_lnr = REF_EMPTY_NODE;
		nref[i].nref_next = NULL;
	}
	i = REFS_BLOCK_LEN;
	nref[i].nref_lnr = REF_LINK_TO_NEXT;
	nref[i].nref_next = NULL;

	return nref;
}

/* chfs_free_refblock - freeing a refblock */
void
chfs_free_refblock(struct chfs_node_ref *nref)
{
	pool_cache_put(chfs_nrefs_cache, nref);
}

/*
 * chfs_alloc_node_ref - allocating a node ref from a refblock
 *
 * Allocating a node ref from a refblock, it there isn't any free element in the
 * block, a new block will be allocated and be linked to the current block.
 */
struct chfs_node_ref*
chfs_alloc_node_ref(struct chfs_eraseblock *cheb)
{
	struct chfs_node_ref *nref, *new, *old;
	old = cheb->last_node;
	nref = cheb->last_node;

	if (!nref) {
		/* There haven't been any nref allocated for this block yet */
		nref = chfs_alloc_refblock();

		cheb->first_node = nref;
		cheb->last_node = nref;
		nref->nref_lnr = cheb->lnr;
		KASSERT(cheb->lnr == nref->nref_lnr);

		return nref;
	}

	nref++;
	if (nref->nref_lnr == REF_LINK_TO_NEXT) {
		/* this was the last element, allocate a new block */
		new = chfs_alloc_refblock();
		nref->nref_next = new;
		nref = new;
	}

	cheb->last_node = nref;
	nref->nref_lnr = cheb->lnr;

	KASSERT(old->nref_lnr == nref->nref_lnr &&
	    nref->nref_lnr == cheb->lnr);

	return nref;
}

/* chfs_free_node_refs - freeing an eraseblock's node refs */
void
chfs_free_node_refs(struct chfs_eraseblock *cheb)
{
	struct chfs_node_ref *nref, *block;

	block = nref = cheb->first_node;

	while (nref) {
		if (nref->nref_lnr == REF_LINK_TO_NEXT) {
			nref = nref->nref_next;
			chfs_free_refblock(block);
			block = nref;
			continue;
		}
		nref++;
	}
}

/* chfs_alloc_dirent - allocating a directory entry */
struct chfs_dirent*
chfs_alloc_dirent(int namesize)
{
	struct chfs_dirent *ret;
	size_t size = sizeof(struct chfs_dirent) + namesize;

	ret = kmem_alloc(size, KM_SLEEP);

	return ret;
}

/* chfs_free_dirent - freeing a directory entry */
void
chfs_free_dirent(struct chfs_dirent *dirent)
{
	size_t size = sizeof(struct chfs_dirent) + dirent->nsize + 1;

	kmem_free(dirent, size);
}

/* chfs_alloc_full_dnode - allocating a full data node */
struct chfs_full_dnode*
chfs_alloc_full_dnode(void)
{
	struct chfs_full_dnode *ret;
	ret = kmem_alloc(sizeof(struct chfs_full_dnode), KM_SLEEP);
	ret->nref = NULL;
	ret->frags = 0;
	return ret;
}

/* chfs_free_full_dnode - freeing a full data node */
void
chfs_free_full_dnode(struct chfs_full_dnode *fd)
{
	kmem_free(fd,(sizeof(struct chfs_full_dnode)));
}

/* chfs_alloc_flash_vnode - allocating vnode info (used on flash) */
struct chfs_flash_vnode*
chfs_alloc_flash_vnode(void)
{
	struct chfs_flash_vnode *ret;
	ret = pool_cache_get(chfs_flash_vnode_cache, 0);
	return ret;
}

/* chfs_free_flash_vnode - freeing vnode info */
void
chfs_free_flash_vnode(struct chfs_flash_vnode *fvnode)
{
	pool_cache_put(chfs_flash_vnode_cache, fvnode);
}

/* chfs_alloc_flash_dirent - allocating a directory entry (used on flash) */
struct chfs_flash_dirent_node*
chfs_alloc_flash_dirent(void)
{
	struct chfs_flash_dirent_node *ret;
	ret = pool_cache_get(chfs_flash_dirent_cache, 0);
	return ret;
}

/* chfs_free_flash_dirent - freeing a (flash) directory entry */
void
chfs_free_flash_dirent(struct chfs_flash_dirent_node *fdnode)
{
	pool_cache_put(chfs_flash_dirent_cache, fdnode);
}

/* chfs_alloc_flash_dnode - allocating a data node (used on flash) */
struct chfs_flash_data_node*
chfs_alloc_flash_dnode(void)
{
	struct chfs_flash_data_node *ret;
	ret = pool_cache_get(chfs_flash_dnode_cache, 0);
	return ret;
}

/* chfs_free_flash_dnode - freeing a (flash) data node */
void
chfs_free_flash_dnode(struct chfs_flash_data_node *fdnode)
{
	pool_cache_put(chfs_flash_dnode_cache, fdnode);
}

/* chfs_alloc_node_frag - allocating a fragment of a node */
struct chfs_node_frag*
chfs_alloc_node_frag(void)
{
	struct chfs_node_frag *ret;
	ret = pool_cache_get(chfs_node_frag_cache, 0);
	return ret;
}

/* chfs_free_node_frag - freeing a fragment of a node */
void
chfs_free_node_frag(struct chfs_node_frag *frag)
{
	pool_cache_put(chfs_node_frag_cache, frag);
}

/* chfs_alloc_tmp_dnode - allocating a temporarly used dnode */
struct chfs_tmp_dnode *
chfs_alloc_tmp_dnode(void)
{
	struct chfs_tmp_dnode *ret;
	ret = pool_cache_get(chfs_tmp_dnode_cache, 0);
	ret->next = NULL;
	return ret;
}

/* chfs_free_tmp_dnode - freeing a temporarly used dnode */
void
chfs_free_tmp_dnode(struct chfs_tmp_dnode *td)
{
	pool_cache_put(chfs_tmp_dnode_cache, td);
}

/* chfs_alloc_tmp_dnode_info - allocating a temporarly used dnode descriptor */
struct chfs_tmp_dnode_info *
chfs_alloc_tmp_dnode_info(void)
{
	struct chfs_tmp_dnode_info *ret;
	ret = pool_cache_get(chfs_tmp_dnode_info_cache, 0);
	ret->tmpnode = NULL;
	return ret;
}

/* chfs_free_tmp_dnode_info - freeing a temporarly used dnode descriptor */
void
chfs_free_tmp_dnode_info(struct chfs_tmp_dnode_info *di)
{
	pool_cache_put(chfs_tmp_dnode_info_cache, di);
}


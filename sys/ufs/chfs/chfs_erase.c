/*	$NetBSD: chfs_erase.c,v 1.1 2011/11/24 15:51:31 ahoka Exp $	*/

/*-
 * Copyright (c) 2010 Department of Software Engineering,
 *		      University of Szeged, Hungary
 * Copyright (c) 2010 David Tengeri <dtengeri@inf.u-szeged.hu>
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

/*
 * chfs_erase.c
 *
 * Copyright (C) 2010  David Tengeri <dtengeri@inf.u-szeged.hu>,
 *                     ...
 *                     University of Szeged, Hungary
 */

#include "chfs.h"


/**
 * chfs_remap_leb - unmap and then map a leb
 * @chmp: chfs mount structure
 *
 * This function gets an eraseblock from the erasable queue, unmaps it through
 * EBH and maps another eraseblock to the same LNR.
 * EBH will find a free eraseblock if any or will erase one if there isn't any
 * free, just dirty block.
 *
 * Returns zero on case of success, errorcode otherwise.
 *
 * Needs more brainstorming here.
 */
int
chfs_remap_leb(struct chfs_mount *chmp)
{
	int err;
	struct chfs_eraseblock *cheb;
	dbg("chfs_remap_leb\n");
	uint32_t dirty, unchecked, used, free, wasted;

	//dbg("chmp->chm_nr_erasable_blocks: %d\n", chmp->chm_nr_erasable_blocks);
	//dbg("ltree: %p ecl: %p\n", &chmp->chm_ebh->ltree_lock, &chmp->chm_lock_sizes);
	KASSERT(!rw_write_held(&chmp->chm_lock_wbuf));
	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));
	KASSERT(mutex_owned(&chmp->chm_lock_sizes));

	if (!chmp->chm_nr_erasable_blocks) {
		//TODO
		/* We don't have any erasable blocks, need to check if there are
		 * blocks on erasable_pending_wbuf_queue, flush the data and then
		 * we can remap it.
		 * If there aren't any blocks on that list too, we need to GC?
		 */
		if (!TAILQ_EMPTY(&chmp->chm_erasable_pending_wbuf_queue)) {
			cheb = TAILQ_FIRST(&chmp->chm_erasable_pending_wbuf_queue);
			TAILQ_REMOVE(&chmp->chm_erasable_pending_wbuf_queue, cheb, queue);
			if (chmp->chm_wbuf_len) {
				mutex_exit(&chmp->chm_lock_sizes);
				chfs_flush_pending_wbuf(chmp);
				mutex_enter(&chmp->chm_lock_sizes);
			}
			TAILQ_INSERT_TAIL(&chmp->chm_erase_pending_queue, cheb, queue);
			chmp->chm_nr_erasable_blocks++;
		} else {
			/* We can't delete any block. */
			//FIXME should we return ENOSPC?
			return ENOSPC;
		}
	}
	cheb = TAILQ_FIRST(&chmp->chm_erase_pending_queue);
	TAILQ_REMOVE(&chmp->chm_erase_pending_queue, cheb, queue);
	chmp->chm_nr_erasable_blocks--;
	
	dirty = cheb->dirty_size;
	unchecked = cheb->unchecked_size;
	used = cheb->used_size;
	free = cheb->free_size;
	wasted = cheb->wasted_size;

	// Free allocated node references for this eraseblock
	chfs_free_node_refs(cheb);

	err = chfs_unmap_leb(chmp, cheb->lnr);
	if (err)
		return err;

	err = chfs_map_leb(chmp, cheb->lnr);
	if (err)
		return err;
	// Reset state to default and change chmp sizes too 
	chfs_change_size_dirty(chmp, cheb, -dirty);
	chfs_change_size_unchecked(chmp, cheb, -unchecked);
	chfs_change_size_used(chmp, cheb, -used);
	chfs_change_size_free(chmp, cheb, chmp->chm_ebh->eb_size - free);
	chfs_change_size_wasted(chmp, cheb, -wasted);

	KASSERT(cheb->dirty_size == 0);
	KASSERT(cheb->unchecked_size == 0);
	KASSERT(cheb->used_size == 0);
	KASSERT(cheb->free_size == chmp->chm_ebh->eb_size);
	KASSERT(cheb->wasted_size == 0);

	cheb->first_node = NULL;
	cheb->last_node  = NULL;
	//put it to free_queue
	TAILQ_INSERT_TAIL(&chmp->chm_free_queue, cheb, queue);
	chmp->chm_nr_free_blocks++;
	dbg("remaped (free: %d, erasable: %d)\n", chmp->chm_nr_free_blocks, chmp->chm_nr_erasable_blocks);
	KASSERT(!TAILQ_EMPTY(&chmp->chm_free_queue));

	return 0;
}

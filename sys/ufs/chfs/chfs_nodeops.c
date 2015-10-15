/*	$NetBSD: chfs_nodeops.c,v 1.4 2013/12/09 09:35:17 wiz Exp $	*/

/*-
 * Copyright (c) 2010 Department of Software Engineering,
 *		      University of Szeged, Hungary
 * Copyright (C) 2010 David Tengeri <dtengeri@inf.u-szeged.hu>
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

/*
 * chfs_update_eb_dirty - updates dirty and free space, first and
 *			      last node references
 * Returns zero in case of success, 1 in case of fail.
 */
int
chfs_update_eb_dirty(struct chfs_mount *chmp,
    struct chfs_eraseblock *cheb, uint32_t size)
{
	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));
	KASSERT(!mutex_owned(&chmp->chm_lock_sizes));

	if (!size)
		return 0;

	if (size > cheb->free_size) {
		chfs_err("free_size (%d) is less than dirty space (%d) "
		    "on block (%d)\n", cheb->free_size, size, cheb->lnr);
		return 1;
	}
	mutex_enter(&chmp->chm_lock_sizes);
	chfs_change_size_free(chmp, cheb, -size);
	chfs_change_size_dirty(chmp, cheb, size);
	mutex_exit(&chmp->chm_lock_sizes);
	return 0;
}

/*
 * chfs_add_node_to_list - adds a data node ref to vnode cache's dnode list
 * This function inserts a data node ref to the list of vnode cache.
 * The list is sorted by data node's lnr and offset.
 */
void
chfs_add_node_to_list(struct chfs_mount *chmp,
    struct chfs_vnode_cache *vc,
    struct chfs_node_ref *new, struct chfs_node_ref **list)
{
	KASSERT(mutex_owned(&chmp->chm_lock_vnocache));

	struct chfs_node_ref *nextref = *list;
	struct chfs_node_ref *prevref = NULL;

	while (nextref && nextref != (struct chfs_node_ref *)vc &&
	    (nextref->nref_lnr <= new->nref_lnr)) {
		if (nextref->nref_lnr == new->nref_lnr) {
			while (nextref && nextref !=
			    (struct chfs_node_ref *)vc &&
			    (CHFS_GET_OFS(nextref->nref_offset) <
				CHFS_GET_OFS(new->nref_offset))) {
				prevref = nextref;
				nextref = nextref->nref_next;
			}
			break;
		}
		prevref = nextref;
		nextref = nextref->nref_next;
	}

	if (nextref && nextref != (struct chfs_node_ref *)vc &&
	    nextref->nref_lnr == new->nref_lnr &&
	    CHFS_GET_OFS(nextref->nref_offset) ==
	    CHFS_GET_OFS(new->nref_offset)) {
		new->nref_next = nextref->nref_next;
		chfs_mark_node_obsolete(chmp, nextref);
	} else {
		new->nref_next = nextref;
	}

	KASSERT(new->nref_next != NULL);

	if (prevref) {
		prevref->nref_next = new;
	} else {
		*list = new;
	}
}

/*
 * chfs_remove_node_from_list - removes a node from a list
 * Usually used for removing data nodes.
 */
void
chfs_remove_node_from_list(struct chfs_mount *chmp,
	struct chfs_vnode_cache *vc,
	struct chfs_node_ref *old_nref, struct chfs_node_ref **list)
{
	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));
	KASSERT(mutex_owned(&chmp->chm_lock_vnocache));

	struct chfs_node_ref *tmpnref;

	if (*list == (struct chfs_node_ref *)vc) {
		/* list is empty */
		return;
	}

	KASSERT(old_nref->nref_next != NULL);

	if (*list == old_nref) {
		*list = old_nref->nref_next;
	} else {
		tmpnref = *list;
		while (tmpnref->nref_next &&
			tmpnref->nref_next != (struct chfs_node_ref *)vc) {
			if (tmpnref->nref_next == old_nref) {
				tmpnref->nref_next = old_nref->nref_next;
				break;
			}
			tmpnref = tmpnref->nref_next;
		}
	}
}

/*
 * chfs_remove_and_obsolete - removes a node from a list and obsoletes the nref
 * We should use this function carefully on data nodes, 
 * because removing a frag will also obsolete the node ref.
 */
void
chfs_remove_and_obsolete(struct chfs_mount *chmp,
	struct chfs_vnode_cache *vc,
	struct chfs_node_ref *old_nref, struct chfs_node_ref **list)
{
	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));
	KASSERT(mutex_owned(&chmp->chm_lock_vnocache));

	chfs_remove_node_from_list(chmp, vc, old_nref, list);

	dbg("[MARK] vno: %llu lnr: %u ofs: %u\n", vc->vno, old_nref->nref_lnr,
		old_nref->nref_offset);
	chfs_mark_node_obsolete(chmp, old_nref);
}

/* chfs_add_fd_to_inode - adds a directory entry to an inode */
void
chfs_add_fd_to_inode(struct chfs_mount *chmp,
    struct chfs_inode *parent, struct chfs_dirent *new)
{
	struct chfs_dirent *fd, *tmpfd;

	/* update highest version */
	if (new->version > parent->chvc->highest_version) {
		parent->chvc->highest_version = new->version;
	}

	TAILQ_FOREACH_SAFE(fd, &parent->dents, fds, tmpfd) {
		if (fd->nhash > new->nhash) {
			/* insert new before fd */
			TAILQ_INSERT_BEFORE(fd, new, fds);
			return;
		} else if (fd->nhash == new->nhash &&
		    !strcmp(fd->name, new->name)) {
			if (new->version > fd->version) {
				/* replace fd with new */
				TAILQ_INSERT_BEFORE(fd, new, fds);
				TAILQ_REMOVE(&parent->dents, fd, fds);
				if (fd->nref) {
					mutex_enter(&chmp->chm_lock_vnocache);
					chfs_remove_and_obsolete(chmp, parent->chvc, fd->nref,
						&parent->chvc->dirents);
					mutex_exit(&chmp->chm_lock_vnocache);
				}
				chfs_free_dirent(fd);
			} else {
				/* new is older (normally it's not an option) */
				chfs_mark_node_obsolete(chmp, new->nref);
				chfs_free_dirent(new);
			}
			return;
		}
	}
	/* if we couldnt fit it elsewhere, lets add to the end */
	/* FIXME insert tail or insert head? */
	TAILQ_INSERT_HEAD(&parent->dents, new, fds);
}


/* chfs_add_vnode_ref_to_vc - adds a vnode info to the vnode cache */
void
chfs_add_vnode_ref_to_vc(struct chfs_mount *chmp,
    struct chfs_vnode_cache *vc, struct chfs_node_ref *new)
{
	KASSERT(mutex_owned(&chmp->chm_lock_vnocache));
	struct chfs_node_ref *nref;

	/* store only the last one, drop the others */
	while (vc->v != (struct chfs_node_ref *)vc) {
		nref = vc->v;
		chfs_remove_and_obsolete(chmp, vc, nref, &vc->v);
	}

	new->nref_next = (struct chfs_node_ref *)vc;
	vc->v = new;
}

/* chfs_nref_next - step to the next in-memory nref */
struct chfs_node_ref *
chfs_nref_next(struct chfs_node_ref *nref)
{
	nref++;
	if (nref->nref_lnr == REF_LINK_TO_NEXT) {
		/* end of chain */
		if (!nref->nref_next)
			return NULL;

		/* link to the next block */
		nref = nref->nref_next;
	}
	/* end of chain */
	if (nref->nref_lnr == REF_EMPTY_NODE)
		return NULL;

	return nref;
}

/* chfs_nref_len - calculates the length of an nref */
int
chfs_nref_len(struct chfs_mount *chmp,
    struct chfs_eraseblock *cheb, struct chfs_node_ref *nref)
{
	struct chfs_node_ref *next;

	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));

	if (!cheb)
		cheb = &chmp->chm_blocks[nref->nref_lnr];

	next = chfs_nref_next(nref);

	if (!next) {
		return chmp->chm_ebh->eb_size - cheb->free_size -
		    CHFS_GET_OFS(nref->nref_offset);
	}
	return CHFS_GET_OFS(next->nref_offset) -
	    CHFS_GET_OFS(nref->nref_offset);
}

/* chfs_mark_node_obsolete - marks a node as obsolete */
void
chfs_mark_node_obsolete(struct chfs_mount *chmp,
    struct chfs_node_ref *nref)
{
	int len;
	struct chfs_eraseblock *cheb;

	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));

	KASSERT(!CHFS_REF_OBSOLETE(nref));

	KASSERT(nref->nref_lnr <= chmp->chm_ebh->peb_nr);
	cheb = &chmp->chm_blocks[nref->nref_lnr];

#ifdef DIAGNOSTIC
	if (cheb->used_size + cheb->free_size + cheb->dirty_size +
	    cheb->unchecked_size + cheb->wasted_size != chmp->chm_ebh->eb_size) {
		dbg("eraseblock leak detected!\nused: %u\nfree: %u\n"
		    "dirty: %u\nunchecked: %u\nwasted: %u\ntotal: %u\nshould be: %zu\n",
		    cheb->used_size, cheb->free_size, cheb->dirty_size,
		    cheb->unchecked_size, cheb->wasted_size, cheb->used_size + cheb->free_size +
		    cheb->dirty_size + cheb->unchecked_size + cheb->wasted_size,
		    chmp->chm_ebh->eb_size);
	}
#endif

	len = chfs_nref_len(chmp, cheb, nref);

	mutex_enter(&chmp->chm_lock_sizes);
	
	if (CHFS_REF_FLAGS(nref) == CHFS_UNCHECKED_NODE_MASK) {
		chfs_change_size_unchecked(chmp, cheb, -len);
	} else {
		chfs_change_size_used(chmp, cheb, -len);

		KASSERT(cheb->used_size <= chmp->chm_ebh->eb_size);
	}
	chfs_change_size_dirty(chmp, cheb, len);

#ifdef DIAGNOSTIC
	if (cheb->used_size + cheb->free_size + cheb->dirty_size +
	    cheb->unchecked_size + cheb->wasted_size != chmp->chm_ebh->eb_size) {
		panic("eraseblock leak detected!\nused: %u\nfree: %u\n"
		    "dirty: %u\nunchecked: %u\nwasted: %u\ntotal: %u\nshould be: %zu\n",
		    cheb->used_size, cheb->free_size, cheb->dirty_size,
		    cheb->unchecked_size, cheb->wasted_size, cheb->used_size + cheb->free_size +
		    cheb->dirty_size + cheb->unchecked_size + cheb->wasted_size,
		    chmp->chm_ebh->eb_size);
	}
#endif
	nref->nref_offset = CHFS_GET_OFS(nref->nref_offset) |
	    CHFS_OBSOLETE_NODE_MASK;

	if (chmp->chm_flags & CHFS_MP_FLAG_SCANNING) {
		/*Scan is in progress, do nothing now*/
		mutex_exit(&chmp->chm_lock_sizes);
		return;
	}

	if (cheb == chmp->chm_nextblock) {
		dbg("Not moving nextblock to dirty/erase_pending list\n");
	} else if (!cheb->used_size && !cheb->unchecked_size) {
		if (cheb == chmp->chm_gcblock) {
			dbg("gcblock is completely dirtied\n");
			chmp->chm_gcblock = NULL;
		} else {
			/* remove from a tailq, but we don't know which tailq contains this cheb
			 * so we remove it from the dirty list now */
			//TAILQ_REMOVE(&chmp->chm_dirty_queue, cheb, queue);
			int removed = 0;
			struct chfs_eraseblock *eb, *tmpeb;
			//XXX ugly code
			TAILQ_FOREACH_SAFE(eb, &chmp->chm_free_queue, queue, tmpeb) {
				if (eb == cheb) {
					TAILQ_REMOVE(&chmp->chm_free_queue, cheb, queue);
					removed = 1;
					break;
				}
			}
			if (removed == 0) {
				TAILQ_FOREACH_SAFE(eb, &chmp->chm_dirty_queue, queue, tmpeb) {
					if (eb == cheb) {
						TAILQ_REMOVE(&chmp->chm_dirty_queue, cheb, queue);
						removed = 1;
						break;
					}
				}
			}
			if (removed == 0) {
				TAILQ_FOREACH_SAFE(eb, &chmp->chm_very_dirty_queue, queue, tmpeb) {
					if (eb == cheb) {
						TAILQ_REMOVE(&chmp->chm_very_dirty_queue, cheb, queue);
						removed = 1;
						break;
					}
				}
			}
			if (removed == 0) {
				TAILQ_FOREACH_SAFE(eb, &chmp->chm_clean_queue, queue, tmpeb) {
					if (eb == cheb) {
						TAILQ_REMOVE(&chmp->chm_clean_queue, cheb, queue);
						removed = 1;
						break;
					}
				}
			}
		}
		if (chmp->chm_wbuf_len) {
			dbg("Adding block to erasable pending wbuf queue\n");
			TAILQ_INSERT_TAIL(&chmp->chm_erasable_pending_wbuf_queue,
			    cheb, queue);
		} else {
			TAILQ_INSERT_TAIL(&chmp->chm_erase_pending_queue,
			    cheb, queue);
			chmp->chm_nr_erasable_blocks++;
		}
		chfs_remap_leb(chmp);
	} else if (cheb == chmp->chm_gcblock) {
		dbg("Not moving gcblock to dirty list\n");
	} else if (cheb->dirty_size > MAX_DIRTY_TO_CLEAN &&
	    cheb->dirty_size - len <= MAX_DIRTY_TO_CLEAN) {
		dbg("Freshly dirtied, remove it from clean queue and "
		    "add it to dirty\n");
		TAILQ_REMOVE(&chmp->chm_clean_queue, cheb, queue);
		TAILQ_INSERT_TAIL(&chmp->chm_dirty_queue, cheb, queue);
	} else if (VERY_DIRTY(chmp, cheb->dirty_size) &&
	    !VERY_DIRTY(chmp, cheb->dirty_size - len)) {
		dbg("Becomes now very dirty, remove it from dirty "
		    "queue and add it to very dirty\n");
		TAILQ_REMOVE(&chmp->chm_dirty_queue, cheb, queue);
		TAILQ_INSERT_TAIL(&chmp->chm_very_dirty_queue, cheb, queue);
	} else {
		dbg("Leave cheb where it is\n");
	}
	mutex_exit(&chmp->chm_lock_sizes);
	return;
}

/*
 * chfs_close_eraseblock - close an eraseblock
 *
 * This function close the physical chain of the nodes on the eraseblock,
 * convert its free size to dirty and add it to clean, dirty or very dirty list.
 */
int
chfs_close_eraseblock(struct chfs_mount *chmp,
    struct chfs_eraseblock *cheb)
{
	uint32_t offset;
	struct chfs_node_ref *nref;

	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));

	offset = chmp->chm_ebh->eb_size - cheb->free_size;

	// Close the chain
	nref = chfs_alloc_node_ref(cheb);
	if (!nref)
		return ENOMEM;

	nref->nref_next = NULL;
	nref->nref_offset = offset;

	// Mark space as dirty
	chfs_update_eb_dirty(chmp, cheb, cheb->free_size);

	if (cheb->dirty_size < MAX_DIRTY_TO_CLEAN) {
		TAILQ_INSERT_TAIL(&chmp->chm_clean_queue, cheb, queue);
	} else if (VERY_DIRTY(chmp, cheb->dirty_size)) {
		TAILQ_INSERT_TAIL(&chmp->chm_very_dirty_queue, cheb, queue);
	} else {
		TAILQ_INSERT_TAIL(&chmp->chm_dirty_queue, cheb, queue);
	}
	return 0;
}

/*
 * chfs_reserve_space_normal - 
 * checks available space and calls chfs_reserve_space
 * used during writing
 */
int
chfs_reserve_space_normal(struct chfs_mount *chmp, uint32_t size, int prio)
{
	int ret;

	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));

	mutex_enter(&chmp->chm_lock_sizes);
	while (chmp->chm_nr_free_blocks + chmp->chm_nr_erasable_blocks < chmp->chm_resv_blocks_write) {
		dbg("free: %d, erasable: %d, resv: %d\n", chmp->chm_nr_free_blocks, chmp->chm_nr_erasable_blocks, chmp->chm_resv_blocks_write);
		uint32_t avail, dirty;
		if (prio == ALLOC_DELETION && chmp->chm_nr_free_blocks + chmp->chm_nr_erasable_blocks >= chmp->chm_resv_blocks_deletion)
			break;

		dirty = chmp->chm_dirty_size - chmp->chm_nr_erasable_blocks * chmp->chm_ebh->eb_size + chmp->chm_unchecked_size;
		if (dirty < chmp->chm_nospc_dirty) {
			dbg("dirty: %u < nospc_dirty: %u\n", dirty, chmp->chm_nospc_dirty);
			ret = ENOSPC;
			mutex_exit(&chmp->chm_lock_sizes);
			goto out;
		}

		avail = chmp->chm_free_size - (chmp->chm_resv_blocks_write * chmp->chm_ebh->eb_size);
		if (size > avail) {
			dbg("size: %u > avail: %u\n", size, avail);
			ret = ENOSPC;
			mutex_exit(&chmp->chm_lock_sizes);
			goto out;
		}

		mutex_exit(&chmp->chm_lock_sizes);
		ret = chfs_gcollect_pass(chmp);
		mutex_enter(&chmp->chm_lock_sizes);

		if (chmp->chm_nr_erasable_blocks ||
		    !TAILQ_EMPTY(&chmp->chm_erasable_pending_wbuf_queue) ||
		    ret == EAGAIN) {
			ret = chfs_remap_leb(chmp);
		}

		if (ret) {
			mutex_exit(&chmp->chm_lock_sizes);
			goto out;
		}
	}

	mutex_exit(&chmp->chm_lock_sizes);
	ret = chfs_reserve_space(chmp, size);
out:
	return ret;
}


/* chfs_reserve_space_gc - tries to reserve space for GC */
int
chfs_reserve_space_gc(struct chfs_mount *chmp, uint32_t size)
{
	int ret;

	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));

	mutex_enter(&chmp->chm_lock_sizes);
	chfs_remap_leb(chmp);

	if (size > chmp->chm_free_size) {
		dbg("size: %u\n", size);
		mutex_exit(&chmp->chm_lock_sizes);
		return ENOSPC;
	}

	mutex_exit(&chmp->chm_lock_sizes);
	ret = chfs_reserve_space(chmp, size);
	return ret;
}

/*
 * chfs_reserve_space - finds a block which free size is >= requested size
 * Returns zero in case of success, error code in case of fail.
 */
int
chfs_reserve_space(struct chfs_mount *chmp, uint32_t size)
{
	//TODO define minimum reserved blocks, which is needed for writing
	//TODO check we have enough free blocks to write
	//TODO if no: need erase and GC

	int err;
	struct chfs_eraseblock *cheb;

	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));
	KASSERT(!mutex_owned(&chmp->chm_lock_sizes));

	cheb = chmp->chm_nextblock;
	if (cheb && size > cheb->free_size) {
		dbg("size: %u > free_size: %u\n", size, cheb->free_size);
		/*
		 * There isn't enough space on this eraseblock, we mark this as
		 * dirty and close the physical chain of the node refs.
		 */
		//Write out pending data if any
		if (chmp->chm_wbuf_len) {
			chfs_flush_pending_wbuf(chmp);
			//FIXME need goto restart here?
		}

		while (chmp->chm_wbuf_ofs < chmp->chm_ebh->eb_size) {
			dbg("wbuf ofs: %zu - eb_size: %zu\n",
			    chmp->chm_wbuf_ofs, chmp->chm_ebh->eb_size);
			chfs_flush_pending_wbuf(chmp);
		}

		if (!(chmp->chm_wbuf_ofs % chmp->chm_ebh->eb_size) && !chmp->chm_wbuf_len)
			chmp->chm_wbuf_ofs = 0xffffffff;

		err = chfs_close_eraseblock(chmp, cheb);
		if (err)
			return err;

		cheb = NULL;
	}
	if (!cheb) {
		//get a block for nextblock
		if (TAILQ_EMPTY(&chmp->chm_free_queue)) {
			// If this succeeds there will be a block on free_queue
			dbg("cheb remap (free: %d)\n", chmp->chm_nr_free_blocks);
			err = chfs_remap_leb(chmp);
			if (err)
				return err;
		}
		cheb = TAILQ_FIRST(&chmp->chm_free_queue);
		TAILQ_REMOVE(&chmp->chm_free_queue, cheb, queue);
		chmp->chm_nextblock = cheb;
		chmp->chm_nr_free_blocks--;
	}

	return 0;
}


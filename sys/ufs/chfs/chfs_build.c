/*	$NetBSD: chfs_build.c,v 1.5 2012/10/19 12:44:39 ttoth Exp $	*/

/*-
 * Copyright (c) 2010 Department of Software Engineering,
 *		      University of Szeged, Hungary
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
 * chfs_calc_trigger_levels - setup filesystem parameters
 * Setups filesystem parameters (reserved blocks and GC trigger level)
 * for a specific flash.
 */
void
chfs_calc_trigger_levels(struct chfs_mount *chmp)
{
	uint32_t size;

	chmp->chm_resv_blocks_deletion = 2;

	size = chmp->chm_ebh->flash_size / 50;  /* 2% of flash size */
	size += chmp->chm_ebh->peb_nr * 100;
	size += chmp->chm_ebh->eb_size - 1;

	chmp->chm_resv_blocks_write =
	    chmp->chm_resv_blocks_deletion + (size / chmp->chm_ebh->eb_size);
	chmp->chm_resv_blocks_gctrigger = chmp->chm_resv_blocks_write + 1;
	chmp->chm_resv_blocks_gcmerge = chmp->chm_resv_blocks_deletion + 1;
	chmp->chm_vdirty_blocks_gctrigger = chmp->chm_resv_blocks_gctrigger * 10;

	chmp->chm_nospc_dirty =
	    chmp->chm_ebh->eb_size + (chmp->chm_ebh->flash_size / 100);
}


/*
 * chfs_build_set_vnodecache_nlink - set pvno and nlink in vnodecaches
 * Travels vc's directory entries and sets the pvno and nlink
 * attribute of the vnode where the dirent's vno points.
 */
void
chfs_build_set_vnodecache_nlink(struct chfs_mount *chmp,
    struct chfs_vnode_cache *vc)
{
	struct chfs_dirent *fd, *tmpfd;

	TAILQ_FOREACH_SAFE(fd, &vc->scan_dirents, fds, tmpfd) {
		struct chfs_vnode_cache *child_vc;

		if (!fd->vno)
			continue;

		mutex_enter(&chmp->chm_lock_vnocache);
		child_vc = chfs_vnode_cache_get(chmp, fd->vno);
		mutex_exit(&chmp->chm_lock_vnocache);
		if (!child_vc) {
			chfs_mark_node_obsolete(chmp, fd->nref);
			TAILQ_REMOVE(&vc->scan_dirents, fd, fds);
			continue;
		}
		if (fd->type == CHT_DIR) {
			if (child_vc->nlink < 1)
				child_vc->nlink = 1;

			if (child_vc->pvno) {
				chfs_err("found a hard link: child dir: %s"
				    ", (vno: %llu) of dir vno: %llu\n",
				    fd->name, (unsigned long long)fd->vno,
				    (unsigned long long)vc->vno);
			} else {
				child_vc->pvno = vc->vno;
			}
		}
		child_vc->nlink++;
		vc->nlink++;
	}
}

/*
 * chfs_build_remove_unlinked vnode
 */
void
chfs_build_remove_unlinked_vnode(struct chfs_mount *chmp,
    struct chfs_vnode_cache *vc,
    struct chfs_dirent_list *unlinked)
{
	struct chfs_node_ref *nref;
	struct chfs_dirent *fd, *tmpfd;

	dbg("START\n");
	dbg("vno: %llu\n", (unsigned long long)vc->vno);

	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));
	nref = vc->dnode;
	/* The vnode cache is at the end of the data node's chain */
	while (nref != (struct chfs_node_ref *)vc) {
		struct chfs_node_ref *next = nref->nref_next;
		dbg("mark dnode\n");
		chfs_mark_node_obsolete(chmp, nref);
		nref = next;
	}
	vc->dnode = (struct chfs_node_ref *)vc;
	nref = vc->dirents;
	/* The vnode cache is at the end of the dirent node's chain */
	while (nref != (struct chfs_node_ref *)vc) {
		struct chfs_node_ref *next = nref->nref_next;
		dbg("mark dirent\n");
		chfs_mark_node_obsolete(chmp, nref);
		nref = next;
	}
	vc->dirents = (struct chfs_node_ref *)vc;
	if (!TAILQ_EMPTY(&vc->scan_dirents)) {
		TAILQ_FOREACH_SAFE(fd, &vc->scan_dirents, fds, tmpfd) {
			struct chfs_vnode_cache *child_vc;
			dbg("dirent dump:\n");
			dbg(" ->vno:     %llu\n", (unsigned long long)fd->vno);
			dbg(" ->version: %llu\n", (unsigned long long)fd->version);
			dbg(" ->nhash:   0x%x\n", fd->nhash);
			dbg(" ->nsize:   %d\n", fd->nsize);
			dbg(" ->name:    %s\n", fd->name);
			dbg(" ->type:    %d\n", fd->type);
			TAILQ_REMOVE(&vc->scan_dirents, fd, fds);

			if (!fd->vno) {
				chfs_free_dirent(fd);
				continue;
			}
			mutex_enter(&chmp->chm_lock_vnocache);
			child_vc = chfs_vnode_cache_get(chmp, fd->vno);
			mutex_exit(&chmp->chm_lock_vnocache);
			if (!child_vc) {
				chfs_free_dirent(fd);
				continue;
			}
			/*
			 * Decrease nlink in child. If it is 0, add to unlinked
			 * dirents or just free it otherwise.
			 */
			child_vc->nlink--;

			if (!child_vc->nlink) {
				// XXX HEAD or TAIL?
				// original code did HEAD, but we could add
				// it to the TAIL easily with TAILQ.
				TAILQ_INSERT_TAIL(unlinked, fd, fds);
			} else {
				chfs_free_dirent(fd);
			}
		}
	} else {
		dbg("there are no scan dirents\n");
	}

	nref = vc->v;
	while ((struct chfs_vnode_cache *)nref != vc) {
		chfs_mark_node_obsolete(chmp, nref);
		nref = nref->nref_next;
	}
	vc->v = (struct chfs_node_ref *)vc;

	mutex_enter(&chmp->chm_lock_vnocache);
	if (vc->vno != CHFS_ROOTINO)
		vc->state = VNO_STATE_UNCHECKED;
	mutex_exit(&chmp->chm_lock_vnocache);
	dbg("END\n");
}

/*
 * chfs_build_filesystem - build in-memory representation of filesystem
 *
 * Step 1:
 * Scans through the eraseblocks mapped in EBH.
 * During scan builds up the map of vnodes and directory entries and puts them
 * into the vnode_cache.
 * Step 2:
 * Scans the directory tree and set the nlink in the vnode caches.
 * Step 3:
 * Scans vnode caches with nlink = 0
 */
int
chfs_build_filesystem(struct chfs_mount *chmp)
{
	int i,err = 0;
	struct chfs_vnode_cache *vc;
	struct chfs_dirent *fd, *tmpfd;
	struct chfs_node_ref **nref;
	struct chfs_dirent_list unlinked;
	struct chfs_vnode_cache *notregvc;

	TAILQ_INIT(&unlinked);

	mutex_enter(&chmp->chm_lock_mountfields);

	/* Step 1 */
	chmp->chm_flags |= CHFS_MP_FLAG_SCANNING;
	for (i = 0; i < chmp->chm_ebh->peb_nr; i++) {
		chmp->chm_blocks[i].lnr = i;
		chmp->chm_blocks[i].free_size = chmp->chm_ebh->eb_size;
		/* If the LEB is add to free list skip it. */
		if (chmp->chm_ebh->lmap[i] < 0) {
			TAILQ_INSERT_TAIL(&chmp->chm_free_queue,
			    &chmp->chm_blocks[i], queue);
			chmp->chm_nr_free_blocks++;
			continue;
		}

		err = chfs_scan_eraseblock(chmp, &chmp->chm_blocks[i]);
		switch (err) {
		case CHFS_BLK_STATE_FREE:
			chmp->chm_nr_free_blocks++;
			TAILQ_INSERT_TAIL(&chmp->chm_free_queue,
			    &chmp->chm_blocks[i], queue);
			break;
		case CHFS_BLK_STATE_CLEAN:
			TAILQ_INSERT_TAIL(&chmp->chm_clean_queue,
			    &chmp->chm_blocks[i], queue);
			break;
		case CHFS_BLK_STATE_PARTDIRTY:
			if (chmp->chm_blocks[i].free_size > chmp->chm_wbuf_pagesize &&
			    (!chmp->chm_nextblock ||
				chmp->chm_blocks[i].free_size >
				chmp->chm_nextblock->free_size)) {
				/* convert the old nextblock's free size to
				 * dirty and put it on a list */
				if (chmp->chm_nextblock) {
					err = chfs_close_eraseblock(chmp,
					    chmp->chm_nextblock);
					if (err)
						return err;
				}
				chmp->chm_nextblock = &chmp->chm_blocks[i];
			} else {
				/* convert the scanned block's free size to
				 * dirty and put it on a list */
				err = chfs_close_eraseblock(chmp,
				    &chmp->chm_blocks[i]);
				if (err)
					return err;
			}
			break;
		case CHFS_BLK_STATE_ALLDIRTY:
			/*
			 * The block has a valid EBH header, but it doesn't
			 * contain any valid data.
			 */
			TAILQ_INSERT_TAIL(&chmp->chm_erase_pending_queue,
			    &chmp->chm_blocks[i], queue);
			chmp->chm_nr_erasable_blocks++;
			break;
		default:
			/* It was an error, unknown  state */
			break;
		}

	}
	chmp->chm_flags &= ~CHFS_MP_FLAG_SCANNING;


	//TODO need bad block check (and bad block handling in EBH too!!)
	/* Now EBH only checks block is bad  during its scan operation.
	 * Need check at erase + write + read...
	 */

	/* Step 2 */
	chmp->chm_flags |= CHFS_MP_FLAG_BUILDING;
	for (i = 0; i < VNODECACHE_SIZE; i++) {
		vc = chmp->chm_vnocache_hash[i];
		while (vc) {
			dbg("vc->vno: %llu\n", (unsigned long long)vc->vno);
			if (!TAILQ_EMPTY(&vc->scan_dirents))
				chfs_build_set_vnodecache_nlink(chmp, vc);
			vc = vc->next;
		}
	}

	/* Step 3 */
	for (i =  0; i < VNODECACHE_SIZE; i++) {
		vc = chmp->chm_vnocache_hash[i];
		while (vc) {
			if (vc->nlink) {
				vc = vc->next;
				continue;
			}

			chfs_build_remove_unlinked_vnode(chmp,
			    vc, &unlinked);
			vc = vc->next;
		}
	}
	/* Remove the newly unlinked vnodes. They are on the unlinked list */
	TAILQ_FOREACH_SAFE(fd, &unlinked, fds, tmpfd) {
		TAILQ_REMOVE(&unlinked, fd, fds);
		mutex_enter(&chmp->chm_lock_vnocache);
		vc = chfs_vnode_cache_get(chmp, fd->vno);
		mutex_exit(&chmp->chm_lock_vnocache);
		if (vc) {
			chfs_build_remove_unlinked_vnode(chmp,
			    vc, &unlinked);
		}
		chfs_free_dirent(fd);
	}

	chmp->chm_flags &= ~CHFS_MP_FLAG_BUILDING;

	/* Free all dirents */
	for (i =  0; i < VNODECACHE_SIZE; i++) {
		vc = chmp->chm_vnocache_hash[i];
		while (vc) {
			TAILQ_FOREACH_SAFE(fd, &vc->scan_dirents, fds, tmpfd) {
				TAILQ_REMOVE(&vc->scan_dirents, fd, fds);
				if (fd->vno == 0) {
					nref = &fd->nref;
					*nref = fd->nref->nref_next;
				} else if (fd->type == CHT_DIR) {
					/* set state every non-VREG file's vc */
					mutex_enter(&chmp->chm_lock_vnocache);
					notregvc = chfs_vnode_cache_get(chmp, fd->vno);
					notregvc->state = VNO_STATE_PRESENT;
					mutex_exit(&chmp->chm_lock_vnocache);
				}
				chfs_free_dirent(fd);
			}
			KASSERT(TAILQ_EMPTY(&vc->scan_dirents));
			vc = vc->next;
		}
	}

	/* Set up chmp->chm_wbuf_ofs for the first write */
	if (chmp->chm_nextblock) {
		dbg("free_size: %d\n", chmp->chm_nextblock->free_size);
		chmp->chm_wbuf_ofs = chmp->chm_ebh->eb_size -
		    chmp->chm_nextblock->free_size;
	} else {
		chmp->chm_wbuf_ofs = 0xffffffff;
	}
	mutex_exit(&chmp->chm_lock_mountfields);

	return 0;
}


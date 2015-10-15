/*	$NetBSD: chfs_scan.c,v 1.6 2015/02/07 04:19:52 christos Exp $	*/

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

#include "chfs.h"

/*
 * chfs_scan_make_vnode_cache - makes a new vnode cache during scan
 * This function returns a vnode cache belonging to @vno.
 */
struct chfs_vnode_cache *
chfs_scan_make_vnode_cache(struct chfs_mount *chmp, ino_t vno)
{
	struct chfs_vnode_cache *vc;

	KASSERT(mutex_owned(&chmp->chm_lock_vnocache));

	/* vnode cache already exists */
	vc = chfs_vnode_cache_get(chmp, vno);
	if (vc) {
		return vc;
	}

	/* update max vnode number if needed */
	if (vno > chmp->chm_max_vno) {
		chmp->chm_max_vno = vno;
	}

	/* create new vnode cache */
	vc = chfs_vnode_cache_alloc(vno);

	chfs_vnode_cache_add(chmp, vc);

	if (vno == CHFS_ROOTINO) {
		vc->nlink = 2;
		vc->pvno = CHFS_ROOTINO;
		vc->state = VNO_STATE_CHECKEDABSENT;
	}

	return vc;
}

/*
 * chfs_scan_check_node_hdr - checks node magic and crc
 * Returns 0 if everything is OK, error code otherwise.
 */
int
chfs_scan_check_node_hdr(struct chfs_flash_node_hdr *nhdr)
{
	uint16_t magic;
	uint32_t crc, hdr_crc;

	magic = le16toh(nhdr->magic);

	if (magic != CHFS_FS_MAGIC_BITMASK) {
		dbg("bad magic\n");
		return CHFS_NODE_BADMAGIC;
	}

	hdr_crc = le32toh(nhdr->hdr_crc);
	crc = crc32(0, (uint8_t *)nhdr, CHFS_NODE_HDR_SIZE - 4);

	if (crc != hdr_crc) {
		dbg("bad crc\n");
		return CHFS_NODE_BADCRC;
	}

	return CHFS_NODE_OK;
}

/* chfs_scan_check_vnode - check vnode crc and add it to vnode cache */
int
chfs_scan_check_vnode(struct chfs_mount *chmp,
    struct chfs_eraseblock *cheb, void *buf, off_t ofs)
{
	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));
	struct chfs_vnode_cache *vc;
	struct chfs_flash_vnode *vnode = buf;
	struct chfs_node_ref *nref;
	int err;
	uint32_t crc;
	ino_t vno;

	crc = crc32(0, (uint8_t *)vnode,
	    sizeof(struct chfs_flash_vnode) - 4);

	/* check node crc */
	if (crc != le32toh(vnode->node_crc)) {
		err = chfs_update_eb_dirty(chmp,
		    cheb, le32toh(vnode->length));
		if (err) {
			return err;
		}

		return CHFS_NODE_BADCRC;
	}

	vno = le64toh(vnode->vno);

	/* find the corresponding vnode cache */
	mutex_enter(&chmp->chm_lock_vnocache);
	vc = chfs_vnode_cache_get(chmp, vno);
	if (!vc) {
		vc = chfs_scan_make_vnode_cache(chmp, vno);
		if (!vc) {
			mutex_exit(&chmp->chm_lock_vnocache);
			return ENOMEM;
		}
	}

	nref = chfs_alloc_node_ref(cheb);

	nref->nref_offset = ofs;

	KASSERT(nref->nref_lnr == cheb->lnr);

	/* check version of vnode */
	if ((struct chfs_vnode_cache *)vc->v != vc) {
		if (le64toh(vnode->version) > *vc->vno_version) {
			*vc->vno_version = le64toh(vnode->version);
			chfs_add_vnode_ref_to_vc(chmp, vc, nref);
		} else {
			err = chfs_update_eb_dirty(chmp, cheb,
			    sizeof(struct chfs_flash_vnode));
			return CHFS_NODE_OK;
		}
	} else {
		vc->vno_version = kmem_alloc(sizeof(uint64_t), KM_SLEEP);
		if (!vc->vno_version)
			return ENOMEM;
		*vc->vno_version = le64toh(vnode->version);
		chfs_add_vnode_ref_to_vc(chmp, vc, nref);
	}
	mutex_exit(&chmp->chm_lock_vnocache);

	/* update sizes */
	mutex_enter(&chmp->chm_lock_sizes);
	chfs_change_size_free(chmp, cheb, -le32toh(vnode->length));
	chfs_change_size_used(chmp, cheb, le32toh(vnode->length));
	mutex_exit(&chmp->chm_lock_sizes);

	KASSERT(cheb->used_size <= chmp->chm_ebh->eb_size);

	KASSERT(cheb->used_size + cheb->free_size + cheb->dirty_size + cheb->unchecked_size + cheb->wasted_size == chmp->chm_ebh->eb_size);

	return CHFS_NODE_OK;
}

/* chfs_scan_mark_dirent_obsolete - marks a directory entry "obsolete" */
int
chfs_scan_mark_dirent_obsolete(struct chfs_mount *chmp,
    struct chfs_vnode_cache *vc, struct chfs_dirent *fd)
{
	struct chfs_eraseblock *cheb;
	struct chfs_node_ref *prev, *nref;

	nref = fd->nref;
	cheb = &chmp->chm_blocks[fd->nref->nref_lnr];

	/* remove dirent's node ref from vnode cache */
	prev = vc->dirents;
	if (prev && prev == nref) {
		vc->dirents = prev->nref_next;
	} else if (prev && prev != (void *)vc) {
		while (prev->nref_next && prev->nref_next != (void *)vc) {
			if (prev->nref_next == nref) {
				prev->nref_next = nref->nref_next;
				break;
			}
			prev = prev->nref_next;
		}
	}

	KASSERT(cheb->used_size + cheb->free_size + cheb->dirty_size +
	    cheb->unchecked_size + cheb->wasted_size == chmp->chm_ebh->eb_size);

	return 0;
}

/* chfs_add_fd_to_list - adds a directory entry to its parent's vnode cache */
void
chfs_add_fd_to_list(struct chfs_mount *chmp,
    struct chfs_dirent *new, struct chfs_vnode_cache *pvc)
{
	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));
	int size;
	struct chfs_eraseblock *cheb, *oldcheb;
	struct chfs_dirent *fd, *tmpfd;

	dbg("adding fd to list: %s\n", new->name);

	/* update highest version if needed */
	if ((new->version > pvc->highest_version))
		pvc->highest_version = new->version;

	size = CHFS_PAD(sizeof(struct chfs_flash_dirent_node) +
	    new->nsize);
	cheb = &chmp->chm_blocks[new->nref->nref_lnr];

	mutex_enter(&chmp->chm_lock_sizes);	
	TAILQ_FOREACH_SAFE(fd, &pvc->scan_dirents, fds, tmpfd) {
		if (fd->nhash > new->nhash) {
			/* insert new before fd */
			TAILQ_INSERT_BEFORE(fd, new, fds);
			goto out;
		} else if (fd->nhash == new->nhash &&
		    !strcmp(fd->name, new->name)) {
			if (new->version > fd->version) {
				/* replace fd with new */
				TAILQ_INSERT_BEFORE(fd, new, fds);
				chfs_change_size_free(chmp, cheb, -size);
				chfs_change_size_used(chmp, cheb, size);

				TAILQ_REMOVE(&pvc->scan_dirents, fd, fds);
				if (fd->nref) {
					size = CHFS_PAD(sizeof(struct chfs_flash_dirent_node) + fd->nsize);
					chfs_scan_mark_dirent_obsolete(chmp, pvc, fd);
					oldcheb = &chmp->chm_blocks[fd->nref->nref_lnr];
					chfs_change_size_used(chmp, oldcheb, -size);
					chfs_change_size_dirty(chmp, oldcheb, size);
				}
				chfs_free_dirent(fd);
			} else {
				/* new dirent is older */
				chfs_scan_mark_dirent_obsolete(chmp, pvc, new);
				chfs_change_size_free(chmp, cheb, -size);
				chfs_change_size_dirty(chmp, cheb, size);
				chfs_free_dirent(new);
			}
			mutex_exit(&chmp->chm_lock_sizes);
			return;
		}
	}
	/* if we couldnt fit it elsewhere, lets add to the end */
	TAILQ_INSERT_TAIL(&pvc->scan_dirents, new, fds);

out:
	/* update sizes */
	chfs_change_size_free(chmp, cheb, -size);
	chfs_change_size_used(chmp, cheb, size);
	mutex_exit(&chmp->chm_lock_sizes);

	KASSERT(cheb->used_size <= chmp->chm_ebh->eb_size);

	KASSERT(cheb->used_size + cheb->free_size + cheb->dirty_size + cheb->unchecked_size + cheb->wasted_size == chmp->chm_ebh->eb_size);
}

/* chfs_scan_check_dirent_node - check vnode crc and add to vnode cache */
int
chfs_scan_check_dirent_node(struct chfs_mount *chmp,
    struct chfs_eraseblock *cheb, void *buf, off_t ofs)
{
	int err, namelen;
	uint32_t crc;
	struct chfs_dirent *fd;
	struct chfs_vnode_cache *parentvc;
	struct chfs_flash_dirent_node *dirent = buf;

	/* check crc */
	crc = crc32(0, (uint8_t *)dirent, sizeof(*dirent) - 4);
	if (crc != le32toh(dirent->node_crc)) {
		err = chfs_update_eb_dirty(chmp, cheb, le32toh(dirent->length));
		if (err)
			return err;
		return CHFS_NODE_BADCRC;
	}

	/* allocate space for name */
	namelen = dirent->nsize;

	fd = chfs_alloc_dirent(namelen + 1);
	if (!fd)
		return ENOMEM;

	/* allocate an nref */
	fd->nref = chfs_alloc_node_ref(cheb);
	if (!fd->nref)
		return ENOMEM;

	KASSERT(fd->nref->nref_lnr == cheb->lnr);

	memcpy(&fd->name, dirent->name, namelen);
	fd->nsize = namelen;
	fd->name[namelen] = 0;
	crc = crc32(0, fd->name, dirent->nsize);
	if (crc != le32toh(dirent->name_crc)) {
		chfs_err("Directory entry's name has bad crc: read: 0x%x, "
		    "calculated: 0x%x\n", le32toh(dirent->name_crc), crc);
		chfs_free_dirent(fd);
		err = chfs_update_eb_dirty(chmp, cheb, le32toh(dirent->length));
		if (err)
			return err;
		return CHFS_NODE_BADNAMECRC;
	}

	/* check vnode_cache of parent node */
	mutex_enter(&chmp->chm_lock_vnocache);
	parentvc = chfs_scan_make_vnode_cache(chmp, le64toh(dirent->pvno));
	if (!parentvc) {
		chfs_free_dirent(fd);
		return ENOMEM;
	}

	fd->nref->nref_offset = ofs;

	dbg("add dirent to #%llu\n", (unsigned long long)parentvc->vno);
	chfs_add_node_to_list(chmp, parentvc, fd->nref, &parentvc->dirents);
	mutex_exit(&chmp->chm_lock_vnocache);

	fd->vno = le64toh(dirent->vno);
	fd->version = le64toh(dirent->version);
	fd->nhash = hash32_buf(fd->name, namelen, HASH32_BUF_INIT);
	fd->type = dirent->dtype;

	chfs_add_fd_to_list(chmp, fd, parentvc);

	return CHFS_NODE_OK;
}

/* chfs_scan_check_data_node - check vnode crc and add to vnode cache */
int
chfs_scan_check_data_node(struct chfs_mount *chmp,
    struct chfs_eraseblock *cheb, void *buf, off_t ofs)
{
	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));
	int err;
	uint32_t crc, vno;
	struct chfs_node_ref *nref;
	struct chfs_vnode_cache *vc;
	struct chfs_flash_data_node *dnode = buf;

	/* check crc */
	crc = crc32(0, (uint8_t *)dnode, sizeof(struct chfs_flash_data_node) - 4);
	if (crc != le32toh(dnode->node_crc)) {
		err = chfs_update_eb_dirty(chmp, cheb, le32toh(dnode->length));
		if (err)
			return err;
		return CHFS_NODE_BADCRC;
	}
	/*
	 * Don't check data nodes crc and version here, it will be done in
	 * the background GC thread.
	 */
	nref = chfs_alloc_node_ref(cheb);
	if (!nref)
		return ENOMEM;

	nref->nref_offset = CHFS_GET_OFS(ofs) | CHFS_UNCHECKED_NODE_MASK;

	KASSERT(nref->nref_lnr == cheb->lnr);

	vno = le64toh(dnode->vno);
	mutex_enter(&chmp->chm_lock_vnocache);
	vc = chfs_vnode_cache_get(chmp, vno);
	if (!vc) {
		vc = chfs_scan_make_vnode_cache(chmp, vno);
		if (!vc)
			return ENOMEM;
	}
	chfs_add_node_to_list(chmp, vc, nref, &vc->dnode);
	mutex_exit(&chmp->chm_lock_vnocache);

	dbg("chmpfree: %u, chebfree: %u, dnode: %u\n", chmp->chm_free_size, cheb->free_size, dnode->length);

	/* update sizes */
	mutex_enter(&chmp->chm_lock_sizes);
	chfs_change_size_free(chmp, cheb, -dnode->length);
	chfs_change_size_unchecked(chmp, cheb, dnode->length);
	mutex_exit(&chmp->chm_lock_sizes);
	return CHFS_NODE_OK;
}

/* chfs_scan_classify_cheb - determine eraseblock's state */
int
chfs_scan_classify_cheb(struct chfs_mount *chmp,
    struct chfs_eraseblock *cheb)
{
	if (cheb->free_size == chmp->chm_ebh->eb_size)
		return CHFS_BLK_STATE_FREE;
	else if (cheb->dirty_size < MAX_DIRTY_TO_CLEAN)
		return CHFS_BLK_STATE_CLEAN;
	else if (cheb->used_size || cheb->unchecked_size)
		return CHFS_BLK_STATE_PARTDIRTY;
	else
		return CHFS_BLK_STATE_ALLDIRTY;
}


/*
 * chfs_scan_eraseblock - scans an eraseblock and looking for nodes
 *
 * This function scans a whole eraseblock, checks the nodes on it and add them
 * to the vnode cache.
 * Returns eraseblock state on success, error code if fails.
 */
int
chfs_scan_eraseblock(struct chfs_mount *chmp,
    struct chfs_eraseblock *cheb)
{
	int err;
	size_t len, retlen;
	off_t ofs = 0;
	int lnr = cheb->lnr;
	u_char *buf;
	struct chfs_flash_node_hdr *nhdr;
	int read_free = 0;
	struct chfs_node_ref *nref;

	dbg("scanning eraseblock content: %d free_size: %d\n", cheb->lnr, cheb->free_size);
	dbg("scanned physical block: %d\n", chmp->chm_ebh->lmap[lnr]);
	buf = kmem_alloc(CHFS_MAX_NODE_SIZE, KM_SLEEP);

	while((ofs + CHFS_NODE_HDR_SIZE) < chmp->chm_ebh->eb_size) {
		memset(buf, 0 , CHFS_MAX_NODE_SIZE);
		err = chfs_read_leb(chmp,
		    lnr, buf, ofs, CHFS_NODE_HDR_SIZE, &retlen);
		if (err)
			goto err_return;

		if (retlen != CHFS_NODE_HDR_SIZE) {
			chfs_err("Error reading node header: "
			    "read: %zu instead of: %zu\n",
			    CHFS_NODE_HDR_SIZE, retlen);
			err = EIO;
			goto err_return;
		}

		/* first we check if the buffer we read is full with 0xff, if yes maybe
		 * the blocks remaining area is free. We increase read_free and if it
		 * reaches MAX_READ_FREE we stop reading the block */
		if (check_pattern(buf, 0xff, 0, CHFS_NODE_HDR_SIZE)) {
			read_free += CHFS_NODE_HDR_SIZE;
			if (read_free >= MAX_READ_FREE(chmp)) {
				dbg("rest of the block is free. Size: %d\n", cheb->free_size);
				kmem_free(buf, CHFS_MAX_NODE_SIZE);
				return chfs_scan_classify_cheb(chmp, cheb);
			}
			ofs += CHFS_NODE_HDR_SIZE;
			continue;
		} else {
			chfs_update_eb_dirty(chmp, cheb, read_free);
			read_free = 0;
		}

		nhdr = (struct chfs_flash_node_hdr *)buf;

		err = chfs_scan_check_node_hdr(nhdr);
		if (err) {
			dbg("node hdr error\n");
			err = chfs_update_eb_dirty(chmp, cheb, 4);
			if (err)
				goto err_return;

			ofs += 4;
			continue;
		}
		ofs += CHFS_NODE_HDR_SIZE;
		if (ofs > chmp->chm_ebh->eb_size) {
			chfs_err("Second part of node is on the next eraseblock.\n");
			err = EIO;
			goto err_return;
		}
		switch (le16toh(nhdr->type)) {
		case CHFS_NODETYPE_VNODE:
		/* vnode information */
			/* read up the node */
			len = le32toh(nhdr->length) - CHFS_NODE_HDR_SIZE;
			err = chfs_read_leb(chmp,
			    lnr, buf + CHFS_NODE_HDR_SIZE,
			    ofs, len,  &retlen);
			if (err)
				goto err_return;

			if (retlen != len) {
				chfs_err("Error reading vnode: read: %zu instead of: %zu\n",
				    len, retlen);
				err = EIO;
				goto err_return;
			}
			KASSERT(lnr == cheb->lnr);
			err = chfs_scan_check_vnode(chmp,
			    cheb, buf, ofs - CHFS_NODE_HDR_SIZE);
			if (err)
				goto err_return;

			break;
		case CHFS_NODETYPE_DIRENT:
		/* directory entry */
			/* read up the node */
			len = le32toh(nhdr->length) - CHFS_NODE_HDR_SIZE;

			err = chfs_read_leb(chmp,
			    lnr, buf + CHFS_NODE_HDR_SIZE,
			    ofs, len, &retlen);
			if (err)
				goto err_return;

			if (retlen != len) {
				chfs_err("Error reading dirent node: read: %zu "
				    "instead of: %zu\n", len, retlen);
				err = EIO;
				goto err_return;
			}

			KASSERT(lnr == cheb->lnr);

			err = chfs_scan_check_dirent_node(chmp,
			    cheb, buf, ofs - CHFS_NODE_HDR_SIZE);
			if (err)
				goto err_return;

			break;
		case CHFS_NODETYPE_DATA:
		/* data node */
			len = sizeof(struct chfs_flash_data_node) -
			    CHFS_NODE_HDR_SIZE;
			err = chfs_read_leb(chmp,
			    lnr, buf + CHFS_NODE_HDR_SIZE,
			    ofs, len, &retlen);
			if (err)
				goto err_return;

			if (retlen != len) {
				chfs_err("Error reading data node: read: %zu "
				    "instead of: %zu\n", len, retlen);
				err = EIO;
				goto err_return;
			}
			KASSERT(lnr == cheb->lnr);
			err = chfs_scan_check_data_node(chmp,
			    cheb, buf, ofs - CHFS_NODE_HDR_SIZE);
			if (err)
				goto err_return;

			break;
		case CHFS_NODETYPE_PADDING:
		/* padding node, set size and update dirty */
			nref = chfs_alloc_node_ref(cheb);
			nref->nref_offset = ofs - CHFS_NODE_HDR_SIZE;
			nref->nref_offset = CHFS_GET_OFS(nref->nref_offset) |
			    CHFS_OBSOLETE_NODE_MASK;

			err = chfs_update_eb_dirty(chmp, cheb,
			    le32toh(nhdr->length));
			if (err)
				goto err_return;

			break;
		default:
		/* unknown node type, update dirty and skip */
			err = chfs_update_eb_dirty(chmp, cheb,
			    le32toh(nhdr->length));
			if (err)
				goto err_return;

			break;
		}
		ofs += le32toh(nhdr->length) - CHFS_NODE_HDR_SIZE;
	}

	KASSERT(cheb->used_size + cheb->free_size + cheb->dirty_size +
	    cheb->unchecked_size + cheb->wasted_size == chmp->chm_ebh->eb_size);

	err = chfs_scan_classify_cheb(chmp, cheb);
	/* FALLTHROUGH */
    err_return:
	kmem_free(buf, CHFS_MAX_NODE_SIZE);
	return err;
}

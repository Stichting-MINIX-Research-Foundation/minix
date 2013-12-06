/*	$NetBSD: chfs_write.c,v 1.5 2012/10/19 12:44:39 ttoth Exp $	*/

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


#include <sys/param.h>
#include <sys/buf.h>

#include "chfs.h"


/* chfs_write_flash_vnode - writes out a vnode information to flash */
int
chfs_write_flash_vnode(struct chfs_mount *chmp,
    struct chfs_inode *ip, int prio)
{
	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));

	struct chfs_flash_vnode *fvnode;
	struct chfs_vnode_cache* chvc;
	struct chfs_node_ref *nref;
	struct iovec vec;
	size_t size, retlen;
	int err = 0, retries = 0;

	/* root vnode is in-memory only */
	if (ip->ino == CHFS_ROOTINO)
		return 0;

	fvnode = chfs_alloc_flash_vnode();
	if (!fvnode)
		return ENOMEM;

	chvc = ip->chvc;

	/* setting up flash_vnode's fields */
	size = sizeof(*fvnode);
	fvnode->magic = htole16(CHFS_FS_MAGIC_BITMASK);
	fvnode->type = htole16(CHFS_NODETYPE_VNODE);
	fvnode->length = htole32(CHFS_PAD(size));
	fvnode->hdr_crc = htole32(crc32(0, (uint8_t *)fvnode,
		CHFS_NODE_HDR_SIZE - 4));
	fvnode->vno = htole64(ip->ino);
	fvnode->version = htole64(++ip->chvc->highest_version);
	fvnode->mode = htole32(ip->mode);
	fvnode->dn_size = htole32(ip->size);
	fvnode->atime = htole32(ip->atime);
	fvnode->ctime = htole32(ip->ctime);
	fvnode->mtime = htole32(ip->mtime);
	fvnode->gid = htole32(ip->gid);
	fvnode->uid = htole32(ip->uid);
	fvnode->node_crc = htole32(crc32(0, (uint8_t *)fvnode, size - 4));

retry:
	/* setting up the next eraseblock where we will write */
	if (prio == ALLOC_GC) {
		/* GC called this function */
		err = chfs_reserve_space_gc(chmp, CHFS_PAD(size));
		if (err)
			goto out;
	} else {
		chfs_gc_trigger(chmp);
		if (prio == ALLOC_NORMAL)
			err = chfs_reserve_space_normal(chmp,
			    CHFS_PAD(size), ALLOC_NORMAL);
		else
			err = chfs_reserve_space_normal(chmp,
			    CHFS_PAD(size), ALLOC_DELETION);
		if (err)
			goto out;
	}

	/* allocating a new node reference */
	nref = chfs_alloc_node_ref(chmp->chm_nextblock);
	if (!nref) {
		err = ENOMEM;
		goto out;
	}

	mutex_enter(&chmp->chm_lock_sizes);

	/* caculating offset and sizes  */
	nref->nref_offset = chmp->chm_ebh->eb_size - chmp->chm_nextblock->free_size;
	chfs_change_size_free(chmp, chmp->chm_nextblock, -CHFS_PAD(size));
	vec.iov_base = fvnode;
	vec.iov_len = CHFS_PAD(size);

	/* write it into the writebuffer */
	err = chfs_write_wbuf(chmp, &vec, 1, nref->nref_offset, &retlen);
	if (err || retlen != CHFS_PAD(size)) {
		/* there was an error during write */
		chfs_err("error while writing out flash vnode to the media\n");
		chfs_err("err: %d | size: %zu | retlen : %zu\n",
		    err, CHFS_PAD(size), retlen);
		chfs_change_size_dirty(chmp,
		    chmp->chm_nextblock, CHFS_PAD(size));
		if (retries) {
			err = EIO;
			mutex_exit(&chmp->chm_lock_sizes);
			goto out;
		}

		/* try again */
		retries++;
		mutex_exit(&chmp->chm_lock_sizes);
		goto retry;
	}

	/* everything went well */
	chfs_change_size_used(chmp,
	    &chmp->chm_blocks[nref->nref_lnr], CHFS_PAD(size));
	mutex_exit(&chmp->chm_lock_sizes);
	
	/* add the new nref to vnode cache */
	mutex_enter(&chmp->chm_lock_vnocache);
	chfs_add_vnode_ref_to_vc(chmp, chvc, nref);
	mutex_exit(&chmp->chm_lock_vnocache);
	KASSERT(chmp->chm_blocks[nref->nref_lnr].used_size <= chmp->chm_ebh->eb_size);
out:
	chfs_free_flash_vnode(fvnode);
	return err;
}

/* chfs_write_flash_dirent - writes out a directory entry to flash */
int
chfs_write_flash_dirent(struct chfs_mount *chmp, struct chfs_inode *pdir,
    struct chfs_inode *ip, struct chfs_dirent *fd,
    ino_t ino, int prio)
{
	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));

	struct chfs_flash_dirent_node *fdirent;
	struct chfs_node_ref *nref;
	struct iovec vec[2];
	size_t size, retlen;
	int err = 0, retries = 0;
	uint8_t *name;
	size_t namelen;

	KASSERT(fd->vno != CHFS_ROOTINO);

	/* setting up flash_dirent's fields */
	fdirent = chfs_alloc_flash_dirent();
	if (!fdirent)
		return ENOMEM;

	size = sizeof(*fdirent) + fd->nsize;
	namelen = CHFS_PAD(size) - sizeof(*fdirent);

	name = kmem_zalloc(namelen, KM_SLEEP);
	memcpy(name, fd->name, fd->nsize);

	fdirent->magic = htole16(CHFS_FS_MAGIC_BITMASK);
	fdirent->type = htole16(CHFS_NODETYPE_DIRENT);
	fdirent->length = htole32(CHFS_PAD(size));
	fdirent->hdr_crc = htole32(crc32(0, (uint8_t *)fdirent,
		CHFS_NODE_HDR_SIZE - 4));
	fdirent->vno = htole64(ino);
	fdirent->pvno = htole64(pdir->ino);
	fdirent->version = htole64(++pdir->chvc->highest_version);
	fdirent->mctime = ip?ip->ctime:0;
	fdirent->nsize = fd->nsize;
	fdirent->dtype = fd->type;
	fdirent->name_crc = crc32(0, (uint8_t *)&(fd->name), fd->nsize);
	fdirent->node_crc = crc32(0, (uint8_t *)fdirent, sizeof(*fdirent) - 4);

	/* directory's name is written out right after the dirent */
	vec[0].iov_base = fdirent;
	vec[0].iov_len  = sizeof(*fdirent);
	vec[1].iov_base = name;
	vec[1].iov_len  = namelen;
	
retry:
	/* setting up the next eraseblock where we will write */
	if (prio == ALLOC_GC) {
		/* the GC calls this function */
		err = chfs_reserve_space_gc(chmp, CHFS_PAD(size));
		if (err)
			goto out;
	} else {
		chfs_gc_trigger(chmp);
		if (prio == ALLOC_NORMAL)
			err = chfs_reserve_space_normal(chmp,
			    CHFS_PAD(size), ALLOC_NORMAL);
		else
			err = chfs_reserve_space_normal(chmp,
			    CHFS_PAD(size), ALLOC_DELETION);
		if (err)
			goto out;
	}

	/* allocating a new node reference */
	nref = chfs_alloc_node_ref(chmp->chm_nextblock);
	if (!nref) {
		err = ENOMEM;
		goto out;
	}

	mutex_enter(&chmp->chm_lock_sizes);

	nref->nref_offset = chmp->chm_ebh->eb_size - chmp->chm_nextblock->free_size;
	chfs_change_size_free(chmp, chmp->chm_nextblock, -CHFS_PAD(size));

	/* write it into the writebuffer */
	err = chfs_write_wbuf(chmp, vec, 2, nref->nref_offset, &retlen);
	if (err || retlen != CHFS_PAD(size)) {
		/* there was an error during write */
		chfs_err("error while writing out flash dirent node to the media\n");
		chfs_err("err: %d | size: %zu | retlen : %zu\n",
		    err, CHFS_PAD(size), retlen);
		chfs_change_size_dirty(chmp,
		    chmp->chm_nextblock, CHFS_PAD(size));
		if (retries) {
			err = EIO;
			mutex_exit(&chmp->chm_lock_sizes);
			goto out;
		}

		/* try again */
		retries++;
		mutex_exit(&chmp->chm_lock_sizes);
		goto retry;
	}


	/* everything went well */
	chfs_change_size_used(chmp,
	    &chmp->chm_blocks[nref->nref_lnr], CHFS_PAD(size));
	mutex_exit(&chmp->chm_lock_sizes);
	KASSERT(chmp->chm_blocks[nref->nref_lnr].used_size <= chmp->chm_ebh->eb_size);

	/* add the new nref to the directory chain of vnode cache */
	fd->nref = nref;
	if (prio != ALLOC_DELETION) {
		mutex_enter(&chmp->chm_lock_vnocache);
		chfs_add_node_to_list(chmp,
			pdir->chvc, nref, &pdir->chvc->dirents);
		mutex_exit(&chmp->chm_lock_vnocache);
	}
out:
	chfs_free_flash_dirent(fdirent);
	return err;
}

/* chfs_write_flash_dnode - writes out a data node to flash */
int
chfs_write_flash_dnode(struct chfs_mount *chmp, struct vnode *vp,
    struct buf *bp, struct chfs_full_dnode *fd)
{
	KASSERT(mutex_owned(&chmp->chm_lock_mountfields));

	int err = 0, retries = 0;
	size_t size, retlen;
	off_t ofs;
	struct chfs_flash_data_node *dnode;
	struct chfs_node_ref *nref;
	struct chfs_inode *ip = VTOI(vp);
	struct iovec vec[2];
	uint32_t len;
	void *tmpbuf = NULL;

	KASSERT(ip->ino != CHFS_ROOTINO);

	dnode = chfs_alloc_flash_dnode();
	if (!dnode)
		return ENOMEM;

	/* initialize flash data node */
	ofs = bp->b_blkno * PAGE_SIZE;
	len = MIN((vp->v_size - ofs), bp->b_resid);
	size = sizeof(*dnode) + len;

	dnode->magic = htole16(CHFS_FS_MAGIC_BITMASK);
	dnode->type = htole16(CHFS_NODETYPE_DATA);
	dnode->length = htole32(CHFS_PAD(size));
	dnode->hdr_crc = htole32(crc32(0, (uint8_t *)dnode,
		CHFS_NODE_HDR_SIZE - 4));
	dnode->vno = htole64(ip->ino);
	dnode->version = htole64(++ip->chvc->highest_version);
	dnode->offset = htole64(ofs);
	dnode->data_length = htole32(len);
	dnode->data_crc = htole32(crc32(0, (uint8_t *)bp->b_data, len));
	dnode->node_crc = htole32(crc32(0, (uint8_t *)dnode,
		sizeof(*dnode) - 4));

	dbg("dnode @%llu %ub v%llu\n", (unsigned long long)dnode->offset,
		dnode->data_length, (unsigned long long)dnode->version);

	/* pad data if needed */
	if (CHFS_PAD(size) - sizeof(*dnode)) {
		tmpbuf = kmem_zalloc(CHFS_PAD(size)
		    - sizeof(*dnode), KM_SLEEP);
		memcpy(tmpbuf, bp->b_data, len);
	}

	/* creating iovecs for writebuffer 
	 * data is written out right after the data node */
	vec[0].iov_base = dnode;
	vec[0].iov_len = sizeof(*dnode);
	vec[1].iov_base = tmpbuf;
	vec[1].iov_len = CHFS_PAD(size) - sizeof(*dnode);

	fd->ofs = ofs;
	fd->size = len;

retry:
	/* Reserve space for data node. This will set up the next eraseblock
	 * where to we will write.
	 */
	chfs_gc_trigger(chmp);
	err = chfs_reserve_space_normal(chmp,
	    CHFS_PAD(size), ALLOC_NORMAL);
	if (err)
		goto out;

	/* allocating a new node reference */
	nref = chfs_alloc_node_ref(chmp->chm_nextblock);
	if (!nref) {
		err = ENOMEM;
		goto out;
	}

	nref->nref_offset =
	    chmp->chm_ebh->eb_size - chmp->chm_nextblock->free_size;

	KASSERT(nref->nref_offset < chmp->chm_ebh->eb_size);
	
	mutex_enter(&chmp->chm_lock_sizes);

	chfs_change_size_free(chmp,
	    chmp->chm_nextblock, -CHFS_PAD(size));

	/* write it into the writebuffer */
	err = chfs_write_wbuf(chmp, vec, 2, nref->nref_offset, &retlen);
	if (err || retlen != CHFS_PAD(size)) {
		/* there was an error during write */
		chfs_err("error while writing out flash data node to the media\n");
		chfs_err("err: %d | size: %zu | retlen : %zu\n",
		    err, size, retlen);
		chfs_change_size_dirty(chmp,
		    chmp->chm_nextblock, CHFS_PAD(size));
		if (retries) {
			err = EIO;
			mutex_exit(&chmp->chm_lock_sizes);
			goto out;
		}

		/* try again */
		retries++;
		mutex_exit(&chmp->chm_lock_sizes);
		goto retry;
	}
	/* everything went well */
	ip->write_size += fd->size;
	chfs_change_size_used(chmp,
	    &chmp->chm_blocks[nref->nref_lnr], CHFS_PAD(size));
	mutex_exit(&chmp->chm_lock_sizes);

	mutex_enter(&chmp->chm_lock_vnocache);
	if (fd->nref != NULL) {
		chfs_remove_frags_of_node(chmp, &ip->fragtree, fd->nref);
		chfs_remove_and_obsolete(chmp, ip->chvc, fd->nref, &ip->chvc->dnode);
	}

	/* add the new nref to the data node chain of vnode cache */
	KASSERT(chmp->chm_blocks[nref->nref_lnr].used_size <= chmp->chm_ebh->eb_size);
	fd->nref = nref;
	chfs_add_node_to_list(chmp, ip->chvc, nref, &ip->chvc->dnode);
	mutex_exit(&chmp->chm_lock_vnocache);
out:
	chfs_free_flash_dnode(dnode);
	if (CHFS_PAD(size) - sizeof(*dnode)) {
		kmem_free(tmpbuf, CHFS_PAD(size) - sizeof(*dnode));
	}

	return err;
}

/*
 * chfs_do_link - makes a copy from a node
 * This function writes the dirent of the new node to the media.
 */
int
chfs_do_link(struct chfs_inode *ip, struct chfs_inode *parent, const char *name, int namelen, enum chtype type)
{
	int error = 0;
	struct vnode *vp = ITOV(ip);
	struct ufsmount *ump = VFSTOUFS(vp->v_mount);
	struct chfs_mount *chmp = ump->um_chfs;
	struct chfs_dirent *newfd = NULL;

	/* setting up the new directory entry */
	newfd = chfs_alloc_dirent(namelen + 1);

	newfd->vno = ip->ino;
	newfd->type = type;
	newfd->nsize = namelen;
	memcpy(newfd->name, name, namelen);
	newfd->name[newfd->nsize] = 0;

	ip->chvc->nlink++;
	parent->chvc->nlink++;
	ip->iflag |= IN_CHANGE;
	chfs_update(vp, NULL, NULL, UPDATE_WAIT);

	mutex_enter(&chmp->chm_lock_mountfields);

	/* update vnode information */
	error = chfs_write_flash_vnode(chmp, ip, ALLOC_NORMAL);
	if (error)
		return error;

	/* write out the new dirent */
	error = chfs_write_flash_dirent(chmp,
	    parent, ip, newfd, ip->ino, ALLOC_NORMAL);
	/* TODO: what should we do if error isn't zero? */

	mutex_exit(&chmp->chm_lock_mountfields);

	/* add fd to the fd list */
	TAILQ_INSERT_TAIL(&parent->dents, newfd, fds);

	return error;
}


/*
 * chfs_do_unlink - delete a node
 * This function set the nlink and vno of the node to zero and
 * write its dirent to the media.
 */
int
chfs_do_unlink(struct chfs_inode *ip,
    struct chfs_inode *parent, const char *name, int namelen)
{
	struct chfs_dirent *fd, *tmpfd;
	int error = 0;
	struct vnode *vp = ITOV(ip);
	struct ufsmount *ump = VFSTOUFS(vp->v_mount);
	struct chfs_mount *chmp = ump->um_chfs;
	struct chfs_node_ref *nref;

	vflushbuf(vp, 0);

	mutex_enter(&chmp->chm_lock_mountfields);

	/* remove the full direntry from the parent dents list */
	TAILQ_FOREACH_SAFE(fd, &parent->dents, fds, tmpfd) {
		if (fd->vno == ip->ino &&
		    fd->nsize == namelen &&
		    !memcmp(fd->name, name, fd->nsize)) {

			/* remove every fragment of the file */
			chfs_kill_fragtree(chmp, &ip->fragtree);

			/* decrease number of links to the file */
			if (fd->type == CHT_DIR && ip->chvc->nlink == 2)
				ip->chvc->nlink = 0;
			else
				ip->chvc->nlink--;

			fd->type = CHT_BLANK;

			/* remove from parent's directory entries */
			TAILQ_REMOVE(&parent->dents, fd, fds);

			mutex_enter(&chmp->chm_lock_vnocache);

			dbg("FD->NREF vno: %llu, lnr: %u, ofs: %u\n",
			    fd->vno, fd->nref->nref_lnr, fd->nref->nref_offset);
			chfs_remove_and_obsolete(chmp, parent->chvc, fd->nref,
				&parent->chvc->dirents);

			error = chfs_write_flash_dirent(chmp,
			    parent, ip, fd, 0, ALLOC_DELETION);

			dbg("FD->NREF vno: %llu, lnr: %u, ofs: %u\n",
			    fd->vno, fd->nref->nref_lnr, fd->nref->nref_offset);
			/* set nref_next field */
			chfs_add_node_to_list(chmp, parent->chvc, fd->nref,
				&parent->chvc->dirents);
			/* remove from the list */
			chfs_remove_and_obsolete(chmp, parent->chvc, fd->nref,
				&parent->chvc->dirents);

			/* clean dnode list */
			while (ip->chvc->dnode != (struct chfs_node_ref *)ip->chvc) {
				nref = ip->chvc->dnode;
				chfs_remove_frags_of_node(chmp, &ip->fragtree, nref);
				chfs_remove_and_obsolete(chmp, ip->chvc, nref, &ip->chvc->dnode);
			}

			/* clean vnode information (list) */
			while (ip->chvc->v != (struct chfs_node_ref *)ip->chvc) {
				nref = ip->chvc->v;
				chfs_remove_and_obsolete(chmp, ip->chvc, nref, &ip->chvc->v);
			}

			/* decrease number of links to parent */
			parent->chvc->nlink--;

			mutex_exit(&chmp->chm_lock_vnocache);
			//TODO: if error
		}
	}
	mutex_exit(&chmp->chm_lock_mountfields);

	return error;
}

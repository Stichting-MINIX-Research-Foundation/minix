/*	$NetBSD: chfs_vnode.c,v 1.14 2015/01/11 17:29:57 hannken Exp $	*/

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
#include "chfs_inode.h"
#include <sys/kauth.h>
#include <sys/namei.h>
#include <sys/uio.h>
#include <sys/buf.h>

#include <miscfs/genfs/genfs.h>

/* chfs_vnode_lookup - lookup for a vnode */
static bool
chfs_vnode_lookup_selector(void *ctx, struct vnode *vp)
{
	ino_t *ino = ctx;

	return (VTOI(vp) != NULL && VTOI(vp)->ino == *ino);
}
struct vnode *
chfs_vnode_lookup(struct chfs_mount *chmp, ino_t vno)
{
	struct vnode_iterator *marker;
	struct vnode *vp;

	vfs_vnode_iterator_init(chmp->chm_fsmp, &marker);
	vp = vfs_vnode_iterator_next(marker, chfs_vnode_lookup_selector, &vno);
	vfs_vnode_iterator_destroy(marker);

	return vp;
}

/* chfs_readvnode - reads a vnode from the flash and setups its inode */
int
chfs_readvnode(struct mount *mp, ino_t ino, struct vnode **vpp)
{
	struct ufsmount* ump = VFSTOUFS(mp);
	struct chfs_mount *chmp = ump->um_chfs;
	struct chfs_vnode_cache *chvc;
	struct chfs_flash_vnode *chfvn;
	struct chfs_inode *ip;
	int err;
	char* buf;
	size_t retlen, len;
	struct vnode* vp = NULL;
	dbg("readvnode | ino: %llu\n", (unsigned long long)ino);

	len = sizeof(struct chfs_flash_vnode);

	KASSERT(vpp != NULL);

	if (vpp != NULL) {
		vp = *vpp;
	}

	ip = VTOI(vp);
	chvc = ip->chvc;

	/* root node is in-memory only */
	if (chvc && ino != CHFS_ROOTINO) {
		dbg("offset: %" PRIu32 ", lnr: %d\n",
		    CHFS_GET_OFS(chvc->v->nref_offset), chvc->v->nref_lnr);

		KASSERT((void *)chvc != (void *)chvc->v);

		/* reading */
		buf = kmem_alloc(len, KM_SLEEP);
		err = chfs_read_leb(chmp, chvc->v->nref_lnr, buf,
		    CHFS_GET_OFS(chvc->v->nref_offset), len, &retlen);
		if (err) {
			kmem_free(buf, len);
			return err;
		}
		if (retlen != len) {
			chfs_err("Error reading vnode: read: %zu insted of: %zu\n",
			    len, retlen);
			kmem_free(buf, len);
			return EIO;
		}
		chfvn = (struct chfs_flash_vnode*)buf;

		/* setup inode fields */
		chfs_set_vnode_size(vp, chfvn->dn_size);
		ip->mode = chfvn->mode;
		ip->ch_type = IFTOCHT(ip->mode);
		vp->v_type = CHTTOVT(ip->ch_type);
		ip->version = chfvn->version;
		ip->uid = chfvn->uid;
		ip->gid = chfvn->gid;
		ip->atime = chfvn->atime;
		ip->mtime = chfvn->mtime;
		ip->ctime = chfvn->ctime;

		kmem_free(buf, len);
	}


	*vpp = vp;
	return 0;
}

/* 
 * chfs_readddirent - 
 * reads a directory entry from flash and adds it to its inode 
 */
int
chfs_readdirent(struct mount *mp, struct chfs_node_ref *chnr, struct chfs_inode *pdir)
{
	struct ufsmount *ump = VFSTOUFS(mp);
	struct chfs_mount *chmp = ump->um_chfs;
	struct chfs_flash_dirent_node chfdn;
	struct chfs_dirent *fd;
	size_t len = sizeof(struct chfs_flash_dirent_node);
	size_t retlen;
	int err = 0;

	/* read flash_dirent_node */
	err = chfs_read_leb(chmp, chnr->nref_lnr, (char *)&chfdn,
	    CHFS_GET_OFS(chnr->nref_offset), len, &retlen);
	if (err) {
		return err;
	}
	if (retlen != len) {
		chfs_err("Error reading vnode: read: %zu insted of: %zu\n",
		    retlen, len);
		return EIO;
	}

	/* set fields of dirent */
	fd = chfs_alloc_dirent(chfdn.nsize + 1);
	fd->version = chfdn.version;
	fd->vno = chfdn.vno;
	fd->type = chfdn.dtype;
	fd->nsize = chfdn.nsize;

	/* read the name of the dirent */
	err = chfs_read_leb(chmp, chnr->nref_lnr, fd->name,
	    CHFS_GET_OFS(chnr->nref_offset) + len, chfdn.nsize, &retlen);
	if (err) {
		return err;
	}

	if (retlen != chfdn.nsize) {
		chfs_err("Error reading vnode: read: %zu insted of: %zu\n",
		    len, retlen);
		return EIO;
	}

	fd->name[fd->nsize] = 0;
	fd->nref = chnr;

	/* add to inode */
	chfs_add_fd_to_inode(chmp, pdir, fd);
	return 0;
}

/* chfs_makeinode - makes a new file and initializes its structures */
int
chfs_makeinode(int mode, struct vnode *dvp, struct vnode **vpp,
    struct componentname *cnp, enum vtype type)
{
	struct chfs_inode *ip, *pdir;
	struct vnode *vp;
	struct ufsmount* ump = VFSTOUFS(dvp->v_mount);
	struct chfs_mount* chmp = ump->um_chfs;
	struct chfs_vnode_cache* chvc;
	int error;
	ino_t vno;
	struct chfs_dirent *nfd;

	dbg("makeinode\n");
	pdir = VTOI(dvp);

	*vpp = NULL;

	/* number of vnode will be the new maximum */
	vno = ++(chmp->chm_max_vno);

	error = VFS_VGET(dvp->v_mount, vno, &vp);
	if (error)
		return (error);

	/* setup vnode cache */
	mutex_enter(&chmp->chm_lock_vnocache);
	chvc = chfs_vnode_cache_get(chmp, vno);

	chvc->pvno = pdir->ino;
	chvc->vno_version = kmem_alloc(sizeof(uint64_t), KM_SLEEP);
	*(chvc->vno_version) = 1;
	if (type != VDIR)
		chvc->nlink = 1;
	else
		chvc->nlink = 2;
	chvc->state = VNO_STATE_CHECKEDABSENT;
	mutex_exit(&chmp->chm_lock_vnocache);

	/* setup inode */
	ip = VTOI(vp);
	ip->ino = vno;

	if (type == VDIR)
		chfs_set_vnode_size(vp, 512);
	else
		chfs_set_vnode_size(vp, 0);

	ip->uid = kauth_cred_geteuid(cnp->cn_cred);
	ip->gid = kauth_cred_getegid(cnp->cn_cred);
	ip->version = 1;
	ip->iflag |= (IN_ACCESS | IN_CHANGE | IN_UPDATE);

	ip->chvc = chvc;
	ip->target = NULL;

	ip->mode = mode;
	vp->v_type = type;		/* Rest init'd in chfs_loadvnode(). */
	ip->ch_type = VTTOCHT(vp->v_type);

	/* authorize setting SGID if needed */
	if (ip->mode & ISGID) {
		error = kauth_authorize_vnode(cnp->cn_cred, KAUTH_VNODE_WRITE_SECURITY,
		    vp, NULL, genfs_can_chmod(vp->v_type, cnp->cn_cred, ip->uid,
		    ip->gid, mode));
		if (error)
			ip->mode &= ~ISGID;
	}

	/* write vnode information to the flash */
	chfs_update(vp, NULL, NULL, UPDATE_WAIT);

	mutex_enter(&chmp->chm_lock_mountfields);

	error = chfs_write_flash_vnode(chmp, ip, ALLOC_NORMAL);
	if (error) {
		mutex_exit(&chmp->chm_lock_mountfields);
		vput(vp);
		return error;
	}

	/* update parent's vnode information and write it to the flash */
	pdir->iflag |= (IN_ACCESS | IN_CHANGE | IN_MODIFY | IN_UPDATE);
	chfs_update(dvp, NULL, NULL, UPDATE_WAIT);

	error = chfs_write_flash_vnode(chmp, pdir, ALLOC_NORMAL);
	if (error) {
		mutex_exit(&chmp->chm_lock_mountfields);
		vput(vp);
		return error;
	}

	/* setup directory entry */
	nfd = chfs_alloc_dirent(cnp->cn_namelen + 1);
	nfd->vno = ip->ino;
	nfd->version = (++pdir->chvc->highest_version);
	nfd->type = ip->ch_type;
	nfd->nsize = cnp->cn_namelen;
	memcpy(&(nfd->name), cnp->cn_nameptr, cnp->cn_namelen);
	nfd->name[nfd->nsize] = 0;
	nfd->nhash = hash32_buf(nfd->name, cnp->cn_namelen, HASH32_BUF_INIT);

	/* write out */
	error = chfs_write_flash_dirent(chmp, pdir, ip, nfd, ip->ino, ALLOC_NORMAL);
	if (error) {
        mutex_exit(&chmp->chm_lock_mountfields);
		vput(vp);
		return error;
	}

	//TODO set parent's dir times

	/* add dirent to parent */
	chfs_add_fd_to_inode(chmp, pdir, nfd);

	pdir->chvc->nlink++;

	mutex_exit(&chmp->chm_lock_mountfields);

	VOP_UNLOCK(vp);
	*vpp = vp;
	return (0);
}

/* chfs_set_vnode_size - updates size of vnode and also inode */
void
chfs_set_vnode_size(struct vnode *vp, size_t size)
{
	struct chfs_inode *ip;

	KASSERT(vp != NULL);

	ip = VTOI(vp);
	KASSERT(ip != NULL);

	ip->size = size;
	vp->v_size = vp->v_writesize = size;
	return;
}

/*
 * chfs_change_size_free - updates free size 
 * "change" parameter is positive if we have to increase the size
 * and negative if we have to decrease it
 */
void
chfs_change_size_free(struct chfs_mount *chmp,
	struct chfs_eraseblock *cheb, int change)
{
	KASSERT(mutex_owned(&chmp->chm_lock_sizes));
	KASSERT((int)(chmp->chm_free_size + change) >= 0);
	KASSERT((int)(cheb->free_size + change) >= 0);
	KASSERT((int)(cheb->free_size + change) <= chmp->chm_ebh->eb_size);
	chmp->chm_free_size += change;
	cheb->free_size += change;
	return;
}

/*
 * chfs_change_size_dirty - updates dirty size 
 * "change" parameter is positive if we have to increase the size
 * and negative if we have to decrease it
 */
void
chfs_change_size_dirty(struct chfs_mount *chmp,
	struct chfs_eraseblock *cheb, int change)
{
	KASSERT(mutex_owned(&chmp->chm_lock_sizes));
	KASSERT((int)(chmp->chm_dirty_size + change) >= 0);
	KASSERT((int)(cheb->dirty_size + change) >= 0);
	KASSERT((int)(cheb->dirty_size + change) <= chmp->chm_ebh->eb_size);
	chmp->chm_dirty_size += change;
	cheb->dirty_size += change;
	return;
}

/*
 * chfs_change_size_unchecked - updates unchecked size 
 * "change" parameter is positive if we have to increase the size
 * and negative if we have to decrease it
 */
void
chfs_change_size_unchecked(struct chfs_mount *chmp,
	struct chfs_eraseblock *cheb, int change)
{
	KASSERT(mutex_owned(&chmp->chm_lock_sizes));
	KASSERT((int)(chmp->chm_unchecked_size + change) >= 0);
	KASSERT((int)(cheb->unchecked_size + change) >= 0);
	KASSERT((int)(cheb->unchecked_size + change) <= chmp->chm_ebh->eb_size);
	chmp->chm_unchecked_size += change;
	cheb->unchecked_size += change;
	return;
}

/*
 * chfs_change_size_used - updates used size
 * "change" parameter is positive if we have to increase the size
 * and negative if we have to decrease it
 */
void
chfs_change_size_used(struct chfs_mount *chmp,
	struct chfs_eraseblock *cheb, int change)
{
	KASSERT(mutex_owned(&chmp->chm_lock_sizes));
	KASSERT((int)(chmp->chm_used_size + change) >= 0);
	KASSERT((int)(cheb->used_size + change) >= 0);
	KASSERT((int)(cheb->used_size + change) <= chmp->chm_ebh->eb_size);
	chmp->chm_used_size += change;
	cheb->used_size += change;
	return;
}

/*
 * chfs_change_size_wasted - updates wasted size 
 * "change" parameter is positive if we have to increase the size
 * and negative if we have to decrease it
 */
void
chfs_change_size_wasted(struct chfs_mount *chmp,
	struct chfs_eraseblock *cheb, int change)
{
	KASSERT(mutex_owned(&chmp->chm_lock_sizes));
	KASSERT((int)(chmp->chm_wasted_size + change) >= 0);
	KASSERT((int)(cheb->wasted_size + change) >= 0);
	KASSERT((int)(cheb->wasted_size + change) <= chmp->chm_ebh->eb_size);
	chmp->chm_wasted_size += change;
	cheb->wasted_size += change;
	return;
}


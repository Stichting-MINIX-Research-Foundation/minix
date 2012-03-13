/*	$NetBSD: chfs_vnode.c,v 1.2 2011/11/24 21:09:37 agc Exp $	*/

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
#include <sys/malloc.h>
#include <sys/kauth.h>
#include <sys/namei.h>
#include <sys/uio.h>
#include <sys/buf.h>

struct vnode *
chfs_vnode_lookup(struct chfs_mount *chmp, ino_t vno)
{
	struct vnode *vp;
	struct chfs_inode *ip;

	TAILQ_FOREACH(vp, &chmp->chm_fsmp->mnt_vnodelist, v_mntvnodes) {
		ip = VTOI(vp);
		if (ip && ip->ino == vno)
			return vp;
	}
	return NULL;
}

int
chfs_readvnode(struct mount* mp, ino_t ino, struct vnode** vpp)
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

	if (chvc && ino != CHFS_ROOTINO) {
		/* debug... */
		printf("readvnode; offset: %" PRIu32 ", lnr: %d\n",
		    CHFS_GET_OFS(chvc->v->nref_offset), chvc->v->nref_lnr);

		KASSERT((void *)chvc != (void *)chvc->v);

		buf = kmem_alloc(len, KM_SLEEP);
		err = chfs_read_leb(chmp, chvc->v->nref_lnr, buf,
		    CHFS_GET_OFS(chvc->v->nref_offset), len, &retlen);
		if (err)
			return err;
		if (retlen != len) {
			chfs_err("Error reading vnode: read: %zu insted of: %zu\n",
			    len, retlen);
			return EIO;
		}
		chfvn = (struct chfs_flash_vnode*)buf;
		chfs_set_vnode_size(vp, chfvn->dn_size);
		ip->mode = chfvn->mode;
		vp->v_type = IFTOVT(ip->mode);
		ip->version = chfvn->version;
		//ip->chvc->highest_version = ip->version;
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

int
chfs_readdirent(struct mount *mp, struct chfs_node_ref *chnr, struct chfs_inode *pdir)
{
	struct ufsmount *ump = VFSTOUFS(mp);
	struct chfs_mount *chmp = ump->um_chfs;
	struct chfs_flash_dirent_node chfdn;
	struct chfs_dirent *fd;//, *pdents;
	size_t len = sizeof(struct chfs_flash_dirent_node);
//	struct chfs_vnode_cache* parent;
	size_t retlen;
	int err = 0;

//	parent = chfs_get_vnode_cache(chmp, pdir->ino);

	//read flash_dirent_node
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

	//set fields of dirent
	fd = chfs_alloc_dirent(chfdn.nsize + 1);
	fd->version = chfdn.version;
	fd->vno = chfdn.vno;
	fd->type = chfdn.dtype;
	fd->nsize = chfdn.nsize;
//	fd->next = NULL;

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

	chfs_add_fd_to_inode(chmp, pdir, fd);
/*
  pdents = pdir->i_chfs_ext.dents;
  if (!pdents)
  pdir->i_chfs_ext.dents = fd;
  else {
  while (pdents->next != NULL) {
  pdents = pdents->next;
  }
  pdents->next = fd;
  }
*/
	return 0;
}

/*
 * Allocate a new inode.
 */
int
chfs_makeinode(int mode, struct vnode *dvp, struct vnode **vpp,
    struct componentname *cnp, int type)
{
	struct chfs_inode *ip, *pdir;
	struct vnode *vp;
	struct ufsmount* ump = VFSTOUFS(dvp->v_mount);
	struct chfs_mount* chmp = ump->um_chfs;
	struct chfs_vnode_cache* chvc;
	int error, ismember = 0;
	ino_t vno;
	struct chfs_dirent *nfd;//, *fd;

	dbg("makeinode\n");
	pdir = VTOI(dvp);

	*vpp = NULL;

	vno = ++(chmp->chm_max_vno);

	error = VFS_VGET(dvp->v_mount, vno, &vp);
	if (error)
		return (error);

	mutex_enter(&chmp->chm_lock_vnocache);
	chvc = chfs_vnode_cache_get(chmp, vno);
	mutex_exit(&chmp->chm_lock_vnocache);

	chvc->pvno = pdir->ino;
	chvc->vno_version = kmem_alloc(sizeof(uint64_t), KM_SLEEP);
	*(chvc->vno_version) = 1;
	if (type != VDIR)
		chvc->nlink = 1;
	else
		chvc->nlink = 2;
//	chfs_vnode_cache_set_state(chmp, chvc, VNO_STATE_CHECKEDABSENT);
	chvc->state = VNO_STATE_CHECKEDABSENT;

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
	//ip->chvc->highest_version = 1;
	ip->target = NULL;

	ip->mode = mode;
	vp->v_type = type;	/* Rest init'd in getnewvnode(). */
	if ((ip->mode & ISGID) && (kauth_cred_ismember_gid(cnp->cn_cred,
		ip->gid, &ismember) != 0 || !ismember) &&
	    kauth_authorize_generic(cnp->cn_cred, KAUTH_GENERIC_ISSUSER, NULL))
		ip->mode &= ~ISGID;

	chfs_update(vp, NULL, NULL, UPDATE_WAIT);

	mutex_enter(&chmp->chm_lock_mountfields);

	//write inode to flash
	error = chfs_write_flash_vnode(chmp, ip, ALLOC_NORMAL);
	if (error) {
		mutex_exit(&chmp->chm_lock_mountfields);
		vput(vp);
		vput(dvp);
		return error;
	}
	//update parent directory and write it to the flash
	pdir->iflag |= (IN_ACCESS | IN_CHANGE | IN_MODIFY | IN_UPDATE);
	chfs_update(dvp, NULL, NULL, UPDATE_WAIT);

	error = chfs_write_flash_vnode(chmp, pdir, ALLOC_NORMAL);
	if (error) {
		mutex_exit(&chmp->chm_lock_mountfields);
		vput(vp);
		vput(dvp);
		return error;
	}
	vput(dvp);

	//set up node's full dirent
	nfd = chfs_alloc_dirent(cnp->cn_namelen + 1);
	nfd->vno = ip->ino;
	nfd->version = (++pdir->chvc->highest_version);
	nfd->type = type;
//	nfd->next = NULL;
	nfd->nsize = cnp->cn_namelen;
	memcpy(&(nfd->name), cnp->cn_nameptr, cnp->cn_namelen);
	nfd->name[nfd->nsize] = 0;
	nfd->nhash = hash32_buf(nfd->name, cnp->cn_namelen, HASH32_BUF_INIT);

	// write out direntry
	error = chfs_write_flash_dirent(chmp, pdir, ip, nfd, ip->ino, ALLOC_NORMAL);
	if (error) {
        mutex_exit(&chmp->chm_lock_mountfields);
		vput(vp);
		return error;
	}

	//TODO set parent's dir times

	chfs_add_fd_to_inode(chmp, pdir, nfd);
/*
  fd = pdir->i_chfs_ext.dents;
  if (!fd)
  pdir->i_chfs_ext.dents = nfd;
  else {
  while (fd->next != NULL) {
  fd = fd->next;
  }
  fd->next = nfd;
  }
*/
	//pdir->i_nlink++;
	pdir->chvc->nlink++;

	mutex_exit(&chmp->chm_lock_mountfields);

	*vpp = vp;
	return (0);
}

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


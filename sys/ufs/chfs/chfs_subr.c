/*	$NetBSD: chfs_subr.c,v 1.9 2013/10/20 17:18:38 christos Exp $	*/

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

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/dirent.h>
#include <sys/event.h>
#include <sys/kmem.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/systm.h>
#include <sys/swap.h>
#include <sys/vnode.h>
#include <sys/kauth.h>
#include <sys/proc.h>
#include <sys/atomic.h>

#include <uvm/uvm.h>

#include <miscfs/specfs/specdev.h>
#include <miscfs/genfs/genfs.h>
#include "chfs.h"


/*
 * chfs_mem_info -
 * Returns information about the number of available memory pages,
 * including physical and virtual ones.
 *
 * If 'total' is true, the value returned is the total amount of memory
 * pages configured for the system (either in use or free).
 * If it is FALSE, the value returned is the amount of free memory pages.
 *
 * Remember to remove DUMMYFS_PAGES_RESERVED from the returned value to avoid
 * excessive memory usage.
 *
 */
size_t
chfs_mem_info(bool total)
{
	size_t size;

	size = 0;
	size += uvmexp.swpgavail;
	if (!total) {
		size -= uvmexp.swpgonly;
	}
	size += uvmexp.free;
	size += uvmexp.filepages;
	if (size > uvmexp.wired) {
		size -= uvmexp.wired;
	} else {
		size = 0;
	}

	return size;
}


/*
 * chfs_dir_lookup -
 * Looks for a directory entry in the directory represented by node.
 * 'cnp' describes the name of the entry to look for.  Note that the .
 * and .. components are not allowed as they do not physically exist
 * within directories.
 *
 * Returns a pointer to the entry when found, otherwise NULL.
 */
struct chfs_dirent *
chfs_dir_lookup(struct chfs_inode *ip, struct componentname *cnp)
{
	bool found;
	struct chfs_dirent *fd;
	dbg("dir_lookup()\n");

	KASSERT(IMPLIES(cnp->cn_namelen == 1, cnp->cn_nameptr[0] != '.'));
	KASSERT(IMPLIES(cnp->cn_namelen == 2, !(cnp->cn_nameptr[0] == '.' &&
		    cnp->cn_nameptr[1] == '.')));

	found = false;
	TAILQ_FOREACH(fd, &ip->dents, fds) {
		KASSERT(cnp->cn_namelen < 0xffff);
		if (fd->vno == 0)
			continue;
		if (fd->nsize == (uint16_t)cnp->cn_namelen &&
		    memcmp(fd->name, cnp->cn_nameptr, fd->nsize) == 0) {
			found = true;
			break;
		}
	}

	return found ? fd : NULL;
}

/*
 * chfs_filldir - 
 * Creates a (kernel) dirent and moves it to the given memory address.
 * Used during readdir.
 */
int
chfs_filldir(struct uio* uio, ino_t ino, const char *name,
    int namelen, enum chtype type)
{
	struct dirent dent;
	int error;

	memset(&dent, 0, sizeof(dent));

	dent.d_fileno = ino;
	switch (type) {
	case CHT_BLK:
		dent.d_type = DT_BLK;
		break;

	case CHT_CHR:
		dent.d_type = DT_CHR;
		break;

	case CHT_DIR:
		dent.d_type = DT_DIR;
		break;

	case CHT_FIFO:
		dent.d_type = DT_FIFO;
		break;

	case CHT_LNK:
		dent.d_type = DT_LNK;
		break;

	case CHT_REG:
		dent.d_type = DT_REG;
		break;

	case CHT_SOCK:
		dent.d_type = DT_SOCK;
		break;

	default:
		KASSERT(0);
	}
	dent.d_namlen = namelen;
	(void)memcpy(dent.d_name, name, dent.d_namlen);
	dent.d_reclen = _DIRENT_SIZE(&dent);

	if (dent.d_reclen > uio->uio_resid) {
		error = -1;
	} else {
		error = uiomove(&dent, dent.d_reclen, uio);
	}

	return error;
}

/*
 * chfs_chsize - change size of the given vnode
 * Caller should execute chfs_update on vp after a successful execution.
 * The vnode must be locked on entry and remain locked on exit.
 */
int
chfs_chsize(struct vnode *vp, u_quad_t size, kauth_cred_t cred)
{
	struct chfs_mount *chmp;
	struct chfs_inode *ip;

	ip = VTOI(vp);
	chmp = ip->chmp;

	dbg("chfs_chsize\n");

	switch (ip->ch_type) {
	case CHT_DIR:
		return EISDIR;
	case CHT_LNK:
	case CHT_REG:
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return EROFS;
		break;
	case CHT_BLK:
	case CHT_CHR:
	case CHT_FIFO:
		return 0;
	default:
		return EOPNOTSUPP; /* XXX why not ENODEV? */
	}

	vflushbuf(vp, 0);

	mutex_enter(&chmp->chm_lock_mountfields);

	if (ip->size < size) {
		uvm_vnp_setsize(vp, size);
		chfs_set_vnode_size(vp, size);
		ip->iflag |= IN_CHANGE | IN_UPDATE;

		mutex_exit(&chmp->chm_lock_mountfields);
		return 0;
	}

	if (size != 0) {
		ubc_zerorange(&vp->v_uobj, size, ip->size - size, UBC_UNMAP_FLAG(vp));
	}
	
	/* drop unused fragments */
	chfs_truncate_fragtree(ip->chmp, &ip->fragtree, size);

	uvm_vnp_setsize(vp, size);
	chfs_set_vnode_size(vp, size);
	ip->iflag |= IN_CHANGE | IN_UPDATE;
	mutex_exit(&chmp->chm_lock_mountfields);
	return 0;
}

/*
 * chfs_chflags - change flags of the given vnode
 * Caller should execute chfs_update on vp after a successful execution.
 * The vnode must be locked on entry and remain locked on exit.
 */
int
chfs_chflags(struct vnode *vp, int flags, kauth_cred_t cred)
{
	struct chfs_inode *ip;
	int error = 0;
	kauth_action_t action = KAUTH_VNODE_WRITE_FLAGS;
	bool changing_sysflags = false;

	ip = VTOI(vp);

	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return EROFS;

	if ((flags & SF_SNAPSHOT) != (ip->flags & SF_SNAPSHOT))
		return EPERM;

	/* Indicate we're changing system flags if we are. */
	if ((ip->flags & SF_SETTABLE) != (flags & SF_SETTABLE) ||
	    (flags & UF_SETTABLE) != flags) {
		action |= KAUTH_VNODE_WRITE_SYSFLAGS;
		changing_sysflags = true;
	}

	/* Indicate the node has system flags if it does. */
	if (ip->flags & (SF_IMMUTABLE | SF_APPEND)) {
		action |= KAUTH_VNODE_HAS_SYSFLAGS;
	}

	error = kauth_authorize_vnode(cred, action, vp, NULL,
	    genfs_can_chflags(cred, CHTTOVT(ip->ch_type), ip->uid, changing_sysflags));
	if (error)
		return error;

	if (changing_sysflags) {
		ip->flags = flags;
	} else {
		ip->flags &= SF_SETTABLE;
		ip->flags |= (flags & UF_SETTABLE);
	}
	ip->iflag |= IN_CHANGE;
	error = chfs_update(vp, NULL, NULL, UPDATE_WAIT);
	if (error)
		return error;

	if (flags & (IMMUTABLE | APPEND))
		return 0;

	return error;
}


/* chfs_itimes - updates a vnode times to the given data */
void
chfs_itimes(struct chfs_inode *ip, const struct timespec *acc,
    const struct timespec *mod, const struct timespec *cre)
{
	struct timespec now;

	if (!(ip->iflag & (IN_ACCESS | IN_CHANGE | IN_UPDATE | IN_MODIFY))) {
		return;
	}

	vfs_timestamp(&now);
	if (ip->iflag & IN_ACCESS) {
		if (acc == NULL)
			acc = &now;
		ip->atime = acc->tv_sec;
	}
	if (ip->iflag & (IN_UPDATE | IN_MODIFY)) {
		if (mod == NULL)
			mod = &now;
		ip->mtime = mod->tv_sec;
	}
	if (ip->iflag & (IN_CHANGE | IN_MODIFY)) {
		if (cre == NULL)
			cre = &now;
		ip->ctime = cre->tv_sec;
	}
	if (ip->iflag & (IN_ACCESS | IN_MODIFY))
		ip->iflag |= IN_ACCESSED;
	if (ip->iflag & (IN_UPDATE | IN_CHANGE))
		ip->iflag |= IN_MODIFIED;
	ip->iflag &= ~(IN_ACCESS | IN_CHANGE | IN_UPDATE | IN_MODIFY);
}

/* chfs_update - updates a vnode times */
int
chfs_update(struct vnode *vp, const struct timespec *acc,
    const struct timespec *mod, int flags)
{
	struct chfs_inode *ip;

	/* XXX ufs_reclaim calls this function unlocked! */

	ip = VTOI(vp);
	chfs_itimes(ip, acc, mod, NULL);

	return (0);
}


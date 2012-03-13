/*	$NetBSD: chfs_subr.c,v 1.2 2011/11/24 21:09:37 agc Exp $	*/

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

/*
 * Efficient memory file system supporting functions.
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
#include "chfs.h"
//#include <fs/chfs/chfs_vnops.h>
//#include </root/xipffs/netbsd.chfs/chfs.h>

/* --------------------------------------------------------------------- */

/*
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


/* --------------------------------------------------------------------- */

/*
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
	//CHFS_VALIDATE_DIR(node);

	//node->chn_status |= CHFS_NODE_ACCESSED;

	found = false;
//	fd = ip->dents;
//	while(fd) {
	TAILQ_FOREACH(fd, &ip->dents, fds) {
		KASSERT(cnp->cn_namelen < 0xffff);
		if (fd->vno == 0)
			continue;
		/*dbg("dirent dump:\n");
		  dbg(" ->vno:     %d\n", fd->vno);
		  dbg(" ->version: %ld\n", fd->version);
		  dbg(" ->nhash:   0x%x\n", fd->nhash);
		  dbg(" ->nsize:   %d\n", fd->nsize);
		  dbg(" ->name:    %s\n", fd->name);
		  dbg(" ->type:    %d\n", fd->type);*/
		if (fd->nsize == (uint16_t)cnp->cn_namelen &&
		    memcmp(fd->name, cnp->cn_nameptr, fd->nsize) == 0) {
			found = true;
			break;
		}
//		fd = fd->next;
	}

	return found ? fd : NULL;
}

/* --------------------------------------------------------------------- */

int
chfs_filldir(struct uio* uio, ino_t ino, const char *name,
    int namelen, enum vtype type)
{
	struct dirent dent;
	int error;

	memset(&dent, 0, sizeof(dent));

	dent.d_fileno = ino;
	switch (type) {
	case VBLK:
		dent.d_type = DT_BLK;
		break;

	case VCHR:
		dent.d_type = DT_CHR;
		break;

	case VDIR:
		dent.d_type = DT_DIR;
		break;

	case VFIFO:
		dent.d_type = DT_FIFO;
		break;

	case VLNK:
		dent.d_type = DT_LNK;
		break;

	case VREG:
		dent.d_type = DT_REG;
		break;

	case VSOCK:
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


/* --------------------------------------------------------------------- */

/*
 * Change size of the given vnode.
 * Caller should execute chfs_update on vp after a successful execution.
 * The vnode must be locked on entry and remain locked on exit.
 */
int
chfs_chsize(struct vnode *vp, u_quad_t size, kauth_cred_t cred)
{
	struct chfs_mount *chmp;
	struct chfs_inode *ip;
	struct buf *bp;
	int blknum, append;
	int error = 0;
	char *buf = NULL;
	struct chfs_full_dnode *fd;

	ip = VTOI(vp);
	chmp = ip->chmp;

	dbg("chfs_chsize\n");

	switch (vp->v_type) {
	case VDIR:
		return EISDIR;
	case VLNK:
	case VREG:
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return EROFS;
		break;
	case VBLK:
	case VCHR:
	case VFIFO:
		return 0;
	default:
		return EOPNOTSUPP; /* XXX why not ENODEV? */
	}

	vflushbuf(vp, 0);

	mutex_enter(&chmp->chm_lock_mountfields);
	chfs_flush_pending_wbuf(chmp);

	/* handle truncate to zero as a special case */
	if (size == 0) {
		dbg("truncate to zero");
		chfs_truncate_fragtree(ip->chmp,
		    &ip->fragtree, size);
		chfs_set_vnode_size(vp, size);

		mutex_exit(&chmp->chm_lock_mountfields);

		return 0;
	}


	/* allocate zeros for the new data */
	buf = kmem_zalloc(size, KM_SLEEP);
	bp = getiobuf(vp, true);

	if (ip->size != 0) {
		/* read the whole data */
		bp->b_blkno = 0;
		bp->b_bufsize = bp->b_resid = bp->b_bcount = ip->size;
		bp->b_data = kmem_alloc(ip->size, KM_SLEEP);

		error = chfs_read_data(chmp, vp, bp);
		if (error) {
			mutex_exit(&chmp->chm_lock_mountfields);
			putiobuf(bp);

			return error;
		}

		/* create the new data */
		dbg("create new data vap%llu ip%llu\n",
			(unsigned long long)size, (unsigned long long)ip->size);
		append = size - ip->size;
		if (append > 0) {
			memcpy(buf, bp->b_data, ip->size);
		} else {
			memcpy(buf, bp->b_data, size);
			chfs_truncate_fragtree(ip->chmp,
				&ip->fragtree, size);
		}

		kmem_free(bp->b_data, ip->size);

		struct chfs_node_frag *lastfrag = frag_last(&ip->fragtree);
		fd = lastfrag->node;
		chfs_mark_node_obsolete(chmp, fd->nref);

		blknum = lastfrag->ofs / PAGE_SIZE;
		lastfrag->size = append > PAGE_SIZE ? PAGE_SIZE : size % PAGE_SIZE;
	} else {
		fd = chfs_alloc_full_dnode();
		blknum = 0;
	}

	chfs_set_vnode_size(vp, size);

	// write the new data
	for (bp->b_blkno = blknum; bp->b_blkno * PAGE_SIZE < size; bp->b_blkno++) {
		uint64_t writesize = MIN(size - bp->b_blkno * PAGE_SIZE, PAGE_SIZE);

		bp->b_bufsize = bp->b_resid = bp->b_bcount = writesize;
		bp->b_data = kmem_alloc(writesize, KM_SLEEP);

		memcpy(bp->b_data, buf + (bp->b_blkno * PAGE_SIZE), writesize);

		if (bp->b_blkno != blknum) {
			fd = chfs_alloc_full_dnode();
		}

		error = chfs_write_flash_dnode(chmp, vp, bp, fd);
		if (error) {
			mutex_exit(&chmp->chm_lock_mountfields);
			kmem_free(bp->b_data, writesize);
			putiobuf(bp);

			return error;
		}
		if (bp->b_blkno != blknum) {
			chfs_add_full_dnode_to_inode(chmp, ip, fd);
		}
		kmem_free(bp->b_data, writesize);
	}

	mutex_exit(&chmp->chm_lock_mountfields);

	kmem_free(buf, size);
	putiobuf(bp);

	return 0;
}
#if 0
	int error;
	struct chfs_node *node;

	KASSERT(VOP_ISLOCKED(vp));

	node = VP_TO_CHFS_NODE(vp);

	// Decide whether this is a valid operation based on the file type.
	error = 0;
	switch (vp->v_type) {
	case VDIR:
		return EISDIR;

	case VREG:
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return EROFS;
		break;

	case VBLK:
	case VCHR:
	case VFIFO:
		// Allow modifications of special files even if in the file
		// system is mounted read-only (we are not modifying the
		// files themselves, but the objects they represent).
		return 0;

	default:
		return ENODEV;
	}

	// Immutable or append-only files cannot be modified, either.
	if (node->chn_flags & (IMMUTABLE | APPEND))
		return EPERM;

	error = chfs_truncate(vp, size);
	// chfs_truncate will raise the NOTE_EXTEND and NOTE_ATTRIB kevents
	// for us, as will update dn_status; no need to do that here.

	KASSERT(VOP_ISLOCKED(vp));

	return error;
#endif

/* --------------------------------------------------------------------- */

/*
 * Change flags of the given vnode.
 * Caller should execute chfs_update on vp after a successful execution.
 * The vnode must be locked on entry and remain locked on exit.
 */
int
chfs_chflags(struct vnode *vp, int flags, kauth_cred_t cred)
{
	struct chfs_mount *chmp;
	struct chfs_inode *ip;
	int error = 0;

	ip = VTOI(vp);
	chmp = ip->chmp;

	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return EROFS;

	if (kauth_cred_geteuid(cred) != ip->uid &&
	    (error = kauth_authorize_generic(cred,
		KAUTH_GENERIC_ISSUSER, NULL)))
		return error;

	if (kauth_authorize_generic(cred, KAUTH_GENERIC_ISSUSER,
		NULL) == 0) {
		if ((ip->flags & (SF_IMMUTABLE | SF_APPEND)) &&
		    kauth_authorize_system(curlwp->l_cred,
			KAUTH_SYSTEM_CHSYSFLAGS, 0, NULL, NULL, NULL))
			return EPERM;

		if ((flags & SF_SNAPSHOT) !=
		    (ip->flags & SF_SNAPSHOT))
			return EPERM;

		ip->flags = flags;
	} else {
		if ((ip->flags & (SF_IMMUTABLE | SF_APPEND)) ||
		    (flags & UF_SETTABLE) != flags)
			return EPERM;

		if ((ip->flags & SF_SETTABLE) !=
		    (flags & SF_SETTABLE))
			return EPERM;

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

/* --------------------------------------------------------------------- */

void
chfs_itimes(struct chfs_inode *ip, const struct timespec *acc,
    const struct timespec *mod, const struct timespec *cre)
{
	//dbg("itimes\n");
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
		//ip->i_modrev++;
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

/* --------------------------------------------------------------------- */

int
chfs_update(struct vnode *vp, const struct timespec *acc,
    const struct timespec *mod, int flags)
{

	struct chfs_inode *ip;

	/* XXX ufs_reclaim calls this function unlocked! */
//	KASSERT(VOP_ISLOCKED(vp));

#if 0
	if (flags & UPDATE_CLOSE)
		; /* XXX Need to do anything special? */
#endif

	ip = VTOI(vp);
	chfs_itimes(ip, acc, mod, NULL);

//	KASSERT(VOP_ISLOCKED(vp));
	return (0);
}

/* --------------------------------------------------------------------- */
/*
  int
  chfs_truncate(struct vnode *vp, off_t length)
  {
  bool extended;
  int error;
  struct chfs_node *node;
  printf("CHFS: truncate()\n");

  node = VP_TO_CHFS_NODE(vp);
  extended = length > node->chn_size;

  if (length < 0) {
  error = EINVAL;
  goto out;
  }

  if (node->chn_size == length) {
  error = 0;
  goto out;
  }

  error = chfs_reg_resize(vp, length);
  if (error == 0)
  node->chn_status |= CHFS_NODE_CHANGED | CHFS_NODE_MODIFIED;

  out:
  chfs_update(vp, NULL, NULL, 0);

  return error;
  }*/



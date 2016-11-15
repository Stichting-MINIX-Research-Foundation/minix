/* $NetBSD: nilfs_vnops.c,v 1.32 2015/04/20 23:03:08 riastradh Exp $ */

/*
 * Copyright (c) 2008, 2009 Reinoud Zandijk
 * All rights reserved.
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#include <sys/cdefs.h>
#ifndef lint
__KERNEL_RCSID(0, "$NetBSD: nilfs_vnops.c,v 1.32 2015/04/20 23:03:08 riastradh Exp $");
#endif /* not lint */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/resourcevar.h>	/* defines plimit structure in proc struct */
#include <sys/kernel.h>
#include <sys/file.h>		/* define FWRITE ... */
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/signalvar.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/lockf.h>
#include <sys/kauth.h>

#include <miscfs/genfs/genfs.h>
#include <uvm/uvm_extern.h>

#include <fs/nilfs/nilfs_mount.h>
#include "nilfs.h"
#include "nilfs_subr.h"
#include "nilfs_bswap.h"


#define VTOI(vnode) ((struct nilfs_node *) (vnode)->v_data)


/* externs */
extern int prtactive;

/* implementations of vnode functions; table follows at end */
/* --------------------------------------------------------------------- */

int
nilfs_inactive(void *v)
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		bool         *a_recycle;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nilfs_node *nilfs_node = VTOI(vp);

	DPRINTF(NODE, ("nilfs_inactive called for nilfs_node %p\n", VTOI(vp)));

	if (nilfs_node == NULL) {
		DPRINTF(NODE, ("nilfs_inactive: inactive NULL NILFS node\n"));
		VOP_UNLOCK(vp);
		return 0;
	}

	/*
	 * Optionally flush metadata to disc. If the file has not been
	 * referenced anymore in a directory we ought to free up the resources
	 * on disc if applicable.
	 */
	VOP_UNLOCK(vp);

	return 0;
}

/* --------------------------------------------------------------------- */

int
nilfs_reclaim(void *v)
{
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nilfs_node *nilfs_node = VTOI(vp);

	DPRINTF(NODE, ("nilfs_reclaim called for node %p\n", nilfs_node));
	if (prtactive && vp->v_usecount > 1)
		vprint("nilfs_reclaim(): pushing active", vp);

	if (nilfs_node == NULL) {
		DPRINTF(NODE, ("nilfs_reclaim(): null nilfsnode\n"));
		return 0;
	}

	/* update note for closure */
	nilfs_update(vp, NULL, NULL, NULL, UPDATE_CLOSE);

	/* remove from vnode cache. */
	vcache_remove(vp->v_mount, &nilfs_node->ino, sizeof(nilfs_node->ino));

	/* dispose all node knowledge */
	genfs_node_destroy(vp);
	nilfs_dispose_node(&nilfs_node);

	vp->v_data = NULL;

	return 0;
}

/* --------------------------------------------------------------------- */

int
nilfs_read(void *v)
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp     = ap->a_vp;
	struct uio   *uio    = ap->a_uio;
	int           ioflag = ap->a_ioflag;
	int           advice = IO_ADV_DECODE(ap->a_ioflag);
	struct uvm_object    *uobj;
	struct nilfs_node      *nilfs_node = VTOI(vp);
	uint64_t file_size;
	vsize_t len;
	int error;

	DPRINTF(READ, ("nilfs_read called\n"));

	/* can this happen? some filingsystems have this check */
	if (uio->uio_offset < 0)
		return EINVAL;
	if (uio->uio_resid == 0)
		return 0;

	/* protect against rogue programs reading raw directories and links */
	if ((ioflag & IO_ALTSEMANTICS) == 0) {
		if (vp->v_type == VDIR)
			return EISDIR;
		/* all but regular files just give EINVAL */
		if (vp->v_type != VREG)
			return EINVAL;
	}

	assert(nilfs_node);
	file_size = nilfs_rw64(nilfs_node->inode.i_size);

	/* read contents using buffercache */
	uobj = &vp->v_uobj;
	error = 0;
	while (uio->uio_resid > 0) {
		/* reached end? */
		if (file_size <= uio->uio_offset)
			break;

		/* maximise length to file extremity */
		len = MIN(file_size - uio->uio_offset, uio->uio_resid);
		if (len == 0)
			break;

		/* ubc, here we come, prepare to trap */
		error = ubc_uiomove(uobj, uio, len, advice,
		    UBC_READ | UBC_PARTIALOK | UBC_UNMAP_FLAG(vp));
		if (error)
			break;
	}

	/* note access time unless not requested */
	if (!(vp->v_mount->mnt_flag & MNT_NOATIME)) {
		nilfs_node->i_flags |= IN_ACCESS;
		if ((ioflag & IO_SYNC) == IO_SYNC)
			error = nilfs_update(vp, NULL, NULL, NULL, UPDATE_WAIT);
	}

	return error;
}

/* --------------------------------------------------------------------- */

int
nilfs_write(void *v)
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp     = ap->a_vp;
	struct uio   *uio    = ap->a_uio;
	int           ioflag = ap->a_ioflag;
	int           advice = IO_ADV_DECODE(ap->a_ioflag);
	struct uvm_object    *uobj;
	struct nilfs_node      *nilfs_node = VTOI(vp);
	uint64_t file_size;
	vsize_t len;
	int error, resid, extended;

	DPRINTF(WRITE, ("nilfs_write called\n"));

	/* can this happen? some filingsystems have this check */
	if (uio->uio_offset < 0)
		return EINVAL;
	if (uio->uio_resid == 0)
		return 0;

	/* protect against rogue programs writing raw directories or links */
	if ((ioflag & IO_ALTSEMANTICS) == 0) {
		if (vp->v_type == VDIR)
			return EISDIR;
		/* all but regular files just give EINVAL for now */
		if (vp->v_type != VREG)
			return EINVAL;
	}

	assert(nilfs_node);
	panic("nilfs_write() called\n");

	/* remember old file size */
	assert(nilfs_node);
	file_size = nilfs_rw64(nilfs_node->inode.i_size);

	/* if explicitly asked to append, uio_offset can be wrong? */
	if (ioflag & IO_APPEND)
		uio->uio_offset = file_size;

#if 0
	extended = (uio->uio_offset + uio->uio_resid > file_size);
	if (extended) {
		DPRINTF(WRITE, ("extending file from %"PRIu64" to %"PRIu64"\n",
			file_size, uio->uio_offset + uio->uio_resid));
		error = nilfs_grow_node(nilfs_node, uio->uio_offset + uio->uio_resid);
		if (error)
			return error;
		file_size = uio->uio_offset + uio->uio_resid;
	}
#endif

	/* write contents using buffercache */
	uobj = &vp->v_uobj;
	resid = uio->uio_resid;
	error = 0;

	uvm_vnp_setwritesize(vp, file_size);
	while (uio->uio_resid > 0) {
		/* maximise length to file extremity */
		len = MIN(file_size - uio->uio_offset, uio->uio_resid);
		if (len == 0)
			break;

		/* ubc, here we come, prepare to trap */
		error = ubc_uiomove(uobj, uio, len, advice,
		    UBC_WRITE | UBC_UNMAP_FLAG(vp));
		if (error)
			break;
	}
	uvm_vnp_setsize(vp, file_size);

	/* mark node changed and request update */
	nilfs_node->i_flags |= IN_CHANGE | IN_UPDATE;
	if (vp->v_mount->mnt_flag & MNT_RELATIME)
		nilfs_node->i_flags |= IN_ACCESS;

	/*
	 * XXX TODO FFS has code here to reset setuid & setgid when we're not
	 * the superuser as a precaution against tampering.
	 */

	/* if we wrote a thing, note write action on vnode */
	if (resid > uio->uio_resid)
		VN_KNOTE(vp, NOTE_WRITE | (extended ? NOTE_EXTEND : 0));

	if (error) {
		/* bring back file size to its former size */
		/* take notice of its errors? */
//		(void) nilfs_chsize(vp, (u_quad_t) old_size, NOCRED);

		/* roll back uio */
		uio->uio_offset -= resid - uio->uio_resid;
		uio->uio_resid = resid;
	} else {
		/* if we write and we're synchronous, update node */
		if ((resid > uio->uio_resid) && ((ioflag & IO_SYNC) == IO_SYNC))
			error = nilfs_update(vp, NULL, NULL, NULL, UPDATE_WAIT);
	}

	return error;
}


/* --------------------------------------------------------------------- */

/*
 * bmap functionality that translates logical block numbers to the virtual
 * block numbers to be stored on the vnode itself.
 *
 * Important alert!
 *
 * If runp is not NULL, the number of contiguous blocks __starting from the
 * next block after the queried block__ will be returned in runp.
 */

int
nilfs_trivial_bmap(void *v)
{
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
	} */ *ap = v;
	struct vnode  *vp  = ap->a_vp;	/* our node	*/
	struct vnode **vpp = ap->a_vpp;	/* return node	*/
	daddr_t *bnp  = ap->a_bnp;	/* translated	*/
	daddr_t  bn   = ap->a_bn;	/* origional	*/
	int     *runp = ap->a_runp;
	struct nilfs_node *node = VTOI(vp);
	uint64_t *l2vmap;
	uint32_t blocksize;
	int blks, run, error;

	DPRINTF(TRANSLATE, ("nilfs_bmap() called\n"));
	/* XXX could return `-1' to indicate holes/zero's */

	blocksize = node->nilfsdev->blocksize;
	blks = MAXPHYS / blocksize;

	/* get mapping memory */
	l2vmap = malloc(sizeof(uint64_t) * blks, M_TEMP, M_WAITOK);

	/* get virtual block numbers for the vnode's buffer span */
	error = nilfs_btree_nlookup(node, bn, blks, l2vmap);
	if (error) {
		free(l2vmap, M_TEMP);
		return error;
	}

	/* store virtual blocks on our own vp */
	if (vpp)
		*vpp = vp;

	/* start at virt[0] */
	*bnp = l2vmap[0];

	/* get runlength */
	run = 1;
	while ((run < blks) && (l2vmap[run] == *bnp + run))
		run++;
	run--;	/* see comment at start of function */

	/* set runlength */
	if (runp)
		*runp = run;

	DPRINTF(TRANSLATE, ("\tstart %"PRIu64" -> %"PRIu64" run %d\n",
		bn, *bnp, run));

	/* mark not translated on virtual block number 0 */
	if (*bnp == 0)
		*bnp = -1;

	/* return success */
	free(l2vmap, M_TEMP);
	return 0;
}

/* --------------------------------------------------------------------- */

static void
nilfs_read_filebuf(struct nilfs_node *node, struct buf *bp)
{
	struct nilfs_device *nilfsdev = node->nilfsdev;
	struct buf *nbp;
	uint64_t *l2vmap, *v2pmap;
	uint64_t from, blks;
	uint32_t blocksize, buf_offset;
	uint8_t  *buf_pos;
	int blk2dev = nilfsdev->blocksize / DEV_BSIZE;
	int i, error;

	/*
	 * Translate all the block sectors into a series of buffers to read
	 * asynchronously from the nilfs device. Note that this lookup may
	 * induce readin's too.
	 */

	blocksize = nilfsdev->blocksize;

	from = bp->b_blkno;
	blks = bp->b_bcount / blocksize;

	DPRINTF(READ, ("\tread in from inode %"PRIu64" blkno %"PRIu64" "
			"+ %"PRIu64" blocks\n", node->ino, from, blks));

	DPRINTF(READ, ("\t\tblkno %"PRIu64" "
			"+ %d bytes\n", bp->b_blkno, bp->b_bcount));

	/* get mapping memory */
	l2vmap = malloc(sizeof(uint64_t) * blks, M_TEMP, M_WAITOK);
	v2pmap = malloc(sizeof(uint64_t) * blks, M_TEMP, M_WAITOK);

	/* get virtual block numbers for the vnode's buffer span */
	for (i = 0; i < blks; i++)
		l2vmap[i] = from + i;

	/* translate virtual block numbers to physical block numbers */
	error = nilfs_nvtop(node, blks, l2vmap, v2pmap);
	if (error)
		goto out;

	/* issue translated blocks */
	bp->b_resid = bp->b_bcount;
	for (i = 0; i < blks; i++) {
		DPRINTF(READ, ("read_filebuf : ino %"PRIu64" blk %d -> "
			"%"PRIu64" -> %"PRIu64"\n",
			node->ino, i, l2vmap[i], v2pmap[i]));

		buf_offset = i * blocksize;
		buf_pos    = (uint8_t *) bp->b_data + buf_offset;

		/* note virtual block 0 marks not mapped */
		if (l2vmap[i] == 0) {
			memset(buf_pos, 0, blocksize);
			nestiobuf_done(bp, blocksize, 0);
			continue;
		}

		/* nest iobuf */
		nbp = getiobuf(NULL, true);
		nestiobuf_setup(bp, nbp, buf_offset, blocksize);
		KASSERT(nbp->b_vp == node->vnode);
		/* nbp is B_ASYNC */

		nbp->b_lblkno   = i;
		nbp->b_blkno    = v2pmap[i] * blk2dev;	/* in DEV_BSIZE */
		nbp->b_rawblkno = nbp->b_blkno;

		VOP_STRATEGY(nilfsdev->devvp, nbp);
	}

	if ((bp->b_flags & B_ASYNC) == 0)
		biowait(bp);

out:
	free(l2vmap, M_TEMP);
	free(v2pmap, M_TEMP);
	if (error) {
		bp->b_error = EIO;
		biodone(bp);
	}
}


static void
nilfs_write_filebuf(struct nilfs_node *node, struct buf *bp)
{
	/* TODO pass on to segment collector */
	panic("nilfs_strategy writing called\n");
}


int
nilfs_vfsstrategy(void *v)
{
	struct vop_strategy_args /* {
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct buf   *bp = ap->a_bp;
	struct nilfs_node *node = VTOI(vp);

	DPRINTF(STRATEGY, ("nilfs_strategy called\n"));

	/* check if we ought to be here */
	if (vp->v_type == VBLK || vp->v_type == VCHR)
		panic("nilfs_strategy: spec");

	/* translate if needed and pass on */
	if (bp->b_flags & B_READ) {
		nilfs_read_filebuf(node, bp);
		return bp->b_error;
	}

	/* send to segment collector */
	nilfs_write_filebuf(node, bp);
	return bp->b_error;
}

/* --------------------------------------------------------------------- */

int
nilfs_readdir(void *v)
{
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		kauth_cred_t a_cred;
		int *a_eofflag;
		off_t **a_cookies;
		int *a_ncookies;
	} */ *ap = v;
	struct uio *uio = ap->a_uio;
	struct vnode *vp = ap->a_vp;
	struct nilfs_node *node = VTOI(vp);
	struct nilfs_dir_entry *ndirent;
	struct dirent dirent;
	struct buf *bp;
	uint64_t file_size, diroffset, transoffset, blkoff;
	uint64_t blocknr;
	uint32_t blocksize = node->nilfsdev->blocksize;
	uint8_t *pos, name_len;
	int error;

	DPRINTF(READDIR, ("nilfs_readdir called\n"));

	if (vp->v_type != VDIR)
		return ENOTDIR;

	file_size = nilfs_rw64(node->inode.i_size);

	/* we are called just as long as we keep on pushing data in */
	error = 0;
	if ((uio->uio_offset < file_size) &&
	    (uio->uio_resid >= sizeof(struct dirent))) {
		diroffset   = uio->uio_offset;
		transoffset = diroffset;

		blocknr = diroffset / blocksize;
		blkoff  = diroffset % blocksize;
		error = nilfs_bread(node, blocknr, 0, &bp);
		if (error)
			return EIO;
		while (diroffset < file_size) {
			DPRINTF(READDIR, ("readdir : offset = %"PRIu64"\n",
				diroffset));
			if (blkoff >= blocksize) {
				blkoff = 0; blocknr++;
				brelse(bp, BC_AGE);
				error = nilfs_bread(node, blocknr, 0, &bp);
				if (error)
					return EIO;
			}

			/* read in one dirent */
			pos = (uint8_t *) bp->b_data + blkoff;
			ndirent = (struct nilfs_dir_entry *) pos;

			name_len = ndirent->name_len;
			memset(&dirent, 0, sizeof(struct dirent));
			dirent.d_fileno = nilfs_rw64(ndirent->inode);
			dirent.d_type   = ndirent->file_type;	/* 1:1 ? */
			dirent.d_namlen = name_len;
			strncpy(dirent.d_name, ndirent->name, name_len);
			dirent.d_reclen = _DIRENT_SIZE(&dirent);
			DPRINTF(READDIR, ("copying `%*.*s`\n", name_len,
				name_len, dirent.d_name));

			/* 
			 * If there isn't enough space in the uio to return a
			 * whole dirent, break off read
			 */
			if (uio->uio_resid < _DIRENT_SIZE(&dirent))
				break;

			/* transfer */
			if (name_len)
				uiomove(&dirent, _DIRENT_SIZE(&dirent), uio);

			/* advance */
			diroffset += nilfs_rw16(ndirent->rec_len);
			blkoff    += nilfs_rw16(ndirent->rec_len);

			/* remember the last entry we transfered */
			transoffset = diroffset;
		}
		brelse(bp, BC_AGE);

		/* pass on last transfered offset */
		uio->uio_offset = transoffset;
	}

	if (ap->a_eofflag)
		*ap->a_eofflag = (uio->uio_offset >= file_size);

	return error;
}

/* --------------------------------------------------------------------- */

int
nilfs_lookup(void *v)
{
	struct vop_lookup_v2_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct vnode **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct mount *mp = dvp->v_mount;
	uint64_t ino;
	const char *name;
	int namelen, nameiop, islastcn, mounted_ro;
	int vnodetp;
	int error, found;

	*vpp = NULL;

	DPRINTF(LOOKUP, ("nilfs_lookup called\n"));

	/* simplify/clarification flags */
	nameiop     = cnp->cn_nameiop;
	islastcn    = cnp->cn_flags & ISLASTCN;
	mounted_ro  = mp->mnt_flag & MNT_RDONLY;

	/* check exec/dirread permissions first */
	error = VOP_ACCESS(dvp, VEXEC, cnp->cn_cred);
	if (error)
		return error;

	DPRINTF(LOOKUP, ("\taccess ok\n"));

	/*
	 * If requesting a modify on the last path element on a read-only
	 * filingsystem, reject lookup; XXX why is this repeated in every FS ?
	 */
	if (islastcn && mounted_ro && (nameiop == DELETE || nameiop == RENAME))
		return EROFS;

	DPRINTF(LOOKUP, ("\tlooking up cnp->cn_nameptr '%s'\n",
	    cnp->cn_nameptr));
	/* look in the namecache */
	if (cache_lookup(dvp, cnp->cn_nameptr, cnp->cn_namelen,
			 cnp->cn_nameiop, cnp->cn_flags, NULL, vpp)) {
		return *vpp == NULLVP ? ENOENT : 0;
	}

	DPRINTF(LOOKUP, ("\tNOT found in cache\n"));

	/*
	 * Obviously, the file is not (anymore) in the namecache, we have to
	 * search for it. There are three basic cases: '.', '..' and others.
	 * 
	 * Following the guidelines of VOP_LOOKUP manpage and tmpfs.
	 */
	error = 0;
	if ((cnp->cn_namelen == 1) && (cnp->cn_nameptr[0] == '.')) {
		DPRINTF(LOOKUP, ("\tlookup '.'\n"));
		/* special case 1 '.' */
		vref(dvp);
		*vpp = dvp;
		/* done */
	} else if (cnp->cn_flags & ISDOTDOT) {
		/* special case 2 '..' */
		DPRINTF(LOOKUP, ("\tlookup '..'\n"));

		/* get our node */
		name    = "..";
		namelen = 2;
		error = nilfs_lookup_name_in_dir(dvp, name, namelen,
				&ino, &found);
		if (error)
			goto out;
		if (!found)
			error = ENOENT;

		if (error == 0) {
			DPRINTF(LOOKUP, ("\tfound '..'\n"));
			/* try to create/reuse the node */
			error = vcache_get(mp, &ino, sizeof(ino), vpp);

			if (!error) {
				DPRINTF(LOOKUP,
					("\tnode retrieved/created OK\n"));
			}
		}
	} else {
		DPRINTF(LOOKUP, ("\tlookup file\n"));
		/* all other files */
		/* lookup filename in the directory returning its inode */
		name    = cnp->cn_nameptr;
		namelen = cnp->cn_namelen;
		error = nilfs_lookup_name_in_dir(dvp, name, namelen,
				&ino, &found);
		if (error)
			goto out;
		if (!found) {
			DPRINTF(LOOKUP, ("\tNOT found\n"));
			/*
			 * UGH, didn't find name. If we're creating or
			 * renaming on the last name this is OK and we ought
			 * to return EJUSTRETURN if its allowed to be created.
			 */
			error = ENOENT;
			if (islastcn &&
				(nameiop == CREATE || nameiop == RENAME))
					error = 0;
			if (!error) {
				error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred);
				if (!error) {
					error = EJUSTRETURN;
				}
			}
			/* done */
		} else {
			/* try to create/reuse the node */
			error = vcache_get(mp, &ino, sizeof(ino), vpp);
			if (!error) {
				/*
				 * If we are not at the last path component
				 * and found a non-directory or non-link entry
				 * (which may itself be pointing to a
				 * directory), raise an error.
				 */
				vnodetp = (*vpp)->v_type;
				if ((vnodetp != VDIR) && (vnodetp != VLNK)) {
					if (!islastcn) {
						vrele(*vpp);
						*vpp = NULL;
						error = ENOTDIR;
					}
				}

			}
		}
	}	

out:
	/*
	 * Store result in the cache if requested. If we are creating a file,
	 * the file might not be found and thus putting it into the namecache
	 * might be seen as negative caching.
	 */
	if (error == 0 && nameiop != CREATE)
		cache_enter(dvp, *vpp, cnp->cn_nameptr, cnp->cn_namelen,
			    cnp->cn_flags);

	DPRINTFIF(LOOKUP, error, ("nilfs_lookup returing error %d\n", error));

	if (error)
		return error;
	return 0;
}

/* --------------------------------------------------------------------- */

static void
nilfs_ctime_to_timespec(struct timespec *ts, uint64_t ctime)
{
	ts->tv_sec  = ctime;
	ts->tv_nsec = 0;
}


int
nilfs_getattr(void *v)
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
		struct lwp   *a_l;
	} */ *ap = v;
	struct vnode       *vp  = ap->a_vp;
	struct vattr       *vap = ap->a_vap;
	struct nilfs_node  *node = VTOI(vp);
	struct nilfs_inode *inode = &node->inode;

	DPRINTF(VFSCALL, ("nilfs_getattr called\n"));

	/* basic info */
	vattr_null(vap);
	vap->va_type      = vp->v_type;
	vap->va_mode      = nilfs_rw16(inode->i_mode) & ALLPERMS;
	vap->va_nlink     = nilfs_rw16(inode->i_links_count);
	vap->va_uid       = nilfs_rw32(inode->i_uid);
	vap->va_gid       = nilfs_rw32(inode->i_gid);
	vap->va_fsid      = vp->v_mount->mnt_stat.f_fsidx.__fsid_val[0];
	vap->va_fileid    = node->ino;
	vap->va_size      = nilfs_rw64(inode->i_size);
	vap->va_blocksize = node->nilfsdev->blocksize;

	/* times */
	nilfs_ctime_to_timespec(&vap->va_atime, nilfs_rw64(inode->i_mtime));
	nilfs_ctime_to_timespec(&vap->va_mtime, nilfs_rw64(inode->i_mtime));
	nilfs_ctime_to_timespec(&vap->va_ctime, nilfs_rw64(inode->i_ctime));
	nilfs_ctime_to_timespec(&vap->va_birthtime, nilfs_rw64(inode->i_ctime));

	vap->va_gen       = nilfs_rw32(inode->i_generation);
	vap->va_flags     = 0;	/* vattr flags */
	vap->va_bytes     = nilfs_rw64(inode->i_blocks) * vap->va_blocksize;
	vap->va_filerev   = vap->va_gen;  /* XXX file revision? same as gen? */
	vap->va_vaflags   = 0;  /* XXX chflags flags */

	return 0;
}

/* --------------------------------------------------------------------- */

#if 0
static int
nilfs_chown(struct vnode *vp, uid_t new_uid, gid_t new_gid,
	  kauth_cred_t cred)
{
	return EINVAL;
}


static int
nilfs_chmod(struct vnode *vp, mode_t mode, kauth_cred_t cred)
{

	return EINVAL;
}


/* exported */
int
nilfs_chsize(struct vnode *vp, u_quad_t newsize, kauth_cred_t cred)
{
	return EINVAL;
}


static int
nilfs_chflags(struct vnode *vp, mode_t mode, kauth_cred_t cred)
{
	return EINVAL;
}


static int
nilfs_chtimes(struct vnode *vp,
	struct timespec *atime, struct timespec *mtime,
	struct timespec *birthtime, int setattrflags,
	kauth_cred_t cred)
{
	return EINVAL;
}
#endif


int
nilfs_setattr(void *v)
{
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
		struct lwp   *a_l;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	vp = vp;
	DPRINTF(VFSCALL, ("nilfs_setattr called\n"));
	return EINVAL;
}

/* --------------------------------------------------------------------- */

/*
 * Return POSIX pathconf information for NILFS file systems.
 */
int
nilfs_pathconf(void *v)
{
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		register_t *a_retval;
	} */ *ap = v;
	uint32_t bits;

	DPRINTF(VFSCALL, ("nilfs_pathconf called\n"));

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = (1<<16)-1;	/* 16 bits */
		return 0;
	case _PC_NAME_MAX:
		*ap->a_retval = NILFS_MAXNAMLEN;
		return 0;
	case _PC_PATH_MAX:
		*ap->a_retval = PATH_MAX;
		return 0;
	case _PC_PIPE_BUF:
		*ap->a_retval = PIPE_BUF;
		return 0;
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return 0;
	case _PC_NO_TRUNC:
		*ap->a_retval = 1;
		return 0;
	case _PC_SYNC_IO:
		*ap->a_retval = 0;     /* synchronised is off for performance */
		return 0;
	case _PC_FILESIZEBITS:
		/* 64 bit file offsets -> 2+floor(2log(2^64-1)) = 2 + 63 = 65 */
		bits = 64; /* XXX ought to deliver 65 */
#if 0
		if (nilfs_node)
			bits = 64 * vp->v_mount->mnt_dev_bshift;
#endif
		*ap->a_retval = bits;
		return 0;
	}

	return EINVAL;
}


/* --------------------------------------------------------------------- */

int
nilfs_open(void *v)
{
	struct vop_open_args /* {
		struct vnode *a_vp;
		int a_mode;
		kauth_cred_t a_cred;
		struct proc *a_p;
	} */ *ap = v;
	int flags;

	DPRINTF(VFSCALL, ("nilfs_open called\n"));

	/*
	 * Files marked append-only must be opened for appending.
	 */
	flags = 0;
	if ((flags & APPEND) && (ap->a_mode & (FWRITE | O_APPEND)) == FWRITE)
		return (EPERM);

	return 0;
}


/* --------------------------------------------------------------------- */

int
nilfs_close(void *v)
{
	struct vop_close_args /* {
		struct vnode *a_vp;
		int a_fflag;
		kauth_cred_t a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nilfs_node *nilfs_node = VTOI(vp);

	DPRINTF(VFSCALL, ("nilfs_close called\n"));
	nilfs_node = nilfs_node;	/* shut up gcc */

	mutex_enter(vp->v_interlock);
		if (vp->v_usecount > 1)
			nilfs_itimes(nilfs_node, NULL, NULL, NULL);
	mutex_exit(vp->v_interlock);

	return 0;
}


/* --------------------------------------------------------------------- */

static int
nilfs_check_possible(struct vnode *vp, struct vattr *vap, mode_t mode)
{
	int flags;

	/* check if we are allowed to write */
	switch (vap->va_type) {
	case VDIR:
	case VLNK:
	case VREG:
		/*
		 * normal nodes: check if we're on a read-only mounted
		 * filingsystem and bomb out if we're trying to write.
		 */
		if ((mode & VWRITE) && (vp->v_mount->mnt_flag & MNT_RDONLY))
			return EROFS;
		break;
	case VBLK:
	case VCHR:
	case VSOCK:
	case VFIFO:
		/*
		 * special nodes: even on read-only mounted filingsystems
		 * these are allowed to be written to if permissions allow.
		 */
		break;
	default:
		/* no idea what this is */
		return EINVAL;
	}

	/* noone may write immutable files */
	/* TODO: get chflags(2) flags */
	flags = 0;
	if ((mode & VWRITE) && (flags & IMMUTABLE))
		return EPERM;

	return 0;
}

static int
nilfs_check_permitted(struct vnode *vp, struct vattr *vap, mode_t mode,
    kauth_cred_t cred)
{

	/* ask the generic genfs_can_access to advice on security */
	return kauth_authorize_vnode(cred, KAUTH_ACCESS_ACTION(mode,
	    vp->v_type, vap->va_mode), vp, NULL, genfs_can_access(vp->v_type,
	    vap->va_mode, vap->va_uid, vap->va_gid, mode, cred));
}

int
nilfs_access(void *v)
{
	struct vop_access_args /* {
		struct vnode *a_vp;
		int a_mode;
		kauth_cred_t a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode    *vp   = ap->a_vp;
	mode_t	         mode = ap->a_mode;
	kauth_cred_t     cred = ap->a_cred;
	/* struct nilfs_node *nilfs_node = VTOI(vp); */
	struct vattr vap;
	int error;

	DPRINTF(VFSCALL, ("nilfs_access called\n"));

	error = VOP_GETATTR(vp, &vap, NULL);
	if (error)
		return error;

	error = nilfs_check_possible(vp, &vap, mode);
	if (error)
		return error;

	error = nilfs_check_permitted(vp, &vap, mode, cred);

	return error;
}

/* --------------------------------------------------------------------- */

int
nilfs_create(void *v)
{
	struct vop_create_v3_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	struct vnode  *dvp = ap->a_dvp;
	struct vnode **vpp = ap->a_vpp;
	struct vattr  *vap  = ap->a_vap;
	struct componentname *cnp = ap->a_cnp;
	int error;

	DPRINTF(VFSCALL, ("nilfs_create called\n"));
	error = nilfs_create_node(dvp, vpp, vap, cnp);

	return error;
}

/* --------------------------------------------------------------------- */

int
nilfs_mknod(void *v)
{
	struct vop_mknod_v3_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	struct vnode  *dvp = ap->a_dvp;
	struct vnode **vpp = ap->a_vpp;
	struct vattr  *vap  = ap->a_vap;
	struct componentname *cnp = ap->a_cnp;
	int error;

	DPRINTF(VFSCALL, ("nilfs_mknod called\n"));
	error = nilfs_create_node(dvp, vpp, vap, cnp);

	return error;
}

/* --------------------------------------------------------------------- */

int
nilfs_mkdir(void *v)
{
	struct vop_mkdir_v3_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	struct vnode  *dvp = ap->a_dvp;
	struct vnode **vpp = ap->a_vpp;
	struct vattr  *vap  = ap->a_vap;
	struct componentname *cnp = ap->a_cnp;
	int error;

	DPRINTF(VFSCALL, ("nilfs_mkdir called\n"));
	error = nilfs_create_node(dvp, vpp, vap, cnp);

	return error;
}

/* --------------------------------------------------------------------- */

static int
nilfs_do_link(struct vnode *dvp, struct vnode *vp, struct componentname *cnp)
{
	struct nilfs_node *nilfs_node, *dir_node;
	struct vattr vap;
	int error;

	DPRINTF(VFSCALL, ("nilfs_link called\n"));
	KASSERT(dvp != vp);
	KASSERT(vp->v_type != VDIR);
	KASSERT(dvp->v_mount == vp->v_mount);

	/* lock node */
	error = vn_lock(vp, LK_EXCLUSIVE);
	if (error)
		return error;

	/* get attributes */
	dir_node = VTOI(dvp);
	nilfs_node = VTOI(vp);

	error = VOP_GETATTR(vp, &vap, FSCRED);
	if (error) {
		VOP_UNLOCK(vp);
		return error;
	}

	/* check link count overflow */
	if (vap.va_nlink >= (1<<16)-1) {	/* uint16_t */
		VOP_UNLOCK(vp);
		return EMLINK;
	}

	error = nilfs_dir_attach(dir_node->ump, dir_node, nilfs_node,
	    &vap, cnp);
	if (error)
		VOP_UNLOCK(vp);
	return error;
}

int
nilfs_link(void *v)
{
	struct vop_link_v2_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct vnode *vp  = ap->a_vp;
	struct componentname *cnp = ap->a_cnp;
	int error;

	error = nilfs_do_link(dvp, vp, cnp);
	if (error)
		VOP_ABORTOP(dvp, cnp);

	VN_KNOTE(vp, NOTE_LINK);
	VN_KNOTE(dvp, NOTE_WRITE);

	return error;
}

/* --------------------------------------------------------------------- */

static int
nilfs_do_symlink(struct nilfs_node *nilfs_node, char *target)
{
	return EROFS;
}


int
nilfs_symlink(void *v)
{
	struct vop_symlink_v3_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap = v;
	struct vnode  *dvp = ap->a_dvp;
	struct vnode **vpp = ap->a_vpp;
	struct vattr  *vap  = ap->a_vap;
	struct componentname *cnp = ap->a_cnp;
	struct nilfs_node *dir_node;
	struct nilfs_node *nilfs_node;
	int error;

	DPRINTF(VFSCALL, ("nilfs_symlink called\n"));
	DPRINTF(VFSCALL, ("\tlinking to `%s`\n",  ap->a_target));
	error = nilfs_create_node(dvp, vpp, vap, cnp);
	KASSERT(((error == 0) && (*vpp != NULL)) || ((error && (*vpp == NULL))));
	if (!error) {
		dir_node = VTOI(dvp);
		nilfs_node = VTOI(*vpp);
		KASSERT(nilfs_node);
		error = nilfs_do_symlink(nilfs_node, ap->a_target);
		if (error) {
			/* remove node */
			nilfs_shrink_node(nilfs_node, 0);
			nilfs_dir_detach(nilfs_node->ump, dir_node, nilfs_node, cnp);
		}
	}
	return error;
}

/* --------------------------------------------------------------------- */

int
nilfs_readlink(void *v)
{
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		kauth_cred_t a_cred;
	} */ *ap = v;
#if 0
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	kauth_cred_t cred = ap->a_cred;
	struct nilfs_node *nilfs_node;
	struct pathcomp pathcomp;
	struct vattr vattr;
	uint8_t *pathbuf, *targetbuf, *tmpname;
	uint8_t *pathpos, *targetpos;
	char *mntonname;
	int pathlen, targetlen, namelen, mntonnamelen, len, l_ci;
	int first, error;
#endif
	ap = ap;

	DPRINTF(VFSCALL, ("nilfs_readlink called\n"));

	return EROFS;
}

/* --------------------------------------------------------------------- */

/* note: i tried to follow the logics of the tmpfs rename code */
int
nilfs_rename(void *v)
{
	struct vop_rename_args /* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap = v;
	struct vnode *tvp = ap->a_tvp;
	struct vnode *tdvp = ap->a_tdvp;
	struct vnode *fvp = ap->a_fvp;
	struct vnode *fdvp = ap->a_fdvp;
	struct componentname *tcnp = ap->a_tcnp;
	struct componentname *fcnp = ap->a_fcnp;
	struct nilfs_node *fnode, *fdnode, *tnode, *tdnode;
	struct vattr fvap, tvap;
	int error;

	DPRINTF(VFSCALL, ("nilfs_rename called\n"));

	/* disallow cross-device renames */
	if (fvp->v_mount != tdvp->v_mount ||
	    (tvp != NULL && fvp->v_mount != tvp->v_mount)) {
		error = EXDEV;
		goto out_unlocked;
	}

	fnode  = VTOI(fvp);
	fdnode = VTOI(fdvp);
	tnode  = (tvp == NULL) ? NULL : VTOI(tvp);
	tdnode = VTOI(tdvp);

	/* lock our source dir */
	if (fdnode != tdnode) {
		error = vn_lock(fdvp, LK_EXCLUSIVE | LK_RETRY);
		if (error != 0)
			goto out_unlocked;
	}

	/* get info about the node to be moved */
	vn_lock(fvp, LK_SHARED | LK_RETRY);
	error = VOP_GETATTR(fvp, &fvap, FSCRED);
	VOP_UNLOCK(fvp);
	KASSERT(error == 0);

	/* check when to delete the old already existing entry */
	if (tvp) {
		/* get info about the node to be moved to */
		error = VOP_GETATTR(tvp, &tvap, FSCRED);
		KASSERT(error == 0);

		/* if both dirs, make sure the destination is empty */
		if (fvp->v_type == VDIR && tvp->v_type == VDIR) {
			if (tvap.va_nlink > 2) {
				error = ENOTEMPTY;
				goto out;
			}
		}
		/* if moving dir, make sure destination is dir too */
		if (fvp->v_type == VDIR && tvp->v_type != VDIR) {
			error = ENOTDIR;
			goto out;
		}
		/* if we're moving a non-directory, make sure dest is no dir */
		if (fvp->v_type != VDIR && tvp->v_type == VDIR) {
			error = EISDIR;
			goto out;
		}
	}

	/* dont allow renaming directories acros directory for now */
	if (fdnode != tdnode) {
		if (fvp->v_type == VDIR) {
			error = EINVAL;
			goto out;
		}
	}

	/* remove existing entry if present */
	if (tvp) 
		nilfs_dir_detach(tdnode->ump, tdnode, tnode, tcnp);

	/* create new directory entry for the node */
	error = nilfs_dir_attach(tdnode->ump, tdnode, fnode, &fvap, tcnp);
	if (error)
		goto out;

	/* unlink old directory entry for the node, if failing, unattach new */
	error = nilfs_dir_detach(tdnode->ump, fdnode, fnode, fcnp);
	if (error)
		nilfs_dir_detach(tdnode->ump, tdnode, fnode, tcnp);

out:
        if (fdnode != tdnode)
                VOP_UNLOCK(fdvp);

out_unlocked:
	VOP_ABORTOP(tdvp, tcnp);
	if (tdvp == tvp)
		vrele(tdvp);
	else
		vput(tdvp);
	if (tvp)
		vput(tvp);
	VOP_ABORTOP(fdvp, fcnp);

	/* release source nodes. */
	vrele(fdvp);
	vrele(fvp);

	return error;
}

/* --------------------------------------------------------------------- */

int
nilfs_remove(void *v)
{
	struct vop_remove_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct vnode *vp  = ap->a_vp;
	struct componentname *cnp = ap->a_cnp;
	struct nilfs_node *dir_node = VTOI(dvp);
	struct nilfs_node *nilfs_node = VTOI(vp);
	struct nilfs_mount *ump = dir_node->ump;
	int error;

	DPRINTF(VFSCALL, ("nilfs_remove called\n"));
	if (vp->v_type != VDIR) {
		error = nilfs_dir_detach(ump, dir_node, nilfs_node, cnp);
		DPRINTFIF(NODE, error, ("\tgot error removing file\n"));
	} else {
		DPRINTF(NODE, ("\tis a directory: perm. denied\n"));
		error = EPERM;
	}

	if (error == 0) {
		VN_KNOTE(vp, NOTE_DELETE);
		VN_KNOTE(dvp, NOTE_WRITE);
	}

	if (dvp == vp)
		vrele(vp);
	else
		vput(vp);
	vput(dvp);

	return error;
}

/* --------------------------------------------------------------------- */

int
nilfs_rmdir(void *v)
{
	struct vop_rmdir_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct nilfs_node *dir_node = VTOI(dvp);
	struct nilfs_node *nilfs_node = VTOI(vp);
	struct nilfs_mount *ump = dir_node->ump;
	int refcnt, error;

	DPRINTF(NOTIMPL, ("nilfs_rmdir called\n"));

	/* don't allow '.' to be deleted */
	if (dir_node == nilfs_node) {
		vrele(dvp);
		vput(vp);
		return EINVAL;
	}

	/* check to see if the directory is empty */
	error = 0;
	refcnt = 2; /* XXX */
	if (refcnt > 1) {
		/* NOT empty */
		vput(dvp);
		vput(vp);
		return ENOTEMPTY;
	}

	/* detach the node from the directory */
	error = nilfs_dir_detach(ump, dir_node, nilfs_node, cnp);
	if (error == 0) {
		cache_purge(vp);
//		cache_purge(dvp);	/* XXX from msdosfs, why? */
		VN_KNOTE(vp, NOTE_DELETE);
	}
	DPRINTFIF(NODE, error, ("\tgot error removing file\n"));

	/* unput the nodes and exit */
	vput(dvp);
	vput(vp);

	return error;
}

/* --------------------------------------------------------------------- */

int
nilfs_fsync(void *v)
{
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		kauth_cred_t a_cred;
		int a_flags;
		off_t offlo;
		off_t offhi;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
//	struct nilfs_node *nilfs_node = VTOI(vp);
//	int error, flags, wait;

	DPRINTF(STRATEGY, ("nilfs_fsync called : %s, %s\n",
		(ap->a_flags & FSYNC_WAIT)     ? "wait":"no wait",
		(ap->a_flags & FSYNC_DATAONLY) ? "data_only":"complete"));

	vp = vp;
	return 0;
}

/* --------------------------------------------------------------------- */

int
nilfs_advlock(void *v)
{
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		void *a_id;
		int a_op;
		struct flock *a_fl;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nilfs_node *nilfs_node = VTOI(vp);
	uint64_t file_size;

	DPRINTF(LOCKING, ("nilfs_advlock called\n"));

	assert(nilfs_node);
	file_size = nilfs_rw64(nilfs_node->inode.i_size);

	return lf_advlock(ap, &nilfs_node->lockf, file_size);
}

/* --------------------------------------------------------------------- */


/* Global vfs vnode data structures for nilfss */
int (**nilfs_vnodeop_p) __P((void *));

const struct vnodeopv_entry_desc nilfs_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, nilfs_lookup },	/* lookup */
	{ &vop_create_desc, nilfs_create },	/* create */
	{ &vop_mknod_desc, nilfs_mknod },	/* mknod */	/* TODO */
	{ &vop_open_desc, nilfs_open },		/* open */
	{ &vop_close_desc, nilfs_close },	/* close */
	{ &vop_access_desc, nilfs_access },	/* access */
	{ &vop_getattr_desc, nilfs_getattr },	/* getattr */
	{ &vop_setattr_desc, nilfs_setattr },	/* setattr */	/* TODO chflags */
	{ &vop_read_desc, nilfs_read },		/* read */
	{ &vop_write_desc, nilfs_write },	/* write */	/* WRITE */
	{ &vop_fallocate_desc, genfs_eopnotsupp }, /* fallocate */
	{ &vop_fdiscard_desc, genfs_eopnotsupp }, /* fdiscard */
	{ &vop_fcntl_desc, genfs_fcntl },	/* fcntl */	/* TODO? */
	{ &vop_ioctl_desc, genfs_enoioctl },	/* ioctl */	/* TODO? */
	{ &vop_poll_desc, genfs_poll },		/* poll */	/* TODO/OK? */
	{ &vop_kqfilter_desc, genfs_kqfilter },	/* kqfilter */	/* ? */
	{ &vop_revoke_desc, genfs_revoke },	/* revoke */	/* TODO? */
	{ &vop_mmap_desc, genfs_mmap },		/* mmap */	/* OK? */
	{ &vop_fsync_desc, nilfs_fsync },	/* fsync */
	{ &vop_seek_desc, genfs_seek },		/* seek */
	{ &vop_remove_desc, nilfs_remove },	/* remove */
	{ &vop_link_desc, nilfs_link },		/* link */	/* TODO */
	{ &vop_rename_desc, nilfs_rename },	/* rename */ 	/* TODO */
	{ &vop_mkdir_desc, nilfs_mkdir },	/* mkdir */ 
	{ &vop_rmdir_desc, nilfs_rmdir },	/* rmdir */
	{ &vop_symlink_desc, nilfs_symlink },	/* symlink */	/* TODO */
	{ &vop_readdir_desc, nilfs_readdir },	/* readdir */
	{ &vop_readlink_desc, nilfs_readlink },	/* readlink */	/* TEST ME */
	{ &vop_abortop_desc, genfs_abortop },	/* abortop */	/* TODO/OK? */
	{ &vop_inactive_desc, nilfs_inactive },	/* inactive */
	{ &vop_reclaim_desc, nilfs_reclaim },	/* reclaim */
	{ &vop_lock_desc, genfs_lock },		/* lock */
	{ &vop_unlock_desc, genfs_unlock },	/* unlock */
	{ &vop_bmap_desc, nilfs_trivial_bmap },	/* bmap */	/* 1:1 bmap */
	{ &vop_strategy_desc, nilfs_vfsstrategy },/* strategy */
/*	{ &vop_print_desc, nilfs_print },	*/	/* print */
	{ &vop_islocked_desc, genfs_islocked },	/* islocked */
	{ &vop_pathconf_desc, nilfs_pathconf },	/* pathconf */
	{ &vop_advlock_desc, nilfs_advlock },	/* advlock */	/* TEST ME */
	{ &vop_bwrite_desc, vn_bwrite },	/* bwrite */	/* ->strategy */
	{ &vop_getpages_desc, genfs_getpages },	/* getpages */
	{ &vop_putpages_desc, genfs_putpages },	/* putpages */
	{ NULL, NULL }
};


const struct vnodeopv_desc nilfs_vnodeop_opv_desc = {
	&nilfs_vnodeop_p, nilfs_vnodeop_entries
};


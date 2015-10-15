/* $NetBSD: udf_vnops.c,v 1.101 2015/04/20 23:03:08 riastradh Exp $ */

/*
 * Copyright (c) 2006, 2008 Reinoud Zandijk
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
 * Generic parts are derived from software contributed to The NetBSD Foundation
 * by Julio M. Merino Vidal, developed as part of Google's Summer of Code
 * 2005 program.
 *
 */

#include <sys/cdefs.h>
#ifndef lint
__KERNEL_RCSID(0, "$NetBSD: udf_vnops.c,v 1.101 2015/04/20 23:03:08 riastradh Exp $");
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

#include <fs/udf/ecma167-udf.h>
#include <fs/udf/udf_mount.h>
#include <sys/dirhash.h>

#include "udf.h"
#include "udf_subr.h"
#include "udf_bswap.h"


#define VTOI(vnode) ((struct udf_node *) (vnode)->v_data)

/* forward declarations */
static int udf_do_readlink(struct udf_node *udf_node, uint64_t filesize,
	uint8_t *targetbuf, int *length);

/* externs */
extern int prtactive;

/* implementations of vnode functions; table follows at end */
/* --------------------------------------------------------------------- */

int
udf_inactive(void *v)
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		bool         *a_recycle;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct udf_node *udf_node = VTOI(vp);
	int refcnt;

	DPRINTF(NODE, ("udf_inactive called for udf_node %p\n", VTOI(vp)));

	if (udf_node == NULL) {
		DPRINTF(NODE, ("udf_inactive: inactive NULL UDF node\n"));
		VOP_UNLOCK(vp);
		return 0;
	}

	/*
	 * Optionally flush metadata to disc.
	 */
	if (udf_node->fe) {
		refcnt = udf_rw16(udf_node->fe->link_cnt);
	} else {
		assert(udf_node->efe);
		refcnt = udf_rw16(udf_node->efe->link_cnt);
	}

	if ((refcnt == 0) && (vp->v_vflag & VV_SYSTEM)) {
		DPRINTF(VOLUMES, ("UDF_INACTIVE deleting VV_SYSTEM\n"));
		/* system nodes are not writen out on inactive, so flush */
		udf_node->i_flags = 0;
	}

	*ap->a_recycle = false;
	if ((refcnt == 0) && ((vp->v_vflag & VV_SYSTEM) == 0)) {
		*ap->a_recycle = true;
		VOP_UNLOCK(vp);
		return 0;
	}

	/* write out its node */
	if (udf_node->i_flags & (IN_CHANGE | IN_UPDATE | IN_MODIFIED))
		udf_update(vp, NULL, NULL, NULL, 0);
	VOP_UNLOCK(vp);

	return 0;
}

/* --------------------------------------------------------------------- */

int
udf_reclaim(void *v)
{
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct udf_node *udf_node = VTOI(vp);
	int refcnt;

	DPRINTF(NODE, ("udf_reclaim called for node %p\n", udf_node));
	if (prtactive && vp->v_usecount > 1)
		vprint("udf_reclaim(): pushing active", vp);

	if (udf_node == NULL) {
		DPRINTF(NODE, ("udf_reclaim(): null udfnode\n"));
		return 0;
	}

	/*
	 * If the file has not been referenced anymore in a directory
	 * we ought to free up the resources on disc if applicable.
	 */
	if (udf_node->fe) {
		refcnt = udf_rw16(udf_node->fe->link_cnt);
	} else {
		assert(udf_node->efe);
		refcnt = udf_rw16(udf_node->efe->link_cnt);
	}

	if ((refcnt == 0) && ((vp->v_vflag & VV_SYSTEM) == 0)) {
	 	/* remove this file's allocation */
		DPRINTF(NODE, ("udf_inactive deleting unlinked file\n"));
		udf_delete_node(udf_node);
	}

	/* update note for closure */
	udf_update(vp, NULL, NULL, NULL, UPDATE_CLOSE);

	/* async check to see if all node descriptors are written out */
	while ((volatile int) udf_node->outstanding_nodedscr > 0) {
		vprint("udf_reclaim(): waiting for writeout\n", vp);
		tsleep(&udf_node->outstanding_nodedscr, PRIBIO, "recl wait", hz/8);
	}

	vcache_remove(vp->v_mount, &udf_node->loc.loc, 
	    sizeof(udf_node->loc.loc));

	/* dispose all node knowledge */
	udf_dispose_node(udf_node);

	return 0;
}

/* --------------------------------------------------------------------- */

int
udf_read(void *v)
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
	struct udf_node      *udf_node = VTOI(vp);
	struct file_entry    *fe;
	struct extfile_entry *efe;
	uint64_t file_size;
	vsize_t len;
	int error;

	/*
	 * XXX reading from extended attributes not yet implemented. FreeBSD
	 * has it in mind to forward the IO_EXT read call to the
	 * VOP_READEXTATTR().
	 */

	DPRINTF(READ, ("udf_read called\n"));

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

	assert(udf_node);
	assert(udf_node->fe || udf_node->efe);

	/* get file/directory filesize */
	if (udf_node->fe) {
		fe = udf_node->fe;
		file_size = udf_rw64(fe->inf_len);
	} else {
		assert(udf_node->efe);
		efe = udf_node->efe;
		file_size = udf_rw64(efe->inf_len);
	}

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
		udf_node->i_flags |= IN_ACCESS;
		if ((ioflag & IO_SYNC) == IO_SYNC) {
			int uerror;

			uerror = udf_update(vp, NULL, NULL, NULL, UPDATE_WAIT);
			if (error == 0)
				error = uerror;
		}
	}

	return error;
}

/* --------------------------------------------------------------------- */

int
udf_write(void *v)
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
	kauth_cred_t  cred   = ap->a_cred;
	int           advice = IO_ADV_DECODE(ap->a_ioflag);
	struct uvm_object    *uobj;
	struct udf_node      *udf_node = VTOI(vp);
	struct file_entry    *fe;
	struct extfile_entry *efe;
	uint64_t file_size, old_size, old_offset;
	vsize_t len;
	int aflag = ioflag & IO_SYNC ? B_SYNC : 0;
	int error;
	int resid, extended;

	/*
	 * XXX writing to extended attributes not yet implemented. FreeBSD has
	 * it in mind to forward the IO_EXT read call to the
	 * VOP_READEXTATTR().
	 */

	DPRINTF(WRITE, ("udf_write called\n"));

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

	assert(udf_node);
	assert(udf_node->fe || udf_node->efe);

	/* get file/directory filesize */
	if (udf_node->fe) {
		fe = udf_node->fe;
		file_size = udf_rw64(fe->inf_len);
	} else {
		assert(udf_node->efe);
		efe = udf_node->efe;
		file_size = udf_rw64(efe->inf_len);
	}
	old_size = file_size;

	/* if explicitly asked to append, uio_offset can be wrong? */
	if (ioflag & IO_APPEND)
		uio->uio_offset = file_size;

	extended = (uio->uio_offset + uio->uio_resid > file_size);
	if (extended) {
		DPRINTF(WRITE, ("extending file from %"PRIu64" to %"PRIu64"\n",
			file_size, uio->uio_offset + uio->uio_resid));
		error = udf_grow_node(udf_node, uio->uio_offset + uio->uio_resid);
		if (error)
			return error;
		file_size = uio->uio_offset + uio->uio_resid;
	}

	/* write contents using buffercache */
	uobj = &vp->v_uobj;
	resid = uio->uio_resid;
	error = 0;

	uvm_vnp_setwritesize(vp, file_size);
	old_offset = uio->uio_offset;
	while (uio->uio_resid > 0) {
		/* maximise length to file extremity */
		len = MIN(file_size - uio->uio_offset, uio->uio_resid);
		if (len == 0)
			break;

		genfs_node_wrlock(vp);
		error = GOP_ALLOC(vp, uio->uio_offset, len, aflag, cred);
		genfs_node_unlock(vp);
		if (error)
			break;

		/* ubc, here we come, prepare to trap */
		error = ubc_uiomove(uobj, uio, len, advice,
		    UBC_WRITE | UBC_UNMAP_FLAG(vp));
		if (error)
			break;

		/*
		 * flush what we just wrote if necessary.
		 * XXXUBC simplistic async flushing.
		 *
		 * Directories are excluded since its file data that we want
		 * to purge.
		 */
		if ((vp->v_type != VDIR) &&
		  (old_offset >> 16 != uio->uio_offset >> 16)) {
			mutex_enter(vp->v_interlock);
			error = VOP_PUTPAGES(vp, (old_offset >> 16) << 16,
			    (uio->uio_offset >> 16) << 16,
			    PGO_CLEANIT | PGO_LAZY);
			old_offset = uio->uio_offset;
		}
	}
	uvm_vnp_setsize(vp, file_size);

	/* mark node changed and request update */
	udf_node->i_flags |= IN_CHANGE | IN_UPDATE;
	if (vp->v_mount->mnt_flag & MNT_RELATIME)
		udf_node->i_flags |= IN_ACCESS;

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
		(void) udf_chsize(vp, (u_quad_t) old_size, cred);

		/* roll back uio */
		uio->uio_offset -= resid - uio->uio_resid;
		uio->uio_resid = resid;
	} else {
		/* if we write and we're synchronous, update node */
		if ((resid > uio->uio_resid) && ((ioflag & IO_SYNC) == IO_SYNC))
			error = udf_update(vp, NULL, NULL, NULL, UPDATE_WAIT);
	}

	return error;
}


/* --------------------------------------------------------------------- */

/*
 * `Special' bmap functionality that translates all incomming requests to
 * translate to vop_strategy() calls with the same blocknumbers effectively
 * not translating at all.
 */

int
udf_trivial_bmap(void *v)
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
	struct udf_node *udf_node = VTOI(vp);
	uint32_t lb_size;

	/* get logical block size */
	lb_size = udf_rw32(udf_node->ump->logical_vol->lb_size);

	/* could return `-1' to indicate holes/zeros */
	/* translate 1:1 */
	*bnp = bn;

	/* set the vnode to read the data from with strategy on itself */
	if (vpp)
		*vpp = vp;

	/* set runlength of maximum block size */
	if (runp)
		*runp = MAXPHYS / lb_size;	/* or with -1 ? */

	/* return success */
	return 0;
}

/* --------------------------------------------------------------------- */

int
udf_vfsstrategy(void *v)
{
	struct vop_strategy_args /* {
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct buf   *bp = ap->a_bp;
	struct udf_node *udf_node = VTOI(vp);
	uint32_t lb_size, sectors;

	DPRINTF(STRATEGY, ("udf_strategy called\n"));

	/* check if we ought to be here */
	if (vp->v_type == VBLK || vp->v_type == VCHR)
		panic("udf_strategy: spec");

	/* only filebuffers ought to be read/write by this, no descriptors */
	assert(bp->b_blkno >= 0);

	/* get sector size */
	lb_size = udf_rw32(udf_node->ump->logical_vol->lb_size);

	/* calculate length to fetch/store in sectors */
	sectors = bp->b_bcount / lb_size;
	assert(bp->b_bcount > 0);

	/* NEVER assume later that this buffer is already translated */
	/* bp->b_lblkno = bp->b_blkno; */

	/* check assertions: we OUGHT to always get multiples of this */
	assert(sectors * lb_size == bp->b_bcount);
	__USE(sectors);

	/* issue buffer */
	if (bp->b_flags & B_READ) {
		DPRINTF(STRATEGY, ("\tread vp %p buf %p (blk no %"PRIu64")"
		    ", for %d sectors\n",
		    vp, bp, bp->b_blkno, sectors));

		/* read buffer from the udf_node, translate vtop on the way*/
		udf_read_filebuf(udf_node, bp);
	} else {
		DPRINTF(STRATEGY, ("\twrite vp %p buf %p (blk no %"PRIu64")"
		    ", for %d sectors\n",
		    vp, bp, bp->b_blkno, sectors));

		/* write buffer to the udf_node, translate vtop on the way*/
		udf_write_filebuf(udf_node, bp);
	}

	return bp->b_error;
}

/* --------------------------------------------------------------------- */

int
udf_readdir(void *v)
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
	struct udf_node *udf_node = VTOI(vp);
	struct file_entry    *fe;
	struct extfile_entry *efe;
	struct fileid_desc *fid;
	struct dirent *dirent;
	uint64_t file_size, diroffset, transoffset;
	uint32_t lb_size;
	int error;

	DPRINTF(READDIR, ("udf_readdir called\n"));

	/* This operation only makes sense on directory nodes. */
	if (vp->v_type != VDIR)
		return ENOTDIR;

	/* get directory filesize */
	if (udf_node->fe) {
		fe = udf_node->fe;
		file_size = udf_rw64(fe->inf_len);
	} else {
		assert(udf_node->efe);
		efe = udf_node->efe;
		file_size = udf_rw64(efe->inf_len);
	}

	dirent = malloc(sizeof(struct dirent), M_UDFTEMP, M_WAITOK | M_ZERO);

	/*
	 * Add `.' pseudo entry if at offset zero since its not in the fid
	 * stream
	 */
	if (uio->uio_offset == 0) {
		DPRINTF(READDIR, ("\t'.' inserted\n"));
		strcpy(dirent->d_name, ".");
		dirent->d_fileno = udf_get_node_id(&udf_node->loc);
		dirent->d_type = DT_DIR;
		dirent->d_namlen = strlen(dirent->d_name);
		dirent->d_reclen = _DIRENT_SIZE(dirent);
		uiomove(dirent, _DIRENT_SIZE(dirent), uio);

		/* mark with magic value that we have done the dummy */
		uio->uio_offset = UDF_DIRCOOKIE_DOT;
	}

	/* we are called just as long as we keep on pushing data in */
	error = 0;
	if (uio->uio_offset < file_size) {
		/* allocate temporary space for fid */
		lb_size = udf_rw32(udf_node->ump->logical_vol->lb_size);
		fid = malloc(lb_size, M_UDFTEMP, M_WAITOK);

		if (uio->uio_offset == UDF_DIRCOOKIE_DOT)
			uio->uio_offset = 0;

		diroffset   = uio->uio_offset;
		transoffset = diroffset;
		while (diroffset < file_size) {
			DPRINTF(READDIR, ("\tread in fid stream\n"));
			/* transfer a new fid/dirent */
			error = udf_read_fid_stream(vp, &diroffset, fid, dirent);
			DPRINTFIF(READDIR, error, ("read error in read fid "
			    "stream : %d\n", error));
			if (error)
				break;

			/* 
			 * If there isn't enough space in the uio to return a
			 * whole dirent, break off read
			 */
			if (uio->uio_resid < _DIRENT_SIZE(dirent))
				break;

			/* remember the last entry we transfered */
			transoffset = diroffset;

			/* skip deleted entries */
			if (fid->file_char & UDF_FILE_CHAR_DEL)
				continue;

			/* skip not visible files */
			if (fid->file_char & UDF_FILE_CHAR_VIS)
				continue;

			/* copy dirent to the caller */
			DPRINTF(READDIR, ("\tread dirent `%s', type %d\n",
			    dirent->d_name, dirent->d_type));
			uiomove(dirent, _DIRENT_SIZE(dirent), uio);
		}

		/* pass on last transfered offset */
		uio->uio_offset = transoffset;
		free(fid, M_UDFTEMP);
	}

	if (ap->a_eofflag)
		*ap->a_eofflag = (uio->uio_offset >= file_size);

#ifdef DEBUG
	if (udf_verbose & UDF_DEBUG_READDIR) {
		printf("returning offset %d\n", (uint32_t) uio->uio_offset);
		if (ap->a_eofflag)
			printf("returning EOF ? %d\n", *ap->a_eofflag);
		if (error)
			printf("readdir returning error %d\n", error);
	}
#endif

	free(dirent, M_UDFTEMP);
	return error;
}

/* --------------------------------------------------------------------- */

int
udf_lookup(void *v)
{
	struct vop_lookup_v2_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct vnode **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct udf_node  *dir_node, *res_node;
	struct udf_mount *ump;
	struct long_ad    icb_loc;
	mode_t mode;
	uid_t d_uid;
	gid_t d_gid;
	const char *name;
	int namelen, nameiop, islastcn, mounted_ro;
	int error, found;

	dir_node = VTOI(dvp);
	ump = dir_node->ump;
	*vpp = NULL;

	DPRINTF(LOOKUP, ("udf_lookup called, lookup `%s`\n",
		cnp->cn_nameptr));

	/* simplify/clarification flags */
	nameiop     = cnp->cn_nameiop;
	islastcn    = cnp->cn_flags & ISLASTCN;
	mounted_ro  = dvp->v_mount->mnt_flag & MNT_RDONLY;

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
		if (islastcn && cnp->cn_nameiop == RENAME) {
			error = EISDIR;
			goto out;
		}
		vref(dvp);
		*vpp = dvp;
		/* done */
		goto done;
	} else if (cnp->cn_flags & ISDOTDOT) {
		/* special case 2 '..' */
		DPRINTF(LOOKUP, ("\tlookup '..'\n"));

		if (islastcn && cnp->cn_nameiop == RENAME) {
			error = EINVAL;
			goto out;
		}

		/* get our node */
		name    = "..";
		namelen = 2;
		error = udf_lookup_name_in_dir(dvp, name, namelen,
				&icb_loc, &found);
		if (error)
			goto out;
		if (!found)
			error = ENOENT;

		/* first unlock parent */
		VOP_UNLOCK(dvp);

		if (error == 0) {
			DPRINTF(LOOKUP, ("\tfound '..'\n"));
			/* try to create/reuse the node */
			error = udf_get_node(ump, &icb_loc, &res_node);

			if (!error) {
				DPRINTF(LOOKUP,
					("\tnode retrieved/created OK\n"));
				*vpp = res_node->vnode;
			}
		}

		/* try to relock parent */
		vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY);
		goto out;
	}

	/* all other files */
	DPRINTF(LOOKUP, ("\tlookup file/dir in directory\n"));

	/* lookup filename in the directory; location icb_loc */
	name    = cnp->cn_nameptr;
	namelen = cnp->cn_namelen;
	error = udf_lookup_name_in_dir(dvp, name, namelen,
			&icb_loc, &found);
	if (error)
		goto out;
	if (!found) {
		DPRINTF(LOOKUP, ("\tNOT found\n"));
		/*
		 * The entry was not found in the directory.  This is
		 * valid if we are creating or renaming an entry and
		 * are working on the last component of the path name.
		 */
		if (islastcn && (cnp->cn_nameiop == CREATE ||
				 cnp->cn_nameiop == RENAME)) {
			error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred);
			if (error) {
				goto out;
			}
			error = EJUSTRETURN;
		} else {
			error = ENOENT;
		}
		/* done */
		goto done;
	}

	/*
	 * XXX NOTE tmpfs has a test here that tests that intermediate
	 * components i.e. not the last one ought to be either a directory or
	 * a link. It seems to function well without this code.
	 */

	/* try to create/reuse the node */
	error = udf_get_node(ump, &icb_loc, &res_node);
	if (error)
		goto out;

	/* check permissions */
	if (islastcn && (cnp->cn_nameiop == DELETE ||
			 cnp->cn_nameiop == RENAME)  ) {
		error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred);
		if (error) {
			vput(res_node->vnode);
			goto out;
		}

		/*
		 * Check if the directory has its sticky bit set. If so, ask
		 * for clearance since only the owner of a file or directory
		 * can remove/rename from taht directory.
		 */
		mode = udf_getaccessmode(dir_node);
		if ((mode & S_ISTXT) != 0) {
			udf_getownership(dir_node, &d_uid, &d_gid);
			error = kauth_authorize_vnode(cnp->cn_cred,
			    KAUTH_VNODE_DELETE, res_node->vnode,
			    dir_node->vnode, genfs_can_sticky(cnp->cn_cred,
			    d_uid, d_uid));
			if (error) {
				error = EPERM;
				vput(res_node->vnode);
				goto out;
			}
		}
	}

	*vpp = res_node->vnode;

done:
	/*
	 * Store result in the cache if requested. If we are creating a file,
	 * the file might not be found and thus putting it into the namecache
	 * might be seen as negative caching.
	 */
	if (nameiop != CREATE)
		cache_enter(dvp, *vpp, cnp->cn_nameptr, cnp->cn_namelen,
			    cnp->cn_flags);

out:
	if (error == 0 && *vpp != dvp)
		VOP_UNLOCK(*vpp);
	DPRINTFIF(LOOKUP, error, ("udf_lookup returing error %d\n", error));

	return error;
}

/* --------------------------------------------------------------------- */

int
udf_getattr(void *v)
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
		struct lwp   *a_l;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct udf_node *udf_node = VTOI(vp);
	struct udf_mount *ump = udf_node->ump;
	struct file_entry    *fe  = udf_node->fe;
	struct extfile_entry *efe = udf_node->efe;
	struct filetimes_extattr_entry *ft_extattr;
	struct device_extattr_entry *devattr;
	struct vattr *vap = ap->a_vap;
	struct timestamp *atime, *mtime, *attrtime, *creatime;
	uint64_t filesize, blkssize;
	uint32_t nlink;
	uint32_t offset, a_l;
	uint8_t *filedata, *targetbuf;
	uid_t uid;
	gid_t gid;
	int length, error;

	DPRINTF(CALL, ("udf_getattr called\n"));

	/* update times before we returning values */ 
	udf_itimes(udf_node, NULL, NULL, NULL);

	/* get descriptor information */
	if (fe) {
		nlink    = udf_rw16(fe->link_cnt);
		uid      = (uid_t)udf_rw32(fe->uid);
		gid      = (gid_t)udf_rw32(fe->gid);
		filesize = udf_rw64(fe->inf_len);
		blkssize = udf_rw64(fe->logblks_rec);
		atime    = &fe->atime;
		mtime    = &fe->mtime;
		attrtime = &fe->attrtime;
		filedata = fe->data;

		/* initial guess */
		creatime = mtime;

		/* check our extended attribute if present */
		error = udf_extattr_search_intern(udf_node,
			UDF_FILETIMES_ATTR_NO, "", &offset, &a_l);
		if (!error) {
			ft_extattr = (struct filetimes_extattr_entry *)
				(filedata + offset);
			if (ft_extattr->existence & UDF_FILETIMES_FILE_CREATION)
				creatime = &ft_extattr->times[0];
		}
	} else {
		assert(udf_node->efe);
		nlink    = udf_rw16(efe->link_cnt);
		uid      = (uid_t)udf_rw32(efe->uid);
		gid      = (gid_t)udf_rw32(efe->gid);
		filesize = udf_rw64(efe->inf_len);	/* XXX or obj_size? */
		blkssize = udf_rw64(efe->logblks_rec);
		atime    = &efe->atime;
		mtime    = &efe->mtime;
		attrtime = &efe->attrtime;
		creatime = &efe->ctime;
		filedata = efe->data;
	}

	/* do the uid/gid translation game */
	if (uid == (uid_t) -1)
		uid = ump->mount_args.anon_uid;
	if (gid == (gid_t) -1)
		gid = ump->mount_args.anon_gid;

	/* fill in struct vattr with values from the node */
	vattr_null(vap);
	vap->va_type      = vp->v_type;
	vap->va_mode      = udf_getaccessmode(udf_node);
	vap->va_nlink     = nlink;
	vap->va_uid       = uid;
	vap->va_gid       = gid;
	vap->va_fsid      = vp->v_mount->mnt_stat.f_fsidx.__fsid_val[0];
	vap->va_fileid    = udf_get_node_id(&udf_node->loc);   /* inode hash XXX */
	vap->va_size      = filesize;
	vap->va_blocksize = udf_node->ump->discinfo.sector_size;  /* wise? */

	/*
	 * BUG-ALERT: UDF doesn't count '.' as an entry, so we'll have to add
	 * 1 to the link count if its a directory we're requested attributes
	 * of.
	 */
	if (vap->va_type == VDIR)
		vap->va_nlink++;

	/*
	 * BUG-ALERT: Posix requires the va_size to be pathlength for symbolic
	 * links.
	 */
	if (vap->va_type == VLNK) {
		/* claim temporary buffers for translation */
		targetbuf = malloc(PATH_MAX+1, M_UDFTEMP, M_WAITOK);
		error = udf_do_readlink(udf_node, filesize, targetbuf, &length);
		if (!error) {
			vap->va_size = length;
			KASSERT(length == strlen(targetbuf));
		}
		free(targetbuf, M_UDFTEMP);
		/* XXX return error? */
	}

	/* access times */
	udf_timestamp_to_timespec(ump, atime,    &vap->va_atime);
	udf_timestamp_to_timespec(ump, mtime,    &vap->va_mtime);
	udf_timestamp_to_timespec(ump, attrtime, &vap->va_ctime);
	udf_timestamp_to_timespec(ump, creatime, &vap->va_birthtime);

	vap->va_gen       = 1;		/* no multiple generations yes (!?) */
	vap->va_flags     = 0;		/* no flags */
	vap->va_bytes     = blkssize * udf_node->ump->discinfo.sector_size;
	vap->va_filerev   = 1;		/* TODO file revision numbers? */
	vap->va_vaflags   = 0;
	/* TODO get vaflags from the extended attributes? */

	if ((vap->va_type == VBLK) || (vap->va_type == VCHR)) {
		error = udf_extattr_search_intern(udf_node,
				UDF_DEVICESPEC_ATTR_NO, "",
				&offset, &a_l);
		/* if error, deny access */
		if (error || (filedata == NULL)) {
			vap->va_mode = 0;	/* or v_type = VNON?  */
		} else {
			devattr = (struct device_extattr_entry *)
				filedata + offset;
			vap->va_rdev = makedev(
				udf_rw32(devattr->major),
				udf_rw32(devattr->minor)
				);
			/* TODO we could check the implementator */
		}
	}

	return 0;
}

/* --------------------------------------------------------------------- */

static int
udf_chown(struct vnode *vp, uid_t new_uid, gid_t new_gid,
	  kauth_cred_t cred)
{
	struct udf_node  *udf_node = VTOI(vp);
	uid_t uid;
	gid_t gid;
	int error;

#ifdef notyet
	/* TODO get vaflags from the extended attributes? */
	/* Immutable or append-only files cannot be modified, either. */
	if (udf_node->flags & (IMMUTABLE | APPEND))
		return EPERM;
#endif

	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return EROFS;

	/* retrieve old values */
	udf_getownership(udf_node, &uid, &gid);

	/* only one could be specified */
	if (new_uid == VNOVAL)
		new_uid = uid;
	if (new_gid == VNOVAL)
		new_gid = gid;

	/* check if we can fit it in an 32 bits */
	if ((uid_t) ((uint32_t) new_uid) != new_uid)
		return EINVAL;
	if ((gid_t) ((uint32_t) new_gid) != new_gid)
		return EINVAL;

	/* check permissions */
	error = kauth_authorize_vnode(cred, KAUTH_VNODE_CHANGE_OWNERSHIP,
	    vp, NULL, genfs_can_chown(cred, uid, gid, new_uid, new_gid));
	if (error)
		return (error);

	/* change the ownership */
	udf_setownership(udf_node, new_uid, new_gid);

	/* mark node changed */
	udf_node->i_flags |= IN_CHANGE;

	return 0;
}


static int
udf_chmod(struct vnode *vp, mode_t mode, kauth_cred_t cred)
{
	struct udf_node  *udf_node = VTOI(vp);
	uid_t uid;
	gid_t gid;
	int error;

#ifdef notyet
	/* TODO get vaflags from the extended attributes? */
	/* Immutable or append-only files cannot be modified, either. */
	if (udf_node->flags & (IMMUTABLE | APPEND))
		return EPERM;
#endif

	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return EROFS;

	/* retrieve uid/gid values */
	udf_getownership(udf_node, &uid, &gid);

	/* check permissions */
	error = kauth_authorize_vnode(cred, KAUTH_VNODE_WRITE_SECURITY, vp,
	    NULL, genfs_can_chmod(vp->v_type, cred, uid, gid, mode));
	if (error)
		return (error);

	/* change mode */
	udf_setaccessmode(udf_node, mode);

	/* mark node changed */
	udf_node->i_flags |= IN_CHANGE;

	return 0;
}


/* exported */
int
udf_chsize(struct vnode *vp, u_quad_t newsize, kauth_cred_t cred)
{
	struct udf_node  *udf_node = VTOI(vp);
	int error, extended;

	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return EROFS;

	/* Decide whether this is a valid operation based on the file type. */
	switch (vp->v_type) {
	case VDIR:
		return EISDIR;
	case VREG:
		if (vp->v_mount->mnt_flag & MNT_RDONLY)
			return EROFS;
		break;
	case VBLK:
		/* FALLTHROUGH */
	case VCHR:
		/* FALLTHROUGH */
	case VFIFO:
		/* Allow modifications of special files even if in the file
		 * system is mounted read-only (we are not modifying the
		 * files themselves, but the objects they represent). */
		return 0;
	default:
		/* Anything else is unsupported. */
		return EOPNOTSUPP;
	}

#if notyet
	/* TODO get vaflags from the extended attributes? */
	/* Immutable or append-only files cannot be modified, either. */
	if (node->flags & (IMMUTABLE | APPEND))
		return EPERM;
#endif

	/* resize file to the requested size */
	error = udf_resize_node(udf_node, newsize, &extended);

	if (error == 0) {
		/* mark change */
		udf_node->i_flags |= IN_CHANGE | IN_MODIFY;
		if (vp->v_mount->mnt_flag & MNT_RELATIME)
			udf_node->i_flags |= IN_ACCESS;
		VN_KNOTE(vp, NOTE_ATTRIB | (extended ? NOTE_EXTEND : 0));
		udf_update(vp, NULL, NULL, NULL, 0);
	}

	return error;
}


static int
udf_chflags(struct vnode *vp, mode_t mode, kauth_cred_t cred)
{
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return EROFS;

	/*
	 * XXX we can't do this yet, as its not described in the standard yet
	 */

	return EOPNOTSUPP;
}


static int
udf_chtimes(struct vnode *vp,
	struct timespec *atime, struct timespec *mtime,
	struct timespec *birthtime, int setattrflags,
	kauth_cred_t cred)
{
	struct udf_node  *udf_node = VTOI(vp);
	uid_t uid;
	gid_t gid;
	int error;

#ifdef notyet
	/* TODO get vaflags from the extended attributes? */
	/* Immutable or append-only files cannot be modified, either. */
	if (udf_node->flags & (IMMUTABLE | APPEND))
		return EPERM;
#endif

	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return EROFS;

	/* retrieve uid/gid values */
	udf_getownership(udf_node, &uid, &gid);

	/* check permissions */
	error = kauth_authorize_vnode(cred, KAUTH_VNODE_WRITE_TIMES, vp,
	    NULL, genfs_can_chtimes(vp, setattrflags, uid, cred));
	if (error)
		return (error);

	/* update node flags depending on what times are passed */
	if (atime->tv_sec != VNOVAL)
		if (!(vp->v_mount->mnt_flag & MNT_NOATIME))
			udf_node->i_flags |= IN_ACCESS;
	if ((mtime->tv_sec != VNOVAL) || (birthtime->tv_sec != VNOVAL)) {
		udf_node->i_flags |= IN_CHANGE | IN_UPDATE;
		if (vp->v_mount->mnt_flag & MNT_RELATIME)
			udf_node->i_flags |= IN_ACCESS;
	}

	return udf_update(vp, atime, mtime, birthtime, 0);
}


int
udf_setattr(void *v)
{
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
		struct lwp   *a_l;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
/*	struct udf_node  *udf_node = VTOI(vp); */
/*	struct udf_mount *ump = udf_node->ump; */
	kauth_cred_t cred = ap->a_cred;
	struct vattr *vap = ap->a_vap;
	int error;

	DPRINTF(CALL, ("udf_setattr called\n"));

	/* Abort if any unsettable attribute is given. */
	error = 0;
	if (vap->va_type != VNON ||
	    vap->va_nlink != VNOVAL ||
	    vap->va_fsid != VNOVAL ||
	    vap->va_fileid != VNOVAL ||
	    vap->va_blocksize != VNOVAL ||
#ifdef notyet
	    /* checks are debated */
	    vap->va_ctime.tv_sec != VNOVAL ||
	    vap->va_ctime.tv_nsec != VNOVAL ||
	    vap->va_birthtime.tv_sec != VNOVAL ||
	    vap->va_birthtime.tv_nsec != VNOVAL ||
#endif
	    vap->va_gen != VNOVAL ||
	    vap->va_rdev != VNOVAL ||
	    vap->va_bytes != VNOVAL)
		error = EINVAL;

	DPRINTF(ATTR, ("setattr changing:\n"));
	if (error == 0 && (vap->va_flags != VNOVAL)) {
		DPRINTF(ATTR, ("\tchflags\n"));
	 	error = udf_chflags(vp, vap->va_flags, cred);
	}

	if (error == 0 && (vap->va_size != VNOVAL)) {
		DPRINTF(ATTR, ("\tchsize\n"));
		error = udf_chsize(vp, vap->va_size, cred);
	}

	if (error == 0 && (vap->va_uid != VNOVAL || vap->va_gid != VNOVAL)) {
		DPRINTF(ATTR, ("\tchown\n"));
		error = udf_chown(vp, vap->va_uid, vap->va_gid, cred);
	}

	if (error == 0 && (vap->va_mode != VNOVAL)) {
		DPRINTF(ATTR, ("\tchmod\n"));
		error = udf_chmod(vp, vap->va_mode, cred);
	}

	if (error == 0 &&
	    ((vap->va_atime.tv_sec != VNOVAL &&
	      vap->va_atime.tv_nsec != VNOVAL)   ||
	     (vap->va_mtime.tv_sec != VNOVAL &&
	      vap->va_mtime.tv_nsec != VNOVAL))
	    ) {
		DPRINTF(ATTR, ("\tchtimes\n"));
		error = udf_chtimes(vp, &vap->va_atime, &vap->va_mtime,
		    &vap->va_birthtime, vap->va_vaflags, cred);
	}
	VN_KNOTE(vp, NOTE_ATTRIB);

	return error;
}

/* --------------------------------------------------------------------- */

/*
 * Return POSIX pathconf information for UDF file systems.
 */
int
udf_pathconf(void *v)
{
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		register_t *a_retval;
	} */ *ap = v;
	uint32_t bits;

	DPRINTF(CALL, ("udf_pathconf called\n"));

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = (1<<16)-1;	/* 16 bits */
		return 0;
	case _PC_NAME_MAX:
		*ap->a_retval = UDF_MAXNAMLEN;
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
		if (udf_node)
			bits = 64 * vp->v_mount->mnt_dev_bshift;
#endif
		*ap->a_retval = bits;
		return 0;
	}

	return EINVAL;
}


/* --------------------------------------------------------------------- */

int
udf_open(void *v)
{
	struct vop_open_args /* {
		struct vnode *a_vp;
		int a_mode;
		kauth_cred_t a_cred;
		struct proc *a_p;
	} */ *ap = v;
	int flags;

	DPRINTF(CALL, ("udf_open called\n"));

	/*
	 * Files marked append-only must be opened for appending.
	 * TODO: get chflags(2) flags from extened attribute.
	 */
	flags = 0;
	if ((flags & APPEND) && (ap->a_mode & (FWRITE | O_APPEND)) == FWRITE)
		return (EPERM);

	return 0;
}


/* --------------------------------------------------------------------- */

int
udf_close(void *v)
{
	struct vop_close_args /* {
		struct vnode *a_vp;
		int a_fflag;
		kauth_cred_t a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct udf_node *udf_node = VTOI(vp);
	int async = vp->v_mount->mnt_flag & MNT_ASYNC;
	int error;

	DPRINTF(CALL, ("udf_close called\n"));
	udf_node = udf_node;	/* shut up gcc */

	if (!async && (vp->v_type != VDIR)) {
		mutex_enter(vp->v_interlock);
		error = VOP_PUTPAGES(vp, 0, 0, PGO_CLEANIT);
		if (error)
			return error;
	}

	mutex_enter(vp->v_interlock);
		if (vp->v_usecount > 1)
			udf_itimes(udf_node, NULL, NULL, NULL);
	mutex_exit(vp->v_interlock);

	return 0;
}


/* --------------------------------------------------------------------- */

static int
udf_check_possible(struct vnode *vp, struct vattr *vap, mode_t mode)
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
	/* TODO: get chflags(2) flags from extened attribute. */
	flags = 0;
	if ((mode & VWRITE) && (flags & IMMUTABLE))
		return EPERM;

	return 0;
}

static int
udf_check_permitted(struct vnode *vp, struct vattr *vap, mode_t mode,
    kauth_cred_t cred)
{
	/* ask the generic genfs_can_access to advice on security */
	return kauth_authorize_vnode(cred, KAUTH_ACCESS_ACTION(mode,
	    vp->v_type, vap->va_mode), vp, NULL, genfs_can_access(vp->v_type,
	    vap->va_mode, vap->va_uid, vap->va_gid, mode, cred));
}

int
udf_access(void *v)
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
	/* struct udf_node *udf_node = VTOI(vp); */
	struct vattr vap;
	int error;

	DPRINTF(CALL, ("udf_access called\n"));

	error = VOP_GETATTR(vp, &vap, NULL);
	if (error)
		return error;

	error = udf_check_possible(vp, &vap, mode);
	if (error)
		return error;

	error = udf_check_permitted(vp, &vap, mode, cred);

	return error;
}

/* --------------------------------------------------------------------- */

int
udf_create(void *v)
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

	DPRINTF(CALL, ("udf_create called\n"));
	error = udf_create_node(dvp, vpp, vap, cnp);

	return error;
}

/* --------------------------------------------------------------------- */

int
udf_mknod(void *v)
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

	DPRINTF(CALL, ("udf_mknod called\n"));
	error = udf_create_node(dvp, vpp, vap, cnp);

	return error;
}

/* --------------------------------------------------------------------- */

int
udf_mkdir(void *v)
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

	DPRINTF(CALL, ("udf_mkdir called\n"));
	error = udf_create_node(dvp, vpp, vap, cnp);

	return error;
}

/* --------------------------------------------------------------------- */

static int
udf_do_link(struct vnode *dvp, struct vnode *vp, struct componentname *cnp)
{
	struct udf_node *udf_node, *dir_node;
	struct vattr vap;
	int error;

	DPRINTF(CALL, ("udf_link called\n"));
	KASSERT(dvp != vp);
	KASSERT(vp->v_type != VDIR);
	KASSERT(dvp->v_mount == vp->v_mount);

	/* get attributes */
	dir_node = VTOI(dvp);
	udf_node = VTOI(vp);

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

	error = udf_dir_attach(dir_node->ump, dir_node, udf_node, &vap, cnp);
	if (error)
		VOP_UNLOCK(vp);
	return error;
}

int
udf_link(void *v)
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

	error = udf_do_link(dvp, vp, cnp);
	if (error)
		VOP_ABORTOP(dvp, cnp);

	VN_KNOTE(vp, NOTE_LINK);
	VN_KNOTE(dvp, NOTE_WRITE);

	return error;
}

/* --------------------------------------------------------------------- */

static int
udf_do_symlink(struct udf_node *udf_node, char *target)
{
	struct pathcomp pathcomp;
	uint8_t *pathbuf, *pathpos, *compnamepos;
	char *mntonname;
	int pathlen, len, compnamelen, mntonnamelen;
	int error;

	/* process `target' to an UDF structure */
	pathbuf = malloc(UDF_SYMLINKBUFLEN, M_UDFTEMP, M_WAITOK);
	pathpos = pathbuf;
	pathlen = 0;

	if (*target == '/') {
		/* symlink starts from the root */
		len = UDF_PATH_COMP_SIZE;
		memset(&pathcomp, 0, len);
		pathcomp.type = UDF_PATH_COMP_ROOT;

		/* check if its mount-point relative! */
		mntonname    = udf_node->ump->vfs_mountp->mnt_stat.f_mntonname;
		mntonnamelen = strlen(mntonname);
		if (strlen(target) >= mntonnamelen) {
			if (strncmp(target, mntonname, mntonnamelen) == 0) {
				pathcomp.type = UDF_PATH_COMP_MOUNTROOT;
				target += mntonnamelen;
			}
		} else {
			target++;
		}

		memcpy(pathpos, &pathcomp, len);
		pathpos += len;
		pathlen += len;
	}

	error = 0;
	while (*target) {
		/* ignore multiple '/' */
		while (*target == '/') {
			target++;
		}
		if (!*target)
			break;

		/* extract component name */
		compnamelen = 0;
		compnamepos = target;
		while ((*target) && (*target != '/')) {
			target++;
			compnamelen++;
		}

		/* just trunc if too long ?? (security issue) */
		if (compnamelen >= 127) {
			error = ENAMETOOLONG;
			break;
		}

		/* convert unix name to UDF name */
		len = sizeof(struct pathcomp);
		memset(&pathcomp, 0, len);
		pathcomp.type = UDF_PATH_COMP_NAME;
		len = UDF_PATH_COMP_SIZE;

		if ((compnamelen == 2) && (strncmp(compnamepos, "..", 2) == 0))
			pathcomp.type = UDF_PATH_COMP_PARENTDIR;
		if ((compnamelen == 1) && (*compnamepos == '.'))
			pathcomp.type = UDF_PATH_COMP_CURDIR;

		if (pathcomp.type == UDF_PATH_COMP_NAME) {
			unix_to_udf_name(
				(char *) &pathcomp.ident, &pathcomp.l_ci,
				compnamepos, compnamelen,
				&udf_node->ump->logical_vol->desc_charset);
			len = UDF_PATH_COMP_SIZE + pathcomp.l_ci;
		}

		if (pathlen + len >= UDF_SYMLINKBUFLEN) {
			error = ENAMETOOLONG;
			break;
		}

		memcpy(pathpos, &pathcomp, len);
		pathpos += len;
		pathlen += len;
	}

	if (error) {
		/* aparently too big */
		free(pathbuf, M_UDFTEMP);
		return error;
	}

	error = udf_grow_node(udf_node, pathlen);
	if (error) {
		/* failed to pregrow node */
		free(pathbuf, M_UDFTEMP);
		return error;
	}

	/* write out structure on the new file */
	error = vn_rdwr(UIO_WRITE, udf_node->vnode,
		pathbuf, pathlen, 0,
		UIO_SYSSPACE, IO_NODELOCKED | IO_ALTSEMANTICS,
		FSCRED, NULL, NULL);

	/* return status of symlink contents writeout */
	free(pathbuf, M_UDFTEMP);
	return error;
}


int
udf_symlink(void *v)
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
	struct udf_node *dir_node;
	struct udf_node *udf_node;
	int error;

	DPRINTF(CALL, ("udf_symlink called\n"));
	DPRINTF(CALL, ("\tlinking to `%s`\n",  ap->a_target));
	error = udf_create_node(dvp, vpp, vap, cnp);
	KASSERT(((error == 0) && (*vpp != NULL)) || ((error && (*vpp == NULL))));
	if (!error) {
		dir_node = VTOI(dvp);
		udf_node = VTOI(*vpp);
		KASSERT(udf_node);
		error = udf_do_symlink(udf_node, ap->a_target);
		if (error) {
			/* remove node */
			udf_dir_detach(udf_node->ump, dir_node, udf_node, cnp);
			vrele(*vpp);
			*vpp = NULL;
		}
	}
	return error;
}

/* --------------------------------------------------------------------- */

static int
udf_do_readlink(struct udf_node *udf_node, uint64_t filesize,
	uint8_t *targetbuf, int *length)
{
	struct pathcomp pathcomp;
	uint8_t *pathbuf, *tmpname;
	uint8_t *pathpos, *targetpos;
	char *mntonname;
	int pathlen, targetlen, namelen, mntonnamelen, len, l_ci;
	int first, error;

	pathbuf   = malloc(UDF_SYMLINKBUFLEN, M_UDFTEMP, M_WAITOK);
	tmpname   = malloc(PATH_MAX+1, M_UDFTEMP, M_WAITOK);
	memset(pathbuf, 0, UDF_SYMLINKBUFLEN);
	memset(targetbuf, 0, PATH_MAX);

	/* read contents of file in our temporary buffer */
	error = vn_rdwr(UIO_READ, udf_node->vnode,
		pathbuf, filesize, 0,
		UIO_SYSSPACE, IO_NODELOCKED | IO_ALTSEMANTICS,
		FSCRED, NULL, NULL);
	if (error) {
		/* failed to read in symlink contents */
		free(pathbuf, M_UDFTEMP);
		free(tmpname, M_UDFTEMP);
		return error;
	}

	/* convert to a unix path */
	pathpos   = pathbuf;
	pathlen   = 0;
	targetpos = targetbuf;
	targetlen = PATH_MAX;
	mntonname    = udf_node->ump->vfs_mountp->mnt_stat.f_mntonname;
	mntonnamelen = strlen(mntonname);

	error = 0;
	first = 1;
	while (filesize - pathlen >= UDF_PATH_COMP_SIZE) {
		len = UDF_PATH_COMP_SIZE;
		memcpy(&pathcomp, pathpos, len);
		l_ci = pathcomp.l_ci;
		switch (pathcomp.type) {
		case UDF_PATH_COMP_ROOT :
			/* XXX should check for l_ci; bugcompatible now */
			if ((targetlen < 1) || !first) {
				error = EINVAL;
				break;
			}
			*targetpos++ = '/'; targetlen--;
			break;
		case UDF_PATH_COMP_MOUNTROOT :
			/* XXX what should it be if l_ci > 0 ? [4/48.16.1.2] */
			if (l_ci || (targetlen < mntonnamelen+1) || !first) {
				error = EINVAL;
				break;
			}
			memcpy(targetpos, mntonname, mntonnamelen);
			targetpos += mntonnamelen; targetlen -= mntonnamelen;
			if (filesize-pathlen > UDF_PATH_COMP_SIZE+l_ci) {
				/* more follows, so must be directory */
				*targetpos++ = '/'; targetlen--;
			}
			break;
		case UDF_PATH_COMP_PARENTDIR :
			/* XXX should check for l_ci; bugcompatible now */
			if (targetlen < 3) {
				error = EINVAL;
				break;
			}
			*targetpos++ = '.'; targetlen--;
			*targetpos++ = '.'; targetlen--;
			*targetpos++ = '/'; targetlen--;
			break;
		case UDF_PATH_COMP_CURDIR :
			/* XXX should check for l_ci; bugcompatible now */
			if (targetlen < 2) {
				error = EINVAL;
				break;
			}
			*targetpos++ = '.'; targetlen--;
			*targetpos++ = '/'; targetlen--;
			break;
		case UDF_PATH_COMP_NAME :
			if (l_ci == 0) {
				error = EINVAL;
				break;
			}
			memset(tmpname, 0, PATH_MAX);
			memcpy(&pathcomp, pathpos, len + l_ci);
			udf_to_unix_name(tmpname, MAXPATHLEN,
				pathcomp.ident, l_ci,
				&udf_node->ump->logical_vol->desc_charset);
			namelen = strlen(tmpname);
			if (targetlen < namelen + 1) {
				error = EINVAL;
				break;
			}
			memcpy(targetpos, tmpname, namelen);
			targetpos += namelen; targetlen -= namelen;
			if (filesize-pathlen > UDF_PATH_COMP_SIZE+l_ci) {
				/* more follows, so must be directory */
				*targetpos++ = '/'; targetlen--;
			}
			break;
		default :
			error = EINVAL;
			break;
		}
		first = 0;
		if (error)
			break;
		pathpos += UDF_PATH_COMP_SIZE + l_ci;
		pathlen += UDF_PATH_COMP_SIZE + l_ci;

	}
	/* all processed? */
	if (filesize - pathlen > 0)
		error = EINVAL;

	free(pathbuf, M_UDFTEMP);
	free(tmpname, M_UDFTEMP);

	*length = PATH_MAX - targetlen;
	return error;
}


int
udf_readlink(void *v)
{
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct udf_node *udf_node = VTOI(vp);
	struct file_entry    *fe  = udf_node->fe;
	struct extfile_entry *efe = udf_node->efe;
	struct uio *uio = ap->a_uio;
	uint64_t filesize;
	uint8_t *targetbuf;
	int length;
	int error;

	DPRINTF(CALL, ("udf_readlink called\n"));

	if (fe) {
		filesize = udf_rw64(fe->inf_len);
	} else {
		assert(udf_node->efe);
		filesize = udf_rw64(efe->inf_len);
	}

	/* claim temporary buffers for translation */
	targetbuf = malloc(PATH_MAX+1, M_UDFTEMP, M_WAITOK);

	error = udf_do_readlink(udf_node, filesize, targetbuf, &length);

	/* uiomove() to destination */
	if (!error)
		uiomove(targetbuf, length, uio);

	free(targetbuf, M_UDFTEMP);
	return error;
}

/* --------------------------------------------------------------------- */

/*
 * udf_rename() moved to udf_rename.c
 */

/* --------------------------------------------------------------------- */

int
udf_remove(void *v)
{
	struct vop_remove_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct vnode *vp  = ap->a_vp;
	struct componentname *cnp = ap->a_cnp;
	struct udf_node *dir_node = VTOI(dvp);
	struct udf_node *udf_node = VTOI(vp);
	struct udf_mount *ump = dir_node->ump;
	int error;

	DPRINTF(CALL, ("udf_remove called\n"));
	if (vp->v_type != VDIR) {
		error = udf_dir_detach(ump, dir_node, udf_node, cnp);
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
udf_rmdir(void *v)
{
	struct vop_rmdir_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct udf_node *dir_node = VTOI(dvp);
	struct udf_node *udf_node = VTOI(vp);
	struct udf_mount *ump = dir_node->ump;
	int error, isempty;

	DPRINTF(NOTIMPL, ("udf_rmdir '%s' called\n", cnp->cn_nameptr));

	/* don't allow '.' to be deleted */
	if (dir_node == udf_node) {
		vrele(dvp);
		vput(vp);
		return EINVAL;
	}

	/* make sure our `leaf' node's hash is populated */
	dirhash_get(&udf_node->dir_hash);
	error = udf_dirhash_fill(udf_node);
	if (error) {
		dirhash_put(udf_node->dir_hash);
		return error;
	}

	/* check to see if the directory is empty */
	isempty = dirhash_dir_isempty(udf_node->dir_hash);
	dirhash_put(udf_node->dir_hash);

	if (!isempty) {
		vput(dvp);
		vput(vp);
		return ENOTEMPTY;
	}

	/* detach the node from the directory, udf_node is an empty dir here */
	error = udf_dir_detach(ump, dir_node, udf_node, cnp);
	if (error == 0) {
		cache_purge(vp);
//		cache_purge(dvp);	/* XXX from msdosfs, why? */
		/*
		 * Bug alert: we need to remove '..' from the detaching
		 * udf_node so further lookups of this are not possible. This
		 * prevents a process in a deleted directory from going to its
		 * deleted parent. Since `udf_node' is garanteed to be empty
		 * here, trunc it so no fids are there.
		 */
		dirhash_purge(&udf_node->dir_hash);
		udf_shrink_node(udf_node, 0);
		VN_KNOTE(vp, NOTE_DELETE);
	}
	DPRINTFIF(NODE, error, ("\tgot error removing dir\n"));

	/* unput the nodes and exit */
	vput(dvp);
	vput(vp);

	return error;
}

/* --------------------------------------------------------------------- */

int
udf_fsync(void *v)
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
	struct udf_node *udf_node = VTOI(vp);
	int error, flags, wait;

	DPRINTF(SYNC, ("udf_fsync called on %p : %s, %s\n",
		udf_node,
		(ap->a_flags & FSYNC_WAIT)     ? "wait":"no wait",
		(ap->a_flags & FSYNC_DATAONLY) ? "data_only":"complete"));

	/* flush data and wait for it when requested */
	wait = (ap->a_flags & FSYNC_WAIT) ? UPDATE_WAIT : 0;
	error = vflushbuf(vp, ap->a_flags);
	if (error)
		return error;

	if (udf_node == NULL) {
		printf("udf_fsync() called on NULL udf_node!\n");
		return 0;
	}
	if (vp->v_tag != VT_UDF) {
		printf("udf_fsync() called on node not tagged as UDF node!\n");
		return 0;
	}

	/* set our times */
	udf_itimes(udf_node, NULL, NULL, NULL);

	/* if called when mounted readonly, never write back */
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return 0;

	/* if only data is requested, return */
	if (ap->a_flags & FSYNC_DATAONLY)
		return 0;

	/* check if the node is dirty 'enough'*/
	flags = udf_node->i_flags & (IN_MODIFIED | IN_ACCESSED);
	if (flags == 0)
		return 0;

	/* if we don't have to wait, check for IO pending */
	if (!wait) {
		if (vp->v_numoutput > 0) {
			DPRINTF(SYNC, ("udf_fsync %p, rejecting on v_numoutput\n", udf_node));
			return 0;
		}
		if (udf_node->outstanding_bufs > 0) {
			DPRINTF(SYNC, ("udf_fsync %p, rejecting on outstanding_bufs\n", udf_node));
			return 0;
		}
		if (udf_node->outstanding_nodedscr > 0) {
			DPRINTF(SYNC, ("udf_fsync %p, rejecting on outstanding_nodedscr\n", udf_node));
			return 0;
		}
	}

	/* wait until vp->v_numoutput reaches zero i.e. is finished */
	if (wait) {
		DPRINTF(SYNC, ("udf_fsync %p, waiting\n", udf_node));
		mutex_enter(vp->v_interlock);
		while (vp->v_numoutput) {
			DPRINTF(SYNC, ("udf_fsync %p, v_numoutput %d\n", udf_node, vp->v_numoutput));
			cv_timedwait(&vp->v_cv, vp->v_interlock, hz/8);
		}
		mutex_exit(vp->v_interlock);
		DPRINTF(SYNC, ("udf_fsync %p, fin wait\n", udf_node));
	}

	/* write out node and wait for it if requested */
	DPRINTF(SYNC, ("udf_fsync %p, writeout node\n", udf_node));
	error = udf_writeout_node(udf_node, wait);
	if (error)
		return error;

	/* TODO/XXX if ap->a_flags & FSYNC_CACHE, we ought to do a disc sync */

	return 0;
}

/* --------------------------------------------------------------------- */

int
udf_advlock(void *v)
{
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		void *a_id;
		int a_op;
		struct flock *a_fl;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct udf_node *udf_node = VTOI(vp);
	struct file_entry    *fe;
	struct extfile_entry *efe;
	uint64_t file_size;

	DPRINTF(LOCKING, ("udf_advlock called\n"));

	/* get directory filesize */
	if (udf_node->fe) {
		fe = udf_node->fe;
		file_size = udf_rw64(fe->inf_len);
	} else {
		assert(udf_node->efe);
		efe = udf_node->efe;
		file_size = udf_rw64(efe->inf_len);
	}

	return lf_advlock(ap, &udf_node->lockf, file_size);
}

/* --------------------------------------------------------------------- */

/* Global vfs vnode data structures for udfs */
int (**udf_vnodeop_p)(void *);

const struct vnodeopv_entry_desc udf_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, udf_lookup },	/* lookup */
	{ &vop_create_desc, udf_create },	/* create */
	{ &vop_mknod_desc, udf_mknod },		/* mknod */	/* TODO */
	{ &vop_open_desc, udf_open },		/* open */
	{ &vop_close_desc, udf_close },		/* close */
	{ &vop_access_desc, udf_access },	/* access */
	{ &vop_getattr_desc, udf_getattr },	/* getattr */
	{ &vop_setattr_desc, udf_setattr },	/* setattr */	/* TODO chflags */
	{ &vop_read_desc, udf_read },		/* read */
	{ &vop_write_desc, udf_write },		/* write */	/* WRITE */
	{ &vop_fallocate_desc, genfs_eopnotsupp }, /* fallocate */
	{ &vop_fdiscard_desc, genfs_eopnotsupp }, /* fdiscard */
	{ &vop_fcntl_desc, genfs_fcntl },	/* fcntl */	/* TODO? */
	{ &vop_ioctl_desc, genfs_enoioctl },	/* ioctl */	/* TODO? */
	{ &vop_poll_desc, genfs_poll },		/* poll */	/* TODO/OK? */
	{ &vop_kqfilter_desc, genfs_kqfilter },	/* kqfilter */	/* ? */
	{ &vop_revoke_desc, genfs_revoke },	/* revoke */	/* TODO? */
	{ &vop_mmap_desc, genfs_mmap },		/* mmap */	/* OK? */
	{ &vop_fsync_desc, udf_fsync },		/* fsync */
	{ &vop_seek_desc, genfs_seek },		/* seek */
	{ &vop_remove_desc, udf_remove },	/* remove */
	{ &vop_link_desc, udf_link },		/* link */	/* TODO */
	{ &vop_rename_desc, udf_rename },	/* rename */ 	/* TODO */
	{ &vop_mkdir_desc, udf_mkdir },		/* mkdir */ 
	{ &vop_rmdir_desc, udf_rmdir },		/* rmdir */
	{ &vop_symlink_desc, udf_symlink },	/* symlink */	/* TODO */
	{ &vop_readdir_desc, udf_readdir },	/* readdir */
	{ &vop_readlink_desc, udf_readlink },	/* readlink */	/* TEST ME */
	{ &vop_abortop_desc, genfs_abortop },	/* abortop */	/* TODO/OK? */
	{ &vop_inactive_desc, udf_inactive },	/* inactive */
	{ &vop_reclaim_desc, udf_reclaim },	/* reclaim */
	{ &vop_lock_desc, genfs_lock },		/* lock */
	{ &vop_unlock_desc, genfs_unlock },	/* unlock */
	{ &vop_bmap_desc, udf_trivial_bmap },	/* bmap */	/* 1:1 bmap */
	{ &vop_strategy_desc, udf_vfsstrategy },/* strategy */
/*	{ &vop_print_desc, udf_print },	*/	/* print */
	{ &vop_islocked_desc, genfs_islocked },	/* islocked */
	{ &vop_pathconf_desc, udf_pathconf },	/* pathconf */
	{ &vop_advlock_desc, udf_advlock },	/* advlock */	/* TEST ME */
	{ &vop_bwrite_desc, vn_bwrite },	/* bwrite */	/* ->strategy */
	{ &vop_getpages_desc, genfs_getpages },	/* getpages */
	{ &vop_putpages_desc, genfs_putpages },	/* putpages */
	{ NULL, NULL }
};


const struct vnodeopv_desc udf_vnodeop_opv_desc = {
	&udf_vnodeop_p, udf_vnodeop_entries
};

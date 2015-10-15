/*	$NetBSD: lfs_rename.c,v 1.16 2015/09/21 01:24:23 dholland Exp $	*/
/*  from NetBSD: ufs_rename.c,v 1.6 2013/01/22 09:39:18 dholland Exp  */

/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Taylor R Campbell.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*-
 * Copyright (c) 1999, 2000, 2001, 2002, 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Konrad E. Schroder <perseant@hhhh.org>.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1986, 1989, 1991, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)lfs_vnops.c	8.13 (Berkeley) 6/10/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lfs_rename.c,v 1.16 2015/09/21 01:24:23 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/namei.h>
#include <sys/resourcevar.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/vnode_if.h>
#include <sys/pool.h>
#include <sys/signalvar.h>
#include <sys/kauth.h>
#include <sys/syslog.h>

#include <uvm/uvm.h>
#include <uvm/uvm_pmap.h>
#include <uvm/uvm_stat.h>
#include <uvm/uvm_pager.h>

#include <miscfs/fifofs/fifo.h>
#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>

#include <ufs/lfs/ulfs_inode.h>
#include <ufs/lfs/ulfsmount.h>
#include <ufs/lfs/ulfs_bswap.h>
#include <ufs/lfs/ulfs_extern.h>

#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_accessors.h>
#include <ufs/lfs/lfs_extern.h>

/*
 * ulfs_gro_directory_empty_p: Return true if the directory vp is
 * empty.  dvp is its parent.
 *
 * vp and dvp must be locked and referenced.
 */
static bool
ulfs_gro_directory_empty_p(struct mount *mp, kauth_cred_t cred,
    struct vnode *vp, struct vnode *dvp)
{

	(void)mp;
	KASSERT(mp != NULL);
	KASSERT(vp != NULL);
	KASSERT(dvp != NULL);
	KASSERT(vp != dvp);
	KASSERT(vp->v_mount == mp);
	KASSERT(dvp->v_mount == mp);
	KASSERT(VOP_ISLOCKED(vp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(dvp) == LK_EXCLUSIVE);

	return ulfs_dirempty(VTOI(vp), VTOI(dvp)->i_number, cred);
}

/*
 * ulfs_gro_rename_check_possible: Check whether a rename is possible
 * independent of credentials.
 */
static int
ulfs_gro_rename_check_possible(struct mount *mp,
    struct vnode *fdvp, struct vnode *fvp,
    struct vnode *tdvp, struct vnode *tvp)
{

	(void)mp;
	KASSERT(mp != NULL);
	KASSERT(fdvp != NULL);
	KASSERT(fvp != NULL);
	KASSERT(tdvp != NULL);
	KASSERT(fdvp != fvp);
	KASSERT(fdvp != tvp);
	KASSERT(tdvp != fvp);
	KASSERT(tdvp != tvp);
	KASSERT(fvp != tvp);
	KASSERT(fdvp->v_type == VDIR);
	KASSERT(tdvp->v_type == VDIR);
	KASSERT(fdvp->v_mount == mp);
	KASSERT(fvp->v_mount == mp);
	KASSERT(tdvp->v_mount == mp);
	KASSERT((tvp == NULL) || (tvp->v_mount == mp));
	KASSERT(VOP_ISLOCKED(fdvp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(fvp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(tdvp) == LK_EXCLUSIVE);
	KASSERT((tvp == NULL) || (VOP_ISLOCKED(tvp) == LK_EXCLUSIVE));

	return genfs_ufslike_rename_check_possible(
	    VTOI(fdvp)->i_flags, VTOI(fvp)->i_flags,
	    VTOI(tdvp)->i_flags, (tvp? VTOI(tvp)->i_flags : 0),
	    (tvp != NULL),
	    IMMUTABLE, APPEND);
}

/*
 * ulfs_gro_rename_check_permitted: Check whether a rename is permitted
 * given our credentials.
 */
static int
ulfs_gro_rename_check_permitted(struct mount *mp, kauth_cred_t cred,
    struct vnode *fdvp, struct vnode *fvp,
    struct vnode *tdvp, struct vnode *tvp)
{

	(void)mp;
	KASSERT(mp != NULL);
	KASSERT(fdvp != NULL);
	KASSERT(fvp != NULL);
	KASSERT(tdvp != NULL);
	KASSERT(fdvp != fvp);
	KASSERT(fdvp != tvp);
	KASSERT(tdvp != fvp);
	KASSERT(tdvp != tvp);
	KASSERT(fvp != tvp);
	KASSERT(fdvp->v_type == VDIR);
	KASSERT(tdvp->v_type == VDIR);
	KASSERT(fdvp->v_mount == mp);
	KASSERT(fvp->v_mount == mp);
	KASSERT(tdvp->v_mount == mp);
	KASSERT((tvp == NULL) || (tvp->v_mount == mp));
	KASSERT(VOP_ISLOCKED(fdvp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(fvp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(tdvp) == LK_EXCLUSIVE);
	KASSERT((tvp == NULL) || (VOP_ISLOCKED(tvp) == LK_EXCLUSIVE));

	return genfs_ufslike_rename_check_permitted(cred,
	    fdvp, VTOI(fdvp)->i_mode, VTOI(fdvp)->i_uid,
	    fvp, VTOI(fvp)->i_uid,
	    tdvp, VTOI(tdvp)->i_mode, VTOI(tdvp)->i_uid,
	    tvp, (tvp? VTOI(tvp)->i_uid : 0));
}

/*
 * ulfs_gro_remove_check_possible: Check whether a remove is possible
 * independent of credentials.
 */
static int
ulfs_gro_remove_check_possible(struct mount *mp,
    struct vnode *dvp, struct vnode *vp)
{

	(void)mp;
	KASSERT(mp != NULL);
	KASSERT(dvp != NULL);
	KASSERT(vp != NULL);
	KASSERT(dvp != vp);
	KASSERT(dvp->v_type == VDIR);
	KASSERT(vp->v_type != VDIR);
	KASSERT(dvp->v_mount == mp);
	KASSERT(vp->v_mount == mp);
	KASSERT(VOP_ISLOCKED(dvp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(vp) == LK_EXCLUSIVE);

	return genfs_ufslike_remove_check_possible(
	    VTOI(dvp)->i_flags, VTOI(vp)->i_flags,
	    IMMUTABLE, APPEND);
}

/*
 * ulfs_gro_remove_check_permitted: Check whether a remove is permitted
 * given our credentials.
 */
static int
ulfs_gro_remove_check_permitted(struct mount *mp, kauth_cred_t cred,
    struct vnode *dvp, struct vnode *vp)
{

	(void)mp;
	KASSERT(mp != NULL);
	KASSERT(dvp != NULL);
	KASSERT(vp != NULL);
	KASSERT(dvp != vp);
	KASSERT(dvp->v_type == VDIR);
	KASSERT(vp->v_type != VDIR);
	KASSERT(dvp->v_mount == mp);
	KASSERT(vp->v_mount == mp);
	KASSERT(VOP_ISLOCKED(dvp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(vp) == LK_EXCLUSIVE);

	return genfs_ufslike_remove_check_permitted(cred,
	    dvp, VTOI(dvp)->i_mode, VTOI(dvp)->i_uid, vp, VTOI(vp)->i_uid);
}

/*
 * ulfs_rename_ulr_overlap_p: True iff tulr overlaps with fulr so that
 * entering a directory entry at tulr may move fulr.
 */
static bool
ulfs_rename_ulr_overlap_p(const struct ulfs_lookup_results *fulr,
    const struct ulfs_lookup_results *tulr)
{
	doff_t from_prev_start, from_prev_end, to_start, to_end;

	KASSERT(fulr != NULL);
	KASSERT(tulr != NULL);
	KASSERT(fulr != tulr);

	/*
	 * fulr is from a DELETE lookup, so fulr->ulr_count is the size
	 * of the preceding entry (d_reclen).
	 */
	from_prev_end = fulr->ulr_offset;
	KASSERT(fulr->ulr_count <= from_prev_end);
	from_prev_start = (from_prev_end - fulr->ulr_count);

	/*
	 * tulr is from a RENAME lookup, so tulr->ulr_count is the size
	 * of the free space for an entry that we are about to fill.
	 */
	to_start = tulr->ulr_offset;
	KASSERT(tulr->ulr_count < (LFS_MAXDIRSIZE - to_start));
	to_end = (to_start + tulr->ulr_count);

	return
	    (((to_start <= from_prev_start) && (from_prev_start < to_end)) ||
		((to_start <= from_prev_end) && (from_prev_end < to_end)));
}

/*
 * ulfs_direct_namlen: Return the namlen of the directory entry ep from
 * the directory vp.
 */
static int			/* XXX int?  uint8_t?  */
ulfs_direct_namlen(const LFS_DIRHEADER *ep, const struct vnode *vp)
{
	struct lfs *fs;

	KASSERT(ep != NULL);
	KASSERT(vp != NULL);
	KASSERT(VTOI(vp) != NULL);
	KASSERT(VTOI(vp)->i_ump != NULL);
	KASSERT(VTOI(vp)->i_lfs != NULL);
	fs = VTOI(vp)->i_lfs;

	return lfs_dir_getnamlen(fs, ep);
}

/*
 * ulfs_rename_recalculate_fulr: If we have just entered a directory into
 * dvp at tulr, and we were about to remove one at fulr for an entry
 * named fcnp, fulr may be invalid.  So, if necessary, recalculate it.
 */
static int
ulfs_rename_recalculate_fulr(struct vnode *dvp,
    struct ulfs_lookup_results *fulr, const struct ulfs_lookup_results *tulr,
    const struct componentname *fcnp)
{
	struct mount *mp;
	struct lfs *fs;
	struct ulfsmount *ump;
	/* XXX int is a silly type for this; blame ulfsmount::um_dirblksiz.  */
	int dirblksiz;
	doff_t search_start, search_end;
	doff_t offset;		/* Offset of entry we're examining.  */
	struct buf *bp;		/* I/O block we're examining.  */
	char *dirbuf;		/* Pointer into directory at search_start.  */
	LFS_DIRHEADER *ep;	/* Pointer to the entry we're examining.  */
	/* XXX direct::d_reclen is 16-bit;
	 * ulfs_lookup_results::ulr_reclen is 32-bit.  Blah.  */
	uint32_t reclen;	/* Length of the entry we're examining.  */
	uint32_t prev_reclen;	/* Length of the preceding entry.  */
	int error;

	KASSERT(dvp != NULL);
	KASSERT(dvp->v_mount != NULL);
	KASSERT(VTOI(dvp) != NULL);
	KASSERT(fulr != NULL);
	KASSERT(tulr != NULL);
	KASSERT(fulr != tulr);
	KASSERT(ulfs_rename_ulr_overlap_p(fulr, tulr));

	mp = dvp->v_mount;
	ump = VFSTOULFS(mp);
	fs = ump->um_lfs;
	KASSERT(ump != NULL);
	KASSERT(ump == VTOI(dvp)->i_ump);
	KASSERT(fs == VTOI(dvp)->i_lfs);

	dirblksiz = fs->um_dirblksiz;
	KASSERT(0 < dirblksiz);
	KASSERT((dirblksiz & (dirblksiz - 1)) == 0);

	/* A directory block may not span across multiple I/O blocks.  */
	KASSERT(dirblksiz <= mp->mnt_stat.f_iosize);

	/* Find the bounds of the search.  */
	search_start = tulr->ulr_offset;
	KASSERT(fulr->ulr_reclen < (LFS_MAXDIRSIZE - fulr->ulr_offset));
	search_end = (fulr->ulr_offset + fulr->ulr_reclen);

	/* Compaction must happen only within a directory block. (*)  */
	KASSERT(search_start <= search_end);
	KASSERT((search_end - (search_start &~ (dirblksiz - 1))) <= dirblksiz);

	dirbuf = NULL;
	bp = NULL;
	error = ulfs_blkatoff(dvp, (off_t)search_start, &dirbuf, &bp, false);
	if (error)
		return error;
	KASSERT(dirbuf != NULL);
	KASSERT(bp != NULL);

	/*
	 * Guarantee we sha'n't go past the end of the buffer we got.
	 * dirbuf is bp->b_data + (search_start & (iosize - 1)), and
	 * the valid range is [bp->b_data, bp->b_data + bp->b_bcount).
	 */
	KASSERT((search_end - search_start) <=
	    (bp->b_bcount - (search_start & (mp->mnt_stat.f_iosize - 1))));

	prev_reclen = fulr->ulr_count;
	offset = search_start;

	/*
	 * Search from search_start to search_end for the entry matching
	 * fcnp, which must be there because we found it before and it
	 * should only at most have moved earlier.
	 */
	for (;;) {
		KASSERT(search_start <= offset);
		KASSERT(offset < search_end);

		/*
		 * Examine the directory entry at offset.
		 */
		ep = (LFS_DIRHEADER *)(dirbuf + (offset - search_start));
		reclen = lfs_dir_getreclen(fs, ep);

		if (lfs_dir_getino(fs, ep) == 0)
			goto next;	/* Entry is unused.  */

		if (lfs_dir_getino(fs, ep) == ULFS_WINO)
			goto next;	/* Entry is whiteout.  */

		if (fcnp->cn_namelen != ulfs_direct_namlen(ep, dvp))
			goto next;	/* Wrong name length.  */

		if (memcmp(lfs_dir_nameptr(fs, ep), fcnp->cn_nameptr, fcnp->cn_namelen))
			goto next;	/* Wrong name.  */

		/* Got it!  */
		break;

next:
		if (! ((reclen < search_end) &&
			(offset < (search_end - reclen)))) {
			brelse(bp, 0);
			return EIO;	/* XXX Panic?  What?  */
		}

		/* We may not move past the search end.  */
		KASSERT(reclen < search_end);
		KASSERT(offset < (search_end - reclen));

		/*
		 * We may not move across a directory block boundary;
		 * see (*) above.
		 */
		KASSERT((offset &~ (dirblksiz - 1)) ==
		    ((offset + reclen) &~ (dirblksiz - 1)));

		prev_reclen = reclen;
		offset += reclen;
	}

	/*
	 * Found the entry.  Record where.
	 */
	fulr->ulr_offset = offset;
	fulr->ulr_reclen = reclen;

	/*
	 * Record the preceding record length, but not if we're at the
	 * start of a directory block.
	 */
	fulr->ulr_count = ((offset & (dirblksiz - 1))? prev_reclen : 0);

	brelse(bp, 0);
	return 0;
}

/*
 * ulfs_gro_remove: Rename an object over another link to itself,
 * effectively removing just the original link.
 */
static int
ulfs_gro_remove(struct mount *mp, kauth_cred_t cred,
    struct vnode *dvp, struct componentname *cnp, void *de, struct vnode *vp)
{
	struct ulfs_lookup_results *ulr = de;
	int error;

	KASSERT(mp != NULL);
	KASSERT(dvp != NULL);
	KASSERT(cnp != NULL);
	KASSERT(ulr != NULL);
	KASSERT(vp != NULL);
	KASSERT(dvp != vp);
	KASSERT(dvp->v_mount == mp);
	KASSERT(vp->v_mount == mp);
	KASSERT(dvp->v_type == VDIR);
	KASSERT(vp->v_type != VDIR);
	KASSERT(VOP_ISLOCKED(dvp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(vp) == LK_EXCLUSIVE);
	KASSERT(cnp->cn_nameiop == DELETE);

	/* XXX ulfs_dirremove decrements vp's link count for us.  */
	error = ulfs_dirremove(dvp, ulr, VTOI(vp), cnp->cn_flags, 0);
	if (error)
		goto out1;

	VN_KNOTE(dvp, NOTE_WRITE);
	VN_KNOTE(vp, (VTOI(vp)->i_nlink? NOTE_LINK : NOTE_DELETE));

out1:
	return error;
}

/*
 * ulfs_gro_lookup: Look up and save the lookup results.
 */
static int
ulfs_gro_lookup(struct mount *mp, struct vnode *dvp,
    struct componentname *cnp, void *de_ret, struct vnode **vp_ret)
{
	struct ulfs_lookup_results *ulr_ret = de_ret;
	struct vnode *vp = NULL;
	int error;

	(void)mp;
	KASSERT(mp != NULL);
	KASSERT(dvp != NULL);
	KASSERT(cnp != NULL);
	KASSERT(ulr_ret != NULL);
	KASSERT(vp_ret != NULL);
	KASSERT(VOP_ISLOCKED(dvp) == LK_EXCLUSIVE);

	/* Kludge cargo-culted from dholland's ulfs_rename.  */
	cnp->cn_flags &=~ MODMASK;
	cnp->cn_flags |= (LOCKPARENT | LOCKLEAF);

	error = relookup(dvp, &vp, cnp, 0 /* dummy */);
	if ((error == 0) && (vp == NULL)) {
		error = ENOENT;
		goto out;
	} else if (error) {
		return error;
	}

	/*
	 * Thanks to VFS insanity, relookup locks vp, which screws us
	 * in various ways.
	 */
	KASSERT(vp != NULL);
	VOP_UNLOCK(vp);

out:	*ulr_ret = VTOI(dvp)->i_crap;
	*vp_ret = vp;
	return error;
}

/*
 * ulfs_rmdired_p: Check whether the directory vp has been rmdired.
 *
 * vp must be locked and referenced.
 */
static bool
ulfs_rmdired_p(struct vnode *vp)
{

	KASSERT(vp != NULL);
	KASSERT(VOP_ISLOCKED(vp) == LK_EXCLUSIVE);
	KASSERT(vp->v_type == VDIR);

	/* XXX Is this correct?  */
	return (VTOI(vp)->i_size == 0);
}

/*
 * ulfs_read_dotdot: Store in *ino_ret the inode number of the parent
 * of the directory vp.
 */
static int
ulfs_read_dotdot(struct vnode *vp, kauth_cred_t cred, ino_t *ino_ret)
{
	struct lfs *fs;
	union lfs_dirtemplate dirbuf;
	LFS_DIRHEADER *dotdot;
	const char *name;
	int error;

	KASSERT(vp != NULL);
	KASSERT(ino_ret != NULL);
	KASSERT(vp->v_type == VDIR);

	KASSERT(VTOI(vp) != NULL);
	KASSERT(VTOI(vp)->i_lfs != NULL);
	fs = VTOI(vp)->i_lfs;

	error = ulfs_bufio(UIO_READ, vp, &dirbuf, sizeof dirbuf, (off_t)0,
	    IO_NODELOCKED, cred, NULL, NULL);
	if (error)
		return error;

	dotdot = lfs_dirtemplate_dotdot(fs, &dirbuf);
	name = lfs_dirtemplate_dotdotname(fs, &dirbuf);
	if (lfs_dir_getnamlen(fs, dotdot) != 2 ||
	    name[0] != '.' ||
	    name[1] != '.')
		/* XXX Panic?  Print warning?  */
		return ENOTDIR;

	*ino_ret = lfs_dir_getino(fs, dotdot);
	return 0;
}

/*
 * ulfs_gro_lock_directory: Lock the directory vp, but fail if it has
 * been rmdir'd.
 */
static int
ulfs_gro_lock_directory(struct mount *mp, struct vnode *vp)
{

	(void)mp;
	KASSERT(mp != NULL);
	KASSERT(vp != NULL);
	KASSERT(vp->v_mount == mp);

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);

	if (ulfs_rmdired_p(vp)) {
		VOP_UNLOCK(vp);
		return ENOENT;
	}

	return 0;
}

/*
 * ulfs_gro_genealogy: Analyze the genealogy of the source and target
 * directories.
 */
static int
ulfs_gro_genealogy(struct mount *mp, kauth_cred_t cred,
    struct vnode *fdvp, struct vnode *tdvp,
    struct vnode **intermediate_node_ret)
{
	struct vnode *vp, *dvp;
	ino_t dotdot_ino = -1;	/* XXX  gcc 4.8: maybe-uninitialized */
	int error;

	KASSERT(mp != NULL);
	KASSERT(fdvp != NULL);
	KASSERT(tdvp != NULL);
	KASSERT(fdvp != tdvp);
	KASSERT(intermediate_node_ret != NULL);
	KASSERT(fdvp->v_mount == mp);
	KASSERT(tdvp->v_mount == mp);
	KASSERT(fdvp->v_type == VDIR);
	KASSERT(tdvp->v_type == VDIR);

	/*
	 * We need to provisionally lock tdvp to keep rmdir from
	 * deleting it -- or any ancestor -- at an inopportune moment.
	 */
	error = ulfs_gro_lock_directory(mp, tdvp);
	if (error)
		return error;

	vp = tdvp;
	vref(vp);

	for (;;) {
		KASSERT(vp != NULL);
		KASSERT(VOP_ISLOCKED(vp) == LK_EXCLUSIVE);
		KASSERT(vp->v_mount == mp);
		KASSERT(vp->v_type == VDIR);
		KASSERT(!ulfs_rmdired_p(vp));

		/* Did we hit the root without finding fdvp?  */
		if (VTOI(vp)->i_number == ULFS_ROOTINO) {
			vput(vp);
			*intermediate_node_ret = NULL;
			return 0;
		}

		error = ulfs_read_dotdot(vp, cred, &dotdot_ino);
		if (error) {
			vput(vp);
			return error;
		}

		/* Did we find that fdvp is an ancestor of tdvp?  */
		if (VTOI(fdvp)->i_number == dotdot_ino) {
			/* Unlock vp, but keep it referenced.  */
			VOP_UNLOCK(vp);
			*intermediate_node_ret = vp;
			return 0;
		}

		/* Neither -- keep ascending the family tree.  */

		/*
		 * Unlock vp so that we can lock the parent, but keep
		 * vp referenced until after we have found the parent,
		 * so that dotdot_ino will not be recycled.
		 *
		 * XXX This guarantees that vp's inode number will not
		 * be recycled, but why can't dotdot_ino be recycled?
		 */
		VOP_UNLOCK(vp);
		error = VFS_VGET(mp, dotdot_ino, &dvp);
		vrele(vp);
		if (error)
			return error;

		KASSERT(dvp != NULL);
		KASSERT(VOP_ISLOCKED(dvp) == LK_EXCLUSIVE);
		vp = dvp;

		if (vp->v_type != VDIR) {
			/*
			 * XXX Panic?  Print a warning?  Can this
			 * happen if we lose the race I suspect to
			 * exist above, and the `..' inode number has
			 * been recycled?
			 */
			vput(vp);
			return ENOTDIR;
		}

		if (ulfs_rmdired_p(vp)) {
			vput(vp);
			return ENOENT;
		}
	}
}

/*
 * ulfs_gro_rename: Actually perform the rename operation.
 */
static int
ulfs_gro_rename(struct mount *mp, kauth_cred_t cred,
    struct vnode *fdvp, struct componentname *fcnp,
    void *fde, struct vnode *fvp,
    struct vnode *tdvp, struct componentname *tcnp,
    void *tde, struct vnode *tvp)
{
	struct lfs *fs;
	struct ulfs_lookup_results *fulr = fde;
	struct ulfs_lookup_results *tulr = tde;
	bool directory_p, reparent_p;
	int error;

	KASSERT(mp != NULL);
	KASSERT(fdvp != NULL);
	KASSERT(fcnp != NULL);
	KASSERT(fulr != NULL);
	KASSERT(fvp != NULL);
	KASSERT(tdvp != NULL);
	KASSERT(tcnp != NULL);
	KASSERT(tulr != NULL);
	KASSERT(fulr != tulr);
	KASSERT(fdvp != fvp);
	KASSERT(fdvp != tvp);
	KASSERT(tdvp != fvp);
	KASSERT(tdvp != tvp);
	KASSERT(fvp != tvp);
	KASSERT(fdvp->v_mount == mp);
	KASSERT(fvp->v_mount == mp);
	KASSERT(tdvp->v_mount == mp);
	KASSERT((tvp == NULL) || (tvp->v_mount == mp));
	KASSERT(VOP_ISLOCKED(fdvp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(fvp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(tdvp) == LK_EXCLUSIVE);
	KASSERT((tvp == NULL) || (VOP_ISLOCKED(tvp) == LK_EXCLUSIVE));

	fs = VTOI(fdvp)->i_lfs;
	KASSERT(fs == VTOI(tdvp)->i_lfs);

	/*
	 * We shall need to temporarily bump the link count, so make
	 * sure there is room to do so.
	 */
	if ((nlink_t)VTOI(fvp)->i_nlink >= LINK_MAX)
		return EMLINK;

	directory_p = (fvp->v_type == VDIR);
	KASSERT(directory_p == ((VTOI(fvp)->i_mode & LFS_IFMT) == LFS_IFDIR));
	KASSERT((tvp == NULL) || (directory_p == (tvp->v_type == VDIR)));
	KASSERT((tvp == NULL) || (directory_p ==
		((VTOI(tvp)->i_mode & LFS_IFMT) == LFS_IFDIR)));

	reparent_p = (fdvp != tdvp);
	KASSERT(reparent_p == (VTOI(fdvp)->i_number != VTOI(tdvp)->i_number));

	/*
	 * Commence hacking of the data on disk.
	 */

	error = 0;

	/*
	 * 1) Bump link count while we're moving stuff
	 *    around.  If we crash somewhere before
	 *    completing our work, the link count
	 *    may be wrong, but correctable.
	 */

	KASSERT((nlink_t)VTOI(fvp)->i_nlink < LINK_MAX);
	VTOI(fvp)->i_nlink++;
	DIP_ASSIGN(VTOI(fvp), nlink, VTOI(fvp)->i_nlink);
	VTOI(fvp)->i_flag |= IN_CHANGE;
	error = lfs_update(fvp, NULL, NULL, UPDATE_DIROP);
	if (error)
		goto whymustithurtsomuch;

	/*
	 * 2) If target doesn't exist, link the target
	 *    to the source and unlink the source.
	 *    Otherwise, rewrite the target directory
	 *    entry to reference the source inode and
	 *    expunge the original entry's existence.
	 */

	if (tvp == NULL) {
		/*
		 * Account for ".." in new directory.
		 * When source and destination have the same
		 * parent we don't fool with the link count.
		 */
		if (directory_p && reparent_p) {
			if ((nlink_t)VTOI(tdvp)->i_nlink >= LINK_MAX) {
				error = EMLINK;
				goto whymustithurtsomuch;
			}
			KASSERT((nlink_t)VTOI(tdvp)->i_nlink < LINK_MAX);
			VTOI(tdvp)->i_nlink++;
			DIP_ASSIGN(VTOI(tdvp), nlink, VTOI(tdvp)->i_nlink);
			VTOI(tdvp)->i_flag |= IN_CHANGE;
			error = lfs_update(tdvp, NULL, NULL, UPDATE_DIROP);
			if (error) {
				/*
				 * Link count update didn't take --
				 * back out the in-memory link count.
				 */
				KASSERT(0 < VTOI(tdvp)->i_nlink);
				VTOI(tdvp)->i_nlink--;
				DIP_ASSIGN(VTOI(tdvp), nlink,
				    VTOI(tdvp)->i_nlink);
				VTOI(tdvp)->i_flag |= IN_CHANGE;
				goto whymustithurtsomuch;
			}
		}

		error = ulfs_direnter(tdvp, tulr,
		    NULL, tcnp, VTOI(fvp)->i_number, LFS_IFTODT(VTOI(fvp)->i_mode),
		    NULL);
		if (error) {
			if (directory_p && reparent_p) {
				/*
				 * Directory update didn't take, but
				 * the link count update did -- back
				 * out the in-memory link count and the
				 * on-disk link count.
				 */
				KASSERT(0 < VTOI(tdvp)->i_nlink);
				VTOI(tdvp)->i_nlink--;
				DIP_ASSIGN(VTOI(tdvp), nlink,
				    VTOI(tdvp)->i_nlink);
				VTOI(tdvp)->i_flag |= IN_CHANGE;
				(void)lfs_update(tdvp, NULL, NULL,
				    UPDATE_WAIT | UPDATE_DIROP);
			}
			goto whymustithurtsomuch;
		}
	} else {
		if (directory_p)
			/* XXX WTF?  Why purge here?  Why not purge others?  */
			cache_purge(tdvp);

		/*
		 * Make the target directory's entry for tcnp point at
		 * the source node.
		 *
		 * XXX ulfs_dirrewrite decrements tvp's link count, but
		 * doesn't touch the link count of the new inode.  Go
		 * figure.
		 */
		error = ulfs_dirrewrite(VTOI(tdvp), tulr->ulr_offset,
		    VTOI(tvp), VTOI(fvp)->i_number, LFS_IFTODT(VTOI(fvp)->i_mode),
		    ((directory_p && reparent_p) ? reparent_p : directory_p),
		    IN_CHANGE | IN_UPDATE);
		if (error)
			goto whymustithurtsomuch;

		/*
		 * If the source and target are directories, and the
		 * target is in the same directory as the source,
		 * decrement the link count of the common parent
		 * directory, since we are removing the target from
		 * that directory.
		 */
		if (directory_p && !reparent_p) {
			KASSERT(fdvp == tdvp);
			/* XXX check, don't kassert */
			KASSERT(0 < VTOI(tdvp)->i_nlink);
			VTOI(tdvp)->i_nlink--;
			DIP_ASSIGN(VTOI(tdvp), nlink, VTOI(tdvp)->i_nlink);
			VTOI(tdvp)->i_flag |= IN_CHANGE;
		}

		if (directory_p) {
			/*
			 * XXX I don't understand the following comment
			 * from ulfs_rename -- in particular, the part
			 * about `there may be other hard links'.
			 *
			 * Truncate inode. The only stuff left in the directory
			 * is "." and "..". The "." reference is inconsequential
			 * since we are quashing it. We have removed the "."
			 * reference and the reference in the parent directory,
			 * but there may be other hard links.
			 *
			 * XXX The ulfs_dirempty call earlier does
			 * not guarantee anything about nlink.
			 */
			if (VTOI(tvp)->i_nlink != 1)
				ulfs_dirbad(VTOI(tvp), (doff_t)0,
				    "hard-linked directory");
			VTOI(tvp)->i_nlink = 0;
			DIP_ASSIGN(VTOI(tvp), nlink, 0);
			error = lfs_truncate(tvp, (off_t)0, IO_SYNC, cred);
			if (error)
				goto whymustithurtsomuch;
		}
	}

	/*
	 * If the source is a directory with a new parent, the link
	 * count of the old parent directory must be decremented and
	 * ".." set to point to the new parent.
	 *
	 * XXX ulfs_dirrewrite updates the link count of fdvp, but not
	 * the link count of fvp or the link count of tdvp.  Go figure.
	 */
	if (directory_p && reparent_p) {
		off_t position;

		/*
		 * The .. entry goes immediately after the . entry, so
		 * the position is the record length of the . entry,
		 * namely LFS_DIRECTSIZ(1).
		 */
		position = LFS_DIRECTSIZ(fs, 1);
		error = ulfs_dirrewrite(VTOI(fvp), position,
		    VTOI(fdvp), VTOI(tdvp)->i_number, LFS_DT_DIR, 0, IN_CHANGE);
#if 0		/* XXX This branch was not in ulfs_rename! */
		if (error)
			goto whymustithurtsomuch;
#endif

		/* XXX WTF?  Why purge here?  Why not purge others?  */
		cache_purge(fdvp);
	}

	/*
	 * 3) Unlink the source.
	 */

	/*
	 * ulfs_direnter may compact the directory in the process of
	 * inserting a new entry.  That may invalidate fulr, which we
	 * need in order to remove the old entry.  In that case, we
	 * need to recalculate what fulr should be.
	 */
	if (!reparent_p && (tvp == NULL) &&
	    ulfs_rename_ulr_overlap_p(fulr, tulr)) {
		error = ulfs_rename_recalculate_fulr(fdvp, fulr, tulr, fcnp);
#if 0				/* XXX */
		if (error)	/* XXX Try to back out changes?  */
			goto whymustithurtsomuch;
#endif
	}

	/*
	 * XXX 0 means !isrmdir.  But can't this be an rmdir?
	 * XXX Well, turns out that argument to ulfs_dirremove is ignored...
	 * XXX And it turns out ulfs_dirremove updates the link count of fvp.
	 * XXX But it doesn't update the link count of fdvp.  Go figure.
	 * XXX fdvp's link count is updated in ulfs_dirrewrite instead.
	 * XXX Actually, sometimes it doesn't update fvp's link count.
	 * XXX I hate the world.
	 */
	error = ulfs_dirremove(fdvp, fulr, VTOI(fvp), fcnp->cn_flags, 0);
	if (error)
#if 0				/* XXX */
		goto whymustithurtsomuch;
#endif
		goto arghmybrainhurts;

	/*
	 * XXX Perhaps this should go at the top, in case the file
	 * system is modified but incompletely so because of an
	 * intermediate error.
	 */
	genfs_rename_knote(fdvp, fvp, tdvp, tvp,
	    ((tvp != NULL) && (VTOI(tvp)->i_nlink == 0)));
#if 0				/* XXX */
	genfs_rename_cache_purge(fdvp, fvp, tdvp, tvp);
#endif
	goto arghmybrainhurts;

whymustithurtsomuch:
	KASSERT(0 < VTOI(fvp)->i_nlink);
	VTOI(fvp)->i_nlink--;
	DIP_ASSIGN(VTOI(fvp), nlink, VTOI(fvp)->i_nlink);
	VTOI(fvp)->i_flag |= IN_CHANGE;

arghmybrainhurts:
/*ihateyou:*/
	return error;
}

/*
 * lfs_gro_rename: Actually perform the rename operation.  Do a little
 * LFS bookkeeping and then defer to ulfs_gro_rename.
 */
static int
lfs_gro_rename(struct mount *mp, kauth_cred_t cred,
    struct vnode *fdvp, struct componentname *fcnp,
    void *fde, struct vnode *fvp,
    struct vnode *tdvp, struct componentname *tcnp,
    void *tde, struct vnode *tvp)
{
	int error;

	KASSERT(mp != NULL);
	KASSERT(fdvp != NULL);
	KASSERT(fcnp != NULL);
	KASSERT(fde != NULL);
	KASSERT(fvp != NULL);
	KASSERT(tdvp != NULL);
	KASSERT(tcnp != NULL);
	KASSERT(tde != NULL);
	KASSERT(fdvp != fvp);
	KASSERT(fdvp != tvp);
	KASSERT(tdvp != fvp);
	KASSERT(tdvp != tvp);
	KASSERT(fvp != tvp);
	KASSERT(fdvp->v_mount == mp);
	KASSERT(fvp->v_mount == mp);
	KASSERT(tdvp->v_mount == mp);
	KASSERT((tvp == NULL) || (tvp->v_mount == mp));
	KASSERT(VOP_ISLOCKED(fdvp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(fvp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(tdvp) == LK_EXCLUSIVE);
	KASSERT((tvp == NULL) || (VOP_ISLOCKED(tvp) == LK_EXCLUSIVE));

	error = lfs_set_dirop(tdvp, tvp);
	if (error != 0)
		return error;

	MARK_VNODE(fdvp);
	MARK_VNODE(fvp);

	error = ulfs_gro_rename(mp, cred,
	    fdvp, fcnp, fde, fvp,
	    tdvp, tcnp, tde, tvp);

	UNMARK_VNODE(fdvp);
	UNMARK_VNODE(fvp);
	UNMARK_VNODE(tdvp);
	if (tvp) {
		UNMARK_VNODE(tvp);
	}
	lfs_unset_dirop(VFSTOULFS(mp)->um_lfs, tdvp, "rename");
	vrele(tdvp);
	if (tvp) {
		vrele(tvp);
	}

	return error;
}

static const struct genfs_rename_ops lfs_genfs_rename_ops = {
	.gro_directory_empty_p		= ulfs_gro_directory_empty_p,
	.gro_rename_check_possible	= ulfs_gro_rename_check_possible,
	.gro_rename_check_permitted	= ulfs_gro_rename_check_permitted,
	.gro_remove_check_possible	= ulfs_gro_remove_check_possible,
	.gro_remove_check_permitted	= ulfs_gro_remove_check_permitted,
	.gro_rename			= lfs_gro_rename,
	.gro_remove			= ulfs_gro_remove,
	.gro_lookup			= ulfs_gro_lookup,
	.gro_genealogy			= ulfs_gro_genealogy,
	.gro_lock_directory		= ulfs_gro_lock_directory,
};

/*
 * lfs_sane_rename: The hairiest vop, with the saner API.
 *
 * Arguments:
 *
 * . fdvp (from directory vnode),
 * . fcnp (from component name),
 * . tdvp (to directory vnode),
 * . tcnp (to component name),
 * . cred (credentials structure), and
 * . posixly_correct (flag for behaviour if target & source link same file).
 *
 * fdvp and tdvp may be the same, and must be referenced and unlocked.
 */
static int
lfs_sane_rename(
    struct vnode *fdvp, struct componentname *fcnp,
    struct vnode *tdvp, struct componentname *tcnp,
    kauth_cred_t cred, bool posixly_correct)
{
	struct ulfs_lookup_results fulr, tulr;

	/*
	 * XXX Provisional kludge -- ulfs_lookup does not reject rename
	 * of . or .. (from or to), so we hack it here.  This is not
	 * the right place: it should be caller's responsibility to
	 * reject this case.
	 */
	KASSERT(fcnp != NULL);
	KASSERT(tcnp != NULL);
	KASSERT(fcnp != tcnp);
	KASSERT(fcnp->cn_nameptr != NULL);
	KASSERT(tcnp->cn_nameptr != NULL);

	if ((fcnp->cn_flags | tcnp->cn_flags) & ISDOTDOT)
		return EINVAL;	/* XXX EISDIR?  */
	if ((fcnp->cn_namelen == 1) && (fcnp->cn_nameptr[0] == '.'))
		return EINVAL;
	if ((tcnp->cn_namelen == 1) && (tcnp->cn_nameptr[0] == '.'))
		return EINVAL;

	return genfs_sane_rename(&lfs_genfs_rename_ops,
	    fdvp, fcnp, &fulr, tdvp, tcnp, &tulr,
	    cred, posixly_correct);
}

/*
 * lfs_rename: The hairiest vop, with the insanest API.  Defer to
 * genfs_insane_rename immediately.
 */
int
lfs_rename(void *v)
{

	return genfs_insane_rename(v, &lfs_sane_rename);
}

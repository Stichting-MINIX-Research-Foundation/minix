/*	$NetBSD: ufs_rename.c,v 1.12 2015/03/27 17:27:56 riastradh Exp $	*/

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

/*
 * UFS Rename
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ufs_rename.c,v 1.12 2015/03/27 17:27:56 riastradh Exp $");

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/kauth.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/pool.h>
#include <sys/vnode.h>
#include <sys/vnode_if.h>
#include <sys/wapbl.h>

#include <miscfs/genfs/genfs.h>

#include <ufs/ufs/dir.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufs_bswap.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ufs/ufs_wapbl.h>
#include <ufs/ufs/ufsmount.h>

/*
 * Forward declarations
 */

static int ufs_sane_rename(struct vnode *, struct componentname *,
    struct vnode *, struct componentname *,
    kauth_cred_t, bool);
static bool ufs_rename_ulr_overlap_p(const struct ufs_lookup_results *,
    const struct ufs_lookup_results *);
static int ufs_rename_recalculate_fulr(struct vnode *,
    struct ufs_lookup_results *, const struct ufs_lookup_results *,
    const struct componentname *);
static int ufs_direct_namlen(const struct direct *, const struct vnode *);
static int ufs_read_dotdot(struct vnode *, kauth_cred_t, ino_t *);
static int ufs_dirbuf_dotdot_namlen(const struct dirtemplate *,
    const struct vnode *);

static const struct genfs_rename_ops ufs_genfs_rename_ops;

/*
 * ufs_sane_rename: The hairiest vop, with the saner API.
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
ufs_sane_rename(
    struct vnode *fdvp, struct componentname *fcnp,
    struct vnode *tdvp, struct componentname *tcnp,
    kauth_cred_t cred, bool posixly_correct)
{
	struct ufs_lookup_results fulr, tulr;

	return genfs_sane_rename(&ufs_genfs_rename_ops,
	    fdvp, fcnp, &fulr, tdvp, tcnp, &tulr,
	    cred, posixly_correct);
}

/*
 * ufs_rename: The hairiest vop, with the insanest API.  Defer to
 * genfs_insane_rename immediately.
 */
int
ufs_rename(void *v)
{

	return genfs_insane_rename(v, &ufs_sane_rename);
}

/*
 * ufs_gro_directory_empty_p: Return true if the directory vp is
 * empty.  dvp is its parent.
 *
 * vp and dvp must be locked and referenced.
 */
bool
ufs_gro_directory_empty_p(struct mount *mp, kauth_cred_t cred,
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

	return ufs_dirempty(VTOI(vp), VTOI(dvp)->i_number, cred);
}

/*
 * ufs_gro_rename_check_possible: Check whether a rename is possible
 * independent of credentials.
 */
int
ufs_gro_rename_check_possible(struct mount *mp,
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
 * ufs_gro_rename_check_permitted: Check whether a rename is permitted
 * given our credentials.
 */
int
ufs_gro_rename_check_permitted(struct mount *mp, kauth_cred_t cred,
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
 * ufs_gro_remove_check_possible: Check whether a remove is possible
 * independent of credentials.
 */
int
ufs_gro_remove_check_possible(struct mount *mp,
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
 * ufs_gro_remove_check_permitted: Check whether a remove is permitted
 * given our credentials.
 */
int
ufs_gro_remove_check_permitted(struct mount *mp, kauth_cred_t cred,
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
 * A virgin directory (no blushing please).
 *
 * XXX Copypasta from ufs_vnops.c.  Kill!
 */
static const struct dirtemplate mastertemplate = {
	0,	12,			DT_DIR,	1,	".",
	0,	UFS_DIRBLKSIZ - 12,	DT_DIR,	2,	".."
};

/*
 * ufs_gro_rename: Actually perform the rename operation.
 */
int
ufs_gro_rename(struct mount *mp, kauth_cred_t cred,
    struct vnode *fdvp, struct componentname *fcnp,
    void *fde, struct vnode *fvp,
    struct vnode *tdvp, struct componentname *tcnp,
    void *tde, struct vnode *tvp)
{
	struct ufs_lookup_results *fulr = fde;
	struct ufs_lookup_results *tulr = tde;
	bool directory_p, reparent_p;
	struct direct *newdir;
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

	/*
	 * We shall need to temporarily bump the link count, so make
	 * sure there is room to do so.
	 */
	if ((nlink_t)VTOI(fvp)->i_nlink >= LINK_MAX)
		return EMLINK;

	directory_p = (fvp->v_type == VDIR);
	KASSERT(directory_p == ((VTOI(fvp)->i_mode & IFMT) == IFDIR));
	KASSERT((tvp == NULL) || (directory_p == (tvp->v_type == VDIR)));
	KASSERT((tvp == NULL) || (directory_p ==
		((VTOI(tvp)->i_mode & IFMT) == IFDIR)));

	reparent_p = (fdvp != tdvp);
	KASSERT(reparent_p == (VTOI(fdvp)->i_number != VTOI(tdvp)->i_number));

	/*
	 * Commence hacking of the data on disk.
	 */

	error = UFS_WAPBL_BEGIN(mp);
	if (error)
		goto ihateyou;

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
	error = UFS_UPDATE(fvp, NULL, NULL, UPDATE_DIROP);
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
			error = UFS_UPDATE(tdvp, NULL, NULL, UPDATE_DIROP);
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

		newdir = pool_cache_get(ufs_direct_cache, PR_WAITOK);
		ufs_makedirentry(VTOI(fvp), tcnp, newdir);
		error = ufs_direnter(tdvp, tulr, NULL, newdir, tcnp, NULL);
		pool_cache_put(ufs_direct_cache, newdir);
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
				(void)UFS_UPDATE(tdvp, NULL, NULL,
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
		 * XXX ufs_dirrewrite decrements tvp's link count, but
		 * doesn't touch the link count of the new inode.  Go
		 * figure.
		 */
		error = ufs_dirrewrite(VTOI(tdvp), tulr->ulr_offset,
		    VTOI(tvp), VTOI(fvp)->i_number, IFTODT(VTOI(fvp)->i_mode),
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
			UFS_WAPBL_UPDATE(tdvp, NULL, NULL, 0);
		}

		if (directory_p) {
			/*
			 * XXX I don't understand the following comment
			 * from ufs_rename -- in particular, the part
			 * about `there may be other hard links'.
			 *
			 * Truncate inode. The only stuff left in the directory
			 * is "." and "..". The "." reference is inconsequential
			 * since we are quashing it. We have removed the "."
			 * reference and the reference in the parent directory,
			 * but there may be other hard links.
			 *
			 * XXX The ufs_dirempty call earlier does
			 * not guarantee anything about nlink.
			 */
			if (VTOI(tvp)->i_nlink != 1)
				ufs_dirbad(VTOI(tvp), (doff_t)0,
				    "hard-linked directory");
			VTOI(tvp)->i_nlink = 0;
			DIP_ASSIGN(VTOI(tvp), nlink, 0);
			error = UFS_TRUNCATE(tvp, (off_t)0, IO_SYNC, cred);
			if (error)
				goto whymustithurtsomuch;
		}
	}

	/*
	 * If the source is a directory with a new parent, the link
	 * count of the old parent directory must be decremented and
	 * ".." set to point to the new parent.
	 *
	 * XXX ufs_dirrewrite updates the link count of fdvp, but not
	 * the link count of fvp or the link count of tdvp.  Go figure.
	 */
	if (directory_p && reparent_p) {
		error = ufs_dirrewrite(VTOI(fvp), mastertemplate.dot_reclen,
		    VTOI(fdvp), VTOI(tdvp)->i_number, DT_DIR, 0, IN_CHANGE);
#if 0		/* XXX This branch was not in ufs_rename! */
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
	 * ufs_direnter may compact the directory in the process of
	 * inserting a new entry.  That may invalidate fulr, which we
	 * need in order to remove the old entry.  In that case, we
	 * need to recalculate what fulr should be.
	 */
	if (!reparent_p && (tvp == NULL) &&
	    ufs_rename_ulr_overlap_p(fulr, tulr)) {
		error = ufs_rename_recalculate_fulr(fdvp, fulr, tulr, fcnp);
#if 0				/* XXX */
		if (error)	/* XXX Try to back out changes?  */
			goto whymustithurtsomuch;
#endif
	}

	/*
	 * XXX 0 means !isrmdir.  But can't this be an rmdir?
	 * XXX Well, turns out that argument to ufs_dirremove is ignored...
	 * XXX And it turns out ufs_dirremove updates the link count of fvp.
	 * XXX But it doesn't update the link count of fdvp.  Go figure.
	 * XXX fdvp's link count is updated in ufs_dirrewrite instead.
	 * XXX Actually, sometimes it doesn't update fvp's link count.
	 * XXX I hate the world.
	 */
	error = ufs_dirremove(fdvp, fulr, VTOI(fvp), fcnp->cn_flags, 0);
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
	UFS_WAPBL_UPDATE(fvp, NULL, NULL, 0);

arghmybrainhurts:
	UFS_WAPBL_END(mp);

ihateyou:
	return error;
}

/*
 * ufs_rename_ulr_overlap_p: True iff tulr overlaps with fulr so that
 * entering a directory entry at tulr may move fulr.
 */
static bool
ufs_rename_ulr_overlap_p(const struct ufs_lookup_results *fulr,
    const struct ufs_lookup_results *tulr)
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
	KASSERT(tulr->ulr_count < (UFS_MAXDIRSIZE - to_start));
	to_end = (to_start + tulr->ulr_count);

	return
	    (((to_start <= from_prev_start) && (from_prev_start < to_end)) ||
		((to_start <= from_prev_end) && (from_prev_end < to_end)));
}

/*
 * ufs_rename_recalculate_fulr: If we have just entered a directory into
 * dvp at tulr, and we were about to remove one at fulr for an entry
 * named fcnp, fulr may be invalid.  So, if necessary, recalculate it.
 */
static int
ufs_rename_recalculate_fulr(struct vnode *dvp,
    struct ufs_lookup_results *fulr, const struct ufs_lookup_results *tulr,
    const struct componentname *fcnp)
{
	struct mount *mp;
	struct ufsmount *ump;
	int needswap;
	/* XXX int is a silly type for this; blame ufsmount::um_dirblksiz.  */
	int dirblksiz;
	doff_t search_start, search_end;
	doff_t offset;		/* Offset of entry we're examining.  */
	struct buf *bp;		/* I/O block we're examining.  */
	char *dirbuf;		/* Pointer into directory at search_start.  */
	struct direct *ep;	/* Pointer to the entry we're examining.  */
	/* XXX direct::d_reclen is 16-bit;
	 * ufs_lookup_results::ulr_reclen is 32-bit.  Blah.  */
	uint32_t reclen;	/* Length of the entry we're examining.  */
	uint32_t prev_reclen;	/* Length of the preceding entry.  */
	int error;

	KASSERT(dvp != NULL);
	KASSERT(dvp->v_mount != NULL);
	KASSERT(VTOI(dvp) != NULL);
	KASSERT(fulr != NULL);
	KASSERT(tulr != NULL);
	KASSERT(fulr != tulr);
	KASSERT(ufs_rename_ulr_overlap_p(fulr, tulr));

	mp = dvp->v_mount;
	ump = VFSTOUFS(mp);
	KASSERT(ump != NULL);
	KASSERT(ump == VTOI(dvp)->i_ump);

	needswap = UFS_MPNEEDSWAP(ump);

	dirblksiz = ump->um_dirblksiz;
	KASSERT(0 < dirblksiz);
	KASSERT((dirblksiz & (dirblksiz - 1)) == 0);

	/* A directory block may not span across multiple I/O blocks.  */
	KASSERT(dirblksiz <= mp->mnt_stat.f_iosize);

	/* Find the bounds of the search.  */
	search_start = tulr->ulr_offset;
	KASSERT(fulr->ulr_reclen < (UFS_MAXDIRSIZE - fulr->ulr_offset));
	search_end = (fulr->ulr_offset + fulr->ulr_reclen);

	/* Compaction must happen only within a directory block. (*)  */
	KASSERT(search_start <= search_end);
	KASSERT((search_end - (search_start &~ (dirblksiz - 1))) <= dirblksiz);

	dirbuf = NULL;
	bp = NULL;
	error = ufs_blkatoff(dvp, (off_t)search_start, &dirbuf, &bp, false);
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
		ep = (struct direct *)(dirbuf + (offset - search_start));
		reclen = ufs_rw16(ep->d_reclen, needswap);

		if (ep->d_ino == 0)
			goto next;	/* Entry is unused.  */

		if (ufs_rw32(ep->d_ino, needswap) == UFS_WINO)
			goto next;	/* Entry is whiteout.  */

		if (fcnp->cn_namelen != ufs_direct_namlen(ep, dvp))
			goto next;	/* Wrong name length.  */

		if (memcmp(ep->d_name, fcnp->cn_nameptr, fcnp->cn_namelen))
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
 * ufs_direct_namlen: Return the namlen of the directory entry ep from
 * the directory vp.
 */
static int			/* XXX int?  uint8_t?  */
ufs_direct_namlen(const struct direct *ep, const struct vnode *vp)
{
	bool swap;

	KASSERT(ep != NULL);
	KASSERT(vp != NULL);
	KASSERT(VTOI(vp) != NULL);
	KASSERT(VTOI(vp)->i_ump != NULL);

#if (BYTE_ORDER == LITTLE_ENDIAN)
	swap = (UFS_MPNEEDSWAP(VTOI(vp)->i_ump) == 0);
#else
	swap = (UFS_MPNEEDSWAP(VTOI(vp)->i_ump) != 0);
#endif

	return ((FSFMT(vp) && swap)? ep->d_type : ep->d_namlen);
}

/*
 * ufs_gro_remove: Rename an object over another link to itself,
 * effectively removing just the original link.
 */
int
ufs_gro_remove(struct mount *mp, kauth_cred_t cred,
    struct vnode *dvp, struct componentname *cnp, void *de, struct vnode *vp)
{
	struct ufs_lookup_results *ulr = de;
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

	error = UFS_WAPBL_BEGIN(mp);
	if (error)
		goto out0;

	/* XXX ufs_dirremove decrements vp's link count for us.  */
	error = ufs_dirremove(dvp, ulr, VTOI(vp), cnp->cn_flags, 0);
	if (error)
		goto out1;

	VN_KNOTE(dvp, NOTE_WRITE);
	VN_KNOTE(vp, (VTOI(vp)->i_nlink? NOTE_LINK : NOTE_DELETE));

out1:	UFS_WAPBL_END(mp);
out0:
	return error;
}

/*
 * ufs_gro_lookup: Look up and save the lookup results.
 */
int
ufs_gro_lookup(struct mount *mp, struct vnode *dvp,
    struct componentname *cnp, void *de_ret, struct vnode **vp_ret)
{
	struct ufs_lookup_results *ulr_ret = de_ret;
	struct vnode *vp = NULL;
	int error;

	(void)mp;
	KASSERT(mp != NULL);
	KASSERT(dvp != NULL);
	KASSERT(cnp != NULL);
	KASSERT(ulr_ret != NULL);
	KASSERT(vp_ret != NULL);
	KASSERT(VOP_ISLOCKED(dvp) == LK_EXCLUSIVE);

	/* Kludge cargo-culted from dholland's ufs_rename.  */
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
 * ufs_rmdired_p: Check whether the directory vp has been rmdired.
 *
 * vp must be locked and referenced.
 */
static bool
ufs_rmdired_p(struct vnode *vp)
{

	KASSERT(vp != NULL);
	KASSERT(VOP_ISLOCKED(vp) == LK_EXCLUSIVE);
	KASSERT(vp->v_type == VDIR);

	/* XXX Is this correct?  */
	return (VTOI(vp)->i_size == 0);
}

/*
 * ufs_read_dotdot: Store in *ino_ret the inode number of the parent
 * of the directory vp.
 */
static int
ufs_read_dotdot(struct vnode *vp, kauth_cred_t cred, ino_t *ino_ret)
{
	struct dirtemplate dirbuf;
	int error;

	KASSERT(vp != NULL);
	KASSERT(ino_ret != NULL);
	KASSERT(vp->v_type == VDIR);

	error = ufs_bufio(UIO_READ, vp, &dirbuf, sizeof dirbuf, (off_t)0,
	    IO_NODELOCKED, cred, NULL, NULL);
	if (error)
		return error;

	if (ufs_dirbuf_dotdot_namlen(&dirbuf, vp) != 2 ||
	    dirbuf.dotdot_name[0] != '.' ||
	    dirbuf.dotdot_name[1] != '.')
		/* XXX Panic?  Print warning?  */
		return ENOTDIR;

	*ino_ret = ufs_rw32(dirbuf.dotdot_ino,
	    UFS_MPNEEDSWAP(VTOI(vp)->i_ump));
	return 0;
}

/*
 * ufs_dirbuf_dotdot_namlen: Return the namlen of the directory buffer
 * dirbuf that came from the directory vp.  Swap byte order if
 * necessary.
 */
static int			/* XXX int?  uint8_t?  */
ufs_dirbuf_dotdot_namlen(const struct dirtemplate *dirbuf,
    const struct vnode *vp)
{
	bool swap;

	KASSERT(dirbuf != NULL);
	KASSERT(vp != NULL);
	KASSERT(VTOI(vp) != NULL);
	KASSERT(VTOI(vp)->i_ump != NULL);

#if (BYTE_ORDER == LITTLE_ENDIAN)
	swap = (UFS_MPNEEDSWAP(VTOI(vp)->i_ump) == 0);
#else
	swap = (UFS_MPNEEDSWAP(VTOI(vp)->i_ump) != 0);
#endif

	return ((FSFMT(vp) && swap)?
	    dirbuf->dotdot_type : dirbuf->dotdot_namlen);
}

/*
 * ufs_gro_genealogy: Analyze the genealogy of the source and target
 * directories.
 */
int
ufs_gro_genealogy(struct mount *mp, kauth_cred_t cred,
    struct vnode *fdvp, struct vnode *tdvp,
    struct vnode **intermediate_node_ret)
{
	struct vnode *vp, *dvp;
	ino_t dotdot_ino = 0;	/* XXX: gcc */
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
	error = ufs_gro_lock_directory(mp, tdvp);
	if (error)
		return error;

	vp = tdvp;
	vref(vp);

	for (;;) {
		KASSERT(vp != NULL);
		KASSERT(VOP_ISLOCKED(vp) == LK_EXCLUSIVE);
		KASSERT(vp->v_mount == mp);
		KASSERT(vp->v_type == VDIR);
		KASSERT(!ufs_rmdired_p(vp));

		/* Did we hit the root without finding fdvp?  */
		if (VTOI(vp)->i_number == UFS_ROOTINO) {
			vput(vp);
			*intermediate_node_ret = NULL;
			return 0;
		}

		error = ufs_read_dotdot(vp, cred, &dotdot_ino);
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
		error = vcache_get(mp, &dotdot_ino, sizeof(dotdot_ino), &dvp);
		vput(vp);
		if (error)
			return error;
		error = vn_lock(dvp, LK_EXCLUSIVE);
		if (error) {
			vrele(dvp);
			return error;
		}

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

		if (ufs_rmdired_p(vp)) {
			vput(vp);
			return ENOENT;
		}
	}
}

/*
 * ufs_gro_lock_directory: Lock the directory vp, but fail if it has
 * been rmdir'd.
 */
int
ufs_gro_lock_directory(struct mount *mp, struct vnode *vp)
{

	(void)mp;
	KASSERT(mp != NULL);
	KASSERT(vp != NULL);
	KASSERT(vp->v_mount == mp);

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);

	if (ufs_rmdired_p(vp)) {
		VOP_UNLOCK(vp);
		return ENOENT;
	}

	return 0;
}

static const struct genfs_rename_ops ufs_genfs_rename_ops = {
	.gro_directory_empty_p		= ufs_gro_directory_empty_p,
	.gro_rename_check_possible	= ufs_gro_rename_check_possible,
	.gro_rename_check_permitted	= ufs_gro_rename_check_permitted,
	.gro_remove_check_possible	= ufs_gro_remove_check_possible,
	.gro_remove_check_permitted	= ufs_gro_remove_check_permitted,
	.gro_rename			= ufs_gro_rename,
	.gro_remove			= ufs_gro_remove,
	.gro_lookup			= ufs_gro_lookup,
	.gro_genealogy			= ufs_gro_genealogy,
	.gro_lock_directory		= ufs_gro_lock_directory,
};

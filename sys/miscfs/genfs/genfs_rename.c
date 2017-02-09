/*	$NetBSD: genfs_rename.c,v 1.2 2014/02/06 10:57:12 hannken Exp $	*/

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
 * Generic rename abstraction.
 *
 * Rename is unbelievably hairy.  Try to use this if you can --
 * otherwise you are practically guaranteed to get it wrong.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: genfs_rename.c,v 1.2 2014/02/06 10:57:12 hannken Exp $");

#include <sys/param.h>
#include <sys/kauth.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/fstrans.h>
#include <sys/types.h>

#include <miscfs/genfs/genfs.h>

/*
 * Sample copypasta for implementing VOP_RENAME via genfs_rename.
 * Don't change this template without carefully considering whether
 * every other file system that already uses it needs to change too.
 * That way, once we have changed all the file systems to use it, we
 * can easily replace mumblefs_rename by mumblefs_sane_rename and
 * eliminate the insane API altogether.
 */

/* begin sample copypasta */
#if 0

static const struct genfs_rename_ops mumblefs_genfs_rename_ops;

/*
 * mumblefs_sane_rename: The hairiest vop, with the saner API.
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
mumblefs_sane_rename(
    struct vnode *fdvp, struct componentname *fcnp,
    struct vnode *tdvp, struct componentname *tcnp,
    kauth_cred_t cred, bool posixly_correct)
{
	struct mumblefs_lookup_results fulr, tulr;

	return genfs_sane_rename(&mumblefs_genfs_rename_ops,
	    fdvp, fcnp, &fulr, tdvp, tcnp, &tulr,
	    cred, posixly_correct);
}

/*
 * mumblefs_rename: The hairiest vop, with the insanest API.  Defer to
 * genfs_insane_rename immediately.
 */
int
mumblefs_rename(void *v)
{

	return genfs_insane_rename(v, &mumblefs_sane_rename);
}

#endif
/* end sample copypasta */

/*
 * Forward declarations
 */

static int genfs_rename_enter(const struct genfs_rename_ops *, struct mount *,
    kauth_cred_t,
    struct vnode *, struct componentname *, void *, struct vnode **,
    struct vnode *, struct componentname *, void *, struct vnode **);
static int genfs_rename_enter_common(const struct genfs_rename_ops *,
    struct mount *, kauth_cred_t, struct vnode *,
    struct componentname *, void *, struct vnode **,
    struct componentname *, void *, struct vnode **);
static int genfs_rename_enter_separate(const struct genfs_rename_ops *,
    struct mount *, kauth_cred_t,
    struct vnode *, struct componentname *, void *, struct vnode **,
    struct vnode *, struct componentname *, void *, struct vnode **);
static int genfs_rename_lock(const struct genfs_rename_ops *, struct mount *,
    kauth_cred_t, int, int, int,
    struct vnode *, struct componentname *, bool, void *, struct vnode **,
    struct vnode *, struct componentname *, bool, void *, struct vnode **);
static void genfs_rename_exit(const struct genfs_rename_ops *, struct mount *,
    struct vnode *, struct vnode *,
    struct vnode *, struct vnode *);
static int genfs_rename_remove(const struct genfs_rename_ops *, struct mount *,
    kauth_cred_t,
    struct vnode *, struct componentname *, void *, struct vnode *);

/*
 * genfs_insane_rename: Generic implementation of the insane API for
 * the rename vop.
 *
 * Arguments:
 *
 * . fdvp (from directory vnode),
 * . fvp (from vnode),
 * . fcnp (from component name),
 * . tdvp (to directory vnode),
 * . tvp (to vnode, or NULL), and
 * . tcnp (to component name).
 *
 * Any pair of vnode parameters may have the same vnode.
 *
 * On entry,
 *
 * . fdvp, fvp, tdvp, and tvp are referenced,
 * . fdvp and fvp are unlocked, and
 * . tdvp and tvp (if nonnull) are locked.
 *
 * On exit,
 *
 * . fdvp, fvp, tdvp, and tvp (if nonnull) are unreferenced, and
 * . tdvp and tvp (if nonnull) are unlocked.
 */
int
genfs_insane_rename(void *v,
    int (*sane_rename)(struct vnode *fdvp, struct componentname *fcnp,
	struct vnode *tdvp, struct componentname *tcnp,
	kauth_cred_t cred, bool posixly_correct))
{
	struct vop_rename_args	/* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap = v;
	struct vnode *fdvp = ap->a_fdvp;
	struct vnode *fvp = ap->a_fvp;
	struct componentname *fcnp = ap->a_fcnp;
	struct vnode *tdvp = ap->a_tdvp;
	struct vnode *tvp = ap->a_tvp;
	struct mount *mp = fdvp->v_mount;
	struct componentname *tcnp = ap->a_tcnp;
	kauth_cred_t cred;
	int error;

	KASSERT(fdvp != NULL);
	KASSERT(fvp != NULL);
	KASSERT(fcnp != NULL);
	KASSERT(fcnp->cn_nameptr != NULL);
	KASSERT(tdvp != NULL);
	KASSERT(tcnp != NULL);
	KASSERT(fcnp->cn_nameptr != NULL);
	/* KASSERT(VOP_ISLOCKED(fdvp) != LK_EXCLUSIVE); */
	/* KASSERT(VOP_ISLOCKED(fvp) != LK_EXCLUSIVE); */
	KASSERT(VOP_ISLOCKED(tdvp) == LK_EXCLUSIVE);
	KASSERT((tvp == NULL) || (VOP_ISLOCKED(tvp) == LK_EXCLUSIVE));
	KASSERT(fdvp->v_type == VDIR);
	KASSERT(tdvp->v_type == VDIR);

	fstrans_start(mp, FSTRANS_SHARED);

	cred = fcnp->cn_cred;

	/*
	 * XXX Want a better equality test.  `tcnp->cn_cred == cred'
	 * hoses p2k because puffs transmits the creds separately and
	 * allocates distinct but equivalent structures for them.
	 */
	KASSERT(kauth_cred_uidmatch(cred, tcnp->cn_cred));

	/*
	 * Sanitize our world from the VFS insanity.  Unlock the target
	 * directory and node, which are locked.  Release the children,
	 * which are referenced, since we'll be looking them up again
	 * later.
	 */

	VOP_UNLOCK(tdvp);
	if ((tvp != NULL) && (tvp != tdvp))
		VOP_UNLOCK(tvp);

	vrele(fvp);
	if (tvp != NULL)
		vrele(tvp);

	error = (*sane_rename)(fdvp, fcnp, tdvp, tcnp, cred, false);

	/*
	 * All done, whether with success or failure.  Release the
	 * directory nodes now, as the caller expects from the VFS
	 * protocol.
	 */
	vrele(fdvp);
	vrele(tdvp);

	fstrans_done(mp);

	return error;
}

/*
 * genfs_sane_rename: Generic implementation of the saner API for the
 * rename vop.  Handles ancestry checks, locking, and permissions
 * checks.  Caller is responsible for implementing the genfs rename
 * operations.
 *
 * fdvp and tdvp must be referenced and unlocked.
 */
int
genfs_sane_rename(const struct genfs_rename_ops *ops,
    struct vnode *fdvp, struct componentname *fcnp, void *fde,
    struct vnode *tdvp, struct componentname *tcnp, void *tde,
    kauth_cred_t cred, bool posixly_correct)
{
	struct mount *mp;
	struct vnode *fvp = NULL, *tvp = NULL;
	int error;

	KASSERT(ops != NULL);
	KASSERT(fdvp != NULL);
	KASSERT(fcnp != NULL);
	KASSERT(tdvp != NULL);
	KASSERT(tcnp != NULL);
	/* KASSERT(VOP_ISLOCKED(fdvp) != LK_EXCLUSIVE); */
	/* KASSERT(VOP_ISLOCKED(tdvp) != LK_EXCLUSIVE); */
	KASSERT(fdvp->v_type == VDIR);
	KASSERT(tdvp->v_type == VDIR);
	KASSERT(fdvp->v_mount == tdvp->v_mount);
	KASSERT(fcnp != tcnp);
	KASSERT(fcnp->cn_nameiop == DELETE);
	KASSERT(tcnp->cn_nameiop == RENAME);

        /* XXX Want a better equality test.  */
	KASSERT(kauth_cred_uidmatch(cred, fcnp->cn_cred));
	KASSERT(kauth_cred_uidmatch(cred, tcnp->cn_cred));

	mp = fdvp->v_mount;
	KASSERT(mp != NULL);
	KASSERT(mp == tdvp->v_mount);
	/* XXX How can we be sure this stays true?  */
	KASSERT((mp->mnt_flag & MNT_RDONLY) == 0);

	/* Reject rename("x/..", ...) and rename(..., "x/..") early.  */
	if ((fcnp->cn_flags | tcnp->cn_flags) & ISDOTDOT)
		return EINVAL;	/* XXX EISDIR?  */

	error = genfs_rename_enter(ops, mp, cred,
	    fdvp, fcnp, fde, &fvp,
	    tdvp, tcnp, tde, &tvp);
	if (error)
		return error;

	/*
	 * Check that everything is locked and looks right.
	 */
	KASSERT(fvp != NULL);
	KASSERT(VOP_ISLOCKED(fdvp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(fvp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(tdvp) == LK_EXCLUSIVE);
	KASSERT((tvp == NULL) || (VOP_ISLOCKED(tvp) == LK_EXCLUSIVE));

	/*
	 * If the source and destination are the same object, we need
	 * only at most delete the source entry.  We are guaranteed at
	 * this point that the entries are distinct.
	 */
	if (fvp == tvp) {
		KASSERT(tvp != NULL);
		if (fvp->v_type == VDIR)
			/* XXX This shouldn't be possible.  */
			error = EINVAL;
		else if (posixly_correct)
			/* POSIX sez to leave them alone.  */
			error = 0;
		else if ((fdvp == tdvp) &&
		    (fcnp->cn_namelen == tcnp->cn_namelen) &&
		    (memcmp(fcnp->cn_nameptr, tcnp->cn_nameptr,
			fcnp->cn_namelen) == 0))
			/* Renaming an entry over itself does nothing.  */
			error = 0;
		else
			/* XXX Can't use VOP_REMOVE because of locking.  */
			error = genfs_rename_remove(ops, mp, cred,
			    fdvp, fcnp, fde, fvp);
		goto out;
	}
	KASSERT(fvp != tvp);
	KASSERT((fdvp != tdvp) ||
	    (fcnp->cn_namelen != tcnp->cn_namelen) ||
	    (memcmp(fcnp->cn_nameptr, tcnp->cn_nameptr, fcnp->cn_namelen)
		!= 0));

	/*
	 * If the target exists, refuse to rename a directory over a
	 * non-directory or vice versa, or to clobber a non-empty
	 * directory.
	 */
	if (tvp != NULL) {
		if (fvp->v_type == VDIR && tvp->v_type == VDIR)
			error =
			    (ops->gro_directory_empty_p(mp, cred, tvp, tdvp)?
				0 : ENOTEMPTY);
		else if (fvp->v_type == VDIR && tvp->v_type != VDIR)
			error = ENOTDIR;
		else if (fvp->v_type != VDIR && tvp->v_type == VDIR)
			error = EISDIR;
		else
			error = 0;
		if (error)
			goto out;
		KASSERT((fvp->v_type == VDIR) == (tvp->v_type == VDIR));
	}

	/*
	 * Authorize the rename.
	 */
	error = ops->gro_rename_check_possible(mp, fdvp, fvp, tdvp, tvp);
	if (error)
		goto out;
	error = ops->gro_rename_check_permitted(mp, cred, fdvp, fvp, tdvp, tvp);
	error = kauth_authorize_vnode(cred, KAUTH_VNODE_DELETE, fvp, fdvp,
	    error);
	error = kauth_authorize_vnode(cred, KAUTH_VNODE_RENAME, tvp, tdvp,
	    error);
	if (error)
		goto out;

	/*
	 * Everything is hunky-dory.  Shuffle the directory entries.
	 */
	error = ops->gro_rename(mp, cred,
	    fdvp, fcnp, fde, fvp,
	    tdvp, tcnp, tde, tvp);
	if (error)
		goto out;

	/* Success!  */

out:	genfs_rename_exit(ops, mp, fdvp, fvp, tdvp, tvp);
	return error;
}

/*
 * genfs_rename_knote: Note events about the various vnodes in a
 * rename.  To be called by gro_rename on success.  The only pair of
 * vnodes that may be identical is {fdvp, tdvp}.  deleted_p is true iff
 * the rename overwrote the last link to tvp.
 */
void
genfs_rename_knote(struct vnode *fdvp, struct vnode *fvp,
    struct vnode *tdvp, struct vnode *tvp, bool deleted_p)
{
	long fdvp_events, tdvp_events;
	bool directory_p, reparent_p, replaced_p;

	KASSERT(fdvp != NULL);
	KASSERT(fvp != NULL);
	KASSERT(tdvp != NULL);
	KASSERT(fdvp != fvp);
	KASSERT(fdvp != tvp);
	KASSERT(tdvp != fvp);
	KASSERT(tdvp != tvp);
	KASSERT(fvp != tvp);
	KASSERT(VOP_ISLOCKED(fdvp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(fvp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(tdvp) == LK_EXCLUSIVE);
	KASSERT((tvp == NULL) || (VOP_ISLOCKED(tvp) == LK_EXCLUSIVE));

	directory_p = (fvp->v_type == VDIR);
	reparent_p = (fdvp != tdvp);
	replaced_p = (tvp != NULL);

	KASSERT((tvp == NULL) || (directory_p == (tvp->v_type == VDIR)));
	KASSERT(!deleted_p || replaced_p);

	fdvp_events = NOTE_WRITE;
	if (directory_p && reparent_p)
		fdvp_events |= NOTE_LINK;
	VN_KNOTE(fdvp, fdvp_events);

	VN_KNOTE(fvp, NOTE_RENAME);

	if (reparent_p) {
		tdvp_events = NOTE_WRITE;
		if (!replaced_p) {
			tdvp_events |= NOTE_EXTEND;
			if (directory_p)
				tdvp_events |= NOTE_LINK;
		}
		VN_KNOTE(tdvp, tdvp_events);
	}

	if (replaced_p)
		VN_KNOTE(tvp, (deleted_p? NOTE_DELETE : NOTE_LINK));
}

/*
 * genfs_rename_cache_purge: Purge the name cache.  To be called by
 * gro_rename on success.  The only pair of vnodes that may be
 * identical is {fdvp, tdvp}.
 */
void
genfs_rename_cache_purge(struct vnode *fdvp, struct vnode *fvp,
    struct vnode *tdvp, struct vnode *tvp)
{

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

	/*
	 * XXX What actually needs to be purged?
	 */

	cache_purge(fdvp);

	if (fvp->v_type == VDIR)
		cache_purge(fvp);

	if (tdvp != fdvp)
		cache_purge(tdvp);

	if ((tvp != NULL) && (tvp->v_type == VDIR))
		cache_purge(tvp);
}

/*
 * genfs_rename_enter: Look up fcnp in fdvp, and store the lookup
 * results in *fde_ret and the associated vnode in *fvp_ret; fail if
 * not found.  Look up tcnp in tdvp, and store the lookup results in
 * *tde_ret and the associated vnode in *tvp_ret; store null instead if
 * not found.  Fail if anything has been mounted on any of the nodes
 * involved.
 *
 * fdvp and tdvp must be referenced.
 *
 * On entry, nothing is locked.
 *
 * On success, everything is locked, and *fvp_ret, and *tvp_ret if
 * nonnull, are referenced.  The only pairs of vnodes that may be
 * identical are {fdvp, tdvp} and {fvp, tvp}.
 *
 * On failure, everything remains as was.
 *
 * Locking everything including the source and target nodes is
 * necessary to make sure that, e.g., link count updates are OK.  The
 * locking order is, in general, ancestor-first, matching the order you
 * need to use to look up a descendant anyway.
 */
static int
genfs_rename_enter(const struct genfs_rename_ops *ops,
    struct mount *mp, kauth_cred_t cred,
    struct vnode *fdvp, struct componentname *fcnp,
    void *fde_ret, struct vnode **fvp_ret,
    struct vnode *tdvp, struct componentname *tcnp,
    void *tde_ret, struct vnode **tvp_ret)
{
	int error;

	KASSERT(mp != NULL);
	KASSERT(fdvp != NULL);
	KASSERT(fcnp != NULL);
	KASSERT(fvp_ret != NULL);
	KASSERT(tdvp != NULL);
	KASSERT(tcnp != NULL);
	KASSERT(tvp_ret != NULL);
	KASSERT(fvp_ret != tvp_ret);
	KASSERT(fdvp->v_type == VDIR);
	KASSERT(tdvp->v_type == VDIR);
	KASSERT(fdvp->v_mount == mp);
	KASSERT(tdvp->v_mount == mp);

	if (fdvp == tdvp)
		error = genfs_rename_enter_common(ops, mp, cred, fdvp,
		    fcnp, fde_ret, fvp_ret,
		    tcnp, tde_ret, tvp_ret);
	else
		error = genfs_rename_enter_separate(ops, mp, cred,
		    fdvp, fcnp, fde_ret, fvp_ret,
		    tdvp, tcnp, tde_ret, tvp_ret);

	if (error)
		return error;

	KASSERT(*fvp_ret != NULL);
	KASSERT(VOP_ISLOCKED(*fvp_ret) == LK_EXCLUSIVE);
	KASSERT((*tvp_ret == NULL) || (VOP_ISLOCKED(*tvp_ret) == LK_EXCLUSIVE));
	KASSERT(*fvp_ret != fdvp);
	KASSERT(*fvp_ret != tdvp);
	KASSERT(*tvp_ret != fdvp);
	KASSERT(*tvp_ret != tdvp);
	return 0;
}

/*
 * genfs_rename_enter_common: Lock and look up with a common
 * source/target directory.
 */
static int
genfs_rename_enter_common(const struct genfs_rename_ops *ops,
    struct mount *mp, kauth_cred_t cred, struct vnode *dvp,
    struct componentname *fcnp,
    void *fde_ret, struct vnode **fvp_ret,
    struct componentname *tcnp,
    void *tde_ret, struct vnode **tvp_ret)
{
	struct vnode *fvp, *tvp;
	int error;

	KASSERT(ops != NULL);
	KASSERT(mp != NULL);
	KASSERT(dvp != NULL);
	KASSERT(fcnp != NULL);
	KASSERT(fvp_ret != NULL);
	KASSERT(tcnp != NULL);
	KASSERT(tvp_ret != NULL);
	KASSERT(dvp->v_type == VDIR);
	KASSERT(dvp->v_mount == mp);

	error = ops->gro_lock_directory(mp, dvp);
	if (error)
		goto fail0;

	/* Did we lose a race with mount?  */
	if (dvp->v_mountedhere != NULL) {
		error = EBUSY;
		goto fail1;
	}

	KASSERT(fcnp->cn_nameiop == DELETE);
	error = ops->gro_lookup(mp, dvp, fcnp, fde_ret, &fvp);
	if (error)
		goto fail1;

	KASSERT(fvp != NULL);

	/* Refuse to rename `.'.  */
	if (fvp == dvp) {
		error = EINVAL;
		goto fail2;
	}
	KASSERT(fvp != dvp);

	KASSERT(tcnp->cn_nameiop == RENAME);
	error = ops->gro_lookup(mp, dvp, tcnp, tde_ret, &tvp);
	if (error == ENOENT) {
		tvp = NULL;
	} else if (error) {
		goto fail2;
	} else {
		KASSERT(tvp != NULL);

		/* Refuse to rename over `.'.  */
		if (tvp == dvp) {
			error = EISDIR; /* XXX EINVAL?  */
			goto fail2;
		}
	}
	KASSERT(tvp != dvp);

	/*
	 * We've looked up both nodes.  Now lock them and check them.
	 */

	vn_lock(fvp, LK_EXCLUSIVE | LK_RETRY);
	KASSERT(fvp->v_mount == mp);
	/* Refuse to rename a mount point.  */
	if ((fvp->v_type == VDIR) && (fvp->v_mountedhere != NULL)) {
		error = EBUSY;
		goto fail3;
	}

	if ((tvp != NULL) && (tvp != fvp)) {
		vn_lock(tvp, LK_EXCLUSIVE | LK_RETRY);
		KASSERT(tvp->v_mount == mp);
		/* Refuse to rename over a mount point.  */
		if ((tvp->v_type == VDIR) && (tvp->v_mountedhere != NULL)) {
			error = EBUSY;
			goto fail4;
		}
	}

	KASSERT(VOP_ISLOCKED(dvp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(fvp) == LK_EXCLUSIVE);
	KASSERT((tvp == NULL) || (VOP_ISLOCKED(tvp) == LK_EXCLUSIVE));

	*fvp_ret = fvp;
	*tvp_ret = tvp;
	return 0;

fail4:	if ((tvp != NULL) && (tvp != fvp))
		VOP_UNLOCK(tvp);
fail3:	VOP_UNLOCK(fvp);
	if (tvp != NULL)
		vrele(tvp);
fail2:	vrele(fvp);
fail1:	VOP_UNLOCK(dvp);
fail0:	return error;
}

/*
 * genfs_rename_enter_separate: Lock and look up with separate source
 * and target directories.
 */
static int
genfs_rename_enter_separate(const struct genfs_rename_ops *ops,
    struct mount *mp, kauth_cred_t cred,
    struct vnode *fdvp, struct componentname *fcnp,
    void *fde_ret, struct vnode **fvp_ret,
    struct vnode *tdvp, struct componentname *tcnp,
    void *tde_ret, struct vnode **tvp_ret)
{
	struct vnode *intermediate_node;
	struct vnode *fvp, *tvp;
	int error;

	KASSERT(ops != NULL);
	KASSERT(mp != NULL);
	KASSERT(fdvp != NULL);
	KASSERT(fcnp != NULL);
	KASSERT(fvp_ret != NULL);
	KASSERT(tdvp != NULL);
	KASSERT(tcnp != NULL);
	KASSERT(tvp_ret != NULL);
	KASSERT(fdvp != tdvp);
	KASSERT(fcnp != tcnp);
	KASSERT(fcnp->cn_nameiop == DELETE);
	KASSERT(tcnp->cn_nameiop == RENAME);
	KASSERT(fvp_ret != tvp_ret);
	KASSERT(fdvp->v_type == VDIR);
	KASSERT(tdvp->v_type == VDIR);
	KASSERT(fdvp->v_mount == mp);
	KASSERT(tdvp->v_mount == mp);

	error = ops->gro_genealogy(mp, cred, fdvp, tdvp, &intermediate_node);
	if (error)
		return error;

	/*
	 * intermediate_node == NULL means fdvp is not an ancestor of tdvp.
	 */
	if (intermediate_node == NULL)
		error = genfs_rename_lock(ops, mp, cred,
		    ENOTEMPTY, EISDIR, EINVAL,
		    tdvp, tcnp, true, tde_ret, &tvp,
		    fdvp, fcnp, false, fde_ret, &fvp);
	else
		error = genfs_rename_lock(ops, mp, cred,
		    EINVAL, EISDIR, EINVAL,
		    fdvp, fcnp, false, fde_ret, &fvp,
		    tdvp, tcnp, true, tde_ret, &tvp);
	if (error)
		goto out;

	KASSERT(fvp != NULL);

	/*
	 * Reject rename("foo/bar", "foo/bar/baz/quux/zot").
	 */
	if (fvp == intermediate_node) {
		genfs_rename_exit(ops, mp, fdvp, fvp, tdvp, tvp);
		error = EINVAL;
		goto out;
	}

	*fvp_ret = fvp;
	*tvp_ret = tvp;
	error = 0;

out:	if (intermediate_node != NULL)
		vrele(intermediate_node);
	return error;
}

/*
 * genfs_rename_lock: Lock directories a and b, which must be distinct,
 * and look up and lock nodes a and b.  Do a first and then b.
 * Directory b may not be an ancestor of directory a, although
 * directory a may be an ancestor of directory b.  Fail with
 * overlap_error if node a is directory b.  Neither componentname may
 * be `.' or `..'.
 *
 * a_dvp and b_dvp must be referenced.
 *
 * On entry, a_dvp and b_dvp are unlocked.
 *
 * On success,
 * . a_dvp and b_dvp are locked,
 * . *a_dirent_ret is filled with a directory entry whose node is
 *     locked and referenced,
 * . *b_vp_ret is filled with the corresponding vnode,
 * . *b_dirent_ret is filled either with null or with a directory entry
 *     whose node is locked and referenced,
 * . *b_vp is filled either with null or with the corresponding vnode,
 *     and
 * . the only pair of vnodes that may be identical is a_vp and b_vp.
 *
 * On failure, a_dvp and b_dvp are left unlocked, and *a_dirent_ret,
 * *a_vp, *b_dirent_ret, and *b_vp are left alone.
 */
static int
genfs_rename_lock(const struct genfs_rename_ops *ops,
    struct mount *mp, kauth_cred_t cred,
    int overlap_error, int a_dot_error, int b_dot_error,
    struct vnode *a_dvp, struct componentname *a_cnp, bool a_missing_ok,
    void *a_de_ret, struct vnode **a_vp_ret,
    struct vnode *b_dvp, struct componentname *b_cnp, bool b_missing_ok,
    void *b_de_ret, struct vnode **b_vp_ret)
{
	struct vnode *a_vp, *b_vp;
	int error;

	KASSERT(ops != NULL);
	KASSERT(mp != NULL);
	KASSERT(a_dvp != NULL);
	KASSERT(a_cnp != NULL);
	KASSERT(a_vp_ret != NULL);
	KASSERT(b_dvp != NULL);
	KASSERT(b_cnp != NULL);
	KASSERT(b_vp_ret != NULL);
	KASSERT(a_dvp != b_dvp);
	KASSERT(a_vp_ret != b_vp_ret);
	KASSERT(a_dvp->v_type == VDIR);
	KASSERT(b_dvp->v_type == VDIR);
	KASSERT(a_dvp->v_mount == mp);
	KASSERT(b_dvp->v_mount == mp);
	KASSERT(a_missing_ok != b_missing_ok);

	error = ops->gro_lock_directory(mp, a_dvp);
	if (error)
		goto fail0;

	/* Did we lose a race with mount?  */
	if (a_dvp->v_mountedhere != NULL) {
		error = EBUSY;
		goto fail1;
	}

	error = ops->gro_lookup(mp, a_dvp, a_cnp, a_de_ret, &a_vp);
	if (error) {
		if (a_missing_ok && (error == ENOENT))
			a_vp = NULL;
		else
			goto fail1;
	} else {
		KASSERT(a_vp != NULL);

		/* Refuse to rename (over) `.'.  */
		if (a_vp == a_dvp) {
			error = a_dot_error;
			goto fail2;
		}

		if (a_vp == b_dvp) {
			error = overlap_error;
			goto fail2;
		}
	}

	KASSERT(a_vp != a_dvp);
	KASSERT(a_vp != b_dvp);

	error = ops->gro_lock_directory(mp, b_dvp);
	if (error)
		goto fail2;

	/* Did we lose a race with mount?  */
	if (b_dvp->v_mountedhere != NULL) {
		error = EBUSY;
		goto fail3;
	}

	error = ops->gro_lookup(mp, b_dvp, b_cnp, b_de_ret, &b_vp);
	if (error) {
		if (b_missing_ok && (error == ENOENT))
			b_vp = NULL;
		else
			goto fail3;
	} else {
		KASSERT(b_vp != NULL);

		/* Refuse to rename (over) `.'.  */
		if (b_vp == b_dvp) {
			error = b_dot_error;
			goto fail4;
		}

		/* b is not an ancestor of a.  */
		if (b_vp == a_dvp) {
			/*
			 * We have a directory hard link before us.
			 * XXX What error should this return?  EDEADLK?
			 * Panic?
			 */
			error = EIO;
			goto fail4;
		}
	}
	KASSERT(b_vp != b_dvp);
	KASSERT(b_vp != a_dvp);

	/*
	 * We've looked up both nodes.  Now lock them and check them.
	 */

	if (a_vp != NULL) {
		vn_lock(a_vp, LK_EXCLUSIVE | LK_RETRY);
		KASSERT(a_vp->v_mount == mp);
		/* Refuse to rename (over) a mount point.  */
		if ((a_vp->v_type == VDIR) && (a_vp->v_mountedhere != NULL)) {
			error = EBUSY;
			goto fail5;
		}
	}

	if ((b_vp != NULL) && (b_vp != a_vp)) {
		vn_lock(b_vp, LK_EXCLUSIVE | LK_RETRY);
		KASSERT(b_vp->v_mount == mp);
		/* Refuse to rename (over) a mount point.  */
		if ((b_vp->v_type == VDIR) && (b_vp->v_mountedhere != NULL)) {
			error = EBUSY;
			goto fail6;
		}
	}

	KASSERT(VOP_ISLOCKED(a_dvp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(b_dvp) == LK_EXCLUSIVE);
	KASSERT(a_missing_ok || (a_vp != NULL));
	KASSERT(b_missing_ok || (b_vp != NULL));
	KASSERT((a_vp == NULL) || (VOP_ISLOCKED(a_vp) == LK_EXCLUSIVE));
	KASSERT((b_vp == NULL) || (VOP_ISLOCKED(b_vp) == LK_EXCLUSIVE));

	*a_vp_ret = a_vp;
	*b_vp_ret = b_vp;
	return 0;

fail6:	if ((b_vp != NULL) && (b_vp != a_vp))
		VOP_UNLOCK(b_vp);
fail5:	if (a_vp != NULL)
		VOP_UNLOCK(a_vp);
fail4:	if (b_vp != NULL)
		vrele(b_vp);
fail3:	VOP_UNLOCK(b_dvp);
fail2:	if (a_vp != NULL)
		vrele(a_vp);
fail1:	VOP_UNLOCK(a_dvp);
fail0:	return error;
}

/*
 * genfs_rename_exit: Unlock everything we locked for rename.
 *
 * fdvp and tdvp must be referenced.
 *
 * On entry, everything is locked, and fvp and tvp referenced.
 *
 * On exit, everything is unlocked, and fvp and tvp are released.
 */
static void
genfs_rename_exit(const struct genfs_rename_ops *ops,
    struct mount *mp,
    struct vnode *fdvp, struct vnode *fvp,
    struct vnode *tdvp, struct vnode *tvp)
{

	(void)ops;
	KASSERT(ops != NULL);
	KASSERT(mp != NULL);
	KASSERT(fdvp != NULL);
	KASSERT(fvp != NULL);
	KASSERT(fdvp != fvp);
	KASSERT(fdvp != tvp);
	KASSERT(tdvp != tvp);
	KASSERT(tdvp != fvp);
	KASSERT(VOP_ISLOCKED(fdvp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(tdvp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(fvp) == LK_EXCLUSIVE);
	KASSERT((tvp == NULL) || (VOP_ISLOCKED(tvp) == LK_EXCLUSIVE));

	if ((tvp != NULL) && (tvp != fvp))
		VOP_UNLOCK(tvp);
	VOP_UNLOCK(fvp);
	if (tvp != NULL)
		vrele(tvp);
	if (tdvp != fdvp)
		VOP_UNLOCK(tdvp);
	vrele(fvp);
	VOP_UNLOCK(fdvp);
}

/*
 * genfs_rename_remove: Remove the entry for the non-directory vp with
 * componentname cnp from the directory dvp, using the lookup results
 * de.  It is the responsibility of gro_remove to purge the name cache
 * and note kevents.
 *
 * Everything must be locked and referenced.
 */
static int
genfs_rename_remove(const struct genfs_rename_ops *ops,
    struct mount *mp, kauth_cred_t cred,
    struct vnode *dvp, struct componentname *cnp, void *de, struct vnode *vp)
{
	int error;

	KASSERT(ops != NULL);
	KASSERT(mp != NULL);
	KASSERT(dvp != NULL);
	KASSERT(cnp != NULL);
	KASSERT(vp != NULL);
	KASSERT(dvp != vp);
	KASSERT(dvp->v_type == VDIR);
	KASSERT(vp->v_type != VDIR);
	KASSERT(dvp->v_mount == mp);
	KASSERT(vp->v_mount == mp);
	KASSERT(VOP_ISLOCKED(dvp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(vp) == LK_EXCLUSIVE);

	error = ops->gro_remove_check_possible(mp, dvp, vp);
	if (error)
		return error;

	error = ops->gro_remove_check_permitted(mp, cred, dvp, vp);
	error = kauth_authorize_vnode(cred, KAUTH_VNODE_DELETE, vp, dvp,
	    error);
	if (error)
		return error;

	error = ops->gro_remove(mp, cred, dvp, cnp, de, vp);
	if (error)
		return error;

	return 0;
}

static int
genfs_ufslike_check_sticky(kauth_cred_t, mode_t, uid_t, struct vnode *, uid_t);

/*
 * genfs_ufslike_rename_check_possible: Check whether a rename is
 * possible independent of credentials, assuming UFS-like inode flag
 * semantics.  clobber_p is true iff the target node already exists.
 */
int
genfs_ufslike_rename_check_possible(
    unsigned long fdflags, unsigned long fflags,
    unsigned long tdflags, unsigned long tflags, bool clobber_p,
    unsigned long immutable, unsigned long append)
{

	if ((fdflags | fflags) & (immutable | append))
		return EPERM;

	if (tdflags & (immutable | (clobber_p? append : 0)))
		return EPERM;

	if (clobber_p && (tflags & (immutable | append)))
		return EPERM;

	return 0;
}

/*
 * genfs_ufslike_rename_check_permitted: Check whether a rename is
 * permitted given our credentials, assuming UFS-like permission and
 * ownership semantics.
 *
 * The only pair of vnodes that may be identical is {fdvp, tdvp}.
 *
 * Everything must be locked and referenced.
 */
int
genfs_ufslike_rename_check_permitted(kauth_cred_t cred,
    struct vnode *fdvp, mode_t fdmode, uid_t fduid,
    struct vnode *fvp, uid_t fuid,
    struct vnode *tdvp, mode_t tdmode, uid_t tduid,
    struct vnode *tvp, uid_t tuid)
{
	int error;

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
	KASSERT(fdvp->v_mount == fvp->v_mount);
	KASSERT(fdvp->v_mount == tdvp->v_mount);
	KASSERT((tvp == NULL) || (fdvp->v_mount == tvp->v_mount));
	KASSERT(VOP_ISLOCKED(fdvp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(fvp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(tdvp) == LK_EXCLUSIVE);
	KASSERT((tvp == NULL) || (VOP_ISLOCKED(tvp) == LK_EXCLUSIVE));

	/*
	 * We need to remove or change an entry in the source directory.
	 */
	error = VOP_ACCESS(fdvp, VWRITE, cred);
	if (error)
		return error;

	/*
	 * If we are changing directories, then we need to write to the
	 * target directory to add or change an entry.  Also, if fvp is
	 * a directory, we need to write to it to change its `..'
	 * entry.
	 */
	if (fdvp != tdvp) {
		error = VOP_ACCESS(tdvp, VWRITE, cred);
		if (error)
			return error;
		if (fvp->v_type == VDIR) {
			error = VOP_ACCESS(fvp, VWRITE, cred);
			if (error)
				return error;
		}
	}

	error = genfs_ufslike_check_sticky(cred, fdmode, fduid, fvp, fuid);
	if (error)
		return error;

	error = genfs_ufslike_check_sticky(cred, tdmode, tduid, tvp, tuid);
	if (error)
		return error;

	return 0;
}

/*
 * genfs_ufslike_remove_check_possible: Check whether a remove is
 * possible independent of credentials, assuming UFS-like inode flag
 * semantics.
 */
int
genfs_ufslike_remove_check_possible(unsigned long dflags, unsigned long flags,
    unsigned long immutable, unsigned long append)
{

	/*
	 * We want to delete the entry.  If the directory is immutable,
	 * we can't write to it to delete the entry.  If the directory
	 * is append-only, the only change we can make is to add
	 * entries, so we can't delete entries.  If the node is
	 * immutable, we can't change the links to it, so we can't
	 * delete the entry.  If the node is append-only...well, this
	 * is what UFS does.
	 */
	if ((dflags | flags) & (immutable | append))
		return EPERM;

	return 0;
}

/*
 * genfs_ufslike_remove_check_permitted: Check whether a remove is
 * permitted given our credentials, assuming UFS-like permission and
 * ownership semantics.
 *
 * Everything must be locked and referenced.
 */
int
genfs_ufslike_remove_check_permitted(kauth_cred_t cred,
    struct vnode *dvp, mode_t dmode, uid_t duid,
    struct vnode *vp, uid_t uid)
{
	int error;

	KASSERT(dvp != NULL);
	KASSERT(vp != NULL);
	KASSERT(dvp != vp);
	KASSERT(dvp->v_type == VDIR);
	KASSERT(vp->v_type != VDIR);
	KASSERT(dvp->v_mount == vp->v_mount);
	KASSERT(VOP_ISLOCKED(dvp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(vp) == LK_EXCLUSIVE);

	/*
	 * We need to write to the directory to remove from it.
	 */
	error = VOP_ACCESS(dvp, VWRITE, cred);
	if (error)
		return error;

	error = genfs_ufslike_check_sticky(cred, dmode, duid, vp, uid);
	if (error)
		return error;

	return 0;
}

/*
 * genfs_ufslike_check_sticky: Check whether a party with credentials
 * cred may change an entry in a sticky directory, assuming UFS-like
 * permission, ownership, and stickiness semantics: If the directory is
 * sticky and the entry exists, the user must own either the directory
 * or the entry's node in order to change the entry.
 *
 * Everything must be locked and referenced.
 */
int
genfs_ufslike_check_sticky(kauth_cred_t cred, mode_t dmode, uid_t duid,
    struct vnode *vp, uid_t uid)
{

	if ((dmode & S_ISTXT) && (vp != NULL))
		return genfs_can_sticky(cred, duid, uid);

	return 0;
}

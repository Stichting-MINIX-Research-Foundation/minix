/*	$NetBSD: msdosfs_rename.c,v 1.3 2021/10/23 16:58:17 thorpej Exp $	*/

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
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
 * MS-DOS FS Rename
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: msdosfs_rename.c,v 1.3 2021/10/23 16:58:17 thorpej Exp $");

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/kauth.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/vnode_if.h>

#include <miscfs/genfs/genfs.h>

#include <fs/msdosfs/bpb.h>
#include <fs/msdosfs/direntry.h>
#include <fs/msdosfs/denode.h>
#include <fs/msdosfs/msdosfsmount.h>
#include <fs/msdosfs/fat.h>

/*
 * Forward declarations
 */

static int msdosfs_sane_rename(struct vnode *, struct componentname *,
    struct vnode *, struct componentname *,
    kauth_cred_t, bool);
static bool msdosfs_rmdired_p(struct vnode *);
static int msdosfs_read_dotdot(struct vnode *, kauth_cred_t, unsigned long *);
static int msdosfs_rename_replace_dotdot(struct vnode *,
    struct vnode *, struct vnode *, kauth_cred_t);
static int msdosfs_gro_lock_directory(struct mount *, struct vnode *);

static const struct genfs_rename_ops msdosfs_genfs_rename_ops;

/*
 * msdosfs_rename: The hairiest vop, with the insanest API.
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
 * . tdvp and tvp are unlocked.
 */
int
msdosfs_rename(void *v)
{
	struct vop_rename_args  /* {
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

	cred = fcnp->cn_cred;
	KASSERT(tcnp->cn_cred == cred);

	/*
	 * Sanitize our world from the VFS insanity.  Unlock the target
	 * directory and node, which are locked.  Release the children,
	 * which are referenced.  Check for rename("x", "y/."), which
	 * it is our responsibility to reject, not the caller's.  (But
	 * the caller does reject rename("x/.", "y").  Go figure.)
	 */

	VOP_UNLOCK(tdvp);
	if ((tvp != NULL) && (tvp != tdvp))
		VOP_UNLOCK(tvp);

	vrele(fvp);
	if (tvp != NULL)
		vrele(tvp);

	if (tvp == tdvp) {
		error = EINVAL;
		goto out;
	}

	error = msdosfs_sane_rename(fdvp, fcnp, tdvp, tcnp, cred, false);

out:	/*
	 * All done, whether with success or failure.  Release the
	 * directory nodes now, as the caller expects from the VFS
	 * protocol.
	 */
	vrele(fdvp);
	vrele(tdvp);

	return error;
}

/*
 * msdosfs_sane_rename: The hairiest vop, with the saner API.
 *
 * Arguments:
 *
 * . fdvp (from directory vnode),
 * . fcnp (from component name),
 * . tdvp (to directory vnode), and
 * . tcnp (to component name).
 *
 * fdvp and tdvp must be referenced and unlocked.
 */
static int
msdosfs_sane_rename(
    struct vnode *fdvp, struct componentname *fcnp,
    struct vnode *tdvp, struct componentname *tcnp,
    kauth_cred_t cred, bool posixly_correct)
{
	struct msdosfs_lookup_results fmlr, tmlr;

	return genfs_sane_rename(&msdosfs_genfs_rename_ops,
	    fdvp, fcnp, &fmlr, tdvp, tcnp, &tmlr,
	    cred, posixly_correct);
}

/*
 * msdosfs_gro_directory_empty_p: Return true if the directory vp is
 * empty.  dvp is its parent.
 *
 * vp and dvp must be locked and referenced.
 */
static bool
msdosfs_gro_directory_empty_p(struct mount *mp, kauth_cred_t cred,
    struct vnode *vp, struct vnode *dvp)
{

	(void)mp;
	(void)cred;
	(void)dvp;
	KASSERT(mp != NULL);
	KASSERT(vp != NULL);
	KASSERT(dvp != NULL);
	KASSERT(vp != dvp);
	KASSERT(vp->v_mount == mp);
	KASSERT(dvp->v_mount == mp);
	KASSERT(VOP_ISLOCKED(vp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(dvp) == LK_EXCLUSIVE);

	return msdosfs_dosdirempty(VTODE(vp));
}

/*
 * Return a UFS-like mode for vp.
 */
static mode_t
msdosfs_vnode_mode(struct vnode *vp)
{
	struct msdosfsmount *pmp;
	mode_t mode, mask;

	KASSERT(vp != NULL);

	pmp = VTODE(vp)->de_pmp;
	KASSERT(pmp != NULL);

	if (VTODE(vp)->de_Attributes & ATTR_READONLY)
		mode = S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH;
	else
		mode = S_IRWXU|S_IRWXG|S_IRWXO;

	if (vp->v_type == VDIR)
		mask = pmp->pm_dirmask;
	else
		mask = pmp->pm_mask;

	return (mode & mask);
}

/*
 * msdosfs_gro_rename_check_possible: Check whether renaming fvp in fdvp
 * to tvp in tdvp is possible independent of credentials.
 */
static int
msdosfs_gro_rename_check_possible(struct mount *mp,
    struct vnode *fdvp, struct vnode *fvp,
    struct vnode *tdvp, struct vnode *tvp)
{

	(void)mp;
	(void)fdvp;
	(void)fvp;
	(void)tdvp;
	(void)tvp;
	KASSERT(mp != NULL);
	KASSERT(fdvp != NULL);
	KASSERT(fvp != NULL);
	KASSERT(tdvp != NULL);
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

	/* It's always possible: no error.  */
	return 0;
}

/*
 * msdosfs_gro_rename_check_permitted: ...
 */
static int
msdosfs_gro_rename_check_permitted(struct mount *mp, kauth_cred_t cred,
    struct vnode *fdvp, struct vnode *fvp,
    struct vnode *tdvp, struct vnode *tvp)
{
	struct msdosfsmount *pmp;

	KASSERT(mp != NULL);
	KASSERT(fdvp != NULL);
	KASSERT(fvp != NULL);
	KASSERT(tdvp != NULL);
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

	pmp = VFSTOMSDOSFS(mp);
	KASSERT(pmp != NULL);

	return genfs_ufslike_rename_check_permitted(cred,
	    fdvp, msdosfs_vnode_mode(fdvp), pmp->pm_uid,
	    fvp, pmp->pm_uid,
	    tdvp, msdosfs_vnode_mode(tdvp), pmp->pm_uid,
	    tvp, (tvp? pmp->pm_uid : 0));
}

/*
 * msdosfs_gro_remove_check_possible: ...
 */
static int
msdosfs_gro_remove_check_possible(struct mount *mp,
    struct vnode *dvp, struct vnode *vp)
{

	KASSERT(mp != NULL);
	KASSERT(dvp != NULL);
	KASSERT(vp != NULL);
	KASSERT(dvp != vp);
	KASSERT(dvp->v_mount == mp);
	KASSERT(vp->v_mount == mp);
	KASSERT(VOP_ISLOCKED(dvp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(vp) == LK_EXCLUSIVE);

	/* It's always possible: no error.  */
	return 0;
}

/*
 * msdosfs_gro_remove_check_permitted: ...
 */
static int
msdosfs_gro_remove_check_permitted(struct mount *mp, kauth_cred_t cred,
    struct vnode *dvp, struct vnode *vp)
{
	struct msdosfsmount *pmp;

	KASSERT(mp != NULL);
	KASSERT(dvp != NULL);
	KASSERT(vp != NULL);
	KASSERT(dvp != vp);
	KASSERT(dvp->v_mount == mp);
	KASSERT(vp->v_mount == mp);
	KASSERT(VOP_ISLOCKED(dvp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(vp) == LK_EXCLUSIVE);

	pmp = VFSTOMSDOSFS(mp);
	KASSERT(pmp != NULL);

	return genfs_ufslike_remove_check_permitted(cred,
	    dvp, msdosfs_vnode_mode(dvp), pmp->pm_uid, vp, pmp->pm_uid);
}

/*
 * msdosfs_gro_rename: Actually perform the rename operation.
 */
static int
msdosfs_gro_rename(struct mount *mp, kauth_cred_t cred,
    struct vnode *fdvp, struct componentname *fcnp,
    void *fde, struct vnode *fvp,
    struct vnode *tdvp, struct componentname *tcnp,
    void *tde, struct vnode *tvp, nlink_t *tvp_nlinkp)
{
	struct msdosfs_lookup_results *fmlr = fde;
	struct msdosfs_lookup_results *tmlr = tde;
	struct msdosfsmount *pmp;
	bool directory_p, reparent_p;
	unsigned char toname[12], oldname[12];
	int error;

	KASSERT(mp != NULL);
	KASSERT(fdvp != NULL);
	KASSERT(fcnp != NULL);
	KASSERT(fmlr != NULL);
	KASSERT(fvp != NULL);
	KASSERT(tdvp != NULL);
	KASSERT(tcnp != NULL);
	KASSERT(tmlr != NULL);
	KASSERT(fmlr != tmlr);
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
	 * We shall need to temporarily bump the reference count, so
	 * make sure there is room to do so.
	 */
	if (VTODE(fvp)->de_refcnt >= LONG_MAX)
		return EMLINK;

	/*
	 * XXX There is a pile of logic here to handle a voodoo flag
	 * DE_RENAME.  I think this is a vestige of days when the file
	 * system hackers didn't understand concurrency or race
	 * conditions; I believe it serves no useful function
	 * whatsoever.
	 */

	directory_p = (fvp->v_type == VDIR);
	KASSERT(directory_p ==
	    ((VTODE(fvp)->de_Attributes & ATTR_DIRECTORY) != 0));
	KASSERT((tvp == NULL) || (directory_p == (tvp->v_type == VDIR)));
	KASSERT((tvp == NULL) || (directory_p ==
		((VTODE(fvp)->de_Attributes & ATTR_DIRECTORY) != 0)));
	if (directory_p) {
		if (VTODE(fvp)->de_flag & DE_RENAME)
			return EINVAL;
		VTODE(fvp)->de_flag |= DE_RENAME;
	}

	reparent_p = (fdvp != tdvp);
	KASSERT(reparent_p == (VTODE(fdvp)->de_StartCluster !=
		VTODE(tdvp)->de_StartCluster));

	/*
	 * XXX Hold it right there -- surely if we crash after
	 * removede, we'll fail to provide rename's guarantee that
	 * there will be something at the target pathname?
	 */
	if (tvp != NULL) {
		error = msdosfs_removede(VTODE(tdvp), VTODE(tvp), tmlr);
		if (error)
			goto out;
	}

	/*
	 * Convert the filename in tcnp into a dos filename. We copy this
	 * into the denode and directory entry for the destination
	 * file/directory.
	 */
	error = msdosfs_uniqdosname(VTODE(tdvp), tcnp, toname);
	if (error)
		goto out;

	/*
	 * First write a new entry in the destination directory and
	 * mark the entry in the source directory as deleted.  Then
	 * move the denode to the correct hash chain for its new
	 * location in the filesystem.  And, if we moved a directory,
	 * then update its .. entry to point to the new parent
	 * directory.
	 */

	/* Save the old name in case we need to back out.  */
	memcpy(oldname, VTODE(fvp)->de_Name, 11);
	memcpy(VTODE(fvp)->de_Name, toname, 11);

	error = msdosfs_createde(VTODE(fvp), VTODE(tdvp), tmlr, 0, tcnp);
	if (error) {
		/* Directory entry didn't take -- back out the name change.  */
		memcpy(VTODE(fvp)->de_Name, oldname, 11);
		goto out;
	}

	/*
	 * createde doesn't increment de_refcnt, but removede
	 * decrements it.  Go figure.
	 */
	KASSERT(VTODE(fvp)->de_refcnt < LONG_MAX);
	VTODE(fvp)->de_refcnt++;

	/*
	 * XXX Yes, createde and removede have arguments swapped.  Go figure.
	 */
	error = msdosfs_removede(VTODE(fdvp), VTODE(fvp), fmlr);
	if (error) {
#if 0		/* XXX Back out the new directory entry?  Panic?  */
		(void)msdosfs_removede(VTODE(tdvp), VTODE(fvp), tmlr);
		memcpy(VTODE(fvp)->de_Name, oldname, 11);
#endif
		goto out;
	}

	pmp = VFSTOMSDOSFS(mp);

	if (!directory_p) {
		struct denode_key old_key = VTODE(fvp)->de_key;
		struct denode_key new_key = VTODE(fvp)->de_key;

		error = msdosfs_pcbmap(VTODE(tdvp),
		    de_cluster(pmp, tmlr->mlr_fndoffset), NULL,
		    &new_key.dk_dirclust, NULL);
		if (error)	/* XXX Back everything out?  Panic?  */
			goto out;
		new_key.dk_diroffset = tmlr->mlr_fndoffset;
		if (new_key.dk_dirclust != MSDOSFSROOT)
			new_key.dk_diroffset &= pmp->pm_crbomask;
		vcache_rekey_enter(pmp->pm_mountp, fvp, &old_key,
		    sizeof(old_key), &new_key, sizeof(new_key));
		VTODE(fvp)->de_key = new_key;
		vcache_rekey_exit(pmp->pm_mountp, fvp, &old_key,
		    sizeof(old_key), &VTODE(fvp)->de_key,
		    sizeof(VTODE(fvp)->de_key));
	}

	/*
	 * If we moved a directory to a new parent directory, then we must
	 * fixup the ".." entry in the moved directory.
	 */
	if (directory_p && reparent_p) {
		error = msdosfs_rename_replace_dotdot(fvp, fdvp, tdvp, cred);
		if (error)
			goto out;
	}

out:;
	if (tvp != NULL)
		*tvp_nlinkp = (error ? 1 : 0);

	genfs_rename_cache_purge(fdvp, fvp, tdvp, tvp);

	if (directory_p)
		VTODE(fvp)->de_flag &=~ DE_RENAME;

	return error;
}

/*
 * msdosfs_gro_remove: Rename an object over another link to itself,
 * effectively removing just the original link.
 */
static int
msdosfs_gro_remove(struct mount *mp, kauth_cred_t cred,
    struct vnode *dvp, struct componentname *cnp, void *de, struct vnode *vp,
    nlink_t *tvp_nlinkp)
{
	struct msdosfs_lookup_results *mlr = de;
	int error;

	KASSERT(mp != NULL);
	KASSERT(dvp != NULL);
	KASSERT(cnp != NULL);
	KASSERT(mlr != NULL);
	KASSERT(vp != NULL);
	KASSERT(dvp != vp);
	KASSERT(dvp->v_mount == mp);
	KASSERT(vp->v_mount == mp);
	KASSERT(dvp->v_type == VDIR);
	KASSERT(vp->v_type != VDIR);
	KASSERT(VOP_ISLOCKED(dvp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(vp) == LK_EXCLUSIVE);

	error = msdosfs_removede(VTODE(dvp), VTODE(vp), mlr);

	*tvp_nlinkp = (error ? 1 : 0);

	return error;
}

/*
 * msdosfs_gro_lookup: Look up and save the lookup results.
 */
static int
msdosfs_gro_lookup(struct mount *mp, struct vnode *dvp,
    struct componentname *cnp, void *de_ret, struct vnode **vp_ret)
{
	struct msdosfs_lookup_results *mlr_ret = de_ret;
	struct vnode *vp;
	int error;

	(void)mp;
	KASSERT(mp != NULL);
	KASSERT(dvp != NULL);
	KASSERT(cnp != NULL);
	KASSERT(mlr_ret != NULL);
	KASSERT(vp_ret != NULL);
	KASSERT(VOP_ISLOCKED(dvp) == LK_EXCLUSIVE);

	/* Kludge cargo-culted from dholland's ufs_rename.  */
	cnp->cn_flags &=~ MODMASK;
	cnp->cn_flags |= (LOCKPARENT | LOCKLEAF);

	error = relookup(dvp, &vp, cnp, 0);
	if ((error == 0) && (vp == NULL)) {
		error = ENOENT;
		goto out;
	}
	if (error)
		return error;

	/*
	 * Thanks to VFS insanity, relookup locks vp, which screws us
	 * in various ways.
	 */
	VOP_UNLOCK(vp);

out:
	*mlr_ret = VTODE(dvp)->de_crap;
	*vp_ret = vp;
	return error;
}

/*
 * msdosfs_rmdired_p: Check whether the directory vp has been rmdired.
 *
 * vp must be locked and referenced.
 */
static bool
msdosfs_rmdired_p(struct vnode *vp)
{

	KASSERT(vp != NULL);
	KASSERT(VOP_ISLOCKED(vp) == LK_EXCLUSIVE);
	KASSERT(vp->v_type == VDIR);

	return (VTODE(vp)->de_FileSize == 0);
}

/*
 * msdosfs_gro_genealogy: Analyze the genealogy of the source and target
 * directories.
 */
static int
msdosfs_gro_genealogy(struct mount *mp, kauth_cred_t cred,
    struct vnode *fdvp, struct vnode *tdvp,
    struct vnode **intermediate_node_ret)
{
	struct msdosfsmount *pmp;
	struct vnode *vp, *dvp;
	unsigned long dotdot_cn;
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

	pmp = VFSTOMSDOSFS(mp);
	KASSERT(pmp != NULL);

	/*
	 * We need to provisionally lock tdvp to keep rmdir from
	 * deleting it -- or any ancestor -- at an inopportune moment.
	 */
	error = msdosfs_gro_lock_directory(mp, tdvp);
	if (error)
		return error;

	vp = tdvp;
	vref(vp);

	for (;;) {
		KASSERT(vp->v_type == VDIR);

		/* Did we hit the root without finding fdvp?  */
		if ((vp->v_vflag & VV_ROOT) != 0) {
			vput(vp);
			*intermediate_node_ret = NULL;
			return 0;
		}

		error = msdosfs_read_dotdot(vp, cred, &dotdot_cn);
		if (error) {
			vput(vp);
			return error;
		}

		/* Did we find that fdvp is an ancestor?  */
		if (VTODE(fdvp)->de_StartCluster == dotdot_cn) {
			/* Unlock vp, but keep it referenced.  */
			VOP_UNLOCK(vp);
			*intermediate_node_ret = vp;
			return 0;
		}

		/* Neither -- keep ascending.  */

		error = msdosfs_deget(pmp, dotdot_cn,
		    (dotdot_cn ? 0 : MSDOSFSROOT_OFS), &dvp);
		vput(vp);
		if (error)
			return error;
		error = vn_lock(dvp, LK_EXCLUSIVE);
		if (error) {
			vrele(dvp);
			return error;
		}

		KASSERT(dvp != NULL);
		KASSERT(dvp->v_type == VDIR);

		vp = dvp;

		if (msdosfs_rmdired_p(vp)) {
			vput(vp);
			return ENOENT;
		}
	}
}

/*
 * msdosfs_read_dotdot: Store in *cn_ret the cluster number of the
 * parent of the directory vp.
 */
static int
msdosfs_read_dotdot(struct vnode *vp, kauth_cred_t cred, unsigned long *cn_ret)
{
	struct msdosfsmount *pmp;
	unsigned long start_cn, cn;
	struct buf *bp;
	struct direntry *ep;
	int error;

	KASSERT(vp != NULL);
	KASSERT(cn_ret != NULL);
	KASSERT(vp->v_type == VDIR);
	KASSERT(VTODE(vp) != NULL);

	pmp = VTODE(vp)->de_pmp;
	KASSERT(pmp != NULL);

	start_cn = VTODE(vp)->de_StartCluster;
	error = bread(pmp->pm_devvp, de_bn2kb(pmp, cntobn(pmp, start_cn)),
	    pmp->pm_bpcluster, 0, &bp);
	if (error)
		return error;

	ep = (struct direntry *)bp->b_data + 1;
	if (((ep->deAttributes & ATTR_DIRECTORY) == ATTR_DIRECTORY) &&
	    (memcmp(ep->deName, "..         ", 11) == 0)) {
		cn = getushort(ep->deStartCluster);
		if (FAT32(pmp))
			cn |= getushort(ep->deHighClust) << 16;
		*cn_ret = cn;
		error = 0;
	} else {
		error = ENOTDIR;
	}

	brelse(bp, 0);

	return error;
}

/*
 * msdosfs_rename_replace_dotdot: Change the target of the `..' entry of
 * the directory vp from fdvp to tdvp.
 */
static int
msdosfs_rename_replace_dotdot(struct vnode *vp,
    struct vnode *fdvp, struct vnode *tdvp,
    kauth_cred_t cred)
{
	struct msdosfsmount *pmp;
	struct direntry *dotdotp;
	struct buf *bp;
	daddr_t bn;
	u_long cn;
	int error;

	pmp = VFSTOMSDOSFS(fdvp->v_mount);

	cn = VTODE(vp)->de_StartCluster;
	if (cn == MSDOSFSROOT) {
		/* this should never happen */
		panic("msdosfs_rename: updating .. in root directory?");
	} else
		bn = cntobn(pmp, cn);

	error = bread(pmp->pm_devvp, de_bn2kb(pmp, bn),
	    pmp->pm_bpcluster, B_MODIFY, &bp);
	if (error)
		return error;

	dotdotp = (struct direntry *)bp->b_data + 1;
	putushort(dotdotp->deStartCluster, VTODE(tdvp)->de_StartCluster);
	if (FAT32(pmp)) {
		putushort(dotdotp->deHighClust,
			VTODE(tdvp)->de_StartCluster >> 16);
	} else {
		putushort(dotdotp->deHighClust, 0);
	}

	error = bwrite(bp);

	return error;
}

/*
 * msdosfs_gro_lock_directory: Lock the directory vp, but fail if it has
 * been rmdir'd.
 */
static int
msdosfs_gro_lock_directory(struct mount *mp, struct vnode *vp)
{
	int error;

	(void)mp;
	KASSERT(vp != NULL);

	error = vn_lock(vp, LK_EXCLUSIVE);
	if (error)
		return error;

	KASSERT(mp != NULL);
	KASSERT(vp->v_mount == mp);

	if (msdosfs_rmdired_p(vp)) {
		VOP_UNLOCK(vp);
		return ENOENT;
	}

	return 0;
}

static const struct genfs_rename_ops msdosfs_genfs_rename_ops = {
	.gro_directory_empty_p		= msdosfs_gro_directory_empty_p,
	.gro_rename_check_possible	= msdosfs_gro_rename_check_possible,
	.gro_rename_check_permitted	= msdosfs_gro_rename_check_permitted,
	.gro_remove_check_possible	= msdosfs_gro_remove_check_possible,
	.gro_remove_check_permitted	= msdosfs_gro_remove_check_permitted,
	.gro_rename			= msdosfs_gro_rename,
	.gro_remove			= msdosfs_gro_remove,
	.gro_lookup			= msdosfs_gro_lookup,
	.gro_genealogy			= msdosfs_gro_genealogy,
	.gro_lock_directory		= msdosfs_gro_lock_directory,
};

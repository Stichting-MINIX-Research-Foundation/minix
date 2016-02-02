/* $NetBSD: udf_rename.c,v 1.12 2014/11/10 19:44:08 riz Exp $ */

/*
 * Copyright (c) 2013 Reinoud Zandijk
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
 * Comments and trivial code from the reference implementation in tmpfs.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: udf_rename.c,v 1.12 2014/11/10 19:44:08 riz Exp $");

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/kauth.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/stat.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/vnode.h>
#include <sys/vnode_if.h>

#include <miscfs/genfs/genfs.h>

#include <fs/udf/ecma167-udf.h>
#include <fs/udf/udf_mount.h>
#include <sys/dirhash.h>

#include "udf.h"
#include "udf_subr.h"
#include "udf_bswap.h"


/* forwards */
static int udf_sane_rename( struct vnode *, struct componentname *,
    struct vnode *, struct componentname *,
    kauth_cred_t, bool);
static bool udf_rmdired_p(struct vnode *);
static int udf_gro_lock_directory(struct mount *, struct vnode *);

static const struct genfs_rename_ops udf_genfs_rename_ops;


#define VTOI(vnode) ((struct udf_node *) (vnode)->v_data)


/*
 * udf_sane_rename: The hairiest vop, with the saner API.
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
udf_sane_rename( struct vnode *fdvp, struct componentname *fcnp,
    struct vnode *tdvp, struct componentname *tcnp,
    kauth_cred_t cred, bool posixly_correct)
{
	DPRINTF(CALL, ("udf_sane_rename '%s' -> '%s'\n",
		fcnp->cn_nameptr, tcnp->cn_nameptr));
	return genfs_sane_rename(&udf_genfs_rename_ops,
	    fdvp, fcnp, NULL, tdvp, tcnp, NULL,
	    cred, posixly_correct);
}


/*
 * udf_rename: the hairiest vop, with the insanest API. Pass to
 * genfs_insane_rename immediately.
 */
int
udf_rename(void *v)
{
	struct vop_rename_args /* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap = v;
	DPRINTF(CALL, ("udf_rename called\n"));
	return genfs_insane_rename(ap, &udf_sane_rename);
}


/*
 * udf_gro_directory_empty_p: return true if the directory vp is empty. dvp is
 * its parent.
 *
 * vp and dvp must be locked and referenced.
 */
static bool
udf_gro_directory_empty_p(struct mount *mp, kauth_cred_t cred,
    struct vnode *vp, struct vnode *dvp)
{
	struct udf_node *udf_node = VTOI(vp);
	int error, isempty;

	KASSERT(mp != NULL);
	KASSERT(vp != NULL);
	KASSERT(dvp != NULL);
	KASSERT(vp != dvp);
	KASSERT(vp->v_mount == mp);
	KASSERT(dvp->v_mount == mp);
	KASSERT(VOP_ISLOCKED(vp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(dvp) == LK_EXCLUSIVE);

	DPRINTF(CALL, ("udf_gro_directory_empty_p called\n"));
	/* make sure our `leaf' node's hash is populated */
	dirhash_get(&udf_node->dir_hash);
	error = udf_dirhash_fill(udf_node);
	if (error) {
		dirhash_put(udf_node->dir_hash);
		/* VERY unlikely, answer its not empty */
		return 0;
	}

	/* check to see if the directory is empty */
	isempty = dirhash_dir_isempty(udf_node->dir_hash);
	dirhash_put(udf_node->dir_hash);

	return isempty;
}


/*
 * udf_gro_rename_check_possible: check whether a rename is possible
 * independent of credentials.
 */
static int
udf_gro_rename_check_possible(struct mount *mp,
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

	DPRINTF(CALL, ("udf_gro_rename_check_possible called\n"));

	/* flags not implemented since they are not defined (yet) in UDF */
	return 0;
}


/*
 * udf_gro_rename_check_permitted: check whether a rename is permitted given
 * our credentials.
 */
static int
udf_gro_rename_check_permitted(struct mount *mp, kauth_cred_t cred,
    struct vnode *fdvp, struct vnode *fvp,
    struct vnode *tdvp, struct vnode *tvp)
{
	struct udf_node *fdir_node = VTOI(fdvp);
	struct udf_node *tdir_node = VTOI(tdvp);
	struct udf_node *f_node = VTOI(fvp);
	struct udf_node *t_node = (tvp? VTOI(tvp): NULL);
	mode_t fdmode, tdmode;
	uid_t fduid, tduid, fuid, tuid;
	gid_t gdummy;

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

	DPRINTF(CALL, ("udf_gro_rename_check_permitted called\n"));
	fdmode = udf_getaccessmode(fdir_node);
	tdmode = udf_getaccessmode(tdir_node);

	udf_getownership(fdir_node, &fduid, &gdummy);
	udf_getownership(tdir_node, &tduid, &gdummy);
	udf_getownership(f_node,    &fuid, &gdummy);

	tuid = 0;
	if (t_node)
		udf_getownership(t_node, &tuid, &gdummy);

	return genfs_ufslike_rename_check_permitted(cred,
	    fdvp, fdmode, fduid,
	    fvp,  fuid,
	    tdvp, tdmode, tduid,
	    tvp,  tuid);
}


/*
 * udf_gro_remove_check_possible: check whether a remove is possible
 * independent of credentials.
 *
 * XXX could check for special attributes?
 */
static int
udf_gro_remove_check_possible(struct mount *mp,
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

	DPRINTF(CALL, ("udf_gro_remove_check_possible called\n"));

	/* flags not implemented since they are not defined (yet) in UDF */
	return 0;
}


/*
 * udf_gro_remove_check_permitted: check whether a remove is permitted given
 * our credentials.
 */
static int
udf_gro_remove_check_permitted(struct mount *mp, kauth_cred_t cred,
    struct vnode *dvp, struct vnode *vp)
{
	struct udf_node *dir_node = VTOI(dvp);
	struct udf_node *udf_node = VTOI(vp);
	mode_t dmode;
	uid_t duid, uid;
	gid_t gdummy;

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

	DPRINTF(CALL, ("udf_gro_remove_check_permitted called\n"));
	dmode = udf_getaccessmode(dir_node);

	udf_getownership(dir_node, &duid, &gdummy);
	udf_getownership(udf_node, &uid,  &gdummy);

	return genfs_ufslike_remove_check_permitted(cred,
	    dvp, dmode, duid,
	    vp, uid);
}


/*
 * udf_gro_rename: actually perform the rename operation.
 */
static int
udf_gro_rename(struct mount *mp, kauth_cred_t cred,
    struct vnode *fdvp, struct componentname *fcnp,
    void *fde, struct vnode *fvp,
    struct vnode *tdvp, struct componentname *tcnp,
    void *tde, struct vnode *tvp)
{
	struct udf_node *fnode, *fdnode, *tnode, *tdnode;
	struct vattr fvap;
	int error;

	(void)cred;
	KASSERT(mp != NULL);
	KASSERT(fdvp != NULL);
	KASSERT(fcnp != NULL);
	KASSERT(fvp != NULL);
	KASSERT(tdvp != NULL);
	KASSERT(tcnp != NULL);
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

	DPRINTF(CALL, ("udf_gro_rename called\n"));
	DPRINTF(NODE, ("udf_gro_rename called : %s -> %s\n",
		fcnp->cn_nameptr, tcnp->cn_nameptr));

	fnode  = VTOI(fvp);
	fdnode = VTOI(fdvp);
	tnode  = (tvp == NULL) ? NULL : VTOI(tvp);
	tdnode = VTOI(tdvp);

	/* get attribute information */
	error = VOP_GETATTR(fvp, &fvap, NULL);
	if (error)
		return error;

	/* remove existing entry if present */
	if (tvp) 
		udf_dir_detach(tdnode->ump, tdnode, tnode, tcnp);

	/* create new directory entry for the node */
	error = udf_dir_attach(tdnode->ump, tdnode, fnode, &fvap, tcnp);
	if (error)
		return error;

	/* unlink old directory entry for the node, if failing, unattach new */
	error = udf_dir_detach(tdnode->ump, fdnode, fnode, fcnp);
	if (error)
		goto rollback_attach;

	if ((fdnode != tdnode) && (fvp->v_type == VDIR)) {
		/* update fnode's '..' entry */
		error = udf_dir_update_rootentry(fnode->ump, fnode, tdnode);
		if (error)
			goto rollback;
	}

	VN_KNOTE(fvp, NOTE_RENAME);
	genfs_rename_cache_purge(fdvp, fvp, tdvp, tvp);
	return 0;

rollback:
	/* 'try' to recover from this situation */
	udf_dir_attach(tdnode->ump, fdnode, fnode, &fvap, fcnp);
rollback_attach:
	udf_dir_detach(tdnode->ump, tdnode, fnode, tcnp);

	return error;
}


/*
 * udf_gro_remove: rename an object over another link to itself, effectively
 * removing just the original link.
 */
static int
udf_gro_remove(struct mount *mp, kauth_cred_t cred,
    struct vnode *dvp, struct componentname *cnp, void *de, struct vnode *vp)
{
	struct udf_node *dir_node, *udf_node;

	KASSERT(mp != NULL);
	KASSERT(dvp != NULL);
	KASSERT(cnp != NULL);
	KASSERT(vp != NULL);
	KASSERT(dvp != vp);
	KASSERT(dvp->v_mount == mp);
	KASSERT(vp->v_mount == mp);
	KASSERT(dvp->v_type == VDIR);
	KASSERT(vp->v_type != VDIR);
	KASSERT(VOP_ISLOCKED(dvp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(vp) == LK_EXCLUSIVE);

	DPRINTF(CALL, ("udf_gro_remove called\n"));

	dir_node = VTOI(dvp);
	udf_node = VTOI(vp);
	udf_dir_detach(dir_node->ump, dir_node, udf_node, cnp);

	return 0;
}


/*
 * udf_gro_lookup: look up and save the lookup results.
 */
static int
udf_gro_lookup(struct mount *mp, struct vnode *dvp,
    struct componentname *cnp, void *de_ret, struct vnode **vp_ret)
{
	struct udf_node *dir_node, *res_node;
	struct long_ad   icb_loc;
	const char *name;
	int namelen, error;
	int found;

	(void)mp;
	KASSERT(mp != NULL);
	KASSERT(dvp != NULL);
	KASSERT(cnp != NULL);
	KASSERT(vp_ret != NULL);
	KASSERT(VOP_ISLOCKED(dvp) == LK_EXCLUSIVE);

	dir_node = VTOI(dvp);

	DPRINTF(CALL, ("udf_gro_lookup called\n"));

	/* lookup filename in the directory; location icb_loc */
	name    = cnp->cn_nameptr;
	namelen = cnp->cn_namelen;
	error = udf_lookup_name_in_dir(dvp, name, namelen,
			&icb_loc, &found);
	if (error)
		return error;
	if (!found)
		return ENOENT;

	DPRINTF(LOOKUP, ("udf_gro_lookup found '%s'\n", name));
	error = udf_get_node(dir_node->ump, &icb_loc, &res_node);
	if (error)
		return error;
	*vp_ret = res_node->vnode;
	VOP_UNLOCK(res_node->vnode);

	return 0;
}


/*
 * udf_rmdired_p: check whether the directory vp has been rmdired.
 *
 * vp must be locked and referenced.
 */
static bool
udf_rmdired_p(struct vnode *vp)
{
	DPRINTF(CALL, ("udf_rmdired_p called\n"));

	KASSERT(vp != NULL);
	KASSERT(VOP_ISLOCKED(vp) == LK_EXCLUSIVE);
	KASSERT(vp->v_type == VDIR);

	return (VTOI(vp)->i_flags & IN_DELETED);
}


/*
 * udf_gro_genealogy: analyze the genealogy of the source and target
 * directories.
 */
static int
udf_gro_genealogy(struct mount *mp, kauth_cred_t cred,
    struct vnode *fdvp, struct vnode *tdvp,
    struct vnode **intermediate_node_ret)
{
	struct udf_mount *ump;
	struct udf_node *parent_node;
	struct vnode *vp, *dvp;
	struct long_ad parent_loc;
	const char *name;
	int namelen;
	int error, found;

	(void)cred;
	KASSERT(mp != NULL);
	KASSERT(fdvp != NULL);
	KASSERT(tdvp != NULL);
	KASSERT(fdvp != tdvp);
	KASSERT(intermediate_node_ret != NULL);
	KASSERT(fdvp->v_mount == mp);
	KASSERT(tdvp->v_mount == mp);
	KASSERT(fdvp->v_type == VDIR);
	KASSERT(tdvp->v_type == VDIR);

	DPRINTF(CALL, ("udf_gro_genealogy called\n"));

	/*
	 * We need to provisionally lock tdvp to keep rmdir from deleting it
	 * -- or any ancestor -- at an inopportune moment.
	 *
	 * XXX WHY is this not in genfs's rename? XXX
	 */
	error = udf_gro_lock_directory(mp, tdvp);
	if (error)
		return error;

	name     = "..";
	namelen  = 2;
	error    = 0;

	ump = VTOI(tdvp)->ump;

	/* if nodes are equal, it is no use looking */
	KASSERT(udf_compare_icb(&VTOI(fdvp)->loc, &VTOI(tdvp)->loc) != 0);

	/* start at destination vnode and walk up the tree */
	vp = tdvp;
	vref(vp);

	for (;;) {
		KASSERT(vp != NULL);
		KASSERT(VOP_ISLOCKED(vp) == LK_EXCLUSIVE);
		KASSERT(vp->v_mount == mp);
		KASSERT(vp->v_type == VDIR);
		KASSERT(!udf_rmdired_p(vp));

		DPRINTF(NODE, ("udf_gro_genealogy : "
			"fdvp %p, looking at vp %p\n",
			fdvp, vp));

		/* sanity check */
		if (vp->v_type != VDIR) {
			vput(vp);
			return ENOTDIR;
		}

		/* go down one level */
		error = udf_lookup_name_in_dir(vp, name, namelen,
			&parent_loc, &found);
		DPRINTF(NODE, ("\tlookup of parent '..' resulted in error %d, "
			"found %d\n", error, found));
		if (!found)
			error = ENOENT;
		if (error) {
			vput(vp);
			return error;
		}

		/* did we encounter the root node? i.e. loop back */
		if (udf_compare_icb(&parent_loc, &VTOI(vp)->loc) == 0) {
			DPRINTF(NODE, ("ROOT found!\n"));
			vput(vp);
			*intermediate_node_ret = NULL;
			return 0;
		}

		/* Did we find that fdvp is an ancestor of tdvp? */
		if (udf_compare_icb(&parent_loc, &VTOI(fdvp)->loc) == 0) {
			DPRINTF(NODE, ("fdvp is ancestor of tdvp\n"));
			*intermediate_node_ret = vp;
			VOP_UNLOCK(vp);
			return 0;
		}

		/*
		 * Unlock vp so that we can lock the parent, but keep child vp
		 * referenced until after we have found the parent, so that
		 * parent_node will not be recycled.
		 */
		DPRINTF(NODE, ("\tgetting the parent node\n"));
		VOP_UNLOCK(vp);
		error = udf_get_node(ump, &parent_loc, &parent_node);
		vrele(vp);
		if (error) 
			return error;

		dvp = parent_node->vnode;

		/* switch */
		KASSERT(dvp != NULL);
		KASSERT(VOP_ISLOCKED(dvp) == LK_EXCLUSIVE);
		vp  = dvp;

		/* sanity check */
		if (vp->v_type != VDIR) {
			/* 
			 * Odd, but can happen if we loose the race and the
			 * '..' node has been recycled.
			 */
			vput(vp);
			return ENOTDIR;
		}

		if (udf_rmdired_p(vp)) {
			vput(vp);
			return ENOENT;
		}
	}
}


/*
 * udf_gro_lock_directory: lock the directory vp, but fail if it has been
 * rmdir'd.
 */
static int
udf_gro_lock_directory(struct mount *mp, struct vnode *vp)
{

	(void)mp;
	KASSERT(mp != NULL);
	KASSERT(vp != NULL);
	KASSERT(vp->v_mount == mp);

	DPRINTF(CALL, ("udf_gro_lock_directory called\n"));
	DPRINTF(LOCKING, ("udf_gro_lock_directory called\n"));

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);

	if (udf_rmdired_p(vp)) {
		VOP_UNLOCK(vp);
		return ENOENT;
	}

	return 0;
}


static const struct genfs_rename_ops udf_genfs_rename_ops = {
	.gro_directory_empty_p		= udf_gro_directory_empty_p,
	.gro_rename_check_possible	= udf_gro_rename_check_possible,
	.gro_rename_check_permitted	= udf_gro_rename_check_permitted,
	.gro_remove_check_possible	= udf_gro_remove_check_possible,
	.gro_remove_check_permitted	= udf_gro_remove_check_permitted,
	.gro_rename			= udf_gro_rename,
	.gro_remove			= udf_gro_remove,
	.gro_lookup			= udf_gro_lookup,
	.gro_genealogy			= udf_gro_genealogy,
	.gro_lock_directory		= udf_gro_lock_directory,
};

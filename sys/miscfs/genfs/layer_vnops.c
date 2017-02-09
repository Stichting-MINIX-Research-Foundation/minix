/*	$NetBSD: layer_vnops.c,v 1.58 2014/05/25 13:51:25 hannken Exp $	*/

/*
 * Copyright (c) 1999 National Aeronautics & Space Administration
 * All rights reserved.
 *
 * This software was written by William Studenmund of the
 * Numerical Aerospace Simulation Facility, NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the National Aeronautics & Space Administration
 *    nor the names of its contributors may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NATIONAL AERONAUTICS & SPACE ADMINISTRATION
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ADMINISTRATION OR CONTRIB-
 * UTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * John Heidemann of the UCLA Ficus project.
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
 *	@(#)null_vnops.c	8.6 (Berkeley) 5/27/95
 *
 * Ancestors:
 *	@(#)lofs_vnops.c	1.2 (Berkeley) 6/18/92
 *	Id: lofs_vnops.c,v 1.11 1992/05/30 10:05:43 jsp Exp jsp
 *	...and...
 *	@(#)null_vnodeops.c 1.20 92/07/07 UCLA Ficus project
 */

/*
 * Generic layer vnode operations.
 *
 * The layer.h, layer_extern.h, layer_vfs.c, and layer_vnops.c files provide
 * the core implementation of stacked file-systems.
 *
 * The layerfs duplicates a portion of the file system name space under
 * a new name.  In this respect, it is similar to the loopback file system.
 * It differs from the loopback fs in two respects: it is implemented using
 * a stackable layers technique, and it is "layerfs-nodes" stack above all
 * lower-layer vnodes, not just over directory vnodes.
 *
 * OPERATION OF LAYERFS
 *
 * The layerfs is the minimum file system layer, bypassing all possible
 * operations to the lower layer for processing there.  The majority of its
 * activity centers on the bypass routine, through which nearly all vnode
 * operations pass.
 *
 * The bypass routine accepts arbitrary vnode operations for handling by
 * the lower layer.  It begins by examining vnode operation arguments and
 * replacing any layered nodes by their lower-layer equivalents.  It then
 * invokes an operation on the lower layer.  Finally, it replaces the
 * layered nodes in the arguments and, if a vnode is returned by the
 * operation, stacks a layered node on top of the returned vnode.
 *
 * The bypass routine in this file, layer_bypass(), is suitable for use
 * by many different layered filesystems. It can be used by multiple
 * filesystems simultaneously. Alternatively, a layered fs may provide
 * its own bypass routine, in which case layer_bypass() should be used as
 * a model. For instance, the main functionality provided by umapfs, the user
 * identity mapping file system, is handled by a custom bypass routine.
 *
 * Typically a layered fs registers its selected bypass routine as the
 * default vnode operation in its vnodeopv_entry_desc table. Additionally
 * the filesystem must store the bypass entry point in the layerm_bypass
 * field of struct layer_mount. All other layer routines in this file will
 * use the layerm_bypass() routine.
 *
 * Although the bypass routine handles most operations outright, a number
 * of operations are special cased and handled by the layerfs.  For instance,
 * layer_getattr() must change the fsid being returned.  While layer_lock()
 * and layer_unlock() must handle any locking for the current vnode as well
 * as pass the lock request down.  layer_inactive() and layer_reclaim() are
 * not bypassed so that they can handle freeing layerfs-specific data.  Also,
 * certain vnode operations (create, mknod, remove, link, rename, mkdir,
 * rmdir, and symlink) change the locking state within the operation.  Ideally
 * these operations should not change the lock state, but should be changed
 * to let the caller of the function unlock them.  Otherwise, all intermediate
 * vnode layers (such as union, umapfs, etc) must catch these functions to do
 * the necessary locking at their layer.
 *
 * INSTANTIATING VNODE STACKS
 *
 * Mounting associates "layerfs-nodes" stack and lower layer, in effect
 * stacking two VFSes.  The initial mount creates a single vnode stack for
 * the root of the new layerfs.  All other vnode stacks are created as a
 * result of vnode operations on this or other layerfs vnode stacks.
 *
 * New vnode stacks come into existence as a result of an operation which
 * returns a vnode.  The bypass routine stacks a layerfs-node above the new
 * vnode before returning it to the caller.
 *
 * For example, imagine mounting a null layer with:
 *
 *	"mount_null /usr/include /dev/layer/null"
 *
 * Changing directory to /dev/layer/null will assign the root layerfs-node,
 * which was created when the null layer was mounted).  Now consider opening
 * "sys".  A layer_lookup() would be performed on the root layerfs-node.
 * This operation would bypass through to the lower layer which would return
 * a vnode representing the UFS "sys".  Then, layer_bypass() builds a
 * layerfs-node aliasing the UFS "sys" and returns this to the caller.
 * Later operations on the layerfs-node "sys" will repeat this process when
 * constructing other vnode stacks.
 *
 * INVOKING OPERATIONS ON LOWER LAYERS
 *
 * There are two techniques to invoke operations on a lower layer when the
 * operation cannot be completely bypassed.  Each method is appropriate in
 * different situations.  In both cases, it is the responsibility of the
 * aliasing layer to make the operation arguments "correct" for the lower
 * layer by mapping any vnode arguments to the lower layer.
 *
 * The first approach is to call the aliasing layer's bypass routine.  This
 * method is most suitable when you wish to invoke the operation currently
 * being handled on the lower layer.  It has the advantage that the bypass
 * routine already must do argument mapping.  An example of this is
 * layer_getattr().
 *
 * A second approach is to directly invoke vnode operations on the lower
 * layer with the VOP_OPERATIONNAME interface.  The advantage of this method
 * is that it is easy to invoke arbitrary operations on the lower layer.
 * The disadvantage is that vnode's arguments must be manually mapped.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: layer_vnops.c,v 1.58 2014/05/25 13:51:25 hannken Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/kmem.h>
#include <sys/buf.h>
#include <sys/kauth.h>

#include <miscfs/genfs/layer.h>
#include <miscfs/genfs/layer_extern.h>
#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>

/*
 * This is the 08-June-99 bypass routine, based on the 10-Apr-92 bypass
 *		routine by John Heidemann.
 *	The new element for this version is that the whole nullfs
 * system gained the concept of locks on the lower node.
 *    The 10-Apr-92 version was optimized for speed, throwing away some
 * safety checks.  It should still always work, but it's not as
 * robust to programmer errors.
 *
 * In general, we map all vnodes going down and unmap them on the way back.
 *
 * Also, some BSD vnode operations have the side effect of vrele'ing
 * their arguments.  With stacking, the reference counts are held
 * by the upper node, not the lower one, so we must handle these
 * side-effects here.  This is not of concern in Sun-derived systems
 * since there are no such side-effects.
 *
 * New for the 08-June-99 version: we also handle operations which unlock
 * the passed-in node (typically they vput the node).
 *
 * This makes the following assumptions:
 * - only one returned vpp
 * - no INOUT vpp's (Sun's vop_open has one of these)
 * - the vnode operation vector of the first vnode should be used
 *   to determine what implementation of the op should be invoked
 * - all mapped vnodes are of our vnode-type (NEEDSWORK:
 *   problems on rmdir'ing mount points and renaming?)
 */
int
layer_bypass(void *v)
{
	struct vop_generic_args /* {
		struct vnodeop_desc *a_desc;
		<other random data follows, presumably>
	} */ *ap = v;
	int (**our_vnodeop_p)(void *);
	struct vnode **this_vp_p;
	int error;
	struct vnode *old_vps[VDESC_MAX_VPS], *vp0;
	struct vnode **vps_p[VDESC_MAX_VPS];
	struct vnode ***vppp;
	struct mount *mp;
	struct vnodeop_desc *descp = ap->a_desc;
	int reles, i, flags;

#ifdef DIAGNOSTIC
	/*
	 * We require at least one vp.
	 */
	if (descp->vdesc_vp_offsets == NULL ||
	    descp->vdesc_vp_offsets[0] == VDESC_NO_OFFSET)
		panic("%s: no vp's in map.\n", __func__);
#endif

	vps_p[0] =
	    VOPARG_OFFSETTO(struct vnode**, descp->vdesc_vp_offsets[0], ap);
	vp0 = *vps_p[0];
	mp = vp0->v_mount;
	flags = MOUNTTOLAYERMOUNT(mp)->layerm_flags;
	our_vnodeop_p = vp0->v_op;

	if (flags & LAYERFS_MBYPASSDEBUG)
		printf("%s: %s\n", __func__, descp->vdesc_name);

	/*
	 * Map the vnodes going in.
	 * Later, we'll invoke the operation based on
	 * the first mapped vnode's operation vector.
	 */
	reles = descp->vdesc_flags;
	for (i = 0; i < VDESC_MAX_VPS; reles >>= 1, i++) {
		if (descp->vdesc_vp_offsets[i] == VDESC_NO_OFFSET)
			break;   /* bail out at end of list */
		vps_p[i] = this_vp_p =
		    VOPARG_OFFSETTO(struct vnode**, descp->vdesc_vp_offsets[i],
		    ap);
		/*
		 * We're not guaranteed that any but the first vnode
		 * are of our type.  Check for and don't map any
		 * that aren't.  (We must always map first vp or vclean fails.)
		 */
		if (i && (*this_vp_p == NULL ||
		    (*this_vp_p)->v_op != our_vnodeop_p)) {
			old_vps[i] = NULL;
		} else {
			old_vps[i] = *this_vp_p;
			*(vps_p[i]) = LAYERVPTOLOWERVP(*this_vp_p);
			/*
			 * XXX - Several operations have the side effect
			 * of vrele'ing their vp's.  We must account for
			 * that.  (This should go away in the future.)
			 */
			if (reles & VDESC_VP0_WILLRELE)
				vref(*this_vp_p);
		}
	}

	/*
	 * Call the operation on the lower layer
	 * with the modified argument structure.
	 */
	error = VCALL(*vps_p[0], descp->vdesc_offset, ap);

	/*
	 * Maintain the illusion of call-by-value
	 * by restoring vnodes in the argument structure
	 * to their original value.
	 */
	reles = descp->vdesc_flags;
	for (i = 0; i < VDESC_MAX_VPS; reles >>= 1, i++) {
		if (descp->vdesc_vp_offsets[i] == VDESC_NO_OFFSET)
			break;   /* bail out at end of list */
		if (old_vps[i]) {
			*(vps_p[i]) = old_vps[i];
			if (reles & VDESC_VP0_WILLRELE)
				vrele(*(vps_p[i]));
		}
	}

	/*
	 * Map the possible out-going vpp
	 * (Assumes that the lower layer always returns
	 * a VREF'ed vpp unless it gets an error.)
	 */
	if (descp->vdesc_vpp_offset != VDESC_NO_OFFSET && !error) {
		vppp = VOPARG_OFFSETTO(struct vnode***,
				 descp->vdesc_vpp_offset, ap);
		/*
		 * Only vop_lookup, vop_create, vop_makedir, vop_mknod
		 * and vop_symlink return vpp's. vop_lookup doesn't call bypass
		 * as a lookup on "." would generate a locking error.
		 * So all the calls which get us here have a unlocked vpp. :-)
		 */
		error = layer_node_create(mp, **vppp, *vppp);
		if (error) {
			vrele(**vppp);
			**vppp = NULL;
		}
	}
	return error;
}

/*
 * We have to carry on the locking protocol on the layer vnodes
 * as we progress through the tree. We also have to enforce read-only
 * if this layer is mounted read-only.
 */
int
layer_lookup(void *v)
{
	struct vop_lookup_v2_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode * a_dvp;
		struct vnode ** a_vpp;
		struct componentname * a_cnp;
	} */ *ap = v;
	struct componentname *cnp = ap->a_cnp;
	struct vnode *dvp, *lvp, *ldvp;
	int error, flags = cnp->cn_flags;

	dvp = ap->a_dvp;

	if ((flags & ISLASTCN) && (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME)) {
		*ap->a_vpp = NULL;
		return EROFS;
	}

	ldvp = LAYERVPTOLOWERVP(dvp);
	ap->a_dvp = ldvp;
	error = VCALL(ldvp, ap->a_desc->vdesc_offset, ap);
	lvp = *ap->a_vpp;
	*ap->a_vpp = NULL;

	if (error == EJUSTRETURN && (flags & ISLASTCN) &&
	    (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME))
		error = EROFS;

	/*
	 * We must do the same locking and unlocking at this layer as
	 * is done in the layers below us.
	 */
	if (ldvp == lvp) {
		/*
		 * Got the same object back, because we looked up ".",
		 * or ".." in the root node of a mount point.
		 * So we make another reference to dvp and return it.
		 */
		vref(dvp);
		*ap->a_vpp = dvp;
		vrele(lvp);
	} else if (lvp != NULL) {
		/* Note: dvp and ldvp are both locked. */
		error = layer_node_create(dvp->v_mount, lvp, ap->a_vpp);
		if (error) {
			vrele(lvp);
		}
	}
	return error;
}

/*
 * Setattr call. Disallow write attempts if the layer is mounted read-only.
 */
int
layer_setattr(void *v)
{
	struct vop_setattr_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
		struct lwp *a_l;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;

  	if ((vap->va_flags != VNOVAL || vap->va_uid != (uid_t)VNOVAL ||
	    vap->va_gid != (gid_t)VNOVAL || vap->va_atime.tv_sec != VNOVAL ||
	    vap->va_mtime.tv_sec != VNOVAL || vap->va_mode != (mode_t)VNOVAL) &&
	    (vp->v_mount->mnt_flag & MNT_RDONLY))
		return EROFS;
	if (vap->va_size != VNOVAL) {
 		switch (vp->v_type) {
 		case VDIR:
 			return EISDIR;
 		case VCHR:
 		case VBLK:
 		case VSOCK:
 		case VFIFO:
			return 0;
		case VREG:
		case VLNK:
 		default:
			/*
			 * Disallow write attempts if the filesystem is
			 * mounted read-only.
			 */
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return EROFS;
		}
	}
	return LAYERFS_DO_BYPASS(vp, ap);
}

/*
 *  We handle getattr only to change the fsid.
 */
int
layer_getattr(void *v)
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
		struct lwp *a_l;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	int error;

	error = LAYERFS_DO_BYPASS(vp, ap);
	if (error) {
		return error;
	}
	/* Requires that arguments be restored. */
	ap->a_vap->va_fsid = vp->v_mount->mnt_stat.f_fsidx.__fsid_val[0];
	return 0;
}

int
layer_access(void *v)
{
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		kauth_cred_t a_cred;
		struct lwp *a_l;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	mode_t mode = ap->a_mode;

	/*
	 * Disallow write attempts on read-only layers;
	 * unless the file is a socket, fifo, or a block or
	 * character device resident on the file system.
	 */
	if (mode & VWRITE) {
		switch (vp->v_type) {
		case VDIR:
		case VLNK:
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return EROFS;
			break;
		default:
			break;
		}
	}
	return LAYERFS_DO_BYPASS(vp, ap);
}

/*
 * We must handle open to be able to catch MNT_NODEV and friends.
 */
int
layer_open(void *v)
{
	struct vop_open_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int a_mode;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	enum vtype lower_type = LAYERVPTOLOWERVP(vp)->v_type;

	if (((lower_type == VBLK) || (lower_type == VCHR)) &&
	    (vp->v_mount->mnt_flag & MNT_NODEV))
		return ENXIO;

	return LAYERFS_DO_BYPASS(vp, ap);
}

/*
 * If vinvalbuf is calling us, it's a "shallow fsync" -- don't bother
 * syncing the underlying vnodes, since they'll be fsync'ed when
 * reclaimed; otherwise, pass it through to the underlying layer.
 *
 * XXX Do we still need to worry about shallow fsync?
 */
int
layer_fsync(void *v)
{
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		kauth_cred_t a_cred;
		int  a_flags;
		off_t offlo;
		off_t offhi;
		struct lwp *a_l;
	} */ *ap = v;
	int error;

	if (ap->a_flags & FSYNC_RECLAIM) {
		return 0;
	}
	if (ap->a_vp->v_type == VBLK || ap->a_vp->v_type == VCHR) {
		error = spec_fsync(v);
		if (error)
			return error;
	}
	return LAYERFS_DO_BYPASS(ap->a_vp, ap);
}

int
layer_inactive(void *v)
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		bool *a_recycle;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	/*
	 * If we did a remove, don't cache the node.
	 */
	*ap->a_recycle = ((VTOLAYER(vp)->layer_flags & LAYERFS_REMOVED) != 0);

	/*
	 * Do nothing (and _don't_ bypass).
	 * Wait to vrele lowervp until reclaim,
	 * so that until then our layer_node is in the
	 * cache and reusable.
	 *
	 * NEEDSWORK: Someday, consider inactive'ing
	 * the lowervp and then trying to reactivate it
	 * with capabilities (v_id)
	 * like they do in the name lookup cache code.
	 * That's too much work for now.
	 */
	VOP_UNLOCK(vp);
	return 0;
}

int
layer_remove(void *v)
{
	struct vop_remove_args /* {
		struct vonde		*a_dvp;
		struct vnode		*a_vp;
		struct componentname	*a_cnp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	int error;

	vref(vp);
	error = LAYERFS_DO_BYPASS(vp, ap);
	if (error == 0) {
		VTOLAYER(vp)->layer_flags |= LAYERFS_REMOVED;
	}
	vrele(vp);

	return error;
}

int
layer_rename(void *v)
{
	struct vop_rename_args  /* {
		struct vnode		*a_fdvp;
		struct vnode		*a_fvp;
		struct componentname	*a_fcnp;
		struct vnode		*a_tdvp;
		struct vnode		*a_tvp;
		struct componentname	*a_tcnp;
	} */ *ap = v;
	struct vnode *fdvp = ap->a_fdvp, *tvp;
	int error;

	tvp = ap->a_tvp;
	if (tvp) {
		if (tvp->v_mount != fdvp->v_mount)
			tvp = NULL;
		else
			vref(tvp);
	}
	error = LAYERFS_DO_BYPASS(fdvp, ap);
	if (tvp) {
		if (error == 0)
			VTOLAYER(tvp)->layer_flags |= LAYERFS_REMOVED;
		vrele(tvp);
	}
	return error;
}

int
layer_rmdir(void *v)
{
	struct vop_rmdir_args /* {
		struct vnode		*a_dvp;
		struct vnode		*a_vp;
		struct componentname	*a_cnp;
	} */ *ap = v;
	int		error;
	struct vnode	*vp = ap->a_vp;

	vref(vp);
	error = LAYERFS_DO_BYPASS(vp, ap);
	if (error == 0) {
		VTOLAYER(vp)->layer_flags |= LAYERFS_REMOVED;
	}
	vrele(vp);

	return error;
}

int
layer_revoke(void *v)
{
        struct vop_revoke_args /* {
		struct vnode *a_vp;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vnode *lvp = LAYERVPTOLOWERVP(vp);
	int error;

	/*
	 * We will most likely end up in vclean which uses the v_usecount
	 * to determine if a vnode is active.  Take an extra reference on
	 * the lower vnode so it will always close and inactivate.
	 */
	vref(lvp);
	error = LAYERFS_DO_BYPASS(vp, ap);
	vrele(lvp);

	return error;
}

int
layer_reclaim(void *v)
{
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
		struct lwp *a_l;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct layer_mount *lmp = MOUNTTOLAYERMOUNT(vp->v_mount);
	struct layer_node *xp = VTOLAYER(vp);
	struct vnode *lowervp = xp->layer_lowervp;

	/*
	 * Note: in vop_reclaim, the node's struct lock has been
	 * decomissioned, so we have to be careful about calling
	 * VOP's on ourself.  We must be careful as VXLOCK is set.
	 */
	if (vp == lmp->layerm_rootvp) {
		/*
		 * Oops! We no longer have a root node. Most likely reason is
		 * that someone forcably unmunted the underlying fs.
		 *
		 * Now getting the root vnode will fail. We're dead. :-(
		 */
		lmp->layerm_rootvp = NULL;
	}
	vcache_remove(vp->v_mount, &lowervp, sizeof(lowervp));
	/* After this assignment, this node will not be re-used. */
	xp->layer_lowervp = NULL;
	kmem_free(vp->v_data, lmp->layerm_size);
	vp->v_data = NULL;
	vrele(lowervp);

	return 0;
}

int
layer_lock(void *v)
{
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vnode *lowervp = LAYERVPTOLOWERVP(vp);
	int flags = ap->a_flags;
	int error;

	if (ISSET(flags, LK_NOWAIT)) {
		error = VOP_LOCK(lowervp, flags);
		if (error)
			return error;
		if (mutex_tryenter(vp->v_interlock)) {
			error = vdead_check(vp, VDEAD_NOWAIT);
			mutex_exit(vp->v_interlock);
		} else
			error = EBUSY;
		if (error)
			VOP_UNLOCK(lowervp);
		return error;
	}

	error = VOP_LOCK(lowervp, flags);
	if (error)
		return error;

	mutex_enter(vp->v_interlock);
	error = vdead_check(vp, VDEAD_NOWAIT);
	if (error) {
		VOP_UNLOCK(lowervp);
		error = vdead_check(vp, 0);
		KASSERT(error == ENOENT);
	}
	mutex_exit(vp->v_interlock);

	return error;
}

/*
 * We just feed the returned vnode up to the caller - there's no need
 * to build a layer node on top of the node on which we're going to do
 * i/o. :-)
 */
int
layer_bmap(void *v)
{
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
	} */ *ap = v;
	struct vnode *vp;

	vp = LAYERVPTOLOWERVP(ap->a_vp);
	ap->a_vp = vp;

	return VCALL(vp, ap->a_desc->vdesc_offset, ap);
}

int
layer_print(void *v)
{
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	printf ("\ttag VT_LAYERFS, vp=%p, lowervp=%p\n", vp, LAYERVPTOLOWERVP(vp));
	return 0;
}

int
layer_getpages(void *v)
{
	struct vop_getpages_args /* {
		struct vnode *a_vp;
		voff_t a_offset;
		struct vm_page **a_m;
		int *a_count;
		int a_centeridx;
		vm_prot_t a_access_type;
		int a_advice;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	KASSERT(mutex_owned(vp->v_interlock));

	if (ap->a_flags & PGO_LOCKED) {
		return EBUSY;
	}
	ap->a_vp = LAYERVPTOLOWERVP(vp);
	KASSERT(vp->v_interlock == ap->a_vp->v_interlock);

	/* Just pass the request on to the underlying layer. */
	return VCALL(ap->a_vp, VOFFSET(vop_getpages), ap);
}

int
layer_putpages(void *v)
{
	struct vop_putpages_args /* {
		struct vnode *a_vp;
		voff_t a_offlo;
		voff_t a_offhi;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	KASSERT(mutex_owned(vp->v_interlock));

	ap->a_vp = LAYERVPTOLOWERVP(vp);
	KASSERT(vp->v_interlock == ap->a_vp->v_interlock);

	if (ap->a_flags & PGO_RECLAIM) {
		mutex_exit(vp->v_interlock);
		return 0;
	}

	/* Just pass the request on to the underlying layer. */
	return VCALL(ap->a_vp, VOFFSET(vop_putpages), ap);
}

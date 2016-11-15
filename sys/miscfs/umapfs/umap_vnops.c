/*	$NetBSD: umap_vnops.c,v 1.57 2014/11/09 18:08:07 maxv Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * the UCLA Ficus project.
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
 *	@(#)umap_vnops.c	8.6 (Berkeley) 5/22/95
 */

/*
 * Umap Layer
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: umap_vnops.c,v 1.57 2014/11/09 18:08:07 maxv Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/buf.h>
#include <sys/kauth.h>

#include <miscfs/umapfs/umap.h>
#include <miscfs/genfs/genfs.h>
#include <miscfs/genfs/layer_extern.h>

/*
 * Note: If the LAYERFS_MBYPASSDEBUG flag is set, it is possible
 * that the debug printing will bomb out, because kauth routines
 * do not handle NOCRED or FSCRED like other credentials and end
 * up dereferencing an inappropriate pointer.
 *
 * That should be fixed in kauth rather than here.
 */

int	umap_lookup(void *);
int	umap_getattr(void *);
int	umap_print(void *);
int	umap_rename(void *);

/*
 * Global vfs data structures
 */
/*
 * XXX - strategy, bwrite are hand coded currently.  They should
 * go away with a merged buffer/block cache.
 *
 */
int (**umap_vnodeop_p)(void *);
const struct vnodeopv_entry_desc umap_vnodeop_entries[] = {
	{ &vop_default_desc,	umap_bypass },

	{ &vop_lookup_desc,	umap_lookup },
	{ &vop_getattr_desc,	umap_getattr },
	{ &vop_print_desc,	umap_print },
	{ &vop_rename_desc,	umap_rename },

	{ &vop_fsync_desc,	layer_fsync },
	{ &vop_inactive_desc,	layer_inactive },
	{ &vop_reclaim_desc,	layer_reclaim },
	{ &vop_lock_desc,	layer_lock },
	{ &vop_open_desc,	layer_open },
	{ &vop_setattr_desc,	layer_setattr },
	{ &vop_access_desc,	layer_access },
	{ &vop_remove_desc,	layer_remove },
	{ &vop_revoke_desc,	layer_revoke },
	{ &vop_rmdir_desc,	layer_rmdir },

	{ &vop_bmap_desc,	layer_bmap },
	{ &vop_getpages_desc,	layer_getpages },
	{ &vop_putpages_desc,	layer_putpages },

	{ NULL, NULL }
};
const struct vnodeopv_desc umapfs_vnodeop_opv_desc =
	{ &umap_vnodeop_p, umap_vnodeop_entries };

/*
 * This is the 08-June-1999 bypass routine.
 * See layer_vnops.c:layer_bypass for more details.
 */
int
umap_bypass(void *v)
{
	struct vop_generic_args /* {
		struct vnodeop_desc *a_desc;
		<other random data follows, presumably>
	} */ *ap = v;
	int (**our_vnodeop_p)(void *);
	kauth_cred_t *credpp = NULL, credp = 0;
	kauth_cred_t savecredp = 0, savecompcredp = 0;
	kauth_cred_t compcredp = 0;
	struct vnode **this_vp_p;
	int error;
	struct vnode *old_vps[VDESC_MAX_VPS], *vp0;
	struct vnode **vps_p[VDESC_MAX_VPS];
	struct vnode ***vppp;
	struct vnodeop_desc *descp = ap->a_desc;
	int reles, i, flags;
	struct componentname **compnamepp = 0;

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
	flags = MOUNTTOUMAPMOUNT(vp0->v_mount)->umapm_flags;
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
			*(vps_p[i]) = UMAPVPTOLOWERVP(*this_vp_p);
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
	 * Fix the credentials.  (That's the purpose of this layer.)
	 */

	if (descp->vdesc_cred_offset != VDESC_NO_OFFSET) {

		credpp = VOPARG_OFFSETTO(kauth_cred_t*,
		    descp->vdesc_cred_offset, ap);

		/* Save old values */

		savecredp = *credpp;
		if (savecredp != NOCRED && savecredp != FSCRED)
			*credpp = kauth_cred_dup(savecredp);
		credp = *credpp;

		if ((flags & LAYERFS_MBYPASSDEBUG) &&
		    kauth_cred_geteuid(credp) != 0)
			printf("umap_bypass: user was %d, group %d\n",
			    kauth_cred_geteuid(credp), kauth_cred_getegid(credp));

		/* Map all ids in the credential structure. */

		umap_mapids(vp0->v_mount, credp);

		if ((flags & LAYERFS_MBYPASSDEBUG) &&
		    kauth_cred_geteuid(credp) != 0)
			printf("umap_bypass: user now %d, group %d\n",
			    kauth_cred_geteuid(credp), kauth_cred_getegid(credp));
	}

	/* BSD often keeps a credential in the componentname structure
	 * for speed.  If there is one, it better get mapped, too.
	 */

	if (descp->vdesc_componentname_offset != VDESC_NO_OFFSET) {

		compnamepp = VOPARG_OFFSETTO(struct componentname**,
		    descp->vdesc_componentname_offset, ap);

		savecompcredp = (*compnamepp)->cn_cred;
		if (savecompcredp != NOCRED && savecompcredp != FSCRED)
			(*compnamepp)->cn_cred = kauth_cred_dup(savecompcredp);
		compcredp = (*compnamepp)->cn_cred;

		if ((flags & LAYERFS_MBYPASSDEBUG) &&
		    kauth_cred_geteuid(compcredp) != 0)
			printf("umap_bypass: component credit user was %d, group %d\n",
			    kauth_cred_geteuid(compcredp), kauth_cred_getegid(compcredp));

		/* Map all ids in the credential structure. */

		umap_mapids(vp0->v_mount, compcredp);

		if ((flags & LAYERFS_MBYPASSDEBUG) &&
		    kauth_cred_geteuid(compcredp) != 0)
			printf("umap_bypass: component credit user now %d, group %d\n",
			    kauth_cred_geteuid(compcredp), kauth_cred_getegid(compcredp));
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
		error = layer_node_create(old_vps[0]->v_mount, **vppp, *vppp);
		if (error) {
			vrele(**vppp);
			**vppp = NULL;
		}
	}

	/*
	 * Free duplicate cred structure and restore old one.
	 */
	if (descp->vdesc_cred_offset != VDESC_NO_OFFSET) {
		if ((flags & LAYERFS_MBYPASSDEBUG) && credp &&
		    kauth_cred_geteuid(credp) != 0)
			printf("umap_bypass: returning-user was %d\n",
			    kauth_cred_geteuid(credp));

		if (savecredp != NOCRED && savecredp != FSCRED && credpp) {
			kauth_cred_free(credp);
			*credpp = savecredp;
			if ((flags & LAYERFS_MBYPASSDEBUG) && credpp &&
			    kauth_cred_geteuid(*credpp) != 0)
			 	printf("umap_bypass: returning-user now %d\n\n",
				    kauth_cred_geteuid(savecredp));
		}
	}

	if (descp->vdesc_componentname_offset != VDESC_NO_OFFSET) {
		if ((flags & LAYERFS_MBYPASSDEBUG) && compcredp &&
		    kauth_cred_geteuid(compcredp) != 0)
			printf("umap_bypass: returning-component-user was %d\n",
			    kauth_cred_geteuid(compcredp));

		if (savecompcredp != NOCRED && savecompcredp != FSCRED) {
			kauth_cred_free(compcredp);
			(*compnamepp)->cn_cred = savecompcredp;
			if ((flags & LAYERFS_MBYPASSDEBUG) && savecompcredp &&
			    kauth_cred_geteuid(savecompcredp) != 0)
			 	printf("umap_bypass: returning-component-user now %d\n",
				    kauth_cred_geteuid(savecompcredp));
		}
	}

	return (error);
}

/*
 * This is based on the 08-June-1999 bypass routine.
 * See layer_vnops.c:layer_bypass for more details.
 */
int
umap_lookup(void *v)
{
	struct vop_lookup_v2_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode * a_dvp;
		struct vnode ** a_vpp;
		struct componentname * a_cnp;
	} */ *ap = v;
	struct componentname *cnp = ap->a_cnp;
	kauth_cred_t savecompcredp = NULL;
	kauth_cred_t compcredp = NULL;
	struct vnode *dvp, *vp, *ldvp;
	struct mount *mp;
	int error;
	int flags, cnf = cnp->cn_flags;

	dvp = ap->a_dvp;
	mp = dvp->v_mount;

	if ((cnf & ISLASTCN) && (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
		(cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);

	flags = MOUNTTOUMAPMOUNT(mp)->umapm_flags;
	ldvp = UMAPVPTOLOWERVP(dvp);

	if (flags & LAYERFS_MBYPASSDEBUG)
		printf("umap_lookup\n");

	/*
	 * Fix the credentials.  (That's the purpose of this layer.)
	 *
	 * BSD often keeps a credential in the componentname structure
	 * for speed.  If there is one, it better get mapped, too.
	 */

	if ((savecompcredp = cnp->cn_cred)) {
		compcredp = kauth_cred_dup(savecompcredp);
		cnp->cn_cred = compcredp;

		if ((flags & LAYERFS_MBYPASSDEBUG) &&
		    kauth_cred_geteuid(compcredp) != 0)
			printf("umap_lookup: component credit user was %d, group %d\n",
			    kauth_cred_geteuid(compcredp), kauth_cred_getegid(compcredp));

		/* Map all ids in the credential structure. */
		umap_mapids(mp, compcredp);
	}

	if ((flags & LAYERFS_MBYPASSDEBUG) && compcredp &&
	    kauth_cred_geteuid(compcredp) != 0)
		printf("umap_lookup: component credit user now %d, group %d\n",
		    kauth_cred_geteuid(compcredp), kauth_cred_getegid(compcredp));

	ap->a_dvp = ldvp;
	error = VCALL(ldvp, ap->a_desc->vdesc_offset, ap);
	vp = *ap->a_vpp;
	*ap->a_vpp = NULL;

	if (error == EJUSTRETURN && (cnf & ISLASTCN) &&
	    (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME))
		error = EROFS;

	/* Do locking fixup as appropriate. See layer_lookup() for info */
	if (ldvp == vp) {
		*ap->a_vpp = dvp;
		vref(dvp);
		vrele(vp);
	} else if (vp != NULL) {
		error = layer_node_create(mp, vp, ap->a_vpp);
		if (error) {
			vrele(vp);
		}
	}

	/*
	 * Free duplicate cred structure and restore old one.
	 */
	if ((flags & LAYERFS_MBYPASSDEBUG) && compcredp &&
	    kauth_cred_geteuid(compcredp) != 0)
		printf("umap_lookup: returning-component-user was %d\n",
			    kauth_cred_geteuid(compcredp));

	if (savecompcredp != NOCRED && savecompcredp != FSCRED) {
		if (compcredp)
			kauth_cred_free(compcredp);
		cnp->cn_cred = savecompcredp;
		if ((flags & LAYERFS_MBYPASSDEBUG) && savecompcredp &&
		    kauth_cred_geteuid(savecompcredp) != 0)
		 	printf("umap_lookup: returning-component-user now %d\n",
			    kauth_cred_geteuid(savecompcredp));
	}

	return (error);
}

/*
 *  We handle getattr to change the fsid.
 */
int
umap_getattr(void *v)
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
		struct lwp *a_l;
	} */ *ap = v;
	uid_t uid;
	gid_t gid;
	int error, tmpid, nentries, gnentries, flags;
	u_long (*mapdata)[2];
	u_long (*gmapdata)[2];
	struct vnode **vp1p;
	const struct vnodeop_desc *descp = ap->a_desc;

	if ((error = umap_bypass(ap)) != 0)
		return (error);
	/* Requires that arguments be restored. */
	ap->a_vap->va_fsid = ap->a_vp->v_mount->mnt_stat.f_fsidx.__fsid_val[0];

	flags = MOUNTTOUMAPMOUNT(ap->a_vp->v_mount)->umapm_flags;
	/*
	 * Umap needs to map the uid and gid returned by a stat
	 * into the proper values for this site.  This involves
	 * finding the returned uid in the mapping information,
	 * translating it into the uid on the other end,
	 * and filling in the proper field in the vattr
	 * structure pointed to by ap->a_vap.  The group
	 * is easier, since currently all groups will be
	 * translate to the NULLGROUP.
	 */

	/* Find entry in map */

	uid = ap->a_vap->va_uid;
	gid = ap->a_vap->va_gid;
	if ((flags & LAYERFS_MBYPASSDEBUG))
		printf("umap_getattr: mapped uid = %d, mapped gid = %d\n", uid,
		    gid);

	vp1p = VOPARG_OFFSETTO(struct vnode**, descp->vdesc_vp_offsets[0], ap);
	nentries =  MOUNTTOUMAPMOUNT((*vp1p)->v_mount)->info_nentries;
	mapdata =  (MOUNTTOUMAPMOUNT((*vp1p)->v_mount)->info_mapdata);
	gnentries =  MOUNTTOUMAPMOUNT((*vp1p)->v_mount)->info_gnentries;
	gmapdata =  (MOUNTTOUMAPMOUNT((*vp1p)->v_mount)->info_gmapdata);

	/* Reverse map the uid for the vnode.  Since it's a reverse
		map, we can't use umap_mapids() to do it. */

	tmpid = umap_reverse_findid(uid, mapdata, nentries);

	if (tmpid != -1) {
		ap->a_vap->va_uid = (uid_t) tmpid;
		if ((flags & LAYERFS_MBYPASSDEBUG))
			printf("umap_getattr: original uid = %d\n", uid);
	} else
		ap->a_vap->va_uid = (uid_t) NOBODY;

	/* Reverse map the gid for the vnode. */

	tmpid = umap_reverse_findid(gid, gmapdata, gnentries);

	if (tmpid != -1) {
		ap->a_vap->va_gid = (gid_t) tmpid;
		if ((flags & LAYERFS_MBYPASSDEBUG))
			printf("umap_getattr: original gid = %d\n", gid);
	} else
		ap->a_vap->va_gid = (gid_t) NULLGROUP;

	return (0);
}

int
umap_print(void *v)
{
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	printf("\ttag VT_UMAPFS, vp=%p, lowervp=%p\n", vp,
	    UMAPVPTOLOWERVP(vp));
	return (0);
}

int
umap_rename(void *v)
{
	struct vop_rename_args  /* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap = v;
	int error, flags;
	struct componentname *compnamep;
	kauth_cred_t compcredp, savecompcredp;
	struct vnode *vp;
	struct vnode *tvp;

	/*
	 * Rename is irregular, having two componentname structures.
	 * We need to map the cre in the second structure,
	 * and then bypass takes care of the rest.
	 */

	vp = ap->a_fdvp;
	flags = MOUNTTOUMAPMOUNT(vp->v_mount)->umapm_flags;
	compnamep = ap->a_tcnp;
	compcredp = compnamep->cn_cred;

	savecompcredp = compcredp;
	compcredp = compnamep->cn_cred = kauth_cred_dup(savecompcredp);

	if ((flags & LAYERFS_MBYPASSDEBUG) &&
	    kauth_cred_geteuid(compcredp) != 0)
		printf("umap_rename: rename component credit user was %d, group %d\n",
		    kauth_cred_geteuid(compcredp), kauth_cred_getegid(compcredp));

	/* Map all ids in the credential structure. */

	umap_mapids(vp->v_mount, compcredp);

	if ((flags & LAYERFS_MBYPASSDEBUG) &&
	    kauth_cred_geteuid(compcredp) != 0)
		printf("umap_rename: rename component credit user now %d, group %d\n",
		    kauth_cred_geteuid(compcredp), kauth_cred_getegid(compcredp));

	tvp = ap->a_tvp;
	if (tvp) {
		if (tvp->v_mount != vp->v_mount)
			tvp = NULL;
		else
			vref(tvp);
	}
	error = umap_bypass(ap);
	if (tvp) {
		if (error == 0)
			VTOLAYER(tvp)->layer_flags |= LAYERFS_REMOVED;
		vrele(tvp);
	}

	/* Restore the additional mapped componentname cred structure. */

	kauth_cred_free(compcredp);
	compnamep->cn_cred = savecompcredp;

	return error;
}

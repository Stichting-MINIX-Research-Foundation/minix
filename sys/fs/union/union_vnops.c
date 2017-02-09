/*	$NetBSD: union_vnops.c,v 1.63 2015/04/20 23:03:08 riastradh Exp $	*/

/*
 * Copyright (c) 1992, 1993, 1994, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)union_vnops.c	8.33 (Berkeley) 7/31/95
 */

/*
 * Copyright (c) 1992, 1993, 1994, 1995 Jan-Simon Pendry.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)union_vnops.c	8.33 (Berkeley) 7/31/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: union_vnops.c,v 1.63 2015/04/20 23:03:08 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/kauth.h>

#include <fs/union/union.h>
#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>

int union_lookup(void *);
int union_create(void *);
int union_whiteout(void *);
int union_mknod(void *);
int union_open(void *);
int union_close(void *);
int union_access(void *);
int union_getattr(void *);
int union_setattr(void *);
int union_read(void *);
int union_write(void *);
int union_ioctl(void *);
int union_poll(void *);
int union_revoke(void *);
int union_mmap(void *);
int union_fsync(void *);
int union_seek(void *);
int union_remove(void *);
int union_link(void *);
int union_rename(void *);
int union_mkdir(void *);
int union_rmdir(void *);
int union_symlink(void *);
int union_readdir(void *);
int union_readlink(void *);
int union_abortop(void *);
int union_inactive(void *);
int union_reclaim(void *);
int union_lock(void *);
int union_unlock(void *);
int union_bmap(void *);
int union_print(void *);
int union_islocked(void *);
int union_pathconf(void *);
int union_advlock(void *);
int union_strategy(void *);
int union_bwrite(void *);
int union_getpages(void *);
int union_putpages(void *);
int union_kqfilter(void *);

static int union_lookup1(struct vnode *, struct vnode **,
			      struct vnode **, struct componentname *);


/*
 * Global vfs data structures
 */
int (**union_vnodeop_p)(void *);
const struct vnodeopv_entry_desc union_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, union_lookup },		/* lookup */
	{ &vop_create_desc, union_create },		/* create */
	{ &vop_whiteout_desc, union_whiteout },		/* whiteout */
	{ &vop_mknod_desc, union_mknod },		/* mknod */
	{ &vop_open_desc, union_open },			/* open */
	{ &vop_close_desc, union_close },		/* close */
	{ &vop_access_desc, union_access },		/* access */
	{ &vop_getattr_desc, union_getattr },		/* getattr */
	{ &vop_setattr_desc, union_setattr },		/* setattr */
	{ &vop_read_desc, union_read },			/* read */
	{ &vop_write_desc, union_write },		/* write */
	{ &vop_fallocate_desc, genfs_eopnotsupp },	/* fallocate */
	{ &vop_fdiscard_desc, genfs_eopnotsupp },	/* fdiscard */
	{ &vop_ioctl_desc, union_ioctl },		/* ioctl */
	{ &vop_poll_desc, union_poll },			/* select */
	{ &vop_revoke_desc, union_revoke },		/* revoke */
	{ &vop_mmap_desc, union_mmap },			/* mmap */
	{ &vop_fsync_desc, union_fsync },		/* fsync */
	{ &vop_seek_desc, union_seek },			/* seek */
	{ &vop_remove_desc, union_remove },		/* remove */
	{ &vop_link_desc, union_link },			/* link */
	{ &vop_rename_desc, union_rename },		/* rename */
	{ &vop_mkdir_desc, union_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, union_rmdir },		/* rmdir */
	{ &vop_symlink_desc, union_symlink },		/* symlink */
	{ &vop_readdir_desc, union_readdir },		/* readdir */
	{ &vop_readlink_desc, union_readlink },		/* readlink */
	{ &vop_abortop_desc, union_abortop },		/* abortop */
	{ &vop_inactive_desc, union_inactive },		/* inactive */
	{ &vop_reclaim_desc, union_reclaim },		/* reclaim */
	{ &vop_lock_desc, union_lock },			/* lock */
	{ &vop_unlock_desc, union_unlock },		/* unlock */
	{ &vop_bmap_desc, union_bmap },			/* bmap */
	{ &vop_strategy_desc, union_strategy },		/* strategy */
	{ &vop_bwrite_desc, union_bwrite },		/* bwrite */
	{ &vop_print_desc, union_print },		/* print */
	{ &vop_islocked_desc, union_islocked },		/* islocked */
	{ &vop_pathconf_desc, union_pathconf },		/* pathconf */
	{ &vop_advlock_desc, union_advlock },		/* advlock */
	{ &vop_getpages_desc, union_getpages },		/* getpages */
	{ &vop_putpages_desc, union_putpages },		/* putpages */
	{ &vop_kqfilter_desc, union_kqfilter },		/* kqfilter */
	{ NULL, NULL }
};
const struct vnodeopv_desc union_vnodeop_opv_desc =
	{ &union_vnodeop_p, union_vnodeop_entries };

#define NODE_IS_SPECIAL(vp) \
	((vp)->v_type == VBLK || (vp)->v_type == VCHR || \
	(vp)->v_type == VSOCK || (vp)->v_type == VFIFO)

static int
union_lookup1(struct vnode *udvp, struct vnode **dvpp, struct vnode **vpp,
	struct componentname *cnp)
{
	int error;
	struct vnode *tdvp;
	struct vnode *dvp;
	struct mount *mp;

	dvp = *dvpp;

	/*
	 * If stepping up the directory tree, check for going
	 * back across the mount point, in which case do what
	 * lookup would do by stepping back down the mount
	 * hierarchy.
	 */
	if (cnp->cn_flags & ISDOTDOT) {
		while ((dvp != udvp) && (dvp->v_vflag & VV_ROOT)) {
			/*
			 * Don't do the NOCROSSMOUNT check
			 * at this level.  By definition,
			 * union fs deals with namespaces, not
			 * filesystems.
			 */
			tdvp = dvp;
			*dvpp = dvp = dvp->v_mount->mnt_vnodecovered;
			VOP_UNLOCK(tdvp);
			vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY);
		}
	}

        error = VOP_LOOKUP(dvp, &tdvp, cnp);
	if (error)
		return (error);
	if (dvp != tdvp) {
		if (cnp->cn_flags & ISDOTDOT)
			VOP_UNLOCK(dvp);
		error = vn_lock(tdvp, LK_EXCLUSIVE);
		if (cnp->cn_flags & ISDOTDOT)
			vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY);
		if (error) {
			vrele(tdvp);
			return error;
		}
		dvp = tdvp;
	}

	/*
	 * Lastly check if the current node is a mount point in
	 * which case walk up the mount hierarchy making sure not to
	 * bump into the root of the mount tree (ie. dvp != udvp).
	 */
	while (dvp != udvp && (dvp->v_type == VDIR) &&
	       (mp = dvp->v_mountedhere)) {
		if (vfs_busy(mp, NULL))
			continue;
		vput(dvp);
		error = VFS_ROOT(mp, &tdvp);
		vfs_unbusy(mp, false, NULL);
		if (error) {
			return (error);
		}
		dvp = tdvp;
	}

	*vpp = dvp;
	return (0);
}

int
union_lookup(void *v)
{
	struct vop_lookup_v2_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap = v;
	int error;
	int uerror, lerror;
	struct vnode *uppervp, *lowervp;
	struct vnode *upperdvp, *lowerdvp;
	struct vnode *dvp = ap->a_dvp;
	struct union_node *dun = VTOUNION(dvp);
	struct componentname *cnp = ap->a_cnp;
	struct union_mount *um = MOUNTTOUNIONMOUNT(dvp->v_mount);
	kauth_cred_t saved_cred = NULL;
	int iswhiteout;
	struct vattr va;

#ifdef notyet
	if (cnp->cn_namelen == 3 &&
			cnp->cn_nameptr[2] == '.' &&
			cnp->cn_nameptr[1] == '.' &&
			cnp->cn_nameptr[0] == '.') {
		dvp = *ap->a_vpp = LOWERVP(ap->a_dvp);
		if (dvp == NULLVP)
			return (ENOENT);
		vref(dvp);
		vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY);
		return (0);
	}
#endif

	if ((cnp->cn_flags & ISLASTCN) &&
	    (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);

start:
	upperdvp = dun->un_uppervp;
	lowerdvp = dun->un_lowervp;
	uppervp = NULLVP;
	lowervp = NULLVP;
	iswhiteout = 0;

	/*
	 * do the lookup in the upper level.
	 * if that level comsumes additional pathnames,
	 * then assume that something special is going
	 * on and just return that vnode.
	 */
	if (upperdvp != NULLVP) {
		uerror = union_lookup1(um->um_uppervp, &upperdvp,
					&uppervp, cnp);
		if (cnp->cn_consume != 0) {
			if (uppervp != upperdvp)
				VOP_UNLOCK(uppervp);
			*ap->a_vpp = uppervp;
			return (uerror);
		}
		if (uerror == ENOENT || uerror == EJUSTRETURN) {
			if (cnp->cn_flags & ISWHITEOUT) {
				iswhiteout = 1;
			} else if (lowerdvp != NULLVP) {
				lerror = VOP_GETATTR(upperdvp, &va,
					cnp->cn_cred);
				if (lerror == 0 && (va.va_flags & OPAQUE))
					iswhiteout = 1;
			}
		}
	} else {
		uerror = ENOENT;
	}

	/*
	 * in a similar way to the upper layer, do the lookup
	 * in the lower layer.   this time, if there is some
	 * component magic going on, then vput whatever we got
	 * back from the upper layer and return the lower vnode
	 * instead.
	 */
	if (lowerdvp != NULLVP && !iswhiteout) {
		int nameiop;

		vn_lock(lowerdvp, LK_EXCLUSIVE | LK_RETRY);

		/*
		 * Only do a LOOKUP on the bottom node, since
		 * we won't be making changes to it anyway.
		 */
		nameiop = cnp->cn_nameiop;
		cnp->cn_nameiop = LOOKUP;
		if (um->um_op == UNMNT_BELOW) {
			saved_cred = cnp->cn_cred;
			cnp->cn_cred = um->um_cred;
		}

		/*
		 * we shouldn't have to worry about locking interactions
		 * between the lower layer and our union layer (w.r.t.
		 * `..' processing) because we don't futz with lowervp
		 * locks in the union-node instantiation code path.
		 */
		lerror = union_lookup1(um->um_lowervp, &lowerdvp,
				&lowervp, cnp);
		if (um->um_op == UNMNT_BELOW)
			cnp->cn_cred = saved_cred;
		cnp->cn_nameiop = nameiop;

		if (lowervp != lowerdvp)
			VOP_UNLOCK(lowerdvp);

		if (cnp->cn_consume != 0) {
			if (uppervp != NULLVP) {
				if (uppervp == upperdvp)
					vrele(uppervp);
				else
					vput(uppervp);
				uppervp = NULLVP;
			}
			*ap->a_vpp = lowervp;
			return (lerror);
		}
	} else {
		lerror = ENOENT;
		if ((cnp->cn_flags & ISDOTDOT) && dun->un_pvp != NULLVP) {
			lowervp = LOWERVP(dun->un_pvp);
			if (lowervp != NULLVP) {
				vref(lowervp);
				vn_lock(lowervp, LK_EXCLUSIVE | LK_RETRY);
				lerror = 0;
			}
		}
	}

	/*
	 * EJUSTRETURN is used by underlying filesystems to indicate that
	 * a directory modification op was started successfully.
	 * This will only happen in the upper layer, since
	 * the lower layer only does LOOKUPs.
	 * If this union is mounted read-only, bounce it now.
	 */

	if ((uerror == EJUSTRETURN) && (cnp->cn_flags & ISLASTCN) &&
	    (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    ((cnp->cn_nameiop == CREATE) || (cnp->cn_nameiop == RENAME)))
		uerror = EROFS;

	/*
	 * at this point, we have uerror and lerror indicating
	 * possible errors with the lookups in the upper and lower
	 * layers.  additionally, uppervp and lowervp are (locked)
	 * references to existing vnodes in the upper and lower layers.
	 *
	 * there are now three cases to consider.
	 * 1. if both layers returned an error, then return whatever
	 *    error the upper layer generated.
	 *
	 * 2. if the top layer failed and the bottom layer succeeded
	 *    then two subcases occur.
	 *    a.  the bottom vnode is not a directory, in which
	 *	  case just return a new union vnode referencing
	 *	  an empty top layer and the existing bottom layer.
	 *    b.  the bottom vnode is a directory, in which case
	 *	  create a new directory in the top-level and
	 *	  continue as in case 3.
	 *
	 * 3. if the top layer succeeded then return a new union
	 *    vnode referencing whatever the new top layer and
	 *    whatever the bottom layer returned.
	 */

	*ap->a_vpp = NULLVP;


	/* case 1. */
	if ((uerror != 0) && (lerror != 0)) {
		return (uerror);
	}

	/* case 2. */
	if (uerror != 0 /* && (lerror == 0) */ ) {
		if (lowervp->v_type == VDIR) { /* case 2b. */
			/*
			 * We may be racing another process to make the
			 * upper-level shadow directory.  Be careful with
			 * locks/etc!
			 * If we have to create a shadow directory and want
			 * to commit the node we have to restart the lookup
			 * to get the componentname right.
			 */
			if (upperdvp) {
				VOP_UNLOCK(upperdvp);
				uerror = union_mkshadow(um, upperdvp, cnp,
				    &uppervp);
				vn_lock(upperdvp, LK_EXCLUSIVE | LK_RETRY);
				if (uerror == 0 && cnp->cn_nameiop != LOOKUP) {
					vrele(uppervp);
					if (lowervp != NULLVP)
						vput(lowervp);
					goto start;
				}
			}
			if (uerror) {
				if (lowervp != NULLVP) {
					vput(lowervp);
					lowervp = NULLVP;
				}
				return (uerror);
			}
		}
	} else { /* uerror == 0 */
		if (uppervp != upperdvp)
			VOP_UNLOCK(uppervp);
	}

	if (lowervp != NULLVP)
		VOP_UNLOCK(lowervp);

	error = union_allocvp(ap->a_vpp, dvp->v_mount, dvp, upperdvp, cnp,
			      uppervp, lowervp, 1);

	if (error) {
		if (uppervp != NULLVP)
			vrele(uppervp);
		if (lowervp != NULLVP)
			vrele(lowervp);
		return error;
	}

	return 0;
}

int
union_create(void *v)
{
	struct vop_create_v3_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	struct union_node *un = VTOUNION(ap->a_dvp);
	struct vnode *dvp = un->un_uppervp;
	struct componentname *cnp = ap->a_cnp;

	if (dvp != NULLVP) {
		int error;
		struct vnode *vp;
		struct mount *mp;

		mp = ap->a_dvp->v_mount;

		vp = NULL;
		error = VOP_CREATE(dvp, &vp, cnp, ap->a_vap);
		if (error)
			return (error);

		error = union_allocvp(ap->a_vpp, mp, NULLVP, NULLVP, cnp, vp,
				NULLVP, 1);
		if (error)
			vrele(vp);
		return (error);
	}

	return (EROFS);
}

int
union_whiteout(void *v)
{
	struct vop_whiteout_args /* {
		struct vnode *a_dvp;
		struct componentname *a_cnp;
		int a_flags;
	} */ *ap = v;
	struct union_node *un = VTOUNION(ap->a_dvp);
	struct componentname *cnp = ap->a_cnp;

	if (un->un_uppervp == NULLVP)
		return (EOPNOTSUPP);

	return (VOP_WHITEOUT(un->un_uppervp, cnp, ap->a_flags));
}

int
union_mknod(void *v)
{
	struct vop_mknod_v3_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	struct union_node *un = VTOUNION(ap->a_dvp);
	struct vnode *dvp = un->un_uppervp;
	struct componentname *cnp = ap->a_cnp;

	if (dvp != NULLVP) {
		int error;
		struct vnode *vp;
		struct mount *mp;

		mp = ap->a_dvp->v_mount;
		error = VOP_MKNOD(dvp, &vp, cnp, ap->a_vap);
		if (error)
			return (error);

		error = union_allocvp(ap->a_vpp, mp, NULLVP, NULLVP,
				      cnp, vp, NULLVP, 1);
		if (error)
			vrele(vp);
		return (error);
	}

	return (EROFS);
}

int
union_open(void *v)
{
	struct vop_open_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int a_mode;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct union_node *un = VTOUNION(ap->a_vp);
	struct vnode *tvp;
	int mode = ap->a_mode;
	kauth_cred_t cred = ap->a_cred;
	struct lwp *l = curlwp;
	int error;

	/*
	 * If there is an existing upper vp then simply open that.
	 */
	tvp = un->un_uppervp;
	if (tvp == NULLVP) {
		/*
		 * If the lower vnode is being opened for writing, then
		 * copy the file contents to the upper vnode and open that,
		 * otherwise can simply open the lower vnode.
		 */
		tvp = un->un_lowervp;
		if ((ap->a_mode & FWRITE) && (tvp->v_type == VREG)) {
			error = union_copyup(un, (mode&O_TRUNC) == 0, cred, l);
			if (error == 0)
				error = VOP_OPEN(un->un_uppervp, mode, cred);
			return (error);
		}

		/*
		 * Just open the lower vnode, but check for nodev mount flag
		 */
		if ((tvp->v_type == VBLK || tvp->v_type == VCHR) &&
		    (ap->a_vp->v_mount->mnt_flag & MNT_NODEV))
			return ENXIO;
		un->un_openl++;
		vn_lock(tvp, LK_EXCLUSIVE | LK_RETRY);
		error = VOP_OPEN(tvp, mode, cred);
		VOP_UNLOCK(tvp);

		return (error);
	}
	/*
	 * Just open the upper vnode, checking for nodev mount flag first
	 */
	if ((tvp->v_type == VBLK || tvp->v_type == VCHR) &&
	    (ap->a_vp->v_mount->mnt_flag & MNT_NODEV))
		return ENXIO;

	error = VOP_OPEN(tvp, mode, cred);

	return (error);
}

int
union_close(void *v)
{
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct union_node *un = VTOUNION(ap->a_vp);
	struct vnode *vp;
	int error;
	bool do_lock;

	vp = un->un_uppervp;
	if (vp != NULLVP) {
		do_lock = false;
	} else {
		KASSERT(un->un_openl > 0);
		--un->un_openl;
		vp = un->un_lowervp;
		do_lock = true;
	}

	KASSERT(vp != NULLVP);
	ap->a_vp = vp;
	if (do_lock)
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	error = VCALL(vp, VOFFSET(vop_close), ap);
	if (do_lock)
		VOP_UNLOCK(vp);

	return error;
}

/*
 * Check access permission on the union vnode.
 * The access check being enforced is to check
 * against both the underlying vnode, and any
 * copied vnode.  This ensures that no additional
 * file permissions are given away simply because
 * the user caused an implicit file copy.
 */
int
union_access(void *v)
{
	struct vop_access_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int a_mode;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct union_node *un = VTOUNION(vp);
	int error = EACCES;
	struct union_mount *um = MOUNTTOUNIONMOUNT(vp->v_mount);

	/*
	 * Disallow write attempts on read-only file systems;
	 * unless the file is a socket, fifo, or a block or
	 * character device resident on the file system.
	 */
	if (ap->a_mode & VWRITE) {
		switch (vp->v_type) {
		case VDIR:
		case VLNK:
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
			break;
		case VBAD:
		case VBLK:
		case VCHR:
		case VSOCK:
		case VFIFO:
		case VNON:
		default:
			break;
		}
	}


	if ((vp = un->un_uppervp) != NULLVP) {
		ap->a_vp = vp;
		return (VCALL(vp, VOFFSET(vop_access), ap));
	}

	if ((vp = un->un_lowervp) != NULLVP) {
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		ap->a_vp = vp;
		error = VCALL(vp, VOFFSET(vop_access), ap);
		if (error == 0) {
			if (um->um_op == UNMNT_BELOW) {
				ap->a_cred = um->um_cred;
				error = VCALL(vp, VOFFSET(vop_access), ap);
			}
		}
		VOP_UNLOCK(vp);
		if (error)
			return (error);
	}

	return (error);
}

/*
 * We handle getattr only to change the fsid and
 * track object sizes
 */
int
union_getattr(void *v)
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
	} */ *ap = v;
	int error;
	struct union_node *un = VTOUNION(ap->a_vp);
	struct vnode *vp = un->un_uppervp;
	struct vattr *vap;
	struct vattr va;


	/*
	 * Some programs walk the filesystem hierarchy by counting
	 * links to directories to avoid stat'ing all the time.
	 * This means the link count on directories needs to be "correct".
	 * The only way to do that is to call getattr on both layers
	 * and fix up the link count.  The link count will not necessarily
	 * be accurate but will be large enough to defeat the tree walkers.
	 *
	 * To make life more interesting, some filesystems don't keep
	 * track of link counts in the expected way, and return a
	 * link count of `1' for those directories; if either of the
	 * component directories returns a link count of `1', we return a 1.
	 */

	vap = ap->a_vap;

	vp = un->un_uppervp;
	if (vp != NULLVP) {
		error = VOP_GETATTR(vp, vap, ap->a_cred);
		if (error)
			return (error);
		mutex_enter(&un->un_lock);
		union_newsize(ap->a_vp, vap->va_size, VNOVAL);
	}

	if (vp == NULLVP) {
		vp = un->un_lowervp;
	} else if (vp->v_type == VDIR) {
		vp = un->un_lowervp;
		if (vp != NULLVP)
			vap = &va;
	} else {
		vp = NULLVP;
	}

	if (vp != NULLVP) {
		if (vp == un->un_lowervp)
			vn_lock(vp, LK_SHARED | LK_RETRY);
		error = VOP_GETATTR(vp, vap, ap->a_cred);
		if (vp == un->un_lowervp)
			VOP_UNLOCK(vp);
		if (error)
			return (error);
		mutex_enter(&un->un_lock);
		union_newsize(ap->a_vp, VNOVAL, vap->va_size);
	}

	if ((vap != ap->a_vap) && (vap->va_type == VDIR)) {
		/*
		 * Link count manipulation:
		 *	- If both return "2", return 2 (no subdirs)
		 *	- If one or the other return "1", return "1" (ENOCLUE)
		 */
		if ((ap->a_vap->va_nlink == 2) &&
		    (vap->va_nlink == 2))
			;
		else if (ap->a_vap->va_nlink != 1) {
			if (vap->va_nlink == 1)
				ap->a_vap->va_nlink = 1;
			else
				ap->a_vap->va_nlink += vap->va_nlink;
		}
	}
	ap->a_vap->va_fsid = ap->a_vp->v_mount->mnt_stat.f_fsidx.__fsid_val[0];
	return (0);
}

int
union_setattr(void *v)
{
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vattr *vap = ap->a_vap;
	struct vnode *vp = ap->a_vp;
	struct union_node *un = VTOUNION(vp);
	bool size_only;		/* All but va_size are VNOVAL. */
	int error;

	size_only = (vap->va_flags == VNOVAL && vap->va_uid == (uid_t)VNOVAL &&
	    vap->va_gid == (gid_t)VNOVAL && vap->va_atime.tv_sec == VNOVAL &&
	    vap->va_mtime.tv_sec == VNOVAL && vap->va_mode == (mode_t)VNOVAL);

	if (!size_only && (vp->v_mount->mnt_flag & MNT_RDONLY))
		return (EROFS);
	if (vap->va_size != VNOVAL) {
 		switch (vp->v_type) {
 		case VDIR:
 			return (EISDIR);
 		case VCHR:
 		case VBLK:
 		case VSOCK:
 		case VFIFO:
			break;
		case VREG:
		case VLNK:
 		default:
			/*
			 * Disallow write attempts if the filesystem is
			 * mounted read-only.
			 */
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
		}
	}

	/*
	 * Handle case of truncating lower object to zero size,
	 * by creating a zero length upper object.  This is to
	 * handle the case of open with O_TRUNC and O_CREAT.
	 */
	if ((un->un_uppervp == NULLVP) &&
	    /* assert(un->un_lowervp != NULLVP) */
	    (un->un_lowervp->v_type == VREG)) {
		error = union_copyup(un, (vap->va_size != 0),
						ap->a_cred, curlwp);
		if (error)
			return (error);
	}

	/*
	 * Try to set attributes in upper layer, ignore size change to zero
	 * for devices to handle O_TRUNC and return read-only filesystem error
	 * otherwise.
	 */
	if (un->un_uppervp != NULLVP) {
		error = VOP_SETATTR(un->un_uppervp, vap, ap->a_cred);
		if ((error == 0) && (vap->va_size != VNOVAL)) {
			mutex_enter(&un->un_lock);
			union_newsize(ap->a_vp, vap->va_size, VNOVAL);
		}
	} else {
		KASSERT(un->un_lowervp != NULLVP);
		if (NODE_IS_SPECIAL(un->un_lowervp)) {
			if (size_only &&
			    (vap->va_size == 0 || vap->va_size == VNOVAL))
				error = 0;
			else
				error = EROFS;
		} else {
			error = EROFS;
		}
	}

	return (error);
}

int
union_read(void *v)
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	int error;
	struct vnode *vp = OTHERVP(ap->a_vp);
	int dolock = (vp == LOWERVP(ap->a_vp));

	if (dolock)
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	error = VOP_READ(vp, ap->a_uio, ap->a_ioflag, ap->a_cred);
	if (dolock)
		VOP_UNLOCK(vp);

	/*
	 * XXX
	 * perhaps the size of the underlying object has changed under
	 * our feet.  take advantage of the offset information present
	 * in the uio structure.
	 */
	if (error == 0) {
		struct union_node *un = VTOUNION(ap->a_vp);
		off_t cur = ap->a_uio->uio_offset;
		off_t usz = VNOVAL, lsz = VNOVAL;

		mutex_enter(&un->un_lock);
		if (vp == un->un_uppervp) {
			if (cur > un->un_uppersz)
				usz = cur;
		} else {
			if (cur > un->un_lowersz)
				lsz = cur;
		}

		if (usz != VNOVAL || lsz != VNOVAL)
			union_newsize(ap->a_vp, usz, lsz);
		else
			mutex_exit(&un->un_lock);
	}

	return (error);
}

int
union_write(void *v)
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	int error;
	struct vnode *vp;
	struct union_node *un = VTOUNION(ap->a_vp);

	vp = UPPERVP(ap->a_vp);
	if (vp == NULLVP) {
		vp = LOWERVP(ap->a_vp);
		if (NODE_IS_SPECIAL(vp)) {
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
			error = VOP_WRITE(vp, ap->a_uio, ap->a_ioflag,
			    ap->a_cred);
			VOP_UNLOCK(vp);
			return error;
		}
		panic("union: missing upper layer in write");
	}

	error = VOP_WRITE(vp, ap->a_uio, ap->a_ioflag, ap->a_cred);

	/*
	 * the size of the underlying object may be changed by the
	 * write.
	 */
	if (error == 0) {
		off_t cur = ap->a_uio->uio_offset;

		mutex_enter(&un->un_lock);
		if (cur > un->un_uppersz)
			union_newsize(ap->a_vp, cur, VNOVAL);
		else
			mutex_exit(&un->un_lock);
	}

	return (error);
}

int
union_ioctl(void *v)
{
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		int  a_command;
		void *a_data;
		int  a_fflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *ovp = OTHERVP(ap->a_vp);

	ap->a_vp = ovp;
	return (VCALL(ovp, VOFFSET(vop_ioctl), ap));
}

int
union_poll(void *v)
{
	struct vop_poll_args /* {
		struct vnode *a_vp;
		int a_events;
	} */ *ap = v;
	struct vnode *ovp = OTHERVP(ap->a_vp);

	ap->a_vp = ovp;
	return (VCALL(ovp, VOFFSET(vop_poll), ap));
}

int
union_revoke(void *v)
{
	struct vop_revoke_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	if (UPPERVP(vp))
		VOP_REVOKE(UPPERVP(vp), ap->a_flags);
	if (LOWERVP(vp))
		VOP_REVOKE(LOWERVP(vp), ap->a_flags);
	vgone(vp);	/* XXXAD?? */
	return (0);
}

int
union_mmap(void *v)
{
	struct vop_mmap_args /* {
		struct vnode *a_vp;
		vm_prot_t a_prot;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *ovp = OTHERVP(ap->a_vp);

	ap->a_vp = ovp;
	return (VCALL(ovp, VOFFSET(vop_mmap), ap));
}

int
union_fsync(void *v)
{
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		kauth_cred_t a_cred;
		int  a_flags;
		off_t offhi;
		off_t offlo;
	} */ *ap = v;
	int error = 0;
	struct vnode *targetvp;

	/*
	 * If vinvalbuf is calling us, it's a "shallow fsync" -- don't
	 * bother syncing the underlying vnodes, since (a) they'll be
	 * fsync'ed when reclaimed and (b) we could deadlock if
	 * they're locked; otherwise, pass it through to the
	 * underlying layer.
	 */
	if (ap->a_vp->v_type == VBLK || ap->a_vp->v_type == VCHR) {
		error = spec_fsync(v);
		if (error)
			return error;
	}

	if (ap->a_flags & FSYNC_RECLAIM)
		return 0;

	targetvp = OTHERVP(ap->a_vp);
	if (targetvp != NULLVP) {
		int dolock = (targetvp == LOWERVP(ap->a_vp));

		if (dolock)
			vn_lock(targetvp, LK_EXCLUSIVE | LK_RETRY);
		error = VOP_FSYNC(targetvp, ap->a_cred, ap->a_flags,
			    ap->a_offlo, ap->a_offhi);
		if (dolock)
			VOP_UNLOCK(targetvp);
	}

	return (error);
}

int
union_seek(void *v)
{
	struct vop_seek_args /* {
		struct vnode *a_vp;
		off_t  a_oldoff;
		off_t  a_newoff;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *ovp = OTHERVP(ap->a_vp);

	ap->a_vp = ovp;
	return (VCALL(ovp, VOFFSET(vop_seek), ap));
}

int
union_remove(void *v)
{
	struct vop_remove_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	int error;
	struct union_node *dun = VTOUNION(ap->a_dvp);
	struct union_node *un = VTOUNION(ap->a_vp);
	struct componentname *cnp = ap->a_cnp;

	if (dun->un_uppervp == NULLVP)
		panic("union remove: null upper vnode");

	if (un->un_uppervp != NULLVP) {
		struct vnode *dvp = dun->un_uppervp;
		struct vnode *vp = un->un_uppervp;

		/*
		 * Account for VOP_REMOVE to vrele dvp and vp.
		 * Note: VOP_REMOVE will unlock dvp and vp.
		 */
		vref(dvp);
		vref(vp);
		if (union_dowhiteout(un, cnp->cn_cred))
			cnp->cn_flags |= DOWHITEOUT;
		error = VOP_REMOVE(dvp, vp, cnp);
		if (!error)
			union_removed_upper(un);
		vrele(ap->a_dvp);
		vrele(ap->a_vp);
	} else {
		error = union_mkwhiteout(
			MOUNTTOUNIONMOUNT(UNIONTOV(dun)->v_mount),
			dun->un_uppervp, ap->a_cnp, un);
		vput(ap->a_dvp);
		vput(ap->a_vp);
	}

	return (error);
}

int
union_link(void *v)
{
	struct vop_link_v2_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	int error = 0;
	struct componentname *cnp = ap->a_cnp;
	struct union_node *dun;
	struct vnode *vp;
	struct vnode *dvp;

	dun = VTOUNION(ap->a_dvp);

	KASSERT((ap->a_cnp->cn_flags & LOCKPARENT) != 0);

	if (ap->a_dvp->v_op != ap->a_vp->v_op) {
		vp = ap->a_vp;
	} else {
		struct union_node *un = VTOUNION(ap->a_vp);
		if (un->un_uppervp == NULLVP) {
			const bool droplock = (dun->un_uppervp == un->un_dirvp);

			/*
			 * Needs to be copied before we can link it.
			 */
			vn_lock(ap->a_vp, LK_EXCLUSIVE | LK_RETRY);
			if (droplock)
				VOP_UNLOCK(dun->un_uppervp);
			error = union_copyup(un, 1, cnp->cn_cred, curlwp);
			if (droplock) {
				vn_lock(dun->un_uppervp,
				    LK_EXCLUSIVE | LK_RETRY);
				/*
				 * During copyup, we dropped the lock on the
				 * dir and invalidated any saved namei lookup
				 * state for the directory we'll be entering
				 * the link in.  We need to re-run the lookup
				 * in that directory to reset any state needed
				 * for VOP_LINK.
				 * Call relookup on the union-layer to reset
				 * the state.
				 */
				vp  = NULLVP;
				if (dun->un_uppervp == NULLVP)
					 panic("union: null upperdvp?");
				error = relookup(ap->a_dvp, &vp, ap->a_cnp, 0);
				if (error) {
					VOP_UNLOCK(ap->a_vp);
					return EROFS;	/* ? */
				}
				if (vp != NULLVP) {
					/*
					 * The name we want to create has
					 * mysteriously appeared (a race?)
					 */
					error = EEXIST;
					VOP_UNLOCK(ap->a_vp);
					vput(vp);
					return (error);
				}
			}
			VOP_UNLOCK(ap->a_vp);
		}
		vp = un->un_uppervp;
	}

	dvp = dun->un_uppervp;
	if (dvp == NULLVP)
		error = EROFS;

	if (error)
		return (error);

	return VOP_LINK(dvp, vp, cnp);
}

int
union_rename(void *v)
{
	struct vop_rename_args  /* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap = v;
	int error;

	struct vnode *fdvp = ap->a_fdvp;
	struct vnode *fvp = ap->a_fvp;
	struct vnode *tdvp = ap->a_tdvp;
	struct vnode *tvp = ap->a_tvp;

	/*
	 * Account for VOP_RENAME to vrele all nodes.
	 * Note: VOP_RENAME will unlock tdvp.
	 */

	if (fdvp->v_op == union_vnodeop_p) {	/* always true */
		struct union_node *un = VTOUNION(fdvp);
		if (un->un_uppervp == NULLVP) {
			/*
			 * this should never happen in normal
			 * operation but might if there was
			 * a problem creating the top-level shadow
			 * directory.
			 */
			error = EXDEV;
			goto bad;
		}

		fdvp = un->un_uppervp;
		vref(fdvp);
	}

	if (fvp->v_op == union_vnodeop_p) {	/* always true */
		struct union_node *un = VTOUNION(fvp);
		if (un->un_uppervp == NULLVP) {
			/* XXX: should do a copyup */
			error = EXDEV;
			goto bad;
		}

		if (un->un_lowervp != NULLVP)
			ap->a_fcnp->cn_flags |= DOWHITEOUT;

		fvp = un->un_uppervp;
		vref(fvp);
	}

	if (tdvp->v_op == union_vnodeop_p) {
		struct union_node *un = VTOUNION(tdvp);
		if (un->un_uppervp == NULLVP) {
			/*
			 * this should never happen in normal
			 * operation but might if there was
			 * a problem creating the top-level shadow
			 * directory.
			 */
			error = EXDEV;
			goto bad;
		}

		tdvp = un->un_uppervp;
		vref(tdvp);
	}

	if (tvp != NULLVP && tvp->v_op == union_vnodeop_p) {
		struct union_node *un = VTOUNION(tvp);

		tvp = un->un_uppervp;
		if (tvp != NULLVP) {
			vref(tvp);
		}
	}

	error = VOP_RENAME(fdvp, fvp, ap->a_fcnp, tdvp, tvp, ap->a_tcnp);
	goto out;

bad:
	vput(tdvp);
	if (tvp != NULLVP)
		vput(tvp);
	vrele(fdvp);
	vrele(fvp);

out:
	if (fdvp != ap->a_fdvp) {
		vrele(ap->a_fdvp);
	}
	if (fvp != ap->a_fvp) {
		vrele(ap->a_fvp);
	}
	if (tdvp != ap->a_tdvp) {
		vrele(ap->a_tdvp);
	}
	if (tvp != ap->a_tvp) {
		vrele(ap->a_tvp);
	}
	return (error);
}

int
union_mkdir(void *v)
{
	struct vop_mkdir_v3_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	struct union_node *un = VTOUNION(ap->a_dvp);
	struct vnode *dvp = un->un_uppervp;
	struct componentname *cnp = ap->a_cnp;

	if (dvp != NULLVP) {
		int error;
		struct vnode *vp;

		vp = NULL;
		error = VOP_MKDIR(dvp, &vp, cnp, ap->a_vap);
		if (error) {
			vrele(ap->a_dvp);
			return (error);
		}

		error = union_allocvp(ap->a_vpp, ap->a_dvp->v_mount, ap->a_dvp,
				NULLVP, cnp, vp, NULLVP, 1);
		if (error)
			vrele(vp);
		return (error);
	}

	return (EROFS);
}

int
union_rmdir(void *v)
{
	struct vop_rmdir_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	int error;
	struct union_node *dun = VTOUNION(ap->a_dvp);
	struct union_node *un = VTOUNION(ap->a_vp);
	struct componentname *cnp = ap->a_cnp;

	if (dun->un_uppervp == NULLVP)
		panic("union rmdir: null upper vnode");

	error = union_check_rmdir(un, cnp->cn_cred);
	if (error) {
		vput(ap->a_dvp);
		vput(ap->a_vp);
		return error;
	}

	if (un->un_uppervp != NULLVP) {
		struct vnode *dvp = dun->un_uppervp;
		struct vnode *vp = un->un_uppervp;

		/*
		 * Account for VOP_RMDIR to vrele dvp and vp.
		 * Note: VOP_RMDIR will unlock dvp and vp.
		 */
		vref(dvp);
		vref(vp);
		if (union_dowhiteout(un, cnp->cn_cred))
			cnp->cn_flags |= DOWHITEOUT;
		error = VOP_RMDIR(dvp, vp, ap->a_cnp);
		if (!error)
			union_removed_upper(un);
		vrele(ap->a_dvp);
		vrele(ap->a_vp);
	} else {
		error = union_mkwhiteout(
			MOUNTTOUNIONMOUNT(UNIONTOV(dun)->v_mount),
			dun->un_uppervp, ap->a_cnp, un);
		vput(ap->a_dvp);
		vput(ap->a_vp);
	}

	return (error);
}

int
union_symlink(void *v)
{
	struct vop_symlink_v3_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap = v;
	struct union_node *un = VTOUNION(ap->a_dvp);
	struct vnode *dvp = un->un_uppervp;
	struct componentname *cnp = ap->a_cnp;

	if (dvp != NULLVP) {
		int error;

		error = VOP_SYMLINK(dvp, ap->a_vpp, cnp, ap->a_vap,
				    ap->a_target);
		return (error);
	}

	return (EROFS);
}

/*
 * union_readdir works in concert with getdirentries and
 * readdir(3) to provide a list of entries in the unioned
 * directories.  getdirentries is responsible for walking
 * down the union stack.  readdir(3) is responsible for
 * eliminating duplicate names from the returned data stream.
 */
int
union_readdir(void *v)
{
	struct vop_readdir_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct uio *a_uio;
		kauth_cred_t a_cred;
		int *a_eofflag;
		u_long *a_cookies;
		int a_ncookies;
	} */ *ap = v;
	struct union_node *un = VTOUNION(ap->a_vp);
	struct vnode *uvp = un->un_uppervp;

	if (uvp == NULLVP)
		return (0);

	ap->a_vp = uvp;
	return (VCALL(uvp, VOFFSET(vop_readdir), ap));
}

int
union_readlink(void *v)
{
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		kauth_cred_t a_cred;
	} */ *ap = v;
	int error;
	struct vnode *vp = OTHERVP(ap->a_vp);
	int dolock = (vp == LOWERVP(ap->a_vp));

	if (dolock)
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	ap->a_vp = vp;
	error = VCALL(vp, VOFFSET(vop_readlink), ap);
	if (dolock)
		VOP_UNLOCK(vp);

	return (error);
}

int
union_abortop(void *v)
{
	struct vop_abortop_args /* {
		struct vnode *a_dvp;
		struct componentname *a_cnp;
	} */ *ap = v;

	KASSERT(UPPERVP(ap->a_dvp) != NULL);

	ap->a_dvp = UPPERVP(ap->a_dvp);
	return VCALL(ap->a_dvp, VOFFSET(vop_abortop), ap);
}

int
union_inactive(void *v)
{
	struct vop_inactive_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		bool *a_recycle;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct union_node *un = VTOUNION(vp);
	struct vnode **vpp;

	/*
	 * Do nothing (and _don't_ bypass).
	 * Wait to vrele lowervp until reclaim,
	 * so that until then our union_node is in the
	 * cache and reusable.
	 *
	 * NEEDSWORK: Someday, consider inactive'ing
	 * the lowervp and then trying to reactivate it
	 * with capabilities (v_id)
	 * like they do in the name lookup cache code.
	 * That's too much work for now.
	 */

	if (un->un_dircache != 0) {
		for (vpp = un->un_dircache; *vpp != NULLVP; vpp++)
			vrele(*vpp);
		free(un->un_dircache, M_TEMP);
		un->un_dircache = 0;
	}

	*ap->a_recycle = ((un->un_cflags & UN_CACHED) == 0);
	VOP_UNLOCK(vp);

	return (0);
}

int
union_reclaim(void *v)
{
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap = v;

	union_freevp(ap->a_vp);

	return (0);
}

static int
union_lock1(struct vnode *vp, struct vnode *lockvp, int flags)
{
	struct vop_lock_args ap;

	if (lockvp == vp) {
		ap.a_vp = vp;
		ap.a_flags = flags;
		return genfs_lock(&ap);
	} else
		return VOP_LOCK(lockvp, flags);
}

static int
union_unlock1(struct vnode *vp, struct vnode *lockvp)
{
	struct vop_unlock_args ap;

	if (lockvp == vp) {
		ap.a_vp = vp;
		return genfs_unlock(&ap);
	} else
		return VOP_UNLOCK(lockvp);
}

int
union_lock(void *v)
{
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp, *lockvp;
	struct union_node *un = VTOUNION(vp);
	int flags = ap->a_flags;
	int error;

	if ((flags & LK_NOWAIT) != 0) {
		if (!mutex_tryenter(&un->un_lock))
			return EBUSY;
		lockvp = LOCKVP(vp);
		error = union_lock1(vp, lockvp, flags);
		mutex_exit(&un->un_lock);
		if (error)
			return error;
		if (mutex_tryenter(vp->v_interlock)) {
			error = vdead_check(vp, VDEAD_NOWAIT);
			mutex_exit(vp->v_interlock);
		} else
			error = EBUSY;
		if (error)
			union_unlock1(vp, lockvp);
		return error;
	}

	mutex_enter(&un->un_lock);
	for (;;) {
		lockvp = LOCKVP(vp);
		mutex_exit(&un->un_lock);
		error = union_lock1(vp, lockvp, flags);
		if (error != 0)
			return error;
		mutex_enter(&un->un_lock);
		if (lockvp == LOCKVP(vp))
			break;
		union_unlock1(vp, lockvp);
	}
	mutex_exit(&un->un_lock);

	mutex_enter(vp->v_interlock);
	error = vdead_check(vp, VDEAD_NOWAIT);
	if (error) {
		union_unlock1(vp, lockvp);
		error = vdead_check(vp, 0);
		KASSERT(error == ENOENT);
	}
	mutex_exit(vp->v_interlock);
	return error;
}

int
union_unlock(void *v)
{
	struct vop_unlock_args /* {
		struct vnode *a_vp;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp, *lockvp;

	lockvp = LOCKVP(vp);
	union_unlock1(vp, lockvp);

	return 0;
}

int
union_bmap(void *v)
{
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
	} */ *ap = v;
	int error;
	struct vnode *vp = OTHERVP(ap->a_vp);
	int dolock = (vp == LOWERVP(ap->a_vp));

	if (dolock)
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	ap->a_vp = vp;
	error = VCALL(vp, VOFFSET(vop_bmap), ap);
	if (dolock)
		VOP_UNLOCK(vp);

	return (error);
}

int
union_print(void *v)
{
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	printf("\ttag VT_UNION, vp=%p, uppervp=%p, lowervp=%p\n",
			vp, UPPERVP(vp), LOWERVP(vp));
	if (UPPERVP(vp) != NULLVP)
		vprint("union: upper", UPPERVP(vp));
	if (LOWERVP(vp) != NULLVP)
		vprint("union: lower", LOWERVP(vp));
	if (VTOUNION(vp)->un_dircache) {
		struct vnode **vpp;
		for (vpp = VTOUNION(vp)->un_dircache; *vpp != NULLVP; vpp++)
			vprint("dircache:", *vpp);
	}

	return (0);
}

int
union_islocked(void *v)
{
	struct vop_islocked_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp;
	struct union_node *un;

	un = VTOUNION(ap->a_vp);
	mutex_enter(&un->un_lock);
	vp = LOCKVP(ap->a_vp);
	mutex_exit(&un->un_lock);

	if (vp == ap->a_vp)
		return genfs_islocked(ap);
	else
		return VOP_ISLOCKED(vp);
}

int
union_pathconf(void *v)
{
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		int *a_retval;
	} */ *ap = v;
	int error;
	struct vnode *vp = OTHERVP(ap->a_vp);
	int dolock = (vp == LOWERVP(ap->a_vp));

	if (dolock)
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	ap->a_vp = vp;
	error = VCALL(vp, VOFFSET(vop_pathconf), ap);
	if (dolock)
		VOP_UNLOCK(vp);

	return (error);
}

int
union_advlock(void *v)
{
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		void *a_id;
		int  a_op;
		struct flock *a_fl;
		int  a_flags;
	} */ *ap = v;
	struct vnode *ovp = OTHERVP(ap->a_vp);

	ap->a_vp = ovp;
	return (VCALL(ovp, VOFFSET(vop_advlock), ap));
}

int
union_strategy(void *v)
{
	struct vop_strategy_args /* {
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap = v;
	struct vnode *ovp = OTHERVP(ap->a_vp);
	struct buf *bp = ap->a_bp;

	KASSERT(ovp != NULLVP);
	if (!NODE_IS_SPECIAL(ovp))
		KASSERT((bp->b_flags & B_READ) || ovp != LOWERVP(bp->b_vp));

	return (VOP_STRATEGY(ovp, bp));
}

int
union_bwrite(void *v)
{
	struct vop_bwrite_args /* {
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap = v;
	struct vnode *ovp = OTHERVP(ap->a_vp);
	struct buf *bp = ap->a_bp;

	KASSERT(ovp != NULLVP);
	if (!NODE_IS_SPECIAL(ovp))
		KASSERT((bp->b_flags & B_READ) || ovp != LOWERVP(bp->b_vp));

	return (VOP_BWRITE(ovp, bp));
}

int
union_getpages(void *v)
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
	ap->a_vp = OTHERVP(vp);
	KASSERT(vp->v_interlock == ap->a_vp->v_interlock);

	/* Just pass the request on to the underlying layer. */
	return VCALL(ap->a_vp, VOFFSET(vop_getpages), ap);
}

int
union_putpages(void *v)
{
	struct vop_putpages_args /* {
		struct vnode *a_vp;
		voff_t a_offlo;
		voff_t a_offhi;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	KASSERT(mutex_owned(vp->v_interlock));

	ap->a_vp = OTHERVP(vp);
	KASSERT(vp->v_interlock == ap->a_vp->v_interlock);

	if (ap->a_flags & PGO_RECLAIM) {
		mutex_exit(vp->v_interlock);
		return 0;
	}

	/* Just pass the request on to the underlying layer. */
	return VCALL(ap->a_vp, VOFFSET(vop_putpages), ap);
}

int
union_kqfilter(void *v)
{
	struct vop_kqfilter_args /* {
		struct vnode	*a_vp;
		struct knote	*a_kn;
	} */ *ap = v;
	int error;

	/*
	 * We watch either the upper layer file (if it already exists),
	 * or the lower layer one. If there is lower layer file only
	 * at this moment, we will keep watching that lower layer file
	 * even if upper layer file would be created later on.
	 */
	if (UPPERVP(ap->a_vp))
		error = VOP_KQFILTER(UPPERVP(ap->a_vp), ap->a_kn);
	else if (LOWERVP(ap->a_vp))
		error = VOP_KQFILTER(LOWERVP(ap->a_vp), ap->a_kn);
	else {
		/* panic? */
		error = EOPNOTSUPP;
	}

	return (error);
}

/*-
 * Copyright (c) 1992, 1993, 1994, 1995 Jan-Simon Pendry.
 * Copyright (c) 1992, 1993, 1994, 1995
 *      The Regents of the University of California.
 * Copyright (c) 2005, 2006 Masanori Ozawa <ozawa@ongs.co.jp>, ONGS Inc.
 * Copyright (c) 2006 Daichi Goto <daichi@freebsd.org>
 * All rights reserved.
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
 *	@(#)union_vnops.c	8.32 (Berkeley) 6/23/95
 * $FreeBSD: src/sys/fs/unionfs/union_vnops.c,v 1.152 2008/01/13 14:44:06 attilio Exp $
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/dirent.h>
#include <sys/proc.h>

#include <fs/unionfs/unionfs.h>

#if 0
#define UNIONFS_INTERNAL_DEBUG(msg, args...)    printf(msg, ## args)
#define UNIONFS_IDBG_RENAME
#else
#define UNIONFS_INTERNAL_DEBUG(msg, args...)
#endif

static int
unionfs_lookup(void *v)
{
	struct vop_lookup_args *ap = v;
	int		iswhiteout;
	int		lockflag;
	int		error , uerror, lerror;
	u_long		nameiop;
	u_long		cnflags, cnflagsbk;
	struct unionfs_node *dunp;
	struct vnode   *dvp, *udvp, *ldvp, *vp, *uvp, *lvp, *dtmpvp;
	struct vattr	va;
	struct componentname *cnp;

	iswhiteout = 0;
	lockflag = 0;
	error = uerror = lerror = ENOENT;
	cnp = ap->a_cnp;
	nameiop = cnp->cn_nameiop;
	cnflags = cnp->cn_flags;
	dvp = ap->a_dvp;
	dunp = VTOUNIONFS(dvp);
	udvp = dunp->un_uppervp;
	ldvp = dunp->un_lowervp;
	vp = uvp = lvp = NULLVP;
	*(ap->a_vpp) = NULLVP;

	UNIONFS_INTERNAL_DEBUG("unionfs_lookup: enter: nameiop=%ld, flags=%lx, path=%s\n", nameiop, cnflags, cnp->cn_nameptr);

	if (dvp->v_type != VDIR)
		return (ENOTDIR);

	/*
	 * If read-only and op is not LOOKUP, will return EROFS.
	 */
	if ((cnflags & ISLASTCN) &&
	    (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    LOOKUP != nameiop)
		return (EROFS);

	/*
	 * lookup dotdot
	 */
	if (cnflags & ISDOTDOT) {
		if (LOOKUP != nameiop && udvp == NULLVP)
			return (EROFS);

		if (udvp != NULLVP) {
			dtmpvp = udvp;
			if (ldvp != NULLVP)
				VOP_UNLOCK(ldvp);
		}
		else
			dtmpvp = ldvp;

		error = VOP_LOOKUP(dtmpvp, &vp, cnp);

		if (dtmpvp == udvp && ldvp != NULLVP) {
			VOP_UNLOCK(udvp);
			vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY);
		}

		if (error == 0) {
			/*
			 * Exchange lock and reference from vp to
			 * dunp->un_dvp. vp is upper/lower vnode, but it
			 * will need to return the unionfs vnode.
			 */
			if (nameiop == DELETE  || nameiop == RENAME)
				VOP_UNLOCK(vp);
			vrele(vp);

			VOP_UNLOCK(dvp);
			*(ap->a_vpp) = dunp->un_dvp;
			vref(dunp->un_dvp);

			vn_lock(dunp->un_dvp, LK_EXCLUSIVE | LK_RETRY);
			vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY);
		} else if (error == ENOENT && nameiop != CREATE)
			cache_enter(dvp, NULLVP, cnp->cn_nameptr,
				    cnp->cn_namelen, cnp->cn_flags);

		UNIONFS_INTERNAL_DEBUG("unionfs_lookup: leave (%d)\n", error);

		return (error);
	}

	/*
	 * lookup upper layer
	 */
	if (udvp != NULLVP) {
		uerror = VOP_LOOKUP(udvp, &uvp, cnp);

		if (uerror == 0) {
			if (udvp == uvp) {	/* is dot */
				vrele(uvp);
				*(ap->a_vpp) = dvp;
				vref(dvp);

				UNIONFS_INTERNAL_DEBUG("unionfs_lookup: leave (%d)\n", uerror);

				return (uerror);
			}
		}

		/* check whiteout */
		if (uerror == ENOENT || uerror == EJUSTRETURN)
			if (cnp->cn_flags & ISWHITEOUT)
				iswhiteout = 1;	/* don't lookup lower */
		if (iswhiteout == 0 && ldvp != NULLVP)
			if (VOP_GETATTR(udvp, &va, cnp->cn_cred) == 0 &&
			    (va.va_flags & OPAQUE))
				iswhiteout = 1;	/* don't lookup lower */
#if 0
		UNIONFS_INTERNAL_DEBUG("unionfs_lookup: debug: whiteout=%d, path=%s\n", iswhiteout, cnp->cn_nameptr);
#endif
	}

	/*
	 * lookup lower layer
	 */
	if (ldvp != NULLVP && !(cnflags & DOWHITEOUT) && iswhiteout == 0) {
		/* always op is LOOKUP */
		cnp->cn_nameiop = LOOKUP;
		cnflagsbk = cnp->cn_flags;
		cnp->cn_flags = cnflags;

		lerror = VOP_LOOKUP(ldvp, &lvp, cnp);

		cnp->cn_nameiop = nameiop;
		if (udvp != NULLVP && (uerror == 0 || uerror == EJUSTRETURN))
			cnp->cn_flags = cnflagsbk;

		if (lerror == 0) {
			if (ldvp == lvp) {	/* is dot */
				if (uvp != NULLVP)
					vrele(uvp);	/* no need? */
				vrele(lvp);
				*(ap->a_vpp) = dvp;
				vref(dvp);

				UNIONFS_INTERNAL_DEBUG("unionfs_lookup: leave (%d)\n", lerror);
				if (uvp != NULL)
					VOP_UNLOCK(uvp);
				return (lerror);
			}
		}
	}

	/*
	 * check lookup result
	 */
	if (uvp == NULLVP && lvp == NULLVP) {
		UNIONFS_INTERNAL_DEBUG("unionfs_lookup: leave (%d)\n",
		    (udvp != NULLVP ? uerror : lerror));
		return (udvp != NULLVP ? uerror : lerror);
	}

	/*
	 * check vnode type
	 */
	if (uvp != NULLVP && lvp != NULLVP && uvp->v_type != lvp->v_type) {
		vput(lvp);
		lvp = NULLVP;
	}

	/*
	 * check shadow dir
	 */
	if (uerror != 0 && uerror != EJUSTRETURN && udvp != NULLVP &&
	    lerror == 0 && lvp != NULLVP && lvp->v_type == VDIR &&
	    !(dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (1 < cnp->cn_namelen || '.' != *(cnp->cn_nameptr))) {
		/* get unionfs vnode in order to create a new shadow dir. */
		error = unionfs_nodeget(dvp->v_mount, NULLVP, lvp, dvp, &vp,
		    cnp);
		if (error != 0)
			goto unionfs_lookup_out;
		error = unionfs_mkshadowdir(MOUNTTOUNIONFSMOUNT(dvp->v_mount),
		    udvp, VTOUNIONFS(vp), cnp);
		if (error != 0) {
			UNIONFSDEBUG("unionfs_lookup: Unable to create shadow dir.");
			vput(vp);
			goto unionfs_lookup_out;
		}
	}
	/*
	 * get unionfs vnode.
	 */
	else {
		if (uvp != NULLVP)
			error = uerror;
		else
			error = lerror;
		if (error != 0)
			goto unionfs_lookup_out;
		error = unionfs_nodeget(dvp->v_mount, uvp, lvp, dvp, &vp, cnp);
		if (error != 0) {
			UNIONFSDEBUG("unionfs_lookup: Unable to create unionfs vnode.");
			goto unionfs_lookup_out;
		}
	}

	*(ap->a_vpp) = vp;

	cache_enter(dvp, vp, cnp->cn_nameptr, cnp->cn_namelen,
		    cnp->cn_flags);

	/* XXXAD lock status on error */
unionfs_lookup_out:
	if (uvp != NULLVP)
		vrele(uvp);
	if (lvp != NULLVP)
		vrele(lvp);

	if (error == ENOENT && nameiop != CREATE)
		cache_enter(dvp, NULLVP, cnp->cn_nameptr, cnp->cn_namelen,
			    cnp->cn_flags);

	UNIONFS_INTERNAL_DEBUG("unionfs_lookup: leave (%d)\n", error);

	return (error);
}

static int
unionfs_create(void *v)
{
	struct vop_create_args *ap = v;
	struct unionfs_node *dunp;
	struct componentname *cnp;
	struct vnode   *udvp;
	struct vnode   *vp;
	int		error;

	UNIONFS_INTERNAL_DEBUG("unionfs_create: enter\n");

	dunp = VTOUNIONFS(ap->a_dvp);
	cnp = ap->a_cnp;
	udvp = dunp->un_uppervp;
	error = EROFS;

	if (udvp != NULLVP) {
		if ((error = VOP_CREATE(udvp, &vp, cnp, ap->a_vap)) == 0) {
			error = unionfs_nodeget(ap->a_dvp->v_mount, vp, NULLVP,
			    ap->a_dvp, ap->a_vpp, cnp);
			if (error) {
				vput(vp);
			} else {
				vrele(vp);
			}
		}
	}

	UNIONFS_INTERNAL_DEBUG("unionfs_create: leave (%d)\n", error);

	return (error);
}

static int
unionfs_whiteout(void *v)
{
	struct vop_whiteout_args *ap = v;
	struct unionfs_node *dunp;
	struct componentname *cnp;
	struct vnode   *udvp;
	int		error;

	UNIONFS_INTERNAL_DEBUG("unionfs_whiteout: enter\n");

	dunp = VTOUNIONFS(ap->a_dvp);
	cnp = ap->a_cnp;
	udvp = dunp->un_uppervp;
	error = EOPNOTSUPP;

	if (udvp != NULLVP) {
		switch (ap->a_flags) {
		case CREATE:
		case DELETE:
		case LOOKUP:
			error = VOP_WHITEOUT(udvp, cnp, ap->a_flags);
			break;
		default:
			error = EINVAL;
			break;
		}
	}

	UNIONFS_INTERNAL_DEBUG("unionfs_whiteout: leave (%d)\n", error);

	return (error);
}

static int
unionfs_mknod(void *v)
{
	struct vop_mknod_args *ap = v;
	struct unionfs_node *dunp;
	struct componentname *cnp;
	struct vnode   *udvp;
	struct vnode   *vp;
	int		error;

	UNIONFS_INTERNAL_DEBUG("unionfs_mknod: enter\n");

	dunp = VTOUNIONFS(ap->a_dvp);
	cnp = ap->a_cnp;
	udvp = dunp->un_uppervp;
	error = EROFS;

	if (udvp != NULLVP) {
		if ((error = VOP_MKNOD(udvp, &vp, cnp, ap->a_vap)) == 0) {
			error = unionfs_nodeget(ap->a_dvp->v_mount, vp, NULLVP,
			    ap->a_dvp, ap->a_vpp, cnp);
			if (error) {
				vput(vp);
			} else {
				vrele(vp);
			}
		}
	}

	UNIONFS_INTERNAL_DEBUG("unionfs_mknod: leave (%d)\n", error);

	return (error);
}

static int
unionfs_open(void *v)
{
	struct vop_open_args *ap = v;
	int		error;
	struct unionfs_node *unp;
	struct unionfs_node_status *unsp;
	struct vnode   *uvp;
	struct vnode   *lvp;
	struct vnode   *targetvp;
	kauth_cred_t   cred;

	UNIONFS_INTERNAL_DEBUG("unionfs_open: enter\n");

	error = 0;
	unp = VTOUNIONFS(ap->a_vp);
	uvp = unp->un_uppervp;
	lvp = unp->un_lowervp;
	targetvp = NULLVP;
	cred = ap->a_cred;

	unionfs_get_node_status(unp, &unsp);

	if (unsp->uns_lower_opencnt > 0 || unsp->uns_upper_opencnt > 0) {
		/* vnode is already opend. */
		if (unsp->uns_upper_opencnt > 0)
			targetvp = uvp;
		else
			targetvp = lvp;

		if (targetvp == lvp &&
		    (ap->a_mode & FWRITE) && lvp->v_type == VREG)
			targetvp = NULLVP;
	}
	if (targetvp == NULLVP) {
		if (uvp == NULLVP) {
			if ((ap->a_mode & FWRITE) && lvp->v_type == VREG) {
				error = unionfs_copyfile(unp,
				    !(ap->a_mode & O_TRUNC), cred);
				if (error != 0)
					goto unionfs_open_abort;
				targetvp = uvp = unp->un_uppervp;
			} else
				targetvp = lvp;
		} else
			targetvp = uvp;
	}

	error = VOP_OPEN(targetvp, ap->a_mode, cred);
	if (error == 0) {
		if (targetvp == uvp) {
			if (uvp->v_type == VDIR && lvp != NULLVP &&
			    unsp->uns_lower_opencnt <= 0) {
				/* open lower for readdir */
				error = VOP_OPEN(lvp, FREAD, cred);
				if (error != 0) {
					VOP_CLOSE(uvp, ap->a_mode, cred);
					goto unionfs_open_abort;
				}
				unsp->uns_node_flag |= UNS_OPENL_4_READDIR;
				unsp->uns_lower_opencnt++;
			}
			unsp->uns_upper_opencnt++;
		} else {
			unsp->uns_lower_opencnt++;
			unsp->uns_lower_openmode = ap->a_mode;
		}
	}

unionfs_open_abort:
	if (error != 0)
		unionfs_tryrem_node_status(unp, unsp);

	UNIONFS_INTERNAL_DEBUG("unionfs_open: leave (%d)\n", error);

	return (error);
}

static int
unionfs_close(void *v)
{
	struct vop_close_args *ap = v;
	int		error;
	struct unionfs_node *unp;
	struct unionfs_node_status *unsp;
	kauth_cred_t   cred;
	struct vnode   *ovp;

	UNIONFS_INTERNAL_DEBUG("unionfs_close: enter\n");

	KASSERT(VOP_ISLOCKED(ap->a_vp) == LK_EXCLUSIVE);
	unp = VTOUNIONFS(ap->a_vp);
	cred = ap->a_cred;

	unionfs_get_node_status(unp, &unsp);

	if (unsp->uns_lower_opencnt <= 0 && unsp->uns_upper_opencnt <= 0) {
#ifdef DIAGNOSTIC
		printf("unionfs_close: warning: open count is 0\n");
#endif
		if (unp->un_uppervp != NULLVP)
			ovp = unp->un_uppervp;
		else
			ovp = unp->un_lowervp;
	} else if (unsp->uns_upper_opencnt > 0)
		ovp = unp->un_uppervp;
	else
		ovp = unp->un_lowervp;

	error = VOP_CLOSE(ovp, ap->a_fflag, cred);

	if (error != 0)
		goto unionfs_close_abort;

	if (ovp == unp->un_uppervp) {
		unsp->uns_upper_opencnt--;
		if (unsp->uns_upper_opencnt == 0) {
			if (unsp->uns_node_flag & UNS_OPENL_4_READDIR) {
				VOP_CLOSE(unp->un_lowervp, FREAD, cred);
				unsp->uns_node_flag &= ~UNS_OPENL_4_READDIR;
				unsp->uns_lower_opencnt--;
			}
		}
	} else
		unsp->uns_lower_opencnt--;

unionfs_close_abort:
	unionfs_tryrem_node_status(unp, unsp);

	UNIONFS_INTERNAL_DEBUG("unionfs_close: leave (%d)\n", error);

	return (error);
}

/*
 * Check the access mode toward shadow file/dir.
 */
static int
unionfs_check_corrected_access(u_short mode, struct vattr *va, kauth_cred_t cred)
{
	int		result;
	int		error;
	uid_t		uid;	/* upper side vnode's uid */
	gid_t		gid;	/* upper side vnode's gid */
	u_short		vmode;	/* upper side vnode's mode */
	u_short		mask;

	mask = 0;
	uid = va->va_uid;
	gid = va->va_gid;
	vmode = va->va_mode;

	/* check owner */
	if (kauth_cred_getuid(cred) == uid) {
		if (mode & VEXEC)
			mask |= S_IXUSR;
		if (mode & VREAD)
			mask |= S_IRUSR;
		if (mode & VWRITE)
			mask |= S_IWUSR;
		return ((vmode & mask) == mask ? 0 : EACCES);
	}

	/* check group */
	error = kauth_cred_ismember_gid(cred, gid, &result);
	if (error != 0)
		return error;
	if (result) {
		if (mode & VEXEC)
			mask |= S_IXGRP;
		if (mode & VREAD)
			mask |= S_IRGRP;
		if (mode & VWRITE)
			mask |= S_IWGRP;
		return ((vmode & mask) == mask ? 0 : EACCES);
	}

	/* check other */
	if (mode & VEXEC)
		mask |= S_IXOTH;
	if (mode & VREAD)
		mask |= S_IROTH;
	if (mode & VWRITE)
		mask |= S_IWOTH;

	return ((vmode & mask) == mask ? 0 : EACCES);
}

static int
unionfs_access(void *v)
{	
	struct vop_access_args *ap = v;
	struct unionfs_mount *ump;
	struct unionfs_node *unp;
	struct vnode   *uvp;
	struct vnode   *lvp;
	struct vattr	va;
	int		mode;
	int		error;

	UNIONFS_INTERNAL_DEBUG("unionfs_access: enter\n");

	ump = MOUNTTOUNIONFSMOUNT(ap->a_vp->v_mount);
	unp = VTOUNIONFS(ap->a_vp);
	uvp = unp->un_uppervp;
	lvp = unp->un_lowervp;
	mode = ap->a_mode;
	error = EACCES;

	if ((mode & VWRITE) &&
	    (ap->a_vp->v_mount->mnt_flag & MNT_RDONLY)) {
		switch (ap->a_vp->v_type) {
		case VREG:
		case VDIR:
		case VLNK:
			return (EROFS);
		default:
			break;
		}
	}

	if (uvp != NULLVP) {
		error = VOP_ACCESS(uvp, mode, ap->a_cred);

		UNIONFS_INTERNAL_DEBUG("unionfs_access: leave (%d)\n", error);

		return (error);
	}

	if (lvp != NULLVP) {
		if (mode & VWRITE) {
			if (ump->um_uppervp->v_mount->mnt_flag & MNT_RDONLY) {
				switch (ap->a_vp->v_type) {
				case VREG:
				case VDIR:
				case VLNK:
					return (EROFS);
				default:
					break;
				}
			} else if (ap->a_vp->v_type == VREG || ap->a_vp->v_type == VDIR) {
				/* check shadow file/dir */
				if (ump->um_copymode != UNIONFS_TRANSPARENT) {
					error = unionfs_create_uppervattr(ump,
					    lvp, &va, ap->a_cred);
					if (error != 0)
						return (error);

					error = unionfs_check_corrected_access(
					    mode, &va, ap->a_cred);
					if (error != 0)
						return (error);
				}
			}
			mode &= ~VWRITE;
			mode |= VREAD; /* will copy to upper */
		}
		error = VOP_ACCESS(lvp, mode, ap->a_cred);
	}

	UNIONFS_INTERNAL_DEBUG("unionfs_access: leave (%d)\n", error);

	return (error);
}

static int
unionfs_getattr(void *v)
{
	struct vop_getattr_args *ap = v;
	int		error;
	struct unionfs_node *unp;
	struct unionfs_mount *ump;
	struct vnode   *uvp;
	struct vnode   *lvp;
	struct vattr	va;

	UNIONFS_INTERNAL_DEBUG("unionfs_getattr: enter\n");

	unp = VTOUNIONFS(ap->a_vp);
	ump = MOUNTTOUNIONFSMOUNT(ap->a_vp->v_mount);
	uvp = unp->un_uppervp;
	lvp = unp->un_lowervp;

	if (uvp != NULLVP) {
		if ((error = VOP_GETATTR(uvp, ap->a_vap, ap->a_cred)) == 0)
			ap->a_vap->va_fsid = ap->a_vp->v_mount->mnt_stat.f_fsid;

		UNIONFS_INTERNAL_DEBUG("unionfs_getattr: leave mode=%o, uid=%d, gid=%d (%d)\n",
		    ap->a_vap->va_mode, ap->a_vap->va_uid,
		    ap->a_vap->va_gid, error);

		return (error);
	}

	error = VOP_GETATTR(lvp, ap->a_vap, ap->a_cred);

	if (error == 0 && !(ump->um_uppervp->v_mount->mnt_flag & MNT_RDONLY)) {
		/* correct the attr toward shadow file/dir. */
		if (ap->a_vp->v_type == VREG || ap->a_vp->v_type == VDIR) {
			unionfs_create_uppervattr_core(ump, ap->a_vap, &va);
			ap->a_vap->va_mode = va.va_mode;
			ap->a_vap->va_uid = va.va_uid;
			ap->a_vap->va_gid = va.va_gid;
		}
	}

	if (error == 0)
		ap->a_vap->va_fsid = ap->a_vp->v_mount->mnt_stat.f_fsid;

	UNIONFS_INTERNAL_DEBUG("unionfs_getattr: leave mode=%o, uid=%d, gid=%d (%d)\n",
	    ap->a_vap->va_mode, ap->a_vap->va_uid, ap->a_vap->va_gid, error);

	return (error);
}

static int
unionfs_setattr(void *v)
{
	struct vop_setattr_args *ap = v;
	int		error;
	struct unionfs_node *unp;
	struct vnode   *uvp;
	struct vnode   *lvp;
	struct vattr   *vap;

	UNIONFS_INTERNAL_DEBUG("unionfs_setattr: enter\n");

	error = EROFS;
	unp = VTOUNIONFS(ap->a_vp);
	uvp = unp->un_uppervp;
	lvp = unp->un_lowervp;
	vap = ap->a_vap;

	if ((ap->a_vp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (vap->va_flags != VNOVAL || vap->va_uid != (uid_t)VNOVAL ||
	     vap->va_gid != (gid_t)VNOVAL || vap->va_atime.tv_sec != VNOVAL ||
	     vap->va_mtime.tv_sec != VNOVAL || vap->va_mode != (mode_t)VNOVAL))
		return (EROFS);

	if (uvp == NULLVP && lvp->v_type == VREG) {
		error = unionfs_copyfile(unp, (vap->va_size != 0),
		    ap->a_cred);
		if (error != 0)
			return (error);
		uvp = unp->un_uppervp;
	}

	if (uvp != NULLVP)
		error = VOP_SETATTR(uvp, vap, ap->a_cred);

	UNIONFS_INTERNAL_DEBUG("unionfs_setattr: leave (%d)\n", error);

	return (error);
}

static int
unionfs_read(void *v)
{
	struct vop_read_args *ap = v;
	int		error;
	struct unionfs_node *unp;
	struct vnode   *tvp;

	/* UNIONFS_INTERNAL_DEBUG("unionfs_read: enter\n"); */

	unp = VTOUNIONFS(ap->a_vp);
	tvp = (unp->un_uppervp != NULLVP ? unp->un_uppervp : unp->un_lowervp);

	error = VOP_READ(tvp, ap->a_uio, ap->a_ioflag, ap->a_cred);

	/* UNIONFS_INTERNAL_DEBUG("unionfs_read: leave (%d)\n", error); */

	return (error);
}

static int
unionfs_write(void *v)
{
	struct vop_write_args *ap = v;
	int		error;
	struct unionfs_node *unp;
	struct vnode   *tvp;

	/* UNIONFS_INTERNAL_DEBUG("unionfs_write: enter\n"); */

	unp = VTOUNIONFS(ap->a_vp);
	tvp = (unp->un_uppervp != NULLVP ? unp->un_uppervp : unp->un_lowervp);

	error = VOP_WRITE(tvp, ap->a_uio, ap->a_ioflag, ap->a_cred);

	/* UNIONFS_INTERNAL_DEBUG("unionfs_write: leave (%d)\n", error); */

	return (error);
}

static int
unionfs_ioctl(void *v)
{
	struct vop_ioctl_args *ap = v;
	int error;
	struct unionfs_node *unp;
	struct unionfs_node_status *unsp;
	struct vnode   *ovp;

	UNIONFS_INTERNAL_DEBUG("unionfs_ioctl: enter\n");

 	vn_lock(ap->a_vp, LK_EXCLUSIVE | LK_RETRY);
	unp = VTOUNIONFS(ap->a_vp);
	unionfs_get_node_status(unp, &unsp);
	ovp = (unsp->uns_upper_opencnt ? unp->un_uppervp : unp->un_lowervp);
	unionfs_tryrem_node_status(unp, unsp);
	VOP_UNLOCK(ap->a_vp);

	if (ovp == NULLVP)
		return (EBADF);

	error = VOP_IOCTL(ovp, ap->a_command, ap->a_data, ap->a_fflag,
	    ap->a_cred);

	UNIONFS_INTERNAL_DEBUG("unionfs_ioctl: lease (%d)\n", error);

	return (error);
}

static int
unionfs_poll(void *v)
{
	struct vop_poll_args *ap = v;
	struct unionfs_node *unp;
	struct unionfs_node_status *unsp;
	struct vnode   *ovp;

 	vn_lock(ap->a_vp, LK_EXCLUSIVE | LK_RETRY);
	unp = VTOUNIONFS(ap->a_vp);
	unionfs_get_node_status(unp, &unsp);
	ovp = (unsp->uns_upper_opencnt ? unp->un_uppervp : unp->un_lowervp);
	unionfs_tryrem_node_status(unp, unsp);
	VOP_UNLOCK(ap->a_vp);

	if (ovp == NULLVP)
		return (EBADF);

	return (VOP_POLL(ovp, ap->a_events));
}

static int
unionfs_fsync(void *v)
{
	struct vop_fsync_args *ap = v;
	struct unionfs_node *unp;
	struct unionfs_node_status *unsp;
	struct vnode   *ovp;

	unp = VTOUNIONFS(ap->a_vp);
	unionfs_get_node_status(unp, &unsp);
	ovp = (unsp->uns_upper_opencnt ? unp->un_uppervp : unp->un_lowervp);
	unionfs_tryrem_node_status(unp, unsp);

	if (ovp == NULLVP)
		return (EBADF);

	return (VOP_FSYNC(ovp, ap->a_cred, ap->a_flags, ap->a_offlo, ap->a_offhi));
}

static int
unionfs_remove(void *v)
{
	struct vop_remove_args *ap = v;
	int		error;
	struct unionfs_node *dunp;
	struct unionfs_node *unp;
	struct unionfs_mount *ump;
	struct vnode   *udvp;
	struct vnode   *uvp;
	struct vnode   *lvp;
	struct componentname *cnp;

	UNIONFS_INTERNAL_DEBUG("unionfs_remove: enter\n");

	error = 0;
	dunp = VTOUNIONFS(ap->a_dvp);
	unp = VTOUNIONFS(ap->a_vp);
	udvp = dunp->un_uppervp;
	uvp = unp->un_uppervp;
	lvp = unp->un_lowervp;
	cnp = ap->a_cnp;

	if (udvp == NULLVP)
		return (EROFS);

	if (uvp != NULLVP) {
		ump = MOUNTTOUNIONFSMOUNT(ap->a_vp->v_mount);
		if (ump->um_whitemode == UNIONFS_WHITE_ALWAYS || lvp != NULLVP)
			cnp->cn_flags |= DOWHITEOUT;
		error = VOP_REMOVE(udvp, uvp, cnp);
	} else if (lvp != NULLVP)
		error = unionfs_mkwhiteout(udvp, cnp, unp->un_path);

	UNIONFS_INTERNAL_DEBUG("unionfs_remove: leave (%d)\n", error);

	return (error);
}

static int
unionfs_link(void *v)
{
#if 0
	struct vop_link_v2_args *ap = v;
	int		error;
	int		needrelookup;
	struct unionfs_node *dunp;
	struct unionfs_node *unp;
	struct vnode   *udvp;
	struct vnode   *uvp;
	struct componentname *cnp;

	UNIONFS_INTERNAL_DEBUG("unionfs_link: enter\n");

	error = 0;
	needrelookup = 0;
	dunp = VTOUNIONFS(ap->a_tdvp);
	unp = NULL;
	udvp = dunp->un_uppervp;
	uvp = NULLVP;
	cnp = ap->a_cnp;

	if (udvp == NULLVP)
		return (EROFS);

	if (ap->a_vp->v_op != unionfs_vnodeop_p)
		uvp = ap->a_vp;
	else {
		unp = VTOUNIONFS(ap->a_vp);

		if (unp->un_uppervp == NULLVP) {
			if (ap->a_vp->v_type != VREG)
				return (EOPNOTSUPP);

			error = unionfs_copyfile(unp, 1, cnp->cn_cred);
			if (error != 0)
				return (error);
			needrelookup = 1;
		}
		uvp = unp->un_uppervp;
	}

	if (needrelookup != 0)
		error = unionfs_relookup_for_create(ap->a_tdvp, cnp);

	if (error == 0)
		error = VOP_LINK(udvp, uvp, cnp);

	UNIONFS_INTERNAL_DEBUG("unionfs_link: leave (%d)\n", error);

	return (error);
#else
	panic("XXXAD");
	return 0;
#endif
}

static int
unionfs_rename(void *v)
{
	struct vop_rename_args *ap = v;
	int		error;
	struct vnode   *fdvp;
	struct vnode   *fvp;
	struct componentname *fcnp;
	struct vnode   *tdvp;
	struct vnode   *tvp;
	struct componentname *tcnp;
	struct vnode   *ltdvp;
	struct vnode   *ltvp;

	/* rename target vnodes */
	struct vnode   *rfdvp;
	struct vnode   *rfvp;
	struct vnode   *rtdvp;
	struct vnode   *rtvp;

	int		needrelookup;
	struct unionfs_mount *ump;
	struct unionfs_node *unp;

	UNIONFS_INTERNAL_DEBUG("unionfs_rename: enter\n");

	error = 0;
	fdvp = ap->a_fdvp;
	fvp = ap->a_fvp;
	fcnp = ap->a_fcnp;
	tdvp = ap->a_tdvp;
	tvp = ap->a_tvp;
	tcnp = ap->a_tcnp;
	ltdvp = NULLVP;
	ltvp = NULLVP;
	rfdvp = fdvp;
	rfvp = fvp;
	rtdvp = tdvp;
	rtvp = tvp;
	needrelookup = 0;

	/* check for cross device rename */
	if (fvp->v_mount != tdvp->v_mount ||
	    (tvp != NULLVP && fvp->v_mount != tvp->v_mount)) {
		error = EXDEV;
		goto unionfs_rename_abort;
	}

	/* Renaming a file to itself has no effect. */
	if (fvp == tvp)
		goto unionfs_rename_abort;

	/*
	 * from/to vnode is unionfs node.
	 */

	unp = VTOUNIONFS(fdvp);
#ifdef UNIONFS_IDBG_RENAME
	UNIONFS_INTERNAL_DEBUG("fdvp=%p, ufdvp=%p, lfdvp=%p\n", fdvp, unp->un_uppervp, unp->un_lowervp);
#endif
	if (unp->un_uppervp == NULLVP) {
		error = ENODEV;
		goto unionfs_rename_abort;
	}
	rfdvp = unp->un_uppervp;
	vref(rfdvp);

	unp = VTOUNIONFS(fvp);
#ifdef UNIONFS_IDBG_RENAME
	UNIONFS_INTERNAL_DEBUG("fvp=%p, ufvp=%p, lfvp=%p\n", fvp, unp->un_uppervp, unp->un_lowervp);
#endif
	ump = MOUNTTOUNIONFSMOUNT(fvp->v_mount);
	if (unp->un_uppervp == NULLVP) {
		switch (fvp->v_type) {
		case VREG:
			if ((error = vn_lock(fvp, LK_EXCLUSIVE)) != 0)
				goto unionfs_rename_abort;
			error = unionfs_copyfile(unp, 1, fcnp->cn_cred);
			VOP_UNLOCK(fvp);
			if (error != 0)
				goto unionfs_rename_abort;
			break;
		case VDIR:
			if ((error = vn_lock(fvp, LK_EXCLUSIVE)) != 0)
				goto unionfs_rename_abort;
			error = unionfs_mkshadowdir(ump, rfdvp, unp, fcnp);
			VOP_UNLOCK(fvp);
			if (error != 0)
				goto unionfs_rename_abort;
			break;
		default:
			error = ENODEV;
			goto unionfs_rename_abort;
		}

		needrelookup = 1;
	}

	if (unp->un_lowervp != NULLVP)
		fcnp->cn_flags |= DOWHITEOUT;
	rfvp = unp->un_uppervp;
	vref(rfvp);

	unp = VTOUNIONFS(tdvp);
#ifdef UNIONFS_IDBG_RENAME
	UNIONFS_INTERNAL_DEBUG("tdvp=%p, utdvp=%p, ltdvp=%p\n", tdvp, unp->un_uppervp, unp->un_lowervp);
#endif
	if (unp->un_uppervp == NULLVP) {
		error = ENODEV;
		goto unionfs_rename_abort;
	}
	rtdvp = unp->un_uppervp;
	ltdvp = unp->un_lowervp;
	vref(rtdvp);

	if (tdvp == tvp) {
		rtvp = rtdvp;
		vref(rtvp);
	} else if (tvp != NULLVP) {
		unp = VTOUNIONFS(tvp);
#ifdef UNIONFS_IDBG_RENAME
		UNIONFS_INTERNAL_DEBUG("tvp=%p, utvp=%p, ltvp=%p\n", tvp, unp->un_uppervp, unp->un_lowervp);
#endif
		if (unp->un_uppervp == NULLVP)
			rtvp = NULLVP;
		else {
			if (tvp->v_type == VDIR) {
				error = EINVAL;
				goto unionfs_rename_abort;
			}
			rtvp = unp->un_uppervp;
			ltvp = unp->un_lowervp;
			vref(rtvp);
		}
	}

	if (needrelookup != 0) {
		if ((error = vn_lock(fdvp, LK_EXCLUSIVE)) != 0)
			goto unionfs_rename_abort;
		error = unionfs_relookup_for_delete(fdvp, fcnp);
		VOP_UNLOCK(fdvp);
		if (error != 0)
			goto unionfs_rename_abort;

		/* Locke of tvp is canceled in order to avoid recursive lock. */
		if (tvp != NULLVP && tvp != tdvp)
			VOP_UNLOCK(tvp);
		error = unionfs_relookup_for_rename(tdvp, tcnp);
		if (tvp != NULLVP && tvp != tdvp)
			vn_lock(tvp, LK_EXCLUSIVE | LK_RETRY);
		if (error != 0)
			goto unionfs_rename_abort;
	}

	error = VOP_RENAME(rfdvp, rfvp, fcnp, rtdvp, rtvp, tcnp);

	if (error == 0) {
		if (rtvp != NULLVP && rtvp->v_type == VDIR)
			cache_purge(tdvp);
		if (fvp->v_type == VDIR && fdvp != tdvp)
			cache_purge(fdvp);
	}

	if (fdvp != rfdvp)
		vrele(fdvp);
	if (fvp != rfvp)
		vrele(fvp);
	if (ltdvp != NULLVP)
		VOP_UNLOCK(ltdvp);
	if (tdvp != rtdvp)
		vrele(tdvp);
	if (ltvp != NULLVP)
		VOP_UNLOCK(ltvp);
	if (tvp != rtvp && tvp != NULLVP) {
		if (rtvp == NULLVP)
			vput(tvp);
		else
			vrele(tvp);
	}

	UNIONFS_INTERNAL_DEBUG("unionfs_rename: leave (%d)\n", error);

	return (error);

unionfs_rename_abort:
	if (fdvp != rfdvp)
		vrele(rfdvp);
	if (fvp != rfvp)
		vrele(rfvp);
	if (tdvp != rtdvp)
		vrele(rtdvp);
	vput(tdvp);
	if (tvp != rtvp && rtvp != NULLVP)
		vrele(rtvp);
	if (tvp != NULLVP) {
		if (tdvp != tvp)
			vput(tvp);
		else
			vrele(tvp);
	}
	vrele(fdvp);
	vrele(fvp);

	UNIONFS_INTERNAL_DEBUG("unionfs_rename: leave (%d)\n", error);

	return (error);
}

static int
unionfs_mkdir(void *v)
{
	struct vop_mkdir_args *ap = v;
	int		error;
	struct unionfs_node *dunp;
	struct componentname *cnp;
	struct vnode   *udvp;
	struct vnode   *uvp;
	struct vattr	va;

	UNIONFS_INTERNAL_DEBUG("unionfs_mkdir: enter\n");

	error = EROFS;
	dunp = VTOUNIONFS(ap->a_dvp);
	cnp = ap->a_cnp;
	udvp = dunp->un_uppervp;

	if (udvp != NULLVP) {
		/* check opaque */
		if (!(cnp->cn_flags & ISWHITEOUT)) {
			error = VOP_GETATTR(udvp, &va, cnp->cn_cred);
			if (error != 0)
				return (error);
			if (va.va_flags & OPAQUE) 
				cnp->cn_flags |= ISWHITEOUT;
		}

		if ((error = VOP_MKDIR(udvp, &uvp, cnp, ap->a_vap)) == 0) {
			error = unionfs_nodeget(ap->a_dvp->v_mount, uvp, NULLVP,
			    ap->a_dvp, ap->a_vpp, cnp);
			if (error) {
				vput(uvp);
			} else {
				vrele(uvp);
			}
		}
	}

	UNIONFS_INTERNAL_DEBUG("unionfs_mkdir: leave (%d)\n", error);

	return (error);
}

static int
unionfs_rmdir(void *v)
{
	struct vop_rmdir_args *ap = v;
	int		error;
	struct unionfs_node *dunp;
	struct unionfs_node *unp;
	struct unionfs_mount *ump;
	struct componentname *cnp;
	struct vnode   *udvp;
	struct vnode   *uvp;
	struct vnode   *lvp;

	UNIONFS_INTERNAL_DEBUG("unionfs_rmdir: enter\n");

	error = 0;
	dunp = VTOUNIONFS(ap->a_dvp);
	unp = VTOUNIONFS(ap->a_vp);
	cnp = ap->a_cnp;
	udvp = dunp->un_uppervp;
	uvp = unp->un_uppervp;
	lvp = unp->un_lowervp;

	if (udvp == NULLVP)
		return (EROFS);

	if (udvp == uvp)
		return (EOPNOTSUPP);

	if (uvp != NULLVP) {
		if (lvp != NULLVP) {
			error = unionfs_check_rmdir(ap->a_vp, cnp->cn_cred);
			if (error != 0)
				return (error);
		}
		ump = MOUNTTOUNIONFSMOUNT(ap->a_vp->v_mount);
		if (ump->um_whitemode == UNIONFS_WHITE_ALWAYS || lvp != NULLVP)
			cnp->cn_flags |= DOWHITEOUT;
		error = VOP_RMDIR(udvp, uvp, cnp);
	}
	else if (lvp != NULLVP)
		error = unionfs_mkwhiteout(udvp, cnp, unp->un_path);

	if (error == 0) {
		cache_purge(ap->a_dvp);
		cache_purge(ap->a_vp);
	}

	UNIONFS_INTERNAL_DEBUG("unionfs_rmdir: leave (%d)\n", error);

	return (error);
}

static int
unionfs_symlink(void *v)
{
	struct vop_symlink_args *ap = v;
	int		error;
	struct unionfs_node *dunp;
	struct componentname *cnp;
	struct vnode   *udvp;
	struct vnode   *uvp;

	UNIONFS_INTERNAL_DEBUG("unionfs_symlink: enter\n");

	error = EROFS;
	dunp = VTOUNIONFS(ap->a_dvp);
	cnp = ap->a_cnp;
	udvp = dunp->un_uppervp;

	if (udvp != NULLVP) {
		error = VOP_SYMLINK(udvp, &uvp, cnp, ap->a_vap, ap->a_target);
		if (error == 0) {
			error = unionfs_nodeget(ap->a_dvp->v_mount, uvp, NULLVP,
			    ap->a_dvp, ap->a_vpp, cnp);
			if (error) {
				vput(uvp);
			} else {
				vrele(uvp);
			}
		}
	}

	UNIONFS_INTERNAL_DEBUG("unionfs_symlink: leave (%d)\n", error);

	return (error);
}

static int
unionfs_readdir(void *v)
{
	struct vop_readdir_args *ap = v;
	int		error;
	int		eofflag;
	int		locked;
	struct unionfs_node *unp;
	struct unionfs_node_status *unsp;
	struct uio     *uio;
	struct vnode   *uvp;
	struct vnode   *lvp;
	struct vattr    va;

	int		ncookies_bk;
	off_t          *cookies_bk;

	UNIONFS_INTERNAL_DEBUG("unionfs_readdir: enter\n");

	error = 0;
	eofflag = 0;
	locked = 0;
	unp = VTOUNIONFS(ap->a_vp);
	uio = ap->a_uio;
	uvp = unp->un_uppervp;
	lvp = unp->un_lowervp;
	ncookies_bk = 0;
	cookies_bk = NULL;

	if (ap->a_vp->v_type != VDIR)
		return (ENOTDIR);

	/* check opaque */
	if (uvp != NULLVP && lvp != NULLVP) {
		if ((error = VOP_GETATTR(uvp, &va, ap->a_cred)) != 0)
			goto unionfs_readdir_exit;
		if (va.va_flags & OPAQUE)
			lvp = NULLVP;
	}

	/* check the open count. unionfs needs to open before readdir. */
	VOP_UNLOCK(ap->a_vp);
	vn_lock(ap->a_vp, LK_EXCLUSIVE | LK_RETRY);
	unionfs_get_node_status(unp, &unsp);
	if ((uvp != NULLVP && unsp->uns_upper_opencnt <= 0) ||
	    (lvp != NULLVP && unsp->uns_lower_opencnt <= 0)) {
		unionfs_tryrem_node_status(unp, unsp);
		error = EBADF;
	}
	if (error != 0)
		goto unionfs_readdir_exit;

	/* upper only */
	if (uvp != NULLVP && lvp == NULLVP) {
		error = VOP_READDIR(uvp, uio, ap->a_cred, ap->a_eofflag,
		    ap->a_cookies, ap->a_ncookies);
		unsp->uns_readdir_status = 0;

		goto unionfs_readdir_exit;
	}

	/* lower only */
	if (uvp == NULLVP && lvp != NULLVP) {
		error = VOP_READDIR(lvp, uio, ap->a_cred, ap->a_eofflag,
		    ap->a_cookies, ap->a_ncookies);
		unsp->uns_readdir_status = 2;

		goto unionfs_readdir_exit;
	}

	/*
	 * readdir upper and lower
	 */
	KASSERT(uvp != NULLVP);
	KASSERT(lvp != NULLVP);
	if (uio->uio_offset == 0)
		unsp->uns_readdir_status = 0;

	if (unsp->uns_readdir_status == 0) {
		/* read upper */
		error = VOP_READDIR(uvp, uio, ap->a_cred, &eofflag,
				    ap->a_cookies, ap->a_ncookies);

		if (error != 0 || eofflag == 0)
			goto unionfs_readdir_exit;
		unsp->uns_readdir_status = 1;

		/*
		 * ufs(and other fs) needs size of uio_resid larger than
		 * DIRBLKSIZ.
		 * size of DIRBLKSIZ equals DEV_BSIZE.
		 * (see: ufs/ufs/ufs_vnops.c ufs_readdir func , ufs/ufs/dir.h)
		 */
		if (uio->uio_resid <= (uio->uio_resid & (DEV_BSIZE -1)))
			goto unionfs_readdir_exit;

		/*
		 * backup cookies
		 * It prepares to readdir in lower.
		 */
		if (ap->a_ncookies != NULL) {
			ncookies_bk = *(ap->a_ncookies);
			*(ap->a_ncookies) = 0;
		}
		if (ap->a_cookies != NULL) {
			cookies_bk = *(ap->a_cookies);
			*(ap->a_cookies) = NULL;
		}
	}

	/* initialize for readdir in lower */
	if (unsp->uns_readdir_status == 1) {
		unsp->uns_readdir_status = 2;
		uio->uio_offset = 0;
	}

	if (lvp == NULLVP) {
		error = EBADF;
		goto unionfs_readdir_exit;
	}
	/* read lower */
	error = VOP_READDIR(lvp, uio, ap->a_cred, ap->a_eofflag,
			    ap->a_cookies, ap->a_ncookies);

	if (cookies_bk != NULL) {
		/* merge cookies */
		int		size;
		off_t         *newcookies, *pos;

		size = *(ap->a_ncookies) + ncookies_bk;
		newcookies = (off_t *) malloc(size * sizeof(off_t),
		    M_TEMP, M_WAITOK);
		pos = newcookies;

		memcpy(pos, cookies_bk, ncookies_bk * sizeof(off_t));
		pos += ncookies_bk * sizeof(off_t);
		memcpy(pos, *(ap->a_cookies), *(ap->a_ncookies) * sizeof(off_t));
		free(cookies_bk, M_TEMP);
		free(*(ap->a_cookies), M_TEMP);
		*(ap->a_ncookies) = size;
		*(ap->a_cookies) = newcookies;
	}

unionfs_readdir_exit:
	if (error != 0 && ap->a_eofflag != NULL)
		*(ap->a_eofflag) = 1;

	UNIONFS_INTERNAL_DEBUG("unionfs_readdir: leave (%d)\n", error);

	return (error);
}

static int
unionfs_readlink(void *v)
{
	struct vop_readlink_args *ap = v;
	int error;
	struct unionfs_node *unp;
	struct vnode   *vp;

	UNIONFS_INTERNAL_DEBUG("unionfs_readlink: enter\n");

	unp = VTOUNIONFS(ap->a_vp);
	vp = (unp->un_uppervp != NULLVP ? unp->un_uppervp : unp->un_lowervp);

	error = VOP_READLINK(vp, ap->a_uio, ap->a_cred);

	UNIONFS_INTERNAL_DEBUG("unionfs_readlink: leave (%d)\n", error);

	return (error);
}

static int
unionfs_inactive(void *v)
{
	struct vop_inactive_args *ap = v;
	*ap->a_recycle = true;
	VOP_UNLOCK(ap->a_vp);
	return (0);
}

static int
unionfs_reclaim(void *v)
{
	struct vop_reclaim_args *ap = v;

	/* UNIONFS_INTERNAL_DEBUG("unionfs_reclaim: enter\n"); */

	unionfs_noderem(ap->a_vp);

	/* UNIONFS_INTERNAL_DEBUG("unionfs_reclaim: leave\n"); */

	return (0);
}

static int
unionfs_print(void *v)
{
	struct vop_print_args *ap = v;
	struct unionfs_node *unp;
	/* struct unionfs_node_status *unsp; */

	unp = VTOUNIONFS(ap->a_vp);
	/* unionfs_get_node_status(unp, &unsp); */

	printf("unionfs_vp=%p, uppervp=%p, lowervp=%p\n",
	    ap->a_vp, unp->un_uppervp, unp->un_lowervp);
	/*
	printf("unionfs opencnt: uppervp=%d, lowervp=%d\n",
	    unsp->uns_upper_opencnt, unsp->uns_lower_opencnt);
	*/

	if (unp->un_uppervp != NULLVP)
		vprint("unionfs: upper", unp->un_uppervp);
	if (unp->un_lowervp != NULLVP)
		vprint("unionfs: lower", unp->un_lowervp);

	return (0);
}

static int
unionfs_lock(void *v)
{
	struct vop_lock_args *ap = v;
	int		error;
	int		flags;
	struct vnode   *lvp;
	struct vnode   *uvp;
	struct unionfs_node *unp;

	unp = VTOUNIONFS(ap->a_vp);
	lvp = unp->un_lowervp;
	uvp = unp->un_uppervp;
	flags = ap->a_flags;
	error = 0;

	if (lvp != NULLVP) {
		error = VOP_LOCK(lvp, flags);
	}
	if (error == 0 && uvp != NULLVP) {
		error = VOP_LOCK(uvp, flags);
		if (error != 0) {
			VOP_UNLOCK(lvp);
		}
	}

	return error;
}

static int
unionfs_unlock(void *v)
{
	struct vop_unlock_args *ap = v;
	int		error;
	struct vnode   *lvp;
	struct vnode   *uvp;
	struct unionfs_node *unp;

	unp = VTOUNIONFS(ap->a_vp);
	lvp = unp->un_lowervp;
	uvp = unp->un_uppervp;
	error = 0;

	if (lvp != NULLVP) {
		error = VOP_UNLOCK(lvp);
	}
	if (error == 0 && uvp != NULLVP) {
		error = VOP_UNLOCK(uvp);
	}

	return error;
}

static int
unionfs_pathconf(void *v)
{
	struct vop_pathconf_args *ap = v;
	struct unionfs_node *unp;
	struct vnode   *vp;

	unp = VTOUNIONFS(ap->a_vp);
	vp = (unp->un_uppervp != NULLVP ? unp->un_uppervp : unp->un_lowervp);

	return (VOP_PATHCONF(vp, ap->a_name, ap->a_retval));
}

static int
unionfs_advlock(void *v)
{
	struct vop_advlock_args *ap = v;
	int error;
	struct unionfs_node *unp;
	struct unionfs_node_status *unsp;
	struct vnode   *vp;
	struct vnode   *uvp;
	kauth_cred_t	cred;

	UNIONFS_INTERNAL_DEBUG("unionfs_advlock: enter\n");

	vp = ap->a_vp;
	cred = kauth_cred_get();

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);

	unp = VTOUNIONFS(ap->a_vp);
	uvp = unp->un_uppervp;

	if (uvp == NULLVP) {
		error = unionfs_copyfile(unp, 1, cred);
		if (error != 0)
			goto unionfs_advlock_abort;
		uvp = unp->un_uppervp;

		unionfs_get_node_status(unp, &unsp);
		if (unsp->uns_lower_opencnt > 0) {
			/* try reopen the vnode */
			error = VOP_OPEN(uvp, unsp->uns_lower_openmode, cred);
			if (error)
				goto unionfs_advlock_abort;
			unsp->uns_upper_opencnt++;
			VOP_CLOSE(unp->un_lowervp, unsp->uns_lower_openmode, cred);
			unsp->uns_lower_opencnt--;
		} else
			unionfs_tryrem_node_status(unp, unsp);
	}

	VOP_UNLOCK(vp);

	error = VOP_ADVLOCK(uvp, ap->a_id, ap->a_op, ap->a_fl, ap->a_flags);

	UNIONFS_INTERNAL_DEBUG("unionfs_advlock: leave (%d)\n", error);

	return error;

unionfs_advlock_abort:
	VOP_UNLOCK(vp);

	UNIONFS_INTERNAL_DEBUG("unionfs_advlock: leave (%d)\n", error);

	return error;
}

static int
unionfs_strategy(void *v)
{
	struct vop_strategy_args *ap = v;
	struct unionfs_node *unp;
	struct vnode   *vp;

	unp = VTOUNIONFS(ap->a_vp);
	vp = (unp->un_uppervp != NULLVP ? unp->un_uppervp : unp->un_lowervp);

#ifdef DIAGNOSTIC
	if (vp == NULLVP)
		panic("unionfs_strategy: nullvp");
	if ((ap->a_bp->b_flags & B_READ) == 0 && vp == unp->un_lowervp)
		panic("unionfs_strategy: writing to lowervp");
#endif

	return (VOP_STRATEGY(vp, ap->a_bp));
}

static int
unionfs_kqfilter(void *v)
{
	struct vop_kqfilter_args *ap = v;
	struct unionfs_node *unp;
	struct vnode   *tvp;

	unp = VTOUNIONFS(ap->a_vp);
	tvp = (unp->un_uppervp != NULLVP ? unp->un_uppervp : unp->un_lowervp);

	return VOP_KQFILTER(tvp, ap->a_kn);
}

static int
unionfs_bmap(void *v)
{
	struct vop_bmap_args *ap = v;
	struct unionfs_node *unp;
	struct vnode   *tvp;

	unp = VTOUNIONFS(ap->a_vp);
	tvp = (unp->un_uppervp != NULLVP ? unp->un_uppervp : unp->un_lowervp);

	return VOP_BMAP(tvp, ap->a_bn, ap->a_vpp, ap->a_bnp, ap->a_runp);
}

static int
unionfs_mmap(void *v)
{
	struct vop_mmap_args *ap = v;
	struct unionfs_node *unp;
	struct vnode   *tvp;

	unp = VTOUNIONFS(ap->a_vp);
	tvp = (unp->un_uppervp != NULLVP ? unp->un_uppervp : unp->un_lowervp);

	return VOP_MMAP(tvp, ap->a_prot, ap->a_cred);
}

static int
unionfs_abortop(void *v)
{
	struct vop_abortop_args *ap = v;
	struct unionfs_node *unp;
	struct vnode   *tvp;

	unp = VTOUNIONFS(ap->a_dvp);
	tvp = (unp->un_uppervp != NULLVP ? unp->un_uppervp : unp->un_lowervp);

	return VOP_ABORTOP(tvp, ap->a_cnp);
}

static int
unionfs_islocked(void *v)
{
	struct vop_islocked_args *ap = v;
	struct unionfs_node *unp;
	struct vnode   *tvp;

	unp = VTOUNIONFS(ap->a_vp);
	tvp = (unp->un_uppervp != NULLVP ? unp->un_uppervp : unp->un_lowervp);

	return VOP_ISLOCKED(tvp);
}

static int
unionfs_seek(void *v)
{
	struct vop_seek_args *ap = v;
	struct unionfs_node *unp;
	struct vnode   *tvp;

	unp = VTOUNIONFS(ap->a_vp);
	tvp = (unp->un_uppervp != NULLVP ? unp->un_uppervp : unp->un_lowervp);

	return VOP_SEEK(tvp, ap->a_oldoff, ap->a_newoff, ap->a_cred);
}

static int
unionfs_putpages(void *v)
{
	struct vop_putpages_args /* {
		struct vnode *a_vp;
		voff_t a_offlo;
		voff_t a_offhi;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp, *tvp;
	struct unionfs_node *unp;

	KASSERT(mutex_owned(vp->v_interlock));

	unp = VTOUNIONFS(vp);
	tvp = (unp->un_uppervp != NULLVP ? unp->un_uppervp : unp->un_lowervp);
	KASSERT(tvp->v_interlock == vp->v_interlock);

	if (ap->a_flags & PGO_RECLAIM) {
		mutex_exit(vp->v_interlock);
		return 0;
	}
	return VOP_PUTPAGES(tvp, ap->a_offlo, ap->a_offhi, ap->a_flags);
}

static int
unionfs_getpages(void *v)
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
	struct vnode *vp = ap->a_vp, *tvp;
	struct unionfs_node *unp;

	KASSERT(mutex_owned(vp->v_interlock));

	unp = VTOUNIONFS(vp);
	tvp = (unp->un_uppervp != NULLVP ? unp->un_uppervp : unp->un_lowervp);
	KASSERT(tvp->v_interlock == vp->v_interlock);

	if (ap->a_flags & PGO_LOCKED) {
		return EBUSY;
	}
	return VOP_GETPAGES(tvp, ap->a_offset, ap->a_m, ap->a_count,
	    ap->a_centeridx, ap->a_access_type, ap->a_advice, ap->a_flags);
}

static int
unionfs_revoke(void *v)
{
	struct vop_revoke_args *ap = v;
	struct unionfs_node *unp;
	struct vnode   *tvp;
	int error;

	unp = VTOUNIONFS(ap->a_vp);
	tvp = (unp->un_uppervp != NULLVP ? unp->un_uppervp : unp->un_lowervp);

	error = VOP_REVOKE(tvp, ap->a_flags);
	if (error == 0) {
		vgone(ap->a_vp);	/* ??? */
	}
	return error;
}

/*
 * Global vfs data structures
 */
int (**unionfs_vnodeop_p)(void *);
const struct vnodeopv_entry_desc unionfs_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, unionfs_lookup },		/* lookup */
	{ &vop_create_desc, unionfs_create },		/* create */
	{ &vop_whiteout_desc, unionfs_whiteout },	/* whiteout */
	{ &vop_mknod_desc, unionfs_mknod },		/* mknod */
	{ &vop_open_desc, unionfs_open },		/* open */
	{ &vop_close_desc, unionfs_close },		/* close */
	{ &vop_access_desc, unionfs_access },		/* access */
	{ &vop_getattr_desc, unionfs_getattr },		/* getattr */
	{ &vop_setattr_desc, unionfs_setattr },		/* setattr */
	{ &vop_read_desc, unionfs_read },		/* read */
	{ &vop_write_desc, unionfs_write },		/* write */
	{ &vop_fallocate_desc, genfs_eopnotsupp },	/* fallocate */
	{ &vop_fdiscard_desc, genfs_eopnotsupp },	/* fdiscard */
	{ &vop_ioctl_desc, unionfs_ioctl },		/* ioctl */
	{ &vop_poll_desc, unionfs_poll },		/* select */
	{ &vop_revoke_desc, unionfs_revoke },		/* revoke */
	{ &vop_mmap_desc, unionfs_mmap },		/* mmap */
	{ &vop_fsync_desc, unionfs_fsync },		/* fsync */
	{ &vop_seek_desc, unionfs_seek },		/* seek */
	{ &vop_remove_desc, unionfs_remove },		/* remove */
	{ &vop_link_desc, unionfs_link },		/* link */
	{ &vop_rename_desc, unionfs_rename },		/* rename */
	{ &vop_mkdir_desc, unionfs_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, unionfs_rmdir },		/* rmdir */
	{ &vop_symlink_desc, unionfs_symlink },		/* symlink */
	{ &vop_readdir_desc, unionfs_readdir },		/* readdir */
	{ &vop_readlink_desc, unionfs_readlink },	/* readlink */
	{ &vop_abortop_desc, unionfs_abortop },		/* abortop */
	{ &vop_inactive_desc, unionfs_inactive },	/* inactive */
	{ &vop_reclaim_desc, unionfs_reclaim },		/* reclaim */
	{ &vop_lock_desc, unionfs_lock },		/* lock */
	{ &vop_unlock_desc, unionfs_unlock },		/* unlock */
	{ &vop_bmap_desc, unionfs_bmap },		/* bmap */
	{ &vop_strategy_desc, unionfs_strategy },	/* strategy */
	{ &vop_print_desc, unionfs_print },		/* print */
	{ &vop_islocked_desc, unionfs_islocked },	/* islocked */
	{ &vop_pathconf_desc, unionfs_pathconf },	/* pathconf */
	{ &vop_advlock_desc, unionfs_advlock },		/* advlock */
	{ &vop_getpages_desc, unionfs_getpages },	/* getpages */
	{ &vop_putpages_desc, unionfs_putpages },	/* putpages */
	{ &vop_kqfilter_desc, unionfs_kqfilter },	/* kqfilter */
#ifdef notdef
	{ &vop_bwrite_desc, unionfs_bwrite },		/* bwrite */
#endif
	{ NULL, NULL }
};
const struct vnodeopv_desc unionfs_vnodeop_opv_desc =
	{ &unionfs_vnodeop_p, unionfs_vnodeop_entries };

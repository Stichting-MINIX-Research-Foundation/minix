/*-
 * Copyright (c) 1994 Jan-Simon Pendry
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2005, 2006 Masanori Ozawa <ozawa@ongs.co.jp>, ONGS Inc.
 * Copyright (c) 2006 Daichi Goto <daichi@freebsd.org>
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
 *	@(#)union_subr.c	8.20 (Berkeley) 5/20/95
 * $FreeBSD: src/sys/fs/unionfs/union_subr.c,v 1.99 2008/01/24 12:34:27 attilio Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/filedesc.h>
#include <sys/stat.h>
#include <sys/kauth.h>
#include <sys/resourcevar.h>

#include <fs/unionfs/unionfs.h>

MALLOC_DEFINE(M_UNIONFSPATH, "UNIONFS path", "UNIONFS path private part");

/*
 * Make a new or get existing unionfs node.
 * 
 * uppervp and lowervp should be unlocked. Because if new unionfs vnode is
 * locked, uppervp or lowervp is locked too. In order to prevent dead lock,
 * you should not lock plurality simultaneously.
 */
int
unionfs_nodeget(struct mount *mp, struct vnode *uppervp,
		struct vnode *lowervp, struct vnode *dvp,
		struct vnode **vpp, struct componentname *cnp)
{
	struct unionfs_mount *ump;
	struct unionfs_node *unp;
	struct vnode   *vp;
	int		error;
	const char	*path;

	ump = MOUNTTOUNIONFSMOUNT(mp);
	path = (cnp ? cnp->cn_nameptr : NULL);

	if (uppervp == NULLVP && lowervp == NULLVP)
		panic("unionfs_nodeget: upper and lower is null");

	/* If it has no ISLASTCN flag, path check is skipped. */
	if (cnp && !(cnp->cn_flags & ISLASTCN))
		path = NULL;

	if ((uppervp == NULLVP || ump->um_uppervp != uppervp) ||
	    (lowervp == NULLVP || ump->um_lowervp != lowervp)) {
		if (dvp == NULLVP)
			return (EINVAL);
	}

	/*
	 * Get a new vnode and share the lock with upper layer vnode,
	 * unless layers are inverted.
	 */
	vnode_t *svp = (uppervp != NULLVP) ? uppervp : lowervp;
	error = getnewvnode(VT_UNION, mp, unionfs_vnodeop_p,
	    svp->v_interlock, &vp);
	if (error != 0) {
		return (error);
	}
	if (dvp != NULLVP)
		vref(dvp);
	if (uppervp != NULLVP)
		vref(uppervp);
	if (lowervp != NULLVP)
		vref(lowervp);

	unp = kmem_zalloc(sizeof(*unp), KM_SLEEP);
	unp->un_vnode = vp;
	unp->un_uppervp = uppervp;
	unp->un_lowervp = lowervp;
	unp->un_dvp = dvp;

	if (path != NULL) {
		unp->un_path = (char *)
		    malloc(cnp->cn_namelen +1, M_UNIONFSPATH, M_WAITOK|M_ZERO);
		memcpy(unp->un_path, cnp->cn_nameptr, cnp->cn_namelen);
		unp->un_path[cnp->cn_namelen] = '\0';
	}
	vp->v_type = (uppervp != NULLVP ? uppervp->v_type : lowervp->v_type);
	vp->v_data = unp;
	uvm_vnp_setsize(vp, 0);

	if ((uppervp != NULLVP && ump->um_uppervp == uppervp) &&
	    (lowervp != NULLVP && ump->um_lowervp == lowervp))
		vp->v_vflag |= VV_ROOT;

	*vpp = vp;

	return (0);
}

/*
 * Clean up the unionfs node.
 */
void
unionfs_noderem(struct vnode *vp)
{
	struct unionfs_mount *ump;
	struct unionfs_node *unp;
	struct unionfs_node_status *unsp;
	struct vnode   *lvp;
	struct vnode   *uvp;

	ump = MOUNTTOUNIONFSMOUNT(vp->v_mount);

	/*
	 * Use the interlock to protect the clearing of v_data to
	 * prevent faults in unionfs_lock().
	 */
	unp = VTOUNIONFS(vp);
	lvp = unp->un_lowervp;
	uvp = unp->un_uppervp;
	unp->un_lowervp = unp->un_uppervp = NULLVP;
	vp->v_data = NULL;
	
	if (lvp != NULLVP)
		vrele(lvp);
	if (uvp != NULLVP)
		vrele(uvp);
	if (unp->un_dvp != NULLVP) {
		vrele(unp->un_dvp);
		unp->un_dvp = NULLVP;
	}
	if (unp->un_path) {
		free(unp->un_path, M_UNIONFSPATH);
		unp->un_path = NULL;
	}

	while ((unsp = LIST_FIRST(&(unp->un_unshead))) != NULL) {
		LIST_REMOVE(unsp, uns_list);
		free(unsp, M_TEMP);
	}
	kmem_free(unp, sizeof(*unp));
}

/*
 * Get the unionfs node status.
 * You need exclusive lock this vnode.
 */
void
unionfs_get_node_status(struct unionfs_node *unp,
			struct unionfs_node_status **unspp)
{
	struct unionfs_node_status *unsp;
	pid_t pid;
	lwpid_t lid;

	KASSERT(NULL != unspp);
	KASSERT(VOP_ISLOCKED(UNIONFSTOV(unp)) == LK_EXCLUSIVE);

	pid = curproc->p_pid;
	lid = curlwp->l_lid;

	LIST_FOREACH(unsp, &(unp->un_unshead), uns_list) {
		if (unsp->uns_pid == pid && unsp->uns_lid == lid) {
			*unspp = unsp;
			return;
		}
	}

	/* create a new unionfs node status */
	unsp = kmem_zalloc(sizeof(*unsp), KM_SLEEP);
	unsp->uns_pid = pid;
	unsp->uns_lid = lid;
	LIST_INSERT_HEAD(&(unp->un_unshead), unsp, uns_list);

	*unspp = unsp;
}

/*
 * Remove the unionfs node status, if you can.
 * You need exclusive lock this vnode.
 */
void
unionfs_tryrem_node_status(struct unionfs_node *unp,
			   struct unionfs_node_status *unsp)
{
	KASSERT(NULL != unsp);
	KASSERT(VOP_ISLOCKED(UNIONFSTOV(unp)) == LK_EXCLUSIVE);

	if (0 < unsp->uns_lower_opencnt || 0 < unsp->uns_upper_opencnt)
		return;

	LIST_REMOVE(unsp, uns_list);
	kmem_free(unsp, sizeof(*unsp));
}

/*
 * Create upper node attr.
 */
void
unionfs_create_uppervattr_core(struct unionfs_mount *ump,
			       struct vattr *lva,
			       struct vattr *uva)
{
	vattr_null(uva);
	uva->va_type = lva->va_type;
	uva->va_atime = lva->va_atime;
	uva->va_mtime = lva->va_mtime;
	uva->va_ctime = lva->va_ctime;

	switch (ump->um_copymode) {
	case UNIONFS_TRANSPARENT:
		uva->va_mode = lva->va_mode;
		uva->va_uid = lva->va_uid;
		uva->va_gid = lva->va_gid;
		break;
	case UNIONFS_MASQUERADE:
		if (ump->um_uid == lva->va_uid) {
			uva->va_mode = lva->va_mode & 077077;
			uva->va_mode |= (lva->va_type == VDIR ? ump->um_udir : ump->um_ufile) & 0700;
			uva->va_uid = lva->va_uid;
			uva->va_gid = lva->va_gid;
		} else {
			uva->va_mode = (lva->va_type == VDIR ? ump->um_udir : ump->um_ufile);
			uva->va_uid = ump->um_uid;
			uva->va_gid = ump->um_gid;
		}
		break;
	default:		/* UNIONFS_TRADITIONAL */
		uva->va_mode = 0777 & ~curproc->p_cwdi->cwdi_cmask;
		uva->va_uid = ump->um_uid;
		uva->va_gid = ump->um_gid;
		break;
	}
}

/*
 * Create upper node attr.
 */
int
unionfs_create_uppervattr(struct unionfs_mount *ump,
			  struct vnode *lvp,
			  struct vattr *uva,
			  kauth_cred_t cred)
{
	int		error;
	struct vattr	lva;

	if ((error = VOP_GETATTR(lvp, &lva, cred)))
		return (error);

	unionfs_create_uppervattr_core(ump, &lva, uva);

	return (error);
}

/*
 * relookup
 * 
 * dvp should be locked on entry and will be locked on return.
 * 
 * If an error is returned, *vpp will be invalid, otherwise it will hold a
 * locked, referenced vnode. If *vpp == dvp then remember that only one
 * LK_EXCLUSIVE lock is held.
 */
static int
unionfs_relookup(struct vnode *dvp, struct vnode **vpp,
		 struct componentname *cnp, struct componentname *cn,
		 char **pnbuf_ret,
		 const char *path, int pathlen, u_long nameiop)
{
	int	error;
	char *pnbuf;

	cn->cn_namelen = pathlen;
	pnbuf = PNBUF_GET();
	memcpy(pnbuf, path, pathlen);
	pnbuf[pathlen] = '\0';

	cn->cn_nameiop = nameiop;
	cn->cn_flags = (LOCKPARENT | LOCKLEAF | ISLASTCN);
	cn->cn_cred = cnp->cn_cred;

	cn->cn_nameptr = pnbuf;
	cn->cn_consume = cnp->cn_consume;

	if (nameiop == DELETE)
		cn->cn_flags |= (cnp->cn_flags & DOWHITEOUT);

	vref(dvp);
	VOP_UNLOCK(dvp);

	if ((error = relookup(dvp, vpp, cn, 0))) {
		PNBUF_PUT(pnbuf);
		*pnbuf_ret = NULL;
		vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY);
	} else {
		*pnbuf_ret = pnbuf;
		vrele(dvp);
	}

	return (error);
}

/*
 * relookup for CREATE namei operation.
 *
 * dvp is unionfs vnode. dvp should be locked.
 *
 * If it called 'unionfs_copyfile' function by unionfs_link etc,
 * VOP_LOOKUP information is broken.
 * So it need relookup in order to create link etc.
 */
int
unionfs_relookup_for_create(struct vnode *dvp, struct componentname *cnp)
{
	int	error;
	struct vnode *udvp;
	struct vnode *vp;
	struct componentname cn;
	char *pnbuf;

	udvp = UNIONFSVPTOUPPERVP(dvp);
	vp = NULLVP;

	error = unionfs_relookup(udvp, &vp, cnp, &cn, &pnbuf,
	    cnp->cn_nameptr,
	    strlen(cnp->cn_nameptr), CREATE);
	if (error)
		return (error);

	if (vp != NULLVP) {
		if (udvp == vp)
			vrele(vp);
		else
			vput(vp);

		error = EEXIST;
	}

	PNBUF_PUT(pnbuf);

	if (!error) {
		cnp->cn_flags = cn.cn_flags;
	}

	return (error);
}

/*
 * relookup for DELETE namei operation.
 *
 * dvp is unionfs vnode. dvp should be locked.
 */
int
unionfs_relookup_for_delete(struct vnode *dvp, struct componentname *cnp)
{
	int	error;
	struct vnode *udvp;
	struct vnode *vp;
	struct componentname cn;
	char *pnbuf;

	udvp = UNIONFSVPTOUPPERVP(dvp);
	vp = NULLVP;

	error = unionfs_relookup(udvp, &vp, cnp, &cn, &pnbuf, cnp->cn_nameptr,
	    strlen(cnp->cn_nameptr), DELETE);
	if (error)
		return (error);

	if (vp == NULLVP)
		error = ENOENT;
	else {
		if (udvp == vp)
			vrele(vp);
		else
			vput(vp);
	}

	PNBUF_PUT(pnbuf);

	if (!error) {
		cnp->cn_flags = cn.cn_flags;
	}

	return (error);
}

/*
 * relookup for RENAME namei operation.
 *
 * dvp is unionfs vnode. dvp should be locked.
 */
int
unionfs_relookup_for_rename(struct vnode *dvp, struct componentname *cnp)
{
	int error;
	struct vnode *udvp;
	struct vnode *vp;
	struct componentname cn;
	char *pnbuf;

	udvp = UNIONFSVPTOUPPERVP(dvp);
	vp = NULLVP;

	error = unionfs_relookup(udvp, &vp, cnp, &cn, &pnbuf, cnp->cn_nameptr,
	    strlen(cnp->cn_nameptr), RENAME);
	if (error)
		return (error);

	if (vp != NULLVP) {
		if (udvp == vp)
			vrele(vp);
		else
			vput(vp);
	}

	PNBUF_PUT(pnbuf);

	if (!error) {
		cnp->cn_flags = cn.cn_flags;
	}

	return (error);

}

/*
 * Update the unionfs_node.
 * 
 * uvp is new locked upper vnode. unionfs vnode's lock will be exchanged to the
 * uvp's lock and lower's lock will be unlocked.
 */
static void
unionfs_node_update(struct unionfs_node *unp, struct vnode *uvp)
{
	struct vnode   *vp;
	struct vnode   *lvp;

	vp = UNIONFSTOV(unp);
	lvp = unp->un_lowervp;

	/*
	 * lock update
	 */
	mutex_enter(vp->v_interlock);
	unp->un_uppervp = uvp;
	KASSERT(VOP_ISLOCKED(lvp) == LK_EXCLUSIVE);
	mutex_exit(vp->v_interlock);
}

/*
 * Create a new shadow dir.
 * 
 * udvp should be locked on entry and will be locked on return.
 * 
 * If no error returned, unp will be updated.
 */
int
unionfs_mkshadowdir(struct unionfs_mount *ump, struct vnode *udvp,
		    struct unionfs_node *unp, struct componentname *cnp)
{
	int		error;
	struct vnode   *lvp;
	struct vnode   *uvp;
	struct vattr	va;
	struct vattr	lva;
	struct componentname cn;
	char *pnbuf;

	if (unp->un_uppervp != NULLVP)
		return (EEXIST);

	lvp = unp->un_lowervp;
	uvp = NULLVP;

	memset(&cn, 0, sizeof(cn));

	if ((error = VOP_GETATTR(lvp, &lva, cnp->cn_cred)))
		goto unionfs_mkshadowdir_abort;

	if ((error = unionfs_relookup(udvp, &uvp, cnp, &cn, &pnbuf,
			cnp->cn_nameptr, cnp->cn_namelen, CREATE)))
		goto unionfs_mkshadowdir_abort;
	if (uvp != NULLVP) {
		if (udvp == uvp)
			vrele(uvp);
		else
			vput(uvp);

		error = EEXIST;
		goto unionfs_mkshadowdir_free_out;
	}

	unionfs_create_uppervattr_core(ump, &lva, &va);

	error = VOP_MKDIR(udvp, &uvp, &cn, &va);

	if (!error) {
		unionfs_node_update(unp, uvp);

		/*
		 * XXX The bug which cannot set uid/gid was corrected.
		 * Ignore errors.   XXXNETBSD Why is this done as root?
		 */
		va.va_type = VNON;
		VOP_SETATTR(uvp, &va, lwp0.l_cred);
	}

unionfs_mkshadowdir_free_out:
	PNBUF_PUT(pnbuf);

unionfs_mkshadowdir_abort:

	return (error);
}

/*
 * Create a new whiteout.
 * 
 * dvp should be locked on entry and will be locked on return.
 */
int
unionfs_mkwhiteout(struct vnode *dvp, struct componentname *cnp, const char *path)
{
	int		error;
	struct vnode   *wvp;
	struct componentname cn;
	char *pnbuf;

	if (path == NULL)
		path = cnp->cn_nameptr;

	wvp = NULLVP;
	if ((error = unionfs_relookup(dvp, &wvp, cnp, &cn, &pnbuf,
			path, strlen(path), CREATE)))
		return (error);
	if (wvp != NULLVP) {
		PNBUF_PUT(pnbuf);
		if (dvp == wvp)
			vrele(wvp);
		else
			vput(wvp);

		return (EEXIST);
	}

	PNBUF_PUT(pnbuf);

	return (error);
}

/*
 * Create a new vnode for create a new shadow file.
 * 
 * If an error is returned, *vpp will be invalid, otherwise it will hold a
 * locked, referenced and opened vnode.
 * 
 * unp is never updated.
 */
static int
unionfs_vn_create_on_upper(struct vnode **vpp, struct vnode *udvp,
			   struct unionfs_node *unp, struct vattr *uvap)
{
	struct unionfs_mount *ump;
	struct vnode   *vp;
	struct vnode   *lvp;
	kauth_cred_t    cred;
	struct vattr	lva;
	int		fmode;
	int		error;
	struct componentname cn;
	char *pnbuf;

	ump = MOUNTTOUNIONFSMOUNT(UNIONFSTOV(unp)->v_mount);
	vp = NULLVP;
	lvp = unp->un_lowervp;
	cred = kauth_cred_get();
	fmode = FFLAGS(O_WRONLY | O_CREAT | O_TRUNC | O_EXCL);
	error = 0;

	if ((error = VOP_GETATTR(lvp, &lva, cred)) != 0)
		return (error);
	unionfs_create_uppervattr_core(ump, &lva, uvap);

	if (unp->un_path == NULL)
		panic("unionfs: un_path is null");

	cn.cn_namelen = strlen(unp->un_path);
	pnbuf = PNBUF_GET();
	memcpy(pnbuf, unp->un_path, cn.cn_namelen + 1);
	cn.cn_nameiop = CREATE;
	cn.cn_flags = (LOCKPARENT | LOCKLEAF | ISLASTCN);
	cn.cn_cred = cred;
	cn.cn_nameptr = pnbuf;
	cn.cn_consume = 0;

	vref(udvp);
	if ((error = relookup(udvp, &vp, &cn, 0)) != 0)
		goto unionfs_vn_create_on_upper_free_out2;
	vrele(udvp);

	if (vp != NULLVP) {
		if (vp == udvp)
			vrele(vp);
		else
			vput(vp);
		error = EEXIST;
		goto unionfs_vn_create_on_upper_free_out1;
	}

	if ((error = VOP_CREATE(udvp, &vp, &cn, uvap)) != 0)
		goto unionfs_vn_create_on_upper_free_out1;

	if ((error = VOP_OPEN(vp, fmode, cred)) != 0) {
		vput(vp);
		goto unionfs_vn_create_on_upper_free_out1;
	}
	vp->v_writecount++;
	*vpp = vp;

unionfs_vn_create_on_upper_free_out1:
	VOP_UNLOCK(udvp);

unionfs_vn_create_on_upper_free_out2:
	PNBUF_PUT(pnbuf);

	return (error);
}

/*
 * Copy from lvp to uvp.
 * 
 * lvp and uvp should be locked and opened on entry and will be locked and
 * opened on return.
 */
static int
unionfs_copyfile_core(struct vnode *lvp, struct vnode *uvp,
		      kauth_cred_t cred)
{
	int		error;
	off_t		offset;
	int		count;
	int		bufoffset;
	char           *buf;
	struct uio	uio;
	struct iovec	iov;

	error = 0;
	memset(&uio, 0, sizeof(uio));
	UIO_SETUP_SYSSPACE(&uio);
	uio.uio_offset = 0;

	buf = kmem_alloc(MAXBSIZE, KM_SLEEP);
	if (buf == NULL)
		return ENOMEM;

	while (error == 0) {
		offset = uio.uio_offset;

		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		iov.iov_base = buf;
		iov.iov_len = MAXBSIZE;
		uio.uio_resid = iov.iov_len;
		uio.uio_rw = UIO_READ;

		if ((error = VOP_READ(lvp, &uio, 0, cred)) != 0)
			break;
		if ((count = MAXBSIZE - uio.uio_resid) == 0)
			break;

		bufoffset = 0;
		while (bufoffset < count) {
			uio.uio_iov = &iov;
			uio.uio_iovcnt = 1;
			iov.iov_base = buf + bufoffset;
			iov.iov_len = count - bufoffset;
			uio.uio_offset = offset + bufoffset;
			uio.uio_resid = iov.iov_len;
			uio.uio_rw = UIO_WRITE;

			if ((error = VOP_WRITE(uvp, &uio, 0, cred)) != 0)
				break;

			bufoffset += (count - bufoffset) - uio.uio_resid;
		}

		uio.uio_offset = offset + bufoffset;
	}

	kmem_free(buf, MAXBSIZE);

	return (error);
}

/*
 * Copy file from lower to upper.
 * 
 * If you need copy of the contents, set 1 to docopy. Otherwise, set 0 to
 * docopy.
 * 
 * If no error returned, unp will be updated.
 */
int
unionfs_copyfile(struct unionfs_node *unp, int docopy, kauth_cred_t cred)
{
	int		error;
	struct vnode   *udvp;
	struct vnode   *lvp;
	struct vnode   *uvp;
	struct vattr	uva;

	lvp = unp->un_lowervp;
	uvp = NULLVP;

	if ((UNIONFSTOV(unp)->v_mount->mnt_flag & MNT_RDONLY))
		return (EROFS);
	if (unp->un_dvp == NULLVP)
		return (EINVAL);
	if (unp->un_uppervp != NULLVP)
		return (EEXIST);
	udvp = VTOUNIONFS(unp->un_dvp)->un_uppervp;
	if (udvp == NULLVP)
		return (EROFS);
	if ((udvp->v_mount->mnt_flag & MNT_RDONLY))
		return (EROFS);

	error = VOP_ACCESS(lvp, VREAD, cred);
	if (error != 0)
		return (error);

	error = unionfs_vn_create_on_upper(&uvp, udvp, unp, &uva);
	if (error != 0)
		return (error);

	if (docopy != 0) {
		error = VOP_OPEN(lvp, FREAD, cred);
		if (error == 0) {
			error = unionfs_copyfile_core(lvp, uvp, cred);
			VOP_CLOSE(lvp, FREAD, cred);
		}
	}
	VOP_CLOSE(uvp, FWRITE, cred);
	uvp->v_writecount--;

	if (error == 0) {
		/* Reset the attributes. Ignore errors. */
		uva.va_type = VNON;
		VOP_SETATTR(uvp, &uva, cred);
	}

	unionfs_node_update(unp, uvp);

	return (error);
}

/*
 * It checks whether vp can rmdir. (check empty)
 *
 * vp is unionfs vnode.
 * vp should be locked.
 */
int
unionfs_check_rmdir(struct vnode *vp, kauth_cred_t cred)
{
	int		error;
	int		eofflag;
	int		lookuperr;
	struct vnode   *uvp;
	struct vnode   *lvp;
	struct vnode   *tvp;
	struct vattr	va;
	struct componentname cn;
	/*
	 * The size of buf needs to be larger than DIRBLKSIZ.
	 */
	char		buf[256 * 6];
	struct dirent  *dp;
	struct dirent  *edp;
	struct uio	uio;
	struct iovec	iov;

	KASSERT(VOP_ISLOCKED(vp) == LK_EXCLUSIVE);

	eofflag = 0;
	uvp = UNIONFSVPTOUPPERVP(vp);
	lvp = UNIONFSVPTOLOWERVP(vp);

	/* check opaque */
	if ((error = VOP_GETATTR(uvp, &va, cred)) != 0)
		return (error);
	if (va.va_flags & OPAQUE)
		return (0);

	/* open vnode */
	if ((error = VOP_ACCESS(vp, VEXEC|VREAD, cred)) != 0)
		return (error);
	if ((error = VOP_OPEN(vp, FREAD, cred)) != 0)
		return (error);

	UIO_SETUP_SYSSPACE(&uio);
	uio.uio_rw = UIO_READ;
	uio.uio_offset = 0;

	while (!error && !eofflag) {
		iov.iov_base = buf;
		iov.iov_len = sizeof(buf);
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_resid = iov.iov_len;

		error = VOP_READDIR(lvp, &uio, cred, &eofflag, NULL, NULL);
		if (error)
			break;

		edp = (struct dirent*)&buf[sizeof(buf) - uio.uio_resid];
		for (dp = (struct dirent*)buf; !error && dp < edp;
		     dp = (struct dirent*)((char *)dp + dp->d_reclen)) {
			if (dp->d_type == DT_WHT ||
			    (dp->d_namlen == 1 && dp->d_name[0] == '.') ||
			    (dp->d_namlen == 2 && !memcmp(dp->d_name, "..", 2)))
				continue;

			cn.cn_namelen = dp->d_namlen;
			cn.cn_nameptr = dp->d_name;
			cn.cn_nameiop = LOOKUP;
			cn.cn_flags = (LOCKPARENT | LOCKLEAF | RDONLY | ISLASTCN);
			cn.cn_cred = cred;
			cn.cn_consume = 0;

			/*
			 * check entry in lower.
			 * Sometimes, readdir function returns
			 * wrong entry.
			 */
			lookuperr = VOP_LOOKUP(lvp, &tvp, &cn);

			if (!lookuperr)
				vput(tvp);
			else
				continue; /* skip entry */

			/*
			 * check entry
			 * If it has no exist/whiteout entry in upper,
			 * directory is not empty.
			 */
			cn.cn_flags = (LOCKPARENT | LOCKLEAF | RDONLY | ISLASTCN);
			lookuperr = VOP_LOOKUP(uvp, &tvp, &cn);

			if (!lookuperr)
				vput(tvp);

			/* ignore exist or whiteout entry */
			if (!lookuperr ||
			    (lookuperr == ENOENT && (cn.cn_flags & ISWHITEOUT)))
				continue;

			error = ENOTEMPTY;
		}
	}

	/* close vnode */
	VOP_CLOSE(vp, FREAD, cred);

	return (error);
}

#ifdef DIAGNOSTIC

struct vnode   *
unionfs_checkuppervp(struct vnode *vp, const char *fil, int lno)
{
	struct unionfs_node *unp;

	unp = VTOUNIONFS(vp);

#ifdef notyet
	if (vp->v_op != unionfs_vnodeop_p) {
		printf("unionfs_checkuppervp: on non-unionfs-node.\n");
#ifdef KDB
		kdb_enter(KDB_WHY_UNIONFS,
		    "unionfs_checkuppervp: on non-unionfs-node.\n");
#endif
		panic("unionfs_checkuppervp");
	};
#endif
	return (unp->un_uppervp);
}

struct vnode   *
unionfs_checklowervp(struct vnode *vp, const char *fil, int lno)
{
	struct unionfs_node *unp;

	unp = VTOUNIONFS(vp);

#ifdef notyet
	if (vp->v_op != unionfs_vnodeop_p) {
		printf("unionfs_checklowervp: on non-unionfs-node.\n");
#ifdef KDB
		kdb_enter(KDB_WHY_UNIONFS,
		    "unionfs_checklowervp: on non-unionfs-node.\n");
#endif
		panic("unionfs_checklowervp");
	};
#endif
	return (unp->un_lowervp);
}
#endif

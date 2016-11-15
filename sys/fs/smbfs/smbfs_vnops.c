/*	$NetBSD: smbfs_vnops.c,v 1.93 2014/12/21 10:48:53 hannken Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jaromir Dolecek.
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
 * Copyright (c) 2000-2001 Boris Popov
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * FreeBSD: src/sys/fs/smbfs/smbfs_vnops.c,v 1.15 2001/12/20 15:56:45 bp Exp
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: smbfs_vnops.c,v 1.93 2014/12/21 10:48:53 hannken Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/lockf.h>
#include <sys/kauth.h>
#include <sys/mallocvar.h>

#include <machine/limits.h>

#include <uvm/uvm.h>
#include <uvm/uvm_extern.h>

#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>

#include <fs/smbfs/smbfs.h>
#include <fs/smbfs/smbfs_node.h>
#include <fs/smbfs/smbfs_subr.h>

#include <miscfs/genfs/genfs.h>

/*
 * Prototypes for SMBFS vnode operations
 */
int smbfs_create(void *);
int smbfs_open(void *);
int smbfs_close(void *);
int smbfs_access(void *);
int smbfs_getattr(void *);
int smbfs_setattr(void *);
int smbfs_read(void *);
int smbfs_write(void *);
int smbfs_fsync(void *);
int smbfs_remove(void *);
int smbfs_link(void *);
int smbfs_lookup(void *);
int smbfs_rename(void *);
int smbfs_mkdir(void *);
int smbfs_rmdir(void *);
int smbfs_symlink(void *);
int smbfs_readdir(void *);
int smbfs_strategy(void *);
int smbfs_print(void *);
int smbfs_pathconf(void *ap);
int smbfs_advlock(void *);

int (**smbfs_vnodeop_p)(void *);
static struct vnodeopv_entry_desc smbfs_vnodeop_entries[] = {
	{ &vop_default_desc,		vn_default_error },
	{ &vop_access_desc,		smbfs_access },
	{ &vop_advlock_desc,		smbfs_advlock },
	{ &vop_close_desc,		smbfs_close },
	{ &vop_create_desc,		smbfs_create },
	{ &vop_fallocate_desc,		genfs_eopnotsupp },
	{ &vop_fdiscard_desc,		genfs_eopnotsupp },
	{ &vop_fsync_desc,		smbfs_fsync },
	{ &vop_getattr_desc,		smbfs_getattr },
	{ &vop_getpages_desc,		genfs_compat_getpages },
	{ &vop_inactive_desc,		smbfs_inactive },
	{ &vop_ioctl_desc,		genfs_enoioctl },
	{ &vop_islocked_desc,		genfs_islocked },
	{ &vop_link_desc,		smbfs_link },
	{ &vop_lock_desc,		genfs_lock },
	{ &vop_lookup_desc,		smbfs_lookup },
	{ &vop_mkdir_desc,		smbfs_mkdir },
	{ &vop_mknod_desc,		genfs_eopnotsupp },
	{ &vop_open_desc,		smbfs_open },
	{ &vop_pathconf_desc,		smbfs_pathconf },
	{ &vop_print_desc,		smbfs_print },
	{ &vop_putpages_desc,		genfs_putpages },
	{ &vop_read_desc,		smbfs_read },
	{ &vop_readdir_desc,		smbfs_readdir },
	{ &vop_reclaim_desc,		smbfs_reclaim },
	{ &vop_remove_desc,		smbfs_remove },
	{ &vop_rename_desc,		smbfs_rename },
	{ &vop_rmdir_desc,		smbfs_rmdir },
	{ &vop_setattr_desc,		smbfs_setattr },
	{ &vop_strategy_desc,		smbfs_strategy },
	{ &vop_symlink_desc,		smbfs_symlink },
	{ &vop_unlock_desc,		genfs_unlock },
	{ &vop_write_desc,		smbfs_write },
	{ &vop_mmap_desc,		genfs_mmap },		/* mmap */
	{ &vop_seek_desc,		genfs_seek },		/* seek */
	{ &vop_kqfilter_desc,		smbfs_kqfilter},	/* kqfilter */
	{ NULL, NULL }
};
const struct vnodeopv_desc smbfs_vnodeop_opv_desc =
	{ &smbfs_vnodeop_p, smbfs_vnodeop_entries };

static int
smbfs_check_possible(struct vnode *vp, struct smbnode *np, mode_t mode)
{

        /*
         * Disallow write attempts on read-only file systems;
         * unless the file is a socket, fifo, or a block or
         * character device resident on the file system.
         */
	if (mode & VWRITE) {
		switch (vp->v_type) {
		case VREG:
		case VDIR:
		case VLNK:
 			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return EROFS;
		default:
			break;
		}
	}

	return 0;
}

static int
smbfs_check_permitted(struct vnode *vp, struct smbnode *np, mode_t mode,
    kauth_cred_t cred)
{
	struct smbmount *smp = VTOSMBFS(vp);
	mode_t file_mode = (vp->v_type == VDIR) ? smp->sm_args.dir_mode :
	    smp->sm_args.file_mode;

	return kauth_authorize_vnode(cred, KAUTH_ACCESS_ACTION(mode,
	    vp->v_type, file_mode), vp, NULL, genfs_can_access(vp->v_type,
	    file_mode, smp->sm_args.uid, smp->sm_args.gid, mode, cred));
}

int
smbfs_access(void *v)
{
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct smbnode *np = VTOSMB(vp);
	u_int acc_mode = ap->a_mode;
	int error;
#ifdef SMB_VNODE_DEBUG
	struct smbmount *smp = VTOSMBFS(vp);
#endif

        SMBVDEBUG("file '%.*s', node mode=%o, acc mode=%x\n",
	    (int) np->n_nmlen, np->n_name,
	    (vp->v_type == VDIR) ? smp->sm_args.dir_mode : smp->sm_args.file_mode,
	    acc_mode);

	error = smbfs_check_possible(vp, np, acc_mode);
	if (error)
		return error;

	error = smbfs_check_permitted(vp, np, acc_mode, ap->a_cred);

	return error;
}

/* ARGSUSED */
int
smbfs_open(void *v)
{
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct lwp *l = curlwp;
	struct vnode *vp = ap->a_vp;
	struct smbnode *np = VTOSMB(vp);
	struct smb_cred scred;
	struct vattr vattr;
	u_int32_t sv_caps = SMB_CAPS(SSTOVC(np->n_mount->sm_share));
	int error, accmode;

	SMBVDEBUG("%.*s,%d\n", (int) np->n_nmlen, np->n_name,
	    (np->n_flag & NOPEN) != 0);
	if (vp->v_type != VREG && vp->v_type != VDIR) {
		SMBFSERR("open eacces vtype=%d\n", vp->v_type);
		return EACCES;
	}
	if (vp->v_type == VDIR) {
		if ((sv_caps & SMB_CAP_NT_SMBS) == 0) {
			np->n_flag |= NOPEN;
			return 0;
		}
		goto do_open;	/* skip 'modified' check */
	}

	if (np->n_flag & NMODIFIED) {
		if ((error = smbfs_vinvalbuf(vp, V_SAVE, ap->a_cred, l, 1)) == EINTR)
			return error;
		smbfs_attr_cacheremove(vp);
		error = VOP_GETATTR(vp, &vattr, ap->a_cred);
		if (error)
			return error;
		np->n_mtime.tv_sec = vattr.va_mtime.tv_sec;
	} else {
		error = VOP_GETATTR(vp, &vattr, ap->a_cred);
		if (error)
			return error;
		if (np->n_mtime.tv_sec != vattr.va_mtime.tv_sec) {
			error = smbfs_vinvalbuf(vp, V_SAVE, ap->a_cred, l, 1);
			if (error == EINTR)
				return error;
			np->n_mtime.tv_sec = vattr.va_mtime.tv_sec;
		}
	}

do_open:
	if ((np->n_flag & NOPEN) != 0)
		return 0;

	smb_makescred(&scred, l, ap->a_cred);
	if (vp->v_type == VDIR)
		error = smbfs_smb_ntcreatex(np,
		    SMB_SM_DENYNONE|SMB_AM_OPENREAD, &scred);
	else {
		/*
		 * Use DENYNONE to give unixy semantics of permitting
		 * everything not forbidden by permissions.  Ie denial
		 * is up to server with clients/openers needing to use
		 * advisory locks for further control.
		 */
		accmode = SMB_SM_DENYNONE|SMB_AM_OPENREAD;
		if ((vp->v_mount->mnt_flag & MNT_RDONLY) == 0)
			accmode = SMB_SM_DENYNONE|SMB_AM_OPENRW;
		error = smbfs_smb_open(np, accmode, &scred);
		if (error) {
			if (ap->a_mode & FWRITE)
				return EACCES;

			error = smbfs_smb_open(np,
			    SMB_SM_DENYNONE|SMB_AM_OPENREAD, &scred);
		}
	}
	if (!error)
		np->n_flag |= NOPEN;
	smbfs_attr_cacheremove(vp);
	return error;
}

/*
 * Close called.
 */
int
smbfs_close(void *v)
{
	struct vop_close_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int  a_fflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	int error;
	struct lwp *l = curlwp;
	struct vnode *vp = ap->a_vp;
	struct smbnode *np = VTOSMB(vp);

	/* Flush all file data */
	error = smbfs_vinvalbuf(vp, V_SAVE, ap->a_cred, l, 1);
	if (error)
		return (error);

	/*
	 * We must close the directory lookup context now, so that
	 * later directory changes would be properly detected.
	 * Ideally, the lookup routines should handle such case, and
	 * the context would be removed only in smbfs_inactive().
	 */
	if (vp->v_type == VDIR && (np->n_flag & NOPEN) != 0) {
		struct smb_share *ssp = np->n_mount->sm_share;
		struct smb_cred scred;

		smb_makescred(&scred, l, ap->a_cred);

		if (np->n_dirseq != NULL) {
			smbfs_findclose(np->n_dirseq, &scred);
			np->n_dirseq = NULL;
		}

		if (SMB_CAPS(SSTOVC(ssp)) & SMB_CAP_NT_SMBS)
			smbfs_smb_close(ssp, np->n_fid, &np->n_mtime, &scred);

		np->n_flag &= ~NOPEN;
		smbfs_attr_cacheremove(vp);
	}

	return (0);
}

/*
 * smbfs_getattr call from vfs.
 */
int
smbfs_getattr(void *v)
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct smbnode *np = VTOSMB(vp);
	struct vattr *va=ap->a_vap;
	struct smbfattr fattr;
	struct smb_cred scred;
	u_quad_t oldsize;
	int error;

	SMBVDEBUG("%p: '%.*s' isroot %d\n", vp,
		(int) np->n_nmlen, np->n_name, (vp->v_vflag & VV_ROOT) != 0);

	if ((error = smbfs_attr_cachelookup(vp, va)) == 0)
		return (0);

	SMBVDEBUG0("not in the cache\n");
	smb_makescred(&scred, curlwp, ap->a_cred);
	oldsize = np->n_size;
	error = smbfs_smb_lookup(np, NULL, 0, &fattr, &scred);
	if (error) {
		SMBVDEBUG("error %d\n", error);
		return error;
	}
	smbfs_attr_cacheenter(vp, &fattr);
	smbfs_attr_cachelookup(vp, va);
	if ((np->n_flag & NOPEN) != 0)
		np->n_size = oldsize;
	return 0;
}

int
smbfs_setattr(void *v)
{
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct lwp *l = curlwp;
	struct vnode *vp = ap->a_vp;
	struct smbnode *np = VTOSMB(vp);
	struct vattr *vap = ap->a_vap;
	struct timespec *mtime, *atime;
	struct smb_cred scred;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	u_quad_t tsize = 0;
	int isreadonly, doclose, error = 0;

	SMBVDEBUG0("\n");
	if (vap->va_flags != VNOVAL)
		return EOPNOTSUPP;
	isreadonly = (vp->v_mount->mnt_flag & MNT_RDONLY);
	/*
	 * Disallow write attempts if the filesystem is mounted read-only.
	 */
  	if ((vap->va_uid != (uid_t)VNOVAL || vap->va_gid != (gid_t)VNOVAL ||
	     vap->va_atime.tv_sec != VNOVAL || vap->va_mtime.tv_sec != VNOVAL ||
	     vap->va_mode != (mode_t)VNOVAL) && isreadonly)
		return EROFS;
	smb_makescred(&scred, l, ap->a_cred);
	if (vap->va_size != VNOVAL) {
 		switch (vp->v_type) {
 		case VDIR:
 			return EISDIR;
 		case VREG:
			break;
 		default:
			return EINVAL;
  		};
		if (isreadonly)
			return EROFS;
		doclose = 0;
 		tsize = np->n_size;
 		np->n_size = vap->va_size;
		uvm_vnp_setsize(vp, vap->va_size);
		if ((np->n_flag & NOPEN) == 0) {
			error = smbfs_smb_open(np,
			    SMB_SM_DENYNONE|SMB_AM_OPENRW, &scred);
			if (error == 0)
				doclose = 1;
		}
		if (error == 0)
			error = smbfs_smb_setfsize(np, vap->va_size, &scred);
		if (doclose)
			smbfs_smb_close(ssp, np->n_fid, NULL, &scred);
		if (error) {
			np->n_size = tsize;
			uvm_vnp_setsize(vp, tsize);
			return (error);
		}
  	}
	mtime = atime = NULL;
	if (vap->va_mtime.tv_sec != VNOVAL)
		mtime = &vap->va_mtime;
	if (vap->va_atime.tv_sec != VNOVAL)
		atime = &vap->va_atime;
	if (mtime != atime) {
		error = kauth_authorize_vnode(ap->a_cred,
		    KAUTH_VNODE_WRITE_TIMES, ap->a_vp, NULL,
		    genfs_can_chtimes(ap->a_vp, vap->va_vaflags,
		    VTOSMBFS(vp)->sm_args.uid, ap->a_cred));
		if (error)
			return (error);

#if 0
		if (mtime == NULL)
			mtime = &np->n_mtime;
		if (atime == NULL)
			atime = &np->n_atime;
#endif
		/*
		 * If file is opened, then we can use handle based calls.
		 * If not, use path based ones.
		 */
		if ((np->n_flag & NOPEN) == 0) {
			if (vcp->vc_flags & SMBV_WIN95) {
				error = VOP_OPEN(vp, FWRITE, ap->a_cred);
				if (!error) {
/*				error = smbfs_smb_setfattrNT(np, 0, mtime, atime, &scred);
				VOP_GETATTR(vp, &vattr, ap->a_cred);*/
				if (mtime)
					np->n_mtime = *mtime;
				VOP_CLOSE(vp, FWRITE, ap->a_cred);
				}
			} else if (SMB_CAPS(vcp) & SMB_CAP_NT_SMBS) {
				error = smbfs_smb_setpattrNT(np, 0, mtime, atime, &scred);
			} else if (SMB_DIALECT(vcp) >= SMB_DIALECT_LANMAN2_0) {
				error = smbfs_smb_setptime2(np, mtime, atime, 0, &scred);
			} else {
				error = smbfs_smb_setpattr(np, 0, mtime, &scred);
			}
		} else {
			if (SMB_CAPS(vcp) & SMB_CAP_NT_SMBS) {
				error = smbfs_smb_setfattrNT(np, 0, mtime, atime, &scred);
			} else if (SMB_DIALECT(vcp) >= SMB_DIALECT_LANMAN1_0) {
				error = smbfs_smb_setftime(np, mtime, atime, &scred);
			} else {
				/*
				 * XXX I have no idea how to handle this for core
				 * level servers. The possible solution is to
				 * update mtime after file is closed.
				 */
			}
		}
	}
	/*
	 * Invalidate attribute cache in case if server doesn't set
	 * required attributes.
	 */
	smbfs_attr_cacheremove(vp);	/* invalidate cache */
	VOP_GETATTR(vp, vap, ap->a_cred);
	np->n_mtime.tv_sec = vap->va_mtime.tv_sec;
	VN_KNOTE(vp, NOTE_ATTRIB);
	return error;
}
/*
 * smbfs_read call.
 */
int
smbfs_read(void *v)
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	if (vp->v_type != VREG && vp->v_type != VDIR)
		return EPERM;

	return smbfs_readvnode(vp, ap->a_uio, ap->a_cred);
}

int
smbfs_write(void *v)
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;

	SMBVDEBUG("%d,ofs=%lld,sz=%zu\n",vp->v_type,
	    (long long int)uio->uio_offset, uio->uio_resid);
	if (vp->v_type != VREG)
		return (EPERM);
	return smbfs_writevnode(vp, uio, ap->a_cred, ap->a_ioflag);
}
/*
 * smbfs_create call
 * Create a regular file. On entry the directory to contain the file being
 * created is locked.  We must release before we return.
 */
int
smbfs_create(void *v)
{
	struct vop_create_v3_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct vattr *vap = ap->a_vap;
	struct componentname *cnp = ap->a_cnp;
	struct smbnode *dnp = VTOSMB(dvp);
	struct smbfattr fattr;
	struct smb_cred scred;
	const char *name = cnp->cn_nameptr;
	int nmlen = cnp->cn_namelen;
	int error = EINVAL;


	if (vap->va_type != VREG)
		goto out;

	smb_makescred(&scred, curlwp, cnp->cn_cred);
	error = smbfs_smb_create(dnp, name, nmlen, &scred);
	if (error)
		goto out;

	error = smbfs_smb_lookup(dnp, name, nmlen, &fattr, &scred);
	if (error)
		goto out;
	error = smbfs_nget(VTOVFS(dvp), dvp, name, nmlen, &fattr, ap->a_vpp);
	if (error)
		goto out;

	cache_enter(dvp, *ap->a_vpp, cnp->cn_nameptr, cnp->cn_namelen,
		    cnp->cn_flags);

  out:
	VN_KNOTE(dvp, NOTE_WRITE);
	return (error);
}

int
smbfs_remove(void *v)
{
	struct vop_remove_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode * a_dvp;
		struct vnode * a_vp;
		struct componentname * a_cnp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct smbnode *np = VTOSMB(vp);
	struct smb_cred scred;
	int error;

	if (vp->v_type == VDIR || (np->n_flag & NOPEN) != 0
	    || vp->v_usecount != 1) {
		/* XXX Eventually should do something along NFS sillyrename */
		error = EPERM;
	} else {
		smb_makescred(&scred, curlwp, cnp->cn_cred);
		error = smbfs_smb_delete(np, &scred);
	}
	if (error == 0)
		np->n_flag |= NGONE;

	VN_KNOTE(ap->a_vp, NOTE_DELETE);
	VN_KNOTE(ap->a_dvp, NOTE_WRITE);
	if (dvp == vp)
		vrele(vp);
	else
		vput(vp);
	vput(dvp);
	return (error);
}

/*
 * smbfs_file rename call
 */
int
smbfs_rename(void *v)
{
	struct vop_rename_args  /* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap = v;
	struct vnode *fvp = ap->a_fvp;
	struct vnode *tvp = ap->a_tvp;
	struct vnode *fdvp = ap->a_fdvp;
	struct vnode *tdvp = ap->a_tdvp;
	struct componentname *tcnp = ap->a_tcnp;
/*	struct componentname *fcnp = ap->a_fcnp;*/
	struct smb_cred scred;
#ifdef notyet
	u_int16_t flags = 6;
#endif
	int error=0;

	/* Check for cross-device rename */
	if ((fvp->v_mount != tdvp->v_mount) ||
	    (tvp && (fvp->v_mount != tvp->v_mount))) {
		error = EXDEV;
		goto out;
	}

	if (tvp && tvp->v_usecount > 1) {
		error = EBUSY;
		goto out;
	}
#ifdef notnow
	flags = 0x10;			/* verify all writes */
	if (fvp->v_type == VDIR) {
		flags |= 2;
	} else if (fvp->v_type == VREG) {
		flags |= 1;
	} else {
		error = EINVAL;
		goto out;
	}
#endif
	smb_makescred(&scred, curlwp, tcnp->cn_cred);
	/*
	 * It seems that Samba doesn't implement SMB_COM_MOVE call...
	 */
#ifdef notnow
	if (SMB_DIALECT(SSTOCN(smp->sm_share)) >= SMB_DIALECT_LANMAN1_0) {
		error = smbfs_smb_move(VTOSMB(fvp), VTOSMB(tdvp),
		    tcnp->cn_nameptr, tcnp->cn_namelen, flags, &scred);
	} else
#endif
	{
		/*
		 * We have to do the work atomicaly
		 */
		if (tvp && tvp != fvp) {
			error = smbfs_smb_delete(VTOSMB(tvp), &scred);
			if (error)
				goto out;
			VTOSMB(tvp)->n_flag |= NGONE;
			VN_KNOTE(tdvp, NOTE_WRITE);
			VN_KNOTE(tvp, NOTE_DELETE);
			cache_purge(tvp);
		}
		error = smbfs_smb_rename(VTOSMB(fvp), VTOSMB(tdvp),
		    tcnp->cn_nameptr, tcnp->cn_namelen, &scred);
		VTOSMB(fvp)->n_flag |= NGONE;
		VN_KNOTE(fdvp, NOTE_WRITE);
		VN_KNOTE(fvp, NOTE_RENAME);
	}

	if (fvp->v_type == VDIR) {
		if (tvp != NULL && tvp->v_type == VDIR)
			cache_purge(tdvp);
		cache_purge(fdvp);
	}

	smbfs_attr_cacheremove(fdvp);
	smbfs_attr_cacheremove(tdvp);

out:

	if (tdvp == tvp)
		vrele(tdvp);
	else
		vput(tdvp);
	if (tvp)
		vput(tvp);

	vrele(fdvp);
	vrele(fvp);

	return (error);
}

/*
 * somtime it will come true...
 */
int
smbfs_link(void *v)
{
	return genfs_eopnotsupp(v);
}

/*
 * smbfs_symlink link create call.
 * Sometime it will be functional...
 */
int
smbfs_symlink(void *v)
{
	return genfs_eopnotsupp(v);
}

int
smbfs_mkdir(void *v)
{
	struct vop_mkdir_v3_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	struct vnode *dvp = ap->a_dvp;
/*	struct vattr *vap = ap->a_vap;*/
	struct vnode *vp;
	struct componentname *cnp = ap->a_cnp;
	struct smbnode *dnp = VTOSMB(dvp);
	struct smb_cred scred;
	struct smbfattr fattr;
	const char *name = cnp->cn_nameptr;
	int len = cnp->cn_namelen;
	int error;

	if ((name[0] == '.') && ((len == 1) || ((len == 2) && (name[1] == '.')))){
		error = EEXIST;
		goto out;
	}

	smb_makescred(&scred, curlwp, cnp->cn_cred);
	error = smbfs_smb_mkdir(dnp, name, len, &scred);
	if (error)
		goto out;
	error = smbfs_smb_lookup(dnp, name, len, &fattr, &scred);
	if (error)
		goto out;
	error = smbfs_nget(VTOVFS(dvp), dvp, name, len, &fattr, &vp);
	if (error)
		goto out;
	*ap->a_vpp = vp;

 out:
	VN_KNOTE(dvp, NOTE_WRITE | NOTE_LINK);

	return (error);
}

/*
 * smbfs_remove directory call
 */
int
smbfs_rmdir(void *v)
{
	struct vop_rmdir_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
/*	struct smbmount *smp = VTOSMBFS(vp);*/
	struct smbnode *dnp = VTOSMB(dvp);
	struct smbnode *np = VTOSMB(vp);
	struct smb_cred scred;
	int error;

	if (dvp == vp) {
		vrele(dvp);
		vput(dvp);
		return (EINVAL);
	}

	smb_makescred(&scred, curlwp, cnp->cn_cred);
	error = smbfs_smb_rmdir(np, &scred);
	if (error == 0)
		np->n_flag |= NGONE;
	dnp->n_flag |= NMODIFIED;
	smbfs_attr_cacheremove(dvp);
	VN_KNOTE(dvp, NOTE_WRITE | NOTE_LINK);
	VN_KNOTE(vp, NOTE_DELETE);
	cache_purge(dvp);
	cache_purge(vp);
	vput(vp);
	vput(dvp);

	return (error);
}

/*
 * smbfs_readdir call
 */
int
smbfs_readdir(void *v)
{
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		kauth_cred_t a_cred;
		int *a_eofflag;
		u_long *a_cookies;
		int a_ncookies;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	if (vp->v_type != VDIR)
		return (EPERM);
#ifdef notnow
	if (ap->a_ncookies) {
		printf("smbfs_readdir: no support for cookies now...");
		return (EOPNOTSUPP);
	}
#endif
	return (smbfs_readvnode(vp, ap->a_uio, ap->a_cred));
}

/* ARGSUSED */
int
smbfs_fsync(void *v)
{
	/*return (smb_flush(ap->a_vp, ap->a_cred, ap->a_waitfor, ap->a_l, 1));*/
    return (0);
}

int
smbfs_print(void *v)
{
	struct vop_print_args /* {
	struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct smbnode *np = VTOSMB(vp);

	printf("tag VT_SMBFS, name = %.*s, parent = %p, open = %d\n",
	    (int)np->n_nmlen, np->n_name,
	    np->n_parent ? np->n_parent : NULL,
	    (np->n_flag & NOPEN) != 0);
	return (0);
}

int
smbfs_pathconf(void *v)
{
	struct vop_pathconf_args  /* {
		struct vnode *vp;
		int a_name;
		register_t *a_retval;
	} */ *ap = v;
	register_t *retval = ap->a_retval;
	int error = 0;

	switch (ap->a_name) {
	case _PC_PIPE_BUF:
		*retval = PIPE_BUF;
		break;
	case _PC_SYNC_IO:
		*retval = 1;
		break;
	case _PC_LINK_MAX:
		*retval = 0;
		break;
	case _PC_NAME_MAX:
		*retval = ap->a_vp->v_mount->mnt_stat.f_namemax;
		break;
	case _PC_PATH_MAX:
		*retval = 800;	/* XXX: a correct one ? */
		break;
	default:
		error = EINVAL;
		break;
	}

	return (error);
}

int
smbfs_strategy(void *v)
{
	struct vop_strategy_args /* {
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap = v;
	struct buf *bp = ap->a_bp;
	kauth_cred_t cr;
	struct lwp *l;
	int error = 0;

	SMBVDEBUG0("\n");
	if ((bp->b_flags & (B_PHYS|B_ASYNC)) == (B_PHYS|B_ASYNC))
		panic("smbfs physio/async");
	if (bp->b_flags & B_ASYNC) {
		l = NULL;
		cr = NULL;
	} else {
		l = curlwp;	/* XXX */
		cr = l->l_cred;
	}

	if ((bp->b_flags & B_ASYNC) == 0)
		error = smbfs_doio(bp, cr, l);

	return (error);
}

#ifndef __NetBSD__
static char smbfs_atl[] = "rhsvda";
static int
smbfs_getextattr(struct vop_getextattr_args *ap)
/* {
        IN struct vnode *a_vp;
        IN char *a_name;
        INOUT struct uio *a_uio;
        IN kauth_cred_t a_cred;
};
*/
{
	struct vnode *vp = ap->a_vp;
	struct lwp *l = ap->a_l;
	kauth_cred_t cred = ap->a_cred;
	struct uio *uio = ap->a_uio;
	const char *name = ap->a_name;
	struct smbnode *np = VTOSMB(vp);
	struct vattr vattr;
	char buf[10];
	int i, attr, error;

	error = VOP_ACCESS(vp, VREAD, cred, td);
	if (error)
		return error;
	error = VOP_GETATTR(vp, &vattr, cred, td);
	if (error)
		return error;
	if (strcmp(name, "dosattr") == 0) {
		attr = np->n_dosattr;
		for (i = 0; i < 6; i++, attr >>= 1)
			buf[i] = (attr & 1) ? smbfs_atl[i] : '-';
		buf[i] = 0;
		error = uiomove(buf, i, uio);

	} else
		error = EINVAL;
	return error;
}
#endif /* !__NetBSD__ */

/*
 * Since we expected to support F_GETLK (and SMB protocol has no such function),
 * it is necessary to use lf_advlock(). It would be nice if this function had
 * a callback mechanism because it will help to improve a level of consistency.
 */
int
smbfs_advlock(void *v)
{
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		void *a_id;
		int  a_op;
		struct flock *a_fl;
		int  a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct smbnode *np = VTOSMB(vp);
	struct flock *fl = ap->a_fl;
	struct lwp *l = curlwp;
	struct smb_cred scred;
	u_quad_t size;
	off_t start, end, oadd;
	int error, lkop;

	if (vp->v_type == VDIR) {
		/*
		 * SMB protocol have no support for directory locking.
		 * Although locks can be processed on local machine, I don't
		 * think that this is a good idea, because some programs
		 * can work wrong assuming directory is locked. So, we just
		 * return 'operation not supported'.
		 */
		 return EOPNOTSUPP;
	}
	size = np->n_size;
	switch (fl->l_whence) {

	case SEEK_SET:
	case SEEK_CUR:
		start = fl->l_start;
		break;

	case SEEK_END:
#ifndef __NetBSD__
		if (size > OFF_MAX ||
		    (fl->l_start > 0 && size > OFF_MAX - fl->l_start))
			return EOVERFLOW;
#endif
		start = size + fl->l_start;
		break;

	default:
		return EINVAL;
	}
	if (start < 0)
		return EINVAL;
	if (fl->l_len < 0) {
		if (start == 0)
			return EINVAL;
		end = start - 1;
		start += fl->l_len;
		if (start < 0)
			return EINVAL;
	} else if (fl->l_len == 0)
		end = -1;
	else {
		oadd = fl->l_len - 1;
#ifndef __NetBSD__
		if (oadd > OFF_MAX - start)
			return EOVERFLOW;
#endif
		end = start + oadd;
	}
	smb_makescred(&scred, l, l ? l->l_cred : NULL);
	switch (ap->a_op) {
	case F_SETLK:
		switch (fl->l_type) {
		case F_WRLCK:
			lkop = SMB_LOCK_EXCL;
			break;
		case F_RDLCK:
			lkop = SMB_LOCK_SHARED;
			break;
		case F_UNLCK:
			lkop = SMB_LOCK_RELEASE;
			break;
		default:
			return EINVAL;
		}
		error = lf_advlock(ap, &np->n_lockf, size);
		if (error)
			break;
		/*
		 * The ID we use for smb_lock is passed as PID to SMB
		 * server. It MUST agree with PID as setup in basic
		 * SMB header in later write requests, otherwise SMB server
		 * returns EDEADLK. See also smb_rq_new() on SMB header setup.
		 */
		error = smbfs_smb_lock(np, lkop,(void *)1, start, end, &scred);
		if (error) {
			ap->a_op = F_UNLCK;
			lf_advlock(ap, &np->n_lockf, size);
		}
		break;
	case F_UNLCK:
		lf_advlock(ap, &np->n_lockf, size);
		error = smbfs_smb_lock(np, SMB_LOCK_RELEASE,  ap->a_id, start, end, &scred);
		break;
	case F_GETLK:
		error = lf_advlock(ap, &np->n_lockf, size);
		break;
	default:
		return EINVAL;
	}

	return error;
}

static int
smbfs_pathcheck(struct smbmount *smp, const char *name, int nmlen)
{
	static const char * const badchars = "*/\\:<>;?";
	static const char * const badchars83 = " +|,[]=";
	const char *cp;
	int i;

	if (SMB_DIALECT(SSTOVC(smp->sm_share)) < SMB_DIALECT_LANMAN2_0) {
		/*
		 * Name should conform 8.3 format
		 */
		if (nmlen > 12)
			return (ENAMETOOLONG);

		if ((cp = memchr(name, '.', nmlen)) == NULL
		    || cp == name || (cp - name) > 8
		    || (cp = memchr(cp + 1, '.', nmlen - (cp - name))) != NULL)
			goto bad;

		for (cp = name, i = 0; i < nmlen; i++, cp++)
			if (strchr(badchars83, *cp) != NULL)
				goto bad;
	}

	for (cp = name, i = 0; i < nmlen; i++, cp++)
		if (strchr(badchars, *cp) != NULL)
			goto bad;

	/* name is fine */
	return (0);

 bad:
	return (ENOENT);
}

/*
 * Things go even weird without fixed inode numbers...
 */
int
smbfs_lookup(void *v)
{
	struct vop_lookup_v2_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct componentname *cnp = ap->a_cnp;
	struct vnode *dvp = ap->a_dvp;
	struct vnode **vpp = ap->a_vpp;
	struct mount *mp = dvp->v_mount;
	struct smbnode *dnp;
	struct smbfattr fattr;
	struct smb_cred scred;
	const char *name = cnp->cn_nameptr;
	int flags = cnp->cn_flags;
	int nameiop = cnp->cn_nameiop;
	int nmlen = cnp->cn_namelen;
	int error, islastcn, isdot;

	/*
	 * Check accessiblity of directory.
	 */
	error = VOP_ACCESS(dvp, VEXEC, cnp->cn_cred);
	if (error)
		return (error);

	if ((cnp->cn_flags & ISLASTCN) &&
	    (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);

	SMBVDEBUG("%d '%.*s' in '%.*s'\n", nameiop, nmlen, name,
	    (int) VTOSMB(dvp)->n_nmlen, VTOSMB(dvp)->n_name);

	islastcn = flags & ISLASTCN;

	/*
	 * Before tediously performing a linear scan of the directory,
	 * check the name cache to see if the directory/name pair
	 * we are looking for is known already.
	 * If the directory/name pair is found in the name cache,
	 * we have to ensure the directory has not changed from
	 * the time the cache entry has been created. If it has,
	 * the cache entry has to be ignored.
	 */
	if (cache_lookup(dvp, cnp->cn_nameptr, cnp->cn_namelen,
			 cnp->cn_nameiop, cnp->cn_flags,
			 NULL, vpp)) {
		struct vattr vattr;
		struct vnode *newvp;
		bool killit = false;

		error = VOP_ACCESS(dvp, VEXEC, cnp->cn_cred);
		if (error != 0) {
			if (*vpp != NULLVP) {
				vrele(*vpp);
			}
			*vpp = NULLVP;
			return error;
		}

		if (*vpp == NULLVP) {
			if (!VOP_GETATTR(dvp, &vattr, cnp->cn_cred)
			    && vattr.va_mtime.tv_sec == VTOSMB(dvp)->n_nctime)
				return ENOENT;
			cache_purge(dvp);
			VTOSMB(dvp)->n_nctime = 0;
			goto dolookup;
		}

		newvp = *vpp;
		if (newvp != dvp)
			vn_lock(newvp, LK_SHARED | LK_RETRY);
		error = VOP_GETATTR(newvp, &vattr, cnp->cn_cred);
		/*
		 * If the file type on the server is inconsistent
		 * with what it was when we created the vnode,
		 * kill the bogus vnode now and fall through to
		 * the code below to create a new one with the
		 * right type.
		 */
		if (error == 0 &&
		    ((newvp->v_type == VDIR &&
		    (VTOSMB(newvp)->n_dosattr & SMB_FA_DIR) == 0) ||
		    (newvp->v_type == VREG &&
		    (VTOSMB(newvp)->n_dosattr & SMB_FA_DIR) != 0)))
			killit = true;
		else if (error == 0
			&& vattr.va_ctime.tv_sec == VTOSMB(newvp)->n_ctime)
		{
			if (newvp != dvp)
				VOP_UNLOCK(newvp);
			/* nfsstats.lookupcache_hits++; */
			return (0);
		}

		cache_purge(newvp);
		if (newvp != dvp) {
			if (killit) {
				VOP_UNLOCK(newvp);
				vgone(newvp);
			} else
				vput(newvp);
		} else
			vrele(newvp);
		*vpp = NULLVP;
	}

 dolookup:

	/* ensure the name is sane */
	if (nameiop != LOOKUP) {
		error = smbfs_pathcheck(VFSTOSMBFS(mp), cnp->cn_nameptr,
					cnp->cn_namelen);
		if (error)
			return (error);
	}

	dnp = VTOSMB(dvp);
	isdot = (nmlen == 1 && name[0] == '.');

	/*
	 * entry is not in the cache or has been expired
	 */
	smb_makescred(&scred, curlwp, cnp->cn_cred);
	if (flags & ISDOTDOT)
		error = smbfs_smb_lookup(VTOSMB(dnp->n_parent), NULL, 0,
		    &fattr, &scred);
	else
		error = smbfs_smb_lookup(dnp, name, nmlen, &fattr, &scred);

	if (error) {
		/* Not found */

		if (error != ENOENT)
			return (error);

		/*
		 * Handle RENAME or CREATE case...
		 */
		if ((nameiop == CREATE || nameiop == RENAME) && islastcn) {
			/*
			 * Access for write is interpreted as allowing
			 * creation of files in the directory.
			 */
			error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred);
			if (error)
				return (error);

			return (EJUSTRETURN);
		}

		/*
		 * Insert name into cache (as non-existent) if appropriate.
		 */
		if (nameiop != CREATE)
			cache_enter(dvp, *vpp, cnp->cn_nameptr, cnp->cn_namelen,
				    cnp->cn_flags);

		return (ENOENT);
	}

	/* Found */

	/* Handle RENAME case... */
	if (nameiop == RENAME && islastcn) {
		error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred);
		if (error)
			return (error);

		if (isdot)
			return (EISDIR);
		error = smbfs_nget(mp, dvp, name, nmlen, &fattr, vpp);
		if (error)
			return (error);
		return (0);
	}

	if (isdot) {
		vref(dvp);
		*vpp = dvp;
		error = 0;
	} else {
		error = smbfs_nget(mp, dvp, name, nmlen,
		    ((flags & ISDOTDOT) ? NULL : &fattr), vpp);
	}
	if (error)
		return error;

	if (cnp->cn_nameiop != DELETE || !islastcn) {
		VTOSMB(*vpp)->n_ctime = VTOSMB(*vpp)->n_mtime.tv_sec;
		cache_enter(dvp, *vpp, cnp->cn_nameptr, cnp->cn_namelen,
			    cnp->cn_flags);
#ifdef notdef
	} else if (error == ENOENT && cnp->cn_nameiop != CREATE) {
		VTOSMB(*vpp)->n_nctime = VTOSMB(*vpp)->n_mtime.tv_sec;
		cache_enter(dvp, *vpp, cnp->cn_nameptr, cnp->cn_namelen,
			    cnp->cn_flags);
#endif
	}

	return (0);
}

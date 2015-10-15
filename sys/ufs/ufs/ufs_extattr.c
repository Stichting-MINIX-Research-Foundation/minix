/*	$NetBSD: ufs_extattr.c,v 1.46 2014/11/19 16:26:47 manu Exp $	*/

/*-
 * Copyright (c) 1999-2002 Robert N. M. Watson
 * Copyright (c) 2002-2003 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
 *
 * This software was developed for the FreeBSD Project in part by Network
 * Associates Laboratories, the Security Research Division of Network
 * Associates, Inc. under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"),
 * as part of the DARPA CHATS research program.
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
 */

/*
 * Support for file system extended attributes on the UFS1 file system.
 *
 * Extended attributes are defined in the form name=value, where name is
 * a nul-terminated string in the style of a file name, and value is a
 * binary blob of zero or more bytes.  The UFS1 extended attribute service
 * layers support for extended attributes onto a backing file, in the style
 * of the quota implementation, meaning that it requires no underlying format
 * changes to the file system.  This design choice exchanges simplicity,
 * usability, and easy deployment for performance.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ufs_extattr.c,v 1.46 2014/11/19 16:26:47 manu Exp $");

#ifdef _KERNEL_OPT
#include "opt_ffs.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/kauth.h>
#include <sys/kernel.h>
#include <sys/namei.h>
#include <sys/kmem.h>
#include <sys/fcntl.h>
#include <sys/lwp.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/lock.h>
#include <sys/dirent.h>
#include <sys/extattr.h>
#include <sys/sysctl.h>

#include <ufs/ufs/dir.h>
#include <ufs/ufs/extattr.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufs_bswap.h>
#include <ufs/ufs/ufs_extern.h>

int ufs_extattr_sync = 1;
int ufs_extattr_autocreate = 1024;

static int	ufs_extattr_valid_attrname(int attrnamespace,
		    const char *attrname);
static int	ufs_extattr_enable_with_open(struct ufsmount *ump,
		    struct vnode *vp, int attrnamespace, const char *attrname,
		    struct lwp *l);
static int	ufs_extattr_enable(struct ufsmount *ump, int attrnamespace,
		    const char *attrname, struct vnode *backing_vnode,
		    struct lwp *l);
static int	ufs_extattr_disable(struct ufsmount *ump, int attrnamespace,
		    const char *attrname, struct lwp *l);
static int	ufs_extattr_get(struct vnode *vp, int attrnamespace,
		    const char *name, struct uio *uio, size_t *size,
		    kauth_cred_t cred, struct lwp *l);
static int	ufs_extattr_list(struct vnode *vp, int attrnamespace,
		    struct uio *uio, size_t *size, int flag,
		    kauth_cred_t cred, struct lwp *l);
static int	ufs_extattr_set(struct vnode *vp, int attrnamespace,
		    const char *name, struct uio *uio, kauth_cred_t cred,
		    struct lwp *l);
static int	ufs_extattr_rm(struct vnode *vp, int attrnamespace,
		    const char *name, kauth_cred_t cred, struct lwp *l);
static struct ufs_extattr_list_entry *ufs_extattr_find_attr(struct ufsmount *,
		    int, const char *);
static int	ufs_extattr_get_header(struct vnode *, 
		    struct ufs_extattr_list_entry *, 
		    struct ufs_extattr_header *, off_t *);

/*
 * Convert a FreeBSD extended attribute and namespace to a consistent string
 * representation.
 *
 * The returned value, if not NULL, is guaranteed to be an allocated object
 * of its size as returned by strlen() + 1 and must be freed by the caller.
 */
static char *
from_freebsd_extattr(int attrnamespace, const char *attrname)
{
	const char *namespace;
	char *attr;
	size_t len;

	if (attrnamespace == EXTATTR_NAMESPACE_SYSTEM)
		namespace = "system";
	else if (attrnamespace == EXTATTR_NAMESPACE_USER)
		namespace = "user";
	else
		return NULL;

	/* <namespace>.<attrname>\0 */
	len = strlen(namespace) + 1 + strlen(attrname) + 1;

	attr = kmem_alloc(len, KM_SLEEP);

	snprintf(attr, len, "%s.%s", namespace, attrname);

	return attr;
}

/*
 * Internal wrapper around a conversion-check-free sequence.
 */
static int
internal_extattr_check_cred(vnode_t *vp, int attrnamespace, const char *name,
    kauth_cred_t cred, int access_mode)
{
	char *attr;
	int error;

	attr = from_freebsd_extattr(attrnamespace, name);
	if (attr == NULL)
		return EINVAL;

	error = extattr_check_cred(vp, attr, cred, access_mode);

	kmem_free(attr, strlen(attr) + 1);

	return error;
}

/*
 * Per-FS attribute lock protecting attribute operations.
 * XXX Right now there is a lot of lock contention due to having a single
 * lock per-FS; really, this should be far more fine-grained.
 */
static void
ufs_extattr_uepm_lock(struct ufsmount *ump)
{

	/* XXX Why does this need to be recursive? */
	if (mutex_owned(&ump->um_extattr.uepm_lock)) {
		ump->um_extattr.uepm_lockcnt++;
		return;
	}
	mutex_enter(&ump->um_extattr.uepm_lock);
}

static void
ufs_extattr_uepm_unlock(struct ufsmount *ump)
{

	if (ump->um_extattr.uepm_lockcnt != 0) {
		KASSERT(mutex_owned(&ump->um_extattr.uepm_lock));
		ump->um_extattr.uepm_lockcnt--;
		return;
	}
	mutex_exit(&ump->um_extattr.uepm_lock);
}

/*-
 * Determine whether the name passed is a valid name for an actual
 * attribute.
 *
 * Invalid currently consists of:
 *	 NULL pointer for attrname
 *	 zero-length attrname (used to retrieve application attribute list)
 */
static int
ufs_extattr_valid_attrname(int attrnamespace, const char *attrname)
{

	if (attrname == NULL)
		return (0);
	if (strlen(attrname) == 0)
		return (0);
	return (1);
}

/*
 * Autocreate an attribute storage
 */
static int
ufs_extattr_autocreate_attr(struct vnode *vp, int attrnamespace,
    const char *attrname, struct lwp *l, struct ufs_extattr_list_entry **uelep)
{
	struct mount *mp = vp->v_mount;
	struct ufsmount *ump = VFSTOUFS(mp);
	struct vnode *backing_vp;
	struct nameidata nd;
	struct pathbuf *pb;
	char *path;
	struct ufs_extattr_fileheader uef;
	struct ufs_extattr_list_entry *uele;
	int error;

	path = PNBUF_GET();

	/* 
	 * We only support system and user namespace autocreation
	 */ 
	switch (attrnamespace) {
	case EXTATTR_NAMESPACE_SYSTEM:
		(void)snprintf(path, PATH_MAX, "%s/%s/%s/%s", 
			       mp->mnt_stat.f_mntonname,
			       UFS_EXTATTR_FSROOTSUBDIR,
			       UFS_EXTATTR_SUBDIR_SYSTEM,
			       attrname);
		break;
	case EXTATTR_NAMESPACE_USER:
		(void)snprintf(path, PATH_MAX, "%s/%s/%s/%s", 
			       mp->mnt_stat.f_mntonname,
			       UFS_EXTATTR_FSROOTSUBDIR,
			       UFS_EXTATTR_SUBDIR_USER,
			       attrname);
		break;
	default:
		PNBUF_PUT(path);
		*uelep = NULL;
		return EINVAL;
		break;
	}

	/*
	 * Release extended attribute mount lock, otherwise
	 * we can deadlock with another thread that would lock 
	 * vp after we unlock it below, and call 
	 * ufs_extattr_uepm_lock(ump), for instance
	 * in ufs_getextattr().
	 */
	ufs_extattr_uepm_unlock(ump);

	/*
	 * XXX unlock/lock should only be done when setting extattr
	 * on backing store or one of its parent directory 
	 * including root, but we always do it for now.
	 */ 
	KASSERT(VOP_ISLOCKED(vp) == LK_EXCLUSIVE);
	VOP_UNLOCK(vp);

	pb = pathbuf_create(path);
	NDINIT(&nd, CREATE, LOCKPARENT, pb);
	
	/*
	 * Since we do not hold ufs_extattr_uepm_lock anymore,
	 * another thread may race with us for backend creation,
	 * but only one can succeed here thanks to O_EXCL
	 */
	error = vn_open(&nd, O_CREAT|O_EXCL|O_RDWR, 0600);

	/*
	 * Reacquire the lock on the vnode
	 */
	KASSERT(VOP_ISLOCKED(vp) == 0);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);

	ufs_extattr_uepm_lock(ump);

	if (error != 0) {
		pathbuf_destroy(pb);
		PNBUF_PUT(path);
		*uelep = NULL;
		return error;
	}

	KASSERT(nd.ni_vp != NULL);
	KASSERT(VOP_ISLOCKED(nd.ni_vp) == LK_EXCLUSIVE);
	KASSERT(VOP_ISLOCKED(nd.ni_dvp) == 0);

	/*
 	 * backing_vp is the backing store. 
	 */	
	backing_vp = nd.ni_vp;
	pathbuf_destroy(pb);
	PNBUF_PUT(path);

	uef.uef_magic = UFS_EXTATTR_MAGIC;
	uef.uef_version = UFS_EXTATTR_VERSION;
	uef.uef_size = ufs_extattr_autocreate;

	error = vn_rdwr(UIO_WRITE, backing_vp, &uef, sizeof(uef), 0,
		        UIO_SYSSPACE, IO_NODELOCKED|IO_APPEND, 
			l->l_cred, NULL, l);

	VOP_UNLOCK(backing_vp);

	if (error != 0) {
		printf("%s: write uef header failed for %s, error = %d\n", 
		       __func__, attrname, error);
		vn_close(backing_vp, FREAD|FWRITE, l->l_cred);
		*uelep = NULL;
		return error;
	}

	/*
	 * Now enable attribute. 
	 */
	error = ufs_extattr_enable(ump,attrnamespace, attrname, backing_vp, l);
	KASSERT(VOP_ISLOCKED(backing_vp) == 0);

	if (error != 0) {
		printf("%s: enable %s failed, error %d\n", 
		       __func__, attrname, error);
		vn_close(backing_vp, FREAD|FWRITE, l->l_cred);
		*uelep = NULL;
		return error;
	}

	uele = ufs_extattr_find_attr(ump, attrnamespace, attrname);
	if (uele == NULL) {
		printf("%s: atttribute %s created but not found!\n",
		       __func__, attrname);
		vn_close(backing_vp, FREAD|FWRITE, l->l_cred);
		*uelep = NULL;
		return ESRCH; /* really internal error */
	}

	printf("%s: EA backing store autocreated for %s\n",
	       mp->mnt_stat.f_mntonname, attrname);

	*uelep = uele;
	return 0;
}

/*
 * Locate an attribute given a name and mountpoint.
 * Must be holding uepm lock for the mount point.
 */
static struct ufs_extattr_list_entry *
ufs_extattr_find_attr(struct ufsmount *ump, int attrnamespace,
    const char *attrname)
{
	struct ufs_extattr_list_entry *search_attribute;

	for (search_attribute = LIST_FIRST(&ump->um_extattr.uepm_list);
	    search_attribute != NULL;
	    search_attribute = LIST_NEXT(search_attribute, uele_entries)) {
		if (!(strncmp(attrname, search_attribute->uele_attrname,
		    UFS_EXTATTR_MAXEXTATTRNAME)) &&
		    (attrnamespace == search_attribute->uele_attrnamespace)) {
			return (search_attribute);
		}
	}

	return (0);
}

/*
 * Initialize per-FS structures supporting extended attributes.  Do not
 * start extended attributes yet.
 */
void
ufs_extattr_uepm_init(struct ufs_extattr_per_mount *uepm)
{

	uepm->uepm_flags = 0;
	uepm->uepm_lockcnt = 0;

	LIST_INIT(&uepm->uepm_list);
	mutex_init(&uepm->uepm_lock, MUTEX_DEFAULT, IPL_NONE);
	uepm->uepm_flags |= UFS_EXTATTR_UEPM_INITIALIZED;
}

/*
 * Destroy per-FS structures supporting extended attributes.  Assumes
 * that EAs have already been stopped, and will panic if not.
 */
void
ufs_extattr_uepm_destroy(struct ufs_extattr_per_mount *uepm)
{

	if (!(uepm->uepm_flags & UFS_EXTATTR_UEPM_INITIALIZED))
		panic("ufs_extattr_uepm_destroy: not initialized");

	if ((uepm->uepm_flags & UFS_EXTATTR_UEPM_STARTED))
		panic("ufs_extattr_uepm_destroy: called while still started");

	/*
	 * It's not clear that either order for the next three lines is
	 * ideal, and it should never be a problem if this is only called
	 * during unmount, and with vfs_busy().
	 */
	uepm->uepm_flags &= ~UFS_EXTATTR_UEPM_STARTED;
	uepm->uepm_flags &= ~UFS_EXTATTR_UEPM_INITIALIZED;
	mutex_destroy(&uepm->uepm_lock);
}

/*
 * Start extended attribute support on an FS.
 */
int
ufs_extattr_start(struct mount *mp, struct lwp *l)
{
	struct ufsmount *ump;
	int error = 0;

	ump = VFSTOUFS(mp);

	if (!(ump->um_extattr.uepm_flags & UFS_EXTATTR_UEPM_INITIALIZED))
		ufs_extattr_uepm_init(&ump->um_extattr); 

	ufs_extattr_uepm_lock(ump);

	if (!(ump->um_extattr.uepm_flags & UFS_EXTATTR_UEPM_INITIALIZED)) {
		error = EOPNOTSUPP;
		goto unlock;
	}
	if (ump->um_extattr.uepm_flags & UFS_EXTATTR_UEPM_STARTED) {
		error = EBUSY;
		goto unlock;
	}

	ump->um_extattr.uepm_flags |= UFS_EXTATTR_UEPM_STARTED;

	ump->um_extattr.uepm_ucred = l->l_cred;
	kauth_cred_hold(ump->um_extattr.uepm_ucred);

 unlock:
	ufs_extattr_uepm_unlock(ump);

	return (error);
}

/*
 * Helper routine: given a locked parent directory and filename, return
 * the locked vnode of the inode associated with the name.  Will not
 * follow symlinks, may return any type of vnode.  Lock on parent will
 * be released even in the event of a failure.  In the event that the
 * target is the parent (i.e., "."), there will be two references and
 * one lock, requiring the caller to possibly special-case.
 */
static int
ufs_extattr_lookup(struct vnode *start_dvp, int lockparent, const char *dirname,
    struct vnode **vp, struct lwp *l)
{
	struct vop_lookup_v2_args vargs;
	struct componentname cnp;
	struct vnode *target_vp;
	char *pnbuf;
	int error;

	KASSERT(VOP_ISLOCKED(start_dvp) == LK_EXCLUSIVE);

	pnbuf = PNBUF_GET();

	memset(&cnp, 0, sizeof(cnp));
	cnp.cn_nameiop = LOOKUP;
	cnp.cn_flags = ISLASTCN | lockparent;
	cnp.cn_cred = l->l_cred;
	cnp.cn_nameptr = pnbuf;
	error = copystr(dirname, pnbuf, MAXPATHLEN, &cnp.cn_namelen);
	if (error) {
		if (lockparent == 0) {
			VOP_UNLOCK(start_dvp);
		}
		PNBUF_PUT(pnbuf);
		printf("ufs_extattr_lookup: copystr failed\n");
		return (error);
	}
	cnp.cn_namelen--;	/* trim nul termination */
	vargs.a_desc = NULL;
	vargs.a_dvp = start_dvp;
	vargs.a_vpp = &target_vp;
	vargs.a_cnp = &cnp;
	error = ufs_lookup(&vargs);
	PNBUF_PUT(pnbuf);
	if (error) {
		if (lockparent == 0) {
			VOP_UNLOCK(start_dvp);
		}
		return (error);
	}
#if 0
	if (target_vp == start_dvp)
		panic("ufs_extattr_lookup: target_vp == start_dvp");
#endif

	if (target_vp != start_dvp) {
		error = vn_lock(target_vp, LK_EXCLUSIVE);
		if (lockparent == 0)
			VOP_UNLOCK(start_dvp);
		if (error) {
			vrele(target_vp);
			return error;
		}
	}

	KASSERT(VOP_ISLOCKED(target_vp) == LK_EXCLUSIVE);
	*vp = target_vp;
	return (0);
}

/*
 * Enable an EA using the passed filesystem, backing vnode, attribute name,
 * namespace, and proc.  Will perform a VOP_OPEN() on the vp, so expects vp
 * to be locked when passed in.  The vnode will be returned unlocked,
 * regardless of success/failure of the function.  As a result, the caller
 * will always need to vrele(), but not vput().
 */
static int
ufs_extattr_enable_with_open(struct ufsmount *ump, struct vnode *vp,
    int attrnamespace, const char *attrname, struct lwp *l)
{
	int error;

	error = VOP_OPEN(vp, FREAD|FWRITE, l->l_cred);
	if (error) {
		printf("ufs_extattr_enable_with_open.VOP_OPEN(): failed "
		    "with %d\n", error);
		VOP_UNLOCK(vp);
		return (error);
	}

	mutex_enter(vp->v_interlock);
	vp->v_writecount++;
	mutex_exit(vp->v_interlock);

	vref(vp);

	VOP_UNLOCK(vp);

	error = ufs_extattr_enable(ump, attrnamespace, attrname, vp, l);
	if (error != 0)
		vn_close(vp, FREAD|FWRITE, l->l_cred);
	return (error);
}

/*
 * Given a locked directory vnode, iterate over the names in the directory
 * and use ufs_extattr_lookup() to retrieve locked vnodes of potential
 * attribute files.  Then invoke ufs_extattr_enable_with_open() on each
 * to attempt to start the attribute.  Leaves the directory locked on
 * exit.
 */
static int
ufs_extattr_iterate_directory(struct ufsmount *ump, struct vnode *dvp,
    int attrnamespace, struct lwp *l)
{
	struct vop_readdir_args vargs;
	struct statvfs *sbp = &ump->um_mountp->mnt_stat;
	struct dirent *dp, *edp;
	struct vnode *attr_vp;
	struct uio auio;
	struct iovec aiov;
	char *dirbuf;
	int error, eofflag = 0;

	if (dvp->v_type != VDIR)
		return (ENOTDIR);

	dirbuf = kmem_alloc(UFS_DIRBLKSIZ, KM_SLEEP);

	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_offset = 0;
	UIO_SETUP_SYSSPACE(&auio);

	vargs.a_desc = NULL;
	vargs.a_vp = dvp;
	vargs.a_uio = &auio;
	vargs.a_cred = l->l_cred;
	vargs.a_eofflag = &eofflag;
	vargs.a_ncookies = NULL;
	vargs.a_cookies = NULL;

	while (!eofflag) {
		auio.uio_resid = UFS_DIRBLKSIZ;
		aiov.iov_base = dirbuf;
		aiov.iov_len = UFS_DIRBLKSIZ;
		error = ufs_readdir(&vargs);
		if (error) {
			printf("ufs_extattr_iterate_directory: ufs_readdir "
			    "%d\n", error);
			return (error);
		}

		/*
		 * XXXRW: While in UFS, we always get UFS_DIRBLKSIZ returns from
		 * the directory code on success, on other file systems this
		 * may not be the case.  For portability, we should check the
		 * read length on return from ufs_readdir().
		 */
		edp = (struct dirent *)&dirbuf[UFS_DIRBLKSIZ];
		for (dp = (struct dirent *)dirbuf; dp < edp; ) {
			if (dp->d_reclen == 0)
				break;
			/* Skip "." and ".." */
			if (dp->d_name[0] == '.' &&
			    (dp->d_name[1] == '\0' ||
			     (dp->d_name[1] == '.' && dp->d_name[2] == '\0')))
				goto next;
			error = ufs_extattr_lookup(dvp, LOCKPARENT,
			    dp->d_name, &attr_vp, l);
			if (error == ENOENT) {
				goto next; /* keep silent */
			} else if (error) {
				printf("ufs_extattr_iterate_directory: lookup "
				    "%s %d\n", dp->d_name, error);
			} else if (attr_vp == dvp) {
				vrele(attr_vp);
			} else if (attr_vp->v_type != VREG) {
				vput(attr_vp);
			} else {
				error = ufs_extattr_enable_with_open(ump,
				    attr_vp, attrnamespace, dp->d_name, l);
				vrele(attr_vp);
				if (error) {
					printf("ufs_extattr_iterate_directory: "
					    "enable %s %d\n", dp->d_name,
					    error);
				} else if (bootverbose) {
					printf("%s: EA %s loaded\n",
					       sbp->f_mntonname, dp->d_name);
				}
			}
 next:
			dp = (struct dirent *) ((char *)dp + dp->d_reclen);
			if (dp >= edp)
				break;
		}
	}
	kmem_free(dirbuf, UFS_DIRBLKSIZ);
	
	return (0);
}

/*
 * Auto-start of extended attributes, to be executed (optionally) at
 * mount-time.
 */
int
ufs_extattr_autostart(struct mount *mp, struct lwp *l)
{
	struct vnode *rvp, *attr_dvp, *attr_system_dvp, *attr_user_dvp;
	int error;

	/*
	 * Does UFS_EXTATTR_FSROOTSUBDIR exist off the filesystem root?
	 * If so, automatically start EA's.
	 */
	error = VFS_ROOT(mp, &rvp);
	if (error) {
		printf("ufs_extattr_autostart.VFS_ROOT() returned %d\n",
		    error);
		return (error);
	}

	KASSERT(VOP_ISLOCKED(rvp) == LK_EXCLUSIVE);

	error = ufs_extattr_lookup(rvp, 0,
	    UFS_EXTATTR_FSROOTSUBDIR, &attr_dvp, l);
	if (error) {
		/* rvp ref'd but now unlocked */
		KASSERT(VOP_ISLOCKED(rvp) == 0);
		vrele(rvp);
		return (error);
	}
	if (rvp == attr_dvp) {
		/* Should never happen. */
		KASSERT(VOP_ISLOCKED(rvp) == LK_EXCLUSIVE);
		vrele(attr_dvp);
		vput(rvp);
		return (EINVAL);
	}
	KASSERT(VOP_ISLOCKED(rvp) == 0);
	vrele(rvp);

	KASSERT(VOP_ISLOCKED(attr_dvp) == LK_EXCLUSIVE);

	if (attr_dvp->v_type != VDIR) {
		printf("ufs_extattr_autostart: %s != VDIR\n",
		    UFS_EXTATTR_FSROOTSUBDIR);
		goto return_vput_attr_dvp;
	}

	error = ufs_extattr_start(mp, l);
	if (error) {
		printf("ufs_extattr_autostart: ufs_extattr_start failed (%d)\n",
		    error);
		goto return_vput_attr_dvp;
	}

	/*
	 * Look for two subdirectories: UFS_EXTATTR_SUBDIR_SYSTEM,
	 * UFS_EXTATTR_SUBDIR_USER.  For each, iterate over the sub-directory,
	 * and start with appropriate type.  Failures in either don't
	 * result in an over-all failure.  attr_dvp is left locked to
	 * be cleaned up on exit.
	 */
	error = ufs_extattr_lookup(attr_dvp, LOCKPARENT,
	    UFS_EXTATTR_SUBDIR_SYSTEM, &attr_system_dvp, l);
	KASSERT(VOP_ISLOCKED(attr_dvp) == LK_EXCLUSIVE);
	if (error == 0) {
		KASSERT(VOP_ISLOCKED(attr_system_dvp) == LK_EXCLUSIVE);
		error = ufs_extattr_iterate_directory(VFSTOUFS(mp),
		    attr_system_dvp, EXTATTR_NAMESPACE_SYSTEM, l);
		if (error)
			printf("ufs_extattr_iterate_directory returned %d\n",
			    error);
		KASSERT(VOP_ISLOCKED(attr_system_dvp) == LK_EXCLUSIVE);
		vput(attr_system_dvp);
	}

	error = ufs_extattr_lookup(attr_dvp, LOCKPARENT,
	    UFS_EXTATTR_SUBDIR_USER, &attr_user_dvp, l);
	KASSERT(VOP_ISLOCKED(attr_dvp) == LK_EXCLUSIVE);
	if (error == 0) {
		KASSERT(VOP_ISLOCKED(attr_user_dvp) == LK_EXCLUSIVE);
		error = ufs_extattr_iterate_directory(VFSTOUFS(mp),
		    attr_user_dvp, EXTATTR_NAMESPACE_USER, l);
		if (error)
			printf("ufs_extattr_iterate_directory returned %d\n",
			    error);
		KASSERT(VOP_ISLOCKED(attr_user_dvp) == LK_EXCLUSIVE);
		vput(attr_user_dvp);
	}

	/* Mask startup failures in sub-directories. */
	error = 0;

 return_vput_attr_dvp:
	KASSERT(VOP_ISLOCKED(attr_dvp) == LK_EXCLUSIVE);
	vput(attr_dvp);

	return (error);
}

/*
 * Stop extended attribute support on an FS.
 */
void
ufs_extattr_stop(struct mount *mp, struct lwp *l)
{
	struct ufs_extattr_list_entry *uele;
	struct ufsmount *ump = VFSTOUFS(mp);

	ufs_extattr_uepm_lock(ump);

	/*
	 * If we haven't been started, no big deal.  Just short-circuit
	 * the processing work.
	 */
	if (!(ump->um_extattr.uepm_flags & UFS_EXTATTR_UEPM_STARTED)) {
		goto unlock;
	}

	while (LIST_FIRST(&ump->um_extattr.uepm_list) != NULL) {
		uele = LIST_FIRST(&ump->um_extattr.uepm_list);
		ufs_extattr_disable(ump, uele->uele_attrnamespace,
		    uele->uele_attrname, l);
	}

	ump->um_extattr.uepm_flags &= ~UFS_EXTATTR_UEPM_STARTED;

	kauth_cred_free(ump->um_extattr.uepm_ucred);
	ump->um_extattr.uepm_ucred = NULL;

 unlock:
	ufs_extattr_uepm_unlock(ump);
}

/*
 * Enable a named attribute on the specified filesystem; provide an
 * unlocked backing vnode to hold the attribute data.
 */
static int
ufs_extattr_enable(struct ufsmount *ump, int attrnamespace,
    const char *attrname, struct vnode *backing_vnode, struct lwp *l)
{
	struct ufs_extattr_list_entry *attribute;
	struct iovec aiov;
	struct uio auio;
	int error = 0;

	if (!ufs_extattr_valid_attrname(attrnamespace, attrname))
		return (EINVAL);
	if (backing_vnode->v_type != VREG)
		return (EINVAL);

	attribute = kmem_zalloc(sizeof(*attribute), KM_SLEEP);

	if (!(ump->um_extattr.uepm_flags & UFS_EXTATTR_UEPM_STARTED)) {
		error = EOPNOTSUPP;
		goto free_exit;
	}

	if (ufs_extattr_find_attr(ump, attrnamespace, attrname)) {
		error = EEXIST;
		goto free_exit;
	}

	strncpy(attribute->uele_attrname, attrname,
	    UFS_EXTATTR_MAXEXTATTRNAME);
	attribute->uele_attrnamespace = attrnamespace;
	memset(&attribute->uele_fileheader, 0,
	    sizeof(struct ufs_extattr_fileheader));
	
	attribute->uele_backing_vnode = backing_vnode;

	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	aiov.iov_base = (void *) &attribute->uele_fileheader;
	aiov.iov_len = sizeof(struct ufs_extattr_fileheader);
	auio.uio_resid = sizeof(struct ufs_extattr_fileheader);
	auio.uio_offset = (off_t) 0;
	auio.uio_rw = UIO_READ;
	UIO_SETUP_SYSSPACE(&auio);

	vn_lock(backing_vnode, LK_SHARED | LK_RETRY);
	error = VOP_READ(backing_vnode, &auio, IO_NODELOCKED,
	    ump->um_extattr.uepm_ucred);

	if (error)
		goto unlock_free_exit;

	if (auio.uio_resid != 0) {
		printf("ufs_extattr_enable: malformed attribute header\n");
		error = EINVAL;
		goto unlock_free_exit;
	}

	/*
	 * Try to determine the byte order of the attribute file.
	 */
	if (attribute->uele_fileheader.uef_magic != UFS_EXTATTR_MAGIC) {
		attribute->uele_flags |= UELE_F_NEEDSWAP;
		attribute->uele_fileheader.uef_magic =
		    ufs_rw32(attribute->uele_fileheader.uef_magic,
			     UELE_NEEDSWAP(attribute));
		if (attribute->uele_fileheader.uef_magic != UFS_EXTATTR_MAGIC) {
			printf("ufs_extattr_enable: invalid attribute header "
			       "magic\n");
			error = EINVAL;
			goto unlock_free_exit;
		}
	}
	attribute->uele_fileheader.uef_version =
	    ufs_rw32(attribute->uele_fileheader.uef_version,
		     UELE_NEEDSWAP(attribute));
	attribute->uele_fileheader.uef_size =
	    ufs_rw32(attribute->uele_fileheader.uef_size,
		     UELE_NEEDSWAP(attribute));

	if (attribute->uele_fileheader.uef_version != UFS_EXTATTR_VERSION) {
		printf("ufs_extattr_enable: incorrect attribute header "
		    "version\n");
		error = EINVAL;
		goto unlock_free_exit;
	}

	LIST_INSERT_HEAD(&ump->um_extattr.uepm_list, attribute,
	    uele_entries);

	VOP_UNLOCK(backing_vnode);
	return (0);

 unlock_free_exit:
	VOP_UNLOCK(backing_vnode);

 free_exit:
	kmem_free(attribute, sizeof(*attribute));
	return (error);
}

/*
 * Disable extended attribute support on an FS.
 */
static int
ufs_extattr_disable(struct ufsmount *ump, int attrnamespace,
    const char *attrname, struct lwp *l)
{
	struct ufs_extattr_list_entry *uele;
	int error = 0;

	if (!ufs_extattr_valid_attrname(attrnamespace, attrname))
		return (EINVAL);

	uele = ufs_extattr_find_attr(ump, attrnamespace, attrname);
	if (!uele)
		return (ENODATA);

	LIST_REMOVE(uele, uele_entries);

	error = vn_close(uele->uele_backing_vnode, FREAD|FWRITE,
	    l->l_cred);

	kmem_free(uele, sizeof(*uele));

	return (error);
}

/*
 * VFS call to manage extended attributes in UFS.  If filename_vp is
 * non-NULL, it must be passed in locked, and regardless of errors in
 * processing, will be unlocked.
 */
int
ufs_extattrctl(struct mount *mp, int cmd, struct vnode *filename_vp,
    int attrnamespace, const char *attrname)
{
	struct lwp *l = curlwp;
	struct ufsmount *ump = VFSTOUFS(mp);
	int error;

	/*
	 * Only privileged processes can configure extended attributes.
	 */
	error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_FS_EXTATTR,
	    0, mp, NULL, NULL);
	if (error) {
		if (filename_vp != NULL)
			VOP_UNLOCK(filename_vp);
		return (error);
	}

	switch(cmd) {
	case UFS_EXTATTR_CMD_START:
		if (filename_vp != NULL) {
			VOP_UNLOCK(filename_vp);
			return (EINVAL);
		}
		if (attrname != NULL)
			return (EINVAL);

		error = ufs_extattr_autostart(mp, l);
		return (error);
		
	case UFS_EXTATTR_CMD_STOP:
		if (filename_vp != NULL) {
			VOP_UNLOCK(filename_vp);
			return (EINVAL);
		}
		if (attrname != NULL)
			return (EINVAL);

		ufs_extattr_stop(mp, l);
		return (0);

	case UFS_EXTATTR_CMD_ENABLE:
		if (filename_vp == NULL)
			return (EINVAL);
		if (attrname == NULL) {
			VOP_UNLOCK(filename_vp);
			return (EINVAL);
		}

		/*
		 * ufs_extattr_enable_with_open() will always unlock the
		 * vnode, regardless of failure.
		 */
		ufs_extattr_uepm_lock(ump);
		error = ufs_extattr_enable_with_open(ump, filename_vp,
		    attrnamespace, attrname, l);
		ufs_extattr_uepm_unlock(ump);
		return (error);

	case UFS_EXTATTR_CMD_DISABLE:
		if (filename_vp != NULL) {
			VOP_UNLOCK(filename_vp);
			return (EINVAL);
		}
		if (attrname == NULL)
			return (EINVAL);

		ufs_extattr_uepm_lock(ump);
		error = ufs_extattr_disable(ump, attrnamespace, attrname, l);
		ufs_extattr_uepm_unlock(ump);
		return (error);

	default:
		return (EINVAL);
	}
}

/*
 * Read extended attribute header for a given vnode and attribute.
 * Backing vnode should be locked and unlocked by caller.
 */
static int
ufs_extattr_get_header(struct vnode *vp, struct ufs_extattr_list_entry *uele,
    struct ufs_extattr_header *ueh, off_t *bap)
{
	struct mount *mp = vp->v_mount;
	struct ufsmount *ump = VFSTOUFS(mp);
	struct inode *ip = VTOI(vp);
	off_t base_offset;
	struct iovec aiov;
	struct uio aio;
	int error;

	/*
	 * Find base offset of header in file based on file header size, and
	 * data header size + maximum data size, indexed by inode number.
	 */
	base_offset = sizeof(struct ufs_extattr_fileheader) +
	    ip->i_number * (sizeof(struct ufs_extattr_header) +
	    uele->uele_fileheader.uef_size);

	/*
	 * Read in the data header to see if the data is defined, and if so
	 * how much.
	 */
	memset(ueh, 0, sizeof(struct ufs_extattr_header));
	aiov.iov_base = ueh;
	aiov.iov_len = sizeof(struct ufs_extattr_header);
	aio.uio_iov = &aiov;
	aio.uio_iovcnt = 1;
	aio.uio_rw = UIO_READ;
	aio.uio_offset = base_offset;
	aio.uio_resid = sizeof(struct ufs_extattr_header);
	UIO_SETUP_SYSSPACE(&aio);

	error = VOP_READ(uele->uele_backing_vnode, &aio,
	    IO_NODELOCKED, ump->um_extattr.uepm_ucred);
	if (error)
		return error;

	/*
	 * Attribute headers are kept in file system byte order.
	 * XXX What about the blob of data?
	 */
	ueh->ueh_flags = ufs_rw32(ueh->ueh_flags, UELE_NEEDSWAP(uele));
	ueh->ueh_len   = ufs_rw32(ueh->ueh_len, UELE_NEEDSWAP(uele));
	ueh->ueh_i_gen = ufs_rw32(ueh->ueh_i_gen, UELE_NEEDSWAP(uele));

	/* Defined? */
	if ((ueh->ueh_flags & UFS_EXTATTR_ATTR_FLAG_INUSE) == 0)
		return ENODATA;

	/* Valid for the current inode generation? */
	if (ueh->ueh_i_gen != ip->i_gen) {
		/*
		 * The inode itself has a different generation number
		 * than the uele data.  For now, the best solution
		 * is to coerce this to undefined, and let it get cleaned
		 * up by the next write or extattrctl clean.
		 */
		printf("%s (%s): inode gen inconsistency (%u, %jd)\n",
		       __func__,  mp->mnt_stat.f_mntonname, ueh->ueh_i_gen,
		       (intmax_t)ip->i_gen);
		return ENODATA;
	}

	/* Local size consistency check. */
	if (ueh->ueh_len > uele->uele_fileheader.uef_size)
		return ENXIO;

	/* Return base offset */
	if (bap != NULL)
		*bap = base_offset;

	return 0;
}

/*
 * Vnode operation to retrieve a named extended attribute.
 */
int
ufs_getextattr(struct vop_getextattr_args *ap)
/*
vop_getextattr {
	IN struct vnode *a_vp;
	IN int a_attrnamespace;
	IN const char *a_name;
	INOUT struct uio *a_uio;
	OUT size_t *a_size;
	IN kauth_cred_t a_cred;
};
*/
{
	struct mount *mp = ap->a_vp->v_mount;
	struct ufsmount *ump = VFSTOUFS(mp);
	int error;

	if (!(ump->um_extattr.uepm_flags & UFS_EXTATTR_UEPM_STARTED))
		return (EOPNOTSUPP);

	ufs_extattr_uepm_lock(ump);

	error = ufs_extattr_get(ap->a_vp, ap->a_attrnamespace, ap->a_name,
	    ap->a_uio, ap->a_size, ap->a_cred, curlwp);

	ufs_extattr_uepm_unlock(ump);

	return (error);
}

/*
 * Real work associated with retrieving a named attribute--assumes that
 * the attribute lock has already been grabbed.
 */
static int
ufs_extattr_get(struct vnode *vp, int attrnamespace, const char *name,
    struct uio *uio, size_t *size, kauth_cred_t cred, struct lwp *l)
{
	struct ufs_extattr_list_entry *attribute;
	struct ufs_extattr_header ueh;
	struct mount *mp = vp->v_mount;
	struct ufsmount *ump = VFSTOUFS(mp);
	off_t base_offset;
	size_t len, old_len;
	int error = 0;

	if (strlen(name) == 0)
		return (EINVAL);

	error = internal_extattr_check_cred(vp, attrnamespace, name, cred,
	    VREAD);
	if (error)
		return (error);

	attribute = ufs_extattr_find_attr(ump, attrnamespace, name);
	if (!attribute)
		return (ENODATA);

	/*
	 * Allow only offsets of zero to encourage the read/replace
	 * extended attribute semantic.  Otherwise we can't guarantee
	 * atomicity, as we don't provide locks for extended attributes.
	 */
	if (uio != NULL && uio->uio_offset != 0)
		return (ENXIO);

	/*
	 * Don't need to get a lock on the backing file if the getattr is
	 * being applied to the backing file, as the lock is already held.
	 */
	if (attribute->uele_backing_vnode != vp)
		vn_lock(attribute->uele_backing_vnode, LK_SHARED | LK_RETRY);

	error = ufs_extattr_get_header(vp, attribute, &ueh, &base_offset);
	if (error)
		goto vopunlock_exit;

	/* Return full data size if caller requested it. */
	if (size != NULL)
		*size = ueh.ueh_len;

	/* Return data if the caller requested it. */
	if (uio != NULL) {
		/* Allow for offset into the attribute data. */
		uio->uio_offset = base_offset + sizeof(struct
		    ufs_extattr_header);

		/*
		 * Figure out maximum to transfer -- use buffer size and
		 * local data limit.
		 */
		len = MIN(uio->uio_resid, ueh.ueh_len);
		old_len = uio->uio_resid;
		uio->uio_resid = len;

		error = VOP_READ(attribute->uele_backing_vnode, uio,
		    IO_NODELOCKED, ump->um_extattr.uepm_ucred);
		if (error)
			goto vopunlock_exit;

		uio->uio_resid = old_len - (len - uio->uio_resid);
	}

 vopunlock_exit:

	if (uio != NULL)
		uio->uio_offset = 0;

	if (attribute->uele_backing_vnode != vp)
		VOP_UNLOCK(attribute->uele_backing_vnode);

	return (error);
}

/*
 * Vnode operation to list extended attribute for a vnode
 */
int
ufs_listextattr(struct vop_listextattr_args *ap)
/*
vop_listextattr {
	IN struct vnode *a_vp;
	IN int a_attrnamespace;
	INOUT struct uio *a_uio;
	OUT size_t *a_size;
	IN int flag;
	IN kauth_cred_t a_cred;
	struct proc *a_p;
};
*/
{
	struct mount *mp = ap->a_vp->v_mount;
	struct ufsmount *ump = VFSTOUFS(mp);
	int error;

	if (!(ump->um_extattr.uepm_flags & UFS_EXTATTR_UEPM_STARTED))
		return (EOPNOTSUPP);

	ufs_extattr_uepm_lock(ump);

	error = ufs_extattr_list(ap->a_vp, ap->a_attrnamespace,
	    ap->a_uio, ap->a_size, ap->a_flag, ap->a_cred, curlwp);

	ufs_extattr_uepm_unlock(ump);

	return (error);
}

/*
 * Real work associated with retrieving list of attributes--assumes that
 * the attribute lock has already been grabbed.
 */
static int
ufs_extattr_list(struct vnode *vp, int attrnamespace,
    struct uio *uio, size_t *size, int flag, 
    kauth_cred_t cred, struct lwp *l)
{
	struct ufs_extattr_list_entry *uele;
	struct ufs_extattr_header ueh;
	struct mount *mp = vp->v_mount;
	struct ufsmount *ump = VFSTOUFS(mp);
	size_t listsize = 0;
	int error = 0;

	/*
	 * XXX: We can move this inside the loop and iterate on individual
	 *	attributes.
	 */
	error = internal_extattr_check_cred(vp, attrnamespace, "", cred,
	    VREAD);
	if (error)
		return (error);

	LIST_FOREACH(uele, &ump->um_extattr.uepm_list, uele_entries) {
		unsigned char attrnamelen;

		if (uele->uele_attrnamespace != attrnamespace)
			continue;

		error = ufs_extattr_get_header(vp, uele, &ueh, NULL);
		if (error == ENODATA)
			continue;	
		if (error != 0)
			return error;

		/*
		 * Don't need to get a lock on the backing file if 
		 * the listattr is being applied to the backing file, 
		 * as the lock is already held.
		 */
		if (uele->uele_backing_vnode != vp)
			vn_lock(uele->uele_backing_vnode, LK_SHARED | LK_RETRY);

		/*
		 * +1 for trailing NUL (listxattr flavor)
		 *  or leading name length (extattr_list_file flavor)
	 	 */
		attrnamelen = strlen(uele->uele_attrname);
		listsize += attrnamelen + 1;

		/* Return data if the caller requested it. */
		if (uio != NULL) {
			/*
			 * We support two flavors. Either NUL-terminated
			 * strings (a la listxattr), or non NUL-terminated,
			 * one byte length prefixed strings (for
			 * extattr_list_file). EXTATTR_LIST_LENPREFIX switches
		 	 * that second behavior.
			 */
			if (flag & EXTATTR_LIST_LENPREFIX) {
				uint8_t len = (uint8_t)attrnamelen;

				/* Copy leading name length */
				error = uiomove(&len, sizeof(len), uio);
				if (error != 0)
					break;	
			} else {
				/* Include trailing NULL */
				attrnamelen++; 
			}

			error = uiomove(uele->uele_attrname, 
					(size_t)attrnamelen, uio);
			if (error != 0)
				break;	
		}

		if (uele->uele_backing_vnode != vp)
			VOP_UNLOCK(uele->uele_backing_vnode);

		if (error != 0)
			return error;
	}

	if (uio != NULL)
		uio->uio_offset = 0;

	/* Return full data size if caller requested it. */
	if (size != NULL)
		*size = listsize;

	return 0;
}

/*
 * Vnode operation to remove a named attribute.
 */
int
ufs_deleteextattr(struct vop_deleteextattr_args *ap)
/*
vop_deleteextattr {
	IN struct vnode *a_vp;
	IN int a_attrnamespace;
	IN const char *a_name;
	IN kauth_cred_t a_cred;
};
*/
{
	struct mount *mp = ap->a_vp->v_mount;
	struct ufsmount *ump = VFSTOUFS(mp); 
	int error;

	if (!(ump->um_extattr.uepm_flags & UFS_EXTATTR_UEPM_STARTED))
		return (EOPNOTSUPP);

	ufs_extattr_uepm_lock(ump);

	error = ufs_extattr_rm(ap->a_vp, ap->a_attrnamespace, ap->a_name,
	    ap->a_cred, curlwp);

	ufs_extattr_uepm_unlock(ump);

	return (error);
}

/*
 * Vnode operation to set a named attribute.
 */
int
ufs_setextattr(struct vop_setextattr_args *ap)
/*
vop_setextattr {
	IN struct vnode *a_vp;
	IN int a_attrnamespace;
	IN const char *a_name;
	INOUT struct uio *a_uio;
	IN kauth_cred_t a_cred;
};
*/
{
	struct mount *mp = ap->a_vp->v_mount;
	struct ufsmount *ump = VFSTOUFS(mp); 
	int error;

	if (!(ump->um_extattr.uepm_flags & UFS_EXTATTR_UEPM_STARTED))
		return (EOPNOTSUPP);

	ufs_extattr_uepm_lock(ump);

	/*
	 * XXX: No longer a supported way to delete extended attributes.
	 */
	if (ap->a_uio == NULL) {
		ufs_extattr_uepm_unlock(ump);
		return (EINVAL);
	}

	error = ufs_extattr_set(ap->a_vp, ap->a_attrnamespace, ap->a_name,
	    ap->a_uio, ap->a_cred, curlwp);

	ufs_extattr_uepm_unlock(ump);

	return (error);
}

/*
 * Real work associated with setting a vnode's extended attributes;
 * assumes that the attribute lock has already been grabbed.
 */
static int
ufs_extattr_set(struct vnode *vp, int attrnamespace, const char *name,
    struct uio *uio, kauth_cred_t cred, struct lwp *l)
{
	struct ufs_extattr_list_entry *attribute;
	struct ufs_extattr_header ueh;
	struct iovec local_aiov;
	struct uio local_aio;
	struct mount *mp = vp->v_mount;
	struct ufsmount *ump = VFSTOUFS(mp);
	struct inode *ip = VTOI(vp);
	off_t base_offset;
	int error = 0, ioflag;

	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return (EROFS);

	if (!ufs_extattr_valid_attrname(attrnamespace, name))
		return (EINVAL);

	error = internal_extattr_check_cred(vp, attrnamespace, name, cred,
	    VWRITE);
	if (error)
		return (error);

	attribute = ufs_extattr_find_attr(ump, attrnamespace, name);
	if (!attribute) {
		error = ufs_extattr_autocreate_attr(vp, attrnamespace, 
						    name, l, &attribute);
		if (error == EEXIST) {
			/* Another thread raced us for backend creation */
			error = 0;
			attribute = 
			    ufs_extattr_find_attr(ump, attrnamespace, name);
		}

		if (error || !attribute)
			return ENODATA;
	}

	/*
	 * Early rejection of invalid offsets/length.
	 * Reject: any offset but 0 (replace)
	 *	 Any size greater than attribute size limit
 	 */
	if (uio->uio_offset != 0 ||
	    uio->uio_resid > attribute->uele_fileheader.uef_size)
		return (ENXIO);

	/*
	 * Find base offset of header in file based on file header size, and
	 * data header size + maximum data size, indexed by inode number.
	 */
	base_offset = sizeof(struct ufs_extattr_fileheader) +
	    ip->i_number * (sizeof(struct ufs_extattr_header) +
	    attribute->uele_fileheader.uef_size);

	/*
	 * Write out a data header for the data.
	 */
	ueh.ueh_len = ufs_rw32((uint32_t) uio->uio_resid,
	    UELE_NEEDSWAP(attribute));
	ueh.ueh_flags = ufs_rw32(UFS_EXTATTR_ATTR_FLAG_INUSE,
				 UELE_NEEDSWAP(attribute));
	ueh.ueh_i_gen = ufs_rw32(ip->i_gen, UELE_NEEDSWAP(attribute));
	local_aiov.iov_base = &ueh;
	local_aiov.iov_len = sizeof(struct ufs_extattr_header);
	local_aio.uio_iov = &local_aiov;
	local_aio.uio_iovcnt = 1;
	local_aio.uio_rw = UIO_WRITE;
	local_aio.uio_offset = base_offset;
	local_aio.uio_resid = sizeof(struct ufs_extattr_header);
	UIO_SETUP_SYSSPACE(&local_aio);

	/*
	 * Don't need to get a lock on the backing file if the setattr is
	 * being applied to the backing file, as the lock is already held.
	 */
	if (attribute->uele_backing_vnode != vp)
		vn_lock(attribute->uele_backing_vnode, 
		    LK_EXCLUSIVE | LK_RETRY);

	ioflag = IO_NODELOCKED;
	if (ufs_extattr_sync)
		ioflag |= IO_SYNC;
	error = VOP_WRITE(attribute->uele_backing_vnode, &local_aio, ioflag,
	    ump->um_extattr.uepm_ucred);
	if (error)
		goto vopunlock_exit;

	if (local_aio.uio_resid != 0) {
		error = ENXIO;
		goto vopunlock_exit;
	}

	/*
	 * Write out user data.
	 * XXX NOT ATOMIC WITH RESPECT TO THE HEADER.
	 */
	uio->uio_offset = base_offset + sizeof(struct ufs_extattr_header);

	ioflag = IO_NODELOCKED;
	if (ufs_extattr_sync)
		ioflag |= IO_SYNC;
	error = VOP_WRITE(attribute->uele_backing_vnode, uio, ioflag,
	    ump->um_extattr.uepm_ucred);

 vopunlock_exit:
	uio->uio_offset = 0;

	if (attribute->uele_backing_vnode != vp)
		VOP_UNLOCK(attribute->uele_backing_vnode);

	return (error);
}

/*
 * Real work associated with removing an extended attribute from a vnode.
 * Assumes the attribute lock has already been grabbed.
 */
static int
ufs_extattr_rm(struct vnode *vp, int attrnamespace, const char *name,
    kauth_cred_t cred, struct lwp *l)
{
	struct ufs_extattr_list_entry *attribute;
	struct ufs_extattr_header ueh;
	struct mount *mp = vp->v_mount;
	struct ufsmount *ump = VFSTOUFS(mp);
	struct iovec local_aiov;
	struct uio local_aio;
	off_t base_offset;
	int error = 0, ioflag;

	if (vp->v_mount->mnt_flag & MNT_RDONLY)  
		return (EROFS);

	if (!ufs_extattr_valid_attrname(attrnamespace, name))
		return (EINVAL);

	error = internal_extattr_check_cred(vp, attrnamespace, name, cred,
	    VWRITE);
	if (error)
		return (error);

	attribute = ufs_extattr_find_attr(ump, attrnamespace, name);
	if (!attribute)
		return (ENODATA);

	/*
	 * Don't need to get a lock on the backing file if the getattr is
	 * being applied to the backing file, as the lock is already held.
	 */
	if (attribute->uele_backing_vnode != vp)
		vn_lock(attribute->uele_backing_vnode, LK_EXCLUSIVE | LK_RETRY);

	error = ufs_extattr_get_header(vp, attribute, &ueh, &base_offset);
	if (error)
		goto vopunlock_exit;

	/* Flag it as not in use. */
	ueh.ueh_flags = 0;		/* No need to byte swap 0 */
	ueh.ueh_len = 0;		/* ...ditto... */

	local_aiov.iov_base = &ueh;
	local_aiov.iov_len = sizeof(struct ufs_extattr_header);
	local_aio.uio_iov = &local_aiov;
	local_aio.uio_iovcnt = 1;
	local_aio.uio_rw = UIO_WRITE;
	local_aio.uio_offset = base_offset;
	local_aio.uio_resid = sizeof(struct ufs_extattr_header);
	UIO_SETUP_SYSSPACE(&local_aio);

	ioflag = IO_NODELOCKED;
	if (ufs_extattr_sync)
		ioflag |= IO_SYNC;
	error = VOP_WRITE(attribute->uele_backing_vnode, &local_aio, ioflag,
	    ump->um_extattr.uepm_ucred);
	if (error)
		goto vopunlock_exit;

	if (local_aio.uio_resid != 0)
		error = ENXIO;

 vopunlock_exit:
	VOP_UNLOCK(attribute->uele_backing_vnode);

	return (error);
}

/*
 * Called by UFS when an inode is no longer active and should have its
 * attributes stripped.
 */
void
ufs_extattr_vnode_inactive(struct vnode *vp, struct lwp *l)
{
	struct ufs_extattr_list_entry *uele;
	struct mount *mp = vp->v_mount;
	struct ufsmount *ump = VFSTOUFS(mp);

	/*
	 * In that case, we cannot lock. We should not have any active vnodes
	 * on the fs if this is not yet initialized but is going to be, so
	 * this can go unlocked.
	 */
	if (!(ump->um_extattr.uepm_flags & UFS_EXTATTR_UEPM_INITIALIZED))
		return;

	if (!(ump->um_extattr.uepm_flags & UFS_EXTATTR_UEPM_STARTED))
		return;

	ufs_extattr_uepm_lock(ump);

	LIST_FOREACH(uele, &ump->um_extattr.uepm_list, uele_entries)
		ufs_extattr_rm(vp, uele->uele_attrnamespace,
		    uele->uele_attrname, lwp0.l_cred, l);

	ufs_extattr_uepm_unlock(ump);
}

void
ufs_extattr_init(void)
{

}

void
ufs_extattr_done(void)
{

}

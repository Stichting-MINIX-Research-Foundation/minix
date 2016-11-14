/*	$NetBSD: vfs_xattr.c,v 1.33 2014/09/05 09:20:59 matt Exp $	*/

/*-
 * Copyright (c) 2005, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 */

/*
 * VFS extended attribute support.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vfs_xattr.c,v 1.33 2014/09/05 09:20:59 matt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/extattr.h>
#include <sys/xattr.h>
#include <sys/sysctl.h>
#include <sys/syscallargs.h>
#include <sys/kauth.h>
#include <sys/ktrace.h>

#include <miscfs/genfs/genfs.h>

/*
 * Credential check based on process requesting service, and per-attribute
 * permissions.
 *
 * NOTE: Vnode must be locked.
 */
int
extattr_check_cred(struct vnode *vp, const char *attr, kauth_cred_t cred,
    int access)
{

	if (cred == NOCRED)
		return (0);

	return kauth_authorize_vnode(cred, kauth_extattr_action(access), vp,
	    NULL, genfs_can_extattr(cred, access, vp, attr));
}

/*
 * Default vfs_extattrctl routine for file systems that do not support
 * it.
 */
/*ARGSUSED*/
int
vfs_stdextattrctl(struct mount *mp, int cmt, struct vnode *vp,
    int attrnamespace, const char *attrname)
{

	if (vp != NULL)
		VOP_UNLOCK(vp);
	return (EOPNOTSUPP);
}

/*
 * Push extended attribute configuration information into the file
 * system.
 *
 * NOTE: Not all file systems that support extended attributes will
 * require the use of this system call.
 */
int
sys_extattrctl(struct lwp *l, const struct sys_extattrctl_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(int) cmd;
		syscallarg(const char *) filename;
		syscallarg(int) attrnamespace;
		syscallarg(const char *) attrname;
	} */
	struct vnode *path_vp, *file_vp;
	struct pathbuf *file_pb;
	struct nameidata file_nd;
	char attrname[EXTATTR_MAXNAMELEN];
	int error;

	if (SCARG(uap, attrname) != NULL) {
		error = copyinstr(SCARG(uap, attrname), attrname,
		    sizeof(attrname), NULL);
		if (error)
			return (error);
	}

	error = namei_simple_user(SCARG(uap, path),
				NSM_FOLLOW_NOEMULROOT, &path_vp);
	if (error) {
		return (error);
	}

	file_vp = NULL;
	if (SCARG(uap, filename) != NULL) {
		error = pathbuf_copyin(SCARG(uap, filename), &file_pb);
		if (error) {
			vrele(path_vp);
			return (error);
		}
		NDINIT(&file_nd, LOOKUP, FOLLOW | LOCKLEAF, file_pb);
		error = namei(&file_nd);
		if (error) {
			pathbuf_destroy(file_pb);
			vrele(path_vp);
			return (error);
		}
		file_vp = file_nd.ni_vp;
		pathbuf_destroy(file_pb);
	}

	error = VFS_EXTATTRCTL(path_vp->v_mount, SCARG(uap, cmd), file_vp,
	    SCARG(uap, attrnamespace),
	    SCARG(uap, attrname) != NULL ? attrname : NULL);

	if (file_vp != NULL)
		vrele(file_vp);
	vrele(path_vp);

	return (error);
}

/*****************************************************************************
 * Internal routines to manipulate file system extended attributes:
 *	- set
 *	- get
 *	- delete
 *	- list
 *****************************************************************************/

/*
 * extattr_set_vp:
 *
 *	Set a named extended attribute on a file or directory.
 */
static int
extattr_set_vp(struct vnode *vp, int attrnamespace, const char *attrname,
    const void *data, size_t nbytes, struct lwp *l, register_t *retval,
    int flag)
{
	struct uio auio;
	struct iovec aiov;
	ssize_t cnt;
	int error;

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);

	if (flag) {
		size_t attrlen;

		error = VOP_GETEXTATTR(vp, attrnamespace, attrname, NULL,
				       &attrlen, l->l_cred);

		switch (error) {
		case ENODATA:
			if (flag & XATTR_REPLACE)
				goto done;
			break;
		case 0:
			if (flag & XATTR_CREATE) {
				error = EEXIST;
				goto done;
			}
			break;
		default:
			goto done;
			break;
		}
	}

	aiov.iov_base = __UNCONST(data);	/* XXXUNCONST kills const */
	aiov.iov_len = nbytes;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = 0;
	if (nbytes > INT_MAX) {
		error = EINVAL;
		goto done;
	}
	auio.uio_resid = nbytes;
	auio.uio_rw = UIO_WRITE;
	KASSERT(l == curlwp);
	auio.uio_vmspace = l->l_proc->p_vmspace;
	cnt = nbytes;

	ktrkuser("xattr-name", (void *)__UNCONST(attrname), strlen(attrname));
	ktrkuser("xattr-val", __UNCONST(data), nbytes);

	error = VOP_SETEXTATTR(vp, attrnamespace, attrname, &auio, l->l_cred);
	cnt -= auio.uio_resid;
	retval[0] = cnt;

 done:
	VOP_UNLOCK(vp);
	return (error);
}

/*
 * extattr_get_vp:
 *
 *	Get a named extended attribute on a file or directory.
 */
static int
extattr_get_vp(struct vnode *vp, int attrnamespace, const char *attrname,
    void *data, size_t nbytes, struct lwp *l, register_t *retval)
{
	struct uio auio, *auiop;
	struct iovec aiov;
	ssize_t cnt;
	size_t size, *sizep;
	int error;

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);

	/*
	 * Slightly unusual semantics: if the user provides a NULL data
	 * pointer, they don't want to receive the data, just the maximum
	 * read length.
	 */
	auiop = NULL;
	sizep = NULL;
	cnt = 0;
	if (data != NULL) {
		aiov.iov_base = data;
		aiov.iov_len = nbytes;
		auio.uio_iov = &aiov;
		auio.uio_offset = 0;
		if (nbytes > INT_MAX) {
			error = EINVAL;
			goto done;
		}
		auio.uio_resid = nbytes;
		auio.uio_rw = UIO_READ;
		KASSERT(l == curlwp);
		auio.uio_vmspace = l->l_proc->p_vmspace;
		auiop = &auio;
		cnt = nbytes;
	} else
		sizep = &size;

	ktrkuser("xattr-name", (void *)__UNCONST(attrname), strlen(attrname));

	error = VOP_GETEXTATTR(vp, attrnamespace, attrname, auiop, sizep,
	    l->l_cred);

	if (auiop != NULL) {
		cnt -= auio.uio_resid;
		retval[0] = cnt;

		ktrkuser("xattr-val", data, cnt);
	} else
		retval[0] = size;

 done:
	VOP_UNLOCK(vp);
	return (error);
}

/*
 * extattr_delete_vp:
 *
 *	Delete a named extended attribute on a file or directory.
 */
static int
extattr_delete_vp(struct vnode *vp, int attrnamespace, const char *attrname,
    struct lwp *l)
{
	int error;

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);

	ktrkuser("xattr-name", (void *)__UNCONST(attrname), strlen(attrname));

	error = VOP_DELETEEXTATTR(vp, attrnamespace, attrname, l->l_cred);
	if (error == EOPNOTSUPP)
		error = VOP_SETEXTATTR(vp, attrnamespace, attrname, NULL,
		    l->l_cred);

	VOP_UNLOCK(vp);
	return (error);
}

/*
 * extattr_list_vp:
 *
 *	Retrieve a list of extended attributes on a file or directory.
 */
static int
extattr_list_vp(struct vnode *vp, int attrnamespace, void *data, size_t nbytes,
    int flag, struct lwp *l, register_t *retval)
{
	struct uio auio, *auiop;
	size_t size, *sizep;
	struct iovec aiov;
	ssize_t cnt;
	int error;

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);

	auiop = NULL;
	sizep = NULL;
	cnt = 0;
	if (data != NULL) {
		aiov.iov_base = data;
		aiov.iov_len = nbytes;
		auio.uio_iov = &aiov;
		auio.uio_offset = 0;
		if (nbytes > INT_MAX) {
			error = EINVAL;
			goto done;
		}
		auio.uio_resid = nbytes;
		auio.uio_rw = UIO_READ;
		KASSERT(l == curlwp);
		auio.uio_vmspace = l->l_proc->p_vmspace;
		auiop = &auio;
		cnt = nbytes;
	} else
		sizep = &size;

	error = VOP_LISTEXTATTR(vp, attrnamespace, auiop, sizep, 
				flag, l->l_cred);

	if (auiop != NULL) {
		cnt -= auio.uio_resid;
		retval[0] = cnt;

		ktrkuser("xattr-list", data, cnt);
	} else
		retval[0] = size;

 done:
	VOP_UNLOCK(vp);
	return (error);
}

/*****************************************************************************
 * BSD <sys/extattr.h> API for file system extended attributes
 *****************************************************************************/

int
sys_extattr_set_fd(struct lwp *l, const struct sys_extattr_set_fd_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(int) attrnamespace;
		syscallarg(const char *) attrname;
		syscallarg(const void *) data;
		syscallarg(size_t) nbytes;
	} */
	struct file *fp;
	struct vnode *vp;
	char attrname[EXTATTR_MAXNAMELEN];
	int error;

	error = copyinstr(SCARG(uap, attrname), attrname, sizeof(attrname),
	    NULL);
	if (error)
		return (error);

	error = fd_getvnode(SCARG(uap, fd), &fp);
	if (error)
		return (error);
	vp = fp->f_vnode;

	error = extattr_set_vp(vp, SCARG(uap, attrnamespace), attrname,
	    SCARG(uap, data), SCARG(uap, nbytes), l, retval, 0);

	fd_putfile(SCARG(uap, fd));
	return (error);
}

int
sys_extattr_set_file(struct lwp *l, const struct sys_extattr_set_file_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(int) attrnamespace;
		syscallarg(const char *) attrname;
		syscallarg(const void *) data;
		syscallarg(size_t) nbytes;
	} */
	struct vnode *vp;
	char attrname[EXTATTR_MAXNAMELEN];
	int error;

	error = copyinstr(SCARG(uap, attrname), attrname, sizeof(attrname),
	    NULL);
	if (error)
		return (error);

	error = namei_simple_user(SCARG(uap, path),
				NSM_FOLLOW_NOEMULROOT, &vp);
	if (error)
		return (error);

	error = extattr_set_vp(vp, SCARG(uap, attrnamespace), attrname,
	    SCARG(uap, data), SCARG(uap, nbytes), l, retval, 0);

	vrele(vp);
	return (error);
}

int
sys_extattr_set_link(struct lwp *l, const struct sys_extattr_set_link_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(int) attrnamespace;
		syscallarg(const char *) attrname;
		syscallarg(const void *) data;
		syscallarg(size_t) nbytes;
	} */
	struct vnode *vp;
	char attrname[EXTATTR_MAXNAMELEN];
	int error;

	error = copyinstr(SCARG(uap, attrname), attrname, sizeof(attrname),
	    NULL);
	if (error)
		return (error);

	error = namei_simple_user(SCARG(uap, path),
				NSM_NOFOLLOW_NOEMULROOT, &vp);
	if (error)
		return (error);

	error = extattr_set_vp(vp, SCARG(uap, attrnamespace), attrname,
	    SCARG(uap, data), SCARG(uap, nbytes), l, retval, 0);

	vrele(vp);
	return (error);
}

int
sys_extattr_get_fd(struct lwp *l, const struct sys_extattr_get_fd_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(int) attrnamespace;
		syscallarg(const char *) attrname;
		syscallarg(void *) data;
		syscallarg(size_t) nbytes;
	} */
	struct file *fp;
	struct vnode *vp;
	char attrname[EXTATTR_MAXNAMELEN];
	int error;

	error = copyinstr(SCARG(uap, attrname), attrname, sizeof(attrname),
	    NULL);
	if (error)
		return (error);

	error = fd_getvnode(SCARG(uap, fd), &fp);
	if (error)
		return (error);
	vp = fp->f_vnode;

	error = extattr_get_vp(vp, SCARG(uap, attrnamespace), attrname,
	    SCARG(uap, data), SCARG(uap, nbytes), l, retval);

	fd_putfile(SCARG(uap, fd));
	return (error);
}

int
sys_extattr_get_file(struct lwp *l, const struct sys_extattr_get_file_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(int) attrnamespace;
		syscallarg(const char *) attrname;
		syscallarg(void *) data;
		syscallarg(size_t) nbytes;
	} */
	struct vnode *vp;
	char attrname[EXTATTR_MAXNAMELEN];
	int error;

	error = copyinstr(SCARG(uap, attrname), attrname, sizeof(attrname),
	    NULL);
	if (error)
		return (error);

	error = namei_simple_user(SCARG(uap, path),
				NSM_FOLLOW_NOEMULROOT, &vp);
	if (error)
		return (error);

	error = extattr_get_vp(vp, SCARG(uap, attrnamespace), attrname,
	    SCARG(uap, data), SCARG(uap, nbytes), l, retval);

	vrele(vp);
	return (error);
}

int
sys_extattr_get_link(struct lwp *l, const struct sys_extattr_get_link_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(int) attrnamespace;
		syscallarg(const char *) attrname;
		syscallarg(void *) data;
		syscallarg(size_t) nbytes;
	} */
	struct vnode *vp;
	char attrname[EXTATTR_MAXNAMELEN];
	int error;

	error = copyinstr(SCARG(uap, attrname), attrname, sizeof(attrname),
	    NULL);
	if (error)
		return (error);

	error = namei_simple_user(SCARG(uap, path),
				NSM_NOFOLLOW_NOEMULROOT, &vp);
	if (error)
		return (error);

	error = extattr_get_vp(vp, SCARG(uap, attrnamespace), attrname,
	    SCARG(uap, data), SCARG(uap, nbytes), l, retval);

	vrele(vp);
	return (error);
}

int
sys_extattr_delete_fd(struct lwp *l, const struct sys_extattr_delete_fd_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(int) attrnamespace;
		syscallarg(const char *) attrname;
	} */
	struct file *fp;
	struct vnode *vp;
	char attrname[EXTATTR_MAXNAMELEN];
	int error;

	error = copyinstr(SCARG(uap, attrname), attrname, sizeof(attrname),
	    NULL);
	if (error)
		return (error);

	error = fd_getvnode(SCARG(uap, fd), &fp);
	if (error)
		return (error);
	vp = fp->f_vnode;

	error = extattr_delete_vp(vp, SCARG(uap, attrnamespace), attrname, l);

	fd_putfile(SCARG(uap, fd));
	return (error);
}

int
sys_extattr_delete_file(struct lwp *l, const struct sys_extattr_delete_file_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(int) attrnamespace;
		syscallarg(const char *) attrname;
	} */
	struct vnode *vp;
	char attrname[EXTATTR_MAXNAMELEN];
	int error;

	error = copyinstr(SCARG(uap, attrname), attrname, sizeof(attrname),
	    NULL);
	if (error)
		return (error);

	error = namei_simple_user(SCARG(uap, path),
				NSM_FOLLOW_NOEMULROOT, &vp);
	if (error)
		return (error);

	error = extattr_delete_vp(vp, SCARG(uap, attrnamespace), attrname, l);

	vrele(vp);
	return (error);
}

int
sys_extattr_delete_link(struct lwp *l, const struct sys_extattr_delete_link_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(int) attrnamespace;
		syscallarg(const char *) attrname;
	} */
	struct vnode *vp;
	char attrname[EXTATTR_MAXNAMELEN];
	int error;

	error = copyinstr(SCARG(uap, attrname), attrname, sizeof(attrname),
	    NULL);
	if (error)
		return (error);

	error = namei_simple_user(SCARG(uap, path),
				NSM_NOFOLLOW_NOEMULROOT, &vp);
	if (error)
		return (error);

	error = extattr_delete_vp(vp, SCARG(uap, attrnamespace), attrname, l);

	vrele(vp);
	return (error);
}

int
sys_extattr_list_fd(struct lwp *l, const struct sys_extattr_list_fd_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(int) attrnamespace;
		syscallarg(void *) data;
		syscallarg(size_t) nbytes;
	} */
	struct file *fp;
	struct vnode *vp;
	int error;

	error = fd_getvnode(SCARG(uap, fd), &fp);
	if (error)
		return (error);
	vp = fp->f_vnode;

	error = extattr_list_vp(vp, SCARG(uap, attrnamespace),
	    SCARG(uap, data), SCARG(uap, nbytes),
	    EXTATTR_LIST_LENPREFIX, l, retval);

	fd_putfile(SCARG(uap, fd));
	return (error);
}

int
sys_extattr_list_file(struct lwp *l, const struct sys_extattr_list_file_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(int) attrnamespace;
		syscallarg(void *) data;
		syscallarg(size_t) nbytes;
	} */
	struct vnode *vp;
	int error;

	error = namei_simple_user(SCARG(uap, path),
				NSM_FOLLOW_NOEMULROOT, &vp);
	if (error)
		return (error);

	error = extattr_list_vp(vp, SCARG(uap, attrnamespace),
	    SCARG(uap, data), SCARG(uap, nbytes),
	    EXTATTR_LIST_LENPREFIX, l, retval);

	vrele(vp);
	return (error);
}

int
sys_extattr_list_link(struct lwp *l, const struct sys_extattr_list_link_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(int) attrnamespace;
		syscallarg(void *) data;
		syscallarg(size_t) nbytes;
	} */
	struct vnode *vp;
	int error;

	error = namei_simple_user(SCARG(uap, path),
				NSM_NOFOLLOW_NOEMULROOT, &vp);
	if (error)
		return (error);

	error = extattr_list_vp(vp, SCARG(uap, attrnamespace),
	    SCARG(uap, data), SCARG(uap, nbytes),
	    EXTATTR_LIST_LENPREFIX, l, retval);

	vrele(vp);
	return (error);
}

/*****************************************************************************
 * Linux-compatible <sys/xattr.h> API for file system extended attributes
 *****************************************************************************/

#define MATCH_NS(ns, key) (strncmp(ns, key, sizeof(ns) - 1) == 0)
static int
xattr_native(const char *key) {
	if (MATCH_NS("system.", key))
		return EXTATTR_NAMESPACE_SYSTEM;
	else if (MATCH_NS("user.", key))
		return EXTATTR_NAMESPACE_USER;
	else if (MATCH_NS("security.", key))
		return EXTATTR_NAMESPACE_SYSTEM;
	else if (MATCH_NS("trusted.", key))
		return EXTATTR_NAMESPACE_SYSTEM;
	else 
		return EXTATTR_NAMESPACE_USER;
	
}
#undef MATCH_NS

#define XATTR_ERRNO(e) ((e) == EOPNOTSUPP ? ENOTSUP : (e))

int
sys_setxattr(struct lwp *l, const struct sys_setxattr_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(const char *) name;
		syscallarg(void *) value;
		syscallarg(size_t) size;
		syscallarg(int) flags;
	} */
	struct vnode *vp;
	char attrname[XATTR_NAME_MAX];
	int attrnamespace;
	register_t attrlen;
	int error;

	error = copyinstr(SCARG(uap, name), attrname, sizeof(attrname),
	    NULL);
	if (error)
		goto out;

	error = namei_simple_user(SCARG(uap, path),
				NSM_FOLLOW_NOEMULROOT, &vp);
	if (error)
		goto out;

	attrnamespace = xattr_native(attrname);

	error = extattr_set_vp(vp, attrnamespace,
	    attrname, SCARG(uap, value), SCARG(uap, size), l, 
	    &attrlen, SCARG(uap, flags));

	vrele(vp);
out:
	*retval = (error == 0) ? 0 : -1;
	return (XATTR_ERRNO(error));
}

int
sys_lsetxattr(struct lwp *l, const struct sys_lsetxattr_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(const char *) name;
		syscallarg(void *) value;
		syscallarg(size_t) size;
		syscallarg(int) flags;
	} */
	struct vnode *vp;
	char attrname[XATTR_NAME_MAX];
	int attrnamespace;
	register_t attrlen;
	int error;

	error = copyinstr(SCARG(uap, name), attrname, sizeof(attrname),
	    NULL);
	if (error)
		goto out;

	error = namei_simple_user(SCARG(uap, path),
				NSM_NOFOLLOW_NOEMULROOT, &vp);
	if (error)
		goto out;

	attrnamespace = xattr_native(attrname);

	error = extattr_set_vp(vp, attrnamespace,
	    attrname, SCARG(uap, value), SCARG(uap, size), l,
	    &attrlen, SCARG(uap, flags));

	vrele(vp);
out:
	*retval = (error == 0) ? 0 : -1;
	return (XATTR_ERRNO(error));
}

int
sys_fsetxattr(struct lwp *l, const struct sys_fsetxattr_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(const char *) name;
		syscallarg(void *) value;
		syscallarg(size_t) size;
		syscallarg(int) flags;
	} */
	struct file *fp;
	struct vnode *vp;
	char attrname[XATTR_NAME_MAX];
	int attrnamespace;
	register_t attrlen;
	int error;

	error = copyinstr(SCARG(uap, name), attrname, sizeof(attrname),
	    NULL);
	if (error)
		goto out;

	error = fd_getvnode(SCARG(uap, fd), &fp);
	if (error)
		goto out;
	vp = fp->f_vnode;

	attrnamespace = xattr_native(attrname);

	error = extattr_set_vp(vp, attrnamespace,
	    attrname, SCARG(uap, value), SCARG(uap, size), l,
	    &attrlen, SCARG(uap, flags));

	fd_putfile(SCARG(uap, fd));
out:
	*retval = (error == 0) ? 0 : -1;
	return (XATTR_ERRNO(error));
}

int
sys_getxattr(struct lwp *l, const struct sys_getxattr_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(const char *) name;
		syscallarg(void *) value;
		syscallarg(size_t) size;
	} */
	struct vnode *vp;
	char attrname[XATTR_NAME_MAX];
	int attrnamespace;
	int error;

	error = copyinstr(SCARG(uap, name), attrname, sizeof(attrname),
	    NULL);
	if (error)
		return (error);

	error = namei_simple_user(SCARG(uap, path),
				NSM_FOLLOW_NOEMULROOT, &vp);
	if (error)
		return (error);

	attrnamespace = xattr_native(attrname);

	error = extattr_get_vp(vp, attrnamespace,
	    attrname, SCARG(uap, value), SCARG(uap, size), l, retval);

	vrele(vp);
	return (XATTR_ERRNO(error));
}

int
sys_lgetxattr(struct lwp *l, const struct sys_lgetxattr_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(const char *) name;
		syscallarg(void *) value;
		syscallarg(size_t) size;
	} */
	struct vnode *vp;
	char attrname[XATTR_NAME_MAX];
	int attrnamespace;
	int error;

	error = copyinstr(SCARG(uap, name), attrname, sizeof(attrname),
	    NULL);
	if (error)
		return (error);

	error = namei_simple_user(SCARG(uap, path),
				NSM_NOFOLLOW_NOEMULROOT, &vp);
	if (error)
		return (error);

	attrnamespace = xattr_native(attrname);

	error = extattr_get_vp(vp, attrnamespace,
	    attrname, SCARG(uap, value), SCARG(uap, size), l, retval);

	vrele(vp);
	return (XATTR_ERRNO(error));
}

int
sys_fgetxattr(struct lwp *l, const struct sys_fgetxattr_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(const char *) name;
		syscallarg(void *) value;
		syscallarg(size_t) size;
	} */
	struct file *fp;
	struct vnode *vp;
	char attrname[XATTR_NAME_MAX];
	int attrnamespace;
	int error;

	error = copyinstr(SCARG(uap, name), attrname, sizeof(attrname),
	    NULL);
	if (error)
		return (error);

	error = fd_getvnode(SCARG(uap, fd), &fp);
	if (error)
		return (error);
	vp = fp->f_vnode;

	attrnamespace = xattr_native(attrname);

	error = extattr_get_vp(vp, attrnamespace,
	    attrname, SCARG(uap, value), SCARG(uap, size), l, retval);

	fd_putfile(SCARG(uap, fd));
	return (XATTR_ERRNO(error));
}

int
sys_listxattr(struct lwp *l, const struct sys_listxattr_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(char *) list;
		syscallarg(size_t) size;
	} */
	struct vnode *vp;
	char *list;
	size_t size;
	register_t listsize_usr, listsize_sys;
	int error;

	error = namei_simple_user(SCARG(uap, path),
				NSM_FOLLOW_NOEMULROOT, &vp);
	if (error)
		return (error);

	list = SCARG(uap, list);
	size = SCARG(uap, size);

	error = extattr_list_vp(vp, EXTATTR_NAMESPACE_USER,
	    list, size, 0, l, &listsize_usr);
	if (error)
		goto out;

	if (list)
		list += listsize_usr;
	if (size)
		size -= listsize_usr;

	error = extattr_list_vp(vp, EXTATTR_NAMESPACE_SYSTEM,
	    list, size, 0, l, &listsize_sys);
	switch (error) {
	case EPERM:
		error = 0; /* Ignore and just skip system EA */
		listsize_sys = 0;
		break;
	case 0:
		break;
	default: 
		goto out;
		break;
	}

	*retval = listsize_usr + listsize_sys; 	

out:
	vrele(vp);
	return (XATTR_ERRNO(error));
}

int
sys_llistxattr(struct lwp *l, const struct sys_llistxattr_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(char *) list;
		syscallarg(size_t) size;
	} */
	struct vnode *vp;
	char *list;
	size_t size;
	register_t listsize_usr, listsize_sys;
	int error;

	error = namei_simple_user(SCARG(uap, path),
				NSM_NOFOLLOW_NOEMULROOT, &vp);
	if (error)
		return (error);

	list = SCARG(uap, list);
	size = SCARG(uap, size);

	error = extattr_list_vp(vp, EXTATTR_NAMESPACE_USER,
	    list, size, 0, l, &listsize_usr);
	if (error)
		goto out;
	if (list)
		list += listsize_usr;
	if (size)
		size -= listsize_usr;

	error = extattr_list_vp(vp, EXTATTR_NAMESPACE_SYSTEM,
	    list, size, 0, l, &listsize_sys);
	switch (error) {
	case EPERM:
		error = 0; /* Ignore and just skip system EA */
		listsize_sys = 0;
		break;
	case 0:
		break;
	default: 
		goto out;
		break;
	}

	*retval = listsize_usr + listsize_sys; 	
out:
	vrele(vp);
	return (XATTR_ERRNO(error));
}

int
sys_flistxattr(struct lwp *l, const struct sys_flistxattr_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(char *) list;
		syscallarg(size_t) size;
	} */
	struct file *fp;
	struct vnode *vp;
	char *list;
	size_t size;
	register_t listsize_usr, listsize_sys;
	int error;

	error = fd_getvnode(SCARG(uap, fd), &fp);
	if (error)
		return (error);
	vp = fp->f_vnode;

	list = SCARG(uap, list);
	size = SCARG(uap, size);

	error = extattr_list_vp(vp, EXTATTR_NAMESPACE_USER,
	    list, size, 0, l, &listsize_usr);
	if (error)
		goto out;

	if (list)
		list += listsize_usr;
	if (size)
		size -= listsize_usr;

	error = extattr_list_vp(vp, EXTATTR_NAMESPACE_SYSTEM,
	    list, size, 0, l, &listsize_sys);
	switch (error) {
	case EPERM:
		error = 0; /* Ignore and just skip system EA */
		listsize_sys = 0;
		break;
	case 0:
		break;
	default: 
		goto out;
		break;
	}

	*retval = listsize_usr + listsize_sys; 	
out:

	fd_putfile(SCARG(uap, fd));
	return (XATTR_ERRNO(error));
}

int
sys_removexattr(struct lwp *l, const struct sys_removexattr_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(const char *) name;
	} */
	struct vnode *vp;
	char attrname[XATTR_NAME_MAX];
	int attrnamespace;
	int error;

	error = copyinstr(SCARG(uap, name), attrname, sizeof(attrname),
	    NULL);
	if (error)
		return (error);

	error = namei_simple_user(SCARG(uap, path),
				NSM_FOLLOW_NOEMULROOT, &vp);
	if (error)
		return (error);

	attrnamespace = xattr_native(attrname);

	error = extattr_delete_vp(vp, attrnamespace, attrname, l);

	vrele(vp);
	return (XATTR_ERRNO(error));
}

int
sys_lremovexattr(struct lwp *l, const struct sys_lremovexattr_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(const char *) name;
	} */
	struct vnode *vp;
	char attrname[XATTR_NAME_MAX];
	int attrnamespace;
	int error;

	error = copyinstr(SCARG(uap, name), attrname, sizeof(attrname),
	    NULL);
	if (error)
		return (error);

	error = namei_simple_user(SCARG(uap, path),
				NSM_NOFOLLOW_NOEMULROOT, &vp);
	if (error)
		return (error);

	attrnamespace = xattr_native(attrname);

	error = extattr_delete_vp(vp, attrnamespace, attrname, l);

	vrele(vp);
	return (XATTR_ERRNO(error));
}

int
sys_fremovexattr(struct lwp *l, const struct sys_fremovexattr_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(const char *) name;
	} */
	struct file *fp;
	struct vnode *vp;
	char attrname[XATTR_NAME_MAX];
	int attrnamespace;
	int error;

	error = copyinstr(SCARG(uap, name), attrname, sizeof(attrname),
	    NULL);
	if (error)
		return (error);

	error = fd_getvnode(SCARG(uap, fd), &fp);
	if (error)
		return (error);
	vp = fp->f_vnode;

	attrnamespace = xattr_native(attrname);

	error = extattr_delete_vp(vp, attrnamespace, attrname, l);

	fd_putfile(SCARG(uap, fd));
	return (XATTR_ERRNO(error));
}

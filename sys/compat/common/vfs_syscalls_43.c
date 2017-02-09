/*	$NetBSD: vfs_syscalls_43.c,v 1.57 2014/09/05 09:21:54 matt Exp $	*/

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
 *
 *	@(#)vfs_syscalls.c	8.28 (Berkeley) 12/10/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vfs_syscalls_43.c,v 1.57 2014/09/05 09:21:54 matt Exp $");

#if defined(_KERNEL_OPT)
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/dirent.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/unistd.h>
#include <sys/resourcevar.h>
#include <sys/sysctl.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/vfs_syscalls.h>

#include <compat/sys/stat.h>
#include <compat/sys/mount.h>
#include <compat/sys/dirent.h>

#include <compat/common/compat_util.h>
#include <compat/common/compat_mod.h>

static void cvtstat(struct stat *, struct stat43 *);

/*
 * Convert from an old to a new stat structure.
 */
static void
cvtstat(struct stat *st, struct stat43 *ost)
{

	ost->st_dev = st->st_dev;
	ost->st_ino = st->st_ino;
	ost->st_mode = st->st_mode & 0xffff;
	ost->st_nlink = st->st_nlink;
	ost->st_uid = st->st_uid;
	ost->st_gid = st->st_gid;
	ost->st_rdev = st->st_rdev;
	if (st->st_size < (quad_t)1 << 32)
		ost->st_size = st->st_size;
	else
		ost->st_size = -2;
	ost->st_atime = st->st_atime;
	ost->st_mtime = st->st_mtime;
	ost->st_ctime = st->st_ctime;
	ost->st_blksize = st->st_blksize;
	ost->st_blocks = st->st_blocks;
	ost->st_flags = st->st_flags;
	ost->st_gen = st->st_gen;
}

/*
 * Get file status; this version follows links.
 */
/* ARGSUSED */
int
compat_43_sys_stat(struct lwp *l, const struct compat_43_sys_stat_args *uap, register_t *retval)
{
	/* {
		syscallarg(char *) path;
		syscallarg(struct stat43 *) ub;
	} */
	struct stat sb;
	struct stat43 osb;
	int error;

	error = do_sys_stat(SCARG(uap, path), FOLLOW, &sb);
	if (error)
		return (error);
	cvtstat(&sb, &osb);
	error = copyout((void *)&osb, (void *)SCARG(uap, ub), sizeof (osb));
	return (error);
}

/*
 * Get file status; this version does not follow links.
 */
/* ARGSUSED */
int
compat_43_sys_lstat(struct lwp *l, const struct compat_43_sys_lstat_args *uap, register_t *retval)
{
	/* {
		syscallarg(char *) path;
		syscallarg(struct ostat *) ub;
	} */
	struct vnode *vp, *dvp;
	struct stat sb, sb1;
	struct stat43 osb;
	int error;
	struct pathbuf *pb;
	struct nameidata nd;
	int ndflags;

	error = pathbuf_copyin(SCARG(uap, path), &pb);
	if (error) {
		return error;
	}

	ndflags = NOFOLLOW | LOCKLEAF | LOCKPARENT | TRYEMULROOT;
again:
	NDINIT(&nd, LOOKUP, ndflags, pb);
	if ((error = namei(&nd))) {
		if (error == EISDIR && (ndflags & LOCKPARENT) != 0) {
			/*
			 * Should only happen on '/'. Retry without LOCKPARENT;
			 * this is safe since the vnode won't be a VLNK.
			 */
			ndflags &= ~LOCKPARENT;
			goto again;
		}
		pathbuf_destroy(pb);
		return (error);
	}
	/*
	 * For symbolic links, always return the attributes of its
	 * containing directory, except for mode, size, and links.
	 */
	vp = nd.ni_vp;
	dvp = nd.ni_dvp;
	pathbuf_destroy(pb);
	if (vp->v_type != VLNK) {
		if ((ndflags & LOCKPARENT) != 0) {
			if (dvp == vp)
				vrele(dvp);
			else
				vput(dvp);
		}
		error = vn_stat(vp, &sb);
		vput(vp);
		if (error)
			return (error);
	} else {
		error = vn_stat(dvp, &sb);
		vput(dvp);
		if (error) {
			vput(vp);
			return (error);
		}
		error = vn_stat(vp, &sb1);
		vput(vp);
		if (error)
			return (error);
		sb.st_mode &= ~S_IFDIR;
		sb.st_mode |= S_IFLNK;
		sb.st_nlink = sb1.st_nlink;
		sb.st_size = sb1.st_size;
		sb.st_blocks = sb1.st_blocks;
	}
	cvtstat(&sb, &osb);
	error = copyout((void *)&osb, (void *)SCARG(uap, ub), sizeof (osb));
	return (error);
}

/*
 * Return status information about a file descriptor.
 */
/* ARGSUSED */
int
compat_43_sys_fstat(struct lwp *l, const struct compat_43_sys_fstat_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(struct stat43 *) sb;
	} */
	struct stat ub;
	struct stat43 oub;
	int error;

	error = do_sys_fstat(SCARG(uap, fd), &ub);
	if (error == 0) {
		cvtstat(&ub, &oub);
		error = copyout((void *)&oub, (void *)SCARG(uap, sb),
		    sizeof (oub));
	}

	return (error);
}


/*
 * Truncate a file given a file descriptor.
 */
/* ARGSUSED */
int
compat_43_sys_ftruncate(struct lwp *l, const struct compat_43_sys_ftruncate_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(long) length;
	} */
	struct sys_ftruncate_args /* {
		syscallarg(int) fd;
		syscallarg(int) pad;
		syscallarg(off_t) length;
	} */ nuap;

	SCARG(&nuap, fd) = SCARG(uap, fd);
	SCARG(&nuap, length) = SCARG(uap, length);
	return (sys_ftruncate(l, &nuap, retval));
}

/*
 * Truncate a file given its path name.
 */
/* ARGSUSED */
int
compat_43_sys_truncate(struct lwp *l, const struct compat_43_sys_truncate_args *uap, register_t *retval)
{
	/* {
		syscallarg(char *) path;
		syscallarg(long) length;
	} */
	struct sys_truncate_args /* {
		syscallarg(char *) path;
		syscallarg(int) pad;
		syscallarg(off_t) length;
	} */ nuap;

	SCARG(&nuap, path) = SCARG(uap, path);
	SCARG(&nuap, length) = SCARG(uap, length);
	return (sys_truncate(l, &nuap, retval));
}


/*
 * Reposition read/write file offset.
 */
int
compat_43_sys_lseek(struct lwp *l, const struct compat_43_sys_lseek_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(long) offset;
		syscallarg(int) whence;
	} */
	struct sys_lseek_args /* {
		syscallarg(int) fd;
		syscallarg(int) pad;
		syscallarg(off_t) offset;
		syscallarg(int) whence;
	} */ nuap;
	off_t qret;
	int error;

	SCARG(&nuap, fd) = SCARG(uap, fd);
	SCARG(&nuap, offset) = SCARG(uap, offset);
	SCARG(&nuap, whence) = SCARG(uap, whence);
	error = sys_lseek(l, &nuap, (register_t *)&qret);
	*(long *)retval = qret;
	return (error);
}


/*
 * Create a file.
 */
int
compat_43_sys_creat(struct lwp *l, const struct compat_43_sys_creat_args *uap, register_t *retval)
{
	/* {
		syscallarg(char *) path;
		syscallarg(int) mode;
	} */
	struct sys_open_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
		syscallarg(int) mode;
	} */ nuap;

	SCARG(&nuap, path) = SCARG(uap, path);
	SCARG(&nuap, mode) = SCARG(uap, mode);
	SCARG(&nuap, flags) = O_WRONLY | O_CREAT | O_TRUNC;
	return (sys_open(l, &nuap, retval));
}

/*ARGSUSED*/
int
compat_43_sys_quota(struct lwp *l, const void *v, register_t *retval)
{

	return (ENOSYS);
}


/*
 * Read a block of directory entries in a file system independent format.
 */
int
compat_43_sys_getdirentries(struct lwp *l, const struct compat_43_sys_getdirentries_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(char *) buf;
		syscallarg(u_int) count;
		syscallarg(long *) basep;
	} */
	struct dirent *bdp;
	struct vnode *vp;
	void *tbuf;			/* Current-format */
	char *inp;			/* Current-format */
	int len, reclen;		/* Current-format */
	char *outp;			/* Dirent12-format */
	int resid, old_reclen = 0;	/* Dirent12-format */
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	struct dirent43 idb;
	off_t off;		/* true file offset */
	int buflen, error, eofflag, nbytes;
	struct vattr va;
	off_t *cookiebuf = NULL, *cookie;
	int ncookies;
	long loff;
		 
	/* fd_getvnode() will use the descriptor for us */
	if ((error = fd_getvnode(SCARG(uap, fd), &fp)) != 0)
		return (error);

	if ((fp->f_flag & FREAD) == 0) {
		error = EBADF;
		goto out1;
	}

	vp = fp->f_vnode;
	if (vp->v_type != VDIR) {
		error = ENOTDIR;
		goto out1;
	}

	vn_lock(vp, LK_SHARED | LK_RETRY);
	error = VOP_GETATTR(vp, &va, l->l_cred);
	VOP_UNLOCK(vp);
	if (error)
		goto out1;

	loff = fp->f_offset;
	nbytes = SCARG(uap, count);
	buflen = min(MAXBSIZE, nbytes);
	if (buflen < va.va_blocksize)
		buflen = va.va_blocksize;
	tbuf = malloc(buflen, M_TEMP, M_WAITOK);

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	off = fp->f_offset;
again:
	aiov.iov_base = tbuf;
	aiov.iov_len = buflen;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_resid = buflen;
	auio.uio_offset = off;
	UIO_SETUP_SYSSPACE(&auio);
	/*
         * First we read into the malloc'ed buffer, then
         * we massage it into user space, one record at a time.
         */
	error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag, &cookiebuf,
	    &ncookies);
	if (error)
		goto out;

	inp = (char *)tbuf;
	outp = SCARG(uap, buf);
	resid = nbytes;
	if ((len = buflen - auio.uio_resid) == 0)
		goto eof;

	for (cookie = cookiebuf; len > 0; len -= reclen) {
		bdp = (struct dirent *)inp;
		reclen = bdp->d_reclen;
		if (reclen & 3)
			panic(__func__);
		if (bdp->d_fileno == 0) {
			inp += reclen;	/* it is a hole; squish it out */
			if (cookie)
				off = *cookie++;
			else
				off += reclen;
			continue;
		}
		old_reclen = _DIRENT_RECLEN(&idb, bdp->d_namlen);
		if (reclen > len || resid < old_reclen) {
			/* entry too big for buffer, so just stop */
			outp++;
			break;
		}
		/*
		 * Massage in place to make a Dirent12-shaped dirent (otherwise
		 * we have to worry about touching user memory outside of
		 * the copyout() call).
		 */
		idb.d_fileno = (uint32_t)bdp->d_fileno;
		idb.d_reclen = (uint16_t)old_reclen;
		idb.d_namlen = (uint16_t)bdp->d_namlen;
		strcpy(idb.d_name, bdp->d_name);
		if ((error = copyout(&idb, outp, old_reclen)))
			goto out;
		/* advance past this real entry */
		inp += reclen;
		if (cookie)
			off = *cookie++; /* each entry points to itself */
		else
			off += reclen;
		/* advance output past Dirent12-shaped entry */
		outp += old_reclen;
		resid -= old_reclen;
	}

	/* if we squished out the whole block, try again */
	if (outp == SCARG(uap, buf)) {
		if (cookiebuf)
			free(cookiebuf, M_TEMP);
		cookiebuf = NULL;
		goto again;
	}
	fp->f_offset = off;	/* update the vnode offset */

eof:
	*retval = nbytes - resid;
out:
	VOP_UNLOCK(vp);
	if (cookiebuf)
		free(cookiebuf, M_TEMP);
	free(tbuf, M_TEMP);
out1:
	fd_putfile(SCARG(uap, fd));
	if (error)
		return error;
	return copyout(&loff, SCARG(uap, basep), sizeof(long));
}

/*
 * sysctl helper routine for vfs.generic.conf lookups.
 */
#if defined(COMPAT_09) || defined(COMPAT_43) || defined(COMPAT_44)

static int
sysctl_vfs_generic_conf(SYSCTLFN_ARGS)
{
        struct vfsconf vfc;
        extern const char * const mountcompatnames[];
        extern int nmountcompatnames;
	struct sysctlnode node;
	struct vfsops *vfsp;
	u_int vfsnum;

	if (namelen != 1)
		return (ENOTDIR);
	vfsnum = name[0];
	if (vfsnum >= nmountcompatnames ||
	    mountcompatnames[vfsnum] == NULL)
		return (EOPNOTSUPP);
	vfsp = vfs_getopsbyname(mountcompatnames[vfsnum]);
	if (vfsp == NULL)
		return (EOPNOTSUPP);

	vfc.vfc_vfsops = vfsp;
	strncpy(vfc.vfc_name, vfsp->vfs_name, sizeof(vfc.vfc_name));
	vfc.vfc_typenum = vfsnum;
	vfc.vfc_refcount = vfsp->vfs_refcount;
	vfc.vfc_flags = 0;
	vfc.vfc_mountroot = vfsp->vfs_mountroot;
	vfc.vfc_next = NULL;
	vfs_delref(vfsp);

	node = *rnode;
	node.sysctl_data = &vfc;
	return (sysctl_lookup(SYSCTLFN_CALL(&node)));
}

/*
 * Top level filesystem related information gathering.
 */
void
compat_sysctl_vfs(struct sysctllog **clog)
{
	extern int nmountcompatnames;

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_IMMEDIATE,
		       CTLTYPE_INT, "maxtypenum",
		       SYSCTL_DESCR("Highest valid filesystem type number"),
		       NULL, nmountcompatnames, NULL, 0,
		       CTL_VFS, VFS_GENERIC, VFS_MAXTYPENUM, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "conf",
		       SYSCTL_DESCR("Filesystem configuration information"),
		       sysctl_vfs_generic_conf, 0, NULL,
		       sizeof(struct vfsconf),
		       CTL_VFS, VFS_GENERIC, VFS_CONF, CTL_EOL);
}
#endif

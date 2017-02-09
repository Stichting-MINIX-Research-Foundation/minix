/*	$NetBSD: vfs_syscalls_12.c,v 1.31 2014/09/05 09:21:54 matt Exp $	*/

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
 *	From: @(#)vfs_syscalls.c	8.28 (Berkeley) 12/10/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vfs_syscalls_12.c,v 1.31 2014/09/05 09:21:54 matt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/socketvar.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/dirent.h>
#include <sys/vfs_syscalls.h>

#include <sys/syscallargs.h>

#include <compat/sys/stat.h>
#include <compat/sys/dirent.h>

/*
 * Convert from a new to an old stat structure.
 */
void
compat_12_stat_conv(const struct stat *st, struct stat12 *ost)
{

	ost->st_dev = st->st_dev;
	ost->st_ino = st->st_ino;
	ost->st_mode = st->st_mode & 0xffff;
	if (st->st_nlink >= (1 << 15))
		ost->st_nlink = (1 << 15) - 1;
	else
		ost->st_nlink = st->st_nlink;
	ost->st_uid = st->st_uid;
	ost->st_gid = st->st_gid;
	ost->st_rdev = st->st_rdev;
	timespec_to_timespec50(&st->st_atimespec, &ost->st_atimespec);
	timespec_to_timespec50(&st->st_mtimespec, &ost->st_mtimespec);
	timespec_to_timespec50(&st->st_ctimespec, &ost->st_ctimespec);
	ost->st_size = st->st_size;
	ost->st_blocks = st->st_blocks;
	ost->st_blksize = st->st_blksize;
	ost->st_flags = st->st_flags;
	ost->st_gen = st->st_gen;
}

/*
 * Read a block of directory entries in a file system independent format.
 */
int
compat_12_sys_getdirentries(struct lwp *l, const struct compat_12_sys_getdirentries_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(char *) buf;
		syscallarg(u_int) count;
		syscallarg(long *) basep;
	} */
	struct dirent *bdp;
	struct vnode *vp;
	char *inp, *tbuf;		/* Current-format */
	int len, reclen;		/* Current-format */
	char *outp;			/* Dirent12-format */
	int resid, old_reclen = 0;	/* Dirent12-format */
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	struct dirent12 idb;
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

	vp = (struct vnode *)fp->f_vnode;
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

	inp = tbuf;
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
		idb.d_type = (uint8_t)bdp->d_type;
		idb.d_namlen = (uint8_t)bdp->d_namlen;
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
 * Get file status; this version follows links.
 */
/* ARGSUSED */
int
compat_12_sys_stat(struct lwp *l, const struct compat_12_sys_stat_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(struct stat12 *) ub;
	} */
	struct stat sb;
	struct stat12 osb;
	int error;

	error = do_sys_stat(SCARG(uap, path), FOLLOW, &sb);
	if (error)
		return (error);
	compat_12_stat_conv(&sb, &osb);
	error = copyout(&osb, SCARG(uap, ub), sizeof (osb));
	return (error);
}


/*
 * Get file status; this version does not follow links.
 */
/* ARGSUSED */
int
compat_12_sys_lstat(struct lwp *l, const struct compat_12_sys_lstat_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(struct stat12 *) ub;
	} */
	struct stat sb;
	struct stat12 osb;
	int error;

	error = do_sys_stat(SCARG(uap, path), NOFOLLOW, &sb);
	if (error)
		return (error);
	compat_12_stat_conv(&sb, &osb);
	error = copyout(&osb, SCARG(uap, ub), sizeof (osb));
	return (error);
}

/*
 * Return status information about a file descriptor.
 */
/* ARGSUSED */
int
compat_12_sys_fstat(struct lwp *l, const struct compat_12_sys_fstat_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) fd;
		syscallarg(struct stat12 *) sb;
	} */
	struct stat ub;
	struct stat12 oub;
	int error;

	error = do_sys_fstat(SCARG(uap, fd), &ub);
	if (error == 0) {
		compat_12_stat_conv(&ub, &oub);
		error = copyout(&oub, SCARG(uap, sb), sizeof (oub));
	}
	return (error);
}

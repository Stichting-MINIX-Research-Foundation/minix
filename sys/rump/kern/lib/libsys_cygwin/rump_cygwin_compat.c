/*	$NetBSD: rump_cygwin_compat.c,v 1.1 2013/04/10 16:44:54 pooka Exp $	*/

/*
 * Copyright (c) 2013 Antti Kantee.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/stat.h>
#include <sys/syscallargs.h>
#include <sys/vnode.h>
#include <sys/vfs_syscalls.h>

#include <compat/sys/time_types.h>

#include "rump_cygwin_syscallargs.h"

struct cygwin_stat {
        int		st_dev;
        int64_t		st_ino;
        int		st_mode;
        unsigned short	st_nlink;
        int		st_uid;
        int		st_gid;
        int		st_rdev;
        off_t		st_size;

        struct timespec50 st_atim;
        struct timespec50 st_mtim;
        struct timespec50 st_ctim;

        long		st_blksize;
        uint64_t	st_blocks;

        struct timespec50 st_btim;
};

#define PARCOPY(a) ssb->a = sb->a
static void
bsd_to_cygwin_stat(const struct stat *sb, struct cygwin_stat *ssb)
{

	memset(ssb, 0, sizeof(*ssb));
	PARCOPY(st_dev);
	PARCOPY(st_ino);
	PARCOPY(st_mode);
	PARCOPY(st_nlink);
	PARCOPY(st_uid);
	PARCOPY(st_gid);
	PARCOPY(st_rdev);
	PARCOPY(st_size);
	PARCOPY(st_blksize);
	PARCOPY(st_blocks);

	timespec_to_timespec50(&sb->st_atimespec, &ssb->st_atim);
	timespec_to_timespec50(&sb->st_mtimespec, &ssb->st_mtim);
	timespec_to_timespec50(&sb->st_ctimespec, &ssb->st_ctim);
	timespec_to_timespec50(&sb->st_birthtimespec, &ssb->st_btim);
}

int
rump_cygwin_sys_stat(struct lwp *l, const struct rump_cygwin_sys_stat_args *uap,
	register_t *retval)
{
	struct cygwin_stat ssb;
	struct stat sb;
	int error;

	error = do_sys_stat(SCARG(uap, path), FOLLOW, &sb);
	if (error)
		return error;

	bsd_to_cygwin_stat(&sb, &ssb);

	return copyout(&ssb, SCARG(uap, sp), sizeof(ssb));
}

int
rump_cygwin_sys_fstat(struct lwp *l, const struct rump_cygwin_sys_fstat_args *uap,
	register_t *retval)
{
	struct cygwin_stat ssb;
	struct stat sb;
	int error;

	error = do_sys_fstat(SCARG(uap, fd), &sb);
	if (error)
		return error;

	bsd_to_cygwin_stat(&sb, &ssb);

	return copyout(&ssb, SCARG(uap, sp), sizeof(ssb));
}

int
rump_cygwin_sys_lstat(struct lwp *l, const struct rump_cygwin_sys_lstat_args *uap,
	register_t *retval)
{
	struct cygwin_stat ssb;
	struct stat sb;
	int error;

	error = do_sys_stat(SCARG(uap, path), NOFOLLOW, &sb);
	if (error)
		return error;

	bsd_to_cygwin_stat(&sb, &ssb);

	return copyout(&ssb, SCARG(uap, sp), sizeof(ssb));
}

int
rump_cygwin_sys_open(struct lwp *l, const struct rump_cygwin_sys_open_args *uap,
	register_t *retval)
{
	/* {
		syscallarg(const char *) path;
		syscallarg(int) flags;
		syscallarg(int) mode;
	} */
	struct sys_open_args ua;
	int sflags, flags;

	sflags = SCARG(uap, flags);
	flags = sflags & (3 | O_APPEND | O_ASYNC | O_CREAT | O_TRUNC | O_EXCL);
	
	SCARG(&ua, path) = SCARG(uap, path);
	SCARG(&ua, flags) = flags;
	SCARG(&ua, mode) = SCARG(uap, mode);

	return sys_open(l, &ua, retval);
}

#define CYGWIN_NAME_MAX 255
struct cygwin_dirent {
	long		d_version;
	int64_t		d_ino;
	unsigned char	d_type;
	unsigned char	d_unused[3];
	uint32_t	d_internal;
	char		d_name[CYGWIN_NAME_MAX + 1];
} __packed;

#define CYGWIN_NAMEOFF(dp) 	   offsetof(struct cygwin_dirent,d_name)
#define CYGWIN_RECLEN(dp, namlen)  ((CYGWIN_NAMEOFF(dp) + (namlen) + 1))

int
rump_cygwin_sys_getdents(struct lwp *l,
	const struct rump_cygwin_sys_getdents_args *uap, register_t *retval)
{
	struct file *fp;
	struct dirent *bdp;
	struct cygwin_dirent idb;
	char *buf, *inp, *outp;
	size_t resid, buflen, nbytes;
	size_t reclen, cygwin_reclen;
	int error, done;

	if ((error = fd_getvnode(SCARG(uap, fd), &fp)) != 0)
		return (error);

	/*
	 * Sneaky, but avoids having "rewind" f_offset due to the
	 * conversions not fitting from our intermediate kernel buffer
	 * into the user buffer
	 */
	nbytes = SCARG(uap, nbytes);
	buflen = min(MAXBSIZE, (nbytes*8)/10);
	buf = kmem_alloc(buflen, KM_SLEEP);

	if ((fp->f_flag & FREAD) == 0) {
		error = EBADF;
		goto out;
	}

	resid = nbytes;
	outp = SCARG(uap, buf);

 again:
	if ((error = vn_readdir(fp, buf, UIO_SYSSPACE, buflen, &done,
	    l, NULL, NULL)) != 0)
		goto out;
	if (done == 0)
		goto eof;

	for (inp = buf; done > 0; done -= reclen) {
		bdp = (struct dirent *)inp;
		reclen = bdp->d_reclen;

		/* skip empty entries */
		if (bdp->d_fileno == 0) {
			inp += reclen;
			continue;
		}

		cygwin_reclen = CYGWIN_RECLEN(&idb, bdp->d_namlen);
		if (resid < cygwin_reclen) {
			panic("impossible shortage of resid");
		}

		memset(&idb, 0, sizeof(idb));
		idb.d_ino = bdp->d_fileno;
		idb.d_type = bdp->d_type;
		strlcpy(idb.d_name, bdp->d_name, sizeof(idb.d_name));
		if ((error = copyout(&idb, outp, cygwin_reclen)) != 0)
			goto out;

		inp += reclen;
		outp += cygwin_reclen;
		resid -= cygwin_reclen;
	}

	/* if we squished out the whole block, try again */
	if (outp == SCARG(uap, buf)) {
		goto again;
	}

 eof:
	*retval = nbytes - resid;
 out:
	kmem_free(buf, buflen);
 	fd_putfile(SCARG(uap, fd));

	return (error);
}

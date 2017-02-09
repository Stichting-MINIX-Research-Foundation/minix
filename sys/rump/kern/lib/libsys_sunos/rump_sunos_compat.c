/*	$NetBSD: rump_sunos_compat.c,v 1.1 2013/04/09 13:08:33 pooka Exp $	*/

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

#include "rump_sunos_syscallargs.h"

#define SUNOS_MAXNAMLEN 255

struct sunos_dirent {
	uint64_t	d_fileno;
	int64_t		d_off;
	unsigned short	d_reclen;
	char		d_name[SUNOS_MAXNAMLEN + 1];
};

#define SUNOS_NAMEOFF(dp)	((char *)&(dp)->d_name - (char *)dp)
#define SUNOS_RECLEN(de,namlen)	ALIGN((SUNOS_NAMEOFF(de) + (namlen) + 1))

/*
 * Rump kernels always use the _FILE_OFFSET_BITS=64 API.
 */
#ifdef __LP64__
struct sunos_stat {
        unsigned long	st_dev;
        uint64_t	st_ino;
        unsigned int	st_mode;
        unsigned int	st_nlink;
        unsigned int	st_uid;
        unsigned int	st_gid;
        unsigned long	st_rdev;
        off_t		st_size;

        struct timespec	st_atim;
        struct timespec	st_mtim;
        struct timespec	st_ctim;
        int		st_blksize;
        uint64_t	st_blocks;
        char		st_fstype[16];
};
#else
struct sunos_stat {
        unsigned long	st_dev;
	long		st_pad1[3];
        uint64_t	st_ino;
        unsigned int	st_mode;
        unsigned int	st_nlink;
        unsigned int	st_uid;
        unsigned int	st_gid;
        unsigned long	st_rdev;
	long		st_pad2[2];
        off_t		st_size;

        struct timespec50 st_atim;
        struct timespec50 st_mtim;
        struct timespec50 st_ctim;

        int		st_blksize;
        uint64_t	st_blocks;
        char            st_fstype[16];
	long		st_pad4[8];
};
#endif

#define PARCOPY(a) ssb->a = sb->a
static void
bsd_to_sunos_stat(const struct stat *sb, struct sunos_stat *ssb)
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

#ifdef __LP64__
	ssb->st_atim = sb->st_atimespec;
	ssb->st_mtim = sb->st_mtimespec;
	ssb->st_ctim = sb->st_ctimespec;
#else
	timespec_to_timespec50(&sb->st_atimespec, &ssb->st_atim);
	timespec_to_timespec50(&sb->st_mtimespec, &ssb->st_mtim);
	timespec_to_timespec50(&sb->st_ctimespec, &ssb->st_ctim);
#endif
}

int
rump_sunos_sys_stat(struct lwp *l, const struct rump_sunos_sys_stat_args *uap,
	register_t *retval)
{
	struct sunos_stat ssb;
	struct stat sb;
	int error;

	error = do_sys_stat(SCARG(uap, path), FOLLOW, &sb);
	if (error)
		return error;

	bsd_to_sunos_stat(&sb, &ssb);

	return copyout(&ssb, SCARG(uap, sp), sizeof(ssb));
}

int
rump_sunos_sys_fstat(struct lwp *l, const struct rump_sunos_sys_fstat_args *uap,
	register_t *retval)
{
	struct sunos_stat ssb;
	struct stat sb;
	int error;

	error = do_sys_fstat(SCARG(uap, fd), &sb);
	if (error)
		return error;

	bsd_to_sunos_stat(&sb, &ssb);

	return copyout(&ssb, SCARG(uap, sp), sizeof(ssb));
}

int
rump_sunos_sys_lstat(struct lwp *l, const struct rump_sunos_sys_lstat_args *uap,
	register_t *retval)
{
	struct sunos_stat ssb;
	struct stat sb;
	int error;

	error = do_sys_stat(SCARG(uap, path), NOFOLLOW, &sb);
	if (error)
		return error;

	bsd_to_sunos_stat(&sb, &ssb);

	return copyout(&ssb, SCARG(uap, sp), sizeof(ssb));
}

int
rump_sunos_sys_open(struct lwp *l, const struct rump_sunos_sys_open_args *uap,
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
	flags =  (sflags & (0x8 | 0x4 | 0x3)); /* nonblock/append/rw */
	flags |= (sflags & 0x10)	? O_SYNC : 0;
	flags |= (sflags & 0x40)	? O_DSYNC : 0;
	flags |= (sflags & 0x8000)	? O_RSYNC : 0;
	flags |= (sflags & 0x80)	? O_NONBLOCK : 0;
	flags |= (sflags & 0x100)	? O_CREAT : 0;
	flags |= (sflags & 0x200)	? O_TRUNC : 0;
	flags |= (sflags & 0x400)	? O_EXCL : 0;
	flags |= (sflags & 0x20000)	? O_NOFOLLOW : 0;
	
	SCARG(&ua, path) = SCARG(uap, path);
	SCARG(&ua, flags) = flags;
	SCARG(&ua, mode) = SCARG(uap, mode);

	return sys_open(l, &ua, retval);
}

/*-
 * Copyright (c) 1992, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Lawrence Berkeley Laboratory.
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
 *      @(#)sunos_misc.c        8.1 (Berkeley) 6/18/93
 *
 *      Header: sunos_misc.c,v 1.16 93/04/07 02:46:27 torek Exp
 */

int
rump_sunos_sys_getdents(struct lwp *l,
	const struct rump_sunos_sys_getdents_args *uap, register_t *retval)
{
	struct dirent *bdp;
	struct vnode *vp;
	char *inp, *buf;	/* BSD-format */
	int len, reclen;	/* BSD-format */
	char *outp;		/* Sun-format */
	int resid, sunos_reclen;/* Sun-format */
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	struct sunos_dirent idb;
	off_t off;			/* true file offset */
	int buflen, error, eofflag;
	off_t *cookiebuf, *cookie;
	int ncookies;

	if ((error = fd_getvnode(SCARG(uap, fd), &fp)) != 0)
		return (error);

	if ((fp->f_flag & FREAD) == 0) {
		error = EBADF;
		goto out1;
	}

	vp = fp->f_data;
	if (vp->v_type != VDIR) {
		error = EINVAL;
		goto out1;
	}

	buflen = min(MAXBSIZE, SCARG(uap, nbytes));
	buf = malloc(buflen, M_TEMP, M_WAITOK);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	off = fp->f_offset;
again:
	aiov.iov_base = buf;
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

	inp = buf;
	outp = SCARG(uap, buf);
	resid = SCARG(uap, nbytes);
	if ((len = buflen - auio.uio_resid) == 0)
		goto eof;

	for (cookie = cookiebuf; len > 0; len -= reclen) {
		bdp = (struct dirent *)inp;
		reclen = bdp->d_reclen;
		if (reclen & 3)
			panic("sunos_getdents");
		if ((*cookie >> 32) != 0) {
			printf("rump_sunos_sys_getdents: offset too large\n");
			error = EINVAL;
			goto out;
		}
		if (bdp->d_fileno == 0) {
			inp += reclen;	/* it is a hole; squish it out */
			if (cookie)
				off = *cookie++;
			else
				off += reclen;
			continue;
		}
		sunos_reclen = SUNOS_RECLEN(&idb, bdp->d_namlen);
		if (reclen > len || resid < sunos_reclen) {
			/* entry too big for buffer, so just stop */
			outp++;
			break;
		}
		if (cookie)
			off = *cookie++;	/* each entry points to next */
		else
			off += reclen;
		/*
		 * Massage in place to make a Sun-shaped dirent (otherwise
		 * we have to worry about touching user memory outside of
		 * the copyout() call).
		 */
		idb.d_fileno = bdp->d_fileno;
		idb.d_off = off;
		idb.d_reclen = sunos_reclen;
		strlcpy(idb.d_name, bdp->d_name, sizeof(idb.d_name));
		if ((error = copyout((void *)&idb, outp, sunos_reclen)) != 0)
			goto out;
		/* advance past this real entry */
		inp += reclen;
		/* advance output past Sun-shaped entry */
		outp += sunos_reclen;
		resid -= sunos_reclen;
	}

	/* if we squished out the whole block, try again */
	if (outp == SCARG(uap, buf)) {
		if (cookiebuf)
			free(cookiebuf, M_TEMP);
		cookiebuf = NULL;
		goto again;
	}
	fp->f_offset = off;		/* update the vnode offset */

eof:
	*retval = SCARG(uap, nbytes) - resid;
out:
	VOP_UNLOCK(vp);
	free(cookiebuf, M_TEMP);
	free(buf, M_TEMP);
 out1:
 	fd_putfile(SCARG(uap, fd));
	return (error);
}

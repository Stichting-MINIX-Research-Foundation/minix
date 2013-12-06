/*	$NetBSD: stat.h,v 1.6 2013/10/04 21:07:37 christos Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)stat.h	8.12 (Berkeley) 8/17/94
 */

#ifndef _COMPAT_SYS_STAT_H_
#define	_COMPAT_SYS_STAT_H_

#ifdef _KERNEL
#include <compat/sys/time.h>

struct stat43 {				/* BSD-4.3 stat struct */
	uint16_t  st_dev;		/* inode's device */
	uint32_t  st_ino;		/* inode's number */
	uint16_t  st_mode;		/* inode protection mode */
	uint16_t  st_nlink;		/* number of hard links */
	uint16_t  st_uid;		/* user ID of the file's owner */
	uint16_t  st_gid;		/* group ID of the file's group */
	uint16_t  st_rdev;		/* device type */
	int32_t	  st_size;		/* file size, in bytes */
	struct	  timespec50 st_atimespec;/* time of last access */
	struct	  timespec50 st_mtimespec;/* time of last data modification */
	struct	  timespec50 st_ctimespec;/* time of last file status change */
	int32_t	  st_blksize;		/* optimal blocksize for I/O */
	int32_t	  st_blocks;		/* blocks allocated for file */
	uint32_t  st_flags;		/* user defined flags for file */
	uint32_t  st_gen;		/* file generation number */
};
#endif /* defined(_KERNEL) */

struct stat12 {				/* NetBSD-1.2 stat struct */
	uint32_t  st_dev;		/* inode's device */
	uint32_t  st_ino;		/* inode's number */
	uint16_t  st_mode;		/* inode protection mode */
	uint16_t  st_nlink;		/* number of hard links */
	uid_t	  st_uid;		/* user ID of the file's owner */
	gid_t	  st_gid;		/* group ID of the file's group */
	uint32_t  st_rdev;		/* device type */
	struct	  timespec50 st_atimespec;/* time of last access */
	struct	  timespec50 st_mtimespec;/* time of last data modification */
	struct	  timespec50 st_ctimespec;/* time of last file status change */
	off_t	  st_size;		/* file size, in bytes */
	int64_t	  st_blocks;		/* blocks allocated for file */
	uint32_t  st_blksize;		/* optimal blocksize for I/O */
	uint32_t  st_flags;		/* user defined flags for file */
	uint32_t  st_gen;		/* file generation number */
	int32_t	  st_lspare;
	int64_t	  st_qspare[2];
};

/*
 * On systems with 8 byte longs and 4 byte time_ts, padding the time_ts
 * is required in order to have a consistent ABI.  This is because the
 * stat structure used to contain timespecs, which had different
 * alignment constraints than a time_t and a long alone.  The padding
 * should be removed the next time the stat structure ABI is changed.
 * (This will happen whever we change to 8 byte time_t.)
 */
#if defined(_LP64)	/* XXXX  && _BSD_TIME_T_ == int */
#define	__STATPAD(x)	int x;
#else
#define	__STATPAD(x)	/* nothing */
#endif

struct stat13 {
	uint32_t  st_dev;		/* inode's device */
	uint32_t  st_ino;		/* inode's number */
	mode_t	  st_mode;		/* inode protection mode */
	nlink_t	  st_nlink;		/* number of hard links */
	uid_t	  st_uid;		/* user ID of the file's owner */
	gid_t	  st_gid;		/* group ID of the file's group */
	uint32_t  st_rdev;		/* device type */
#if defined(_NETBSD_SOURCE)
	struct	  timespec50 st_atimespec;/* time of last access */
	struct	  timespec50 st_mtimespec;/* time of last data modification */
	struct	  timespec50 st_ctimespec;/* time of last file status change */
#else
	__STATPAD(__pad0)
	int32_t	  st_atime;		/* time of last access */
	__STATPAD(__pad1)
	long	  st_atimensec;		/* nsec of last access */
	int32_t	  st_mtime;		/* time of last data modification */
	__STATPAD(__pad2)
	long	  st_mtimensec;		/* nsec of last data modification */
	int32_t	  st_ctime;		/* time of last file status change */
	__STATPAD(__pad3)
	long	  st_ctimensec;		/* nsec of last file status change */
#endif
	off_t	  st_size;		/* file size, in bytes */
	blkcnt_t  st_blocks;		/* blocks allocated for file */
	blksize_t st_blksize;		/* optimal blocksize for I/O */
	uint32_t  st_flags;		/* user defined flags for file */
	uint32_t  st_gen;		/* file generation number */
	uint32_t  st_spare0;
#if defined(_NETBSD_SOURCE)
	struct timespec50 st_birthtimespec;
#else
	int32_t	  st_birthtime;
	__STATPAD(__pad4)
	long	  st_birthtimensec;
#endif
#if !defined(_LP64)
	int	__pad5;
#endif
};

struct stat30 {
	uint32_t  st_dev;		/* inode's device */
	mode_t	  st_mode;		/* inode protection mode */
	ino_t	  st_ino;		/* inode's number */
	nlink_t	  st_nlink;		/* number of hard links */
	uid_t	  st_uid;		/* user ID of the file's owner */
	gid_t	  st_gid;		/* group ID of the file's group */
	uint32_t  st_rdev;		/* device type */
#if defined(_NETBSD_SOURCE)
	struct	  timespec50 st_atimespec;/* time of last access */
	struct	  timespec50 st_mtimespec;/* time of last data modification */
	struct	  timespec50 st_ctimespec;/* time of last file status change */
	struct 	  timespec50 st_birthtimespec; /* time of creation */
#else
	int32_t	  st_atime;		/* time of last access */
	__STATPAD(__pad0)
	long	  st_atimensec;		/* nsec of last access */
	int32_t	  st_mtime;		/* time of last data modification */
	__STATPAD(__pad1)
	long	  st_mtimensec;		/* nsec of last data modification */
	int32_t	  st_ctime;		/* time of last file status change */
	__STATPAD(__pad2)
	long	  st_ctimensec;		/* nsec of last file status change */
	int32_t	  st_birthtime;		/* time of creation */
	__STATPAD(__pad3)
	long	  st_birthtimensec;	/* nsec of time of creation */
#endif
	off_t	  st_size;		/* file size, in bytes */
	blkcnt_t  st_blocks;		/* blocks allocated for file */
	blksize_t st_blksize;		/* optimal blocksize for I/O */
	uint32_t  st_flags;		/* user defined flags for file */
	uint32_t  st_gen;		/* file generation number */
	uint32_t  st_spare[2];
};

#undef __STATPAD

#if defined(_KERNEL)
void compat_12_stat_conv(const struct stat *st, struct stat12 *ost);
#endif

#if !defined(_KERNEL) && !defined(_STANDALONE)

__BEGIN_DECLS
int	__compat_stat(const char *, struct stat12 *) __dso_hidden;
int	__compat_fstat(int, struct stat12 *) __dso_hidden;
int	__compat_mknod(const char *, mode_t, uint32_t) __dso_hidden;
int	__compat___stat13(const char *, struct stat13 *) __dso_hidden;
int	__compat___fstat13(int, struct stat13 *) __dso_hidden;
int	__compat___stat30(const char *, struct stat30 *) __dso_hidden;
int	__compat___fstat30(int, struct stat30 *) __dso_hidden;
int	__stat50(const char *, struct stat *);
int	__fstat50(int, struct stat *);
int	__mknod50(const char *, mode_t, dev_t);
#if defined(_XOPEN_SOURCE) || defined(_NETBSD_SOURCE)
int	__compat_lstat(const char *, struct stat12 *) __dso_hidden;
int	__compat___lstat13(const char *, struct stat13 *) __dso_hidden;
int	__compat___lstat30(const char *, struct stat30 *) __dso_hidden;
int	__lstat50(const char *, struct stat *);
#endif /* defined(_XOPEN_SOURCE) || defined(_NETBSD_SOURCE) */
__END_DECLS

#endif /* !_KERNEL && !_STANDALONE */
#endif /* !_COMPAT_SYS_STAT_H_ */

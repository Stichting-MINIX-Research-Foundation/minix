/*	$NetBSD: statvfs.h,v 1.17 2011/11/18 21:17:45 christos Exp $	 */

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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

#ifndef	_SYS_STATVFS_H_
#define	_SYS_STATVFS_H_

#include <sys/cdefs.h>
#include <sys/featuretest.h>
#include <sys/stdint.h>
#include <machine/ansi.h>
#include <sys/ansi.h>
#include <sys/fstypes.h>

#define	_VFS_NAMELEN	32
#define	_VFS_MNAMELEN	1024

#ifndef	fsblkcnt_t
typedef	__fsblkcnt_t	fsblkcnt_t;	/* fs block count (statvfs) */
#define	fsblkcnt_t	__fsblkcnt_t
#endif

#ifndef	fsfilcnt_t
typedef	__fsfilcnt_t	fsfilcnt_t;	/* fs file count */
#define	fsfilcnt_t	__fsfilcnt_t
#endif

#ifndef	uid_t
typedef	__uid_t		uid_t;		/* user id */
#define	uid_t		__uid_t
#endif

#ifdef	_BSD_SIZE_T_
typedef	_BSD_SIZE_T_		size_t;
#define	_SIZE_T
#undef	_BSD_SIZE_T_
#endif

struct statvfs {
	unsigned long	f_flag;		/* copy of mount exported flags */
	unsigned long	f_bsize;	/* file system block size */
	unsigned long	f_frsize;	/* fundamental file system block size */
	unsigned long	f_iosize;	/* optimal file system block size */

	/* The following are in units of f_frsize */
	fsblkcnt_t	f_blocks;	/* number of blocks in file system, */
	fsblkcnt_t	f_bfree;	/* free blocks avail in file system */
	fsblkcnt_t	f_bavail;	/* free blocks avail to non-root */
	fsblkcnt_t	f_bresvd;	/* blocks reserved for root */

	fsfilcnt_t	f_files;	/* total file nodes in file system */
	fsfilcnt_t	f_ffree;	/* free file nodes in file system */
	fsfilcnt_t	f_favail;	/* free file nodes avail to non-root */
	fsfilcnt_t	f_fresvd;	/* file nodes reserved for root */

	uint64_t  	f_syncreads;	/* count of sync reads since mount */
	uint64_t  	f_syncwrites;	/* count of sync writes since mount */

	uint64_t  	f_asyncreads;	/* count of async reads since mount */
	uint64_t  	f_asyncwrites;	/* count of async writes since mount */

	fsid_t		f_fsidx;	/* NetBSD compatible fsid */
	unsigned long	f_fsid;		/* Posix compatible fsid */
	unsigned long	f_namemax;	/* maximum filename length */
	uid_t		f_owner;	/* user that mounted the file system */

	uint32_t	f_spare[4];	/* spare space */

	char	f_fstypename[_VFS_NAMELEN]; /* fs type name */
	char	f_mntonname[_VFS_MNAMELEN];  /* directory on which mounted */
	char	f_mntfromname[_VFS_MNAMELEN];  /* mounted file system */

};

#if defined(_NETBSD_SOURCE) && !defined(_POSIX_SOURCE) && \
    !defined(_XOPEN_SOURCE)
#define	VFS_NAMELEN	_VFS_NAMELEN
#define	VFS_MNAMELEN	_VFS_MNAMELEN
#endif

#define	ST_RDONLY	MNT_RDONLY
#define	ST_NOSUID	MNT_NOSUID
#ifdef __minix
#define	ST_NOTRUNC	__MNT_UNUSED1
#endif /* !__minix*/

#define	ST_WAIT		MNT_WAIT
#define	ST_NOWAIT	MNT_NOWAIT

__BEGIN_DECLS
int	statvfs(const char *__restrict, struct statvfs *__restrict);
int	fstatvfs(int, struct statvfs *);
int	getvfsstat(struct statvfs *, size_t, int);
__END_DECLS

#endif /* !_SYS_STATVFS_H_ */

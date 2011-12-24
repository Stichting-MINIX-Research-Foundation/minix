/*	$NetBSD: fcntl.h,v 1.42 2012/01/25 00:28:35 christos Exp $	*/

/*-
 * Copyright (c) 1983, 1990, 1993
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
 *	@(#)fcntl.h	8.3 (Berkeley) 1/21/94
 */

#ifndef _SYS_FCNTL_H_
#define	_SYS_FCNTL_H_

/*
 * This file includes the definitions for open and fcntl
 * described by POSIX for <fcntl.h>; it also includes
 * related kernel definitions.
 */

#ifndef _KERNEL
#include <sys/featuretest.h>
#include <sys/types.h>
#if defined(_XOPEN_SOURCE) || defined(_NETBSD_SOURCE)
#include <sys/stat.h>
#endif /* _XOPEN_SOURCE || _NETBSD_SOURCE */
#endif /* !_KERNEL */

/*
 * File status flags: these are used by open(2), fcntl(2).
 * They are also used (indirectly) in the kernel file structure f_flags,
 * which is a superset of the open/fcntl flags.  Open flags and f_flags
 * are inter-convertible using OFLAGS(fflags) and FFLAGS(oflags).
 * Open/fcntl flags begin with O_; kernel-internal flags begin with F.
 */
/* open-only flags */
#define	O_RDONLY	0x00000000	/* open for reading only */
#define	O_WRONLY	0x00000001	/* open for writing only */
#define	O_RDWR		0x00000002	/* open for reading and writing */
#define	O_ACCMODE	0x00000003	/* mask for above modes */

/* File status flags for open() and fcntl().  POSIX Table 6-5. */
#define O_APPEND       02000	/* set append mode */
#define O_NONBLOCK     04000	/* no delay */
#define O_REOPEN      010000	/* automatically re-open device after driver
				 * restart
				 */
#define O_CLOEXEC     020000	/* close on exec */
#if defined(_NETBSD_SOURCE)
#define O_NOSIGPIPE   040000	/* don't deliver sigpipe */
#endif

#ifndef __minix  /* NOT SUPPORTED! */
#if defined(_NETBSD_SOURCE)
#define	O_SHLOCK	0x00000010	/* open with shared file lock */
#define	O_EXLOCK	0x00000020	/* open with exclusive file lock */
#define	O_ASYNC		0x00000040	/* signal pgrp when data ready */
#endif
#if (_POSIX_C_SOURCE - 0) >= 199309L || \
    (defined(_XOPEN_SOURCE) && defined(_XOPEN_SOURCE_EXTENDED)) || \
    (_XOPEN_SOURCE - 0) >= 500 || defined(_NETBSD_SOURCE)
#define	O_SYNC		0x00000080	/* synchronous writes */
#endif
#if defined(_NETBSD_SOURCE)
#define	O_NOFOLLOW	0x00000100	/* don't follow symlinks on the last */
					/* path component */
#endif
#endif /* !__minix */

/* Oflag values for open().  POSIX Table 6-4. */
#define O_CREAT        00100	/* creat file if it doesn't exist */
#define O_EXCL         00200	/* exclusive use flag */
#define O_NOCTTY       00400	/* do not assign a controlling terminal */
#define O_TRUNC        01000	/* truncate flag */

#ifndef __minix /* NOT SUPPORTED! */
#if (_POSIX_C_SOURCE - 0) >= 199309L || (_XOPEN_SOURCE - 0) >= 500 || \
    defined(_NETBSD_SOURCE)
#define	O_DSYNC		0x00010000	/* write: I/O data completion */
#define	O_RSYNC		0x00020000	/* read: I/O completion as for write */
#endif

#if defined(_NETBSD_SOURCE)
#define	O_ALT_IO	0x00040000	/* use alternate i/o semantics */
#define	O_DIRECT	0x00080000	/* direct I/O hint */
#endif
#endif /* !__minix */

/*
 * Constants used for fcntl(2)
 */

/* command values */
/* These values are used for cmd in fcntl().  POSIX Table 6-1.  */
#define F_DUPFD            0	/* duplicate file descriptor */
#define F_GETFD	           1	/* get file descriptor flags */
#define F_SETFD            2	/* set file descriptor flags */
#define F_GETFL            3	/* get file status flags */
#define F_SETFL            4	/* set file status flags */
#define F_GETLK            5	/* get record locking information */
#define F_SETLK            6	/* set record locking information */
#define F_SETLKW           7	/* set record locking info; wait if blocked */
#define F_FREESP           8	/* free a section of a regular file */
#if defined(_NETBSD_SOURCE)
#define F_GETNOSIGPIPE     9
#define F_SETNOSIGPIPE    10
#endif

/* File descriptor flags used for fcntl().  POSIX Table 6-2. */
#define FD_CLOEXEC         1	/* close on exec flag for third arg of fcntl */

/* record locking flags (F_GETLK, F_SETLK, F_SETLKW) */
#define F_RDLCK            1	/* shared or read lock */
#define F_WRLCK            2	/* exclusive or write lock */
#define F_UNLCK            3	/* unlock */

/*
 * Advisory file segment locking data type -
 * information passed to system by user
 */
struct flock {
  short l_type;			/* type: F_RDLCK, F_WRLCK, or F_UNLCK */
  short l_whence;		/* flag for starting offset */
  off_t l_start;		/* relative offset in bytes */
  off_t l_len;			/* size; if 0, then until EOF */
  pid_t l_pid;			/* process id of the locks' owner */
};

#if defined(_NETBSD_SOURCE)
/* lock operations for flock(2) */
#define LOCK_SH		F_RDLCK		/* Shared lock */
#define LOCK_EX		F_WRLCK		/* Exclusive lock */
#define LOCK_NB		0x0080		/* Do not block when locking */
#define LOCK_UN		F_UNLCK		/* Unlock */
#endif

/* Always ensure that these are consistent with <stdio.h> and <unistd.h>! */
#ifndef	SEEK_SET
#define	SEEK_SET	0	/* set file offset to offset */
#endif
#ifndef	SEEK_CUR
#define	SEEK_CUR	1	/* set file offset to current plus offset */
#endif
#ifndef	SEEK_END
#define	SEEK_END	2	/* set file offset to EOF plus offset */
#endif

/*
 * posix_advise advisories.
 */

#define	POSIX_FADV_NORMAL	0	/* default advice / no advice */
#define	POSIX_FADV_RANDOM	1	/* random access */
#define	POSIX_FADV_SEQUENTIAL	2	/* sequential access(lower to higher) */
#define	POSIX_FADV_WILLNEED	3	/* be needed in near future */
#define	POSIX_FADV_DONTNEED	4	/* not be needed in near future */
#define	POSIX_FADV_NOREUSE	5	/* be accessed once */

/*
 * Constants for X/Open Extended API set 2 (a.k.a. C063)
 * linkat(2) - also part of Posix-2008/XPG7
 */
#if (_POSIX_C_SOURCE - 0) >= 200809L || (_XOPEN_SOURCE - 0) >= 700 || \
    defined(_NETBSD_SOURCE)
#if defined(_INCOMPLETE_XOPEN_C063) || defined(_KERNEL) || defined(__minix)
#define	AT_FDCWD		-100	/* Use cwd for relative link target */
#define	AT_EACCESS		0x100	/* Use euid/egid for access checks */
#define	AT_SYMLINK_NOFOLLOW	0x200	/* Do not follow symlinks */
#define	AT_SYMLINK_FOLLOW	0x400	/* Follow symlinks */
#define	AT_REMOVEDIR		0x800	/* Remove directory only */
#endif
#endif


#ifndef _KERNEL
#include <sys/cdefs.h>

__BEGIN_DECLS
int	open(const char *, int, ...);
int	creat(const char *, mode_t);
int	fcntl(int, int, ...);
#if defined(_NETBSD_SOURCE)
int	flock(int, int);
#endif /* _NETBSD_SOURCE */
__END_DECLS
#endif /* _KERNEL */

#endif /* !_SYS_FCNTL_H_ */

/*	$NetBSD: types.h,v 1.89 2012/03/17 21:30:29 christos Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1991, 1993, 1994
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
 *	@(#)types.h	8.4 (Berkeley) 1/21/94
 */

#ifndef _SYS_TYPES_H_
#define	_SYS_TYPES_H_

#include <sys/featuretest.h>

/* Machine type dependent parameters. */
#include <machine/types.h>

#include <machine/ansi.h>
#include <machine/int_types.h>


#include <sys/ansi.h>

#ifndef	int8_t
typedef	__int8_t	int8_t;
#define	int8_t		__int8_t
#endif

#ifndef	uint8_t
typedef	__uint8_t	uint8_t;
#define	uint8_t		__uint8_t
#endif

#ifndef	int16_t
typedef	__int16_t	int16_t;
#define	int16_t		__int16_t
#endif

#ifndef	uint16_t
typedef	__uint16_t	uint16_t;
#define	uint16_t	__uint16_t
#endif

#ifndef	int32_t
typedef	__int32_t	int32_t;
#define	int32_t		__int32_t
#endif

#ifndef	uint32_t
typedef	__uint32_t	uint32_t;
#define	uint32_t	__uint32_t
#endif

#ifndef	int64_t
typedef	__int64_t	int64_t;
#define	int64_t		__int64_t
#endif

#ifndef	uint64_t
typedef	__uint64_t	uint64_t;
#define	uint64_t	__uint64_t
#endif

typedef	uint8_t		u_int8_t;
typedef	uint16_t	u_int16_t;
typedef	uint32_t	u_int32_t;
typedef	uint64_t	u_int64_t;

#ifdef __minix
typedef uint8_t 	u8_t;
typedef uint16_t	u16_t;
typedef uint32_t	u32_t;
typedef uint64_t 	u64_t;

typedef int8_t		i8_t;
typedef int16_t		i16_t;
typedef int32_t		i32_t;
typedef int64_t		i64_t;

typedef uint64_t	big_ino_t;
typedef int64_t		big_off_t;
typedef u32_t		big_dev_t;
typedef u32_t		big_gid_t;
typedef u32_t		big_mode_t;
typedef u32_t		big_nlink_t;
typedef u32_t		big_uid_t;


#ifdef _MINIX
/* some Minix specific types that do not conflict with posix */
typedef uint32_t zone_t;      /* zone number */
typedef uint32_t block_t;     /* block number */
typedef uint32_t bit_t;       /* bit number in a bit map */
typedef uint16_t zone1_t;     /* zone number for V1 file systems */
typedef uint32_t bitchunk_t; /* collection of bits in a bitmap */
#endif

/* ANSI C makes writing down the promotion of unsigned types very messy.  When
 * sizeof(short) == sizeof(int), there is no promotion, so the type stays
 * unsigned.  When the compiler is not ANSI, there is usually no loss of
 * unsignedness, and there are usually no prototypes so the promoted type
 * doesn't matter.  The use of types like Ino_t is an attempt to use ints
 * (which are not promoted) while providing information to the reader.
 */

typedef unsigned long  Ino_t;
#endif /* __minix */

#include <machine/endian.h>

#if defined(_NETBSD_SOURCE)
typedef	unsigned char	u_char;
typedef	unsigned short	u_short;
typedef	unsigned int	u_int;
typedef	unsigned long	u_long;

typedef unsigned char	unchar;		/* Sys V compatibility */
typedef	unsigned short	ushort;		/* Sys V compatibility */
typedef	unsigned int	uint;		/* Sys V compatibility */
typedef unsigned long	ulong;		/* Sys V compatibility */
#endif

typedef	uint64_t	u_quad_t;	/* quads */
typedef	int64_t		quad_t;
typedef	quad_t *	qaddr_t;

/*
 * The types longlong_t and u_longlong_t exist for use with the
 * Sun-derived XDR routines involving these types, and their usage
 * in other contexts is discouraged.  Further note that these types
 * may not be equivalent to "long long" and "unsigned long long",
 * they are only guaranteed to be signed and unsigned 64-bit types
 * respectively.  Portable programs that need 64-bit types should use
 * the C99 types int64_t and uint64_t instead.
 */

typedef	int64_t		longlong_t;	/* for XDR */
typedef	uint64_t	u_longlong_t;	/* for XDR */

typedef	int64_t		blkcnt_t;	/* fs block count */
typedef	uint32_t		blksize_t;	/* fs optimal block size */

#ifndef	fsblkcnt_t
typedef	__fsblkcnt_t	fsblkcnt_t;	/* fs block count (statvfs) */
#define fsblkcnt_t	__fsblkcnt_t
#endif

#ifndef	fsfilcnt_t
typedef	__fsfilcnt_t	fsfilcnt_t;	/* fs file count */
#define fsfilcnt_t	__fsfilcnt_t
#endif

#if !defined(_KERNEL) && !defined(_STANDALONE)
/* We don't and shouldn't use caddr_t in the kernel anymore */
#ifndef	caddr_t
typedef	__caddr_t	caddr_t;	/* core address */
#define	caddr_t		__caddr_t
#endif
#endif

#ifdef __daddr_t
typedef	__daddr_t	daddr_t;	/* disk address */
#undef __daddr_t
#else
typedef	int64_t		daddr_t;	/* disk address */
#endif

typedef	uint32_t	dev_t;		/* device number */
typedef	uint32_t	fixpt_t;	/* fixed point number */

#ifndef	gid_t
typedef	__gid_t		gid_t;		/* group id */
#define	gid_t		__gid_t
#endif

typedef	int		idtype_t;	/* type of the id */
typedef	uint32_t	id_t;		/* group id, process id or user id */
typedef	uint32_t	ino_t;		/* inode number */
typedef	long		key_t;		/* IPC key (for Sys V IPC) */

#ifndef	mode_t
typedef	__mode_t	mode_t;		/* permissions */
#define	mode_t		__mode_t
#endif

typedef	int16_t	nlink_t;	/* link count */

#ifndef	off_t
typedef	__off_t		off_t;		/* file offset */
#define	off_t		__off_t
#endif

#ifndef	pid_t
typedef	__pid_t		pid_t;		/* process id */
#define	pid_t		__pid_t
#endif
typedef int32_t		lwpid_t;	/* LWP id */
typedef uint32_t	rlim_t;		/* resource limit */
typedef	int32_t		segsz_t;	/* segment size */
typedef	int32_t		swblk_t;	/* swap offset */

#ifndef	uid_t
typedef	__uid_t		uid_t;		/* user id */
#define	uid_t		__uid_t
#endif

typedef int		mqd_t;

typedef	unsigned long	cpuid_t;

typedef	int		psetid_t;

#if defined(_KERNEL) || defined(_STANDALONE)
/*
 * Boolean type definitions for the kernel environment.  User-space
 * boolean definitions are found in <stdbool.h>.
 */
#define bool	_Bool
#define true	1
#define false	0

/*
 * Deprecated Mach-style boolean_t type.  Should not be used by new code.
 */
typedef int	boolean_t;
#ifndef TRUE
#define	TRUE	1
#endif
#ifndef FALSE
#define	FALSE	0
#endif

#endif /* _KERNEL || _STANDALONE */

#if defined(_KERNEL) || defined(_LIBC)
/*
 * semctl(2)'s argument structure.  This is here for the benefit of
 * <sys/syscallargs.h>.  It is not in the user's namespace in SUSv2.
 * The SUSv2 semctl(2) takes variable arguments.
 */
union __semun {
	int		val;		/* value for SETVAL */
	struct semid_ds	*buf;		/* buffer for IPC_STAT & IPC_SET */
	unsigned short	*array;		/* array for GETALL & SETALL */
};
#include <sys/stdint.h>
#endif /* _KERNEL || _LIBC */

/*
 * These belong in unistd.h, but are placed here too to ensure that
 * long arguments will be promoted to off_t if the program fails to
 * include that header or explicitly cast them to off_t.
 */
#if defined(_NETBSD_SOURCE)
#ifndef __OFF_T_SYSCALLS_DECLARED
#define __OFF_T_SYSCALLS_DECLARED
#ifndef _KERNEL
#include <sys/cdefs.h>
__BEGIN_DECLS
off_t	 lseek(int, off_t, int);
int	 ftruncate(int, off_t);
int	 truncate(const char *, off_t);
__END_DECLS
#endif /* !_KERNEL */
#endif /* __OFF_T_SYSCALLS_DECLARED */
#endif /* defined(_NETBSD_SOURCE) */

#if defined(_NETBSD_SOURCE)
/* Major, minor numbers, dev_t's. */
typedef int32_t __devmajor_t, __devminor_t;
#define devmajor_t __devmajor_t
#define devminor_t __devminor_t
#define NODEVMAJOR (-1)
/* LSC Our major / minor numbering scheme is not the exactly the same, to be updated? */
#define	major(x)	((devmajor_t)(((uint32_t)(x) & 0x0000ff00) >>  8))
#define	minor(x)	((devminor_t)( \
				   (((uint32_t)(x) & 0x000000ff) >>  0)))
#define	makedev(x,y)	((dev_t)((((x) <<  8) & 0x0000ff00) | \
				  \
				 (((y) <<  0) & 0x000000ff)))
#endif

#ifdef	_BSD_CLOCK_T_
typedef	_BSD_CLOCK_T_		clock_t;
#undef	_BSD_CLOCK_T_
#endif

#ifdef	_BSD_SIZE_T_
typedef	_BSD_SIZE_T_		size_t;
#define _SIZE_T
#undef	_BSD_SIZE_T_
#endif

#ifdef	_BSD_SSIZE_T_
typedef	_BSD_SSIZE_T_		ssize_t;
#undef	_BSD_SSIZE_T_
#endif

#ifdef	_BSD_TIME_T_
typedef	_BSD_TIME_T_		time_t;
#undef	_BSD_TIME_T_
#endif

#ifdef	_BSD_CLOCKID_T_
typedef	_BSD_CLOCKID_T_		clockid_t;
#undef	_BSD_CLOCKID_T_
#endif

#ifndef __minix
#ifdef	_BSD_TIMER_T_
typedef	_BSD_TIMER_T_		timer_t;
#undef	_BSD_TIMER_T_
#endif
#endif

#ifdef	_BSD_SUSECONDS_T_
typedef	_BSD_SUSECONDS_T_	suseconds_t;
#undef	_BSD_SUSECONDS_T_
#endif

#ifdef	_BSD_USECONDS_T_
typedef	_BSD_USECONDS_T_	useconds_t;
#undef	_BSD_USECONDS_T_
#endif

#ifdef _NETBSD_SOURCE
#include <sys/fd_set.h>

#define	NBBY			8

typedef struct kauth_cred *kauth_cred_t;

typedef int pri_t;

#endif

#if defined(__STDC__) && (defined(_KERNEL) || defined(_KMEMUSER))
/*
 * Forward structure declarations for function prototypes.  We include the
 * common structures that cross subsystem boundaries here; others are mostly
 * used in the same place that the structure is defined.
 */
struct	lwp;
typedef struct lwp lwp_t;
struct	__ucontext;
struct	proc;
typedef struct proc proc_t;
struct	pgrp;
struct	rusage;
struct	file;
typedef struct file file_t;
struct	buf;
typedef struct buf buf_t;
struct	tty;
struct	uio;
#endif

#ifdef _KERNEL
#define SET(t, f)	((t) |= (f))
#define	ISSET(t, f)	((t) & (f))
#define	CLR(t, f)	((t) &= ~(f))
#endif

#ifndef __minix
#if !defined(_KERNEL) && !defined(_STANDALONE)
#if (_POSIX_C_SOURCE - 0L) >= 199506L || (_XOPEN_SOURCE - 0) >= 500 || \
    defined(_NETBSD_SOURCE)
#include <pthread_types.h>
#endif
#endif
#endif /* !__minix */

#endif /* !_SYS_TYPES_H_ */

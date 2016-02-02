/*	$NetBSD: param.h,v 1.486 2015/08/29 14:07:45 uebayasi Exp $	*/

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
 *	@(#)param.h	8.3 (Berkeley) 4/4/95
 */

#ifndef _SYS_PARAM_H_
#define	_SYS_PARAM_H_

#ifdef _KERNEL_OPT
#include "opt_param.h"
#endif

/*
 * Historic BSD #defines -- probably will remain untouched for all time.
 */
#define	BSD	199506		/* System version (year & month). */
#define	BSD4_3	1
#define	BSD4_4	1

/*
 *	#define __NetBSD_Version__ MMmmrrpp00
 *
 *	M = major version
 *	m = minor version; a minor number of 99 indicates current.
 *	r = 0 (*)
 *	p = patchlevel
 *
 * When new releases are made, src/gnu/usr.bin/groff/tmac/mdoc.local
 * needs to be updated and the changes sent back to the groff maintainers.
 *
 * (*)	Up to 2.0I "release" used to be "",A-Z,Z[A-Z] but numeric
 *	    	e.g. NetBSD-1.2D  = 102040000 ('D' == 4)
 *	NetBSD-2.0H 	(200080000) was changed on 20041001 to:
 *	2.99.9		(299000900)
 */

#define	__NetBSD_Version__	799002100	/* NetBSD 7.99.21 */

#define __NetBSD_Prereq__(M,m,p) (((((M) * 100000000) + \
    (m) * 1000000) + (p) * 100) <= __NetBSD_Version__)

/*
 * Historical NetBSD #define
 *
 * NetBSD 1.4 was the last release for which this value was incremented.
 * The value is now permanently fixed at 199905. It will never be
 * changed again.
 *
 * New code must use __NetBSD_Version__ instead, and should not even
 * count on NetBSD being defined.
 *
 */

#define	NetBSD	199905		/* NetBSD version (year & month). */

/*
 * There macros determine if we are running in protected mode or not.
 *   _HARDKERNEL: code uses kernel namespace and runs in hw priviledged mode
 *   _SOFTKERNEL: code uses kernel namespace but runs without hw priviledges
 */
#if defined(_KERNEL) && !defined(_RUMPKERNEL)
#define _HARDKERNEL
#endif
#if defined(_KERNEL) && defined(_RUMPKERNEL)
#define _SOFTKERNEL
#endif

#include <sys/null.h>

#ifndef __ASSEMBLER__
#include <sys/inttypes.h>
#include <sys/types.h>

/*
 * Machine-independent constants (some used in following include files).
 * Redefined constants are from POSIX 1003.1 limits file.
 *
 * MAXCOMLEN should be >= sizeof(ac_comm) (see <acct.h>)
 * MAXHOSTNAMELEN should be >= (_POSIX_HOST_NAME_MAX + 1) (see <limits.h>)
 * MAXLOGNAME should be >= UT_NAMESIZE (see <utmp.h>)
 */
#include <sys/syslimits.h>

#define	MAXCOMLEN	16		/* max command name remembered */
#define	MAXINTERP	PATH_MAX	/* max interpreter file name length */
/* DEPRECATED: use LOGIN_NAME_MAX instead. */
#define	MAXLOGNAME	(LOGIN_NAME_MAX - 1) /* max login name length */
#define	NCARGS		ARG_MAX		/* max bytes for an exec function */
#define	NGROUPS		NGROUPS_MAX	/* max number groups */
#define	NOGROUP		65535		/* marker for empty group set member */
#define	MAXHOSTNAMELEN	256		/* max hostname size */

#ifndef NOFILE
#define	NOFILE		OPEN_MAX	/* max open files per process */
#endif
#ifndef MAXUPRC				/* max simultaneous processes */
#define	MAXUPRC		CHILD_MAX	/* POSIX 1003.1-compliant default */
#else
#if (MAXUPRC - 0) < CHILD_MAX
#error MAXUPRC less than CHILD_MAX.  See options(4) for details.
#endif /* (MAXUPRC - 0) < CHILD_MAX */
#endif /* !defined(MAXUPRC) */

/* Macros for min/max. */
#define	MIN(a,b)	((/*CONSTCOND*/(a)<(b))?(a):(b))
#define	MAX(a,b)	((/*CONSTCOND*/(a)>(b))?(a):(b))

/* More types and definitions used throughout the kernel. */
#ifdef _KERNEL
#include <sys/cdefs.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/ucred.h>
#include <sys/uio.h>
#include <uvm/uvm_param.h>
#ifndef NPROC
#define	NPROC	(20 + 16 * MAXUSERS)
#endif
#ifndef NTEXT
#define	NTEXT	(80 + NPROC / 8)		/* actually the object cache */
#endif
#ifndef NVNODE
#define	NVNODE	(NPROC + NTEXT + 100)
#define	NVNODE_IMPLICIT
#endif
#ifndef VNODE_KMEM_MAXPCT
#define	VNODE_KMEM_MAXPCT	60
#endif
#ifndef BUFCACHE_VA_MAXPCT
#define	BUFCACHE_VA_MAXPCT	20
#endif
#define	VNODE_COST	2048			/* assumed space in bytes */
#endif /* _KERNEL */

/* Signals. */
#include <sys/signal.h>

/* Machine type dependent parameters. */
#include <machine/param.h>
#include <machine/limits.h>

/* pages ("clicks") to disk blocks */
#define	ctod(x)		((x) << (PGSHIFT - DEV_BSHIFT))
#define	dtoc(x)		((x) >> (PGSHIFT - DEV_BSHIFT))

/* bytes to pages */
#define	ctob(x)		((x) << PGSHIFT)
#define	btoc(x)		(((x) + PGOFSET) >> PGSHIFT)

/* bytes to disk blocks */
#define	dbtob(x)	((x) << DEV_BSHIFT)
#define	btodb(x)	((x) >> DEV_BSHIFT)

#ifndef COHERENCY_UNIT
#define	COHERENCY_UNIT		64
#endif
#ifndef CACHE_LINE_SIZE
#define	CACHE_LINE_SIZE		64
#endif
#ifndef MAXCPUS
#define	MAXCPUS			32
#endif
#ifndef MAX_LWP_PER_PROC
#define	MAX_LWP_PER_PROC	8000
#endif

/*
 * Stack macros.  On most architectures, the stack grows down,
 * towards lower addresses; it is the rare architecture where
 * it grows up, towards higher addresses.
 *
 * STACK_GROW and STACK_SHRINK adjust a stack pointer by some
 * size, no questions asked.  STACK_ALIGN aligns a stack pointer.
 *
 * STACK_ALLOC returns a pointer to allocated stack space of
 * some size; given such a pointer and a size, STACK_MAX gives
 * the maximum (in the "maxsaddr" sense) stack address of the
 * allocated memory.
 */
#if defined(_KERNEL) || defined(__EXPOSE_STACK)

#ifndef STACK_ALIGNBYTES
#define STACK_ALIGNBYTES	__ALIGNBYTES
#endif

#ifdef __MACHINE_STACK_GROWS_UP
#define	STACK_GROW(sp, _size)		(((char *)(void *)(sp)) + (_size))
#define	STACK_SHRINK(sp, _size)		(((char *)(void *)(sp)) - (_size))
#define	STACK_ALIGN(sp, bytes)	\
	((char *)((((unsigned long)(sp)) + (bytes)) & ~(bytes)))
#define	STACK_ALLOC(sp, _size)		((char *)(void *)(sp))
#define	STACK_MAX(p, _size)		(((char *)(void *)(p)) + (_size))
#else
#define	STACK_GROW(sp, _size)		(((char *)(void *)(sp)) - (_size))
#define	STACK_SHRINK(sp, _size)		(((char *)(void *)(sp)) + (_size))
#define	STACK_ALIGN(sp, bytes)	\
	((char *)(((unsigned long)(sp)) & ~(bytes)))
#define	STACK_ALLOC(sp, _size)		(((char *)(void *)(sp)) - (_size))
#define	STACK_MAX(p, _size)		((char *)(void *)(p))
#endif
#define	STACK_LEN_ALIGN(len, bytes)	(((len) + (bytes)) & ~(bytes))

#endif /* defined(_KERNEL) || defined(__EXPOSE_STACK) */

/*
 * Round p (pointer or byte index) up to a correctly-aligned value for all
 * data types (int, long, ...).   The result is u_int and must be cast to
 * any desired pointer type.
 *
 * ALIGNED_POINTER is a boolean macro that checks whether an address
 * is valid to fetch data elements of type t from on this architecture.
 * This does not reflect the optimal alignment, just the possibility
 * (within reasonable limits).
 *
 */
#define ALIGNBYTES	__ALIGNBYTES
#ifndef ALIGN
#define	ALIGN(p)		(((uintptr_t)(p) + ALIGNBYTES) & ~ALIGNBYTES)
#endif
#ifndef ALIGNED_POINTER
#define	ALIGNED_POINTER(p,t)	((((uintptr_t)(p)) & (sizeof(t) - 1)) == 0)
#endif

/*
 * Historic priority levels.  These are meaningless and remain only
 * for source compatibility.  Do not use in new code.
 */
#define	PSWP	0
#define	PVM	4
#define	PINOD	8
#define	PRIBIO	16
#define	PVFS	20
#define	PZERO	22
#define	PSOCK	24
#define	PWAIT	32
#define	PLOCK	36
#define	PPAUSE	40
#define	PUSER	50
#define	MAXPRI	127

#define	PCATCH		0x100	/* OR'd with pri for tsleep to check signals */
#define	PNORELOCK	0x200	/* OR'd with pri for tsleep to not relock */

/*
 * New priority levels.
 */
#define	PRI_COUNT		224
#define	PRI_NONE		(-1)

#define	PRI_KERNEL_RT		192
#define	NPRI_KERNEL_RT		32
#define	MAXPRI_KERNEL_RT	(PRI_KERNEL_RT + NPRI_KERNEL_RT - 1)

#define	PRI_USER_RT		128
#define	NPRI_USER_RT		64
#define	MAXPRI_USER_RT		(PRI_USER_RT + NPRI_USER_RT - 1)

#define	PRI_KTHREAD		96
#define	NPRI_KTHREAD		32
#define	MAXPRI_KTHREAD		(PRI_KTHREAD + NPRI_KTHREAD - 1)

#define	PRI_KERNEL		64
#define	NPRI_KERNEL		32
#define	MAXPRI_KERNEL		(PRI_KERNEL + NPRI_KERNEL - 1)

#define	PRI_USER		0
#define	NPRI_USER		64
#define	MAXPRI_USER		(PRI_USER + NPRI_USER - 1)

/* Priority range used by POSIX real-time features */
#define	SCHED_PRI_MIN		0
#define	SCHED_PRI_MAX		63

/*
 * Kernel thread priorities.
 */
#define	PRI_SOFTSERIAL	MAXPRI_KERNEL_RT
#define	PRI_SOFTNET	(MAXPRI_KERNEL_RT - schedppq * 1)
#define	PRI_SOFTBIO	(MAXPRI_KERNEL_RT - schedppq * 2)
#define	PRI_SOFTCLOCK	(MAXPRI_KERNEL_RT - schedppq * 3)

#define	PRI_XCALL	MAXPRI_KTHREAD
#define	PRI_PGDAEMON	(MAXPRI_KTHREAD - schedppq * 1)
#define	PRI_VM		(MAXPRI_KTHREAD - schedppq * 2)
#define	PRI_IOFLUSH	(MAXPRI_KTHREAD - schedppq * 3)
#define	PRI_BIO		(MAXPRI_KTHREAD - schedppq * 4)

#define	PRI_IDLE	PRI_USER

/*
 * Miscellaneous.
 */
#define	NBPW	sizeof(int)	/* number of bytes per word (integer) */

#define	CMASK	022		/* default file mask: S_IWGRP|S_IWOTH */
#define	NODEV	(dev_t)(-1)	/* non-existent device */

/*
 * File system parameters and macros.
 *
 * The file system is made out of blocks of at most MAXBSIZE units, with
 * smaller units (fragments) only in the last direct block.  MAXBSIZE
 * primarily determines the size of buffers in the buffer pool.  It may be
 * made larger without any effect on existing file systems; however making
 * it smaller may make some file systems unmountable.
 */
#ifndef MAXBSIZE				/* XXX */
#define	MAXBSIZE	MAXPHYS
#endif
#define	MAXFRAG 	8

/*
 * MAXPATHLEN defines the longest permissible path length after expanding
 * symbolic links. It is used to allocate a temporary buffer from the buffer
 * pool in which to do the name expansion, hence should be a power of two,
 * and must be less than or equal to MAXBSIZE.  MAXSYMLINKS defines the
 * maximum number of symbolic links that may be expanded in a path name.
 * It should be set high enough to allow all legitimate uses, but halt
 * infinite loops reasonably quickly.
 *
 * MAXSYMLINKS should be >= _POSIX_SYMLOOP_MAX (see <limits.h>)
 */
#define	MAXPATHLEN	PATH_MAX
#define	MAXSYMLINKS	32

/*
 * This is the maximum individual filename component length enforced by
 * namei. Filesystems cannot exceed this limit. The upper bound for that
 * limit is NAME_MAX. We don't bump it for now, for compatibility with
 * old binaries during the time where MAXPATHLEN was 511 and NAME_MAX was
 * 255
 */
#define	KERNEL_NAME_MAX	255

/* Bit map related macros. */
#define	setbit(a,i)	((a)[(i)/NBBY] |= 1<<((i)%NBBY))
#define	clrbit(a,i)	((a)[(i)/NBBY] &= ~(1<<((i)%NBBY)))
#define	isset(a,i)	((a)[(i)/NBBY] & (1<<((i)%NBBY)))
#define	isclr(a,i)	(((a)[(i)/NBBY] & (1<<((i)%NBBY))) == 0)

/* Macros for counting and rounding. */
#ifndef howmany
#define	howmany(x, y)	(((x)+((y)-1))/(y))
#endif
#define	roundup(x, y)	((((x)+((y)-1))/(y))*(y))
#define	rounddown(x,y)	(((x)/(y))*(y))

/*
 * Rounding to powers of two.  The naive definitions of roundup2 and
 * rounddown2,
 *
 *	#define	roundup2(x,m)	(((x) + ((m) - 1)) & ~((m) - 1))
 *	#define	rounddown2(x,m)	((x) & ~((m) - 1)),
 *
 * exhibit a quirk of integer arithmetic in C because the complement
 * happens in the type of m, not in the type of x.  So if unsigned int
 * is 32-bit, and m is an unsigned int while x is a uint64_t, then
 * roundup2 and rounddown2 would have the unintended effect of clearing
 * the upper 32 bits of the result(!).  These definitions avoid the
 * pitfalls of C arithmetic depending on the types of x and m, and
 * additionally avoid multiply evaluating their arguments.
 */
#define	roundup2(x,m)	((((x) - 1) | ((m) - 1)) + 1)
#define	rounddown2(x,m)	((x) & ~((__typeof__(x))((m) - 1)))

#define	powerof2(x)	((((x)-1)&(x))==0)

/*
 * Constants for setting the parameters of the kernel memory allocator.
 *
 * 2 ** MINBUCKET is the smallest unit of memory that will be
 * allocated. It must be at least large enough to hold a pointer.
 *
 * Units of memory less or equal to MAXALLOCSAVE will permanently
 * allocate physical memory; requests for these size pieces of
 * memory are quite fast. Allocations greater than MAXALLOCSAVE must
 * always allocate and free physical memory; requests for these
 * size allocations should be done infrequently as they will be slow.
 *
 * Constraints: NBPG <= MAXALLOCSAVE <= 2 ** (MINBUCKET + 14), and
 * MAXALLOCSAVE must be a power of two.
 */
#ifdef _LP64
#define	MINBUCKET	5		/* 5 => min allocation of 32 bytes */
#else
#define	MINBUCKET	4		/* 4 => min allocation of 16 bytes */
#endif
#define	MAXALLOCSAVE	(2 * NBPG)

/*
 * Scale factor for scaled integers used to count %cpu time and load avgs.
 *
 * The number of CPU `tick's that map to a unique `%age' can be expressed
 * by the formula (1 / (2 ^ (FSHIFT - 11))).  The maximum load average that
 * can be calculated (assuming 32 bits) can be closely approximated using
 * the formula (2 ^ (2 * (16 - FSHIFT))) for (FSHIFT < 15).
 *
 * For the scheduler to maintain a 1:1 mapping of CPU `tick' to `%age',
 * FSHIFT must be at least 11; this gives us a maximum load avg of ~1024.
 */
#define	FSHIFT	11		/* bits to right of fixed binary point */
#define	FSCALE	(1<<FSHIFT)

/*
 * The time for a process to be blocked before being very swappable.
 * This is a number of seconds which the system takes as being a non-trivial
 * amount of real time.  You probably shouldn't change this;
 * it is used in subtle ways (fractions and multiples of it are, that is, like
 * half of a ``long time'', almost a long time, etc.)
 * It is related to human patience and other factors which don't really
 * change over time.
 */
#define        MAXSLP          20

/*
 * Defaults for Unified Buffer Cache parameters.
 * These may be overridden in <machine/param.h>.
 */

#ifndef UBC_WINSHIFT
#define	UBC_WINSHIFT	13
#endif
#ifndef UBC_NWINS
#define	UBC_NWINS	1024
#endif

#ifdef _KERNEL
/*
 * macro to convert from milliseconds to hz without integer overflow
 * Default version using only 32bits arithmetics.
 * 64bit port can define 64bit version in their <machine/param.h>
 * 0x20000 is safe for hz < 20000
 */
#ifndef mstohz
#define mstohz(ms) \
	(__predict_false((ms) >= 0x20000) ? \
	    ((ms +0u) / 1000u) * hz : \
	    ((ms +0u) * hz) / 1000u)
#endif
#ifndef hztoms
#define hztoms(t) \
	(__predict_false((t) >= 0x20000) ? \
	    ((t +0u) / hz) * 1000u : \
	    ((t +0u) * 1000u) / hz)
#endif

extern const int schedppq;
extern size_t coherency_unit;

#endif /* _KERNEL */

/*
 * Minimum alignment of "struct lwp" needed by the architecture.
 * This counts when packing a lock byte into a word alongside a
 * pointer to an LWP.
 */
#ifndef MIN_LWP_ALIGNMENT
#define	MIN_LWP_ALIGNMENT	32
#endif
#endif /* !__ASSEMBLER__ */

#endif /* !_SYS_PARAM_H_ */

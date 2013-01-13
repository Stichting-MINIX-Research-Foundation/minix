/*	$NetBSD: param.h,v 1.421 2012/10/13 17:54:40 dholland Exp $	*/

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

/*
 * Historic BSD #defines -- probably will remain untouched for all time.
 */
#define	BSD	199506		/* System version (year & month). */
#define	BSD4_3	1
#ifndef __minix
#define	BSD4_4	1
#endif

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

#define	__NetBSD_Version__	699001300	/* NetBSD 6.99.13 */

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
#ifndef __minix
#define	NCARGS		ARG_MAX		/* max bytes for an exec function */
#endif
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
 * Miscellaneous.
 */
#define	NBPW	sizeof(int)	/* number of bytes per word (integer) */

#define	CMASK	022		/* default file mask: S_IWGRP|S_IWOTH */
#define	NODEV	(dev_t)(-1)	/* non-existent device */

#define	CBLOCK	64		/* Clist block size, must be a power of 2. */
#define	CBQSIZE	(CBLOCK/NBBY)	/* Quote bytes/cblock - can do better. */
				/* Data chars/clist. */
#define	CBSIZE	(CBLOCK - (int)sizeof(struct cblock *) - CBQSIZE)
#define	CROUND	(CBLOCK - 1)	/* Clist rounding. */

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
#define	roundup2(x, m)	(((x) + (m) - 1) & ~((m) - 1))
#define	powerof2(x)	((((x)-1)&(x))==0)

/* Macros for min/max. */
#define	MIN(a,b)	((/*CONSTCOND*/(a)<(b))?(a):(b))
#define	MAX(a,b)	((/*CONSTCOND*/(a)>(b))?(a):(b))

#endif /* !__ASSEMBLER__ */

#endif /* !_SYS_PARAM_H_ */

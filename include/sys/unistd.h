/*	$NetBSD: unistd.h,v 1.52 2009/08/30 16:38:48 christos Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)unistd.h	8.2 (Berkeley) 1/7/94
 */

#ifndef _SYS_UNISTD_H_
#define	_SYS_UNISTD_H_

#include <sys/featuretest.h>

/* compile-time symbolic constants */
#define	_POSIX_JOB_CONTROL	1
				/* implementation supports job control */

/*
 * According to POSIX 1003.1:
 * "The saved set-user-ID capability allows a program to regain the
 * effective user ID established at the last exec call."
 * However, the setuid/setgid function as specified by POSIX 1003.1 does
 * not allow changing the effective ID from the super-user without also
 * changed the saved ID, so it is impossible to get super-user privileges
 * back later.  Instead we provide this feature independent of the current
 * effective ID through the seteuid/setegid function.  In addition, we do
 * not use the saved ID as specified by POSIX 1003.1 in setuid/setgid,
 * because this would make it impossible for a set-user-ID executable
 * owned by a user other than the super-user to permanently revoke its
 * extra privileges.
 */
#ifdef	_NOT_AVAILABLE
#define	_POSIX_SAVED_IDS	1
				/* saved set-user-ID and set-group-ID */
#endif

#define	_POSIX_VERSION			200112L
#define	_POSIX2_VERSION			200112L

/* execution-time symbolic constants */

/*
 * POSIX options and option groups we unconditionally do or don't
 * implement.  Those options which are implemented (or not) entirely
 * in user mode are defined in <unistd.h>.  Please keep this list in
 * alphabetical order.
 *
 * Anything which is defined as zero below **must** have an
 * implementation for the corresponding sysconf() which is able to
 * determine conclusively whether or not the feature is supported.
 * Anything which is defined as other than -1 below **must** have
 * complete headers, types, and function declarations as specified by
 * the POSIX standard; however, if the relevant sysconf() function
 * returns -1, the functions may be stubbed out.
 */
					/* Advisory information */
#undef	_POSIX_ADVISORY_INFO
					/* asynchronous I/O is available */
#define	_POSIX_ASYNCHRONOUS_IO		200112L
					/* barriers */
#define	_POSIX_BARRIERS			200112L
					/* chown requires correct privileges */
#define	_POSIX_CHOWN_RESTRICTED		1
					/* clock selection */
#define	_POSIX_CLOCK_SELECTION		-1
					/* CPU type */
#undef	_POSIX_CPUTYPE
					/* file synchronization is available */
#define	_POSIX_FSYNC			1
					/* support IPv6 */
#define	_POSIX_IPV6			0
					/* job control is available */
#define	_POSIX_JOB_CONTROL		1
					/* memory mapped files */
#define	_POSIX_MAPPED_FILES		1
					/* memory locking whole address space */
#define	_POSIX_MEMLOCK			1
					/* memory locking address ranges */
#define	_POSIX_MEMLOCK_RANGE		1
					/* memory access protections */
#define	_POSIX_MEMORY_PROTECTION	1
					/* message passing is available */
#define	_POSIX_MESSAGE_PASSING		200112L
					/* monotonic clock */
#define	_POSIX_MONOTONIC_CLOCK		200112L
					/* too-long path comp generate errors */
#define	_POSIX_NO_TRUNC			1
					/* prioritized I/O */
#define	_POSIX_PRIORITIZED_IO		-1
					/* priority scheduling */
#define	_POSIX_PRIORITY_SCHEDULING	200112L
					/* raw sockets */
#define	_POSIX_RAW_SOCKETS		200112L
					/* read/write locks */
#define	_POSIX_READER_WRITER_LOCKS	200112L
					/* realtime signals */
#undef	_POSIX_REALTIME_SIGNALS
					/* regular expressions */
#define	_POSIX_REGEXP			1
					/* semaphores */
#define	_POSIX_SEMAPHORES		0
					/* shared memory */
#undef	_POSIX_SHARED_MEMORY_OBJECTS
					/* shell */
#define	_POSIX_SHELL			1
					/* spin locks */
#define	_POSIX_SPIN_LOCKS		200112L
					/* sporadic server */
#undef	_POSIX_SPORADIC_SERVER
					/* synchronized I/O is available */
#define	_POSIX_SYNCHRONIZED_IO		1
					/* threads */
#define	_POSIX_THREADS			200112L
					/* pthread_attr for stack size */
#define	_POSIX_THREAD_ATTR_STACKSIZE	200112L
					/* pthread_attr for stack address */
#define	_POSIX_THREAD_ATTR_STACKADDR	200112L
					/* _r functions */
#define	_POSIX_THREAD_SAFE_FUNCTIONS	200112L
					/* timeouts */
#undef	_POSIX_TIMEOUTS
					/* timers */
#undef	_POSIX_TIMERS
					/* typed memory objects */
#undef	_POSIX_TYPED_MEMORY_OBJECTS
					/* may disable terminal spec chars */
#define	_POSIX_VDISABLE			__CAST(unsigned char, '\377')

					/* C binding */
#define	_POSIX2_C_BIND			200112L

					/* XPG4.2 shared memory */
#define	_XOPEN_SHM			0

/* access function */
#define	F_OK		0	/* test for existence of file */
#define	X_OK		0x01	/* test for execute or search permission */
#define	W_OK		0x02	/* test for write permission */
#define	R_OK		0x04	/* test for read permission */

/* whence values for lseek(2) */
#define	SEEK_SET	0	/* set file offset to offset */
#define	SEEK_CUR	1	/* set file offset to current plus offset */
#define	SEEK_END	2	/* set file offset to EOF plus offset */

#if defined(_NETBSD_SOURCE)
/* whence values for lseek(2); renamed by POSIX 1003.1 */
#define	L_SET		SEEK_SET
#define	L_INCR		SEEK_CUR
#define	L_XTND		SEEK_END
#endif

/* configurable pathname variables; use as argument to pathconf(3) */
#define _PC_LINK_MAX	   1	/* link count */
#define _PC_MAX_CANON	   2	/* size of the canonical input queue */
#define _PC_MAX_INPUT	   3	/* type-ahead buffer size */
#define _PC_NAME_MAX	   4	/* file name size */
#define _PC_PATH_MAX	   5	/* pathname size */
#define _PC_PIPE_BUF	   6	/* pipe size */
#define _PC_NO_TRUNC	   7	/* treatment of long name components */
#define _PC_VDISABLE	   8	/* tty disable */
#define _PC_CHOWN_RESTRICTED 9	/* chown restricted or not */

/* configurable system variables; use as argument to sysconf(3) */
/*
 * XXX The value of _SC_CLK_TCK is embedded in <time.h>.
 * XXX The value of _SC_PAGESIZE is embedded in <sys/shm.h>.
 */
#define _SC_ARG_MAX	   1
#define _SC_CHILD_MAX	   2
#define _SC_CLOCKS_PER_SEC 3
#define _SC_CLK_TCK	   3
#define _SC_NGROUPS_MAX	   4
#define _SC_OPEN_MAX	   5
#define _SC_JOB_CONTROL	   6
#define _SC_SAVED_IDS	   7
#define _SC_VERSION	   8
#define _SC_STREAM_MAX	   9
#define _SC_TZNAME_MAX    10
#define _SC_PAGESIZE	  11
#define _SC_PAGE_SIZE	  _SC_PAGESIZE

#endif /* !_SYS_UNISTD_H_ */

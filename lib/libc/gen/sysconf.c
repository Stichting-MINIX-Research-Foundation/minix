/*	$NetBSD: sysconf.c,v 1.36 2013/12/19 19:11:50 rmind Exp $	*/

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Sean Eric Fagan of Cygnus Support.
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
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)sysconf.c	8.2 (Berkeley) 3/20/94";
#else
__RCSID("$NetBSD: sysconf.c,v 1.36 2013/12/19 19:11:50 rmind Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <errno.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <paths.h>
#include <pwd.h>

#ifdef __weak_alias
__weak_alias(sysconf,__sysconf)
#endif

/*
 * sysconf --
 *	get configurable system variables.
 *
 * XXX
 * POSIX 1003.1 (ISO/IEC 9945-1, 4.8.1.3) states that the variable values
 * not change during the lifetime of the calling process.  This would seem
 * to require that any change to system limits kill all running processes.
 * A workaround might be to cache the values when they are first retrieved
 * and then simply return the cached value on subsequent calls.  This is
 * less useful than returning up-to-date values, however.
 */
long
sysconf(int name)
{
	struct rlimit rl;
	size_t len;
	uint64_t mem;
	int mib[CTL_MAXNAME], value;
	unsigned int mib_len;
	struct clockinfo tmpclock;
	static int clk_tck;

	len = sizeof(value);

	/* Default length of the MIB */
	mib_len = 2;

	switch (name) {

/* 1003.1 */
	case _SC_ARG_MAX:
		mib[0] = CTL_KERN;
		mib[1] = KERN_ARGMAX;
		break;
	case _SC_CHILD_MAX:
		return (getrlimit(RLIMIT_NPROC, &rl) ? -1 : (long)rl.rlim_cur);
	case _O_SC_CLK_TCK:
		/*
		 * For applications compiled when CLK_TCK was a compile-time
		 * constant.
		 */
		return 100;
	case _SC_CLK_TCK:
		/*
		 * Has to be handled specially because it returns a
		 * struct clockinfo instead of an integer. Also, since
		 * this might be called often by some things that
		 * don't grok CLK_TCK can be a macro expanding to a
		 * function, cache the value.
		 */
		if (clk_tck == 0) {
			mib[0] = CTL_KERN;
			mib[1] = KERN_CLOCKRATE;
			len = sizeof(struct clockinfo);
			clk_tck = sysctl(mib, 2, &tmpclock, &len, NULL, 0)
			    == -1 ? -1 : tmpclock.hz;
		}
		return(clk_tck);
	case _SC_JOB_CONTROL:
		mib[0] = CTL_KERN;
		mib[1] = KERN_JOB_CONTROL;
		goto yesno;
	case _SC_NGROUPS_MAX:
		mib[0] = CTL_KERN;
		mib[1] = KERN_NGROUPS;
		break;
	case _SC_OPEN_MAX:
		return (getrlimit(RLIMIT_NOFILE, &rl) ? -1 : (long)rl.rlim_cur);
	case _SC_STREAM_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_STREAM_MAX;
		break;
	case _SC_TZNAME_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_TZNAME_MAX;
		break;
	case _SC_SAVED_IDS:
		mib[0] = CTL_KERN;
		mib[1] = KERN_SAVED_IDS;
		goto yesno;
	case _SC_VERSION:
		mib[0] = CTL_KERN;
		mib[1] = KERN_POSIX1;
		break;

/* 1003.1b */
	case _SC_PAGESIZE:
		return _getpagesize();
	case _SC_FSYNC:
		mib[0] = CTL_KERN;
		mib[1] = KERN_FSYNC;
		goto yesno;
	case _SC_SYNCHRONIZED_IO:
		mib[0] = CTL_KERN;
		mib[1] = KERN_SYNCHRONIZED_IO;
		goto yesno;
	case _SC_MAPPED_FILES:
		mib[0] = CTL_KERN;
		mib[1] = KERN_MAPPED_FILES;
		goto yesno;
	case _SC_MEMLOCK:
		mib[0] = CTL_KERN;
		mib[1] = KERN_MEMLOCK;
		goto yesno;
	case _SC_MEMLOCK_RANGE:
		mib[0] = CTL_KERN;
		mib[1] = KERN_MEMLOCK_RANGE;
		goto yesno;
	case _SC_MEMORY_PROTECTION:
		mib[0] = CTL_KERN;
		mib[1] = KERN_MEMORY_PROTECTION;
		goto yesno;
	case _SC_MONOTONIC_CLOCK:
		mib[0] = CTL_KERN;
		mib[1] = KERN_MONOTONIC_CLOCK;
		goto yesno;
	case _SC_SEMAPHORES:
		mib[0] = CTL_KERN;
		mib[1] = KERN_POSIX_SEMAPHORES;
		goto yesno;
	case _SC_TIMERS:
		mib[0] = CTL_KERN;
		mib[1] = KERN_POSIX_TIMERS;
		goto yesno;

/* 1003.1c */
	case _SC_LOGIN_NAME_MAX:
		mib[0] = CTL_KERN;
		mib[1] = KERN_LOGIN_NAME_MAX;
		break;
	case _SC_THREADS:
		mib[0] = CTL_KERN;
		mib[1] = KERN_POSIX_THREADS;
		goto yesno;

/* 1003.1j */
	case _SC_BARRIERS:
		mib[0] = CTL_KERN;
		mib[1] = KERN_POSIX_BARRIERS;
		goto yesno;
	case _SC_SPIN_LOCKS:
		mib[0] = CTL_KERN;
		mib[1] = KERN_POSIX_SPIN_LOCKS;
		goto yesno;
	/* Historical; Threads option in 1003.1-2001 */
	case _SC_READER_WRITER_LOCKS:
		mib[0] = CTL_KERN;
		mib[1] = KERN_POSIX_READER_WRITER_LOCKS;
		goto yesno;

/* 1003.2 */
	case _SC_BC_BASE_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_BC_BASE_MAX;
		break;
	case _SC_BC_DIM_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_BC_DIM_MAX;
		break;
	case _SC_BC_SCALE_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_BC_SCALE_MAX;
		break;
	case _SC_BC_STRING_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_BC_STRING_MAX;
		break;
	case _SC_COLL_WEIGHTS_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_COLL_WEIGHTS_MAX;
		break;
	case _SC_EXPR_NEST_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_EXPR_NEST_MAX;
		break;
	case _SC_LINE_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_LINE_MAX;
		break;
	case _SC_RE_DUP_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_RE_DUP_MAX;
		break;
	case _SC_2_VERSION:
		mib[0] = CTL_USER;
		mib[1] = USER_POSIX2_VERSION;
		break;
	case _SC_2_C_BIND:
		mib[0] = CTL_USER;
		mib[1] = USER_POSIX2_C_BIND;
		goto yesno;
	case _SC_2_C_DEV:
		mib[0] = CTL_USER;
		mib[1] = USER_POSIX2_C_DEV;
		goto yesno;
	case _SC_2_CHAR_TERM:
		mib[0] = CTL_USER;
		mib[1] = USER_POSIX2_CHAR_TERM;
		goto yesno;
	case _SC_2_FORT_DEV:
		mib[0] = CTL_USER;
		mib[1] = USER_POSIX2_FORT_DEV;
		goto yesno;
	case _SC_2_FORT_RUN:
		mib[0] = CTL_USER;
		mib[1] = USER_POSIX2_FORT_RUN;
		goto yesno;
	case _SC_2_LOCALEDEF:
		mib[0] = CTL_USER;
		mib[1] = USER_POSIX2_LOCALEDEF;
		goto yesno;
	case _SC_2_SW_DEV:
		mib[0] = CTL_USER;
		mib[1] = USER_POSIX2_SW_DEV;
		goto yesno;
	case _SC_2_UPE:
		mib[0] = CTL_USER;
		mib[1] = USER_POSIX2_UPE;
		goto yesno;

/* XPG 4.2 */
	case _SC_IOV_MAX:
		mib[0] = CTL_KERN;
		mib[1] = KERN_IOV_MAX;
		break;
	case _SC_XOPEN_SHM:
		mib[0] = CTL_KERN;
		mib[1] = KERN_SYSVIPC;
		mib[2] = KERN_SYSVIPC_SHM;
		mib_len = 3;
		goto yesno;

/* 1003.1-2001, XSI Option Group */
	case _SC_AIO_LISTIO_MAX:
		if (sysctlgetmibinfo("kern.aio_listio_max", &mib[0], &mib_len,
		    NULL, NULL, NULL, SYSCTL_VERSION))
			return -1;
		break; 
	case _SC_AIO_MAX:
		if (sysctlgetmibinfo("kern.aio_max", &mib[0], &mib_len,
		    NULL, NULL, NULL, SYSCTL_VERSION))
			return -1;
		break; 
	case _SC_ASYNCHRONOUS_IO:
		if (sysctlgetmibinfo("kern.posix_aio", &mib[0], &mib_len,
		    NULL, NULL, NULL, SYSCTL_VERSION))
			return -1;
		goto yesno;
	case _SC_MESSAGE_PASSING:
		if (sysctlgetmibinfo("kern.posix_msg", &mib[0], &mib_len,
		    NULL, NULL, NULL, SYSCTL_VERSION))
			return -1;
		goto yesno;
	case _SC_MQ_OPEN_MAX:
		if (sysctlgetmibinfo("kern.mqueue.mq_open_max", &mib[0],
		    &mib_len, NULL, NULL, NULL, SYSCTL_VERSION))
			return -1;
		break; 
	case _SC_MQ_PRIO_MAX:
		if (sysctlgetmibinfo("kern.mqueue.mq_prio_max", &mib[0],
		    &mib_len, NULL, NULL, NULL, SYSCTL_VERSION))
			return -1;
		break; 
	case _SC_PRIORITY_SCHEDULING:
		if (sysctlgetmibinfo("kern.posix_sched", &mib[0], &mib_len,
		    NULL, NULL, NULL, SYSCTL_VERSION))
			return -1;
		goto yesno;
	case _SC_ATEXIT_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_ATEXIT_MAX;
		break;

/* 1003.1-2001, TSF */
	case _SC_GETGR_R_SIZE_MAX:
		return _GETGR_R_SIZE_MAX;
	case _SC_GETPW_R_SIZE_MAX:
		return _GETPW_R_SIZE_MAX;

/* Unsorted */
	case _SC_HOST_NAME_MAX:
		return MAXHOSTNAMELEN;
	case _SC_PASS_MAX:
		return _PASSWORD_LEN;
	case _SC_REGEXP:
		return _POSIX_REGEXP;
	case _SC_SHARED_MEMORY_OBJECTS:
		return _POSIX_SHARED_MEMORY_OBJECTS;
	case _SC_SHELL:
		return _POSIX_SHELL;
#ifndef __minix
	case _SC_SPAWN:
		return _POSIX_SPAWN;
#endif /* !__minix */
	case _SC_SYMLOOP_MAX:
		return MAXSYMLINKS;

yesno:		if (sysctl(mib, mib_len, &value, &len, NULL, 0) == -1)
			return (-1);
		if (value == 0)
			return (-1);
		return (value);

/* Extensions */
	case _SC_NPROCESSORS_CONF:
		mib[0] = CTL_HW;
		mib[1] = HW_NCPU;
		break;
	case _SC_NPROCESSORS_ONLN:
		mib[0] = CTL_HW;
		mib[1] = HW_NCPUONLINE;
		break;

/* Linux/Solaris */
	case _SC_PHYS_PAGES:
		len = sizeof(mem);
		mib[0] = CTL_HW;
		mib[1] = HW_PHYSMEM64;
		return sysctl(mib, 2, &mem, &len, NULL, 0) == -1 ? -1 : 
		    (long)(mem / _getpagesize()); 

/* Native */
	case _SC_SCHED_RT_TS:
		if (sysctlgetmibinfo("kern.sched.rtts", &mib[0], &mib_len,
		    NULL, NULL, NULL, SYSCTL_VERSION))      
			return -1;              
		break;
	case _SC_SCHED_PRI_MIN:
		if (sysctlgetmibinfo("kern.sched.pri_min", &mib[0], &mib_len,
		    NULL, NULL, NULL, SYSCTL_VERSION))
			return -1;
		break;
	case _SC_SCHED_PRI_MAX:
		if (sysctlgetmibinfo("kern.sched.pri_max", &mib[0], &mib_len,
		    NULL, NULL, NULL, SYSCTL_VERSION))
			return -1;
		break;
	case _SC_THREAD_DESTRUCTOR_ITERATIONS:
		return _POSIX_THREAD_DESTRUCTOR_ITERATIONS;
	case _SC_THREAD_KEYS_MAX:
		return _POSIX_THREAD_KEYS_MAX;
	case _SC_THREAD_STACK_MIN:
		return _getpagesize();
	case _SC_THREAD_THREADS_MAX:
		if (sysctlgetmibinfo("kern.maxproc", &mib[0], &mib_len,
		    NULL, NULL, NULL, SYSCTL_VERSION))	/* XXX */
			return -1;
		goto yesno;
	case _SC_THREAD_ATTR_STACKADDR:
		return _POSIX_THREAD_ATTR_STACKADDR;
	case _SC_THREAD_ATTR_STACKSIZE:
		return _POSIX_THREAD_ATTR_STACKSIZE;
	case _SC_THREAD_SAFE_FUNCTIONS:
		return _POSIX_THREAD_SAFE_FUNCTIONS;
	case _SC_THREAD_PRIORITY_SCHEDULING:
	case _SC_THREAD_PRIO_INHERIT:
	case _SC_THREAD_PRIO_PROTECT:
	case _SC_THREAD_PROCESS_SHARED:
		return -1;
	case _SC_TTY_NAME_MAX:
		return pathconf(_PATH_DEV, _PC_NAME_MAX);
	default:
		errno = EINVAL;
		return (-1);
	}
	return (sysctl(mib, mib_len, &value, &len, NULL, 0) == -1 ? -1 : value); 
}

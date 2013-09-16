/*	$NetBSD: syscall_stats.h,v 1.4 2008/11/12 12:36:28 ad Exp $	*/

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by David Laight.
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

#ifndef _SYS_SYCALL_STAT_H_
#define	_SYS_SYCALL_STAT_H_

#ifdef _KERNEL_OPT
#include "opt_syscall_stats.h"
#endif

#ifdef SYSCALL_STATS
#include <sys/syscall.h>

extern uint64_t syscall_counts[SYS_NSYSENT];
extern uint64_t syscall_count_user, syscall_count_system, syscall_count_interrupt;
#define	SYSCALL_COUNT(table, code) ((table)[code]++)

#if defined(SYSCALL_TIMES) && defined(__HAVE_CPU_COUNTER)

#include <machine/cpu_counter.h>
extern uint64_t syscall_times[SYS_NSYSENT];

#ifdef SYSCALL_TIMES_HASCOUNTER
/* Force use of cycle counter - needed for Soekris systems */
#define SYSCALL_TIME() (cpu_counter32())
#else
#define SYSCALL_TIME() (cpu_hascounter() ? cpu_counter32() : 0u)
#endif

#ifdef SYSCALL_TIMES_PROCTIMES
#define SYSCALL_TIME_UPDATE_PROC(l, fld, delta) \
	(l)->l_proc->p_##fld##ticks += (delta)
#else
#define SYSCALL_TIME_UPDATE_PROC(l, fld, delta)
#endif

/* lwp creation */
#define SYSCALL_TIME_LWP_INIT(l) do { \
    (l)->l_syscall_counter = &syscall_count_system; \
    SYSCALL_TIME_WAKEUP(l); \
} while (0)

/* System call entry hook */
#define SYSCALL_TIME_SYS_ENTRY(l, table, code) do { \
	uint32_t now = SYSCALL_TIME(); \
	SYSCALL_TIME_UPDATE_PROC(l, u, elapsed = now - (l)->l_syscall_time); \
	(l)->l_syscall_counter = (table) + (code); \
	(l)->l_syscall_time = now; \
} while (0)

/* System call - process sleep */
#define SYSCALL_TIME_SLEEP(l) do { \
	uint32_t now = SYSCALL_TIME(); \
	uint32_t elapsed = now - (l)->l_syscall_time; \
	(l)->l_syscall_time = now; \
	*(l)->l_syscall_counter += elapsed; \
	SYSCALL_TIME_UPDATE_PROC(l, s, elapsed); \
} while (0)

/* Process wakeup */
#define SYSCALL_TIME_WAKEUP(l) \
	(l)->l_syscall_time = SYSCALL_TIME()

/* System call exit */
#define SYSCALL_TIME_SYS_EXIT(l) do { \
	uint32_t now = SYSCALL_TIME(); \
	uint32_t elapsed = now - (l)->l_syscall_time; \
	(l)->l_syscall_time = now; \
	*(l)->l_syscall_counter += elapsed; \
	(l)->l_syscall_counter = &syscall_count_user; \
	SYSCALL_TIME_UPDATE_PROC(l, s, elapsed); \
} while (0)

#ifdef _notyet
/* Interrupt entry hook */
#define SYSCALL_TIME_ISR_ENTRY(l, old) do { \
	uint32_t now = SYSCALL_TIME(); \
	uint32_t elapsed = now - (l)->l_syscall_time; \
	(l)->l_syscall_time = now; \
	old = (l)->l_syscall_counter; \
	if ((l)->l_syscall_counter != &syscall_count_interrupt) \
		if ((l)->l_syscall_counter == &syscall_count_user) \
			SYSCALL_TIME_UPDATE_PROC(l, u, elapsed); \
		else { \
			*(l)->l_syscall_counter += elapsed; \
			SYSCALL_TIME_UPDATE_PROC(l, s, elapsed); \
		} \
		(l)->l_syscall_counter = &syscall_count_interrupt; \
	} \
} while (0)

/* Interrupt exit hook */
#define SYSCALL_TIME_ISR_EXIT(l, saved) do { \
	uint32_t now = SYSCALL_TIME(); \
	SYSCALL_TIME_UPDATE_PROC(l, i, now - (l)->l_syscall_time); \
	(l)->l_syscall_time = now; \
	(l)->l_syscall_counter = saved; \
} while (0)
#endif

#endif
#endif

#ifndef SYSCALL_TIME_SYS_ENTRY
#define SYSCALL_TIME_LWP_INIT(l)
#define SYSCALL_TIME_SYS_ENTRY(l,table,code)
#define SYSCALL_TIME_SLEEP(l)
#define SYSCALL_TIME_WAKEUP(l)
#define SYSCALL_TIME_SYS_EXIT(l)
#define SYSCALL_TIME_ISR_ENTRY(l,old)
#define SYSCALL_TIME_ISR_EXIT(l,saved)
#undef SYSCALL_TIMES
#endif

#ifndef SYSCALL_COUNT
#define SYSCALL_COUNT(table, code)
#endif

#endif /* !_SYS_SYCALL_STAT_H_ */

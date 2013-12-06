/*	$NetBSD: userret.h,v 1.26 2013/04/07 07:54:53 kiyohara Exp $	*/

/*-
 * Copyright (c) 1998, 2000, 2003, 2006, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum, and Andrew Doran.
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

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 */

#ifndef _SYS_USERRET_H_
#define	_SYS_USERRET_H_

#include <sys/lockdebug.h>
#include <sys/intr.h>

/*
 * Define the MI code needed before returning to user mode, for
 * trap and syscall.
 */
static __inline void
mi_userret(struct lwp *l)
{
#ifndef __HAVE_PREEMPTION
	struct cpu_info *ci;
#endif

	KASSERT(l->l_blcnt == 0);
#ifndef __HAVE_PREEMPTION
	KASSERT(curcpu()->ci_biglock_count == 0);
#endif

	/*
	 * Handle "exceptional" events: pending signals, stop/exit actions,
	 * etc.  Note that the event must be flagged BEFORE any AST is
	 * posted as we are reading unlocked.
	 */
#ifdef __HAVE_PREEMPTION
	if (__predict_false(l->l_flag & LW_USERRET)) {
		lwp_userret(l);
	}
	l->l_kpriority = false;
	/*
	 * cpu_set_curpri(prio) is a MD optimized version of:
	 *
	 *	kpreempt_disable();
	 *	curcpu()->ci_schedstate.spc_curpriority = prio;
	 *	kpreempt_enable();
	 */
	cpu_set_curpri(l->l_priority);	/* XXX this needs to die */
#else
	ci = l->l_cpu;
	if (((l->l_flag & LW_USERRET) | ci->ci_data.cpu_softints) != 0) {
		lwp_userret(l);
		ci = l->l_cpu;
	}
	l->l_kpriority = false;
	ci->ci_schedstate.spc_curpriority = l->l_priority;
#endif

	LOCKDEBUG_BARRIER(NULL, 0);
	KASSERT(l->l_nopreempt == 0);
}

#endif	/* !_SYS_USERRET_H_ */

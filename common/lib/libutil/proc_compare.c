/*	$NetBSD: proc_compare.c,v 1.1 2011/10/21 02:09:00 christos Exp $	*/

/*-
 * Copyright (c) 1990, 1993
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
 */
#ifndef _STANDALONE
# ifndef _KERNEL

#  if HAVE_NBTOOL_CONFIG_H
#   include "nbtool_config.h"
#  endif

#  include <sys/cdefs.h>
#  if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: proc_compare.c,v 1.1 2011/10/21 02:09:00 christos Exp $");
#  endif

#  include <sys/types.h>
#  include <sys/inttypes.h>
#  include <sys/sysctl.h>
#  include <stdio.h>
#  include <util.h>
#  include <errno.h>
#  define PROC		struct kinfo_proc2
#  define LWP		struct kinfo_lwp
#  define P_RTIME_SEC	p_rtime_sec
#  define P_RTIME_USEC	p_rtime_usec
# else
#  include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: proc_compare.c,v 1.1 2011/10/21 02:09:00 christos Exp $");
#  include <sys/param.h>
#  include <sys/inttypes.h>
#  include <sys/systm.h>
#  include <sys/proc.h>
#  include <sys/lwp.h>
#  include <lib/libkern/libkern.h>
#  define PROC		struct proc
#  define LWP		struct lwp
#  define P_RTIME_SEC	p_rtime.sec
#  define P_RTIME_USEC	p_rtime.frac
# endif
/*
 * Returns 1 if p2 is "better" than p1
 *
 * The algorithm for picking the "interesting" process is thus:
 *
 *	1) Only foreground processes are eligible - implied.
 *	2) Runnable processes are favored over anything else.  The runner
 *	   with the highest CPU utilization is picked (l_pctcpu).  Ties are
 *	   broken by picking the highest pid.
 *	3) The sleeper with the shortest sleep time is next.  With ties,
 *	   we pick out just "short-term" sleepers (P_SINTR == 0).
 *	4) Further ties are broken by picking the one started last.
 *	5) Finally the one with the biggest pid wins, but that is nonsense
 *	   because of pid randomization.
 */
#define	ISRUN(p)	((p)->p_nrlwps > 0)
#define	TESTAB(a, b)	(((a) << 1) | (b))
#define	ONLYA	2
#define	ONLYB	1
#define	BOTH	3

int
proc_compare(const PROC *p1, const LWP *l1, const PROC *p2, const LWP *l2)
{
	/*
	 * see if at least one of them is runnable
	 */
	switch (TESTAB(ISRUN(p1), ISRUN(p2))) {
	case ONLYA:
		return 0;
	case ONLYB:
		return 1;
	case BOTH:
		/*
		 * tie - favor one with highest recent CPU utilization
		 */
		if (l2->l_pctcpu > l1->l_pctcpu)
			return 1;
		goto out;
	}
	/*
 	 * weed out zombies
	 */
	switch (TESTAB(P_ZOMBIE(p1), P_ZOMBIE(p2))) {
	case ONLYA:
		return 1;
	case ONLYB:
		return 0;
	case BOTH:
		goto out;
	}
	/*
	 * pick the one with the smallest sleep time
	 */
	if (l1->l_slptime < l2->l_slptime)
		return 0;
	if (l2->l_slptime < l1->l_slptime)
		return 1;

	/*
	 * favor one sleeping in a non-interruptible sleep
	 */
	if ((l1->l_flag & LW_SINTR) && (l2->l_flag & LW_SINTR) == 0)
		return 0;
	if ((l2->l_flag & LW_SINTR) && (l1->l_flag & LW_SINTR) == 0)
		return 1;
out:
	/* tie, return the one with the smallest realtime */
	if (p1->P_RTIME_SEC < p2->P_RTIME_SEC)
		return 0;
	if (p2->P_RTIME_SEC < p1->P_RTIME_SEC)
		return 1;
	if (p1->P_RTIME_USEC < p2->P_RTIME_USEC)
		return 0;
	if (p2->P_RTIME_USEC < p1->P_RTIME_USEC)
		return 1;
		
	return p2->p_pid > p1->p_pid;	/* Nonsense */
}
#endif /* STANDALONE */

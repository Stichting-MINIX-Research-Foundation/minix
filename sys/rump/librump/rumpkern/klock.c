/*	$NetBSD: klock.c,v 1.8 2013/04/30 00:03:53 pooka Exp $	*/

/*
 * Copyright (c) 2007-2010 Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the
 * Finnish Cultural Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: klock.c,v 1.8 2013/04/30 00:03:53 pooka Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/evcnt.h>

#include <rump/rumpuser.h>

#include "rump_private.h"

/*
 * giant lock
 */

struct rumpuser_mtx *rump_giantlock;
static int giantcnt;
static struct lwp *giantowner;

static struct evcnt ev_biglock_fast;
static struct evcnt ev_biglock_slow;
static struct evcnt ev_biglock_recurse;

void 
rump_biglock_init(void)
{

	evcnt_attach_dynamic(&ev_biglock_fast, EVCNT_TYPE_MISC, NULL,
	    "rump biglock", "fast");
	evcnt_attach_dynamic(&ev_biglock_slow, EVCNT_TYPE_MISC, NULL,
	    "rump biglock", "slow");
	evcnt_attach_dynamic(&ev_biglock_recurse, EVCNT_TYPE_MISC, NULL,
	    "rump biglock", "recurse");
}

void
rump_kernel_bigwrap(int *nlocks)
{

	KASSERT(giantcnt > 0 && curlwp == giantowner);
	giantowner = NULL; 
	*nlocks = giantcnt;
	giantcnt = 0;
}

void
rump_kernel_bigunwrap(int nlocks)
{

	KASSERT(giantowner == NULL);
	giantowner = curlwp;
	giantcnt = nlocks;
}

void
_kernel_lock(int nlocks)
{
	struct lwp *l = curlwp;

	while (nlocks) {
		if (giantowner == l) {
			giantcnt += nlocks;
			nlocks = 0;
			ev_biglock_recurse.ev_count++;
		} else {
			if (rumpuser_mutex_tryenter(rump_giantlock) != 0) {
				rump_unschedule_cpu1(l, NULL);
				rumpuser_mutex_enter_nowrap(rump_giantlock);
				rump_schedule_cpu(l);
				ev_biglock_slow.ev_count++;
			} else {
				ev_biglock_fast.ev_count++;
			}
			giantowner = l;
			giantcnt = 1;
			nlocks--;
		}
	}
}

void
_kernel_unlock(int nlocks, int *countp)
{

	if (giantowner != curlwp) {
		KASSERT(nlocks == 0);
		if (countp)
			*countp = 0;
		return;
	}

	if (countp)
		*countp = giantcnt;
	if (nlocks == 0)
		nlocks = giantcnt;
	if (nlocks == -1) {
		KASSERT(giantcnt == 1);
		nlocks = 1;
	}
	KASSERT(nlocks <= giantcnt);
	while (nlocks--) {
		giantcnt--;
	}

	if (giantcnt == 0) {
		giantowner = NULL;
		rumpuser_mutex_exit(rump_giantlock);
	}
}

bool
_kernel_locked_p(void)
{

	return giantowner == curlwp;
}

void
rump_user_unschedule(int nlocks, int *countp, void *interlock)
{

	_kernel_unlock(nlocks, countp);
	/*
	 * XXX: technically we should unschedule_cpu1() here, but that
	 * requires rump_intr_enter/exit to be implemented.
	 */
	rump_unschedule_cpu_interlock(curlwp, interlock);
}

void
rump_user_schedule(int nlocks, void *interlock)
{

	rump_schedule_cpu_interlock(curlwp, interlock);

	if (nlocks)
		_kernel_lock(nlocks);
}

/*	$NetBSD: npf_worker.c,v 1.1 2013/06/02 02:20:04 rmind Exp $	*/

/*-
 * Copyright (c) 2010-2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This material is based upon work partially supported by The
 * NetBSD Foundation under a contract with Mindaugas Rasiukevicius.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: npf_worker.c,v 1.1 2013/06/02 02:20:04 rmind Exp $");

#include <sys/param.h>
#include <sys/types.h>

#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/kthread.h>

#include "npf_impl.h"

#define	NPF_MAX_WORKS		4
#define	WORKER_INTERVAL		mstohz(5 * 1000)

static kmutex_t		worker_lock;
static kcondvar_t	worker_cv;
static kcondvar_t	worker_event_cv;
static lwp_t *		worker_lwp;
static uint64_t		worker_loop;

static npf_workfunc_t	work_funcs[NPF_MAX_WORKS];

static void	npf_worker(void *) __dead;

int
npf_worker_sysinit(void)
{
	mutex_init(&worker_lock, MUTEX_DEFAULT, IPL_SOFTNET);
	cv_init(&worker_cv, "npfgccv");
	cv_init(&worker_event_cv, "npfevcv");
	worker_lwp = (lwp_t *)0xdeadbabe;
	worker_loop = 1;

	if (kthread_create(PRI_NONE, KTHREAD_MPSAFE | KTHREAD_MUSTJOIN, NULL,
	    npf_worker, NULL, &worker_lwp, "npfgc")) {
		return ENOMEM;
	}
	return 0;
}

void
npf_worker_sysfini(void)
{
	lwp_t *l = worker_lwp;

	/* Notify the worker and wait for the exit. */
	mutex_enter(&worker_lock);
	worker_lwp = NULL;
	cv_broadcast(&worker_cv);
	mutex_exit(&worker_lock);
	kthread_join(l);

	/* LWP has exited, destroy the structures. */
	cv_destroy(&worker_cv);
	cv_destroy(&worker_event_cv);
	mutex_destroy(&worker_lock);
}

void
npf_worker_signal(void)
{
	mutex_enter(&worker_lock);
	cv_signal(&worker_cv);
	mutex_exit(&worker_lock);
}

static bool
npf_worker_testset(npf_workfunc_t find, npf_workfunc_t set)
{
	for (u_int i = 0; i < NPF_MAX_WORKS; i++) {
		if (work_funcs[i] == find) {
			work_funcs[i] = set;
			return true;
		}
	}
	return false;
}

void
npf_worker_register(npf_workfunc_t func)
{
	mutex_enter(&worker_lock);
	npf_worker_testset(NULL, func);
	mutex_exit(&worker_lock);
}

void
npf_worker_unregister(npf_workfunc_t func)
{
	uint64_t l = worker_loop;

	mutex_enter(&worker_lock);
	npf_worker_testset(func, NULL);
	while (worker_loop == l) {
		cv_signal(&worker_cv);
		cv_wait(&worker_event_cv, &worker_lock);
	}
	mutex_exit(&worker_lock);
}

static void
npf_worker(void *arg)
{
	for (;;) {
		const bool finish = (worker_lwp == NULL);
		u_int i = NPF_MAX_WORKS;
		npf_workfunc_t work;

		/* Run the jobs. */
		while (i--) {
			if ((work = work_funcs[i]) != NULL) {
				work();
			}
		}

		/* Exit if requested and all jobs are done. */
		if (finish) {
			break;
		}

		/* Sleep and periodically wake up, unless we get notified. */
		mutex_enter(&worker_lock);
		worker_loop++;
		cv_broadcast(&worker_event_cv);
		cv_timedwait(&worker_cv, &worker_lock, WORKER_INTERVAL);
		mutex_exit(&worker_lock);
	}
	kthread_exit(0);
}

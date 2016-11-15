/*	$NetBSD: sysmon_taskq.c,v 1.19 2015/04/28 11:58:49 martin Exp $	*/

/*
 * Copyright (c) 2001, 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * General purpose task queue for sysmon back-ends.  This can be
 * used to run callbacks that require thread context.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sysmon_taskq.c,v 1.19 2015/04/28 11:58:49 martin Exp $");

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/once.h>

#include <dev/sysmon/sysmon_taskq.h>

struct sysmon_task {
	TAILQ_ENTRY(sysmon_task) st_list;
	void (*st_func)(void *);
	void *st_arg;
	u_int st_pri;
};

static TAILQ_HEAD(, sysmon_task) sysmon_task_queue =
    TAILQ_HEAD_INITIALIZER(sysmon_task_queue);

static kmutex_t sysmon_task_queue_mtx;
static kmutex_t sysmon_task_queue_init_mtx;
static kcondvar_t sysmon_task_queue_cv;

static int sysmon_task_queue_initialized;
static int sysmon_task_queue_cleanup_sem;
static struct lwp *sysmon_task_queue_lwp;
static void sysmon_task_queue_thread(void *);

MODULE(MODULE_CLASS_MISC, sysmon_taskq, NULL);

/*
 * XXX	Normally, all initialization would be handled as part of
 *	the module(9) framework.  However, there are a number of
 *	users of the sysmon_taskq facility that are not modular,
 *	and these can directly call sysmon_task_queue_init()
 *	directly.  To accomodate these non-standard users, we
 *	make sure that sysmon_task_queue_init() handles multiple
 *	invocations.  And we also ensure that, if any non-module
 *	user exists, we don't allow the module to be unloaded.
 *	(We can't use module_hold() for this, since the module(9)
 *	framework itself isn't necessarily initialized yet.)
 */

/*
 * tq_preinit:
 *
 *	Early one-time initialization of task-queue
 */

ONCE_DECL(once_tq);

static int
tq_preinit(void)
{

	mutex_init(&sysmon_task_queue_mtx, MUTEX_DEFAULT, IPL_VM);
	mutex_init(&sysmon_task_queue_init_mtx, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&sysmon_task_queue_cv, "smtaskq");
	sysmon_task_queue_initialized = 0;

	return 0;
}

/*
 * sysmon_task_queue_init:
 *
 *	Initialize the sysmon task queue.
 */
void
sysmon_task_queue_init(void)
{
	int error;

	(void)RUN_ONCE(&once_tq, tq_preinit);

	mutex_enter(&sysmon_task_queue_init_mtx);
	if (sysmon_task_queue_initialized++) {
		mutex_exit(&sysmon_task_queue_init_mtx);
		return;
	}

	mutex_exit(&sysmon_task_queue_init_mtx);

	error = kthread_create(PRI_NONE, KTHREAD_MPSAFE, NULL,
	    sysmon_task_queue_thread, NULL, &sysmon_task_queue_lwp, "sysmon");
	if (error) {
		printf("Unable to create sysmon task queue thread, "
		    "error = %d\n", error);
		panic("sysmon_task_queue_init");
	}
}

/*
 * sysmon_task_queue_fini:
 *
 *	Tear town the sysmon task queue.
 */
int
sysmon_task_queue_fini(void)
{

	if (sysmon_task_queue_initialized > 1)
		return EBUSY;

	mutex_enter(&sysmon_task_queue_mtx);

	sysmon_task_queue_cleanup_sem = 1;
	cv_signal(&sysmon_task_queue_cv);

	while (sysmon_task_queue_cleanup_sem != 0)
		cv_wait(&sysmon_task_queue_cv,
			&sysmon_task_queue_mtx);

	mutex_exit(&sysmon_task_queue_mtx);

	return 0;
}

/*
 * sysmon_task_queue_thread:
 *
 *	The sysmon task queue execution thread.  We execute callbacks that
 *	have been queued for us.
 */
static void
sysmon_task_queue_thread(void *arg)
{
	struct sysmon_task *st;

	/*
	 * Run through all the tasks before we check for the exit
	 * condition; it's probably more important to actually run
	 * all the tasks before we exit.
	 */
	mutex_enter(&sysmon_task_queue_mtx);
	for (;;) {
		st = TAILQ_FIRST(&sysmon_task_queue);
		if (st != NULL) {
			TAILQ_REMOVE(&sysmon_task_queue, st, st_list);
			mutex_exit(&sysmon_task_queue_mtx);
			(*st->st_func)(st->st_arg);
			free(st, M_TEMP);
			mutex_enter(&sysmon_task_queue_mtx);
		} else {
			/* Check for the exit condition. */
			if (sysmon_task_queue_cleanup_sem != 0)
				break;
			cv_wait(&sysmon_task_queue_cv, &sysmon_task_queue_mtx);
		}
	}
	/* Time to die. */
	sysmon_task_queue_cleanup_sem = 0;
	cv_broadcast(&sysmon_task_queue_cv);
	mutex_exit(&sysmon_task_queue_mtx);
	kthread_exit(0);
}

/*
 * sysmon_task_queue_sched:
 *
 *	Schedule a task for deferred execution.
 */
int
sysmon_task_queue_sched(u_int pri, void (*func)(void *), void *arg)
{
	struct sysmon_task *st, *lst;

	(void)RUN_ONCE(&once_tq, tq_preinit);

	if (sysmon_task_queue_lwp == NULL)
		aprint_debug("WARNING: Callback scheduled before sysmon "
		    "task queue thread present\n");

	if (func == NULL)
		return EINVAL;

	st = malloc(sizeof(*st), M_TEMP, M_NOWAIT);
	if (st == NULL)
		return ENOMEM;

	st->st_func = func;
	st->st_arg = arg;
	st->st_pri = pri;

	mutex_enter(&sysmon_task_queue_mtx);
	TAILQ_FOREACH(lst, &sysmon_task_queue, st_list) {
		if (st->st_pri > lst->st_pri) {
			TAILQ_INSERT_BEFORE(lst, st, st_list);
			break;
		}
	}

	if (lst == NULL)
		TAILQ_INSERT_TAIL(&sysmon_task_queue, st, st_list);

	cv_broadcast(&sysmon_task_queue_cv);
	mutex_exit(&sysmon_task_queue_mtx);

	return 0;
}

static
int   
sysmon_taskq_modcmd(modcmd_t cmd, void *arg)
{
	int ret;
 
	switch (cmd) { 
	case MODULE_CMD_INIT:
		sysmon_task_queue_init();
		ret = 0;
		break;
 
	case MODULE_CMD_FINI: 
		ret = sysmon_task_queue_fini();
		break;
 
	case MODULE_CMD_STAT:
	default: 
		ret = ENOTTY;
	}
 
	return ret;
}

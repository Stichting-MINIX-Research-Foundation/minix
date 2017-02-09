/*	$NetBSD: subr_workqueue.c,v 1.33 2012/10/07 22:16:21 matt Exp $	*/

/*-
 * Copyright (c)2002, 2005, 2006, 2007 YAMAMOTO Takashi,
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_workqueue.c,v 1.33 2012/10/07 22:16:21 matt Exp $");

#include <sys/param.h>
#include <sys/cpu.h>
#include <sys/systm.h>
#include <sys/kthread.h>
#include <sys/kmem.h>
#include <sys/proc.h>
#include <sys/workqueue.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/queue.h>

typedef struct work_impl {
	SIMPLEQ_ENTRY(work_impl) wk_entry;
} work_impl_t;

SIMPLEQ_HEAD(workqhead, work_impl);

struct workqueue_queue {
	kmutex_t q_mutex;
	kcondvar_t q_cv;
	struct workqhead q_queue;
	lwp_t *q_worker;
};

struct workqueue {
	void (*wq_func)(struct work *, void *);
	void *wq_arg;
	int wq_flags;

	char wq_name[MAXCOMLEN];
	pri_t wq_prio;
	void *wq_ptr;
};

#define	WQ_SIZE		(roundup2(sizeof(struct workqueue), coherency_unit))
#define	WQ_QUEUE_SIZE	(roundup2(sizeof(struct workqueue_queue), coherency_unit))

#define	POISON	0xaabbccdd

static size_t
workqueue_size(int flags)
{

	return WQ_SIZE
	    + ((flags & WQ_PERCPU) != 0 ? ncpu : 1) * WQ_QUEUE_SIZE
	    + coherency_unit;
}

static struct workqueue_queue *
workqueue_queue_lookup(struct workqueue *wq, struct cpu_info *ci)
{
	u_int idx = 0;

	if (wq->wq_flags & WQ_PERCPU) {
		idx = ci ? cpu_index(ci) : cpu_index(curcpu());
	}

	return (void *)((uintptr_t)(wq) + WQ_SIZE + (idx * WQ_QUEUE_SIZE));
}

static void
workqueue_runlist(struct workqueue *wq, struct workqhead *list)
{
	work_impl_t *wk;
	work_impl_t *next;

	/*
	 * note that "list" is not a complete SIMPLEQ.
	 */

	for (wk = SIMPLEQ_FIRST(list); wk != NULL; wk = next) {
		next = SIMPLEQ_NEXT(wk, wk_entry);
		(*wq->wq_func)((void *)wk, wq->wq_arg);
	}
}

static void
workqueue_worker(void *cookie)
{
	struct workqueue *wq = cookie;
	struct workqueue_queue *q;

	/* find the workqueue of this kthread */
	q = workqueue_queue_lookup(wq, curlwp->l_cpu);

	for (;;) {
		struct workqhead tmp;

		/*
		 * we violate abstraction of SIMPLEQ.
		 */

#if defined(DIAGNOSTIC)
		tmp.sqh_last = (void *)POISON;
#endif /* defined(DIAGNOSTIC) */

		mutex_enter(&q->q_mutex);
		while (SIMPLEQ_EMPTY(&q->q_queue))
			cv_wait(&q->q_cv, &q->q_mutex);
		tmp.sqh_first = q->q_queue.sqh_first; /* XXX */
		SIMPLEQ_INIT(&q->q_queue);
		mutex_exit(&q->q_mutex);

		workqueue_runlist(wq, &tmp);
	}
}

static void
workqueue_init(struct workqueue *wq, const char *name,
    void (*callback_func)(struct work *, void *), void *callback_arg,
    pri_t prio, int ipl)
{

	strncpy(wq->wq_name, name, sizeof(wq->wq_name));

	wq->wq_prio = prio;
	wq->wq_func = callback_func;
	wq->wq_arg = callback_arg;
}

static int
workqueue_initqueue(struct workqueue *wq, struct workqueue_queue *q,
    int ipl, struct cpu_info *ci)
{
	int error, ktf;

	KASSERT(q->q_worker == NULL);

	mutex_init(&q->q_mutex, MUTEX_DEFAULT, ipl);
	cv_init(&q->q_cv, wq->wq_name);
	SIMPLEQ_INIT(&q->q_queue);
	ktf = ((wq->wq_flags & WQ_MPSAFE) != 0 ? KTHREAD_MPSAFE : 0);
	if (wq->wq_prio < PRI_KERNEL)
		ktf |= KTHREAD_TS;
	if (ci) {
		error = kthread_create(wq->wq_prio, ktf, ci, workqueue_worker,
		    wq, &q->q_worker, "%s/%u", wq->wq_name, ci->ci_index);
	} else {
		error = kthread_create(wq->wq_prio, ktf, ci, workqueue_worker,
		    wq, &q->q_worker, "%s", wq->wq_name);
	}
	if (error != 0) {
		mutex_destroy(&q->q_mutex);
		cv_destroy(&q->q_cv);
		KASSERT(q->q_worker == NULL);
	}
	return error;
}

struct workqueue_exitargs {
	work_impl_t wqe_wk;
	struct workqueue_queue *wqe_q;
};

static void
workqueue_exit(struct work *wk, void *arg)
{
	struct workqueue_exitargs *wqe = (void *)wk;
	struct workqueue_queue *q = wqe->wqe_q;

	/*
	 * only competition at this point is workqueue_finiqueue.
	 */

	KASSERT(q->q_worker == curlwp);
	KASSERT(SIMPLEQ_EMPTY(&q->q_queue));
	mutex_enter(&q->q_mutex);
	q->q_worker = NULL;
	cv_signal(&q->q_cv);
	mutex_exit(&q->q_mutex);
	kthread_exit(0);
}

static void
workqueue_finiqueue(struct workqueue *wq, struct workqueue_queue *q)
{
	struct workqueue_exitargs wqe;

	KASSERT(wq->wq_func == workqueue_exit);

	wqe.wqe_q = q;
	KASSERT(SIMPLEQ_EMPTY(&q->q_queue));
	KASSERT(q->q_worker != NULL);
	mutex_enter(&q->q_mutex);
	SIMPLEQ_INSERT_TAIL(&q->q_queue, &wqe.wqe_wk, wk_entry);
	cv_signal(&q->q_cv);
	while (q->q_worker != NULL) {
		cv_wait(&q->q_cv, &q->q_mutex);
	}
	mutex_exit(&q->q_mutex);
	mutex_destroy(&q->q_mutex);
	cv_destroy(&q->q_cv);
}

/* --- */

int
workqueue_create(struct workqueue **wqp, const char *name,
    void (*callback_func)(struct work *, void *), void *callback_arg,
    pri_t prio, int ipl, int flags)
{
	struct workqueue *wq;
	struct workqueue_queue *q;
	void *ptr;
	int error = 0;

	CTASSERT(sizeof(work_impl_t) <= sizeof(struct work));

	ptr = kmem_zalloc(workqueue_size(flags), KM_SLEEP);
	wq = (void *)roundup2((uintptr_t)ptr, coherency_unit);
	wq->wq_ptr = ptr;
	wq->wq_flags = flags;

	workqueue_init(wq, name, callback_func, callback_arg, prio, ipl);

	if (flags & WQ_PERCPU) {
		struct cpu_info *ci;
		CPU_INFO_ITERATOR cii;

		/* create the work-queue for each CPU */
		for (CPU_INFO_FOREACH(cii, ci)) {
			q = workqueue_queue_lookup(wq, ci);
			error = workqueue_initqueue(wq, q, ipl, ci);
			if (error) {
				break;
			}
		}
	} else {
		/* initialize a work-queue */
		q = workqueue_queue_lookup(wq, NULL);
		error = workqueue_initqueue(wq, q, ipl, NULL);
	}

	if (error != 0) {
		workqueue_destroy(wq);
	} else {
		*wqp = wq;
	}

	return error;
}

void
workqueue_destroy(struct workqueue *wq)
{
	struct workqueue_queue *q;
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;

	wq->wq_func = workqueue_exit;
	for (CPU_INFO_FOREACH(cii, ci)) {
		q = workqueue_queue_lookup(wq, ci);
		if (q->q_worker != NULL) {
			workqueue_finiqueue(wq, q);
		}
	}
	kmem_free(wq->wq_ptr, workqueue_size(wq->wq_flags));
}

void
workqueue_enqueue(struct workqueue *wq, struct work *wk0, struct cpu_info *ci)
{
	struct workqueue_queue *q;
	work_impl_t *wk = (void *)wk0;

	KASSERT(wq->wq_flags & WQ_PERCPU || ci == NULL);
	q = workqueue_queue_lookup(wq, ci);

	mutex_enter(&q->q_mutex);
	SIMPLEQ_INSERT_TAIL(&q->q_queue, wk, wk_entry);
	cv_signal(&q->q_cv);
	mutex_exit(&q->q_mutex);
}

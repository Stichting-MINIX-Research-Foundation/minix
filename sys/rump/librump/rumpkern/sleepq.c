/*	$NetBSD: sleepq.c,v 1.16 2014/04/24 12:04:28 pooka Exp $	*/

/*
 * Copyright (c) 2008 Antti Kantee.  All Rights Reserved.
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
__KERNEL_RCSID(0, "$NetBSD: sleepq.c,v 1.16 2014/04/24 12:04:28 pooka Exp $");

#include <sys/param.h>
#include <sys/condvar.h>
#include <sys/mutex.h>
#include <sys/once.h>
#include <sys/queue.h>
#include <sys/sleepq.h>
#include <sys/syncobj.h>
#include <sys/atomic.h>

#include "rump_private.h"

syncobj_t sleep_syncobj;
static kcondvar_t sq_cv;

static int
sqinit1(void)
{

	cv_init(&sq_cv, "sleepq");

	return 0;
}

void
sleepq_init(sleepq_t *sq)
{
	static ONCE_DECL(sqctl);

	RUN_ONCE(&sqctl, sqinit1);

	TAILQ_INIT(sq);
}

void
sleepq_enqueue(sleepq_t *sq, wchan_t wc, const char *wmsg, syncobj_t *sob)
{
	struct lwp *l = curlwp;

	l->l_wchan = wc;
	l->l_wmesg = wmsg;
	l->l_sleepq = sq;
	TAILQ_INSERT_TAIL(sq, l, l_sleepchain);
}

int
sleepq_block(int timo, bool catch)
{
	struct lwp *l = curlwp;
	int error = 0;
	kmutex_t *mp = l->l_mutex;
	int biglocks = l->l_biglocks;

	while (l->l_wchan) {
		l->l_mutex = mp; /* keep sleepq lock until woken up */
		error = cv_timedwait(&sq_cv, mp, timo);
		if (error == EWOULDBLOCK || error == EINTR) {
			if (l->l_wchan) {
				TAILQ_REMOVE(l->l_sleepq, l, l_sleepchain);
				l->l_wchan = NULL;
				l->l_wmesg = NULL;
			}
		}
	}
	mutex_spin_exit(mp);

	if (biglocks)
		KERNEL_LOCK(biglocks, curlwp);

	return error;
}

void
sleepq_wake(sleepq_t *sq, wchan_t wchan, u_int expected, kmutex_t *mp)
{
	struct lwp *l, *l_next;
	bool found = false;

	for (l = TAILQ_FIRST(sq); l; l = l_next) {
		l_next = TAILQ_NEXT(l, l_sleepchain);
		if (l->l_wchan == wchan) {
			found = true;
			l->l_wchan = NULL;
			l->l_wmesg = NULL;
			TAILQ_REMOVE(sq, l, l_sleepchain);
			if (--expected == 0)
				break;
		}
	}
	if (found)
		cv_broadcast(&sq_cv);

	mutex_spin_exit(mp);
}

void
sleepq_unsleep(struct lwp *l, bool cleanup)
{

	l->l_wchan = NULL;
	l->l_wmesg = NULL;
	TAILQ_REMOVE(l->l_sleepq, l, l_sleepchain);
	cv_broadcast(&sq_cv);

	if (cleanup) {
		mutex_spin_exit(l->l_mutex);
	}
}

/*
 * Thread scheduler handles priorities.  Therefore no action here.
 * (maybe do something if we're deperate?)
 */
void
sleepq_changepri(struct lwp *l, pri_t pri)
{

}

void
sleepq_lendpri(struct lwp *l, pri_t pri)
{

}

struct lwp *
syncobj_noowner(wchan_t wc)
{

	return NULL;
}

void
lwp_unlock_to(struct lwp *l, kmutex_t *new)
{
	kmutex_t *old;

	KASSERT(mutex_owned(l->l_mutex));

	old = l->l_mutex;
	membar_exit();
	l->l_mutex = new;
	mutex_spin_exit(old);
}

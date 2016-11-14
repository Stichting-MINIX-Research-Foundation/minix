/*	$NetBSD: ltsleep.c,v 1.33 2013/05/15 13:58:14 pooka Exp $	*/

/*
 * Copyright (c) 2009, 2010 Antti Kantee.  All Rights Reserved.
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

/*
 * Implementation of the tsleep/mtsleep kernel sleep interface.  There
 * are two sides to our implementation.  For historic spinlocks we
 * assume the kernel is giantlocked and use kernel giantlock as the
 * wait interlock.  For mtsleep, we use the interlock supplied by
 * the caller.  This duality leads to some if/else messiness in the code ...
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ltsleep.c,v 1.33 2013/05/15 13:58:14 pooka Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/queue.h>

#include <rump/rumpuser.h>

#include "rump_private.h"

struct ltsleeper {
	wchan_t id;
	union {
		struct rumpuser_cv *user;
		kcondvar_t kern;
	} u;
	bool iskwait;
	LIST_ENTRY(ltsleeper) entries;
};
#define ucv u.user
#define kcv u.kern

static LIST_HEAD(, ltsleeper) sleepers = LIST_HEAD_INITIALIZER(sleepers);
static kmutex_t qlock;

static int
sleeper(wchan_t ident, int timo, kmutex_t *kinterlock)
{
	struct ltsleeper lts;
	struct timespec ts;
	int rv;

	lts.id = ident;
	if (kinterlock) {
		lts.iskwait = true;
		cv_init(&lts.kcv, "mtsleep");
	} else {
		lts.iskwait = false;
		rumpuser_cv_init(&lts.ucv);
	}

	mutex_spin_enter(&qlock);
	LIST_INSERT_HEAD(&sleepers, &lts, entries);
	mutex_exit(&qlock);

	if (timo) {
		if (kinterlock) {
			rv = cv_timedwait(&lts.kcv, kinterlock, timo);
		} else {
			/*
			 * Calculate wakeup-time.
			 */
			ts.tv_sec = timo / hz;
			ts.tv_nsec = (timo % hz) * (1000000000/hz);
			rv = rumpuser_cv_timedwait(lts.ucv, rump_giantlock,
			    ts.tv_sec, ts.tv_nsec);
		}

		if (rv != 0)
			rv = EWOULDBLOCK;
	} else {
		if (kinterlock) {
			cv_wait(&lts.kcv, kinterlock);
		} else {
			rumpuser_cv_wait(lts.ucv, rump_giantlock);
		}
		rv = 0;
	}

	mutex_spin_enter(&qlock);
	LIST_REMOVE(&lts, entries);
	mutex_exit(&qlock);

	if (kinterlock)
		cv_destroy(&lts.kcv);
	else
		rumpuser_cv_destroy(lts.ucv);

	return rv;
}

int
tsleep(wchan_t ident, pri_t prio, const char *wmesg, int timo)
{
	int rv, nlocks;

	/*
	 * Since we cannot use slock as the rumpuser interlock,
	 * require that everyone using this prehistoric interface
	 * is biglocked.  Wrap around the biglock and drop lockcnt,
	 * but retain the rumpuser mutex so that we can use it as an
	 * interlock to rumpuser_cv_wait().
	 */
	rump_kernel_bigwrap(&nlocks);
	rv = sleeper(ident, timo, NULL);
	rump_kernel_bigunwrap(nlocks);

	return rv;
}

int
mtsleep(wchan_t ident, pri_t prio, const char *wmesg, int timo, kmutex_t *lock)
{
	int rv;

	rv = sleeper(ident, timo, lock);
	if (prio & PNORELOCK)
		mutex_exit(lock);

	return rv;
}

void
wakeup(wchan_t ident)
{
	struct ltsleeper *ltsp;

	mutex_spin_enter(&qlock);
	LIST_FOREACH(ltsp, &sleepers, entries) {
		if (ltsp->id == ident) {
			if (ltsp->iskwait) {
				cv_broadcast(&ltsp->kcv);
			} else {
				rumpuser_cv_broadcast(ltsp->ucv);
			}
		}
	}
	mutex_exit(&qlock);
}

void
rump_tsleep_init()
{

	mutex_init(&qlock, MUTEX_SPIN, IPL_NONE);
}

/*	$NetBSD: rumpfiber.c,v 1.12 2015/02/15 00:54:32 justin Exp $	*/

/*
 * Copyright (c) 2007-2013 Antti Kantee.  All Rights Reserved.
 * Copyright (c) 2014 Justin Cormack.  All Rights Reserved.
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

/* Based partly on code from Xen Minios with the following license */

/* 
 ****************************************************************************
 * (C) 2005 - Grzegorz Milos - Intel Research Cambridge
 ****************************************************************************
 *
 *        File: sched.c
 *      Author: Grzegorz Milos
 *     Changes: Robert Kaiser
 *              
 *        Date: Aug 2005
 * 
 * Environment: Xen Minimal OS
 * Description: simple scheduler for Mini-Os
 *
 * The scheduler is non-preemptive (cooperative), and schedules according 
 * to Round Robin algorithm.
 *
 ****************************************************************************
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */

#include "rumpuser_port.h"

#if !defined(lint)
__RCSID("$NetBSD: rumpfiber.c,v 1.12 2015/02/15 00:54:32 justin Exp $");
#endif /* !lint */

#include <sys/mman.h>
#include <sys/time.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <rump/rumpuser.h>

#include "rumpuser_int.h"
#include "rumpfiber.h"

static void init_sched(void);
static void join_thread(struct thread *);
static void switch_threads(struct thread *prev, struct thread *next);
static struct thread *get_current(void);
static int64_t now(void);
static void msleep(uint64_t millisecs);
static void abssleep(uint64_t millisecs);

TAILQ_HEAD(thread_list, thread);

static struct thread_list exited_threads = TAILQ_HEAD_INITIALIZER(exited_threads);
static struct thread_list thread_list = TAILQ_HEAD_INITIALIZER(thread_list);
static struct thread *current_thread = NULL;

static void (*scheduler_hook)(void *, void *);

static void printk(const char *s);

static void
printk(const char *msg)
{
	int ret __attribute__((unused));

	ret = write(2, msg, strlen(msg));
}

static struct thread *
get_current(void)
{

	return current_thread;
}

static int64_t
now(void)
{
	struct timespec ts;
	int rv;

	rv = clock_gettime(CLOCK_MONOTONIC, &ts);
	assert(rv == 0);
	return (ts.tv_sec * 1000LL) + (ts.tv_nsec / 1000000LL);
}

void
schedule(void)
{
	struct thread *prev, *next, *thread, *tmp;
	int64_t tm, wakeup;
	struct timespec sl;

	prev = get_current();

	do {
		tm = now();	
		wakeup = tm + 1000; /* wake up in 1s max */
		next = NULL;
		TAILQ_FOREACH_SAFE(thread, &thread_list, thread_list, tmp) {
			if (!is_runnable(thread) && thread->wakeup_time >= 0) {
				if (thread->wakeup_time <= tm) {
					thread->flags |= THREAD_TIMEDOUT;
					wake(thread);
				} else if (thread->wakeup_time < wakeup)
					wakeup = thread->wakeup_time;
			}
			if (is_runnable(thread)) {
				next = thread;
				/* Put this thread on the end of the list */
				TAILQ_REMOVE(&thread_list, thread, thread_list);
				TAILQ_INSERT_TAIL(&thread_list, thread, thread_list);
				break;
			}
		}
		if (next)
			break;
		sl.tv_sec = (wakeup - tm) / 1000;
		sl.tv_nsec = ((wakeup - tm) - 1000 * sl.tv_sec) * 1000000;
#ifdef HAVE_CLOCK_NANOSLEEP
		clock_nanosleep(CLOCK_MONOTONIC, 0, &sl, NULL);
#else
		nanosleep(&sl, NULL);
#endif
	} while (1);

	if (prev != next)
		switch_threads(prev, next);

	TAILQ_FOREACH_SAFE(thread, &exited_threads, thread_list, tmp) {
		if (thread != prev) {
			TAILQ_REMOVE(&exited_threads, thread, thread_list);
			if ((thread->flags & THREAD_EXTSTACK) == 0)
				munmap(thread->ctx.uc_stack.ss_sp, STACKSIZE);
			free(thread->name);
			free(thread);
		}
	}
}

static void
create_ctx(ucontext_t *ctx, void *stack, size_t stack_size,
	void (*f)(void *), void *data)
{

	getcontext(ctx);
	ctx->uc_stack.ss_sp = stack;
	ctx->uc_stack.ss_size = stack_size;
	ctx->uc_stack.ss_flags = 0;
	ctx->uc_link = NULL; /* TODO may link to main thread */
	/* may have to do bounce function to call, if args to makecontext are ints */
	makecontext(ctx, (void (*)(void))f, 1, data);
}

/* TODO see notes in rumpuser_thread_create, have flags here */
struct thread *
create_thread(const char *name, void *cookie, void (*f)(void *), void *data,
	void *stack, size_t stack_size)
{
	struct thread *thread = calloc(1, sizeof(struct thread));

	if (!thread) {
		return NULL;
	}

	if (!stack) {
		assert(stack_size == 0);
		stack = mmap(NULL, STACKSIZE, PROT_READ | PROT_WRITE,
		    MAP_SHARED | MAP_ANON, -1, 0);
		if (stack == MAP_FAILED) {
			free(thread);
			return NULL;
		}
		stack_size = STACKSIZE;
	} else {
		thread->flags = THREAD_EXTSTACK;
	}
	create_ctx(&thread->ctx, stack, stack_size, f, data);
	
	thread->name = strdup(name);
	thread->cookie = cookie;

	/* Not runnable, not exited, not sleeping */
	thread->wakeup_time = -1;
	thread->lwp = NULL;
	set_runnable(thread);
	TAILQ_INSERT_TAIL(&thread_list, thread, thread_list);

	return thread;
}

static void
switch_threads(struct thread *prev, struct thread *next)
{
	int ret;

	current_thread = next;
	if (scheduler_hook)
		scheduler_hook(prev->cookie, next->cookie);
	ret = swapcontext(&prev->ctx, &next->ctx);
	if (ret < 0) {
		printk("swapcontext failed\n");
		abort();
	}
}

struct join_waiter {
    struct thread *jw_thread;
    struct thread *jw_wanted;
    TAILQ_ENTRY(join_waiter) jw_entries;
};
static TAILQ_HEAD(, join_waiter) joinwq = TAILQ_HEAD_INITIALIZER(joinwq);

void
exit_thread(void)
{
	struct thread *thread = get_current();
	struct join_waiter *jw_iter;

	/* if joinable, gate until we are allowed to exit */
	while (thread->flags & THREAD_MUSTJOIN) {
		thread->flags |= THREAD_JOINED;

		/* see if the joiner is already there */
		TAILQ_FOREACH(jw_iter, &joinwq, jw_entries) {
			if (jw_iter->jw_wanted == thread) {
				wake(jw_iter->jw_thread);
				break;
			}
		}
		block(thread);
		schedule();
	}

	/* Remove from the thread list */
	TAILQ_REMOVE(&thread_list, thread, thread_list);
	clear_runnable(thread);
	/* Put onto exited list */
	TAILQ_INSERT_HEAD(&exited_threads, thread, thread_list);

	/* Schedule will free the resources */
	while (1) {
		schedule();
		printk("schedule() returned!  Trying again\n");
	}
}

static void
join_thread(struct thread *joinable)
{
	struct join_waiter jw;
	struct thread *thread = get_current();

	assert(joinable->flags & THREAD_MUSTJOIN);

	/* wait for exiting thread to hit thread_exit() */
	while (! (joinable->flags & THREAD_JOINED)) {

		jw.jw_thread = thread;
		jw.jw_wanted = joinable;
		TAILQ_INSERT_TAIL(&joinwq, &jw, jw_entries);
		block(thread);
		schedule();
		TAILQ_REMOVE(&joinwq, &jw, jw_entries);
	}

	/* signal exiting thread that we have seen it and it may now exit */
	assert(joinable->flags & THREAD_JOINED);
	joinable->flags &= ~THREAD_MUSTJOIN;

	wake(joinable);
}

static void msleep(uint64_t millisecs)
{
	struct thread *thread = get_current();

	thread->wakeup_time = now() + millisecs;
	clear_runnable(thread);
	schedule();
}

static void abssleep(uint64_t millisecs)
{
	struct thread *thread = get_current();

	thread->wakeup_time = millisecs;
	clear_runnable(thread);
	schedule();
}

/* like abssleep, except against realtime clock instead of monotonic clock */
int abssleep_real(uint64_t millisecs)
{
	struct thread *thread = get_current();
	struct timespec ts;
	uint64_t real_now;
	int rv;

	clock_gettime(CLOCK_REALTIME, &ts);
	real_now = 1000*ts.tv_sec + ts.tv_nsec/(1000*1000);
	thread->wakeup_time = now() + (millisecs - real_now);

	clear_runnable(thread);
	schedule();

	rv = !!(thread->flags & THREAD_TIMEDOUT);
	thread->flags &= ~THREAD_TIMEDOUT;
	return rv;
}

void wake(struct thread *thread)
{

	thread->wakeup_time = -1;
	set_runnable(thread);
}

void block(struct thread *thread)
{

	thread->wakeup_time = -1;
	clear_runnable(thread);
}

int is_runnable(struct thread *thread)
{

	return thread->flags & RUNNABLE_FLAG;
}

void set_runnable(struct thread *thread)
{

	thread->flags |= RUNNABLE_FLAG;
}

void clear_runnable(struct thread *thread)
{

	thread->flags &= ~RUNNABLE_FLAG;
}

static void
init_sched(void)
{
	struct thread *thread = calloc(1, sizeof(struct thread));

	if (!thread) {
		abort();
	}

	thread->name = strdup("init");
	thread->flags = 0;
	thread->wakeup_time = -1;
	thread->lwp = NULL;
	set_runnable(thread);
	TAILQ_INSERT_TAIL(&thread_list, thread, thread_list);
	current_thread = thread;
}

void
set_sched_hook(void (*f)(void *, void *))
{

	scheduler_hook = f;
}

struct thread *
init_mainthread(void *cookie)
{

	current_thread->cookie = cookie;
	return current_thread;
}

/* rump functions below */

struct rumpuser_hyperup rumpuser__hyp;

int
rumpuser_init(int version, const struct rumpuser_hyperup *hyp)
{
	int rv;

	if (version != RUMPUSER_VERSION) {
		printk("rumpuser version mismatch\n");
		abort();
	}

	rv = rumpuser__random_init();
	if (rv != 0) {
		ET(rv);
	}

	rumpuser__hyp = *hyp;

	init_sched();

	return 0;
}

int
rumpuser_clock_gettime(int enum_rumpclock, int64_t *sec, long *nsec)
{
	enum rumpclock rclk = enum_rumpclock;
	struct timespec ts;
	clockid_t clk;
	int rv;

	switch (rclk) {
	case RUMPUSER_CLOCK_RELWALL:
		clk = CLOCK_REALTIME;
		break;
	case RUMPUSER_CLOCK_ABSMONO:
		clk = CLOCK_MONOTONIC;
		break;
	default:
		abort();
	}

	if (clock_gettime(clk, &ts) == -1) {
		rv = errno;
	} else {
		*sec = ts.tv_sec;
		*nsec = ts.tv_nsec;
		rv = 0;
	}

	ET(rv);
}

int
rumpuser_clock_sleep(int enum_rumpclock, int64_t sec, long nsec)
{
	enum rumpclock rclk = enum_rumpclock;
	uint64_t msec;
	int nlocks;

	rumpkern_unsched(&nlocks, NULL);
	switch (rclk) {
	case RUMPUSER_CLOCK_RELWALL:
		msec = sec * 1000 + nsec / (1000*1000UL);
		msleep(msec);
		break;
	case RUMPUSER_CLOCK_ABSMONO:
		msec = sec * 1000 + nsec / (1000*1000UL);
		abssleep(msec);
		break;
	}
	rumpkern_sched(nlocks, NULL);

	return 0;
}

int
rumpuser_getparam(const char *name, void *buf, size_t blen)
{
	int rv;
	const char *ncpu = "1";

	if (strcmp(name, RUMPUSER_PARAM_NCPU) == 0) {
		strncpy(buf, ncpu, blen);
		rv = 0;
	} else if (strcmp(name, RUMPUSER_PARAM_HOSTNAME) == 0) {
		char tmp[MAXHOSTNAMELEN];

		if (gethostname(tmp, sizeof(tmp)) == -1) {
			snprintf(buf, blen, "rump-%05d", (int)getpid());
		} else {
			snprintf(buf, blen, "rump-%05d.%s",
			    (int)getpid(), tmp);
		}
		rv = 0;
	} else if (*name == '_') {
		rv = EINVAL;
	} else {
		if (getenv_r(name, buf, blen) == -1)
			rv = errno;
		else
			rv = 0;
	}

	ET(rv);
}

void
rumpuser_putchar(int c)
{

	putchar(c);
}

__dead void
rumpuser_exit(int rv)
{

	if (rv == RUMPUSER_PANIC)
		abort();
	else
		exit(rv);
}

void
rumpuser_seterrno(int error)
{

	errno = error;
}

/*
 * This is meant for safe debugging prints from the kernel.
 */
void
rumpuser_dprintf(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
}

int
rumpuser_kill(int64_t pid, int rumpsig)
{
	int sig;

	sig = rumpuser__sig_rump2host(rumpsig);
	if (sig > 0)
		raise(sig);
	return 0;
}

/* thread functions */

TAILQ_HEAD(waithead, waiter);
struct waiter {
	struct thread *who;
	TAILQ_ENTRY(waiter) entries;
	int onlist;
};

static int
wait(struct waithead *wh, uint64_t msec)
{
	struct waiter w;

	w.who = get_current();
	TAILQ_INSERT_TAIL(wh, &w, entries);
	w.onlist = 1;
	block(w.who);
	if (msec)
		w.who->wakeup_time = now() + msec;
	schedule();

	/* woken up by timeout? */
	if (w.onlist)
		TAILQ_REMOVE(wh, &w, entries);

	return w.onlist ? ETIMEDOUT : 0;
}

static void
wakeup_one(struct waithead *wh)
{
	struct waiter *w;

	if ((w = TAILQ_FIRST(wh)) != NULL) {
		TAILQ_REMOVE(wh, w, entries);
		w->onlist = 0;
		wake(w->who);
	}
}

static void
wakeup_all(struct waithead *wh)
{
	struct waiter *w;

	while ((w = TAILQ_FIRST(wh)) != NULL) {
		TAILQ_REMOVE(wh, w, entries);
		w->onlist = 0;
		wake(w->who);
	}
}

int
rumpuser_thread_create(void *(*f)(void *), void *arg, const char *thrname,
	int joinable, int pri, int cpuidx, void **tptr)
{
	struct thread *thr;

	thr = create_thread(thrname, NULL, (void (*)(void *))f, arg, NULL, 0);

	if (!thr)
		return EINVAL;

	/*
	 * XXX: should be supplied as a flag to create_thread() so as to
	 * _ensure_ it's set before the thread runs (and could exit).
	 * now we're trusting unclear semantics of create_thread()
	 */
	if (thr && joinable)
		thr->flags |= THREAD_MUSTJOIN;

	*tptr = thr;
	return 0;
}

void
rumpuser_thread_exit(void)
{

	exit_thread();
}

int
rumpuser_thread_join(void *p)
{

	join_thread(p);
	return 0;
}

struct rumpuser_mtx {
	struct waithead waiters;
	int v;
	int flags;
	struct lwp *o;
};

void
rumpuser_mutex_init(struct rumpuser_mtx **mtxp, int flags)
{
	struct rumpuser_mtx *mtx;

	mtx = malloc(sizeof(*mtx));
	memset(mtx, 0, sizeof(*mtx));
	mtx->flags = flags;
	TAILQ_INIT(&mtx->waiters);
	*mtxp = mtx;
}

void
rumpuser_mutex_enter(struct rumpuser_mtx *mtx)
{
	int nlocks;

	if (rumpuser_mutex_tryenter(mtx) != 0) {
		rumpkern_unsched(&nlocks, NULL);
		while (rumpuser_mutex_tryenter(mtx) != 0)
			wait(&mtx->waiters, 0);
		rumpkern_sched(nlocks, NULL);
	}
}

void
rumpuser_mutex_enter_nowrap(struct rumpuser_mtx *mtx)
{
	int rv;

	rv = rumpuser_mutex_tryenter(mtx);
	/* one VCPU supported, no preemption => must succeed */
	if (rv != 0) {
		printk("no voi ei\n");
	}
}

int
rumpuser_mutex_tryenter(struct rumpuser_mtx *mtx)
{
	struct lwp *l = get_current()->lwp;

	if (mtx->v && mtx->o != l)
		return EBUSY;

	mtx->v++;
	mtx->o = l;

	return 0;
}

void
rumpuser_mutex_exit(struct rumpuser_mtx *mtx)
{

	assert(mtx->v > 0);
	if (--mtx->v == 0) {
		mtx->o = NULL;
		wakeup_one(&mtx->waiters);
	}
}

void
rumpuser_mutex_destroy(struct rumpuser_mtx *mtx)
{

	assert(TAILQ_EMPTY(&mtx->waiters) && mtx->o == NULL);
	free(mtx);
}

void
rumpuser_mutex_owner(struct rumpuser_mtx *mtx, struct lwp **lp)
{

	*lp = mtx->o;
}

struct rumpuser_rw {
	struct waithead rwait;
	struct waithead wwait;
	int v;
	struct lwp *o;
};

void
rumpuser_rw_init(struct rumpuser_rw **rwp)
{
	struct rumpuser_rw *rw;

	rw = malloc(sizeof(*rw));
	memset(rw, 0, sizeof(*rw));
	TAILQ_INIT(&rw->rwait);
	TAILQ_INIT(&rw->wwait);

	*rwp = rw;
}

void
rumpuser_rw_enter(int enum_rumprwlock, struct rumpuser_rw *rw)
{
	enum rumprwlock lk = enum_rumprwlock;
	struct waithead *w = NULL;
	int nlocks;

	switch (lk) {
	case RUMPUSER_RW_WRITER:
		w = &rw->wwait;
		break;
	case RUMPUSER_RW_READER:
		w = &rw->rwait;
		break;
	}

	if (rumpuser_rw_tryenter(enum_rumprwlock, rw) != 0) {
		rumpkern_unsched(&nlocks, NULL);
		while (rumpuser_rw_tryenter(enum_rumprwlock, rw) != 0)
			wait(w, 0);
		rumpkern_sched(nlocks, NULL);
	}
}

int
rumpuser_rw_tryenter(int enum_rumprwlock, struct rumpuser_rw *rw)
{
	enum rumprwlock lk = enum_rumprwlock;
	int rv;

	switch (lk) {
	case RUMPUSER_RW_WRITER:
		if (rw->o == NULL) {
			rw->o = rumpuser_curlwp();
			rv = 0;
		} else {
			rv = EBUSY;
		}
		break;
	case RUMPUSER_RW_READER:
		if (rw->o == NULL && TAILQ_EMPTY(&rw->wwait)) {
			rw->v++;
			rv = 0;
		} else {
			rv = EBUSY;
		}
		break;
	default:
		rv = EINVAL;
	}

	return rv;
}

void
rumpuser_rw_exit(struct rumpuser_rw *rw)
{

	if (rw->o) {
		rw->o = NULL;
	} else {
		rw->v--;
	}

	/* standard procedure, don't let readers starve out writers */
	if (!TAILQ_EMPTY(&rw->wwait)) {
		if (rw->o == NULL)
			wakeup_one(&rw->wwait);
	} else if (!TAILQ_EMPTY(&rw->rwait) && rw->o == NULL) {
		wakeup_all(&rw->rwait);
	}
}

void
rumpuser_rw_destroy(struct rumpuser_rw *rw)
{

	free(rw);
}

void
rumpuser_rw_held(int enum_rumprwlock, struct rumpuser_rw *rw, int *rvp)
{
	enum rumprwlock lk = enum_rumprwlock;

	switch (lk) {
	case RUMPUSER_RW_WRITER:
		*rvp = rw->o == rumpuser_curlwp();
		break;
	case RUMPUSER_RW_READER:
		*rvp = rw->v > 0;
		break;
	}
}

void
rumpuser_rw_downgrade(struct rumpuser_rw *rw)
{

	assert(rw->o == rumpuser_curlwp());
	rw->v = -1;
}

int
rumpuser_rw_tryupgrade(struct rumpuser_rw *rw)
{

	if (rw->v == -1) {
		rw->v = 1;
		rw->o = rumpuser_curlwp();
		return 0;
	}

	return EBUSY;
}

struct rumpuser_cv {
	struct waithead waiters;
	int nwaiters;
};

void
rumpuser_cv_init(struct rumpuser_cv **cvp)
{
	struct rumpuser_cv *cv;

	cv = malloc(sizeof(*cv));
	memset(cv, 0, sizeof(*cv));
	TAILQ_INIT(&cv->waiters);
	*cvp = cv;
}

void
rumpuser_cv_destroy(struct rumpuser_cv *cv)
{

	assert(cv->nwaiters == 0);
	free(cv);
}

static void
cv_unsched(struct rumpuser_mtx *mtx, int *nlocks)
{

	rumpkern_unsched(nlocks, mtx);
	rumpuser_mutex_exit(mtx);
}

static void
cv_resched(struct rumpuser_mtx *mtx, int nlocks)
{

	/* see rumpuser(3) */
	if ((mtx->flags & (RUMPUSER_MTX_KMUTEX | RUMPUSER_MTX_SPIN)) ==
	    (RUMPUSER_MTX_KMUTEX | RUMPUSER_MTX_SPIN)) {
		rumpkern_sched(nlocks, mtx);
		rumpuser_mutex_enter_nowrap(mtx);
	} else {
		rumpuser_mutex_enter_nowrap(mtx);
		rumpkern_sched(nlocks, mtx);
	}
}

void
rumpuser_cv_wait(struct rumpuser_cv *cv, struct rumpuser_mtx *mtx)
{
	int nlocks;

	cv->nwaiters++;
	cv_unsched(mtx, &nlocks);
	wait(&cv->waiters, 0);
	cv_resched(mtx, nlocks);
	cv->nwaiters--;
}

void
rumpuser_cv_wait_nowrap(struct rumpuser_cv *cv, struct rumpuser_mtx *mtx)
{

	cv->nwaiters++;
	rumpuser_mutex_exit(mtx);
	wait(&cv->waiters, 0);
	rumpuser_mutex_enter_nowrap(mtx);
	cv->nwaiters--;
}

int
rumpuser_cv_timedwait(struct rumpuser_cv *cv, struct rumpuser_mtx *mtx,
	int64_t sec, int64_t nsec)
{
	int nlocks;
	int rv;

	cv->nwaiters++;
	cv_unsched(mtx, &nlocks);
	rv = wait(&cv->waiters, sec * 1000 + nsec / (1000*1000));
	cv_resched(mtx, nlocks);
	cv->nwaiters--;

	return rv;
}

void
rumpuser_cv_signal(struct rumpuser_cv *cv)
{

	wakeup_one(&cv->waiters);
}

void
rumpuser_cv_broadcast(struct rumpuser_cv *cv)
{

	wakeup_all(&cv->waiters);
}

void
rumpuser_cv_has_waiters(struct rumpuser_cv *cv, int *rvp)
{

	*rvp = cv->nwaiters != 0;
}

/*
 * curlwp
 */

void
rumpuser_curlwpop(int enum_rumplwpop, struct lwp *l)
{
	struct thread *thread;
	enum rumplwpop op = enum_rumplwpop;

	switch (op) {
	case RUMPUSER_LWP_CREATE:
	case RUMPUSER_LWP_DESTROY:
		break;
	case RUMPUSER_LWP_SET:
		thread = get_current();
		thread->lwp = l;
		break;
	case RUMPUSER_LWP_CLEAR:
		thread = get_current();
		assert(thread->lwp == l);
		thread->lwp = NULL;
		break;
	}
}

struct lwp *
rumpuser_curlwp(void)
{

	return get_current()->lwp;
}

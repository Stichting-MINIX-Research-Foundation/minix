/*
 * test100: <pthread.h> over libmthread, as ported software uses it:
 * mutex types and attributes (recursive, errorcheck), zero-filled and
 * statically initialized mutexes and condition variables, trylock, and
 * pthread_cond_wait() with a recursive mutex held more than once,
 * pthread_cond_timedwait() (timeouts, wakeups, CLOCK_MONOTONIC conditions),
 * thread IDs (never 0) and sched_yield().
 */
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int max_error = 5;
#include "common.h"

static void
test_types(void)
{
	pthread_mutexattr_t attr;
	pthread_mutex_t m;
	int type;

	subtest = 1;

	if (pthread_mutexattr_init(&attr) != 0) e(1);
	if (pthread_mutexattr_gettype(&attr, &type) != 0) e(2);
	if (type != PTHREAD_MUTEX_DEFAULT) e(3);
	if (pthread_mutexattr_settype(&attr, 12345) != EINVAL) e(4);

	/* Recursive: the owner may lock again, and must unlock as often. */
	if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0)
		e(5);
	if (pthread_mutexattr_gettype(&attr, &type) != 0 ||
	    type != PTHREAD_MUTEX_RECURSIVE) e(6);
	if (pthread_mutex_init(&m, &attr) != 0) e(7);
	if (pthread_mutex_lock(&m) != 0) e(8);
	if (pthread_mutex_lock(&m) != 0) e(9);
	if (pthread_mutex_trylock(&m) != 0) e(10);
	if (pthread_mutex_unlock(&m) != 0) e(11);
	if (pthread_mutex_unlock(&m) != 0) e(12);
	if (pthread_mutex_destroy(&m) != EBUSY) e(13);	/* still held */
	if (pthread_mutex_unlock(&m) != 0) e(14);
	if (pthread_mutex_unlock(&m) != EPERM) e(15);	/* not held */
	if (pthread_mutex_destroy(&m) != 0) e(16);

	/* Errorcheck: relocking reports EDEADLK. */
	if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK) != 0)
		e(17);
	if (pthread_mutex_init(&m, &attr) != 0) e(18);
	if (pthread_mutex_lock(&m) != 0) e(19);
	if (pthread_mutex_lock(&m) != EDEADLK) e(20);
	if (pthread_mutex_trylock(&m) != EDEADLK) e(21);
	if (pthread_mutex_unlock(&m) != 0) e(22);
	if (pthread_mutex_destroy(&m) != 0) e(23);
	if (pthread_mutexattr_destroy(&attr) != 0) e(24);
}

static void
test_lazy(void)
{
	static pthread_mutex_t sm = PTHREAD_MUTEX_INITIALIZER;
	static pthread_cond_t sc = PTHREAD_COND_INITIALIZER;
	struct { pthread_mutex_t m; pthread_cond_t c; } *z;

	subtest = 2;

	/* Statically initialized. */
	if (pthread_mutex_lock(&sm) != 0) e(1);
	if (pthread_cond_signal(&sc) != 0) e(2);
	if (pthread_mutex_unlock(&sm) != 0) e(3);

	/* Zero-filled memory is a valid unlocked mutex / condition. */
	if ((z = calloc(1, sizeof(*z))) == NULL) { e(4); return; }
	if (pthread_mutex_trylock(&z->m) != 0) e(5);
	if (pthread_mutex_trylock(&z->m) != EDEADLK) e(6);
	if (pthread_mutex_unlock(&z->m) != 0) e(7);
	if (pthread_cond_broadcast(&z->c) != 0) e(8);
	if (pthread_mutex_destroy(&z->m) != 0) e(9);
	if (pthread_cond_destroy(&z->c) != 0) e(10);
	free(z);

	/* Condition variable attributes are accepted. */
	{
		pthread_condattr_t ca;
		pthread_cond_t c;

		if (pthread_condattr_init(&ca) != 0) e(11);
		if (pthread_cond_init(&c, &ca) != 0) e(12);
		if (pthread_cond_destroy(&c) != 0) e(13);
		if (pthread_condattr_destroy(&ca) != 0) e(14);
	}
}

static pthread_mutex_t rm;
static pthread_cond_t rc;
static int flag;

static void *
signaller(void *arg)
{

	(void)arg;
	/* The waiter released the recursive mutex completely. */
	if (pthread_mutex_trylock(&rm) != 0) e(1);
	flag = 1;
	if (pthread_cond_signal(&rc) != 0) e(2);
	if (pthread_mutex_unlock(&rm) != 0) e(3);
	return NULL;
}

static void
test_cond_recursive(void)
{
	pthread_mutexattr_t attr;
	pthread_t t;

	subtest = 3;

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	if (pthread_mutex_init(&rm, &attr) != 0) e(4);
	if (pthread_cond_init(&rc, NULL) != 0) e(5);

	if (pthread_mutex_lock(&rm) != 0) e(6);
	if (pthread_mutex_lock(&rm) != 0) e(7);	/* held twice */
	if (pthread_create(&t, NULL, signaller, NULL) != 0) e(8);
	while (!flag)
		if (pthread_cond_wait(&rc, &rm) != 0) { e(9); break; }
	/* Held twice again after the wait. */
	if (pthread_mutex_unlock(&rm) != 0) e(10);
	if (pthread_mutex_unlock(&rm) != 0) e(11);
	if (pthread_mutex_unlock(&rm) != EPERM) e(12);
	if (pthread_join(t, NULL) != 0) e(13);
	pthread_mutex_destroy(&rm);
	pthread_cond_destroy(&rc);
	pthread_mutexattr_destroy(&attr);
}

static pthread_mutex_t tm = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t tc;
static int tflag;

static void *
timed_signaller(void *arg)
{

	(void)arg;
	pthread_mutex_lock(&tm);
	tflag = 1;
	pthread_cond_signal(&tc);
	pthread_mutex_unlock(&tm);
	return NULL;
}

static long
ms_since(clockid_t clk, const struct timespec *t0)
{
	struct timespec t1;

	clock_gettime(clk, &t1);
	return (t1.tv_sec - t0->tv_sec) * 1000L +
	    (t1.tv_nsec - t0->tv_nsec) / 1000000L;
}

static void
deadline(clockid_t clk, struct timespec *ts, long ms)
{

	clock_gettime(clk, ts);
	ts->tv_sec += ms / 1000;
	ts->tv_nsec += (ms % 1000) * 1000000L;
	if (ts->tv_nsec >= 1000000000L) {
		ts->tv_sec++;
		ts->tv_nsec -= 1000000000L;
	}
}

static void
test_timedwait(void)
{
	pthread_condattr_t ca;
	struct timespec ts, t0;
	clockid_t clk;
	pthread_t t;
	long ms;
	int r;

	subtest = 4;

	if (pthread_cond_init(&tc, NULL) != 0) e(1);

	/* No other thread: times out after (about) the given time. */
	clock_gettime(CLOCK_REALTIME, &t0);
	deadline(CLOCK_REALTIME, &ts, 300);
	pthread_mutex_lock(&tm);
	if ((r = pthread_cond_timedwait(&tc, &tm, &ts)) != ETIMEDOUT) e(2);
	/* The mutex is held again on return. */
	if (pthread_mutex_trylock(&tm) != EDEADLK) e(3);
	pthread_mutex_unlock(&tm);
	ms = ms_since(CLOCK_REALTIME, &t0);
	if (ms < 250 || ms > 3000) e(4);

	/* A deadline in the past times out at once. */
	ts.tv_sec -= 10;
	pthread_mutex_lock(&tm);
	if (pthread_cond_timedwait(&tc, &tm, &ts) != ETIMEDOUT) e(5);
	/* Invalid timespec. */
	ts.tv_nsec = 1000000000L;
	if (pthread_cond_timedwait(&tc, &tm, &ts) != EINVAL) e(6);
	pthread_mutex_unlock(&tm);

	/* Signaled by another thread long before the deadline. */
	tflag = 0;
	clock_gettime(CLOCK_REALTIME, &t0);
	deadline(CLOCK_REALTIME, &ts, 10000);
	pthread_mutex_lock(&tm);
	if (pthread_create(&t, NULL, timed_signaller, NULL) != 0) e(7);
	while (!tflag) {
		if ((r = pthread_cond_timedwait(&tc, &tm, &ts)) != 0) {
			e(8);
			break;
		}
	}
	pthread_mutex_unlock(&tm);
	if (pthread_join(t, NULL) != 0) e(9);
	if (ms_since(CLOCK_REALTIME, &t0) > 5000) e(10);
	if (pthread_cond_destroy(&tc) != 0) e(11);

	/* Condition variables on CLOCK_MONOTONIC. */
	if (pthread_condattr_init(&ca) != 0) e(12);
	if (pthread_condattr_getclock(&ca, &clk) != 0 ||
	    clk != CLOCK_REALTIME) e(13);
	if (pthread_condattr_setclock(&ca, 12345) != EINVAL) e(14);
	if (pthread_condattr_setclock(&ca, CLOCK_MONOTONIC) != 0) e(15);
	if (pthread_condattr_getclock(&ca, &clk) != 0 ||
	    clk != CLOCK_MONOTONIC) e(16);
	if (pthread_cond_init(&tc, &ca) != 0) e(17);
	clock_gettime(CLOCK_MONOTONIC, &t0);
	deadline(CLOCK_MONOTONIC, &ts, 200);
	pthread_mutex_lock(&tm);
	if (pthread_cond_timedwait(&tc, &tm, &ts) != ETIMEDOUT) e(18);
	pthread_mutex_unlock(&tm);
	ms = ms_since(CLOCK_MONOTONIC, &t0);
	if (ms < 150 || ms > 3000) e(19);
	if (pthread_cond_destroy(&tc) != 0) e(20);
	pthread_condattr_destroy(&ca);
}

/*
 * Subtest 5: thread IDs and sched_yield().  pthread_t 0 must never name a
 * thread (libstdc++'s std::thread treats it as "no thread"; libmthread's
 * first thread used to get ID 0), IDs round-trip through pthread_self(),
 * pthread_join() and pthread_equal(), and sched_yield() switches threads.
 */
static volatile int yield_turns;
static pthread_t self_seen[2];

static void *
yield_thread(void *arg)
{
	int idx = (int) (intptr_t) arg;
	int i;

	self_seen[idx] = pthread_self();
	for (i = 0; i < 5; i++) {
		yield_turns++;
		sched_yield();
	}
	return (void *) (intptr_t) (idx + 10);
}

static void
test_ids_yield(void)
{
	pthread_t t[2], me;
	void *rv;
	int i, before;

	subtest = 5;

	me = pthread_self();
	if (me == 0) e(1);
	if (!pthread_equal(me, pthread_self())) e(2);
	for (i = 0; i < 2; i++) {
		if (pthread_create(&t[i], NULL, yield_thread,
		    (void *) (intptr_t) i) != 0) e(3);
		if (t[i] == 0) e(4);
		if (pthread_equal(t[i], me)) e(5);
	}
	if (pthread_equal(t[0], t[1])) e(6);

	/* Neither thread has run yet: they only run when this one yields. */
	before = yield_turns;
	if (sched_yield() != 0) e(7);
	if (yield_turns == before) e(8);

	for (i = 0; i < 2; i++) {
		if (pthread_join(t[i], &rv) != 0) e(9);
		if ((intptr_t) rv != i + 10) e(10);
		if (!pthread_equal(self_seen[i], t[i])) e(11);
	}
	if (yield_turns != 10) e(12);
	if (pthread_join(me, NULL) != EDEADLK) e(13);
	if (pthread_join(0, NULL) == 0) e(14);
}

int
main(int argc, char **argv)
{

	start(100);

	test_types();
	test_lazy();
	test_cond_recursive();
	test_timedwait();
	test_ids_yield();

	quit();
	return 0;
}

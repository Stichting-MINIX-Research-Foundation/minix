/*
 * test102: unnamed POSIX semaphores (<semaphore.h>) and C11 threads
 * (<threads.h>), both provided by libmthread, as ported software uses them
 * (foot's render workers: a "start" and a "done" semaphore; fcft: C11
 * mutexes and condition variables): counting, trywait/timedwait, waking
 * waiting threads; thread results, mutex types and timed locks, condition
 * variables, call_once and thread-specific storage with destructors; and
 * a default thread stack big enough for ported code (it was 8 KB).
 */
#include <errno.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <time.h>

int max_error = 5;
#include "common.h"

static struct timespec
deadline(long ms)
{
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += ms / 1000;
	ts.tv_nsec += (ms % 1000) * 1000000L;
	if (ts.tv_nsec >= 1000000000L) {
		ts.tv_sec++;
		ts.tv_nsec -= 1000000000L;
	}
	return ts;
}

static void
test_sem_basic(void)
{
	sem_t s;
	struct timespec ts;
	int v;

	subtest = 1;

	errno = 0;
	if (sem_init(&s, 1, 0) != -1 || errno != ENOSYS) e(1);
	if (sem_open("/x", 0) != SEM_FAILED || errno != ENOSYS) e(2);

	if (sem_init(&s, 0, 2) != 0) e(3);
	if (sem_getvalue(&s, &v) != 0 || v != 2) e(4);
	if (sem_trywait(&s) != 0) e(5);
	if (sem_wait(&s) != 0) e(6);
	errno = 0;
	if (sem_trywait(&s) != -1 || errno != EAGAIN) e(7);

	/* Nobody posts: a timed wait times out (it must not hang). */
	ts = deadline(100);
	errno = 0;
	if (sem_timedwait(&s, &ts) != -1 || errno != ETIMEDOUT) e(8);
	ts.tv_nsec = 1000000000L;
	errno = 0;
	if (sem_timedwait(&s, &ts) != -1 || errno != EINVAL) e(9);

	if (sem_post(&s) != 0 || sem_post(&s) != 0) e(10);
	if (sem_getvalue(&s, &v) != 0 || v != 2) e(11);
	/* Available: no timeout even with a past deadline. */
	ts.tv_sec = 0;
	ts.tv_nsec = 0;
	if (sem_timedwait(&s, &ts) != 0) e(12);

	if (sem_destroy(&s) != 0) e(13);
	errno = 0;
	if (sem_post(&s) != -1 || errno != EINVAL) e(14);
}

/* foot's render worker pattern. */
#define WORKERS	3
#define ROUNDS	20

static sem_t start_sem, done_sem;
static int work_done;

static int
worker(void *arg)
{
	int id = (int)(intptr_t)arg;

	for (;;) {
		if (sem_wait(&start_sem) != 0)
			return -1;
		if (work_done < 0)	/* told to quit */
			return id;
		work_done++;
		if (sem_post(&done_sem) != 0)
			return -1;
	}
}

static void
test_sem_threads(void)
{
	thrd_t t[WORKERS];
	int i, r, res;

	subtest = 2;

	if (sem_init(&start_sem, 0, 0) != 0) e(1);
	if (sem_init(&done_sem, 0, 0) != 0) e(2);
	for (i = 0; i < WORKERS; i++)
		if (thrd_create(&t[i], worker, (void *)(intptr_t)(i + 1)) !=
		    thrd_success) e(3);

	for (r = 0; r < ROUNDS; r++) {
		for (i = 0; i < WORKERS; i++)
			if (sem_post(&start_sem) != 0) e(4);
		for (i = 0; i < WORKERS; i++)
			if (sem_wait(&done_sem) != 0) e(5);
	}
	if (work_done != ROUNDS * WORKERS) e(6);

	/* A destroy while threads wait fails (let them all block first). */
	for (i = 0; i < WORKERS; i++)
		thrd_yield();
	errno = 0;
	if (sem_destroy(&start_sem) != -1 || errno != EBUSY) e(7);

	work_done = -1;
	for (i = 0; i < WORKERS; i++)
		if (sem_post(&start_sem) != 0) e(8);
	for (i = 0; i < WORKERS; i++) {
		if (thrd_join(t[i], &res) != thrd_success) e(9);
		if (res != i + 1) e(10);
	}
	if (sem_destroy(&start_sem) != 0) e(11);
	if (sem_destroy(&done_sem) != 0) e(12);
}

static mtx_t lock;
static cnd_t cond;
static int ready, woken;
static thrd_t main_thr;

static int
waiter(void *arg)
{
	(void)arg;
	if (thrd_equal(thrd_current(), main_thr)) return -1;
	if (mtx_lock(&lock) != thrd_success) return -1;
	while (!ready)
		if (cnd_wait(&cond, &lock) != thrd_success) return -1;
	woken++;
	if (mtx_unlock(&lock) != thrd_success) return -1;
	return 7;
}

static int
timed_locker(void *arg)
{
	struct timespec ts = deadline(100);

	(void)arg;
	/* main holds the lock and only gives it up after joining us. */
	return mtx_timedlock(&lock, &ts);
}

static void
test_c11_sync(void)
{
	thrd_t t[2];
	struct timespec ts;
	mtx_t rec;
	int i, res;

	subtest = 3;

	main_thr = thrd_current();
	if (!thrd_equal(main_thr, thrd_current())) e(1);

	if (mtx_init(&lock, mtx_plain) != thrd_success) e(2);
	if (mtx_init(&rec, mtx_recursive | mtx_timed) != thrd_success) e(3);
	if (mtx_init(&rec, 0x40) == thrd_success) e(4);
	if (mtx_init(&rec, mtx_recursive) != thrd_success) e(5);
	if (cnd_init(&cond) != thrd_success) e(6);

	/* Recursive: lock twice, unlock twice. */
	if (mtx_lock(&rec) != thrd_success) e(7);
	if (mtx_lock(&rec) != thrd_success) e(8);
	if (mtx_trylock(&rec) != thrd_success) e(9);
	for (i = 0; i < 3; i++)
		if (mtx_unlock(&rec) != thrd_success) e(10);
	mtx_destroy(&rec);

	/* Plain: trylock on a held mutex is busy. */
	if (mtx_lock(&lock) != thrd_success) e(11);
	if (mtx_trylock(&lock) != thrd_busy) e(12);

	/* Timed lock against a holder that does not let go. */
	if (thrd_create(&t[0], timed_locker, NULL) != thrd_success) e(13);
	if (thrd_join(t[0], &res) != thrd_success) e(14);
	if (res != thrd_timedout) e(15);
	if (mtx_unlock(&lock) != thrd_success) e(16);

	/* Condition variable: two waiters, one broadcast. */
	for (i = 0; i < 2; i++)
		if (thrd_create(&t[i], waiter, NULL) != thrd_success) e(17);
	thrd_yield();		/* let them block in cnd_wait */
	if (mtx_lock(&lock) != thrd_success) e(18);
	ready = 1;
	if (cnd_broadcast(&cond) != thrd_success) e(19);
	if (mtx_unlock(&lock) != thrd_success) e(20);
	for (i = 0; i < 2; i++) {
		if (thrd_join(t[i], &res) != thrd_success) e(21);
		if (res != 7) e(22);
	}
	if (woken != 2) e(23);

	/* Timed wait without a signal. */
	if (mtx_lock(&lock) != thrd_success) e(24);
	ts = deadline(50);
	if (cnd_timedwait(&cond, &lock, &ts) != thrd_timedout) e(25);
	if (mtx_unlock(&lock) != thrd_success) e(26);

	cnd_destroy(&cond);
	mtx_destroy(&lock);

	/* Zero-filled objects work, like the pthread ones. */
	{
		static mtx_t zm;
		if (mtx_lock(&zm) != thrd_success) e(27);
		if (mtx_unlock(&zm) != thrd_success) e(28);
	}

	ts.tv_sec = 0;
	ts.tv_nsec = 10000000L;
	if (thrd_sleep(&ts, NULL) != 0) e(29);
}

static once_flag once = ONCE_FLAG_INIT;
static int once_count;
static tss_t key;
static int dtor_count;

static void
once_func(void)
{
	once_count++;
}

static void
dtor(void *p)
{
	if (p == (void *)&dtor_count)
		dtor_count++;
}

static int
tss_thread(void *arg)
{
	call_once(&once, once_func);
	if (tss_get(key) != NULL) return 1;
	if (tss_set(key, &dtor_count) != thrd_success) return 2;
	if (tss_get(key) != &dtor_count) return 3;
	(void)arg;
	thrd_yield();
	if (tss_get(key) != &dtor_count) return 4;
	return 0;
}

static int
exiting_thread(void *arg)
{
	(void)arg;
	thrd_exit(42);
	return 0;
}

static void
test_c11_misc(void)
{
	thrd_t t[3];
	int i, res;

	subtest = 4;

	if (tss_create(&key, dtor) != thrd_success) e(1);
	for (i = 0; i < 2; i++)
		if (thrd_create(&t[i], tss_thread, (void *)(intptr_t)(i + 1)) !=
		    thrd_success) e(2);
	for (i = 0; i < 2; i++) {
		if (thrd_join(t[i], &res) != thrd_success) e(3);
		if (res != 0) e(4);
	}
	call_once(&once, once_func);
	if (once_count != 1) e(5);
	if (dtor_count != 2) e(6);
	if (tss_get(key) != NULL) e(8);
	tss_delete(key);

	if (thrd_create(&t[2], exiting_thread, NULL) != thrd_success) e(9);
	if (thrd_join(t[2], &res) != thrd_success) e(10);
	if (res != 42) e(11);
}

/* Ported software expects a sizeable default stack (glibc: 8 MB). */
#define BIG_FRAME	(256 * 1024)

static int
deep_stack(void *arg)
{
	volatile char buf[BIG_FRAME];
	size_t i;
	int sum = 0;

	(void)arg;
	for (i = 0; i < sizeof(buf); i += 512)
		buf[i] = (char)(i >> 9);
	for (i = 0; i < sizeof(buf); i += 512)
		sum += buf[i];
	return sum != 0 ? 5 : 0;
}

static void
test_default_stack(void)
{
	thrd_t t;
	int res;

	subtest = 5;

	if (thrd_create(&t, deep_stack, NULL) != thrd_success) e(1);
	if (thrd_join(t, &res) != thrd_success) e(2);
	if (res != 5) e(3);
}

int
main(int argc, char **argv)
{

	start(102);

	test_sem_basic();
	test_sem_threads();
	test_c11_sync();
	test_c11_misc();
	test_default_stack();

	quit();
	return 0;
}

/*
 * c11threads.c - C11 threads (<threads.h>) over libmthread's POSIX threads.
 *
 * Thin wrappers: C11 status codes instead of error numbers, and an int
 * thread result passed through pthread's void * one.  mtx_timedlock() polls
 * with trylock and yields to the other (cooperative) threads until the
 * deadline - libmthread has no timed mutex wait.
 */
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <threads.h>
#include <time.h>

#include <minix/mthread.h>

struct thrd_start {
	thrd_start_t	func;
	void		*arg;
};

static int
status(int r)
{
	switch (r) {
	case 0:
		return thrd_success;
	case EBUSY:
	case EDEADLK:
		return thrd_busy;
	case ENOMEM:
	case EAGAIN:
		return thrd_nomem;
	case ETIMEDOUT:
		return thrd_timedout;
	default:
		return thrd_error;
	}
}

static void *
thrd_trampoline(void *arg)
{
	struct thrd_start s = *(struct thrd_start *)arg;

	free(arg);
	return (void *)(intptr_t)s.func(s.arg);
}

int
thrd_create(thrd_t *thr, thrd_start_t func, void *arg)
{
	struct thrd_start *s;
	int r;

	if ((s = malloc(sizeof(*s))) == NULL)
		return thrd_nomem;
	s->func = func;
	s->arg = arg;
	if ((r = pthread_create(thr, NULL, thrd_trampoline, s)) != 0) {
		free(s);
		return status(r);
	}
	return thrd_success;
}

thrd_t
thrd_current(void)
{
	return pthread_self();
}

int
thrd_detach(thrd_t thr)
{
	return pthread_detach(thr) == 0 ? thrd_success : thrd_error;
}

int
thrd_equal(thrd_t a, thrd_t b)
{
	return pthread_equal(a, b);
}

void
thrd_exit(int res)
{
	pthread_exit((void *)(intptr_t)res);
	/* libmthread returns here for the main thread: end the process. */
	exit(res);
}

int
thrd_join(thrd_t thr, int *res)
{
	void *v;

	if (pthread_join(thr, &v) != 0)
		return thrd_error;
	if (res != NULL)
		*res = (int)(intptr_t)v;
	return thrd_success;
}

int
thrd_sleep(const struct timespec *duration, struct timespec *remaining)
{
	if (nanosleep(duration, remaining) == 0)
		return 0;
	return errno == EINTR ? -1 : -2;
}

void
thrd_yield(void)
{
	mthread_yield();
}

int
mtx_init(mtx_t *mtx, int type)
{
	pthread_mutexattr_t attr;
	int r;

	if ((type & ~(mtx_recursive | mtx_timed)) != 0)
		return thrd_error;
	if (pthread_mutexattr_init(&attr) != 0)
		return thrd_error;
	if ((type & mtx_recursive) &&
	    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0) {
		pthread_mutexattr_destroy(&attr);
		return thrd_error;
	}
	*mtx = NULL;
	r = pthread_mutex_init(mtx, &attr);
	pthread_mutexattr_destroy(&attr);
	return r == 0 ? thrd_success : thrd_error;
}

void
mtx_destroy(mtx_t *mtx)
{
	pthread_mutex_destroy(mtx);
}

int
mtx_lock(mtx_t *mtx)
{
	return pthread_mutex_lock(mtx) == 0 ? thrd_success : thrd_error;
}

int
mtx_trylock(mtx_t *mtx)
{
	return status(pthread_mutex_trylock(mtx));
}

int
mtx_timedlock(mtx_t *__restrict mtx, const struct timespec *__restrict ts)
{
	struct timespec now;
	int r;

	for (;;) {
		r = pthread_mutex_trylock(mtx);
		if (r != EBUSY && r != EDEADLK)
			return r == 0 ? thrd_success : thrd_error;
		if (clock_gettime(CLOCK_REALTIME, &now) != 0)
			return thrd_error;
		if (now.tv_sec > ts->tv_sec ||
		    (now.tv_sec == ts->tv_sec && now.tv_nsec >= ts->tv_nsec))
			return thrd_timedout;
		mthread_yield();
	}
}

int
mtx_unlock(mtx_t *mtx)
{
	return pthread_mutex_unlock(mtx) == 0 ? thrd_success : thrd_error;
}

int
cnd_init(cnd_t *cnd)
{
	int r;

	*cnd = NULL;
	r = pthread_cond_init(cnd, NULL);
	return r == 0 ? thrd_success : status(r);
}

void
cnd_destroy(cnd_t *cnd)
{
	pthread_cond_destroy(cnd);
}

int
cnd_signal(cnd_t *cnd)
{
	return pthread_cond_signal(cnd) == 0 ? thrd_success : thrd_error;
}

int
cnd_broadcast(cnd_t *cnd)
{
	return pthread_cond_broadcast(cnd) == 0 ? thrd_success : thrd_error;
}

int
cnd_wait(cnd_t *cnd, mtx_t *mtx)
{
	return pthread_cond_wait(cnd, mtx) == 0 ? thrd_success : thrd_error;
}

int
cnd_timedwait(cnd_t *__restrict cnd, mtx_t *__restrict mtx,
    const struct timespec *__restrict ts)
{
	int r = pthread_cond_timedwait(cnd, mtx, ts);

	if (r == ETIMEDOUT)
		return thrd_timedout;
	return r == 0 ? thrd_success : thrd_error;
}

void
call_once(once_flag *flag, void (*func)(void))
{
	pthread_once(flag, func);
}

int
tss_create(tss_t *key, tss_dtor_t dtor)
{
	return pthread_key_create(key, dtor) == 0 ? thrd_success : thrd_error;
}

void
tss_delete(tss_t key)
{
	pthread_key_delete(key);
}

void *
tss_get(tss_t key)
{
	return pthread_getspecific(key);
}

int
tss_set(tss_t key, void *val)
{
	return pthread_setspecific(key, val) == 0 ? thrd_success : thrd_error;
}

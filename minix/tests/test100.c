/*
 * test100: <pthread.h> over libmthread, as ported software uses it:
 * mutex types and attributes (recursive, errorcheck), zero-filled and
 * statically initialized mutexes and condition variables, trylock, and
 * pthread_cond_wait() with a recursive mutex held more than once.
 */
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int
main(int argc, char **argv)
{

	start(100);

	test_types();
	test_lazy();
	test_cond_recursive();

	quit();
	return 0;
}

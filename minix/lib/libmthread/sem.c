/*
 * sem.c - unnamed POSIX semaphores (<semaphore.h>) over libmthread.
 *
 * MINIX has no kernel semaphores and no kernel threads: a semaphore is a
 * counter guarded by a mutex, with a condition variable for waiters, all
 * within one process.  Process-shared (pshared != 0) and named semaphores
 * are not supported (ENOSYS).  As with every libmthread primitive, a
 * thread waiting on a semaphore lets the other threads of the process
 * run; a process whose only thread waits on a semaphore that nobody can
 * post never wakes up.
 */
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <time.h>

#define SEM_MAGIC	0x53454d31	/* "SEM1" */

struct _sem_st {
	unsigned int	ss_magic;
	pthread_mutex_t	ss_lock;
	pthread_cond_t	ss_cond;
	unsigned int	ss_value;
	unsigned int	ss_waiters;
};

static struct _sem_st *
sem_get(sem_t *sem)
{
	if (sem == NULL || *sem == NULL || (*sem)->ss_magic != SEM_MAGIC) {
		errno = EINVAL;
		return NULL;
	}
	return *sem;
}

int
sem_init(sem_t *sem, int pshared, unsigned int value)
{
	struct _sem_st *s;
	int r;

	if (pshared != 0) {
		errno = ENOSYS;
		return -1;
	}
	if (sem == NULL) {
		errno = EINVAL;
		return -1;
	}
	if ((s = malloc(sizeof(*s))) == NULL) {
		errno = ENOSPC;
		return -1;
	}
	s->ss_lock = NULL;
	s->ss_cond = NULL;
	if ((r = pthread_mutex_init(&s->ss_lock, NULL)) != 0) {
		free(s);
		errno = r == ENOMEM ? ENOSPC : r;
		return -1;
	}
	if ((r = pthread_cond_init(&s->ss_cond, NULL)) != 0) {
		pthread_mutex_destroy(&s->ss_lock);
		free(s);
		errno = r == ENOMEM ? ENOSPC : r;
		return -1;
	}
	s->ss_value = value;
	s->ss_waiters = 0;
	s->ss_magic = SEM_MAGIC;
	*sem = s;
	return 0;
}

int
sem_destroy(sem_t *sem)
{
	struct _sem_st *s;

	if ((s = sem_get(sem)) == NULL)
		return -1;
	if (s->ss_waiters > 0) {
		errno = EBUSY;
		return -1;
	}
	s->ss_magic = 0;
	pthread_cond_destroy(&s->ss_cond);
	pthread_mutex_destroy(&s->ss_lock);
	free(s);
	*sem = NULL;
	return 0;
}

int
sem_post(sem_t *sem)
{
	struct _sem_st *s;

	if ((s = sem_get(sem)) == NULL)
		return -1;
	pthread_mutex_lock(&s->ss_lock);
	if (s->ss_value == SEM_VALUE_MAX) {
		pthread_mutex_unlock(&s->ss_lock);
		errno = EOVERFLOW;
		return -1;
	}
	s->ss_value++;
	if (s->ss_waiters > 0)
		pthread_cond_signal(&s->ss_cond);
	pthread_mutex_unlock(&s->ss_lock);
	return 0;
}

/* Wait until the value is positive and take one; abstime NULL: no
 * timeout.  Returns 0 or an error number. */
static int
sem_take(struct _sem_st *s, const struct timespec *abstime)
{
	int r = 0;

	pthread_mutex_lock(&s->ss_lock);
	if (s->ss_value == 0 && abstime != NULL &&
	    (abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000L)) {
		pthread_mutex_unlock(&s->ss_lock);
		return EINVAL;
	}
	s->ss_waiters++;
	while (s->ss_value == 0) {
		if (abstime == NULL)
			r = pthread_cond_wait(&s->ss_cond, &s->ss_lock);
		else
			r = pthread_cond_timedwait(&s->ss_cond, &s->ss_lock,
			    abstime);
		if (r != 0)
			break;
	}
	s->ss_waiters--;
	if (s->ss_value > 0) {
		s->ss_value--;
		r = 0;
	}
	pthread_mutex_unlock(&s->ss_lock);
	return r;
}

int
sem_wait(sem_t *sem)
{
	struct _sem_st *s;
	int r;

	if ((s = sem_get(sem)) == NULL)
		return -1;
	if ((r = sem_take(s, NULL)) != 0) {
		errno = r;
		return -1;
	}
	return 0;
}

int
sem_timedwait(sem_t *sem, const struct timespec *abstime)
{
	struct _sem_st *s;
	int r;

	if ((s = sem_get(sem)) == NULL)
		return -1;
	if (abstime == NULL) {
		errno = EINVAL;
		return -1;
	}
	if ((r = sem_take(s, abstime)) != 0) {
		errno = r;
		return -1;
	}
	return 0;
}

int
sem_trywait(sem_t *sem)
{
	struct _sem_st *s;

	if ((s = sem_get(sem)) == NULL)
		return -1;
	pthread_mutex_lock(&s->ss_lock);
	if (s->ss_value == 0) {
		pthread_mutex_unlock(&s->ss_lock);
		errno = EAGAIN;
		return -1;
	}
	s->ss_value--;
	pthread_mutex_unlock(&s->ss_lock);
	return 0;
}

int
sem_getvalue(sem_t * __restrict sem, int * __restrict sval)
{
	struct _sem_st *s;

	if ((s = sem_get(sem)) == NULL)
		return -1;
	if (sval == NULL) {
		errno = EINVAL;
		return -1;
	}
	*sval = (int)s->ss_value;
	return 0;
}

/* Named semaphores: not supported. */
sem_t *
sem_open(const char *name, int oflag, ...)
{
	(void)name;
	(void)oflag;
	errno = ENOSYS;
	return SEM_FAILED;
}

int
sem_close(sem_t *sem)
{
	(void)sem;
	errno = ENOSYS;
	return -1;
}

int
sem_unlink(const char *name)
{
	(void)name;
	errno = ENOSYS;
	return -1;
}

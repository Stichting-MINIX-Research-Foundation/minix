/*	pthread.h - POSIX threads for MINIX, provided by libmthread.
 *
 * MINIX has no kernel threads: libmthread implements user-level
 * ("green") threads that switch only when a thread blocks in, or yields
 * through, an mthread call - a blocking system call stops all threads of the
 * process.  This header exposes libmthread's pthread compatibility API
 * (<minix/mthread.h> with _MTHREADIFY_PTHREADS) under the standard name, so
 * that ported software using mutexes, condition variables, once, keys and
 * (cooperative) threads builds unchanged.  Link with -lpthread (an alias of
 * -lmthread) or use gcc -pthread.
 *
 * $MINIX$
 */

#ifndef _PTHREAD_H_
#define _PTHREAD_H_

#ifndef _MTHREADIFY_PTHREADS
#define _MTHREADIFY_PTHREADS
#endif
#include <minix/mthread.h>
#include <signal.h>
#include <time.h>

#define PTHREAD_CREATE_JOINABLE	MTHREAD_CREATE_JOINABLE
#define PTHREAD_CREATE_DETACHED	MTHREAD_CREATE_DETACHED
#define PTHREAD_STACK_MIN	MTHREAD_STACK_MIN
#define PTHREAD_KEYS_MAX	MTHREAD_KEYS_MAX

/* Mutex types: libmthread mutexes are all of one (non-recursive) kind;
 * pthread_mutexattr_settype() fails with EINVAL (see <minix/mthread.h>). */
#define PTHREAD_MUTEX_NORMAL	0
#define PTHREAD_MUTEX_DEFAULT	0
#define PTHREAD_MUTEX_ERRORCHECK 2

__BEGIN_DECLS
static __inline int
pthread_condattr_init(pthread_condattr_t *__a)
{
	*__a = NULL;
	return 0;
}

static __inline int
pthread_condattr_destroy(pthread_condattr_t *__a)
{
	(void)__a;
	return 0;
}

/* pthread_sigmask(): declared by <signal.h>, in libc (one signal mask per
 * process - there are no kernel threads). */
__END_DECLS

#endif /* _PTHREAD_H_ */

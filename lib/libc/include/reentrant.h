/*	$NetBSD: reentrant.h,v 1.18 2015/01/20 18:31:25 christos Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by J.T. Conklin, by Nathan J. Williams, and by Jason R. Thorpe.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Requirements:
 * 
 * 1. The thread safe mechanism should be lightweight so the library can
 *    be used by non-threaded applications without unreasonable overhead.
 * 
 * 2. There should be no dependency on a thread engine for non-threaded
 *    applications.
 * 
 * 3. There should be no dependency on any particular thread engine.
 * 
 * 4. The library should be able to be compiled without support for thread
 *    safety.
 * 
 * 
 * Rationale:
 * 
 * One approach for thread safety is to provide discrete versions of the
 * library: one thread safe, the other not.  The disadvantage of this is
 * that libc is rather large, and two copies of a library which are 99%+
 * identical is not an efficent use of resources.
 * 
 * Another approach is to provide a single thread safe library.  However,
 * it should not add significant run time or code size overhead to non-
 * threaded applications.
 * 
 * Since the NetBSD C library is used in other projects, it should be
 * easy to replace the mutual exclusion primitives with ones provided by
 * another system.  Similarly, it should also be easy to remove all
 * support for thread safety completely if the target environment does
 * not support threads.
 * 
 * 
 * Implementation Details:
 * 
 * The thread primitives used by the library (mutex_t, mutex_lock, etc.)
 * are macros which expand to the cooresponding primitives provided by
 * the thread engine or to nothing.  The latter is used so that code is
 * not unreasonably cluttered with #ifdefs when all thread safe support
 * is removed.
 * 
 * The thread macros can be directly mapped to the mutex primitives from
 * pthreads, however it should be reasonably easy to wrap another mutex
 * implementation so it presents a similar interface.
 * 
 * The thread functions operate by dispatching to symbols which are, by
 * default, weak-aliased to no-op functions in thread-stub/thread-stub.c
 * (some uses of thread operations are conditional on __isthreaded, but
 * not all of them are).
 *
 * When the thread library is linked in, it provides strong-alias versions
 * of those symbols which dispatch to its own real thread operations.
 *
 */

#if !defined(__minix) || !defined(_LIBC_REENTRANT_H)
#ifdef __minix
/*
 * If _REENTRANT is not defined, the header may not be included more than once.
 * This is probably a NetBSD libc bug, but for now we solve it for MINIX3 only.
 */
#define _LIBC_REENTRANT_H
#endif /* __minix */

/*
 * Abstract thread interface for thread-safe libraries.  These routines
 * will use stubs in libc if the application is not linked against the
 * pthread library, and the real function in the pthread library if it
 * is.
 */

#ifndef __minix

#include <pthread.h>
#include <signal.h>

#define	mutex_t			pthread_mutex_t
#define	MUTEX_INITIALIZER	PTHREAD_MUTEX_INITIALIZER

#define	mutexattr_t		pthread_mutexattr_t

#define	MUTEX_TYPE_NORMAL	PTHREAD_MUTEX_NORMAL
#define	MUTEX_TYPE_ERRORCHECK	PTHREAD_MUTEX_ERRORCHECK
#define	MUTEX_TYPE_RECURSIVE	PTHREAD_MUTEX_RECURSIVE

#define	cond_t			pthread_cond_t
#define	COND_INITIALIZER	PTHREAD_COND_INITIALIZER

#define	condattr_t		pthread_condattr_t

#define	rwlock_t		pthread_rwlock_t
#define	RWLOCK_INITIALIZER	PTHREAD_RWLOCK_INITIALIZER

#define	rwlockattr_t		pthread_rwlockattr_t

#define	thread_key_t		pthread_key_t

#define	thr_t			pthread_t

#define	thrattr_t		pthread_attr_t

#define	once_t			pthread_once_t
#define	ONCE_INITIALIZER	PTHREAD_ONCE_INIT

#else /* __minix */

typedef struct {
	int pto_done;
} once_t;
#define ONCE_INITIALIZER	{ .pto_done = 0 }

#endif /* __minix */

#ifdef _REENTRANT

#ifndef __LIBC_THREAD_STUBS

__BEGIN_DECLS
int	__libc_mutex_init(mutex_t *, const mutexattr_t *);
int	__libc_mutex_lock(mutex_t *);
int	__libc_mutex_trylock(mutex_t *);
int	__libc_mutex_unlock(mutex_t *);
int	__libc_mutex_destroy(mutex_t *);

int	__libc_mutexattr_init(mutexattr_t *);
int	__libc_mutexattr_settype(mutexattr_t *, int);
int	__libc_mutexattr_destroy(mutexattr_t *);
__END_DECLS

#define	mutex_init(m, a)	__libc_mutex_init((m), (a))
#define	mutex_lock(m)		__libc_mutex_lock((m))
#define	mutex_trylock(m)	__libc_mutex_trylock((m))
#define	mutex_unlock(m)		__libc_mutex_unlock((m))
#define	mutex_destroy(m)	__libc_mutex_destroy((m))

#define	mutexattr_init(ma)	__libc_mutexattr_init((ma))
#define	mutexattr_settype(ma, t) __libc_mutexattr_settype((ma), (t))
#define	mutexattr_destroy(ma)	__libc_mutexattr_destroy((ma))

__BEGIN_DECLS
int	__libc_cond_init(cond_t *, const condattr_t *);
int	__libc_cond_signal(cond_t *);
int	__libc_cond_broadcast(cond_t *);
int	__libc_cond_wait(cond_t *, mutex_t *);
#ifndef __LIBC12_SOURCE__
int	__libc_cond_timedwait(cond_t *, mutex_t *, const struct timespec *);
#endif
int	__libc_cond_destroy(cond_t *);
__END_DECLS

#define	cond_init(c, t, a)     	__libc_cond_init((c), (a))
#define	cond_signal(c)		__libc_cond_signal((c))
#define	cond_broadcast(c)	__libc_cond_broadcast((c))
#define	cond_wait(c, m)		__libc_cond_wait((c), (m))
#define	cond_timedwait(c, m, t)	__libc_cond_timedwait((c), (m), (t))
#define	cond_destroy(c)		__libc_cond_destroy((c))

__BEGIN_DECLS
int	__libc_rwlock_init(rwlock_t *, const rwlockattr_t *);
int	__libc_rwlock_rdlock(rwlock_t *);
int	__libc_rwlock_wrlock(rwlock_t *);
int	__libc_rwlock_tryrdlock(rwlock_t *);
int	__libc_rwlock_trywrlock(rwlock_t *);
int	__libc_rwlock_unlock(rwlock_t *);
int	__libc_rwlock_destroy(rwlock_t *);
__END_DECLS

#define	rwlock_init(l, a)	__libc_rwlock_init((l), (a))
#define	rwlock_rdlock(l)	__libc_rwlock_rdlock((l))
#define	rwlock_wrlock(l)	__libc_rwlock_wrlock((l))
#define	rwlock_tryrdlock(l)	__libc_rwlock_tryrdlock((l))
#define	rwlock_trywrlock(l)	__libc_rwlock_trywrlock((l))
#define	rwlock_unlock(l)	__libc_rwlock_unlock((l))
#define	rwlock_destroy(l)	__libc_rwlock_destroy((l))

__BEGIN_DECLS
int	__libc_thr_keycreate(thread_key_t *, void (*)(void *));
int	__libc_thr_setspecific(thread_key_t, const void *);
void	*__libc_thr_getspecific(thread_key_t);
int	__libc_thr_keydelete(thread_key_t);
__END_DECLS

#define	thr_keycreate(k, d)	__libc_thr_keycreate((k), (d))
#define	thr_setspecific(k, p)	__libc_thr_setspecific((k), (p))
#define	thr_getspecific(k)	__libc_thr_getspecific((k))
#define	thr_keydelete(k)	__libc_thr_keydelete((k))

__BEGIN_DECLS
int	__libc_thr_once(once_t *, void (*)(void));
int	__libc_thr_sigsetmask(int, const sigset_t *, sigset_t *);
thr_t	__libc_thr_self(void);
int	__libc_thr_yield(void);
void	__libc_thr_create(thr_t *, const thrattr_t *,
	    void *(*)(void *), void *);
void	__libc_thr_exit(void *) __attribute__((__noreturn__));
int	*__libc_thr_errno(void);
int	__libc_thr_setcancelstate(int, int *);
unsigned int	__libc_thr_curcpu(void);

extern int __isthreaded;
__END_DECLS

#define	thr_once(o, f)		__libc_thr_once((o), (f))
#define	thr_sigsetmask(f, n, o)	__libc_thr_sigsetmask((f), (n), (o))
#define	thr_self()		__libc_thr_self()
#define	thr_yield()		__libc_thr_yield()
#define	thr_create(tp, ta, f, a) __libc_thr_create((tp), (ta), (f), (a))
#define	thr_exit(v)		__libc_thr_exit((v))
#define	thr_errno()		__libc_thr_errno()
#define	thr_enabled()		(__isthreaded)
#define thr_setcancelstate(n, o) __libc_thr_setcancelstate((n),(o))
#define thr_curcpu()		__libc_thr_curcpu()

#else /* __LIBC_THREAD_STUBS */

__BEGIN_DECLS
void	__libc_thr_init_stub(void);

int	__libc_mutex_init_stub(mutex_t *, const mutexattr_t *);
int	__libc_mutex_lock_stub(mutex_t *);
int	__libc_mutex_trylock_stub(mutex_t *);
int	__libc_mutex_unlock_stub(mutex_t *);
int	__libc_mutex_destroy_stub(mutex_t *);

int	__libc_mutexattr_init_stub(mutexattr_t *); 
int	__libc_mutexattr_destroy_stub(mutexattr_t *);
int	__libc_mutexattr_settype_stub(mutexattr_t *, int);

int	__libc_cond_init_stub(cond_t *, const condattr_t *);
int	__libc_cond_signal_stub(cond_t *);
int	__libc_cond_broadcast_stub(cond_t *);
int	__libc_cond_wait_stub(cond_t *, mutex_t *);
int	__libc_cond_timedwait_stub(cond_t *, mutex_t *,
				   const struct timespec *);
int	__libc_cond_destroy_stub(cond_t *);

int	__libc_rwlock_init_stub(rwlock_t *, const rwlockattr_t *);
int	__libc_rwlock_rdlock_stub(rwlock_t *);
int	__libc_rwlock_wrlock_stub(rwlock_t *);
int	__libc_rwlock_tryrdlock_stub(rwlock_t *);
int	__libc_rwlock_trywrlock_stub(rwlock_t *);
int	__libc_rwlock_unlock_stub(rwlock_t *);
int	__libc_rwlock_destroy_stub(rwlock_t *);

int	__libc_thr_keycreate_stub(thread_key_t *, void (*)(void *));
int	__libc_thr_setspecific_stub(thread_key_t, const void *);
void	*__libc_thr_getspecific_stub(thread_key_t);
int	__libc_thr_keydelete_stub(thread_key_t);

int	__libc_thr_once_stub(once_t *, void (*)(void));
int	__libc_thr_sigsetmask_stub(int, const sigset_t *, sigset_t *);
thr_t	__libc_thr_self_stub(void);
int	__libc_thr_yield_stub(void);
int	__libc_thr_create_stub(thr_t *, const thrattr_t *,
	    void *(*)(void *), void *);
void	__libc_thr_exit_stub(void *) __dead;
int	*__libc_thr_errno_stub(void);
int	__libc_thr_setcancelstate_stub(int, int *);
int	__libc_thr_equal_stub(pthread_t, pthread_t);
unsigned int	__libc_thr_curcpu_stub(void);
__END_DECLS

#endif /* __LIBC_THREAD_STUBS */

#define	FLOCKFILE(fp)		__flockfile_internal(fp, 1)
#define	FUNLOCKFILE(fp)		__funlockfile_internal(fp, 1)

#else /* _REENTRANT */

#ifndef __empty
#define __empty do {} while (/*CONSTCOND*/0)
#endif
#define	mutex_init(m, a) __empty
#define	mutex_lock(m) __empty
#define	mutex_trylock(m) __empty
#define	mutex_unlock(m)	__empty
#define	mutex_destroy(m) __empty

#define	cond_init(c, t, a) __empty
#define	cond_signal(c) __empty
#define	cond_broadcast(c) __empty
#define	cond_wait(c, m) __empty
#define	cond_timedwait(c, m, t) __empty
#define	cond_destroy(c) __empty

#define	rwlock_init(l, a) __empty
#define	rwlock_rdlock(l) __empty
#define	rwlock_wrlock(l) __empty
#define	rwlock_tryrdlock(l) __empty
#define	rwlock_trywrlock(l) __empty
#define	rwlock_unlock(l) __empty
#define	rwlock_destroy(l) __empty

#define	thr_keycreate(k, d) /*LINTED*/0
#define	thr_setspecific(k, p) __empty
#define	thr_getspecific(k) /*LINTED*/0
#define	thr_keydelete(k) __empty

#define	mutexattr_init(ma) __empty
#define	mutexattr_settype(ma, t) __empty
#define	mutexattr_destroy(ma) __empty

static inline int
thr_once(once_t *once_control, void (*routine)(void))
{
	if (__predict_false(once_control->pto_done == 0)) {
		(*routine)();
		once_control->pto_done = 1;
	}
	return 0;
}
#define	thr_sigsetmask(f, n, o)	__empty
#define	thr_self() __empty
#define	thr_errno() __empty
#define	thr_curcpu()		((unsigned int)0)

#define	FLOCKFILE(fp) __empty
#define	FUNLOCKFILE(fp) __empty

#endif /* _REENTRANT */

#endif /* !defined(__minix) || !defined(_LIBC_REENTRANT_H) */

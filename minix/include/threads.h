/*	threads.h - C11 threads for MINIX, provided by libmthread.
 *
 * The C11 thread, mutex, condition variable, once and thread-specific
 * storage API, mapped onto libmthread's POSIX threads (<pthread.h>): the
 * same cooperative user-level threads - a thread runs until it blocks in,
 * or yields through, a thread call; a blocking system call stops all
 * threads of the process.  Link with -lpthread (gcc -pthread).
 *
 * Status values and mutex types have glibc's values (thrd_success == 0).
 * Zero-filled mtx_t and cnd_t objects are initialized on first use, as
 * the pthread objects they are.
 *
 * $MINIX$
 */

#ifndef _THREADS_H_
#define _THREADS_H_

#include <sys/cdefs.h>
#include <pthread.h>
#include <time.h>

#if !defined(__cplusplus) && \
    (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L)
#define thread_local	_Thread_local
#endif

#define ONCE_FLAG_INIT		PTHREAD_ONCE_INIT
#define TSS_DTOR_ITERATIONS	4

typedef pthread_cond_t	cnd_t;
typedef pthread_t	thrd_t;
typedef pthread_key_t	tss_t;
typedef pthread_mutex_t	mtx_t;
typedef pthread_once_t	once_flag;
typedef void		(*tss_dtor_t)(void *);
typedef int		(*thrd_start_t)(void *);

enum {
	mtx_plain	= 0,
	mtx_recursive	= 1,
	mtx_timed	= 2
};

enum {
	thrd_success	= 0,
	thrd_busy	= 1,
	thrd_error	= 2,
	thrd_nomem	= 3,
	thrd_timedout	= 4
};

__BEGIN_DECLS
void	call_once(once_flag *, void (*)(void));

int	cnd_broadcast(cnd_t *);
void	cnd_destroy(cnd_t *);
int	cnd_init(cnd_t *);
int	cnd_signal(cnd_t *);
int	cnd_timedwait(cnd_t *__restrict, mtx_t *__restrict,
	    const struct timespec *__restrict);
int	cnd_wait(cnd_t *, mtx_t *);

void	mtx_destroy(mtx_t *);
int	mtx_init(mtx_t *, int);
int	mtx_lock(mtx_t *);
int	mtx_timedlock(mtx_t *__restrict, const struct timespec *__restrict);
int	mtx_trylock(mtx_t *);
int	mtx_unlock(mtx_t *);

int	thrd_create(thrd_t *, thrd_start_t, void *);
thrd_t	thrd_current(void);
int	thrd_detach(thrd_t);
int	thrd_equal(thrd_t, thrd_t);
__dead void thrd_exit(int);
int	thrd_join(thrd_t, int *);
int	thrd_sleep(const struct timespec *, struct timespec *);
void	thrd_yield(void);

int	tss_create(tss_t *, tss_dtor_t);
void	tss_delete(tss_t);
void	*tss_get(tss_t);
int	tss_set(tss_t, void *);
__END_DECLS

#endif /* _THREADS_H_ */

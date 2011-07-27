#ifndef _MTHREAD_H
#define _MTHREAD_H

#include <minix/config.h>	/* MUST be first */
#include <minix/ansi.h>		/* MUST be second */
#include <minix/const.h>
#include <sys/types.h>
#include <stdio.h>
#include <ucontext.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#ifdef __NBSD_LIBC
#include <sys/signal.h>
#endif

typedef int mthread_thread_t;
typedef int mthread_once_t;
typedef int mthread_key_t;
typedef void * mthread_condattr_t;
typedef void * mthread_mutexattr_t;

struct __mthread_tcb;
typedef struct {
  struct __mthread_tcb *mq_head;
  struct __mthread_tcb *mq_tail;
} mthread_queue_t;

struct __mthread_mutex {
  mthread_queue_t mm_queue;	/* Queue of threads blocked on this mutex */
  mthread_thread_t mm_owner;	/* Thread ID that currently owns mutex */
#ifdef MTHREAD_STRICT
  struct __mthread_mutex *mm_prev;
  struct __mthread_mutex *mm_next;
#endif
  unsigned int mm_magic;
};
typedef struct __mthread_mutex *mthread_mutex_t;

struct __mthread_cond {
  struct __mthread_mutex *mc_mutex;	/* Associate mutex with condition */
#ifdef MTHREAD_STRICT
  struct __mthread_cond *mc_prev;
  struct __mthread_cond *mc_next;
#endif
  unsigned int mc_magic;
};
typedef struct __mthread_cond *mthread_cond_t;

struct __mthread_attr {
  size_t ma_stacksize;
  char *ma_stackaddr;
  int ma_detachstate;
  struct __mthread_attr *ma_prev;
  struct __mthread_attr *ma_next;
}; 
typedef struct __mthread_attr *mthread_attr_t;

#define MTHREAD_CREATE_JOINABLE 001
#define MTHREAD_CREATE_DETACHED 002
#define MTHREAD_ONCE_INIT 0
#define MTHREAD_STACK_MIN MINSIGSTKSZ
#define MTHREAD_KEYS_MAX 128

/* allocate.c */
_PROTOTYPE( int mthread_create, (mthread_thread_t *thread,
				 mthread_attr_t *tattr,
				 void *(*proc)(void *), void *arg)	);
_PROTOTYPE( int mthread_detach, (mthread_thread_t thread)		);
_PROTOTYPE( int mthread_equal, (mthread_thread_t l, mthread_thread_t r)	);
_PROTOTYPE( void mthread_exit, (void *value)				);
_PROTOTYPE( int mthread_join, (mthread_thread_t thread, void **value)	);
_PROTOTYPE( int mthread_once, (mthread_once_t *once,
			       void (*proc)(void))			);
_PROTOTYPE( mthread_thread_t mthread_self, (void)			);

/* attribute.c */
_PROTOTYPE( int mthread_attr_destroy, (mthread_attr_t *tattr)		);
_PROTOTYPE( int mthread_attr_getdetachstate, (mthread_attr_t *tattr,
					      int *detachstate)		);
_PROTOTYPE( int mthread_attr_getstack, (mthread_attr_t *tattr,
					void **stackaddr,
					size_t *stacksize)		);
_PROTOTYPE( int mthread_attr_getstacksize, (mthread_attr_t *tattr,
					    size_t *stacksize)		);
_PROTOTYPE( int mthread_attr_init, (mthread_attr_t *tattr)		);
_PROTOTYPE( int mthread_attr_setdetachstate, (mthread_attr_t *tattr,
					      int detachstate)		);
_PROTOTYPE( int mthread_attr_setstack, (mthread_attr_t *tattr,
					void *stackaddr,
					size_t stacksize)		);
_PROTOTYPE( int mthread_attr_setstacksize, (mthread_attr_t *tattr,
					    size_t stacksize)		);


/* condition.c */
_PROTOTYPE( int mthread_cond_broadcast, (mthread_cond_t *cond)		);
_PROTOTYPE( int mthread_cond_destroy, (mthread_cond_t *cond)		);
_PROTOTYPE( int mthread_cond_init, (mthread_cond_t *cond,
				    mthread_condattr_t *cattr)		);
_PROTOTYPE( int mthread_cond_signal, (mthread_cond_t *cond)		);
_PROTOTYPE( int mthread_cond_wait, (mthread_cond_t *cond,
				    mthread_mutex_t *mutex)		);

/* key.c */
_PROTOTYPE( int mthread_key_create, (mthread_key_t *key,
				      void (*destructor)(void *))	);
_PROTOTYPE( int mthread_key_delete, (mthread_key_t key)			);
_PROTOTYPE( void *mthread_getspecific, (mthread_key_t key)		);
_PROTOTYPE( int mthread_setspecific, (mthread_key_t key, void *value)	);

/* misc.c */
_PROTOTYPE( void mthread_stats, (void)					);
_PROTOTYPE( void mthread_verify_f, (char *f, int l)					);
#define mthread_verify() mthread_verify_f(__FILE__, __LINE__)

/* mutex.c */
_PROTOTYPE( int mthread_mutex_destroy, (mthread_mutex_t *mutex)	);
_PROTOTYPE( int mthread_mutex_init, (mthread_mutex_t *mutex,
				     mthread_mutexattr_t *mattr)	);
_PROTOTYPE( int mthread_mutex_lock, (mthread_mutex_t *mutex)		);
_PROTOTYPE( int mthread_mutex_trylock, (mthread_mutex_t *mutex)	);
_PROTOTYPE( int mthread_mutex_unlock, (mthread_mutex_t *mutex)	);

/* schedule.c */
_PROTOTYPE( void mthread_init, (void)					);
_PROTOTYPE( int mthread_yield, (void)					);
_PROTOTYPE( void mthread_yield_all, (void)				);

#endif

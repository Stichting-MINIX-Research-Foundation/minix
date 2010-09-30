#ifndef _MTHREAD_H
#define _MTHREAD_H

#include <minix/config.h>	/* MUST be first */
#include <ansi.h>		/* MUST be second */
#include <minix/const.h>
#include <sys/types.h>
#include <stdio.h>
#include <ucontext.h>
#include <errno.h>
#include <stdlib.h>
#include <alloca.h>
#include <limits.h>

typedef int mthread_thread_t;
typedef int mthread_once_t;
typedef void * mthread_condattr_t;
typedef void * mthread_mutexattr_t;

struct __mthread_tcb;
typedef struct {
  struct __mthread_tcb *head;
  struct __mthread_tcb *tail;
} mthread_queue_t;

struct __mthread_mutex {
  mthread_queue_t queue;	/* Queue of threads blocked on this mutex */
  mthread_thread_t owner;	/* Thread ID that currently owns mutex */
  struct __mthread_mutex *prev;
  struct __mthread_mutex *next;
};
typedef struct __mthread_mutex *mthread_mutex_t;

struct __mthread_cond {
  struct __mthread_mutex *mutex;	/* Associate mutex with condition */
  struct __mthread_cond *prev;
  struct __mthread_cond *next;
};
typedef struct __mthread_cond *mthread_cond_t;

struct __mthread_attr {
  size_t a_stacksize;
  char *a_stackaddr;
  int a_detachstate;
  struct __mthread_attr *prev;
  struct __mthread_attr *next;
}; 
typedef struct __mthread_attr *mthread_attr_t;

#define MTHREAD_CREATE_JOINABLE 001
#define MTHREAD_CREATE_DETACHED 002
#define MTHREAD_ONCE_INIT 0
#define MTHREAD_STACK_MIN MINSIGSTKSZ

/* allocate.c */
_PROTOTYPE( int mthread_create, (mthread_thread_t *thread,
				 mthread_attr_t *tattr,
				 void (*proc)(void *), void *arg)	);
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

/* misc.c */
_PROTOTYPE( void mthread_stats, (void)					);
_PROTOTYPE( void mthread_verify_f, (char *f, int l)					);
#define mthread_verify() mthread_verify_f(__FILE__, __LINE__)

/* mutex.c */
_PROTOTYPE( int mthread_mutex_destroy, (mthread_mutex_t *mutex)	);
_PROTOTYPE( int mthread_mutex_init, (mthread_mutex_t *mutex,
				     mthread_mutexattr_t *mattr)	);
#if 0
_PROTOTYPE( int mthread_mutex_lock, (mthread_mutex_t *mutex)		);
#endif
_PROTOTYPE( int mthread_mutex_lock_f, (mthread_mutex_t *mutex,
					char *file, int line)		);
#define mthread_mutex_lock(x) mthread_mutex_lock_f(x, __FILE__, __LINE__)
_PROTOTYPE( int mthread_mutex_trylock, (mthread_mutex_t *mutex)	);
_PROTOTYPE( int mthread_mutex_unlock, (mthread_mutex_t *mutex)	);

/* schedule.c */
_PROTOTYPE( void mthread_init, (void)					);
_PROTOTYPE( int mthread_yield, (void)					);
_PROTOTYPE( void mthread_yield_all, (void)				);

#endif

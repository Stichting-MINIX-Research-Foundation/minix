#ifndef _MTHREAD_H
#define _MTHREAD_H
#define _SYSTEM

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

typedef struct {
  mthread_thread_t head;
  mthread_thread_t tail;
} mthread_queue_t;

struct __mthread_mutex {
  mthread_queue_t queue;	/* Threads blocked on this mutex */
  mthread_thread_t owner;	/* Thread that currently owns mutex */
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

typedef enum {
  CONDITION, DEAD, EXITING, FALLBACK_EXITING, MUTEX, RUNNABLE
} mthread_state_t;

struct __mthread_attr {
  size_t a_stacksize;
  char *a_stackaddr;
  int a_detachstate;
  struct __mthread_attr *prev;
  struct __mthread_attr *next;
}; 
typedef struct __mthread_attr *mthread_attr_t;

typedef struct {
  mthread_thread_t m_next;		/* Next thread to run */
  mthread_state_t m_state;		/* Thread state */
  struct __mthread_attr m_attr;		/* Thread attributes */
  struct __mthread_cond *m_cond;	/* Condition variable that this thread
  					 * might be blocking on */
  void *(*m_proc)(void *);		/* Procedure to run */
  void *m_arg;				/* Argument passed to procedure */
  void *m_result;			/* Result after procedure returns */
  mthread_cond_t m_exited;		/* Condition variable signaling this
  					 * thread has ended */
  mthread_mutex_t m_exitm;		/* Mutex to accompany exit condition */
  ucontext_t m_context;			/* Thread machine context */
} mthread_tcb_t;

#define NO_THREAD -1
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
_PROTOTYPE( int mthread_mutex_lock, (mthread_mutex_t *mutex)		);
_PROTOTYPE( int mthread_mutex_trylock, (mthread_mutex_t *mutex)	);
_PROTOTYPE( int mthread_mutex_unlock, (mthread_mutex_t *mutex)	);

/* schedule.c */
_PROTOTYPE( void mthread_schedule, (void)				);
_PROTOTYPE( void mthread_suspend, (mthread_state_t state)		);
_PROTOTYPE( void mthread_unsuspend, (mthread_thread_t thread)		);
_PROTOTYPE( void mthread_init, (void)					);
_PROTOTYPE( int mthread_yield, (void)					);
_PROTOTYPE( void mthread_yield_all, (void)				);

/* queue.c */
_PROTOTYPE( void mthread_queue_init, (mthread_queue_t *queue)		);
_PROTOTYPE( void mthread_queue_add, (mthread_queue_t *queue, 
				     mthread_thread_t thread)		);
_PROTOTYPE( mthread_thread_t mthread_queue_remove, (mthread_queue_t *queue));
_PROTOTYPE( int mthread_queue_isempty, (mthread_queue_t *queue)	);

#endif

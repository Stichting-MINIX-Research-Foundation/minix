#ifndef _MTHREAD_H
#define _MTHREAD_H

#include <minix/config.h>	/* MUST be first */
#include <minix/const.h>
#include <sys/types.h>
#include <stdio.h>
#include <ucontext.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/signal.h>

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

typedef struct {
  mthread_mutex_t mutex;
  mthread_cond_t cond;
} mthread_event_t;

typedef struct {
  unsigned int readers;
  mthread_thread_t writer;
  mthread_mutex_t queue;
  mthread_event_t drain;
} mthread_rwlock_t; 

#define MTHREAD_CREATE_JOINABLE 001
#define MTHREAD_CREATE_DETACHED 002
#define MTHREAD_ONCE_INIT 0
#define MTHREAD_STACK_MIN MINSIGSTKSZ
#define MTHREAD_KEYS_MAX 128

/* allocate.c */
int mthread_create(mthread_thread_t *thread, mthread_attr_t *tattr, void
	*(*proc)(void *), void *arg);
int mthread_detach(mthread_thread_t thread);
int mthread_equal(mthread_thread_t l, mthread_thread_t r);
void mthread_exit(void *value);
int mthread_join(mthread_thread_t thread, void **value);
int mthread_once(mthread_once_t *once, void (*proc)(void));
mthread_thread_t mthread_self(void);

/* attribute.c */
int mthread_attr_destroy(mthread_attr_t *tattr);
int mthread_attr_getdetachstate(mthread_attr_t *tattr, int
	*detachstate);
int mthread_attr_getstack(mthread_attr_t *tattr, void **stackaddr,
	size_t *stacksize);
int mthread_attr_getstacksize(mthread_attr_t *tattr, size_t *stacksize);
int mthread_attr_init(mthread_attr_t *tattr);
int mthread_attr_setdetachstate(mthread_attr_t *tattr, int detachstate);
int mthread_attr_setstack(mthread_attr_t *tattr, void *stackaddr, size_t
	stacksize);
int mthread_attr_setstacksize(mthread_attr_t *tattr, size_t stacksize);


/* condition.c */
int mthread_cond_broadcast(mthread_cond_t *cond);
int mthread_cond_destroy(mthread_cond_t *cond);
int mthread_cond_init(mthread_cond_t *cond, mthread_condattr_t *cattr);
int mthread_cond_signal(mthread_cond_t *cond);
int mthread_cond_wait(mthread_cond_t *cond, mthread_mutex_t *mutex);

/* key.c */
int mthread_key_create(mthread_key_t *key, void (*destructor)(void *));
int mthread_key_delete(mthread_key_t key);
void *mthread_getspecific(mthread_key_t key);
int mthread_setspecific(mthread_key_t key, void *value);

/* misc.c */
void mthread_stats(void);
void mthread_verify_f(char *f, int l);
#define mthread_verify() mthread_verify_f(__FILE__, __LINE__)
void mthread_stacktrace(mthread_thread_t t);
void mthread_stacktraces(void);

/* mutex.c */
int mthread_mutex_destroy(mthread_mutex_t *mutex);
int mthread_mutex_init(mthread_mutex_t *mutex, mthread_mutexattr_t
	*mattr);
int mthread_mutex_lock(mthread_mutex_t *mutex);
int mthread_mutex_trylock(mthread_mutex_t *mutex);
int mthread_mutex_unlock(mthread_mutex_t *mutex);

/* event.c */
int mthread_event_destroy(mthread_event_t *event);
int mthread_event_init(mthread_event_t *event);
int mthread_event_wait(mthread_event_t *event);
int mthread_event_fire(mthread_event_t *event);
int mthread_event_fire_all(mthread_event_t *event);

/* rwlock.c */
int mthread_rwlock_destroy(mthread_rwlock_t *rwlock);
int mthread_rwlock_init(mthread_rwlock_t *rwlock);
int mthread_rwlock_rdlock(mthread_rwlock_t *rwlock);
int mthread_rwlock_wrlock(mthread_rwlock_t *rwlock);
int mthread_rwlock_unlock(mthread_rwlock_t *rwlock);

/* schedule.c */
void mthread_init(void);
int mthread_yield(void);
void mthread_yield_all(void);

#endif

#ifndef __VFS_WORKERS_H__
#define __VFS_WORKERS_H__
#include <minix/mthread.h>

#define thread_t	mthread_thread_t
#define mutex_t		mthread_mutex_t
#define cond_t		mthread_cond_t
#define attr_t		mthread_attr_t

#define mutex_init	mthread_mutex_init
#define mutex_destroy	mthread_mutex_destroy
#define mutex_lock	mthread_mutex_lock
#define mutex_trylock	mthread_mutex_trylock
#define mutex_unlock	mthread_mutex_unlock

#define cond_init	mthread_cond_init
#define cond_destroy	mthread_cond_destroy
#define cond_wait	mthread_cond_wait
#define cond_signal	mthread_cond_signal

struct fproc;

struct worker_thread {
  thread_t w_tid;
  mutex_t w_event_mutex;
  cond_t w_event;
  struct fproc *w_fp;
  message w_m_in;
  message w_m_out;
  int w_err_code;
  message *w_sendrec;
  message *w_drv_sendrec;
  endpoint_t w_task;
  struct dmap *w_dmap;
  struct worker_thread *w_next;
};

#endif

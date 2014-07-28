#ifndef __MTHREAD_PROTO_H__
#define __MTHREAD_PROTO_H__

/* allocate.c */
mthread_tcb_t * mthread_find_tcb(mthread_thread_t thread);
void mthread_thread_reset(mthread_thread_t thread);

/* attribute.c */
void mthread_init_valid_attributes(void);
#ifdef MDEBUG
int mthread_attr_verify(void);
#endif

/* cond.c */
void mthread_init_valid_conditions(void);
#ifdef MDEBUG
int mthread_cond_verify(void);
#endif

/* key.c */
void mthread_init_keys(void);
void mthread_cleanup_values(void);

/* misc.c */
#ifdef MDEBUG
#define mthread_panic(m) mthread_panic_f(__FILE__, __LINE__, (m))
void mthread_panic_f(const char *file, int line, const char *msg);
#define mthread_debug(m) mthread_debug_f(__FILE__, __LINE__, (m))
void mthread_debug_f(const char *file, int line, const char *msg);
#else
__dead void mthread_panic_s(void);
# define mthread_panic(m) mthread_panic_s()
# define mthread_debug(m)
#endif

/* mutex.c */
void mthread_init_valid_mutexes(void);

#ifdef MTHREAD_STRICT
int mthread_mutex_valid(mthread_mutex_t *mutex);
#else
# define mthread_mutex_valid(x) ((*x)->mm_magic == MTHREAD_INIT_MAGIC)
#endif

#ifdef MDEBUG
int mthread_mutex_verify(void);
#endif

/* schedule.c */
int mthread_getcontext(ucontext_t *ctxt);
void mthread_init_scheduler(void);
void mthread_schedule(void);
void mthread_suspend(mthread_state_t state);
void mthread_unsuspend(mthread_thread_t thread);

/* queue.c */
#ifdef MDEBUG
void mthread_dump_queue(mthread_queue_t *queue);
#endif
void mthread_queue_init(mthread_queue_t *queue);
void mthread_queue_add(mthread_queue_t *queue, mthread_thread_t thread);
mthread_thread_t mthread_queue_remove(mthread_queue_t *queue);
int mthread_queue_isempty(mthread_queue_t *queue);

#endif

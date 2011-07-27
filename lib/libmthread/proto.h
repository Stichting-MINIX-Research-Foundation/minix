#ifndef __MTHREAD_PROTO_H__
#define __MTHREAD_PROTO_H__

/* allocate.c */
_PROTOTYPE( mthread_tcb_t * mthread_find_tcb, (mthread_thread_t thread)	);

/* attribute.c */
_PROTOTYPE( void mthread_init_valid_attributes, (void)			);
#ifdef MDEBUG
_PROTOTYPE( int mthread_attr_verify, (void)				);
#endif

/* cond.c */
_PROTOTYPE( void mthread_init_valid_conditions, (void)			);
#ifdef MDEBUG
_PROTOTYPE( int mthread_cond_verify, (void)				);
#endif

/* key.c */
_PROTOTYPE( void mthread_init_keys, (void)				);
_PROTOTYPE( void mthread_cleanup_values, (void)				);

/* misc.c */
#ifdef MDEBUG
#define mthread_panic(m) mthread_panic_f(__FILE__, __LINE__, (m))
_PROTOTYPE( void mthread_panic_f, (const char *file, int line,
				   const char *msg)			);
#define mthread_debug(m) mthread_debug_f(__FILE__, __LINE__, (m))
_PROTOTYPE( void mthread_debug_f, (const char *file, int line,
				   const char *msg)			);
#else
_PROTOTYPE( void mthread_panic_s, (void)					);
# define mthread_panic(m) mthread_panic_s()
# define mthread_debug(m)
#endif

/* mutex.c */
_PROTOTYPE( void mthread_init_valid_mutexes, (void)			);

#ifdef MTHREAD_STRICT
_PROTOTYPE( int mthread_mutex_valid, (mthread_mutex_t *mutex)		);
#else
# define mthread_mutex_valid(x) ((*x)->mm_magic == MTHREAD_INIT_MAGIC)
#endif

#ifdef MDEBUG
_PROTOTYPE( int mthread_mutex_verify, (void)				);
#endif

/* schedule.c */
_PROTOTYPE( int mthread_getcontext, (ucontext_t *ctxt)			);
_PROTOTYPE( void mthread_init_scheduler, (void)				);
_PROTOTYPE( void mthread_schedule, (void)				);
_PROTOTYPE( void mthread_suspend, (mthread_state_t state)		);
_PROTOTYPE( void mthread_unsuspend, (mthread_thread_t thread)		);

/* queue.c */
#ifdef MDEBUG
_PROTOTYPE( void mthread_dump_queue, (mthread_queue_t *queue)		);
#endif
_PROTOTYPE( void mthread_queue_init, (mthread_queue_t *queue)		);
_PROTOTYPE( void mthread_queue_add, (mthread_queue_t *queue, 
				     mthread_thread_t thread)		);
_PROTOTYPE( mthread_thread_t mthread_queue_remove, (mthread_queue_t *queue));
_PROTOTYPE( int mthread_queue_isempty, (mthread_queue_t *queue)	);

#endif

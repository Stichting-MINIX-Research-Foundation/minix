#ifndef __MTHREAD_PROTO_H__
#define __MTHREAD_PROTO_H__

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

/* misc.c */
#define mthread_panic(m) mthread_panic_f(__FILE__, __LINE__, (m))
_PROTOTYPE( void mthread_panic_f, (const char *file, int line,
				   const char *msg)			);
#define mthread_debug(m) mthread_debug_f(__FILE__, __LINE__, (m))
_PROTOTYPE( void mthread_debug_f, (const char *file, int line,
				   const char *msg)			);

/* mutex.c */
_PROTOTYPE( void mthread_init_valid_mutexes, (void)			);
_PROTOTYPE( int mthread_mutex_valid, (mthread_mutex_t *mutex)		);
#ifdef MDEBUG
_PROTOTYPE( int mthread_mutex_verify, (void)				);
#endif


/* schedule.c */
_PROTOTYPE( int mthread_getcontext, (ucontext_t *ctxt)			);
_PROTOTYPE( void mthread_init_scheduler, (void)				);

/* queue.c */
_PROTOTYPE( void mthread_dump_queue, (mthread_queue_t *queue)		);

#endif

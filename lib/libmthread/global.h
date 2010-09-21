/* EXTERN should be extern, except for the allocate file */
#ifdef ALLOCATE
#undef EXTERN
#define EXTERN
#endif

#include <assert.h>

#define NO_THREADS 3
#define MAX_THREAD_POOL 1000
#define STACKSZ 4096
#define isokthreadid(i)	(i >= 0 && i < no_threads)

EXTERN mthread_thread_t current_thread;
EXTERN int ret_code;
EXTERN mthread_queue_t free_threads;
EXTERN mthread_queue_t run_queue;		/* FIFO of runnable threads */
EXTERN mthread_tcb_t *scheduler;
EXTERN mthread_tcb_t *threads;
EXTERN mthread_tcb_t fallback;
EXTERN mthread_tcb_t mainthread;
EXTERN int no_threads;
EXTERN int used_threads;
EXTERN int running_main_thread;


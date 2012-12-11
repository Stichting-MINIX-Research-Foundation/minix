/* EXTERN should be extern, except for the allocate file */
#ifdef ALLOCATE
#undef EXTERN
#define EXTERN
#endif

#include <assert.h>
#include <sys/types.h>
#include <sys/signal.h>

#define MTHREAD_RND_SCHED	0	/* Enable/disable random scheduling */
#define NO_THREADS 4 
#define MAX_THREAD_POOL 1024
#define STACKSZ 4096
#define MAIN_THREAD (-1)
#define NO_THREAD (-2)
#define isokthreadid(i)	(i == MAIN_THREAD || (i >= 0 && i < no_threads))
#define MTHREAD_INIT_MAGIC 0xca11ab1e
#define MTHREAD_NOT_INUSE  0xdefec7

typedef enum {
  MS_CONDITION, MS_DEAD, MS_EXITING, MS_MUTEX, MS_RUNNABLE, MS_NEEDRESET
} mthread_state_t;

struct __mthread_tcb {
  mthread_thread_t m_tid;		/* My own ID */
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
  struct __mthread_tcb *m_next;		/* Next thread in linked list */
};
typedef struct __mthread_tcb mthread_tcb_t;

EXTERN mthread_thread_t current_thread;
EXTERN mthread_queue_t free_threads;
EXTERN mthread_queue_t run_queue;		/* FIFO of runnable threads */
EXTERN mthread_tcb_t **threads;
EXTERN mthread_tcb_t mainthread;
EXTERN int no_threads;
EXTERN int used_threads;
EXTERN int need_reset;
EXTERN int running_main_thread;


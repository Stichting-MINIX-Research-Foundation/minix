#define ALLOCATE
#include <errno.h>
#include <minix/mthread.h>
#include <string.h>

#include <machine/param.h>
#include <machine/vmparam.h>

#include <sys/mman.h>

#include <uvm/uvm_param.h>

#include "global.h"
#include "proto.h"

static int mthread_increase_thread_pool(void);
static void mthread_thread_init(mthread_thread_t thread, mthread_attr_t
	*tattr, void *(*proc)(void *), void *arg);

static void mthread_thread_stop(mthread_thread_t thread);
static void mthread_trampoline(void);

static int initialized = 0;
#define MTHREAD_GUARDSIZE 	(1 << PGSHIFT) 	/* 1 page */

static struct __mthread_attr default_attr = {	MTHREAD_STACK_MIN,
						NULL,
						MTHREAD_CREATE_JOINABLE,
						NULL, NULL };

/*===========================================================================*
 *				mthread_equal				     *
 *===========================================================================*/
int mthread_equal(l, r)
mthread_thread_t l;
mthread_thread_t r;
{
/* Compare two thread ids */

  return(l == r);
}


/*===========================================================================*
 *				mthread_create				     *
 *===========================================================================*/
int mthread_create(threadid, tattr, proc, arg)
mthread_thread_t *threadid;
mthread_attr_t *tattr;
void *(*proc)(void *);
void *arg;
{
/* Register procedure proc for execution in a thread. */
  mthread_thread_t thread;

  if (proc == NULL)
	return(EINVAL);

  if (!mthread_queue_isempty(&free_threads)) {
  	thread = mthread_queue_remove(&free_threads);
  	mthread_thread_init(thread, tattr, proc, arg);
 	used_threads++;
 	if(threadid != NULL) 
 		*threadid = (mthread_thread_t) thread;
#ifdef MDEBUG
 	printf("Inited thread %d\n", thread);
#endif
 	return(0);
  } else  {
  	if (mthread_increase_thread_pool() == -1) 
  		return(EAGAIN);

  	return mthread_create(threadid, tattr, proc, arg);
  }
}


/*===========================================================================*
 *				mthread_detach				     *
 *===========================================================================*/
int mthread_detach(detach)
mthread_thread_t detach;
{
/* Mark a thread as detached. Consequently, upon exit, resources allocated for
 * this thread are automatically freed.
 */
  mthread_tcb_t *tcb;

  if (!isokthreadid(detach)) 
  	return(ESRCH);

  tcb = mthread_find_tcb(detach);
  if (tcb->m_state == MS_DEAD) {
  	return(ESRCH);
  } else if (tcb->m_attr.ma_detachstate != MTHREAD_CREATE_DETACHED) {
  	if (tcb->m_state == MS_EXITING) 
  		mthread_thread_stop(detach);
  	else 
		tcb->m_attr.ma_detachstate = MTHREAD_CREATE_DETACHED;
  }

  return(0);
}


/*===========================================================================*
 *				mthread_exit				     *
 *===========================================================================*/
void mthread_exit(value)
void *value;
{
/* Make a thread stop running and store the result value. */
  mthread_tcb_t *tcb;

  tcb = mthread_find_tcb(current_thread);

  if (tcb->m_state == MS_EXITING)	/* Already stopping, nothing to do. */
  	return;

  mthread_cleanup_values();

  tcb->m_result = value;
  tcb->m_state = MS_EXITING;

  if (tcb->m_attr.ma_detachstate == MTHREAD_CREATE_DETACHED) {
	mthread_thread_stop(current_thread);
  } else {
  	/* Joinable thread; notify possibly waiting thread */
	if (mthread_cond_signal(&(tcb->m_exited)) != 0) 
		mthread_panic("Couldn't signal exit");

	/* The thread that's actually doing the join will eventually clean
	 * up this thread (i.e., call mthread_thread_stop).
	 */
  }

  mthread_schedule();
}

/*===========================================================================*
 *			mthread_find_tcb				     *
 *===========================================================================*/
mthread_tcb_t * mthread_find_tcb(thread)
mthread_thread_t thread;
{
  mthread_tcb_t *rt = NULL;

  if (!isokthreadid(thread)) mthread_panic("Invalid thread id");

  if (thread == MAIN_THREAD)
  	rt = &mainthread;
  else
  	rt = threads[thread];

  return(rt);
}


/*===========================================================================*
 *			mthread_increase_thread_pool			     *
 *===========================================================================*/
static int mthread_increase_thread_pool(void)
{
/* Increase thread pool. No fancy algorithms, just double the size. */
  mthread_tcb_t **new_tcb;
  int new_no_threads, old_no_threads, i;

  old_no_threads = no_threads;

  if (old_no_threads == 0)
  	new_no_threads = NO_THREADS;
  else
	new_no_threads = 2 * old_no_threads;


  if (new_no_threads >= MAX_THREAD_POOL) {
  	mthread_debug("Reached max number of threads");
  	return(-1);
  }

  /* Allocate space to store pointers to thread control blocks */
  if (old_no_threads == 0)	/* No data yet: allocate space */
  	new_tcb = calloc(new_no_threads, sizeof(mthread_tcb_t *));
  else				/* Preserve existing data: reallocate space */
	new_tcb = realloc(threads, new_no_threads * sizeof(mthread_tcb_t *));

  if (new_tcb == NULL) {
  	mthread_debug("Can't increase thread pool");
  	return(-1);
  }

  /* Allocate space for thread control blocks itself */
  for (i = old_no_threads; i < new_no_threads; i++) {
  	new_tcb[i] = malloc(sizeof(mthread_tcb_t));
  	if (new_tcb[i] == NULL) {
  		mthread_debug("Can't allocate space for tcb");
  		return(-1);
  	}
  	memset(new_tcb[i], '\0', sizeof(mthread_tcb_t)); /* Clear entry */
  }

  /* We can breath again, let's tell the others about the good news */
  threads = new_tcb; 
  no_threads = new_no_threads;

  /* Add newly available threads to free_threads */
  for (i = old_no_threads; i < new_no_threads; i++) {
	mthread_queue_add(&free_threads, i);
	mthread_thread_reset(i);
  }

#ifdef MDEBUG
  printf("Increased thread pool from %d to %d threads\n", old_no_threads,
  	 new_no_threads);
#endif
  return(0);
}


/*===========================================================================*
 *				mthread_init				     *
 *===========================================================================*/
static void __attribute__((__constructor__, __used__)) mthread_init(void)
{
/* Initialize thread system; allocate thread structures and start creating
 * threads.
 */

  if (initialized) return;

  no_threads = 0;
  used_threads = 0;
  need_reset = 0;
  running_main_thread = 1;	/* mthread_init can only be called from the
				 * main thread. Calling it from a thread will
				 * not enter this clause.
				 */

  if (mthread_getcontext(&(mainthread.m_context)) == -1)
	mthread_panic("Couldn't save state for main thread");
  current_thread = MAIN_THREAD;

  mthread_init_valid_mutexes();
  mthread_init_valid_conditions();
  mthread_init_valid_attributes();
  mthread_init_keys();
  mthread_init_scheduler();

  initialized = 1;
}


/*===========================================================================*
 *				mthread_join				     *
 *===========================================================================*/
int mthread_join(join, value)
mthread_thread_t join;
void **value;
{
/* Wait for a thread to stop running and copy the result. */

  mthread_tcb_t *tcb;

  if (!isokthreadid(join))
  	return(ESRCH);
  else if (join == current_thread) 
	return(EDEADLK);

  tcb = mthread_find_tcb(join);
  if (tcb->m_state == MS_DEAD) 
  	return(ESRCH);
  else if (tcb->m_attr.ma_detachstate == MTHREAD_CREATE_DETACHED) 
	return(EINVAL);

  /* When the thread hasn't exited yet, we have to wait for that to happen */
  if (tcb->m_state != MS_EXITING) {
  	mthread_cond_t *c;
  	mthread_mutex_t *m;

  	c = &(tcb->m_exited);
  	m = &(tcb->m_exitm);

  	if (mthread_mutex_init(m, NULL) != 0)
		mthread_panic("Couldn't initialize mutex to join\n");

	if (mthread_mutex_lock(m) != 0)
		mthread_panic("Couldn't lock mutex to join\n");

	if (mthread_cond_wait(c, m) != 0) 
		mthread_panic("Couldn't wait for join condition\n");
		
	if (mthread_mutex_unlock(m) != 0)
		mthread_panic("Couldn't unlock mutex to join\n");

	if (mthread_mutex_destroy(m) != 0)
		mthread_panic("Couldn't destroy mutex to join\n");
  }

  /* Thread has exited; copy results */
  if(value != NULL)
	*value = tcb->m_result;

  /* Deallocate resources */
  mthread_thread_stop(join);
  return(0);
}


/*===========================================================================*
 *				mthread_once				     *
 *===========================================================================*/
int mthread_once(once, proc)
mthread_once_t *once;
void (*proc)(void);
{
/* Run procedure proc just once */

  if (once == NULL || proc == NULL) 
  	return(EINVAL);

  if (*once != 1) proc();
  *once = 1;
  return(0);
}


/*===========================================================================*
 *				mthread_self				     *
 *===========================================================================*/
mthread_thread_t mthread_self(void)
{
/* Return the thread id of the thread calling this function. */

  return(current_thread);
}


/*===========================================================================*
 *				mthread_thread_init			     *
 *===========================================================================*/
static void mthread_thread_init(thread, tattr, proc, arg)
mthread_thread_t thread;
mthread_attr_t *tattr;
void *(*proc)(void *);
void *arg;
{
/* Initialize a thread so that it, when unsuspended, will run the given
 * procedure with the given parameter. The thread is marked as runnable.
 */

#define THIS_CTX (&(threads[thread]->m_context))
  mthread_tcb_t *tcb;
  size_t stacksize;
  char *stackaddr;

  tcb = mthread_find_tcb(thread);
  tcb->m_next = NULL;
  tcb->m_state = MS_DEAD;
  tcb->m_proc = proc;
  tcb->m_arg = arg;
  /* Threads use a copy of the provided attributes. This way, if another
   * thread modifies the attributes (such as detach state), already running
   * threads are not affected.
   */
  if (tattr != NULL)
  	tcb->m_attr = *((struct __mthread_attr *) *tattr);
  else {
  	tcb->m_attr = default_attr;
  }

  if (mthread_cond_init(&(tcb->m_exited), NULL) != 0)
  	mthread_panic("Could not initialize thread");

  tcb->m_context.uc_link = NULL;

  /* Construct this thread's context to run procedure proc. */
  if (mthread_getcontext(&(tcb->m_context)) == -1)
  	mthread_panic("Failed to initialize context state");

  stacksize = tcb->m_attr.ma_stacksize;
  stackaddr = tcb->m_attr.ma_stackaddr;

  if (stacksize == (size_t) 0) {
	/* User provided too small a stack size. Forget about that stack and
	 * allocate a new one ourselves.
	 */
	stacksize = (size_t) MTHREAD_STACK_MIN;
	tcb->m_attr.ma_stackaddr = stackaddr = NULL;
  }

  if (stackaddr == NULL) {
	/* Allocate stack space */
	size_t guarded_stacksize;
	char *guard_start, *guard_end;

	stacksize = round_page(stacksize + MTHREAD_GUARDSIZE);
	stackaddr = mmap(NULL, stacksize,
			       PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE,
			       -1, 0);
	if (stackaddr == MAP_FAILED)
  		mthread_panic("Failed to allocate stack to thread");

#if defined(__i386__) || defined(__arm__)
	guard_start = stackaddr;
	guard_end = stackaddr + MTHREAD_GUARDSIZE;
	guarded_stacksize = stackaddr + stacksize - guard_end;

	/* The stack will be used from (stackaddr+stacksize) to stackaddr. That
	 * is, growing downwards. So the "top" of the stack may not grow into
	 * stackaddr+MTHREAD_GUARDSIZE.
	 *
	 * +-------+ stackaddr + stacksize
	 * |       |
	 * |   |   |
	 * |  \|/  |
	 * |       |
	 * +-------+ stackaddr + MTHREAD_GUARDSIZE
	 * | GUARD |
	 * +-------+ stackaddr
	 */
#else
# error "Unsupported platform"
#endif
	stacksize = guarded_stacksize;
	if (munmap(guard_start, MTHREAD_GUARDSIZE) != 0)
		mthread_panic("unable to unmap stack space for guard");
	tcb->m_context.uc_stack.ss_sp = guard_end;
  } else
  	tcb->m_context.uc_stack.ss_sp = stackaddr;

  tcb->m_context.uc_stack.ss_size = stacksize;
  makecontext(&(tcb->m_context), mthread_trampoline, 0);

  mthread_unsuspend(thread); /* Make thread runnable */
}


/*===========================================================================*
 *				mthread_thread_reset			     *
 *===========================================================================*/
void mthread_thread_reset(thread)
mthread_thread_t thread;
{
/* Reset the thread to its default values. Free the allocated stack space. */

  mthread_tcb_t *rt;
  if (!isokthreadid(thread)) mthread_panic("Invalid thread id"); 

  rt = mthread_find_tcb(thread);
  rt->m_tid = thread;
  rt->m_next = NULL;
  rt->m_state = MS_DEAD;
  rt->m_proc = NULL;
  rt->m_arg = NULL;
  rt->m_result = NULL;
  rt->m_cond = NULL;
  if (rt->m_attr.ma_stackaddr == NULL) { /* We allocated stack space */
	if (rt->m_context.uc_stack.ss_sp) {
		if (munmap(rt->m_context.uc_stack.ss_sp,
				 rt->m_context.uc_stack.ss_size) != 0) {
			mthread_panic("unable to unmap memory");
		}
	}
	rt->m_context.uc_stack.ss_sp = NULL;
  }
  rt->m_context.uc_stack.ss_size = 0;
  rt->m_context.uc_link = NULL;
}


/*===========================================================================*
 *				mthread_thread_stop			     *
 *===========================================================================*/
static void mthread_thread_stop(thread)
mthread_thread_t thread;
{
/* Stop thread from running. Deallocate resources. */
  mthread_tcb_t *stop_thread;

  if (!isokthreadid(thread)) mthread_panic("Invalid thread id"); 

  stop_thread = mthread_find_tcb(thread);

  if (stop_thread->m_state == MS_DEAD) {
  	/* Already dead, nothing to do */
  	return;
  }

  if (mthread_cond_destroy(&(stop_thread->m_exited)) != 0)
  	mthread_panic("Could not destroy condition at thread deallocation\n");

  /* Can't deallocate ourselves (i.e., we're a detached thread) */
  if (thread == current_thread) {
	stop_thread->m_state = MS_NEEDRESET;
	need_reset++;
  } else {
	mthread_thread_reset(thread);
	used_threads--;
	mthread_queue_add(&free_threads, thread);
  }
}


/*===========================================================================*
 *				mthread_trampoline			     *
 *===========================================================================*/
static void mthread_trampoline(void)
{
/* Execute the /current_thread's/ procedure. Store its result. */

  mthread_tcb_t *tcb;
  void *r;

  tcb = mthread_find_tcb(current_thread);

  r = (tcb->m_proc)(tcb->m_arg);
  mthread_exit(r); 
}

/* pthread compatibility layer. */
__weak_alias(pthread_create, mthread_create)
__weak_alias(pthread_detach, mthread_detach)
__weak_alias(pthread_equal, mthread_equal)
__weak_alias(pthread_exit, mthread_exit)
__weak_alias(pthread_join, mthread_join)
__weak_alias(pthread_once, mthread_once)
__weak_alias(pthread_self, mthread_self)


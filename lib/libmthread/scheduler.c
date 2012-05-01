#include <minix/mthread.h>
#include "global.h"
#include "proto.h"

#define MAIN_CTX	&(mainthread.m_context)
#define MAIN_STATE	mainthread.m_state
#define OLD_CTX		&(threads[old_thread]->m_context)
#define CURRENT_CTX	&(threads[current_thread]->m_context)
#define CURRENT_STATE	threads[current_thread]->m_state
static int yield_all;

/*===========================================================================*
 *				mthread_getcontext			     *
 *===========================================================================*/
int mthread_getcontext(ctx)
ucontext_t *ctx;
{
/* Retrieve this process' current state.*/

  /* We're not interested in FPU state nor signals, so ignore them. 
   * Coincidentally, this significantly speeds up performance.
   */
  ctx->uc_flags |= (UCF_IGNFPU | UCF_IGNSIGM);
  return getcontext(ctx);
}


/*===========================================================================*
 *				mthread_schedule			     *
 *===========================================================================*/
void mthread_schedule(void)
{
/* Pick a new thread to run and run it. In practice, this involves taking the 
 * first thread off the (FIFO) run queue and resuming that thread. 
 */

  mthread_thread_t old_thread;
  mthread_tcb_t *new_tcb, *old_tcb;
  ucontext_t *new_ctx, *old_ctx;

  MTHREAD_CHECK_INIT();	/* Make sure libmthread is initialized */

  old_thread = current_thread;

  if (mthread_queue_isempty(&run_queue)) {
	/* No runnable threads. Let main thread run. */

	/* We keep track whether we're running the program's 'main' thread or
	 * a spawned thread. In case we're already running the main thread and
	 * there are no runnable threads, we can't jump back to its context. 
	 * Instead, we simply return.
	 */
	if (running_main_thread) return;

	/* We're running the last runnable spawned thread. Return to main
	 * thread as there is no work left.
	 */
	current_thread = MAIN_THREAD;
  } else {
	current_thread = mthread_queue_remove(&run_queue);
  }

  /* Find thread entries in tcb... */
  new_tcb = mthread_find_tcb(current_thread);
  old_tcb = mthread_find_tcb(old_thread);

  /* ...and subsequently their contexts */
  new_ctx = &(new_tcb->m_context);
  old_ctx = &(old_tcb->m_context);

  /* Are we running the 'main' thread after swap? */
  running_main_thread = (current_thread == MAIN_THREAD);

  if (swapcontext(old_ctx, new_ctx) == -1)
	mthread_panic("Could not swap context");
  
}


/*===========================================================================*
 *				mthread_init_scheduler			     *
 *===========================================================================*/
void mthread_init_scheduler(void)
{
/* Initialize the scheduler */
  mthread_queue_init(&run_queue);
  yield_all = 0;

}


/*===========================================================================*
 *				mthread_suspend				     *
 *===========================================================================*/
void mthread_suspend(state)
mthread_state_t state;
{
/* Stop the current thread from running. There can be multiple reasons for
 * this; the process tries to lock a locked mutex (i.e., has to wait for it to
 * become unlocked), the process has to wait for a condition, the thread
 * volunteered to let another thread to run (i.e., it called yield and remains
 * runnable itself), or the thread is dead.
 */

  int continue_thread = 0;
  mthread_tcb_t *tcb;
  ucontext_t *ctx;

  if (state == MS_DEAD) mthread_panic("Shouldn't suspend with MS_DEAD state");
  tcb = mthread_find_tcb(current_thread);
  tcb->m_state = state;
  ctx = &(tcb->m_context);

  /* Save current thread's context */
  if (mthread_getcontext(ctx) != 0)
	mthread_panic("Couldn't save current thread's context");
  
  /* We return execution here with setcontext/swapcontext, but also when we
   * simply return from the getcontext call. If continue_thread is non-zero, we
   * are continuing the execution of this thread after a call from setcontext 
   * or swapcontext.
   */

  if(!continue_thread) {
  	continue_thread = 1;
	mthread_schedule(); /* Let other thread run. */
  }
}


/*===========================================================================*
 *				mthread_unsuspend			     *
 *===========================================================================*/
void mthread_unsuspend(thread)
mthread_thread_t thread; /* Thread to make runnable */
{
/* Mark the state of a thread runnable and add it to the run queue */
  mthread_tcb_t *tcb;

  if (!isokthreadid(thread)) mthread_panic("Invalid thread id");
  
  tcb = mthread_find_tcb(thread);
  tcb->m_state = MS_RUNNABLE;
  mthread_queue_add(&run_queue, thread);
}


/*===========================================================================*
 *				mthread_yield				     *
 *===========================================================================*/
int mthread_yield(void)
{
/* Defer further execution of the current thread and let another thread run. */
  mthread_tcb_t *tcb;
  mthread_thread_t t;

  MTHREAD_CHECK_INIT();	/* Make sure libmthread is initialized */

  /* Detached threads cannot clean themselves up. This is a perfect moment to
   * do it */
  for (t = (mthread_thread_t) 0; need_reset > 0 && t < no_threads; t++) {
	tcb = mthread_find_tcb(t);
	if (tcb->m_state == MS_NEEDRESET) {
		mthread_thread_reset(t);
		used_threads--;
		need_reset--;
		mthread_queue_add(&free_threads, t);
	}
  }

  if (mthread_queue_isempty(&run_queue)) {	/* No point in yielding. */
  	return(-1);
  } else if (current_thread == NO_THREAD) {
  	/* Can't yield this thread */
  	return(-1);
  }

  mthread_queue_add(&run_queue, current_thread);
  mthread_suspend(MS_RUNNABLE);	/* We're still runnable, but we're just kind
				 * enough to let someone else run.
				 */
  return(0);
}


/*===========================================================================*
 *				mthread_yield_all			     *
 *===========================================================================*/
void mthread_yield_all(void)
{
/* Yield until there are no more runnable threads left. Two threads calling
 * this function will lead to a deadlock.
 */

  MTHREAD_CHECK_INIT();	/* Make sure libmthread is initialized */

  if (yield_all) mthread_panic("Deadlock: two threads trying to yield_all");
  yield_all = 1;

  /* This works as follows. Thread A is running and threads B, C, and D are
   * runnable. As A is running, it is NOT on the run_queue (see
   * mthread_schedule). It calls mthread_yield and will be added to the run
   * queue, allowing B to run. B runs and suspends eventually, possibly still
   * in a runnable state. Then C and D run. Eventually A will run again (and is
   * thus not on the list). If B, C, and D are dead, waiting for a condition,
   * or waiting for a lock, they are not on the run queue either. At that
   * point A is the only runnable thread left.
   */
  while (!mthread_queue_isempty(&run_queue)) {
	(void) mthread_yield();
  }

  /* Done yielding all threads. */
  yield_all = 0;
}


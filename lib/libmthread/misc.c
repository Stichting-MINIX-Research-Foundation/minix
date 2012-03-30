#include <minix/mthread.h>
#include <stdio.h>
#include "global.h"
#include "proto.h"

/*===========================================================================*
 *				mthread_debug_f				     *
 *===========================================================================*/
#ifdef MDEBUG
void mthread_debug_f(const char *file, int line, const char *msg)
{
  /* Print debug message */
  printf("MTH (%s:%d): %s\n", file, line, msg);
}
#endif

/*===========================================================================*
 *				mthread_panic_f				     *
 *===========================================================================*/
#ifdef MDEBUG
void mthread_panic_f(const char *file, int line, const char *msg)
{
  /* Print panic message to stdout and exit */
  volatile int *sf;

  sf = NULL;

  printf("mthread panic (%s:%d): ", file, line);
  printf("%s", msg);
  printf("\n");
  fflush(stdout);	/* Force debug print to screen */
  *((int *) sf ) = 1;	/* Cause segfault to generate trace */
  exit(1);
}
#else
void mthread_panic_s(void)
{
  /* Silent panic */
  volatile int *sf;

  sf = NULL;
  *((int *) sf ) = 1;	/* Cause segfault to generate trace */
  exit(1);
}
#endif


/*===========================================================================*
 *				mthread_verify_f			     *
 *===========================================================================*/
#ifdef MDEBUG
void mthread_verify_f(char *file, int line)
{
  /* Verify library state. It is assumed this function is never called from
   * a spawned thread, but from the 'main' thread. The library should be 
   * quiescent; no mutexes, conditions, or threads in use. All threads are to
   * be in DEAD state.
   */
  mthread_thread_t t;
  mthread_tcb_t *tcb;
  int threads_ok = 1, conditions_ok = 1, mutexes_ok = 1, attributes_ok = 1;

  for (t = (mthread_thread_t) 0; threads_ok && t < no_threads; t++) {
  	tcb = mthread_find_tcb(t);
  	if (tcb->m_state != MS_DEAD) threads_ok = 0;
  }

  conditions_ok = mthread_cond_verify();
  mutexes_ok = mthread_mutex_verify();
  attributes_ok = mthread_attr_verify();

  printf("(%s:%d) VERIFY ", file, line);
  printf("| threads: %s |", (threads_ok ? "ok": "NOT ok"));
  printf("| cond: %s |", (conditions_ok ? "ok": "NOT ok"));
  printf("| mutex: %s |", (mutexes_ok ? "ok": "NOT ok"));
  printf("| attr: %s |", (attributes_ok ? "ok": "NOT ok"));
  printf("\n");

  if(!threads_ok || !conditions_ok || !mutexes_ok)
	mthread_panic("Library state corrupt\n");
}


/*===========================================================================*
 *				mthread_stats				     *
 *===========================================================================*/
void mthread_stats(void)
{
  mthread_thread_t t;
  mthread_tcb_t *tcb;
  int st_run, st_dead, st_cond, st_mutex, st_exit;
  st_run = st_dead = st_cond = st_mutex = st_exit = 0;

  for (t = (mthread_thread_t) 0; t < no_threads; t++) {
  	tcb = mthread_find_tcb(t);
  	switch(tcb->m_state) {
  		case MS_RUNNABLE: st_run++; break;
  		case MS_DEAD: st_dead++; break;
  		case MS_MUTEX: st_mutex++; break;
  		case MS_CONDITION: st_cond++; break;
  		case MS_EXITING: st_exit++; break;
  		default: mthread_panic("Unknown state");
  	}
  }

  printf("Pool: %-5d In use: %-5d R: %-5d D: %-5d M: %-5d C: %-5d E: %-5d\n",
  	 no_threads, used_threads, st_run, st_dead, st_mutex, st_cond,
	 st_exit);
}

#endif


/*===========================================================================*
 *				mthread_stacktrace			     *
 *===========================================================================*/
void mthread_stacktrace(mthread_thread_t t)
{
  unsigned long bp, hbp, pc;
  mthread_tcb_t *tcb;
  ucontext_t *ctx;
  mcontext_t *mtx;
  struct stackframe_s *frame;

  printf("thread %d: ", t);

  tcb = mthread_find_tcb(t);
  ctx = &tcb->m_context;
  mtx = &ctx->uc_mcontext;
  frame = &mtx->mc_p_reg;
  bp = frame->fp;

  while (bp) {
	pc = ((unsigned long *) bp)[1];
	hbp = ((unsigned long *) bp)[0];

	printf("0x%lx ", (unsigned long) pc);

	if (hbp != 0 && hbp <= bp) {
		pc = -1;
		printf("0x%lx ", (unsigned long) pc);
		break;
	}
	bp = hbp;
  }

  printf("\n");
}

/*===========================================================================*
 *				mthread_stacktraces			     *
 *===========================================================================*/
void mthread_stacktraces(void)
{
  mthread_thread_t t;

  mthread_stacktrace(MAIN_THREAD);

  for (t = (mthread_thread_t) 0; t < no_threads; t++)
	mthread_stacktrace(t);
}

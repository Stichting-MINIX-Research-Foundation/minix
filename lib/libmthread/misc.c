#include <minix/mthread.h>
#include <stdio.h>
#include "proto.h"
#include "global.h"

/*===========================================================================*
 *				mthread_debug_f				     *
 *===========================================================================*/
PUBLIC void mthread_debug_f(const char *file, int line, const char *msg)
{
  /* Print debug message */
#ifdef MDEBUG
  printf("MTH (%s:%d): %s\n", file, line, msg);
#endif
}


/*===========================================================================*
 *				mthread_panic_f				     *
 *===========================================================================*/
PUBLIC void mthread_panic_f(const char *file, int line, const char *msg)
{
  /* Print panic message to stdout and exit */
  printf("mthreads panic (%s:%d): ", file, line);
  printf(msg);
  printf("\n");
  exit(1);
}


/*===========================================================================*
 *				mthread_verify_f			     *
 *===========================================================================*/
#ifdef MDEBUG
PUBLIC void mthread_verify_f(char *file, int line)
{
  /* Verify library state. It is assumed this function is never called from
   * a spawned thread, but from the 'main' thread. The library should be 
   * quiescent; no mutexes, conditions, or threads in use. All threads are to
   * be in DEAD state.
   */
  int i;
  int threads_ok = 1, conditions_ok = 1, mutexes_ok = 1, attributes_ok = 1;

  for (i = 0; threads_ok && i < no_threads; i++)
  	if (threads[i].m_state != DEAD) threads_ok = 0;

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
#else
PUBLIC void mthread_verify_f(char *f, int l) { ; }
#endif


/*===========================================================================*
 *				mthread_stats				     *
 *===========================================================================*/
PUBLIC void mthread_stats(void)
{
  int i, st_run, st_dead, st_cond, st_mutex, st_exit, st_fbexit;;
  st_run = st_dead = st_cond = st_mutex = st_exit = st_fbexit = 0;

  for (i = 0; i < no_threads; i++) {
  	switch(threads[i].m_state) {
  		case RUNNABLE: st_run++; break;
  		case DEAD: st_dead++; break;
  		case MUTEX: st_mutex++; break;
  		case CONDITION: st_cond++; break;
  		case EXITING: st_exit++; break;
  		case FALLBACK_EXITING: st_fbexit++; break;
  		default: mthread_panic("Unknown state");
  	}
  }

  printf("Pool: %-5d In use: %-5d R: %-5d D: %-5d M: %-5d C: %-5d E: %-5d"
  	 "F: %-5d\n",
  	 no_threads, used_threads, st_run, st_dead, st_mutex, st_cond,
  	 st_exit, st_fbexit);
}

#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <sys/time.h>
#include <time.h>

#ifdef __weak_alias
__weak_alias(adjtime, __adjtime50);
#endif

int adjtime(const struct timeval *delta, struct timeval *olddelta)
{
  message m;

  m.m2_i2 = 0; /* use adjtime() method to slowly adjust the clock. */
  m.m2_i1 = (clockid_t) CLOCK_REALTIME;
  m.m2_l1 = (time_t) delta->tv_sec;
  m.m2_l2 = (long) delta->tv_usec * 1000; /* convert usec to nsec */

  if (_syscall(PM_PROC_NR, CLOCK_SETTIME, &m) < 0)
  	return -1;

  if (olddelta != NULL) {
	/* the kernel returns immediately and the adjustment happens in the 
	 * background. Also, any currently running adjustment is stopped by 
	 * another call to adjtime(2), so the only values possible on Minix
	 * for olddelta are those of delta.
	 */
	olddelta->tv_sec = delta->tv_sec;
	olddelta->tv_usec = delta->tv_usec;
  }

  return 0;
}


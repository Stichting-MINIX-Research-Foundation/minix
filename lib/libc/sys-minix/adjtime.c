#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>
#include <sys/time.h>
#include <time.h>

#ifdef __weak_alias
__weak_alias(adjtime, __adjtime50);
#endif

int adjtime(const struct timeval *delta, struct timeval *olddelta)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_pm_time.clk_id = CLOCK_REALTIME;
  m.m_lc_pm_time.now = 0; /* use adjtime() method to slowly adjust the clock. */
  m.m_lc_pm_time.sec = delta->tv_sec;
  m.m_lc_pm_time.nsec = delta->tv_usec * 1000; /* convert usec to nsec */

  if (_syscall(PM_PROC_NR, PM_CLOCK_SETTIME, &m) < 0)
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


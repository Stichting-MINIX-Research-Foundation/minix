#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <time.h>

#ifdef __weak_alias
__weak_alias(clock_settime, __clock_settime50);
#endif

int clock_settime(clockid_t clock_id, const struct timespec *ts)
{
  message m;

  m.m2_i2 = 1; /* set time immediately. don't use adjtime() method. */
  m.m2_i1 = (clockid_t) clock_id;
  m.m2_l1 = (time_t) ts->tv_sec;
  m.m2_l2 = (long) ts->tv_nsec;

  if (_syscall(PM_PROC_NR, CLOCK_SETTIME, &m) < 0)
  	return -1;

  return 0;
}


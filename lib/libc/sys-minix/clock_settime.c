#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>
#include <time.h>

#ifdef __weak_alias
__weak_alias(clock_settime, __clock_settime50);
#endif

int clock_settime(clockid_t clock_id, const struct timespec *ts)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.PM_TIME_CLK_ID = (clockid_t) clock_id;
  m.PM_TIME_NOW = 1; /* set time immediately. don't use adjtime() method. */
  m.PM_TIME_SEC = (time_t) ts->tv_sec;
  m.PM_TIME_NSEC = (long) ts->tv_nsec;

  if (_syscall(PM_PROC_NR, PM_CLOCK_SETTIME, &m) < 0)
  	return -1;

  return 0;
}


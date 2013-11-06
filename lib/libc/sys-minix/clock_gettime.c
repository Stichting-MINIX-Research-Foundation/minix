#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>
#include <sys/time.h>

#ifdef __weak_alias
__weak_alias(clock_gettime, __clock_gettime50);
#endif

int clock_gettime(clockid_t clock_id, struct timespec *res)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.PM_TIME_CLK_ID = (clockid_t) clock_id;

  if (_syscall(PM_PROC_NR, PM_CLOCK_GETTIME, &m) < 0)
  	return -1;

  res->tv_sec = (time_t) m.PM_TIME_SEC;
  res->tv_nsec = (long) m.PM_TIME_USEC;

  return 0;
}


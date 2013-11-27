#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>
#include <sys/time.h>

#ifdef __weak_alias
__weak_alias(clock_getres, __clock_getres50);
#endif

int clock_getres(clockid_t clock_id, struct timespec *res)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.PM_TIME_CLK_ID = clock_id;

  if (_syscall(PM_PROC_NR, PM_CLOCK_GETRES, &m) < 0)
  	return -1;

  res->tv_sec = m.PM_TIME_SEC;
  res->tv_nsec = m.PM_TIME_NSEC;

  return 0;
}


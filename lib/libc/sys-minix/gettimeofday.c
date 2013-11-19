/*
gettimeofday.c
*/

#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <sys/time.h>

#ifdef __weak_alias
__weak_alias(gettimeofday, _gettimeofday)
#endif

int gettimeofday(struct timeval *__restrict tp, void *__restrict tzp)
{
  message m;

  memset(&m, 0, sizeof(m));

  if (_syscall(PM_PROC_NR, PM_GETTIMEOFDAY, &m) < 0)
  	return -1;

  tp->tv_sec = m.PM_TIME_SEC;
  tp->tv_usec = m.PM_TIME_USEC;

  return 0;
}


/*
gettimeofday.c
*/

#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <sys/time.h>

#ifdef __weak_alias
__weak_alias(gettimeofday, _gettimeofday)
#endif

int gettimeofday(struct timeval *__restrict tp, void *__restrict tzp)
{
  message m;

  if (_syscall(PM_PROC_NR, GETTIMEOFDAY, &m) < 0)
  	return -1;

  tp->tv_sec = m.m2_l1;
  tp->tv_usec = m.m2_l2;

  return 0;
}


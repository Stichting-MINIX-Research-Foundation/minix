/*
gettimeofday.c
*/

#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <sys/time.h>

int gettimeofday(struct timeval *__restrict tp, void *__restrict tzp)
{
  message m;

  memset(&m, 0, sizeof(m));

  if (_syscall(PM_PROC_NR, PM_GETTIMEOFDAY, &m) < 0)
  	return -1;

  tp->tv_sec = m.m_pm_lc_time.sec;
  tp->tv_usec = m.m_pm_lc_time.nsec / 1000;

  return 0;
}


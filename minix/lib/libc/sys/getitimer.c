#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <sys/time.h>

/*
 * This is the implementation for the function to
 * invoke the interval timer retrieval system call.
 */
int getitimer(int which, struct itimerval *value)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_pm_itimer.which = which;
  m.m_lc_pm_itimer.value = 0;		/* only retrieve the timer */
  m.m_lc_pm_itimer.ovalue = (vir_bytes)value;

  return _syscall(PM_PROC_NR, PM_ITIMER, &m);
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(getitimer, __getitimer50)
#endif

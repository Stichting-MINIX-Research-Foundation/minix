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
  m.PM_ITIMER_WHICH = which;
  m.PM_ITIMER_VALUE = NULL;		/* only retrieve the timer */
  m.PM_ITIMER_OVALUE = (char *) value;

  return _syscall(PM_PROC_NR, PM_ITIMER, &m);
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(getitimer, __getitimer50)
#endif

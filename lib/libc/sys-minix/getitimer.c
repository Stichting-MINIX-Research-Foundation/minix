#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <sys/time.h>

/*
 * This is the implementation for the function to
 * invoke the interval timer retrieval system call.
 */
int getitimer(int which, struct itimerval *value)
{
  message m;

  m.m1_i1 = which;
  m.m1_p1 = NULL;		/* only retrieve the timer */
  m.m1_p2 = (char *) value;

  return _syscall(PM_PROC_NR, ITIMER, &m);
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(getitimer, __getitimer50)
#endif

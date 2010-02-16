#include <lib.h>
#define getitimer _getitimer
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

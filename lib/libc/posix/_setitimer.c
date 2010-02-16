#include <lib.h>
#define setitimer _setitimer
#include <sys/time.h>

/*
 * This is the implementation of the function to
 * invoke the interval timer setting system call.
 */
int setitimer(int which, const struct itimerval *_RESTRICT value,
		struct itimerval *_RESTRICT ovalue)
{
  message m;

  /* A null pointer for 'value' would make setitimer behave like getitimer,
   * which is not according to the specification, so disallow null pointers.
   */
  if (value == NULL) return(EINVAL);

  m.m1_i1 = which;
  m.m1_p1 = (char *) value;
  m.m1_p2 = (char *) ovalue;

  return _syscall(PM_PROC_NR, ITIMER, &m);
}

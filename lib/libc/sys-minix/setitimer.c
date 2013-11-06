#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <sys/time.h>

/*
 * This is the implementation of the function to
 * invoke the interval timer setting system call.
 */
int setitimer(int which, const struct itimerval *__restrict value,
		struct itimerval *__restrict ovalue)
{
  message m;

  /* A null pointer for 'value' would make setitimer behave like getitimer,
   * which is not according to the specification, so disallow null pointers.
   */
  if (value == NULL) {
	errno = EINVAL;
	return -1;
  }

  memset(&m, 0, sizeof(m));
  m.PM_ITIMER_WHICH = which;
  m.PM_ITIMER_VALUE = (char *) __UNCONST(value);
  m.PM_ITIMER_OVALUE = (char *) ovalue;

  return _syscall(PM_PROC_NR, PM_ITIMER, &m);
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(setitimer, __setitimer50)
#endif

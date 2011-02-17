#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <time.h>

#ifdef __weak_alias
__weak_alias(stime, _stime)
#endif

int stime(long *top)
{
  message m;

  m.m2_l1 = *top;
  return(_syscall(PM_PROC_NR, STIME, &m));
}

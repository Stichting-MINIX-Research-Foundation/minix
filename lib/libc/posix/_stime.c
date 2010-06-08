#include <lib.h>
#define stime	_stime
#include <time.h>

PUBLIC int stime(top)
long *top;
{
  message m;

  m.m2_l1 = *top;
  return(_syscall(PM_PROC_NR, STIME, &m));
}

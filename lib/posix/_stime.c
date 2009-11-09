#include <lib.h>
#define stime	_stime
#include <minix/minlib.h>
#include <time.h>

PUBLIC int stime(top)
long *top;
{
  message m;

  m.m2_l1 = *top;
  return(_syscall(MM, STIME, &m));
}

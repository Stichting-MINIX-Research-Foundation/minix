#include "syslib.h"

PUBLIC int sys_sendsig(proc, smp)
int proc;
struct sigmsg *smp;
{
  message m;

  m.m1_i1 = proc;
  m.m1_p1 = (char *) smp;
  return(_taskcall(SYSTASK, SYS_SENDSIG, &m));
}

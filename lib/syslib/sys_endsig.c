#include "syslib.h"

PUBLIC int sys_endsig(proc)
int proc;
{
  message m;

  m.m1_i1 = proc;
  return(_taskcall(SYSTASK, SYS_ENDSIG, &m));
}

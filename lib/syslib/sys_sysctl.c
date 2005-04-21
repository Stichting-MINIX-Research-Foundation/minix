#include "syslib.h"

int sys_sysctl(int proc, int request, int priv, vir_bytes argp)
{
  message m;

  m.m2_i1 = proc;
  m.m2_i2 = request;
  m.m2_i3 = priv;
  m.m2_p1 = (char *) argp;

  return _taskcall(SYSTASK, SYS_SYSCTL, &m);
}

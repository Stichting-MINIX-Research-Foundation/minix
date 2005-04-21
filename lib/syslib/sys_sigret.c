#include "syslib.h"

PUBLIC int sys_sigreturn(proc, scp, flags)
int proc;
vir_bytes scp;
int flags;
{
  message m;

  m.m1_i1 = proc;
  m.m1_i2 = flags;
  m.m1_p1 = (char *) scp;
  return(_taskcall(SYSTASK, SYS_SIGRETURN, &m));
}

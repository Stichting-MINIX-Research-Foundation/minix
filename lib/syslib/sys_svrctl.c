#include "syslib.h"

int sys_svrctl(int proc, int request, int priv, vir_bytes argp)
{
  message m;

  m.CTL_PROC_NR = proc;
  m.CTL_REQUEST = request;
  m.CTL_MM_PRIV = priv;
  m.CTL_ARG_PTR = (char *) argp;

  return _taskcall(SYSTASK, SYS_PRIVCTL, &m);
}

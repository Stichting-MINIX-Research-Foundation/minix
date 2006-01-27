#include "syslib.h"

int sys_privctl(int proc, int request, int i, vir_bytes p)
{
  message m;

  m.CTL_PROC_NR = proc;
  m.CTL_REQUEST = request;
  m.CTL_MM_PRIV = i;
  m.CTL_ARG_PTR = (char *) p;

  return _taskcall(SYSTASK, _SYS_PRIVCTL, &m);
}

#include "syslib.h"

int sys_privctl(int proc, int request, int i, void *p)
{
  message m;

  m.CTL_ENDPT = proc;
  m.CTL_REQUEST = request;
  m.CTL_MM_PRIV = i;
  m.CTL_ARG_PTR = p;

  return _taskcall(SYSTASK, SYS_PRIVCTL, &m);
}

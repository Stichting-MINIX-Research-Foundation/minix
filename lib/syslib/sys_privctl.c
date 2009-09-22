#include "syslib.h"

int sys_privctl(endpoint_t proc_ep, int request, int i, void *p)
{
  message m;

  m.CTL_ENDPT = proc_ep;
  m.CTL_REQUEST = request;
  m.CTL_ARG_PTR = p;

  return _taskcall(SYSTASK, SYS_PRIVCTL, &m);
}

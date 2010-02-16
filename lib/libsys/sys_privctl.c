#include "syslib.h"

int sys_privctl(endpoint_t proc_ep, int request, void *p)
{
  message m;

  m.CTL_ENDPT = proc_ep;
  m.CTL_REQUEST = request;
  m.CTL_ARG_PTR = p;

  return _kernel_call(SYS_PRIVCTL, &m);
}

int sys_privquery_mem(endpoint_t proc_ep, phys_bytes start, phys_bytes len)
{
  message m;

  m.CTL_ENDPT = proc_ep;
  m.CTL_REQUEST = SYS_PRIV_QUERY_MEM;
  m.CTL_PHYSSTART = start;
  m.CTL_PHYSLEN = len;

  return _kernel_call(SYS_PRIVCTL, &m);
}

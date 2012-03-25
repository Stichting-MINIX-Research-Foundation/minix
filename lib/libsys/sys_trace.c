#include "syslib.h"

int sys_trace(req, proc_ep, addr, data_p)
int req;
endpoint_t proc_ep;
long addr, *data_p;
{
  message m;
  int r;

  m.CTL_ENDPT = proc_ep;
  m.CTL_REQUEST = req;
  m.CTL_ADDRESS = addr;
  if (data_p) m.CTL_DATA = *data_p;
  r = _kernel_call(SYS_TRACE, &m);
  if (data_p) *data_p = m.CTL_DATA;
  return(r);
}

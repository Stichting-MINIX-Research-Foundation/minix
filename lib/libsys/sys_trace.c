#include "syslib.h"

int sys_trace(req, proc_ep, addr, data_p)
int req;
endpoint_t proc_ep;
long addr, *data_p;
{
  message m;
  int r;

  m.m_lsys_krn_sys_trace.endpt = proc_ep;
  m.m_lsys_krn_sys_trace.request = req;
  m.m_lsys_krn_sys_trace.address = addr;
  if (data_p) m.m_lsys_krn_sys_trace.data = *data_p;
  r = _kernel_call(SYS_TRACE, &m);
  if (data_p) *data_p = m.m_krn_lsys_sys_trace.data;
  return(r);
}

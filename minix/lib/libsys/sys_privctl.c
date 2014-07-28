#include "syslib.h"

int sys_privctl(endpoint_t proc_ep, int request, void *p)
{
  message m;

  m.m_lsys_krn_sys_privctl.endpt = proc_ep;
  m.m_lsys_krn_sys_privctl.request = request;
  m.m_lsys_krn_sys_privctl.arg_ptr = (vir_bytes)p;

  return _kernel_call(SYS_PRIVCTL, &m);
}

int sys_privquery_mem(endpoint_t proc_ep, phys_bytes start, phys_bytes len)
{
  message m;

  m.m_lsys_krn_sys_privctl.endpt = proc_ep;
  m.m_lsys_krn_sys_privctl.request = SYS_PRIV_QUERY_MEM;
  m.m_lsys_krn_sys_privctl.phys_start = start;
  m.m_lsys_krn_sys_privctl.phys_len = len;

  return _kernel_call(SYS_PRIVCTL, &m);
}

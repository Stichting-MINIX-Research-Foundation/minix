#include "syslib.h"

int sys_statectl(int request)
{
  message m;

  m.m_lsys_krn_sys_statectl.request = request;

  return _kernel_call(SYS_STATECTL, &m);
}

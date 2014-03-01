#include "syslib.h"

int sys_statectl(int request, void* address, int length)
{
  message m;

  m.m_lsys_krn_sys_statectl.request = request;
  m.m_lsys_krn_sys_statectl.address = address;
  m.m_lsys_krn_sys_statectl.length = length;

  return _kernel_call(SYS_STATECTL, &m);
}

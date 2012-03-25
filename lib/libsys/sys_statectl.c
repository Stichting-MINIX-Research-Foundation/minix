#include "syslib.h"

int sys_statectl(int request)
{
  message m;

  m.CTL_REQUEST = request;

  return _kernel_call(SYS_STATECTL, &m);
}

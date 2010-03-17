#include "syslib.h"

int sys_update(endpoint_t src_ep, endpoint_t dst_ep)
{
  message m;

  m.SYS_UPD_SRC_ENDPT = src_ep;
  m.SYS_UPD_DST_ENDPT = dst_ep;

  return _kernel_call(SYS_UPDATE, &m);
}

#include "syslib.h"

/*===========================================================================*
 *                                sys_runctl			     	     *
 *===========================================================================*/
int sys_runctl(endpoint_t proc_ep, int action, int flags)
{
  message m;

  m.RC_ENDPT = proc_ep;
  m.RC_ACTION = action;
  m.RC_FLAGS = flags;

  return(_kernel_call(SYS_RUNCTL, &m));
}

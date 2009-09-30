#include "syslib.h"

/*===========================================================================*
 *                                sys_runctl			     	     *
 *===========================================================================*/
PUBLIC int sys_runctl(endpoint_t proc_ep, int action)
{
  message m;

  m.RC_ENDPT = proc_ep;
  m.RC_ACTION = action;

  return(_taskcall(SYSTASK, SYS_RUNCTL, &m));
}

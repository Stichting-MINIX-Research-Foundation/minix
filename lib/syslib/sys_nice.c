#include "syslib.h"

/*===========================================================================*
 *                                sys_nice			     	     *
 *===========================================================================*/
PUBLIC int sys_nice(endpoint_t proc_ep, int prio)
{
  message m;

  m.PR_ENDPT = proc_ep;
  m.PR_PRIORITY = prio;
  return(_taskcall(SYSTASK, SYS_NICE, &m));
}

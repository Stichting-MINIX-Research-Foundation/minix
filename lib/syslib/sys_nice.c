#include "syslib.h"

/*===========================================================================*
 *                                sys_nice			     	     *
 *===========================================================================*/
PUBLIC int sys_nice(int proc, int prio)
{
  message m;

  m.PR_ENDPT = proc;
  m.PR_PRIORITY = prio;
  return(_taskcall(SYSTASK, SYS_NICE, &m));
}

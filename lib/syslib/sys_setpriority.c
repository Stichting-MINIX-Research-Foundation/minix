#include "syslib.h"

/*===========================================================================*
 *                                sys_xit			     	     *
 *===========================================================================*/
PUBLIC int sys_setpriority(int proc, int prio)
{
  message m;

  m.m1_i1 = proc;
  m.m1_i2 = prio;
  return(_taskcall(SYSTASK, SYS_SETPRIORITY, &m));
}

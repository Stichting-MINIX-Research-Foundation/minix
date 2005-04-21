#include "syslib.h"

PUBLIC int sys_getsp(proc, newsp)
int proc;			/* process whose sp is wanted */
vir_bytes *newsp;		/* place to put sp read from kernel */
{
/* Ask the kernel what the sp is. */

  message m;
  int r;

  m.m1_i1 = proc;
  r = _taskcall(SYSTASK, SYS_GETSP, &m);
  *newsp = (vir_bytes) m.STACK_PTR;
  return(r);
}

#include "syslib.h"

/*===========================================================================*
 *                                sys_xit			     	     *
 *===========================================================================*/
PUBLIC int sys_xit(parent, proc)
int parent;			/* parent of exiting process */
int proc;			/* which process has exited */
{
/* A process has exited.  Tell the kernel. */

  message m;

  m.PR_PPROC_NR = parent;
  m.PR_PROC_NR = proc;
  return(_taskcall(SYSTASK, SYS_XIT, &m));
}

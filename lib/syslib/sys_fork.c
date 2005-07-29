#include "syslib.h"

PUBLIC int sys_fork(parent, child)
int parent;			/* process doing the fork */
int child;			/* which proc has been created by the fork */
{
/* A process has forked.  Tell the kernel. */

  message m;

  m.PR_PPROC_NR = parent;
  m.PR_PROC_NR = child;
  return(_taskcall(SYSTASK, SYS_FORK, &m));
}

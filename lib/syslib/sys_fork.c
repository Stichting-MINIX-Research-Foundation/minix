#include "syslib.h"

PUBLIC int sys_fork(parent, child, pid)
int parent;			/* process doing the fork */
int child;			/* which proc has been created by the fork */
int pid;			/* process id assigned by MM */
{
/* A process has forked.  Tell the kernel. */

  message m;

  m.PR_PPROC_NR = parent;
  m.PR_PROC_NR = child;
  m.PR_PID = pid;
  return(_taskcall(SYSTASK, SYS_FORK, &m));
}

#include "syslib.h"

PUBLIC int sys_fork(parent, child, child_endpoint)
int parent;			/* process doing the fork */
int child;			/* which proc has been created by the fork */
int *child_endpoint;
{
/* A process has forked.  Tell the kernel. */

  message m;
  int r;

  m.PR_ENDPT = parent;
  m.PR_SLOT = child;
  r = _taskcall(SYSTASK, SYS_FORK, &m);
  *child_endpoint = m.PR_ENDPT;
  return r;
}

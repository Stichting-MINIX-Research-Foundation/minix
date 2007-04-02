#include "syslib.h"

PUBLIC int sys_fork(parent, child, child_endpoint, map_ptr)
int parent;			/* process doing the fork */
int child;			/* which proc has been created by the fork */
int *child_endpoint;
struct mem_map *map_ptr;
{
/* A process has forked.  Tell the kernel. */

  message m;
  int r;

  m.PR_ENDPT = parent;
  m.PR_SLOT = child;
  m.PR_MEM_PTR = (char *) map_ptr;
  r = _taskcall(SYSTASK, SYS_FORK, &m);
  *child_endpoint = m.PR_ENDPT;
  return r;
}

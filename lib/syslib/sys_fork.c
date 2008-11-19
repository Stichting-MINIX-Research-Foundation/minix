#include "syslib.h"

PUBLIC int sys_fork(parent, child, child_endpoint, map_ptr, flags)
endpoint_t parent;		/* process doing the fork */
endpoint_t child;		/* which proc has been created by the fork */
int *child_endpoint;
struct mem_map *map_ptr;
u32_t flags;
{
/* A process has forked.  Tell the kernel. */

  message m;
  int r;

  m.PR_ENDPT = parent;
  m.PR_SLOT = child;
  m.PR_MEM_PTR = (char *) map_ptr;
  m.PR_FORK_FLAGS = flags;
  r = _taskcall(SYSTASK, SYS_FORK, &m);
  *child_endpoint = m.PR_ENDPT;
  return r;
}

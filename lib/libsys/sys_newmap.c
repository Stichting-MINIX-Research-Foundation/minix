#include "syslib.h"

PUBLIC int sys_newmap(proc_ep, ptr)
endpoint_t proc_ep;		/* process whose map is to be changed */
struct mem_map *ptr;		/* pointer to new map */
{
/* A process has been assigned a new memory map.  Tell the kernel. */

  message m;

  m.PR_ENDPT = proc_ep;
  m.PR_MEM_PTR = (char *) ptr;
  return(_kernel_call(SYS_NEWMAP, &m));
}

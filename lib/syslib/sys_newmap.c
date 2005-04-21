#include "syslib.h"

PUBLIC int sys_newmap(proc, ptr)
int proc;			/* process whose map is to be changed */
struct mem_map *ptr;		/* pointer to new map */
{
/* A process has been assigned a new memory map.  Tell the kernel. */

  message m;

  m.PR_PROC_NR = proc;
  m.PR_MEM_PTR = (char *) ptr;
  return(_taskcall(SYSTASK, SYS_NEWMAP, &m));
}

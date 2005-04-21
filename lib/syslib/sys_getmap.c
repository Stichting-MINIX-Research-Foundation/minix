#include "syslib.h"

PUBLIC int sys_getmap(proc, ptr)
int proc;			/* process whose map is to be fetched */
struct mem_map *ptr;		/* pointer to new map */
{
/* Want to know map of a process, ask the kernel. */

  message m;

  m.m1_i1 = proc;
  m.m1_p1 = (char *) ptr;
  return(_taskcall(SYSTASK, SYS_GETMAP, &m));
}

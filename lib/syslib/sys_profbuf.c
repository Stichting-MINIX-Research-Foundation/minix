#include "syslib.h"

/*===========================================================================*
 *                                sys_profbuf				     *
 *===========================================================================*/
PUBLIC int sys_profbuf(ctl_ptr, mem_ptr)
void *ctl_ptr;				/* pointer to control structure */
void *mem_ptr;				/* pointer to profiling table */
{
  message m;

  m.PROF_CTL_PTR       = ctl_ptr;
  m.PROF_MEM_PTR       = mem_ptr;

  return(_taskcall(SYSTASK, SYS_PROFBUF, &m));
}


#include "syslib.h"

/*===========================================================================*
 *                                sys_profbuf				     *
 *===========================================================================*/
int sys_profbuf(ctl_ptr, mem_ptr)
void *ctl_ptr;				/* pointer to control structure */
void *mem_ptr;				/* pointer to profiling table */
{
  message m;

  m.PROF_CTL_PTR       = ctl_ptr;
  m.PROF_MEM_PTR       = mem_ptr;

  return(_kernel_call(SYS_PROFBUF, &m));
}


#include "syslib.h"

/*===========================================================================*
 *                                sys_profbuf				     *
 *===========================================================================*/
int sys_profbuf(ctl_ptr, mem_ptr)
void *ctl_ptr;				/* pointer to control structure */
void *mem_ptr;				/* pointer to profiling table */
{
  message m;

  m.m_lsys_krn_sys_profbuf.ctl_ptr = (vir_bytes)ctl_ptr;
  m.m_lsys_krn_sys_profbuf.mem_ptr = (vir_bytes)mem_ptr;

  return(_kernel_call(SYS_PROFBUF, &m));
}


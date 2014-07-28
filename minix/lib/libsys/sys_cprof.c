#include "syslib.h"

/*===========================================================================*
 *                                sys_cprof				     *
 *===========================================================================*/
int sys_cprof(action, size, endpt, ctl_ptr, mem_ptr)
int action; 				/* get/reset profiling tables */
int size;				/* size of allocated memory */
endpoint_t endpt;			/* caller endpoint */
void *ctl_ptr;				/* location of info struct */
void *mem_ptr;				/* location of allocated memory */
{
  message m;

  m.m_lsys_krn_sys_cprof.action		= action;
  m.m_lsys_krn_sys_cprof.mem_size	= size;
  m.m_lsys_krn_sys_cprof.endpt		= endpt;
  m.m_lsys_krn_sys_cprof.ctl_ptr	= (vir_bytes)ctl_ptr;
  m.m_lsys_krn_sys_cprof.mem_ptr	= (vir_bytes)mem_ptr;

  return(_kernel_call(SYS_CPROF, &m));
}


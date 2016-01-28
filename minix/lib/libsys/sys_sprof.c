#include "syslib.h"

#if SPROFILE

/*===========================================================================*
 *                                sys_sprof				     *
 *===========================================================================*/
int sys_sprof(action, size, freq, type, endpt, ctl_ptr, mem_ptr)
int action; 				/* start/stop profiling */
int size;				/* available profiling memory */
int freq;				/* sample frequency */
int type;
endpoint_t endpt;			/* caller endpoint */
vir_bytes ctl_ptr;			/* location of info struct */
vir_bytes mem_ptr;			/* location of profiling memory */
{
  message m;

  m.m_lsys_krn_sys_sprof.action		= action;
  m.m_lsys_krn_sys_sprof.mem_size	= size;
  m.m_lsys_krn_sys_sprof.freq		= freq;
  m.m_lsys_krn_sys_sprof.intr_type	= type;
  m.m_lsys_krn_sys_sprof.endpt		= endpt;
  m.m_lsys_krn_sys_sprof.ctl_ptr	= ctl_ptr;
  m.m_lsys_krn_sys_sprof.mem_ptr	= mem_ptr;

  return(_kernel_call(SYS_SPROF, &m));
}

#endif


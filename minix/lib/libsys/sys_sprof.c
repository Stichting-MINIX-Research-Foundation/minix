#include "syslib.h"

#if SPROFILE

/*===========================================================================*
 *                                sys_sprof				     *
 *===========================================================================*/
int sys_sprof(int action, int size, int freq, int type, endpoint_t endpt, vir_bytes ctl_ptr, vir_bytes mem_ptr)
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


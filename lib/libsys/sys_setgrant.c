
#include "syslib.h"

#include <minix/safecopies.h>

int sys_setgrant(cp_grant_t *grants, int ngrants)
{
  message m;

  m.m_lsys_krn_sys_setgrant.addr = (vir_bytes)grants;
  m.m_lsys_krn_sys_setgrant.size = ngrants;

  return _kernel_call(SYS_SETGRANT, &m);
}

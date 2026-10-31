#include "syslib.h"

int sys_stime(time_t boottime)
{
  message m;
  int r;

  m.m_lsys_krn_sys_stime.boot_time = boottime;
  r = _kernel_call(SYS_STIME, &m);
  return(r);
}

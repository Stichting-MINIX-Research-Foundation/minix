#include "syslib.h"

int sys_stime(boottime)
time_t boottime;		/* New boottime */
{
  message m;
  int r;

  m.T_BOOTTIME = boottime;
  r = _kernel_call(SYS_STIME, &m);
  return(r);
}

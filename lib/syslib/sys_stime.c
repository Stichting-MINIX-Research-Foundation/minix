#include "syslib.h"

PUBLIC int sys_stime(boottime)
time_t boottime;		/* New boottime */
{
  message m;
  int r;

  m.T_BOOTTIME = boottime;
  r = _taskcall(SYSTASK, SYS_STIME, &m);
  return(r);
}

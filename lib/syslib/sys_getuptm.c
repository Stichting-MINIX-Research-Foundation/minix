#include "syslib.h"

PUBLIC int sys_getuptime(ticks)
clock_t *ticks;			/* pointer to store ticks */
{
/* Fetch the system time. */
  message m;
  int r;

  m.T_PROC_NR = NONE;
  r = _taskcall(SYSTASK, SYS_TIMES, &m);
  *ticks = m.T_BOOT_TICKS;
  return(r);
}

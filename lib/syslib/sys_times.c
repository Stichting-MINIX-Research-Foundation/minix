#include "syslib.h"

PUBLIC int sys_times(proc, ptr)
int proc;			/* proc whose times are needed */
clock_t ptr[5];			/* pointer to time buffer */
{
/* Fetch the accounting info for a proc. */
  message m;
  int r;

  m.T_ENDPT = proc;
  r = _taskcall(SYSTASK, SYS_TIMES, &m);
  ptr[0] = m.T_USER_TIME;
  ptr[1] = m.T_SYSTEM_TIME;
  ptr[2] = 0;
  ptr[3] = 0;
  ptr[4] = m.T_BOOT_TICKS;
  return(r);
}

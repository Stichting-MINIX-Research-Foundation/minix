#include "syslib.h"

PUBLIC int sys_times(proc, ptr)
int proc;			/* proc whose times are needed */
clock_t ptr[5];			/* pointer to time buffer */
{
/* Fetch the accounting info for a proc. */
  message m;
  int r;

  m.T_PROC_NR = proc;
  r = _taskcall(SYSTASK, SYS_TIMES, &m);
  ptr[0] = m.T_USER_TIME;
  ptr[1] = m.T_SYSTEM_TIME;
  ptr[2] = m.T_CHILD_UTIME;
  ptr[3] = m.T_CHILD_STIME;
  ptr[4] = m.T_BOOT_TICKS;
  return(r);
}

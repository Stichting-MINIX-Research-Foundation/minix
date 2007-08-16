#include "syslib.h"

PUBLIC int sys_times(proc, user_time, sys_time, uptime)
int proc;			/* proc whose times are needed */
clock_t *user_time;		/* time spend in the process itself */
clock_t *sys_time;		/* time spend in system on behalf of the
				 * process
				 */
clock_t *uptime;		/* time the system is running */
{
/* Fetch the accounting info for a proc. */
  message m;
  int r;

  m.T_ENDPT = proc;
  r = _taskcall(SYSTASK, SYS_TIMES, &m);
  if (user_time) *user_time = m.T_USER_TIME;
  if (sys_time) *sys_time = m.T_SYSTEM_TIME;
  if (uptime) *uptime = m.T_BOOT_TICKS;
  return(r);
}

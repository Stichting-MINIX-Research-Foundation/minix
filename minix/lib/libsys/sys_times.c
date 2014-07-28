#include "syslib.h"

int sys_times(proc_ep, user_time, sys_time, uptime, boottime)
endpoint_t proc_ep;		/* proc_ep whose times are needed */
clock_t *user_time;		/* time spend in the process itself */
clock_t *sys_time;		/* time spend in system on behalf of the
				 * process
				 */
clock_t *uptime;		/* time the system is running */
time_t *boottime;		/* boot time */
{
/* Fetch the accounting info for a proc_ep. */
  message m;
  int r;

  m.m_lsys_krn_sys_times.endpt = proc_ep;
  r = _kernel_call(SYS_TIMES, &m);
  if (user_time) *user_time = m.m_krn_lsys_sys_times.user_time;
  if (sys_time) *sys_time = m.m_krn_lsys_sys_times.system_time;
  if (uptime) *uptime = m.m_krn_lsys_sys_times.boot_ticks;
  if (boottime) *boottime = m.m_krn_lsys_sys_times.boot_time;
  return(r);
}

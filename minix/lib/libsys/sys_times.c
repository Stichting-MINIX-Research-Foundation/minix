#include "syslib.h"

int sys_times(endpoint_t proc_ep, clock_t *user_time, clock_t *sys_time,
	clock_t *uptime, time_t *boottime)
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

#include "syslib.h"

/*
 * Ask the kernel to schedule a synchronous alarm for the caller, using either
 * an absolute or a relative number of clock ticks.  The new alarm replaces any
 * previously set alarm.  If a relative expiry time of zero is given, the
 * current alarm is stopped.  Return OK or a negative error code.  On success,
 * optionally return the time left on the previous timer (TMR_NEVER if none was
 * set) and the current time.
 */
int
sys_setalarm2(clock_t exp_time, int abs_time, clock_t * time_left,
	clock_t * uptime)
{
	message m;
	int r;

	m.m_lsys_krn_sys_setalarm.exp_time = exp_time; /* expiration time */
	m.m_lsys_krn_sys_setalarm.abs_time = abs_time; /* time is absolute? */

	if ((r = _kernel_call(SYS_SETALARM, &m)) != OK)
		return r;

	if (time_left != NULL)
		*time_left = m.m_lsys_krn_sys_setalarm.time_left;
	if (uptime != NULL)
		*uptime = m.m_lsys_krn_sys_setalarm.uptime;
	return OK;
}

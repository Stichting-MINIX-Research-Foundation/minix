#include "syslib.h"
#include <time.h>

int sys_settime(int now, clockid_t clk_id, time_t sec, long nsec)
{
	message m;
	int r;

	m.m_lsys_krn_sys_settime.now = now;
	m.m_lsys_krn_sys_settime.clock_id = clk_id;
	m.m_lsys_krn_sys_settime.sec = sec;
	m.m_lsys_krn_sys_settime.nsec = nsec;

	r = _kernel_call(SYS_SETTIME, &m);
	return r;
}

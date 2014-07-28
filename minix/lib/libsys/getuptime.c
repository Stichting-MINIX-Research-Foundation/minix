#include "sysutil.h"

/*===========================================================================*
 *                               getuptime			    	     *
 *===========================================================================*/
int getuptime(ticks, realtime, boottime)
clock_t *ticks;					/* monotonic time in ticks */
clock_t *realtime;				/* wall time in ticks */
time_t *boottime;
{
    message m;
    int s;

    m.m_type = SYS_TIMES;			/* request time information */
    m.m_lsys_krn_sys_times.endpt = NONE;	/* ignore process times */
    s = _kernel_call(SYS_TIMES, &m);
    *ticks = m.m_krn_lsys_sys_times.boot_ticks;
    *realtime = m.m_krn_lsys_sys_times.real_ticks;
    *boottime = m.m_krn_lsys_sys_times.boot_time;
    return(s);
}

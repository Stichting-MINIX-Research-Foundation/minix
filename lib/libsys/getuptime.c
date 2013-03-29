#include "sysutil.h"

/*===========================================================================*
 *                               getuptime			    	     *
 *===========================================================================*/
int getuptime(ticks, realtime, boottime)
clock_t *ticks;				/* monotonic time in ticks */
clock_t *realtime;			/* wall time in ticks */
time_t *boottime;
{
    message m;
    int s;

    m.m_type = SYS_TIMES;		/* request time information */
    m.T_ENDPT = NONE;			/* ignore process times */
    s = _kernel_call(SYS_TIMES, &m);
    *ticks = m.T_BOOT_TICKS;
    *realtime = m.T_REAL_TICKS;
    *boottime = m.T_BOOTTIME;
    return(s);
}






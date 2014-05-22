#include "sysutil.h"

/*===========================================================================*
 *                               getuptime			    	     *
 *===========================================================================*/
int getticks(ticks)
clock_t *ticks;					/* monotonic time in ticks */
{
    message m;
    int s;

    m.m_type = SYS_TIMES;			/* request time information */
    m.m_lsys_krn_sys_times.endpt = NONE;	/* ignore process times */
    s = _kernel_call(SYS_TIMES, &m);
    *ticks = m.m_krn_lsys_sys_times.boot_ticks;
    return(s);
}

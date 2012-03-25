#include "sysutil.h"

/*===========================================================================*
 *                               getuptime2			    	     *
 *===========================================================================*/
int getuptime2(ticks, boottime)
clock_t *ticks;				/* uptime in ticks */
time_t *boottime;
{
    message m;
    int s;

    m.m_type = SYS_TIMES;		/* request time information */
    m.T_ENDPT = NONE;			/* ignore process times */
    s = _kernel_call(SYS_TIMES, &m);
    *ticks = m.T_BOOT_TICKS;
    *boottime = m.T_BOOTTIME;
    return(s);
}






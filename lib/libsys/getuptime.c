#include "sysutil.h"

/*===========================================================================*
 *                               getuptime			    	     *
 *===========================================================================*/
int getuptime(ticks)
clock_t *ticks;				/* uptime in ticks */
{
    message m;
    int s;

    m.m_type = SYS_TIMES;		/* request time information */
    m.T_ENDPT = NONE;			/* ignore process times */
    s = _kernel_call(SYS_TIMES, &m);
    *ticks = m.T_BOOT_TICKS;
    return(s);
}






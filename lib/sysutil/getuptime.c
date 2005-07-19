#include "sysutil.h"

/*===========================================================================*
 *                               getuptime			    	     *
 *===========================================================================*/
PUBLIC int getuptime(ticks)
clock_t *ticks;				/* uptime in ticks */
{
    message m;
    int s;

    m.m_type = SYS_TIMES;		/* request time information */
    m.T_PROC_NR = NONE;			/* ignore process times */
    s = _taskcall(SYSTASK, SYS_TIMES, &m);
    *ticks = m.T_BOOT_TICKS;
    return(s);
}






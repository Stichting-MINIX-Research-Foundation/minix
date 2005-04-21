#include "utils.h"

/*===========================================================================*
 *                               tick_delay			    	     *
 *===========================================================================*/
PUBLIC int tick_delay(ticks)
long ticks;				/* number of ticks to wait */
{
    message m;
    int s;

    if (ticks <= 0) return;		/* check for robustness */

    m.m_type = SYS_SYNCALRM;		/* request a synchronous alarm */
    m.ALRM_PROC_NR = SELF;		/* SELF means this process nr */
    m.ALRM_EXP_TIME = ticks;		/* request message after ticks */
    m.ALRM_ABS_TIME = 0;		/* ticks are relative to now */
    s = _taskcall(SYSTASK, SYS_SYNCALRM, &m);

    if (OK == s) receive(HARDWARE,&m);	/* await synchronous alarm */
    return(s);

}






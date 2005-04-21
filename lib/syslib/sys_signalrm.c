#include "syslib.h"

/*===========================================================================*
 *                              sys_signalrm		     	     	     *
 *===========================================================================*/
PUBLIC int sys_signalrm(proc_nr, ticks)
int proc_nr;		/* process to send SYN_ALARM message to */
clock_t *ticks;		/* how many ticks / return ticks left here */
{
/* Ask the clock to schedule a synchronous alarm for the caller. The process
 * number can be SELF if the caller doesn't know its process number. 
 */
    message m;
    int s;

    m.m_type= SYS_SIGNALRM;		/* the alarm type requested */
    m.ALRM_PROC_NR = proc_nr;		/* receiving process */
    m.ALRM_EXP_TIME = *ticks;		/* the expiration time */
    m.ALRM_ABS_TIME = 0;		/* ticks are relative to now */

    s = _taskcall(SYSTASK, SYS_SIGNALRM, &m);

    *ticks = m.ALRM_TIME_LEFT;		/* returned by SYSTEM task */
    return s;
}


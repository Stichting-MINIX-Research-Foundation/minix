#include "syslib.h"

/*===========================================================================*
 *                               sys_syncalrm		     	     	     *
 *===========================================================================*/
PUBLIC int sys_syncalrm(proc_nr, exp_time, abs_time)
int proc_nr;		/* process to send SYN_ALARM message to */
clock_t exp_time;	/* expiration time for the alarm */
int abs_time;		/* use absolute or relative expiration time */
{
/* Ask the SYSTEM schedule a synchronous alarm for the caller. The process
 * number can be SELF if the caller doesn't know its process number.
 */
    message m;

    m.m_type= SYS_SYNCALRM;		/* the alarm type requested */
    m.ALRM_PROC_NR = proc_nr;		/* receiving process */
    m.ALRM_EXP_TIME = exp_time;		/* the expiration time */
    m.ALRM_ABS_TIME = abs_time;		/* time is absolute? */
    return _taskcall(SYSTASK, SYS_SYNCALRM, &m);
}


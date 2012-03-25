#include "syslib.h"

/*===========================================================================*
 *                               sys_setalarm		     	     	     *
 *===========================================================================*/
int sys_setalarm(exp_time, abs_time)
clock_t exp_time;	/* expiration time for the alarm */
int abs_time;		/* use absolute or relative expiration time */
{
/* Ask the SYSTEM schedule a synchronous alarm for the caller. The process
 * number can be SELF if the caller doesn't know its process number.
 */
    message m;
    m.ALRM_EXP_TIME = exp_time;		/* the expiration time */
    m.ALRM_ABS_TIME = abs_time;		/* time is absolute? */
    return _kernel_call(SYS_SETALARM, &m);
}


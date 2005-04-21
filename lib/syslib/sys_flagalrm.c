#include "syslib.h"

/*===========================================================================*
 *                               sys_flagalrm			     	     *
 *===========================================================================*/
PUBLIC int sys_flagalrm(ticks, flag_ptr)
clock_t ticks;		/* number of ticks until the flag is set */
int *flag_ptr;		/* pointer to timeout flag to be set */
{
/* Make a call to the clock to schedule a timeout flag alarm for the caller. */
    message m;

    if (flag_ptr != NULL)		/* expect the worst */
    	*flag_ptr = 0;			/* reset timeout flag first */
    m.m_type = SYS_FLAGALRM;		/* alarm type requested */
    m.ALRM_PROC_NR = SELF;		/* m_source will be used */
    m.ALRM_EXP_TIME = ticks;		/* alarm is due after ticks */
    m.ALRM_ABS_TIME = 0;		/* ticks are relative to now */
    m.ALRM_FLAG_PTR = (char *) flag_ptr;
    return _taskcall(SYSTASK, SYS_FLAGALRM, &m);
}



/* The system call implemented in this file:
 *   m_type:	SYS_SETPRIORITY
 *
 * The parameters for this system call are:
 *    m1_i1:	which
 *    m1_i2:	who
 *    m1_i3:	prio
 */

#include "../kernel.h"
#include "../system.h"
#include <minix/type.h>
#include <sys/resource.h>

/*===========================================================================*
 *				do_setpriority					*
 *===========================================================================*/
PUBLIC int do_setpriority(message *m_ptr)
{
	int which_proc, pri, q, niceperq;
	struct proc *which_procp;

	which_proc = m_ptr->m1_i1;
	pri  = m_ptr->m1_i2;

	/* pri is currently between PRIO_MIN and PRIO_MAX. We have to
	 * scale this between MIN_USER_Q and MAX_USER_Q.
	 */

	if(pri < PRIO_MIN || pri > PRIO_MAX)
		return EINVAL;

	if(which_proc < 0 || which_proc >= NR_TASKS+NR_PROCS)
		return EINVAL;

	which_procp = proc_addr(which_proc);

	q = MAX_USER_Q + (pri - PRIO_MIN) * (MIN_USER_Q-MAX_USER_Q+1) / (PRIO_MAX-PRIO_MIN+1);

	/* The below shouldn't happen. */
	if(q < MAX_USER_Q) q = MAX_USER_Q;
	if(q > MIN_USER_Q) q = MIN_USER_Q;

	/* max_priority is the base priority. */
	which_procp->p_max_priority = q;
	lock_unready(which_procp);
	which_procp->p_priority = q;

	/* Runnable? Put it (back) on its new run queue. */
	if(!which_procp->p_rts_flags)
		lock_ready(which_procp);

	return OK;
}



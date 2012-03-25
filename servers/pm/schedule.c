#include "pm.h"
#include <assert.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/sched.h>
#include <minix/sysinfo.h>
#include <minix/type.h>
#include <machine/archtypes.h>
#include <lib.h>
#include "mproc.h"

#include <machine/archtypes.h>
#include <timers.h>
#include "kernel/proc.h"

/*===========================================================================*
 *				init_scheduling				     *
 *===========================================================================*/
void sched_init(void)
{
	struct mproc *trmp;
	endpoint_t parent_e;
	int proc_nr, s;
 
	for (proc_nr=0, trmp=mproc; proc_nr < NR_PROCS; proc_nr++, trmp++) {
		/* Don't take over system processes. When the system starts,
		 * init is blocked on RTS_NO_QUANTUM until PM assigns a 
		 * scheduler, from which other. Given that all other user
		 * processes are forked from init and system processes are 
		 * managed by RS, there should be no other process that needs 
		 * to be assigned a scheduler here */
		if (trmp->mp_flags & IN_USE && !(trmp->mp_flags & PRIV_PROC)) {
			assert(_ENDPOINT_P(trmp->mp_endpoint) == INIT_PROC_NR);
			parent_e = mproc[trmp->mp_parent].mp_endpoint;
			assert(parent_e == trmp->mp_endpoint);
			s = sched_start(SCHED_PROC_NR,	/* scheduler_e */
				trmp->mp_endpoint,	/* schedulee_e */
				parent_e,		/* parent_e */
				USER_Q, 		/* maxprio */
				USER_QUANTUM, 		/* quantum */
				-1,			/* don't change cpu */
				&trmp->mp_scheduler);	/* *newsched_e */
			if (s != OK) {
				printf("PM: SCHED denied taking over scheduling of %s: %d\n",
					trmp->mp_name, s);
			}
		}
 	}
}

/*===========================================================================*
 *				sched_start_user			     *
 *===========================================================================*/
int sched_start_user(endpoint_t ep, struct mproc *rmp)
{
	unsigned maxprio;
	endpoint_t inherit_from;
	int rv;

	/* convert nice to priority */
	if ((rv = nice_to_priority(rmp->mp_nice, &maxprio)) != OK) {
		return rv;
	}
	
	/* scheduler must know the parent, which is not the case for a child
	 * of a system process created by a regular fork; in this case the 
	 * scheduler should inherit settings from init rather than the real 
	 * parent
	 */
	if (mproc[rmp->mp_parent].mp_flags & PRIV_PROC) {
		assert(mproc[rmp->mp_parent].mp_scheduler == NONE);
		inherit_from = INIT_PROC_NR;
	} else {
		inherit_from = mproc[rmp->mp_parent].mp_endpoint;
	}
	
	/* inherit quantum */
	return sched_inherit(ep, 			/* scheduler_e */
		rmp->mp_endpoint, 			/* schedulee_e */
		inherit_from, 				/* parent_e */
		maxprio, 				/* maxprio */
		&rmp->mp_scheduler);			/* *newsched_e */
}

/*===========================================================================*
 *				sched_nice				     *
 *===========================================================================*/
int sched_nice(struct mproc *rmp, int nice)
{
	int rv;
	message m;
	unsigned maxprio;

	/* If the kernel is the scheduler, we don't allow messing with the
	 * priority. If you want to control process priority, assign the process
	 * to a user-space scheduler */
	if (rmp->mp_scheduler == KERNEL || rmp->mp_scheduler == NONE)
		return (EINVAL);

	if ((rv = nice_to_priority(nice, &maxprio)) != OK) {
		return rv;
	}

	m.SCHEDULING_ENDPOINT	= rmp->mp_endpoint;
	m.SCHEDULING_MAXPRIO	= (int) maxprio;
	if ((rv = _taskcall(rmp->mp_scheduler, SCHEDULING_SET_NICE, &m))) {
		return rv;
	}

	return (OK);
}

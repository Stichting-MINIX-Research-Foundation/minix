#include "pm.h"
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/sysinfo.h>
#include <minix/type.h>
#include <machine/archtypes.h>
#include <lib.h>
#include "mproc.h"

/*===========================================================================*
 *				init_scheduling				     *
 *===========================================================================*/
PUBLIC void sched_init(void)
{
	struct mproc *trmp;
	int proc_nr;

	for (proc_nr=0, trmp=mproc; proc_nr < NR_PROCS; proc_nr++, trmp++) {
		/* Don't take over system processes. When the system starts,
		 * this will typically only take over init, from which other
		 * user space processes will inherit. */
		if (trmp->mp_flags & IN_USE && !(trmp->mp_flags & PRIV_PROC)) {
			if (sched_start(SCHED_PROC_NR, trmp,
					(SEND_PRIORITY | SEND_TIME_SLICE))) {
				printf("PM: SCHED denied taking over scheduling of %s\n",
					trmp->mp_name);
			}
		}
	}
}

/*===========================================================================*
 *				sched_start				     *
 *===========================================================================*/
PUBLIC int sched_start(endpoint_t ep, struct mproc *rmp, int flags)
{
	int rv;
	message m;

	m.SCHEDULING_ENDPOINT	= rmp->mp_endpoint;
	m.SCHEDULING_PARENT	= mproc[rmp->mp_parent].mp_endpoint;
	m.SCHEDULING_NICE	= rmp->mp_nice;

	/* Send the request to the scheduler */
	if ((rv = _taskcall(ep, SCHEDULING_START, &m))) {
		return rv;
	}

	/* Store the process' scheduler. Note that this might not be the
	 * scheduler we sent the SCHEDULING_START message to. That scheduler
	 * might have forwarded the scheduling message on to another scheduler
	 * before returning the message.
	 */
	rmp->mp_scheduler = m.SCHEDULING_SCHEDULER;
	return (OK);
}

/*===========================================================================*
 *				sched_stop				     *
 *===========================================================================*/
PUBLIC int sched_stop(struct mproc *rmp)
{
	int rv;
	message m;

	/* If the kernel is the scheduler, it will implicitly stop scheduling
	 * once another process takes over or the process terminates */
	if (rmp->mp_scheduler == KERNEL || rmp->mp_scheduler == NONE)
		return(OK);

	m.SCHEDULING_ENDPOINT	= rmp->mp_endpoint;
	if ((rv = _taskcall(rmp->mp_scheduler, SCHEDULING_STOP, &m))) {
		return rv;
	}

	/* sched_stop is either called when the process is exiting or it is
	 * being moved between schedulers. If it is being moved between
	 * schedulers, we need to set the mp_scheduler to NONE so that PM
	 * doesn't forward messages to the process' scheduler while being moved
	 * (such as sched_nice). */
	rmp->mp_scheduler = NONE;
	return (OK);
}

/*===========================================================================*
 *				sched_nice				     *
 *===========================================================================*/
PUBLIC int sched_nice(struct mproc *rmp, int nice)
{
	int rv;
	message m;

	/* If the kernel is the scheduler, we don't allow messing with the
	 * priority. If you want to control process priority, assign the process
	 * to a user-space scheduler */
	if (rmp->mp_scheduler == KERNEL || rmp->mp_scheduler == NONE)
		return (EINVAL);

	m.SCHEDULING_ENDPOINT	= rmp->mp_endpoint;
	m.SCHEDULING_NICE	= nice;
	if ((rv = _taskcall(rmp->mp_scheduler, SCHEDULING_SET_NICE, &m))) {
		return rv;
	}

	return (OK);
}

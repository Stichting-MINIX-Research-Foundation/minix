#include "syslib.h"
#include <assert.h>
#include <machine/archtypes.h>
#include <minix/timers.h>
#include <minix/sched.h>
#include <string.h>

/*===========================================================================*
 *				sched_inherit				     *
 *===========================================================================*/
int sched_inherit(endpoint_t scheduler_e, 
	endpoint_t schedulee_e, endpoint_t parent_e, unsigned maxprio, 
	endpoint_t *newscheduler_e)
{
	int rv;
	message m;

	assert(_ENDPOINT_P(scheduler_e) >= 0);
	assert(_ENDPOINT_P(schedulee_e) >= 0);
	assert(_ENDPOINT_P(parent_e) >= 0);
	assert(maxprio < NR_SCHED_QUEUES);
	assert(newscheduler_e);

	memset(&m, 0, sizeof(m));
	m.m_lsys_sched_scheduling_start.endpoint	= schedulee_e;
	m.m_lsys_sched_scheduling_start.parent		= parent_e;
	m.m_lsys_sched_scheduling_start.maxprio		= maxprio;

	/* Send the request to the scheduler */
	if ((rv = _taskcall(scheduler_e, SCHEDULING_INHERIT, &m))) {
		return rv;
	}

	/* Store the process' scheduler. Note that this might not be the
	 * scheduler we sent the SCHEDULING_INHERIT message to. That scheduler
	 * might have forwarded the scheduling message on to another scheduler
	 * before returning the message.
	 */
	*newscheduler_e = m.m_sched_lsys_scheduling_start.scheduler;
	return (OK);
}

/*===========================================================================*
 *				sched_start				     *
 *===========================================================================*/
int sched_start(endpoint_t scheduler_e,
			endpoint_t schedulee_e, 
			endpoint_t parent_e,
			int maxprio,
			int quantum,
			int cpu,
			endpoint_t *newscheduler_e)
{
	int rv;
	message m;

	/* No scheduler given? We are done. */
	if(scheduler_e == NONE) {
		return OK;
	}

	assert(_ENDPOINT_P(schedulee_e) >= 0);
	assert(_ENDPOINT_P(parent_e) >= 0);
	assert(maxprio >= 0);
	assert(maxprio < NR_SCHED_QUEUES);
	assert(quantum > 0);
	assert(newscheduler_e);

	/* The KERNEL must schedule this process. */
	if(scheduler_e == KERNEL) {
		if ((rv = sys_schedctl(SCHEDCTL_FLAG_KERNEL, 
			schedulee_e, maxprio, quantum, cpu)) != OK) {
			return rv;
		}
		*newscheduler_e = scheduler_e;
		return OK;
	}

	/* A user-space scheduler must schedule this process. */
	memset(&m, 0, sizeof(m));
	m.m_lsys_sched_scheduling_start.endpoint	= schedulee_e;
	m.m_lsys_sched_scheduling_start.parent		= parent_e;
	m.m_lsys_sched_scheduling_start.maxprio		= maxprio;
	m.m_lsys_sched_scheduling_start.quantum		= quantum;

	/* Send the request to the scheduler */
	if ((rv = _taskcall(scheduler_e, SCHEDULING_START, &m))) {
		return rv;
	}

	/* Store the process' scheduler. Note that this might not be the
	 * scheduler we sent the SCHEDULING_START message to. That scheduler
	 * might have forwarded the scheduling message on to another scheduler
	 * before returning the message.
	 */
	*newscheduler_e = m.m_sched_lsys_scheduling_start.scheduler;
	return (OK);
}

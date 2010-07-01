#include "syslib.h"
#include <assert.h>
#include <machine/archtypes.h>
#include <timers.h>

#include "kernel/config.h"
#include "kernel/const.h"
#include "kernel/type.h"
#include "kernel/proc.h"

/*===========================================================================*
 *				sched_inherit				     *
 *===========================================================================*/
PUBLIC int sched_inherit(endpoint_t scheduler_e, 
	endpoint_t schedulee_e, endpoint_t parent_e, unsigned maxprio, 
	endpoint_t *newscheduler_e)
{
	int rv;
	message m;

	assert(_ENDPOINT_P(scheduler_e) >= 0);
	assert(_ENDPOINT_P(schedulee_e) >= 0);
	assert(_ENDPOINT_P(parent_e) >= 0);
	assert(maxprio >= 0);
	assert(maxprio < NR_SCHED_QUEUES);
	assert(newscheduler_e);
	
	m.SCHEDULING_ENDPOINT	= schedulee_e;
	m.SCHEDULING_PARENT	= parent_e;
	m.SCHEDULING_MAXPRIO	= (int) maxprio;

	/* Send the request to the scheduler */
	if ((rv = _taskcall(scheduler_e, SCHEDULING_INHERIT, &m))) {
		return rv;
	}

	/* Store the process' scheduler. Note that this might not be the
	 * scheduler we sent the SCHEDULING_INHERIT message to. That scheduler
	 * might have forwarded the scheduling message on to another scheduler
	 * before returning the message.
	 */
	*newscheduler_e = m.SCHEDULING_SCHEDULER;
	return (OK);
}

/*===========================================================================*
 *				sched_start				     *
 *===========================================================================*/
PUBLIC int sched_start(endpoint_t scheduler_e, endpoint_t schedulee_e, 
	endpoint_t parent_e, unsigned maxprio, unsigned quantum,
	endpoint_t *newscheduler_e)
{
	int rv;
	message m;

	assert(_ENDPOINT_P(scheduler_e) >= 0);
	assert(_ENDPOINT_P(schedulee_e) >= 0);
	assert(_ENDPOINT_P(parent_e) >= 0);
	assert(maxprio >= 0);
	assert(maxprio < NR_SCHED_QUEUES);
	assert(quantum > 0);
	assert(newscheduler_e);
	
	m.SCHEDULING_ENDPOINT	= schedulee_e;
	m.SCHEDULING_PARENT	= parent_e;
	m.SCHEDULING_MAXPRIO	= (int) maxprio;
	m.SCHEDULING_QUANTUM	= (int) quantum;

	/* Send the request to the scheduler */
	if ((rv = _taskcall(scheduler_e, SCHEDULING_START, &m))) {
		return rv;
	}

	/* Store the process' scheduler. Note that this might not be the
	 * scheduler we sent the SCHEDULING_START message to. That scheduler
	 * might have forwarded the scheduling message on to another scheduler
	 * before returning the message.
	 */
	*newscheduler_e = m.SCHEDULING_SCHEDULER;
	return (OK);
}

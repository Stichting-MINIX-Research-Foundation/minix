#include "syslib.h"
#include <assert.h>
#include <string.h>

/*===========================================================================*
 *				sched_stop				     *
 *===========================================================================*/
int sched_stop(endpoint_t scheduler_e, endpoint_t schedulee_e)
{
	int rv;
	message m;
	
	/* If the kernel is the scheduler, it will implicitly stop scheduling
	 * once another process takes over or the process terminates */
	if (scheduler_e == KERNEL || scheduler_e == NONE)
		return(OK);

	/* User-scheduled, perform the call */
	assert(_ENDPOINT_P(scheduler_e) >= 0);
	assert(_ENDPOINT_P(schedulee_e) >= 0);

	memset(&m, 0, sizeof(m));
	m.SCHEDULING_ENDPOINT	= schedulee_e;
	if ((rv = _taskcall(scheduler_e, SCHEDULING_STOP, &m))) {
		return rv;
	}

	return (OK);
}

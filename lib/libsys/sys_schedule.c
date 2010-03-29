#include "syslib.h"

PUBLIC int sys_schedule(endpoint_t proc_ep, char priority, char quantum)
{
	message m;

	m.SCHEDULING_ENDPOINT = proc_ep;
	m.SCHEDULING_PRIORITY = priority;
	m.SCHEDULING_QUANTUM  = quantum;
	return(_kernel_call(SYS_SCHEDULE, &m));
}

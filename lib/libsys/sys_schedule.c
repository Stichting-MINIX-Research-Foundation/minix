#include "syslib.h"

int sys_schedule(endpoint_t proc_ep,
			int priority,
			int quantum,
			int cpu)
{
	message m;

	m.SCHEDULING_ENDPOINT = proc_ep;
	m.SCHEDULING_PRIORITY = priority;
	m.SCHEDULING_QUANTUM  = quantum;
	m.SCHEDULING_CPU = cpu;
	return(_kernel_call(SYS_SCHEDULE, &m));
}

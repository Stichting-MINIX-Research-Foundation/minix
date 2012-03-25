#include "syslib.h"

int sys_schedctl(unsigned flags,
			endpoint_t proc_ep,
			int priority,
			int quantum,
			int cpu)
{
	message m;

	m.SCHEDCTL_FLAGS = (int) flags;
	m.SCHEDCTL_ENDPOINT = proc_ep;
	m.SCHEDCTL_PRIORITY = priority;
	m.SCHEDCTL_QUANTUM = quantum;
	m.SCHEDCTL_CPU = cpu;
	return(_kernel_call(SYS_SCHEDCTL, &m));
}

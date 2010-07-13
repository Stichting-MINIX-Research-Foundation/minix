#include "syslib.h"

PUBLIC int sys_schedctl(unsigned flags, endpoint_t proc_ep, unsigned priority,
	unsigned quantum)
{
	message m;

	m.SCHEDCTL_FLAGS = (int) flags;
	m.SCHEDCTL_ENDPOINT = proc_ep;
	m.SCHEDCTL_PRIORITY = priority;
	m.SCHEDCTL_QUANTUM = quantum;
	return(_kernel_call(SYS_SCHEDCTL, &m));
}

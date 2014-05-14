#include "syslib.h"

int sys_schedule(endpoint_t proc_ep,
			int priority,
			int quantum,
			int cpu)
{
	message m;

	m.m_lsys_krn_schedule.endpoint = proc_ep;
	m.m_lsys_krn_schedule.priority = priority;
	m.m_lsys_krn_schedule.quantum  = quantum;
	m.m_lsys_krn_schedule.cpu = cpu;
	return(_kernel_call(SYS_SCHEDULE, &m));
}

#include "syslib.h"

PUBLIC int sys_schedctl(endpoint_t proc_ep)
{
	message m;

	m.SCHEDULING_ENDPOINT = proc_ep;
	return(_kernel_call(SYS_SCHEDCTL, &m));
}
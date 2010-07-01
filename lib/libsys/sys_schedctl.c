#include "syslib.h"

PUBLIC int sys_schedctl(unsigned flags, endpoint_t proc_ep)
{
	message m;

	m.SCHEDCTL_FLAGS = (int) flags;
	m.SCHEDCTL_ENDPOINT = proc_ep;
	return(_kernel_call(SYS_SCHEDCTL, &m));
}

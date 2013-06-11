#include "syslib.h"
#include <sys/resource.h>

int sys_getrusage(endpoint_t proc, struct rusage *r_usage)
{
	/* Fetch the resource usage for a process. */
	message m;
	int r;

	m.RU_ENDPT = proc;
	m.RU_RUSAGE_ADDR = r_usage;
	r = _kernel_call(SYS_GETRUSAGE, &m);
	return(r);
}

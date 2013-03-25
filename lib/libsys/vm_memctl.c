#include "syslib.h"
#include <unistd.h>
#include <string.h>

int
vm_memctl(endpoint_t ep, int req)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.VM_RS_CTL_ENDPT = ep;
	m.VM_RS_CTL_REQ = req;

	return _taskcall(VM_PROC_NR, VM_RS_MEMCTL, &m);
}

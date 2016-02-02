#include "syslib.h"
#include <unistd.h>
#include <string.h>

int
vm_memctl(endpoint_t ep, int req, void** addr, size_t *len)
{
	message m;
	int r;

	memset(&m, 0, sizeof(m));
	m.VM_RS_CTL_ENDPT = ep;
	m.VM_RS_CTL_REQ = req;
	m.VM_RS_CTL_ADDR = addr ? *addr : 0;
	m.VM_RS_CTL_LEN = len ? *len : 0;

	r = _taskcall(VM_PROC_NR, VM_RS_MEMCTL, &m);
	if(r != OK) {
		return r;
	}
	if(addr) {
		*addr = m.VM_RS_CTL_ADDR;
	}
	if(len) {
		*len = m.VM_RS_CTL_LEN;
	}

	return OK;
}

#include <lib.h>
#define vm_memctl _vm_memctl
#include <unistd.h>

PUBLIC int vm_memctl(endpoint_t ep, int req)
{
	message m;
	m.VM_RS_CTL_ENDPT = ep;
	m.VM_RS_CTL_REQ = req;

	return _syscall(VM_PROC_NR, VM_RS_MEMCTL, &m);
}

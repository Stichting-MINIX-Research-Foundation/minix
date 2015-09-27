
#include "syslib.h"
#include <string.h>
#include <minix/vm.h>

int
vm_getrusage(endpoint_t endpt, void * addr, int children)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_lsys_vm_rusage.endpt = endpt;
	m.m_lsys_vm_rusage.addr = (vir_bytes)addr;
	m.m_lsys_vm_rusage.children = children;

	return _taskcall(VM_PROC_NR, VM_GETRUSAGE, &m);
}

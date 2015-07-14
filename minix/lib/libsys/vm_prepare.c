#include "syslib.h"

#include <unistd.h>
#include <string.h>

int
vm_prepare(endpoint_t src_e, endpoint_t dst_e, int flags)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_lsys_vm_update.src = src_e;
	m.m_lsys_vm_update.dst = dst_e;
	m.m_lsys_vm_update.flags = flags;

	return _taskcall(VM_PROC_NR, VM_RS_PREPARE, &m);
}

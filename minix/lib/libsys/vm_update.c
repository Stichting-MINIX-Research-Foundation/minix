#include "syslib.h"

#include <unistd.h>
#include <string.h>

int
vm_update(endpoint_t src_e, endpoint_t dst_e)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_lsys_vm_update.src = src_e;
	m.m_lsys_vm_update.dst = dst_e;

	return _taskcall(VM_PROC_NR, VM_RS_UPDATE, &m);
}

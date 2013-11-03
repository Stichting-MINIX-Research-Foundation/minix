#include "syslib.h"

#include <unistd.h>
#include <string.h>

int
vm_set_priv(endpoint_t ep, void *buf, int sys_proc)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.VM_RS_NR = ep;
	m.VM_RS_BUF = (long) buf;
	m.VM_RS_SYS = sys_proc;

	return _taskcall(VM_PROC_NR, VM_RS_SET_PRIV, &m);
}

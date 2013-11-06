#include "syslib.h"

#include <unistd.h>
#include <string.h>

int
vm_update(endpoint_t src_e, endpoint_t dst_e)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.VM_RS_SRC_ENDPT = src_e;
	m.VM_RS_DST_ENDPT = dst_e;

	return _taskcall(VM_PROC_NR, VM_RS_UPDATE, &m);
}

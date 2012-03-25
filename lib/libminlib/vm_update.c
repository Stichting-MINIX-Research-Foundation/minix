#include <lib.h>
#include <unistd.h>

int vm_update(endpoint_t src_e, endpoint_t dst_e)
{
	message m;
	m.VM_RS_SRC_ENDPT = src_e;
	m.VM_RS_DST_ENDPT = dst_e;

	return _syscall(VM_PROC_NR, VM_RS_UPDATE, &m);
}

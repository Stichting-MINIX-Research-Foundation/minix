#include "syslib.h"

#include <sys/mman.h>
#include <minix/vm.h>
#include <stdarg.h>
#include <string.h>

void *
vm_map_phys(endpoint_t who, void *phaddr, size_t len)
{
	message m;
	int r;

	memset(&m, 0, sizeof(m));
	m.VMMP_EP = who;
	m.VMMP_PHADDR = phaddr;
	m.VMMP_LEN = len;

	r = _taskcall(VM_PROC_NR, VM_MAP_PHYS, &m);

	if (r != OK) return MAP_FAILED;

	return (void *) m.VMMP_VADDR_REPLY;
}

int
vm_unmap_phys(endpoint_t who, void *vaddr, size_t len)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.VMUP_EP = who;
	m.VMUP_VADDR = vaddr;

	return _taskcall(VM_PROC_NR, VM_UNMAP_PHYS, &m);
}

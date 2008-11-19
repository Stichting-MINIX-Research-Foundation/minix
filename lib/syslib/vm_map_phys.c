#define _SYSTEM 1
#include <lib.h>
#define vm_map_phys	_vm_map_phys
#define vm_unmap_phys	_vm_unmap_phys
#include <sys/mman.h>
#include <minix/vm.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

PUBLIC void *vm_map_phys(endpoint_t who, size_t len, void *phaddr)
{
	message m;
	int r;

	m.VMMP_EP = who;
	m.VMMP_PHADDR = phaddr;
	m.VMMP_LEN = len;

	r = _syscall(VM_PROC_NR, VM_MAP_PHYS, &m);

	if(r != OK) return MAP_FAILED;

	return (void *) m.VMMP_VADDR_REPLY;
}

PUBLIC int vm_unmap_phys(endpoint_t who, void *vaddr, size_t len)
{
	message m;
	int r;

	m.VMUP_EP = who;
	m.VMUP_VADDR = vaddr;

	return _syscall(VM_PROC_NR, VM_UNMAP_PHYS, &m);
}


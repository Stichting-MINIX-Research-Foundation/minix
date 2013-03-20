#define _SYSTEM 1
#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>
#include <minix/u64.h>
#include <minix/vm.h>

/* INCLUDES HERE */

#ifdef __weak_alias
__weak_alias(vm_remap, _vm_remap)
__weak_alias(vm_unmap, _vm_unmap)
__weak_alias(vm_getphys, _vm_getphys)
__weak_alias(vm_getrefcount, _vm_getrefcount)
__weak_alias(minix_mmap, _minix_mmap)
__weak_alias(minix_munmap, _minix_munmap)
#endif


#include <sys/mman.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

void *minix_mmap_for(endpoint_t forwhom,
	void *addr, size_t len, int prot, int flags, int fd, u64_t offset)
{
	message m;
	int r;

	m.VMM_ADDR = (vir_bytes) addr;
	m.VMM_LEN = len;
	m.VMM_PROT = prot;
	m.VMM_FLAGS = flags;
	m.VMM_FD = fd;
	m.VMM_OFFSET_LO = ex64lo(offset);

	if(forwhom != SELF) {
		m.VMM_FLAGS |= MAP_THIRDPARTY;
		m.VMM_FORWHOM = forwhom;
	} else {
		m.VMM_OFFSET_HI = ex64hi(offset);
	}

	r = _syscall(VM_PROC_NR, VM_MMAP, &m);

	if(r != OK) {
		return MAP_FAILED;
	}

	return (void *) m.VMM_RETADDR;
}

void *minix_mmap(void *addr, size_t len, int prot, int flags,
	int fd, off_t offset)
{
	return minix_mmap_for(SELF, addr, len, prot, flags, fd, offset);
}

int minix_munmap(void *addr, size_t len)
{
	message m;

	m.VMUM_ADDR = addr;
	m.VMUM_LEN = len;

	return _syscall(VM_PROC_NR, VM_MUNMAP, &m);
}


void *vm_remap(endpoint_t d,
			endpoint_t s,
			void *da,
			void *sa,
			size_t size)
{
	message m;
	int r;

	m.VMRE_D = d;
	m.VMRE_S = s;
	m.VMRE_DA = (char *) da;
	m.VMRE_SA = (char *) sa;
	m.VMRE_SIZE = size;

	r = _syscall(VM_PROC_NR, VM_REMAP, &m);
	if (r != OK)
		return MAP_FAILED;
	return (void *) m.VMRE_RETA;
}

void *vm_remap_ro(endpoint_t d,
			endpoint_t s,
			void *da,
			void *sa,
			size_t size)
{
	message m;
	int r;

	m.VMRE_D = d;
	m.VMRE_S = s;
	m.VMRE_DA = (char *) da;
	m.VMRE_SA = (char *) sa;
	m.VMRE_SIZE = size;

	r = _syscall(VM_PROC_NR, VM_REMAP_RO, &m);
	if (r != OK)
		return MAP_FAILED;
	return (void *) m.VMRE_RETA;
}

int vm_unmap(endpoint_t endpt, void *addr)
{
	message m;

	m.VMUN_ENDPT = endpt;
	m.VMUN_ADDR = (long) addr;

	return _syscall(VM_PROC_NR, VM_SHM_UNMAP, &m);
}

unsigned long vm_getphys(endpoint_t endpt, void *addr)
{
	message m;
	int r;

	m.VMPHYS_ENDPT = endpt;
	m.VMPHYS_ADDR = (long) addr;

	r = _syscall(VM_PROC_NR, VM_GETPHYS, &m);
	if (r != OK)
		return 0;
	return m.VMPHYS_RETA;
}

u8_t vm_getrefcount(endpoint_t endpt, void *addr)
{
	message m;
	int r;

	m.VMREFCNT_ENDPT = endpt;
	m.VMREFCNT_ADDR = (long) addr;

	r = _syscall(VM_PROC_NR, VM_GETREF, &m);
	if (r != OK)
		return (u8_t) -1;
	return (u8_t) m.VMREFCNT_RETC;
}


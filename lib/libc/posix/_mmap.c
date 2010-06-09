#define _SYSTEM 1
#include <lib.h>
#define mmap	_mmap
#define munmap	_munmap
#define munmap_text	_munmap_text
#define vm_remap _vm_remap
#define vm_unmap _vm_unmap
#define vm_getphys _vm_getphys
#define vm_getrefcount _vm_getrefcount
#include <sys/mman.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

PUBLIC void *mmap(void *addr, size_t len, int prot, int flags,
	int fd, off_t offset)
{
	message m;
	int r;

	m.VMM_ADDR = (vir_bytes) addr;
	m.VMM_LEN = len;
	m.VMM_PROT = prot;
	m.VMM_FLAGS = flags;
	m.VMM_FD = fd;
	m.VMM_OFFSET = offset;

	r = _syscall(VM_PROC_NR, VM_MMAP, &m);

	if(r != OK) {
		return MAP_FAILED;
	}

	return (void *) m.VMM_RETADDR;
}

PUBLIC int munmap(void *addr, size_t len)
{
	message m;

	m.VMUM_ADDR = addr;
	m.VMUM_LEN = len;

	return _syscall(VM_PROC_NR, VM_MUNMAP, &m);
}


PUBLIC int munmap_text(void *addr, size_t len)
{
	message m;

	m.VMUM_ADDR = addr;
	m.VMUM_LEN = len;

	return _syscall(VM_PROC_NR, VM_MUNMAP_TEXT, &m);
}

PUBLIC void *vm_remap(endpoint_t d,
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

PUBLIC int vm_unmap(endpoint_t endpt, void *addr)
{
	message m;

	m.VMUN_ENDPT = endpt;
	m.VMUN_ADDR = (long) addr;

	return _syscall(VM_PROC_NR, VM_SHM_UNMAP, &m);
}

PUBLIC unsigned long vm_getphys(int endpt, void *addr)
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

PUBLIC u8_t vm_getrefcount(endpoint_t endpt, void *addr)
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


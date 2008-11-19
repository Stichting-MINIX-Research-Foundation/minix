#define _SYSTEM 1
#include <lib.h>
#define mmap	_mmap
#define munmap	_munmap
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

	return _syscall(VM_PROC_NR, VM_UNMAP, &m);
}

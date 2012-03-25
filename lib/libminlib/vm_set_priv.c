#include <lib.h>
#include <unistd.h>

int vm_set_priv(int nr, void *buf)
{
	message m;
	m.VM_RS_NR = nr;
	m.VM_RS_BUF = (long) buf;
	return _syscall(VM_PROC_NR, VM_RS_SET_PRIV, &m);
}


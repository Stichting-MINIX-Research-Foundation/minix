#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(brk, _brk)
#endif

extern char *_brksize;

/* Both OSF/1 and SYSVR4 man pages specify that brk(2) returns int.
 * However, BSD4.3 specifies that brk() returns char*.  POSIX omits
 * brk() on the grounds that it imposes a memory model on an architecture.
 * On the other hand, they are so crucial to correct operation of so many
 * parts of the system, that we have chosen to hide the name brk using _brk,
 * as with system calls.  In this way, if a user inadvertently defines a
 * procedure brk, MINIX may continue to work because the true call is _brk.
 */
int brk(addr)
void *addr;
{
  message m;

  if (addr != _brksize) {
	memset(&m, 0, sizeof(m));
	m.m_lc_vm_brk.addr = addr;
	if (_syscall(VM_PROC_NR, VM_BRK, &m) < 0) return(-1);
	_brksize = addr;
  }
  return(0);
}


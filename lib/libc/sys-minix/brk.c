#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <unistd.h>

#ifdef __weak_alias
__weak_alias(brk, _brk)
#endif

extern char *_brksize;

/* Both OSF/1 and SYSVR4 man pages specify that brk(2) returns int.
 * However, BSD4.3 specifies that brk() returns char*.  POSIX omits
 * brk() on the grounds that it imposes a memory model on an architecture.
 * For this reason, brk() and sbrk() are not in the lib/posix directory.
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
	m.PMBRK_ADDR = addr;
	if (_syscall(PM_PROC_NR, BRK, &m) < 0) return(-1);
	_brksize = m.m2_p1;
  }
  return(0);
}


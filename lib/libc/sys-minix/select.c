#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <sys/time.h>
#include <sys/select.h>

int select(int nfds,
	fd_set *readfds, fd_set *writefds, fd_set *errorfds,
	struct timeval *timeout)
{
  message m;

  m.SEL_NFDS = nfds;
  m.SEL_READFDS = (char *) readfds;
  m.SEL_WRITEFDS = (char *) writefds;
  m.SEL_ERRORFDS = (char *) errorfds;
  m.SEL_TIMEOUT = (char *) timeout;

  return (_syscall(VFS_PROC_NR, SELECT, &m));
}


#if defined(__minix) && defined(__weak_alias)
__weak_alias(select, __select50)
#endif

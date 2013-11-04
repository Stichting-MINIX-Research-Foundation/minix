#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>
#include <sys/time.h>
#include <sys/select.h>

int select(int nfds,
	fd_set *readfds, fd_set *writefds, fd_set *errorfds,
	struct timeval *timeout)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.VFS_SELECT_NFDS = nfds;
  m.VFS_SELECT_READFDS = (char *) readfds;
  m.VFS_SELECT_WRITEFDS = (char *) writefds;
  m.VFS_SELECT_ERRORFDS = (char *) errorfds;
  m.VFS_SELECT_TIMEOUT = (char *) timeout;

  return (_syscall(VFS_PROC_NR, VFS_SELECT, &m));
}


#if defined(__minix) && defined(__weak_alias)
__weak_alias(select, __select50)
#endif

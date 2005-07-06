
#include <lib.h>
#include <sys/time.h>
#include <sys/select.h>

PUBLIC int select(int nfds,
	fd_set *readfds, fd_set *writefds, fd_set *errorfds,
	struct timeval *timeout)
{
  message m;

  m.SEL_NFDS = nfds;
  m.SEL_READFDS = (char *) readfds;
  m.SEL_WRITEFDS = (char *) writefds;
  m.SEL_ERRORFDS = (char *) errorfds;
  m.SEL_TIMEOUT = (char *) timeout;

  return (_syscall(FS, SELECT, &m));
}


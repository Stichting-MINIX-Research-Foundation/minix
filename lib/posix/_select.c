
#include <lib.h>
#include <sys/select.h>

PUBLIC int select(int nfds,
	fd_set *readfds, fd_set *writefds, fd_set *errorfds,
	struct timeval *timeout)
{
  message m;

  m.m8_i1 = nfds;
  m.m8_p1 = (char *) readfds;
  m.m8_p2 = (char *) writefds;
  m.m8_p3 = (char *) errorfds;
  m.m8_p4 = (char *) timeout;

  return (_syscall(FS, SELECT, &m));
}

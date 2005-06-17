
#include <lib.h>
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

#define FD_BITMASK(b)	(1L << ((b) % _FDSETBITSPERWORD))
#define FD_BITWORD(b)	((b)/_FDSETBITSPERWORD)

PUBLIC void FD_CLR(int fd, fd_set *fdset)
{
	if(fd < 0 || fd >= FD_SETSIZE) return;
	fdset->_fdsetval[FD_BITWORD(fd)] &= ~(FD_BITMASK(fd));
	return;
}

PUBLIC int FD_ISSET(int fd, fd_set *fdset)
{
	if(fd < 0 || fd >= FD_SETSIZE)
		return 0;
	if(fdset->_fdsetval[FD_BITWORD(fd)] & FD_BITMASK(fd))
		return 1;
	return 0;
}

PUBLIC void FD_SET(int fd, fd_set *fdset)
{
	if(fd < 0 || fd >= FD_SETSIZE)
		return;
	fdset->_fdsetval[FD_BITWORD(fd)] |= FD_BITMASK(fd);
	return;
}

PUBLIC void FD_ZERO(fd_set *fdset)
{
	int i;
	for(i = 0; i < _FDSETWORDS; i++)
		fdset->_fdsetval[i] = 0;
	return;
}


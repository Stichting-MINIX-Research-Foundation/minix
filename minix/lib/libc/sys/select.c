#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <sys/select.h>

/* from sigaction.c: is any current signal handler SA_RESTART? */
int __libc_select_may_restart(void);

/*
 * libminc (the minimal libc statically linked into services and drivers)
 * does not pull in sigaction.o; provide a do-nothing fallback there so
 * service binaries still link.  In the full libc the strong definition in
 * sigaction.c wins.
 */
__weak int
__libc_select_may_restart(void)
{
	return 0;
}

int select(int nfds,
	fd_set *readfds, fd_set *writefds, fd_set *errorfds,
	struct timeval *timeout)
{
  message m;
  struct timeval start, now, elapsed, wait;
  int haveto = (timeout != NULL);
  int r;

  /*
   * POSIX/BSD semantics: EINTR only surfaces to the caller when the
   * interrupting handler was not installed with SA_RESTART.  Restart when
   * a restartable handler exists, with the time waiting so far deducted
   * from the timeout (so a periodic timer can't starve a finite wait).
   * VFS reuses the message buffer for the reply, so rebuild it per attempt.
   */
  if (haveto) {
	wait = *timeout;
	gettimeofday(&start, NULL);
  }

  for (;;) {
	memset(&m, 0, sizeof(m));
	m.m_lc_vfs_select.nfds = nfds;
	m.m_lc_vfs_select.readfds = readfds;
	m.m_lc_vfs_select.writefds = writefds;
	m.m_lc_vfs_select.errorfds = errorfds;
	m.m_lc_vfs_select.timeout = haveto ? (vir_bytes)&wait : (vir_bytes)NULL;

	r = _syscall(VFS_PROC_NR, VFS_SELECT, &m);
	if (!(r == -1 && errno == EINTR && __libc_select_may_restart()))
		break;
	if (!haveto)
		continue;
	gettimeofday(&now, NULL);
	timersub(&now, &start, &elapsed);
	if (timercmp(&elapsed, timeout, >=)) {
		r = 0;	/* fully expired during signal handling */
		break;
	}
	timersub(timeout, &elapsed, &wait);
  }

  return r;
}


#if defined(__minix) && defined(__weak_alias)
__weak_alias(select, __select50)
#endif

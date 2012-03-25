#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <fcntl.h>
#include <stdarg.h>

#ifdef __weak_alias
__weak_alias(fcntl, _fcntl)
#endif

int fcntl(int fd, int cmd, ...)
{
  va_list argp;
  message m;

  va_start(argp, cmd);

  /* Set up for the sensible case where there is no variable parameter.  This
   * covers F_GETFD, F_GETFL and invalid commands.
   */
  m.m1_i3 = 0;
  m.m1_p1 = NULL;

  /* Adjust for the stupid cases. */
  switch(cmd) {
     case F_DUPFD:
     case F_SETFD:
     case F_SETFL:
	m.m1_i3 = va_arg(argp, int);
	break;
     case F_GETLK:
     case F_SETLK:
     case F_SETLKW:
     case F_FREESP:
	m.m1_p1 = (char *) va_arg(argp, struct flock *);
	break;
  }

  /* Clean up and make the system call. */
  va_end(argp);
  m.m1_i1 = fd;
  m.m1_i2 = cmd;
  return(_syscall(VFS_PROC_NR, FCNTL, &m));
}

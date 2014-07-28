#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <fcntl.h>
#include <stdarg.h>

int fcntl(int fd, int cmd, ...)
{
  va_list argp;
  message m;

  va_start(argp, cmd);

  /* Set up for the sensible case where there is no variable parameter.  This
   * covers F_GETFD, F_GETFL and invalid commands.
   */
  memset(&m, 0, sizeof(m));

  /* Adjust for the stupid cases. */
  switch(cmd) {
     case F_DUPFD:
     case F_SETFD:
     case F_SETFL:
	m.m_lc_vfs_fcntl.arg_int = va_arg(argp, int);
	break;
     case F_GETLK:
     case F_SETLK:
     case F_SETLKW:
     case F_FREESP:
	m.m_lc_vfs_fcntl.arg_ptr = va_arg(argp, struct flock *);
	break;
  }

  /* Clean up and make the system call. */
  va_end(argp);
  m.m_lc_vfs_fcntl.fd = fd;
  m.m_lc_vfs_fcntl.cmd = cmd;
  return(_syscall(VFS_PROC_NR, VFS_FCNTL, &m));
}

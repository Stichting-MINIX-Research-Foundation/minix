#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>

#ifdef __weak_alias
__weak_alias(fcntl, _fcntl)
#endif

static int __fcntl_321(int fd, int cmd, va_list argp);

int __fcntl_321(int fd, int cmd, va_list argp)
{
  message m;
  struct flock_321 f_321;
  struct flock *flock;
  int r;

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
	/* VFS expects old format, so translate */
	flock = (struct flock *) va_arg(argp, struct flock *);
	f_321.l_type = flock->l_type;
	f_321.l_whence = flock->l_whence;
	f_321.l_start = flock->l_start;
	f_321.l_len = flock->l_len;
	f_321.l_pid = flock->l_pid;
	m.m1_p1 = (char *) &f_321;
	break;
  }

  /* Clean up and make the system call. */
  m.m1_i1 = fd;
  m.m1_i2 = cmd;

  r = _syscall(VFS_PROC_NR, FCNTL_321, &m);

  if (r == 0) {
	/* Maybe we need to convert back */

	switch(cmd) {
	case F_GETLK:
	case F_SETLK:
	case F_SETLKW:
	case F_FREESP:
		/* VFS expected old format but libc new format, so translate */
		flock->l_type = f_321.l_type;
		flock->l_whence = f_321.l_whence;
		flock->l_start = f_321.l_start;
		flock->l_len = f_321.l_len;
		flock->l_pid = f_321.l_pid;
		break;
	}
  }

  return r;
}

int fcntl(int fd, int cmd, ...)
{
  va_list argp, argp_321;
  message m;
  int r, org_errno;

  va_start(argp, cmd);
  va_start(argp_321, cmd);

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
  org_errno = errno;
  r = _syscall(VFS_PROC_NR, FCNTL, &m);

  if (r == -1 && errno == ENOSYS) {
	errno = org_errno;
	r = __fcntl_321(fd, cmd, argp_321);
  }

  va_end(argp_321);

  return r;
}

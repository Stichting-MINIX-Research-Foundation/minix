#include <lib.h>
#define fcntl _fcntl
#include <fcntl.h>
#include <stdarg.h>

#if _ANSI
PUBLIC int fcntl(int fd, int cmd, ...)
#else
PUBLIC int fcntl(fd, cmd)
int fd;
int cmd;
#endif
{
  va_list argp;
  message m;

  va_start(argp, cmd);

  /* Set up for the sensible case where there is no variable parameter.  This
   * covers F_GETFD, F_GETFL and invalid commands.
   */
  m.m1_i3 = 0;
  m.m1_p1 = NIL_PTR;

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
	m.m1_p1 = (char *) va_arg(argp, struct flock *);
	break;
  }

  /* Clean up and make the system call. */
  va_end(argp);
  m.m1_i1 = fd;
  m.m1_i2 = cmd;
  return(_syscall(FS, FCNTL, &m));
}

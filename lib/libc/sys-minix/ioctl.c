#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <sys/ioctl.h>

#ifdef __weak_alias
__weak_alias(ioctl, _ioctl)
#endif

int ioctl(fd, request, data)
int fd;
int request;
void *data;
{
  message m;

  m.TTY_LINE = fd;
  m.TTY_REQUEST = request;
  m.ADDRESS = (char *) data;
  return(_syscall(VFS_PROC_NR, IOCTL, &m));
}

#include <lib.h>
#define ioctl	_ioctl
#include <minix/com.h>
#include <sys/ioctl.h>

PUBLIC int ioctl(fd, request, data)
int fd;
int request;
void *data;
{
  message m;

  m.TTY_LINE = fd;
  m.TTY_REQUEST = request;
  m.ADDRESS = (char *) data;
  return(_syscall(FS, IOCTL, &m));
}

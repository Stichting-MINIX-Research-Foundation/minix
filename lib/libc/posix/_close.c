#include <lib.h>
#define close	_close
#include <unistd.h>

PUBLIC int close(fd)
int fd;
{
  message m;

  m.m1_i1 = fd;
  return(_syscall(FS, CLOSE, &m));
}

#include <lib.h>
#define fchmod	_fchmod
#include <sys/stat.h>

PUBLIC int fchmod(fd, mode)
int fd;
_mnx_Mode_t mode;
{
  message m;

  m.m3_i1 = fd;
  m.m3_i2 = mode;
  return(_syscall(FS, FCHMOD, &m));
}

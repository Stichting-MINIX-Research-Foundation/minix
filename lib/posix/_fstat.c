#include <lib.h>
#define fstat	_fstat
#include <sys/stat.h>

PUBLIC int fstat(fd, buffer)
int fd;
struct stat *buffer;
{
  message m;

  m.m1_i1 = fd;
  m.m1_p1 = (char *) buffer;
  return(_syscall(FS, FSTAT, &m));
}

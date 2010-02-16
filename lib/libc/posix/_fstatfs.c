#include <lib.h>
#define fstatfs	_fstatfs
#include <sys/stat.h>
#include <sys/statfs.h>

PUBLIC int fstatfs(int fd, struct statfs *buffer)
{
  message m;

  m.m1_i1 = fd;
  m.m1_p1 = (char *) buffer;
  return(_syscall(FS, FSTATFS, &m));
}

#include <lib.h>
#define lseek	_lseek
#include <unistd.h>

PUBLIC off_t lseek(fd, offset, whence)
int fd;
off_t offset;
int whence;
{
  message m;

  m.m2_i1 = fd;
  m.m2_l1 = offset;
  m.m2_i2 = whence;
  if (_syscall(FS, LSEEK, &m) < 0) return( (off_t) -1);
  return( (off_t) m.m2_l1);
}

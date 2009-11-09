#include <lib.h>
#define fchown	_fchown
#include <string.h>
#include <unistd.h>

PUBLIC int fchown(fd, owner, grp)
int fd;
_mnx_Uid_t owner;
_mnx_Gid_t grp;
{
  message m;

  m.m1_i1 = fd;
  m.m1_i2 = owner;
  m.m1_i3 = grp;
  return(_syscall(FS, FCHOWN, &m));
}

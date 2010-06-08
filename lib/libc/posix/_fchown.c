#include <lib.h>
#define fchown	_fchown
#include <string.h>
#include <unistd.h>

PUBLIC int fchown(int fd, uid_t owner, gid_t grp)
{
  message m;

  m.m1_i1 = fd;
  m.m1_i2 = owner;
  m.m1_i3 = grp;
  return(_syscall(VFS_PROC_NR, FCHOWN, &m));
}

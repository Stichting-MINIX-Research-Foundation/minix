#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(fchown, _fchown)
#endif

int fchown(int fd, uid_t owner, gid_t grp)
{
  message m;

  m.m1_i1 = fd;
  m.m1_i2 = owner;
  m.m1_i3 = grp;
  return(_syscall(VFS_PROC_NR, FCHOWN, &m));
}

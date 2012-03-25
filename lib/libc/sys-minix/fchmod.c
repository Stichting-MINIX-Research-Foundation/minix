#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <sys/stat.h>

#ifdef __weak_alias
__weak_alias(fchmod, _fchmod)
#endif

int fchmod(int fd, mode_t mode)
{
  message m;

  m.m1_i1 = fd;
  m.m1_i2 = mode;
  return(_syscall(VFS_PROC_NR, FCHMOD, &m));
}

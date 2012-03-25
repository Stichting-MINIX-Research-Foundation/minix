#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <unistd.h>

#ifdef __weak_alias
__weak_alias(fsync, _fsync)
#endif

int fsync(int fd)
{
  message m;

  m.m1_i1 = fd;

  return(_syscall(VFS_PROC_NR, FSYNC, &m));
}

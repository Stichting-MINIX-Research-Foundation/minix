#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <sys/stat.h>
#include <sys/statfs.h>

#ifdef __weak_alias
__weak_alias(fstatfs, _fstatfs)
#endif

int fstatfs(int fd, struct statfs *buffer)
{
  message m;

  m.m1_i1 = fd;
  m.m1_p1 = (char *) buffer;
  return(_syscall(VFS_PROC_NR, FSTATFS, &m));
}

#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <sys/statvfs.h>

#ifdef __weak_alias
__weak_alias(fstatvfs, _fstatvfs)
#endif

int fstatvfs(int fd, struct statvfs *buffer)
{
  message m;

  m.FSTATVFS_FD = fd;
  m.FSTATVFS_BUF = (char *) buffer;
  return(_syscall(VFS_PROC_NR, FSTATVFS, &m));
}

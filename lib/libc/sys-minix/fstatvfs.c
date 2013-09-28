#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <sys/statvfs.h>

#ifdef __weak_alias
__weak_alias(fstatvfs1, _fstatvfs1)
__weak_alias(fstatvfs, _fstatvfs)
#endif

int fstatvfs1(int fd, struct statvfs *buffer, int flags)
{
  message m;

  m.VFS_FSTATVFS1_FD = fd;
  m.VFS_FSTATVFS1_BUF = (char *) buffer;
  m.VFS_FSTATVFS1_FLAGS = flags;
  return(_syscall(VFS_PROC_NR, FSTATVFS1, &m));
}

int fstatvfs(int fd, struct statvfs *buffer)
{
  return fstatvfs1(fd, buffer, ST_WAIT);
}

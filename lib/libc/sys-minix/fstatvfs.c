#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <sys/statvfs.h>

#ifdef __weak_alias
__weak_alias(fstatvfs1, _fstatvfs1)
__weak_alias(fstatvfs, _fstatvfs)
#endif

int fstatvfs1(int fd, struct statvfs *buffer, int flags)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.VFS_STATVFS1_FD = fd;
  m.VFS_STATVFS1_BUF = (char *) buffer;
  m.VFS_STATVFS1_FLAGS = flags;
  return(_syscall(VFS_PROC_NR, VFS_FSTATVFS1, &m));
}

int fstatvfs(int fd, struct statvfs *buffer)
{
  return fstatvfs1(fd, buffer, ST_WAIT);
}

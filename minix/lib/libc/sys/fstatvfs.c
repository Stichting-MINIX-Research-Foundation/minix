#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <sys/statvfs.h>

#if defined(__weak_alias)
__weak_alias(fstatvfs, _fstatvfs)
#endif

int fstatvfs1(int fd, struct statvfs *buffer, int flags)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_vfs_statvfs1.fd = fd;
  m.m_lc_vfs_statvfs1.buf = (vir_bytes)buffer;
  m.m_lc_vfs_statvfs1.flags = flags;
  return(_syscall(VFS_PROC_NR, VFS_FSTATVFS1, &m));
}

int fstatvfs(int fd, struct statvfs *buffer)
{
  return fstatvfs1(fd, buffer, ST_WAIT);
}

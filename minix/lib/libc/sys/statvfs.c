#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <sys/types.h>
#include <sys/statvfs.h>
#include <string.h>

#ifdef __weak_alias
__weak_alias(statvfs, _statvfs)
#endif

int statvfs1(const char *name, struct statvfs *buffer, int flags)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_vfs_statvfs1.len = strlen(name) + 1;
  m.m_lc_vfs_statvfs1.name =  (vir_bytes)name;
  m.m_lc_vfs_statvfs1.buf = (vir_bytes)buffer;
  m.m_lc_vfs_statvfs1.flags = flags;
  return(_syscall(VFS_PROC_NR, VFS_STATVFS1, &m));
}

int statvfs(const char *name, struct statvfs *buffer)
{
  return statvfs1(name, buffer, ST_WAIT);
}

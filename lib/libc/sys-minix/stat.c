#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <sys/stat.h>
#include <string.h>

#ifdef __weak_alias
__weak_alias(_stat, __stat50);
__weak_alias(_lstat, __lstat50);
__weak_alias(_fstat, __fstat50);

__weak_alias(stat, __stat50);
__weak_alias(lstat, __lstat50);
__weak_alias(fstat, __fstat50);
#endif

int stat(const char *name, struct stat *buffer)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_vfs_stat.len = strlen(name) + 1;
  m.m_lc_vfs_stat.name = (vir_bytes)name;
  m.m_lc_vfs_stat.buf = (vir_bytes)buffer;

  return _syscall(VFS_PROC_NR, VFS_STAT, &m);
}

int fstat(int fd, struct stat *buffer)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_vfs_fstat.fd = fd;
  m.m_lc_vfs_fstat.buf = (vir_bytes)buffer;

  return _syscall(VFS_PROC_NR, VFS_FSTAT, &m);
}

int lstat(const char *name, struct stat *buffer)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_vfs_stat.len = strlen(name) + 1;
  m.m_lc_vfs_stat.name = (vir_bytes)name;
  m.m_lc_vfs_stat.buf = (vir_bytes)buffer;

  return _syscall(VFS_PROC_NR, VFS_LSTAT, &m);
}

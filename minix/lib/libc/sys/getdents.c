#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <dirent.h>

ssize_t getdents(int fd, char *buffer, size_t nbytes)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_vfs_readwrite.fd = fd;
  m.m_lc_vfs_readwrite.len = nbytes;
  m.m_lc_vfs_readwrite.buf = (vir_bytes)buffer;
  m.m_lc_vfs_readwrite.cum_io = 0;
  return _syscall(VFS_PROC_NR, VFS_GETDENTS, &m);
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(getdents, __getdents30)
#endif

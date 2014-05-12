#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <unistd.h>
#include <string.h>

#ifdef __weak_alias
__weak_alias(readlink, _readlink)
#endif

ssize_t readlink(const char *name, char *buffer, size_t bufsiz)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_vfs_readlink.namelen = strlen(name) + 1;
  m.m_lc_vfs_readlink.bufsize = bufsiz;
  m.m_lc_vfs_readlink.name = (vir_bytes)name;
  m.m_lc_vfs_readlink.buf = (vir_bytes)buffer;

  return(_syscall(VFS_PROC_NR, VFS_READLINK, &m));
}

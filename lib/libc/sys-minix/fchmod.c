#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <sys/stat.h>

int fchmod(int fd, mode_t mode)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_vfs_fchmod.fd = fd;
  m.m_lc_vfs_fchmod.mode = mode;
  return(_syscall(VFS_PROC_NR, VFS_FCHMOD, &m));
}

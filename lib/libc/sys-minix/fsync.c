#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <unistd.h>

int fsync(int fd)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_vfs_fsync.fd = fd;

  return(_syscall(VFS_PROC_NR, VFS_FSYNC, &m));
}

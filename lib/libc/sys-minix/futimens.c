#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <sys/stat.h>

int futimens(int fd, const struct timespec tv[2])
{
  message m;
  static const struct timespec now[2] = { {0, UTIME_NOW}, {0, UTIME_NOW} };

  if (tv == NULL) tv = now;

  memset(&m, 0, sizeof(m));
  m.m_vfs_utimens.fd = fd;
  m.m_vfs_utimens.atime = tv[0].tv_sec;
  m.m_vfs_utimens.mtime = tv[1].tv_sec;
  m.m_vfs_utimens.ansec = tv[0].tv_nsec;
  m.m_vfs_utimens.mnsec = tv[1].tv_nsec;
  m.m_vfs_utimens.name = NULL;
  m.m_vfs_utimens.flags = 0;

  return(_syscall(VFS_PROC_NR, VFS_UTIMENS, &m));
}

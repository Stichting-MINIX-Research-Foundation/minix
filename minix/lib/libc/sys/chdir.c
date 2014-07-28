#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <unistd.h>

int chdir(name)
const char *name;
{
  message m;

  memset(&m, 0, sizeof(m));
  _loadname(name, &m);
  return(_syscall(VFS_PROC_NR, VFS_CHDIR, &m));
}

int fchdir(fd)
int fd;
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_vfs_fchdir.fd = fd;
  return(_syscall(VFS_PROC_NR, VFS_FCHDIR, &m));
}

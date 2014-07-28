#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <sys/stat.h>
#include <string.h>

int mkdir(const char *name, mode_t mode)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_vfs_path.mode = mode;
  _loadname(name, &m);
  return(_syscall(VFS_PROC_NR, VFS_MKDIR, &m));
}

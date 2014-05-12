#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>
#include <unistd.h>

int symlink(const char *name, const char *name2)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_vfs_link.len1 = strlen(name) + 1;
  m.m_lc_vfs_link.len2 = strlen(name2) + 1;
  m.m_lc_vfs_link.name1 = (vir_bytes)name;
  m.m_lc_vfs_link.name2 = (vir_bytes)name2;
  return(_syscall(VFS_PROC_NR, VFS_SYMLINK, &m));
}

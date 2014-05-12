#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

int mknod(const char *name, mode_t mode, dev_t dev)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_vfs_mknod.len = strlen(name) + 1;
  m.m_lc_vfs_mknod.mode = mode;
  m.m_lc_vfs_mknod.device = dev;
  m.m_lc_vfs_mknod.name = (vir_bytes)name;
  return(_syscall(VFS_PROC_NR, VFS_MKNOD, &m));
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(mknod, __mknod50)
#endif

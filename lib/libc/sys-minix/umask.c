#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>
#include <sys/stat.h>

mode_t umask(mode_t complmode)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_vfs_umask.mask = complmode;
  return( (mode_t) _syscall(VFS_PROC_NR, VFS_UMASK, &m));
}

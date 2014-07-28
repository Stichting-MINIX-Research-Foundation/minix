#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(__posix_chown, chown)
#endif

int chown(const char *name, uid_t owner, gid_t grp)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_vfs_chown.len = strlen(name) + 1;
  m.m_lc_vfs_chown.owner = owner;
  m.m_lc_vfs_chown.group = grp;
  m.m_lc_vfs_chown.name = (vir_bytes)name;
  return(_syscall(VFS_PROC_NR, VFS_CHOWN, &m));
}

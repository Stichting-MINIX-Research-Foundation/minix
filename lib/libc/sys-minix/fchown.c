#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(__posix_fchown, fchown)
#endif

int fchown(int fd, uid_t owner, gid_t grp)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_vfs_chown.fd = fd;
  m.m_lc_vfs_chown.owner = owner;
  m.m_lc_vfs_chown.group = grp;
  return(_syscall(VFS_PROC_NR, VFS_FCHOWN, &m));
}
